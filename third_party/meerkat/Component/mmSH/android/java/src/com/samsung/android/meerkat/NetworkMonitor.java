/*
 * Copyright 2021 Samsung Electronics Co., Ltd
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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.wifi.WifiManager;
import android.util.Log;

public class NetworkMonitor extends BroadcastReceiver {

  private static final String TAG = "MeerkatNetworkMonitor";

  public static abstract class NetworkStateChangedEventListener {
    public abstract void onChanged(boolean isWifiConnected);
  }

  private ConnectivityManager connectivityManager;
  private Context context;
  private boolean isLastWifiConnected;
  private NetworkStateChangedEventListener changedEventListener;

  NetworkMonitor(Context context) {
    this.context = context;
    connectivityManager = context.getSystemService(ConnectivityManager.class);
  }

  @Override
  public void onReceive(Context context, Intent intent) {
    boolean isConnected = isWifiConnected();
    Log.i(TAG, "onReceive() " + isLastWifiConnected + ", " + isConnected);
    if (isLastWifiConnected != isConnected) {
      changedEventListener.onChanged(isConnected);
      isLastWifiConnected = isConnected;
    }
  }

  public void start(NetworkStateChangedEventListener listener) {
    isLastWifiConnected = isWifiConnected();
    if (listener != null) {
      listener.onChanged(isLastWifiConnected);
      changedEventListener = listener;
      context.registerReceiver(this, new IntentFilter(connectivityManager.CONNECTIVITY_ACTION));
      Log.i(TAG, "start() Receiver is registered.");
    }
  }

  public void stop() {
    if (changedEventListener!= null)
      context.unregisterReceiver(this);
    Log.i(TAG, "stop() Receiver is unregistered.");
  }

  public boolean isWifiConnected() {
    NetworkInfo networkInfo = connectivityManager.getNetworkInfo(ConnectivityManager.TYPE_WIFI);
    Log.d(TAG, networkInfo.isConnected() + ", NetworkInfo: " + networkInfo);
    return networkInfo.isConnected();
  }

}
