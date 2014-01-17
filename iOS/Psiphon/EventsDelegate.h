//
//  EventsDelegate.h
//  Psiphon
//
//  Created by eugene on 2013-08-09.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol EventsDelegate <NSObject>

@required
-(void) signalHandshakeSuccess;
-(void) signalUnexpectedDisconnect;
@end


