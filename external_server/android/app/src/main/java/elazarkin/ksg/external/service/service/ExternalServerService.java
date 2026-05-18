package elazarkin.ksg.external.service.service;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Binder;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import androidx.core.app.NotificationCompat;
import elazarkin.ksg.external.service.jni.NativeInterface;
import java.util.ArrayList;
import java.util.List;

public class ExternalServerService extends Service
{
    public enum ConnectionState
    {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        CONNECTION_LOST
    }

    private static final String CHANNEL_ID = "ExternalServerChannel";
    private static final int NOTIFICATION_ID = 1;

    private final IBinder binder = new LocalBinder();
    private long clientHandle = 0;
    private boolean isRunning = false;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final List<IConnectionStatusListener> statusListeners = new ArrayList<>();
    private final List<IMessageListener> messageListeners = new ArrayList<>();
    private ISystemCallback systemCallback;
    private final List<String> systemLogs = new ArrayList<>();
    
    private ConnectionState connectionState = ConnectionState.DISCONNECTED;
    private String mLastRpiStatus = "Disconnected";
    private Thread rxThread;

    public interface IConnectionStatusListener
    {
        void onConnection();
        void onConnected();
        void onConnectionLost();
        void onDisconnected();
        void onRpiStatusUpdate(String status);
    }

    public interface IMessageListener
    {
        void onMessageReceived(NativeInterface.Message msg);
    }

    public interface ISystemCallback
    {
        void onSystemLogsUpdated(String log);
    }

    public class LocalBinder extends Binder
    {
        public ExternalServerService getService()
        {
            return ExternalServerService.this;
        }
    }

    @Override
    public void onCreate()
    {
        super.onCreate();
        createNotificationChannel();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId)
    {
        Notification notification;
        notification = createNotification("Service Running");
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q)
        {
            startForeground(NOTIFICATION_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
        }
        else
        {
            startForeground(NOTIFICATION_ID, notification);
        }
        return START_STICKY;
    }

    public void connect(String ip, int remotePort, int localPort)
    {
        if (clientHandle != 0)
        {
            return;
        }

        addLog("[INFO] Attempting connection to " + ip + ":" + remotePort);
        
        connectionState = ConnectionState.CONNECTING;
        notifyConnection();

        clientHandle = NativeInterface.clientCreate(ip, remotePort, localPort, 500);
        if (clientHandle != 0)
        {
            NativeInterface.clientStart(clientHandle);
            isRunning = true;
            startStatusPolling();
            startRxThread();
        }
        else
        {
            addLog("[ERROR] Failed to create native client handle");
            connectionState = ConnectionState.DISCONNECTED;
            notifyDisconnected();
        }
    }

    public void disconnect()
    {
        isRunning = false;
        if (clientHandle != 0)
        {
            NativeInterface.clientStop(clientHandle);
            NativeInterface.clientDestroy(clientHandle);
            clientHandle = 0;
            addLog("[INFO] Native client stopped and destroyed");
        }

        if (rxThread != null)
        {
            try { rxThread.join(500); } catch (InterruptedException e) { }
            rxThread = null;
        }

        connectionState = ConnectionState.DISCONNECTED;
        mLastRpiStatus = "Disconnected";

        notifyDisconnected();
        notifyRpiStatusUpdate(mLastRpiStatus);
    }

    public void sendRawCommand(int commandId, byte[] payload)
    {
        if (clientHandle != 0)
        {
            NativeInterface.clientSendRawCommand(clientHandle, commandId, payload);
        }
    }

    public void addConnectionStatusListener(IConnectionStatusListener listener)
    {
        if (!statusListeners.contains(listener))
        {
            statusListeners.add(listener);
            syncListenerState(listener);
        }
    }

    public void removeConnectionStatusListener(IConnectionStatusListener listener)
    {
        statusListeners.remove(listener);
    }

    public void addMessageListener(IMessageListener listener)
    {
        if (!messageListeners.contains(listener))
        {
            messageListeners.add(listener);
        }
    }

    public void removeMessageListener(IMessageListener listener)
    {
        messageListeners.remove(listener);
    }

    private void syncListenerState(IConnectionStatusListener listener)
    {
        switch (connectionState)
        {
            case CONNECTED:
            {
                listener.onConnected();
                break;
            }
            case CONNECTING:
            {
                listener.onConnection();
                break;
            }
            case CONNECTION_LOST:
            {
                listener.onConnectionLost();
                break;
            }
            case DISCONNECTED:
            default:
            {
                listener.onDisconnected();
                break;
            }
        }
        listener.onRpiStatusUpdate(mLastRpiStatus);
    }

    public void setSystemCallback(ISystemCallback callback)
    {
        this.systemCallback = callback;
    }

    public String getSystemLogs(int max_len)
    {
        StringBuilder sb = new StringBuilder();
        int size = systemLogs.size();
        int start = Math.max(0, size - max_len);
        for (int i = start; i < size; i++)
        {
            sb.append(systemLogs.get(i)).append("\n");
        }
        return sb.toString();
    }

    public List<String> getSystemLogs()
    {
        return new ArrayList<>(systemLogs);
    }

