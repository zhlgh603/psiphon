//
//  ServerEntry.h
//  Psiphon
//
//  Created by eugene on 2013-10-08.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

@interface ServerEntry : NSObject < NSCopying > 

@property NSString* encodedEntry;
@property NSString* ipAddress;
@property int webServerPort;
@property NSString* webServerSecret;
@property NSString* webServerCertificate;
@property int sshPort;
@property NSString* sshUsername;
@property NSString* sshPassword;
@property NSString* sshHostKey;
@property int sshObfuscatedPort;
@property NSString* sshObfuscatedKey;
@property NSMutableArray *capabilities;

@property(readonly) NSString* CAPABILITY_HANDSHAKE;
@property(readonly) NSString* CAPABILITY_VPN;
@property(readonly) NSString* CAPABILITY_OSSH;
@property(readonly) NSString* CAPABILITY_SSH;


- (bool)hasCapability:(NSString*)capability;
- (bool)hasCapabilities:(NSArray*)capabilities;
- (int) getPreferredReachablityTestPort;



@end