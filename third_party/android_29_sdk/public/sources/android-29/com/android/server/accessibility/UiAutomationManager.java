/*
 ** Copyright 2017, The Android Open Source Project
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

package com.android.server.accessibility;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.accessibilityservice.IAccessibilityServiceClient;
import android.annotation.Nullable;
import android.app.UiAutomation;
import android.content.ComponentName;
import android.content.Context;
import android.os.Handler;
import android.os.IBinder;
import android.os.IBinder.DeathRecipient;
import android.os.RemoteException;
import android.util.Slog;
import android.view.accessibility.AccessibilityEvent;

import com.android.internal.util.DumpUtils;
import com.android.server.wm.WindowManagerInternal;

import java.io.FileDescriptor;
import java.io.PrintWriter;

/**
 * Class to manage UiAutomation.
 */
class UiAutomationManager {
    private static final ComponentName COMPONENT_NAME =
            new ComponentName("com.android.server.accessibility", "UiAutomation");
    private static final String LOG_TAG = "UiAutomationManager";

    private final Object mLock;

    private UiAutomationService mUiAutomationService;

    private AccessibilityServiceInfo mUiAutomationServiceInfo;

    private AbstractAccessibilityServiceConnection.SystemSupport mSystemSupport;

    private int mUiAutomationFlags;

    UiAutomationManager(Object lock) {
        mLock = lock;
    }

    private IBinder mUiAutomationServiceOwner;
    private final DeathRecipient mUiAutomationServiceOwnerDeathRecipient =
            new DeathRecipient() {
                @Override
                public void binderDied() {
                    mUiAutomationServiceOwner.unlinkToDeath(this, 0);
                    mUiAutomationServiceOwner = null;
                    destroyUiAutomationService();
                }
            };

    /**
     * Register a UiAutomation. Only one may be registered at a time.
     *
     * @param owner A binder object owned by the process that owns the UiAutomation to be
     *              registered.
     * @param serviceClient The UiAutomation's service interface.
     * @param accessibilityServiceInfo The UiAutomation's service info
     * @param flags The UiAutomation's flags
     * @param id The id for the service connection
     */
    void registerUiTestAutomationServiceLocked(IBinder owner,
            IAccessibilityServiceClient serviceClient,
            Context context, AccessibilityServiceInfo accessibilityServiceInfo,
            int id, Handler mainHandler,
            AccessibilityManagerService.SecurityPolicy securityPolicy,
            AbstractAccessibilityServiceConnection.SystemSupport systemSupport,
            WindowManagerInternal windowManagerInternal,
            GlobalActionPerformer globalActionPerfomer, int flags) {
        synchronized (mLock) {
            accessibilityServiceInfo.setComponentName(COMPONENT_NAME);

            if (mUiAutomationService != null) {
                throw new IllegalStateException("UiAutomationService " + serviceClient
                        + "already registered!");
            }

            try {
                owner.linkToDeath(mUiAutomationServiceOwnerDeathRecipient, 0);
            } catch (RemoteException re) {
                Slog.e(LOG_TAG, "Couldn't register for the death of a UiTestAutomationService!",
                        re);
                return;
            }

            mSystemSupport = systemSupport;
            mUiAutomationService = new UiAutomationService(context, accessibilityServiceInfo, id,
                    mainHandler, mLock, securityPolicy, systemSupport, windowManagerInternal,
                    globalActionPerfomer);
            mUiAutomationServiceOwner = owner;
            mUiAutomationFlags = flags;
            mUiAutomationServiceInfo = accessibilityServiceInfo;
            mUiAutomationService.mServiceInterface = serviceClient;
            mUiAutomationService.onAdded();
            try {
                mUiAutomationService.mServiceInterface.asBinder().linkToDeath(mUiAutomationService,
                        0);
            } catch (RemoteException re) {
                Slog.e(LOG_TAG, "Failed registering death link: " + re);
                destroyUiAutomationService();
                return;
            }

            mUiAutomationService.connectServiceUnknownThread();
        }
    }

    void unregisterUiTestAutomationServiceLocked(IAccessibilityServiceClient serviceClient) {
        synchronized (mLock) {
            if ((mUiAutomationService == null)
                    || (serviceClient == null)
                    || (mUiAutomationService.mServiceInterface == null)
                    || (serviceClient.asBinder()
                    != mUiAutomationService.mServiceInterface.asBinder())) {
                throw new IllegalStateException("UiAutomationService " + serviceClient
                        + " not registered!");
            }

            destroyUiAutomationService();
        }
    }

    void sendAccessibilityEventLocked(AccessibilityEvent event) {
        if (mUiAutomationService != null) {
            mUiAutomationService.notifyAccessibilityEvent(event);
        }
    }

    boolean isUiAutomationRunningLocked() {
        return (mUiAutomationService != null);
    }

    boolean suppressingAccessibilityServicesLocked() {
        return (mUiAutomationService != null) && ((mUiAutomationFlags
                & UiAutomation.FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES) == 0);
    }

    boolean isTouchExplorationEnabledLocked() {
        return (mUiAutomationService != null)
                && mUiAutomationService.mRequestTouchExplorationMode;
    }

