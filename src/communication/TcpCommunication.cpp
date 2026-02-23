#include "communication/TcpCommunication.hpp"

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

TcpCommunication::TcpCommunication(base::ICommunicationListener& listener, const std::string& ip, uint16_t port, size_t bufferSize)
    : base::CommunicationObj(listener, bufferSize),
      m_ip(ip),
      m_port(port),
      m_socketFd(-1)
{
}

TcpCommunication::~TcpCommunication()
{
    close();
}

int32_t TcpCommunication::open()
{
    struct sockaddr_in serverAddr;
    int result = 0;

    if (m_socketFd >= 0)
    {
        return 0; // Already open
    }

    m_socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socketFd < 0)
    {
        return -1;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(m_port);

    if (inet_pton(AF_INET, m_ip.c_str(), &serverAddr.sin_addr) <= 0)
    {
        ::close(m_socketFd);
        m_socketFd = -1;
        return -1;
    }

    result = connect(m_socketFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result < 0)
    {
        ::close(m_socketFd);
        m_socketFd = -1;
        return -1;
    }

    return 0;
}

void TcpCommunication::close()
{
    if (m_socketFd >= 0)
    {
        ::close(m_socketFd);
        m_socketFd = -1;
    }
}

int32_t TcpCommunication::read(uint8_t* buffer, size_t maxLen)
{
    ssize_t bytesRead = 0;

    if (m_socketFd < 0)
    {
        return -1;
    }

    bytesRead = recv(m_socketFd, buffer, maxLen, 0);

    if (bytesRead > 0)
    {
        return (int32_t)bytesRead;
    }
    else if (bytesRead == 0)
    {
        // Connection closed by peer
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

int32_t TcpCommunication::write(const uint8_t* data, size_t length)
{
    ssize_t bytesWritten = 0;

    if (m_socketFd < 0)
    {
        return -1;
    }

    // MSG_NOSIGNAL prevents SIGPIPE if peer disconnects
    bytesWritten = send(m_socketFd, data, length, MSG_NOSIGNAL);

    if (bytesWritten >= 0)
    {
        return (int32_t)bytesWritten;
    }
    else
    {
        return -1;
    }
}

void TcpCommunication::unblock()
{
    if (m_socketFd >= 0)
    {
        // Shutdown read/write to force recv() to return immediately
        shutdown(m_socketFd, SHUT_RDWR);
    }
}

} // namespace communication
