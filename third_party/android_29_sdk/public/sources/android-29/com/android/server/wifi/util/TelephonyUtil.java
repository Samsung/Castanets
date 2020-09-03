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

package com.android.server.wifi.util;

import android.annotation.NonNull;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiEnterpriseConfig;
import android.telephony.ImsiEncryptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.util.Base64;
import android.util.Log;
import android.util.Pair;

import com.android.internal.annotations.VisibleForTesting;
import com.android.server.wifi.CarrierNetworkConfig;
import com.android.server.wifi.WifiNative;

import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.PublicKey;
import java.util.HashMap;

import javax.annotation.Nonnull;
import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.NoSuchPaddingException;

/**
 * Utilities for the Wifi Service to interact with telephony.
 * TODO(b/132188983): Refactor into TelephonyFacade which owns all instances of
 *  TelephonyManager/SubscriptionManager in Wifi
 */
public class TelephonyUtil {
    public static final String TAG = "TelephonyUtil";
    public static final String DEFAULT_EAP_PREFIX = "\0";

    public static final int CARRIER_INVALID_TYPE = -1;
    public static final int CARRIER_MNO_TYPE = 0; // Mobile Network Operator
    public static final int CARRIER_MVNO_TYPE = 1; // Mobile Virtual Network Operator
    public static final String ANONYMOUS_IDENTITY = "anonymous";
    public static final String THREE_GPP_NAI_REALM_FORMAT = "wlan.mnc%s.mcc%s.3gppnetwork.org";

    // IMSI encryption method: RSA-OAEP with SHA-256 hash function
    private static final String IMSI_CIPHER_TRANSFORMATION =
            "RSA/ECB/OAEPwithSHA-256andMGF1Padding";

    private static final HashMap<Integer, String> EAP_METHOD_PREFIX = new HashMap<>();
    static {
        EAP_METHOD_PREFIX.put(WifiEnterpriseConfig.Eap.AKA, "0");
        EAP_METHOD_PREFIX.put(WifiEnterpriseConfig.Eap.SIM, "1");
        EAP_METHOD_PREFIX.put(WifiEnterpriseConfig.Eap.AKA_PRIME, "6");
    }

    /**
     * 3GPP TS 11.11  2G_authentication command/response
     *                Input: [RAND]
     *                Output: [SRES][Cipher Key Kc]
     */
    private static final int START_SRES_POS = 0; // MUST be 0
    private static final int SRES_LEN = 4;
    private static final int START_KC_POS = START_SRES_POS + SRES_LEN;
    private static final int KC_LEN = 8;

    /**
     * Get the identity for the current SIM or null if the SIM is not available
     *
     * @param tm TelephonyManager instance
     * @param config WifiConfiguration that indicates what sort of authentication is necessary
     * @param telephonyUtil TelephonyUtil instance
     * @param carrierNetworkConfig CarrierNetworkConfig instance
     * @return Pair<identify, encrypted identity> or null if the SIM is not available
     * or config is invalid
     */
    public static Pair<String, String> getSimIdentity(TelephonyManager tm,
            TelephonyUtil telephonyUtil,
            WifiConfiguration config, CarrierNetworkConfig carrierNetworkConfig) {
        if (tm == null) {
            Log.e(TAG, "No valid TelephonyManager");
            return null;
        }
        TelephonyManager defaultDataTm = tm.createForSubscriptionId(
                SubscriptionManager.getDefaultDataSubscriptionId());
        if (carrierNetworkConfig == null) {
            Log.e(TAG, "No valid CarrierNetworkConfig");
            return null;
        }
        String imsi = defaultDataTm.getSubscriberId();
        String mccMnc = "";

        if (defaultDataTm.getSimState() == TelephonyManager.SIM_STATE_READY) {
            mccMnc = defaultDataTm.getSimOperator();
        }

        String identity = buildIdentity(getSimMethodForConfig(config), imsi, mccMnc, false);
        if (identity == null) {
            Log.e(TAG, "Failed to build the identity");
            return null;
        }

        ImsiEncryptionInfo imsiEncryptionInfo;
        try {
            imsiEncryptionInfo = defaultDataTm.getCarrierInfoForImsiEncryption(
                    TelephonyManager.KEY_TYPE_WLAN);
        } catch (RuntimeException e) {
            Log.e(TAG, "Failed to get imsi encryption info: " + e.getMessage());
            return null;
        }
        if (imsiEncryptionInfo == null) {
            // Does not support encrypted identity.
            return Pair.create(identity, "");
        }

        String encryptedIdentity = buildEncryptedIdentity(telephonyUtil, identity,
                    imsiEncryptionInfo);

        // In case of failure for encryption, abort current EAP authentication.
        if (encryptedIdentity == null) {
            Log.e(TAG, "failed to encrypt the identity");
            return null;
        }
        return Pair.create(identity, encryptedIdentity);
    }

