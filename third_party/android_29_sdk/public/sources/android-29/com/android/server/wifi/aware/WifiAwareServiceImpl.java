/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.server.wifi.aware;

import android.Manifest;
import android.app.AppOpsManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.database.ContentObserver;
import android.hardware.wifi.V1_0.NanStatusType;
import android.net.wifi.aware.Characteristics;
import android.net.wifi.aware.ConfigRequest;
import android.net.wifi.aware.DiscoverySession;
import android.net.wifi.aware.IWifiAwareDiscoverySessionCallback;
import android.net.wifi.aware.IWifiAwareEventCallback;
import android.net.wifi.aware.IWifiAwareMacAddressProvider;
import android.net.wifi.aware.IWifiAwareManager;
import android.net.wifi.aware.PublishConfig;
import android.net.wifi.aware.SubscribeConfig;
import android.os.Binder;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.ResultReceiver;
import android.os.ShellCallback;
import android.provider.Settings;
import android.util.Log;
import android.util.SparseArray;
import android.util.SparseIntArray;

import com.android.server.wifi.Clock;
import com.android.server.wifi.FrameworkFacade;
import com.android.server.wifi.WifiInjector;
import com.android.server.wifi.util.WifiPermissionsUtil;
import com.android.server.wifi.util.WifiPermissionsWrapper;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.util.List;

/**
 * Implementation of the IWifiAwareManager AIDL interface. Performs validity
 * (permission and clientID-UID mapping) checks and delegates execution to the
 * WifiAwareStateManager singleton handler. Limited state to feedback which has to
 * be provided instantly: client and session IDs.
 */
public class WifiAwareServiceImpl extends IWifiAwareManager.Stub {
    private static final String TAG = "WifiAwareService";
    private static final boolean VDBG = false; // STOPSHIP if true
    /* package */ boolean mDbg = false;

    private Context mContext;
    private AppOpsManager mAppOps;
    private WifiPermissionsUtil mWifiPermissionsUtil;
    private WifiAwareStateManager mStateManager;
    private WifiAwareShellCommand mShellCommand;

    private final Object mLock = new Object();
    private final SparseArray<IBinder.DeathRecipient> mDeathRecipientsByClientId =
            new SparseArray<>();
    private int mNextClientId = 1;
    private final SparseIntArray mUidByClientId = new SparseIntArray();

    public WifiAwareServiceImpl(Context context) {
        mContext = context.getApplicationContext();
        mAppOps = (AppOpsManager) context.getSystemService(Context.APP_OPS_SERVICE);
    }

    /**
     * Proxy for the final native call of the parent class. Enables mocking of
     * the function.
     */
    public int getMockableCallingUid() {
        return getCallingUid();
    }

    /**
     * Start the service: allocate a new thread (for now), start the handlers of
     * the components of the service.
     */
    public void start(HandlerThread handlerThread, WifiAwareStateManager awareStateManager,
            WifiAwareShellCommand awareShellCommand, WifiAwareMetrics awareMetrics,
            WifiPermissionsUtil wifiPermissionsUtil, WifiPermissionsWrapper permissionsWrapper,
            FrameworkFacade frameworkFacade, WifiAwareNativeManager wifiAwareNativeManager,
            WifiAwareNativeApi wifiAwareNativeApi,
            WifiAwareNativeCallback wifiAwareNativeCallback) {
        Log.i(TAG, "Starting Wi-Fi Aware service");

        mWifiPermissionsUtil = wifiPermissionsUtil;
        mStateManager = awareStateManager;
        mShellCommand = awareShellCommand;
        mStateManager.start(mContext, handlerThread.getLooper(), awareMetrics, wifiPermissionsUtil,
                permissionsWrapper, new Clock());

        frameworkFacade.registerContentObserver(mContext,
                Settings.Global.getUriFor(Settings.Global.WIFI_VERBOSE_LOGGING_ENABLED), true,
                new ContentObserver(new Handler(handlerThread.getLooper())) {
                    @Override
                    public void onChange(boolean selfChange) {
                        enableVerboseLogging(frameworkFacade.getIntegerSetting(mContext,
                                Settings.Global.WIFI_VERBOSE_LOGGING_ENABLED, 0), awareStateManager,
                                wifiAwareNativeManager, wifiAwareNativeApi,
                                wifiAwareNativeCallback);
                    }
                });
        enableVerboseLogging(frameworkFacade.getIntegerSetting(mContext,
                Settings.Global.WIFI_VERBOSE_LOGGING_ENABLED, 0), awareStateManager,
                wifiAwareNativeManager, wifiAwareNativeApi, wifiAwareNativeCallback);
    }

