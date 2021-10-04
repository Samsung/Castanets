package com.samsung.offloadsetting;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.preference.CheckBoxPreference;
import android.support.v7.preference.EditTextPreference;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceFragmentCompat;
import android.support.v7.preference.PreferenceManager;
import android.support.v7.preference.PreferenceScreen;
import android.support.v7.preference.SwitchPreferenceCompat;
import android.util.Log;
import android.webkit.URLUtil;
import android.widget.Toast;

import com.samsung.offloadworker.OffloadService;
import com.samsung.offloadworker.R;

import java.util.HashMap;
import java.util.Map;

public class SettingsActivity extends AppCompatActivity {

  private static String TAG = "SettingsActivity";

  protected final String PERMISSON_DENIED_MSG = "The permission was denied. You need to allow the permission to use this app.";
  protected final String PERMISSON_DENIED_DONT_ASK_MSG = "The permission was denied. App info > Storage > Manage storage > Clear all data.";

  private int ACTION_MANAGE_PERMISSION_REQUEST = 1888;
  private int OVERLAY_PERMISSION_REQUEST = 2888;
  private String[] REQUIRED_PERMISSIONS = {Manifest.permission.CAMERA,
      Manifest.permission.RECORD_AUDIO,
      Manifest.permission.WRITE_EXTERNAL_STORAGE};

  protected void onCreate(Bundle savedInstanceState) {
    Log.i(TAG, "onCreate, " + getIntent());
    super.onCreate(savedInstanceState);
    if (handleEdgeIntent(getIntent())) {
      Log.i(TAG, "onCreate, finish()");
      finish();
      return;
    }

    checkPermission();

    if (savedInstanceState == null) {
      getSupportFragmentManager().beginTransaction()
          .replace(android.R.id.content, new PrefsFragment()).commit();
    }
  }

  @Override
  protected void onNewIntent(Intent intent) {
    Log.i(TAG, "onNewIntent, " + intent);
    super.onNewIntent(intent);
    handleEdgeIntent(intent);
  }

  private boolean handleEdgeIntent(Intent intent) {
    String args = intent.getStringExtra("args");
    if (args == null) {
      return false;
    }
    Log.i(TAG, "handleEdgeIntent() args:" + args);
    Map<String, String> argMap = new HashMap();
    for (String param : args.split("\\s+")) {
      String[] keyVal = param.split("=");
      argMap.put(keyVal[0], (keyVal.length == 2) ? keyVal[1] : "");
    }
    if (argMap.get("--type").equals("offloadworker") && argMap.get("--signaling-server") != null) {
      OffloadService.startService(this, argMap.get("--signaling-server"), "discovery");
      return true;
    }
    return false;
  }

  private void enableOffloadService(boolean enable) {
    Log.i(TAG, "enableOffloadService() " + enable);
    if (enable) {
      OffloadService.startService(this, "initialize", "setting");
    } else {
      OffloadService.stopService(this);
    }
  }

