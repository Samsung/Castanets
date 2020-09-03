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
package com.android.internal.os;

import android.os.BatteryStats;

/**
 * Power calculator for the camera subsystem, excluding the flashlight.
 *
 * Note: Power draw for the flash unit should be included in the FlashlightPowerCalculator.
 */
public class CameraPowerCalculator extends PowerCalculator {
    private final double mCameraPowerOnAvg;

    public CameraPowerCalculator(PowerProfile profile) {
        mCameraPowerOnAvg = profile.getAveragePower(PowerProfile.POWER_CAMERA);
    }

    @Override
    public void calculateApp(BatterySipper app, BatteryStats.Uid u, long rawRealtimeUs,
                             long rawUptimeUs, int statsType) {

        // Calculate camera power usage.  Right now, this is a (very) rough estimate based on the
        // average power usage for a typical camera application.
        final BatteryStats.Timer timer = u.getCameraTurnedOnTimer();
        if (timer != null) {
            final long totalTime = timer.getTotalTimeLocked(rawRealtimeUs, statsType) / 1000;
            app.cameraTimeMs = totalTime;
            app.cameraPowerMah = (totalTime * mCameraPowerOnAvg) / (1000*60*60);
        } else {
            app.cameraTimeMs = 0;
            app.cameraPowerMah = 0;
        }
    }
}
