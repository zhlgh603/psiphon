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

#include <tchar.h>

static const TCHAR* VPN_CONNECTION_NAME = _T("PsiphonV");

static const int MAX_WORKER_THREAD_POOL_SIZE = 5;

static const char* LOCAL_LISTEN_IP = "127.0.0.1";
static const unsigned short LOCAL_LISTEN_PORT = 8080;
static const char* HTTP_PROXY_HOST = "localhost"; /* resolved by the server */ 

static const char* WEB_BROWSER_HOME_PAGE = "https://www.facebook.com";

// TODO: VPN settings, home page