package elazarkin.ksg.external.service.fragments;

import android.app.AlertDialog;
import android.os.Bundle;
import android.view.GestureDetector;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.PopupMenu;
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
            binding.mockTableContainer.removeAllViews();
            Toast.makeText(getContext(), "Sniffer Cleared", Toast.LENGTH_SHORT).show();
        });

        // Setup interaction for Mock Row 1
        setupMockRow(binding.mockRow1);
        
        // Setup interaction for Mock Row 2
        setupMockRow(binding.mockRow2);
    }

    private void setupMockRow(View row) {
        // Long click for Context Menu (Protocol, PID, Presets, Mapping)
        row.setOnLongClickListener(v -> {
            showContextMenu(v);
            return true;
        });

        // Double tap for Decoder Editor
        final GestureDetector gestureDetector = new GestureDetector(getContext(), new GestureDetector.SimpleOnGestureListener() {
            @Override
            public boolean onDoubleTap(@NonNull MotionEvent e) {
                showDecoderEditorDialog();
                return true;
            }
            
            @Override
            public boolean onSingleTapConfirmed(@NonNull MotionEvent e) {
                // Just for visual feedback
                row.setSelected(true);
                return true;
            }
        });

        row.setOnTouchListener((v, event) -> {
            gestureDetector.onTouchEvent(event);
            return false; // Let it propagate to long click
        });
    }

    private void showContextMenu(View view) {
        PopupMenu popup = new PopupMenu(getContext(), view);
        popup.getMenuInflater().inflate(R.menu.sniffer_context_menu, popup.getMenu());
        
        popup.setOnMenuItemClickListener(item -> {
            int itemId = item.getItemId();
            if (itemId == R.id.menu_has_pid) {
                Toast.makeText(getContext(), "Toggle Has PID (Mock)", Toast.LENGTH_SHORT).show();
                return true;
            } else if (itemId == R.id.menu_set_pid_idx) {
                Toast.makeText(getContext(), "Set PID Index Dialog (Mock)", Toast.LENGTH_SHORT).show();
                return true;
            } else if (itemId == R.id.preset_rpm || itemId == R.id.preset_speed) {
                Toast.makeText(getContext(), "Preset Applied (Mock)", Toast.LENGTH_SHORT).show();
                return true;
            } else if (itemId == R.id.map_steering) {
                Toast.makeText(getContext(), "Mapped to Steering (Mock)", Toast.LENGTH_SHORT).show();
                return true;
            }
            return false;
        });
        
        popup.show();
    }

    private void showDecoderEditorDialog() {
        View dialogView = LayoutInflater.from(getContext()).inflate(R.layout.dialog_decoder_editor, null);
        
        new AlertDialog.Builder(getContext())
                .setView(dialogView)
                .setPositiveButton("Save & Apply", (dialog, which) -> {
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
