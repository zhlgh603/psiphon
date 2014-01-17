//
//  ServerSelector.h
//  Psiphon
//
//  Created by eugene on 2013-11-15.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import <Foundation/Foundation.h>
@class ServerInterface;

@interface ServerSelector : NSObject
{
    NSOperationQueue* _queue;
    NSMutableArray* _reorderedServerArray;
    ServerInterface* _serverInterface;
}

@property  BOOL stopFlag;


-(id) initWithServerInterface: (ServerInterface*)serverInterface;
-(void) runSelector;


@end
