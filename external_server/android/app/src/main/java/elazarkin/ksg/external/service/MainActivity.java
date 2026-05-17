package elazarkin.ksg.external.service;

import android.os.Bundle;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;
import androidx.navigation.NavController;
import androidx.navigation.fragment.NavHostFragment;
import androidx.navigation.ui.AppBarConfiguration;
import androidx.navigation.ui.NavigationUI;
import elazarkin.ksg.external.service.databinding.ActivityMainBinding;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("service");
    }

    private ActivityMainBinding binding;
    private AppBarConfiguration appBarConfiguration;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        setSupportActionBar(binding.toolbar);

        NavHostFragment navHostFragment = (NavHostFragment) getSupportFragmentManager()
                .findFragmentById(R.id.nav_host_fragment);
        if (navHostFragment != null) {
            NavController navController = navHostFragment.getNavController();

            // Order matches the Drawer menu for consistency
            appBarConfiguration = new AppBarConfiguration.Builder(
                    R.id.nav_connection, R.id.nav_sniffer, R.id.nav_records,
                    R.id.nav_rules, R.id.nav_obd, R.id.nav_settings_remote, 
                    R.id.nav_settings_local, R.id.nav_cockpit)
                    .setOpenableLayout(binding.drawerLayout)
                    .build();

            NavigationUI.setupActionBarWithNavController(this, navController, appBarConfiguration);
            NavigationUI.setupWithNavController(binding.navView, navController);
        }

        binding.btnPanic.setOnClickListener(v -> {
            // TODO: Trigger Global Reset command via Native Layer
            Toast.makeText(this, "PANIC: Sending Reset Command!", Toast.LENGTH_SHORT).show();
        });

        binding.statusLed.setBackgroundColor(getResources().getColor(android.R.color.holo_red_dark, getTheme()));
    }

    @Override
    public boolean onSupportNavigateUp() {
        NavHostFragment navHostFragment = (NavHostFragment) getSupportFragmentManager()
                .findFragmentById(R.id.nav_host_fragment);
        if (navHostFragment != null) {
            NavController navController = navHostFragment.getNavController();
            return NavigationUI.navigateUp(navController, appBarConfiguration)
                    || super.onSupportNavigateUp();
        }
        return super.onSupportNavigateUp();
    }

    public native String stringFromJNI();
}
