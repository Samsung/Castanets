/*
 * Copyright 2017 The Android Open Source Project
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

package com.android.server.location;

import android.app.PendingIntent;
import android.content.Context;
import android.hardware.contexthub.V1_0.ContextHubMsg;
import android.hardware.contexthub.V1_0.IContexthub;
import android.hardware.location.ContextHubInfo;
import android.hardware.location.IContextHubClient;
import android.hardware.location.IContextHubClientCallback;
import android.hardware.location.NanoAppMessage;
import android.os.RemoteException;
import android.util.Log;

import java.util.concurrent.ConcurrentHashMap;
import java.util.function.Consumer;

/**
 * A class that manages registration/unregistration of clients and manages messages to/from clients.
 *
 * @hide
 */
/* package */ class ContextHubClientManager {
    private static final String TAG = "ContextHubClientManager";

    /*
     * The maximum host endpoint ID value that a client can be assigned.
     */
    private static final int MAX_CLIENT_ID = 0x7fff;

    /*
     * Local flag to enable debug logging.
     */
    private static final boolean DEBUG_LOG_ENABLED = false;

    /*
     * The context of the service.
     */
    private final Context mContext;

    /*
     * The proxy to talk to the Context Hub.
     */
    private final IContexthub mContextHubProxy;

    /*
     * A mapping of host endpoint IDs to the ContextHubClientBroker object of registered clients.
     * A concurrent data structure is used since the registration/unregistration can occur in
     * multiple threads.
     */
    private final ConcurrentHashMap<Short, ContextHubClientBroker> mHostEndPointIdToClientMap =
            new ConcurrentHashMap<>();

    /*
     * The next host endpoint ID to start iterating for the next available host endpoint ID.
     */
    private int mNextHostEndPointId = 0;

    /* package */ ContextHubClientManager(
            Context context, IContexthub contextHubProxy) {
        mContext = context;
        mContextHubProxy = contextHubProxy;
    }

    /**
     * Registers a new client with the service.
     *
     * @param contextHubInfo the object describing the hub this client is attached to
     * @param clientCallback the callback interface of the client to register
     *
     * @return the client interface
     *
     * @throws IllegalStateException if max number of clients have already registered
     */
    /* package */ IContextHubClient registerClient(
            ContextHubInfo contextHubInfo, IContextHubClientCallback clientCallback) {
        ContextHubClientBroker broker;
        synchronized (this) {
            short hostEndPointId = getHostEndPointId();
            broker = new ContextHubClientBroker(
                    mContext, mContextHubProxy, this /* clientManager */, contextHubInfo,
                    hostEndPointId, clientCallback);
            mHostEndPointIdToClientMap.put(hostEndPointId, broker);
        }

        try {
            broker.attachDeathRecipient();
        } catch (RemoteException e) {
            // The client process has died, so we close the connection and return null
            Log.e(TAG, "Failed to attach death recipient to client");
            broker.close();
            return null;
        }

        Log.d(TAG, "Registered client with host endpoint ID " + broker.getHostEndPointId());
        return IContextHubClient.Stub.asInterface(broker);
    }

    /**
     * Registers a new client with the service.
     *
     * @param pendingIntent  the callback interface of the client to register
     * @param contextHubInfo the object describing the hub this client is attached to
     * @param nanoAppId      the ID of the nanoapp to receive Intent events for
     *
     * @return the client interface
     *
     * @throws IllegalStateException    if there were too many registered clients at the service
     */
    /* package */ IContextHubClient registerClient(
            ContextHubInfo contextHubInfo, PendingIntent pendingIntent, long nanoAppId) {
        ContextHubClientBroker broker;
        String registerString = "Regenerated";
        synchronized (this) {
            broker = getClientBroker(contextHubInfo.getId(), pendingIntent, nanoAppId);

            if (broker == null) {
                short hostEndPointId = getHostEndPointId();
                broker = new ContextHubClientBroker(
                        mContext, mContextHubProxy, this /* clientManager */, contextHubInfo,
                        hostEndPointId, pendingIntent, nanoAppId);
                mHostEndPointIdToClientMap.put(hostEndPointId, broker);
                registerString = "Registered";
            }
        }

        Log.d(TAG, registerString + " client with host endpoint ID " + broker.getHostEndPointId());
        return IContextHubClient.Stub.asInterface(broker);
    }

    /**
     * Handles a message sent from a nanoapp.
     *
     * @param contextHubId the ID of the hub where the nanoapp sent the message from
     * @param message      the message send by a nanoapp
     */
    /* package */ void onMessageFromNanoApp(int contextHubId, ContextHubMsg message) {
        NanoAppMessage clientMessage = ContextHubServiceUtil.createNanoAppMessage(message);

        if (DEBUG_LOG_ENABLED) {
            Log.v(TAG, "Received " + clientMessage);
        }

        if (clientMessage.isBroadcastMessage()) {
            broadcastMessage(contextHubId, clientMessage);
        } else {
            ContextHubClientBroker proxy = mHostEndPointIdToClientMap.get(message.hostEndPoint);
            if (proxy != null) {
                proxy.sendMessageToClient(clientMessage);
            } else {
                Log.e(TAG, "Cannot send message to unregistered client (host endpoint ID = "
                        + message.hostEndPoint + ")");
            }
        }
    }

    /**
     * Unregisters a client from the service.
     *
     * This method should be invoked as a result of a client calling the ContextHubClient.close(),
     * or if the client process has died.
     *
     * @param hostEndPointId the host endpoint ID of the client that has died
     */
    /* package */ void unregisterClient(short hostEndPointId) {
        if (mHostEndPointIdToClientMap.remove(hostEndPointId) != null) {
            Log.d(TAG, "Unregistered client with host endpoint ID " + hostEndPointId);
        } else {
            Log.e(TAG, "Cannot unregister non-existing client with host endpoint ID "
                    + hostEndPointId);
        }
    }

    /**
     * @param contextHubId the ID of the hub where the nanoapp was loaded
     * @param nanoAppId    the ID of the nanoapp that was loaded
     */
    /* package */ void onNanoAppLoaded(int contextHubId, long nanoAppId) {
        forEachClientOfHub(contextHubId, client -> client.onNanoAppLoaded(nanoAppId));
    }

    /**
     * @param contextHubId the ID of the hub where the nanoapp was unloaded
     * @param nanoAppId    the ID of the nanoapp that was unloaded
     */
    /* package */ void onNanoAppUnloaded(int contextHubId, long nanoAppId) {
        forEachClientOfHub(contextHubId, client -> client.onNanoAppUnloaded(nanoAppId));
    }

    /**
     * @param contextHubId the ID of the hub that has reset
     */
    /* package */ void onHubReset(int contextHubId) {
        forEachClientOfHub(contextHubId, client -> client.onHubReset());
    }

    /**
     * @param contextHubId the ID of the hub that contained the nanoapp that aborted
     * @param nanoAppId the ID of the nanoapp that aborted
     * @param abortCode the nanoapp specific abort code
     */
    /* package */ void onNanoAppAborted(int contextHubId, long nanoAppId, int abortCode) {
        forEachClientOfHub(contextHubId, client -> client.onNanoAppAborted(nanoAppId, abortCode));
    }

    /**
     * Returns an available host endpoint ID.
     *
     * @returns an available host endpoint ID
     *
     * @throws IllegalStateException if max number of clients have already registered
     */
    private short getHostEndPointId() {
        if (mHostEndPointIdToClientMap.size() == MAX_CLIENT_ID + 1) {
            throw new IllegalStateException("Could not register client - max limit exceeded");
        }

        int id = mNextHostEndPointId;
        for (int i = 0; i <= MAX_CLIENT_ID; i++) {
            if (!mHostEndPointIdToClientMap.containsKey((short) id)) {
                mNextHostEndPointId = (id == MAX_CLIENT_ID) ? 0 : id + 1;
                break;
            }

            id = (id == MAX_CLIENT_ID) ? 0 : id + 1;
        }

        return (short) id;
    }

    /**
     * Broadcasts a message from a nanoapp to all clients attached to the associated hub.
     *
     * @param contextHubId the ID of the hub where the nanoapp sent the message from
     * @param message      the message send by a nanoapp
     */
    private void broadcastMessage(int contextHubId, NanoAppMessage message) {
        forEachClientOfHub(contextHubId, client -> client.sendMessageToClient(message));
    }

    /**
     * Runs a command for each client that is attached to a hub with the given ID.
     *
     * @param contextHubId the ID of the hub
     * @param callback     the command to invoke for the client
     */
    private void forEachClientOfHub(int contextHubId, Consumer<ContextHubClientBroker> callback) {
        for (ContextHubClientBroker broker : mHostEndPointIdToClientMap.values()) {
            if (broker.getAttachedContextHubId() == contextHubId) {
                callback.accept(broker);
            }
        }
    }

    /**
     * Retrieves a ContextHubClientBroker object with a matching PendingIntent and Context Hub ID.
     *
     * @param pendingIntent the PendingIntent to match
     * @param contextHubId  the ID of the Context Hub the client is attached to
     * @return the matching ContextHubClientBroker, null if not found
     */
    private ContextHubClientBroker getClientBroker(
            int contextHubId, PendingIntent pendingIntent, long nanoAppId) {
        for (ContextHubClientBroker broker : mHostEndPointIdToClientMap.values()) {
            if (broker.hasPendingIntent(pendingIntent, nanoAppId)
                    && broker.getAttachedContextHubId() == contextHubId) {
                return broker;
            }
        }

        return null;
    }
}
