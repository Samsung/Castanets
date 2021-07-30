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
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.preference.PreferenceManager;
import android.provider.Settings;
import android.support.annotation.Nullable;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationCompat.Builder;
import android.util.Log;

import com.samsung.android.meerkat.NetworkMonitor;
import com.samsung.android.meerkat.NetworkMonitor.NetworkStateChangedEventListener;

import java.io.File;
import java.lang.reflect.Method;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.json.simple.JSONObject;
import org.json.simple.parser.JSONParser;
import org.json.simple.parser.ParseException;

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
    private static final String PREF_KEY_MULTICAST_ADDRESS = "discovery_multicast_address";

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
    private static JSONObject cachedCapability;
    private static final Object cachedCapabilityLock = new Object();

    private Thread meerkatRunner;
    private SharedPreferences sharedPreferences;
    private String multicastAddress;

    private NetworkMonitor networkMonitor;

    public static class Receiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            Log.i(TAG, "onReceive() : " + intent.getAction());
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
                startActivity(context, "com.samsung.offloadsetting.SettingsActivity", null);
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
        networkMonitor = new NetworkMonitor(this);
        startForegroundInternal();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.e(TAG, "onStartCommand : " + intent);
        if (sharedPreferences == null) {
            sharedPreferences = applicationContext.getSharedPreferences(
                    "com.samsung.android.meerkat.CAPABILITY", Context.MODE_PRIVATE);
            sharedPreferences.registerOnSharedPreferenceChangeListener(this);
            initCapability(sharedPreferences.getAll());
        }

        try {
            Bundle bundle = this.getContentResolver().call(
                    Uri.parse("content://com.samsung.offloadsetting.SettingProvider"),
                    PREF_KEY_MULTICAST_ADDRESS, null, null);
            String value = bundle.getString(PREF_KEY_MULTICAST_ADDRESS);
            Log.i(TAG,
                    "SettingProvider key : " + PREF_KEY_MULTICAST_ADDRESS + ", value : " + value);
            if (value != null && value != multicastAddress) {
                stopMeerkatThread();
            }
            multicastAddress = value;
        } catch (Exception e) {
            Log.e(TAG, "SettingProvider error : " + e);
        }

        networkMonitor.start(new NetworkMonitor.NetworkStateChangedEventListener() {
            @Override
            public void onChanged(boolean isWifiConnected) {
                Log.i(TAG, "NetworkStateChanged : " + isWifiConnected);
                if (isWifiConnected) {
                    startMeerkatThread();
                    startForegroundInternal("Meerkat", "Discovery service is running.");
                } else {
                    stopMeerkatThread();
                    startForegroundInternal("Discovery service has been stopped.", "It requires Wi-fi connection for discovery service.");
                }
            }
        });
        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onDestroy() {
        networkMonitor.stop();
        sharedPreferences.unregisterOnSharedPreferenceChangeListener(this);
        stopMeerkatThread();
        super.onDestroy();
    }

    @Override
    public void onSharedPreferenceChanged(SharedPreferences prefs, String key) {
        Log.i(TAG, "key : " + key + ", value : " + prefs.getString(key, ""));
        try {
            JSONParser parser = new JSONParser();
            JSONObject jsonObj = (JSONObject) parser.parse(prefs.getString(key, ""));
            updateCapability(key, jsonObj);
        } catch (ParseException e) {
            Log.e(TAG, "JSON parse error : " + e);
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

    private void startMeerkatThread() {
        if (meerkatRunner == null) {
            meerkatRunner = new Thread(new Runnable() {
                @Override
                public void run() {
                    nativeStartServer(
                            shouldUseDebugIni() ? MEERKAT_INI_PATH : null, multicastAddress);
                }
            });
            meerkatRunner.start();
        }
    }

    private void stopMeerkatThread() {
        if (meerkatRunner != null) {
            Log.i(TAG, "Stopping MeerkatServer thread...");
            nativeStopServer();
            try {
                meerkatRunner.join(1000);
            } catch (Exception e) {
                Log.e(TAG, "meerkatRunner.join() failed. " + e);
            }
            Log.i(TAG, "MeerkatServer thread stopped");
            meerkatRunner = null;
        }
    }

    public static boolean startCastanetsRenderer(String args) {
        // Launch OffloadService directly.
        if (args.matches("(^|.*\\s)--type=offloadworker($|\\s.*)")) {
          Matcher matcher = Pattern.compile("--signaling-server=(\\S+)").matcher(args);
          if (matcher.find() && launchOffloadService(matcher.group(1))) {
            Log.i(TAG, "Launched OffloadService. URL:" + matcher.group(1));
            return true;
          }
        }

        return startActivity(applicationContext, "com.google.android.apps.chrome.Main", args);
    }

    private static boolean startActivity(Context context, String activity, String args) {
        Intent intent = new Intent();
        intent.setClassName(context.getPackageName(), activity);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (args != null)
            intent.putExtra("args", args);
        try {
            context.startActivity(intent);
        } catch (Exception e) {
            Log.e(TAG, e + " Fail to start Chrome renderer!");
            return false;
        }
        return true;
    }

    /**
     * Launch OffloadService
     */
    private static boolean launchOffloadService(String url) {
      try {
          final Class<?> offloadService =
                  Class.forName("com.samsung.offloadworker.OffloadService");
          final Method startService =
                  offloadService.getMethod("startService", Context.class, String.class, String.class);
          startService.invoke(null, applicationContext, url, "discovery");
      } catch (Exception e) {
          Log.e(TAG, "Exception while launching OffloadService.", e);
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
            return cachedCapability.toString();
        }
    }

    private static void initCapability(Map<String, ?> capabilities) {
        synchronized(cachedCapabilityLock) {
            JSONParser parser = new JSONParser();
            JSONObject newCapability = new JSONObject();
            for (Map.Entry<String, ?> capability : capabilities.entrySet()) {
                try {
                    Log.d("Capability [", capability.getKey().toString() + "] : " + capability.getValue().toString());
                    JSONObject jsonObj = (JSONObject) parser.parse(capability.getValue().toString());
                    newCapability.put(capability.getKey().toString(), jsonObj);
                } catch (ParseException e) {
                    Log.e(TAG, "JSON parse error : " + e);
                }
            }
            cachedCapability = newCapability;
        }
    }

    private static void updateCapability(String key, JSONObject jsonObj) {
        synchronized(cachedCapabilityLock) {
            cachedCapability.remove(key);
            cachedCapability.put(key, jsonObj);
        }
    }

    private void startForegroundInternal(String title, String text) {
        Intent intent = new Intent(ACTION_NOTIFICATION_CLICKED);
        intent.setClass(applicationContext, MeerkatServerService.Receiver.class);
        PendingIntent contentIntent = PendingIntent.getBroadcast(
                applicationContext, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);

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

    private void startForegroundInternal() {
        startForegroundInternal("Meerkat", "Meerkat server is running.");
    }

    private native int nativeStartServer(@Nullable String iniPath, @Nullable String multicastAddress);
    private native void nativeStopServer();
}
