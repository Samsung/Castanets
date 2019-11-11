/*
 * Copyright 2019 Samsung Electronics Co., Ltd
 *
 * Licensed under the Flora License, Version 1.1 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://floralicense.org/license/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.samsung.android.meerkat;

import android.content.Intent;
import android.os.Bundle;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import com.google.android.gms.auth.api.signin.GoogleSignIn;
import com.google.android.gms.auth.api.signin.GoogleSignInAccount;
import com.google.android.gms.auth.api.signin.GoogleSignInClient;
import com.google.android.gms.auth.api.signin.GoogleSignInOptions;
import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.Task;

public class MeerkatSignInActivity extends AppCompatActivity implements
        View.OnClickListener {
    private static final String TAG = "MeerkatSignInActivity";
    private static final int RC_GET_TOKEN = 9002;

    private GoogleSignInClient mGoogleSignInClient;
    private TextView mDetailTextView;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.signin_activity);

        mDetailTextView = findViewById(R.id.detail);

        findViewById(R.id.sign_in_button).setOnClickListener(this);
        findViewById(R.id.sign_out_button).setOnClickListener(this);
        findViewById(R.id.start_button).setOnClickListener(this);

        GoogleSignInOptions gso = new GoogleSignInOptions.Builder(GoogleSignInOptions.DEFAULT_SIGN_IN)
                .requestIdToken("401503586848-3ajf0semvlclbffipcuh7oc7qr2kattk.apps.googleusercontent.com")
                .requestEmail()
                .build();
        mGoogleSignInClient = GoogleSignIn.getClient(this, gso);
    }

    @Override
    public void onStart() {
        super.onStart();
        GoogleSignInAccount account = GoogleSignIn.getLastSignedInAccount(this);
        updateUI(account);
    }

    private void signIn() {
        Intent signInIntent = mGoogleSignInClient.getSignInIntent();
        startActivityForResult(signInIntent, RC_GET_TOKEN);
    }

    private void signOut() {
        mGoogleSignInClient.signOut()
                .addOnCompleteListener(this, new OnCompleteListener<Void>() {
                    @Override
                    public void onComplete(Task<Void> task) {
                        updateUI(null);
                    }
                });
    }

    private void startService() {
        ContextCompat.startForegroundService(this, new Intent(this, MeerkatServerService.class));
        finish();
    }

    private void handleSignInResult(Task<GoogleSignInAccount> completedTask) {
        try {
            GoogleSignInAccount account = completedTask.getResult(ApiException.class);
            updateUI(account);
        } catch (ApiException e) {
          Log.w(TAG, "handleSignInResult:failed code=" + e.getStatusCode());
          Toast.makeText(this, "Sign-in failed. Please try again.", Toast.LENGTH_LONG).show();
        }
    }

    private void updateUI(GoogleSignInAccount account) {
        if (account != null) {
            mDetailTextView.setText("Hello, " + account.getDisplayName() +
                    "! Please press START button to start Meerkat.");
            findViewById(R.id.sign_in_button).setVisibility(View.GONE);
            findViewById(R.id.start_and_sign_out).setVisibility(View.VISIBLE);
        } else {
            mDetailTextView.setText("Please sign in to run Meerkat.");
            findViewById(R.id.sign_in_button).setVisibility(View.VISIBLE);
            findViewById(R.id.start_and_sign_out).setVisibility(View.GONE);
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (requestCode == RC_GET_TOKEN) {
            Task<GoogleSignInAccount> task = GoogleSignIn.getSignedInAccountFromIntent(data);
            handleSignInResult(task);
        }
    }

    @Override
    public void onClick(View v) {
        int id = v.getId();
        if (id == R.id.sign_in_button) {
            signIn();
        } else if (id == R.id.sign_out_button) {
            signOut();
        } else if (id == R.id.start_button) {
            startService();
        }
    }
}