    /**
     * Gets Anonymous identity for current active SIM.
     *
     * @param tm TelephonyManager instance
     * @return anonymous identity@realm which is based on current MCC/MNC, {@code null} if SIM is
     * not ready or absent.
     */
    public static String getAnonymousIdentityWith3GppRealm(@Nonnull TelephonyManager tm) {
        if (tm == null) {
            return null;
        }
        TelephonyManager defaultDataTm = tm.createForSubscriptionId(
                SubscriptionManager.getDefaultDataSubscriptionId());
        if (defaultDataTm.getSimState() != TelephonyManager.SIM_STATE_READY) {
            return null;
        }
        String mccMnc = defaultDataTm.getSimOperator();
        if (mccMnc == null || mccMnc.isEmpty()) {
            return null;
        }

        // Extract mcc & mnc from mccMnc
        String mcc = mccMnc.substring(0, 3);
        String mnc = mccMnc.substring(3);

        if (mnc.length() == 2) {
            mnc = "0" + mnc;
        }

        String realm = String.format(THREE_GPP_NAI_REALM_FORMAT, mnc, mcc);
        return ANONYMOUS_IDENTITY + "@" + realm;
    }

    /**
     * Encrypt the given data with the given public key.  The encrypted data will be returned as
     * a Base64 encoded string.
     *
     * @param key The public key to use for encryption
     * @param encodingFlag base64 encoding flag
     * @return Base64 encoded string, or null if encryption failed
     */
    @VisibleForTesting
    public String encryptDataUsingPublicKey(PublicKey key, byte[] data, int encodingFlag) {
        try {
            Cipher cipher = Cipher.getInstance(IMSI_CIPHER_TRANSFORMATION);
            cipher.init(Cipher.ENCRYPT_MODE, key);
            byte[] encryptedBytes = cipher.doFinal(data);

            return Base64.encodeToString(encryptedBytes, 0, encryptedBytes.length, encodingFlag);
        } catch (NoSuchAlgorithmException | NoSuchPaddingException | InvalidKeyException
                | IllegalBlockSizeException | BadPaddingException e) {
            Log.e(TAG, "Encryption failed: " + e.getMessage());
            return null;
        }
    }

    /**
     * Create the encrypted identity.
     *
     * Prefix value:
     * "0" - EAP-AKA Identity
     * "1" - EAP-SIM Identity
     * "6" - EAP-AKA' Identity
     * Encrypted identity format: prefix|IMSI@<NAIRealm>
     * @param telephonyUtil      TelephonyUtil instance
     * @param identity           permanent identity with format based on section 4.1.1.6 of RFC 4187
     *                           and 4.2.1.6 of RFC 4186.
     * @param imsiEncryptionInfo The IMSI encryption info retrieved from the SIM
     * @return "\0" + encryptedIdentity + "{, Key Identifier AVP}"
     */
    private static String buildEncryptedIdentity(TelephonyUtil telephonyUtil, String identity,
            ImsiEncryptionInfo imsiEncryptionInfo) {
        if (imsiEncryptionInfo == null) {
            Log.e(TAG, "imsiEncryptionInfo is not valid");
            return null;
        }
        if (identity == null) {
            Log.e(TAG, "identity is not valid");
            return null;
        }

        // Build and return the encrypted identity.
        String encryptedIdentity = telephonyUtil.encryptDataUsingPublicKey(
                imsiEncryptionInfo.getPublicKey(), identity.getBytes(), Base64.NO_WRAP);
        if (encryptedIdentity == null) {
            Log.e(TAG, "Failed to encrypt IMSI");
            return null;
        }
        encryptedIdentity = DEFAULT_EAP_PREFIX + encryptedIdentity;
        if (imsiEncryptionInfo.getKeyIdentifier() != null) {
            // Include key identifier AVP (Attribute Value Pair).
            encryptedIdentity = encryptedIdentity + "," + imsiEncryptionInfo.getKeyIdentifier();
        }
        return encryptedIdentity;
    }

