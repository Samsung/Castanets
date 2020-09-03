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
package android.multiuser;

import android.app.ActivityManager;
import android.app.IActivityManager;
import android.app.IStopUserCallback;
import android.app.UserSwitchObserver;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.UserInfo;
import android.os.RemoteException;
import android.os.UserHandle;
import android.os.UserManager;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Perf tests for user life cycle events.
 *
 * Running the tests:
 *
 * make MultiUserPerfTests &&
 * adb install -r \
 *     ${ANDROID_PRODUCT_OUT}/data/app/MultiUserPerfTests/MultiUserPerfTests.apk &&
 * adb shell am instrument -e class android.multiuser.UserLifecycleTests \
 *     -w com.android.perftests.multiuser/androidx.test.runner.AndroidJUnitRunner
 *
 * or
 *
 * bit MultiUserPerfTests:android.multiuser.UserLifecycleTests
 *
 * Note: If you use bit for running the tests, benchmark results won't be printed on the host side.
 * But in either case, results can be checked on the device side 'adb logcat -s UserLifecycleTests'
 */
@LargeTest
@RunWith(AndroidJUnit4.class)
public class UserLifecycleTests {
    private static final String TAG = UserLifecycleTests.class.getSimpleName();

    private final int TIMEOUT_IN_SECOND = 30;
    private final int CHECK_USER_REMOVED_INTERVAL_MS = 200;

    private UserManager mUm;
    private ActivityManager mAm;
    private IActivityManager mIam;
    private ArrayList<Integer> mUsersToRemove;

    private final BenchmarkRunner mRunner = new BenchmarkRunner();
    @Rule
    public BenchmarkResultsReporter mReporter = new BenchmarkResultsReporter(mRunner);

    @Before
    public void setUp() {
        final Context context = InstrumentationRegistry.getContext();
        mUm = UserManager.get(context);
        mAm = context.getSystemService(ActivityManager.class);
        mIam = ActivityManager.getService();
        mUsersToRemove = new ArrayList<>();
    }

    @After
    public void tearDown() {
        for (int userId : mUsersToRemove) {
            try {
                mUm.removeUser(userId);
            } catch (Exception e) {
                // Ignore
            }
        }
    }

    @Test
    public void createAndStartUser() throws Exception {
        while (mRunner.keepRunning()) {
            final UserInfo userInfo = mUm.createUser("TestUser", 0);

            final CountDownLatch latch = new CountDownLatch(1);
            registerBroadcastReceiver(Intent.ACTION_USER_STARTED, latch, userInfo.id);
            mIam.startUserInBackground(userInfo.id);
            latch.await(TIMEOUT_IN_SECOND, TimeUnit.SECONDS);

            mRunner.pauseTiming();
            removeUser(userInfo.id);
            mRunner.resumeTiming();
        }
    }

    @Test
    public void switchUser() throws Exception {
        while (mRunner.keepRunning()) {
            mRunner.pauseTiming();
            final int startUser = mAm.getCurrentUser();
            final UserInfo userInfo = mUm.createUser("TestUser", 0);
            mRunner.resumeTiming();

            switchUser(userInfo.id);

            mRunner.pauseTiming();
            switchUser(startUser);
            removeUser(userInfo.id);
            mRunner.resumeTiming();
        }
    }

    /** Tests switching to an already-created, but no-longer-running, user. */
    @Test
    public void switchUser_stopped() throws Exception {
        while (mRunner.keepRunning()) {
            mRunner.pauseTiming();
            final int startUser = mAm.getCurrentUser();
            final int testUser = initializeNewUserAndSwitchBack(/* stopNewUser */ true);
            final CountDownLatch latch = new CountDownLatch(1);
            registerBroadcastReceiver(Intent.ACTION_USER_UNLOCKED, latch, testUser);
            mRunner.resumeTiming();

            mAm.switchUser(testUser);
            boolean success = latch.await(TIMEOUT_IN_SECOND, TimeUnit.SECONDS);

            mRunner.pauseTiming();
            attestTrue("Failed to achieve 2nd ACTION_USER_UNLOCKED for user " + testUser, success);
            switchUser(startUser);
            removeUser(testUser);
            mRunner.resumeTiming();
        }
    }

    /** Tests switching to an already-created already-running non-owner user. */
    @Test
    public void switchUser_running() throws Exception {
        while (mRunner.keepRunning()) {
            mRunner.pauseTiming();
            final int startUser = mAm.getCurrentUser();
            final int testUser = initializeNewUserAndSwitchBack(/* stopNewUser */ false);
            mRunner.resumeTiming();

            switchUser(testUser);

            mRunner.pauseTiming();
            attestTrue("Failed to switch to user " + testUser, mAm.isUserRunning(testUser));
            switchUser(startUser);
            removeUser(testUser);
            mRunner.resumeTiming();
        }
    }

