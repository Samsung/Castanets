package org.chromium.chrome.browser;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.widget.Toast;

import org.chromium.chrome.browser.firstrun.FirstRunActivity;

public class CastanetsSettings extends AppCompatActivity {

    private static String TAG = "CastanetsSettings";

    private int OVERLAY_PERMISSION_REQUEST = 2888;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.i(TAG, "onCreate, " + getIntent());
        super.onCreate(savedInstanceState);

        Intent castanets = getIntent();
        Intent intent = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
            Uri.parse("package:" + getPackageName()));
        startActivityForResult(intent, OVERLAY_PERMISSION_REQUEST);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == OVERLAY_PERMISSION_REQUEST) {
            Log.i(TAG, "OVERLAY_PERMISSION_REQUEST, result=" + resultCode);
            if (!Settings.canDrawOverlays(this)) {
                Toast.makeText(this.getApplicationContext(), "Please allow OVERLAY PERMISSION.", Toast.LENGTH_LONG).show();
            }

            Intent intent = new Intent(this, FirstRunActivity.class);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(intent);
            finish();
        }
    }
}