    boolean canRetrieveInteractiveWindowsLocked() {
        return (mUiAutomationService != null) && mUiAutomationService.mRetrieveInteractiveWindows;
    }

    int getRequestedEventMaskLocked() {
        if (mUiAutomationService == null) return 0;
        return mUiAutomationService.mEventTypes;
    }

    int getRelevantEventTypes() {
        UiAutomationService uiAutomationService;
        synchronized (mLock) {
            uiAutomationService = mUiAutomationService;
        }
        if (uiAutomationService == null) return 0;
        return uiAutomationService.getRelevantEventTypes();
    }

    @Nullable
    AccessibilityServiceInfo getServiceInfo() {
        UiAutomationService uiAutomationService;
        synchronized (mLock) {
            uiAutomationService = mUiAutomationService;
        }
        if (uiAutomationService == null) return null;
        return uiAutomationService.getServiceInfo();
    }

    void dumpUiAutomationService(FileDescriptor fd, final PrintWriter pw, String[] args) {
        UiAutomationService uiAutomationService;
        synchronized (mLock) {
            uiAutomationService = mUiAutomationService;
        }
        if (uiAutomationService != null) {
            uiAutomationService.dump(fd, pw, args);
        }
    }

    private void destroyUiAutomationService() {
        synchronized (mLock) {
            if (mUiAutomationService != null) {
                mUiAutomationService.mServiceInterface.asBinder().unlinkToDeath(
                        mUiAutomationService, 0);
                mUiAutomationService.onRemoved();
                mUiAutomationService.resetLocked();
                mUiAutomationService = null;
                mUiAutomationFlags = 0;
                if (mUiAutomationServiceOwner != null) {
                    mUiAutomationServiceOwner.unlinkToDeath(
                            mUiAutomationServiceOwnerDeathRecipient, 0);
                    mUiAutomationServiceOwner = null;
                }
                mSystemSupport.onClientChangeLocked(false);
            }
        }
    }

    private class UiAutomationService extends AbstractAccessibilityServiceConnection {
        private final Handler mMainHandler;

        UiAutomationService(Context context, AccessibilityServiceInfo accessibilityServiceInfo,
                int id, Handler mainHandler, Object lock,
                AccessibilityManagerService.SecurityPolicy securityPolicy,
                SystemSupport systemSupport, WindowManagerInternal windowManagerInternal,
                GlobalActionPerformer globalActionPerfomer) {
            super(context, COMPONENT_NAME, accessibilityServiceInfo, id, mainHandler, lock,
                    securityPolicy, systemSupport, windowManagerInternal, globalActionPerfomer);
            mMainHandler = mainHandler;
        }

        void connectServiceUnknownThread() {
            // This needs to be done on the main thread
            mMainHandler.post(() -> {
                try {
                    final IAccessibilityServiceClient serviceInterface;
                    final IBinder service;
                    synchronized (mLock) {
                        serviceInterface = mServiceInterface;
                        mService = (serviceInterface == null) ? null : mServiceInterface.asBinder();
                        service = mService;
                    }
                    // If the serviceInterface is null, the UiAutomation has been shut down on
                    // another thread.
                    if (serviceInterface != null) {
                        service.linkToDeath(this, 0);
                        serviceInterface.init(this, mId, mOverlayWindowToken);
                    }
                } catch (RemoteException re) {
                    Slog.w(LOG_TAG, "Error initialized connection", re);
                    destroyUiAutomationService();
                }
            });
        }

        @Override
        public void binderDied() {
            destroyUiAutomationService();
        }

        @Override
        protected boolean isCalledForCurrentUserLocked() {
            // Allow UiAutomation to work for any user
            return true;
        }

        @Override
        protected boolean supportsFlagForNotImportantViews(AccessibilityServiceInfo info) {
            return true;
        }

        @Override
        public void dump(FileDescriptor fd, final PrintWriter pw, String[] args) {
            if (!DumpUtils.checkDumpPermission(mContext, LOG_TAG, pw)) return;
            synchronized (mLock) {
                pw.append("Ui Automation[eventTypes="
                        + AccessibilityEvent.eventTypeToString(mEventTypes));
                pw.append(", notificationTimeout=" + mNotificationTimeout);
                pw.append("]");
            }
        }

        // Since this isn't really an accessibility service, several methods are just stubbed here.
        @Override
        public boolean setSoftKeyboardShowMode(int mode) {
            return false;
        }

        @Override
        public int getSoftKeyboardShowMode() {
            return 0;
        }

        @Override
        public boolean isAccessibilityButtonAvailable() {
            return false;
        }

        @Override
        public void disableSelf() {}

        @Override
        public void onServiceConnected(ComponentName componentName, IBinder service) {}

        @Override
        public void onServiceDisconnected(ComponentName componentName) {}

        @Override
        public boolean isCapturingFingerprintGestures() {
            return false;
        }

        @Override
        public void onFingerprintGestureDetectionActiveChanged(boolean active) {}

        @Override
        public void onFingerprintGesture(int gesture) {}
    }
}
