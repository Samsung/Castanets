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
 * limitations under the License
 */

package android.location;

import android.annotation.IntDef;
import android.annotation.NonNull;
import android.annotation.TestApi;
import android.os.Parcel;
import android.os.Parcelable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A class representing a GNSS satellite measurement, containing raw and computed information.
 */
public final class GnssMeasurement implements Parcelable {
    private int mFlags;
    private int mSvid;
    private int mConstellationType;
    private double mTimeOffsetNanos;
    private int mState;
    private long mReceivedSvTimeNanos;
    private long mReceivedSvTimeUncertaintyNanos;
    private double mCn0DbHz;
    private double mPseudorangeRateMetersPerSecond;
    private double mPseudorangeRateUncertaintyMetersPerSecond;
    private int mAccumulatedDeltaRangeState;
    private double mAccumulatedDeltaRangeMeters;
    private double mAccumulatedDeltaRangeUncertaintyMeters;
    private float mCarrierFrequencyHz;
    private long mCarrierCycles;
    private double mCarrierPhase;
    private double mCarrierPhaseUncertainty;
    private int mMultipathIndicator;
    private double mSnrInDb;
    private double mAutomaticGainControlLevelInDb;
    @NonNull private String mCodeType;

    // The following enumerations must be in sync with the values declared in gps.h

    private static final int HAS_NO_FLAGS = 0;
    private static final int HAS_SNR = (1<<0);
    private static final int HAS_CARRIER_FREQUENCY = (1<<9);
    private static final int HAS_CARRIER_CYCLES = (1<<10);
    private static final int HAS_CARRIER_PHASE = (1<<11);
    private static final int HAS_CARRIER_PHASE_UNCERTAINTY = (1<<12);
    private static final int HAS_AUTOMATIC_GAIN_CONTROL = (1<<13);
    private static final int HAS_CODE_TYPE = (1 << 14);

    /**
     * The status of the multipath indicator.
     * @hide
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({MULTIPATH_INDICATOR_UNKNOWN, MULTIPATH_INDICATOR_DETECTED,
            MULTIPATH_INDICATOR_NOT_DETECTED})
    public @interface MultipathIndicator {}

    /**
     * The indicator is not available or the presence or absence of multipath is unknown.
     */
    public static final int MULTIPATH_INDICATOR_UNKNOWN = 0;

    /**
     * The measurement shows signs of multi-path.
     */
    public static final int MULTIPATH_INDICATOR_DETECTED = 1;

    /**
     * The measurement shows no signs of multi-path.
     */
    public static final int MULTIPATH_INDICATOR_NOT_DETECTED = 2;

