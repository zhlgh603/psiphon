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

import java.util.ArrayList;
import java.util.List;

import com.psiphon3.psiphonlibrary.Utils.MyLog;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiManager;

public class OpenWiFiConnector
{
    private static OpenWiFiConnector mInstance = null;
    private WiFiScanResultsAvailableReceiver mScanResultsReceiver = null;
    private ArrayList<Integer> mEnabledNetworkIDs = null;
    
    private OpenWiFiConnector()
    {
        mScanResultsReceiver = new WiFiScanResultsAvailableReceiver();
        mEnabledNetworkIDs = new ArrayList<Integer>();
    }
    
    private synchronized static OpenWiFiConnector getInstance()
    {
        if (mInstance == null)
        {
            mInstance = new OpenWiFiConnector();
        }
        return mInstance;
    }
    
    public static void activate(Context context)
    {
        context.registerReceiver(getInstance().mScanResultsReceiver, new IntentFilter(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION));
    }
    
    public static void deactivate(Context context)
    {
        OpenWiFiConnector instance = getInstance();
        context.unregisterReceiver(instance.mScanResultsReceiver);
        
        // Disable all enabled networks
        WifiManager wifiManager = (WifiManager)context.getSystemService(Context.WIFI_SERVICE);
        for (Integer networkID : instance.mEnabledNetworkIDs)
        {
            wifiManager.disableNetwork(networkID);
        }
        instance.mEnabledNetworkIDs.clear();
    }
    
    private synchronized static void addEnabledNetworkID(Integer networkID)
    {
        getInstance().mEnabledNetworkIDs.add(networkID);
    }
    
    private static String findBestOpenWiFiSSID(WifiManager wifiManager)
    {
        List<ScanResult> scanResults = wifiManager.getScanResults();

        boolean openNetworkPresent = false;
        int bestSignalLevel = 0;
        String bestOpenNetworkSSID = "";
        for (ScanResult scanResult : scanResults)
        {
            // Find Open networks
            String capabilities = scanResult.capabilities;
            if (!capabilities.contains("PSK") &&
                    !capabilities.contains("WEP") &&
                    !capabilities.contains("EAP"))
            {
                if (!openNetworkPresent ||
                        scanResult.level > bestSignalLevel)
                {
                    bestSignalLevel = scanResult.level;
                    bestOpenNetworkSSID = scanResult.SSID;
                }
                
                openNetworkPresent = true;
            }
        }
        return bestOpenNetworkSSID;
    }
    
    private static void connectToOpenWiFiSSID(WifiManager wifiManager, String SSID)
    {
        WifiConfiguration wifiConfig = new WifiConfiguration();
        wifiConfig.SSID = String.format("\"%s\"", SSID);
        wifiConfig.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
        int networkID = wifiManager.addNetwork(wifiConfig);
        wifiManager.disconnect();
        wifiManager.enableNetwork(networkID, true);
        wifiManager.reconnect();
        addEnabledNetworkID(networkID);
        MyLog.i(R.string.connecting_to_open_wifi, MyLog.Sensitivity.SENSITIVE_FORMAT_ARGS, SSID);
        // TODO: notification?
    }
    
    public static class WiFiScanResultsAvailableReceiver extends BroadcastReceiver
    {
        public WiFiScanResultsAvailableReceiver()
        {
        }
        
        @Override
        public void onReceive(Context context, Intent intent) {
            // Don't do anything if WiFi is already connected/connecting
            ConnectivityManager connManager = (ConnectivityManager)context.getSystemService(Context.CONNECTIVITY_SERVICE);
            NetworkInfo wifiNetworkInfo = connManager.getNetworkInfo(ConnectivityManager.TYPE_WIFI);
            if (!wifiNetworkInfo.isConnectedOrConnecting())
            {
                WifiManager wifiManager = (WifiManager)context.getSystemService(Context.WIFI_SERVICE);
                String bestWiFiSSID = findBestOpenWiFiSSID(wifiManager);
                if (bestWiFiSSID.length() > 0)
                {
                    connectToOpenWiFiSSID(wifiManager, bestWiFiSSID);
                }
            }
        }
    }
}
