/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.server.connectivity.tethering;

import static android.net.ConnectivityManager.TYPE_MOBILE_DUN;
import static android.net.ConnectivityManager.TYPE_MOBILE_HIPRI;
import static android.net.ConnectivityManager.TYPE_NONE;
import static android.net.ConnectivityManager.getNetworkTypeName;
import static android.net.NetworkCapabilities.NET_CAPABILITY_DUN;
import static android.net.NetworkCapabilities.NET_CAPABILITY_NOT_VPN;
import static android.net.NetworkCapabilities.TRANSPORT_CELLULAR;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.ConnectivityManager.NetworkCallback;
import android.net.IpPrefix;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.net.NetworkState;
import android.net.util.PrefixUtils;
import android.net.util.SharedLog;
import android.os.Handler;
import android.os.Process;
import android.util.Log;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.util.StateMachine;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;


/**
 * A class to centralize all the network and link properties information
 * pertaining to the current and any potential upstream network.
 *
 * The owner of UNM gets it to register network callbacks by calling the
 * following methods :
 * Calling #startTrackDefaultNetwork() to track the system default network.
 * Calling #startObserveAllNetworks() to observe all networks. Listening all
 * networks is necessary while the expression of preferred upstreams remains
 * a list of legacy connectivity types.  In future, this can be revisited.
 * Calling #registerMobileNetworkRequest() to bring up mobile DUN/HIPRI network.
 *
 * The methods and data members of this class are only to be accessed and
 * modified from the tethering master state machine thread. Any other
 * access semantics would necessitate the addition of locking.
 *
 * TODO: Move upstream selection logic here.
 *
 * All callback methods are run on the same thread as the specified target
 * state machine.  This class does not require locking when accessed from this
 * thread.  Access from other threads is not advised.
 *
 * @hide
 */
public class UpstreamNetworkMonitor {
    private static final String TAG = UpstreamNetworkMonitor.class.getSimpleName();
    private static final boolean DBG = false;
    private static final boolean VDBG = false;

    public static final int EVENT_ON_CAPABILITIES   = 1;
    public static final int EVENT_ON_LINKPROPERTIES = 2;
    public static final int EVENT_ON_LOST           = 3;
    public static final int NOTIFY_LOCAL_PREFIXES   = 10;

    private static final int CALLBACK_LISTEN_ALL = 1;
    private static final int CALLBACK_DEFAULT_INTERNET = 2;
    private static final int CALLBACK_MOBILE_REQUEST = 3;

    private final Context mContext;
    private final SharedLog mLog;
    private final StateMachine mTarget;
    private final Handler mHandler;
    private final int mWhat;
    private final HashMap<Network, NetworkState> mNetworkMap = new HashMap<>();
    private HashSet<IpPrefix> mLocalPrefixes;
    private ConnectivityManager mCM;
    private EntitlementManager mEntitlementMgr;
    private NetworkCallback mListenAllCallback;
    private NetworkCallback mDefaultNetworkCallback;
    private NetworkCallback mMobileNetworkCallback;
    private boolean mDunRequired;
    // Whether the current default upstream is mobile or not.
    private boolean mIsDefaultCellularUpstream;
    // The current system default network (not really used yet).
    private Network mDefaultInternetNetwork;
    // The current upstream network used for tethering.
    private Network mTetheringUpstreamNetwork;

    public UpstreamNetworkMonitor(Context ctx, StateMachine tgt, SharedLog log, int what) {
        mContext = ctx;
        mTarget = tgt;
        mHandler = mTarget.getHandler();
        mLog = log.forSubComponent(TAG);
        mWhat = what;
        mLocalPrefixes = new HashSet<>();
        mIsDefaultCellularUpstream = false;
    }

    @VisibleForTesting
    public UpstreamNetworkMonitor(
            ConnectivityManager cm, StateMachine tgt, SharedLog log, int what) {
        this((Context) null, tgt, log, what);
        mCM = cm;
    }

