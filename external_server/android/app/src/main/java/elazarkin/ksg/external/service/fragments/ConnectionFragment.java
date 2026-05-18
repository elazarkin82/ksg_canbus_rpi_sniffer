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

public class ConnectionFragment extends Fragment
{
    private FragmentConnectionBinding binding;
    private ExternalServerService serverService;
    private boolean isBound = false;
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
            updateUi(() ->
            {
                binding.btnConnect.setText("Connecting...");
            });
        }

        @Override
        public void onConnected()
        {
            updateUi(() ->
            {
                binding.btnConnect.setText("Disconnect");
            });
        }

        @Override
        public void onConnectionLost()
        {
            updateUi(() ->
            {
                binding.btnConnect.setText("Connecting...");
            });
        }

        @Override
        public void onDisconnected()
        {
            updateUi(() ->
            {
                binding.btnConnect.setText("Connect");
            });
        }

        @Override
        public void onRpiStatusUpdate(String status)
        {
            updateUi(() ->
            {
                binding.rpiStatusText.setText(status);
            });
        }

        private void updateUi(Runnable runnable)
        {
            if (getActivity() != null)
            {
                getActivity().runOnUiThread(() ->
                {
                    if (binding != null)
                    {
                        runnable.run();
                    }
                });
            }
        }
    }

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
                if (!serverService.isClientActive())
                {
                    String ip;
                    int remotePort;
                    int localPort;

                    try
                    {
                        ip = binding.editIp.getText().toString();
                        remotePort = Integer.parseInt(binding.editRemotePort.getText().toString());
                        localPort = Integer.parseInt(binding.editLocalPort.getText().toString());

                        serverService.connect(ip, remotePort, localPort);
                    }
                    catch (NumberFormatException e)
                    {
                        // Handle invalid port input
                    }
                }
                else
                {
                    serverService.disconnect();
                }
            }
        });

        Intent intent;
        intent = new Intent(getContext(), ExternalServerService.class);
        requireContext().startService(intent);
        requireContext().bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE);
    }

    @Override
    public void onDestroyView()
    {
        if (isBound)
        {
            if (serverService != null)
            {
                serverService.removeConnectionStatusListener(statusListener);
            }
            requireContext().unbindService(serviceConnection);
            isBound = false;
        }
        binding = null;
        super.onDestroyView();
    }
}
