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
#include "embeddedvalues.h"

#pragma comment (lib, "crypt32.lib")

HTTPSRequest::HTTPSRequest(void)
{
}

HTTPSRequest::~HTTPSRequest(void)
{
}

bool HTTPSRequest::GetRequest(
        bool& cancel,
        const TCHAR* serverAddress,
        int serverWebPort,
        const TCHAR* requestPath,
        string& response)
{
    // TODO:
    // Use asynchronous mode for cleaner and more effectively cancel functionality:
    // http://msdn.microsoft.com/en-us/magazine/cc716528.aspx
    // http://msdn.microsoft.com/en-us/library/aa383138%28v=vs.85%29.aspx
    // http://msdn.microsoft.com/en-us/library/aa384115%28v=VS.85%29.aspx

    BOOL bRet = FALSE;
    CERT_CONTEXT *pCert = {0};
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

    if (cancel)
    {
        return false;
    }

    AutoHINTERNET hConnect =
            WinHttpConnect(
                hSession,
	            serverAddress,
	            serverWebPort,
	            0);

    if (NULL == hConnect)
    {
	    my_print(false, _T("WinHttpConnect failed (%d)"), GetLastError());
        return false;
    }

    if (cancel)
    {
        return false;
    }

    AutoHINTERNET hRequest =
            WinHttpOpenRequest(
                    hConnect,
	                _T("GET"),
	                requestPath,
	                NULL,
	                WINHTTP_NO_REFERER,
	                WINHTTP_DEFAULT_ACCEPT_TYPES,
	                WINHTTP_FLAG_SECURE); 

    if (NULL == hRequest)
    {
	    my_print(false, _T("WinHttpOpenRequest failed (%d)"), GetLastError());
        return false;
    }

    if (cancel)
    {
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

    if (cancel)
    {
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

    if (cancel)
    {
        return false;
    }

    bRet = WinHttpReceiveResponse(hRequest, NULL);

    if (FALSE == bRet)
    {
	    my_print(false, _T("WinHttpReceiveResponse failed (%d)"), GetLastError());
        return false;
    }

    if (cancel)
    {
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
 //       return false;
    }

    if (cancel)
    {
        return false;
    }

    bRet = WinHttpQueryDataAvailable(hRequest, &dwLen);

    if (FALSE == bRet)
    {
	    my_print(false, _T("WinHttpQueryDataAvailable failed (%d)"), GetLastError());
        return false;
    }

    if (cancel)
    {
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

    response = string((const char*)pBuffer, dwLen);
    my_print(false, _T("got: %s"), NarrowToTString(response).c_str());

    HeapFree(GetProcessHeap(), 0, pBuffer);

    dwLen = sizeof(pCert);

    if (cancel)
    {
        return false;
    }

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

    if (cancel)
    {
        return false;
    }

    my_print(false, _T("cert encoding %d %d"), pCert->dwCertEncodingType, pCert->cbCertEncoded);

    return true;
}

bool HTTPSRequest::ValidateServerCert(PCCERT_CONTEXT pCert)
{
    char* pemStr = NULL; // pem cert string
    BYTE* pbBinary = NULL; //base64 decoded pem
    DWORD cbBinary; //base64 decoded pem size
    HCERTSTORE hCertMemStore = NULL;
    bool bResult = false;

    // Open the store
    hCertMemStore =CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0,
					CERT_STORE_CREATE_NEW_FLAG, NULL);


    if (!(hCertMemStore))
    {
	    my_print(false, _T("CertOpenStore() failed"));
        return false;
    }

    //strip '-----BEGIN CERTIFICATE-----' and '-----END CERTIFICATE-----' from PSIPHON_CA_CERT
    const char* strBeginCert = "-----BEGIN CERTIFICATE-----";
    const char* strEndCert = "-----END CERTIFICATE-----";

    const char* pBegin, *pEnd;
    int nStrLength = 0;

    if ((pBegin = strstr(PSIPHON_CA_CERT,strBeginCert)) &&
        (pEnd = strstr(PSIPHON_CA_CERT,strEndCert)) &&
        pEnd > pBegin )
    {
        nStrLength = [strlen(PSIPHON_CA_CERT) - strlen(strBeginCert) -  strlen(strBeginCert);
        pemStr = new char[nStrLength + 1];
        strncpy(pemStr, pBegin + strlen(strBeginCert), nStrLength);
    }
    else
    {
	    my_print(false, _T("Couldn't get the pem string"));
        return false;
    }
    
    //Base64 decode pem string to BYTE*
    
    //Get the expected pbBinary length
    CryptStringToBinaryA( (LPCSTR)pemStr, (DWORD) strlen(pemStr), CRYPT_STRING_BASE64, NULL, &cbBinary, NULL, NULL);

    pbBinary = new BYTE[cbBinary];

    //Perform base64 decode
    CryptStringToBinaryA( (LPCSTR)pemStr, (DWORD) strlen(pemStr), CRYPT_STRING_BASE64, pbBinary, &cbBinary, NULL, NULL);

    delete pemStr;

    //Add cert to the store
    bResult = (bool)CertAddEncodedCertificateToStore(hCertMemStore, 
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 
        pbBinary, 
        cbBinary, 
        CERT_STORE_ADD_NEW, 
        NULL);

    delete pbBinary;

    if(!bResult)
    {
	    my_print(false, _T("CertAddEncodedCertificateToStore() failed"));
        return false;
    }

    bResult = VerifyCert(hCertMemStore, pCert);
    CertCloseStore(hCertMemStore, 0);
    return bResult;
}


bool HTTPSRequest::VerifyCert(HCERTSTORE hCertStore, PCCERT_CONTEXT pSubjectContext)
{

  //taken from http://etutorials.org/Programming/secure+programming/Chapter+10.+Public+Key+Infrastructure/10.6+Performing+X.509+Certificate+Verification+with+CryptoAPI/

  DWORD           dwFlags;
  PCCERT_CONTEXT  pIssuerContext;
   
  if (!(pSubjectContext = CertDuplicateCertificateContext(pSubjectContext)))
    return false;
  do {
    dwFlags = CERT_STORE_REVOCATION_FLAG | CERT_STORE_SIGNATURE_FLAG |
              CERT_STORE_TIME_VALIDITY_FLAG;
    pIssuerContext = CertGetIssuerCertificateFromStore(hCertStore,
                                                pSubjectContext, 0, &dwFlags);
    CertFreeCertificateContext(pSubjectContext);
    if (pIssuerContext) {
      (PCCERT_CONTEXT)pSubjectContext = pIssuerContext;
      if (dwFlags & CERT_STORE_NO_CRL_FLAG)
        dwFlags &= ~(CERT_STORE_NO_CRL_FLAG | CERT_STORE_REVOCATION_FLAG);
      if (dwFlags) break;
    } else if (GetLastError(  ) == CRYPT_E_SELF_SIGNED) 
    {
        return true;
    }
  } while (pIssuerContext);
  return false;
}
