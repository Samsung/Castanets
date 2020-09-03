/*
 * Copyright (C) 2015 The Android Open Source Project
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
package android.support.v4.media;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.Parcel;
import android.os.Parcelable;
import android.os.RemoteException;
import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.BundleCompat;
import android.support.v4.media.session.MediaControllerCompat;
import android.support.v4.media.session.MediaSessionCompat;
import android.support.v4.os.ResultReceiver;
import android.support.v4.util.ArrayMap;
import android.text.TextUtils;
import android.util.Log;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

import static android.support.v4.media.MediaBrowserProtocol.*;

/**
 * Browses media content offered by a {@link MediaBrowserServiceCompat}.
 * <p>
 * This object is not thread-safe. All calls should happen on the thread on which the browser
 * was constructed.
 * </p>
 */
public final class MediaBrowserCompat {
    private static final String TAG = "MediaBrowserCompat";

    /**
     * Used as an int extra field to denote the page number to subscribe.
     * The value of {@code EXTRA_PAGE} should be greater than or equal to 1.
     *
     * @see android.service.media.MediaBrowserService.BrowserRoot
     * @see #EXTRA_PAGE_SIZE
     * {@hide}
     */
    public static final String EXTRA_PAGE = "android.media.browse.extra.PAGE";

    /**
     * Used as an int extra field to denote the number of media items in a page.
     * The value of {@code EXTRA_PAGE_SIZE} should be greater than or equal to 1.
     *
     * @see android.service.media.MediaBrowserService.BrowserRoot
     * @see #EXTRA_PAGE
     * {@hide}
     */
    public static final String EXTRA_PAGE_SIZE = "android.media.browse.extra.PAGE_SIZE";

    private final MediaBrowserImpl mImpl;

    /**
     * Creates a media browser for the specified media browse service.
     *
     * @param context The context.
     * @param serviceComponent The component name of the media browse service.
     * @param callback The connection callback.
     * @param rootHints An optional bundle of service-specific arguments to send
     * to the media browse service when connecting and retrieving the root id
     * for browsing, or null if none. The contents of this bundle may affect
     * the information returned when browsing.
     */
    public MediaBrowserCompat(Context context, ComponentName serviceComponent,
            ConnectionCallback callback, Bundle rootHints) {
        if (android.os.Build.VERSION.SDK_INT >= 23) {
            mImpl = new MediaBrowserImplApi23(context, serviceComponent, callback, rootHints);
        } else if (android.os.Build.VERSION.SDK_INT >= 21) {
            mImpl = new MediaBrowserImplApi21(context, serviceComponent, callback, rootHints);
        } else {
            mImpl = new MediaBrowserServiceImplBase(context, serviceComponent, callback, rootHints);
        }
    }

    /**
     * Connects to the media browse service.
     * <p>
     * The connection callback specified in the constructor will be invoked
     * when the connection completes or fails.
     * </p>
     */
    public void connect() {
        mImpl.connect();
    }

    /**
     * Disconnects from the media browse service.
     * After this, no more callbacks will be received.
     */
    public void disconnect() {
        mImpl.disconnect();
    }

    /**
     * Returns whether the browser is connected to the service.
     */
    public boolean isConnected() {
        return mImpl.isConnected();
    }

    /**
     * Gets the service component that the media browser is connected to.
     */
    public @NonNull
    ComponentName getServiceComponent() {
        return mImpl.getServiceComponent();
    }

    /**
     * Gets the root id.
     * <p>
     * Note that the root id may become invalid or change when when the
     * browser is disconnected.
     * </p>
     *
     * @throws IllegalStateException if not connected.
     */
    public @NonNull String getRoot() {
        return mImpl.getRoot();
    }

    /**
     * Gets any extras for the media service.
     *
     * @throws IllegalStateException if not connected.
     */
    public @Nullable
    Bundle getExtras() {
        return mImpl.getExtras();
    }

    /**
     * Gets the media session token associated with the media browser.
     * <p>
     * Note that the session token may become invalid or change when when the
     * browser is disconnected.
     * </p>
     *
     * @return The session token for the browser, never null.
     *
     * @throws IllegalStateException if not connected.
     */
     public @NonNull MediaSessionCompat.Token getSessionToken() {
        return mImpl.getSessionToken();
    }

    /**
     * Queries for information about the media items that are contained within
     * the specified id and subscribes to receive updates when they change.
     * <p>
     * The list of subscriptions is maintained even when not connected and is
     * restored after the reconnection. It is ok to subscribe while not connected
     * but the results will not be returned until the connection completes.
     * </p>
     * <p>
     * If the id is already subscribed with a different callback then the new
     * callback will replace the previous one and the child data will be
     * reloaded.
     * </p>
     *
     * @param parentId The id of the parent media item whose list of children
     *            will be subscribed.
     * @param callback The callback to receive the list of children.
     */
    public void subscribe(@NonNull String parentId, @NonNull SubscriptionCallback callback) {
        mImpl.subscribe(parentId, null, callback);
    }

    /**
     * Queries with service-specific arguments for information about the media items
     * that are contained within the specified id and subscribes to receive updates
     * when they change.
     * <p>
     * The list of subscriptions is maintained even when not connected and is
     * restored after the reconnection. It is ok to subscribe while not connected
     * but the results will not be returned until the connection completes.
     * </p>
     * <p>
     * If the id is already subscribed with a different callback then the new
     * callback will replace the previous one and the child data will be
     * reloaded.
     * </p>
     *
     * @param parentId The id of the parent media item whose list of children
     *            will be subscribed.
     * @param options A bundle of service-specific arguments to send to the media
     *            browse service. The contents of this bundle may affect the
     *            information returned when browsing.
     * @param callback The callback to receive the list of children.
     * {@hide}
     */
    public void subscribe(@NonNull String parentId, @NonNull Bundle options,
            @NonNull SubscriptionCallback callback) {
        if (options == null) {
            throw new IllegalArgumentException("options are null");
        }
        mImpl.subscribe(parentId, options, callback);
    }

    /**
     * Unsubscribes for changes to the children of the specified media id.
     * <p>
     * The query callback will no longer be invoked for results associated with
     * this id once this method returns.
     * </p>
     *
     * @param parentId The id of the parent media item whose list of children
     *            will be unsubscribed.
     */
    public void unsubscribe(@NonNull String parentId) {
        mImpl.unsubscribe(parentId, null);
    }

