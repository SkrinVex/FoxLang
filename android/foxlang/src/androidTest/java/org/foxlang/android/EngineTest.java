package org.foxlang.android;

import androidx.test.platform.app.InstrumentationRegistry;
import org.junit.Test;
import static org.junit.Assert.*;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import java.net.ServerSocket;
import java.net.Socket;

public class EngineTest {
    private File directory() throws Exception { return Files.createTempDirectory(InstrumentationRegistry.getInstrumentation().getTargetContext().getCacheDir().toPath(), "fox-test-").toFile(); }
    private FoxLang.RunResult run(FoxLang engine, String code) throws Exception { return engine.runSource(code).get(20,TimeUnit.SECONDS); }
    @Test public void coreUnicodeImportsFilesAndArguments() throws Exception {
        assertTrue(FoxLang.thirdPartyNotices().contains("Mbed TLS"));
        File dir=directory();new File(dir,"nested").mkdir();
        Files.write(new File(dir,"nested/helper.fox").toPath(),"string greeting = \"Привет 🦊\";".getBytes(StandardCharsets.UTF_8));
        Files.write(new File(dir,"nested/main.fox").toPath(),("include(\"helper.fox\"); using os; print(greeting); print(platform()); print(arg_or(0,\"missing\")); string entered=input(); write_file(\"saved.txt\", entered); int answer=40+2;").getBytes(StandardCharsets.UTF_8));
        try(FoxLang engine=new FoxLang(dir,new FoxLang.Host(){},"--пользователь")) {
            FoxLang.RunResult result=engine.runFile("nested/main.fox","Ввод 🦊\n").get(20,TimeUnit.SECONDS);
            assertTrue(result.error,result.success);
            assertTrue(result.output,result.output.contains("Привет 🦊"));assertTrue(result.output.contains("android"));assertTrue(result.output.contains("--пользователь"));
            assertEquals("Ввод 🦊",new String(Files.readAllBytes(new File(dir,"saved.txt").toPath()),StandardCharsets.UTF_8));
            assertEquals("42",engine.getGlobal("answer").get(5,TimeUnit.SECONDS).text);
            FoxLang.RunResult paths=run(engine,"assert(path_join(\"one\",\"two\")==\"one/two\"); assert(path_parent(\"one/two\")==\"one\"); assert(fs_is_dir(fs_temp_dir())); assert(fs_home()==os_cwd()); write_file(path_join(fs_temp_dir(),\"scratch.txt\"),\"temporary\");");
            assertTrue(paths.error,paths.success);
            FoxLang.RunResult protectedHome=run(engine,"bool refused=false; try { fs_remove_all(fs_home()); } catch(e) { refused=true; } assert(refused);");
            assertTrue(protectedHome.error,protectedHome.success);
            engine.setGlobal("host_value","string","Kotlin → 🦊").get(5,TimeUnit.SECONDS);
            assertEquals("Kotlin → 🦊\n",run(engine,"print(host_value);").output);
            assertFalse(run(engine,"int broken = ;").success);
            engine.reset().get(5,TimeUnit.SECONDS);assertEquals("3\n",run(engine,"print(1+2);").output);
        }
    }
    @Test public void graphicsInputResizeClipboardAndSound() throws Exception {
        AtomicReference<FoxLang> ref=new AtomicReference<>();AtomicInteger frames=new AtomicInteger();AtomicReference<byte[]> wav=new AtomicReference<>();
        AtomicInteger closed=new AtomicInteger();AtomicInteger width=new AtomicInteger();
        FoxLang.Host host=new FoxLang.Host(){
            String clipboard="";
            public void onOpen(int w,int h,String title){assertEquals("Окно 🦊",title);}
            public void onClose(){closed.incrementAndGet();}
            public String getClipboard(){return clipboard;}
            public void setClipboard(String text){clipboard=text;}
            public boolean playWav(byte[] bytes){wav.set(bytes);return true;}
            public void onFrame(int[] pixels,int w,int h){
                width.set(w);assertEquals(0xff0a141e,pixels[0]);FoxLang e=ref.get();int n=frames.incrementAndGet();
                if(n==2){e.pointer(20,30);e.key(1,true);}
                if(n==3){e.key(1,false);e.text("🦊");}
                if(n==5){e.pointer(20,90);e.key(1,true);}
                if(n==6)e.key(1,false);
                if(n==8){e.pointer(20,145);e.key(1,true);}
                if(n==9)e.key(1,false);
                if(n==10)e.resize(480,300);
                if(n==14)e.closeWindow();
            }
        };
        try(FoxLang engine=new FoxLang(directory(),host)) {
            ref.set(engine);
            FoxLang.RunResult result=run(engine,"using graphics; using ui; using sound; open_window(320,240,\"Окно 🦊\"); set_clipboard_text(\"буфер 🦊\"); print(clipboard_text()); print(play_tone(440,50,0.2)); set_window_fullscreen(true); print(window_fullscreen()); set_window_fullscreen(false); string value=\"\"; bool checked=false; while(window_poll()){clear_window(rgb(10,20,30)); value=ui_text_field(\"field\",10,20,200,value,\"Текст\"); if(ui_button(\"go\",10,80,100,30,\"OK\")){print(\"CLICK\");} checked=ui_checkbox(\"check\",10,140,\"Flag\",checked); present_window();} close_window(); print(value); print(checked);");
            assertTrue(result.error,result.success);assertEquals(480,width.get());assertEquals(1,closed.get());
            assertTrue(result.output,result.output.contains("CLICK"));assertTrue(result.output.contains("🦊\ntrue"));assertTrue(result.output.contains("буфер 🦊"));
            assertNotNull(wav.get());assertEquals("RIFF",new String(wav.get(),0,4,StandardCharsets.US_ASCII));
            AtomicReference<FoxView> view=new AtomicReference<>();
            InstrumentationRegistry.getInstrumentation().runOnMainSync(()->view.set(new FoxView(InstrumentationRegistry.getInstrumentation().getTargetContext())));
            assertTrue(view.get().playWav(wav.get()));view.get().stopSound();
        }
    }
    @Test public void cancellationAndIndependentInstances() throws Exception {
        CountDownLatch started=new CountDownLatch(1);
        try(FoxLang first=new FoxLang(directory(),new FoxLang.Host(){public void onOutput(String text,boolean error){started.countDown();}});
            FoxLang second=new FoxLang(directory(),new FoxLang.Host(){})) {
            CompletableFuture<FoxLang.RunResult> running=first.runSource("print(\"started\"); while(true){}");
            assertTrue(started.await(10,TimeUnit.SECONDS));first.cancel();assertTrue(running.get(10,TimeUnit.SECONDS).cancelled());
            assertTrue(run(second,"int separate=7;").success);
            first.reset().get(5,TimeUnit.SECONDS);assertTrue(run(first,"int separate=9;").success);
            assertEquals("9",first.getGlobal("separate").get(5,TimeUnit.SECONDS).text);assertEquals("7",second.getGlobal("separate").get(5,TimeUnit.SECONDS).text);
        }
    }
    @Test public void viewTransformsTouchAndCommitsUnicodeIme() throws Exception {
        AtomicReference<FoxLang> engineRef=new AtomicReference<>();
        AtomicReference<FoxView> viewRef=new AtomicReference<>();
        AtomicReference<android.view.inputmethod.InputConnection> input=new AtomicReference<>();
        AtomicInteger frames=new AtomicInteger();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(()->{
            FoxView view=new FoxView(InstrumentationRegistry.getInstrumentation().getTargetContext()) {
                @Override public void onFrame(int[] pixels,int width,int height) {
                    super.onFrame(pixels,width,height);
                    int frame=frames.incrementAndGet();
                    InstrumentationRegistry.getInstrumentation().runOnMainSync(()->{
                        if(frame==2 || frame==3) {
                            android.view.MotionEvent event=android.view.MotionEvent.obtain(0,0,frame==2 ? 0 : 1,40,60,0);
                            onTouchEvent(event);event.recycle();
                        }
                        if(frame==4)input.get().commitText("Кот🦊",1);
                        if(frame==6)input.get().deleteSurroundingTextInCodePoints(1,0);
                        if(frame==8)input.get().setComposingText("Л",1);
                        if(frame==9)input.get().finishComposingText();
                        if(frame==11) {
                            android.graphics.Bitmap bitmap=android.graphics.Bitmap.createBitmap(640,480,android.graphics.Bitmap.Config.ARGB_8888);
                            draw(new android.graphics.Canvas(bitmap));
                            assertEquals(0xff0a141e,bitmap.getPixel(0,0));
                            engineRef.get().closeWindow();
                        }
                    });
                }
            };
            view.layout(0,0,640,480);
            viewRef.set(view);input.set(view.onCreateInputConnection(new android.view.inputmethod.EditorInfo()));
        });
        try(FoxLang engine=new FoxLang(directory(),viewRef.get())) {
            engineRef.set(engine);
            InstrumentationRegistry.getInstrumentation().runOnMainSync(()->viewRef.get().setEngine(engine));
            FoxLang.RunResult result=run(engine,"using graphics; using ui; open_window(320,240,\"IME\"); set_window_resizable(false); string value=\"\"; while(window_poll()){ clear_window(rgb(10,20,30)); value=ui_text_field(\"entry\",10,20,200,value,\"\"); present_window(); } print(value);");
            assertTrue(result.error,result.success);assertEquals("КотЛ\n",result.output);
        }
    }
    @Test public void networkUsesAndroidPermissionsAndNativeHttp() throws Exception {
        try(ServerSocket server=new ServerSocket(0);FoxLang engine=new FoxLang(directory(),new FoxLang.Host(){})) {
            server.setSoTimeout(10000);
            AtomicReference<Exception> serverError=new AtomicReference<>();
            Thread reply=new Thread(()->{try(Socket socket=server.accept()){
                java.io.BufferedReader reader=new java.io.BufferedReader(new java.io.InputStreamReader(socket.getInputStream()));String line;while((line=reader.readLine())!=null&&!line.isEmpty()){}
                socket.getOutputStream().write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK".getBytes(StandardCharsets.US_ASCII));
            }catch(Exception e){serverError.set(e);}});reply.start();
            FoxLang.RunResult result=run(engine,"using http; print(http_get(\"http://127.0.0.1:"+server.getLocalPort()+"/\"));");
            assertTrue(result.error,result.success);assertTrue(result.output,result.output.contains("OK"));reply.join(5000);assertFalse(reply.isAlive());assertNull(serverError.get());
        }
    }
    @Test public void cancellationReleasesListeningServer() throws Exception {
        CountDownLatch listening=new CountDownLatch(1);
        int port;
        try(ServerSocket available=new ServerSocket(0)){port=available.getLocalPort();}
        try(FoxLang engine=new FoxLang(directory(),new FoxLang.Host(){
            public void onOutput(String text,boolean error){if(text.contains("Listening"))listening.countDown();}
        })) {
            CompletableFuture<FoxLang.RunResult> result=engine.runSource("using server; listen("+port+");");
            assertTrue(listening.await(10,TimeUnit.SECONDS));engine.cancel();
            assertTrue(result.get(10,TimeUnit.SECONDS).cancelled());
            try(ServerSocket reused=new ServerSocket(port)){assertTrue(reused.isBound());}
        }
    }
    @Test public void httpsValidatesCertificateAndHostname() throws Exception {
        android.content.res.AssetManager assets=InstrumentationRegistry.getInstrumentation().getContext().getAssets();
        java.security.KeyStore keys=java.security.KeyStore.getInstance("PKCS12");
        try(java.io.InputStream stream=assets.open("tls/server.p12")){keys.load(stream,"foxlang-test".toCharArray());}
        javax.net.ssl.KeyManagerFactory km=javax.net.ssl.KeyManagerFactory.getInstance(javax.net.ssl.KeyManagerFactory.getDefaultAlgorithm());
        km.init(keys,"foxlang-test".toCharArray());
        javax.net.ssl.SSLContext tls=javax.net.ssl.SSLContext.getInstance("TLS");tls.init(km.getKeyManagers(),null,null);
        File dir=directory();
        try(java.io.InputStream cert=assets.open("tls/ca.pem")){Files.copy(cert,new File(dir,"ca.pem").toPath());}
        try(ServerSocket server=tls.getServerSocketFactory().createServerSocket(0);FoxLang engine=new FoxLang(dir,new FoxLang.Host(){})) {
            server.setSoTimeout(15000);
            Thread responder=new Thread(()->{
                for(int i=0;i<3;i++)try(Socket connection=server.accept()) {
                    connection.setSoTimeout(10000);
                    java.io.BufferedReader reader=new java.io.BufferedReader(new java.io.InputStreamReader(connection.getInputStream()));
                    String line;while((line=reader.readLine())!=null&&!line.isEmpty()){}
                    connection.getOutputStream().write("HTTP/1.1 200 OK\r\nContent-Length: 6\r\nConnection: close\r\n\r\nTLS_OK".getBytes(StandardCharsets.US_ASCII));
                } catch(Exception expectedForRejectedCertificate) { /* clients abort rejected TLS handshakes */ }
            });responder.start();
            try {
                FoxLang.RunResult trusted=run(engine,"using http; env_set(\"FOXLANG_CA_BUNDLE\",\"ca.pem\"); print(http_get(\"https://localhost:"+server.getLocalPort()+"/\"));");
                assertTrue(trusted.error,trusted.success);assertTrue(trusted.output.contains("TLS_OK"));
                FoxLang.RunResult wrongName=run(engine,"assert(http_get(\"https://127.0.0.1:"+server.getLocalPort()+"/\")==\"\"); assert(http_status()==0);");
                assertTrue(wrongName.error,wrongName.success);assertTrue(wrongName.output,wrongName.output.contains("HTTP ERROR"));
                FoxLang.RunResult untrusted=run(engine,"env_set(\"FOXLANG_CA_BUNDLE\",\"embedded\"); assert(http_get(\"https://localhost:"+server.getLocalPort()+"/\")==\"\"); assert(http_status()==0);");
                assertTrue(untrusted.error,untrusted.success);assertTrue(untrusted.output.contains("HTTP ERROR"));
            } finally {run(engine,"env_set(\"FOXLANG_CA_BUNDLE\",\"embedded\");");}
            responder.join(5000);assertFalse(responder.isAlive());
        }
    }
}
