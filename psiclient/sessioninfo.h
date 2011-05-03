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

#pragma once

#include <vector>
#include "vpnlist.h"
#include "tstring.h"

class VPNManager;

class SessionInfo
{
public:
    void Set(const ServerEntry& serverEntry);
    tstring GetServerAddress(void) {return m_serverAddress;}
    int GetWebPort(void);
    string GetHandshakeRequestLine(void);
    bool ParseHandshakeResponse(const string& response);
    tstring GetPSK(void);
    vector<tstring> GetHomepages(void);
    ServerEntries GetDiscoveredServerEntries(void);

private:
    ServerEntry m_serverEntry;
    tstring m_serverAddress;
};
