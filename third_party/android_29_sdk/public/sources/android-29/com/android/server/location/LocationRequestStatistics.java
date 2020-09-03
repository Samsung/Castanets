package com.android.server.location;

import android.os.SystemClock;
import android.util.Log;

import java.util.HashMap;

/**
 * Holds statistics for location requests (active requests by provider).
 *
 * <p>Must be externally synchronized.
 */
public class LocationRequestStatistics {
    private static final String TAG = "LocationStats";

    // Maps package name and provider to location request statistics.
    public final HashMap<PackageProviderKey, PackageStatistics> statistics
            = new HashMap<PackageProviderKey, PackageStatistics>();

    /**
     * Signals that a package has started requesting locations.
     *
     * @param packageName Name of package that has requested locations.
     * @param providerName Name of provider that is requested (e.g. "gps").
     * @param intervalMs The interval that is requested in ms.
     */
    public void startRequesting(String packageName, String providerName, long intervalMs,
            boolean isForeground) {
        PackageProviderKey key = new PackageProviderKey(packageName, providerName);
        PackageStatistics stats = statistics.get(key);
        if (stats == null) {
            stats = new PackageStatistics();
            statistics.put(key, stats);
        }
        stats.startRequesting(intervalMs);
        stats.updateForeground(isForeground);
    }

    /**
     * Signals that a package has stopped requesting locations.
     *
     * @param packageName Name of package that has stopped requesting locations.
     * @param providerName Provider that is no longer being requested.
     */
    public void stopRequesting(String packageName, String providerName) {
        PackageProviderKey key = new PackageProviderKey(packageName, providerName);
        PackageStatistics stats = statistics.get(key);
        if (stats != null) {
            stats.stopRequesting();
        }
    }

    /**
     * Signals that a package possibly switched background/foreground.
     *
     * @param packageName Name of package that has stopped requesting locations.
     * @param providerName Provider that is no longer being requested.
     */
    public void updateForeground(String packageName, String providerName, boolean isForeground) {
        PackageProviderKey key = new PackageProviderKey(packageName, providerName);
        PackageStatistics stats = statistics.get(key);
        if (stats != null) {
            stats.updateForeground(isForeground);
        }
    }

    /**
     * A key that holds both package and provider names.
     */
    public static class PackageProviderKey {
        /**
         * Name of package requesting location.
         */
        public final String packageName;
        /**
         * Name of provider being requested (e.g. "gps").
         */
        public final String providerName;

        public PackageProviderKey(String packageName, String providerName) {
            this.packageName = packageName;
            this.providerName = providerName;
        }

        @Override
        public boolean equals(Object other) {
            if (!(other instanceof PackageProviderKey)) {
                return false;
            }

            PackageProviderKey otherKey = (PackageProviderKey) other;
            return packageName.equals(otherKey.packageName)
                    && providerName.equals(otherKey.providerName);
        }

        @Override
        public int hashCode() {
            return packageName.hashCode() + 31 * providerName.hashCode();
        }
    }

    /**
     * Usage statistics for a package/provider pair.
     */
    public static class PackageStatistics {
        // Time when this package first requested location.
        private final long mInitialElapsedTimeMs;
        // Number of active location requests this package currently has.
        private int mNumActiveRequests;
        // Time when this package most recently went from not requesting location to requesting.
        private long mLastActivitationElapsedTimeMs;
        // The fastest interval this package has ever requested.
        private long mFastestIntervalMs;
        // The slowest interval this package has ever requested.
        private long mSlowestIntervalMs;
        // The total time this app has requested location (not including currently running requests).
        private long mTotalDurationMs;

        // Time when this package most recently went to foreground, requesting location. 0 means
        // not currently in foreground.
        private long mLastForegroundElapsedTimeMs;
        // The time this app has requested location (not including currently running requests), while
        // in foreground.
        private long mForegroundDurationMs;

        // Time when package last went dormant (stopped requesting location)
        private long mLastStopElapsedTimeMs;

        private PackageStatistics() {
            mInitialElapsedTimeMs = SystemClock.elapsedRealtime();
            mNumActiveRequests = 0;
            mTotalDurationMs = 0;
            mFastestIntervalMs = Long.MAX_VALUE;
            mSlowestIntervalMs = 0;
            mForegroundDurationMs = 0;
            mLastForegroundElapsedTimeMs = 0;
            mLastStopElapsedTimeMs = 0;
        }

