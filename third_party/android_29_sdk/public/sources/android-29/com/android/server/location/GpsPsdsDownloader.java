/*
 * Copyright (C) 2008 The Android Open Source Project
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

import android.net.TrafficStats;
import android.text.TextUtils;
import android.util.Log;

import com.android.internal.util.TrafficStatsConstants;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Properties;
import java.util.Random;
import java.util.concurrent.TimeUnit;

/**
 * A class for downloading GPS PSDS data.
 *
 * {@hide}
 */
public class GpsPsdsDownloader {

    private static final String TAG = "GpsPsdsDownloader";
    private static final boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG);
    private static final long MAXIMUM_CONTENT_LENGTH_BYTES = 1000000;  // 1MB.
    private static final String DEFAULT_USER_AGENT = "Android";
    private static final int CONNECTION_TIMEOUT_MS = (int) TimeUnit.SECONDS.toMillis(30);
    private static final int READ_TIMEOUT_MS = (int) TimeUnit.SECONDS.toMillis(60);

    private final String[] mPsdsServers;
    // to load balance our server requests
    private int mNextServerIndex;
    private final String mUserAgent;

    GpsPsdsDownloader(Properties properties) {
        // read PSDS servers from the Properties object
        int count = 0;
        String server1 = properties.getProperty("XTRA_SERVER_1");
        String server2 = properties.getProperty("XTRA_SERVER_2");
        String server3 = properties.getProperty("XTRA_SERVER_3");
        if (server1 != null) count++;
        if (server2 != null) count++;
        if (server3 != null) count++;

        // Set User Agent from properties, if possible.
        String agent = properties.getProperty("XTRA_USER_AGENT");
        if (TextUtils.isEmpty(agent)) {
            mUserAgent = DEFAULT_USER_AGENT;
        } else {
            mUserAgent = agent;
        }

        if (count == 0) {
            Log.e(TAG, "No PSDS servers were specified in the GPS configuration");
            mPsdsServers = null;
        } else {
            mPsdsServers = new String[count];
            count = 0;
            if (server1 != null) mPsdsServers[count++] = server1;
            if (server2 != null) mPsdsServers[count++] = server2;
            if (server3 != null) mPsdsServers[count++] = server3;

            // randomize first server
            Random random = new Random();
            mNextServerIndex = random.nextInt(count);
        }
    }

    byte[] downloadPsdsData() {
        byte[] result = null;
        int startIndex = mNextServerIndex;

        if (mPsdsServers == null) {
            return null;
        }

        // load balance our requests among the available servers
        while (result == null) {
            final int oldTag = TrafficStats.getAndSetThreadStatsTag(
                    TrafficStatsConstants.TAG_SYSTEM_GPS);
            try {
                result = doDownload(mPsdsServers[mNextServerIndex]);
            } finally {
                TrafficStats.setThreadStatsTag(oldTag);
            }

            // increment mNextServerIndex and wrap around if necessary
            mNextServerIndex++;
            if (mNextServerIndex == mPsdsServers.length) {
                mNextServerIndex = 0;
            }
            // break if we have tried all the servers
            if (mNextServerIndex == startIndex) break;
        }

        return result;
    }

    protected byte[] doDownload(String url) {
        if (DEBUG) Log.d(TAG, "Downloading PSDS data from " + url);

        HttpURLConnection connection = null;
        try {
            connection = (HttpURLConnection) (new URL(url)).openConnection();
            connection.setRequestProperty(
                    "Accept",
                    "*/*, application/vnd.wap.mms-message, application/vnd.wap.sic");
            connection.setRequestProperty(
                    "x-wap-profile",
                    "http://www.openmobilealliance.org/tech/profiles/UAPROF/ccppschema-20021212#");
            connection.setConnectTimeout(CONNECTION_TIMEOUT_MS);
            connection.setReadTimeout(READ_TIMEOUT_MS);

            connection.connect();
            int statusCode = connection.getResponseCode();
            if (statusCode != HttpURLConnection.HTTP_OK) {
                if (DEBUG) Log.d(TAG, "HTTP error downloading gps PSDS: " + statusCode);
                return null;
            }

            try (InputStream in = connection.getInputStream()) {
                ByteArrayOutputStream bytes = new ByteArrayOutputStream();
                byte[] buffer = new byte[1024];
                int count;
                while ((count = in.read(buffer)) != -1) {
                    bytes.write(buffer, 0, count);
                    if (bytes.size() > MAXIMUM_CONTENT_LENGTH_BYTES) {
                        if (DEBUG) Log.d(TAG, "PSDS file too large");
                        return null;
                    }
                }
                return bytes.toByteArray();
            }
        } catch (IOException ioe) {
            if (DEBUG) Log.d(TAG, "Error downloading gps PSDS: ", ioe);
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
        return null;
    }

}