    /**
     * Tracking the system default network. This method should be called when system is ready.
     *
     * @param defaultNetworkRequest should be the same as ConnectivityService default request
     * @param entitle a EntitlementManager object to communicate between EntitlementManager and
     * UpstreamNetworkMonitor
     */
    public void startTrackDefaultNetwork(NetworkRequest defaultNetworkRequest,
            EntitlementManager entitle) {
        // This is not really a "request", just a way of tracking the system default network.
        // It's guaranteed not to actually bring up any networks because it's the same request
        // as the ConnectivityService default request, and thus shares fate with it. We can't
        // use registerDefaultNetworkCallback because it will not track the system default
        // network if there is a VPN that applies to our UID.
        if (mDefaultNetworkCallback == null) {
            final NetworkRequest trackDefaultRequest = new NetworkRequest(defaultNetworkRequest);
            mDefaultNetworkCallback = new UpstreamNetworkCallback(CALLBACK_DEFAULT_INTERNET);
            cm().requestNetwork(trackDefaultRequest, mDefaultNetworkCallback, mHandler);
        }
        if (mEntitlementMgr == null) {
            mEntitlementMgr = entitle;
        }
    }

    public void startObserveAllNetworks() {
        stop();

        final NetworkRequest listenAllRequest = new NetworkRequest.Builder()
                .clearCapabilities().build();
        mListenAllCallback = new UpstreamNetworkCallback(CALLBACK_LISTEN_ALL);
        cm().registerNetworkCallback(listenAllRequest, mListenAllCallback, mHandler);
    }

    public void stop() {
        releaseMobileNetworkRequest();

        releaseCallback(mListenAllCallback);
        mListenAllCallback = null;

        mTetheringUpstreamNetwork = null;
        mNetworkMap.clear();
    }

    public void updateMobileRequiresDun(boolean dunRequired) {
        final boolean valueChanged = (mDunRequired != dunRequired);
        mDunRequired = dunRequired;
        if (valueChanged && mobileNetworkRequested()) {
            releaseMobileNetworkRequest();
            registerMobileNetworkRequest();
        }
    }

    public boolean mobileNetworkRequested() {
        return (mMobileNetworkCallback != null);
    }

    public void registerMobileNetworkRequest() {
        if (!isCellularUpstreamPermitted()) {
            mLog.i("registerMobileNetworkRequest() is not permitted");
            releaseMobileNetworkRequest();
            return;
        }
        if (mMobileNetworkCallback != null) {
            mLog.e("registerMobileNetworkRequest() already registered");
            return;
        }
        // The following use of the legacy type system cannot be removed until
        // after upstream selection no longer finds networks by legacy type.
        // See also http://b/34364553 .
        final int legacyType = mDunRequired ? TYPE_MOBILE_DUN : TYPE_MOBILE_HIPRI;

        final NetworkRequest mobileUpstreamRequest = new NetworkRequest.Builder()
                .setCapabilities(ConnectivityManager.networkCapabilitiesForType(legacyType))
                .build();

        // The existing default network and DUN callbacks will be notified.
        // Therefore, to avoid duplicate notifications, we only register a no-op.
        mMobileNetworkCallback = new UpstreamNetworkCallback(CALLBACK_MOBILE_REQUEST);

        // TODO: Change the timeout from 0 (no onUnavailable callback) to some
        // moderate callback timeout. This might be useful for updating some UI.
        // Additionally, we log a message to aid in any subsequent debugging.
        mLog.i("requesting mobile upstream network: " + mobileUpstreamRequest);

        cm().requestNetwork(mobileUpstreamRequest, mMobileNetworkCallback, 0, legacyType, mHandler);
    }

    public void releaseMobileNetworkRequest() {
        if (mMobileNetworkCallback == null) return;

        cm().unregisterNetworkCallback(mMobileNetworkCallback);
        mMobileNetworkCallback = null;
    }

