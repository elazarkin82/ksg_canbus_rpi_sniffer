package elazarkin.ksg.external.service;

import android.Manifest;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.view.animation.AlphaAnimation;
import android.view.animation.Animation;
import android.widget.Toast;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.navigation.NavController;
import androidx.navigation.fragment.NavHostFragment;
import androidx.navigation.ui.AppBarConfiguration;
import androidx.navigation.ui.NavigationUI;
import elazarkin.ksg.external.service.databinding.ActivityMainBinding;
import elazarkin.ksg.external.service.service.ExternalServerService;

public class MainActivity extends AppCompatActivity
{
    private static final int PERMISSION_REQUEST_CODE = 100;

    static
    {
        System.loadLibrary("service");
    }

    private ActivityMainBinding binding;
    private AppBarConfiguration appBarConfiguration;
    private ExternalServerService serverService;
    private boolean isBound = false;
    private Animation blinkAnimation;
    private final ConnectionStatusListenerImpl statusListener = new ConnectionStatusListenerImpl();

    private final ServiceConnection serviceConnection = new ServiceConnection()
    {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service)
        {
            ExternalServerService.LocalBinder binder;
            binder = (ExternalServerService.LocalBinder) service;
            serverService = binder.getService();
            isBound = true;
            serverService.addConnectionStatusListener(statusListener);
        }

        @Override
        public void onServiceDisconnected(ComponentName name)
        {
            isBound = false;
            serverService = null;
        }
    };

    private class ConnectionStatusListenerImpl implements ExternalServerService.IConnectionStatusListener
    {
        @Override
        public void onConnection()
        {
            runOnUiThread(() -> setLedState(ExternalServerService.ConnectionState.CONNECTING));
        }

        @Override
        public void onConnected()
        {
            runOnUiThread(() -> setLedState(ExternalServerService.ConnectionState.CONNECTED));
        }

        @Override
        public void onConnectionLost()
        {
            runOnUiThread(() -> setLedState(ExternalServerService.ConnectionState.CONNECTION_LOST));
        }

        @Override
        public void onDisconnected()
        {
            runOnUiThread(() -> setLedState(ExternalServerService.ConnectionState.DISCONNECTED));
        }

        @Override
        public void onRpiStatusUpdate(String status)
        {
            // Toolbar status update if needed
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        setSupportActionBar(binding.toolbar);

        checkAndRequestPermissions();

        NavHostFragment navHostFragment = (NavHostFragment) getSupportFragmentManager()
                .findFragmentById(R.id.nav_host_fragment);

        if (navHostFragment != null)
        {
            NavController navController = navHostFragment.getNavController();

            appBarConfiguration = new AppBarConfiguration.Builder(
                    R.id.nav_connection, R.id.nav_sniffer, R.id.nav_records,
                    R.id.nav_rules, R.id.nav_obd, R.id.nav_settings_remote,
                    R.id.nav_settings_local, R.id.nav_system_logs, R.id.nav_cockpit)
                    .setOpenableLayout(binding.drawerLayout)
                    .build();

            NavigationUI.setupActionBarWithNavController(this, navController, appBarConfiguration);
            NavigationUI.setupWithNavController(binding.navView, navController);
        }

        binding.btnPanic.setOnClickListener(v ->
        {
            Toast.makeText(this, "PANIC: Sending Reset Command!", Toast.LENGTH_SHORT).show();
        });

        initBlinkAnimation();
        
        Intent intent = new Intent(this, ExternalServerService.class);
        bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE);
    }

    private void initBlinkAnimation()
    {
        blinkAnimation = new AlphaAnimation(1.0f, 0.2f);
        blinkAnimation.setDuration(500);
        blinkAnimation.setRepeatMode(Animation.REVERSE);
        blinkAnimation.setRepeatCount(Animation.INFINITE);
    }

    private void setLedState(ExternalServerService.ConnectionState state)
    {
        binding.statusLed.clearAnimation();
        
        switch (state)
        {
            case CONNECTED:
            {
                binding.statusLed.getBackground().setTint(getResources().getColor(android.R.color.holo_green_light, getTheme()));
                break;
            }
            case CONNECTING:
            case CONNECTION_LOST:
            {
                binding.statusLed.getBackground().setTint(getResources().getColor(android.R.color.holo_orange_light, getTheme()));
                binding.statusLed.startAnimation(blinkAnimation);
                break;
            }
            case DISCONNECTED:
            default:
            {
                binding.statusLed.getBackground().setTint(getResources().getColor(android.R.color.holo_red_dark, getTheme()));
                break;
            }
        }
    }

    private void checkAndRequestPermissions()
    {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
        {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS)
                    != PackageManager.PERMISSION_GRANTED)
            {
                ActivityCompat.requestPermissions(this,
                        new String[]{Manifest.permission.POST_NOTIFICATIONS},
                        PERMISSION_REQUEST_CODE);
            }
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults)
    {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);

        if (requestCode == PERMISSION_REQUEST_CODE)
        {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED)
            {
                Toast.makeText(this, "Notification permission granted", Toast.LENGTH_SHORT).show();
            }
            else
            {
                Toast.makeText(this, "Warning: Notifications disabled. Service status won't be visible.", Toast.LENGTH_LONG).show();
            }
        }
    }

    @Override
    public boolean onSupportNavigateUp()
    {
        NavHostFragment navHostFragment = (NavHostFragment) getSupportFragmentManager()
                .findFragmentById(R.id.nav_host_fragment);

        if (navHostFragment != null)
        {
            NavController navController = navHostFragment.getNavController();
            return NavigationUI.navigateUp(navController, appBarConfiguration)
                    || super.onSupportNavigateUp();
        }

        return super.onSupportNavigateUp();
    }

    @Override
    protected void onDestroy()
    {
        if (isBound)
        {
            if (serverService != null)
            {
                serverService.removeConnectionStatusListener(statusListener);
            }
            unbindService(serviceConnection);
            isBound = false;
        }
        super.onDestroy();
    }

    public native String stringFromJNI();
}
