package com.samsung.offloadworker;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

public class OffloadReceiver extends BroadcastReceiver {

  private static String TAG = "OffloadReceiver";
  public static final String ACTION_ALLOW = "com.samsung.offloadworker.ALLOW";
  public static final String ACTION_ALWAYS_ALLOW = "com.samsung.offloadworker.ALWAYS_ALLOW";
  public static final String ACTION_BLOCK = "com.samsung.offloadworker.BLOCK";

  @Override
  public void onReceive(Context context, Intent intent) {
    Log.i(TAG, "onReceive" + intent.toString());
    if (ACTION_ALLOW.equals(intent.getAction())) {
      if (OffloadService.getInstance() != null) {
        OffloadService.getInstance().sendConfirmResult(
            intent.getIntExtra("notification_id", -1), true);
      }
    } else if (ACTION_ALWAYS_ALLOW.equals(intent.getAction())) {
      if (OffloadService.getInstance() != null) {
        OffloadService.getInstance().addFeatureAllow(intent.getStringExtra("client_id"),
                                                     intent.getStringExtra("feature"));
        OffloadService.getInstance().sendConfirmResult(
            intent.getIntExtra("notification_id", -1), true);
      }
    } else if (ACTION_BLOCK.equals(intent.getAction())) {
      if (OffloadService.getInstance() != null) {
        OffloadService.getInstance().sendConfirmResult(
            intent.getIntExtra("notification_id", -1), false);
      }
    }
  }

}
