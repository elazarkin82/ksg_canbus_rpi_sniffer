#ifndef UDP_CANBUS_COMMUNICATION_H
#define UDP_CANBUS_COMMUNICATION_H

#include "communication/UdpCommunication.h"
#include "communication/CanbusProtocol.h"
#include <string.h>

namespace communication
{

class UdpCanbusCommunication : public UdpCommunication, public base::ICommunicationListener
{
public:
    /**
     * @param listener The listener for CANBUS_DATA events.
     * @param ip The remote IP address.
     * @param remotePort The remote port.
     * @param localPort The local port to bind to.
     */
    UdpCanbusCommunication(base::ICommunicationListener& listener, const char* ip, uint16_t remotePort, uint16_t localPort);

    virtual ~UdpCanbusCommunication();

    void setCommandListener(ICommandListener* commandListener);

protected:
    // --- ICommunicationListener Implementation (Internal) ---
    virtual void onDataReceived(const uint8_t* data, size_t length) override;
    virtual void onError(int32_t errorCode) override;
    virtual void onStatusChanged(base::CommunicationStatus status) override;

    // Override write to add debug prints
    virtual int32_t write(const uint8_t* data, size_t length) override;

private:
    void processPacket(const uint8_t* data, size_t length);

    base::ICommunicationListener& m_targetListener;
    ICommandListener* m_commandListener;
};

} // namespace communication

#endif // UDP_CANBUS_COMMUNICATION_H