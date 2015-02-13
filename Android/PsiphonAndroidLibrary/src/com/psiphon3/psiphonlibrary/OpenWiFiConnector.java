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
import java.util.Iterator;
import java.util.List;

import com.psiphon3.psiphonlibrary.Utils.MyLog;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.SystemClock;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiManager;

public class OpenWiFiConnector
{
    private static OpenWiFiConnector mInstance = null;
    private WiFiScanResultsAvailableReceiver mScanResultsReceiver = null;
    private ArrayList<String> mEnabledNetworkQuotedSSIDs = null;
    private boolean mActive = false;
    
    private OpenWiFiConnector()
    {
        mScanResultsReceiver = new WiFiScanResultsAvailableReceiver();
        mEnabledNetworkQuotedSSIDs = new ArrayList<String>();
    }
    
    private synchronized static OpenWiFiConnector getInstance()
    {
        if (mInstance == null)
        {
            mInstance = new OpenWiFiConnector();
        }
        return mInstance;
    }
    
    public synchronized static void setActive(Context context, boolean active)
    {
        OpenWiFiConnector instance = getInstance();
        if (active)
        {
            instance.mActive = true;
            context.registerReceiver(instance.mScanResultsReceiver, new IntentFilter(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION));
        }
        else if (instance.mActive)
        {
            instance.mActive = false;
            context.unregisterReceiver(instance.mScanResultsReceiver);
            
            // Disable all enabled networks
            WifiManager wifiManager = (WifiManager)context.getSystemService(Context.WIFI_SERVICE);
            for (String quotedNetworkSSID : instance.mEnabledNetworkQuotedSSIDs)
            {
                DeconfigureOpenWiFiWithSSID(wifiManager, quotedNetworkSSID);
            }
            instance.mEnabledNetworkQuotedSSIDs.clear();
        }
    }
    
    public static class WiFiScanResultsAvailableReceiver extends BroadcastReceiver
    {
        private final int CONNECTION_ATTEMPT_GRACE_PERIOD_MILLISECONDS = 10000;
        
        private long mLastConnectionAttemptTime = 0;
        
        public WiFiScanResultsAvailableReceiver()
        {
        }
        
        @Override
        public void onReceive(Context context, Intent intent)
        {
            if (!getInstance().mActive)
            {
                return;
            }

            if (SystemClock.elapsedRealtime() - mLastConnectionAttemptTime < CONNECTION_ATTEMPT_GRACE_PERIOD_MILLISECONDS)
            {
                return;
            }
            
            // Don't do anything if WiFi is already connected/connecting
            ConnectivityManager connManager = (ConnectivityManager)context.getSystemService(Context.CONNECTIVITY_SERVICE);
            NetworkInfo wifiNetworkInfo = connManager.getNetworkInfo(ConnectivityManager.TYPE_WIFI);
            if (!wifiNetworkInfo.isConnectedOrConnecting())
            {
                WifiManager wifiManager = (WifiManager)context.getSystemService(Context.WIFI_SERVICE);
                List<ScanResult> scanResults = wifiManager.getScanResults();
                // Prune out enabled networks that are now out of range
                pruneEnabledNetworks(wifiManager, scanResults);
                // Connect to the best available open network
                String bestWiFiSSID = findBestOpenWiFiSSID(scanResults);
                if (bestWiFiSSID.length() > 0)
                {
                    if (!wifiNetworkInfo.isConnectedOrConnecting())
                    {
                        mLastConnectionAttemptTime = SystemClock.elapsedRealtime();
                        connectToOpenWiFiSSID(wifiManager, bestWiFiSSID);
                    }
                }
            }
        }
    }
    
