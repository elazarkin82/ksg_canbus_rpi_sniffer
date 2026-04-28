#ifndef LED_CONTROLLER_UTIL_HPP
#define LED_CONTROLLER_UTIL_HPP

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <mutex>

namespace utils
{

class LedControllerUtil
{
public:
    static LedControllerUtil& getInstance()
    {
        static LedControllerUtil instance;
        return instance;
    }

    // Prevent copying and assignment
    LedControllerUtil(const LedControllerUtil&) = delete;
    LedControllerUtil& operator=(const LedControllerUtil&) = delete;

    int takeControl(const char* ledName)
    {
        return writeAttr(ledName, "trigger", "none\n");
    }

    int turnOn(const char* ledName)
    {
        return writeAttr(ledName, "brightness", "1\n");
    }

    int turnOff(const char* ledName)
    {
        return writeAttr(ledName, "brightness", "0\n");
    }

    int setTimer(const char* ledName, int onMs, int offMs)
    {
        char buf[32];

        if (writeAttr(ledName, "trigger", "timer\n") < 0)
        {
            return -1;
        }

        snprintf(buf, sizeof(buf), "%d\n", onMs);
        if (writeAttr(ledName, "delay_on", buf) < 0)
        {
            return -1;
        }

        snprintf(buf, sizeof(buf), "%d\n", offMs);
        if (writeAttr(ledName, "delay_off", buf) < 0)
        {
            return -1;
        }

        return 0;
    }

    int setTrigger(const char* ledName, const char* trigger)
    {
        return writeAttr(ledName, "trigger", trigger);
    }

private:
    LedControllerUtil() {}
    ~LedControllerUtil() {}

    std::mutex m_mutex;

    int writeToFile(const char* path, const char* value)
    {
        int fd;
        size_t len;
        ssize_t written;

        fd = open(path, O_WRONLY | O_CLOEXEC);
        if (fd < 0)
        {
            fprintf(stderr, "LedControllerUtil: open(%s) failed: %s\n", path, strerror(errno));
            return -1;
        }

        len = strlen(value);
        written = write(fd, value, len);

        if (written < 0)
        {
            fprintf(stderr, "LedControllerUtil: write(%s) failed: %s\n", path, strerror(errno));
            close(fd);
            return -1;
        }

        if ((size_t)written != len)
        {
            fprintf(stderr, "LedControllerUtil: write(%s) incomplete\n", path);
            close(fd);
            return -1;
        }

        close(fd);
        return 0;
    }

    int writeAttr(const char* ledName, const char* attr, const char* value)
    {
        char path[256];
        int n;

        if (!ledName || !attr || !value)
        {
            return -1;
        }

        n = snprintf(path, sizeof(path), "/sys/class/leds/%s/%s", ledName, attr);
        if (n < 0 || n >= (int)sizeof(path))
        {
            fprintf(stderr, "LedControllerUtil: path too long for led %s\n", ledName);
            return -1;
        }

        // Lock the mutex before writing to the hardware file
        std::lock_guard<std::mutex> lock(m_mutex);
        return writeToFile(path, value);
    }
};

} // namespace utils

#endif // LED_CONTROLLER_UTIL_HPP