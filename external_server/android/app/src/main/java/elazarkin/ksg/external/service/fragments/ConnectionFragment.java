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
            ExternalServerService.LocalBinder binder = (ExternalServerService.LocalBinder) service;
            serverService = binder.getService();
            serverService.setStatusListener(ConnectionFragment.this);
            isBound = true;
        }

        @Override
        public void onServiceDisconnected(ComponentName name)
        {
            isBound = false;
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
                if (binding.btnConnect.getText().toString().equalsIgnoreCase("Connect"))
                {
                    String ip = binding.editIp.getText().toString();
                    int remotePort = Integer.parseInt(binding.editRemotePort.getText().toString());
                    int localPort = Integer.parseInt(binding.editLocalPort.getText().toString());

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

        // Start and bind to the service
        Intent intent = new Intent(getContext(), ExternalServerService.class);
        requireContext().startService(intent);
        requireContext().bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE);
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
                    if (!connected)
                    {
                        binding.rpiStatusText.setText("Disconnected / Timeout");
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
