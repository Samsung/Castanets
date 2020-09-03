/*
 * Copyright 2017 The Android Open Source Project
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

package android.app.servertransaction;

import static android.os.Trace.TRACE_TAG_ACTIVITY_MANAGER;

import android.app.ClientTransactionHandler;
import android.content.res.Configuration;
import android.os.IBinder;
import android.os.Parcel;
import android.os.Trace;

import java.util.Objects;

/**
 * Activity move to a different display message.
 * @hide
 */
public class MoveToDisplayItem extends ClientTransactionItem {

    private int mTargetDisplayId;
    private Configuration mConfiguration;

    @Override
    public void execute(ClientTransactionHandler client, IBinder token,
            PendingTransactionActions pendingActions) {
        Trace.traceBegin(TRACE_TAG_ACTIVITY_MANAGER, "activityMovedToDisplay");
        client.handleActivityConfigurationChanged(token, mConfiguration, mTargetDisplayId);
        Trace.traceEnd(TRACE_TAG_ACTIVITY_MANAGER);
    }


    // ObjectPoolItem implementation

    private MoveToDisplayItem() {}

    /** Obtain an instance initialized with provided params. */
    public static MoveToDisplayItem obtain(int targetDisplayId, Configuration configuration) {
        MoveToDisplayItem instance = ObjectPool.obtain(MoveToDisplayItem.class);
        if (instance == null) {
            instance = new MoveToDisplayItem();
        }
        instance.mTargetDisplayId = targetDisplayId;
        instance.mConfiguration = configuration;

        return instance;
    }

    @Override
    public void recycle() {
        mTargetDisplayId = 0;
        mConfiguration = null;
        ObjectPool.recycle(this);
    }


    // Parcelable implementation

    /** Write to Parcel. */
    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeInt(mTargetDisplayId);
        dest.writeTypedObject(mConfiguration, flags);
    }

    /** Read from Parcel. */
    private MoveToDisplayItem(Parcel in) {
        mTargetDisplayId = in.readInt();
        mConfiguration = in.readTypedObject(Configuration.CREATOR);
    }

    public static final @android.annotation.NonNull Creator<MoveToDisplayItem> CREATOR = new Creator<MoveToDisplayItem>() {
        public MoveToDisplayItem createFromParcel(Parcel in) {
            return new MoveToDisplayItem(in);
        }

        public MoveToDisplayItem[] newArray(int size) {
            return new MoveToDisplayItem[size];
        }
    };

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (o == null || getClass() != o.getClass()) {
            return false;
        }
        final MoveToDisplayItem other = (MoveToDisplayItem) o;
        return mTargetDisplayId == other.mTargetDisplayId
                && Objects.equals(mConfiguration, other.mConfiguration);
    }

    @Override
    public int hashCode() {
        int result = 17;
        result = 31 * result + mTargetDisplayId;
        result = 31 * result + mConfiguration.hashCode();
        return result;
    }

    @Override
    public String toString() {
        return "MoveToDisplayItem{targetDisplayId=" + mTargetDisplayId
                + ",configuration=" + mConfiguration + "}";
    }
}