    /**
     * Create an identity used for SIM-based EAP authentication. The identity will be based on
     * the info retrieved from the SIM card, such as IMSI and IMSI encryption info. The IMSI
     * contained in the identity will be encrypted if IMSI encryption info is provided.
     *
     * See  rfc4186 & rfc4187 & rfc5448:
     *
     * Identity format:
     * Prefix | [IMSI || Encrypted IMSI] | @realm | {, Key Identifier AVP}
     * where "|" denotes concatenation, "||" denotes exclusive value, "{}"
     * denotes optional value, and realm is the 3GPP network domain name derived from the given
     * MCC/MNC according to the 3GGP spec(TS23.003).
     *
     * Prefix value:
     * "\0" - Encrypted Identity
     * "0" - EAP-AKA Identity
     * "1" - EAP-SIM Identity
     * "6" - EAP-AKA' Identity
     *
     * Encrypted IMSI:
     * Base64{RSA_Public_Key_Encryption{eapPrefix | IMSI}}
     * where "|" denotes concatenation,
     *
     * @param eapMethod EAP authentication method: EAP-SIM, EAP-AKA, EAP-AKA'
     * @param imsi The IMSI retrieved from the SIM
     * @param mccMnc The MCC MNC identifier retrieved from the SIM
     * @param isEncrypted Whether the imsi is encrypted or not.
     * @return the eap identity, built using either the encrypted or un-encrypted IMSI.
     */
    private static String buildIdentity(int eapMethod, String imsi, String mccMnc,
                                        boolean isEncrypted) {
        if (imsi == null || imsi.isEmpty()) {
            Log.e(TAG, "No IMSI or IMSI is null");
            return null;
        }

        String prefix = isEncrypted ? DEFAULT_EAP_PREFIX : EAP_METHOD_PREFIX.get(eapMethod);
        if (prefix == null) {
            return null;
        }

        /* extract mcc & mnc from mccMnc */
        String mcc;
        String mnc;
        if (mccMnc != null && !mccMnc.isEmpty()) {
            mcc = mccMnc.substring(0, 3);
            mnc = mccMnc.substring(3);
            if (mnc.length() == 2) {
                mnc = "0" + mnc;
            }
        } else {
            // extract mcc & mnc from IMSI, assume mnc size is 3
            mcc = imsi.substring(0, 3);
            mnc = imsi.substring(3, 6);
        }

        String naiRealm = String.format(THREE_GPP_NAI_REALM_FORMAT, mnc, mcc);
        return prefix + imsi + "@" + naiRealm;
    }

    /**
     * Return the associated SIM method for the configuration.
     *
     * @param config WifiConfiguration corresponding to the network.
     * @return the outer EAP method associated with this SIM configuration.
     */
    private static int getSimMethodForConfig(WifiConfiguration config) {
        if (config == null || config.enterpriseConfig == null) {
            return WifiEnterpriseConfig.Eap.NONE;
        }
        int eapMethod = config.enterpriseConfig.getEapMethod();
        if (eapMethod == WifiEnterpriseConfig.Eap.PEAP) {
            // Translate known inner eap methods into an equivalent outer eap method.
            switch (config.enterpriseConfig.getPhase2Method()) {
                case WifiEnterpriseConfig.Phase2.SIM:
                    eapMethod = WifiEnterpriseConfig.Eap.SIM;
                    break;
                case WifiEnterpriseConfig.Phase2.AKA:
                    eapMethod = WifiEnterpriseConfig.Eap.AKA;
                    break;
                case WifiEnterpriseConfig.Phase2.AKA_PRIME:
                    eapMethod = WifiEnterpriseConfig.Eap.AKA_PRIME;
                    break;
            }
        }

        return isSimEapMethod(eapMethod) ? eapMethod : WifiEnterpriseConfig.Eap.NONE;
    }

    /**
     * Checks if the network is a SIM config.
     *
     * @param config Config corresponding to the network.
     * @return true if it is a SIM config, false otherwise.
     */
    public static boolean isSimConfig(WifiConfiguration config) {
        return getSimMethodForConfig(config) != WifiEnterpriseConfig.Eap.NONE;
    }

    /**
     * Returns true if {@code identity} contains an anonymous@realm identity, false otherwise.
     */
    public static boolean isAnonymousAtRealmIdentity(String identity) {
        if (identity == null) return false;
        return identity.startsWith(TelephonyUtil.ANONYMOUS_IDENTITY + "@");
    }

