/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.server;

import android.app.ActivityManager;
import android.location.Geofence;
import android.location.LocationManager;
import android.location.LocationRequest;
import android.os.SystemClock;
import android.stats.location.LocationStatsEnums;
import android.util.Log;
import android.util.StatsLog;

import java.time.Instant;

/**
 * Logger for Location API usage logging.
 */
class LocationUsageLogger {
    private static final String TAG = "LocationUsageLogger";
    private static final boolean D = Log.isLoggable(TAG, Log.DEBUG);

    private static final int ONE_SEC_IN_MILLIS = 1000;
    private static final int ONE_MINUTE_IN_MILLIS = 60000;
    private static final int ONE_HOUR_IN_MILLIS = 3600000;

    private long mLastApiUsageLogHour = 0;

    private int mApiUsageLogHourlyCount = 0;

    private static final int API_USAGE_LOG_HOURLY_CAP = 60;

    private static int providerNameToStatsdEnum(String provider) {
        if (LocationManager.NETWORK_PROVIDER.equals(provider)) {
            return LocationStatsEnums.PROVIDER_NETWORK;
        } else if (LocationManager.GPS_PROVIDER.equals(provider)) {
            return LocationStatsEnums.PROVIDER_GPS;
        } else if (LocationManager.PASSIVE_PROVIDER.equals(provider)) {
            return LocationStatsEnums.PROVIDER_PASSIVE;
        } else if (LocationManager.FUSED_PROVIDER.equals(provider)) {
            return LocationStatsEnums.PROVIDER_FUSED;
        } else {
            return LocationStatsEnums.PROVIDER_UNKNOWN;
        }
    }

    private static int bucketizeIntervalToStatsdEnum(long interval) {
        // LocationManager already converts negative values to 0.
        if (interval < ONE_SEC_IN_MILLIS) {
            return LocationStatsEnums.INTERVAL_BETWEEN_0_SEC_AND_1_SEC;
        } else if (interval < ONE_SEC_IN_MILLIS * 5) {
            return LocationStatsEnums.INTERVAL_BETWEEN_1_SEC_AND_5_SEC;
        } else if (interval < ONE_MINUTE_IN_MILLIS) {
            return LocationStatsEnums.INTERVAL_BETWEEN_5_SEC_AND_1_MIN;
        } else if (interval < ONE_MINUTE_IN_MILLIS * 10) {
            return LocationStatsEnums.INTERVAL_BETWEEN_1_MIN_AND_10_MIN;
        } else if (interval < ONE_HOUR_IN_MILLIS) {
            return LocationStatsEnums.INTERVAL_BETWEEN_10_MIN_AND_1_HOUR;
        } else {
            return LocationStatsEnums.INTERVAL_LARGER_THAN_1_HOUR;
        }
    }

    private static int bucketizeSmallestDisplacementToStatsdEnum(float smallestDisplacement) {
        // LocationManager already converts negative values to 0.
        if (smallestDisplacement == 0) {
            return LocationStatsEnums.DISTANCE_ZERO;
        } else if (smallestDisplacement > 0 && smallestDisplacement <= 100) {
            return LocationStatsEnums.DISTANCE_BETWEEN_0_AND_100;
        } else {
            return LocationStatsEnums.DISTANCE_LARGER_THAN_100;
        }
    }

    private static int bucketizeRadiusToStatsdEnum(float radius) {
        if (radius < 0) {
            return LocationStatsEnums.RADIUS_NEGATIVE;
        } else if (radius < 100) {
            return LocationStatsEnums.RADIUS_BETWEEN_0_AND_100;
        } else if (radius < 200) {
            return LocationStatsEnums.RADIUS_BETWEEN_100_AND_200;
        } else if (radius < 300) {
            return LocationStatsEnums.RADIUS_BETWEEN_200_AND_300;
        } else if (radius < 1000) {
            return LocationStatsEnums.RADIUS_BETWEEN_300_AND_1000;
        } else if (radius < 10000) {
            return LocationStatsEnums.RADIUS_BETWEEN_1000_AND_10000;
        } else {
            return LocationStatsEnums.RADIUS_LARGER_THAN_100000;
        }
    }

    private static int getBucketizedExpireIn(long expireAt) {
        if (expireAt == Long.MAX_VALUE) {
            return LocationStatsEnums.EXPIRATION_NO_EXPIRY;
        }

        long elapsedRealtime = SystemClock.elapsedRealtime();
        long expireIn = Math.max(0, expireAt - elapsedRealtime);

        if (expireIn < 20 * ONE_SEC_IN_MILLIS) {
            return LocationStatsEnums.EXPIRATION_BETWEEN_0_AND_20_SEC;
        } else if (expireIn < ONE_MINUTE_IN_MILLIS) {
            return LocationStatsEnums.EXPIRATION_BETWEEN_20_SEC_AND_1_MIN;
        } else if (expireIn < ONE_MINUTE_IN_MILLIS * 10) {
            return LocationStatsEnums.EXPIRATION_BETWEEN_1_MIN_AND_10_MIN;
        } else if (expireIn < ONE_HOUR_IN_MILLIS) {
            return LocationStatsEnums.EXPIRATION_BETWEEN_10_MIN_AND_1_HOUR;
        } else {
            return LocationStatsEnums.EXPIRATION_LARGER_THAN_1_HOUR;
        }
    }

    private static int categorizeActivityImportance(int importance) {
        if (importance == ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND) {
            return LocationStatsEnums.IMPORTANCE_TOP;
        } else if (importance == ActivityManager
                                    .RunningAppProcessInfo
                                    .IMPORTANCE_FOREGROUND_SERVICE) {
            return LocationStatsEnums.IMPORTANCE_FORGROUND_SERVICE;
        } else {
            return LocationStatsEnums.IMPORTANCE_BACKGROUND;
        }
    }

