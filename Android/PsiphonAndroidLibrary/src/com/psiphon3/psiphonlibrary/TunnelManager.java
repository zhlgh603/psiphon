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

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.net.VpnService;
import android.net.VpnService.Builder;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;

import ca.psiphon.PsiphonTunnel;

import com.psiphon3.psiphonlibrary.Utils.MyLog;

public class TunnelManager implements PsiphonTunnel.HostService, MyLog.ILogger {
    // Android IPC messages

    // Client -> Service
    public final static int MSG_REGISTER = 0;
    public final static int MSG_UNREGISTER = 1;
    public final static int MSG_STOP_VPN_SERVICE = 2;
    // Service -> Client
    public final static int MSG_REGISTER_RESPONSE = 3;
    public final static int MSG_KNOWN_SERVER_REGIONS = 4;
    public final static int MSG_TUNNEL_STARTING = 5;
    public final static int MSG_TUNNEL_STOPPING = 6;
    public final static int MSG_TUNNEL_CONNECTION_STATE = 7;
    public final static int MSG_LOGS = 8;

    public static final String INTENT_ACTION_HANDSHAKE = "com.psiphon3.psiphonlibrary.TunnelManager.HANDSHAKE";

    public final static String DATA_TUNNEL_STATE_AVAILABLE_EGRESS_REGIONS = "availableEgressRegions";
    public final static String DATA_TUNNEL_STATE_IS_CONNECTED = "isConnected";
    public final static String DATA_TUNNEL_STATE_LISTENING_LOCAL_SOCKS_PROXY_PORT = "ListeningLocalSocksProxyPort";
    public final static String DATA_TUNNEL_STATE_LISTENING_LOCAL_HTTP_PROXY_PORT = "ListeningLocalHttpProxyPort";
    public final static String DATA_TUNNEL_STATE_HOME_PAGES = "homePages";
    public static final String DATA_HANDSHAKE_IS_RECONNECT = "isReconnect";
    public static final String DATA_TUNNEL_CONFIG_HANDSHAKE_PENDING_INTENT = "tunnelConfigHandshakePendingIntent";
    public static final String DATA_TUNNEL_CONFIG_NOTIFICATION_PENDING_INTENT = "tunnelConfigNotificationPendingIntent";
    public static final String DATA_TUNNEL_CONFIG_WHOLE_DEVICE = "tunnelConfigWholeDevice";
    public static final String DATA_TUNNEL_CONFIG_EGRESS_REGION = "tunnelConfigEgressRegion";
    public static final String DATA_TUNNEL_CONFIG_NOTIFICATION_SOUND = "tunnelConfigNotificationSound";
    public static final String DATA_TUNNEL_CONFIG_NOTIFICATION_VIBRATE = "tunnelConfigNotificationVibrate";
    public static final String DATA_LOGS = "logs";    

    // Tunnel config, received from the client.
    public static class Config {
        PendingIntent handshakePendingIntent = null;
        PendingIntent notificationPendingIntent = null;
        boolean wholeDevice = false;
        String egressRegion = PsiphonConstants.REGION_CODE_ANY;
        boolean notificationSound = false;
        boolean notificationVibrate = false;
    }

    private Config m_tunnelConfig = new Config();

    // Shared tunnel state, sent to the client in the HANDSHAKE
    // intent and various state-related Messages.
    public static class State {
        ArrayList<String> availableEgressRegions = new ArrayList<String>();
        boolean isConnected = false;
        ArrayList<String> homePages = new ArrayList<String>();
        int listeningLocalSocksProxyPort = 0;
        int listeningLocalHttpProxyPort = 0;
    }

    private State m_tunnelState = new State();

    private Context m_parentContext = null;
    private Service m_parentService = null;
    private boolean m_serviceDestroyed = false;
    private boolean m_firstStart = true;

    private PsiphonTunnel m_tunnel = null;
    private boolean m_isReconnect = false;
    private boolean m_isTunnelStarted = false;
    private String m_lastUpstreamProxyErrorMessage;

    public TunnelManager(Context parentContext, Service parentService) {
        Utils.initializeSecureRandom();

        m_parentContext = parentContext;
        m_parentService = parentService;
    }

    public int onStartCommand(Intent intent, int flags, int startId) {
        if (m_firstStart && intent != null) {
            getTunnelConfig(intent);
            doForeground();
            startTunnel();
            m_firstStart = false;
        }
        return android.app.Service.START_REDELIVER_INTENT;
    }