    /**
     * Checks if the EAP outer method is SIM related.
     *
     * @param eapMethod WifiEnterpriseConfig Eap method.
     * @return true if this EAP outer method is SIM-related, false otherwise.
     */
    public static boolean isSimEapMethod(int eapMethod) {
        return eapMethod == WifiEnterpriseConfig.Eap.SIM
                || eapMethod == WifiEnterpriseConfig.Eap.AKA
                || eapMethod == WifiEnterpriseConfig.Eap.AKA_PRIME;
    }

    // TODO replace some of this code with Byte.parseByte
    private static int parseHex(char ch) {
        if ('0' <= ch && ch <= '9') {
            return ch - '0';
        } else if ('a' <= ch && ch <= 'f') {
            return ch - 'a' + 10;
        } else if ('A' <= ch && ch <= 'F') {
            return ch - 'A' + 10;
        } else {
            throw new NumberFormatException("" + ch + " is not a valid hex digit");
        }
    }

    private static byte[] parseHex(String hex) {
        /* This only works for good input; don't throw bad data at it */
        if (hex == null) {
            return new byte[0];
        }

        if (hex.length() % 2 != 0) {
            throw new NumberFormatException(hex + " is not a valid hex string");
        }

        byte[] result = new byte[(hex.length()) / 2 + 1];
        result[0] = (byte) ((hex.length()) / 2);
        for (int i = 0, j = 1; i < hex.length(); i += 2, j++) {
            int val = parseHex(hex.charAt(i)) * 16 + parseHex(hex.charAt(i + 1));
            byte b = (byte) (val & 0xFF);
            result[j] = b;
        }

        return result;
    }

    private static byte[] parseHexWithoutLength(String hex) {
        byte[] tmpRes = parseHex(hex);
        if (tmpRes.length == 0) {
            return tmpRes;
        }

        byte[] result = new byte[tmpRes.length - 1];
        System.arraycopy(tmpRes, 1, result, 0, tmpRes.length - 1);

        return result;
    }

    private static String makeHex(byte[] bytes) {
        StringBuilder sb = new StringBuilder();
        for (byte b : bytes) {
            sb.append(String.format("%02x", b));
        }
        return sb.toString();
    }