    private static int getCallbackType(
            int apiType, boolean hasListener, boolean hasIntent) {
        if (apiType == LocationStatsEnums.API_SEND_EXTRA_COMMAND) {
            return LocationStatsEnums.CALLBACK_NOT_APPLICABLE;
        }

        // Listener and PendingIntent will not be set at
        // the same time.
        if (hasIntent) {
            return LocationStatsEnums.CALLBACK_PENDING_INTENT;
        } else if (hasListener) {
            return LocationStatsEnums.CALLBACK_LISTENER;
        } else {
            return LocationStatsEnums.CALLBACK_UNKNOWN;
        }
    }

    // Update the hourly count of APIUsage log event.
    // Returns false if hit the hourly log cap.
    private boolean checkApiUsageLogCap() {
        if (D) {
            Log.d(TAG, "checking APIUsage log cap.");
        }

        long currentHour = Instant.now().toEpochMilli() / ONE_HOUR_IN_MILLIS;
        if (currentHour > mLastApiUsageLogHour) {
            mLastApiUsageLogHour = currentHour;
            mApiUsageLogHourlyCount = 0;
            return true;
        } else {
            mApiUsageLogHourlyCount = Math.min(
                mApiUsageLogHourlyCount + 1, API_USAGE_LOG_HOURLY_CAP);
            return mApiUsageLogHourlyCount < API_USAGE_LOG_HOURLY_CAP;
        }
    }

    /**
     * Log a Location API usage event to Statsd.
     * Logging event is capped at 60 per hour. Usage events exceeding
     * the cap will be dropped by LocationUsageLogger.
     */
    public void logLocationApiUsage(int usageType, int apiInUse,
            String packageName, LocationRequest locationRequest,
            boolean hasListener, boolean hasIntent,
            Geofence geofence, int activityImportance) {
        try {
            if (!checkApiUsageLogCap()) {
                return;
            }

            boolean isLocationRequestNull = locationRequest == null;
            boolean isGeofenceNull = geofence == null;
            if (D) {
                Log.d(TAG, "log API Usage to statsd. usageType: " + usageType + ", apiInUse: "
                        + apiInUse + ", packageName: " + (packageName == null ? "" : packageName)
                        + ", locationRequest: "
                        + (isLocationRequestNull ? "" : locationRequest.toString())
                        + ", hasListener: " + hasListener
                        + ", hasIntent: " + hasIntent
                        + ", geofence: "
                        + (isGeofenceNull ? "" : geofence.toString())
                        + ", importance: " + activityImportance);
            }

            StatsLog.write(StatsLog.LOCATION_MANAGER_API_USAGE_REPORTED, usageType,
                    apiInUse, packageName,
                    isLocationRequestNull
                        ? LocationStatsEnums.PROVIDER_UNKNOWN
                        : providerNameToStatsdEnum(locationRequest.getProvider()),
                    isLocationRequestNull
                        ? LocationStatsEnums.QUALITY_UNKNOWN
                        : locationRequest.getQuality(),
                    isLocationRequestNull
                        ? LocationStatsEnums.INTERVAL_UNKNOWN
                        : bucketizeIntervalToStatsdEnum(locationRequest.getInterval()),
                    isLocationRequestNull
                        ? LocationStatsEnums.DISTANCE_UNKNOWN
                        : bucketizeSmallestDisplacementToStatsdEnum(
                                locationRequest.getSmallestDisplacement()),
                    isLocationRequestNull ? 0 : locationRequest.getNumUpdates(),
                    // only log expireIn for USAGE_STARTED
                    isLocationRequestNull || usageType == LocationStatsEnums.USAGE_ENDED
                        ? LocationStatsEnums.EXPIRATION_UNKNOWN
                        : getBucketizedExpireIn(locationRequest.getExpireAt()),
                    getCallbackType(apiInUse, hasListener, hasIntent),
                    isGeofenceNull
                        ? LocationStatsEnums.RADIUS_UNKNOWN
                        : bucketizeRadiusToStatsdEnum(geofence.getRadius()),
                    categorizeActivityImportance(activityImportance));
        } catch (Exception e) {
            // Swallow exceptions to avoid crashing LMS.
            Log.w(TAG, "Failed to log API usage to statsd.", e);
        }
    }

    /**
     * Log a Location API usage event to Statsd.
     * Logging event is capped at 60 per hour. Usage events exceeding
     * the cap will be dropped by LocationUsageLogger.
     */
    public void logLocationApiUsage(int usageType, int apiInUse, String providerName) {
        try {
            if (!checkApiUsageLogCap()) {
                return;
            }

            if (D) {
                Log.d(TAG, "log API Usage to statsd. usageType: " + usageType + ", apiInUse: "
                        + apiInUse + ", providerName: " + providerName);
            }

            StatsLog.write(StatsLog.LOCATION_MANAGER_API_USAGE_REPORTED, usageType, apiInUse,
                    /* package_name= */ null,
                    providerNameToStatsdEnum(providerName),
                    LocationStatsEnums.QUALITY_UNKNOWN,
                    LocationStatsEnums.INTERVAL_UNKNOWN,
                    LocationStatsEnums.DISTANCE_UNKNOWN,
                    /* numUpdates= */ 0,
                    LocationStatsEnums.EXPIRATION_UNKNOWN,
                    getCallbackType(
                            apiInUse,
                            /* isListenerNull= */ true,
                            /* isIntentNull= */ true),
                    /* bucketizedRadius= */ 0,
                    LocationStatsEnums.IMPORTANCE_UNKNOWN);
        } catch (Exception e) {
            // Swallow exceptions to avoid crashing LMS.
            Log.w(TAG, "Failed to log API usage to statsd.", e);
        }
    }
}
