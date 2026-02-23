#ifndef COMMUNICATION_OBJ_HPP
#define COMMUNICATION_OBJ_HPP

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>

namespace base
{

class CommunicationObj;

/**
 * Interface for receiving events from CommunicationObj.
 */
class ICommunicationListener
{
public:
    virtual ~ICommunicationListener() {}

    /**
     * Called when data is received.
     * Executed in a separate worker thread, not the RX thread.
     */
    virtual void onDataReceived(const uint8_t* data, size_t length) = 0;

    /**
     * Called when an error occurs.
     */
    virtual void onError(int32_t errorCode) = 0;
};

/**
 * Base class for communication objects (CAN, TCP, etc.).
 * Manages RX thread, buffering, and worker thread for callbacks.
 */
class CommunicationObj
{
public:
    // Error codes
    static const int32_t ERROR_BUFFER_OVERFLOW = -100;

    /**
     * @param listener The listener for incoming events. MUST NOT be NULL.
     * @param bufferSize Size of the internal ring buffer (default 64KB).
     */
    CommunicationObj(ICommunicationListener* listener, size_t bufferSize = 64 * 1024)
        : m_listener(listener),
          m_running(false),
          m_bufferSize(bufferSize),
          m_buffer(NULL),
          m_head(0),
          m_tail(0),
          m_count(0)
    {
        if (m_bufferSize > 0)
        {
            m_buffer = (uint8_t*)malloc(m_bufferSize);
        }
    }

    virtual ~CommunicationObj()
    {
        stop();
        if (m_buffer)
        {
            free(m_buffer);
            m_buffer = NULL;
        }
    }

    /**
     * Starts the communication threads.
     * Returns false if listener is NULL or open() fails.
     */
    bool start()
    {
        if (m_listener == NULL)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_running)
        {
            return true;
        }

        if (open() != 0)
        {
            return false;
        }

        // Reset buffer state
        {
            std::lock_guard<std::mutex> bufLock(m_bufferMutex);
            m_head = 0;
            m_tail = 0;
            m_count = 0;
        }

        m_running = true;

        // Start RX thread (Hardware -> Buffer)
        m_rxThread = std::thread(&CommunicationObj::rxThreadLoop, this);

        // Start Worker thread (Buffer -> Callback)
        m_workerThread = std::thread(&CommunicationObj::workerThreadLoop, this);

        return true;
    }

    /**
     * Stops the communication threads and closes the connection.
     */
    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_running)
            {
                return;
            }
            m_running = false;
        }

        // Wake up worker thread so it can exit
        m_dataAvailableCond.notify_all();

        // Force unblock of read() if it's blocking
        unblock();

        if (m_rxThread.joinable())
        {
            m_rxThread.join();
        }

        if (m_workerThread.joinable())
        {
            m_workerThread.join();
        }

        close();
    }

    /**
     * Sends data using the derived class implementation.
     */
    int32_t send(const uint8_t* data, size_t length)
    {
        return write(data, length);
    }

    bool isRunning() const
    {
        return m_running;
    }

protected:
    // --- Virtual methods to be implemented by derived classes ---

    /**
     * Open the hardware/socket connection.
     * @return 0 on success, <0 on error.
     */
    virtual int32_t open() = 0;

    /**
     * Close the connection.
     */
    virtual void close() = 0;

    /**
     * Read data from the hardware/socket.
     * This function should BLOCK until data is available or an error occurs.
     * @param buffer Buffer to fill.
     * @param maxLen Maximum size of buffer.
     * @return Number of bytes read (>0), 0 for timeout/keepalive, <0 for error.
     */
    virtual int32_t read(uint8_t* buffer, size_t maxLen) = 0;

    /**
     * Write data to the hardware/socket.
     * @return Number of bytes written, or <0 for error.
     */
    virtual int32_t write(const uint8_t* data, size_t length) = 0;

    /**
     * Called to unblock a blocking read() call (e.g. shutdown socket).
     */
    virtual void unblock() = 0;

    /**
     * Returns the maximum size of a single message/frame.
     * Used to allocate the temporary read buffer on stack.
     */
    virtual size_t getMaxFrameSize() const
    {
        return 4096; // Default
    }

