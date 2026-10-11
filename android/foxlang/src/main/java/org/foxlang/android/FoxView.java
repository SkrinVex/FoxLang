package org.foxlang.android;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Build;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.KeyCharacterMap;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import java.io.File;
import java.io.FileOutputStream;
import java.util.ArrayList;
import java.util.function.BiConsumer;
import java.util.function.Consumer;

/** Software graphics/ui host. FoxLang coordinates use logical dp when resizable;
 * fixed-size windows are fitted with letterboxing. Touch and IME feed the same
 * input API as native desktop windows. Call showKeyboard() for text entry. */
public class FoxView extends View implements FoxLang.Host {
    private FoxLang engine;
    private Bitmap frame;
    private final Paint paint = new Paint();
    private final Object frameLock = new Object();
    private int canvasWidth = 1, canvasHeight = 1, pointerId = -1;
    private boolean resizable = true;
    private BiConsumer<String, Boolean> output = (s,e) -> {};
    private Consumer<Boolean> fullscreen = enabled -> {};
    private final ArrayList<MediaPlayer> sounds = new ArrayList<>();
    public FoxView(Context context) { super(context); init(); }
    public FoxView(Context context, AttributeSet attrs) { super(context, attrs); init(); }
    private void init() { setFocusable(true); setFocusableInTouchMode(true); paint.setFilterBitmap(false); }
    public void setEngine(FoxLang engine) { this.engine = engine; }
    /** The listener runs on the interpreter worker, use post() to update Android UI. */
    public void setOutputListener(BiConsumer<String, Boolean> listener) { output = listener; }
    /** Called on the UI thread so the embedding activity can hide its own chrome. */
    public void setFullscreenListener(Consumer<Boolean> listener) { fullscreen = listener; }
    @Override public void onOutput(String text, boolean error) { output.accept(text, error); }
    private Activity activity() {
        Context c = getContext();
        while (c instanceof ContextWrapper) { if(c instanceof Activity) return (Activity)c; c=((ContextWrapper)c).getBaseContext(); }
        return null;
    }
    @Override public void onOpen(int width, int height, String title) {
        synchronized(frameLock) { canvasWidth = width; canvasHeight = height; }
        post(() -> { resizable = true; Activity a = activity(); if(a != null) a.setTitle(title); setVisibility(VISIBLE); requestFocus(); reportSize(); });
    }
    @Override public void onFrame(int[] pixels, int width, int height) {
        synchronized(frameLock) {
            if(frame == null || frame.getWidth() != width || frame.getHeight() != height) {
                // The old bitmap is left to Android's renderer/GC; do not recycle a
                // bitmap which a hardware display list may still reference.
                frame = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
            }
            canvasWidth = width; canvasHeight = height;
            frame.setPixels(pixels, 0, width, 0, 0, width, height);
        }
        postInvalidate();
    }
    private RectF destination() {
        float scale = Math.min(getWidth() / (float)canvasWidth, getHeight() / (float)canvasHeight);
        float w = canvasWidth * scale, h = canvasHeight * scale;
        return new RectF((getWidth()-w)/2, (getHeight()-h)/2, (getWidth()+w)/2, (getHeight()+h)/2);
    }
    @Override protected void onDraw(Canvas canvas) {
        canvas.drawColor(Color.rgb(24,29,36));
        synchronized(frameLock) { if(frame != null) canvas.drawBitmap(frame, null, destination(), paint); }
    }
    @Override public void onClose() { post(() -> { pointerId = -1; onFullscreen(false); hideKeyboard(); }); }
    @Override public void onResizable(boolean enabled) { post(() -> { resizable = enabled; reportSize(); }); }
    private void reportSize() {
        if(engine != null && resizable && getWidth()>0 && getHeight()>0) {
            float density = getResources().getDisplayMetrics().density;
            engine.resize(Math.max(64, Math.min(4096, Math.round(getWidth()/density))), Math.max(64, Math.min(4096, Math.round(getHeight()/density))));
        }
    }
    @Override protected void onSizeChanged(int w,int h,int oldw,int oldh) { super.onSizeChanged(w,h,oldw,oldh); reportSize(); }
    @Override public void onFullscreen(boolean enabled) { post(() -> {
        fullscreen.accept(enabled);
        Activity a = activity(); if(a == null) return;
        if(Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController c = a.getWindow().getInsetsController();
            if(c != null) { c.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE); if(enabled)c.hide(WindowInsets.Type.systemBars()); else c.show(WindowInsets.Type.systemBars()); }
        } else a.getWindow().getDecorView().setSystemUiVisibility(enabled ? SYSTEM_UI_FLAG_FULLSCREEN|SYSTEM_UI_FLAG_HIDE_NAVIGATION|SYSTEM_UI_FLAG_IMMERSIVE_STICKY : SYSTEM_UI_FLAG_VISIBLE);
    }); }
    private void pointer(MotionEvent e, int index) {
        synchronized(frameLock) {
            RectF r = destination();
            if(r.width() > 0 && r.height() > 0) engine.pointer((int)((e.getX(index)-r.left)*canvasWidth/r.width()),(int)((e.getY(index)-r.top)*canvasHeight/r.height()));
        }
    }
    @Override public boolean onTouchEvent(MotionEvent event) {
        if(engine == null) return false;
        int action = event.getActionMasked();
        if(action == MotionEvent.ACTION_DOWN) {
            requestFocus(); if(getParent()!=null)getParent().requestDisallowInterceptTouchEvent(true);
            pointerId = event.getPointerId(0); pointer(event,0); engine.key(1,true); return true;
        }
        int index = event.findPointerIndex(pointerId);
        if(action == MotionEvent.ACTION_MOVE && index >= 0) pointer(event,index);
        if(action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL ||
           (action == MotionEvent.ACTION_POINTER_UP && event.getPointerId(event.getActionIndex()) == pointerId)) {
            if(index >= 0) pointer(event,index);
            engine.key(1,false); pointerId = -1;
            if(getParent()!=null)getParent().requestDisallowInterceptTouchEvent(false); performClick();
        }
        return true;
    }
    @Override public boolean performClick() { super.performClick(); return true; }
    @Override public boolean onGenericMotionEvent(MotionEvent event) {
        if(engine != null && event.getActionMasked() == MotionEvent.ACTION_SCROLL) { pointer(event,0); engine.wheel(Math.round(event.getAxisValue(MotionEvent.AXIS_VSCROLL))); return true; }
        return super.onGenericMotionEvent(event);
    }
    @Override protected void onFocusChanged(boolean gain,int direction,android.graphics.Rect old) { super.onFocusChanged(gain,direction,old); if(engine != null) engine.focus(gain); }
    @Override public void onWindowFocusChanged(boolean gain) { super.onWindowFocusChanged(gain); if(engine != null) engine.focus(gain); }
    public void showKeyboard() { post(() -> { requestFocus(); ((InputMethodManager)getContext().getSystemService(Context.INPUT_METHOD_SERVICE)).showSoftInput(this, InputMethodManager.SHOW_IMPLICIT); }); }
    public void hideKeyboard() { ((InputMethodManager)getContext().getSystemService(Context.INPUT_METHOD_SERVICE)).hideSoftInputFromWindow(getWindowToken(),0); }
    @Override public boolean onCheckIsTextEditor() { return true; }
    private void tapKey(int code) { if(engine != null) { engine.key(code,true); engine.key(code,false); } }
    @Override public InputConnection onCreateInputConnection(EditorInfo info) {
        info.inputType = android.text.InputType.TYPE_CLASS_TEXT | android.text.InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
        info.imeOptions = EditorInfo.IME_ACTION_DONE | EditorInfo.IME_FLAG_NO_EXTRACT_UI;
        return new BaseInputConnection(this, false) {
            private String composing = "";
            @Override public boolean commitText(CharSequence text,int cursor) { composing=""; if(engine != null) engine.text(text.toString()); return true; }
            @Override public boolean setComposingText(CharSequence text,int cursor) { composing=text.toString(); return true; }
            @Override public boolean finishComposingText() { if(!composing.isEmpty() && engine!=null)engine.text(composing); composing=""; return true; }
            @Override public boolean deleteSurroundingText(int before,int after) { for(int i=0;i<Math.min(4096,before);i++)tapKey(8); for(int i=0;i<Math.min(4096,after);i++)tapKey(46); return true; }
            @Override public boolean deleteSurroundingTextInCodePoints(int before,int after) { return deleteSurroundingText(before,after); }
            @Override public boolean performEditorAction(int action) { tapKey(13); hideKeyboard(); return true; }
            @Override public boolean sendKeyEvent(KeyEvent event) { return event.getAction()==KeyEvent.ACTION_DOWN ? onKeyDown(event.getKeyCode(),event) : onKeyUp(event.getKeyCode(),event); }
        };
    }
    private int keyCode(int code) {
        if(code>=KeyEvent.KEYCODE_A && code<=KeyEvent.KEYCODE_Z)return 65+code-KeyEvent.KEYCODE_A;
        if(code>=KeyEvent.KEYCODE_0 && code<=KeyEvent.KEYCODE_9)return 48+code-KeyEvent.KEYCODE_0;
        if(code>=KeyEvent.KEYCODE_F1 && code<=KeyEvent.KEYCODE_F12)return 112+code-KeyEvent.KEYCODE_F1;
        switch(code) {
            case KeyEvent.KEYCODE_DPAD_LEFT:return 37;case KeyEvent.KEYCODE_DPAD_UP:return 38;case KeyEvent.KEYCODE_DPAD_RIGHT:return 39;case KeyEvent.KEYCODE_DPAD_DOWN:return 40;
            case KeyEvent.KEYCODE_DEL:return 8;case KeyEvent.KEYCODE_FORWARD_DEL:return 46;case KeyEvent.KEYCODE_ENTER:return 13;case KeyEvent.KEYCODE_TAB:return 9;
            case KeyEvent.KEYCODE_ESCAPE:return 27;case KeyEvent.KEYCODE_SPACE:return 32;case KeyEvent.KEYCODE_MOVE_HOME:return 36;case KeyEvent.KEYCODE_MOVE_END:return 35;
            case KeyEvent.KEYCODE_PAGE_UP:return 33;case KeyEvent.KEYCODE_PAGE_DOWN:return 34;case KeyEvent.KEYCODE_INSERT:return 45;
            case KeyEvent.KEYCODE_SHIFT_LEFT:case KeyEvent.KEYCODE_SHIFT_RIGHT:return 16;case KeyEvent.KEYCODE_CTRL_LEFT:case KeyEvent.KEYCODE_CTRL_RIGHT:return 17;
            case KeyEvent.KEYCODE_ALT_LEFT:case KeyEvent.KEYCODE_ALT_RIGHT:return 18;default:return 0;
        }
    }
    @Override public boolean onKeyDown(int code,KeyEvent event) {
        if(engine == null || code==KeyEvent.KEYCODE_BACK)return super.onKeyDown(code,event);
        int key=keyCode(code);if(key!=0)engine.key(key,true);
        int unicode=event.getUnicodeChar();if(unicode>=32 && !event.isCtrlPressed() && (unicode & KeyCharacterMap.COMBINING_ACCENT)==0)engine.text(new String(Character.toChars(unicode)));
        return key!=0 || unicode>=32 || super.onKeyDown(code,event);
    }
    @Override public boolean onKeyUp(int code,KeyEvent event) { int key=keyCode(code);if(engine!=null && key!=0){engine.key(key,false);return true;}return super.onKeyUp(code,event); }
    @Override public String getClipboard() {
        ClipboardManager c=(ClipboardManager)getContext().getSystemService(Context.CLIPBOARD_SERVICE);
        ClipData data=c.getPrimaryClip();return data==null || data.getItemCount()==0 ? "" : String.valueOf(data.getItemAt(0).coerceToText(getContext()));
    }
    @Override public void setClipboard(String text) { ((ClipboardManager)getContext().getSystemService(Context.CLIPBOARD_SERVICE)).setPrimaryClip(ClipData.newPlainText("FoxLang",text)); }
    @Override public synchronized boolean playWav(byte[] bytes) {
        File temporary=null;MediaPlayer player=new MediaPlayer();
        try {
            temporary=File.createTempFile("foxlang-", ".wav",getContext().getCacheDir());
            try(FileOutputStream out=new FileOutputStream(temporary)){out.write(bytes);}
            player.setDataSource(temporary.getPath());player.prepare();temporary.delete();
            sounds.add(player);
            player.setOnCompletionListener(p -> { synchronized(FoxView.this){ sounds.remove(p);p.release(); } });
            player.start();return true;
        } catch(Exception error){player.release();return false;} finally {if(temporary!=null)temporary.delete();}
    }
    @Override public synchronized void stopSound() { for(MediaPlayer p:sounds)p.release();sounds.clear(); }
    @Override public boolean openUri(String target) {
        Uri uri=Uri.parse(target);String scheme=uri.getScheme();
        if(!("https".equalsIgnoreCase(scheme)||"http".equalsIgnoreCase(scheme)||"mailto".equalsIgnoreCase(scheme)||"content".equalsIgnoreCase(scheme)))return false;
        try { Intent intent=new Intent(Intent.ACTION_VIEW,uri).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);if("content".equalsIgnoreCase(scheme))intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);getContext().startActivity(intent);return true; }catch(Exception error){return false;}
    }
    @Override protected void onDetachedFromWindow() { if(engine!=null)engine.cancel();stopSound();super.onDetachedFromWindow(); }
}
