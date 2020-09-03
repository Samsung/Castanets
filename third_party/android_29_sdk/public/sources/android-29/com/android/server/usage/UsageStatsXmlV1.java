/**
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */
package com.android.server.usage;

import android.app.usage.ConfigurationStats;
import android.app.usage.UsageEvents;
import android.app.usage.UsageStats;
import android.content.res.Configuration;
import android.util.ArrayMap;
import android.util.Log;

import com.android.internal.util.XmlUtils;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import org.xmlpull.v1.XmlSerializer;

import java.io.IOException;
import java.net.ProtocolException;

/**
 * UsageStats reader/writer for version 1 of the XML format.
 */
final class UsageStatsXmlV1 {
    private static final String TAG = "UsageStatsXmlV1";

    private static final String INTERACTIVE_TAG = "interactive";
    private static final String NON_INTERACTIVE_TAG = "non-interactive";
    private static final String KEYGUARD_SHOWN_TAG = "keyguard-shown";
    private static final String KEYGUARD_HIDDEN_TAG = "keyguard-hidden";

    private static final String PACKAGES_TAG = "packages";
    private static final String PACKAGE_TAG = "package";

    private static final String CHOOSER_COUNT_TAG = "chosen_action";
    private static final String CATEGORY_TAG = "category";
    private static final String NAME = "name";
    private static final String COUNT = "count";

    private static final String CONFIGURATIONS_TAG = "configurations";
    private static final String CONFIG_TAG = "config";

    private static final String EVENT_LOG_TAG = "event-log";
    private static final String EVENT_TAG = "event";

    // Attributes
    private static final String PACKAGE_ATTR = "package";
    private static final String FLAGS_ATTR = "flags";
    private static final String CLASS_ATTR = "class";
    private static final String TOTAL_TIME_ACTIVE_ATTR = "timeActive";
    private static final String TOTAL_TIME_VISIBLE_ATTR = "timeVisible";
    private static final String TOTAL_TIME_SERVICE_USED_ATTR = "timeServiceUsed";
    private static final String COUNT_ATTR = "count";
    private static final String ACTIVE_ATTR = "active";
    private static final String LAST_EVENT_ATTR = "lastEvent";
    private static final String TYPE_ATTR = "type";
    private static final String INSTANCE_ID_ATTR = "instanceId";
    private static final String SHORTCUT_ID_ATTR = "shortcutId";
    private static final String STANDBY_BUCKET_ATTR = "standbyBucket";
    private static final String APP_LAUNCH_COUNT_ATTR = "appLaunchCount";
    private static final String NOTIFICATION_CHANNEL_ATTR = "notificationChannel";
    private static final String MAJOR_VERSION_ATTR = "majorVersion";
    private static final String MINOR_VERSION_ATTR = "minorVersion";

    // Time attributes stored as an offset of the beginTime.
    private static final String LAST_TIME_ACTIVE_ATTR = "lastTimeActive";
    private static final String LAST_TIME_VISIBLE_ATTR = "lastTimeVisible";
    private static final String LAST_TIME_SERVICE_USED_ATTR = "lastTimeServiceUsed";
    private static final String END_TIME_ATTR = "endTime";
    private static final String TIME_ATTR = "time";