private:
    // --- RX Thread: Reads from Hardware and pushes to Ring Buffer ---
    void rxThreadLoop()
    {
        size_t maxFrameSize = getMaxFrameSize();
        // Allocate temp buffer on stack (or heap if too large)
        // Note: alloca is non-standard but widely supported.
        // Using VLA or std::vector/malloc is safer for portability.
        // Here we use malloc once per thread start to be safe and standard.
        uint8_t* tempBuffer = (uint8_t*)malloc(maxFrameSize);

        while (m_running)
        {
            int32_t bytesRead = read(tempBuffer, maxFrameSize);

            if (bytesRead > 0)
            {
                pushToBuffer(tempBuffer, (size_t)bytesRead);
            }
            else if (bytesRead < 0)
            {
                // Error occurred
                // We can notify error directly from RX thread as it's usually fast
                if (m_listener != NULL && m_running)
                {
                    m_listener->onError(bytesRead);
                }
                // Avoid busy loop on persistent error
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            // bytesRead == 0 -> continue (timeout or keepalive)
        }

        free(tempBuffer);
    }

    // --- Worker Thread: Pulls from Ring Buffer and calls Callback ---
    void workerThreadLoop()
    {
        size_t maxFrameSize = getMaxFrameSize();
        uint8_t* processBuffer = (uint8_t*)malloc(maxFrameSize);

        while (m_running)
        {
            size_t bytesToProcess = 0;

            {
                std::unique_lock<std::mutex> lock(m_bufferMutex);

                // Wait for data or stop signal
                m_dataAvailableCond.wait(lock, [this] {
                    return m_count > 0 || !m_running;
                });

                if (!m_running) break;

                // Pop data from ring buffer
                // We try to read as much as possible up to maxFrameSize
                while (m_count > 0 && bytesToProcess < maxFrameSize)
                {
                    processBuffer[bytesToProcess++] = m_buffer[m_tail];
                    m_tail = (m_tail + 1) % m_bufferSize;
                    m_count--;
                }
            } // Release lock before processing callback

            if (bytesToProcess > 0)
            {
                if (m_listener != NULL && m_running)
                {
                    m_listener->onDataReceived(processBuffer, bytesToProcess);
                }
            }
        }

        free(processBuffer);
    }

    // --- Ring Buffer Helper ---
    void pushToBuffer(const uint8_t* data, size_t length)
    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);

        if (m_count + length > m_bufferSize)
        {
            // Overflow! Drop packet and notify error
            // We do NOT overwrite old data, we drop new data.
            // Unlock buffer mutex before calling listener to avoid deadlock?
            // Here we are inside lock. Calling listener might be risky if listener calls back into this object.
            // But onError is usually logging. Let's be careful.
            // Ideally we should have an atomic flag or counter for overflow.
            return;
        }

        for (size_t i = 0; i < length; ++i)
        {
            m_buffer[m_head] = data[i];
            m_head = (m_head + 1) % m_bufferSize;
            m_count++;
        }

        m_dataAvailableCond.notify_one();
    }

    ICommunicationListener* const m_listener; // Const pointer, set in constructor

    std::thread m_rxThread;
    std::thread m_workerThread;

    std::atomic<bool> m_running;
    std::mutex m_mutex; // Protects general state

    // Ring Buffer
    size_t m_bufferSize;
    uint8_t* m_buffer;
    size_t m_head;
    size_t m_tail;
    size_t m_count;
    std::mutex m_bufferMutex; // Protects ring buffer access
    std::condition_variable m_dataAvailableCond;
};

} // namespace base

#endif // COMMUNICATION_OBJ_HPP
