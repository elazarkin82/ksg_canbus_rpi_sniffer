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

public class SystemLogsFragment extends Fragment
{
    private FragmentSystemLogsBinding binding;
    private ExternalServerService serverService;
    private boolean isBound = false;
    private final SystemCallbackImpl systemCallback = new SystemCallbackImpl();

    private final ServiceConnection serviceConnection = new ServiceConnection()
    {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service)
        {
            ExternalServerService.LocalBinder binder;
            binder = (ExternalServerService.LocalBinder) service;
            serverService = binder.getService();
            isBound = true;

            serverService.setSystemCallback(systemCallback);

            // Populate existing logs using the new getSystemLogs(int max_len)
            binding.logConsole.setText(serverService.getSystemLogs(500));
            
            // Scroll to bottom
            binding.logScroll.post(() ->
            {
                if (binding != null)
                {
                    binding.logScroll.fullScroll(View.FOCUS_DOWN);
                }
            });
        }

        @Override
        public void onServiceDisconnected(ComponentName name)
        {
            isBound = false;
            serverService = null;
        }
    };

    private class SystemCallbackImpl implements ExternalServerService.ISystemCallback
    {
        @Override
        public void onSystemLogsUpdated(String log)
        {
            if (getActivity() != null)
            {
                getActivity().runOnUiThread(() ->
                {
                    if (binding != null)
                    {
                        binding.logConsole.append(log + "\n");
                        
                        // Auto-scroll to bottom
                        binding.logScroll.post(() ->
                        {
                            if (binding != null)
                            {
                                binding.logScroll.fullScroll(View.FOCUS_DOWN);
                            }
                        });
                    }
                });
            }
        }
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
    {
        binding = FragmentSystemLogsBinding.inflate(inflater, container, false);
        return binding.getRoot();
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
    {
        super.onViewCreated(view, savedInstanceState);

        Intent intent;
        intent = new Intent(getContext(), ExternalServerService.class);
        requireContext().bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE);
    }

    @Override
    public void onDestroyView()
    {
        if (isBound)
        {
            if (serverService != null)
            {
                serverService.setSystemCallback(null);
            }
            requireContext().unbindService(serviceConnection);
            isBound = false;
        }
        binding = null;
        super.onDestroyView();
    }
}
