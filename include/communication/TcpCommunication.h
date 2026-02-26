#ifndef TCP_COMMUNICATION_H
#define TCP_COMMUNICATION_H

#include "base/CommunicationObj.hpp"

namespace communication
{

class TcpCommunication : public base::CommunicationObj
{
public:
    /**
     * @param listener The listener for incoming events.
     * @param ip The server IP address.
     * @param port The server port.
     * @param bufferSize Size of the internal ring buffer (default 64KB).
     */
    TcpCommunication(base::ICommunicationListener& listener, const char* ip, uint16_t port, size_t bufferSize = 64 * 1024);

    virtual ~TcpCommunication();

protected:
    // --- CommunicationObj Implementation ---
    virtual int32_t open();
    virtual void close();
    virtual int32_t read(uint8_t* buffer, size_t maxLen);
    virtual int32_t write(const uint8_t* data, size_t length);
    virtual void unblock();

private:
    char m_ip[16]; // IPv4 address string (e.g., "192.168.100.100")
    uint16_t m_port;
    int m_socketFd;
};

} // namespace communication

#endif // TCP_COMMUNICATION_H
