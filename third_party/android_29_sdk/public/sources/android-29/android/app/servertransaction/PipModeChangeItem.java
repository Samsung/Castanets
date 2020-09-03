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

import android.app.ClientTransactionHandler;
import android.content.res.Configuration;
import android.os.IBinder;
import android.os.Parcel;

import java.util.Objects;

/**
 * Picture in picture mode change message.
 * @hide
 */
// TODO(lifecycler): Remove the use of this and just use the configuration change message to
// communicate multi-window mode change with WindowConfiguration.
public class PipModeChangeItem extends ClientTransactionItem {

    private boolean mIsInPipMode;
    private Configuration mOverrideConfig;

    @Override
    public void execute(ClientTransactionHandler client, IBinder token,
            PendingTransactionActions pendingActions) {
        client.handlePictureInPictureModeChanged(token, mIsInPipMode, mOverrideConfig);
    }


    // ObjectPoolItem implementation

    private PipModeChangeItem() {}

    /** Obtain an instance initialized with provided params. */
    public static PipModeChangeItem obtain(boolean isInPipMode, Configuration overrideConfig) {
        PipModeChangeItem instance = ObjectPool.obtain(PipModeChangeItem.class);
        if (instance == null) {
            instance = new PipModeChangeItem();
        }
        instance.mIsInPipMode = isInPipMode;
        instance.mOverrideConfig = overrideConfig;

        return instance;
    }

    @Override
    public void recycle() {
        mIsInPipMode = false;
        mOverrideConfig = null;
        ObjectPool.recycle(this);
    }


    // Parcelable implementation

    /** Write to Parcel. */
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeBoolean(mIsInPipMode);
        dest.writeTypedObject(mOverrideConfig, flags);
    }

    /** Read from Parcel. */
    private PipModeChangeItem(Parcel in) {
        mIsInPipMode = in.readBoolean();
        mOverrideConfig = in.readTypedObject(Configuration.CREATOR);
    }

    public static final @android.annotation.NonNull Creator<PipModeChangeItem> CREATOR =
            new Creator<PipModeChangeItem>() {
        public PipModeChangeItem createFromParcel(Parcel in) {
            return new PipModeChangeItem(in);
        }

        public PipModeChangeItem[] newArray(int size) {
            return new PipModeChangeItem[size];
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
        final PipModeChangeItem other = (PipModeChangeItem) o;
        return mIsInPipMode == other.mIsInPipMode
                && Objects.equals(mOverrideConfig, other.mOverrideConfig);
    }

    @Override
    public int hashCode() {
        int result = 17;
        result = 31 * result + (mIsInPipMode ? 1 : 0);
        result = 31 * result + mOverrideConfig.hashCode();
        return result;
    }

    @Override
    public String toString() {
        return "PipModeChangeItem{isInPipMode=" + mIsInPipMode
                + ",overrideConfig=" + mOverrideConfig + "}";
    }
}