    private void enableVerboseLogging(int verbose, WifiAwareStateManager awareStateManager,
            WifiAwareNativeManager wifiAwareNativeManager, WifiAwareNativeApi wifiAwareNativeApi,
            WifiAwareNativeCallback wifiAwareNativeCallback) {
        boolean dbg;

        if (verbose > 0) {
            dbg = true;
        } else {
            dbg = false;
        }
        if (VDBG) {
            dbg = true; // just override
        }

        mDbg = dbg;
        awareStateManager.mDbg = dbg;
        if (awareStateManager.mDataPathMgr != null) { // needed for unit tests
            awareStateManager.mDataPathMgr.mDbg = dbg;
            WifiInjector.getInstance().getWifiMetrics().getWifiAwareMetrics().mDbg = dbg;
        }
        wifiAwareNativeCallback.mDbg = dbg;
        wifiAwareNativeManager.mDbg = dbg;
        wifiAwareNativeApi.mDbg = dbg;
    }

    /**
     * Start/initialize portions of the service which require the boot stage to be complete.
     */
    public void startLate() {
        Log.i(TAG, "Late initialization of Wi-Fi Aware service");

        mStateManager.startLate();
    }

    @Override
    public boolean isUsageEnabled() {
        enforceAccessPermission();

        return mStateManager.isUsageEnabled();
    }

    @Override
    public Characteristics getCharacteristics() {
        enforceAccessPermission();

        return mStateManager.getCapabilities() == null ? null
                : mStateManager.getCapabilities().toPublicCharacteristics();
    }

    @Override
    public void connect(final IBinder binder, String callingPackage,
            IWifiAwareEventCallback callback, ConfigRequest configRequest,
            boolean notifyOnIdentityChanged) {
        enforceAccessPermission();
        enforceChangePermission();

        final int uid = getMockableCallingUid();
        mAppOps.checkPackage(uid, callingPackage);

        if (callback == null) {
            throw new IllegalArgumentException("Callback must not be null");
        }
        if (binder == null) {
            throw new IllegalArgumentException("Binder must not be null");
        }

        if (notifyOnIdentityChanged) {
            enforceLocationPermission(callingPackage, getMockableCallingUid());
        }

        if (configRequest != null) {
            enforceNetworkStackPermission();
        } else {
            configRequest = new ConfigRequest.Builder().build();
        }
        configRequest.validate();


        int pid = getCallingPid();

        final int clientId;
        synchronized (mLock) {
            clientId = mNextClientId++;
        }

        if (mDbg) {
            Log.v(TAG, "connect: uid=" + uid + ", clientId=" + clientId + ", configRequest"
                    + configRequest + ", notifyOnIdentityChanged=" + notifyOnIdentityChanged);
        }

        IBinder.DeathRecipient dr = new IBinder.DeathRecipient() {
            @Override
            public void binderDied() {
                if (mDbg) Log.v(TAG, "binderDied: clientId=" + clientId);
                binder.unlinkToDeath(this, 0);

                synchronized (mLock) {
                    mDeathRecipientsByClientId.delete(clientId);
                    mUidByClientId.delete(clientId);
                }

                mStateManager.disconnect(clientId);
            }
        };

        try {
            binder.linkToDeath(dr, 0);
        } catch (RemoteException e) {
            Log.e(TAG, "Error on linkToDeath - " + e);
            try {
                callback.onConnectFail(NanStatusType.INTERNAL_FAILURE);
            } catch (RemoteException e1) {
                Log.e(TAG, "Error on onConnectFail()");
            }
            return;
        }

        synchronized (mLock) {
            mDeathRecipientsByClientId.put(clientId, dr);
            mUidByClientId.put(clientId, uid);
        }

        mStateManager.connect(clientId, uid, pid, callingPackage, callback, configRequest,
                notifyOnIdentityChanged);
    }

