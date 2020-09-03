/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.view.textclassifier;

import android.annotation.FloatRange;
import android.annotation.IntDef;
import android.annotation.IntRange;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.app.PendingIntent;
import android.app.RemoteAction;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.BitmapFactory;
import android.graphics.drawable.AdaptiveIconDrawable;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Icon;
import android.os.Bundle;
import android.os.LocaleList;
import android.os.Parcel;
import android.os.Parcelable;
import android.text.SpannedString;
import android.util.ArrayMap;
import android.view.View.OnClickListener;
import android.view.textclassifier.TextClassifier.EntityType;
import android.view.textclassifier.TextClassifier.Utils;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.util.Preconditions;

import com.google.android.textclassifier.AnnotatorModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.time.ZonedDateTime;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;

/**
 * Information for generating a widget to handle classified text.
 *
 * <p>A TextClassification object contains icons, labels, onClickListeners and intents that may
 * be used to build a widget that can be used to act on classified text. There is the concept of a
 * <i>primary action</i> and other <i>secondary actions</i>.
 *
 * <p>e.g. building a view that, when clicked, shares the classified text with the preferred app:
 *
 * <pre>{@code
 *   // Called preferably outside the UiThread.
 *   TextClassification classification = textClassifier.classifyText(allText, 10, 25);
 *
 *   // Called on the UiThread.
 *   Button button = new Button(context);
 *   button.setCompoundDrawablesWithIntrinsicBounds(classification.getIcon(), null, null, null);
 *   button.setText(classification.getLabel());
 *   button.setOnClickListener(v -> classification.getActions().get(0).getActionIntent().send());
 * }</pre>
 *
 * <p>e.g. starting an action mode with menu items that can handle the classified text:
 *
 * <pre>{@code
 *   // Called preferably outside the UiThread.
 *   final TextClassification classification = textClassifier.classifyText(allText, 10, 25);
 *
 *   // Called on the UiThread.
 *   view.startActionMode(new ActionMode.Callback() {
 *
 *       public boolean onCreateActionMode(ActionMode mode, Menu menu) {
 *           for (int i = 0; i < classification.getActions().size(); ++i) {
 *              RemoteAction action = classification.getActions().get(i);
 *              menu.add(Menu.NONE, i, 20, action.getTitle())
 *                 .setIcon(action.getIcon());
 *           }
 *           return true;
 *       }
 *
 *       public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
 *           classification.getActions().get(item.getItemId()).getActionIntent().send();
 *           return true;
 *       }
 *
 *       ...
 *   });
 * }</pre>
 */
public final class TextClassification implements Parcelable {

    /**
     * @hide
     */
    public static final TextClassification EMPTY = new TextClassification.Builder().build();

    private static final String LOG_TAG = "TextClassification";
    // TODO(toki): investigate a way to derive this based on device properties.
    private static final int MAX_LEGACY_ICON_SIZE = 192;

    @Retention(RetentionPolicy.SOURCE)
    @IntDef(value = {IntentType.UNSUPPORTED, IntentType.ACTIVITY, IntentType.SERVICE})
    private @interface IntentType {
        int UNSUPPORTED = -1;
        int ACTIVITY = 0;
        int SERVICE = 1;
    }

    @NonNull private final String mText;
    @Nullable private final Drawable mLegacyIcon;
    @Nullable private final String mLegacyLabel;
    @Nullable private final Intent mLegacyIntent;
    @Nullable private final OnClickListener mLegacyOnClickListener;
    @NonNull private final List<RemoteAction> mActions;
    @NonNull private final EntityConfidence mEntityConfidence;
    @Nullable private final String mId;
    @NonNull private final Bundle mExtras;

