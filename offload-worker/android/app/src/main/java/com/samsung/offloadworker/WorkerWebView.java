package com.samsung.offloadworker;

import android.bluetooth.BluetoothAdapter;
import android.content.Context;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.pm.ApplicationInfo;
import android.net.http.SslError;
import android.net.Uri;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.webkit.JavascriptInterface;
import android.webkit.PermissionRequest;
import android.webkit.SslErrorHandler;
import android.webkit.URLUtil;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import java.util.Map;
import org.json.JSONObject;
import org.json.JSONException;

public class WorkerWebView {

  private static final String TAG = "WorkerWebview";
  private static final boolean INSPECT_DEBUG = true;

  private Context context;
  private Handler handler = new Handler();
  private String serverAddress;
  private View rootView;
  private WebView webView;
  private boolean webViewReady = false;
  private String deviceName;
  private boolean forceConnect = false;

  class WebAppInterface {

    private WorkerWebView workerWebView;

    public WebAppInterface(WorkerWebView workerWebView) {
      this.workerWebView = workerWebView;
    }

    @JavascriptInterface
    public void emit(final String event, final String args) {
      handler.post(new Runnable() {
        @Override
        public void run() {
          workerWebView.onWebEvent(event, args);
        }
      });
    }
  }

  public WorkerWebView(final Context context) {
    Log.i(TAG, "WorkerWebview");
    this.context = context;
    rootView = LayoutInflater.from(this.context).inflate(R.layout.webview, null);

    rootView.findViewById(R.id.closeButton).setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View v) {
        if (OffloadService.getInstance() != null) {
          OffloadService.getInstance().updateWebView(false, null, null);
        }
      }
    });

    deviceName = BluetoothAdapter.getDefaultAdapter().getName();

    webView = rootView.findViewById(R.id.webview);

    WebSettings webSettings = webView.getSettings();
    webSettings.setJavaScriptEnabled(true);
    webSettings.setSupportMultipleWindows(false);
    webSettings.setJavaScriptCanOpenWindowsAutomatically(false);
    webSettings.setLoadWithOverviewMode(true);
    webSettings.setSupportZoom(true);
    webSettings.setBuiltInZoomControls(true);
    webSettings.setLayoutAlgorithm(WebSettings.LayoutAlgorithm.NORMAL);
    webSettings.setCacheMode(WebSettings.LOAD_NO_CACHE);
    webSettings.setDomStorageEnabled(true);
    webSettings.setAllowContentAccess(true);
    webSettings.setAllowFileAccessFromFileURLs(true);
    webSettings.setAllowUniversalAccessFromFileURLs(true);
    webSettings.setMixedContentMode(WebSettings.MIXED_CONTENT_ALWAYS_ALLOW);
    webSettings.setMediaPlaybackRequiresUserGesture(false);

    webView.setWebViewClient(new WebViewClient() {
      @Override
      public void onPageFinished(WebView view, String url) {
        Log.i(TAG, "onPageFinished. " + url);
        webViewReady = true;
        if (serverAddress != null) connect(serverAddress, forceConnect);
        else checkCapability();
      }

      @Override
      public void onReceivedSslError(WebView view, final SslErrorHandler handler,
          SslError error) {
        handler.proceed();
      }
    });
    webView.setWebChromeClient(new WebChromeClient() {
      @Override
      public void onPermissionRequest(final PermissionRequest request) {
        handler.post(new Runnable() {
          @Override
          public void run() {
            request.grant(request.getResources());
          }
        });
      }
    });
    webView.addJavascriptInterface(new WebAppInterface(this), "android");
    webView.setWebContentsDebuggingEnabled(INSPECT_DEBUG);
    webView.loadUrl("file:///android_asset/index-android.html");
  }

  public void destroy() {
    Log.i(TAG, "destroy");
    webView.destroy();
  }

  public View getView() {
    return rootView;
  }

  public void connect(String url, boolean forceConnect) {
    if (url == null || url.isEmpty() || !URLUtil.isValidUrl(url)) {
      Log.e(TAG, "Invalid or empty URL. " + url);
      return;
    }

    if (serverAddress != url) {
      PreferenceManager.getDefaultSharedPreferences(context).edit()
          .putString(context.getString(R.string.key_server_address), url).commit();
    }

    serverAddress = url;
    this.forceConnect = forceConnect;
    if (webViewReady) {
      Log.i(TAG, "connect to " + url + " forceConnect: " + forceConnect);
      webView.loadUrl(String.format(
          "javascript:alert(offloadWorker.connect('%s', {deviceName: '%s', forceConnect: %s}))",
          url, deviceName, forceConnect));
    }
  }

  public void checkCapability() {
    webView.loadUrl(String.format("javascript:offloadWorker.checkCapability()"));
  }

  public void sendConfirmResult(int id, boolean allowed) {
    webView.loadUrl(String.format("javascript:offloadWorker.onConfirmationResult(%s, %s)",
                    id, allowed));
  }

  private void onWebEvent(String event, String args) {
    Log.i(TAG, "emit() " + event + ", " + args);

    if (event.equals("destroyService") && OffloadService.getInstance() != null) {
      OffloadService.stopService(context);
    } else if (event.equals("requestConfirmation")) {
      OffloadService.getInstance().sendConfirmNotification(args);
    } else if (event.equals("writeCapability")) {
      Log.i(TAG, "writeCapability args : " + args);
      if (deviceName != null) {
        try {
          JSONObject replaceArgs = null;
          replaceArgs = new JSONObject(args);
          replaceArgs.put("name", deviceName);
          args = replaceArgs.toString();
        } catch (JSONException e) {
          Log.e(TAG, "JSONObject error : " + e);
        }
      }
      ContentValues value = new ContentValues();
      value.put("offloadjs", args);
      try {
        context.getContentResolver().insert(
          Uri.parse("content://com.samsung.android.meerkat.CapabilityProvider"),
          value);
      } catch (Exception e) {
        Log.e(TAG, "writeCapability error : " + e);
      }

      OffloadService.stopService(context);
    }
  }
}
