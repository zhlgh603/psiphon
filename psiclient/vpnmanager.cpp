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


VPNManager::VPNManager(void) :
    m_vpnState(VPN_STATE_STOPPED),
    m_userSignalledStop(false)
{
}

VPNManager::~VPNManager(void)
{
    Stop();
}

void VPNManager::Toggle()
{
    if (VPN_STATE_STOPPED == m_vpnState)
    {
        // The user clicked the button to start the VPN.
        // Clear this flag so we can do retries on failed connections.
        m_userSignalledStop = false;
        m_vpnConnection.Establish();
    }
    else
    {
        // The user requested to stop the VPN by clicking the button.
        // If a connection was in the "Establishing" state, we will get a
        // "Connection Failed" notification.
        // This flag indicates that we should not retry when a failed
        // connection is signalled.
        m_userSignalledStop = true;
        Stop();
    }
}

void VPNManager::Stop(void)
{
    m_vpnConnection.Remove();
}

void VPNManager::VPNStateChanged(VPNState newState)
{
    m_vpnState = newState;

    if (VPN_STATE_FAILED == m_vpnState &&
        m_userSignalledStop)
    {
        // The user cancelled an in-progress connection.
        // Set the status to STOPPED.
        m_vpnState = VPN_STATE_STOPPED;
    }
    else if (VPN_STATE_FAILED == m_vpnState &&
             !m_userSignalledStop)
    {
        // Connecting to the current server failed.
        // TODO: Try the next one in our list.
        m_vpnConnection.Establish();
    }
}