    public void onCreate() {
        MyLog.setLogger(this);
        m_tunnel = PsiphonTunnel.newPsiphonVpn(this);
    }

    public void onDestroy() {
        m_serviceDestroyed = true;

        stopTunnel();
        
        MyLog.unsetLogger();
    }

    private void getTunnelConfig(Intent intent) {
        m_tunnelConfig.handshakePendingIntent = (PendingIntent) intent
                .getParcelableExtra(TunnelManager.DATA_TUNNEL_CONFIG_HANDSHAKE_PENDING_INTENT);

        m_tunnelConfig.notificationPendingIntent = (PendingIntent) intent
                .getParcelableExtra(TunnelManager.DATA_TUNNEL_CONFIG_NOTIFICATION_PENDING_INTENT);

        m_tunnelConfig.wholeDevice = intent.getBooleanExtra(
                TunnelManager.DATA_TUNNEL_CONFIG_WHOLE_DEVICE, false);

        m_tunnelConfig.egressRegion = intent
                .getStringExtra(TunnelManager.DATA_TUNNEL_CONFIG_EGRESS_REGION);

        m_tunnelConfig.notificationSound = intent.getBooleanExtra(
                TunnelManager.DATA_TUNNEL_CONFIG_NOTIFICATION_SOUND, false);

        m_tunnelConfig.notificationVibrate = intent.getBooleanExtra(
                TunnelManager.DATA_TUNNEL_CONFIG_NOTIFICATION_VIBRATE, false);
    }

    private void doForeground() {
        m_parentService.startForeground(
                R.string.psiphon_service_notification_id,
                this.createNotification(false));
    }

    private Notification createNotification(boolean alert) {
        int contentTextID = -1;
        int iconID = -1;
        CharSequence ticker = null;

        if (m_tunnelState.isConnected) {
            if (m_tunnelConfig.wholeDevice) {
                contentTextID = R.string.psiphon_running_whole_device;
            } else {
                contentTextID = R.string.psiphon_running_browser_only;
            }

            iconID = R.drawable.notification_icon_connected;
        } else {
            contentTextID = R.string.psiphon_service_notification_message_connecting;
            ticker = m_parentService
                    .getText(R.string.psiphon_service_notification_message_connecting);
            iconID = R.drawable.notification_icon_connecting_animation;
        }

        Notification notification = new Notification(iconID, ticker,
                System.currentTimeMillis());

        if (alert) {
            if (m_tunnelConfig.notificationSound) {
                notification.defaults |= Notification.DEFAULT_SOUND;
            }
            if (m_tunnelConfig.notificationVibrate) {
                notification.defaults |= Notification.DEFAULT_VIBRATE;
            }
        }

        notification.setLatestEventInfo(m_parentService,
                m_parentService.getText(R.string.app_name),
                m_parentService.getText(contentTextID),
                m_tunnelConfig.notificationPendingIntent);

        return notification;
    }

    private void setIsConnected(boolean isConnected) {
        boolean alert = (isConnected != m_tunnelState.isConnected);

        m_tunnelState.isConnected = isConnected;

        if (!m_serviceDestroyed) {
            String ns = Context.NOTIFICATION_SERVICE;
            NotificationManager notificationManager = (NotificationManager) m_parentService
                    .getSystemService(ns);
            if (notificationManager != null) {
                notificationManager.notify(
                        R.string.psiphon_service_notification_id,
                        createNotification(alert));
            }
        }
    }

    public IBinder onBind(Intent intent) {
        return m_incomingMessenger.getBinder();
    }

    private final Messenger m_incomingMessenger = new Messenger(
            new IncomingMessageHandler());
    private Messenger m_outgoingMessenger = null;

    private class IncomingMessageHandler extends Handler {
        @Override
        public void handleMessage(Message msg)
        {
            switch (msg.what)
            {
            case TunnelManager.MSG_REGISTER:
                m_outgoingMessenger = msg.replyTo;
                sendClientMessage(MSG_REGISTER_RESPONSE, getTunnelStateBundle());
                break;

            case TunnelManager.MSG_UNREGISTER:
                m_outgoingMessenger = null;
                break;

            case TunnelManager.MSG_STOP_VPN_SERVICE:
                stopVpnServiceHelper();
                break;

            default:
                super.handleMessage(msg);
            }
        }
    }

