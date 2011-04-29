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
#include "serverinfo.h"

ServerInfo::ServerInfo(const ServerEntry& serverEntry) :
    m_serverEntry(serverEntry)
{
}

ServerInfo::~ServerInfo(void)
{
}

bool ServerInfo::DoHandshake(void)
{
    // TODO: implement
    return true;
}

tstring ServerInfo::GetPSK(void)
{
    // TODO: this is a stub
    return _T("1q2w3e4r!");
}

vector<tstring> ServerInfo::GetHomepages(void)
{
    // TODO: this is a stub
    vector<tstring> homepages;
    homepages.push_back(_T("http://www.cnn.com"));
    homepages.push_back(_T("http://www.bbc.co.uk"));
    homepages.push_back(_T("http://www.voanews.com"));
    
    return homepages;
}

vector<ServerEntry> ServerInfo::GetDiscoveredServerEntries(void)
{
    // TODO: this is a stub
    ServerEntries discoveredServers;
    discoveredServers.push_back(ServerEntry("64.34.96.2", 80, "0123456789ABCDEF"));
    discoveredServers.push_back(ServerEntry("192.168.1.250", 80, "FEDCBA9876543210"));
    
    return discoveredServers;
}
