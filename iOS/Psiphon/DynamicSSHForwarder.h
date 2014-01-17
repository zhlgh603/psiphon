//
//  DynamicSSHForwarder.h
//  Psiphon
//
//  Created by eugene on 2013-10-29.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "psi_helper.h"
#import "DynamicSSHForwarderDelegate.h"

@interface DynamicSSHForwarder : NSObject
{
    @private
    int _localPort;
    LIBSSH2_SESSION* _session;
    NSThread* _forwarderThread;
}

@property __weak id<DynamicSSHForwarderDelegate> delegate;
@property (readonly) BOOL isStopped;


- (id) initWithSession:(LIBSSH2_SESSION*)session andPort:(int) port;
- (void) start;
- (void) stop;

@end
