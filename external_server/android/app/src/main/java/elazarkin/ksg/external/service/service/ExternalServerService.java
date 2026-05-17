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

public class ExternalServerService extends Service {
    private static final String CHANNEL_ID = "ExternalServerChannel";
    private static final int NOTIFICATION_ID = 1;

    private final IBinder binder = new LocalBinder();
    private long clientHandle = 0;
    private boolean isRunning = false;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private StatusListener statusListener;
    private final List<String> systemLogs = new ArrayList<>();
    
    public interface StatusListener {
        void onStatusUpdate(String status);
        void onConnectionStateChanged(boolean connected);
        void onLogReceived(String log);
    }

    public class LocalBinder extends Binder {
        public ExternalServerService getService() {
            return ExternalServerService.this;
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        createNotificationChannel();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Notification notification = createNotification("Service Running");
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(NOTIFICATION_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
        } else {
            startForeground(NOTIFICATION_ID, notification);
        }
        return START_STICKY;
    }

    public void connect(String ip, int remotePort, int localPort) {
        if (clientHandle != 0) return;

        addLog("[INFO] Attempting connection to " + ip + ":" + remotePort);
        clientHandle = NativeInterface.clientCreate(ip, remotePort, localPort, 500);
        if (clientHandle != 0) {
            NativeInterface.clientStart(clientHandle);
            isRunning = true;
            startStatusPolling();
        } else {
            addLog("[ERROR] Failed to create native client handle");
        }
    }

    public void disconnect() {
        isRunning = false;
        if (clientHandle != 0) {
            NativeInterface.clientStop(clientHandle);
            NativeInterface.clientDestroy(clientHandle);
            clientHandle = 0;
            addLog("[INFO] Native client stopped and destroyed");
        }
        if (statusListener != null) {
            statusListener.onConnectionStateChanged(false);
        }
    }

    public void setStatusListener(StatusListener listener) {
        this.statusListener = listener;
    }

    public List<String> getSystemLogs() {
        return new ArrayList<>(systemLogs);
    }

    public void addLog(String log) {
        systemLogs.add(log);
        if (statusListener != null) {
            statusListener.onLogReceived(log);
        }
    }

    private void startStatusPolling() {
        if (!isRunning) return;

        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (!isRunning || clientHandle == 0) return;

                boolean connected = NativeInterface.clientIsConnected(clientHandle);
                String status = NativeInterface.clientGetSnifferStatus(clientHandle);

                if (statusListener != null) {
                    statusListener.onConnectionStateChanged(connected);
                    if (status != null) {
                        statusListener.onStatusUpdate(status);
                    }
                }

                handler.postDelayed(this, 500);
            }
        }, 500);
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel serviceChannel = new NotificationChannel(
                    CHANNEL_ID,
                    "External Server Service Channel",
                    NotificationManager.IMPORTANCE_LOW
            );
            NotificationManager manager = getSystemService(NotificationManager.class);
            if (manager != null) {
                manager.createNotificationChannel(serviceChannel);
            }
        }
    }

    private Notification createNotification(String content) {
        return new NotificationCompat.Builder(this, CHANNEL_ID)
                .setContentTitle("KSG CAN Sniffer")
                .setContentText(content)
                .setSmallIcon(android.R.drawable.ic_menu_share)
                .build();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return binder;
    }

    @Override
    public void onDestroy() {
        disconnect();
        super.onDestroy();
    }
}
