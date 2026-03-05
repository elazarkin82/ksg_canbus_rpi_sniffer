#include "sniffer_client.h"
#include "communication/UdpCanbusCommunication.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <cstring>
#include <iostream>

using namespace communication;

class SnifferClient : public base::ICommunicationListener
{
public:
    SnifferClient(const char* ip, uint16_t remote_port, uint16_t local_port, uint32_t keep_alive_interval_ms)
        : m_udp(nullptr),
          m_keepAliveInterval(keep_alive_interval_ms),
          m_running(false),
          m_lastSentTime(0)
    {
        m_udp = new UdpCanbusCommunication(*this, ip, remote_port, local_port);
    }

    ~SnifferClient()
    {
        stop();
        if (m_udp) delete m_udp;
    }

    int start()
    {
        if (m_running) return 0;

        if (!m_udp->start()) return -1;

        m_running = true;
        if (m_keepAliveInterval > 0)
        {
            m_keepAliveThread = std::thread(&SnifferClient::keepAliveLoop, this);
        }
        return 0;
    }

    void stop()
    {
        if (!m_running) return;

        m_running = false;

        // Send reset command (optional, but good practice)
        // sendRawCommand(CMD_SET_FILTERS, nullptr, 0); // Clear filters? Or maybe a specific RESET command?
        // For now, just stop logging
        bool enable = false;
        sendRawCommand(CMD_EXTERNAL_SERVICE_LOGGING_OFF, (const uint8_t*)&enable, 0);

        m_udp->stop();

        if (m_keepAliveThread.joinable())
        {
            m_keepAliveThread.join();
        }
    }

    void sendRawCommand(uint32_t command_id, const uint8_t* payload, size_t len)
    {
        // Debug print
        printf("[SDK] Sending command: 0x%X\n", command_id);

        ExternalMessageV1 msg;
        memset(&msg, 0, sizeof(msg));
        strncpy(msg.magic_key, "v1.00", 8);
        msg.command = command_id;
        msg.data_size = len;
        if (len > 0 && len <= sizeof(msg.data))
        {
            memcpy(msg.data, payload, len);
        }

        size_t send_len = calculateExternalMessageV1Size(len);
        m_udp->send((const uint8_t*)&msg, send_len);

        updateLastSentTime();
    }

    int readMessage(ExternalCanfdMessage* out_msg, int timeout_ms)
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        if (m_queue.empty())
        {
            if (m_queueCond.wait_for(lock, std::chrono::milliseconds(timeout_ms)) == std::cv_status::timeout)
            {
                return 0; // Timeout
            }
        }

        if (m_queue.empty()) return 0; // Should not happen unless spurious wakeup or stop

        *out_msg = m_queue.front();
        m_queue.pop();
        return 1;
    }

    // --- ICommunicationListener ---
    virtual void onDataReceived(const uint8_t* data, size_t length) override
    {
        // We expect ExternalCanfdMessage here (from UdpCanbusCommunication callback for CANFD protocol)
        // Wait, UdpCanbusCommunication parses V1 and CANFD.
        // If it's V1 CMD_CANBUS_DATA, it calls onDataReceived with payload.
        // If it's CANFD, it calls onDataReceived with struct canfd_frame.

        // But wait, UdpCanbusCommunication::processPacket calls m_targetListener.onDataReceived.
        // If it's V1, it passes msg->data.
        // If it's CANFD, it passes &msg->frame.

        // We need to wrap it back into ExternalCanfdMessage or similar structure for the queue.
        // Let's assume we only care about CAN frames for now.

        if (length == sizeof(struct canfd_frame))
        {
            ExternalCanfdMessage msg;
            memset(&msg, 0, sizeof(msg));
            // Magic key doesn't matter for internal queue, but let's set it
            strncpy(msg.magic_key, "canf", 4);
            memcpy(&msg.frame, data, length);

            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_queue.push(msg);
            m_queueCond.notify_one();
        }
    }

    virtual void onError(int32_t errorCode) override
    {
        // Log error
        std::cerr << "Client Error: " << errorCode << std::endl;
    }

