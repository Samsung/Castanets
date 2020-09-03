/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

package com.android.systemui.util.leak;

import static android.service.quicksettings.Tile.STATE_ACTIVE;
import static android.telephony.ims.feature.ImsFeature.STATE_UNAVAILABLE;

import static com.android.internal.logging.MetricsLogger.VIEW_UNKNOWN;
import static com.android.systemui.Dependency.BG_LOOPER_NAME;

import android.annotation.Nullable;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Debug;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.Process;
import android.os.SystemProperties;
import android.provider.Settings;
import android.text.format.DateUtils;
import android.util.Log;
import android.util.LongSparseArray;

import com.android.systemui.Dumpable;
import com.android.systemui.R;
import com.android.systemui.SystemUI;
import com.android.systemui.SystemUIFactory;
import com.android.systemui.plugins.qs.QSTile;
import com.android.systemui.qs.QSHost;
import com.android.systemui.qs.tileimpl.QSTileImpl;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.util.ArrayList;

import javax.inject.Inject;
import javax.inject.Named;
import javax.inject.Singleton;

/**
 */
@Singleton
public class GarbageMonitor implements Dumpable {
    private static final boolean LEAK_REPORTING_ENABLED =
            Build.IS_DEBUGGABLE
                    && SystemProperties.getBoolean("debug.enable_leak_reporting", false);
    private static final String FORCE_ENABLE_LEAK_REPORTING = "sysui_force_enable_leak_reporting";

    private static final boolean HEAP_TRACKING_ENABLED = Build.IS_DEBUGGABLE;

    // whether to use ActivityManager.setHeapLimit
    private static final boolean ENABLE_AM_HEAP_LIMIT = Build.IS_DEBUGGABLE;
    // heap limit value, in KB (overrides R.integer.watch_heap_limit)
    private static final String SETTINGS_KEY_AM_HEAP_LIMIT = "systemui_am_heap_limit";

    private static final String TAG = "GarbageMonitor";

    private static final long GARBAGE_INSPECTION_INTERVAL =
            15 * DateUtils.MINUTE_IN_MILLIS; // 15 min
    private static final long HEAP_TRACK_INTERVAL = 1 * DateUtils.MINUTE_IN_MILLIS; // 1 min
    private static final int HEAP_TRACK_HISTORY_LEN = 720; // 12 hours

    private static final int DO_GARBAGE_INSPECTION = 1000;
    private static final int DO_HEAP_TRACK = 3000;

    private static final int GARBAGE_ALLOWANCE = 5;

