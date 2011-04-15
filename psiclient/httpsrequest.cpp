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
#include "httpsrequest.h"
#include "psiclient.h"
#include <Winhttp.h>
#include <WinCrypt.h>

HTTPSRequest::HTTPSRequest(void)
{
}

HTTPSRequest::~HTTPSRequest(void)
{
}

class AutoHINTERNET
{
public:
    AutoHINTERNET(HINTERNET handle) {m_handle = handle;}
    ~AutoHINTERNET() {WinHttpCloseHandle(m_handle);}
    operator HINTERNET() {return m_handle;}
private:
    HINTERNET m_handle;
};

bool HTTPSRequest::GetRequest(const char* url, string& response)
{
    BOOL bRet = FALSE;
    CERT_CONTEXT *pCert = {0};
    HCERTSTORE hCertStore = NULL;
    DWORD dwRet = 0;
    DWORD dwLen = 0;
    DWORD dwStatusCode;
    DWORD dwFlags = SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
				    SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
				    SECURITY_FLAG_IGNORE_UNKNOWN_CA;
    LPVOID pBuffer = NULL;

    AutoHINTERNET hSession =
                WinHttpOpen(
                    _T("Mozilla/4.0 (compatible; MSIE 5.22)"),
	                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
	                WINHTTP_NO_PROXY_NAME,
	                WINHTTP_NO_PROXY_BYPASS,
                    0 );

    if (NULL == hSession)
    {
	    my_print(false, _T("WinHttpOpen failed (%d)"), GetLastError());
        return false;
    }

    AutoHINTERNET hConnect =
            WinHttpConnect(
                hSession,
	            _T("a.psiphon.ca"),
	            INTERNET_DEFAULT_HTTPS_PORT,
	            0);

    if (NULL == hConnect)
    {
	    my_print(false, _T("WinHttpConnect failed (%d)"), GetLastError());
        return false;
    }

    AutoHINTERNET hRequest =
            WinHttpOpenRequest(
                    hConnect,
	                _T("GET"),
	                _T("/001"),
	                NULL,
	                WINHTTP_NO_REFERER,
	                WINHTTP_DEFAULT_ACCEPT_TYPES,
	                WINHTTP_FLAG_SECURE); 

    if (NULL == hRequest)
    {
	    my_print(false, _T("WinHttpOpenRequest failed (%d)"), GetLastError());
        return false;
    }

    bRet = WinHttpSetOption(
	            hRequest,
	            WINHTTP_OPTION_SECURITY_FLAGS,
	            &dwFlags,
	            sizeof(DWORD));

    if (FALSE == bRet)
    {
	    my_print(false, _T("WinHttpSetOption failed (%d)"), GetLastError());
        return false;
    }

    bRet = WinHttpSendRequest(
                hRequest,
	            WINHTTP_NO_ADDITIONAL_HEADERS,
	            0,
	            WINHTTP_NO_REQUEST_DATA,
	            0,
	            0,
	            0);

    if (FALSE == bRet)
    {
	    my_print(false, _T("WinHttpSendRequest failed (%d)"), GetLastError());
        return false;
    }

    bRet = WinHttpReceiveResponse(hRequest, NULL);

    if (FALSE == bRet)
    {
	    my_print(false, _T("WinHttpReceiveResponse failed (%d)"), GetLastError());
        return false;
    }

    dwLen = sizeof(dwStatusCode);

    bRet = WinHttpQueryHeaders(
                    hRequest, 
                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    NULL, 
                    &dwStatusCode, 
                    &dwLen, 
                    NULL);

    if (FALSE == bRet)
    {
	    my_print(false, _T("WinHttpQueryHeaders failed (%d)"), GetLastError());
        return false;
    }

    if (200 != dwStatusCode)
    {
	    my_print(false, _T("Bad HTTP GET request status code: %d"), dwStatusCode);
        return false;
    }

    bRet = WinHttpQueryDataAvailable(hRequest, &dwLen);

    if (FALSE == bRet)
    {
	    my_print(false, _T("WinHttpQueryDataAvailable failed (%d)"), GetLastError());
        return false;
    }

    pBuffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwLen);

    if (!pBuffer)
    {
        my_print(false, _T("HeapAlloc failed"));
        return false;
    }
    
    bRet = WinHttpReadData(hRequest, pBuffer, dwLen, &dwLen);

    if (FALSE == bRet)
    {
	    my_print(false, _T("WinHttpReadData failed (%d)"), GetLastError());
        HeapFree(GetProcessHeap(), 0, pBuffer);
        return false;
    }

    my_print(false, _T("got: %s"), string((const char*)pBuffer, dwLen).c_str());

    HeapFree(GetProcessHeap(), 0, pBuffer);

    dwLen = sizeof(pCert);

    bRet = WinHttpQueryOption(
	            hRequest,
	            WINHTTP_OPTION_SERVER_CERT_CONTEXT,
	            &pCert,
	            &dwLen);

    if (NULL == pCert)
    {
	    my_print(false, _T("WinHttpQueryOption failed (%d)"), GetLastError());
        return false;
    }

    my_print(false, _T("cert encoding %d %d"), pCert->dwCertEncodingType, pCert->cbCertEncoded);

    return true;
}
