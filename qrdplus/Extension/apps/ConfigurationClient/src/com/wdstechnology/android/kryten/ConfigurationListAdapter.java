/**
 * Copyright (c) 2013 Qualcomm Technologies, Inc.  All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 *
 * Not a Contribution, Apache license notifications and license are retained
 * for attribution purposes only.
 *
 * Copyright (C) 2007 The Android Open Source Project
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

package com.wdstechnology.android.kryten;

import android.content.Context;
import android.database.Cursor;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AbsListView;
import android.widget.CursorAdapter;

/**
 * The back-end data adapter for ConfigurationList.
 */
// TODO: This should be public class ConversationListAdapter extends
// ArrayAdapter<Conversation>
public class ConfigurationListAdapter extends CursorAdapter implements AbsListView.RecyclerListener {
    private static final String TAG = "ConfigurationListAdapter";
    private static final boolean LOCAL_LOGV = false;

    private final LayoutInflater mFactory;
    private OnContentChangedListener mOnContentChangedListener;

    public ConfigurationListAdapter(Context context, Cursor cursor) {
        super(context, cursor, false /* auto-requery */);
        mFactory = LayoutInflater.from(context);
    }

    @Override
    public void bindView(View view, Context context, Cursor cursor) {
        if (!(view instanceof ConfigurationListItem)) {
            Log.e(TAG, "Unexpected bound view: " + view);
            return;
        }

        ConfigurationListItem headerView = (ConfigurationListItem) view;
        // Conversation conv = Conversation.from(context, cursor);
        headerView.bind(context, cursor);
    }

    public void onMovedToScrapHeap(View view) {
        ConfigurationListItem headerView = (ConfigurationListItem) view;
        // headerView.unbind();
    }

    @Override
    public View newView(Context context, Cursor cursor, ViewGroup parent) {
        if (LOCAL_LOGV)
            Log.v(TAG, "inflating new view");
        return mFactory.inflate(R.layout.conversation_list_item, parent, false);
    }

    public interface OnContentChangedListener {
        void onContentChanged(ConfigurationListAdapter adapter);
    }

    public void setOnContentChangedListener(OnContentChangedListener l) {
        mOnContentChangedListener = l;
    }

    @Override
    protected void onContentChanged() {
        if (mCursor != null && !mCursor.isClosed()) {
            if (mOnContentChangedListener != null) {
                mOnContentChangedListener.onContentChanged(this);
            }
        }
    }
}
