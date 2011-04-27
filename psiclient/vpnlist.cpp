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
#include "psiclient.h"
#include "vpnlist.h"
#include "embeddedvalues.h"
#include <algorithm>
#include <sstream>

VPNList::VPNList(void)
{
}

VPNList::~VPNList(void)
{
}

bool VPNList::AddEntryToList(const tstring& hexEncodedEntry)
{
    // TODO
    return false;
}

void VPNList::MarkCurrentServerFailed(void)
{
    ServerEntries serverEntryList = GetList();
    if (serverEntryList.size() > 1)
    {
        // Move the first server to the end of the list
        serverEntryList.push_back(serverEntryList[0]);
        serverEntryList.erase(serverEntryList.begin());
        WriteListToSystem(serverEntryList);
    }
}

ServerEntry VPNList::GetNextServer(void)
{
    ServerEntries serverEntryList = GetList();
    if (serverEntryList.size() < 1)
    {
        throw std::exception("No servers found.  This application is possibly corrupt.");
    }

    // The client always tries the first entry in the list.
    // The list will be rearranged elsewhere, such as when a server has failed,
    // or when new servers are discovered.
    return serverEntryList[0];
}

ServerEntries VPNList::GetList(void)
{
    ServerEntries serverEntryList;
    try
    {
        serverEntryList = GetListFromSystem();
    }
    catch (std::exception &ex)
    {
        my_print(false, string("Using Embedded Server List because the System Server List is corrupt: ") + ex.what());
    }

    if (serverEntryList.size() < 1)
    {
        serverEntryList = GetListFromEmbeddedValues();
        // Write this out immediately, so the next time we'll get it from the system
        WriteListToSystem(serverEntryList);
    }

    return serverEntryList;
}

ServerEntries VPNList::GetListFromEmbeddedValues(void)
{
    return ParseServerEntries(EMBEDDED_SERVER_LIST);
}

ServerEntries VPNList::GetListFromSystem(void)
{
    // TODO: implement
    return ServerEntries();
}

// Adapted from here:
// http://stackoverflow.com/questions/3381614/c-convert-string-to-hexadecimal-and-vice-versa
string Dehexlify(const string& input)
{
    static const char* const lut = "0123456789ABCDEF";
    size_t len = input.length();
    if (len & 1)
    {
        throw std::invalid_argument("Dehexlify: odd length");
    }

    string output;
    output.reserve(len / 2);
    for (size_t i = 0; i < len; i += 2)
    {
        char a = toupper(input[i]);
        const char* p = std::lower_bound(lut, lut + 16, a);
        if (*p != a)
        {
            throw std::invalid_argument("Dehexlify: not a hex digit");
        }

        char b = toupper(input[i + 1]);
        const char* q = std::lower_bound(lut, lut + 16, b);
        if (*q != b)
        {
            throw std::invalid_argument("Dehexlify: not a hex digit");
        }

        output.push_back(((p - lut) << 4) | (q - lut));
    }

    return output;
}

// TODO: Should the errors below throw (preventing any VPN connection from starting),
//       or just continue (throwing out the bad server entry)?
ServerEntries VPNList::ParseServerEntries(const char* serverEntryListString)
{
    ServerEntries serverEntryList;

    stringstream stream(serverEntryListString);
    string item;

    while (getline(stream, item, '\n'))
    {
        string line = Dehexlify(item);

        stringstream lineStream(line);
        string lineItem;
        ServerEntry entry;
        
        if (!getline(lineStream, lineItem, ' '))
        {
            throw std::exception("Server Entries are corrupt: can't parse Server Address");
        }
        entry.serverAddress = lineItem;

        if (!getline(lineStream, lineItem, ' '))
        {
            throw std::exception("Server Entries are corrupt: can't parse Web Server Port");
        }
        entry.webServerPort = atoi(lineItem.c_str());

        if (!getline(lineStream, lineItem, ' '))
        {
            throw std::exception("Server Entries are corrupt: can't parse Web Server Secret");
        }
        entry.webServerSecret = lineItem;

        serverEntryList.push_back(entry);
    }

    return serverEntryList;
}

void VPNList::WriteListToSystem(const ServerEntries& serverEntryList)
{
    // TODO: implement
    return;
}
