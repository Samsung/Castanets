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

package com.android.server.wifi.hotspot2;

import android.content.Context;
import android.net.wifi.hotspot2.PasspointConfiguration;

import com.android.org.conscrypt.TrustManagerImpl;
import com.android.server.wifi.Clock;
import com.android.server.wifi.SIMAccessor;
import com.android.server.wifi.WifiKeyStore;
import com.android.server.wifi.WifiMetrics;
import com.android.server.wifi.WifiNative;

import java.security.KeyStore;
import java.security.NoSuchAlgorithmException;

import javax.net.ssl.SSLContext;

/**
 * Factory class for creating Passpoint related objects. Useful for mocking object creations
 * in the unit tests.
 */
public class PasspointObjectFactory{
    /**
     * Create a PasspointEventHandler instance.
     *
     * @param wifiNative Instance of {@link WifiNative}
     * @param callbacks Instance of {@link PasspointEventHandler.Callbacks}
     * @return {@link PasspointEventHandler}
     */
    public PasspointEventHandler makePasspointEventHandler(WifiNative wifiNative,
            PasspointEventHandler.Callbacks callbacks) {
        return new PasspointEventHandler(wifiNative, callbacks);
    }

    /**
     * Create a PasspointProvider instance.
     *
     * @param keyStore Instance of {@link WifiKeyStore}
     * @param config Configuration for the provider
     * @param providerId Unique identifier for the provider
     * @param packageName Package name of app adding/updating the {@code config}
     * @return {@link PasspointProvider}
     */
    public PasspointProvider makePasspointProvider(PasspointConfiguration config,
            WifiKeyStore keyStore, SIMAccessor simAccessor, long providerId, int creatorUid,
            String packageName) {
        return new PasspointProvider(config, keyStore, simAccessor, providerId, creatorUid,
                packageName);
    }

    /**
     * Create a {@link PasspointConfigUserStoreData} instance.
     *
     * @param keyStore Instance of {@link WifiKeyStore}
     * @param simAccessor Instance of {@link SIMAccessor}
     * @param dataSource Passpoint configuration data source
     * @return {@link PasspointConfigUserStoreData}
     */
    public PasspointConfigUserStoreData makePasspointConfigUserStoreData(WifiKeyStore keyStore,
            SIMAccessor simAccessor, PasspointConfigUserStoreData.DataSource dataSource) {
        return new PasspointConfigUserStoreData(keyStore, simAccessor, dataSource);
    }

    /**
     * Create a {@link PasspointConfigSharedStoreData} instance.
     * @param dataSource Passpoint configuration data source
     * @return {@link PasspointConfigSharedStoreData}
     */
    public PasspointConfigSharedStoreData makePasspointConfigSharedStoreData(
            PasspointConfigSharedStoreData.DataSource dataSource) {
        return new PasspointConfigSharedStoreData(dataSource);
    }

    /**
     * Create a AnqpCache instance.
     *
     * @param clock Instance of {@link Clock}
     * @return {@link AnqpCache}
     */
    public AnqpCache makeAnqpCache(Clock clock) {
        return new AnqpCache(clock);
    }

    /**
     * Create an instance of {@link ANQPRequestManager}.
     *
     * @param handler Instance of {@link PasspointEventHandler}
     * @param clock Instance of {@link Clock}
     * @return {@link ANQPRequestManager}
     */
    public ANQPRequestManager makeANQPRequestManager(PasspointEventHandler handler, Clock clock) {
        return new ANQPRequestManager(handler, clock);
    }

    /**
     * Create an instance of {@link CertificateVerifier}.
     *
     * @return {@link CertificateVerifier}
     */
    public CertificateVerifier makeCertificateVerifier() {
        return new CertificateVerifier();
    }

    /**
     * Create an instance of {@link PasspointProvisioner}.
     *
     * @param context Instance of {@link Context}
     * @param wifiNative Instance of {@link WifiNative}
     * @param passpointManager Instance of {@link PasspointManager}
     * @return {@link PasspointProvisioner}
     */
    public PasspointProvisioner makePasspointProvisioner(Context context, WifiNative wifiNative,
            PasspointManager passpointManager, WifiMetrics wifiMetrics) {
        return new PasspointProvisioner(context, wifiNative, this, passpointManager, wifiMetrics);
    }

    /**
     * Create an instance of {@link OsuNetworkConnection}.
     *
     * @param context
     * @return {@link OsuNetworkConnection}
     */
    public OsuNetworkConnection makeOsuNetworkConnection(Context context) {
        return new OsuNetworkConnection(context);
    }

    /**
     * Create an instance of {@link OsuServerConnection}.
     *
     * @return {@link OsuServerConnection}
     */
    public OsuServerConnection makeOsuServerConnection() {
        return new OsuServerConnection(null);
    }


    /**
     * Create an instance of {@link WfaKeyStore}.
     *
     * @return WfaKeyStore {@link WfaKeyStore}
     */
    public WfaKeyStore makeWfaKeyStore() {
        return new WfaKeyStore();
    }

    /**
     * Create an instance of {@link SSLContext}.
     *
     * @param tlsVersion String indicate TLS version
     * @return SSLContext an instance, corresponding to the TLS version
     */
    public SSLContext getSSLContext(String tlsVersion) {
        SSLContext tlsContext = null;
        try {
            tlsContext = SSLContext.getInstance(tlsVersion);
        } catch (NoSuchAlgorithmException e) {
            e.printStackTrace();
        }
        return tlsContext;
    }

    /**
     * Create an instance of {@link TrustManagerImpl}.
     *
     * @param ks KeyStore used to get root certs
     * @return TrustManagerImpl an instance for delegating root cert validation
     */
    public TrustManagerImpl getTrustManagerImpl(KeyStore ks) {
        return new TrustManagerImpl(ks);
    }

    /**
     * Create an instance of {@link SystemInfo}.
     *
     * @param context Instance of {@link Context}
     * @param wifiNative Instance of {@link WifiNative}
     * @return {@Link Systeminfo} that is used for getting system related info.
     */
    public SystemInfo getSystemInfo(Context context, WifiNative wifiNative) {
        return SystemInfo.getInstance(context, wifiNative);
    }
}
