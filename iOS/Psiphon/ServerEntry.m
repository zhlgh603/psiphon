//
//  ServerEntry.m
//  Psiphon
//
//  Created by eugene on 2013-10-08.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import "ServerEntry.h"

@implementation ServerEntry

@synthesize encodedEntry, ipAddress, webServerPort, webServerCertificate,
webServerSecret, sshPort, sshUsername, sshPassword, sshObfuscatedKey, sshHostKey,
sshObfuscatedPort, capabilities,
CAPABILITY_HANDSHAKE,
CAPABILITY_OSSH,
CAPABILITY_SSH,
CAPABILITY_VPN;


- (id)init {
    self = [super init];
    if (self) {
        CAPABILITY_VPN = @"VPN";
        CAPABILITY_OSSH = @"OSSH";
        CAPABILITY_SSH = @"SSH";
        CAPABILITY_HANDSHAKE = @"handshake";
    }
    return self;
}

- (bool)hasCapability:(NSString*)Capability
{
    return [self.capabilities containsObject:Capability];
}

- (bool)hasCapabilities:(NSArray*)Capabilities
{
    NSSet *capabilitiesSet = [NSSet setWithArray:Capabilities];
    return [capabilitiesSet isSubsetOfSet:[NSSet setWithArray:self.capabilities]];
}

-(int)  getPreferredReachablityTestPort
{
    if ([self hasCapability:@"OSSH"])
    {
        return self.sshObfuscatedPort;
    }
    else if ([self hasCapability:@"SSH"])
    {
        return self.sshPort;
    }
    else if ([self hasCapability:@"handshake"])
    {
        return self.webServerPort;
    }
    
    return -1;
}

#pragma mark -
#pragma mark NSCopying protocol methods

- (id)copyWithZone:(NSZone *)zone
{
	ServerEntry *myClassInstanceCopy = [[ServerEntry allocWithZone: zone] init];
    
	myClassInstanceCopy.encodedEntry = [encodedEntry copy];
	myClassInstanceCopy.ipAddress = [ipAddress copy];
	myClassInstanceCopy.webServerPort = webServerPort;
	myClassInstanceCopy.webServerSecret = [webServerSecret copy];
	myClassInstanceCopy.webServerCertificate = [webServerCertificate copy];

    
    myClassInstanceCopy.sshPort = sshPort;
	myClassInstanceCopy.sshUsername = [sshUsername copy];
	myClassInstanceCopy.sshPassword = [sshPassword copy];
	myClassInstanceCopy.sshHostKey = [sshHostKey copy];
	myClassInstanceCopy.sshObfuscatedPort = sshObfuscatedPort;
	myClassInstanceCopy.sshObfuscatedKey = [sshObfuscatedKey copy];
	myClassInstanceCopy.capabilities = [capabilities mutableCopy];

    
	return myClassInstanceCopy;
}

@end