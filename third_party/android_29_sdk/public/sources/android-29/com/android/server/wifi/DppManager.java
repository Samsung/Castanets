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

import static android.net.wifi.WifiManager.EASY_CONNECT_NETWORK_ROLE_AP;

import android.content.Context;
import android.hardware.wifi.supplicant.V1_2.DppAkm;
import android.hardware.wifi.supplicant.V1_2.DppFailureCode;
import android.hardware.wifi.supplicant.V1_2.DppNetRole;
import android.hardware.wifi.supplicant.V1_2.DppProgressCode;
import android.net.wifi.EasyConnectStatusCallback;
import android.net.wifi.IDppCallback;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiManager;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;
import android.util.Log;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.util.WakeupMessage;
import com.android.server.wifi.WifiNative.DppEventCallback;

/**
 * DPP Manager class
 * Implements the DPP Initiator APIs and callbacks
 */
public class DppManager {
    private static final String TAG = "DppManager";
    public Handler mHandler;

    private DppRequestInfo mDppRequestInfo = null;
    private final WifiNative mWifiNative;
    private String mClientIfaceName;
    private boolean mVerboseLoggingEnabled;
    WifiConfigManager mWifiConfigManager;
    private final Context mContext;
    @VisibleForTesting
    public WakeupMessage mDppTimeoutMessage = null;
    private final Clock mClock;
    private static final String DPP_TIMEOUT_TAG = TAG + " Request Timeout";
    private static final int DPP_TIMEOUT_MS = 40_000; // 40 seconds
    private final DppMetrics mDppMetrics;

    private final DppEventCallback mDppEventCallback = new DppEventCallback() {
        @Override
        public void onSuccessConfigReceived(WifiConfiguration newWifiConfiguration) {
            mHandler.post(() -> {
                DppManager.this.onSuccessConfigReceived(newWifiConfiguration);
            });
        }

        @Override
        public void onSuccessConfigSent() {
            mHandler.post(() -> {
                DppManager.this.onSuccessConfigSent();
            });
        }

        @Override
        public void onProgress(int dppStatusCode) {
            mHandler.post(() -> {
                DppManager.this.onProgress(dppStatusCode);
            });
        }

        @Override
        public void onFailure(int dppStatusCode) {
            mHandler.post(() -> {
                DppManager.this.onFailure(dppStatusCode);
            });
        }
    };

    DppManager(Looper looper, WifiNative wifiNative, WifiConfigManager wifiConfigManager,
            Context context, DppMetrics dppMetrics) {
        mHandler = new Handler(looper);
        mWifiNative = wifiNative;
        mWifiConfigManager = wifiConfigManager;
        mWifiNative.registerDppEventCallback(mDppEventCallback);
        mContext = context;
        mClock = new Clock();
        mDppMetrics = dppMetrics;

        // Setup timer
        mDppTimeoutMessage = new WakeupMessage(mContext, mHandler,
                DPP_TIMEOUT_TAG, () -> {
            timeoutDppRequest();
        });
    }

    private static String encodeStringToHex(String str) {
        if ((str.length() > 1) && (str.charAt(0) == '"') && (str.charAt(str.length() - 1) == '"')) {
            // Remove the surrounding quotes
            str = str.substring(1, str.length() - 1);

            // Convert to Hex
            char[] charsArray = str.toCharArray();
            StringBuffer hexBuffer = new StringBuffer();
            for (int i = 0; i < charsArray.length; i++) {
                hexBuffer.append(Integer.toHexString((int) charsArray[i]));
            }
            return hexBuffer.toString();
        }
        return str;
    }

    private void timeoutDppRequest() {
        logd("DPP timeout");

        if (mDppRequestInfo == null) {
            Log.e(TAG, "DPP timeout with no request info");
            return;
        }

        // Clean up supplicant resources
        if (!mWifiNative.stopDppInitiator(mClientIfaceName)) {
            Log.e(TAG, "Failed to stop DPP Initiator");
        }

        // Clean up resources and let the caller know about the timeout
        onFailure(DppFailureCode.TIMEOUT);
    }

