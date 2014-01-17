//
//  ServerInterface.h
//  Psiphon
//
//  Created by eugene on 2013-10-08.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "PsiphonConstants.h"
#import "ServerInterfaceDelegate.h"

@class ServerEntry;
@class TunnelCore;
typedef void (^PsiphonServerResponseBlock)(NSString* responseString);
typedef MKNKResponseErrorBlock PsiphonServerErrorBlock ;

@interface ServerInterface : NSObject {
     NSMutableArray* _serverEntries;
     NSString* _serverSessionID;
     ServerEntry* _currentServerEntry;
     MKNetworkEngine* _engine;
}

@property __weak id<ServerInterfaceDelegate> delegate;

-(void)generateNewCurrentClientSessionID;
-(ServerEntry*) setCurrentServerEntry;
-(ServerEntry*)  getCurrentServerEntry;
- (void) sendHandshakeRequest;
- (void) sendConnectedRequest;
- (void) fetchRemoteServerList;
-(BOOL) serverWithCapabilitiesExists:(NSArray*) capabilities;
-(void) moveEntriesToFront:(NSArray*) reorderedEntries;
-(NSArray*) getServerEntries;

@end
