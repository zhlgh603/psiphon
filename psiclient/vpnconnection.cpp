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
#include "vpnconnection.h"
#include "psiclient.h"
#include "ras.h"
#include "raserror.h"
#include "webbrowser.h"

VPNConnection::VPNConnection(void)
{
}

VPNConnection::~VPNConnection(void)
{
    Remove();
}

void CALLBACK RasDialCallback(UINT, RASCONNSTATE rasConnState, DWORD dwError)
{
    my_print(false, _T("RasDialCallback (%d %d)"), rasConnState, dwError);
    if (0 != dwError)
    {
        my_print(false, _T("Connection failed."));
    }
    else if (RASCS_Connected == rasConnState)
    {
        my_print(false, _T("Successfully connected."));
        OpenBrowser();
    }
}

bool VPNConnection::Establish(void)
{
    DWORD returnCode = ERROR_SUCCESS;

    Remove();

    // The RasValidateEntryName function validates the format of a connection
    // entry name. The name must contain at least one non-white-space alphanumeric character.
    returnCode = RasValidateEntryName(0, VPN_CONNECTION_NAME);
    if (ERROR_SUCCESS != returnCode &&
        ERROR_ALREADY_EXISTS != returnCode)
    {
        my_print(false, _T("RasValidateEntryName failed (%d)"), returnCode);
        return false;
    }

    // Set up the VPN connection properties
    RASENTRY rasEntry;
    memset(&rasEntry, 0, sizeof(rasEntry));
    rasEntry.dwSize = sizeof(rasEntry);
    rasEntry.dwfOptions = RASEO_IpHeaderCompression |
                          RASEO_RemoteDefaultGateway |
                          RASEO_SwCompression |
                          RASEO_RequireMsEncryptedPw |
                          RASEO_RequireDataEncryption |
                          RASEO_ModemLights;
    rasEntry.dwfOptions2 = RASEO2_UsePreSharedKey |
                           RASEO2_DontNegotiateMultilink |
                           RASEO2_SecureFileAndPrint | 
                           RASEO2_SecureClientForMSNet |
                           RASEO2_DisableNbtOverIP;
    rasEntry.dwVpnStrategy = VS_L2tpOnly;
    lstrcpy(rasEntry.szLocalPhoneNumber, _T("192.168.1.250")); //TODO
    rasEntry.dwfNetProtocols = RASNP_Ip;
    rasEntry.dwFramingProtocol = RASFP_Ppp;
    rasEntry.dwEncryptionType = ET_Require;
    lstrcpy(rasEntry.szDeviceType, RASDT_Vpn);
        
    // The RasSetEntryProperties function changes the connection information
    // for an entry in the phone book or creates a new phone-book entry.
    // If the entry name does not match an existing entry, RasSetEntryProperties
    // creates a new phone-book entry.
    returnCode = RasSetEntryProperties(0, VPN_CONNECTION_NAME, &rasEntry, sizeof(rasEntry), 0, 0);
    if (ERROR_SUCCESS != returnCode)
    {
        my_print(false, _T("RasSetEntryProperties failed (%d)"), returnCode);
        return false;
    }

    // Set the Preshared Secret
    RASCREDENTIALS vpnCredentials;
    memset(&vpnCredentials, 0, sizeof(vpnCredentials));
    vpnCredentials.dwSize = sizeof(vpnCredentials);
    vpnCredentials.dwMask = RASCM_PreSharedKey;
    lstrcpy(vpnCredentials.szPassword, _T("1q2w3e4r!")); // TODO
    returnCode = RasSetCredentials(0, VPN_CONNECTION_NAME, &vpnCredentials, FALSE);
    if (ERROR_SUCCESS != returnCode)
    {
        my_print(false, _T("RasSetCredentials failed (%d)"), returnCode);
        return false;
    }

    // Make the vpn connection
    RASDIALPARAMS vpnParams;
    memset(&vpnParams, 0, sizeof(vpnParams));
    vpnParams.dwSize = sizeof(vpnParams);
    lstrcpy(vpnParams.szEntryName, VPN_CONNECTION_NAME);
    lstrcpy(vpnParams.szUserName, _T("user")); // The server does not care about username
    lstrcpy(vpnParams.szPassword, _T("password")); // This can also be hardcoded because
                                                   // the server authentication (which we
                                                   // really care about) is in IPSec using PSK
    HRASCONN rasConnection = 0;
    returnCode = RasDial(0, 0, &vpnParams, 0, &RasDialCallback, &rasConnection);
    if (ERROR_SUCCESS != returnCode)
    {
        my_print(false, _T("RasDial failed (%d)"), returnCode);
        return false;
    }

    return true;
}

bool VPNConnection::Remove(void)
{
    DWORD returnCode = ERROR_SUCCESS;

    LPRASCONN rasConnections = 0;
    DWORD bufferSize = 0;
    DWORD connections = 0;
    returnCode = RasEnumConnections(rasConnections, &bufferSize, &connections);

    if (ERROR_BUFFER_TOO_SMALL == returnCode)
    {
        // Allocate the memory needed for the array of RAS structure(s).
        rasConnections = (LPRASCONN)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bufferSize);
        if (!rasConnections)
        {
            my_print(false, _T("HeapAlloc failed"));
            return false;
        }
 
        // The first RASCONN structure in the array must contain the RASCONN structure size
        rasConnections[0].dwSize = sizeof(RASCONN);
		
        // Call RasEnumConnections to enumerate active connections
        returnCode = RasEnumConnections(rasConnections, &bufferSize, &connections);

        // If successful, find the one with VPN_CONNECTION_NAME.
        if (ERROR_SUCCESS == returnCode)
        {
            for (DWORD i = 0; i < connections; i++)
            {
                if (!_tcscmp(rasConnections[i].szEntryName, VPN_CONNECTION_NAME))
                {
                    // Hangup
                    HRASCONN rasConnection = rasConnections[i].hrasconn;
                    returnCode = RasHangUp(rasConnection);
                    if (ERROR_SUCCESS != returnCode)
                    {
                        my_print(false, _T("RasHangUp failed (%d)"), returnCode);
                    }

                    RASCONNSTATUS status;
                    memset(&status, 0, sizeof(status));
                    status.dwSize = sizeof(status);
                    // Wait until the connection has been terminated.
                    // See the remarks here:
                    // http://msdn.microsoft.com/en-us/library/aa377567(VS.85).aspx
                    while(ERROR_INVALID_HANDLE != RasGetConnectStatus(rasConnection, &status))
                    {
                        Sleep(0);
                    }
                }
		    }
        }

        //Deallocate memory for the connection buffer
        HeapFree(GetProcessHeap(), 0, rasConnections);
        rasConnections = 0;
    }
    else
    {
        if (connections > 0)
        {
            my_print(false, _T("RasEnumConnections failed to acquire the buffer size"));
            return false;
        }
    }

    // Delete the connection
    returnCode = RasDeleteEntry(0, VPN_CONNECTION_NAME);
    if (ERROR_SUCCESS != returnCode &&
        ERROR_CANNOT_FIND_PHONEBOOK_ENTRY != returnCode)
    {
        my_print(false, _T("RasDeleteEntry failed (%d)"), returnCode);
        return false;
    }
    
    return true;
}