    public void addLog(String log)
    {
        systemLogs.add(log);
        if (systemCallback != null)
        {
            systemCallback.onSystemLogsUpdated(log);
        }
    }

    public boolean isClientActive()
    {
        return connectionState != ConnectionState.DISCONNECTED;
    }

    public boolean isClientConnected()
    {
        return connectionState == ConnectionState.CONNECTED;
    }

    public String getLastRpiStatus()
    {
        return mLastRpiStatus;
    }

    public ConnectionState getConnectionState()
    {
        return connectionState;
    }

    private void startStatusPolling()
    {
        if (!isRunning)
        {
            return;
        }

        handler.postDelayed(new Runnable()
        {
            @Override
            public void run()
            {
                boolean connected;
                String status;

                if (!isRunning || clientHandle == 0)
                {
                    return;
                }

                connected = NativeInterface.clientIsConnected(clientHandle);
                status = NativeInterface.clientGetSnifferStatus(clientHandle);

                updateConnectionState(connected);

                if (status != null && !status.equals(mLastRpiStatus))
                {
                    mLastRpiStatus = status;
                    notifyRpiStatusUpdate(mLastRpiStatus);
                }

                handler.postDelayed(this, 500);
            }
        }, 500);
    }

    private void startRxThread()
    {
        rxThread = new Thread(new Runnable()
        {
            @Override
            public void run()
            {
                byte[] rxBuffer = new byte[65536];
                int[] cmdOut = new int[1];
                double[] timeOut = new double[1];
                int[] lenOut = new int[1];

                while (isRunning && clientHandle != 0)
                {
                    int res = NativeInterface.clientReadMessage(clientHandle, cmdOut, timeOut, rxBuffer, lenOut, 100);
                    if (res > 0)
                    {
                        int length = lenOut[0];
                        byte[] messageData = new byte[length];
                        System.arraycopy(rxBuffer, 0, messageData, 0, length);
                        
                        NativeInterface.Message msg = new NativeInterface.Message(cmdOut[0], timeOut[0], messageData);
                        notifyMessageReceived(msg);
                    }
                }
            }
        });
        rxThread.setName("ServiceRxThread");
        rxThread.start();
    }

    private void updateConnectionState(boolean isCurrentlyConnected)
    {
        ConnectionState newState = connectionState;

        if (isCurrentlyConnected)
        {
            newState = ConnectionState.CONNECTED;
        }
        else
        {
            if (connectionState == ConnectionState.CONNECTED || connectionState == ConnectionState.CONNECTION_LOST)
            {
                newState = ConnectionState.CONNECTION_LOST;
            }
            else
            {
                newState = ConnectionState.CONNECTING;
            }
        }

        if (newState != connectionState)
        {
            connectionState = newState;
            notifyCurrentState();
        }
    }

    private void notifyCurrentState()
    {
        switch (connectionState)
        {
            case CONNECTED:
            {
                notifyConnected();
                break;
            }
            case CONNECTION_LOST:
            {
                notifyConnectionLost();
                break;
            }
            case CONNECTING:
            {
                notifyConnection();
                break;
            }
            case DISCONNECTED:
            {
                notifyDisconnected();
                break;
            }
        }
    }

    private void notifyConnection()
    {
        for (IConnectionStatusListener l : statusListeners)
        {
            l.onConnection();
        }
    }

    private void notifyConnected()
    {
        for (IConnectionStatusListener l : statusListeners)
        {
            l.onConnected();
        }
    }

    private void notifyConnectionLost()
    {
        for (IConnectionStatusListener l : statusListeners)
        {
            l.onConnectionLost();
        }
    }

    private void notifyDisconnected()
    {
        for (IConnectionStatusListener l : statusListeners)
        {
            l.onDisconnected();
        }
    }

    private void notifyRpiStatusUpdate(String status)
    {
        for (IConnectionStatusListener l : statusListeners)
        {
            l.onRpiStatusUpdate(status);
        }
    }

    private void notifyMessageReceived(final NativeInterface.Message msg)
    {
        handler.post(new Runnable()
        {
            @Override
            public void run()
            {
                for (IMessageListener l : messageListeners)
                {
                    l.onMessageReceived(msg);
                }
            }
        });
    }

    private void createNotificationChannel()
    {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
        {
            NotificationChannel serviceChannel;
            NotificationManager manager;

            serviceChannel = new NotificationChannel(
                    CHANNEL_ID,
                    "External Server Service Channel",
                    NotificationManager.IMPORTANCE_LOW
            );
            manager = getSystemService(NotificationManager.class);
            if (manager != null)
            {
                manager.createNotificationChannel(serviceChannel);
            }
        }
    }

    private Notification createNotification(String content)
    {
        return new NotificationCompat.Builder(this, CHANNEL_ID)
                .setContentTitle("KSG CAN Sniffer")
                .setContentText(content)
                .setSmallIcon(android.R.drawable.ic_menu_share)
                .build();
    }

    @Override
    public IBinder onBind(Intent intent)
    {
        return binder;
    }

    @Override
    public void onDestroy()
    {
        disconnect();
        super.onDestroy();
    }
}