    @Override
    public void disconnect(int clientId, IBinder binder) {
        enforceAccessPermission();
        enforceChangePermission();

        int uid = getMockableCallingUid();
        enforceClientValidity(uid, clientId);
        if (mDbg) Log.v(TAG, "disconnect: uid=" + uid + ", clientId=" + clientId);

        if (binder == null) {
            throw new IllegalArgumentException("Binder must not be null");
        }

        synchronized (mLock) {
            IBinder.DeathRecipient dr = mDeathRecipientsByClientId.get(clientId);
            if (dr != null) {
                binder.unlinkToDeath(dr, 0);
                mDeathRecipientsByClientId.delete(clientId);
            }
            mUidByClientId.delete(clientId);
        }

        mStateManager.disconnect(clientId);
    }

    @Override
    public void terminateSession(int clientId, int sessionId) {
        enforceAccessPermission();
        enforceChangePermission();

        int uid = getMockableCallingUid();
        enforceClientValidity(uid, clientId);
        if (VDBG) {
            Log.v(TAG, "terminateSession: sessionId=" + sessionId + ", uid=" + uid + ", clientId="
                    + clientId);
        }

        mStateManager.terminateSession(clientId, sessionId);
    }

    @Override
    public void publish(String callingPackage, int clientId, PublishConfig publishConfig,
            IWifiAwareDiscoverySessionCallback callback) {
        enforceAccessPermission();
        enforceChangePermission();

        int uid = getMockableCallingUid();
        mAppOps.checkPackage(uid, callingPackage);

        enforceLocationPermission(callingPackage, getMockableCallingUid());

        if (callback == null) {
            throw new IllegalArgumentException("Callback must not be null");
        }
        if (publishConfig == null) {
            throw new IllegalArgumentException("PublishConfig must not be null");
        }
        publishConfig.assertValid(mStateManager.getCharacteristics(),
                mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_WIFI_RTT));

        enforceClientValidity(uid, clientId);
        if (VDBG) {
            Log.v(TAG, "publish: uid=" + uid + ", clientId=" + clientId + ", publishConfig="
                    + publishConfig + ", callback=" + callback);
        }