    private static void loadUsageStats(XmlPullParser parser, IntervalStats statsOut)
            throws XmlPullParserException, IOException {
        final String pkg = parser.getAttributeValue(null, PACKAGE_ATTR);
        if (pkg == null) {
            throw new ProtocolException("no " + PACKAGE_ATTR + " attribute present");
        }
        final UsageStats stats = statsOut.getOrCreateUsageStats(pkg);

        // Apply the offset to the beginTime to find the absolute time.
        stats.mLastTimeUsed = statsOut.beginTime + XmlUtils.readLongAttribute(
                parser, LAST_TIME_ACTIVE_ATTR);

        try {
            stats.mLastTimeVisible = statsOut.beginTime + XmlUtils.readLongAttribute(
                    parser, LAST_TIME_VISIBLE_ATTR);
        } catch (IOException e) {
            Log.i(TAG, "Failed to parse mLastTimeVisible");
        }

        try {
            stats.mLastTimeForegroundServiceUsed = statsOut.beginTime + XmlUtils.readLongAttribute(
                    parser, LAST_TIME_SERVICE_USED_ATTR);
        } catch (IOException e) {
            Log.i(TAG, "Failed to parse mLastTimeForegroundServiceUsed");
        }

        stats.mTotalTimeInForeground = XmlUtils.readLongAttribute(parser, TOTAL_TIME_ACTIVE_ATTR);

        try {
            stats.mTotalTimeVisible = XmlUtils.readLongAttribute(parser, TOTAL_TIME_VISIBLE_ATTR);
        } catch (IOException e) {
            Log.i(TAG, "Failed to parse mTotalTimeVisible");
        }

        try {
            stats.mTotalTimeForegroundServiceUsed = XmlUtils.readLongAttribute(parser,
                    TOTAL_TIME_SERVICE_USED_ATTR);
        } catch (IOException e) {
            Log.i(TAG, "Failed to parse mTotalTimeForegroundServiceUsed");
        }

        stats.mLastEvent = XmlUtils.readIntAttribute(parser, LAST_EVENT_ATTR);
        stats.mAppLaunchCount = XmlUtils.readIntAttribute(parser, APP_LAUNCH_COUNT_ATTR,
                0);
        int eventCode;
        while ((eventCode = parser.next()) != XmlPullParser.END_DOCUMENT) {
            final String tag = parser.getName();
            if (eventCode == XmlPullParser.END_TAG && tag.equals(PACKAGE_TAG)) {
                break;
            }
            if (eventCode != XmlPullParser.START_TAG) {
                continue;
            }
            if (tag.equals(CHOOSER_COUNT_TAG)) {
                String action = XmlUtils.readStringAttribute(parser, NAME);
                loadChooserCounts(parser, stats, action);
            }
        }
    }

    private static void loadCountAndTime(XmlPullParser parser,
            IntervalStats.EventTracker tracker)
            throws IOException, XmlPullParserException {
        tracker.count = XmlUtils.readIntAttribute(parser, COUNT_ATTR, 0);
        tracker.duration = XmlUtils.readLongAttribute(parser, TIME_ATTR, 0);
        XmlUtils.skipCurrentTag(parser);
    }

    private static void loadChooserCounts(
            XmlPullParser parser, UsageStats usageStats, String action)
            throws XmlPullParserException, IOException {
        if (action == null) {
            return;
        }
        if (usageStats.mChooserCounts == null) {
            usageStats.mChooserCounts = new ArrayMap<>();
        }
        if (!usageStats.mChooserCounts.containsKey(action)) {
            ArrayMap<String, Integer> counts = new ArrayMap<>();
            usageStats.mChooserCounts.put(action, counts);
        }

        int eventCode;
        while ((eventCode = parser.next()) != XmlPullParser.END_DOCUMENT) {
            final String tag = parser.getName();
            if (eventCode == XmlPullParser.END_TAG && tag.equals(CHOOSER_COUNT_TAG)) {
                break;
            }
            if (eventCode != XmlPullParser.START_TAG) {
                continue;
            }
            if (tag.equals(CATEGORY_TAG)) {
                String category = XmlUtils.readStringAttribute(parser, NAME);
                int count = XmlUtils.readIntAttribute(parser, COUNT);
                usageStats.mChooserCounts.get(action).put(category, count);
            }
        }
    }

    private static void loadConfigStats(XmlPullParser parser, IntervalStats statsOut)
            throws XmlPullParserException, IOException {
        final Configuration config = new Configuration();
        Configuration.readXmlAttrs(parser, config);

        final ConfigurationStats configStats = statsOut.getOrCreateConfigurationStats(config);

        // Apply the offset to the beginTime to find the absolute time.
        configStats.mLastTimeActive = statsOut.beginTime + XmlUtils.readLongAttribute(
                parser, LAST_TIME_ACTIVE_ATTR);

        configStats.mTotalTimeActive = XmlUtils.readLongAttribute(parser, TOTAL_TIME_ACTIVE_ATTR);
        configStats.mActivationCount = XmlUtils.readIntAttribute(parser, COUNT_ATTR);
        if (XmlUtils.readBooleanAttribute(parser, ACTIVE_ATTR)) {
            statsOut.activeConfiguration = configStats.mConfiguration;
        }
    }

