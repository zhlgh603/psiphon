//
//  TunnelCore.h
//  Psiphon
//
//  Created by eugene on 2013-08-12.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "EventsDelegate.h"
#import "ServerInterfaceDelegate.h"
#import "DynamicSSHForwarder.h"
#import "DynamicSSHForwarderDelegate.h"


@class ServerInterface;

@interface TunnelCore : NSObject<ServerInterfaceDelegate, DynamicSSHForwarderDelegate>
{
    @private
    ServerInterface* _serverInterface;
    DynamicSSHForwarder * _forwarder;
    NSThread* _tunnelThread;
    NSCondition* _tunnelCondition;
    BOOL _tunnelShouldStop;
}

@property __weak id<EventsDelegate> eventsDelegate;

-(void) startTunnel;
-(void) stopTunnel;

@end
