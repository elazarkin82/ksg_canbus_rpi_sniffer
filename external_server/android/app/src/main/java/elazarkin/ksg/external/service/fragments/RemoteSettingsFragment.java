package elazarkin.ksg.external.service.fragments;

import android.app.AlertDialog;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.LinearLayout;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import com.google.android.material.button.MaterialButton;
import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;
import elazarkin.ksg.external.service.R;
import elazarkin.ksg.external.service.jni.NativeInterface;
import elazarkin.ksg.external.service.service.ExternalServerService;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;

public class RemoteSettingsFragment extends Fragment implements ExternalServerService.IConnectionStatusListener, ExternalServerService.IMessageListener
{
    private ExternalServerService service;
    private boolean isBound = false;

    private LinearLayout paramsContainer;
    private MaterialButton btnGet;
    private MaterialButton btnSet;

    private final Map<String, String> originalValues = new HashMap<>();
    private final Map<String, EditText> editTextsMap = new HashMap<>();
    private boolean isDirty = false;

    private final ServiceConnection connection = new ServiceConnection()
    {
        @Override
        public void onServiceConnected(ComponentName name, IBinder binder)
        {
            ExternalServerService.LocalBinder lb = (ExternalServerService.LocalBinder) binder;
            service = lb.getService();
            isBound = true;
            service.addConnectionStatusListener(RemoteSettingsFragment.this);
            service.addMessageListener(RemoteSettingsFragment.this);
            updateButtonStates();
        }

        @Override
        public void onServiceDisconnected(ComponentName name)
        {
            service = null;
            isBound = false;
        }
    };

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
    {
        return inflater.inflate(R.layout.fragment_remote_settings, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
    {
        super.onViewCreated(view, savedInstanceState);

        paramsContainer = view.findViewById(R.id.params_container);
        btnGet = view.findViewById(R.id.btn_get_params);
        btnSet = view.findViewById(R.id.btn_set_params);

        btnGet.setOnClickListener(v -> 
        {
            if (service != null)
            {
                service.sendRawCommand(NativeInterface.CMD_GET_PARAMS_REQ, null);
            }
        });

        btnSet.setOnClickListener(v -> 
        {
            showConfirmDialog();
        });
    }

    @Override
    public void onStart()
    {
        super.onStart();
        Intent intent = new Intent(getActivity(), ExternalServerService.class);
        getActivity().bindService(intent, connection, Context.BIND_AUTO_CREATE);
    }

    @Override
    public void onStop()
    {
        super.onStop();
        if (isBound)
        {
            service.removeConnectionStatusListener(this);
            service.removeMessageListener(this);
            getActivity().unbindService(connection);
            isBound = false;
        }
    }

    @Override
    public void onMessageReceived(NativeInterface.Message msg)
    {
        if (msg.command == NativeInterface.CMD_GET_PARAMS_RES)
        {
            String paramsStr = new String(msg.data, StandardCharsets.UTF_8);
            populateParams(paramsStr);
        }
    }

    private void populateParams(String paramsStr)
    {
        paramsContainer.removeAllViews();
        originalValues.clear();
        editTextsMap.clear();
        isDirty = false;

        String[] lines = paramsStr.split("\n");
        for (String line : lines)
        {
            if (!line.contains("="))
            {
                continue;
            }

            String[] parts = line.split("=", 2);
            String key = parts[0].trim();
            String val = parts[1].trim();

            addParamView(key, val);
            originalValues.put(key, val);
        }
        updateButtonStates();
    }

    private void addParamView(String key, String val)
    {
        TextInputLayout layout = new TextInputLayout(getContext());
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.setMargins(0, 8, 0, 8);
        layout.setLayoutParams(lp);
        layout.setHint(key);

        TextInputEditText editText = new TextInputEditText(getContext());
        editText.setText(val);
        editText.addTextChangedListener(new TextWatcher()
        {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) { }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) { }

            @Override
            public void afterTextChanged(Editable s)
            {
                checkDirty();
            }
        });

        layout.addView(editText);
        paramsContainer.addView(layout);
        editTextsMap.put(key, editText);
    }

    private void checkDirty()
    {
        boolean currentlyDirty = false;
        for (Map.Entry<String, String> entry : originalValues.entrySet())
        {
            EditText et = editTextsMap.get(entry.getKey());
            if (et != null && !et.getText().toString().equals(entry.getValue()))
            {
                currentlyDirty = true;
                break;
            }
        }
        isDirty = currentlyDirty;
        updateButtonStates();
    }

    private void updateButtonStates()
    {
        boolean connected = service != null && service.isClientConnected();
        btnGet.setEnabled(connected);
        btnSet.setEnabled(connected && isDirty);
    }

    private void showConfirmDialog()
    {
        new AlertDialog.Builder(getContext())
                .setTitle("Confirm")
                .setMessage("Applying settings will restart the Sniffer service. Continue?")
                .setPositiveButton("Yes", (dialog, which) -> 
                {
                    sendParams();
                })
                .setNegativeButton("No", null)
                .show();
    }

    private void sendParams()
    {
        StringBuilder sb = new StringBuilder();
        for (Map.Entry<String, EditText> entry : editTextsMap.entrySet())
        {
            sb.append(entry.getKey()).append("=").append(entry.getValue().getText().toString()).append("\n");
        }
        
        byte[] payload = sb.toString().getBytes(StandardCharsets.UTF_8);
        service.sendRawCommand(NativeInterface.CMD_SET_PARAMS, payload);
        
        isDirty = false;
        updateButtonStates();
        
        // After sending, the sniffer restarts, so we might want to clear or just wait for new status
        paramsContainer.removeAllViews();
    }

    // --- IConnectionStatusListener Implementation ---

    @Override
    public void onConnection() { updateButtonStates(); }

    @Override
    public void onConnected() { updateButtonStates(); }

    @Override
    public void onConnectionLost() { updateButtonStates(); }

    @Override
    public void onDisconnected() { updateButtonStates(); }

    @Override
    public void onRpiStatusUpdate(String status) { }
}
