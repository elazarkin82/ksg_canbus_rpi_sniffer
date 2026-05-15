#ifndef STATUS_MANAGER_HPP
#define STATUS_MANAGER_HPP

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <string>

namespace utils
{

class StatusManager
{
public:
    static StatusManager& getInstance()
    {
        static StatusManager instance;
        return instance;
    }

    // Prevent copying and assignment
    StatusManager(const StatusManager&) = delete;
    StatusManager& operator=(const StatusManager&) = delete;

    double get_time_ms_from_start()
    {
        uint64_t nowUsec;
        uint64_t diffUsec;
        
        nowUsec = (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
        
        diffUsec = nowUsec - m_startTimeUsec;
        return (double)diffUsec / 1000.0;
    }

    void update_status(const char* key, const char* message_status)
    {
        std::string sKey;
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!key || !message_status)
        {
            return;
        }

        sKey = key;
        // If the key is new, we ensure the string has the requested capacity
        if (m_statusMessages.find(sKey) == m_statusMessages.end())
        {
            m_statusMessages[sKey].reserve(DEFAULT_MSG_CAPACITY);
        }
        
        m_statusMessages[sKey] = message_status;
    }

    void get_status(char* out_status, uint32_t max_size)
    {
        size_t currentLen;
        int written;
        std::unordered_map<std::string, std::string>::iterator it;

        std::lock_guard<std::mutex> lock(m_mutex);

        if (!out_status || max_size == 0)
        {
            return;
        }

        update_available_usb_status();

        out_status[0] = '\0';
        currentLen = 0;

        for (it = m_statusMessages.begin(); it != m_statusMessages.end(); ++it)
        {
            written = snprintf(out_status + currentLen, (size_t)max_size - currentLen, "%s: %s\n", it->first.c_str(), it->second.c_str());
            
            if (written > 0)
            {
                currentLen += (size_t)written;
            }

            if (currentLen >= (size_t)max_size)
            {
                break;
            }
        }
    }

private:
    static const uint32_t DEFAULT_MSG_CAPACITY = 256;

    StatusManager()
    {
        m_startTimeUsec = (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }
    
    ~StatusManager() {}

    void update_available_usb_status()
    {
        DIR* dir;
        struct dirent* ent;
        char usb_list_str[2048];
        size_t usb_list_str_len = 0;

        usb_list_str[0] = '\0';
        dir = opendir("/sys/bus/usb/devices/");
        
        if (dir != NULL)
        {
            while ((ent = readdir(dir)) != NULL)
            {
                // Skip hidden entries
                if (ent->d_name[0] == '.') continue;

                if(usb_list_str_len == 0)
                {
                    usb_list_str_len = snprintf(usb_list_str, sizeof(usb_list_str), "%s", ent->d_name);
                }
                else
                {
                    usb_list_str_len += snprintf(&usb_list_str[usb_list_str_len], sizeof(usb_list_str) - usb_list_str_len, ", %s", ent->d_name);
                }
            }
            closedir(dir);
        }

        if (usb_list_str_len > 0)
        {
            update_status("available_usb", usb_list_str);
        }
        else
        {
            update_status("available_usb", "none");
        }
    }

    uint64_t m_startTimeUsec;
    std::unordered_map<std::string, std::string> m_statusMessages;
    std::mutex m_mutex;
};

} // namespace utils

#endif // STATUS_MANAGER_HPP
