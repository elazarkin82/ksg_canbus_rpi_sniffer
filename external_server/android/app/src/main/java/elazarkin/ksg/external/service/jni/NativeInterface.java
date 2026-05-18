package elazarkin.ksg.external.service.jni;

public class NativeInterface
{
    static
    {
        System.loadLibrary("service");
    }

    // --- Protocol Constants ---
    public static final int CMD_GET_PARAMS_REQ = 0x1009;
    public static final int CMD_SET_PARAMS = 0x1007;
    public static final int CMD_GET_PARAMS_RES = 0x2006;

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

    public static native Message clientReadMessage(long handle, int timeoutMs);

    public static native void clientSendRawCommand(long handle, int commandId, byte[] payload);
}
