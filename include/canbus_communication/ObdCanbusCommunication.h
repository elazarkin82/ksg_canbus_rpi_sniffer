#ifndef OBD_CANBUS_COMMUNICATION_H
#define OBD_CANBUS_COMMUNICATION_H

#include "base/CommunicationObj.hpp"
#include <string>
#include <linux/can.h>
#include <linux/can/raw.h>

namespace canbus_communication
{

class ObdCanbusCommunication : public base::CommunicationObj
{
public:
    /**
     * @param listener The listener for incoming events.
     * @param interfaceName The CAN interface name (e.g., "can0", "vcan0").
     * @param bufferSize Size of the internal ring buffer (default 64KB).
     */
    ObdCanbusCommunication(base::ICommunicationListener& listener, const std::string& interfaceName, size_t bufferSize = 64 * 1024);

    virtual ~ObdCanbusCommunication();

protected:
    // --- CommunicationObj Implementation ---
    virtual int32_t open();
    virtual void close();
    virtual int32_t read(uint8_t* buffer, size_t maxLen);
    virtual int32_t write(const uint8_t* data, size_t length);
    virtual void unblock();
    virtual size_t getMaxFrameSize() const;

private:
    std::string m_interfaceName;
    int m_socketFd;
};

} // namespace canbus_communication

#endif // OBD_CANBUS_COMMUNICATION_H