    /**
     * Start DPP request in Configurator-Initiator mode. The goal of this call is to send the
     * selected Wi-Fi configuration to a remote peer so it could join that network.
     *
     * @param uid                 User ID
     * @param binder              Binder object
     * @param enrolleeUri         The Enrollee URI, scanned externally (e.g. via QR code)
     * @param selectedNetworkId   The selected Wi-Fi network ID to be sent
     * @param enrolleeNetworkRole Network role of remote enrollee: STA or AP
     * @param callback            DPP Callback object
     */
    public void startDppAsConfiguratorInitiator(int uid, IBinder binder,
            String enrolleeUri, int selectedNetworkId,
            @WifiManager.EasyConnectNetworkRole int enrolleeNetworkRole, IDppCallback callback) {
        mDppMetrics.updateDppConfiguratorInitiatorRequests();
        if (mDppRequestInfo != null) {
            try {
                Log.e(TAG, "DPP request already in progress");
                Log.e(TAG, "Ongoing request UID: " + mDppRequestInfo.uid + ", new UID: "
                        + uid);

                mDppMetrics.updateDppFailure(EasyConnectStatusCallback
                        .EASY_CONNECT_EVENT_FAILURE_BUSY);
                // On going DPP. Call the failure callback directly
                callback.onFailure(EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_BUSY);
            } catch (RemoteException e) {
                // Empty
            }
            return;
        }

        mClientIfaceName = mWifiNative.getClientInterfaceName();
        if (mClientIfaceName == null) {
            try {
                Log.e(TAG, "Wi-Fi client interface does not exist");
                // On going DPP. Call the failure callback directly
                mDppMetrics.updateDppFailure(EasyConnectStatusCallback
                        .EASY_CONNECT_EVENT_FAILURE_GENERIC);
                callback.onFailure(EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_GENERIC);
            } catch (RemoteException e) {
                // Empty
            }
            return;
        }

        WifiConfiguration selectedNetwork = mWifiConfigManager
                .getConfiguredNetworkWithoutMasking(selectedNetworkId);

        if (selectedNetwork == null) {
            try {
                Log.e(TAG, "Selected network is null");
                // On going DPP. Call the failure callback directly
                mDppMetrics.updateDppFailure(EasyConnectStatusCallback
                        .EASY_CONNECT_EVENT_FAILURE_INVALID_NETWORK);
                callback.onFailure(EasyConnectStatusCallback
                        .EASY_CONNECT_EVENT_FAILURE_INVALID_NETWORK);
            } catch (RemoteException e) {
                // Empty
            }
            return;
        }

        String password = null;
        String psk = null;
        int securityAkm;

        // Currently support either SAE mode or PSK mode
        if (selectedNetwork.allowedKeyManagement.get(WifiConfiguration.KeyMgmt.SAE)) {
            // SAE
            password = selectedNetwork.preSharedKey;
            securityAkm = DppAkm.SAE;
        } else if (selectedNetwork.allowedKeyManagement.get(WifiConfiguration.KeyMgmt.WPA_PSK)) {
            if (selectedNetwork.preSharedKey.matches(String.format("[0-9A-Fa-f]{%d}", 64))) {
                // PSK
                psk = selectedNetwork.preSharedKey;
            } else {
                // Passphrase
                password = selectedNetwork.preSharedKey;
            }
            securityAkm = DppAkm.PSK;
        } else {
            try {
                // Key management must be either PSK or SAE
                Log.e(TAG, "Key management must be either PSK or SAE");
                mDppMetrics.updateDppFailure(EasyConnectStatusCallback
                        .EASY_CONNECT_EVENT_FAILURE_INVALID_NETWORK);
                callback.onFailure(
                        EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_INVALID_NETWORK);
            } catch (RemoteException e) {
                // Empty
            }
            return;
        }

        mDppRequestInfo = new DppRequestInfo();
        mDppRequestInfo.uid = uid;
        mDppRequestInfo.binder = binder;
        mDppRequestInfo.callback = callback;

        if (!linkToDeath(mDppRequestInfo)) {
            // Notify failure and clean up
            onFailure(EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_GENERIC);
            return;
        }

        logd("Interface " + mClientIfaceName + ": Initializing URI: " + enrolleeUri);

        mDppRequestInfo.startTime = mClock.getElapsedSinceBootMillis();
        mDppTimeoutMessage.schedule(mDppRequestInfo.startTime + DPP_TIMEOUT_MS);

        // Send Enrollee URI and get a peer ID
        int peerId = mWifiNative.addDppPeerUri(mClientIfaceName, enrolleeUri);

        if (peerId < 0) {
            Log.e(TAG, "DPP add URI failure");

            // Notify failure and clean up
            onFailure(DppFailureCode.INVALID_URI);
            return;
        }
        mDppRequestInfo.peerId = peerId;

        // Auth init
        logd("Authenticating");

        String ssidEncoded = encodeStringToHex(selectedNetwork.SSID);
        String passwordEncoded = null;

        if (password != null) {
            passwordEncoded = encodeStringToHex(selectedNetwork.preSharedKey);
        }

        if (!mWifiNative.startDppConfiguratorInitiator(mClientIfaceName,
                mDppRequestInfo.peerId, 0, ssidEncoded, passwordEncoded, psk,
                enrolleeNetworkRole == EASY_CONNECT_NETWORK_ROLE_AP ? DppNetRole.AP
                        : DppNetRole.STA,
                securityAkm)) {
            Log.e(TAG, "DPP Start Configurator Initiator failure");

            // Notify failure and clean up
            onFailure(DppFailureCode.FAILURE);
            return;
        }

        logd("Success: Started DPP Initiator with peer ID "
                + mDppRequestInfo.peerId);
    }

