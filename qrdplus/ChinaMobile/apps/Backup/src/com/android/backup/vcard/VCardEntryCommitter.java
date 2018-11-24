/*
 * Copyright (c) 2013 Qualcomm Technologies, Inc.  All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 *
 * Not a Contribution, Apache license notifications and license are retained
 * for attribution purposes only.
 */
/*
 * Copyright (C) 2009 The Android Open Source Project
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

package com.android.backup.vcard;

import android.content.ContentResolver;
import android.net.Uri;
import android.util.Log;

import java.util.ArrayList;
import com.android.backup.ContactsContract.RawContacts;
import android.database.Cursor;

/**
 * <P>
 * {@link VCardEntryHandler} implementation which commits the entry to
 * ContentResolver.
 * </P>
 * <P>
 * Note:<BR />
 * Each vCard may contain big photo images encoded by BASE64, If we store all
 * vCard entries in memory, OutOfMemoryError may be thrown. Thus, this class
 * push each VCard entry into ContentResolver immediately.
 * </P>
 */
public class VCardEntryCommitter implements VCardEntryHandler {
    public static String LOG_TAG = "VCardEntryComitter";

    private final ContentResolver mContentResolver;
    private long mTimeToCommit;
    private ArrayList<Uri> mCreatedUris = new ArrayList<Uri>();
    private boolean mIsPim = false;
    private boolean mIsUpdate = false;
    private int nlocation = 0;

    private int index = 0;
    private int freeCount = 0;

    private int getFreecount() {
        String type = "com.android.localphone";
        int count = 0;
        Cursor queryCursor = mContentResolver.query(RawContacts.CONTENT_URI, new String[] {
            RawContacts._ID
        },
                RawContacts.ACCOUNT_NAME + " = '" + type + "' AND " + RawContacts.DELETED + " = 0",
                null, null);
        if (queryCursor != null) {
            try {
                count = queryCursor.getCount();
            } finally {
                queryCursor.close();
            }
        }
        android.util.Log.d(LOG_TAG, "count = " + count);
        return (2000 - count);
    }

    public VCardEntryCommitter(ContentResolver resolver) {
        mContentResolver = resolver;
        mIsUpdate = false;
        mIsPim = false;
        nlocation = 0;
        freeCount = getFreecount();
    }

    public VCardEntryCommitter(ContentResolver resolver, boolean isUpdate, int nlocation,
            boolean ispim) {
        mContentResolver = resolver;
        mIsUpdate = isUpdate;
        mIsPim = ispim;
        this.nlocation = nlocation;
        freeCount = getFreecount();
    }

    public void onStart() {
    }

    public void onEnd() {
        if (VCardConfig.showPerformanceLog()) {
            Log.d(LOG_TAG, String.format("time to commit entries: %d ms", mTimeToCommit));
        }
        com.android.backup.vcard.VCardEntry.applyBatchForPim(mContentResolver);
    }

    public void onEntryCreated(final VCardEntry contactStruct) {
        long start = System.currentTimeMillis();
        // index ++;
        Log.d(LOG_TAG, "OnEntryCreated:index = " + index + " freeCount =" + freeCount);
        // if(index <= freeCount){
        mCreatedUris.add(contactStruct.insertToPhone(mContentResolver, 1));
        // }
        mTimeToCommit += System.currentTimeMillis() - start;
    }

    /**
     * Returns the list of created Uris. This list should not be modified by the
     * caller as it is not a clone.
     */
    public ArrayList<Uri> getCreatedUris() {
        return mCreatedUris;
    }
}
