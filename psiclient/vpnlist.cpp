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
#include "config.h"
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
    HKEY key = 0;
    DWORD disposition = 0;
    LONG returnCode = RegCreateKeyEx(HKEY_CURRENT_USER, LOCAL_SETTINGS_REGISTRY_KEY, 0, 0, 0, KEY_READ, 0, &key, &disposition);
    if (ERROR_SUCCESS != returnCode)
    {
        std::stringstream s;
        s << "Create Registry Key failed (" << returnCode << ")";
        throw std::exception(s.str().c_str());
    }

    DWORD bufferLength = 1;
    char *buffer = (char *)malloc(bufferLength * sizeof(char));

    // Using the ANSI version explicitly.
    returnCode = RegQueryValueExA(key, LOCAL_SETTINGS_REGISTRY_VALUE_SERVERS, 0, 0, (LPBYTE)buffer, &bufferLength);
    while (ERROR_MORE_DATA == returnCode)
    {
        // We must ensure that the string is null terminated, as per MSDN
        buffer = (char *)realloc(buffer, bufferLength + 1);
        buffer[bufferLength - 1] = 0;
        returnCode = RegQueryValueExA(key, LOCAL_SETTINGS_REGISTRY_VALUE_SERVERS, 0, 0, (LPBYTE)buffer, &bufferLength);
    }

    string serverEntryListString(buffer);
    free(buffer);
    RegCloseKey(key);

    if (ERROR_FILE_NOT_FOUND == returnCode)
    {
        return ServerEntries();
    }
    else if (ERROR_SUCCESS != returnCode)
    {
        std::stringstream s;
        s << "Query Registry Value failed (" << returnCode << ")";
        throw std::exception(s.str().c_str());
    }

    return ParseServerEntries(serverEntryListString.c_str());
}

// Adapted from here:
// http://stackoverflow.com/questions/3381614/c-convert-string-to-hexadecimal-and-vice-versa
string Hexlify(const string& input)
{
    static const char* const lut = "0123456789ABCDEF";
    size_t len = input.length();

    string output;
    output.reserve(2 * len);
    for (size_t i = 0; i < len; ++i)
    {
        const char c = input[i];
        output.push_back(lut[c >> 4]);
        output.push_back(lut[c & 15]);
    }
    return output;
}

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

// NOTE: This function does not throw because we don't want a failure to prevent a connection attempt.
void VPNList::WriteListToSystem(const ServerEntries& serverEntryList)
{
    HKEY key = 0;
    DWORD disposition = 0;
    LONG returnCode = RegCreateKeyEx(HKEY_CURRENT_USER, LOCAL_SETTINGS_REGISTRY_KEY, 0, 0, 0, KEY_WRITE, 0, &key, &disposition);
    if (ERROR_SUCCESS != returnCode)
    {
        my_print(false, _T("Create Registry Key failed (%d)"), returnCode);
        return;
    }

    string encodedServerEntryList = EncodeServerEntries(serverEntryList);
    // REG_MULTI_SZ needs two terminating null characters.  We're using REG_SZ right now, but I'm leaving this in anyways.
    int bufferLength = encodedServerEntryList.length() + 2;
    char *buffer = (char *)malloc(bufferLength * sizeof(char));
    sprintf_s(buffer, bufferLength, encodedServerEntryList.c_str());
    buffer[bufferLength - 1] = 0;
    buffer[bufferLength - 2] = 0;

    // Using the ANSI version explicitly.
    returnCode = RegSetValueExA(key, LOCAL_SETTINGS_REGISTRY_VALUE_SERVERS, 0, REG_SZ, (PBYTE)buffer, bufferLength);
    if (ERROR_SUCCESS != returnCode)
    {
        my_print(false, _T("Set Registry Value failed (%d)"), returnCode);
    }
    free(buffer);

    RegCloseKey(key);
}

string VPNList::EncodeServerEntries(const ServerEntries& serverEntryList)
{
    string encodedServerList;
    for (ServerEntryIterator it = serverEntryList.begin(); it != serverEntryList.end(); ++it)
    {
        stringstream port;
        port << it->webServerPort;
        string serverEntry = it->serverAddress + " " + port.str() + " " + it->webServerSecret;
        encodedServerList += Hexlify(serverEntry) + "\n";
    }
    return encodedServerList;
}