    private static String makeHex(byte[] bytes, int from, int len) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < len; i++) {
            sb.append(String.format("%02x", bytes[from + i]));
        }
        return sb.toString();
    }

    private static byte[] concatHex(byte[] array1, byte[] array2) {

        int len = array1.length + array2.length;

        byte[] result = new byte[len];

        int index = 0;
        if (array1.length != 0) {
            for (byte b : array1) {
                result[index] = b;
                index++;
            }
        }

        if (array2.length != 0) {
            for (byte b : array2) {
                result[index] = b;
                index++;
            }
        }

        return result;
    }

    /**
     * Calculate SRES and KC as 3G authentication.
     *
     * Standard       Cellular_auth     Type Command
     *
     * 3GPP TS 31.102 3G_authentication [Length][RAND][Length][AUTN]
     *                         [Length][RES][Length][CK][Length][IK] and more
     *
     * @param requestData RAND data from server.
     * @param tm the instance of TelephonyManager.
     * @return the response data processed by SIM. If all request data is malformed, then returns
     * empty string. If request data is invalid, then returns null.
     */
    public static String getGsmSimAuthResponse(String[] requestData, TelephonyManager tm) {
        return getGsmAuthResponseWithLength(requestData, tm, TelephonyManager.APPTYPE_USIM);
    }

    /**
     * Calculate SRES and KC as 2G authentication.
     *
     * Standard       Cellular_auth     Type Command
     *
     * 3GPP TS 31.102 2G_authentication [Length][RAND]
     *                         [Length][SRES][Length][Cipher Key Kc]
     *
     * @param requestData RAND data from server.
     * @param tm the instance of TelephonyManager.
     * @return the response data processed by SIM. If all request data is malformed, then returns
     * empty string. If request data is invalid, then returns null.
     */
    public static String getGsmSimpleSimAuthResponse(String[] requestData, TelephonyManager tm) {
        return getGsmAuthResponseWithLength(requestData, tm, TelephonyManager.APPTYPE_SIM);
    }

    private static String getGsmAuthResponseWithLength(String[] requestData, TelephonyManager tm,
            int appType) {
        if (tm == null) {
            Log.e(TAG, "No valid TelephonyManager");
            return null;
        }
        TelephonyManager defaultDataTm = tm.createForSubscriptionId(
                SubscriptionManager.getDefaultDataSubscriptionId());
        StringBuilder sb = new StringBuilder();
        for (String challenge : requestData) {
            if (challenge == null || challenge.isEmpty()) {
                continue;
            }
            Log.d(TAG, "RAND = " + challenge);

            byte[] rand = null;
            try {
                rand = parseHex(challenge);
            } catch (NumberFormatException e) {
                Log.e(TAG, "malformed challenge");
                continue;
            }

            String base64Challenge = Base64.encodeToString(rand, Base64.NO_WRAP);

            String tmResponse = defaultDataTm.getIccAuthentication(
                    appType, TelephonyManager.AUTHTYPE_EAP_SIM, base64Challenge);
            Log.v(TAG, "Raw Response - " + tmResponse);

            if (tmResponse == null || tmResponse.length() <= 4) {
                Log.e(TAG, "bad response - " + tmResponse);
                return null;
            }

            byte[] result = Base64.decode(tmResponse, Base64.DEFAULT);
            Log.v(TAG, "Hex Response -" + makeHex(result));
            int sresLen = result[0];
            if (sresLen < 0 || sresLen >= result.length) {
                Log.e(TAG, "malformed response - " + tmResponse);
                return null;
            }
            String sres = makeHex(result, 1, sresLen);
            int kcOffset = 1 + sresLen;
            if (kcOffset >= result.length) {
                Log.e(TAG, "malformed response - " + tmResponse);
                return null;
            }
            int kcLen = result[kcOffset];
            if (kcLen < 0 || kcOffset + kcLen > result.length) {
                Log.e(TAG, "malformed response - " + tmResponse);
                return null;
            }
            String kc = makeHex(result, 1 + kcOffset, kcLen);
            sb.append(":" + kc + ":" + sres);
            Log.v(TAG, "kc:" + kc + " sres:" + sres);
        }

        return sb.toString();
    }

    /**
     * Calculate SRES and KC as 2G authentication.
     *
     * Standard       Cellular_auth     Type Command
     *
     * 3GPP TS 11.11  2G_authentication [RAND]
     *                         [SRES][Cipher Key Kc]
     *
     * @param requestData RAND data from server.
     * @param tm the instance of TelephonyManager.
     * @return the response data processed by SIM. If all request data is malformed, then returns
     * empty string. If request data is invalid, then returns null.
     */
    public static String getGsmSimpleSimNoLengthAuthResponse(String[] requestData,
            TelephonyManager tm) {
        if (tm == null) {
            Log.e(TAG, "No valid TelephonyManager");
            return null;
        }
        TelephonyManager defaultDataTm = tm.createForSubscriptionId(
                SubscriptionManager.getDefaultDataSubscriptionId());
        StringBuilder sb = new StringBuilder();
        for (String challenge : requestData) {
            if (challenge == null || challenge.isEmpty()) {
                continue;
            }
            Log.d(TAG, "RAND = " + challenge);

            byte[] rand = null;
            try {
                rand = parseHexWithoutLength(challenge);
            } catch (NumberFormatException e) {
                Log.e(TAG, "malformed challenge");
                continue;
            }

            String base64Challenge = Base64.encodeToString(rand, Base64.NO_WRAP);

            String tmResponse = defaultDataTm.getIccAuthentication(TelephonyManager.APPTYPE_SIM,
                    TelephonyManager.AUTHTYPE_EAP_SIM, base64Challenge);
            Log.v(TAG, "Raw Response - " + tmResponse);

            if (tmResponse == null || tmResponse.length() <= 4) {
                Log.e(TAG, "bad response - " + tmResponse);
                return null;
            }

            byte[] result = Base64.decode(tmResponse, Base64.DEFAULT);
            if (SRES_LEN + KC_LEN != result.length) {
                Log.e(TAG, "malformed response - " + tmResponse);
                return null;
            }
            Log.v(TAG, "Hex Response -" + makeHex(result));
            String sres = makeHex(result, START_SRES_POS, SRES_LEN);
            String kc = makeHex(result, START_KC_POS, KC_LEN);
            sb.append(":" + kc + ":" + sres);
            Log.v(TAG, "kc:" + kc + " sres:" + sres);
        }

        return sb.toString();
    }

    /**
     * Data supplied when making a SIM Auth Request
     */
    public static class SimAuthRequestData {
        public SimAuthRequestData() {}
        public SimAuthRequestData(int networkId, int protocol, String ssid, String[] data) {
            this.networkId = networkId;
            this.protocol = protocol;
            this.ssid = ssid;
            this.data = data;
        }

        public int networkId;
        public int protocol;
        public String ssid;
        // EAP-SIM: data[] contains the 3 rand, one for each of the 3 challenges
        // EAP-AKA/AKA': data[] contains rand & authn couple for the single challenge
        public String[] data;
    }

    /**
     * The response to a SIM Auth request if successful
     */
    public static class SimAuthResponseData {
        public SimAuthResponseData(String type, String response) {
            this.type = type;
            this.response = response;
        }

        public String type;
        public String response;
    }

    public static SimAuthResponseData get3GAuthResponse(SimAuthRequestData requestData,
            TelephonyManager tm) {
        StringBuilder sb = new StringBuilder();
        byte[] rand = null;
        byte[] authn = null;
        String resType = WifiNative.SIM_AUTH_RESP_TYPE_UMTS_AUTH;

        if (requestData.data.length == 2) {
            try {
                rand = parseHex(requestData.data[0]);
                authn = parseHex(requestData.data[1]);
            } catch (NumberFormatException e) {
                Log.e(TAG, "malformed challenge");
            }
        } else {
            Log.e(TAG, "malformed challenge");
        }

        String tmResponse = "";
        if (rand != null && authn != null) {
            String base64Challenge = Base64.encodeToString(concatHex(rand, authn), Base64.NO_WRAP);
            if (tm != null) {
                tmResponse = tm
                        .createForSubscriptionId(SubscriptionManager.getDefaultDataSubscriptionId())
                        .getIccAuthentication(TelephonyManager.APPTYPE_USIM,
                                TelephonyManager.AUTHTYPE_EAP_AKA, base64Challenge);
                Log.v(TAG, "Raw Response - " + tmResponse);
            } else {
                Log.e(TAG, "No valid TelephonyManager");
            }
        }

        boolean goodReponse = false;
        if (tmResponse != null && tmResponse.length() > 4) {
            byte[] result = Base64.decode(tmResponse, Base64.DEFAULT);
            Log.e(TAG, "Hex Response - " + makeHex(result));
            byte tag = result[0];
            if (tag == (byte) 0xdb) {
                Log.v(TAG, "successful 3G authentication ");
                int resLen = result[1];
                String res = makeHex(result, 2, resLen);
                int ckLen = result[resLen + 2];
                String ck = makeHex(result, resLen + 3, ckLen);
                int ikLen = result[resLen + ckLen + 3];
                String ik = makeHex(result, resLen + ckLen + 4, ikLen);
                sb.append(":" + ik + ":" + ck + ":" + res);
                Log.v(TAG, "ik:" + ik + "ck:" + ck + " res:" + res);
                goodReponse = true;
            } else if (tag == (byte) 0xdc) {
                Log.e(TAG, "synchronisation failure");
                int autsLen = result[1];
                String auts = makeHex(result, 2, autsLen);
                resType = WifiNative.SIM_AUTH_RESP_TYPE_UMTS_AUTS;
                sb.append(":" + auts);
                Log.v(TAG, "auts:" + auts);
                goodReponse = true;
            } else {
                Log.e(TAG, "bad response - unknown tag = " + tag);
            }
        } else {
            Log.e(TAG, "bad response - " + tmResponse);
        }

        if (goodReponse) {
            String response = sb.toString();
            Log.v(TAG, "Supplicant Response -" + response);
            return new SimAuthResponseData(resType, response);
        } else {
            return null;
        }
    }

    /**
     * Get the carrier type of current SIM.
     *
     * @param tm {@link TelephonyManager} instance
     * @return carrier type of current active sim, {{@link #CARRIER_INVALID_TYPE}} if sim is not
     * ready or {@code tm} is {@code null}
     */
    public static int getCarrierType(@NonNull TelephonyManager tm) {
        if (tm == null) {
            return CARRIER_INVALID_TYPE;
        }
        TelephonyManager defaultDataTm = tm.createForSubscriptionId(
                SubscriptionManager.getDefaultDataSubscriptionId());

        if (defaultDataTm.getSimState() != TelephonyManager.SIM_STATE_READY) {
            return CARRIER_INVALID_TYPE;
        }

        // If two APIs return the same carrier ID, then is considered as MNO, otherwise MVNO
        if (defaultDataTm.getCarrierIdFromSimMccMnc() == defaultDataTm.getSimCarrierId()) {
            return CARRIER_MNO_TYPE;
        }
        return CARRIER_MVNO_TYPE;
    }

    /**
     * Returns true if at least one SIM is present on the device, false otherwise.
     */
    public static boolean isSimPresent(@Nonnull SubscriptionManager sm) {
        return sm.getActiveSubscriptionIdList().length > 0;
    }
}