    // So many TODOs here, but chief among them is: make this functionality an
    // integral part of this class such that whenever a higher priority network
    // becomes available and useful we (a) file a request to keep it up as
    // necessary and (b) change all upstream tracking state accordingly (by
    // passing LinkProperties up to Tethering).
    public NetworkState selectPreferredUpstreamType(Iterable<Integer> preferredTypes) {
        final TypeStatePair typeStatePair = findFirstAvailableUpstreamByType(
                mNetworkMap.values(), preferredTypes, isCellularUpstreamPermitted());

        mLog.log("preferred upstream type: " + getNetworkTypeName(typeStatePair.type));

        switch (typeStatePair.type) {
            case TYPE_MOBILE_DUN:
            case TYPE_MOBILE_HIPRI:
                // Tethering just selected mobile upstream in spite of the default network being
                // not mobile. This can happen because of the priority list.
                // Notify EntitlementManager to check permission for using mobile upstream.
                if (!mIsDefaultCellularUpstream) {
                    mEntitlementMgr.maybeRunProvisioning();
                }
                // If we're on DUN, put our own grab on it.
                registerMobileNetworkRequest();
                break;
            case TYPE_NONE:
                // If we found NONE and mobile upstream is permitted we don't want to do this
                // as we want any previous requests to keep trying to bring up something we can use.
                if (!isCellularUpstreamPermitted()) releaseMobileNetworkRequest();
                break;
            default:
                // If we've found an active upstream connection that's not DUN/HIPRI
                // we should stop any outstanding DUN/HIPRI requests.
                releaseMobileNetworkRequest();
                break;
        }

        return typeStatePair.ns;
    }

    // Returns null if no current upstream available.
    public NetworkState getCurrentPreferredUpstream() {
        final NetworkState dfltState = (mDefaultInternetNetwork != null)
                ? mNetworkMap.get(mDefaultInternetNetwork)
                : null;
        if (isNetworkUsableAndNotCellular(dfltState)) return dfltState;

        if (!isCellularUpstreamPermitted()) return null;

        if (!mDunRequired) return dfltState;

        // Find a DUN network. Note that code in Tethering causes a DUN request
        // to be filed, but this might be moved into this class in future.
        return findFirstDunNetwork(mNetworkMap.values());
    }

    public void setCurrentUpstream(Network upstream) {
        mTetheringUpstreamNetwork = upstream;
    }

    public Set<IpPrefix> getLocalPrefixes() {
        return (Set<IpPrefix>) mLocalPrefixes.clone();
    }

    private boolean isCellularUpstreamPermitted() {
        if (mEntitlementMgr != null) {
            return mEntitlementMgr.isCellularUpstreamPermitted();
        } else {
            // This flow should only happens in testing.
            return true;
        }
    }

    private void handleAvailable(Network network) {
        if (mNetworkMap.containsKey(network)) return;

        if (VDBG) Log.d(TAG, "onAvailable for " + network);
        mNetworkMap.put(network, new NetworkState(null, null, null, network, null, null));
    }

    private void handleNetCap(Network network, NetworkCapabilities newNc) {
        final NetworkState prev = mNetworkMap.get(network);
        if (prev == null || newNc.equals(prev.networkCapabilities)) {
            // Ignore notifications about networks for which we have not yet
            // received onAvailable() (should never happen) and any duplicate
            // notifications (e.g. matching more than one of our callbacks).
            return;
        }

        if (VDBG) {
            Log.d(TAG, String.format("EVENT_ON_CAPABILITIES for %s: %s",
                    network, newNc));
        }

        // Log changes in upstream network signal strength, if available.
        if (network.equals(mTetheringUpstreamNetwork) && newNc.hasSignalStrength()) {
            final int newSignal = newNc.getSignalStrength();
            final String prevSignal = getSignalStrength(prev.networkCapabilities);
            mLog.logf("upstream network signal strength: %s -> %s", prevSignal, newSignal);
        }

        mNetworkMap.put(network, new NetworkState(
                null, prev.linkProperties, newNc, network, null, null));
        // TODO: If sufficient information is available to select a more
        // preferable upstream, do so now and notify the target.
        notifyTarget(EVENT_ON_CAPABILITIES, network);
    }

