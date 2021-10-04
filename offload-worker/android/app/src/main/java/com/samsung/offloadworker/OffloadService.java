package com.samsung.offloadworker;

import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.graphics.PixelFormat;
import android.graphics.Point;
import android.os.Build;
import android.os.IBinder;
import android.preference.PreferenceManager;
import android.support.v4.app.NotificationCompat;
import android.support.v4.content.ContextCompat;
import android.util.Log;
import android.view.Gravity;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Toast;

import com.google.gson.Gson;
import com.google.gson.reflect.TypeToken;

import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;

public class OffloadService extends Service {

  private static final String TAG = "OffloadService";
  private static final String CHANNEL_ID = "offload_service";
  private static final String CONFIRM_CHANNEL_ID = "offload_service_confirm";
  private static final String ALLOW_LIST_FILE = "allow_list.json";

  private static OffloadService instance;
  private WorkerWebView workerWebView;
  private Boolean workerWebViewVisibility;
  NotificationCompat.Builder builder;
  private List<FeatureAllow> featureAllowList = new ArrayList<>();

  class FeatureAllow {
    public final String clientId;
    public List<String> features;

    public FeatureAllow(String clientId, List<String> features) {
      this.clientId = clientId;
      this.features = features;
    }

    @Override
    public String toString() {
      return "clientId: " + clientId + " features: " + features.toString();
    }
  }

  public static void startService(Context context, String url, String caller) {
    Log.i(TAG, "startService");
    Intent startIntent = new Intent(context, OffloadService.class);
    if (url != null) {
      startIntent.putExtra("url", url);
    }
    if (caller != null) {
      startIntent.putExtra("caller", caller);
    }
    ContextCompat.startForegroundService((Context) context, startIntent);
  }

  public static void stopService(Context context) {
    Log.i(TAG, "stopService");
    Intent stopIntent = new Intent(context, OffloadService.class);
    context.stopService(stopIntent);
  }

  public static OffloadService getInstance() {
    return instance;
  }

  @Override
  public int onStartCommand(Intent intent, int flags, int startId) {
    Boolean visible = intent.hasExtra("visible") ? intent.getBooleanExtra("visible", false) : null;
    String url = intent.getStringExtra("url");
    String caller = intent.hasExtra("caller") ? intent.getStringExtra("caller") : "discovery";
    boolean useDiscovery = PreferenceManager.getDefaultSharedPreferences(this)
        .getBoolean(getString(R.string.key_use_discovery), false);
    Log.i(TAG,
        "onStartCommand " + intent + ", url:" + url + ", caller:" + caller + ", useDiscovery:"
            + useDiscovery);
    if (PreferenceManager.getDefaultSharedPreferences(this)
        .getBoolean(getString(R.string.key_enable_offload), false) && (!"discovery".equals(caller)
        || useDiscovery)) {
      attachWebView(visible, url, caller);
    } else {
      Toast.makeText(this.getApplicationContext(),
          String.format("Ignored the service request to %s.", url), Toast.LENGTH_SHORT).show();
    }
    return START_NOT_STICKY;
  }

  @Override
  public IBinder onBind(Intent intent) {
    return null;
  }

