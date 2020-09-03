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

import android.content.Context;
import android.net.NetworkRequest;
import android.net.ip.IpServer;
import android.net.util.SharedLog;
import android.os.Handler;
import android.telephony.SubscriptionManager;

import com.android.internal.util.StateMachine;
import com.android.server.connectivity.MockableSystemProperties;

import java.util.ArrayList;


/**
 * Capture tethering dependencies, for injection.
 *
 * @hide
 */
public class TetheringDependencies {
    /**
     * Get a reference to the offload hardware interface to be used by tethering.
     */
    public OffloadHardwareInterface getOffloadHardwareInterface(Handler h, SharedLog log) {
        return new OffloadHardwareInterface(h, log);
    }

    /**
     * Get a reference to the UpstreamNetworkMonitor to be used by tethering.
     */
    public UpstreamNetworkMonitor getUpstreamNetworkMonitor(Context ctx, StateMachine target,
            SharedLog log, int what) {
        return new UpstreamNetworkMonitor(ctx, target, log, what);
    }

    /**
     * Get a reference to the IPv6TetheringCoordinator to be used by tethering.
     */
    public IPv6TetheringCoordinator getIPv6TetheringCoordinator(
            ArrayList<IpServer> notifyList, SharedLog log) {
        return new IPv6TetheringCoordinator(notifyList, log);
    }

    /**
     * Get dependencies to be used by IpServer.
     */
    public IpServer.Dependencies getIpServerDependencies() {
        return new IpServer.Dependencies();
    }

    /**
     * Indicates whether tethering is supported on the device.
     */
    public boolean isTetheringSupported() {
        return true;
    }

    /**
     * Get the NetworkRequest that should be fulfilled by the default network.
     */
    public NetworkRequest getDefaultNetworkRequest() {
        return null;
    }

    /**
     * Get a reference to the EntitlementManager to be used by tethering.
     */
    public EntitlementManager getEntitlementManager(Context ctx, StateMachine target,
            SharedLog log, int what, MockableSystemProperties systemProperties) {
        return new EntitlementManager(ctx, target, log, what, systemProperties);
    }

    /**
     * Get default data subscription id to build TetheringConfiguration.
     */
    public int getDefaultDataSubscriptionId() {
        return SubscriptionManager.getDefaultDataSubscriptionId();
    }
}
