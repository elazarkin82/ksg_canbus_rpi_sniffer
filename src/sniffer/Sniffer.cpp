#include "sniffer/Sniffer.h"
#include "communication/CanbusProtocol.h"
#include "canbus_communication/ObdCanbusCommunication.h"
#include "communication/UdpCanbusCommunication.h"

#include <stdio.h>
#include <string.h>
#include <chrono>
#include <algorithm> // For std::sort

namespace sniffer
{

// --- Filter Engine ---
namespace FilterEngine
{
    // --- Static Data ---
    // Raw memory block for rules (64000 bytes)
    static uint8_t rules_raw_memory[64000];

    // Pointer to interpret raw memory as rules array
    static communication::CanFilterRule* rules_ptr = (communication::CanFilterRule*)rules_raw_memory;
    static int rules_count = 0;

    // Calculate MAX_RULES dynamically based on memory size and struct size
    static const int MAX_RULES = sizeof(rules_raw_memory) / sizeof(communication::CanFilterRule);

    // Lookup table: pointers into rules_ptr for each CAN ID
    static communication::CanFilterRule* id_lookup[2048];
    // Counts: number of rules for each CAN ID
    static uint8_t id_counts[2048];

    static std::mutex filter_mutex; // Protects access to the filter engine data

    // --- Functions ---
    static void init_internal()
    {
        rules_count = 0;
        memset(rules_raw_memory, 0, sizeof(rules_raw_memory));
        memset(id_lookup, 0, sizeof(id_lookup));
        memset(id_counts, 0, sizeof(id_counts));
    }

    static void init()
    {
        std::lock_guard<std::mutex> lock(filter_mutex);
        init_internal();
    }

    static void loadRules(const uint8_t* data, size_t length)
    {
        std::lock_guard<std::mutex> lock(filter_mutex);
        size_t num_rules;
        int i;

        // 1. Reset
        init_internal();

        // 2. Copy data directly to raw memory
        if (length > sizeof(rules_raw_memory))
        {
            length = sizeof(rules_raw_memory);
        }

        num_rules = length / sizeof(communication::CanFilterRule);

        if (num_rules > MAX_RULES)
        {
            num_rules = MAX_RULES;
            length = num_rules * sizeof(communication::CanFilterRule);
        }

        memcpy(rules_raw_memory, data, length);
        rules_count = num_rules;

        // 3. Sort by CAN ID
        std::sort(rules_ptr, rules_ptr + rules_count,
            [](const communication::CanFilterRule& a, const communication::CanFilterRule& b) {
                return (a.can_id & 0x7FF) < (b.can_id & 0x7FF);
            });

        // 4. Build lookup table
        for (i = 0; i < rules_count; ++i)
        {
            uint32_t id = rules_ptr[i].can_id & 0x7FF; // Mask priority bits
            if (id >= 2048) continue;

            if (id_counts[id] == 0)
            {
                id_lookup[id] = &rules_ptr[i];
            }
            id_counts[id]++;
        }
    }

