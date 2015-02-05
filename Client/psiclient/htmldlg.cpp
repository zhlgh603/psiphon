/*
 * Copyright (c) 2012, Psiphon Inc.
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

// Adapted from the MSDN htmldlg demo. Available here: http://www.microsoft.com/en-us/download/details.aspx?id=944

#include "stdafx.h"
#include "htmldlg.h"

// Also requires this includes in stdafx.h:
//#include <mshtmhst.h>
//#include <mshtml.h>
//#include <comdef.h>
//#include <comdefsp.h>


/**************************************************************************

   ShowHTMLDlg()

**************************************************************************/

int ShowHTMLDlg(
        HWND hParentWnd, 
        LPCTSTR resourceName, 
        LPCTSTR urlFragment,
        LPCWSTR args,
        wstring& o_result)
{
    o_result.clear();

    // Contrary to the documentation, passing NULL for pvarArgIn 
    // seems to always result in ShowHTMLDialog returning a "bad
    // variable type" error. So we're going to set args to empty 
    // string if not provided.
    if (!args) args = L"";

    bool error = false;

    HINSTANCE hinstMSHTML = LoadLibrary(TEXT("MSHTML.DLL"));

    if (hinstMSHTML)
    {
        SHOWHTMLDIALOGEXFN *pfnShowHTMLDialog;

        pfnShowHTMLDialog = (SHOWHTMLDIALOGEXFN*)GetProcAddress(hinstMSHTML, "ShowHTMLDialogEx");

        if (pfnShowHTMLDialog)
        {
            tstring url;

            url += _T("res://");
            
            TCHAR   szTemp[MAX_PATH*2];
            GetModuleFileName(NULL, szTemp, ARRAYSIZE(szTemp));
            url += szTemp;

            url += _T("/");
            url += resourceName;
            
            if (urlFragment) 
            {
                url += _T("#");
                url += urlFragment;
            }

            IMoniker* pmk = NULL;
            CreateURLMonikerEx(NULL, url.c_str(), &pmk, URL_MK_UNIFORM);

            if (pmk)
            {
                HRESULT  hr;
                VARIANT  varArgs, varReturn;

                VariantInit(&varArgs);
                varArgs.vt = VT_BSTR;
                varArgs.bstrVal = SysAllocString(args);

                VariantInit(&varReturn);

                hr = (*pfnShowHTMLDialog)(
                        hParentWnd, 
                        pmk, 
                        HTMLDLG_MODAL | HTMLDLG_VERIFY, 
                        &varArgs, 
                        L"resizable:yes;", 
                        &varReturn);

                VariantClear(&varArgs);

                pmk->Release();

                if (SUCCEEDED(hr))
                {
                    switch (varReturn.vt)
                    {
                    case VT_BSTR:
                        {
                            o_result = wstring(varReturn.bstrVal, SysStringLen(varReturn.bstrVal));

                            VariantClear(&varReturn);
                        }
                        break;

                    default:
                        // Dialog was cancelled.
                        break;
                    }
                }
                else
                {
                    error = true;
                }

            }
            else
            {
                error = true;
            }
        }
        else
        {
            error = true;
        }

        FreeLibrary(hinstMSHTML);
    }
    else
    {
        error = true;
    }

    if (error) return -1;
    return o_result.length() > 0 ? 1 : 0;
}

/**************************************************************************

ShowModelessHTMLDlg()

**************************************************************************/

