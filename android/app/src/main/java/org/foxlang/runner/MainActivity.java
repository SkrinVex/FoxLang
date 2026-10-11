package org.foxlang.runner;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import org.foxlang.android.FoxLang;
import org.foxlang.android.FoxView;
import org.foxlang.android.FoxControls;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

/** APK entry point used by the example and packaging/build_android.py. */
public class MainActivity extends Activity {
    private FoxLang engine;
    private TextView output;
    private EditText stdin;
    private Button run;
    private File project;
    private String entry;
    private final StringBuilder pendingOutput = new StringBuilder();
    private boolean outputPosted;
    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);
        root.setOnApplyWindowInsetsListener((v,insets)->{v.setPadding(insets.getSystemWindowInsetLeft(),insets.getSystemWindowInsetTop(),insets.getSystemWindowInsetRight(),insets.getSystemWindowInsetBottom());return insets;});
        LinearLayout controls=new LinearLayout(this);
        run=new Button(this);run.setText("Запустить");controls.addView(run);
        Button stop=new Button(this);stop.setText("Стоп");controls.addView(stop);
        Button keyboard=new Button(this);keyboard.setText("Клавиатура");controls.addView(keyboard);
        root.addView(controls);
        stdin=new EditText(this);stdin.setHint("Ввод для input() — перед запуском");root.addView(stdin);
        FoxView view=new FoxView(this);root.addView(view,new LinearLayout.LayoutParams(-1,0,1));
        output=new TextView(this);output.setTextIsSelectable(true);
        ScrollView console=new ScrollView(this);console.addView(output);root.addView(console,new LinearLayout.LayoutParams(-1,(int)(120*getResources().getDisplayMetrics().density)));
        setContentView(root);
        try {
            project=new File(getFilesDir(),"program");
            try(InputStream in=getAssets().open("foxlang-entry.txt")){java.io.ByteArrayOutputStream bytes=new java.io.ByteArrayOutputStream();byte[] buf=new byte[4096];int n;while((n=in.read(buf))!=-1)bytes.write(buf,0,n);entry=bytes.toString("UTF-8");}
            long installed=getPackageManager().getPackageInfo(getPackageName(),0).lastUpdateTime;
            if(getPreferences(MODE_PRIVATE).getLong("installed",0)!=installed || !new File(project,entry).isFile()) {
                copyAssets("program",project);getPreferences(MODE_PRIVATE).edit().putLong("installed",installed).apply();
            }
            engine=new FoxLang(project,view,getIntent().getStringArrayExtra("foxlang.arguments")==null ? new String[0] : getIntent().getStringArrayExtra("foxlang.arguments"));
            view.setEngine(engine);
            FoxControls gamepad = new FoxControls(this,engine,view);
            root.addView(gamepad,root.indexOfChild(console));
            view.setFullscreenListener(enabled -> {
                controls.setVisibility(enabled ? View.GONE : View.VISIBLE);
                stdin.setVisibility(enabled ? View.GONE : View.VISIBLE);
                console.setVisibility(enabled ? View.GONE : View.VISIBLE);
            });
            view.setOutputListener((text,error)->appendOutput(text));
            run.setOnClickListener(v->start());stop.setOnClickListener(v->engine.cancel());keyboard.setOnClickListener(v->view.showKeyboard());
            start();
        } catch(Exception error){output.setText(error.toString());run.setEnabled(false);}
    }
    private void appendOutput(String text) {
        synchronized(pendingOutput) {
            pendingOutput.append(text);
            if(pendingOutput.length()>64000)pendingOutput.delete(0,pendingOutput.length()-64000);
            if(outputPosted)return;
            outputPosted=true;
        }
        output.postOnAnimation(()->{
            String chunk;
            synchronized(pendingOutput){chunk=pendingOutput.toString();pendingOutput.setLength(0);outputPosted=false;}
            String all=output.getText().toString()+chunk;
            output.setText(all.length()>64000 ? all.substring(all.length()-64000) : all);
        });
    }
    private void copyAssets(String source,File target)throws Exception {
        String[] children=getAssets().list(source);
        if(children!=null && children.length>0){if(!target.isDirectory()&&!target.mkdirs())throw new java.io.IOException("Cannot create "+target);for(String name:children)copyAssets(source+"/"+name,new File(target,name));}
        else {try(InputStream in=getAssets().open(source);FileOutputStream out=new FileOutputStream(target)){byte[] buffer=new byte[16384];int n;while((n=in.read(buffer))!=-1)out.write(buffer,0,n);}}
    }
    private void start() {
        run.setEnabled(false);output.setText("");
        String input = stdin.getText().toString();
        engine.reset().thenCompose(v->engine.runFile(entry,input)).whenComplete((result,error)->runOnUiThread(()->{
            run.setEnabled(true);
            if(error!=null)output.append(error.toString());else if(!result.error.isEmpty())output.append(result.error);else if(result.cancelled())output.append("\nОстановлено");
        }));
    }
    @Override protected void onStop(){if(engine!=null)engine.cancel();super.onStop();}
    @Override protected void onDestroy(){if(engine!=null)engine.close();super.onDestroy();}
}
