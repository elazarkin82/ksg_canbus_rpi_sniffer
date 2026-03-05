#ifndef UDP_COMMUNICATION_H
#define UDP_COMMUNICATION_H

#include "base/CommunicationObj.hpp"
#include <netinet/in.h>

namespace communication
{

class UdpCommunication : public base::CommunicationObj
{
public:
    /**
     * @param listener The listener for incoming events.
     * @param ip The remote IP address (for sending).
     * @param remotePort The remote port (for sending). If 0, sends to the last received address.
     * @param localPort The local port to bind to (for receiving).
     * @param bufferSize Size of the internal ring buffer (default 64KB).
     */
    UdpCommunication(base::ICommunicationListener& listener, const char* ip, uint16_t remotePort, uint16_t localPort, size_t bufferSize = 64 * 1024);

    virtual ~UdpCommunication();

protected:
    // --- CommunicationObj Implementation ---
    virtual int32_t open();
    virtual void close();
    virtual int32_t read(uint8_t* buffer, size_t maxLen);
    virtual int32_t write(const uint8_t* data, size_t length);
    virtual void unblock();

private:
    char m_ip[16]; // IPv4 address string
    uint16_t m_remotePort;
    uint16_t m_localPort;
    int m_socketFd;

    struct sockaddr_in m_lastSender;
    bool m_hasLastSender;
};

} // namespace communication

#endif // UDP_COMMUNICATION_H