int ShowModelessHTMLDlg(
	HWND hParentWnd,
	LPCTSTR resourceName,
	LPCTSTR urlFragment,
	LPCWSTR args,
	wstring& o_result)
{
	o_result.clear();

	// Contrary to the documentation, passing NULL for pvarArgIn 
	// seems to always result in ShowHTMLDialog returning a "bad
	// variable type" error. So we're going to set args to empty 
	// string if not provided.
	if (!args) args = L"";

	bool error = false;

	HINSTANCE hinstMSHTML = LoadLibrary(TEXT("MSHTML.DLL"));

	if (hinstMSHTML)
	{
		SHOWHTMLDIALOGEXFN *pfnShowHTMLDialog;

		pfnShowHTMLDialog = (SHOWHTMLDIALOGEXFN*)GetProcAddress(hinstMSHTML, "ShowHTMLDialogEx");

		if (pfnShowHTMLDialog)
		{
			tstring url;

			url += _T("res://");

			TCHAR   szTemp[MAX_PATH * 2];
			GetModuleFileName(NULL, szTemp, ARRAYSIZE(szTemp));
			url += szTemp;

			url += _T("/");
			url += resourceName;

			if (urlFragment)
			{
				url += _T("#");
				url += urlFragment;
			}

			IMoniker* pmk = NULL;
			CreateURLMonikerEx(NULL, url.c_str(), &pmk, URL_MK_UNIFORM);

			if (pmk)
			{
				HRESULT  hr;
				VARIANT  varArgs, varReturn;

				VariantInit(&varArgs);
				varArgs.vt = VT_BSTR;
				varArgs.bstrVal = SysAllocString(args);

				VariantInit(&varReturn);

				hr = (*pfnShowHTMLDialog)(
					hParentWnd,
					pmk,
					HTMLDLG_MODELESS | HTMLDLG_VERIFY,
					&varArgs,
					L"resizable:yes;",
					&varReturn);

				VariantClear(&varArgs);

				pmk->Release();

				if (SUCCEEDED(hr))
				{
					switch (varReturn.vt)
					{
					case VT_UNKNOWN:
					{
                        while (true) {
                            IHTMLWindow2Ptr pWindow;
                            pWindow = (IHTMLWindow2*)varReturn.punkVal;

                            VariantClear(&varReturn);

                            HRESULT hresult;

                            IHTMLDocument2* pDocument;
                            hresult = pWindow->get_document(&pDocument);

                            IDispatch* pScript;
                            hresult = pDocument->get_Script(&pScript);

                            HtmlUi htmlUi(pScript);
                            
                            pScript->Release();

                            wstring fnResult;
                            bool success = htmlUi.CallScript(fnResult, L"whatisdate");

                            int i = 0;
                            

                            /*
                            OLECHAR * szMember = L"whatisdate";
                            DISPID dispid;
                            hresult = pScript->GetIDsOfNames(IID_NULL, &szMember, 1, LOCALE_SYSTEM_DEFAULT, &dispid);

                            DISPPARAMS parameters = { 0 };
                            VARIANT result;
                            VariantInit(&result);
                            EXCEPINFO exception = { 0 };
                            HRESULT hr = pScript->Invoke(
                                dispid,
                                IID_NULL,
                                LOCALE_USER_DEFAULT,
                                DISPATCH_METHOD,
                                &parameters,
                                &result,
                                &exception,
                                0);

                            int i = 0;
                            VariantClear(&result);
                            */

                            Sleep(100);
                            break;


                            /*
                            CComVariant result;
                            CComDispatchDriver disp = pWindow; // of IHTMLWindow2
                            disp.Invoke1(L"eval", &CComVariant(L"confirm('See this?')"), &result);
                            result.ChangeType(VT_BSTR);
                            MessageBoxW(V_BSTR(&result));
                            */

                            /*
                            BSTR bsLanguage = SysAllocString(L"javascript");

                            while (true)
                            {
                                BSTR bsScript = SysAllocString(L"alert('ohhi');whatisdate()");
                                VARIANT vresult;
                                wstring res;
                                if (pWindow->execScript(bsScript, bsLanguage, &vresult) == S_OK)
                                {
                                    res = wstring(vresult.bstrVal, SysStringLen(vresult.bstrVal));
                                }
                                SysFreeString(bsScript);
                                VariantClear(&vresult);
                                Sleep(200);
                            }

                            SysFreeString(bsLanguage);
                            */
                        }
					}
					break;

					default:
						// Dialog was cancelled.
						break;
					}
				}
				else
				{
					error = true;
				}

			}
			else
			{
				error = true;
			}
		}
		else
		{
			error = true;
		}

		FreeLibrary(hinstMSHTML);
	}
	else
	{
		error = true;
	}

	if (error) return -1;
	return o_result.length() > 0 ? 1 : 0;
}


HtmlUi::HtmlUi(IDispatch* scriptEngine)
    : m_scriptEngine(scriptEngine)
{
}

HtmlUi::~HtmlUi()
{
}

bool HtmlUi::CallScript(wstring& o_result, const wchar_t* scriptFn, ...)
{
    o_result.clear();

    HRESULT hr;
    OLECHAR* fnName[] = { (wchar_t*)scriptFn };
    const UINT fnNameCount = 1;
    DISPID fnId;
    hr = m_scriptEngine->GetIDsOfNames(
        IID_NULL, 
        fnName, 
        fnNameCount, 
        LOCALE_SYSTEM_DEFAULT, 
        &fnId);

    DISPPARAMS parameters = { 0 };
    VARIANT result;
    VariantInit(&result);
    hr = m_scriptEngine->Invoke(
        fnId,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_METHOD,
        &parameters,
        &result,
        NULL,
        NULL);

    if (FAILED(hr) || result.vt != VT_BSTR)
    {
        VariantClear(&result);
        return false;
    }

    o_result = result.bstrVal;
    VariantClear(&result);

    return true;
}