    @Test
    public void stopUser() throws Exception {
        while (mRunner.keepRunning()) {
            mRunner.pauseTiming();
            final UserInfo userInfo = mUm.createUser("TestUser", 0);
            final CountDownLatch latch = new CountDownLatch(1);
            registerBroadcastReceiver(Intent.ACTION_USER_STARTED, latch, userInfo.id);
            mIam.startUserInBackground(userInfo.id);
            latch.await(TIMEOUT_IN_SECOND, TimeUnit.SECONDS);
            mRunner.resumeTiming();

            stopUser(userInfo.id, false);

            mRunner.pauseTiming();
            removeUser(userInfo.id);
            mRunner.resumeTiming();
        }
    }

    @Test
    public void lockedBootCompleted() throws Exception {
        while (mRunner.keepRunning()) {
            mRunner.pauseTiming();
            final int startUser = mAm.getCurrentUser();
            final UserInfo userInfo = mUm.createUser("TestUser", 0);
            final CountDownLatch latch = new CountDownLatch(1);
            registerUserSwitchObserver(null, latch, userInfo.id);
            mRunner.resumeTiming();

            mAm.switchUser(userInfo.id);
            latch.await(TIMEOUT_IN_SECOND, TimeUnit.SECONDS);

            mRunner.pauseTiming();
            switchUser(startUser);
            removeUser(userInfo.id);
            mRunner.resumeTiming();
        }
    }

    @Test
    public void managedProfileUnlock() throws Exception {
        while (mRunner.keepRunning()) {
            mRunner.pauseTiming();
            final UserInfo userInfo = mUm.createProfileForUser("TestUser",
                    UserInfo.FLAG_MANAGED_PROFILE, mAm.getCurrentUser());
            final CountDownLatch latch = new CountDownLatch(1);
            registerBroadcastReceiver(Intent.ACTION_USER_UNLOCKED, latch, userInfo.id);
            mRunner.resumeTiming();

            mIam.startUserInBackground(userInfo.id);
            latch.await(TIMEOUT_IN_SECOND, TimeUnit.SECONDS);

            mRunner.pauseTiming();
            removeUser(userInfo.id);
            mRunner.resumeTiming();
        }
    }

    /** Tests starting an already-created, but no-longer-running, profile. */
    @Test
    public void managedProfileUnlock_stopped() throws Exception {
        while (mRunner.keepRunning()) {
            mRunner.pauseTiming();
            final UserInfo userInfo = mUm.createProfileForUser("TestUser",
                    UserInfo.FLAG_MANAGED_PROFILE, mAm.getCurrentUser());
            // Start the profile initially, then stop it. Similar to setQuietModeEnabled.
            final CountDownLatch latch1 = new CountDownLatch(1);
            registerBroadcastReceiver(Intent.ACTION_USER_UNLOCKED, latch1, userInfo.id);
            mIam.startUserInBackground(userInfo.id);
            latch1.await(TIMEOUT_IN_SECOND, TimeUnit.SECONDS);
            stopUser(userInfo.id, true);

            // Now we restart the profile.
            final CountDownLatch latch2 = new CountDownLatch(1);
            registerBroadcastReceiver(Intent.ACTION_USER_UNLOCKED, latch2, userInfo.id);
            mRunner.resumeTiming();

            mIam.startUserInBackground(userInfo.id);
            latch2.await(TIMEOUT_IN_SECOND, TimeUnit.SECONDS);

            mRunner.pauseTiming();
            removeUser(userInfo.id);
            mRunner.resumeTiming();
        }
    }

    @Test
    public void ephemeralUserStopped() throws Exception {
        while (mRunner.keepRunning()) {
            mRunner.pauseTiming();
            final int startUser = mAm.getCurrentUser();
            final UserInfo userInfo = mUm.createUser("TestUser",
                    UserInfo.FLAG_EPHEMERAL | UserInfo.FLAG_DEMO);
            switchUser(userInfo.id);
            final CountDownLatch latch = new CountDownLatch(1);
            InstrumentationRegistry.getContext().registerReceiver(new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    if (Intent.ACTION_USER_STOPPED.equals(intent.getAction()) && intent.getIntExtra(
                            Intent.EXTRA_USER_HANDLE, UserHandle.USER_NULL) == userInfo.id) {
                        latch.countDown();
                    }
                }
            }, new IntentFilter(Intent.ACTION_USER_STOPPED));
            final CountDownLatch switchLatch = new CountDownLatch(1);
            registerUserSwitchObserver(switchLatch, null, startUser);
            mRunner.resumeTiming();

            mAm.switchUser(startUser);
            latch.await(TIMEOUT_IN_SECOND, TimeUnit.SECONDS);