    /**
     * Unsubscribes for changes to the children of the specified media id.
     * <p>
     * The query callback will no longer be invoked for results associated with
     * this id once this method returns.
     * </p>
     *
     * @param parentId The id of the parent media item whose list of children
     *            will be unsubscribed.
     * @param options A bundle sent to the media browse service to subscribe.
     * {@hide}
     */
    public void unsubscribe(@NonNull String parentId, @NonNull Bundle options) {
        if (options == null) {
            throw new IllegalArgumentException("options are null");
        }
        mImpl.unsubscribe(parentId, options);
    }

    /**
     * Retrieves a specific {@link MediaItem} from the connected service. Not
     * all services may support this, so falling back to subscribing to the
     * parent's id should be used when unavailable.
     *
     * @param mediaId The id of the item to retrieve.
     * @param cb The callback to receive the result on.
     */
    public void getItem(final @NonNull String mediaId, @NonNull final ItemCallback cb) {
        mImpl.getItem(mediaId, cb);
    }

    /**
     * A class with information on a single media item for use in browsing media.
     */
    public static class MediaItem implements Parcelable {
        private final int mFlags;
        private final MediaDescriptionCompat mDescription;

        /** @hide */
        @Retention(RetentionPolicy.SOURCE)
        @IntDef(flag=true, value = { FLAG_BROWSABLE, FLAG_PLAYABLE })
        public @interface Flags { }

        /**
         * Flag: Indicates that the item has children of its own.
         */
        public static final int FLAG_BROWSABLE = 1 << 0;

        /**
         * Flag: Indicates that the item is playable.
         * <p>
         * The id of this item may be passed to
         * {@link MediaControllerCompat.TransportControls#playFromMediaId(String, Bundle)}
         * to start playing it.
         * </p>
         */
        public static final int FLAG_PLAYABLE = 1 << 1;

        /**
         * Create a new MediaItem for use in browsing media.
         * @param description The description of the media, which must include a
         *            media id.
         * @param flags The flags for this item.
         */
        public MediaItem(@NonNull MediaDescriptionCompat description, @Flags int flags) {
            if (description == null) {
                throw new IllegalArgumentException("description cannot be null");
            }
            if (TextUtils.isEmpty(description.getMediaId())) {
                throw new IllegalArgumentException("description must have a non-empty media id");
            }
            mFlags = flags;
            mDescription = description;
        }

        /**
         * Private constructor.
         */
        private MediaItem(Parcel in) {
            mFlags = in.readInt();
            mDescription = MediaDescriptionCompat.CREATOR.createFromParcel(in);
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel out, int flags) {
            out.writeInt(mFlags);
            mDescription.writeToParcel(out, flags);
        }

        @Override
        public String toString() {
            final StringBuilder sb = new StringBuilder("MediaItem{");
            sb.append("mFlags=").append(mFlags);
            sb.append(", mDescription=").append(mDescription);
            sb.append('}');
            return sb.toString();
        }

        public static final Parcelable.Creator<MediaItem> CREATOR =
                new Parcelable.Creator<MediaItem>() {
                    @Override
                    public MediaItem createFromParcel(Parcel in) {
                        return new MediaItem(in);
                    }

                    @Override
                    public MediaItem[] newArray(int size) {
                        return new MediaItem[size];
                    }
                };

        /**
         * Gets the flags of the item.
         */
        public @Flags int getFlags() {
            return mFlags;
        }

        /**
         * Returns whether this item is browsable.
         * @see #FLAG_BROWSABLE
         */
        public boolean isBrowsable() {
            return (mFlags & FLAG_BROWSABLE) != 0;
        }

        /**
         * Returns whether this item is playable.
         * @see #FLAG_PLAYABLE
         */
        public boolean isPlayable() {
            return (mFlags & FLAG_PLAYABLE) != 0;
        }

        /**
         * Returns the description of the media.
         */
        public @NonNull MediaDescriptionCompat getDescription() {
            return mDescription;
        }

        /**
         * Returns the media id for this item.
         */
        public @NonNull String getMediaId() {
            return mDescription.getMediaId();
        }
    }

    /**
     * Callbacks for connection related events.
     */
    public static class ConnectionCallback {
        final Object mConnectionCallbackObj;
        private ConnectionCallbackInternal mConnectionCallbackInternal;

        public ConnectionCallback() {
            if (android.os.Build.VERSION.SDK_INT >= 21) {
                mConnectionCallbackObj =
                        MediaBrowserCompatApi21.createConnectionCallback(new StubApi21());
            } else {
                mConnectionCallbackObj = null;
            }
        }

        /**
         * Invoked after {@link MediaBrowserCompat#connect()} when the request has successfully
         * completed.
         */
        public void onConnected() {
        }

        /**
         * Invoked when the client is disconnected from the media browser.
         */
        public void onConnectionSuspended() {
        }

        /**
         * Invoked when the connection to the media browser failed.
         */
        public void onConnectionFailed() {
        }

        void setInternalConnectionCallback(ConnectionCallbackInternal connectionCallbackInternal) {
            mConnectionCallbackInternal = connectionCallbackInternal;
        }

        interface ConnectionCallbackInternal {
            void onConnected();
            void onConnectionSuspended();
            void onConnectionFailed();
        }

        private class StubApi21 implements MediaBrowserCompatApi21.ConnectionCallback {
            @Override
            public void onConnected() {
                if (mConnectionCallbackInternal != null) {
                    mConnectionCallbackInternal.onConnected();
                }
                ConnectionCallback.this.onConnected();
            }

            @Override
            public void onConnectionSuspended() {
                if (mConnectionCallbackInternal != null) {
                    mConnectionCallbackInternal.onConnectionSuspended();
                }
                ConnectionCallback.this.onConnectionSuspended();
            }

            @Override
            public void onConnectionFailed() {
                if (mConnectionCallbackInternal != null) {
                    mConnectionCallbackInternal.onConnectionFailed();
                }
                ConnectionCallback.this.onConnectionFailed();
            }
        }
    }

    /**
     * Callbacks for subscription related events.
     */
    public static abstract class SubscriptionCallback {
        /**
         * Called when the list of children is loaded or updated.
         *
         * @param parentId The media id of the parent media item.
         * @param children The children which were loaded, or null if the id is invalid.
         */
        public void onChildrenLoaded(@NonNull String parentId, List<MediaItem> children) {
        }

        /**
         * Called when the list of children is loaded or updated.
         *
         * @param parentId The media id of the parent media item.
         * @param children The children which were loaded, or null if the id is invalid.
         * @param options A bundle of service-specific arguments to send to the media
         *            browse service. The contents of this bundle may affect the
         *            information returned when browsing.
         * {@hide}
         */
        public void onChildrenLoaded(@NonNull String parentId, List<MediaItem> children,
                @NonNull Bundle options) {
        }

