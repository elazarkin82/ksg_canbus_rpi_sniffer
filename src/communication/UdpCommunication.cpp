#include "communication/UdpCommunication.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

namespace communication
{

UdpCommunication::UdpCommunication(base::ICommunicationListener& listener, const char* ip, uint16_t remotePort, uint16_t localPort, size_t bufferSize)
    : base::CommunicationObj(listener, bufferSize),
      m_remotePort(remotePort),
      m_localPort(localPort),
      m_socketFd(-1),
      m_hasLastSender(false)
{
    memset(m_ip, 0, sizeof(m_ip));
    if (ip)
    {
        strncpy(m_ip, ip, sizeof(m_ip) - 1);
    }
    memset(&m_lastSender, 0, sizeof(m_lastSender));
}

UdpCommunication::~UdpCommunication()
{
    close();
}

int32_t UdpCommunication::open()
{
    struct sockaddr_in localAddr;

    if (m_socketFd >= 0)
    {
        return 0; // Already open
    }

    m_socketFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socketFd < 0)
    {
        fprintf(stderr, "[UdpCommunication] socket() failed: %s\n", strerror(errno));
        return -1;
    }

    // Bind to local port
    memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = htons(m_localPort);

    if (::bind(m_socketFd, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0)
    {
        fprintf(stderr, "[UdpCommunication] bind() failed on port %d: %s\n", m_localPort, strerror(errno));
        ::close(m_socketFd);
        m_socketFd = -1;
        return -1;
    }

    return 0;
}

void UdpCommunication::close()
{
    if (m_socketFd >= 0)
    {
        ::close(m_socketFd);
        m_socketFd = -1;
    }
}

int32_t UdpCommunication::read(uint8_t* buffer, size_t maxLen)
{
    ssize_t bytesRead = 0;
    struct sockaddr_in senderAddr;
    socklen_t addrLen = sizeof(senderAddr);

    if (m_socketFd < 0)
    {
        return -1;
    }

    bytesRead = ::recvfrom(m_socketFd, buffer, maxLen, 0, (struct sockaddr*)&senderAddr, &addrLen);

    if (bytesRead > 0)
    {
        // Store sender address for reply
        memcpy(&m_lastSender, &senderAddr, sizeof(senderAddr));
        m_hasLastSender = true;

        return (int32_t)bytesRead;
    }
    else if (bytesRead == 0)
    {
        // Should not happen for UDP usually, but possible
        return 0;
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

int32_t UdpCommunication::write(const uint8_t* data, size_t length)
{
    ssize_t bytesWritten = 0;
    struct sockaddr_in destAddr;

    if (m_socketFd < 0)
    {
        return -1;
    }

    if (m_remotePort != 0)
    {
        // Use configured remote address
        memset(&destAddr, 0, sizeof(destAddr));
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(m_remotePort);
        if (inet_pton(AF_INET, m_ip, &destAddr.sin_addr) <= 0)
        {
            return -1;
        }
    }
    else
    {
        // Use last sender address (Dynamic mode)
        if (m_hasLastSender)
        {
            memcpy(&destAddr, &m_lastSender, sizeof(destAddr));
        }
        else
        {
            // No sender yet, cannot reply
            return 0; // Or -1? 0 means "nothing sent but no error"
        }
    }

    bytesWritten = ::sendto(m_socketFd, data, length, 0, (struct sockaddr*)&destAddr, sizeof(destAddr));

    if (bytesWritten >= 0)
    {
        return (int32_t)bytesWritten;
    }
    else
    {
        return -1;
    }
}

void UdpCommunication::unblock()
{
    if (m_socketFd >= 0)
    {
        // Shutdown read/write to force recvfrom() to return immediately
        ::shutdown(m_socketFd, SHUT_RDWR);
    }
}

} // namespace communication