  @Override
  @SuppressLint("MissingSuperCall")
  public void onRequestPermissionsResult(
      int requestCode, String[] permissions, int[] grantResults) {
    boolean check_result = true;
    for (int result : grantResults) {
      if (result != PackageManager.PERMISSION_GRANTED) {
        check_result = false;
        break;
      }
    }

    if (!check_result) {
      if (ActivityCompat.shouldShowRequestPermissionRationale(this, REQUIRED_PERMISSIONS[0])
          || ActivityCompat.shouldShowRequestPermissionRationale(this, REQUIRED_PERMISSIONS[1])) {
        showToast(PERMISSON_DENIED_MSG);
      } else {
        showToast(PERMISSON_DENIED_DONT_ASK_MSG);

        Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
            Uri.parse("package:" + getPackageName()));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(intent);
      }
      finish();
    }
  }

  private void checkPermission() {
    Context context = this.getApplicationContext();
    // Check and request CAMERA, RECORD_AUDIO and WRITE_EXTERNAL_STORAGE permissions.
    int cameraPermission = ContextCompat.checkSelfPermission(context, Manifest.permission.CAMERA);
    int recordAudioPermission = ContextCompat
        .checkSelfPermission(context, Manifest.permission.RECORD_AUDIO);
    int writePermission = ContextCompat
        .checkSelfPermission(context, Manifest.permission.WRITE_EXTERNAL_STORAGE);

    if (cameraPermission != PackageManager.PERMISSION_GRANTED
        || recordAudioPermission != PackageManager.PERMISSION_GRANTED
        || writePermission != PackageManager.PERMISSION_GRANTED) {
      Log.w(TAG, "Required permissions was not granted. Request permissions.");
      ActivityCompat.requestPermissions(this,
          REQUIRED_PERMISSIONS,
          ACTION_MANAGE_PERMISSION_REQUEST);
    }
    checkOverlayPermission();
  }

  private void checkOverlayPermission() {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M &&
        !Settings.canDrawOverlays(this)) {
      Intent intent = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
          Uri.parse("package:" + getPackageName()));
      startActivityForResult(intent, OVERLAY_PERMISSION_REQUEST);
    }
  }

  private void showToast(String msg) {
    Toast.makeText(this.getApplicationContext(), msg, Toast.LENGTH_LONG).show();
  }

  public static class PrefsFragment
      extends PreferenceFragmentCompat
      implements SharedPreferences.OnSharedPreferenceChangeListener {

    private SettingsActivity activity;

    public void onAttach(Context context) {
      super.onAttach(context);
      activity = (SettingsActivity) getActivity();
      PreferenceManager.getDefaultSharedPreferences(activity)
          .registerOnSharedPreferenceChangeListener(this);
    }

    public void onDetach() {
      super.onDetach();
      PreferenceManager.getDefaultSharedPreferences(activity)
          .unregisterOnSharedPreferenceChangeListener(this);
      activity = null;
    }

    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
      setPreferencesFromResource(R.xml.preferences, rootKey);
      if (activity == null) {
        Log.e(TAG, "activity is null.");
        return;
      }

      activity.enableOffloadService(
          PreferenceManager.getDefaultSharedPreferences(activity)
              .getBoolean(getString(R.string.key_enable_offload), false));

      SwitchPreferenceCompat enableOffload = (SwitchPreferenceCompat) findPreference(
          getString(R.string.key_enable_offload));

      // Debug Mode
      SwitchPreferenceCompat debugOffload = (SwitchPreferenceCompat) findPreference(
          getString(R.string.key_enable_debug));
      final CheckBoxPreference useDiscovery = (CheckBoxPreference) findPreference(
          getString(R.string.key_use_discovery));
      final EditTextPreference serverAddress = (EditTextPreference) findPreference(
          getString(R.string.key_server_address));
      final PreferenceScreen openWebView = (PreferenceScreen) findPreference(
          getString(R.string.key_open_webview));

      if (enableOffload == null || debugOffload == null || useDiscovery == null
          || serverAddress == null || openWebView == null) {
        Log.e(TAG, "Error to find preferences");
        return;
      }

      enableOffload.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {

        public boolean onPreferenceChange(Preference preference, Object newValue) {
          activity.enableOffloadService((Boolean) newValue);
          return true;
        }
      });

      useDiscovery.setEnabled(debugOffload.isChecked());
      serverAddress.setEnabled(debugOffload.isChecked());
      openWebView.setEnabled(debugOffload.isChecked());

      debugOffload.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
        public boolean onPreferenceChange(Preference preference, Object newValue) {
          boolean debugEnabled = (Boolean) newValue;
          if (OffloadService.getInstance() != null) {
            OffloadService.getInstance().updateWebView(debugEnabled, null, null);
          }
          useDiscovery.setEnabled(debugEnabled);
          serverAddress.setEnabled(debugEnabled);
          openWebView.setEnabled(debugEnabled);
          return true;
        }
      });

      serverAddress.setSummary(serverAddress.getText());
      serverAddress.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
        public boolean onPreferenceChange(Preference preference, Object newValue) {
          String newAddress = (String) newValue;
          if (newAddress.isEmpty() || !URLUtil.isValidUrl(newAddress)) {
            Toast.makeText(getActivity(), "Invalid or empty URL. " + newValue, Toast.LENGTH_LONG)
                .show();
            return false;
          }
          if (OffloadService.getInstance() != null) {
            OffloadService.getInstance().updateWebView(null, newAddress, true);
          }
          return true;
        }
      });

      openWebView.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {
        public boolean onPreferenceClick(Preference preference) {
          OffloadService.startService(getContext(), PreferenceManager
                  .getDefaultSharedPreferences(getContext())
                  .getString(getString(R.string.key_server_address), serverAddress.getText()),
              "setting");
          return true;
        }
      });
    }

    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
      EditTextPreference textPreference;
      if (key == getString(R.string.key_server_address)
          && (textPreference = (EditTextPreference) findPreference(key)) != null) {
        String newAddress = sharedPreferences.getString(key, "");
        textPreference.setText(newAddress);
        textPreference.setSummary(newAddress);
      }
    }
  }
}