        mStateManager.publish(clientId, publishConfig, callback);
    }

    @Override
    public void updatePublish(int clientId, int sessionId, PublishConfig publishConfig) {
        enforceAccessPermission();
        enforceChangePermission();

        if (publishConfig == null) {
            throw new IllegalArgumentException("PublishConfig must not be null");
        }
        publishConfig.assertValid(mStateManager.getCharacteristics(),
                mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_WIFI_RTT));

        int uid = getMockableCallingUid();
        enforceClientValidity(uid, clientId);
        if (VDBG) {
            Log.v(TAG, "updatePublish: uid=" + uid + ", clientId=" + clientId + ", sessionId="
                    + sessionId + ", config=" + publishConfig);
        }

        mStateManager.updatePublish(clientId, sessionId, publishConfig);
    }

    @Override
    public void subscribe(String callingPackage, int clientId, SubscribeConfig subscribeConfig,
            IWifiAwareDiscoverySessionCallback callback) {
        enforceAccessPermission();
        enforceChangePermission();

        int uid = getMockableCallingUid();
        mAppOps.checkPackage(uid, callingPackage);

        enforceLocationPermission(callingPackage, getMockableCallingUid());

        if (callback == null) {
            throw new IllegalArgumentException("Callback must not be null");
        }
        if (subscribeConfig == null) {
            throw new IllegalArgumentException("SubscribeConfig must not be null");
        }
        subscribeConfig.assertValid(mStateManager.getCharacteristics(),
                mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_WIFI_RTT));

        enforceClientValidity(uid, clientId);
        if (VDBG) {
            Log.v(TAG, "subscribe: uid=" + uid + ", clientId=" + clientId + ", config="
                    + subscribeConfig + ", callback=" + callback);
        }

        mStateManager.subscribe(clientId, subscribeConfig, callback);
    }

    @Override
    public void updateSubscribe(int clientId, int sessionId, SubscribeConfig subscribeConfig) {
        enforceAccessPermission();
        enforceChangePermission();

        if (subscribeConfig == null) {
            throw new IllegalArgumentException("SubscribeConfig must not be null");
        }
        subscribeConfig.assertValid(mStateManager.getCharacteristics(),
                mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_WIFI_RTT));

        int uid = getMockableCallingUid();
        enforceClientValidity(uid, clientId);
        if (VDBG) {
            Log.v(TAG, "updateSubscribe: uid=" + uid + ", clientId=" + clientId + ", sessionId="
                    + sessionId + ", config=" + subscribeConfig);
        }

        mStateManager.updateSubscribe(clientId, sessionId, subscribeConfig);
    }

    @Override
    public void sendMessage(int clientId, int sessionId, int peerId, byte[] message, int messageId,
            int retryCount) {
        enforceAccessPermission();
        enforceChangePermission();

        if (retryCount != 0) {
            enforceNetworkStackPermission();
        }

        if (message != null && message.length
                > mStateManager.getCharacteristics().getMaxServiceSpecificInfoLength()) {
            throw new IllegalArgumentException(
                    "Message length longer than supported by device characteristics");
        }
        if (retryCount < 0 || retryCount > DiscoverySession.getMaxSendRetryCount()) {
            throw new IllegalArgumentException("Invalid 'retryCount' must be non-negative "
                    + "and <= DiscoverySession.MAX_SEND_RETRY_COUNT");
        }

        int uid = getMockableCallingUid();
        enforceClientValidity(uid, clientId);
        if (VDBG) {
            Log.v(TAG,
                    "sendMessage: sessionId=" + sessionId + ", uid=" + uid + ", clientId="
                            + clientId + ", peerId=" + peerId + ", messageId=" + messageId
                            + ", retryCount=" + retryCount);
        }

        mStateManager.sendMessage(clientId, sessionId, peerId, message, messageId, retryCount);
    }

    @Override
    public void requestMacAddresses(int uid, List peerIds, IWifiAwareMacAddressProvider callback) {
        enforceNetworkStackPermission();

        mStateManager.requestMacAddresses(uid, peerIds, callback);
    }

    @Override
    public void onShellCommand(FileDescriptor in, FileDescriptor out, FileDescriptor err,
            String[] args, ShellCallback callback, ResultReceiver resultReceiver) {
        mShellCommand.exec(this, in, out, err, args, callback, resultReceiver);
    }

    @Override
    protected void dump(FileDescriptor fd, PrintWriter pw, String[] args) {
        if (mContext.checkCallingOrSelfPermission(
                android.Manifest.permission.DUMP) != PackageManager.PERMISSION_GRANTED) {
            pw.println("Permission Denial: can't dump WifiAwareService from pid="
                    + Binder.getCallingPid() + ", uid=" + Binder.getCallingUid());
            return;
        }
        pw.println("Wi-Fi Aware Service");
        synchronized (mLock) {
            pw.println("  mNextClientId: " + mNextClientId);
            pw.println("  mDeathRecipientsByClientId: " + mDeathRecipientsByClientId);
            pw.println("  mUidByClientId: " + mUidByClientId);
        }
        mStateManager.dump(fd, pw, args);
    }

    private void enforceClientValidity(int uid, int clientId) {
        synchronized (mLock) {
            int uidIndex = mUidByClientId.indexOfKey(clientId);
            if (uidIndex < 0 || mUidByClientId.valueAt(uidIndex) != uid) {
                throw new SecurityException("Attempting to use invalid uid+clientId mapping: uid="
                        + uid + ", clientId=" + clientId);
            }
        }
    }

    private void enforceAccessPermission() {
        mContext.enforceCallingOrSelfPermission(android.Manifest.permission.ACCESS_WIFI_STATE, TAG);
    }

    private void enforceChangePermission() {
        mContext.enforceCallingOrSelfPermission(android.Manifest.permission.CHANGE_WIFI_STATE, TAG);
    }

    private void enforceLocationPermission(String callingPackage, int uid) {
        mWifiPermissionsUtil.enforceLocationPermission(callingPackage, uid);
    }

    private void enforceNetworkStackPermission() {
        mContext.enforceCallingOrSelfPermission(Manifest.permission.NETWORK_STACK, TAG);
    }
}
