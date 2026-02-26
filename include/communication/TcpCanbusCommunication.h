#ifndef TCP_CANBUS_COMMUNICATION_H
#define TCP_CANBUS_COMMUNICATION_H

#include "communication/TcpCommunication.h"
#include "communication/CanbusProtocol.h"
#include <string.h>

namespace communication
{

class TcpCanbusCommunication : public TcpCommunication, public base::ICommunicationListener
{
public:
    /**
     * @param listener The listener for CANBUS_DATA events (the "normal" traffic).
     * @param ip The server IP address.
     * @param port The server port.
     */
    TcpCanbusCommunication(base::ICommunicationListener& listener, const char* ip, uint16_t port);

    virtual ~TcpCanbusCommunication();

    /**
     * Sets the listener for non-CAN commands (e.g. configuration, injection).
     */
    void setCommandListener(ICommandListener* commandListener);

protected:
    // --- ICommunicationListener Implementation (Internal) ---
    // This class listens to its own base TcpCommunication to parse the protocol.
    virtual void onDataReceived(const uint8_t* data, size_t length) override;
    virtual void onError(int32_t errorCode) override;

private:
    void processBuffer(const uint8_t* data, size_t length);

    base::ICommunicationListener& m_targetListener; // The external listener for CAN data
    ICommandListener* m_commandListener;            // Optional listener for other commands
};

} // namespace communication

#endif // TCP_CANBUS_COMMUNICATION_H