    private TextClassification(
            @Nullable String text,
            @Nullable Drawable legacyIcon,
            @Nullable String legacyLabel,
            @Nullable Intent legacyIntent,
            @Nullable OnClickListener legacyOnClickListener,
            @NonNull List<RemoteAction> actions,
            @NonNull EntityConfidence entityConfidence,
            @Nullable String id,
            @NonNull Bundle extras) {
        mText = text;
        mLegacyIcon = legacyIcon;
        mLegacyLabel = legacyLabel;
        mLegacyIntent = legacyIntent;
        mLegacyOnClickListener = legacyOnClickListener;
        mActions = Collections.unmodifiableList(actions);
        mEntityConfidence = Preconditions.checkNotNull(entityConfidence);
        mId = id;
        mExtras = extras;
    }

    /**
     * Gets the classified text.
     */
    @Nullable
    public String getText() {
        return mText;
    }

    /**
     * Returns the number of entities found in the classified text.
     */
    @IntRange(from = 0)
    public int getEntityCount() {
        return mEntityConfidence.getEntities().size();
    }

    /**
     * Returns the entity at the specified index. Entities are ordered from high confidence
     * to low confidence.
     *
     * @throws IndexOutOfBoundsException if the specified index is out of range.
     * @see #getEntityCount() for the number of entities available.
     */
    @NonNull
    public @EntityType String getEntity(int index) {
        return mEntityConfidence.getEntities().get(index);
    }

    /**
     * Returns the confidence score for the specified entity. The value ranges from
     * 0 (low confidence) to 1 (high confidence). 0 indicates that the entity was not found for the
     * classified text.
     */
    @FloatRange(from = 0.0, to = 1.0)
    public float getConfidenceScore(@EntityType String entity) {
        return mEntityConfidence.getConfidenceScore(entity);
    }

    /**
     * Returns a list of actions that may be performed on the text. The list is ordered based on
     * the likelihood that a user will use the action, with the most likely action appearing first.
     */
    public List<RemoteAction> getActions() {
        return mActions;
    }

    /**
     * Returns an icon that may be rendered on a widget used to act on the classified text.
     *
     * <p><strong>NOTE: </strong>This field is not parcelable and only represents the icon of the
     * first {@link RemoteAction} (if one exists) when this object is read from a parcel.
     *
     * @deprecated Use {@link #getActions()} instead.
     */
    @Deprecated
    @Nullable
    public Drawable getIcon() {
        return mLegacyIcon;
    }

    /**
     * Returns a label that may be rendered on a widget used to act on the classified text.
     *
     * <p><strong>NOTE: </strong>This field is not parcelable and only represents the label of the
     * first {@link RemoteAction} (if one exists) when this object is read from a parcel.
     *
     * @deprecated Use {@link #getActions()} instead.
     */
    @Deprecated
    @Nullable
    public CharSequence getLabel() {
        return mLegacyLabel;
    }

    /**
     * Returns an intent that may be fired to act on the classified text.
     *
     * <p><strong>NOTE: </strong>This field is not parcelled and will always return null when this
     * object is read from a parcel.
     *
     * @deprecated Use {@link #getActions()} instead.
     */
    @Deprecated
    @Nullable
    public Intent getIntent() {
        return mLegacyIntent;
    }

    /**
     * Returns the OnClickListener that may be triggered to act on the classified text.
     *
     * <p><strong>NOTE: </strong>This field is not parcelable and only represents the first
     * {@link RemoteAction} (if one exists) when this object is read from a parcel.
     *
     * @deprecated Use {@link #getActions()} instead.
     */
    @Nullable
    public OnClickListener getOnClickListener() {
        return mLegacyOnClickListener;
    }

    /**
     * Returns the id, if one exists, for this object.
     */
    @Nullable
    public String getId() {
        return mId;
    }

    /**
     * Returns the extended data.
     *
     * <p><b>NOTE: </b>Do not modify this bundle.
     */
    @NonNull
    public Bundle getExtras() {
        return mExtras;
    }

    @Override
    public String toString() {
        return String.format(Locale.US,
                "TextClassification {text=%s, entities=%s, actions=%s, id=%s, extras=%s}",
                mText, mEntityConfidence, mActions, mId, mExtras);
    }

