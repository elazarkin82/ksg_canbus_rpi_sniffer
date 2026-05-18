package elazarkin.ksg.external.service.jni;

public class NativeInterface
{
    static
    {
        System.loadLibrary("service");
    }

    // --- Protocol Constants (from CanbusProtocol.h) ---

    // Command IDs (for V1 protocol)
    public static final int CMD_CANBUS_DATA = 0x1001;      // Normal traffic (Sniffer)
    public static final int CMD_CANBUS_TO_SYSTEM = 0x1002; // Inject to System CAN
    public static final int CMD_CANBUS_TO_CAR = 0x1003;    // Inject to Car CAN
    public static final int CMD_EXTERNAL_SERVICE_LOGGING_ON = 0x1004;
    public static final int CMD_EXTERNAL_SERVICE_LOGGING_OFF = 0x1005;
    public static final int CMD_SET_FILTERS = 0x1006;      // Set/Replace all filter rules
    public static final int CMD_SET_PARAMS = 0x1007;       // Set configuration parameters
    public static final int CMD_KEEP_ALIVE_TO_SNIFFER = 0x1008;
    public static final int CMD_GET_PARAMS_REQ = 0x1009;   // Request current parameters

    // Log Commands (From Sniffer to External Server)
    public static final int CMD_CAN_MSG_FROM_SYSTEM = 0x2001; // Log: Message originated from System
    public static final int CMD_CAN_MSG_FROM_COMPUTER = 0x2002; // Log: Message originated from Computer
    public static final int CMD_CAN_MSG_BLOCKED_FROM_SYSTEM = 0x2003; // Log: Blocked message from System
    public static final int CMD_CAN_MSG_BLOCKED_FROM_COMPUTER = 0x2004; // Log: Blocked message from Computer
    public static final int CMD_KEEP_ALIVE_FROM_SNIFFER = 0x2005;
    public static final int CMD_GET_PARAMS_RES = 0x2006;   // Response with current parameters

    // Filter Target Constants
    public static final int FILTER_TARGET_BOTH = 0;
    public static final int FILTER_TARGET_TO_SYSTEM = 1;
    public static final int FILTER_TARGET_TO_CAR = 2;

    public static class Message
    {
        public int command;
        public double timeMs;
        public byte[] data;

        public Message(int command, double timeMs, byte[] data)
        {
            this.command = command;
            this.timeMs = timeMs;
            this.data = data;
        }
    }

    public static native long clientCreate(String ip, int remotePort, int localPort, int keepAliveMs);

    public static native int clientStart(long handle);

    public static native void clientStop(long handle);

    public static native void clientDestroy(long handle);

    public static native boolean clientIsConnected(long handle);

    public static native String clientGetSnifferStatus(long handle);

    /**
     * Reads a message from the client.
     * Unidirectional JNI: Java provides buffers, Native fills them.
     *
     * @param handle native handle
     * @param cmd output array for command ID [1]
     * @param timeMs output array for timestamp [1]
     * @param data output buffer for payload [65536]
     * @param dataLen output array for actual data length [1]
     * @param timeoutMs read timeout
     * @return 1 if message received, 0 on timeout, negative on error
     */
    public static native int clientReadMessage(long handle, int[] cmd, double[] timeMs, byte[] data, int[] dataLen, int timeoutMs);

    public static native void clientSendRawCommand(long handle, int commandId, byte[] payload);
}
