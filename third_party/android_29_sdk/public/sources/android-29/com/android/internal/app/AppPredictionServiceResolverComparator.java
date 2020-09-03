/*
 * Copyright 2018 The Android Open Source Project
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

package com.android.internal.app;

import static android.app.prediction.AppTargetEvent.ACTION_LAUNCH;

import android.app.prediction.AppPredictor;
import android.app.prediction.AppTarget;
import android.app.prediction.AppTargetEvent;
import android.app.prediction.AppTargetId;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.os.Message;
import android.os.UserHandle;
import android.util.Log;

import com.android.internal.app.ResolverActivity.ResolvedComponentInfo;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executors;

/**
 * Uses an {@link AppPredictor} to sort Resolver targets. If the AppPredictionService appears to be
 * disabled by returning an empty sorted target list, {@link AppPredictionServiceResolverComparator}
 * will fallback to using a {@link ResolverRankerServiceResolverComparator}.
 */
class AppPredictionServiceResolverComparator extends AbstractResolverComparator {

    private static final String TAG = "APSResolverComparator";
    private static final boolean DEBUG = false;

    private final AppPredictor mAppPredictor;
    private final Context mContext;
    private final Map<ComponentName, Integer> mTargetRanks = new HashMap<>();
    private final UserHandle mUser;
    private final Intent mIntent;
    private final String mReferrerPackage;
    // If this is non-null (and this is not destroyed), it means APS is disabled and we should fall
    // back to using the ResolverRankerService.
    private ResolverRankerServiceResolverComparator mResolverRankerService;

    AppPredictionServiceResolverComparator(
                Context context,
                Intent intent,
                String referrerPackage,
                AppPredictor appPredictor,
                UserHandle user) {
        super(context, intent);
        mContext = context;
        mIntent = intent;
        mAppPredictor = appPredictor;
        mUser = user;
        mReferrerPackage = referrerPackage;
    }

    @Override
    int compare(ResolveInfo lhs, ResolveInfo rhs) {
        if (mResolverRankerService != null) {
            return mResolverRankerService.compare(lhs, rhs);
        }
        Integer lhsRank = mTargetRanks.get(new ComponentName(lhs.activityInfo.packageName,
                lhs.activityInfo.name));
        Integer rhsRank = mTargetRanks.get(new ComponentName(rhs.activityInfo.packageName,
                rhs.activityInfo.name));
        if (lhsRank == null && rhsRank == null) {
            return 0;
        } else if (lhsRank == null) {
            return -1;
        } else if (rhsRank == null) {
            return 1;
        }
        return lhsRank - rhsRank;
    }

    @Override
    void doCompute(List<ResolvedComponentInfo> targets) {
        if (targets.isEmpty()) {
            mHandler.sendEmptyMessage(RANKER_SERVICE_RESULT);
            return;
        }
        List<AppTarget> appTargets = new ArrayList<>();
        for (ResolvedComponentInfo target : targets) {
            appTargets.add(
                    new AppTarget.Builder(
                        new AppTargetId(target.name.flattenToString()),
                            target.name.getPackageName(),
                            mUser)
                    .setClassName(target.name.getClassName())
                    .build());
        }
        mAppPredictor.sortTargets(appTargets, Executors.newSingleThreadExecutor(),
                sortedAppTargets -> {
                    if (sortedAppTargets.isEmpty()) {
                        if (DEBUG) {
                            Log.d(TAG, "AppPredictionService disabled. Using resolver.");
                        }
                        // APS for chooser is disabled. Fallback to resolver.
                        mResolverRankerService =
                                new ResolverRankerServiceResolverComparator(
                                    mContext, mIntent, mReferrerPackage,
                                        () -> mHandler.sendEmptyMessage(RANKER_SERVICE_RESULT));
                        mResolverRankerService.compute(targets);
                    } else {
                        if (DEBUG) {
                            Log.d(TAG, "AppPredictionService response received");
                        }
                        Message msg =
                            Message.obtain(mHandler, RANKER_SERVICE_RESULT, sortedAppTargets);
                        msg.sendToTarget();
                    }
                }
        );
    }

    @Override
    void handleResultMessage(Message msg) {
        // Null value is okay if we have defaulted to the ResolverRankerService.
        if (msg.what == RANKER_SERVICE_RESULT && msg.obj != null) {
            final List<AppTarget> sortedAppTargets = (List<AppTarget>) msg.obj;
            for (int i = 0; i < sortedAppTargets.size(); i++) {
                mTargetRanks.put(new ComponentName(sortedAppTargets.get(i).getPackageName(),
                        sortedAppTargets.get(i).getClassName()), i);
            }
        } else if (msg.obj == null && mResolverRankerService == null) {
            Log.e(TAG, "Unexpected null result");
        }
    }

    @Override
    float getScore(ComponentName name) {
        if (mResolverRankerService != null) {
            return mResolverRankerService.getScore(name);
        }
        Integer rank = mTargetRanks.get(name);
        if (rank == null) {
            Log.w(TAG, "Score requested for unknown component.");
            return 0f;
        }
        int consecutiveSumOfRanks = (mTargetRanks.size() - 1) * (mTargetRanks.size()) / 2;
        return 1.0f - (((float) rank) / consecutiveSumOfRanks);
    }

    @Override
    void updateModel(ComponentName componentName) {
        if (mResolverRankerService != null) {
            mResolverRankerService.updateModel(componentName);
            return;
        }
        mAppPredictor.notifyAppTargetEvent(
                new AppTargetEvent.Builder(
                    new AppTarget.Builder(
                        new AppTargetId(componentName.toString()),
                        componentName.getPackageName(), mUser)
                        .setClassName(componentName.getClassName()).build(),
                    ACTION_LAUNCH).build());
    }

    @Override
    void destroy() {
        if (mResolverRankerService != null) {
            mResolverRankerService.destroy();
            mResolverRankerService = null;
        }
    }
}