        /**
         * Called when the id doesn't exist or other errors in subscribing.
         * <p>
         * If this is called, the subscription remains until {@link MediaBrowserCompat#unsubscribe}
         * called, because some errors may heal themselves.
         * </p>
         *
         * @param parentId The media id of the parent media item whose children could not be loaded.
         */
        public void onError(@NonNull String parentId) {
        }

        /**
         * Called when the id doesn't exist or other errors in subscribing.
         * <p>
         * If this is called, the subscription remains until {@link MediaBrowserCompat#unsubscribe}
         * called, because some errors may heal themselves.
         * </p>
         *
         * @param parentId The media id of the parent media item whose children could
         *            not be loaded.
         * @param options A bundle of service-specific arguments sent to the media
         *            browse service.
         * {@hide}
         */
        public void onError(@NonNull String parentId, @NonNull Bundle options) {
        }
    }

    /**
     * Callbacks for subscription related events.
     */
    static class SubscriptionCallbackApi21 extends SubscriptionCallback {
        SubscriptionCallback mSubscriptionCallback;
        private final Object mSubscriptionCallbackObj;
        private Bundle mOptions;

        public SubscriptionCallbackApi21(SubscriptionCallback callback, Bundle options) {
            mSubscriptionCallback = callback;
            mOptions = options;
            mSubscriptionCallbackObj =
                    MediaBrowserCompatApi21.createSubscriptionCallback(new StubApi21());
        }

        /**
         * Called when the list of children is loaded or updated.
         *
         * @param parentId The media id of the parent media item.
         * @param children The children which were loaded, or null if the id is invalid.
         */
        public void onChildrenLoaded(@NonNull String parentId, List<MediaItem> children) {
            mSubscriptionCallback.onChildrenLoaded(parentId, children);
        }

        /**
         * Called when the list of children is loaded or updated.
         *
         * @param parentId The media id of the parent media item.
         * @param children The children which were loaded, or null if the id is invalid.
         * @param options A bundle of service-specific arguments to send to the media
         *            browse service. The contents of this bundle may affect the
         *            information returned when browsing.
         * {@hide}
         */
        public void onChildrenLoaded(@NonNull String parentId, List<MediaItem> children,
                @NonNull Bundle options) {
            mSubscriptionCallback.onChildrenLoaded(parentId, children, options);
        }

        /**
         * Called when the id doesn't exist or other errors in subscribing.
         * <p>
         * If this is called, the subscription remains until {@link MediaBrowserCompat#unsubscribe}
         * called, because some errors may heal themselves.
         * </p>
         *
         * @param parentId The media id of the parent media item whose children could not be loaded.
         */
        public void onError(@NonNull String parentId) {
            mSubscriptionCallback.onError(parentId);
        }

        /**
         * Called when the id doesn't exist or other errors in subscribing.
         * <p>
         * If this is called, the subscription remains until {@link MediaBrowserCompat#unsubscribe}
         * called, because some errors may heal themselves.
         * </p>
         *
         * @param parentId The media id of the parent media item whose children could
         *            not be loaded.
         * @param options A bundle of service-specific arguments sent to the media
         *            browse service.
         * {@hide}
         */
        public void onError(@NonNull String parentId, @NonNull Bundle options) {
            mSubscriptionCallback.onError(parentId, options);
        }

        private class StubApi21 implements MediaBrowserCompatApi21.SubscriptionCallback {
            @Override
            public void onChildrenLoaded(@NonNull String parentId, List<Parcel> children) {
                List<MediaBrowserCompat.MediaItem> mediaItems = null;
                if (children != null) {
                    mediaItems = new ArrayList<>();
                    for (Parcel parcel : children) {
                        parcel.setDataPosition(0);
                        mediaItems.add(
                                MediaBrowserCompat.MediaItem.CREATOR.createFromParcel(parcel));
                        parcel.recycle();
                    }
                }
                if (mOptions != null) {
                    SubscriptionCallbackApi21.this.onChildrenLoaded(parentId,
                            MediaBrowserCompatUtils.applyOptions(mediaItems, mOptions),
                            mOptions);
                } else {
                    SubscriptionCallbackApi21.this.onChildrenLoaded(parentId, mediaItems);
                }
            }

            @Override
            public void onError(@NonNull String parentId) {
                if (mOptions != null) {
                    SubscriptionCallbackApi21.this.onError(parentId, mOptions);
                } else {
                    SubscriptionCallbackApi21.this.onError(parentId);
                }
            }
        }
    }

    /**
     * Callback for receiving the result of {@link #getItem}.
     */
    public static abstract class ItemCallback {
        final Object mItemCallbackObj;

        public ItemCallback() {
            if (android.os.Build.VERSION.SDK_INT >= 23) {
                mItemCallbackObj = MediaBrowserCompatApi23.createItemCallback(new StubApi23());
            } else {
                mItemCallbackObj = null;
            }
        }

        /**
         * Called when the item has been returned by the browser service.
         *
         * @param item The item that was returned or null if it doesn't exist.
         */
        public void onItemLoaded(MediaItem item) {
        }

        /**
         * Called when the item doesn't exist or there was an error retrieving it.
         *
         * @param itemId The media id of the media item which could not be loaded.
         */
        public void onError(@NonNull String itemId) {
        }

        private class StubApi23 implements MediaBrowserCompatApi23.ItemCallback {
            @Override
            public void onItemLoaded(Parcel itemParcel) {
                itemParcel.setDataPosition(0);
                MediaItem item = MediaBrowserCompat.MediaItem.CREATOR.createFromParcel(itemParcel);
                itemParcel.recycle();
                ItemCallback.this.onItemLoaded(item);
            }

            @Override
            public void onError(@NonNull String itemId) {
                ItemCallback.this.onError(itemId);
            }
        }
    }

    interface MediaBrowserImpl {
        void connect();
        void disconnect();
        boolean isConnected();
        ComponentName getServiceComponent();
        @NonNull String getRoot();
        @Nullable Bundle getExtras();
        @NonNull MediaSessionCompat.Token getSessionToken();
        void subscribe(@NonNull String parentId, Bundle options,
                @NonNull SubscriptionCallback callback);
        void unsubscribe(@NonNull String parentId, Bundle options);
        void getItem(final @NonNull String mediaId, @NonNull final ItemCallback cb);
    }

    interface MediaBrowserServiceCallbackImpl {
        void onServiceConnected(Messenger callback, String root, MediaSessionCompat.Token session,
                Bundle extra);
        void onConnectionFailed(Messenger callback);
        void onLoadChildren(Messenger callback, String parentId, List list, Bundle options);
    }