    private void handleLinkProp(Network network, LinkProperties newLp) {
        final NetworkState prev = mNetworkMap.get(network);
        if (prev == null || newLp.equals(prev.linkProperties)) {
            // Ignore notifications about networks for which we have not yet
            // received onAvailable() (should never happen) and any duplicate
            // notifications (e.g. matching more than one of our callbacks).
            return;
        }

        if (VDBG) {
            Log.d(TAG, String.format("EVENT_ON_LINKPROPERTIES for %s: %s",
                    network, newLp));
        }

        mNetworkMap.put(network, new NetworkState(
                null, newLp, prev.networkCapabilities, network, null, null));
        // TODO: If sufficient information is available to select a more
        // preferable upstream, do so now and notify the target.
        notifyTarget(EVENT_ON_LINKPROPERTIES, network);
    }

    private void handleSuspended(Network network) {
        if (!network.equals(mTetheringUpstreamNetwork)) return;
        mLog.log("SUSPENDED current upstream: " + network);
    }

    private void handleResumed(Network network) {
        if (!network.equals(mTetheringUpstreamNetwork)) return;
        mLog.log("RESUMED current upstream: " + network);
    }

    private void handleLost(Network network) {
        // There are few TODOs within ConnectivityService's rematching code
        // pertaining to spurious onLost() notifications.
        //
        // TODO: simplify this, probably if favor of code that:
        //     - selects a new upstream if mTetheringUpstreamNetwork has
        //       been lost (by any callback)
        //     - deletes the entry from the map only when the LISTEN_ALL
        //       callback gets notified.

        if (!mNetworkMap.containsKey(network)) {
            // Ignore loss of networks about which we had not previously
            // learned any information or for which we have already processed
            // an onLost() notification.
            return;
        }

        if (VDBG) Log.d(TAG, "EVENT_ON_LOST for " + network);

        // TODO: If sufficient information is available to select a more
        // preferable upstream, do so now and notify the target.  Likewise,
        // if the current upstream network is gone, notify the target of the
        // fact that we now have no upstream at all.
        notifyTarget(EVENT_ON_LOST, mNetworkMap.remove(network));
    }

    private void recomputeLocalPrefixes() {
        final HashSet<IpPrefix> localPrefixes = allLocalPrefixes(mNetworkMap.values());
        if (!mLocalPrefixes.equals(localPrefixes)) {
            mLocalPrefixes = localPrefixes;
            notifyTarget(NOTIFY_LOCAL_PREFIXES, localPrefixes.clone());
        }
    }