    private static void loadEvent(XmlPullParser parser, IntervalStats statsOut)
            throws XmlPullParserException, IOException {
        final String packageName = XmlUtils.readStringAttribute(parser, PACKAGE_ATTR);
        if (packageName == null) {
            throw new ProtocolException("no " + PACKAGE_ATTR + " attribute present");
        }
        final String className = XmlUtils.readStringAttribute(parser, CLASS_ATTR);

        final UsageEvents.Event event = statsOut.buildEvent(packageName, className);

        event.mFlags = XmlUtils.readIntAttribute(parser, FLAGS_ATTR, 0);

        // Apply the offset to the beginTime to find the absolute time of this event.
        event.mTimeStamp = statsOut.beginTime + XmlUtils.readLongAttribute(parser, TIME_ATTR);

        event.mEventType = XmlUtils.readIntAttribute(parser, TYPE_ATTR);

        try {
            event.mInstanceId = XmlUtils.readIntAttribute(parser, INSTANCE_ID_ATTR);
        } catch (IOException e) {
            Log.e(TAG, "Failed to parse mInstanceId", e);
        }

        switch (event.mEventType) {
            case UsageEvents.Event.CONFIGURATION_CHANGE:
                event.mConfiguration = new Configuration();
                Configuration.readXmlAttrs(parser, event.mConfiguration);
                break;
            case UsageEvents.Event.SHORTCUT_INVOCATION:
                final String id = XmlUtils.readStringAttribute(parser, SHORTCUT_ID_ATTR);
                event.mShortcutId = (id != null) ? id.intern() : null;
                break;
            case UsageEvents.Event.STANDBY_BUCKET_CHANGED:
                event.mBucketAndReason = XmlUtils.readIntAttribute(parser, STANDBY_BUCKET_ATTR, 0);
                break;
            case UsageEvents.Event.NOTIFICATION_INTERRUPTION:
                final String channelId =
                        XmlUtils.readStringAttribute(parser, NOTIFICATION_CHANNEL_ATTR);
                event.mNotificationChannelId = (channelId != null) ? channelId.intern() : null;
                break;
        }
        statsOut.addEvent(event);
    }

    private static void writeUsageStats(XmlSerializer xml, final IntervalStats stats,
            final UsageStats usageStats) throws IOException {
        xml.startTag(null, PACKAGE_TAG);

        // Write the time offset.
        XmlUtils.writeLongAttribute(xml, LAST_TIME_ACTIVE_ATTR,
                usageStats.mLastTimeUsed - stats.beginTime);
        XmlUtils.writeLongAttribute(xml, LAST_TIME_VISIBLE_ATTR,
                usageStats.mLastTimeVisible - stats.beginTime);
        XmlUtils.writeLongAttribute(xml, LAST_TIME_SERVICE_USED_ATTR,
                usageStats.mLastTimeForegroundServiceUsed - stats.beginTime);
        XmlUtils.writeStringAttribute(xml, PACKAGE_ATTR, usageStats.mPackageName);
        XmlUtils.writeLongAttribute(xml, TOTAL_TIME_ACTIVE_ATTR, usageStats.mTotalTimeInForeground);
        XmlUtils.writeLongAttribute(xml, TOTAL_TIME_VISIBLE_ATTR, usageStats.mTotalTimeVisible);
        XmlUtils.writeLongAttribute(xml, TOTAL_TIME_SERVICE_USED_ATTR,
                usageStats.mTotalTimeForegroundServiceUsed);
        XmlUtils.writeIntAttribute(xml, LAST_EVENT_ATTR, usageStats.mLastEvent);
        if (usageStats.mAppLaunchCount > 0) {
            XmlUtils.writeIntAttribute(xml, APP_LAUNCH_COUNT_ATTR, usageStats.mAppLaunchCount);
        }
        writeChooserCounts(xml, usageStats);
        xml.endTag(null, PACKAGE_TAG);
    }

    private static void writeCountAndTime(XmlSerializer xml, String tag, int count, long time)
            throws IOException {
        xml.startTag(null, tag);
        XmlUtils.writeIntAttribute(xml, COUNT_ATTR, count);
        XmlUtils.writeLongAttribute(xml, TIME_ATTR, time);
        xml.endTag(null, tag);
    }

