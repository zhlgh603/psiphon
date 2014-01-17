//
//  PsiphonConstants.m
//  Psiphon
//
//  Created by eugene on 2013-10-09.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import "PsiphonConstants.h"

BOOL IS_DEBUG = false; // may be changed by application
NSString* const SERVER_ENTRY_FILENAME = @"psiphon_server_entries.json";
NSString* const LAST_CONNECTED_FILENAME = @"last_connected";
const int CLIENT_SESSION_ID_SIZE_IN_BYTES = 16;
const int SOCKS_PORT = 1080;
const int DEFAULT_WEB_SERVER_PORT = 443;
NSString* const RELAY_PROTOCOL = @"OSSH";
NSString* const LOG_ENTRY_NOTIFICATION_KEY = @"LogEntryKey";
NSString* const NEW_LOG_ENTRY_POSTED = @"NewLogEntryPosted";
const int SSH_CONNECT_TIMEOUT_SECONDS = 20;
const int REACHABILITY_MAX_NUM_OPERATIONS = 10;
const float REACHABILITY_CONNECT_TIMEOUT_SECONDS = 20.f;
const float REACHABILITY_SELECTOR_POLL_SECONDS = 0.1f;
const int SECONDS_BETWEEN_SUCCESSFUL_REMOTE_SERVER_LIST_FETCH = 60*60*6;
const int SECONDS_BETWEEN_UNSUCCESSFUL_REMOTE_SERVER_LIST_FETCH = 60*5;

