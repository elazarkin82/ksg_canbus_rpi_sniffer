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
import elazarkin.ksg.external.service.R;
import elazarkin.ksg.external.service.databinding.FragmentConnectionBinding;
import elazarkin.ksg.external.service.service.ExternalServerService;

public class ConnectionFragment extends Fragment implements ExternalServerService.StatusListener
{
    private FragmentConnectionBinding binding;
    private ExternalServerService serverService;
    private boolean isBound = false;

    private final ServiceConnection serviceConnection = new ServiceConnection()
    {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service)
        {
            ExternalServerService.LocalBinder binder;
            binder = (ExternalServerService.LocalBinder) service;
            serverService = binder.getService();
            isBound = true;

            // Set listener and get immediate sync
            serverService.setStatusListener(ConnectionFragment.this);
            
            // Explicitly sync UI based on current service state
            updateUiFromService();
        }

        @Override
        public void onServiceDisconnected(ComponentName name)
        {
            isBound = false;
            serverService = null;
        }
    };

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
    {
        binding = FragmentConnectionBinding.inflate(inflater, container, false);
        return binding.getRoot();
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
    {
        super.onViewCreated(view, savedInstanceState);

        binding.btnConnect.setOnClickListener(v ->
        {
            if (isBound && serverService != null)
            {
                // Rely on Service state instead of button text
                if (!serverService.isClientActive())
                {
                    String ip;
                    int remotePort;
                    int localPort;

                    ip = binding.editIp.getText().toString();
                    remotePort = Integer.parseInt(binding.editRemotePort.getText().toString());
                    localPort = Integer.parseInt(binding.editLocalPort.getText().toString());

                    serverService.connect(ip, remotePort, localPort);
                    binding.btnConnect.setText("Disconnect");
                }
                else
                {
                    serverService.disconnect();
                    binding.btnConnect.setText("Connect");
                    binding.rpiStatusText.setText("Disconnected");
                }
            }
        });

        // Start and bind to the service to ensure it survives fragment replacement
        Intent intent;
        intent = new Intent(getContext(), ExternalServerService.class);
        requireContext().startService(intent);
        requireContext().bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE);
    }

    private void updateUiFromService()
    {
        if (binding == null || serverService == null)
        {
            return;
        }

        if (serverService.isClientActive())
        {
            binding.btnConnect.setText("Disconnect");
            binding.rpiStatusText.setText(serverService.getLastStatus());
        }
        else
        {
            binding.btnConnect.setText("Connect");
            binding.rpiStatusText.setText("Disconnected");
        }
    }

    @Override
    public void onStatusUpdate(String status)
    {
        if (getActivity() != null)
        {
            getActivity().runOnUiThread(() ->
            {
                if (binding != null)
                {
                    binding.rpiStatusText.setText(status);
                }
            });
        }
    }

    @Override
    public void onConnectionStateChanged(boolean connected)
    {
        if (getActivity() != null)
        {
            getActivity().runOnUiThread(() ->
            {
                if (binding != null)
                {
                    if (connected)
                    {
                        // Text update handled by onStatusUpdate, but we can set specific color/state here
                    }
                    else
                    {
                        if (serverService != null && serverService.isClientActive())
                        {
                            binding.rpiStatusText.setText("Disconnected / Timeout");
                        }
                        else
                        {
                            binding.rpiStatusText.setText("Disconnected");
                        }
                    }
                }
            });
        }
    }

    @Override
    public void onLogReceived(String log)
    {
        // System logs are handled in SystemLogsFragment
    }

    @Override
    public void onDestroyView()
    {
        if (isBound)
        {
            if (serverService != null)
            {
                serverService.setStatusListener(null);
            }
            requireContext().unbindService(serviceConnection);
            isBound = false;
        }
        binding = null;
        super.onDestroyView();
    }
}
