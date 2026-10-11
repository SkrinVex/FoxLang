package org.foxlang.runner;

import android.content.pm.ActivityInfo;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import androidx.lifecycle.Lifecycle;
import androidx.test.core.app.ActivityScenario;
import androidx.test.platform.app.InstrumentationRegistry;
import org.foxlang.android.FoxControls;
import org.foxlang.android.FoxLang;
import org.foxlang.android.FoxView;
import org.junit.Test;
import java.nio.file.Files;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Predicate;
import static org.junit.Assert.*;

/** Runs against the packaged program, including the unmodified raycaster source in CI. */
public class RunnerTest {
    private <T extends View> T find(View view, Class<T> type, Predicate<T> matches) {
        if(type.isInstance(view) && matches.test(type.cast(view)))return type.cast(view);
        if(view instanceof ViewGroup) {
            ViewGroup group=(ViewGroup)view;
            for(int i=0;i<group.getChildCount();i++) {
                T found=find(group.getChildAt(i),type,matches);
                if(found!=null)return found;
            }
        }
        return null;
    }
    private Button runButton(MainActivity activity) {
        return find(activity.getWindow().getDecorView(),Button.class,b->b.getText().toString().equals("Запустить"));
    }
    private FoxView view(MainActivity activity) {
        return find(activity.getWindow().getDecorView(),FoxView.class,v->true);
    }
    private FoxControls controls(MainActivity activity) {
        return find(activity.getWindow().getDecorView(),FoxControls.class,v->true);
    }
    private void await(ActivityScenario<MainActivity> scenario,String message,Predicate<MainActivity> condition) {
        long deadline=SystemClock.elapsedRealtime()+30000;
        AtomicBoolean done=new AtomicBoolean();
        do {
            scenario.onActivity(a->done.set(condition.test(a)));
            if(done.get())return;
            SystemClock.sleep(100);
        } while(SystemClock.elapsedRealtime()<deadline);
        fail(message);
    }
    private boolean hasFrame(MainActivity activity) {
        FoxView view=view(activity);
        if(view==null || view.getWidth()==0 || view.getHeight()==0)return false;
        // Render to a small software target: inspect real pixels without relying on screenshots.
        Bitmap image=Bitmap.createBitmap(24,24,Bitmap.Config.ARGB_8888);
        Canvas canvas=new Canvas(image);canvas.scale(24f/view.getWidth(),24f/view.getHeight());view.draw(canvas);
        int first=image.getPixel(0,0);boolean different=false;
        for(int y=0;y<24;y++)for(int x=0;x<24;x++)different |= image.getPixel(x,y)!=first;
        image.recycle();return different;
    }
    private void touch(View target,int action) {
        long now=SystemClock.uptimeMillis();
        MotionEvent event=MotionEvent.obtain(now,now,action,target.getWidth()/2f,target.getHeight()/2f,0);
        target.dispatchTouchEvent(event);event.recycle();
    }
    @Test public void graphicsSurviveRotationStopRestartAndRecreation() {
        try(ActivityScenario<MainActivity> scenario=ActivityScenario.launch(MainActivity.class)) {
            await(scenario,"Packaged program did not draw a frame",this::hasFrame);
            scenario.onActivity(a->{
                FoxControls controls=controls(a);assertNotNull(controls);assertEquals(6,controls.getChildCount());
                for(int i=0;i<controls.getChildCount();i++) {
                    View key=controls.getChildAt(i);assertTrue(key.isShown());assertTrue(key.getWidth()>0);
                    assertTrue(key.getRight()<=controls.getWidth());assertFalse(key.isLongClickable());
                }
                // Two independent held buttons; cancellation must release both on focus loss.
                touch(controls.getChildAt(1),MotionEvent.ACTION_DOWN);
                touch(controls.getChildAt(3),MotionEvent.ACTION_DOWN);
                assertTrue(controls.getChildAt(1).isPressed());assertTrue(controls.getChildAt(3).isPressed());
                controls.onWindowFocusChanged(false);
                assertFalse(controls.getChildAt(1).isPressed());assertFalse(controls.getChildAt(3).isPressed());
                controls.onWindowFocusChanged(true);
                a.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
            });
            await(scenario,"Landscape layout did not resize",a->view(a).getWidth()>view(a).getHeight() && hasFrame(a));
            for(int cycle=0;cycle<3;cycle++) {
                scenario.moveToState(Lifecycle.State.CREATED);
                scenario.moveToState(Lifecycle.State.RESUMED);
                await(scenario,"Stopping Activity did not release the interpreter",a->runButton(a).isEnabled());
                scenario.onActivity(a->{assertTrue(runButton(a).isShown());runButton(a).performClick();});
                await(scenario,"Restart did not open the game",a->!runButton(a).isEnabled() && hasFrame(a));
                scenario.recreate();
                await(scenario,"Recreated Activity did not start",a->!runButton(a).isEnabled() && hasFrame(a));
            }
        }
    }
    @Test public void stoppingWhileWaitingForWorkerCancelsTheQueuedLaunch() throws Exception {
        CountDownLatch started=new CountDownLatch(1);
        java.io.File directory=Files.createTempDirectory(
            InstrumentationRegistry.getInstrumentation().getTargetContext().getCacheDir().toPath(),"busy-worker-").toFile();
        try(FoxLang blocker=new FoxLang(directory,new FoxLang.Host(){
            public void onOutput(String text,boolean error){started.countDown();}
        })) {
            java.util.concurrent.CompletableFuture<FoxLang.RunResult> busy=blocker.runSource("print(\"ready\"); while(true){}");
            assertTrue(started.await(15,TimeUnit.SECONDS));
            try(ActivityScenario<MainActivity> scenario=ActivityScenario.launch(MainActivity.class)) {
                scenario.moveToState(Lifecycle.State.CREATED);
                scenario.moveToState(Lifecycle.State.RESUMED);
                blocker.cancel();assertTrue(busy.get(10,TimeUnit.SECONDS).cancelled());
                await(scenario,"A cancelled queued launch started after onStop",a->runButton(a).isEnabled());
                scenario.onActivity(a->runButton(a).performClick());
                await(scenario,"Explicit restart did not work",a->!runButton(a).isEnabled() && hasFrame(a));
            }
        }
    }
}