  private WindowManager.LayoutParams overlayLayoutParams(boolean visible) {
    WindowManager.LayoutParams params = new WindowManager.LayoutParams(
        ViewGroup.LayoutParams.WRAP_CONTENT,
        ViewGroup.LayoutParams.WRAP_CONTENT,
        Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
            ? WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
            : WindowManager.LayoutParams.TYPE_PHONE,
        0,
        PixelFormat.TRANSLUCENT);
    params.gravity = Gravity.BOTTOM;
    params.x = 0;
    params.y = 0;

    if (visible) {
      Point outSize = new Point();
      ((WindowManager) getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay()
          .getSize(outSize);
      params.width = outSize.x < outSize.y ? outSize.x : outSize.y;
      params.height = params.width;
      params.flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
    } else {
      params.width = 0;
      params.height = 0;
      params.flags = WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN
          | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
          | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE;
    }
    return params;
  }

  @Override
  public void onCreate() {
    Log.i(TAG, "onCreate");
    super.onCreate();
    instance = this;
    startForegroundInternal();
    readAllowList(this.getApplicationContext());
  }

  @Override
  public void onDestroy() {
    Log.i(TAG, "onDestroy");
    writeAllowList(this.getApplicationContext());
    detachWebView();
    instance = null;
    super.onDestroy();
  }

  private void startForegroundInternal() {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
      NotificationChannel serviceChannel = new NotificationChannel(
          CHANNEL_ID,
          "Offload Service Channel",
          NotificationManager.IMPORTANCE_DEFAULT);
      NotificationManager manager = (NotificationManager) getSystemService(
          NotificationManager.class);
      manager.createNotificationChannel(serviceChannel);
      builder = new NotificationCompat.Builder(this, CHANNEL_ID);
    } else {
      builder = new NotificationCompat.Builder(this);
    }

    try {
      Class<?> settingsActivityClass =
              Class.forName("com.samsung.offloadsetting.SettingsActivity");
      Intent notificationIntent = new Intent(this, settingsActivityClass);
      PendingIntent pendingIntent = PendingIntent
          .getActivity(this, 0, notificationIntent, 0);
      builder.setContentTitle("Offload Service")
          .setContentText("Worker service is started.")
          .setSmallIcon(R.mipmap.ic_launcher_round)
          .setOnlyAlertOnce(true)
          .setContentIntent(pendingIntent);
    } catch (Exception e) {
      Toast.makeText(getApplicationContext(), "Failed to get SettingsActivity class.",
              Toast.LENGTH_SHORT).show();
    }


    startForeground(1, builder.build());
  }

  private void attachWebView(Boolean visible, String serverAddress, String caller) {
    if (workerWebView == null) {
      Log.i(TAG, "Attach WebView to window");
      workerWebView = new WorkerWebView(this);
      ((WindowManager) getSystemService(Context.WINDOW_SERVICE)).addView(workerWebView.getView(),
          overlayLayoutParams(false));
    }
    if (!"initialize".equals(serverAddress)) {
      updateWebView(visible, serverAddress, "setting".equals(caller));
      setNotificationContentText("OffloadWorker is running.");
    } else {
      setNotificationContentText("OffloadWorker is initializing.");
    }
  }

  public void updateWebView(Boolean visible, String serverAddress, Boolean forceConnect) {
    boolean v = visible != null ? visible
        : PreferenceManager.getDefaultSharedPreferences(this)
            .getBoolean(getString(R.string.key_enable_debug), false);
    if (workerWebView != null) {
      if (workerWebViewVisibility == null || workerWebViewVisibility != v) {
        Log.i(TAG, "Update WebView visible=" + v);
        workerWebViewVisibility = v;
        ((WindowManager) getSystemService(Context.WINDOW_SERVICE))
            .updateViewLayout(workerWebView.getView(),
                              overlayLayoutParams(workerWebViewVisibility));
      }
      workerWebView.connect(serverAddress, forceConnect != null ? forceConnect : false);
    }
  }

  private void detachWebView() {
    if (workerWebView != null) {
      Log.i(TAG, "Detach WebView from window");
      ((WindowManager) getSystemService(Context.WINDOW_SERVICE))
          .removeViewImmediate(workerWebView.getView());
      workerWebView.destroy();
      workerWebView = null;
    }
    setNotificationContentText("OffloadWorker is closed.");
  }

  public void setNotificationContentText(String text) {
    if (builder != null) {
      builder.setContentText(text);
      startForeground(1, builder.build());
    }
  }

