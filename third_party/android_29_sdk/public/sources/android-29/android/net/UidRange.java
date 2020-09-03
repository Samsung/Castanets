/*
 * Copyright (C) 2014 The Android Open Source Project
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

package android.net;

import static android.os.UserHandle.PER_USER_RANGE;

import android.os.Parcel;
import android.os.Parcelable;

import java.util.Collection;

/**
 * An inclusive range of UIDs.
 *
 * @hide
 */
public final class UidRange implements Parcelable {
    public final int start;
    public final int stop;

    public UidRange(int startUid, int stopUid) {
        if (startUid < 0) throw new IllegalArgumentException("Invalid start UID.");
        if (stopUid < 0) throw new IllegalArgumentException("Invalid stop UID.");
        if (startUid > stopUid) throw new IllegalArgumentException("Invalid UID range.");
        start = startUid;
        stop  = stopUid;
    }

    public static UidRange createForUser(int userId) {
        return new UidRange(userId * PER_USER_RANGE, (userId + 1) * PER_USER_RANGE - 1);
    }

    /** Returns the smallest user Id which is contained in this UidRange */
    public int getStartUser() {
        return start / PER_USER_RANGE;
    }

    /** Returns the largest user Id which is contained in this UidRange */
    public int getEndUser() {
        return stop / PER_USER_RANGE;
    }

    public boolean contains(int uid) {
        return start <= uid && uid <= stop;
    }

    /**
     * Returns the count of UIDs in this range.
     */
    public int count() {
        return 1 + stop - start;
    }

    /**
     * @return {@code true} if this range contains every UID contained by the {@param other} range.
     */
    public boolean containsRange(UidRange other) {
        return start <= other.start && other.stop <= stop;
    }

    @Override
    public int hashCode() {
        int result = 17;
        result = 31 * result + start;
        result = 31 * result + stop;
        return result;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (o instanceof UidRange) {
            UidRange other = (UidRange) o;
            return start == other.start && stop == other.stop;
        }
        return false;
    }

    @Override
    public String toString() {
        return start + "-" + stop;
    }

    // Implement the Parcelable interface
    // TODO: Consider making this class no longer parcelable, since all users are likely in the
    // system server.
    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeInt(start);
        dest.writeInt(stop);
    }

    public static final @android.annotation.NonNull Creator<UidRange> CREATOR =
        new Creator<UidRange>() {
            @Override
            public UidRange createFromParcel(Parcel in) {
                int start = in.readInt();
                int stop = in.readInt();

                return new UidRange(start, stop);
            }
            @Override
            public UidRange[] newArray(int size) {
                return new UidRange[size];
            }
    };

    /**
     * Returns whether any of the UidRange in the collection contains the specified uid
     *
     * @param ranges The collection of UidRange to check
     * @param uid the uid in question
     * @return {@code true} if the uid is contained within the ranges, {@code false} otherwise
     *
     * @see UidRange#contains(int)
     */
    public static boolean containsUid(Collection<UidRange> ranges, int uid) {
        if (ranges == null) return false;
        for (UidRange range : ranges) {
            if (range.contains(uid)) {
                return true;
            }
        }
        return false;
    }
}
