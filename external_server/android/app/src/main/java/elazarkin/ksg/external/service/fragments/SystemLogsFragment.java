package elazarkin.ksg.external.service.fragments;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import elazarkin.ksg.external.service.databinding.FragmentSystemLogsBinding;
import elazarkin.ksg.external.service.service.ExternalServerService;

public class SystemLogsFragment extends Fragment implements ExternalServerService.StatusListener {

    private FragmentSystemLogsBinding binding;
    private ExternalServerService serverService;
    private boolean isBound = false;

    private final ServiceConnection serviceConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            ExternalServerService.LocalBinder binder = (ExternalServerService.LocalBinder) service;
            serverService = binder.getService();
            serverService.setStatusListener(SystemLogsFragment.this);
            isBound = true;
            
            // Populate existing logs
            StringBuilder sb = new StringBuilder();
            for (String log : serverService.getSystemLogs()) {
                sb.append(log).append("\n");
            }
            binding.logConsole.setText(sb.toString());
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            isBound = false;
        }
    };

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        binding = FragmentSystemLogsBinding.inflate(inflater, container, false);
        return binding.getRoot();
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        
        Intent intent = new Intent(getContext(), ExternalServerService.class);
        requireContext().bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE);
    }

    @Override
    public void onStatusUpdate(String status) {
        // Status updates are handled in ConnectionFragment
    }

    @Override
    public void onConnectionStateChanged(boolean connected) {
        // Connection state handled elsewhere
    }

    @Override
    public void onLogReceived(String log) {
        if (getActivity() != null) {
            getActivity().runOnUiThread(() -> {
                if (binding != null) {
                    String currentText = binding.logConsole.getText().toString();
                    binding.logConsole.setText(currentText + log + "\n");
                }
            });
        }
    }

    @Override
    public void onDestroyView() {
        if (isBound) {
            if (serverService != null) {
                serverService.setStatusListener(null);
            }
            requireContext().unbindService(serviceConnection);
            isBound = false;
        }
        binding = null;
        super.onDestroyView();
    }
}
