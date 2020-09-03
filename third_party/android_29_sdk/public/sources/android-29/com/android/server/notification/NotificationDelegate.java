/**
 * Copyright (c) 2013, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.server.notification;

import android.app.Notification;
import android.service.notification.NotificationStats;

import com.android.internal.statusbar.NotificationVisibility;

public interface NotificationDelegate {
    void onSetDisabled(int status);
    void onClearAll(int callingUid, int callingPid, int userId);
    void onNotificationClick(int callingUid, int callingPid, String key,
            NotificationVisibility nv);
    void onNotificationActionClick(int callingUid, int callingPid, String key, int actionIndex,
            Notification.Action action, NotificationVisibility nv, boolean generatedByAssistant);
    void onNotificationClear(int callingUid, int callingPid,
            String pkg, String tag, int id, int userId, String key,
            @NotificationStats.DismissalSurface int dismissalSurface,
            @NotificationStats.DismissalSentiment int dismissalSentiment,
            NotificationVisibility nv);
    void onNotificationError(int callingUid, int callingPid,
            String pkg, String tag, int id,
            int uid, int initialPid, String message, int userId);
    void onPanelRevealed(boolean clearEffects, int numItems);
    void onPanelHidden();
    void clearEffects();
    void onNotificationVisibilityChanged(
            NotificationVisibility[] newlyVisibleKeys,
            NotificationVisibility[] noLongerVisibleKeys);
    void onNotificationExpansionChanged(String key, boolean userAction, boolean expanded,
            int notificationLocation);
    void onNotificationDirectReplied(String key);
    void onNotificationSettingsViewed(String key);
    void onNotificationBubbleChanged(String key, boolean isBubble);

    /**
     * Notifies that smart replies and actions have been added to the UI.
     */
    void onNotificationSmartSuggestionsAdded(String key, int smartReplyCount, int smartActionCount,
            boolean generatedByAssistant, boolean editBeforeSending);

    /**
     * Notifies a smart reply is sent.
     *
     * @param key the notification key
     * @param clickedIndex the index of clicked reply
     * @param reply the reply that is sent
     * @param notificationLocation the location of the notification containing the smart reply
     * @param modifiedBeforeSending whether the user changed the smart reply before sending
     */
    void onNotificationSmartReplySent(String key, int clickedIndex, CharSequence reply,
            int notificationLocation, boolean modifiedBeforeSending);
}