    static class MediaBrowserServiceImplBase
            implements MediaBrowserImpl, MediaBrowserServiceCallbackImpl {
        private static final boolean DBG = false;

        private static final int CONNECT_STATE_DISCONNECTED = 0;
        private static final int CONNECT_STATE_CONNECTING = 1;
        private static final int CONNECT_STATE_CONNECTED = 2;
        private static final int CONNECT_STATE_SUSPENDED = 3;

        private final Context mContext;
        private final ComponentName mServiceComponent;
        private final ConnectionCallback mCallback;
        private final Bundle mRootHints;
        private final CallbackHandler mHandler = new CallbackHandler(this);
        private final ArrayMap<String, Subscription> mSubscriptions = new ArrayMap<>();

        private int mState = CONNECT_STATE_DISCONNECTED;
        private MediaServiceConnection mServiceConnection;
        private ServiceBinderWrapper mServiceBinderWrapper;
        private Messenger mCallbacksMessenger;
        private String mRootId;
        private MediaSessionCompat.Token mMediaSessionToken;
        private Bundle mExtras;

        public MediaBrowserServiceImplBase(Context context, ComponentName serviceComponent,
                ConnectionCallback callback, Bundle rootHints) {
            if (context == null) {
                throw new IllegalArgumentException("context must not be null");
            }
            if (serviceComponent == null) {
                throw new IllegalArgumentException("service component must not be null");
            }
            if (callback == null) {
                throw new IllegalArgumentException("connection callback must not be null");
            }
            mContext = context;
            mServiceComponent = serviceComponent;
            mCallback = callback;
            mRootHints = rootHints;
        }

        public void connect() {
            if (mState != CONNECT_STATE_DISCONNECTED) {
                throw new IllegalStateException("connect() called while not disconnected (state="
                        + getStateLabel(mState) + ")");
            }
            // TODO: remove this extra check.
            if (DBG) {
                if (mServiceConnection != null) {
                    throw new RuntimeException("mServiceConnection should be null. Instead it is "
                            + mServiceConnection);
                }
            }
            if (mServiceBinderWrapper != null) {
                throw new RuntimeException("mServiceBinderWrapper should be null. Instead it is "
                        + mServiceBinderWrapper);
            }
            if (mCallbacksMessenger != null) {
                throw new RuntimeException("mCallbacksMessenger should be null. Instead it is "
                        + mCallbacksMessenger);
            }

            mState = CONNECT_STATE_CONNECTING;

            final Intent intent = new Intent(MediaBrowserServiceCompat.SERVICE_INTERFACE);
            intent.setComponent(mServiceComponent);

            final ServiceConnection thisConnection = mServiceConnection =
                    new MediaServiceConnection();

            boolean bound = false;
            try {
                bound = mContext.bindService(intent, mServiceConnection, Context.BIND_AUTO_CREATE);
            } catch (Exception ex) {
                Log.e(TAG, "Failed binding to service " + mServiceComponent);
            }

            if (!bound) {
                // Tell them that it didn't work. We are already on the main thread,
                // but we don't want to do callbacks inside of connect(). So post it,
                // and then check that we are on the same ServiceConnection. We know
                // we won't also get an onServiceConnected or onServiceDisconnected,
                // so we won't be doing double callbacks.
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        // Ensure that nobody else came in or tried to connect again.
                        if (thisConnection == mServiceConnection) {
                            forceCloseConnection();
                            mCallback.onConnectionFailed();
                        }
                    }
                });
            }

            if (DBG) {
                Log.d(TAG, "connect...");
                dump();
            }
        }

        public void disconnect() {
            // It's ok to call this any state, because allowing this lets apps not have
            // to check isConnected() unnecessarily. They won't appreciate the extra
            // assertions for this. We do everything we can here to go back to a sane state.
            if (mCallbacksMessenger != null) {
                try {
                    mServiceBinderWrapper.disconnect(mCallbacksMessenger);
                } catch (RemoteException ex) {
                    // We are disconnecting anyway. Log, just for posterity but it's not
                    // a big problem.
                    Log.w(TAG, "RemoteException during connect for " + mServiceComponent);
                }
            }
            forceCloseConnection();

            if (DBG) {
                Log.d(TAG, "disconnect...");
                dump();
            }
        }

        /**
         * Null out the variables and unbind from the service. This doesn't include
         * calling disconnect on the service, because we only try to do that in the
         * clean shutdown cases.
         * <p>
         * Everywhere that calls this EXCEPT for disconnect() should follow it with
         * a call to mCallback.onConnectionFailed(). Disconnect doesn't do that callback
         * for a clean shutdown, but everywhere else is a dirty shutdown and should
         * notify the app.
         */
        private void forceCloseConnection() {
            if (mServiceConnection != null) {
                mContext.unbindService(mServiceConnection);
            }
            mState = CONNECT_STATE_DISCONNECTED;
            mServiceConnection = null;
            mServiceBinderWrapper = null;
            mCallbacksMessenger = null;
            mRootId = null;
            mMediaSessionToken = null;
        }

        public boolean isConnected() {
            return mState == CONNECT_STATE_CONNECTED;
        }

        public @NonNull
        ComponentName getServiceComponent() {
            if (!isConnected()) {
                throw new IllegalStateException("getServiceComponent() called while not connected" +
                        " (state=" + mState + ")");
            }
            return mServiceComponent;
        }

        public @NonNull String getRoot() {
            if (!isConnected()) {
                throw new IllegalStateException("getRoot() called while not connected"
                        + "(state=" + getStateLabel(mState) + ")");
            }
            return mRootId;
        }

        public @Nullable
        Bundle getExtras() {
            if (!isConnected()) {
                throw new IllegalStateException("getExtras() called while not connected (state="
                        + getStateLabel(mState) + ")");
            }
            return mExtras;
        }

        public @NonNull MediaSessionCompat.Token getSessionToken() {
            if (!isConnected()) {
                throw new IllegalStateException("getSessionToken() called while not connected"
                        + "(state=" + mState + ")");
            }
            return mMediaSessionToken;
        }

        public void subscribe(@NonNull String parentId, Bundle options,
                @NonNull SubscriptionCallback callback) {
            // Check arguments.
            if (TextUtils.isEmpty(parentId)) {
                throw new IllegalArgumentException("parentId is empty.");
            }
            if (callback == null) {
                throw new IllegalArgumentException("callback is null");
            }
            // Update or create the subscription.
            Subscription sub = mSubscriptions.get(parentId);
            if (sub == null) {
                sub = new Subscription();
                mSubscriptions.put(parentId, sub);
            }
            sub.setCallbackForOptions(callback, options);

            // If we are connected, tell the service that we are watching. If we aren't
            // connected, the service will be told when we connect.
            if (mState == CONNECT_STATE_CONNECTED) {
                try {
                    mServiceBinderWrapper.addSubscription(parentId, options, mCallbacksMessenger);
                } catch (RemoteException e) {
                    // Process is crashing. We will disconnect, and upon reconnect we will
                    // automatically reregister. So nothing to do here.
                    Log.d(TAG, "addSubscription failed with RemoteException parentId=" + parentId);
                }
            }
        }

        public void unsubscribe(@NonNull String parentId, Bundle options) {
            // Check arguments.
            if (TextUtils.isEmpty(parentId)) {
                throw new IllegalArgumentException("parentId is empty.");
            }

            // Remove from our list.
            Subscription sub = mSubscriptions.get(parentId);

            // Tell the service if necessary.
            if (sub != null && sub.remove(options) && mState == CONNECT_STATE_CONNECTED) {
                try {
                    mServiceBinderWrapper.removeSubscription(
                            parentId, options, mCallbacksMessenger);
                } catch (RemoteException e) {
                    // Process is crashing. We will disconnect, and upon reconnect we will
                    // automatically reregister. So nothing to do here.
                    Log.d(TAG, "removeSubscription failed with RemoteException parentId="
                            + parentId);
                }
            }
            if (sub != null && sub.isEmpty()) {
                mSubscriptions.remove(parentId);
            }
        }

        public void getItem(@NonNull final String mediaId, @NonNull final ItemCallback cb) {
            if (TextUtils.isEmpty(mediaId)) {
                throw new IllegalArgumentException("mediaId is empty.");
            }
            if (cb == null) {
                throw new IllegalArgumentException("cb is null.");
            }
            if (mState != CONNECT_STATE_CONNECTED) {
                Log.i(TAG, "Not connected, unable to retrieve the MediaItem.");
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        cb.onError(mediaId);
                    }
                });
                return;
            }
            ResultReceiver receiver = new ItemReceiver(mediaId, cb, mHandler);
            try {
                mServiceBinderWrapper.getMediaItem(mediaId, receiver);
            } catch (RemoteException e) {
                Log.i(TAG, "Remote error getting media item.");
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        cb.onError(mediaId);
                    }
                });
            }
        }

        public void onServiceConnected(final Messenger callback, final String root,
                final MediaSessionCompat.Token session, final Bundle extra) {
            // Check to make sure there hasn't been a disconnect or a different ServiceConnection.
            if (!isCurrent(callback, "onConnect")) {
                return;
            }
            // Don't allow them to call us twice.
            if (mState != CONNECT_STATE_CONNECTING) {
                Log.w(TAG, "onConnect from service while mState=" + getStateLabel(mState)
                        + "... ignoring");
                return;
            }
            mRootId = root;
            mMediaSessionToken = session;
            mExtras = extra;
            mState = CONNECT_STATE_CONNECTED;

            if (DBG) {
                Log.d(TAG, "ServiceCallbacks.onConnect...");
                dump();
            }
            mCallback.onConnected();

            // we may receive some subscriptions before we are connected, so re-subscribe
            // everything now
            try {
                for (Map.Entry<String, Subscription> subscriptionEntry
                        : mSubscriptions.entrySet()) {
                    String id = subscriptionEntry.getKey();
                    Subscription sub = subscriptionEntry.getValue();
                    for (Bundle options : sub.getOptionsList()) {
                        mServiceBinderWrapper.addSubscription(id, options, mCallbacksMessenger);
                    }
                }
            } catch (RemoteException ex) {
                // Process is crashing. We will disconnect, and upon reconnect we will
                // automatically reregister. So nothing to do here.
                Log.d(TAG, "addSubscription failed with RemoteException.");
            }
        }

        public void onConnectionFailed(final Messenger callback) {
            Log.e(TAG, "onConnectFailed for " + mServiceComponent);

            // Check to make sure there hasn't been a disconnect or a different ServiceConnection.
            if (!isCurrent(callback, "onConnectFailed")) {
                return;
            }
            // Don't allow them to call us twice.
            if (mState != CONNECT_STATE_CONNECTING) {
                Log.w(TAG, "onConnect from service while mState=" + getStateLabel(mState)
                        + "... ignoring");
                return;
            }

            // Clean up
            forceCloseConnection();

            // Tell the app.
            mCallback.onConnectionFailed();
        }

        public void onLoadChildren(final Messenger callback, final String parentId,
                final List list, final Bundle options) {
            // Check that there hasn't been a disconnect or a different ServiceConnection.
            if (!isCurrent(callback, "onLoadChildren")) {
                return;
            }

            List<MediaItem> data = list;
            if (DBG) {
                Log.d(TAG, "onLoadChildren for " + mServiceComponent + " id=" + parentId);
            }

            // Check that the subscription is still subscribed.
            final Subscription subscription = mSubscriptions.get(parentId);
            if (subscription == null) {
                if (DBG) {
                    Log.d(TAG, "onLoadChildren for id that isn't subscribed id=" + parentId);
                }
                return;
            }

            // Tell the app.
            SubscriptionCallback subscriptionCallback = subscription.getCallback(options);
            if (subscriptionCallback != null) {
                if (options == null) {
                    subscriptionCallback.onChildrenLoaded(parentId, data);
                } else {
                    subscriptionCallback.onChildrenLoaded(parentId, data, options);
                }
            }
        }

        /**
         * For debugging.
         */
        private static String getStateLabel(int state) {
            switch (state) {
                case CONNECT_STATE_DISCONNECTED:
                    return "CONNECT_STATE_DISCONNECTED";
                case CONNECT_STATE_CONNECTING:
                    return "CONNECT_STATE_CONNECTING";
                case CONNECT_STATE_CONNECTED:
                    return "CONNECT_STATE_CONNECTED";
                case CONNECT_STATE_SUSPENDED:
                    return "CONNECT_STATE_SUSPENDED";
                default:
                    return "UNKNOWN/" + state;
            }
        }

        /**
         * Return true if {@code callback} is the current ServiceCallbacks. Also logs if it's not.
         */
        private boolean isCurrent(Messenger callback, String funcName) {
            if (mCallbacksMessenger != callback) {
                if (mState != CONNECT_STATE_DISCONNECTED) {
                    Log.i(TAG, funcName + " for " + mServiceComponent + " with mCallbacksMessenger="
                            + mCallbacksMessenger + " this=" + this);
                }
                return false;
            }
            return true;
        }

        /**
         * Log internal state.
         * @hide
         */
        void dump() {
            Log.d(TAG, "MediaBrowserCompat...");
            Log.d(TAG, "  mServiceComponent=" + mServiceComponent);
            Log.d(TAG, "  mCallback=" + mCallback);
            Log.d(TAG, "  mRootHints=" + mRootHints);
            Log.d(TAG, "  mState=" + getStateLabel(mState));
            Log.d(TAG, "  mServiceConnection=" + mServiceConnection);
            Log.d(TAG, "  mServiceBinderWrapper=" + mServiceBinderWrapper);
            Log.d(TAG, "  mCallbacksMessenger=" + mCallbacksMessenger);
            Log.d(TAG, "  mRootId=" + mRootId);
            Log.d(TAG, "  mMediaSessionToken=" + mMediaSessionToken);
        }

        /**
         * ServiceConnection to the other app.
         */
        private class MediaServiceConnection implements ServiceConnection {
            @Override
            public void onServiceConnected(final ComponentName name, final IBinder binder) {
                postOrRun(new Runnable() {
                    @Override
                    public void run() {
                        if (DBG) {
                            Log.d(TAG, "MediaServiceConnection.onServiceConnected name=" + name
                                    + " binder=" + binder);
                            dump();
                        }

                        // Make sure we are still the current connection, and that they haven't
                        // called disconnect().
                        if (!isCurrent("onServiceConnected")) {
                            return;
                        }

                        // Save their binder
                        mServiceBinderWrapper = new ServiceBinderWrapper(binder);

                        // We make a new mServiceCallbacks each time we connect so that we can drop
                        // responses from previous connections.
                        mCallbacksMessenger = new Messenger(mHandler);
                        mHandler.setCallbacksMessenger(mCallbacksMessenger);

                        mState = CONNECT_STATE_CONNECTING;

                        // Call connect, which is async. When we get a response from that we will
                        // say that we're connected.
                        try {
                            if (DBG) {
                                Log.d(TAG, "ServiceCallbacks.onConnect...");
                                dump();
                            }
                            mServiceBinderWrapper.connect(
                                    mContext, mRootHints, mCallbacksMessenger);
                        } catch (RemoteException ex) {
                            // Connect failed, which isn't good. But the auto-reconnect on the
                            // service will take over and we will come back. We will also get the
                            // onServiceDisconnected, which has all the cleanup code. So let that
                            // do it.
                            Log.w(TAG, "RemoteException during connect for " + mServiceComponent);
                            if (DBG) {
                                Log.d(TAG, "ServiceCallbacks.onConnect...");
                                dump();
                            }
                        }
                    }
                });
            }

            @Override
            public void onServiceDisconnected(final ComponentName name) {
                postOrRun(new Runnable() {
                    @Override
                    public void run() {
                        if (DBG) {
                            Log.d(TAG, "MediaServiceConnection.onServiceDisconnected name=" + name
                                    + " this=" + this + " mServiceConnection=" +
                                    mServiceConnection);
                            dump();
                        }

                        // Make sure we are still the current connection, and that they haven't
                        // called disconnect().
                        if (!isCurrent("onServiceDisconnected")) {
                            return;
                        }

                        // Clear out what we set in onServiceConnected
                        mServiceBinderWrapper = null;
                        mCallbacksMessenger = null;
                        mHandler.setCallbacksMessenger(null);

                        // And tell the app that it's suspended.
                        mState = CONNECT_STATE_SUSPENDED;
                        mCallback.onConnectionSuspended();
                    }
                });
            }

            private void postOrRun(Runnable r) {
                if (Thread.currentThread() == mHandler.getLooper().getThread()) {
                    r.run();
                } else {
                    mHandler.post(r);
                }
            }

            /**
             * Return true if this is the current ServiceConnection. Also logs if it's not.
             */
            private boolean isCurrent(String funcName) {
                if (mServiceConnection != this) {
                    if (mState != CONNECT_STATE_DISCONNECTED) {
                        // Check mState, because otherwise this log is noisy.
                        Log.i(TAG, funcName + " for " + mServiceComponent +
                                " with mServiceConnection=" + mServiceConnection + " this=" + this);
                    }
                    return false;
                }
                return true;
            }
        }
    }

    static class MediaBrowserImplApi21 implements MediaBrowserImpl, MediaBrowserServiceCallbackImpl,
            ConnectionCallback.ConnectionCallbackInternal {
        private static final boolean DBG = false;

        protected Object mBrowserObj;

        private final ComponentName mServiceComponent;
        private final CallbackHandler mHandler = new CallbackHandler(this);
        private final ArrayMap<String, Subscription> mSubscriptions = new ArrayMap<>();

        private ServiceBinderWrapper mServiceBinderWrapper;
        private Messenger mCallbacksMessenger;

        public MediaBrowserImplApi21(Context context, ComponentName serviceComponent,
                ConnectionCallback callback, Bundle rootHints) {
            mServiceComponent = serviceComponent;
            callback.setInternalConnectionCallback(this);
            mBrowserObj = MediaBrowserCompatApi21.createBrowser(context, serviceComponent,
                    callback.mConnectionCallbackObj, rootHints);
        }

        @Override
        public void connect() {
            MediaBrowserCompatApi21.connect(mBrowserObj);
        }

        @Override
        public void disconnect() {
            MediaBrowserCompatApi21.disconnect(mBrowserObj);
        }

        @Override
        public boolean isConnected() {
            return MediaBrowserCompatApi21.isConnected(mBrowserObj);
        }

        @Override
        public ComponentName getServiceComponent() {
            return MediaBrowserCompatApi21.getServiceComponent(mBrowserObj);
        }

        @NonNull
        @Override
        public String getRoot() {
            return MediaBrowserCompatApi21.getRoot(mBrowserObj);
        }

        @Nullable
        @Override
        public Bundle getExtras() {
            return MediaBrowserCompatApi21.getExtras(mBrowserObj);
        }

        @NonNull
        @Override
        public MediaSessionCompat.Token getSessionToken() {
            return MediaSessionCompat.Token.fromToken(
                    MediaBrowserCompatApi21.getSessionToken(mBrowserObj));
        }

        @Override
        public void subscribe(@NonNull final String parentId, final Bundle options,
                @NonNull final SubscriptionCallback callback) {
            // Update or create the subscription.
            SubscriptionCallbackApi21 cb21 = new SubscriptionCallbackApi21(callback, options);
            Subscription sub = mSubscriptions.get(parentId);
            if (sub == null) {
                sub = new Subscription();
                mSubscriptions.put(parentId, sub);
            }
            sub.setCallbackForOptions(cb21, options);
            if (MediaBrowserCompatApi21.isConnected(mBrowserObj)) {
                if (options == null || mServiceBinderWrapper == null) {
                    MediaBrowserCompatApi21.subscribe(
                            mBrowserObj, parentId, cb21.mSubscriptionCallbackObj);
                } else {
                    try {
                        mServiceBinderWrapper.addSubscription(
                                parentId, options, mCallbacksMessenger);
                    } catch (RemoteException e) {
                        // Process is crashing. We will disconnect, and upon reconnect we will
                        // automatically reregister. So nothing to do here.
                        Log.i(TAG, "Remote error subscribing media item: " + parentId);
                    }
                }
            }
        }

        @Override
        public void unsubscribe(@NonNull String parentId, Bundle options) {
            // Check arguments.
            if (TextUtils.isEmpty(parentId)) {
                throw new IllegalArgumentException("parentId is empty.");
            }

            // Remove from our list.
            Subscription sub = mSubscriptions.get(parentId);
            if (sub != null && sub.remove(options)) {
                // Tell the service if necessary.
                if (options == null || mServiceBinderWrapper == null) {
                    if (mServiceBinderWrapper != null || sub.isEmpty()) {
                        MediaBrowserCompatApi21.unsubscribe(mBrowserObj, parentId);
                    }
                } else if (mServiceBinderWrapper == null) {
                    try {
                        mServiceBinderWrapper.removeSubscription(
                                parentId, options, mCallbacksMessenger);
                    } catch (RemoteException e) {
                        // Process is crashing. We will disconnect, and upon reconnect we will
                        // automatically reregister. So nothing to do here.
                        Log.d(TAG, "removeSubscription failed with RemoteException parentId="
                                + parentId);
                    }
                }
            }
            if (sub != null && sub.isEmpty()) {
                mSubscriptions.remove(parentId);
            }
        }

        @Override
        public void getItem(@NonNull final String mediaId, @NonNull final ItemCallback cb) {
            if (TextUtils.isEmpty(mediaId)) {
                throw new IllegalArgumentException("mediaId is empty.");
            }
            if (cb == null) {
                throw new IllegalArgumentException("cb is null.");
            }
            if (!MediaBrowserCompatApi21.isConnected(mBrowserObj)) {
                Log.i(TAG, "Not connected, unable to retrieve the MediaItem.");
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        cb.onError(mediaId);
                    }
                });
                return;
            }
            if (mServiceBinderWrapper == null) {
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        // Default framework implementation.
                        cb.onItemLoaded(null);
                    }
                });
                return;
            }
            ResultReceiver receiver = new ItemReceiver(mediaId, cb, mHandler);
            try {
                mServiceBinderWrapper.getMediaItem(mediaId, receiver);
            } catch (RemoteException e) {
                Log.i(TAG, "Remote error getting media item: " + mediaId);
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        cb.onError(mediaId);
                    }
                });
            }
        }

        @Override
        public void onConnected() {
            Bundle extras = MediaBrowserCompatApi21.getExtras(mBrowserObj);
            if (extras == null) {
                return;
            }
            IBinder serviceBinder = BundleCompat.getBinder(extras, EXTRA_MESSENGER_BINDER);
            if (serviceBinder != null) {
                mServiceBinderWrapper = new ServiceBinderWrapper(serviceBinder);
                mCallbacksMessenger = new Messenger(mHandler);
                mHandler.setCallbacksMessenger(mCallbacksMessenger);
                try {
                    mServiceBinderWrapper.registerCallbackMessenger(mCallbacksMessenger);
                } catch (RemoteException e) {
                    Log.i(TAG, "Remote error registering client messenger." );
                }
                onServiceConnected(mCallbacksMessenger, null, null, null);
            }
        }

        @Override
        public void onConnectionSuspended() {
            mServiceBinderWrapper = null;
            mCallbacksMessenger = null;
        }

        @Override
        public void onConnectionFailed() {
            // Do noting
        }

        // FYI, this method is not called by CallbackHandler.
        @Override
        public void onServiceConnected(final Messenger callback, final String root,
                final MediaSessionCompat.Token session, final Bundle extra) {
            // we may receive some subscriptions before we are connected, so re-subscribe
            // everything now
            for (Map.Entry<String, Subscription> subscriptionEntry : mSubscriptions.entrySet()) {
                String id = subscriptionEntry.getKey();
                Subscription sub = subscriptionEntry.getValue();
                List<Bundle> optionsList = sub.getOptionsList();
                List<SubscriptionCallback> callbackList = sub.getCallbacks();
                for (int i = 0; i < optionsList.size(); ++i) {
                    if (optionsList.get(i) == null) {
                        MediaBrowserCompatApi21.subscribe(mBrowserObj, id,
                                ((SubscriptionCallbackApi21)callbackList.get(i))
                                        .mSubscriptionCallbackObj);
                    } else {
                        try {
                            mServiceBinderWrapper.addSubscription(
                                    id, optionsList.get(i), mCallbacksMessenger);
                        } catch (RemoteException e) {
                            // Process is crashing. We will disconnect, and upon reconnect we will
                            // automatically reregister. So nothing to do here.
                            Log.d(TAG, "addSubscription failed with RemoteException parentId=" + id);
                        }
                    }
                }
            }
        }

        @Override
        public void onConnectionFailed(Messenger callback) {
            // This method will not be called.
        }

        @Override
        public void onLoadChildren(Messenger callback, String parentId, List list,
                @NonNull Bundle options) {
            if (mCallbacksMessenger != callback) {
                return;
            }

            List<MediaItem> data = list;
            if (DBG) {
                Log.d(TAG, "onLoadChildren for " + mServiceComponent + " id=" + parentId);
            }

            // Check that the subscription is still subscribed.
            Subscription subscription = mSubscriptions.get(parentId);
            if (subscription == null) {
                if (DBG) {
                    Log.d(TAG, "onLoadChildren for id that isn't subscribed id=" + parentId);
                }
                return;
            }

            // Tell the app.
            subscription.getCallback(options).onChildrenLoaded(parentId, data, options);
        }
    }

    static class MediaBrowserImplApi23 extends MediaBrowserImplApi21 {
        public MediaBrowserImplApi23(Context context, ComponentName serviceComponent,
                ConnectionCallback callback, Bundle rootHints) {
            super(context, serviceComponent, callback, rootHints);
        }

        @Override
        public void getItem(@NonNull String mediaId, @NonNull ItemCallback cb) {
            MediaBrowserCompatApi23.getItem(mBrowserObj, mediaId, cb.mItemCallbackObj);
        }
    }

    private static class Subscription {
        private final List<SubscriptionCallback> mCallbacks;
        private final List<Bundle> mOptionsList;

        public Subscription() {
            mCallbacks = new ArrayList();
            mOptionsList = new ArrayList();
        }

        public boolean isEmpty() {
            return mCallbacks.isEmpty();
        }

        public List<Bundle> getOptionsList() {
            return mOptionsList;
        }

        public List<SubscriptionCallback> getCallbacks() {
            return mCallbacks;
        }

        public void setCallbackForOptions(SubscriptionCallback callback, Bundle options) {
            for (int i = 0; i < mOptionsList.size(); ++i) {
                if (MediaBrowserCompatUtils.areSameOptions(mOptionsList.get(i), options)) {
                    mCallbacks.set(i, callback);
                    return;
                }
            }
            mCallbacks.add(callback);
            mOptionsList.add(options);
        }

        public boolean remove(Bundle options) {
            for (int i = 0; i < mOptionsList.size(); ++i) {
                if (MediaBrowserCompatUtils.areSameOptions(mOptionsList.get(i), options)) {
                    mCallbacks.remove(i);
                    mOptionsList.remove(i);
                    return true;
                }
            }
            return false;
        }

        public SubscriptionCallback getCallback(Bundle options) {
            for (int i = 0; i < mOptionsList.size(); ++i) {
                if (MediaBrowserCompatUtils.areSameOptions(mOptionsList.get(i), options)) {
                    return mCallbacks.get(i);
                }
            }
            return null;
        }
    }

    private static class CallbackHandler extends Handler {
        private final MediaBrowserServiceCallbackImpl mCallbackImpl;
        private WeakReference<Messenger> mCallbacksMessengerRef;

        CallbackHandler(MediaBrowserServiceCallbackImpl callbackImpl) {
            super();
            mCallbackImpl = callbackImpl;
        }

        @Override
        public void handleMessage(Message msg) {
            if (mCallbacksMessengerRef == null) {
                return;
            }
            Bundle data = msg.getData();
            data.setClassLoader(MediaSessionCompat.class.getClassLoader());
            switch (msg.what) {
                case SERVICE_MSG_ON_CONNECT:
                    mCallbackImpl.onServiceConnected(mCallbacksMessengerRef.get(),
                            data.getString(DATA_MEDIA_ITEM_ID),
                            (MediaSessionCompat.Token) data.getParcelable(DATA_MEDIA_SESSION_TOKEN),
                            data.getBundle(DATA_ROOT_HINTS));
                    break;
                case SERVICE_MSG_ON_CONNECT_FAILED:
                    mCallbackImpl.onConnectionFailed(mCallbacksMessengerRef.get());
                    break;
                case SERVICE_MSG_ON_LOAD_CHILDREN:
                    mCallbackImpl.onLoadChildren(mCallbacksMessengerRef.get(),
                            data.getString(DATA_MEDIA_ITEM_ID),
                            data.getParcelableArrayList(DATA_MEDIA_ITEM_LIST),
                            data.getBundle(DATA_OPTIONS));
                    break;
                default:
                    Log.w(TAG, "Unhandled message: " + msg
                            + "\n  Client version: " + CLIENT_VERSION_CURRENT
                            + "\n  Service version: " + msg.arg1);
            }
        }

        void setCallbacksMessenger(Messenger callbacksMessenger) {
            mCallbacksMessengerRef = new WeakReference<>(callbacksMessenger);
        }
    }

    private static class ServiceBinderWrapper {
        private Messenger mMessenger;

        public ServiceBinderWrapper(IBinder target) {
            mMessenger = new Messenger(target);
        }

        void connect(Context context, Bundle rootHint, Messenger callbacksMessenger)
                throws RemoteException {
            Bundle data = new Bundle();
            data.putString(DATA_PACKAGE_NAME, context.getPackageName());
            data.putBundle(DATA_ROOT_HINTS, rootHint);
            sendRequest(CLIENT_MSG_CONNECT, data, callbacksMessenger);
        }

        void disconnect(Messenger callbacksMessenger) throws RemoteException {
            sendRequest(CLIENT_MSG_DISCONNECT, null, callbacksMessenger);
        }

        void addSubscription(String parentId, Bundle options, Messenger callbacksMessenger)
                throws RemoteException {
            Bundle data = new Bundle();
            data.putString(DATA_MEDIA_ITEM_ID, parentId);
            data.putBundle(DATA_OPTIONS, options);
            sendRequest(CLIENT_MSG_ADD_SUBSCRIPTION, data, callbacksMessenger);
        }

        void removeSubscription(String parentId, Bundle options, Messenger callbacksMessenger)
                throws RemoteException {
            Bundle data = new Bundle();
            data.putString(DATA_MEDIA_ITEM_ID, parentId);
            data.putBundle(DATA_OPTIONS, options);
            sendRequest(CLIENT_MSG_REMOVE_SUBSCRIPTION, data, callbacksMessenger);
        }

        void getMediaItem(String mediaId, ResultReceiver receiver) throws RemoteException {
            Bundle data = new Bundle();
            data.putString(DATA_MEDIA_ITEM_ID, mediaId);
            data.putParcelable(DATA_RESULT_RECEIVER, receiver);
            sendRequest(CLIENT_MSG_GET_MEDIA_ITEM, data, null);
        }

        void registerCallbackMessenger(Messenger callbackMessenger) throws RemoteException {
            sendRequest(CLIENT_MSG_REGISTER_CALLBACK_MESSENGER, null, callbackMessenger);
        }

        private void sendRequest(int what, Bundle data, Messenger cbMessenger)
                throws RemoteException {
            Message msg = Message.obtain();
            msg.what = what;
            msg.arg1 = CLIENT_VERSION_CURRENT;
            msg.setData(data);
            msg.replyTo = cbMessenger;
            mMessenger.send(msg);
        }
    }

    private  static class ItemReceiver extends ResultReceiver {
        private final String mMediaId;
        private final ItemCallback mCallback;

        ItemReceiver(String mediaId, ItemCallback callback, Handler handler) {
            super(handler);
            mMediaId = mediaId;
            mCallback = callback;
        }

        @Override
        protected void onReceiveResult(int resultCode, Bundle resultData) {
            resultData.setClassLoader(MediaBrowserCompat.class.getClassLoader());
            if (resultCode != 0 || resultData == null
                    || !resultData.containsKey(MediaBrowserServiceCompat.KEY_MEDIA_ITEM)) {
                mCallback.onError(mMediaId);
                return;
            }
            Parcelable item = resultData.getParcelable(MediaBrowserServiceCompat.KEY_MEDIA_ITEM);
            if (item instanceof MediaItem) {
                mCallback.onItemLoaded((MediaItem) item);
            } else {
                mCallback.onError(mMediaId);
            }
        }
    }
}
