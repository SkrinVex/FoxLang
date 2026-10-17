package org.skrumi.android;

import java.io.File;
import java.util.Objects;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicLong;

/** Android engine. Interpreter operations run on one shared worker, never the UI thread.
 * Instances have independent globals/modules; execution is serialized because the
 * native object allocator is process-wide. cancel() and input events remain immediate. */
public class Skrumi implements AutoCloseable {
    static { System.loadLibrary("skrumi_android"); }
    private static final ExecutorService WORKER = Executors.newSingleThreadExecutor(
        r -> new Thread(null, r, "Skrumi", 8 * 1024 * 1024));
    public interface Host {
        default void onOutput(String text, boolean error) {}
        default void onOpen(int width, int height, String title) { throw new IllegalStateException("Attach a SkrumiView to run graphics"); }
        default void onFrame(int[] argb, int width, int height) {}
        default void onClose() {}
        default void onResizable(boolean enabled) {}
        default void onFullscreen(boolean enabled) {}
        default String getClipboard() { return ""; }
        default void setClipboard(String text) {}
        default boolean playWav(byte[] bytes) { return false; }
        default void stopSound() {}
        default boolean openUri(String uri) { return false; }
    }
    public static final class RunResult {
        public final boolean success;
        public final int exitCode;
        public final String error, output;
        RunResult(String[] raw) { success = Boolean.parseBoolean(raw[0]); exitCode = Integer.parseInt(raw[1]); error = raw[2]; output = raw[3]; }
        public boolean cancelled() { return exitCode == 130; }
    }
    public static final class Value {
        public final String type, text;
        Value(String[] raw) { type = raw[0]; text = raw[1]; }
    }
    private final File directory;
    private final Host host;
    private final AtomicLong epoch = new AtomicLong();
    private final CompletableFuture<Void> ready = new CompletableFuture<>();
    private long handle;
    private boolean closed;
    public Skrumi(File directory, Host host, String... arguments) {
        this.directory = directory.getAbsoluteFile();
        this.host = Objects.requireNonNull(host);
        if (!this.directory.isDirectory() && !this.directory.mkdirs()) throw new IllegalArgumentException("Cannot create project directory");
        final String[] args = arguments.clone();
        WORKER.execute(() -> {
            try { synchronized (this) { handle = nativeCreate(this.directory.getPath(), args); nativeCancel(handle, epoch.get()); } ready.complete(null); }
            catch (Throwable error) { ready.completeExceptionally(error); }
        });
    }
    public static String version() { return nativeVersion(); }
    public static String thirdPartyNotices() { return nativeNotices(); }
    private interface Task<T> { T call() throws Exception; }
    private <T> CompletableFuture<T> submit(Task<T> task) {
        CompletableFuture<T> future = new CompletableFuture<>();
        synchronized (this) { if (closed) { future.completeExceptionally(new IllegalStateException("Skrumi engine is closed")); return future; } }
        WORKER.execute(() -> {
            try { ready.join(); synchronized (this) { if (closed) throw new IllegalStateException("Skrumi engine is closed"); } future.complete(task.call()); }
            catch (Throwable error) { future.completeExceptionally(error); }
        });
        return future;
    }
    public CompletableFuture<RunResult> runSource(String source) { return runSource(source, "main.skm", ""); }
    public CompletableFuture<RunResult> runSource(String source, String filename, String stdin) {
        Objects.requireNonNull(source); Objects.requireNonNull(stdin);
        final long generation = epoch.get();
        final String path = resolve(filename);
        return submit(() -> new RunResult(nativeRun(handle, source, path, stdin, false, host, generation)));
    }
    public CompletableFuture<RunResult> runFile(String filename) { return runFile(filename, ""); }
    public CompletableFuture<RunResult> runFile(String filename, String stdin) {
        Objects.requireNonNull(stdin);
        final long generation = epoch.get(); final String path = resolve(filename);
        return submit(() -> new RunResult(nativeRun(handle, "", path, stdin, true, host, generation)));
    }
    private String resolve(String filename) { File path = new File(Objects.requireNonNull(filename)); return (path.isAbsolute() ? path : new File(directory, filename)).getAbsolutePath(); }
    public CompletableFuture<Value> getGlobal(String name) { return submit(() -> new Value(nativeGet(handle, name))); }
    public CompletableFuture<Void> setGlobal(String name, String type, String value) { return submit(() -> { nativeSet(handle, name, type, value); return null; }); }
    public CompletableFuture<Void> reset() { return submit(() -> { nativeReset(handle); return null; }); }
    /** Cooperative cancellation, including tight VM loops and wait(). Blocking OS IO
     * completes according to its own timeout before cancellation can be observed. */
    public synchronized void cancel() { long generation = epoch.incrementAndGet(); if (handle != 0 && !closed) nativeCancel(handle, generation); }
    public synchronized void event(int kind, int a, int b, String text) { if (handle != 0 && !closed) nativeEvent(handle, kind, a, b, Objects.requireNonNull(text)); }
    public void key(int code, boolean down) { event(0, code, down ? 1 : 0, ""); }
    public void text(String text) { event(1, 0, 0, text); }
    public void pointer(int x, int y) { event(2, x, y, ""); }
    public void wheel(int steps) { event(3, steps, 0, ""); }
    public void focus(boolean focused) { event(4, focused ? 1 : 0, 0, ""); }
    public void resize(int width, int height) { event(5, width, height, ""); }
    public void closeWindow() { event(6, 0, 0, ""); }
    @Override public synchronized void close() {
        if (closed) return;
        cancel(); closed = true;
        WORKER.execute(() -> { synchronized (this) { if (handle != 0) { nativeClose(handle); handle = 0; } } });
    }
    private static native String nativeVersion();
    private static native String nativeNotices();
    private static native long nativeCreate(String directory, String[] arguments);
    private static native String[] nativeRun(long handle, String source, String path, String stdin, boolean file, Host host, long epoch);
    private static native void nativeCancel(long handle, long epoch);
    private static native void nativeEvent(long handle, int kind, int a, int b, String text);
    private static native String[] nativeGet(long handle, String name);
    private static native void nativeSet(long handle, String name, String type, String value);
    private static native void nativeReset(long handle);
    private static native void nativeClose(long handle);
}