    /**
     * Start DPP request in Enrollee-Initiator mode. The goal of this call is to receive a
     * Wi-Fi configuration object from the peer configurator in order to join a network.
     *
     * @param uid             User ID
     * @param binder          Binder object
     * @param configuratorUri The Configurator URI, scanned externally (e.g. via QR code)
     * @param callback        DPP Callback object
     */
    public void startDppAsEnrolleeInitiator(int uid, IBinder binder,
            String configuratorUri, IDppCallback callback) {
        mDppMetrics.updateDppEnrolleeInitiatorRequests();
        if (mDppRequestInfo != null) {
            try {
                Log.e(TAG, "DPP request already in progress");
                Log.e(TAG, "Ongoing request UID: " + mDppRequestInfo.uid + ", new UID: "
                        + uid);

                mDppMetrics.updateDppFailure(EasyConnectStatusCallback
                        .EASY_CONNECT_EVENT_FAILURE_BUSY);
                // On going DPP. Call the failure callback directly
                callback.onFailure(EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_BUSY);
            } catch (RemoteException e) {
                // Empty
            }
            return;
        }

        mDppRequestInfo = new DppRequestInfo();
        mDppRequestInfo.uid = uid;
        mDppRequestInfo.binder = binder;
        mDppRequestInfo.callback = callback;

        if (!linkToDeath(mDppRequestInfo)) {
            // Notify failure and clean up
            onFailure(DppFailureCode.FAILURE);
            return;
        }

        mDppRequestInfo.startTime = mClock.getElapsedSinceBootMillis();
        mDppTimeoutMessage.schedule(mDppRequestInfo.startTime + DPP_TIMEOUT_MS);

        mClientIfaceName = mWifiNative.getClientInterfaceName();
        logd("Interface " + mClientIfaceName + ": Initializing URI: " + configuratorUri);

        // Send Configurator URI and get a peer ID
        int peerId = mWifiNative.addDppPeerUri(mClientIfaceName, configuratorUri);

        if (peerId < 0) {
            Log.e(TAG, "DPP add URI failure");
            onFailure(DppFailureCode.INVALID_URI);
            return;
        }
        mDppRequestInfo.peerId = peerId;

        // Auth init
        logd("Authenticating");

        if (!mWifiNative.startDppEnrolleeInitiator(mClientIfaceName, mDppRequestInfo.peerId,
                0)) {
            Log.e(TAG, "DPP Start Enrollee Initiator failure");

            // Notify failure and clean up
            onFailure(DppFailureCode.FAILURE);
            return;
        }

        logd("Success: Started DPP Initiator with peer ID "
                + mDppRequestInfo.peerId);
    }