    private static final boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG);

    private final Handler mHandler;
    private final TrackedGarbage mTrackedGarbage;
    private final LeakReporter mLeakReporter;
    private final Context mContext;
    private final ActivityManager mAm;
    private MemoryTile mQSTile;
    private DumpTruck mDumpTruck;

    private final LongSparseArray<ProcessMemInfo> mData = new LongSparseArray<>();
    private final ArrayList<Long> mPids = new ArrayList<>();
    private int[] mPidsArray = new int[1];

    private long mHeapLimit;

    /**
     */
    @Inject
    public GarbageMonitor(
            Context context,
            @Named(BG_LOOPER_NAME) Looper bgLooper,
            LeakDetector leakDetector,
            LeakReporter leakReporter) {
        mContext = context.getApplicationContext();
        mAm = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);

        mHandler = new BackgroundHeapCheckHandler(bgLooper);

        mTrackedGarbage = leakDetector.getTrackedGarbage();
        mLeakReporter = leakReporter;

        mDumpTruck = new DumpTruck(mContext);

        if (ENABLE_AM_HEAP_LIMIT) {
            mHeapLimit = Settings.Global.getInt(context.getContentResolver(),
                    SETTINGS_KEY_AM_HEAP_LIMIT,
                    mContext.getResources().getInteger(R.integer.watch_heap_limit));
        }
    }

    public void startLeakMonitor() {
        if (mTrackedGarbage == null) {
            return;
        }

        mHandler.sendEmptyMessage(DO_GARBAGE_INSPECTION);
    }

    public void startHeapTracking() {
        startTrackingProcess(
                android.os.Process.myPid(), mContext.getPackageName(), System.currentTimeMillis());
        mHandler.sendEmptyMessage(DO_HEAP_TRACK);
    }

    private boolean gcAndCheckGarbage() {
        if (mTrackedGarbage.countOldGarbage() > GARBAGE_ALLOWANCE) {
            Runtime.getRuntime().gc();
            return true;
        }
        return false;
    }

    void reinspectGarbageAfterGc() {
        int count = mTrackedGarbage.countOldGarbage();
        if (count > GARBAGE_ALLOWANCE) {
            mLeakReporter.dumpLeak(count);
        }
    }

    public ProcessMemInfo getMemInfo(int pid) {
        return mData.get(pid);
    }

    public int[] getTrackedProcesses() {
        return mPidsArray;
    }

    public void startTrackingProcess(long pid, String name, long start) {
        synchronized (mPids) {
            if (mPids.contains(pid)) return;

            mPids.add(pid);
            updatePidsArrayL();

            mData.put(pid, new ProcessMemInfo(pid, name, start));
        }
    }

    private void updatePidsArrayL() {
        final int N = mPids.size();
        mPidsArray = new int[N];
        StringBuffer sb = new StringBuffer("Now tracking processes: ");
        for (int i = 0; i < N; i++) {
            final int p = mPids.get(i).intValue();
            mPidsArray[i] = p;
            sb.append(p);
            sb.append(" ");
        }
        if (DEBUG) Log.v(TAG, sb.toString());
    }

    private void update() {
        synchronized (mPids) {
            Debug.MemoryInfo[] dinfos = mAm.getProcessMemoryInfo(mPidsArray);
            for (int i = 0; i < dinfos.length; i++) {
                Debug.MemoryInfo dinfo = dinfos[i];
                if (i > mPids.size()) {
                    if (DEBUG) Log.e(TAG, "update: unknown process info received: " + dinfo);
                    break;
                }
                final long pid = mPids.get(i).intValue();
                final ProcessMemInfo info = mData.get(pid);
                info.pss[info.head] = info.currentPss = dinfo.getTotalPss();
                info.uss[info.head] = info.currentUss = dinfo.getTotalPrivateDirty();
                info.head = (info.head + 1) % info.pss.length;
                if (info.currentPss > info.max) info.max = info.currentPss;
                if (info.currentUss > info.max) info.max = info.currentUss;
                if (info.currentPss == 0) {
                    if (DEBUG) Log.v(TAG, "update: pid " + pid + " has pss=0, it probably died");
                    mData.remove(pid);
                }
            }
            for (int i = mPids.size() - 1; i >= 0; i--) {
                final long pid = mPids.get(i).intValue();
                if (mData.get(pid) == null) {
                    mPids.remove(i);
                    updatePidsArrayL();
                }
            }
        }
        if (mQSTile != null) mQSTile.update();
    }

    private void setTile(MemoryTile tile) {
        mQSTile = tile;
        if (tile != null) tile.update();
    }

    private static String formatBytes(long b) {
        String[] SUFFIXES = {"B", "K", "M", "G", "T"};
        int i;
        for (i = 0; i < SUFFIXES.length; i++) {
            if (b < 1024) break;
            b /= 1024;
        }
        return b + SUFFIXES[i];
    }

    private Intent dumpHprofAndGetShareIntent() {
        return mDumpTruck.captureHeaps(getTrackedProcesses()).createShareIntent();
    }

    @Override
    public void dump(@Nullable FileDescriptor fd, PrintWriter pw, @Nullable String[] args) {
        pw.println("GarbageMonitor params:");
        pw.println(String.format("   mHeapLimit=%d KB", mHeapLimit));
        pw.println(String.format("   GARBAGE_INSPECTION_INTERVAL=%d (%.1f mins)",
                GARBAGE_INSPECTION_INTERVAL,
                (float) GARBAGE_INSPECTION_INTERVAL / DateUtils.MINUTE_IN_MILLIS));
        final float htiMins = HEAP_TRACK_INTERVAL / DateUtils.MINUTE_IN_MILLIS;
        pw.println(String.format("   HEAP_TRACK_INTERVAL=%d (%.1f mins)",
                HEAP_TRACK_INTERVAL,
                htiMins));
        pw.println(String.format("   HEAP_TRACK_HISTORY_LEN=%d (%.1f hr total)",
                HEAP_TRACK_HISTORY_LEN,
                (float) HEAP_TRACK_HISTORY_LEN * htiMins / 60f));

        pw.println("GarbageMonitor tracked processes:");

        for (long pid : mPids) {
            final ProcessMemInfo pmi = mData.get(pid);
            if (pmi != null) {
                pmi.dump(fd, pw, args);
            }
        }
    }


    private static class MemoryIconDrawable extends Drawable {
        long pss, limit;
        final Drawable baseIcon;
        final Paint paint = new Paint();
        final float dp;

        MemoryIconDrawable(Context context) {
            baseIcon = context.getDrawable(R.drawable.ic_memory).mutate();
            dp = context.getResources().getDisplayMetrics().density;
            paint.setColor(QSTileImpl.getColorForState(context, STATE_ACTIVE));
        }

        public void setPss(long pss) {
            if (pss != this.pss) {
                this.pss = pss;
                invalidateSelf();
            }
        }

        public void setLimit(long limit) {
            if (limit != this.limit) {
                this.limit = limit;
                invalidateSelf();
            }
        }

        @Override
        public void draw(Canvas canvas) {
            baseIcon.draw(canvas);

            if (limit > 0 && pss > 0) {
                float frac = Math.min(1f, (float) pss / limit);

                final Rect bounds = getBounds();
                canvas.translate(bounds.left + 8 * dp, bounds.top + 5 * dp);
                //android:pathData="M16.0,5.0l-8.0,0.0l0.0,14.0l8.0,0.0z"
                canvas.drawRect(0, 14 * dp * (1 - frac), 8 * dp + 1, 14 * dp + 1, paint);
            }
        }

        @Override
        public void setBounds(int left, int top, int right, int bottom) {
            super.setBounds(left, top, right, bottom);
            baseIcon.setBounds(left, top, right, bottom);
        }

        @Override
        public int getIntrinsicHeight() {
            return baseIcon.getIntrinsicHeight();
        }

        @Override
        public int getIntrinsicWidth() {
            return baseIcon.getIntrinsicWidth();
        }

        @Override
        public void setAlpha(int i) {
            baseIcon.setAlpha(i);
        }

        @Override
        public void setColorFilter(ColorFilter colorFilter) {
            baseIcon.setColorFilter(colorFilter);
            paint.setColorFilter(colorFilter);
        }

        @Override
        public void setTint(int tint) {
            super.setTint(tint);
            baseIcon.setTint(tint);
        }

        @Override
        public void setTintList(ColorStateList tint) {
            super.setTintList(tint);
            baseIcon.setTintList(tint);
        }

        @Override
        public void setTintMode(PorterDuff.Mode tintMode) {
            super.setTintMode(tintMode);
            baseIcon.setTintMode(tintMode);
        }

        @Override
        public int getOpacity() {
            return PixelFormat.TRANSLUCENT;
        }
    }

    private static class MemoryGraphIcon extends QSTile.Icon {
        long pss, limit;

        public void setPss(long pss) {
            this.pss = pss;
        }

        public void setHeapLimit(long limit) {
            this.limit = limit;
        }

        @Override
        public Drawable getDrawable(Context context) {
            final MemoryIconDrawable drawable = new MemoryIconDrawable(context);
            drawable.setPss(pss);
            drawable.setLimit(limit);
            return drawable;
        }
    }

    public static class MemoryTile extends QSTileImpl<QSTile.State> {
        public static final String TILE_SPEC = "dbg:mem";

        // Tell QSTileHost.java to toss this into the default tileset?
        public static final boolean ADD_TO_DEFAULT_ON_DEBUGGABLE_BUILDS = true;

        private final GarbageMonitor gm;
        private ProcessMemInfo pmi;
        private boolean dumpInProgress;

        @Inject
        public MemoryTile(QSHost host) {
            super(host);
            gm = SystemUIFactory.getInstance().getRootComponent().createGarbageMonitor();
        }

        @Override
        public State newTileState() {
            return new QSTile.State();
        }

        @Override
        public Intent getLongClickIntent() {
            return new Intent();
        }

        @Override
        protected void handleClick() {
            if (dumpInProgress) return;

            dumpInProgress = true;
            refreshState();
            new Thread("HeapDumpThread") {
                @Override
                public void run() {
                    try {
                        // wait for animations & state changes
                        Thread.sleep(500);
                    } catch (InterruptedException ignored) { }
                    final Intent shareIntent = gm.dumpHprofAndGetShareIntent();
                    mHandler.post(() -> {
                        dumpInProgress = false;
                        refreshState();
                        getHost().collapsePanels();
                        mContext.startActivity(shareIntent);
                    });
                }
            }.start();
        }

        @Override
        public int getMetricsCategory() {
            return VIEW_UNKNOWN;
        }

        @Override
        public void handleSetListening(boolean listening) {
            if (gm != null) gm.setTile(listening ? this : null);

            final ActivityManager am = mContext.getSystemService(ActivityManager.class);
            if (listening && gm.mHeapLimit > 0) {
                am.setWatchHeapLimit(1024 * gm.mHeapLimit); // why is this in bytes?
            } else {
                am.clearWatchHeapLimit();
            }
        }

        @Override
        public CharSequence getTileLabel() {
            return getState().label;
        }

        @Override
        protected void handleUpdateState(State state, Object arg) {
            pmi = gm.getMemInfo(Process.myPid());
            final MemoryGraphIcon icon = new MemoryGraphIcon();
            icon.setHeapLimit(gm.mHeapLimit);
            state.state = dumpInProgress ? STATE_UNAVAILABLE : STATE_ACTIVE;
            state.label = dumpInProgress
                    ? "Dumping..."
                    : mContext.getString(R.string.heap_dump_tile_name);
            if (pmi != null) {
                icon.setPss(pmi.currentPss);
                state.secondaryLabel =
                        String.format(
                                "pss: %s / %s",
                                formatBytes(pmi.currentPss * 1024),
                                formatBytes(gm.mHeapLimit * 1024));
            } else {
                icon.setPss(0);
                state.secondaryLabel = null;
            }
            state.icon = icon;
        }

        public void update() {
            refreshState();
        }

        public long getPss() {
            return pmi != null ? pmi.currentPss : 0;
        }

        public long getHeapLimit() {
            return gm != null ? gm.mHeapLimit : 0;
        }
    }

    /** */
    public static class ProcessMemInfo implements Dumpable {
        public long pid;
        public String name;
        public long startTime;
        public long currentPss, currentUss;
        public long[] pss = new long[HEAP_TRACK_HISTORY_LEN];
        public long[] uss = new long[HEAP_TRACK_HISTORY_LEN];
        public long max = 1;
        public int head = 0;

        public ProcessMemInfo(long pid, String name, long start) {
            this.pid = pid;
            this.name = name;
            this.startTime = start;
        }

        public long getUptime() {
            return System.currentTimeMillis() - startTime;
        }

        @Override
        public void dump(@Nullable FileDescriptor fd, PrintWriter pw, @Nullable String[] args) {
            pw.print("{ \"pid\": ");
            pw.print(pid);
            pw.print(", \"name\": \"");
            pw.print(name.replace('"', '-'));
            pw.print("\", \"start\": ");
            pw.print(startTime);
            pw.print(", \"pss\": [");
            // write pss values starting from the oldest, which is pss[head], wrapping around to
            // pss[(head-1) % pss.length]
            for (int i = 0; i < pss.length; i++) {
                if (i > 0) pw.print(",");
                pw.print(pss[(head + i) % pss.length]);
            }
            pw.print("], \"uss\": [");
            for (int i = 0; i < uss.length; i++) {
                if (i > 0) pw.print(",");
                pw.print(uss[(head + i) % uss.length]);
            }
            pw.println("] }");
        }
    }

    /** */
    public static class Service extends SystemUI implements Dumpable {
        private GarbageMonitor mGarbageMonitor;

        @Override
        public void start() {
            boolean forceEnable =
                    Settings.Secure.getInt(
                                    mContext.getContentResolver(), FORCE_ENABLE_LEAK_REPORTING, 0)
                            != 0;
            mGarbageMonitor = SystemUIFactory.getInstance().getRootComponent()
                   .createGarbageMonitor();
            if (LEAK_REPORTING_ENABLED || forceEnable) {
                mGarbageMonitor.startLeakMonitor();
            }
            if (HEAP_TRACKING_ENABLED || forceEnable) {
                mGarbageMonitor.startHeapTracking();
            }
        }

        @Override
        public void dump(@Nullable FileDescriptor fd, PrintWriter pw, @Nullable String[] args) {
            if (mGarbageMonitor != null) mGarbageMonitor.dump(fd, pw, args);
        }
    }

    private class BackgroundHeapCheckHandler extends Handler {
        BackgroundHeapCheckHandler(Looper onLooper) {
            super(onLooper);
            if (Looper.getMainLooper().equals(onLooper)) {
                throw new RuntimeException(
                        "BackgroundHeapCheckHandler may not run on the ui thread");
            }
        }

        @Override
        public void handleMessage(Message m) {
            switch (m.what) {
                case DO_GARBAGE_INSPECTION:
                    if (gcAndCheckGarbage()) {
                        postDelayed(GarbageMonitor.this::reinspectGarbageAfterGc, 100);
                    }

                    removeMessages(DO_GARBAGE_INSPECTION);
                    sendEmptyMessageDelayed(DO_GARBAGE_INSPECTION, GARBAGE_INSPECTION_INTERVAL);
                    break;

                case DO_HEAP_TRACK:
                    update();
                    removeMessages(DO_HEAP_TRACK);
                    sendEmptyMessageDelayed(DO_HEAP_TRACK, HEAP_TRACK_INTERVAL);
                    break;
            }
        }
    }
}