    private static void writeChooserCounts(XmlSerializer xml, final UsageStats usageStats)
            throws IOException {
        if (usageStats == null || usageStats.mChooserCounts == null ||
                usageStats.mChooserCounts.keySet().isEmpty()) {
            return;
        }
        final int chooserCountSize = usageStats.mChooserCounts.size();
        for (int i = 0; i < chooserCountSize; i++) {
            final String action = usageStats.mChooserCounts.keyAt(i);
            final ArrayMap<String, Integer> counts = usageStats.mChooserCounts.valueAt(i);
            if (action == null || counts == null || counts.isEmpty()) {
                continue;
            }
            xml.startTag(null, CHOOSER_COUNT_TAG);
            XmlUtils.writeStringAttribute(xml, NAME, action);
            writeCountsForAction(xml, counts);
            xml.endTag(null, CHOOSER_COUNT_TAG);
        }
    }

    private static void writeCountsForAction(XmlSerializer xml, ArrayMap<String, Integer> counts)
            throws IOException {
        final int countsSize = counts.size();
        for (int i = 0; i < countsSize; i++) {
            String key = counts.keyAt(i);
            int count = counts.valueAt(i);
            if (count > 0) {
                xml.startTag(null, CATEGORY_TAG);
                XmlUtils.writeStringAttribute(xml, NAME, key);
                XmlUtils.writeIntAttribute(xml, COUNT, count);
                xml.endTag(null, CATEGORY_TAG);
            }
        }
    }

    private static void writeConfigStats(XmlSerializer xml, final IntervalStats stats,
            final ConfigurationStats configStats, boolean isActive) throws IOException {
        xml.startTag(null, CONFIG_TAG);

        // Write the time offset.
        XmlUtils.writeLongAttribute(xml, LAST_TIME_ACTIVE_ATTR,
                configStats.mLastTimeActive - stats.beginTime);

        XmlUtils.writeLongAttribute(xml, TOTAL_TIME_ACTIVE_ATTR, configStats.mTotalTimeActive);
        XmlUtils.writeIntAttribute(xml, COUNT_ATTR, configStats.mActivationCount);
        if (isActive) {
            XmlUtils.writeBooleanAttribute(xml, ACTIVE_ATTR, true);
        }

        // Now write the attributes representing the configuration object.
        Configuration.writeXmlAttrs(xml, configStats.mConfiguration);

        xml.endTag(null, CONFIG_TAG);
    }

    private static void writeEvent(XmlSerializer xml, final IntervalStats stats,
            final UsageEvents.Event event) throws IOException {
        xml.startTag(null, EVENT_TAG);

        // Store the time offset.
        XmlUtils.writeLongAttribute(xml, TIME_ATTR, event.mTimeStamp - stats.beginTime);

        XmlUtils.writeStringAttribute(xml, PACKAGE_ATTR, event.mPackage);
        if (event.mClass != null) {
            XmlUtils.writeStringAttribute(xml, CLASS_ATTR, event.mClass);
        }
        XmlUtils.writeIntAttribute(xml, FLAGS_ATTR, event.mFlags);
        XmlUtils.writeIntAttribute(xml, TYPE_ATTR, event.mEventType);
        XmlUtils.writeIntAttribute(xml, INSTANCE_ID_ATTR, event.mInstanceId);

        switch (event.mEventType) {
            case UsageEvents.Event.CONFIGURATION_CHANGE:
                if (event.mConfiguration != null) {
                    Configuration.writeXmlAttrs(xml, event.mConfiguration);
                }
                break;
            case UsageEvents.Event.SHORTCUT_INVOCATION:
                if (event.mShortcutId != null) {
                    XmlUtils.writeStringAttribute(xml, SHORTCUT_ID_ATTR, event.mShortcutId);
                }
                break;
            case UsageEvents.Event.STANDBY_BUCKET_CHANGED:
                if (event.mBucketAndReason != 0) {
                    XmlUtils.writeIntAttribute(xml, STANDBY_BUCKET_ATTR, event.mBucketAndReason);
                }
                break;
            case UsageEvents.Event.NOTIFICATION_INTERRUPTION:
                if (event.mNotificationChannelId != null) {
                    XmlUtils.writeStringAttribute(
                            xml, NOTIFICATION_CHANNEL_ATTR, event.mNotificationChannelId);
                }
                break;
        }

        xml.endTag(null, EVENT_TAG);
    }

