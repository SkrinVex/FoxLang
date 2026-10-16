package org.foxlang.android;

import android.content.Context;
import android.view.MotionEvent;
import android.widget.Button;
import android.widget.LinearLayout;
import java.util.HashSet;

/** Optional touch controls. Directions send arrow keys (or WASD); independent
 * buttons support holding several keys. Leaving the view releases every key. */
public class FoxControls extends LinearLayout {
    private final FoxLang engine;
    private final HashSet<Integer> held = new HashSet<>();
    public FoxControls(Context context, FoxLang engine, FoxView view) {
        this(context, engine, view, false);
    }
    public FoxControls(Context context, FoxLang engine, FoxView view, boolean wasd) {
        super(context); this.engine = engine;
        setOrientation(HORIZONTAL); setMotionEventSplittingEnabled(true);
        key("◀", "Left", wasd ? 65 : 37);
        key("▲", "Up", wasd ? 87 : 38);
        key("▼", "Down", wasd ? 83 : 40);
        key("▶", "Right", wasd ? 68 : 39);
        key("␣", "Space", 32);
        Button keyboard = button("⌨", "Keyboard");
        keyboard.setOnClickListener(v -> view.showKeyboard());
    }
    private Button button(String label, String description) {
        Button button = new Button(getContext());
        button.setText(label); button.setContentDescription(description);
        button.setMinWidth(0); button.setMinimumWidth(0); button.setPadding(0,0,0,0);
        button.setFocusable(false); button.setLongClickable(false); button.setTextIsSelectable(false);
        addView(button, new LayoutParams(0, Math.round(48 * getResources().getDisplayMetrics().density), 1));
        return button;
    }
    private void key(String label, String description, int... keys) {
        Button button = button(label, description);
        button.setOnTouchListener((v, event) -> {
            int action = event.getActionMasked();
            if(action == MotionEvent.ACTION_DOWN) {
                v.setPressed(true);
                for(int code: keys) { held.add(code); engine.key(code, true); }
            } else if(action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
                v.setPressed(false);
                for(int code: keys) { held.remove(code); engine.key(code, false); }
            }
            return true;
        });
        // TalkBack and keyboard activation generate a complete key press.
        button.setOnClickListener(v -> {
            for(int code: keys) { engine.key(code, true); engine.key(code, false); }
        });
    }
    public void releaseKeys() {
        for(int code: held) engine.key(code, false);
        held.clear();
        for(int i=0;i<getChildCount();i++)getChildAt(i).setPressed(false);
    }
    @Override public void onWindowFocusChanged(boolean focused) { super.onWindowFocusChanged(focused); if(!focused)releaseKeys(); }
    @Override protected void onDetachedFromWindow() { releaseKeys(); super.onDetachedFromWindow(); }
}