    private void sendClientMessage(int what, Bundle data) {
        if (m_incomingMessenger == null || m_outgoingMessenger == null) {
            return;
        }
        try {
            Message msg = Message.obtain(null, what);
            msg.replyTo = m_incomingMessenger;
            if (data != null) {
                msg.setData(data);
            }
            m_outgoingMessenger.send(msg);
        } catch (RemoteException e) {
            // NOTE: potential stack overflow since MyLog invokes
            // statusEntryAdded which invokes sendClientMessage
            // MyLog.g("sendClientMessage failed: %s", e.getMessage());
        }
    }
    
    private void sendHandshakeIntent(boolean isReconnect) {
        Intent fillInExtras = new Intent();
        fillInExtras.putExtra(DATA_HANDSHAKE_IS_RECONNECT, isReconnect);
        fillInExtras.putExtras(getTunnelStateBundle());
        try {
            m_tunnelConfig.handshakePendingIntent.send(
                    m_parentService, 0, fillInExtras);
        } catch (CanceledException e) {
            MyLog.g("sendHandshakeIntent failed: %s", e.getMessage());
        }
    }

    private Bundle getTunnelStateBundle() {
        Bundle data = new Bundle();
        data.putStringArrayList(DATA_TUNNEL_STATE_AVAILABLE_EGRESS_REGIONS, m_tunnelState.availableEgressRegions);
        data.putBoolean(DATA_TUNNEL_STATE_IS_CONNECTED, m_tunnelState.isConnected);
        data.putStringArrayList(DATA_TUNNEL_STATE_HOME_PAGES, m_tunnelState.homePages);
        data.putInt(DATA_TUNNEL_STATE_LISTENING_LOCAL_SOCKS_PROXY_PORT, m_tunnelState.listeningLocalSocksProxyPort);
        data.putInt(DATA_TUNNEL_STATE_LISTENING_LOCAL_HTTP_PROXY_PORT, m_tunnelState.listeningLocalHttpProxyPort);
        return data;
    }

    private final static String LEGACY_SERVER_ENTRY_FILENAME = "psiphon_server_entries.json";
    private final static int MAX_LEGACY_SERVER_ENTRIES = 100;

    private String getServerEntries() {
        StringBuilder list = new StringBuilder();

        for (String encodedServerEntry : EmbeddedValues.EMBEDDED_SERVER_LIST) {
            list.append(encodedServerEntry);
            list.append("\n");
        }

        // Import legacy server entries
        try {
            FileInputStream file = m_parentContext
                    .openFileInput(LEGACY_SERVER_ENTRY_FILENAME);
            BufferedReader reader = new BufferedReader(new InputStreamReader(
                    file));
            StringBuilder json = new StringBuilder();
            String line;
            while ((line = reader.readLine()) != null) {
                json.append(line);
            }
            file.close();
            JSONObject obj = new JSONObject(json.toString());
            JSONArray jsonServerEntries = obj.getJSONArray("serverEntries");

            // MAX_LEGACY_SERVER_ENTRIES ensures the list we pass through to
            // tunnel-core
            // is unlikely to trigger an OutOfMemoryError
            for (int i = 0; i < jsonServerEntries.length()
                    && i < MAX_LEGACY_SERVER_ENTRIES; i++) {
                list.append(jsonServerEntries.getString(i));
                list.append("\n");
            }

            // Don't need to repeat the import again
            m_parentContext.deleteFile(LEGACY_SERVER_ENTRY_FILENAME);
        } catch (FileNotFoundException e) {
            // pass
        } catch (IOException e) {
            MyLog.g("prepareServerEntries failed: %s", e.getMessage());
        } catch (JSONException e) {
            MyLog.g("prepareServerEntries failed: %s", e.getMessage());
        } catch (OutOfMemoryError e) {
            MyLog.g("prepareServerEntries failed: %s", e.getMessage());

            // Comment from legacy code:
            // Some mature client installs have so many server entries they
            // cannot load them without
            // hitting out-of-memory, so they will not benefit from the
            // MAX_SAVED_SERVER_ENTRIES_MEMORY_SIZE
            // limit added to saveServerEntries(). In this case, we simply
            // ignore the saved list. The client
            // will proceed with the embedded list only, and going forward the
            // MEMORY_SIZE limit will be
            // enforced.
        }

        return list.toString();
    }

