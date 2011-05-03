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
#include "sessioninfo.h"
#include "psiclient.h"
#include "vpnmanager.h"

void SessionInfo::Set(const ServerEntry& serverEntry)
{
    m_serverEntry = serverEntry;
#ifdef _UNICODE
    wstring serverAddress(serverEntry.serverAddress.length(), L' ');
    std::copy(serverEntry.serverAddress.begin(), serverEntry.serverAddress.end(), serverAddress.begin());
#else
    string serverAddress = serverEntry.serverAddress;
#endif
    m_serverAddress = serverAddress;
}

bool SessionInfo::ParseHandshakeResponse(const string& response)
{
    // TODO: implement
    return true;
}

tstring SessionInfo::GetPSK(void)
{
    // TODO: this is a stub
    return _T("1q2w3e4r!");
}

vector<tstring> SessionInfo::GetHomepages(void)
{
    // TODO: this is a stub
    vector<tstring> homepages;
    homepages.push_back(_T("http://www.cnn.com"));
    homepages.push_back(_T("http://www.bbc.co.uk"));
    homepages.push_back(_T("http://www.voanews.com"));
    
    return homepages;
}

vector<ServerEntry> SessionInfo::GetDiscoveredServerEntries(void)
{
    // TODO: this is a stub
    ServerEntries discoveredServers;
    discoveredServers.push_back(ServerEntry("192.168.1.250", 80, "FEDCBA9876543210"));
    discoveredServers.push_back(ServerEntry("64.34.96.2", 80, "0123456789ABCDEF"));
    
    return discoveredServers;
}