        private void startRequesting(long intervalMs) {
            if (mNumActiveRequests == 0) {
                mLastActivitationElapsedTimeMs = SystemClock.elapsedRealtime();
            }

            if (intervalMs < mFastestIntervalMs) {
                mFastestIntervalMs = intervalMs;
            }

            if (intervalMs > mSlowestIntervalMs) {
                mSlowestIntervalMs = intervalMs;
            }

            mNumActiveRequests++;
        }

        private void updateForeground(boolean isForeground) {
            long nowElapsedTimeMs = SystemClock.elapsedRealtime();
            // if previous interval was foreground, accumulate before resetting start
            if (mLastForegroundElapsedTimeMs != 0) {
                mForegroundDurationMs += (nowElapsedTimeMs - mLastForegroundElapsedTimeMs);
            }
            mLastForegroundElapsedTimeMs = isForeground ? nowElapsedTimeMs : 0;
        }

        private void stopRequesting() {
            if (mNumActiveRequests <= 0) {
                // Shouldn't be a possible code path
                Log.e(TAG, "Reference counting corrupted in usage statistics.");
                return;
            }

            mNumActiveRequests--;
            if (mNumActiveRequests == 0) {
                mLastStopElapsedTimeMs = SystemClock.elapsedRealtime();
                long lastDurationMs = mLastStopElapsedTimeMs - mLastActivitationElapsedTimeMs;
                mTotalDurationMs += lastDurationMs;
                updateForeground(false);
            }
        }

        /**
         * Returns the duration that this request has been active.
         */
        public long getDurationMs() {
            long currentDurationMs = mTotalDurationMs;
            if (mNumActiveRequests > 0) {
                currentDurationMs
                        += SystemClock.elapsedRealtime() - mLastActivitationElapsedTimeMs;
            }
            return currentDurationMs;
        }

        /**
         * Returns the duration that this request has been active.
         */
        public long getForegroundDurationMs() {
            long currentDurationMs = mForegroundDurationMs;
            if (mLastForegroundElapsedTimeMs != 0 ) {
                currentDurationMs
                        += SystemClock.elapsedRealtime() - mLastForegroundElapsedTimeMs;
            }
            return currentDurationMs;
        }

        /**
         * Returns the time since the initial request in ms.
         */
        public long getTimeSinceFirstRequestMs() {
            return SystemClock.elapsedRealtime() - mInitialElapsedTimeMs;
        }

        /**
         * Returns the time since the last request stopped in ms.
         */
        public long getTimeSinceLastRequestStoppedMs() {
            return SystemClock.elapsedRealtime() - mLastStopElapsedTimeMs;
        }

        /**
         * Returns the fastest interval that has been tracked.
         */
        public long getFastestIntervalMs() {
            return mFastestIntervalMs;
        }

        /**
         * Returns the slowest interval that has been tracked.
         */
        public long getSlowestIntervalMs() {
            return mSlowestIntervalMs;
        }

        /**
         * Returns true if a request is active for these tracked statistics.
         */
        public boolean isActive() {
            return mNumActiveRequests > 0;
        }

        @Override
        public String toString() {
            StringBuilder s = new StringBuilder();
            if (mFastestIntervalMs == mSlowestIntervalMs) {
                s.append("Interval ").append(mFastestIntervalMs / 1000).append(" seconds");
            } else {
                s.append("Min interval ").append(mFastestIntervalMs / 1000).append(" seconds");
                s.append(": Max interval ").append(mSlowestIntervalMs / 1000).append(" seconds");
            }
            s.append(": Duration requested ")
                    .append((getDurationMs() / 1000) / 60)
                    .append(" total, ")
                    .append((getForegroundDurationMs() / 1000) / 60)
                    .append(" foreground, out of the last ")
                    .append((getTimeSinceFirstRequestMs() / 1000) / 60)
                    .append(" minutes");
            if (isActive()) {
                s.append(": Currently active");
            } else {
                s.append(": Last active ")
                        .append((getTimeSinceLastRequestStoppedMs() / 1000) / 60)
                        .append(" minutes ago");
            }
            return s.toString();
        }
    }
}
