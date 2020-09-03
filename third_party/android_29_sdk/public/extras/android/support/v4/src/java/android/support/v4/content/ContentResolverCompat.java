/*
 * Copyright (C) 2013 The Android Open Source Project
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

package android.support.v4.content;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.support.v4.os.CancellationSignal;
import android.support.v4.os.OperationCanceledException;

/**
 * Helper for accessing features in {@link android.content.ContentResolver}
 * introduced after API level 4 in a backwards compatible fashion.
 */
public final class ContentResolverCompat {
    interface ContentResolverCompatImpl {
        Cursor query(ContentResolver resolver,
                Uri uri, String[] projection, String selection, String[] selectionArgs,
                String sortOrder, CancellationSignal cancellationSignal);
    }

    static class ContentResolverCompatImplBase implements ContentResolverCompatImpl {
        @Override
        public Cursor query(ContentResolver resolver, Uri uri, String[] projection,
                String selection, String[] selectionArgs, String sortOrder,
                CancellationSignal cancellationSignal) {
            // Note that the cancellation signal cannot cancel the query in progress
            // prior to Jellybean so we cancel it preemptively here if needed.
            if (cancellationSignal != null) {
                cancellationSignal.throwIfCanceled();
            }
            return resolver.query(uri, projection, selection, selectionArgs, sortOrder);
        }
    }

    static class ContentResolverCompatImplJB extends ContentResolverCompatImplBase {
        @Override
        public Cursor query(ContentResolver resolver, Uri uri, String[] projection,
                String selection, String[] selectionArgs, String sortOrder,
                CancellationSignal cancellationSignal) {
            try {
                return ContentResolverCompatJellybean.query(resolver,
                        uri, projection, selection, selectionArgs, sortOrder,
                        cancellationSignal != null ?
                                cancellationSignal.getCancellationSignalObject() : null);
            } catch (Exception e) {
                if (ContentResolverCompatJellybean.isFrameworkOperationCanceledException(e)) {
                    // query() can throw a framework OperationCanceledException if it has been
                    // canceled. We catch that and throw the support version instead.
                    throw new OperationCanceledException();
                } else {
                    // If it's not a framework OperationCanceledException, re-throw the exception
                    throw e;
                }
            }
        }
    }

    private static final ContentResolverCompatImpl IMPL;
    static {
        final int version = Build.VERSION.SDK_INT;
        if (version >= 16) {
            IMPL = new ContentResolverCompatImplJB();
        } else {
            IMPL = new ContentResolverCompatImplBase();
        }
    }

    private ContentResolverCompat() {
        /* Hide constructor */
    }

    /**
     * Query the given URI, returning a {@link Cursor} over the result set
     * with optional support for cancellation.
     * <p>
     * For best performance, the caller should follow these guidelines:
     * <ul>
     * <li>Provide an explicit projection, to prevent
     * reading data from storage that aren't going to be used.</li>
     * <li>Use question mark parameter markers such as 'phone=?' instead of
     * explicit values in the {@code selection} parameter, so that queries
     * that differ only by those values will be recognized as the same
     * for caching purposes.</li>
     * </ul>
     * </p>
     *
     * @param uri The URI, using the content:// scheme, for the content to
     *         retrieve.
     * @param projection A list of which columns to return. Passing null will
     *         return all columns, which is inefficient.
     * @param selection A filter declaring which rows to return, formatted as an
     *         SQL WHERE clause (excluding the WHERE itself). Passing null will
     *         return all rows for the given URI.
     * @param selectionArgs You may include ?s in selection, which will be
     *         replaced by the values from selectionArgs, in the order that they
     *         appear in the selection. The values will be bound as Strings.
     * @param sortOrder How to order the rows, formatted as an SQL ORDER BY
     *         clause (excluding the ORDER BY itself). Passing null will use the
     *         default sort order, which may be unordered.
     * @param cancellationSignal A signal to cancel the operation in progress, or null if none.
     * If the operation is canceled, then {@link OperationCanceledException} will be thrown
     * when the query is executed.
     * @return A Cursor object, which is positioned before the first entry, or null
     * @see Cursor
     */
    public static Cursor query(ContentResolver resolver,
            Uri uri, String[] projection, String selection, String[] selectionArgs,
            String sortOrder, CancellationSignal cancellationSignal) {
        return IMPL.query(resolver, uri, projection, selection, selectionArgs,
                sortOrder, cancellationSignal);
    }
}
