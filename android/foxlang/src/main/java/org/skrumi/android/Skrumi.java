package org.skrumi.android;

import java.io.File;

/** Skrumi engine; the legacy API and JNI implementation remain binary compatible. */
public final class Skrumi extends org.foxlang.android.FoxLang {
    public Skrumi(File directory, Host host, String... arguments) {
        super(directory, host, arguments);
    }
    @Override public java.util.concurrent.CompletableFuture<RunResult> runSource(String source) {
        return runSource(source, "main.skm", "");
    }
}
