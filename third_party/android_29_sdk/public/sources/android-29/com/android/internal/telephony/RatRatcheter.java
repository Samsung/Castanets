/*
 * Copyright (C) 2016 The Android Open Source Project
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
package com.android.internal.telephony;

import android.annotation.NonNull;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.PersistableBundle;
import android.os.UserHandle;
import android.telephony.AccessNetworkConstants;
import android.telephony.CarrierConfigManager;
import android.telephony.NetworkRegistrationInfo;
import android.telephony.Rlog;
import android.telephony.ServiceState;
import android.telephony.TelephonyManager.NetworkType;
import android.util.SparseArray;
import android.util.SparseIntArray;

import java.util.Arrays;

/**
 * This class loads configuration from CarrierConfig and uses it to determine
 * what RATs are within a ratcheting family.  For example all the HSPA/HSDPA/HSUPA RATs.
 * Then, until reset the class will only ratchet upwards within the family (order
 * determined by the CarrierConfig data).  The ServiceStateTracker will reset this
 * on cell-change.
 */
public class RatRatcheter {
    private final static String LOG_TAG = "RilRatcheter";

    /**
     * This is a map of RAT types -> RAT families for rapid lookup.
     * The RAT families are defined by RAT type -> RAT Rank SparseIntArrays, so
     * we can compare the priorities of two RAT types by comparing the values
     * stored in the SparseIntArrays, higher values are higher priority.
     */
    private final SparseArray<SparseIntArray> mRatFamilyMap = new SparseArray<>();

    private final Phone mPhone;
    private boolean mVoiceRatchetEnabled = true;
    private boolean mDataRatchetEnabled = true;

    /**
     * Updates the ServiceState with a new set of cell bandwidths IFF the new bandwidth list has a
     * higher aggregate bandwidth.
     *
     * @return Whether the bandwidths were updated.
     */
    public static boolean updateBandwidths(int[] bandwidths, ServiceState serviceState) {
        if (bandwidths == null) {
            return false;
        }

        int ssAggregateBandwidth = Arrays.stream(serviceState.getCellBandwidths()).sum();
        int newAggregateBandwidth = Arrays.stream(bandwidths).sum();

        if (newAggregateBandwidth > ssAggregateBandwidth) {
            serviceState.setCellBandwidths(bandwidths);
            return true;
        }

        return false;
    }