    static bool process(uint32_t can_id, uint8_t* data, size_t len, Sniffer::Source source)
    {
        std::lock_guard<std::mutex> lock(filter_mutex);
        communication::CanFilterRule* rule;
        int count;
        uint32_t masked_id;

        masked_id = can_id & 0x7FF; // Extract Standard ID

        if (masked_id >= 2048 || id_counts[masked_id] == 0)
        {
            return true; // PASS
        }

        rule = id_lookup[masked_id];
        count = id_counts[masked_id];

        for (int i = 0; i < count; ++i, ++rule)
        {
            // Check direction
            if (rule->target == communication::FILTER_TARGET_TO_SYSTEM && source != Sniffer::SOURCE_CAR_COMPUTER)
            {
                continue; // Rule not for this direction
            }
            if (rule->target == communication::FILTER_TARGET_TO_CAR && source != Sniffer::SOURCE_CAR_SYSTEM)
            {
                continue; // Rule not for this direction
            }

            bool match = true;
            if (rule->data_mask != 0)
            {
                if (len <= rule->data_index)
                {
                    match = false;
                }
                else
                {
                    if ((data[rule->data_index] & rule->data_mask) != rule->data_value)
                    {
                        match = false;
                    }
                }
            }

            if (match)
            {
                if (rule->action_type == 1) // DROP
                {
                    return false;
                }
                if (rule->action_type == 2) // MODIFY
                {
                    for (int j = 0; j < 8 && j < (int)len; ++j)
                    {
                        data[j] = (data[j] & ~rule->modification_mask[j]) |
                                  (rule->modification_data[j] & rule->modification_mask[j]);
                    }
                    return true;
                }
                return true; // PASS
            }
        }
        return true; // No rule matched
    }
} // namespace FilterEngine


Sniffer::Sniffer(const SnifferParams& params)
    : m_systemListener(*this, Sniffer::SOURCE_CAR_SYSTEM),
      m_computerListener(*this, Sniffer::SOURCE_CAR_COMPUTER),
      m_externalListener(*this, Sniffer::SOURCE_EXTERNAL),
      m_running(false),
      m_externalServiceLogging(false),
      m_lastExternalMsgTime(std::chrono::steady_clock::now()),
      m_systemCallback(nullptr)
{
    // Initialize CAN interfaces
    m_carSystemCan = new canbus_communication::ObdCanbusCommunication(m_systemListener, params.car_system_can_name);
    m_carComputerCan = new canbus_communication::ObdCanbusCommunication(m_computerListener, params.car_computer_can_name);

    // Initialize External Service (UDP)
    m_externalService = new communication::UdpCanbusCommunication(m_externalListener, "0.0.0.0", 0, params.external_service_port);
    m_externalService->setCommandListener(this);

    FilterEngine::init();
}

Sniffer::~Sniffer()
{
    stop();

    if (m_carSystemCan)
    {
        delete m_carSystemCan;
        m_carSystemCan = nullptr;
    }

    if (m_carComputerCan)
    {
        delete m_carComputerCan;
        m_carComputerCan = nullptr;
    }

    if (m_externalService)
    {
        delete m_externalService;
        m_externalService = nullptr;
    }
}

bool Sniffer::start()
{
    if (m_running)
    {
        return true;
    }

    m_running = true;

    if (!m_carSystemCan->start())
    {
        fprintf(stderr, "Failed to start Car System CAN\n");
        return false;
    }

    if (!m_carComputerCan->start())
    {
        fprintf(stderr, "Failed to start Car Computer CAN\n");
        return false;
    }

    if (!m_externalService->start())
    {
        fprintf(stderr, "Failed to start External Service\n");
        return false;
    }

    m_watchdogThread = std::thread(&Sniffer::watchdogLoop, this);

    return true;
}

void Sniffer::stop()
{
    if (!m_running)
    {
        return;
    }

    m_running = false;

    if (m_watchdogThread.joinable())
    {
        m_watchdogThread.join();
    }

    if (m_carSystemCan) m_carSystemCan->stop();
    if (m_carComputerCan) m_carComputerCan->stop();
    if (m_externalService) m_externalService->stop();
}

void Sniffer::setSystemCallback(ISystemCallback* callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_systemCallback = callback;
}

void Sniffer::CanListener::onDataReceived(const uint8_t* data, size_t length)
{
    m_parent.handleCanData(m_source, data, length);
}

void Sniffer::CanListener::onError(int32_t errorCode)
{
    (void)errorCode;
}

void Sniffer::handleCanData(Source source, const uint8_t* data, size_t length)
{
    uint8_t buffer[72]; // Max CAN FD size
    struct can_frame* frame;
    bool should_forward;
    size_t copyLen;

    if (!m_running) return;

    // Copy data to local buffer for modification
    copyLen = (length < sizeof(buffer)) ? length : sizeof(buffer);
    memcpy(buffer, data, copyLen);

    // The data is a raw CAN frame. We need to cast it to access ID and data.
    // Assuming Standard CAN frame for now as ObdCanbusCommunication uses CAN_RAW
    frame = (struct can_frame*)buffer;

    // Process with filter engine
    should_forward = FilterEngine::process(frame->can_id, frame->data, frame->can_dlc, source);

    if (should_forward)
    {
        if (source == Sniffer::SOURCE_CAR_SYSTEM)
        {
            m_carComputerCan->send(buffer, copyLen);
        }
        else if (source == Sniffer::SOURCE_CAR_COMPUTER)
        {
            m_carSystemCan->send(buffer, copyLen);
        }
    }

    if (m_externalServiceLogging)
    {
        communication::ExternalCanfdMessage msg;

        memset(&msg, 0, sizeof(msg));
        if (source == Sniffer::SOURCE_CAR_SYSTEM)
        {
            memcpy(msg.magic_key, "canS", 4);
        }
        else if (source == Sniffer::SOURCE_CAR_COMPUTER)
        {
            memcpy(msg.magic_key, "canC", 4);
        }

        // Log the MODIFIED frame if it was forwarded, or the ORIGINAL?
        // Usually we want to log what was actually sent.
        // If dropped, we might still want to log it (maybe with a flag?).
        // For now, let's log what would be sent (the modified buffer).

        copyLen = (length < sizeof(msg.frame)) ? length : sizeof(msg.frame);
        memcpy(&msg.frame, buffer, copyLen);

        m_externalService->send((const uint8_t*)&msg, sizeof(msg));
    }
}

void Sniffer::onCommandReceived(uint32_t command, const uint8_t* data, size_t length)
{
    if (!m_running) return;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastExternalMsgTime = std::chrono::steady_clock::now();
    }

    if (command == communication::CMD_SET_FILTERS)
    {
        FilterEngine::loadRules(data, length);
        printf("Filter rules updated\n");
    }
    else if (command == communication::CMD_CANBUS_TO_SYSTEM)
    {
        m_carSystemCan->send(data, length);
    }
    else if (command == communication::CMD_CANBUS_TO_CAR)
    {
        m_carComputerCan->send(data, length);
    }
    else if (command == communication::CMD_CANBUS_DATA)
    {
        // Keep-alive
    }
    else if (command == communication::CMD_EXTERNAL_SERVICE_LOGGING_ON)
    {
        m_externalServiceLogging = true;
        printf("External Service Logging ON\n");
    }
    else if (command == communication::CMD_EXTERNAL_SERVICE_LOGGING_OFF)
    {
        m_externalServiceLogging = false;
        printf("External Service Logging OFF\n");
    }
    else if (command == communication::CMD_SET_PARAMS)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_systemCallback)
        {
            m_systemCallback->onSystemCommand(command, data, length);
        }
    }
}

void Sniffer::watchdogLoop()
{
    while (m_running)
    {
        bool resetNeeded = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_externalServiceLogging)
            {
                std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
                int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastExternalMsgTime).count();

                if (elapsed > 1000)
                {
                    resetNeeded = true;
                }
            }
        }

        if (resetNeeded)
        {
            resetToDefault();
        }
    }
}

void Sniffer::resetToDefault()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_externalServiceLogging)
    {
        m_externalServiceLogging = false;
        printf("Timeout: Resetting Logging to OFF\n");
    }
}

} // namespace sniffer