            mRunner.pauseTiming();
            switchLatch.await(TIMEOUT_IN_SECOND, TimeUnit.SECONDS);
            removeUser(userInfo.id);
            mRunner.resumeTiming();
        }
    }

    @Test
    public void managedProfileStopped() throws Exception {
        while (mRunner.keepRunning()) {
            mRunner.pauseTiming();
            final UserInfo userInfo = mUm.createProfileForUser("TestUser",
                    UserInfo.FLAG_MANAGED_PROFILE, mAm.getCurrentUser());
            final CountDownLatch latch = new CountDownLatch(1);
            registerBroadcastReceiver(Intent.ACTION_USER_UNLOCKED, latch, userInfo.id);
            mIam.startUserInBackground(userInfo.id);
            latch.await(TIMEOUT_IN_SECOND, TimeUnit.SECONDS);
            mRunner.resumeTiming();

            stopUser(userInfo.id, true);

            mRunner.pauseTiming();
            removeUser(userInfo.id);
            mRunner.resumeTiming();
        }
    }

    private void switchUser(int userId) throws Exception {
        final CountDownLatch latch = new CountDownLatch(1);
        registerUserSwitchObserver(latch, null, userId);
        mAm.switchUser(userId);
        latch.await(TIMEOUT_IN_SECOND, TimeUnit.SECONDS);
    }

    private void stopUser(int userId, boolean force) throws Exception {
        final CountDownLatch latch = new CountDownLatch(1);
        mIam.stopUser(userId, force /* force */, new IStopUserCallback.Stub() {
            @Override
            public void userStopped(int userId) throws RemoteException {
                latch.countDown();
            }

            @Override
            public void userStopAborted(int userId) throws RemoteException {
            }
        });
        latch.await(TIMEOUT_IN_SECOND, TimeUnit.SECONDS);
    }

    /**
     * Creates a user and waits for its ACTION_USER_UNLOCKED.
     * Then switches to back to the original user and waits for its switchUser() to finish.
     *
     * @param stopNewUser whether to stop the new user after switching to otherUser.
     * @return userId of the newly created user.
     */
    private int initializeNewUserAndSwitchBack(boolean stopNewUser) throws Exception {
        final int origUser = mAm.getCurrentUser();
        // First, create and switch to testUser, waiting for its ACTION_USER_UNLOCKED
        final int testUser = mUm.createUser("TestUser", 0).id;
        final CountDownLatch latch1 = new CountDownLatch(1);
        registerBroadcastReceiver(Intent.ACTION_USER_UNLOCKED, latch1, testUser);
        mAm.switchUser(testUser);
        attestTrue("Failed to achieve initial ACTION_USER_UNLOCKED for user " + testUser,
                latch1.await(TIMEOUT_IN_SECOND, TimeUnit.SECONDS));

        // Second, switch back to origUser, waiting merely for switchUser() to finish
        switchUser(origUser);
        attestTrue("Didn't switch back to user, " + origUser, origUser == mAm.getCurrentUser());

        if (stopNewUser) {
            stopUser(testUser, true);
            attestFalse("Failed to stop user " + testUser, mAm.isUserRunning(testUser));
        }

        return testUser;
    }

    private void registerUserSwitchObserver(final CountDownLatch switchLatch,
            final CountDownLatch bootCompleteLatch, final int userId) throws Exception {
        ActivityManager.getService().registerUserSwitchObserver(
                new UserSwitchObserver() {
                    @Override
                    public void onUserSwitchComplete(int newUserId) throws RemoteException {
                        if (switchLatch != null && userId == newUserId) {
                            switchLatch.countDown();
                        }
                    }

                    @Override
                    public void onLockedBootComplete(int newUserId) {
                        if (bootCompleteLatch != null && userId == newUserId) {
                            bootCompleteLatch.countDown();
                        }
                    }
                }, TAG);
    }

    private void registerBroadcastReceiver(final String action, final CountDownLatch latch,
            final int userId) {
        InstrumentationRegistry.getContext().registerReceiverAsUser(new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if (action.equals(intent.getAction()) && intent.getIntExtra(
                        Intent.EXTRA_USER_HANDLE, UserHandle.USER_NULL) == userId) {
                    latch.countDown();
                }
            }
        }, UserHandle.of(userId), new IntentFilter(action), null, null);
    }

    private void removeUser(int userId) {
        try {
            mUm.removeUser(userId);
            final long startTime = System.currentTimeMillis();
            final long timeoutInMs = TIMEOUT_IN_SECOND * 1000;
            while (mUm.getUserInfo(userId) != null &&
                    System.currentTimeMillis() - startTime < timeoutInMs) {
                TimeUnit.MILLISECONDS.sleep(CHECK_USER_REMOVED_INTERVAL_MS);
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        } catch (Exception e) {
            // Ignore
        }
        if (mUm.getUserInfo(userId) != null) {
            mUsersToRemove.add(userId);
        }
    }

    private void attestTrue(String message, boolean attestion) {
        if (!attestion) {
            Log.w(TAG, message);
        }
    }

    private void attestFalse(String message, boolean attestion) {
        attestTrue(message, !attestion);
    }
}
