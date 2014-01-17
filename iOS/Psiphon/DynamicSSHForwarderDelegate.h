//
//  DynamicSSHForwarderDelegate.h
//  Psiphon
//
//  Created by eugene on 2013-10-29.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol DynamicSSHForwarderDelegate <NSObject>
@required
-(void) disconnectedSSH;
-(void) readySSHSocks;
@end
