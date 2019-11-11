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
import android.os.Build;
import android.os.IBinder;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationCompat.Builder;
import android.util.Log;

public class MeerkatServerService extends Service {
    static {
        System.loadLibrary("meerkat_server_lib");
    }

    private static final String TAG = "MeerkatServer";
    private static final int MEERKAT_NOTIFICATION_ID = 100;
    private static final String MEERKAT_CHANNEL_ID = "meekat_server";
    private static final String MEERKAT_CHANNEL_GROUP_ID = "meekat";
    private static final String CASTANETS_PACKAGE_NAME = "org.chromium.chrome";
    private static final String ACTION_NOTIFICATION_CLICKED = "com.samsung.android.meerkat.NOTIFICATION_CLICKED";

    private static Context sApplicationContext;
    private static Thread mMainThread;

    public static class Receiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (Intent.ACTION_BOOT_COMPLETED.equals(intent.getAction())) {
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

    @Override
    public void onCreate() {
        super.onCreate();
        sApplicationContext = this.getApplicationContext();
        startForegroundInternal();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (mMainThread == null) {
            mMainThread = new Thread(new Runnable() {
                @Override
                public void run() {
                    nativeStartServer();
                }
            });
            mMainThread.start();
        }
        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onDestroy() {
        if (mMainThread != null) {
            nativeStopServer();
            mMainThread = null;
        }
        super.onDestroy();
    }

    public static boolean startCastanetsRenderer(String args) {
        PackageManager packageManager = sApplicationContext.getPackageManager();
        Intent intent = packageManager.getLaunchIntentForPackage(
                sApplicationContext.getPackageName());
        intent.putExtra("args", args);
        try {
            sApplicationContext.startActivity(intent);
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "Fail to start Chrome renderer!");
            return false;
        }
        return true;
    }

    private void startForegroundInternal() {
        Intent intent = new Intent(ACTION_NOTIFICATION_CLICKED);
        intent.setClass(sApplicationContext, MeerkatServerService.Receiver.class);
        PendingIntent contentIntent = PendingIntent.getBroadcast(
                sApplicationContext, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);

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

    private native int nativeStartServer();
    private native void nativeStopServer();
}
