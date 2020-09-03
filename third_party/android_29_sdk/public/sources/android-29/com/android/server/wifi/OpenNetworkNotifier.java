/*
 * Copyright (C) 2018 The Android Open Source Project
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
 * limitations under the License.
 */

package com.android.server.wifi;

import android.content.Context;
import android.os.Looper;
import android.provider.Settings;

import com.android.internal.messages.nano.SystemMessageProto.SystemMessage;
import com.android.server.wifi.nano.WifiMetricsProto;

/**
 * This class handles the "open wi-fi network available" notification
 *
 * NOTE: These API's are not thread safe and should only be used from ClientModeImpl thread.
 */
public class OpenNetworkNotifier extends AvailableNetworkNotifier {
    public static final String TAG = "WifiOpenNetworkNotifier";
    private static final String STORE_DATA_IDENTIFIER = "OpenNetworkNotifierBlacklist";
    private static final String TOGGLE_SETTINGS_NAME =
            Settings.Global.WIFI_NETWORKS_AVAILABLE_NOTIFICATION_ON;

    public OpenNetworkNotifier(
            Context context,
            Looper looper,
            FrameworkFacade framework,
            Clock clock,
            WifiMetrics wifiMetrics,
            WifiConfigManager wifiConfigManager,
            WifiConfigStore wifiConfigStore,
            ClientModeImpl clientModeImpl,
            ConnectToNetworkNotificationBuilder connectToNetworkNotificationBuilder) {
        super(TAG, STORE_DATA_IDENTIFIER, TOGGLE_SETTINGS_NAME,
                SystemMessage.NOTE_NETWORK_AVAILABLE,
                WifiMetricsProto.ConnectionEvent.NOMINATOR_OPEN_NETWORK_AVAILABLE,
                context, looper, framework, clock,
                wifiMetrics, wifiConfigManager, wifiConfigStore, clientModeImpl,
                connectToNetworkNotificationBuilder);
    }
}