    /**
     * Creates an OnClickListener that triggers the specified PendingIntent.
     *
     * @hide
     */
    public static OnClickListener createIntentOnClickListener(@NonNull final PendingIntent intent) {
        Preconditions.checkNotNull(intent);
        return v -> {
            try {
                intent.send();
            } catch (PendingIntent.CanceledException e) {
                Log.e(LOG_TAG, "Error sending PendingIntent", e);
            }
        };
    }

    /**
     * Creates a PendingIntent for the specified intent.
     * Returns null if the intent is not supported for the specified context.
     *
     * @throws IllegalArgumentException if context or intent is null
     * @hide
     */
    public static PendingIntent createPendingIntent(
            @NonNull final Context context, @NonNull final Intent intent, int requestCode) {
        return PendingIntent.getActivity(
                context, requestCode, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    /**
     * Builder for building {@link TextClassification} objects.
     *
     * <p>e.g.
     *
     * <pre>{@code
     *   TextClassification classification = new TextClassification.Builder()
     *          .setText(classifiedText)
     *          .setEntityType(TextClassifier.TYPE_EMAIL, 0.9)
     *          .setEntityType(TextClassifier.TYPE_OTHER, 0.1)
     *          .addAction(remoteAction1)
     *          .addAction(remoteAction2)
     *          .build();
     * }</pre>
     */
    public static final class Builder {

        @NonNull private List<RemoteAction> mActions = new ArrayList<>();
        @NonNull private final Map<String, Float> mTypeScoreMap = new ArrayMap<>();
        @NonNull
        private final Map<String, AnnotatorModel.ClassificationResult> mClassificationResults =
                new ArrayMap<>();
        @Nullable private String mText;
        @Nullable private Drawable mLegacyIcon;
        @Nullable private String mLegacyLabel;
        @Nullable private Intent mLegacyIntent;
        @Nullable private OnClickListener mLegacyOnClickListener;
        @Nullable private String mId;
        @Nullable private Bundle mExtras;
        @NonNull private final ArrayList<Intent> mActionIntents = new ArrayList<>();
        @Nullable private Bundle mForeignLanguageExtra;

        /**
         * Sets the classified text.
         */
        @NonNull
        public Builder setText(@Nullable String text) {
            mText = text;
            return this;
        }

        /**
         * Sets an entity type for the classification result and assigns a confidence score.
         * If a confidence score had already been set for the specified entity type, this will
         * override that score.
         *
         * @param confidenceScore a value from 0 (low confidence) to 1 (high confidence).
         *      0 implies the entity does not exist for the classified text.
         *      Values greater than 1 are clamped to 1.
         */
        @NonNull
        public Builder setEntityType(
                @NonNull @EntityType String type,
                @FloatRange(from = 0.0, to = 1.0) float confidenceScore) {
            setEntityType(type, confidenceScore, null);
            return this;
        }

        /**
         * @see #setEntityType(String, float)
         *
         * @hide
         */
        @NonNull
        public Builder setEntityType(AnnotatorModel.ClassificationResult classificationResult) {
            setEntityType(
                    classificationResult.getCollection(),
                    classificationResult.getScore(),
                    classificationResult);
            return this;
        }

        /**
         * @see #setEntityType(String, float)
         *
         * @hide
         */
        @NonNull
        private Builder setEntityType(
                @NonNull @EntityType String type,
                @FloatRange(from = 0.0, to = 1.0) float confidenceScore,
                @Nullable AnnotatorModel.ClassificationResult classificationResult) {
            mTypeScoreMap.put(type, confidenceScore);
            mClassificationResults.put(type, classificationResult);
            return this;
        }

        /**
         * Adds an action that may be performed on the classified text. Actions should be added in
         * order of likelihood that the user will use them, with the most likely action being added
         * first.
         */
        @NonNull
        public Builder addAction(@NonNull RemoteAction action) {
            return addAction(action, null);
        }

        /**
         * @param intent the intent in the remote action.
         * @see #addAction(RemoteAction)
         * @hide
         */
        @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
        public Builder addAction(RemoteAction action, @Nullable Intent intent) {
            Preconditions.checkArgument(action != null);
            mActions.add(action);
            mActionIntents.add(intent);
            return this;
        }

        /**
         * Sets the icon for the <i>primary</i> action that may be rendered on a widget used to act
         * on the classified text.
         *
         * <p><strong>NOTE: </strong>This field is not parcelled. If read from a parcel, the
         * returned icon represents the icon of the first {@link RemoteAction} (if one exists).
         *
         * @deprecated Use {@link #addAction(RemoteAction)} instead.
         */
        @Deprecated
        @NonNull
        public Builder setIcon(@Nullable Drawable icon) {
            mLegacyIcon = icon;
            return this;
        }

        /**
         * Sets the label for the <i>primary</i> action that may be rendered on a widget used to
         * act on the classified text.
         *
         * <p><strong>NOTE: </strong>This field is not parcelled. If read from a parcel, the
         * returned label represents the label of the first {@link RemoteAction} (if one exists).
         *
         * @deprecated Use {@link #addAction(RemoteAction)} instead.
         */
        @Deprecated
        @NonNull
        public Builder setLabel(@Nullable String label) {
            mLegacyLabel = label;
            return this;
        }

        /**
         * Sets the intent for the <i>primary</i> action that may be fired to act on the classified
         * text.
         *
         * <p><strong>NOTE: </strong>This field is not parcelled.
         *
         * @deprecated Use {@link #addAction(RemoteAction)} instead.
         */
        @Deprecated
        @NonNull
        public Builder setIntent(@Nullable Intent intent) {
            mLegacyIntent = intent;
            return this;
        }

        /**
         * Sets the OnClickListener for the <i>primary</i> action that may be triggered to act on
         * the classified text.
         *
         * <p><strong>NOTE: </strong>This field is not parcelable. If read from a parcel, the
         * returned OnClickListener represents the first {@link RemoteAction} (if one exists).
         *
         * @deprecated Use {@link #addAction(RemoteAction)} instead.
         */
        @Deprecated
        @NonNull
        public Builder setOnClickListener(@Nullable OnClickListener onClickListener) {
            mLegacyOnClickListener = onClickListener;
            return this;
        }

        /**
         * Sets an id for the TextClassification object.
         */
        @NonNull
        public Builder setId(@Nullable String id) {
            mId = id;
            return this;
        }

        /**
         * Sets the extended data.
         */
        @NonNull
        public Builder setExtras(@Nullable Bundle extras) {
            mExtras = extras;
            return this;
        }

        /**
         * @see #setExtras(Bundle)
         * @hide
         */
        @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
        public Builder setForeignLanguageExtra(@Nullable Bundle extra) {
            mForeignLanguageExtra = extra;
            return this;
        }

        /**
         * Builds and returns a {@link TextClassification} object.
         */
        @NonNull
        public TextClassification build() {
            EntityConfidence entityConfidence = new EntityConfidence(mTypeScoreMap);
            return new TextClassification(mText, mLegacyIcon, mLegacyLabel, mLegacyIntent,
                    mLegacyOnClickListener, mActions, entityConfidence, mId,
                    buildExtras(entityConfidence));
        }

        private Bundle buildExtras(EntityConfidence entityConfidence) {
            final Bundle extras = mExtras == null ? new Bundle() : mExtras;
            if (mActionIntents.stream().anyMatch(Objects::nonNull)) {
                ExtrasUtils.putActionsIntents(extras, mActionIntents);
            }
            if (mForeignLanguageExtra != null) {
                ExtrasUtils.putForeignLanguageExtra(extras, mForeignLanguageExtra);
            }
            List<String> sortedTypes = entityConfidence.getEntities();
            ArrayList<AnnotatorModel.ClassificationResult> sortedEntities = new ArrayList<>();
            for (String type : sortedTypes) {
                sortedEntities.add(mClassificationResults.get(type));
            }
            ExtrasUtils.putEntities(
                    extras, sortedEntities.toArray(new AnnotatorModel.ClassificationResult[0]));
            return extras.isEmpty() ? Bundle.EMPTY : extras;
        }
    }

    /**
     * A request object for generating TextClassification.
     */
    public static final class Request implements Parcelable {

        private final CharSequence mText;
        private final int mStartIndex;
        private final int mEndIndex;
        @Nullable private final LocaleList mDefaultLocales;
        @Nullable private final ZonedDateTime mReferenceTime;
        @NonNull private final Bundle mExtras;
        @Nullable private String mCallingPackageName;

        private Request(
                CharSequence text,
                int startIndex,
                int endIndex,
                LocaleList defaultLocales,
                ZonedDateTime referenceTime,
                Bundle extras) {
            mText = text;
            mStartIndex = startIndex;
            mEndIndex = endIndex;
            mDefaultLocales = defaultLocales;
            mReferenceTime = referenceTime;
            mExtras = extras;
        }

        /**
         * Returns the text providing context for the text to classify (which is specified
         *      by the sub sequence starting at startIndex and ending at endIndex)
         */
        @NonNull
        public CharSequence getText() {
            return mText;
        }

        /**
         * Returns start index of the text to classify.
         */
        @IntRange(from = 0)
        public int getStartIndex() {
            return mStartIndex;
        }

        /**
         * Returns end index of the text to classify.
         */
        @IntRange(from = 0)
        public int getEndIndex() {
            return mEndIndex;
        }

        /**
         * @return ordered list of locale preferences that can be used to disambiguate
         *      the provided text.
         */
        @Nullable
        public LocaleList getDefaultLocales() {
            return mDefaultLocales;
        }

        /**
         * @return reference time based on which relative dates (e.g. "tomorrow") should be
         *      interpreted.
         */
        @Nullable
        public ZonedDateTime getReferenceTime() {
            return mReferenceTime;
        }

        /**
         * Sets the name of the package that is sending this request.
         * <p>
         * For SystemTextClassifier's use.
         * @hide
         */
        @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
        public void setCallingPackageName(@Nullable String callingPackageName) {
            mCallingPackageName = callingPackageName;
        }

        /**
         * Returns the name of the package that sent this request.
         * This returns {@code null} if no calling package name is set.
         */
        @Nullable
        public String getCallingPackageName() {
            return mCallingPackageName;
        }

        /**
         * Returns the extended data.
         *
         * <p><b>NOTE: </b>Do not modify this bundle.
         */
        @NonNull
        public Bundle getExtras() {
            return mExtras;
        }

        /**
         * A builder for building TextClassification requests.
         */
        public static final class Builder {

            private final CharSequence mText;
            private final int mStartIndex;
            private final int mEndIndex;
            private Bundle mExtras;

            @Nullable private LocaleList mDefaultLocales;
            @Nullable private ZonedDateTime mReferenceTime;

            /**
             * @param text text providing context for the text to classify (which is specified
             *      by the sub sequence starting at startIndex and ending at endIndex)
             * @param startIndex start index of the text to classify
             * @param endIndex end index of the text to classify
             */
            public Builder(
                    @NonNull CharSequence text,
                    @IntRange(from = 0) int startIndex,
                    @IntRange(from = 0) int endIndex) {
                Utils.checkArgument(text, startIndex, endIndex);
                mText = text;
                mStartIndex = startIndex;
                mEndIndex = endIndex;
            }

            /**
             * @param defaultLocales ordered list of locale preferences that may be used to
             *      disambiguate the provided text. If no locale preferences exist, set this to null
             *      or an empty locale list.
             *
             * @return this builder
             */
            @NonNull
            public Builder setDefaultLocales(@Nullable LocaleList defaultLocales) {
                mDefaultLocales = defaultLocales;
                return this;
            }

            /**
             * @param referenceTime reference time based on which relative dates (e.g. "tomorrow"
             *      should be interpreted. This should usually be the time when the text was
             *      originally composed. If no reference time is set, now is used.
             *
             * @return this builder
             */
            @NonNull
            public Builder setReferenceTime(@Nullable ZonedDateTime referenceTime) {
                mReferenceTime = referenceTime;
                return this;
            }

            /**
             * Sets the extended data.
             *
             * @return this builder
             */
            @NonNull
            public Builder setExtras(@Nullable Bundle extras) {
                mExtras = extras;
                return this;
            }

            /**
             * Builds and returns the request object.
             */
            @NonNull
            public Request build() {
                return new Request(new SpannedString(mText), mStartIndex, mEndIndex,
                        mDefaultLocales, mReferenceTime,
                        mExtras == null ? Bundle.EMPTY : mExtras);
            }
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeCharSequence(mText);
            dest.writeInt(mStartIndex);
            dest.writeInt(mEndIndex);
            dest.writeParcelable(mDefaultLocales, flags);
            dest.writeString(mReferenceTime == null ? null : mReferenceTime.toString());
            dest.writeString(mCallingPackageName);
            dest.writeBundle(mExtras);
        }

        private static Request readFromParcel(Parcel in) {
            final CharSequence text = in.readCharSequence();
            final int startIndex = in.readInt();
            final int endIndex = in.readInt();
            final LocaleList defaultLocales = in.readParcelable(null);
            final String referenceTimeString = in.readString();
            final ZonedDateTime referenceTime = referenceTimeString == null
                    ? null : ZonedDateTime.parse(referenceTimeString);
            final String callingPackageName = in.readString();
            final Bundle extras = in.readBundle();

            final Request request = new Request(text, startIndex, endIndex,
                    defaultLocales, referenceTime, extras);
            request.setCallingPackageName(callingPackageName);
            return request;
        }

        public static final @android.annotation.NonNull Parcelable.Creator<Request> CREATOR =
                new Parcelable.Creator<Request>() {
                    @Override
                    public Request createFromParcel(Parcel in) {
                        return readFromParcel(in);
                    }

                    @Override
                    public Request[] newArray(int size) {
                        return new Request[size];
                    }
                };
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeString(mText);
        // NOTE: legacy fields are not parcelled.
        dest.writeTypedList(mActions);
        mEntityConfidence.writeToParcel(dest, flags);
        dest.writeString(mId);
        dest.writeBundle(mExtras);
    }

    public static final @android.annotation.NonNull Parcelable.Creator<TextClassification> CREATOR =
            new Parcelable.Creator<TextClassification>() {
                @Override
                public TextClassification createFromParcel(Parcel in) {
                    return new TextClassification(in);
                }

                @Override
                public TextClassification[] newArray(int size) {
                    return new TextClassification[size];
                }
            };

    private TextClassification(Parcel in) {
        mText = in.readString();
        mActions = in.createTypedArrayList(RemoteAction.CREATOR);
        if (!mActions.isEmpty()) {
            final RemoteAction action = mActions.get(0);
            mLegacyIcon = maybeLoadDrawable(action.getIcon());
            mLegacyLabel = action.getTitle().toString();
            mLegacyOnClickListener = createIntentOnClickListener(mActions.get(0).getActionIntent());
        } else {
            mLegacyIcon = null;
            mLegacyLabel = null;
            mLegacyOnClickListener = null;
        }
        mLegacyIntent = null; // mLegacyIntent is not parcelled.
        mEntityConfidence = EntityConfidence.CREATOR.createFromParcel(in);
        mId = in.readString();
        mExtras = in.readBundle();
    }

    // Best effort attempt to try to load a drawable from the provided icon.
    @Nullable
    private static Drawable maybeLoadDrawable(Icon icon) {
        if (icon == null) {
            return null;
        }
        switch (icon.getType()) {
            case Icon.TYPE_BITMAP:
                return new BitmapDrawable(Resources.getSystem(), icon.getBitmap());
            case Icon.TYPE_ADAPTIVE_BITMAP:
                return new AdaptiveIconDrawable(null,
                        new BitmapDrawable(Resources.getSystem(), icon.getBitmap()));
            case Icon.TYPE_DATA:
                return new BitmapDrawable(
                        Resources.getSystem(),
                        BitmapFactory.decodeByteArray(
                                icon.getDataBytes(), icon.getDataOffset(), icon.getDataLength()));
        }
        return null;
    }
}