    /**
     * Reads from the {@link XmlPullParser}, assuming that it is already on the
     * <code><usagestats></code> tag.
     *
     * @param parser The parser from which to read events.
     * @param statsOut The stats object to populate with the data from the XML file.
     */
    public static void read(XmlPullParser parser, IntervalStats statsOut)
            throws XmlPullParserException, IOException {
        statsOut.packageStats.clear();
        statsOut.configurations.clear();
        statsOut.activeConfiguration = null;
        statsOut.events.clear();

        statsOut.endTime = statsOut.beginTime + XmlUtils.readLongAttribute(parser, END_TIME_ATTR);
        try {
            statsOut.majorVersion = XmlUtils.readIntAttribute(parser, MAJOR_VERSION_ATTR);
        } catch (IOException e) {
            Log.e(TAG, "Failed to parse majorVersion", e);
        }

        try {
            statsOut.minorVersion = XmlUtils.readIntAttribute(parser, MINOR_VERSION_ATTR);
        } catch (IOException e) {
            Log.e(TAG, "Failed to parse minorVersion", e);
        }

        int eventCode;
        int outerDepth = parser.getDepth();
        while ((eventCode = parser.next()) != XmlPullParser.END_DOCUMENT
                && (eventCode != XmlPullParser.END_TAG || parser.getDepth() > outerDepth)) {
            if (eventCode != XmlPullParser.START_TAG) {
                continue;
            }

            final String tag = parser.getName();
            switch (tag) {
                case INTERACTIVE_TAG:
                    loadCountAndTime(parser, statsOut.interactiveTracker);
                    break;

                case NON_INTERACTIVE_TAG:
                    loadCountAndTime(parser, statsOut.nonInteractiveTracker);
                    break;

                case KEYGUARD_SHOWN_TAG:
                    loadCountAndTime(parser, statsOut.keyguardShownTracker);
                    break;

                case KEYGUARD_HIDDEN_TAG:
                    loadCountAndTime(parser, statsOut.keyguardHiddenTracker);
                    break;

                case PACKAGE_TAG:
                    loadUsageStats(parser, statsOut);
                    break;

                case CONFIG_TAG:
                    loadConfigStats(parser, statsOut);
                    break;

                case EVENT_TAG:
                    loadEvent(parser, statsOut);
                    break;
            }
        }
    }

    /**
     * Writes the stats object to an XML file. The {@link XmlSerializer}
     * has already written the <code><usagestats></code> tag, but attributes may still
     * be added.
     *
     * @param xml The serializer to which to write the packageStats data.
     * @param stats The stats object to write to the XML file.
     */
    public static void write(XmlSerializer xml, IntervalStats stats) throws IOException {
        XmlUtils.writeLongAttribute(xml, END_TIME_ATTR, stats.endTime - stats.beginTime);
        XmlUtils.writeIntAttribute(xml, MAJOR_VERSION_ATTR, stats.majorVersion);
        XmlUtils.writeIntAttribute(xml, MINOR_VERSION_ATTR, stats.minorVersion);

        writeCountAndTime(xml, INTERACTIVE_TAG, stats.interactiveTracker.count,
                stats.interactiveTracker.duration);
        writeCountAndTime(xml, NON_INTERACTIVE_TAG, stats.nonInteractiveTracker.count,
                stats.nonInteractiveTracker.duration);
        writeCountAndTime(xml, KEYGUARD_SHOWN_TAG, stats.keyguardShownTracker.count,
                stats.keyguardShownTracker.duration);
        writeCountAndTime(xml, KEYGUARD_HIDDEN_TAG, stats.keyguardHiddenTracker.count,
                stats.keyguardHiddenTracker.duration);

        xml.startTag(null, PACKAGES_TAG);
        final int statsCount = stats.packageStats.size();
        for (int i = 0; i < statsCount; i++) {
            writeUsageStats(xml, stats, stats.packageStats.valueAt(i));
        }
        xml.endTag(null, PACKAGES_TAG);

        xml.startTag(null, CONFIGURATIONS_TAG);
        final int configCount = stats.configurations.size();
        for (int i = 0; i < configCount; i++) {
            boolean active = stats.activeConfiguration.equals(stats.configurations.keyAt(i));
            writeConfigStats(xml, stats, stats.configurations.valueAt(i), active);
        }
        xml.endTag(null, CONFIGURATIONS_TAG);

        xml.startTag(null, EVENT_LOG_TAG);
        final int eventCount = stats.events.size();
        for (int i = 0; i < eventCount; i++) {
            writeEvent(xml, stats, stats.events.get(i));
        }
        xml.endTag(null, EVENT_LOG_TAG);
    }

    private UsageStatsXmlV1() {
    }
}