    private synchronized static void pruneEnabledNetworks(WifiManager wifiManager, List<ScanResult> scanResults)
    {
        OpenWiFiConnector instance = getInstance();
        Iterator<String> iterator = instance.mEnabledNetworkQuotedSSIDs.iterator();
        while (iterator.hasNext())
        {
            String enabledNetworkQuotedSSID = iterator.next();
            boolean networkInRange = false;
            for (ScanResult scanResult : scanResults)
            {
                if (scanResult.SSID.equals(enabledNetworkQuotedSSID) &&
                        openNetworkCapabilities(scanResult.capabilities))
                {
                    networkInRange = true;
                    break;
                }
            }
            // Deconfigure this network if it is not in the scan results
            if (!networkInRange)
            {
                DeconfigureOpenWiFiWithSSID(wifiManager, enabledNetworkQuotedSSID);
                iterator.remove();
            }
        }
    }
    
    private static String findBestOpenWiFiSSID(List<ScanResult> scanResults)
    {
        boolean openNetworkPresent = false;
        int bestSignalLevel = 0;
        String bestOpenNetworkSSID = "";
        for (ScanResult scanResult : scanResults)
        {
            String capabilities = scanResult.capabilities;
            if (scanResult.SSID.length() == 0 ||
                    scanResult.SSID.toLowerCase().startsWith("hp-print") ||
                    !openNetworkCapabilities(capabilities))
            {
                // Exclude hidden networks, "HP-Print-*" networks, and networks that are not Open
                continue;
            }
            
            MyLog.i(R.string.detect_open_wifi, MyLog.Sensitivity.SENSITIVE_FORMAT_ARGS, scanResult.SSID, scanResult.level, capabilities);

            if (!openNetworkPresent ||
                    scanResult.level > bestSignalLevel)
            {
                openNetworkPresent = true;
                bestSignalLevel = scanResult.level;
                bestOpenNetworkSSID = scanResult.SSID;
            }
        }
        return bestOpenNetworkSSID;
    }
    
    private static void connectToOpenWiFiSSID(WifiManager wifiManager, String SSID)
    {
        String quotedSSID = String.format("\"%s\"", SSID);
        int networkID = getConfiguredOpenWiFiWithSSID(wifiManager, quotedSSID);
        if (networkID == -1)
        {
            WifiConfiguration wifiConfig = new WifiConfiguration();
            wifiConfig.SSID = quotedSSID;
            wifiConfig.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
            networkID = wifiManager.addNetwork(wifiConfig);
            addEnabledNetworkSSID(quotedSSID);
        }
        wifiManager.disconnect();
        // Don't disable other networks to allow other preferred networks to connect
        wifiManager.enableNetwork(networkID, false);
        wifiManager.reconnect();
        MyLog.i(R.string.connecting_to_open_wifi, MyLog.Sensitivity.SENSITIVE_FORMAT_ARGS, SSID);
        // TODO: notification?
    }

    private static boolean openNetworkCapabilities(String capabilities)
    {
        // doesn't have security and not ad-hoc
        return !capabilities.contains("PSK") &&
                !capabilities.contains("WEP") &&
                !capabilities.contains("EAP") &&
                !capabilities.contains("[IBSS]");
    }
    
    private static int getConfiguredOpenWiFiWithSSID(WifiManager wifiManager, String quotedSSID)
    {
        List<WifiConfiguration> wifiConfigurations = wifiManager.getConfiguredNetworks();
        for (WifiConfiguration network : wifiConfigurations)
        {
            if (network.SSID.equals(quotedSSID)){
                if (network.allowedKeyManagement.get(WifiConfiguration.KeyMgmt.NONE))
                {
                    return network.networkId;
                }
            }
        }
        return -1;
    }

    private static void DeconfigureOpenWiFiWithSSID(WifiManager wifiManager, String quotedNetworkSSID)
    {
        int networkID = getConfiguredOpenWiFiWithSSID(wifiManager, quotedNetworkSSID);
        if (networkID != -1)
        {
            wifiManager.disableNetwork(networkID);
            wifiManager.removeNetwork(networkID);
        }
    }

    private synchronized static void addEnabledNetworkSSID(String quotedNetworkSSID)
    {
        getInstance().mEnabledNetworkQuotedSSIDs.add(quotedNetworkSSID);
    }
}