    /** Constructor */
    public RatRatcheter(Phone phone) {
        mPhone = phone;

        IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED);
        phone.getContext().registerReceiverAsUser(mConfigChangedReceiver, UserHandle.ALL,
                intentFilter, null, null);
        resetRatFamilyMap();
    }

    private @NetworkType int ratchetRat(@NetworkType int oldNetworkType,
                                        @NetworkType int newNetworkType) {
        int oldRat = ServiceState.networkTypeToRilRadioTechnology(oldNetworkType);
        int newRat = ServiceState.networkTypeToRilRadioTechnology(newNetworkType);
        synchronized (mRatFamilyMap) {
            final SparseIntArray oldFamily = mRatFamilyMap.get(oldRat);
            if (oldFamily == null) {
                return newNetworkType;
            }

            final SparseIntArray newFamily = mRatFamilyMap.get(newRat);
            if (newFamily != oldFamily) {
                return newNetworkType;
            }

            // now go with the higher of the two
            final int oldRatRank = newFamily.get(oldRat, -1);
            final int newRatRank = newFamily.get(newRat, -1);
            return ServiceState.rilRadioTechnologyToNetworkType(
                    oldRatRank > newRatRank ? oldRat : newRat);
        }
    }

    /** Ratchets RATs and cell bandwidths if oldSS and newSS have the same RAT family. */
    public void ratchet(@NonNull ServiceState oldSS, @NonNull ServiceState newSS,
                        boolean locationChange) {
        if (!locationChange && isSameRatFamily(oldSS, newSS)) {
            updateBandwidths(oldSS.getCellBandwidths(), newSS);
        }
        // temporarily disable rat ratchet on location change.
        if (locationChange) {
            mVoiceRatchetEnabled = false;
            mDataRatchetEnabled = false;
            return;
        }

        boolean newUsingCA = oldSS.isUsingCarrierAggregation()
                || newSS.isUsingCarrierAggregation()
                || newSS.getCellBandwidths().length > 1;
        NetworkRegistrationInfo oldCsNri = oldSS.getNetworkRegistrationInfo(
                NetworkRegistrationInfo.DOMAIN_CS, AccessNetworkConstants.TRANSPORT_TYPE_WWAN);
        NetworkRegistrationInfo newCsNri = newSS.getNetworkRegistrationInfo(
                NetworkRegistrationInfo.DOMAIN_CS, AccessNetworkConstants.TRANSPORT_TYPE_WWAN);
        if (mVoiceRatchetEnabled) {
            int newPsNetworkType = ratchetRat(oldCsNri.getAccessNetworkTechnology(),
                    newCsNri.getAccessNetworkTechnology());
            newCsNri.setAccessNetworkTechnology(newPsNetworkType);
            newSS.addNetworkRegistrationInfo(newCsNri);
        } else if (oldCsNri.getAccessNetworkTechnology() != oldCsNri.getAccessNetworkTechnology()) {
            // resume rat ratchet on following rat change within the same location
            mVoiceRatchetEnabled = true;
        }

        NetworkRegistrationInfo oldPsNri = oldSS.getNetworkRegistrationInfo(
                NetworkRegistrationInfo.DOMAIN_PS, AccessNetworkConstants.TRANSPORT_TYPE_WWAN);
        NetworkRegistrationInfo newPsNri = newSS.getNetworkRegistrationInfo(
                NetworkRegistrationInfo.DOMAIN_PS, AccessNetworkConstants.TRANSPORT_TYPE_WWAN);
        if (mDataRatchetEnabled) {
            int newPsNetworkType = ratchetRat(oldPsNri.getAccessNetworkTechnology(),
                    newPsNri.getAccessNetworkTechnology());
            newPsNri.setAccessNetworkTechnology(newPsNetworkType);
            newSS.addNetworkRegistrationInfo(newPsNri);
        } else if (oldPsNri.getAccessNetworkTechnology() != newPsNri.getAccessNetworkTechnology()) {
            // resume rat ratchet on following rat change within the same location
            mDataRatchetEnabled = true;
        }

        newSS.setIsUsingCarrierAggregation(newUsingCA);
    }

    private boolean isSameRatFamily(ServiceState ss1, ServiceState ss2) {
        synchronized (mRatFamilyMap) {
            // Either the two technologies are the same or their families must be non-null
            // and the same.
            int dataRat1 = ServiceState.networkTypeToRilRadioTechnology(
                    ss1.getNetworkRegistrationInfo(NetworkRegistrationInfo.DOMAIN_PS,
                            AccessNetworkConstants.TRANSPORT_TYPE_WWAN)
                            .getAccessNetworkTechnology());
            int dataRat2 = ServiceState.networkTypeToRilRadioTechnology(
                    ss2.getNetworkRegistrationInfo(NetworkRegistrationInfo.DOMAIN_PS,
                            AccessNetworkConstants.TRANSPORT_TYPE_WWAN)
                            .getAccessNetworkTechnology());

            if (dataRat1 == dataRat2) return true;
            if (mRatFamilyMap.get(dataRat1) == null) {
                return false;
            }
            return mRatFamilyMap.get(dataRat1) == mRatFamilyMap.get(dataRat2);
        }
    }

    private BroadcastReceiver mConfigChangedReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            final String action = intent.getAction();
            if (CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED.equals(action)) {
                resetRatFamilyMap();
            }
        }
    };

    private void resetRatFamilyMap() {
        synchronized(mRatFamilyMap) {
            mRatFamilyMap.clear();

            final CarrierConfigManager configManager = (CarrierConfigManager)
                    mPhone.getContext().getSystemService(Context.CARRIER_CONFIG_SERVICE);
            if (configManager == null) return;
            PersistableBundle b = configManager.getConfig();
            if (b == null) return;

            // Reads an array of strings, eg:
            // ["GPRS, EDGE", "EVDO, EVDO_A, EVDO_B", "HSPA, HSDPA, HSUPA, HSPAP"]
            // Each string defines a family and the order of rats within the string express
            // the priority of the RAT within the family (ie, we'd move up to later-listed RATs, but
            // not down).
            String[] ratFamilies = b.getStringArray(CarrierConfigManager.KEY_RATCHET_RAT_FAMILIES);
            if (ratFamilies == null) return;
            for (String ratFamily : ratFamilies) {
                String[] rats = ratFamily.split(",");
                if (rats.length < 2) continue;
                SparseIntArray currentFamily = new SparseIntArray(rats.length);
                int pos = 0;
                for (String ratString : rats) {
                    int ratInt;
                    try {
                        ratInt = Integer.parseInt(ratString.trim());
                    } catch (NumberFormatException e) {
                        Rlog.e(LOG_TAG, "NumberFormatException on " + ratString);
                        break;
                    }
                    if (mRatFamilyMap.get(ratInt) != null) {
                        Rlog.e(LOG_TAG, "RAT listed twice: " + ratString);
                        break;
                    }
                    currentFamily.put(ratInt, pos++);
                    mRatFamilyMap.put(ratInt, currentFamily);
                }
            }
        }
    }
}
