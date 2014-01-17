//
//  ServerInterfaceProtocol.h
//  Psiphon
//
//  Created by eugene on 2013-10-23.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol ServerInterfaceDelegate <NSObject>
@required
-(void) handshakeSuccess:(NSString*) response;
-(void) handshakeFailedWithError:(NSError*) error;
@end
