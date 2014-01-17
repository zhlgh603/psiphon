//
//  CheckServerReachabilityOperation.h
//  Psiphon
//
//  Created by eugene on 2013-11-15.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import <Foundation/Foundation.h>
@class ServerEntry;

@interface CheckServerReachabilityOperation : NSOperation

@property BOOL responded;
@property BOOL completed;
@property NSTimeInterval responseTime;
@property ServerEntry* serverEntry;


- (id) initWithServerEntry:(ServerEntry*)entry;

@end
