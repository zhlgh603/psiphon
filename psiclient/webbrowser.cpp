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
#include "psiclient.h"
#include "webbrowser.h"
#include "winuser.h"
#include "Tlhelp32.h"

#include <string>

using namespace std;

WebBrowser::WebBrowser(void)
{
    ZeroMemory(&m_pi, sizeof(m_pi));
}

WebBrowser::~WebBrowser(void)
{
    Close();
}

void WebBrowser::Open(void)
{
    Close();

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    string command_line = "IEXPLORE.EXE ";
    command_line += WEB_BROWSER_HOME_PAGE;

    // Start the child process.
    if (!CreateProcessA(
                "C:\\Program Files (x86)\\Internet Explorer\\IEXPLORE.EXE",
                const_cast<char*>(command_line.c_str()),
                NULL, NULL, FALSE, 0, NULL, NULL,
                &si,
                &m_pi)
        && // TODO: should determine the correct path, but maybe this is OK instead
        !CreateProcessA(
                "C:\\Program Files\\Internet Explorer\\IEXPLORE.EXE",
                const_cast<char*>(command_line.c_str()),
                NULL, NULL, FALSE, 0, NULL, NULL,
                &si,
                &m_pi))
    {
        my_print(false, _T("CreateProcess failed (%d)"), GetLastError());
    }
}

BOOL __stdcall EnumWindowsProc(HWND hWnd, LPARAM lParam)
{
    // close all top-level windows of class "IEFrame" belonging to target process

    DWORD dwProcessID = NULL;
    TCHAR szClassName[1024];

    GetWindowThreadProcessId(hWnd, &dwProcessID);
    if (!GetClassName(hWnd, szClassName, sizeof(szClassName)/sizeof(TCHAR) - 1))
    {
        my_print(false, _T("GetClassName failed (%d)"), GetLastError());
        return TRUE;
    }

    if (dwProcessID == (DWORD)lParam && _tcscmp(szClassName, _T("IEFrame")) == 0)
    {
        // TODO: use EndTask?
        // http://msdn.microsoft.com/en-us/library/ms633492%28VS.85%29.aspx
        //EndTask(hWnd, FALSE, TRUE);

        SendMessage(hWnd, WM_CLOSE, 0, 0);
        Sleep(100);
    }

    return TRUE;
}

void CloseWindows(DWORD dwProcessId)
{
    EnumWindows(EnumWindowsProc, (LPARAM)dwProcessId);
}

void CloseIEProcesses(void)
{
    HANDLE hSnapShot;
    BOOL bContinue;
    PROCESSENTRY32 stProcessEntry;

    hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    ZeroMemory(&stProcessEntry, sizeof(stProcessEntry));
    stProcessEntry.dwSize = sizeof(stProcessEntry);
    bContinue = Process32First(hSnapShot, &stProcessEntry);

    while (bContinue)
    {
        if (0 == _tcsicmp(stProcessEntry.szExeFile, _T("IEXPLORE.EXE")))
        {
            CloseWindows(stProcessEntry.th32ProcessID);
        }

        ZeroMemory(&stProcessEntry, sizeof(stProcessEntry));
        stProcessEntry.dwSize = sizeof(stProcessEntry);
        bContinue = Process32Next(hSnapShot, &stProcessEntry);
    }

    CloseHandle(hSnapShot);
}

bool IsIEProcessRunning(void)
{
    bool bResult = false;
    HANDLE hSnapShot;
    BOOL bContinue;
    PROCESSENTRY32 stProcessEntry;

    hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    ZeroMemory(&stProcessEntry, sizeof(stProcessEntry));
    stProcessEntry.dwSize = sizeof(stProcessEntry);
    bContinue = Process32First(hSnapShot, &stProcessEntry);

    while (bContinue)
    {
        if (0 == _tcsicmp(stProcessEntry.szExeFile, _T("IEXPLORE.EXE")))
        {
            bResult = true;
            break;
        }

        ZeroMemory(&stProcessEntry, sizeof(stProcessEntry));
        stProcessEntry.dwSize = sizeof(stProcessEntry);
        bContinue = Process32Next(hSnapShot, &stProcessEntry);
    }

    CloseHandle(hSnapShot);

    return bResult;
}

extern HWND g_hWnd;

void WebBrowser::Close(void)
{
    if (m_pi.dwProcessId != 0 && IsIEProcessRunning())
    {
        // if we spawned an IE process, close *ALL* IE processes
        // we must close all to cover the case where IE was running
        // befoere our spawn and took over our process

        int rc = MessageBox(
                    g_hWnd,
                    _T("Close Internet Explorer?"),
                    _T("PsiphonY"),
                    MB_YESNO|MB_ICONQUESTION|MB_SETFOREGROUND|MB_TOPMOST|MB_APPLMODAL);
        if (rc == IDYES)
        {
            CloseIEProcesses();
    
        }
    }

    ZeroMemory(&m_pi, sizeof(m_pi));
}