    /**
     * GNSS measurement tracking loop state
     * @hide
     */
    @IntDef(flag = true, prefix = { "STATE_" }, value = {
            STATE_CODE_LOCK, STATE_BIT_SYNC, STATE_SUBFRAME_SYNC,
            STATE_TOW_DECODED, STATE_MSEC_AMBIGUOUS, STATE_SYMBOL_SYNC, STATE_GLO_STRING_SYNC,
            STATE_GLO_TOD_DECODED, STATE_BDS_D2_BIT_SYNC, STATE_BDS_D2_SUBFRAME_SYNC,
            STATE_GAL_E1BC_CODE_LOCK, STATE_GAL_E1C_2ND_CODE_LOCK, STATE_GAL_E1B_PAGE_SYNC,
            STATE_SBAS_SYNC, STATE_TOW_KNOWN, STATE_GLO_TOD_KNOWN, STATE_2ND_CODE_LOCK
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface State {}

    /** This GNSS measurement's tracking state is invalid or unknown. */
    public static final int STATE_UNKNOWN = 0;
    /** This GNSS measurement's tracking state has code lock. */
    public static final int STATE_CODE_LOCK = (1<<0);
    /** This GNSS measurement's tracking state has bit sync. */
    public static final int STATE_BIT_SYNC = (1<<1);
    /** This GNSS measurement's tracking state has sub-frame sync. */
    public static final int STATE_SUBFRAME_SYNC = (1<<2);
    /** This GNSS measurement's tracking state has time-of-week decoded. */
    public static final int STATE_TOW_DECODED = (1<<3);
    /** This GNSS measurement's tracking state contains millisecond ambiguity. */
    public static final int STATE_MSEC_AMBIGUOUS = (1<<4);
    /** This GNSS measurement's tracking state has symbol sync. */
    public static final int STATE_SYMBOL_SYNC = (1<<5);
    /** This Glonass measurement's tracking state has string sync. */
    public static final int STATE_GLO_STRING_SYNC = (1<<6);
    /** This Glonass measurement's tracking state has time-of-day decoded. */
    public static final int STATE_GLO_TOD_DECODED = (1<<7);
    /** This Beidou measurement's tracking state has D2 bit sync. */
    public static final int STATE_BDS_D2_BIT_SYNC = (1<<8);
    /** This Beidou measurement's tracking state has D2 sub-frame sync. */
    public static final int STATE_BDS_D2_SUBFRAME_SYNC = (1<<9);
    /** This Galileo measurement's tracking state has E1B/C code lock. */
    public static final int STATE_GAL_E1BC_CODE_LOCK = (1<<10);
    /** This Galileo measurement's tracking state has E1C secondary code lock. */
    public static final int STATE_GAL_E1C_2ND_CODE_LOCK = (1<<11);
    /** This Galileo measurement's tracking state has E1B page sync. */
    public static final int STATE_GAL_E1B_PAGE_SYNC = (1<<12);
    /** This SBAS measurement's tracking state has whole second level sync. */
    public static final int STATE_SBAS_SYNC = (1<<13);
    /**
     * This GNSS measurement's tracking state has time-of-week known, possibly not decoded
     * over the air but has been determined from other sources. If TOW decoded is set then TOW Known
     * will also be set.
     */
    public static final int STATE_TOW_KNOWN = (1<<14);
    /**
     * This Glonass measurement's tracking state has time-of-day known, possibly not decoded
     * over the air but has been determined from other sources. If TOD decoded is set then TOD Known
     * will also be set.
     */
    public static final int STATE_GLO_TOD_KNOWN = (1<<15);

    /** This GNSS measurement's tracking state has secondary code lock. */
    public static final int STATE_2ND_CODE_LOCK  = (1 << 16);

    /**
     * All the GNSS receiver state flags, for bit masking purposes (not a sensible state for any
     * individual measurement.)
     */
    private static final int STATE_ALL = 0x3fff;  // 2 bits + 4 bits + 4 bits + 4 bits = 14 bits

    /**
     * GNSS measurement accumulated delta range state
     * @hide
     */
    @IntDef(flag = true, prefix = { "ADR_STATE_" }, value = {
            ADR_STATE_VALID, ADR_STATE_RESET, ADR_STATE_CYCLE_SLIP, ADR_STATE_HALF_CYCLE_RESOLVED,
            ADR_STATE_HALF_CYCLE_REPORTED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface AdrState {}

    /**
     * The state of the value {@link #getAccumulatedDeltaRangeMeters()} is invalid or unknown.
     */
    public static final int ADR_STATE_UNKNOWN = 0;

    /**
     * The state of the {@link #getAccumulatedDeltaRangeMeters()} is valid.
     */
    public static final int ADR_STATE_VALID = (1<<0);

    /**
     * The state of the {@link #getAccumulatedDeltaRangeMeters()} has detected a reset.
     */
    public static final int ADR_STATE_RESET = (1<<1);

    /**
     * The state of the {@link #getAccumulatedDeltaRangeMeters()} has a cycle slip detected.
     */
    public static final int ADR_STATE_CYCLE_SLIP = (1<<2);

    /**
     * Reports whether the value {@link #getAccumulatedDeltaRangeMeters()} has resolved the half
     * cycle ambiguity.
     *
     * <p> When this bit is set, the {@link #getAccumulatedDeltaRangeMeters()} corresponds to the
     * carrier phase measurement plus an accumulated integer number of carrier full cycles.
     *
     * <p> When this bit is unset, the {@link #getAccumulatedDeltaRangeMeters()} corresponds to the
     * carrier phase measurement plus an accumulated integer number of carrier half cycles.
     */
    public static final int ADR_STATE_HALF_CYCLE_RESOLVED = (1<<3);

    /**
     * Reports whether the flag {@link #ADR_STATE_HALF_CYCLE_RESOLVED} has been reported by the
     * GNSS hardware.
     *
     * <p> When this bit is set, the value of {@link #getAccumulatedDeltaRangeUncertaintyMeters()}
     * can be low (centimeter level) whether or not the half cycle ambiguity is resolved.
     *
     * <p> When this bit is unset, the value of {@link #getAccumulatedDeltaRangeUncertaintyMeters()}
     * is larger, to cover the potential error due to half cycle ambiguity being unresolved.
     */
    public static final int ADR_STATE_HALF_CYCLE_REPORTED = (1<<4);

    /**
     * All the 'Accumulated Delta Range' flags.
     * @hide
     */
    @TestApi
    public static final int ADR_STATE_ALL =
            ADR_STATE_VALID | ADR_STATE_RESET | ADR_STATE_CYCLE_SLIP |
            ADR_STATE_HALF_CYCLE_RESOLVED | ADR_STATE_HALF_CYCLE_REPORTED;

    // End enumerations in sync with gps.h

    /**
     * @hide
     */
    @TestApi
    public GnssMeasurement() {
        initialize();
    }

    /**
     * Sets all contents to the values stored in the provided object.
     * @hide
     */
    @TestApi
    public void set(GnssMeasurement measurement) {
        mFlags = measurement.mFlags;
        mSvid = measurement.mSvid;
        mConstellationType = measurement.mConstellationType;
        mTimeOffsetNanos = measurement.mTimeOffsetNanos;
        mState = measurement.mState;
        mReceivedSvTimeNanos = measurement.mReceivedSvTimeNanos;
        mReceivedSvTimeUncertaintyNanos = measurement.mReceivedSvTimeUncertaintyNanos;
        mCn0DbHz = measurement.mCn0DbHz;
        mPseudorangeRateMetersPerSecond = measurement.mPseudorangeRateMetersPerSecond;
        mPseudorangeRateUncertaintyMetersPerSecond =
                measurement.mPseudorangeRateUncertaintyMetersPerSecond;
        mAccumulatedDeltaRangeState = measurement.mAccumulatedDeltaRangeState;
        mAccumulatedDeltaRangeMeters = measurement.mAccumulatedDeltaRangeMeters;
        mAccumulatedDeltaRangeUncertaintyMeters =
                measurement.mAccumulatedDeltaRangeUncertaintyMeters;
        mCarrierFrequencyHz = measurement.mCarrierFrequencyHz;
        mCarrierCycles = measurement.mCarrierCycles;
        mCarrierPhase = measurement.mCarrierPhase;
        mCarrierPhaseUncertainty = measurement.mCarrierPhaseUncertainty;
        mMultipathIndicator = measurement.mMultipathIndicator;
        mSnrInDb = measurement.mSnrInDb;
        mAutomaticGainControlLevelInDb = measurement.mAutomaticGainControlLevelInDb;
        mCodeType = measurement.mCodeType;
    }

    /**
     * Resets all the contents to its original state.
     * @hide
     */
    @TestApi
    public void reset() {
        initialize();
    }

    /**
     * Gets the satellite ID.
     *
     * <p>Interpretation depends on {@link #getConstellationType()}.
     * See {@link GnssStatus#getSvid(int)}.
     */
    public int getSvid() {
        return mSvid;
    }

    /**
     * Sets the Satellite ID.
     * @hide
     */
    @TestApi
    public void setSvid(int value) {
        mSvid = value;
    }

    /**
     * Gets the constellation type.
     *
     * <p>The return value is one of those constants with {@code CONSTELLATION_} prefix in
     * {@link GnssStatus}.
     */
    @GnssStatus.ConstellationType
    public int getConstellationType() {
        return mConstellationType;
    }

    /**
     * Sets the constellation type.
     * @hide
     */
    @TestApi
    public void setConstellationType(@GnssStatus.ConstellationType int value) {
        mConstellationType = value;
    }

    /**
     * Gets the time offset at which the measurement was taken in nanoseconds.
     *
     * <p>The reference receiver's time from which this is offset is specified by
     * {@link GnssClock#getTimeNanos()}.
     *
     * <p>The sign of this value is given by the following equation:
     * <pre>
     *      measurement time = TimeNanos + TimeOffsetNanos</pre>
     *
     * <p>The value provides an individual time-stamp for the measurement, and allows sub-nanosecond
     * accuracy.
     */
    public double getTimeOffsetNanos() {
        return mTimeOffsetNanos;
    }

    /**
     * Sets the time offset at which the measurement was taken in nanoseconds.
     * @hide
     */
    @TestApi
    public void setTimeOffsetNanos(double value) {
        mTimeOffsetNanos = value;
    }

    /**
     * Gets per-satellite sync state.
     *
     * <p>It represents the current sync state for the associated satellite.
     *
     * <p>This value helps interpret {@link #getReceivedSvTimeNanos()}.
     */
    @State
    public int getState() {
        return mState;
    }

    /**
     * Sets the sync state.
     * @hide
     */
    @TestApi
    public void setState(@State int value) {
        mState = value;
    }

    /**
     * Gets a string representation of the 'sync state'.
     *
     * <p>For internal and logging use only.
     */
    private String getStateString() {
        if (mState == STATE_UNKNOWN) {
            return "Unknown";
        }

        StringBuilder builder = new StringBuilder();
        if ((mState & STATE_CODE_LOCK) != 0) {
            builder.append("CodeLock|");
        }
        if ((mState & STATE_BIT_SYNC) != 0) {
            builder.append("BitSync|");
        }
        if ((mState & STATE_SUBFRAME_SYNC) != 0) {
            builder.append("SubframeSync|");
        }
        if ((mState & STATE_TOW_DECODED) != 0) {
            builder.append("TowDecoded|");
        }
        if ((mState & STATE_TOW_KNOWN) != 0) {
          builder.append("TowKnown|");
        }
        if ((mState & STATE_MSEC_AMBIGUOUS) != 0) {
            builder.append("MsecAmbiguous|");
        }
        if ((mState & STATE_SYMBOL_SYNC) != 0) {
            builder.append("SymbolSync|");
        }
        if ((mState & STATE_GLO_STRING_SYNC) != 0) {
            builder.append("GloStringSync|");
        }
        if ((mState & STATE_GLO_TOD_DECODED) != 0) {
            builder.append("GloTodDecoded|");
        }
        if ((mState & STATE_GLO_TOD_KNOWN) != 0) {
          builder.append("GloTodKnown|");
        }
        if ((mState & STATE_BDS_D2_BIT_SYNC) != 0) {
            builder.append("BdsD2BitSync|");
        }
        if ((mState & STATE_BDS_D2_SUBFRAME_SYNC) != 0) {
            builder.append("BdsD2SubframeSync|");
        }
        if ((mState & STATE_GAL_E1BC_CODE_LOCK) != 0) {
            builder.append("GalE1bcCodeLock|");
        }
        if ((mState & STATE_GAL_E1C_2ND_CODE_LOCK) != 0) {
            builder.append("E1c2ndCodeLock|");
        }
        if ((mState & STATE_GAL_E1B_PAGE_SYNC) != 0) {
            builder.append("GalE1bPageSync|");
        }
        if ((mState & STATE_SBAS_SYNC) != 0) {
            builder.append("SbasSync|");
        }
        if ((mState & STATE_2ND_CODE_LOCK) != 0) {
            builder.append("2ndCodeLock|");
        }

        int remainingStates = mState & ~STATE_ALL;
        if (remainingStates > 0) {
            builder.append("Other(");
            builder.append(Integer.toBinaryString(remainingStates));
            builder.append(")|");
        }
        builder.setLength(builder.length() - 1);
        return builder.toString();
    }

    /**
     * Gets the received GNSS satellite time, at the measurement time, in nanoseconds.
     *
     * <p>The received satellite time is relative to the beginning of the system week for all
     * constellations except for Glonass where it is relative to the beginning of the Glonass
     * system day.
     *
     * <p>The table below indicates the valid range of the received GNSS satellite time. These
     * ranges depend on the constellation and code being tracked and the state of the tracking
     * algorithms given by the {@link #getState} method. The minimum value of this field is zero.
     * The maximum value of this field is determined by looking across all of the state flags
     * that are set, for the given constellation and code type, and finding the the maximum value
     * in this table.
     *
     * <p>For example, for GPS L1 C/A, if STATE_TOW_KNOWN is set, this field can be any value from 0
     * to 1 week (in nanoseconds), and for GAL E1B code, if only STATE_GAL_E1BC_CODE_LOCK is set,
     * then this field can be any value from 0 to 4 milliseconds (in nanoseconds.)
     *
     * <table border="1">
     *   <thead>
     *     <tr>
     *       <td />
     *       <td colspan="3"><strong>GPS/QZSS</strong></td>
     *       <td><strong>GLNS</strong></td>
     *       <td colspan="2"><strong>BDS</strong></td>
     *       <td colspan="3"><strong>GAL</strong></td>
     *       <td><strong>SBAS</strong></td>
     *     </tr>
     *     <tr>
     *       <td><strong>State Flag</strong></td>
     *       <td><strong>L1 C/A</strong></td>
     *       <td><strong>L5I</strong></td>
     *       <td><strong>L5Q</strong></td>
     *       <td><strong>L1OF</strong></td>
     *       <td><strong>B1I (D1)</strong></td>
     *       <td><strong>B1I &nbsp;(D2)</strong></td>
     *       <td><strong>E1B</strong></td>
     *       <td><strong>E1C</strong></td>
     *       <td><strong>E5AQ</strong></td>
     *       <td><strong>L1 C/A</strong></td>
     *     </tr>
     *   </thead>
     *   <tbody>
     *     <tr>
     *       <td>
     *         <strong>STATE_UNKNOWN</strong>
     *       </td>
     *       <td>0</td>
     *       <td>0</td>
     *       <td>0</td>
     *       <td>0</td>
     *       <td>0</td>
     *       <td>0</td>
     *       <td>0</td>
     *       <td>0</td>
     *       <td>0</td>
     *       <td>0</td>
     *     </tr>
     *     <tr>
     *       <td>
     *         <strong>STATE_CODE_LOCK</strong>
     *       </td>
     *       <td>1 ms</td>
     *       <td>1 ms</td>
     *       <td>1 ms</td>
     *       <td>1 ms</td>
     *       <td>1 ms</td>
     *       <td>1 ms</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>1 ms</td>
     *       <td>1 ms</td>
     *     </tr>
     *     <tr>
     *       <td>
     *         <strong>STATE_SYMBOL_SYNC</strong>
     *       </td>
     *       <td>20 ms (optional)</td>
     *       <td>10 ms</td>
     *       <td>1 ms (optional)</td>
     *       <td>10 ms</td>
     *       <td>20 ms (optional)</td>
     *       <td>2 ms</td>
     *       <td>4 ms (optional)</td>
     *       <td>4 ms (optional)</td>
     *       <td>1 ms (optional)</td>
     *       <td>2 ms</td>
     *     </tr>
     *     <tr>
     *       <td>
     *         <strong>STATE_BIT_SYNC</strong>
     *       </td>
     *       <td>20 ms</td>
     *       <td>20 ms</td>
     *       <td>1 ms (optional)</td>
     *       <td>20 ms</td>
     *       <td>20 ms</td>
     *       <td>-</td>
     *       <td>8 ms</td>
     *       <td>-</td>
     *       <td>1 ms (optional)</td>
     *       <td>4 ms</td>
     *     </tr>
     *     <tr>
     *       <td>
     *         <strong>STATE_SUBFRAME_SYNC</strong>
     *       </td>
     *       <td>6s</td>
     *       <td>6s</td>
     *       <td>-</td>
     *       <td>2 s</td>
     *       <td>6 s</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>100 ms</td>
     *       <td>-</td>
     *     </tr>
     *     <tr>
     *       <td>
     *         <strong>STATE_TOW_DECODED</strong>
     *       </td>
     *       <td colspan="2">1 week</td>
     *       <td>-</td>
     *       <td>1 day</td>
     *       <td colspan="2">1 week</td>
     *       <td colspan="2">1 week</td>
     *       <td>-</td>
     *       <td>1 week</td>
     *     </tr>
     *     <tr>
     *       <td>
     *         <strong>STATE_TOW_KNOWN</strong>
     *       </td>
     *       <td colspan="3">1 week</td>
     *       <td>1 day</td>
     *       <td colspan="2">1 week</td>
     *       <td colspan="3">1 week</td>
     *       <td>1 week</td>
     *     </tr>
     *     <tr>
     *       <td>
     *         <strong>STATE_GLO_STRING_SYNC</strong>
     *       </td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>2 s</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *     </tr>
     *     <tr>
     *       <td>
     *         <strong>STATE_GLO_TOD_DECODED</strong>
     *       </td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>1 day</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *     </tr>
     *     <tr>
     *       <td>
     *         <strong>STATE_GLO_TOD_KNOWN</strong>
     *       </td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>1 day</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *     </tr>
     *     <tr>
     *       <td>
     *         <strong>STATE_BDS_D2_BIT_SYNC</strong>
     *       </td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>2 ms</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *     </tr>
     *     <tr>
     *       <td>
     *         <strong>STATE_BDS_D2_SUBFRAME_SYNC</strong>
     *       </td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>600 ms</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *     </tr>
     *     <tr>
     *       <td>
     *         <strong>STATE_GAL_E1BC_CODE_LOCK</strong>
     *       </td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>4 ms</td>
     *       <td>4 ms</td>
     *       <td>-</td>
     *       <td>-</td>
     *     </tr>
     *     <tr>
     *       <td>
     *         <strong>STATE_GAL_E1C_2ND_CODE_LOCK</strong>
     *       </td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>100 ms</td>
     *       <td>-</td>
     *       <td>-</td>
     *     </tr>
     *     <tr>
     *       <td>
     *         <strong>STATE_2ND_CODE_LOCK</strong>
     *       </td>
     *       <td>-</td>
     *       <td>10 ms (optional)</td>
     *       <td>20 ms</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>100 ms (optional)</td>
     *       <td>100 ms</td>
     *       <td>-</td>
     *     </tr>
     *     <tr>
     *       <td>
     *         <strong>STATE_GAL_E1B_PAGE_SYNC</strong>
     *       </td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>2 s</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *     </tr>
     *     <tr>
     *       <td>
     *         <strong>STATE_SBAS_SYNC</strong>
     *       </td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>-</td>
     *       <td>1 s</td>
     *     </tr>
     *   </tbody>
     * </table>
     *
     * <p>Note: TOW Known refers to the case where TOW is possibly not decoded over the air but has
     * been determined from other sources. If TOW decoded is set then TOW Known must also be set.
     *
     * <p>Note well: if there is any ambiguity in integer millisecond, STATE_MSEC_AMBIGUOUS must be
     * set accordingly, in the 'state' field. This value must be populated, unless the 'state' ==
     * STATE_UNKNOWN.
     *
     * <p>Note on optional flags:
     * <ul>
     *     <li> For L1 C/A and B1I, STATE_SYMBOL_SYNC is optional since the symbol length is the
     *     same as the bit length.
     *     <li> For L5Q and E5aQ, STATE_BIT_SYNC and STATE_SYMBOL_SYNC are optional since they are
     *     implied by STATE_CODE_LOCK.
     *     <li> STATE_2ND_CODE_LOCK for L5I is optional since it is implied by STATE_SYMBOL_SYNC.
     *     <li> STATE_2ND_CODE_LOCK for E1C is optional since it is implied by
     *     STATE_GAL_E1C_2ND_CODE_LOCK.
     *     <li> For E1B and E1C, STATE_SYMBOL_SYNC is optional, because it is implied by
     *     STATE_GAL_E1BC_CODE_LOCK.
     * </ul>
     */
    public long getReceivedSvTimeNanos() {
        return mReceivedSvTimeNanos;
    }

    /**
     * Sets the received GNSS time in nanoseconds.
     * @hide
     */
    @TestApi
    public void setReceivedSvTimeNanos(long value) {
        mReceivedSvTimeNanos = value;
    }

    /**
     * Gets the error estimate (1-sigma) for the received GNSS time, in nanoseconds.
     */
    public long getReceivedSvTimeUncertaintyNanos() {
        return mReceivedSvTimeUncertaintyNanos;
    }

    /**
     * Sets the received GNSS time uncertainty (1-Sigma) in nanoseconds.
     * @hide
     */
    @TestApi
    public void setReceivedSvTimeUncertaintyNanos(long value) {
        mReceivedSvTimeUncertaintyNanos = value;
    }

    /**
     * Gets the Carrier-to-noise density in dB-Hz.
     *
     * <p>Typical range: 10-50 db-Hz.
     *
     * <p>The value contains the measured C/N0 for the signal at the antenna input.
     */
    public double getCn0DbHz() {
        return mCn0DbHz;
    }

    /**
     * Sets the carrier-to-noise density in dB-Hz.
     * @hide
     */
    @TestApi
    public void setCn0DbHz(double value) {
        mCn0DbHz = value;
    }

    /**
     * Gets the Pseudorange rate at the timestamp in m/s.
     *
     * <p>The error estimate for this value is
     * {@link #getPseudorangeRateUncertaintyMetersPerSecond()}.
     *
     * <p>The value is uncorrected, i.e. corrections for receiver and satellite clock frequency
     * errors are not included.
     *
     * <p>A positive 'uncorrected' value indicates that the SV is moving away from the receiver. The
     * sign of the 'uncorrected' 'pseudorange rate' and its relation to the sign of 'doppler shift'
     * is given by the equation:
     *
     * <pre>
     *      pseudorange rate = -k * doppler shift   (where k is a constant)</pre>
     */
    public double getPseudorangeRateMetersPerSecond() {
        return mPseudorangeRateMetersPerSecond;
    }

    /**
     * Sets the pseudorange rate at the timestamp in m/s.
     * @hide
     */
    @TestApi
    public void setPseudorangeRateMetersPerSecond(double value) {
        mPseudorangeRateMetersPerSecond = value;
    }

    /**
     * Gets the pseudorange's rate uncertainty (1-Sigma) in m/s.
     *
     * <p>The uncertainty is represented as an absolute (single sided) value.
     */
    public double getPseudorangeRateUncertaintyMetersPerSecond() {
        return mPseudorangeRateUncertaintyMetersPerSecond;
    }

    /**
     * Sets the pseudorange's rate uncertainty (1-Sigma) in m/s.
     * @hide
     */
    @TestApi
    public void setPseudorangeRateUncertaintyMetersPerSecond(double value) {
        mPseudorangeRateUncertaintyMetersPerSecond = value;
    }

    /**
     * Gets 'Accumulated Delta Range' state.
     *
     * <p>It indicates whether {@link #getAccumulatedDeltaRangeMeters()} is reset or there is a
     * cycle slip (indicating 'loss of lock').
     */
    @AdrState
    public int getAccumulatedDeltaRangeState() {
        return mAccumulatedDeltaRangeState;
    }

    /**
     * Sets the 'Accumulated Delta Range' state.
     * @hide
     */
    @TestApi
    public void setAccumulatedDeltaRangeState(@AdrState int value) {
        mAccumulatedDeltaRangeState = value;
    }

    /**
     * Gets a string representation of the 'Accumulated Delta Range state'.
     *
     * <p>For internal and logging use only.
     */
    private String getAccumulatedDeltaRangeStateString() {
        if (mAccumulatedDeltaRangeState == ADR_STATE_UNKNOWN) {
            return "Unknown";
        }
        StringBuilder builder = new StringBuilder();
        if ((mAccumulatedDeltaRangeState & ADR_STATE_VALID) == ADR_STATE_VALID) {
            builder.append("Valid|");
        }
        if ((mAccumulatedDeltaRangeState & ADR_STATE_RESET) == ADR_STATE_RESET) {
            builder.append("Reset|");
        }
        if ((mAccumulatedDeltaRangeState & ADR_STATE_CYCLE_SLIP) == ADR_STATE_CYCLE_SLIP) {
            builder.append("CycleSlip|");
        }
        if ((mAccumulatedDeltaRangeState & ADR_STATE_HALF_CYCLE_RESOLVED) ==
                ADR_STATE_HALF_CYCLE_RESOLVED) {
            builder.append("HalfCycleResolved|");
        }
        if ((mAccumulatedDeltaRangeState & ADR_STATE_HALF_CYCLE_REPORTED)
                == ADR_STATE_HALF_CYCLE_REPORTED) {
            builder.append("HalfCycleReported|");
        }
        int remainingStates = mAccumulatedDeltaRangeState & ~ADR_STATE_ALL;
        if (remainingStates > 0) {
            builder.append("Other(");
            builder.append(Integer.toBinaryString(remainingStates));
            builder.append(")|");
        }
        builder.deleteCharAt(builder.length() - 1);
        return builder.toString();
    }

    /**
     * Gets the accumulated delta range since the last channel reset, in meters.
     *
     * <p>The error estimate for this value is {@link #getAccumulatedDeltaRangeUncertaintyMeters()}.
     *
     * <p>The availability of the value is represented by {@link #getAccumulatedDeltaRangeState()}.
     *
     * <p>A positive value indicates that the SV is moving away from the receiver.
     * The sign of {@link #getAccumulatedDeltaRangeMeters()} and its relation to the sign of
     * {@link #getCarrierPhase()} is given by the equation:
     *
     * <pre>
     *          accumulated delta range = -k * carrier phase    (where k is a constant)</pre>
     *
     * <p>Similar to the concept of an RTCM "Phaserange", when the accumulated delta range is
     * initially chosen, and whenever it is reset, it will retain the integer nature
     * of the relative carrier phase offset between satellites observed by this receiver, such that
     * the double difference of this value between receivers and satellites may be used, together
     * with integer ambiguity resolution, to determine highly precise relative location between
     * receivers.
     *
     * <p>This includes ensuring that all half-cycle ambiguities are resolved before this value is
     * reported as {@link #ADR_STATE_VALID}.
     *
     * <p>The alignment of the phase measurement will not be adjusted by the receiver so the
     * in-phase and quadrature phase components will have a quarter cycle offset as they do when
     * transmitted from the satellites. If the measurement is from a combination of the in-phase
     * and quadrature phase components, then the alignment of the phase measurement will be aligned
     * to the in-phase component.
     */
    public double getAccumulatedDeltaRangeMeters() {
        return mAccumulatedDeltaRangeMeters;
    }

    /**
     * Sets the accumulated delta range in meters.
     * @hide
     */
    @TestApi
    public void setAccumulatedDeltaRangeMeters(double value) {
        mAccumulatedDeltaRangeMeters = value;
    }

    /**
     * Gets the accumulated delta range's uncertainty (1-Sigma) in meters.
     *
     * <p>The uncertainty is represented as an absolute (single sided) value.
     *
     * <p>The status of the value is represented by {@link #getAccumulatedDeltaRangeState()}.
     */
    public double getAccumulatedDeltaRangeUncertaintyMeters() {
        return mAccumulatedDeltaRangeUncertaintyMeters;
    }

    /**
     * Sets the accumulated delta range's uncertainty (1-sigma) in meters.
     *
     * <p>The status of the value is represented by {@link #getAccumulatedDeltaRangeState()}.
     *
     * @hide
     */
    @TestApi
    public void setAccumulatedDeltaRangeUncertaintyMeters(double value) {
        mAccumulatedDeltaRangeUncertaintyMeters = value;
    }

    /**
     * Returns {@code true} if {@link #getCarrierFrequencyHz()} is available, {@code false}
     * otherwise.
     */
    public boolean hasCarrierFrequencyHz() {
        return isFlagSet(HAS_CARRIER_FREQUENCY);
    }

    /**
     * Gets the carrier frequency of the tracked signal.
     *
     * <p>For example it can be the GPS central frequency for L1 = 1575.45 MHz, or L2 = 1227.60 MHz,
     * L5 = 1176.45 MHz, varying GLO channels, etc. If the field is not set, it is the primary
     * common use central frequency, e.g. L1 = 1575.45 MHz for GPS.
     *
     * <p> For an L1, L5 receiver tracking a satellite on L1 and L5 at the same time, two raw
     * measurement objects will be reported for this same satellite, in one of the measurement
     * objects, all the values related to L1 will be filled, and in the other all of the values
     * related to L5 will be filled.
     *
     * <p>The value is only available if {@link #hasCarrierFrequencyHz()} is {@code true}.
     *
     * @return the carrier frequency of the signal tracked in Hz.
     */
    public float getCarrierFrequencyHz() {
        return mCarrierFrequencyHz;
    }

    /**
     * Sets the Carrier frequency in Hz.
     * @hide
     */
    @TestApi
    public void setCarrierFrequencyHz(float carrierFrequencyHz) {
        setFlag(HAS_CARRIER_FREQUENCY);
        mCarrierFrequencyHz = carrierFrequencyHz;
    }

    /**
     * Resets the Carrier frequency in Hz.
     * @hide
     */
    @TestApi
    public void resetCarrierFrequencyHz() {
        resetFlag(HAS_CARRIER_FREQUENCY);
        mCarrierFrequencyHz = Float.NaN;
    }

    /**
     * Returns {@code true} if {@link #getCarrierCycles()} is available, {@code false} otherwise.
     * 
     * @deprecated use {@link #getAccumulatedDeltaRangeState()} instead.
     */
    @Deprecated
    public boolean hasCarrierCycles() {
        return isFlagSet(HAS_CARRIER_CYCLES);
    }

    /**
     * The number of full carrier cycles between the satellite and the receiver.
     *
     * <p>The reference frequency is given by the value of {@link #getCarrierFrequencyHz()}.
     *
     * <p>The value is only available if {@link #hasCarrierCycles()} is {@code true}.
     *
     * @deprecated use {@link #getAccumulatedDeltaRangeMeters()} instead.
     */
    @Deprecated
    public long getCarrierCycles() {
        return mCarrierCycles;
    }

    /**
     * Sets the number of full carrier cycles between the satellite and the receiver.
     *
     * @deprecated use {@link #setAccumulatedDeltaRangeMeters(double)}
     * and {@link #setAccumulatedDeltaRangeState(int)} instead.
     * 
     * @hide
     */
    @TestApi
    @Deprecated
    public void setCarrierCycles(long value) {
        setFlag(HAS_CARRIER_CYCLES);
        mCarrierCycles = value;
    }

    /**
     * Resets the number of full carrier cycles between the satellite and the receiver.
     * 
     * @deprecated use {@link #setAccumulatedDeltaRangeMeters(double)}
     * and {@link #setAccumulatedDeltaRangeState(int)} instead.
     * @hide
     */
    @TestApi
    @Deprecated
    public void resetCarrierCycles() {
        resetFlag(HAS_CARRIER_CYCLES);
        mCarrierCycles = Long.MIN_VALUE;
    }

    /**
     * Returns {@code true} if {@link #getCarrierPhase()} is available, {@code false} otherwise.
     * 
     * @deprecated use {@link #getAccumulatedDeltaRangeState()} instead.
     */
    @Deprecated
    public boolean hasCarrierPhase() {
        return isFlagSet(HAS_CARRIER_PHASE);
    }

    /**
     * Gets the RF phase detected by the receiver.
     *
     * <p>Range: [0.0, 1.0].
     *
     * <p>This is the fractional part of the complete carrier phase measurement.
     *
     * <p>The reference frequency is given by the value of {@link #getCarrierFrequencyHz()}.
     *
     * <p>The error estimate for this value is {@link #getCarrierPhaseUncertainty()}.
     *
     * <p>The value is only available if {@link #hasCarrierPhase()} is {@code true}.
     *
     * @deprecated use {@link #getAccumulatedDeltaRangeMeters()} instead.
     */
    @Deprecated
    public double getCarrierPhase() {
        return mCarrierPhase;
    }

    /**
     * Sets the RF phase detected by the receiver.
     * 
     * @deprecated use {@link #setAccumulatedDeltaRangeMeters(double)}
     * and {@link #setAccumulatedDeltaRangeState(int)} instead.
     * 
     * @hide
     */
    @TestApi
    @Deprecated
    public void setCarrierPhase(double value) {
        setFlag(HAS_CARRIER_PHASE);
        mCarrierPhase = value;
    }

    /**
     * Resets the RF phase detected by the receiver.
     * 
     * @deprecated use {@link #setAccumulatedDeltaRangeMeters(double)}
     * and {@link #setAccumulatedDeltaRangeState(int)} instead.
     * 
     * @hide
     */
    @TestApi
    @Deprecated
    public void resetCarrierPhase() {
        resetFlag(HAS_CARRIER_PHASE);
        mCarrierPhase = Double.NaN;
    }

    /**
     * Returns {@code true} if {@link #getCarrierPhaseUncertainty()} is available, {@code false}
     * otherwise.
     * 
     * @deprecated use {@link #getAccumulatedDeltaRangeState()} instead.
     */
    @Deprecated
    public boolean hasCarrierPhaseUncertainty() {
        return isFlagSet(HAS_CARRIER_PHASE_UNCERTAINTY);
    }

    /**
     * Gets the carrier-phase's uncertainty (1-Sigma).
     *
     * <p>The uncertainty is represented as an absolute (single sided) value.
     *
     * <p>The value is only available if {@link #hasCarrierPhaseUncertainty()} is {@code true}.
     *
     * @deprecated use {@link #getAccumulatedDeltaRangeUncertaintyMeters()} instead.
     */
    @Deprecated
    public double getCarrierPhaseUncertainty() {
        return mCarrierPhaseUncertainty;
    }

    /**
     * Sets the Carrier-phase's uncertainty (1-Sigma) in cycles.
     * 
     * @deprecated use {@link #setAccumulatedDeltaRangeUncertaintyMeters(double)}
     * and {@link #setAccumulatedDeltaRangeState(int)} instead.
     * 
     * @hide
     */
    @TestApi
    @Deprecated
    public void setCarrierPhaseUncertainty(double value) {
        setFlag(HAS_CARRIER_PHASE_UNCERTAINTY);
        mCarrierPhaseUncertainty = value;
    }

    /**
     * Resets the Carrier-phase's uncertainty (1-Sigma) in cycles.
     * 
     * @deprecated use {@link #setAccumulatedDeltaRangeUncertaintyMeters(double)}
     * and {@link #setAccumulatedDeltaRangeState(int)} instead.
     * 
     * @hide
     */
    @TestApi
    @Deprecated
    public void resetCarrierPhaseUncertainty() {
        resetFlag(HAS_CARRIER_PHASE_UNCERTAINTY);
        mCarrierPhaseUncertainty = Double.NaN;
    }

    /**
     * Gets a value indicating the 'multipath' state of the event.
     */
    @MultipathIndicator
    public int getMultipathIndicator() {
        return mMultipathIndicator;
    }

    /**
     * Sets the 'multi-path' indicator.
     * @hide
     */
    @TestApi
    public void setMultipathIndicator(@MultipathIndicator int value) {
        mMultipathIndicator = value;
    }

    /**
     * Gets a string representation of the 'multi-path indicator'.
     *
     * <p>For internal and logging use only.
     */
    private String getMultipathIndicatorString() {
        switch (mMultipathIndicator) {
            case MULTIPATH_INDICATOR_UNKNOWN:
                return "Unknown";
            case MULTIPATH_INDICATOR_DETECTED:
                return "Detected";
            case MULTIPATH_INDICATOR_NOT_DETECTED:
                return "NotDetected";
            default:
                return "<Invalid: " + mMultipathIndicator + ">";
        }
    }

    /**
     * Returns {@code true} if {@link #getSnrInDb()} is available, {@code false} otherwise.
     */
    public boolean hasSnrInDb() {
        return isFlagSet(HAS_SNR);
    }

    /**
     * Gets the (post-correlation & integration) Signal-to-Noise ratio (SNR) in dB.
     *
     * <p>The value is only available if {@link #hasSnrInDb()} is {@code true}.
     */
    public double getSnrInDb() {
        return mSnrInDb;
    }

    /**
     * Sets the Signal-to-noise ratio (SNR) in dB.
     * @hide
     */
    @TestApi
    public void setSnrInDb(double snrInDb) {
        setFlag(HAS_SNR);
        mSnrInDb = snrInDb;
    }

    /**
     * Resets the Signal-to-noise ratio (SNR) in dB.
     * @hide
     */
    @TestApi
    public void resetSnrInDb() {
        resetFlag(HAS_SNR);
        mSnrInDb = Double.NaN;
    }

    /**
     * Returns {@code true} if {@link #getAutomaticGainControlLevelDb()} is available,
     * {@code false} otherwise.
     */
    public boolean hasAutomaticGainControlLevelDb() {
        return isFlagSet(HAS_AUTOMATIC_GAIN_CONTROL);
    }

    /**
     * Gets the Automatic Gain Control level in dB.
     *
     * <p> AGC acts as a variable gain amplifier adjusting the power of the incoming signal. The AGC
     * level may be used to indicate potential interference. When AGC is at a nominal level, this
     * value must be set as 0. Higher gain (and/or lower input power) shall be output as a positive
     * number. Hence in cases of strong jamming, in the band of this signal, this value will go more
     * negative.
     *
     * <p> Note: Different hardware designs (e.g. antenna, pre-amplification, or other RF HW
     * components) may also affect the typical output of of this value on any given hardware design
     * in an open sky test - the important aspect of this output is that changes in this value are
     * indicative of changes on input signal power in the frequency band for this measurement.
     *
     * <p> The value is only available if {@link #hasAutomaticGainControlLevelDb()} is {@code true}
     */
    public double getAutomaticGainControlLevelDb() {
        return mAutomaticGainControlLevelInDb;
    }

    /**
     * Sets the Automatic Gain Control level in dB.
     * @hide
     */
    @TestApi
    public void setAutomaticGainControlLevelInDb(double agcLevelDb) {
        setFlag(HAS_AUTOMATIC_GAIN_CONTROL);
        mAutomaticGainControlLevelInDb = agcLevelDb;
    }

    /**
     * Resets the Automatic Gain Control level.
     * @hide
     */
    @TestApi
    public void resetAutomaticGainControlLevel() {
        resetFlag(HAS_AUTOMATIC_GAIN_CONTROL);
        mAutomaticGainControlLevelInDb = Double.NaN;
    }

    /**
     * Returns {@code true} if {@link #getCodeType()} is available,
     * {@code false} otherwise.
     */
    public boolean hasCodeType() {
        return isFlagSet(HAS_CODE_TYPE);
    }

    /**
     * Gets the GNSS measurement's code type.
     *
     * <p>Similar to the Attribute field described in RINEX 3.03, e.g., in Tables 4-10, and Table
     * A2 at the RINEX 3.03 Update 1 Document.
     *
     * <p>Returns "A" for GALILEO E1A, GALILEO E6A, IRNSS L5A, IRNSS SA.
     *
     * <p>Returns "B" for GALILEO E1B, GALILEO E6B, IRNSS L5B, IRNSS SB.
     *
     * <p>Returns "C" for GPS L1 C/A,  GPS L2 C/A, GLONASS G1 C/A, GLONASS G2 C/A, GALILEO E1C,
     * GALILEO E6C, SBAS L1 C/A, QZSS L1 C/A, IRNSS L5C.
     *
     * <p>Returns "I" for GPS L5 I, GLONASS G3 I, GALILEO E5a I, GALILEO E5b I, GALILEO E5a+b I,
     * SBAS L5 I, QZSS L5 I, BDS B1 I, BDS B2 I, BDS B3 I.
     *
     * <p>Returns "L" for GPS L1C (P), GPS L2C (L), QZSS L1C (P), QZSS L2C (L), LEX(6) L.
     *
     * <p>Returns "M" for GPS L1M, GPS L2M.
     *
     * <p>Returns "N" for GPS L1 codeless, GPS L2 codeless.
     *
     * <p>Returns "P" for GPS L1P, GPS L2P, GLONASS G1P, GLONASS G2P.
     *
     * <p>Returns "Q" for GPS L5 Q, GLONASS G3 Q, GALILEO E5a Q, GALILEO E5b Q, GALILEO E5a+b Q,
     * SBAS L5 Q, QZSS L5 Q, BDS B1 Q, BDS B2 Q, BDS B3 Q.
     *
     * <p>Returns "S" for GPS L1C (D), GPS L2C (M), QZSS L1C (D), QZSS L2C (M), LEX(6) S.
     *
     * <p>Returns "W" for GPS L1 Z-tracking, GPS L2 Z-tracking.
     *
     * <p>Returns "X" for GPS L1C (D+P), GPS L2C (M+L), GPS L5 (I+Q), GLONASS G3 (I+Q), GALILEO
     * E1 (B+C), GALILEO E5a (I+Q), GALILEO E5b (I+Q), GALILEO E5a+b(I+Q), GALILEO E6 (B+C), SBAS
     * L5 (I+Q), QZSS L1C (D+P), QZSS L2C (M+L), QZSS L5 (I+Q), LEX(6) (S+L), BDS B1 (I+Q), BDS
     * B2 (I+Q), BDS B3 (I+Q), IRNSS L5 (B+C).
     *
     * <p>Returns "Y" for GPS L1Y, GPS L2Y.
     *
     * <p>Returns "Z" for GALILEO E1 (A+B+C), GALILEO E6 (A+B+C), QZSS L1-SAIF.
     *
     * <p>Returns "UNKNOWN" if the GNSS Measurement's code type is unknown.
     *
     * <p>This is used to specify the observation descriptor defined in GNSS Observation Data File
     * Header Section Description in the RINEX standard (Version 3.XX), in cases where the code type
     * does not align with the above listed values. For example, if a code type "G" is added, this
     * string shall be set to "G".
     */
    @NonNull
    public String getCodeType() {
        return mCodeType;
    }

    /**
     * Sets the GNSS measurement's code type.
     *
     * @hide
     */
    @TestApi
    public void setCodeType(@NonNull String codeType) {
        setFlag(HAS_CODE_TYPE);
        mCodeType = codeType;
    }

    /**
     * Resets the GNSS measurement's code type.
     *
     * @hide
     */
    @TestApi
    public void resetCodeType() {
        resetFlag(HAS_CODE_TYPE);
        mCodeType = "UNKNOWN";
    }

    public static final @android.annotation.NonNull Creator<GnssMeasurement> CREATOR = new Creator<GnssMeasurement>() {
        @Override
        public GnssMeasurement createFromParcel(Parcel parcel) {
            GnssMeasurement gnssMeasurement = new GnssMeasurement();

            gnssMeasurement.mFlags = parcel.readInt();
            gnssMeasurement.mSvid = parcel.readInt();
            gnssMeasurement.mConstellationType = parcel.readInt();
            gnssMeasurement.mTimeOffsetNanos = parcel.readDouble();
            gnssMeasurement.mState = parcel.readInt();
            gnssMeasurement.mReceivedSvTimeNanos = parcel.readLong();
            gnssMeasurement.mReceivedSvTimeUncertaintyNanos = parcel.readLong();
            gnssMeasurement.mCn0DbHz = parcel.readDouble();
            gnssMeasurement.mPseudorangeRateMetersPerSecond = parcel.readDouble();
            gnssMeasurement.mPseudorangeRateUncertaintyMetersPerSecond = parcel.readDouble();
            gnssMeasurement.mAccumulatedDeltaRangeState = parcel.readInt();
            gnssMeasurement.mAccumulatedDeltaRangeMeters = parcel.readDouble();
            gnssMeasurement.mAccumulatedDeltaRangeUncertaintyMeters = parcel.readDouble();
            gnssMeasurement.mCarrierFrequencyHz = parcel.readFloat();
            gnssMeasurement.mCarrierCycles = parcel.readLong();
            gnssMeasurement.mCarrierPhase = parcel.readDouble();
            gnssMeasurement.mCarrierPhaseUncertainty = parcel.readDouble();
            gnssMeasurement.mMultipathIndicator = parcel.readInt();
            gnssMeasurement.mSnrInDb = parcel.readDouble();
            gnssMeasurement.mAutomaticGainControlLevelInDb = parcel.readDouble();
            gnssMeasurement.mCodeType = parcel.readString();

            return gnssMeasurement;
        }

        @Override
        public GnssMeasurement[] newArray(int i) {
            return new GnssMeasurement[i];
        }
    };

    @Override
    public void writeToParcel(Parcel parcel, int flags) {
        parcel.writeInt(mFlags);
        parcel.writeInt(mSvid);
        parcel.writeInt(mConstellationType);
        parcel.writeDouble(mTimeOffsetNanos);
        parcel.writeInt(mState);
        parcel.writeLong(mReceivedSvTimeNanos);
        parcel.writeLong(mReceivedSvTimeUncertaintyNanos);
        parcel.writeDouble(mCn0DbHz);
        parcel.writeDouble(mPseudorangeRateMetersPerSecond);
        parcel.writeDouble(mPseudorangeRateUncertaintyMetersPerSecond);
        parcel.writeInt(mAccumulatedDeltaRangeState);
        parcel.writeDouble(mAccumulatedDeltaRangeMeters);
        parcel.writeDouble(mAccumulatedDeltaRangeUncertaintyMeters);
        parcel.writeFloat(mCarrierFrequencyHz);
        parcel.writeLong(mCarrierCycles);
        parcel.writeDouble(mCarrierPhase);
        parcel.writeDouble(mCarrierPhaseUncertainty);
        parcel.writeInt(mMultipathIndicator);
        parcel.writeDouble(mSnrInDb);
        parcel.writeDouble(mAutomaticGainControlLevelInDb);
        parcel.writeString(mCodeType);
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public String toString() {
        final String format = "   %-29s = %s\n";
        final String formatWithUncertainty = "   %-29s = %-25s   %-40s = %s\n";
        StringBuilder builder = new StringBuilder("GnssMeasurement:\n");

        builder.append(String.format(format, "Svid", mSvid));
        builder.append(String.format(format, "ConstellationType", mConstellationType));
        builder.append(String.format(format, "TimeOffsetNanos", mTimeOffsetNanos));

        builder.append(String.format(format, "State", getStateString()));

        builder.append(String.format(
                formatWithUncertainty,
                "ReceivedSvTimeNanos",
                mReceivedSvTimeNanos,
                "ReceivedSvTimeUncertaintyNanos",
                mReceivedSvTimeUncertaintyNanos));

        builder.append(String.format(format, "Cn0DbHz", mCn0DbHz));

        builder.append(String.format(
                formatWithUncertainty,
                "PseudorangeRateMetersPerSecond",
                mPseudorangeRateMetersPerSecond,
                "PseudorangeRateUncertaintyMetersPerSecond",
                mPseudorangeRateUncertaintyMetersPerSecond));

        builder.append(String.format(
                format,
                "AccumulatedDeltaRangeState",
                getAccumulatedDeltaRangeStateString()));

        builder.append(String.format(
                formatWithUncertainty,
                "AccumulatedDeltaRangeMeters",
                mAccumulatedDeltaRangeMeters,
                "AccumulatedDeltaRangeUncertaintyMeters",
                mAccumulatedDeltaRangeUncertaintyMeters));

        builder.append(String.format(
                format,
                "CarrierFrequencyHz",
                hasCarrierFrequencyHz() ? mCarrierFrequencyHz : null));

        builder.append(String.format(
                format,
                "CarrierCycles",
                hasCarrierCycles() ? mCarrierCycles : null));

        builder.append(String.format(
                formatWithUncertainty,
                "CarrierPhase",
                hasCarrierPhase() ? mCarrierPhase : null,
                "CarrierPhaseUncertainty",
                hasCarrierPhaseUncertainty() ? mCarrierPhaseUncertainty : null));

        builder.append(String.format(format, "MultipathIndicator", getMultipathIndicatorString()));

        builder.append(String.format(
                format,
                "SnrInDb",
                hasSnrInDb() ? mSnrInDb : null));
        builder.append(String.format(
                format,
                "AgcLevelDb",
                hasAutomaticGainControlLevelDb() ? mAutomaticGainControlLevelInDb : null));
        builder.append(String.format(
                format,
                "CodeType",
                hasCodeType() ? mCodeType : null));

        return builder.toString();
    }

    private void initialize() {
        mFlags = HAS_NO_FLAGS;
        setSvid(0);
        setTimeOffsetNanos(Long.MIN_VALUE);
        setState(STATE_UNKNOWN);
        setReceivedSvTimeNanos(Long.MIN_VALUE);
        setReceivedSvTimeUncertaintyNanos(Long.MAX_VALUE);
        setCn0DbHz(Double.MIN_VALUE);
        setPseudorangeRateMetersPerSecond(Double.MIN_VALUE);
        setPseudorangeRateUncertaintyMetersPerSecond(Double.MIN_VALUE);
        setAccumulatedDeltaRangeState(ADR_STATE_UNKNOWN);
        setAccumulatedDeltaRangeMeters(Double.MIN_VALUE);
        setAccumulatedDeltaRangeUncertaintyMeters(Double.MIN_VALUE);
        resetCarrierFrequencyHz();
        resetCarrierCycles();
        resetCarrierPhase();
        resetCarrierPhaseUncertainty();
        setMultipathIndicator(MULTIPATH_INDICATOR_UNKNOWN);
        resetSnrInDb();
        resetAutomaticGainControlLevel();
        resetCodeType();
    }

    private void setFlag(int flag) {
        mFlags |= flag;
    }

    private void resetFlag(int flag) {
        mFlags &= ~flag;
    }

    private boolean isFlagSet(int flag) {
        return (mFlags & flag) == flag;
    }
}