private:
    void keepAliveLoop()
    {
        while (m_running)
        {
            uint64_t now = getCurrentTimeMs();
            uint64_t last = m_lastSentTime.load();
            uint64_t diff = now - last;

            if (diff >= m_keepAliveInterval)
            {
                // Debug print
                printf("[SDK] Sending Keep Alive...\n");
                sendRawCommand(CMD_CANBUS_DATA, nullptr, 0);
                std::this_thread::sleep_for(std::chrono::milliseconds(m_keepAliveInterval));
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(m_keepAliveInterval - diff));
            }
        }
    }

    void updateLastSentTime()
    {
        m_lastSentTime.store(getCurrentTimeMs());
    }

    uint64_t getCurrentTimeMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

    UdpCanbusCommunication* m_udp;
    uint32_t m_keepAliveInterval;
    std::atomic<bool> m_running;
    std::thread m_keepAliveThread;
    std::atomic<uint64_t> m_lastSentTime;

    std::queue<ExternalCanfdMessage> m_queue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCond;
};

// --- C API Implementation ---

extern "C" {

void* client_create(const char* ip, uint16_t remote_port, uint16_t local_port, uint32_t keep_alive_interval_ms)
{
    return new SnifferClient(ip, remote_port, local_port, keep_alive_interval_ms);
}

int client_start(void* handle)
{
    if (!handle) return -1;
    return ((SnifferClient*)handle)->start();
}

void client_stop(void* handle)
{
    if (!handle) return;
    ((SnifferClient*)handle)->stop();
}

void client_destroy(void* handle)
{
    if (!handle) return;
    delete (SnifferClient*)handle;
}

void client_send_log_enable(void* handle, bool enable)
{
    if (!handle) return;
    uint32_t cmd = enable ? CMD_EXTERNAL_SERVICE_LOGGING_ON : CMD_EXTERNAL_SERVICE_LOGGING_OFF;
    ((SnifferClient*)handle)->sendRawCommand(cmd, nullptr, 0);
}

void client_send_injection(void* handle, uint8_t target, uint32_t can_id, const uint8_t* data, uint8_t len)
{
    if (!handle) return;
    uint32_t cmd = (target == 1) ? CMD_CANBUS_TO_SYSTEM : CMD_CANBUS_TO_CAR;

    // We need to send a can_frame struct, not just raw data?
    // The Sniffer expects ExternalMessageV1 payload to be... what?
    // In Sniffer::onCommandReceived:
    // m_carSystemCan->send(data, length);
    // ObdCanbusCommunication::send writes raw bytes to socket.
    // So we must send a struct can_frame.

    struct can_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.can_id = can_id;
    frame.can_dlc = len > 8 ? 8 : len;
    memcpy(frame.data, data, frame.can_dlc);

    ((SnifferClient*)handle)->sendRawCommand(cmd, (const uint8_t*)&frame, sizeof(frame));
}

void client_set_filters(void* handle, const CanFilterRule* rules, size_t count)
{
    if (!handle) return;
    size_t size = count * sizeof(CanFilterRule);
    ((SnifferClient*)handle)->sendRawCommand(CMD_SET_FILTERS, (const uint8_t*)rules, size);
}

void client_send_raw_command(void* handle, uint32_t command_id, const uint8_t* payload, size_t len)
{
    if (!handle) return;
    ((SnifferClient*)handle)->sendRawCommand(command_id, payload, len);
}

int client_read_message(void* handle, ExternalCanfdMessage* out_msg, int timeout_ms)
{
    if (!handle) return -1;
    return ((SnifferClient*)handle)->readMessage(out_msg, timeout_ms);
}

CanFilterRule client_create_rule(uint32_t id, uint8_t action, uint8_t target,
                                 uint8_t data_index, uint8_t data_value, uint8_t data_mask)
{
    CanFilterRule rule;
    memset(&rule, 0, sizeof(rule));
    rule.can_id = id;
    rule.action_type = action;
    rule.target = target;
    rule.data_index = data_index;
    rule.data_value = data_value;
    rule.data_mask = data_mask;
    return rule;
}

}
