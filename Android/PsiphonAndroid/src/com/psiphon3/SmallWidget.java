/*
 * Copyright (c) 2013, Psiphon Inc.
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

package com.psiphon3;

import com.psiphon3.psiphonlibrary.MainBase;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.Context;
import android.content.Intent;
import android.widget.RemoteViews;

public class SmallWidget extends AppWidgetProvider {

    @Override
    public void onUpdate(Context context, AppWidgetManager appWidgetManager, int[] appWidgetIds)
    {
        RemoteViews views = new RemoteViews(context.getPackageName(), R.layout.small_widget);

        // The launch activity button
        Intent launchIntent = new Intent(context, StatusActivity.class);
        PendingIntent launchPendingIntent = PendingIntent.getActivity(context,  0, launchIntent, 0);
        views.setOnClickPendingIntent(R.id.launchactivitybutton, launchPendingIntent);

        // The toggle connection button
        Intent toggleIntent = new Intent(
                MainBase.TabbedActivityBase.DO_TOGGLE,
                null,
                context,
                com.psiphon3.StatusActivity.class);
        PendingIntent togglePendingIntent = PendingIntent.getActivity(context, 0, toggleIntent, 0);
        views.setOnClickPendingIntent(R.id.toggleconnectionbutton, togglePendingIntent);

        // Perform this loop procedure for each App Widget that belongs to this provider
        final int N = appWidgetIds.length;
        for (int i = 0; i < N; i++)
        {
            int appWidgetId = appWidgetIds[i];
            
            // Tell the AppWidgetManager to perform an update on the current app widget
            appWidgetManager.updateAppWidget(appWidgetId, views);
        }
    }
}
