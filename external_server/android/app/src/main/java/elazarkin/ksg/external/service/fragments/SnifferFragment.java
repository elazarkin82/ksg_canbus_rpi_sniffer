package elazarkin.ksg.external.service.fragments;

import android.app.AlertDialog;
import android.os.Bundle;
import android.view.GestureDetector;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import elazarkin.ksg.external.service.R;
import elazarkin.ksg.external.service.databinding.FragmentSnifferBinding;

public class SnifferFragment extends Fragment {

    private FragmentSnifferBinding binding;

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        binding = FragmentSnifferBinding.inflate(inflater, container, false);
        return binding.getRoot();
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        // TODO delete this gui example: Handle Clear button
        binding.btnClearSniffer.setOnClickListener(v -> {
            binding.mockSnifferTable.setText("ID     Prot   PID  Dir    Data\n");
            Toast.makeText(getContext(), "Sniffer Cleared", Toast.LENGTH_SHORT).show();
        });

        // TODO delete this gui example: Double-click detector for the mock table
        final GestureDetector gestureDetector = new GestureDetector(getContext(), new GestureDetector.SimpleOnGestureListener() {
            @Override
            public boolean onDoubleTap(@NonNull MotionEvent e) {
                showDecoderEditorDialog();
                return true;
            }
        });

        binding.mockSnifferTable.setOnTouchListener((v, event) -> gestureDetector.onTouchEvent(event));
    }

    private void showDecoderEditorDialog() {
        View dialogView = LayoutInflater.from(getContext()).inflate(R.layout.dialog_decoder_editor, null);
        
        new AlertDialog.Builder(getContext())
                .setView(dialogView)
                .setPositiveButton("Save", (dialog, which) -> {
                    Toast.makeText(getContext(), "Decoder Saved (Mock)", Toast.LENGTH_SHORT).show();
                })
                .setNegativeButton("Cancel", null)
                .show();
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        binding = null;
    }
}
