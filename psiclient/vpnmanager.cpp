/*
 * Copyright (c) 2011, Psiphon Inc.
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

#include "stdafx.h"
#include "vpnmanager.h"
#include "psiclient.h"
#include "webbrowser.h"
#include <algorithm>


VPNManager::VPNManager(void) :
    m_vpnState(VPN_STATE_STOPPED),
    m_userSignalledStop(false)
{
    m_mutex = CreateMutex(NULL, FALSE, 0);
}

VPNManager::~VPNManager(void)
{
    Stop();
    CloseHandle(m_mutex);
}

void VPNManager::Toggle()
{
    AutoMUTEX lock(m_mutex);

    switch (m_vpnState)
    {
    case VPN_STATE_STOPPED:
        // The user clicked the button to start the VPN.
        // Clear this flag so we can do retries on failed connections.
        m_userSignalledStop = false;
        TryNextServer();
        break;

    default:
        // The user requested to stop the VPN by clicking the button.
        //
        // If a connection was in the INITIALIZING state, this flag
        // tells TryNextServer not to Establish, or to Stop if
        // Establish was already called.
        // NOTE that Stop is called here in case TryNextServer has
        // already returned (and the first callback notification has
        // not yet been received).
        //
        // If a connection was in the STARTING state, we will get a
        // "Connection Failed" notification.
        // This flag indicates that we should not retry when a failed
        // connection is signalled.
        m_userSignalledStop = true;
        Stop();
        break;
    }
}

void VPNManager::Stop(void)
{
    AutoMUTEX lock(m_mutex);

    if (m_vpnConnection.Remove())
    {
        // TODO:
        //
        // This call was here for some now-forgotten reason: restore a known state or something.
        // However, it was causing a problem: after this STOPPED state was set, the user was able
        // to start a new Establish, but the RasCallback that was previously cancelled kicked in
        // after that Establish with a FAILED state change that caused a retry, which causes
        // VPNStateChanged to TryNextServer... end up with two simultaneous connection threads.
        //
        // Also:
        //
        // Another potential problem is that Stop() is invoked -- from a user Toggle -- after
        // Establish() but before the RasCallback thread is invoked, and RasHangup simply
        // succeeds (ERROR_NO_CONNECTION).  In this case, we go into the STOPPED state while
        // a connection is still outstanding.  Again, leading to multiple simultanetous connection
        // threads.  Maybe.
        //
        //VPNStateChanged(VPN_STATE_STOPPED);
    }
}

void VPNManager::VPNStateChanged(VPNState newState)
{
    AutoMUTEX lock(m_mutex);

    m_vpnState = newState;

    switch (m_vpnState)
    {
    case VPN_STATE_CONNECTED:
        if (m_serverInfo.get())
        {
            OpenBrowser(m_serverInfo->GetHomepages());
        }
        break;

    case VPN_STATE_FAILED:
        // Either the user cancelled an in-progress connection,
        // or a connection actually failed.
        // Either way, we need to set the status to STOPPED,
        // so that another Toggle() will cause the VPN to start again.
        if (m_userSignalledStop)
        {
            m_vpnState = VPN_STATE_STOPPED;
        }
        else
        {
            // Connecting to the current server failed.
            try
            {
                m_vpnList.MarkCurrentServerFailed();
                // WARNING: Don't try to optimize this code.
                // It is important for the state not to become STOPPED before TryNextServer
                // is called, to ensure that a button click does not invoke a second concurrent
                // TryNextServer.
                TryNextServer();
            }
            catch (std::exception &ex)
            {
                my_print(false, string("VPNStateChanged caught exception: ") + ex.what());
                m_vpnState = VPN_STATE_STOPPED;
            }
        }
        break;

    default:
        // no default actions
        break;
    }
}

void VPNManager::TryNextServer(void)
{
    AutoMUTEX lock(m_mutex);

    // The INITIALIZING state is set here, instead of inside the thread, to prevent a second
    // button click from triggering a second concurrent TryNextServer invocation.
    VPNStateChanged(VPN_STATE_INITIALIZING);

    // This function might not return quickly, because it performs an HTTPS Request.
    // It is run in a thread so that it does not starve the message pump.
    if (!CreateThread(0, 0, TryNextServerThread, (void*)this, 0, 0))
    {
        my_print(false, _T("TryNextServer: CreateThread failed (%d)"), GetLastError());
        Stop();
    }
}

DWORD WINAPI VPNManager::TryNextServerThread(void* object)
{
    VPNManager* This = (VPNManager*)object;

    AutoMUTEX lock(This->m_mutex);

    ServerEntry serverEntry;
    try
    {
        // Try the next server in our list.
        serverEntry = This->m_vpnList.GetNextServer();
    }
    catch (std::exception &ex)
    {
        my_print(false, string("TryNextServerThread caught exception: ") + ex.what());
        This->Stop();
        return 0;
    }

#ifdef _UNICODE
    wstring serverAddress(serverEntry.serverAddress.length(), L' ');
    std::copy(serverEntry.serverAddress.begin(), serverEntry.serverAddress.end(), serverAddress.begin());
#else
    string serverAddress = serverEntry.serverAddress;
#endif

    // NOTE: Toggle may have been clicked since the start of this function.
    //       If it was, don't make the web request.
    if (!This->m_userSignalledStop)
    {
        This->m_serverInfo.reset(new ServerInfo(serverEntry));
        if (This->m_serverInfo->DoHandshake())
        {
            try
            {
                This->m_vpnList.AddEntriesToList(This->m_serverInfo->GetDiscoveredServerEntries());
            }
            catch (std::exception &ex)
            {
                my_print(false, string("TryNextServerThread caught exception: ") + ex.what());
                // This isn't fatal.  The VPN connection can still be established.
            }
        }
        else
        {
            This->VPNStateChanged(VPN_STATE_FAILED);
            return 0;
        }
    }

    // NOTE: Toggle may have been clicked during the web request.
    //       If it was, don't Establish the VPN connection.
    if (!This->m_userSignalledStop)
    {
        if (!This->m_vpnConnection.Establish(serverAddress, This->m_serverInfo->GetPSK()))
        {
            // See note in Stop() which explains why we're not calling Stop() here
            // but just changing the state value.
            //This->Stop();

            This->m_vpnState = VPN_STATE_STOPPED;
        }
    }

    // NOTE: Toggle may have been clicked during Establish.
    //       If it was, Stop the VPN.
    if (This->m_userSignalledStop)
    {
        This->Stop();
    }

    return 0;
}
