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
package com.android.server.appprediction;

import android.annotation.NonNull;
import android.app.prediction.AppPredictionContext;
import android.app.prediction.AppPredictionSessionId;
import android.app.prediction.AppTargetEvent;
import android.app.prediction.IPredictionCallback;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.ParceledListSlice;
import android.os.IBinder;
import android.service.appprediction.IPredictionService;
import android.text.format.DateUtils;

import com.android.internal.infra.AbstractMultiplePendingRequestsRemoteService;


/**
 * Proxy to the {@link android.service.appprediction.AppPredictionService} implemention in another
 * process.
 */
public class RemoteAppPredictionService extends
        AbstractMultiplePendingRequestsRemoteService<RemoteAppPredictionService,
                IPredictionService> {

    private static final String TAG = "RemoteAppPredictionService";

    private static final long TIMEOUT_REMOTE_REQUEST_MILLIS = 2 * DateUtils.SECOND_IN_MILLIS;

    private final RemoteAppPredictionServiceCallbacks mCallback;

    public RemoteAppPredictionService(Context context, String serviceInterface,
            ComponentName componentName, int userId,
            RemoteAppPredictionServiceCallbacks callback, boolean bindInstantServiceAllowed,
            boolean verbose) {
        super(context, serviceInterface, componentName, userId, callback,
                context.getMainThreadHandler(),
                bindInstantServiceAllowed ? Context.BIND_ALLOW_INSTANT : 0,
                verbose, /* initialCapacity= */ 1);
        mCallback = callback;
    }

    @Override
    protected IPredictionService getServiceInterface(IBinder service) {
        return IPredictionService.Stub.asInterface(service);
    }

    @Override
    protected long getTimeoutIdleBindMillis() {
        return PERMANENT_BOUND_TIMEOUT_MS;
    }

    @Override
    protected long getRemoteRequestMillis() {
        return TIMEOUT_REMOTE_REQUEST_MILLIS;
    }

    /**
     * Notifies the service of a new prediction session.
     */
    public void onCreatePredictionSession(@NonNull AppPredictionContext context,
            @NonNull AppPredictionSessionId sessionId) {
        scheduleAsyncRequest((s) -> s.onCreatePredictionSession(context, sessionId));
    }

    /**
     * Records an app target event to the service.
     */
    public void notifyAppTargetEvent(@NonNull AppPredictionSessionId sessionId,
            @NonNull AppTargetEvent event) {
        scheduleAsyncRequest((s) -> s.notifyAppTargetEvent(sessionId, event));
    }

    /**
     * Records when a launch location is shown.
     */
    public void notifyLaunchLocationShown(@NonNull AppPredictionSessionId sessionId,
            @NonNull String launchLocation, @NonNull ParceledListSlice targetIds) {
        scheduleAsyncRequest((s)
                -> s.notifyLaunchLocationShown(sessionId, launchLocation, targetIds));
    }

    /**
     * Requests the service to sort a list of apps or shortcuts.
     */
    public void sortAppTargets(@NonNull AppPredictionSessionId sessionId,
            @NonNull ParceledListSlice targets, @NonNull IPredictionCallback callback) {
        scheduleAsyncRequest((s) -> s.sortAppTargets(sessionId, targets, callback));
    }


    /**
     * Registers a callback for continuous updates of predicted apps or shortcuts.
     */
    public void registerPredictionUpdates(@NonNull AppPredictionSessionId sessionId,
            @NonNull IPredictionCallback callback) {
        scheduleAsyncRequest((s) -> s.registerPredictionUpdates(sessionId, callback));
    }

    /**
     * Unregisters a callback for continuous updates of predicted apps or shortcuts.
     */
    public void unregisterPredictionUpdates(@NonNull AppPredictionSessionId sessionId,
            @NonNull IPredictionCallback callback) {
        scheduleAsyncRequest((s) -> s.unregisterPredictionUpdates(sessionId, callback));
    }

    /**
     * Requests a new set of predicted apps or shortcuts.
     */
    public void requestPredictionUpdate(@NonNull AppPredictionSessionId sessionId) {
        scheduleAsyncRequest((s) -> s.requestPredictionUpdate(sessionId));
    }

    /**
     * Notifies the service of the end of an existing prediction session.
     */
    public void onDestroyPredictionSession(@NonNull AppPredictionSessionId sessionId) {
        scheduleAsyncRequest((s) -> s.onDestroyPredictionSession(sessionId));
    }

    /**
     * Failure callback
     */
    public interface RemoteAppPredictionServiceCallbacks
            extends VultureCallback<RemoteAppPredictionService> {

        /**
         * Notifies a the failure or timeout of a remote call.
         */
        void onFailureOrTimeout(boolean timedOut);

        /**
         * Notifies change in connected state of the remote service.
         */
        void onConnectedStateChanged(boolean connected);
    }

    @Override // from AbstractRemoteService
    protected void handleOnConnectedStateChanged(boolean connected) {
        if (mCallback != null) {
            mCallback.onConnectedStateChanged(connected);
        }
    }
}