    private void startTunnel() {
        Utils.checkSecureRandom();

        stopTunnel();

        m_isTunnelStarted = true;

        // Notify if an upgrade has already been downloaded and is waiting for
        // install
        UpgradeManager.UpgradeInstaller.notifyUpgrade(m_parentContext);

        sendClientMessage(MSG_TUNNEL_STARTING, null);

        MyLog.v(R.string.client_version, MyLog.Sensitivity.NOT_SENSITIVE,
                EmbeddedValues.CLIENT_VERSION);

        MyLog.v(R.string.current_network_type, MyLog.Sensitivity.NOT_SENSITIVE,
                Utils.getNetworkTypeName(m_parentContext));

        MyLog.v(R.string.starting_tunnel, MyLog.Sensitivity.NOT_SENSITIVE);

        m_tunnelState.homePages.clear();

        m_isReconnect = false;

        boolean runVpn = m_tunnelConfig.wholeDevice && Utils.hasVpnService() &&
        // Guard against trying to start WDM mode when the global option flips
        // while starting a TunnelService
                (m_parentService instanceof TunnelVpnService);

        try {
            if (runVpn) {
                if (!m_tunnel.startRouting()) {
                    throw new PsiphonTunnel.Exception(
                            "application is not prepared or revoked");
                }

                MyLog.v(R.string.vpn_service_running,
                        MyLog.Sensitivity.NOT_SENSITIVE);
            }

            m_tunnel.startTunneling(getServerEntries());
        } catch (PsiphonTunnel.Exception e) {
            MyLog.e(R.string.start_tunnel_failed,
                    MyLog.Sensitivity.NOT_SENSITIVE, e.getMessage());

            // Cleanup routing/tunnel
            stopTunnel();

            // Stop service
            m_parentService.stopForeground(true);
            m_parentService.stopSelf();
        }
    }

    private void stopTunnel() {
        if (!m_isTunnelStarted) {
            return;
        }

        MyLog.v(R.string.stopping_tunnel, MyLog.Sensitivity.NOT_SENSITIVE);

        sendClientMessage(MSG_TUNNEL_STOPPING, null);

        m_tunnel.stop();

        setIsConnected(false);

        MyLog.v(R.string.stopped_tunnel, MyLog.Sensitivity.NOT_SENSITIVE);

        m_isTunnelStarted = false;
    }

    // A hack to stop the VpnService, which doesn't respond to normal
    // stopService() calls.
    public void stopVpnServiceHelper() {
        stopTunnel();

        m_parentService.stopForeground(true);
        m_parentService.stopSelf();
    }

    @Override
    public String getAppName() {
        return m_parentContext.getString(R.string.app_name);
    }

    @Override
    public Context getContext() {
        return m_parentContext;
    }

    @Override
    public VpnService getVpnService() {
        return ((TunnelVpnService) m_parentService);
    }

    @Override
    public Builder newVpnServiceBuilder() {
        return ((TunnelVpnService) m_parentService).newBuilder();
    }

    @Override
    public String getPsiphonConfig() {
        try {
            JSONObject json = new JSONObject();

            if (0 > EmbeddedValues.UPGRADE_URL.length()
                    && EmbeddedValues.hasEverBeenSideLoaded(m_parentContext)) {
                json.put("UpgradeDownloadUrl", EmbeddedValues.UPGRADE_URL);

                json.put("UpgradeDownloadFilename",
                        new UpgradeManager.DownloadedUpgradeFile(
                                m_parentContext).getFilename());
            }

            json.put("ClientPlatform", PsiphonConstants.PLATFORM);

            json.put("ClientVersion", EmbeddedValues.CLIENT_VERSION);

            json.put("PropagationChannelId",
                    EmbeddedValues.PROPAGATION_CHANNEL_ID);

            json.put("SponsorId", EmbeddedValues.SPONSOR_ID);

            json.put("RemoteServerListUrl",
                    EmbeddedValues.REMOTE_SERVER_LIST_URL);

            json.put("RemoteServerListSignaturePublicKey",
                    EmbeddedValues.REMOTE_SERVER_LIST_SIGNATURE_PUBLIC_KEY);

            // TODO: configure local proxy ports

            json.put("LocalHttpProxyPort", 0);

            json.put("LocalSocksProxyPort", 0);

            // TODO: configure "UpstreamProxyUrl"

            MyLog.g("EgressRegion", "regionCode", m_tunnelConfig.egressRegion);
            json.put("EgressRegion", m_tunnelConfig.egressRegion);

            return json.toString();

        } catch (JSONException e) {
            return "";
        }
    }

