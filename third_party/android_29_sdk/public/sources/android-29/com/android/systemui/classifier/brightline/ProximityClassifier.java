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

package com.android.systemui.classifier.brightline;

import static com.android.internal.config.sysui.SystemUiDeviceConfigFlags.BRIGHTLINE_FALSING_PROXIMITY_PERCENT_COVERED_THRESHOLD;
import static com.android.systemui.classifier.Classifier.QUICK_SETTINGS;

import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.provider.DeviceConfig;
import android.view.MotionEvent;


/**
 * False touch if proximity sensor is covered for more than a certain percentage of the gesture.
 *
 * This classifer is essentially a no-op for QUICK_SETTINGS, as we assume the sensor may be
 * covered when swiping from the top.
 */
class ProximityClassifier extends FalsingClassifier {

    private static final float PERCENT_COVERED_THRESHOLD = 0.1f;
    private final DistanceClassifier mDistanceClassifier;
    private final float mPercentCoveredThreshold;

    private boolean mNear;
    private long mGestureStartTimeNs;
    private long mPrevNearTimeNs;
    private long mNearDurationNs;
    private float mPercentNear;

    ProximityClassifier(DistanceClassifier distanceClassifier,
            FalsingDataProvider dataProvider) {
        super(dataProvider);
        this.mDistanceClassifier = distanceClassifier;

        mPercentCoveredThreshold = DeviceConfig.getFloat(
                DeviceConfig.NAMESPACE_SYSTEMUI,
                BRIGHTLINE_FALSING_PROXIMITY_PERCENT_COVERED_THRESHOLD,
                PERCENT_COVERED_THRESHOLD);
    }

    @Override
    void onSessionStarted() {
        mPrevNearTimeNs = 0;
        mPercentNear = 0;
    }

    @Override
    void onSessionEnded() {
        mPrevNearTimeNs = 0;
        mPercentNear = 0;
    }

    @Override
    public void onTouchEvent(MotionEvent motionEvent) {
        int action = motionEvent.getActionMasked();

        if (action == MotionEvent.ACTION_DOWN) {
            mGestureStartTimeNs = motionEvent.getEventTimeNano();
            if (mPrevNearTimeNs > 0) {
                // We only care about if the proximity sensor is triggered while a move event is
                // happening.
                mPrevNearTimeNs = motionEvent.getEventTimeNano();
            }
            logDebug("Gesture start time: " + mGestureStartTimeNs);
            mNearDurationNs = 0;
        }

        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
            update(mNear, motionEvent.getEventTimeNano());
            long duration = motionEvent.getEventTimeNano() - mGestureStartTimeNs;

            logDebug("Gesture duration, Proximity duration: " + duration + ", " + mNearDurationNs);

            if (duration == 0) {
                mPercentNear = mNear ? 1.0f : 0.0f;
            } else {
                mPercentNear = (float) mNearDurationNs / (float) duration;
            }
        }

    }

    @Override
    public void onSensorEvent(SensorEvent sensorEvent) {
        if (sensorEvent.sensor.getType() == Sensor.TYPE_PROXIMITY) {
            logDebug("Sensor is: " + (sensorEvent.values[0] < sensorEvent.sensor.getMaximumRange())
                    + " at time " + sensorEvent.timestamp);
            update(
                    sensorEvent.values[0] < sensorEvent.sensor.getMaximumRange(),
                    sensorEvent.timestamp);
        }
    }

    @Override
    public boolean isFalseTouch() {
        if (getInteractionType() == QUICK_SETTINGS) {
            return false;
        }

        logInfo("Percent of gesture in proximity: " + mPercentNear);

        if (mPercentNear > mPercentCoveredThreshold) {
            return !mDistanceClassifier.isLongSwipe();
        }

        return false;
    }

    /**
     * @param near        is the sensor showing the near state right now
     * @param timeStampNs time of this event in nanoseconds
     */
    private void update(boolean near, long timeStampNs) {
        if (mPrevNearTimeNs != 0 && timeStampNs > mPrevNearTimeNs && mNear) {
            mNearDurationNs += timeStampNs - mPrevNearTimeNs;
            logDebug("Updating duration: " + mNearDurationNs);
        }

        if (near) {
            logDebug("Set prevNearTimeNs: " + timeStampNs);
            mPrevNearTimeNs = timeStampNs;
        }

        mNear = near;
    }
}
