package org.skrumi.android;

import android.content.Context;

/** Recommended name for the existing touch controls. */
public final class SkrumiControls extends org.foxlang.android.FoxControls {
    public SkrumiControls(Context context, Skrumi engine, SkrumiView view) { super(context, engine, view); }
    public SkrumiControls(Context context, Skrumi engine, SkrumiView view, boolean wasd) { super(context, engine, view, wasd); }
}
