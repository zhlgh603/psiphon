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
    // TODO
}

bool VPNList::GetNextServer(ServerEntry& serverEntry)
{
    ServerEntries serverEntryList = GetList();
    if (serverEntryList.size() < 1)
    {
        return false;
    }

    // The client always tries the first entry in the list.
    // The list will be rearranged elsewhere, such as when a server has failed,
    // or when new servers are discovered.
    serverEntry = serverEntryList[0];

    return true;
}

ServerEntries VPNList::GetList(void)
{
    ServerEntries serverEntryList = GetListFromSystem();
    if (serverEntryList.size() < 1)
    {
        serverEntryList = GetListFromEmbeddedValues();
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

// TODO: Catch the exceptions somewhere.
// Adapted from here:
// http://stackoverflow.com/questions/3381614/c-convert-string-to-hexadecimal-and-vice-versa
string Dehexlify(const string& input)
{
    static const char* const lut = "0123456789ABCDEF";
    size_t len = input.length();
    if (len & 1)
    {
        throw std::invalid_argument("odd length");
    }

    string output;
    output.reserve(len / 2);
    for (size_t i = 0; i < len; i += 2)
    {
        char a = toupper(input[i]);
        const char* p = std::lower_bound(lut, lut + 16, a);
        if (*p != a)
        {
            throw std::invalid_argument("not a hex digit");
        }

        char b = toupper(input[i + 1]);
        const char* q = std::lower_bound(lut, lut + 16, b);
        if (*q != b)
        {
            throw std::invalid_argument("not a hex digit");
        }

        output.push_back(((p - lut) << 4) | (q - lut));
    }

    return output;
}

// TODO: some error reporting?
// TODO: throw (and catch) exceptions?
ServerEntries VPNList::ParseServerEntries(const char* serverEntryListString)
{
    ServerEntries serverEntryList;

    stringstream stream(serverEntryListString);
    string item;

    while(getline(stream, item, '\n'))
    {
        string line = Dehexlify(item);

        stringstream lineStream(line);
        string lineItem;
        ServerEntry entry;
        
        if (getline(lineStream, lineItem, ' '))
        {
            entry.serverAddress = lineItem;
        }

        if (getline(lineStream, lineItem, ' '))
        {
            entry.webServerPort = atoi(lineItem.c_str());
        }

        if (getline(lineStream, lineItem, ' '))
        {
            entry.webServerSecret = lineItem;
        }

        serverEntryList.push_back(entry);
    }

    return serverEntryList;
}

bool VPNList::WriteListToSystem(const ServerEntries& serverEntryList)
{
    // TODO: implement
    return false;
}
