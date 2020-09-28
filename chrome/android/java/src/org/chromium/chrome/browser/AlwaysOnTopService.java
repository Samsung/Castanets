package org.chromium.chrome.browser;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.os.IBinder;
import android.view.Gravity;
import android.view.WindowManager;
import android.widget.ImageView;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunActivity;

public class AlwaysOnTopService extends Service {
    private static final String TAG = "AlwaysOnTopService";
    private static final int NOTIFICATION = R.string.app_name;
    private static final String APP_NAME = "CASTANETS";

    private ImageView mImgView;
    private NotificationManager mNotificationManager;

    @Override
    public IBinder onBind(Intent arg0) {
        return null;
    }

    @Override
    public void onCreate() {
        super.onCreate();

        WindowManager windowManager = (WindowManager) getSystemService(WINDOW_SERVICE);

        mImgView = new ImageView(this);
        mImgView.setImageResource(R.drawable.fre_product_logo);
        mImgView.setScaleType(ImageView.ScaleType.FIT_XY);

        WindowManager.LayoutParams myParam = new WindowManager.LayoutParams(0, 0, // Hide the view
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE, PixelFormat.TRANSLUCENT);
        myParam.gravity = Gravity.LEFT | Gravity.TOP;
        windowManager.addView(mImgView, myParam);

        mNotificationManager = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        showNotification();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mImgView != null) {
            ((WindowManager) getSystemService(WINDOW_SERVICE)).removeView(mImgView);
            mImgView = null;
        }

        if (mNotificationManager != null) {
            mNotificationManager.cancel(NOTIFICATION);
            mNotificationManager = null;
        }
    }

    private void showNotification() {
        Log.i(TAG, "showNotification Castanets");

        NotificationChannel channel =
                new NotificationChannel(APP_NAME, APP_NAME, NotificationManager.IMPORTANCE_DEFAULT);
        ((NotificationManager) getSystemService(getApplicationContext().NOTIFICATION_SERVICE))
                .createNotificationChannel(channel);

        Intent notificationIntent = new Intent(this, FirstRunActivity.class);
        notificationIntent.setAction(Intent.ACTION_MAIN);
        notificationIntent.addCategory(Intent.CATEGORY_LAUNCHER);
        PendingIntent pendingIntent = PendingIntent.getActivity(
                this, 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);

        // Set the info for the views that show in the notification panel.
        final Notification notification =
                new Notification.Builder(this, APP_NAME)
                        .setSmallIcon(R.drawable.fre_product_logo)
                        .setWhen(System.currentTimeMillis()) // the time stamp
                        .setContentTitle("Castanets - Offloading") // the label of the entry
                        .setContentText("is running.") // the contents of the entry
                        .setContentIntent(pendingIntent)
                        .build();

        startForeground(NOTIFICATION, notification);
        mNotificationManager.notify(NOTIFICATION, notification);
    }
}
