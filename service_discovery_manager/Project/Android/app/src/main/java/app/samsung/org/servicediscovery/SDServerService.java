package app.samsung.org.servicediscovery;

import android.app.Service;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.IBinder;
import android.util.Log;

public class SDServerService extends Service {
    static {
        System.loadLibrary("sd-server-lib");
    }

    private static Context sApplicationContext;
    private static final String TAG = "SDServerService";
    private static final String PACKAGE_NAME = "org.chromium.chrome";

    private Thread mainThread;

    @Override
    public void onCreate() {
        super.onCreate();
        sApplicationContext = this.getApplicationContext();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        mainThread = new Thread(new Runnable() {
            @Override
            public void run() {
                startServer();
            }
        });
        mainThread.start();
        return super.onStartCommand(intent, flags, startId);
    }

    @Override
    public IBinder onBind(Intent intent) {
        // TODO: Return the communication channel to the service.
        throw new UnsupportedOperationException("Not yet implemented");
    }

    public static boolean startChromeRenderer() {
        PackageManager packageManager = sApplicationContext.getPackageManager();
        try {
            packageManager.getPackageInfo(PACKAGE_NAME, 0);
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Not found " + PACKAGE_NAME);
            return false;
        }

        Intent intent = packageManager.getLaunchIntentForPackage(PACKAGE_NAME);
        try {
            sApplicationContext.startActivity(intent);
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "Fail to start Chrome renderer!");
            return false;
        }

        return true;
    }

    public native int startServer();
    public native void stopServer();
}
