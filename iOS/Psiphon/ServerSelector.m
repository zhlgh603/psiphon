//
//  ServerSelector.m
//  Psiphon
//
//  Created by eugene on 2013-11-15.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import "ServerSelector.h"
#import "ServerEntry.h"
#import "CheckServerReachabilityOperation.h"
#import "ServerInterface.h"
#import "Utils.h"

@implementation ServerSelector
@synthesize stopFlag;

-(id) initWithServerInterface: (ServerInterface*)serverInterface;
{
    self = [super init];
    if (self) {
        
        _serverInterface = serverInterface;
        _queue = [[NSOperationQueue alloc]init];
        [_queue setMaxConcurrentOperationCount:REACHABILITY_MAX_NUM_OPERATIONS];
        stopFlag = FALSE;
    }
    return self;
}

-(void) runSelector
{
    NSArray* serverEntries = [_serverInterface getServerEntries];
    
    NSMutableArray* ops = [[NSMutableArray alloc]initWithCapacity:serverEntries.count];
    NSMutableArray* respondingServers = [[NSMutableArray alloc]init];
    
    // Remember the original first entry
    ServerEntry* originalFirstEntry = nil;
    int entriesCount = [serverEntries count];
    if(entriesCount > 0)
    {
        originalFirstEntry = [serverEntries objectAtIndex:0];
        
        if(entriesCount > REACHABILITY_MAX_NUM_OPERATIONS)
        {
            [Utils shuffleSubArray:&serverEntries from:REACHABILITY_MAX_NUM_OPERATIONS to:entriesCount-1];
        }
        
        for (ServerEntry* entry in serverEntries)
        {
            CheckServerReachabilityOperation* op = [[CheckServerReachabilityOperation alloc] initWithServerEntry:entry];
            [ops addObject:op];
        }
        
        [_queue addOperations:ops waitUntilFinished:NO];
        
        for(float wait = 0.0;
            !stopFlag && wait <= REACHABILITY_CONNECT_TIMEOUT_SECONDS;
            wait += REACHABILITY_SELECTOR_POLL_SECONDS)
        {
            if(wait > 0.0)
            {
                int resultCount = 0;
                BOOL workQueueIsFinished = TRUE;
                
                for(CheckServerReachabilityOperation* op in ops)
                {
                    if(!op.completed)
                    {
                        workQueueIsFinished = FALSE;
                        continue;
                    }
                    
                    resultCount += op.responded ? 1 : 0;
                    if (resultCount > 0)
                    {
                        // Use the results we have so far
                        stopFlag = true;
                        break;
                    }
                    
                }
                if (workQueueIsFinished)
                {
                    break;
                }
            }
            
            [NSThread sleepForTimeInterval:REACHABILITY_SELECTOR_POLL_SECONDS];
        }
        
        [_queue cancelAllOperations];
        
        for(CheckServerReachabilityOperation* op in ops)
        {
            if(op.responded == YES)
            {
                [respondingServers addObject:op.serverEntry];
            }
        }
    }
    
    if ([respondingServers count] > 0)
    {
        [Utils shuffle:&respondingServers];

        // If the original first entry is a faster responder, keep it as the first entry.
        // This is to increase the chance that users have a "consistent" outbound IP address,
        // while also taking performance and load balancing into consideration (this is
        // a fast responder; and it ended up as the first entry randomly).
        if (originalFirstEntry != nil)
        {
            for (int i = 0; i < respondingServers.count; i++)
            {
                ServerEntry* serverEntry = [respondingServers objectAtIndex:i];
                if ([originalFirstEntry.ipAddress isEqualToString: serverEntry.ipAddress])
                {
                    if(i != 0)
                    {
                        [respondingServers exchangeObjectAtIndex:i withObjectAtIndex:0];
                        break;
                    }
                }
            }
        }
    
        [_serverInterface moveEntriesToFront:respondingServers];
    }
    else
    {
        [_serverInterface fetchRemoteServerList];
    }
}

@end
