/*
 * Copyright 2019 Samsung Electronics Co., Ltd
 *
 * Licensed under the Flora License, Version 1.1 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://floralicense.org/license/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.samsung.android.meerkat;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.ActivityNotFoundException;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.IBinder;
import android.preference.PreferenceManager;
import android.provider.Settings;
import android.support.annotation.Nullable;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationCompat.Builder;
import android.util.Log;

import java.io.File;

public class MeerkatServerService extends Service
        implements SharedPreferences.OnSharedPreferenceChangeListener {
    private static final String TAG = "MeerkatServerService";
    private static final String MEERKAT_INI_PATH = "/data/local/tmp/meerkat/server.ini";
    private static final int MEERKAT_NOTIFICATION_ID = 100;
    private static final String MEERKAT_CHANNEL_ID = "meekat_server";
    private static final String MEERKAT_CHANNEL_GROUP_ID = "meekat";
    private static final String MEERKAT_LIBRARY_NAME = "meerkat_server_lib";
    private static final String ACTION_NOTIFICATION_CLICKED = "com.samsung.android.meerkat.NOTIFICATION_CLICKED";
    private static final String PREF_KEY_ENABLE_CASTANETS = "enable_castanets";

    static {
        try {
            System.loadLibrary(MEERKAT_LIBRARY_NAME);
        } catch (UnsatisfiedLinkError e) {
            // In a component build, the ".cr" suffix is added to each library name.
            Log.w(TAG,
                    "Couldn't load lib" + MEERKAT_LIBRARY_NAME + ".so, trying lib"
                            + MEERKAT_LIBRARY_NAME + ".cr.so");
            System.loadLibrary(MEERKAT_LIBRARY_NAME + ".cr");
        }
    }

    private static Context applicationContext;
    private static String cachedCapability;
    private static final Object cachedCapabilityLock = new Object();

    private Thread meerkatRunner;
    private SharedPreferences sharedPreferences;

    public static class Receiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (Intent.ACTION_BOOT_COMPLETED.equals(intent.getAction())) {
                if (!PreferenceManager.getDefaultSharedPreferences(context).getBoolean(
                            PREF_KEY_ENABLE_CASTANETS, false)) {
                    Log.i(TAG, "Castanets is Disabled.");
                    return;
                }
                Intent serviceIntent = new Intent(context, MeerkatServerService.class);
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    context.startForegroundService(serviceIntent);
                } else {
                    context.startService(serviceIntent);
                }
            } else if (ACTION_NOTIFICATION_CLICKED.equals(intent.getAction())) {
                context.stopService(new Intent(context, MeerkatServerService.class));
            }
        }
    }

    public static void startService(Context context) {
        context.startForegroundService(new Intent(context, MeerkatServerService.class));
    }

    @Override
    public void onCreate() {
        super.onCreate();
        applicationContext = this.getApplicationContext();
        startForegroundInternal();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        sharedPreferences = applicationContext.getSharedPreferences(
                "com.samsung.android.meerkat.CAPABILITY", Context.MODE_PRIVATE);
                sharedPreferences.registerOnSharedPreferenceChangeListener(this);
        setCapability(sharedPreferences.getString("capability", ""));

        if (meerkatRunner == null) {
            meerkatRunner = new Thread(new Runnable() {
                @Override
                public void run() {
                    nativeStartServer(shouldUseDebugIni() ? MEERKAT_INI_PATH : null);
                }
            });
            meerkatRunner.start();
        }

        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onDestroy() {
        sharedPreferences.unregisterOnSharedPreferenceChangeListener(this);
        if (meerkatRunner != null) {
            nativeStopServer();
            meerkatRunner = null;
        }
        super.onDestroy();
    }

    @Override
    public void onSharedPreferenceChanged(SharedPreferences prefs, String key) {
        if (key.equals("capability")) {
            setCapability(prefs.getString(key, ""));
        }
    }

    private static boolean shouldUseDebugIni() {
        boolean adbEnabled = Settings.System.getInt(applicationContext.getContentResolver(),
                Settings.System.ADB_ENABLED, 0) == 1;
        if (adbEnabled) {
            String debugApp = Settings.System.getString(applicationContext.getContentResolver(),
                    Settings.System.DEBUG_APP);
            File ini_file = new File(MEERKAT_INI_PATH);
            return (ini_file.exists() && ini_file.isFile()) &&
                    applicationContext.getPackageName().equals(debugApp);
        }
        return false;
    }

    public static boolean startCastanetsRenderer(String args) {
        PackageManager packageManager = applicationContext.getPackageManager();
        Intent intent = packageManager.getLaunchIntentForPackage(
                applicationContext.getPackageName());
        intent.putExtra("args", args);
        try {
            applicationContext.startActivity(intent);
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "Fail to start Chrome renderer!");
            return false;
        }
        return true;
    }

    public static String getIdToken() {
        return "server-token-sample";
    }

    public static boolean verifyIdToken(String idTokenString) {
        return true;
    }

    public static String getCapability() {
        synchronized(cachedCapabilityLock) {
            return cachedCapability;
        }
    }

    private static void setCapability(String capability) {
        synchronized(cachedCapabilityLock) {
            cachedCapability = capability;
        }
    }

    private void startForegroundInternal() {
        Intent intent = new Intent(ACTION_NOTIFICATION_CLICKED);
        intent.setClass(applicationContext, MeerkatServerService.Receiver.class);
        PendingIntent contentIntent = PendingIntent.getBroadcast(
                applicationContext, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);

        String title = "Meerkat";
        String text = "Meerkat server is running.";
        NotificationCompat.Builder builder;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(MEERKAT_CHANNEL_ID,
                    MEERKAT_CHANNEL_GROUP_ID,
                    NotificationManager.IMPORTANCE_LOW);

            channel.setShowBadge(false);
            ((NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE))
                    .createNotificationChannel(channel);
            builder = new Builder(this, MEERKAT_CHANNEL_ID);
        } else {
            builder = new Builder(this);
        }
        builder.setAutoCancel(true)
                .setContentIntent(contentIntent)
                .setContentTitle(title)
                .setContentText(text)
                .setSmallIcon(R.mipmap.ic_launcher)
                .setTicker(text)
                .setLocalOnly(true);

        NotificationCompat.BigTextStyle bigTextStyle =
                new NotificationCompat.BigTextStyle(builder);
        bigTextStyle.bigText(text);

        Notification notification = bigTextStyle.build();
        startForeground(MEERKAT_NOTIFICATION_ID, notification);
    }

    private native int nativeStartServer(@Nullable String iniPath);
    private native void nativeStopServer();
}
