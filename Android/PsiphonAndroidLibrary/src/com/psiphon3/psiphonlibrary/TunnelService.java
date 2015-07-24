/*
 * Copyright (c) 2015, Psiphon Inc.
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

package com.psiphon3.psiphonlibrary;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

public class TunnelService extends Service {
    private TunnelManager m_tunnelManager = new TunnelManager(this, this);

    @Override
    public IBinder onBind(Intent intent) {
        return m_tunnelManager.onBind(intent);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return m_tunnelManager.onStartCommand(intent, flags, startId);
    }

    @Override
    public void onCreate() {
        m_tunnelManager.onCreate();
    }

    @Override
    public void onDestroy() {
        m_tunnelManager.onDestroy();
    }
}
