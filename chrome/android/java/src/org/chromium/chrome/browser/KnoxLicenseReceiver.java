package org.chromium.chrome.browser;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;
import android.widget.Toast;

import com.samsung.android.knox.EnterpriseDeviceManager;
import com.samsung.android.knox.license.EnterpriseLicenseManager;
import com.samsung.android.knox.license.KnoxEnterpriseLicenseManager;


public class KnoxLicenseReceiver extends BroadcastReceiver {
    private int DEFAULT_ERROR_CODE = -1;
    public static final String TAG = "Knox";

    private void showToast(Context context, int msg_res) {
        Toast.makeText(context, context.getResources().getString(msg_res), Toast.LENGTH_SHORT)
                .show();
    }

    private void showToast(Context context, String msg) {
        Toast.makeText(context, msg, Toast.LENGTH_SHORT).show();
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        int msg_res = -1;

        if (intent == null) {
            // No intent action is available
            showToast(context, "no_intent");
            return;
        } else {
            String action = intent.getAction();
            if (action == null) {
                // No intent action is available
                showToast(context, "no_intent_action");
                return;
            } else if (action.equals(KnoxEnterpriseLicenseManager.ACTION_LICENSE_STATUS)) {
                // Intent from KPE license activation attempt is obtained
                int errorCode = intent.getIntExtra(
                        KnoxEnterpriseLicenseManager.EXTRA_LICENSE_ERROR_CODE, DEFAULT_ERROR_CODE);

                if (errorCode == KnoxEnterpriseLicenseManager.ERROR_NONE) {
                    // license activated successfully
                    showToast(context, "klm_activated_succesfully");
                    Log.i(TAG, "klm_activated_succesfully");
                    return;
                } else {
                    // license activation failed
                    switch (errorCode) {
                        case KnoxEnterpriseLicenseManager.ERROR_INTERNAL:
                        case KnoxEnterpriseLicenseManager.ERROR_INTERNAL_SERVER:
                        case KnoxEnterpriseLicenseManager.ERROR_INVALID_LICENSE:
                        case KnoxEnterpriseLicenseManager.ERROR_INVALID_PACKAGE_NAME:
                        case KnoxEnterpriseLicenseManager.ERROR_LICENSE_TERMINATED:
                        case KnoxEnterpriseLicenseManager.ERROR_NETWORK_DISCONNECTED:
                        case KnoxEnterpriseLicenseManager.ERROR_NETWORK_GENERAL:
                        case KnoxEnterpriseLicenseManager.ERROR_NOT_CURRENT_DATE:
                        case KnoxEnterpriseLicenseManager.ERROR_NULL_PARAMS:
                        case KnoxEnterpriseLicenseManager.ERROR_UNKNOWN:
                        case KnoxEnterpriseLicenseManager.ERROR_USER_DISAGREES_LICENSE_AGREEMENT:
                            msg_res = errorCode;
                            break;

                        default:
                            showToast(context, "err_klm_code_unknown");
                            Log.i(TAG, "err_klm_code_unknown");
                            return;
                    }

                    // Display error message
                    showToast(context, msg_res);
                    Log.i(TAG, context.getString(msg_res));
                    return;
                }
            }
        }
    }
}
