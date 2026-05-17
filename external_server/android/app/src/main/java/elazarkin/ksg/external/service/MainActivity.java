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

    // Used to load the 'service' library on application startup.
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

        // Setup Navigation
        NavHostFragment navHostFragment = (NavHostFragment) getSupportFragmentManager()
                .findFragmentById(R.id.nav_host_fragment);
        if (navHostFragment != null) {
            NavController navController = navHostFragment.getNavController();

            // Configure which destinations are considered top-level (no back button, show hamburger)
            appBarConfiguration = new AppBarConfiguration.Builder(
                    R.id.nav_dashboard, R.id.nav_sniffer, R.id.nav_records,
                    R.id.nav_cockpit, R.id.nav_rules, R.id.nav_decoder,
                    R.id.nav_obd, R.id.nav_status, R.id.nav_settings)
                    .setOpenableLayout(binding.drawerLayout)
                    .build();

            NavigationUI.setupActionBarWithNavController(this, navController, appBarConfiguration);
            NavigationUI.setupWithNavController(binding.navView, navController);
            
            // Sync Toolbar Title with Navigation (optional, NavigationUI does this by default if setup with ActionBar)
        }

        // Panic Button logic
        binding.btnPanic.setOnClickListener(v -> {
            // TODO: Trigger Global Reset command via Native Layer
            Toast.makeText(this, "PANIC: Sending Reset Command!", Toast.LENGTH_SHORT).show();
        });

        // TODO delete this gui example: Simulated status update
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

    /**
     * A native method that is implemented by the 'service' native library.
     */
    public native String stringFromJNI();
}
