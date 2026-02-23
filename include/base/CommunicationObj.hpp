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
     * @param listener The listener for incoming events. MUST be a valid object reference.
     * @param bufferSize Size of the internal ring buffer (default 64KB).
     */
    CommunicationObj(ICommunicationListener& listener, size_t bufferSize = 64 * 1024)
        : m_listener(listener),
          m_running(false),
          m_bufferSize(bufferSize),
          m_cacheManager(NULL)
    {
    }

    virtual ~CommunicationObj()
    {
        stop();
        if (m_cacheManager)
        {
            delete m_cacheManager;
            m_cacheManager = NULL;
        }
    }

    /**
     * Starts the communication threads.
     */
    bool start()
    {
        size_t maxFrameSize = 0;
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_running)
        {
            return true;
        }

        if (open() != 0)
        {
            return false;
        }

        // Initialize CacheManager with the correct block size
        maxFrameSize = getMaxFrameSize();
        if (m_cacheManager) delete m_cacheManager;
        m_cacheManager = new CacheManager(m_bufferSize, maxFrameSize);

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
        if (m_cacheManager) m_cacheManager->notifyStop();

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
    // --- Inner Class: CacheManager ---
    class CacheManager
    {
    public:
        CacheManager(size_t totalSize, size_t maxFrameSize)
            : m_buffer(NULL),
              m_head(0),
              m_tail(0),
              m_processingIndex(-1)
        {
            // Calculate block size: 4 bytes header + maxFrameSize
            // Align to 4 bytes
            m_blockSize = (maxFrameSize + 4 + 3) & ~3;
            m_numBlocks = totalSize / m_blockSize;
            if (m_numBlocks < 2) m_numBlocks = 2; // Minimum 2 blocks

            m_buffer = (uint8_t*)malloc(m_numBlocks * m_blockSize);
        }

        ~CacheManager()
        {
            if (m_buffer) free(m_buffer);
        }

        // Called by RX Thread
        bool push(const uint8_t* data, size_t length)
        {
            size_t nextHead = 0;
            uint8_t* ptr = NULL;
            std::lock_guard<std::mutex> lock(m_mutex);

            nextHead = (m_head + 1) % m_numBlocks;

            // Check 1: Is the next block currently being processed by callback?
            if ((int32_t)nextHead == m_processingIndex)
            {
                // Drop new packet to protect memory in use
                return false;
            }

            // Check 2: Overwrite old data if full
            if (nextHead == m_tail)
            {
                // Force advance tail (drop oldest packet)
                m_tail = (m_tail + 1) % m_numBlocks;
            }

            // Write data
            ptr = m_buffer + (m_head * m_blockSize);
            *(uint32_t*)ptr = (uint32_t)length;
            memcpy(ptr + 4, data, length);

            m_head = nextHead;
            m_cond.notify_one();
            return true;
        }

        // Called by Worker Thread
        void process(ICommunicationListener& listener, const std::atomic<bool>& running)
        {
            size_t currentIndex = 0;
            uint8_t* ptr = NULL;
            uint32_t len = 0;

            while (running)
            {
                {
                    std::unique_lock<std::mutex> lock(m_mutex);

                    m_cond.wait(lock, [this, &running] {
                        return m_head != m_tail || !running;
                    });

                    if (!running) break;

                    // Lock current block for processing
                    currentIndex = m_tail;
                    m_processingIndex = (int32_t)currentIndex;

                    // Advance tail logically (removed from queue, but kept in memory)
                    m_tail = (m_tail + 1) % m_numBlocks;
                } // Release lock before callback

                // Zero-Copy access
                ptr = m_buffer + (currentIndex * m_blockSize);
                len = *(uint32_t*)ptr;

                if (running)
                {
                    listener.onDataReceived(ptr + 4, len);
                }

                // Release processing lock
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_processingIndex = -1;
                }
            }
        }

        void notifyStop()
        {
            m_cond.notify_all();
        }

    private:
        uint8_t* m_buffer;
        size_t m_blockSize;
        size_t m_numBlocks;

        size_t m_head;
        size_t m_tail;
        int32_t m_processingIndex; // -1 if none

        std::mutex m_mutex;
        std::condition_variable m_cond;
    };

    // --- RX Thread: Reads from Hardware and pushes to CacheManager ---
    void rxThreadLoop()
    {
        size_t maxFrameSize = getMaxFrameSize();
        uint8_t* tempBuffer = (uint8_t*)malloc(maxFrameSize);
        int32_t bytesRead = 0;

        while (m_running)
        {
            bytesRead = read(tempBuffer, maxFrameSize);

            if (bytesRead > 0)
            {
                if (!m_cacheManager->push(tempBuffer, (size_t)bytesRead))
                {
                    // Buffer full and locked by callback -> Drop
                    if (m_running)
                    {
                        // Optional: notify overflow error
                        // m_listener.onError(ERROR_BUFFER_OVERFLOW);
                    }
                }
            }
            else if (bytesRead < 0)
            {
                if (m_running)
                {
                    m_listener.onError(bytesRead);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        }

        free(tempBuffer);
    }

    // --- Worker Thread: Pulls from CacheManager and calls Callback ---
    void workerThreadLoop()
    {
        if (m_cacheManager)
        {
            m_cacheManager->process(m_listener, m_running);
        }
    }

    ICommunicationListener& m_listener;

    std::thread m_rxThread;
    std::thread m_workerThread;

    std::atomic<bool> m_running;
    std::mutex m_mutex; // Protects general state

    size_t m_bufferSize;
    CacheManager* m_cacheManager;
};

} // namespace base

#endif // COMMUNICATION_OBJ_HPP
