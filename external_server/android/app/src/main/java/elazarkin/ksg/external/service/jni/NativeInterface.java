package elazarkin.ksg.external.service.jni;

public class NativeInterface
{
    static
    {
        System.loadLibrary("service");
    }

    public static native long clientCreate(String ip, int remotePort, int localPort, int keepAliveMs);

    public static native int clientStart(long handle);

    public static native void clientStop(long handle);

    public static native void clientDestroy(long handle);

    public static native boolean clientIsConnected(long handle);

    public static native String clientGetSnifferStatus(long handle);

    public static native byte[] clientReadMessage(long handle, int timeoutMs);
}