  public void sendConfirmNotification(String args) {
    Gson gson = new Gson();
    Map argMap = gson.fromJson(args, Map.class);
    int notificationId = (int)(double) argMap.get("id");
    String feature = String.valueOf(argMap.get("feature"));
    String clientId = String.valueOf(argMap.get("clientId"));
    String deviceName = String.valueOf(argMap.get("deviceName"));

    if (featureAllowed(clientId, feature)) {
      Log.i(TAG, feature + " is always allowed for " + clientId);
      sendConfirmResult(notificationId, true);
      return;
    }

    NotificationManager manager =
        (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
    NotificationCompat.Builder builder;
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
      NotificationChannel serviceChannel = new NotificationChannel(
          CONFIRM_CHANNEL_ID,
          "Offload Service Confirm Channel",
          NotificationManager.IMPORTANCE_HIGH);
      manager.createNotificationChannel(serviceChannel);
      builder = new NotificationCompat.Builder(this, CONFIRM_CHANNEL_ID);
    } else {
      builder = new NotificationCompat.Builder(this);
    }

    builder.setContentTitle("Offload Service")
        .setContentText(deviceName + " wants to use your " + feature)
        .setSmallIcon(R.mipmap.ic_launcher_round)
        .setAutoCancel(true)
        .addAction(0, "Allow", getConfirmPendingIntent(
            notificationId, clientId, feature, OffloadReceiver.ACTION_ALLOW))
        .addAction(0, "Always Allow", getConfirmPendingIntent(
            notificationId, clientId, feature, OffloadReceiver.ACTION_ALWAYS_ALLOW))
        .addAction(0, "Block", getConfirmPendingIntent(
            notificationId, clientId, feature, OffloadReceiver.ACTION_BLOCK))
        .setTimeoutAfter(10000)
        .setDeleteIntent(getConfirmPendingIntent(
            notificationId, clientId, feature, OffloadReceiver.ACTION_BLOCK));
    Log.i(TAG, "Send confirm notification id: " + notificationId);
    manager.notify(notificationId, builder.build());
  }

  private PendingIntent getConfirmPendingIntent(int id, String clientId, String feature, String action) {
    Intent intent = new Intent(this, OffloadReceiver.class);
    intent.setAction(action);
    intent.putExtra("notification_id", id);
    intent.putExtra("client_id", clientId);
    intent.putExtra("feature", feature);
    return PendingIntent.getBroadcast(this, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
  }

  public void sendConfirmResult(int id, boolean allowed) {
    Log.i(TAG, "Result of notification id: " + id + " allowed: " + allowed);
    if (workerWebView != null) {
      workerWebView.sendConfirmResult(id, allowed);
    }

    NotificationManager manager =
        (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
    manager.cancel(id);
  }

  public void addFeatureAllow(String clientId, String feature) {
    Log.e(TAG, "addFeatureAllow: " + clientId + " feature: " + feature);
    for (FeatureAllow allow : featureAllowList) {
      if (allow.clientId.equals(clientId)) {
        allow.features.add(feature);
        return;
      }
    }
    featureAllowList.add(new FeatureAllow(clientId, new ArrayList<>(Arrays.asList(feature))));
  }

  public void removeFeatureAllow(String clientId, String feature) {
    Log.e(TAG, "removeFeatureAllow: " + clientId + " feature: " + feature);
    for (FeatureAllow allow : featureAllowList) {
      if (allow.clientId.equals(clientId)) {
        allow.features.remove(feature);
        return;
      }
    }
  }

  public List<String> getClientId() {
    List<String> retVal = new ArrayList<>();
    for (FeatureAllow allow : featureAllowList) {
      retVal.add(allow.clientId);
    }
    return retVal;
  }

  public List<String> getFeatures(String clientId) {
    for (FeatureAllow allow : featureAllowList) {
      if (allow.clientId.equals(clientId)) {
        return allow.features;
      }
    }
    return Collections.emptyList();
  }

  private boolean featureAllowed(String clientId, String feature) {
    for (FeatureAllow allow : featureAllowList) {
      if (allow.clientId.equals(clientId)) {
        return allow.features.contains(feature);
      }
    }
    return false;
  }

  private void readAllowList(Context context) {
    try {
      InputStreamReader reader =
          new InputStreamReader(context.openFileInput(ALLOW_LIST_FILE), "UTF-8");
      Gson gson = new Gson();
      featureAllowList = gson.fromJson(reader, new TypeToken<List<FeatureAllow>>(){}.getType());
      reader.close();
    } catch (IOException e) {
      Log.e(TAG, "Fail to read allow list: " + e);
      featureAllowList = new ArrayList<>();
    }
  }

  private void writeAllowList(Context context) {
    try {
      OutputStreamWriter writer = new OutputStreamWriter(
          context.openFileOutput(ALLOW_LIST_FILE, Context.MODE_PRIVATE), "UTF-8");
      Gson gson = new Gson();
      gson.toJson(featureAllowList, writer);
      writer.close();
    } catch (IOException e) {
      Log.e(TAG, "Fail to write allow list: " + e);
    }
  }

}