    @Override
    public void onDiagnosticMessage(String message) {
        // TODO-TUNNEL-CORE: temporary:
        //MyLog.g("diagnostic", "msg", message);
        android.util.Log.e("PSIPHON-DIAGNOSTIC", message);
    }

    @Override
    public void onAvailableEgressRegions(List<String> regions) {
        m_tunnelState.availableEgressRegions.clear();
        m_tunnelState.availableEgressRegions.addAll(regions);
        Bundle data = new Bundle();
        data.putStringArrayList(
                DATA_TUNNEL_STATE_AVAILABLE_EGRESS_REGIONS, m_tunnelState.availableEgressRegions);
        sendClientMessage(MSG_KNOWN_SERVER_REGIONS, data);
    }

    @Override
    public void onSocksProxyPortInUse(int port) {
        MyLog.e(R.string.socks_port_in_use, MyLog.Sensitivity.NOT_SENSITIVE,
                port);
        stopVpnServiceHelper();
    }

    @Override
    public void onHttpProxyPortInUse(int port) {
        MyLog.e(R.string.http_proxy_port_in_use,
                MyLog.Sensitivity.NOT_SENSITIVE, port);
        stopVpnServiceHelper();
    }

    @Override
    public void onListeningSocksProxyPort(int port) {
        MyLog.v(R.string.socks_running, MyLog.Sensitivity.NOT_SENSITIVE, port);
        m_tunnelState.listeningLocalSocksProxyPort = port;
    }

    @Override
    public void onListeningHttpProxyPort(int port) {
        MyLog.v(R.string.http_proxy_running, MyLog.Sensitivity.NOT_SENSITIVE,
                port);
        m_tunnelState.listeningLocalHttpProxyPort = port;
    }

    @Override
    public void onUpstreamProxyError(String message) {
        // Display the error message only once, and continue trying to connect
        // in
        // case the issue is temporary.
        if (!m_lastUpstreamProxyErrorMessage.equals(message)) {
            MyLog.v(R.string.upstream_proxy_error,
                    MyLog.Sensitivity.SENSITIVE_FORMAT_ARGS, message);
            m_lastUpstreamProxyErrorMessage = message;
        }
    }

    @Override
    public void onConnecting() {
        setIsConnected(false);
        Bundle data = new Bundle();
        data.putBoolean(DATA_TUNNEL_STATE_IS_CONNECTED, false);
        sendClientMessage(MSG_TUNNEL_CONNECTION_STATE, data);
    }

    @Override
    public void onConnected() {
        sendHandshakeIntent(m_isReconnect);
        m_isReconnect = true;

        setIsConnected(true);
        Bundle data = new Bundle();
        data.putBoolean(DATA_TUNNEL_STATE_IS_CONNECTED, true);
        sendClientMessage(MSG_TUNNEL_CONNECTION_STATE, data);
    }

    @Override
    public void onHomepage(String url) {
        for (String homePage : m_tunnelState.homePages) {
            if (homePage.equals(url)) {
                return;
            }
        }
        m_tunnelState.homePages.add(url);
    }

    @Override
    public void onClientUpgradeDownloaded(String filename) {
        UpgradeManager.UpgradeInstaller.notifyUpgrade(m_parentContext);
    }

    @Override
    public void onSplitTunnelRegion(String region) {
        MyLog.v(R.string.split_tunnel_region,
                MyLog.Sensitivity.SENSITIVE_FORMAT_ARGS, region);
    }

    @Override
    public void onUntunneledAddress(String address) {
        MyLog.v(R.string.untunneled_address,
                MyLog.Sensitivity.SENSITIVE_FORMAT_ARGS, address);
    }

    @Override
    public void statusEntryAdded() {
        // TODO-TUNNEL-CORE: temporary implementation only! neither robust nor functional.
        if (m_outgoingMessenger == null) {
            return;
        }
        ArrayList<PsiphonData.StatusEntry> statusEntries = PsiphonData.getPsiphonData().cloneStatusHistory();
        PsiphonData.getPsiphonData().clearStatusHistory();
        ArrayList<String> logs = new ArrayList<String>();
        for (PsiphonData.StatusEntry entry : statusEntries) {
            logs.add(m_parentContext.getString(entry.id(), entry.formatArgs()));
        }
        Bundle data = new Bundle();
        data.putStringArrayList(DATA_LOGS, logs);
        sendClientMessage(MSG_LOGS, data);
    }
}
