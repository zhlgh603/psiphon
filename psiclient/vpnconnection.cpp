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

VPNConnection::VPNConnection(void)
{
    // Don't use INVALID_HANDLE_VALUE.  This needs to be set to 0 for RasDial to succeed.
    m_rasConnection = 0;
}

VPNConnection::~VPNConnection(void)
{
    Remove();
}

void CALLBACK RasDialCallback(UINT, RASCONNSTATE rasConnState, DWORD dwError)
{
    my_print(false, _T("RasDialCallback (%d %d)"), rasConnState, dwError);
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
    m_rasConnection = 0;
    returnCode = RasDial(0, 0, &vpnParams, 0, &RasDialCallback, &m_rasConnection);
    if (ERROR_SUCCESS != returnCode)
    {
        my_print(false, _T("RasDial failed (%d)"), returnCode);
        return false;
    }

    // Check the connection status
    RASCONNSTATUS status;
    memset(&status, 0, sizeof(status));
    status.dwSize = sizeof(status);
    returnCode = RasGetConnectStatus(m_rasConnection, &status);
    if (ERROR_SUCCESS != returnCode)
    {
        my_print(false, _T("RasGetConnectStatus failed (%d)"), returnCode);
        return false;
    }
    if (RASCS_Connected != status.rasconnstate)
    {
        my_print(false, _T("Failed to connect."));
    }

    return true;
}

bool VPNConnection::Remove(void)
{
    DWORD returnCode = ERROR_SUCCESS;

    // Hangup
    if (0 != m_rasConnection)
    {
        returnCode = RasHangUp(m_rasConnection);
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
        while(ERROR_INVALID_HANDLE != RasGetConnectStatus(m_rasConnection, &status))
        {
            Sleep(0);
        }

        m_rasConnection = 0;
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
