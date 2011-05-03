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
#include "config.h"
#include "psiclient.h"
#include "vpnmanager.h"
#include "httpsrequest.h"
#include "webbrowser.h"
#include "embeddedvalues.h"
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
        OpenBrowser(m_currentSessionInfo.GetHomepages());
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
        VPNStateChanged(VPN_STATE_STOPPED);
    }
}

DWORD WINAPI VPNManager::TryNextServerThread(void* data)
{
    // By design, this function doesn't hold the VPNManager lock for the
    // duration. This allows the user to cancel the operation.
    //
    // This is all a stupid mess and a proper state machine should be
    // introduced here.  But it works well enough for now.
    //
    // If the user clicks cancel before Establish() is invoked, this thread
    // will change the state from INITIALIZING --> STOPPED. A second button
    // click before that transition should have no effect.
    //
    // If any of LoadNextServer, DoHandshake, Establish, HandleHandshakeResponse
    // fail, they will transition the state to STOPPED or FAILED.
    // In the case of failed, the a 2nd thread will start before this one
    // exits, and the state will go back to INITIALIZED and bypass STOPPED so
    // user button clicks won't start a new connection thread.
    //
    // When the state goes to STOPPED, another button click can start a new
    // connecttion attempt thread.  So in all cases, once the state becomes
    // STOPPED, this thread must immediately exit without modifying the
    // VPN state.

    VPNManager* manager = (VPNManager*)data;

    tstring serverAddress;
    int webPort;
    tstring handshakeRequest;
    string handshakeResponse;

    if (!manager->LoadNextServer(serverAddress, webPort,
                                 handshakeRequest))
    {
        // Helper function sets state to STOPPED or FAILED
        return 0;
    }

    // NOTE: DoHandshake doesn't hold the VPNManager lock for the entire
    // web request transaction.

    if (!manager->DoHandshake(serverAddress.c_str(), webPort,
                              handshakeRequest.c_str(), handshakeResponse))
    {
        // Helper function sets state to STOPPED or FAILED
        return 0;
    }

    if (!manager->HandleHandshakeResponse(handshakeResponse.c_str()))
    {
        // Helper function sets state to STOPPED or FAILED
        return 0;
    }

    // Abort now if user clicked cancel during web request
    if (manager->GetUserSignalledStop())
    {
        manager->VPNStateChanged(VPN_STATE_STOPPED);
        return 0;
    }

    if (!manager->Establish())
    {
        // Helper function sets state to STOPPED or FAILED
        return 0;
    }

    return 0;
}

bool VPNManager::LoadNextServer(
        tstring& serverAddress,
        int& webPort,
        tstring& handshakeRequestPath)
{
    // Select next server to try to connect to

    AutoMUTEX lock(m_mutex);
    
    ServerEntry serverEntry;

    try
    {
        // Try the next server in our list.
        serverEntry = m_vpnList.GetNextServer();
    }
    catch (std::exception &ex)
    {
        my_print(false, string("LoadNextServer caught exception: ") + ex.what());

        // NOTE: state change assumes we're calling LoadNextServer in sequence in TryNextServer thread
        VPNStateChanged(VPN_STATE_STOPPED);
        return false;
    }

    // Current session holds server entry info and will also be loaded
    // with homepage and other info.

    m_currentSessionInfo.Set(serverEntry);

    // Output values used in next TryNextServer step

    serverAddress = NarrowToTString(serverEntry.serverAddress);
    webPort = serverEntry.webServerPort;
    handshakeRequestPath = tstring(HTTP_HANDSHAKE_REQUEST_PATH) + 
                           _T("?server_secret=") + NarrowToTString(serverEntry.webServerSecret) +
                           _T("&client_id=") + NarrowToTString(CLIENT_ID) +
                           _T("&client_version=") + NarrowToTString(CLIENT_VERSION);

    return true;
}

bool VPNManager::DoHandshake(
        const TCHAR* serverAddress,
        int webPort,
        const TCHAR* handshakeRequestPath,
        string& handshakeResponse)
{
    // Perform handshake HTTPS request

    // NOTE: Lock isn't held while making network request
    //       Ensure AutoMUTEX() is created unless calling
    //       synchronized member function.
    //
    //       An assumption is made that other code won't
    //       change the state value while the HTTP request
    //       is performed and the VPNManager is unlocked.

    // TODO: remove this
    serverAddress = _T("192.168.1.250");

    HTTPSRequest httpsRequest;
    if (!httpsRequest.GetRequest(
                        m_userSignalledStop,
                        serverAddress,
                        webPort,
                        handshakeRequestPath,
                        handshakeResponse))
    {
        // NOTE: state change assumes we're calling DoHandshake in sequence in TryNextServer thread
        if (m_userSignalledStop)
        {
            VPNStateChanged(VPN_STATE_STOPPED);
        }
        else
        {
            VPNStateChanged(VPN_STATE_FAILED);
        }
        return false;
    }

    return true;
}

bool VPNManager::HandleHandshakeResponse(const char* handshakeResponse)
{
    // Parse handshake response
    // - get PSK, which we use to connect to VPN
    // - get homepage, which we'll launch later
    // - add discovered servers to local list

    AutoMUTEX lock(m_mutex);
    
    if (!m_currentSessionInfo.ParseHandshakeResponse(handshakeResponse))
    {
        // NOTE: state change assumes we're calling DoHandshake in sequence in TryNextServer thread
        VPNStateChanged(VPN_STATE_FAILED);
        return false;
    }

    try
    {
        m_vpnList.AddEntriesToList(m_currentSessionInfo.GetDiscoveredServerEntries());
    }
    catch (std::exception &ex)
    {
        my_print(false, string("DoHandshake caught exception: ") + ex.what());
        // This isn't fatal.  The VPN connection can still be established.
    }

    return true;
}

bool VPNManager::Establish(void)
{
    // Kick off the VPN connection establishment

    AutoMUTEX lock(m_mutex);
    
    if (!m_vpnConnection.Establish(NarrowToTString(m_currentSessionInfo.GetServerAddress()),
                                   _T("1q2w3e4r!")))// TODO: NarrowToTString(m_currentSessionInfo.GetPSK())))
    {
        // NOTE: state change assumes we're calling Establish in sequence in TryNextServer thread
        VPNStateChanged(VPN_STATE_STOPPED);
        return false;
    }

    return true;
}