    /**
     * Stop a current DPP session
     *
     * @param uid User ID
     */
    public void stopDppSession(int uid) {
        if (mDppRequestInfo == null) {
            logd("UID " + uid + " called stop DPP session with no active DPP session");
            return;
        }

        if (mDppRequestInfo.uid != uid) {
            Log.e(TAG, "UID " + uid + " called stop DPP session but UID " + mDppRequestInfo.uid
                    + " has started it");
            return;
        }

        // Clean up supplicant resources
        if (!mWifiNative.stopDppInitiator(mClientIfaceName)) {
            Log.e(TAG, "Failed to stop DPP Initiator");
        }

        cleanupDppResources();

        logd("Success: Stopped DPP Initiator");
    }

    private void cleanupDppResources() {
        logd("DPP clean up resources");
        if (mDppRequestInfo == null) {
            return;
        }

        // Cancel pending timeout
        mDppTimeoutMessage.cancel();

        // Remove the URI from the supplicant list
        if (!mWifiNative.removeDppUri(mClientIfaceName, mDppRequestInfo.peerId)) {
            Log.e(TAG, "Failed to remove DPP URI ID " + mDppRequestInfo.peerId);
        }

        mDppRequestInfo.binder.unlinkToDeath(mDppRequestInfo.dr, 0);

        mDppRequestInfo = null;
    }

    private static class DppRequestInfo {
        public int uid;
        public IBinder binder;
        public IBinder.DeathRecipient dr;
        public int peerId;
        public IDppCallback callback;
        public long startTime;

        @Override
        public String toString() {
            return new StringBuilder("DppRequestInfo: uid=").append(uid).append(", binder=").append(
                    binder).append(", dr=").append(dr)
                    .append(", callback=").append(callback)
                    .append(", peerId=").append(peerId).toString();
        }
    }

    /**
     * Enable vervose logging from DppManager
     *
     * @param verbose 0 to disable verbose logging, or any other value to enable.
     */
    public void enableVerboseLogging(int verbose) {
        mVerboseLoggingEnabled = verbose != 0 ? true : false;
    }

    private void onSuccessConfigReceived(WifiConfiguration newWifiConfiguration) {
        try {
            logd("onSuccessConfigReceived");

            if (mDppRequestInfo != null) {
                long now = mClock.getElapsedSinceBootMillis();
                mDppMetrics.updateDppOperationTime((int) (now - mDppRequestInfo.startTime));

                NetworkUpdateResult networkUpdateResult = mWifiConfigManager
                        .addOrUpdateNetwork(newWifiConfiguration, mDppRequestInfo.uid);

                if (networkUpdateResult.isSuccess()) {
                    mDppMetrics.updateDppEnrolleeSuccess();
                    mDppRequestInfo.callback.onSuccessConfigReceived(
                            networkUpdateResult.getNetworkId());
                } else {
                    Log.e(TAG, "DPP configuration received, but failed to update network");
                    mDppMetrics.updateDppFailure(EasyConnectStatusCallback
                            .EASY_CONNECT_EVENT_FAILURE_CONFIGURATION);
                    mDppRequestInfo.callback.onFailure(EasyConnectStatusCallback
                            .EASY_CONNECT_EVENT_FAILURE_CONFIGURATION);
                }
            } else {
                Log.e(TAG, "Unexpected null Wi-Fi configuration object");
            }
        } catch (RemoteException e) {
            Log.e(TAG, "Callback failure");
        }

        // Success, DPP is complete. Clear the DPP session automatically
        cleanupDppResources();
    }

    private void onSuccessConfigSent() {
        try {
            if (mDppRequestInfo == null) {
                Log.e(TAG, "onDppSuccessConfigSent event without a request information object");
                return;
            }

            logd("onSuccessConfigSent");

            long now = mClock.getElapsedSinceBootMillis();
            mDppMetrics.updateDppOperationTime((int) (now - mDppRequestInfo.startTime));

            mDppMetrics.updateDppConfiguratorSuccess(
                    EasyConnectStatusCallback.EASY_CONNECT_EVENT_SUCCESS_CONFIGURATION_SENT);
            mDppRequestInfo.callback.onSuccess(
                    EasyConnectStatusCallback.EASY_CONNECT_EVENT_SUCCESS_CONFIGURATION_SENT);

        } catch (RemoteException e) {
            Log.e(TAG, "Callback failure");
        }

        // Success, DPP is complete. Clear the DPP session automatically
        cleanupDppResources();
    }

