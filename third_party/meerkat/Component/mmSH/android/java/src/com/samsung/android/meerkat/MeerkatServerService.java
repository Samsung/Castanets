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

import com.google.android.gms.auth.api.signin.GoogleSignIn;
import com.google.android.gms.auth.api.signin.GoogleSignInAccount;
import com.google.android.gms.auth.api.signin.GoogleSignInClient;
import com.google.android.gms.auth.api.signin.GoogleSignInOptions;
import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.Task;

import com.google.api.client.googleapis.auth.oauth2.GoogleIdToken;
import com.google.api.client.googleapis.auth.oauth2.GoogleIdTokenVerifier;
import com.google.api.client.http.javanet.NetHttpTransport;
import com.google.api.client.json.jackson2.JacksonFactory;

import java.io.IOException;
import java.security.GeneralSecurityException;
import java.util.Collections;
import java.util.Timer;
import java.util.TimerTask;

public class MeerkatServerService extends Service {
    static {
        System.loadLibrary("meerkat_server_lib");
    }

    private static final String TAG = "MeerkatServerService";
    private static final int MEERKAT_NOTIFICATION_ID = 100;
    private static final String MEERKAT_CHANNEL_ID = "meekat_server";
    private static final String MEERKAT_CHANNEL_GROUP_ID = "meekat";
    private static final String CASTANETS_PACKAGE_NAME = "org.chromium.chrome";
    private static final String ACTION_NOTIFICATION_CLICKED = "com.samsung.android.meerkat.NOTIFICATION_CLICKED";
    private static final String CLIENT_ID = "401503586848-3ajf0semvlclbffipcuh7oc7qr2kattk.apps.googleusercontent.com";
    private static final int ID_TOKEN_REFRESH_RATE = 60 * 1000 * 10;  // 10 minutes

    private static Context sApplicationContext;
    private static String sIdToken;
    private static final Object sIdTokenLock = new Object();

    private GoogleSignInClient mGoogleSignInClient;
    private Thread mMainThread;
    private Timer mRefreshTimer;
    private TimerTask mRefreshTimerTask;

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
        GoogleSignInOptions gso = new GoogleSignInOptions.Builder(GoogleSignInOptions.DEFAULT_SIGN_IN)
                .requestIdToken(CLIENT_ID)
                .requestEmail()
                .build();
        mGoogleSignInClient = GoogleSignIn.getClient(sApplicationContext, gso);

        mRefreshTimer = new Timer();

        startForegroundInternal();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        refreshIdToken();
        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onDestroy() {
        if (mRefreshTimerTask != null) {
            mRefreshTimer.cancel();
            mRefreshTimerTask = null;
        }
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

    public static String getIdToken() {
        synchronized (sIdTokenLock) {
            return sIdToken;
        }
    }

    public static boolean verifyIdToken(String idTokenString) {
        try {
            GoogleIdTokenVerifier verifier = new GoogleIdTokenVerifier.Builder(
                    new NetHttpTransport(), JacksonFactory.getDefaultInstance())
                    .setAudience(Collections.singletonList(CLIENT_ID))
                    .build();

            GoogleIdToken idToken = verifier.verify(idTokenString);
            if (idToken == null) {
                Log.e(TAG, "Invalid token.");
                return false;
            }
        } catch (GeneralSecurityException | IOException e) {
            Log.e(TAG, "Fail to verify ID token - " + e.getMessage());
            return false;
        }

        return true;
    }

    private void refreshIdToken() {
        mRefreshTimerTask = new TimerTask() {
            @Override
            public void run() {
                mGoogleSignInClient.silentSignIn()
                        .addOnCompleteListener(new OnCompleteListener<GoogleSignInAccount>() {
                            @Override
                            public void onComplete(Task<GoogleSignInAccount> task) {
                                handleSignInResult(task);
                            }
                        });
            }
        };
        mRefreshTimer.schedule(mRefreshTimerTask, 0, ID_TOKEN_REFRESH_RATE);
    }

    private void handleSignInResult(Task<GoogleSignInAccount> completedTask) {
        try {
            GoogleSignInAccount account = completedTask.getResult(ApiException.class);
            synchronized (sIdTokenLock) {
                sIdToken = account.getIdToken();
                Log.wtf(TAG, "handleSignInResult:id token " + sIdToken);
            }
	    if (mMainThread == null) {
	        mMainThread = new Thread(new Runnable() {
	            @Override
                    public void run() {
                        nativeStartServer();
                    }
	        });
	        mMainThread.start();
	    }
        } catch (ApiException e) {
            Log.w(TAG, "handleSignInResult:failed code=" + e.getStatusCode());
            stopSelf();
        }
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