    // Fetch (and cache) a ConnectivityManager only if and when we need one.
    private ConnectivityManager cm() {
        if (mCM == null) {
            // MUST call the String variant to be able to write unittests.
            mCM = (ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
        }
        return mCM;
    }

    /**
     * A NetworkCallback class that handles information of interest directly
     * in the thread on which it is invoked. To avoid locking, this MUST be
     * run on the same thread as the target state machine's handler.
     */
    private class UpstreamNetworkCallback extends NetworkCallback {
        private final int mCallbackType;

        UpstreamNetworkCallback(int callbackType) {
            mCallbackType = callbackType;
        }

        @Override
        public void onAvailable(Network network) {
            handleAvailable(network);
        }

        @Override
        public void onCapabilitiesChanged(Network network, NetworkCapabilities newNc) {
            if (mCallbackType == CALLBACK_DEFAULT_INTERNET) {
                mDefaultInternetNetwork = network;
                final boolean newIsCellular = isCellular(newNc);
                if (mIsDefaultCellularUpstream != newIsCellular) {
                    mIsDefaultCellularUpstream = newIsCellular;
                    mEntitlementMgr.notifyUpstream(newIsCellular);
                }
                return;
            }

            handleNetCap(network, newNc);
        }

        @Override
        public void onLinkPropertiesChanged(Network network, LinkProperties newLp) {
            if (mCallbackType == CALLBACK_DEFAULT_INTERNET) return;

            handleLinkProp(network, newLp);
            // Any non-LISTEN_ALL callback will necessarily concern a network that will
            // also match the LISTEN_ALL callback by construction of the LISTEN_ALL callback.
            // So it's not useful to do this work for non-LISTEN_ALL callbacks.
            if (mCallbackType == CALLBACK_LISTEN_ALL) {
                recomputeLocalPrefixes();
            }
        }

        @Override
        public void onNetworkSuspended(Network network) {
            if (mCallbackType == CALLBACK_LISTEN_ALL) {
                handleSuspended(network);
            }
        }

        @Override
        public void onNetworkResumed(Network network) {
            if (mCallbackType == CALLBACK_LISTEN_ALL) {
                handleResumed(network);
            }
        }

        @Override
        public void onLost(Network network) {
            if (mCallbackType == CALLBACK_DEFAULT_INTERNET) {
                mDefaultInternetNetwork = null;
                mIsDefaultCellularUpstream = false;
                mEntitlementMgr.notifyUpstream(false);
                return;
            }

            handleLost(network);
            // Any non-LISTEN_ALL callback will necessarily concern a network that will
            // also match the LISTEN_ALL callback by construction of the LISTEN_ALL callback.
            // So it's not useful to do this work for non-LISTEN_ALL callbacks.
            if (mCallbackType == CALLBACK_LISTEN_ALL) {
                recomputeLocalPrefixes();
            }
        }
    }

    private void releaseCallback(NetworkCallback cb) {
        if (cb != null) cm().unregisterNetworkCallback(cb);
    }

    private void notifyTarget(int which, Network network) {
        notifyTarget(which, mNetworkMap.get(network));
    }

    private void notifyTarget(int which, Object obj) {
        mTarget.sendMessage(mWhat, which, 0, obj);
    }

    private static class TypeStatePair {
        public int type = TYPE_NONE;
        public NetworkState ns = null;
    }

    private static TypeStatePair findFirstAvailableUpstreamByType(
            Iterable<NetworkState> netStates, Iterable<Integer> preferredTypes,
            boolean isCellularUpstreamPermitted) {
        final TypeStatePair result = new TypeStatePair();

        for (int type : preferredTypes) {
            NetworkCapabilities nc;
            try {
                nc = ConnectivityManager.networkCapabilitiesForType(type);
            } catch (IllegalArgumentException iae) {
                Log.e(TAG, "No NetworkCapabilities mapping for legacy type: " +
                       ConnectivityManager.getNetworkTypeName(type));
                continue;
            }
            if (!isCellularUpstreamPermitted && isCellular(nc)) {
                continue;
            }

            nc.setSingleUid(Process.myUid());

            for (NetworkState value : netStates) {
                if (!nc.satisfiedByNetworkCapabilities(value.networkCapabilities)) {
                    continue;
                }

                result.type = type;
                result.ns = value;
                return result;
            }
        }

        return result;
    }

    private static HashSet<IpPrefix> allLocalPrefixes(Iterable<NetworkState> netStates) {
        final HashSet<IpPrefix> prefixSet = new HashSet<>();

        for (NetworkState ns : netStates) {
            final LinkProperties lp = ns.linkProperties;
            if (lp == null) continue;
            prefixSet.addAll(PrefixUtils.localPrefixesFrom(lp));
        }

        return prefixSet;
    }

    private static String getSignalStrength(NetworkCapabilities nc) {
        if (nc == null || !nc.hasSignalStrength()) return "unknown";
        return Integer.toString(nc.getSignalStrength());
    }

    private static boolean isCellular(NetworkState ns) {
        return (ns != null) && isCellular(ns.networkCapabilities);
    }

    private static boolean isCellular(NetworkCapabilities nc) {
        return (nc != null) && nc.hasTransport(TRANSPORT_CELLULAR) &&
               nc.hasCapability(NET_CAPABILITY_NOT_VPN);
    }

    private static boolean hasCapability(NetworkState ns, int netCap) {
        return (ns != null) && (ns.networkCapabilities != null) &&
               ns.networkCapabilities.hasCapability(netCap);
    }

    private static boolean isNetworkUsableAndNotCellular(NetworkState ns) {
        return (ns != null) && (ns.networkCapabilities != null) && (ns.linkProperties != null) &&
               !isCellular(ns.networkCapabilities);
    }

    private static NetworkState findFirstDunNetwork(Iterable<NetworkState> netStates) {
        for (NetworkState ns : netStates) {
            if (isCellular(ns) && hasCapability(ns, NET_CAPABILITY_DUN)) return ns;
        }

        return null;
    }
}