    private void onProgress(int dppStatusCode) {
        try {
            if (mDppRequestInfo == null) {
                Log.e(TAG, "onProgress event without a request information object");
                return;
            }

            logd("onProgress: " + dppStatusCode);

            int dppProgressCode;

            // Convert from HAL codes to WifiManager/user codes
            switch (dppStatusCode) {
                case DppProgressCode.AUTHENTICATION_SUCCESS:
                    dppProgressCode = EasyConnectStatusCallback
                            .EASY_CONNECT_EVENT_PROGRESS_AUTHENTICATION_SUCCESS;
                    break;

                case DppProgressCode.RESPONSE_PENDING:
                    dppProgressCode = EasyConnectStatusCallback
                            .EASY_CONNECT_EVENT_PROGRESS_RESPONSE_PENDING;
                    break;

                default:
                    Log.e(TAG, "onProgress: unknown code " + dppStatusCode);
                    return;
            }

            mDppRequestInfo.callback.onProgress(dppProgressCode);

        } catch (RemoteException e) {
            Log.e(TAG, "Callback failure");
        }
    }

    private void onFailure(int dppStatusCode) {
        try {
            if (mDppRequestInfo == null) {
                Log.e(TAG, "onFailure event without a request information object");
                return;
            }

            logd("OnFailure: " + dppStatusCode);

            long now = mClock.getElapsedSinceBootMillis();
            mDppMetrics.updateDppOperationTime((int) (now - mDppRequestInfo.startTime));

            int dppFailureCode;

            // Convert from HAL codes to WifiManager/user codes
            switch (dppStatusCode) {
                case DppFailureCode.INVALID_URI:
                    dppFailureCode =
                            EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_INVALID_URI;
                    break;

                case DppFailureCode.AUTHENTICATION:
                    dppFailureCode =
                            EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_AUTHENTICATION;
                    break;

                case DppFailureCode.NOT_COMPATIBLE:
                    dppFailureCode =
                            EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_NOT_COMPATIBLE;
                    break;

                case DppFailureCode.CONFIGURATION:
                    dppFailureCode =
                            EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_CONFIGURATION;
                    break;

                case DppFailureCode.BUSY:
                    dppFailureCode = EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_BUSY;
                    break;

                case DppFailureCode.TIMEOUT:
                    dppFailureCode = EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_TIMEOUT;
                    break;

                case DppFailureCode.NOT_SUPPORTED:
                    dppFailureCode =
                            EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_NOT_SUPPORTED;
                    break;

                case DppFailureCode.FAILURE:
                default:
                    dppFailureCode = EasyConnectStatusCallback.EASY_CONNECT_EVENT_FAILURE_GENERIC;
                    break;
            }

            mDppMetrics.updateDppFailure(dppFailureCode);
            mDppRequestInfo.callback.onFailure(dppFailureCode);

        } catch (RemoteException e) {
            Log.e(TAG, "Callback failure");
        }

        // All failures are fatal, clear the DPP session
        cleanupDppResources();
    }

    private void logd(String message) {
        if (mVerboseLoggingEnabled) {
            Log.d(TAG, message);
        }
    }

    private boolean linkToDeath(DppRequestInfo dppRequestInfo) {
        // register for binder death
        dppRequestInfo.dr = new IBinder.DeathRecipient() {
            @Override
            public void binderDied() {
                if (dppRequestInfo == null) {
                    return;
                }

                logd("binderDied: uid=" + dppRequestInfo.uid);

                mHandler.post(() -> {
                    cleanupDppResources();
                });
            }
        };

        try {
            dppRequestInfo.binder.linkToDeath(dppRequestInfo.dr, 0);
        } catch (RemoteException e) {
            Log.e(TAG, "Error on linkToDeath - " + e);
            dppRequestInfo.dr = null;
            return false;
        }

        return true;
    }
}
