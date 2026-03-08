#include "canbus_communication/ObdCanbusCommunication.h"

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

namespace canbus_communication
{

ObdCanbusCommunication::ObdCanbusCommunication(base::ICommunicationListener& listener, const char* interfaceName, size_t bufferSize)
    : base::CommunicationObj(listener, bufferSize),
      m_socketFd(-1)
{
    memset(m_interfaceName, 0, sizeof(m_interfaceName));
    if (interfaceName)
    {
        strncpy(m_interfaceName, interfaceName, sizeof(m_interfaceName) - 1);
    }
}

ObdCanbusCommunication::~ObdCanbusCommunication()
{
    close();
}

int32_t ObdCanbusCommunication::open()
{
    struct sockaddr_can addr;
    struct ifreq ifr;
    int enable_canfd = 1;
    can_err_mask_t err_mask = CAN_ERR_MASK;

    if (m_socketFd >= 0)
    {
        return 0; // Already open
    }

    m_socketFd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (m_socketFd < 0)
    {
        return -1;
    }

    // Enable CAN FD support (optional, but good for future proofing)
    // setsockopt(m_socketFd, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enable_canfd, sizeof(enable_canfd));

    // Enable Error Frames
    setsockopt(m_socketFd, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &err_mask, sizeof(err_mask));

    // Get interface index
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, m_interfaceName, IFNAMSIZ - 1);
    if (ioctl(m_socketFd, SIOCGIFINDEX, &ifr) < 0)
    {
        ::close(m_socketFd);
        m_socketFd = -1;
        return -1;
    }

    // Bind to interface
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(m_socketFd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        ::close(m_socketFd);
        m_socketFd = -1;
        return -1;
    }

    fprintf(stderr, "[ObdCanbusCommunication] Successfully bound to interface '%s'\n", m_interfaceName);
    return 0;
}

void ObdCanbusCommunication::close()
{
    if (m_socketFd >= 0)
    {
        ::close(m_socketFd);
        m_socketFd = -1;
    }
}

int32_t ObdCanbusCommunication::read(uint8_t* buffer, size_t maxLen)
{
    ssize_t bytesRead = 0;

    if (m_socketFd < 0)
    {
        return -1;
    }

    bytesRead = ::read(m_socketFd, buffer, maxLen);

    if (bytesRead > 0)
    {
#ifdef DEBUG_MSG
        if (bytesRead >= sizeof(struct can_frame))
        {
            struct can_frame* frame = (struct can_frame*)buffer;
            fprintf(stderr, "[CANBUS RX] Interface: %s, ID: 0x%X\n", m_interfaceName, frame->can_id);
        }
#endif
        return (int32_t)bytesRead;
    }
    else if (bytesRead == 0)
    {
        // Should not happen on CAN socket unless interface goes down
        return -1;
    }
    else
    {
        // Error
        if (errno == EINTR)
        {
            return 0; // Retry
        }
        return -1;
    }
}

int32_t ObdCanbusCommunication::write(const uint8_t* data, size_t length)
{
    ssize_t bytesWritten = 0;

    if (m_socketFd < 0)
    {
        return -1;
    }

#ifdef DEBUG_MSG
    if (length >= sizeof(struct can_frame))
    {
        const struct can_frame* frame = (const struct can_frame*)data;
        fprintf(stderr, "[CANBUS TX] Interface: %s, ID: 0x%X\n", m_interfaceName, frame->can_id);
    }
#endif

    bytesWritten = ::write(m_socketFd, data, length);

    if (bytesWritten >= 0)
    {
        return (int32_t)bytesWritten;
    }
    else
    {
        return -1;
    }
}

void ObdCanbusCommunication::unblock()
{
    if (m_socketFd >= 0)
    {
        // Shutdown read/write to force read() to return immediately
        ::shutdown(m_socketFd, SHUT_RDWR);
    }
}

size_t ObdCanbusCommunication::getMaxFrameSize() const
{
    // Return max size for CAN FD frame + padding
    // struct canfd_frame is 72 bytes.
    return sizeof(struct canfd_frame) + 16;
}

} // namespace canbus_communication
