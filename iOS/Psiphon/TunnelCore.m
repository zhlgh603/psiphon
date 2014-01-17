//
//  TunnelCore.m
//  Psiphon
//
//  Created by eugene on 2013-08-12.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import "TunnelCore.h"
#import "ServerInterface.h"
#import "PsiphonData.h"
#import "ServerEntry.h"
#import "Utils.h"
#import "psi_helper.h"
#import "ServerSelector.h"


@implementation TunnelCore

@synthesize eventsDelegate = _eventsDelegate;

- (id) init
{
    if (self = [super init])
    {
        _tunnelCondition = [[NSCondition alloc] init];
        _serverInterface = [[ServerInterface alloc] init];
        _serverInterface.delegate = self;
    }
    return self;
}

- (void) startTunnel
{
    [self stopTunnel];
    _tunnelThread = [[NSThread alloc] initWithTarget:self
                                            selector:@selector(runTunnel)
                                              object:nil];
    [_tunnelThread start];
}

- (void) stopTunnel
{
    [_forwarder stop];

    [_tunnelCondition lock];
    _tunnelShouldStop = YES;
    [_tunnelCondition signal];
    [_tunnelCondition unlock];

}

-(BOOL) runTunnelOnce
{
    int serversock;
    LIBSSH2_SESSION *session = NULL;
    
    [[PsiphonData sharedInstance] setTunnelRelayProtocol: @""];
    [[PsiphonData sharedInstance] setTunnelSessionID: @""];
    [_serverInterface generateNewCurrentClientSessionID];
    
    BOOL runAgain = YES;

    [_tunnelCondition lock];
    _tunnelShouldStop = NO;
    [_tunnelCondition unlock];
    
    ServerSelector* serverSelector = [[ServerSelector alloc] initWithServerInterface:_serverInterface];
    [Utils postLogEntryNotification: @"Running server selector...", nil];
    [serverSelector runSelector];

    /*

    Reachability *reach = [Reachability reachabilityForInternetConnection];
    NetworkStatus status = [reach currentReachabilityStatus];
    if(status == NotReachable)
    {
        runAgain = NO;
        return runAgain;
    }

    */
    ServerEntry *entry = [_serverInterface setCurrentServerEntry];
    
    if (entry == nil)
    {
        [Utils postLogEntryNotification: @"no valid server entries", nil];
        runAgain = NO;
        return runAgain;
    }
    
    if(![entry hasCapability: RELAY_PROTOCOL])
    {
        [Utils postLogEntryNotification: @"server doesn't support relay protocol", nil];
        return runAgain;
    }
    
    //connect to server
    [Utils postLogEntryNotification: @"Connecting to the server...", nil];
    serversock = psi_connect_server((const char*)[[entry ipAddress]UTF8String], [entry sshObfuscatedPort], SSH_CONNECT_TIMEOUT_SECONDS);

    
    
    if(INVALID_SOCKET == serversock)
    {
        [Utils postLogEntryNotification: @"Couldn't connect to the server", nil];
        return runAgain;
    }
    [Utils postLogEntryNotification: @"Connected to the server", nil];

    
    /*
    //We are connected, add reachability observer
    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector(reachabilityChanged:)
                                                 name: kReachabilityChangedNotification
                                               object: nil];

    reach =
    [Reachability reachabilityWithHostname: entry.ipAddress];
    [reach startNotifier];
 */
    
    
    //Start SSH session
    [Utils postLogEntryNotification: @"starting SSH session...", nil];

    session = psi_start_ssh_session(serversock,
                                    (const char*)[[entry sshUsername]UTF8String],
                                    (const char*)[[entry sshPassword]UTF8String],
                                    (const char*)[[entry sshObfuscatedKey]UTF8String],
                                    1);
    if(NULL == session)
    {
        [ Utils postLogEntryNotification: @"couldn't start SSH session", nil];
        return runAgain;
    }
    
    
    //Start listening on localhost at local_listen_port
    int port = [Utils findAvailablePort:SOCKS_PORT withMaxIncrement:10];
    if(port == 0)
    {
        runAgain = NO;
        [Utils postLogEntryNotification: @"No available ports to run SOCKS proxy", nil];
        return runAgain;
    }
    
    [[PsiphonData sharedInstance] setSocksPort:port];


    [Utils postLogEntryNotification: @"starting SOCKS on %d", port];
    _forwarder = [[DynamicSSHForwarder alloc] initWithSession:session andPort:port];
    
    _forwarder.delegate = self;
    [_forwarder start];
    

    [_tunnelCondition lock];
    
    while(!_tunnelShouldStop)
    {
        [_tunnelCondition waitUntilDate: [NSDate dateWithTimeIntervalSinceNow:0.1f]];
    }
    [_tunnelCondition unlock];
    
    return runAgain;
}

- (void) reachabilityChanged: (NSNotification *)notification {
    Reachability *reach = [notification object];
    if( [reach isKindOfClass: [Reachability class]]) {
        NetworkStatus status = [reach currentReachabilityStatus];
        if (status == NotReachable)
        {
            [self stopTunnel];
        }
    }
}

- (void) runTunnel
{
    @synchronized(self)
    {
        if([_serverInterface serverWithCapabilitiesExists:@[RELAY_PROTOCOL]] == NO)
        {
            [Utils postLogEntryNotification: @"None of the servers supports relay protocol", nil];
            return;
        }
        
        while([self runTunnelOnce])
        {
            //TODO: figure out what to do here :)
        }
    }
}

#pragma mark - ServerInterface delegate methods

-(void) handshakeFailedWithError:(NSError *)error
{
    //TODO: implement this
}

-(void) handshakeSuccess:(NSString*)response
{
    [_eventsDelegate signalHandshakeSuccess];
    //do connected
    [_serverInterface sendConnectedRequest];
}

#pragma mark - DynamicSSHForwarder delegate methods

-(void) disconnectedSSH
{
    [self stopTunnel];
    [_eventsDelegate signalUnexpectedDisconnect];
    [Utils postLogEntryNotification: @"SSH disconnected", nil];
    

}

-(void) readySSHSocks
{
    [[PsiphonData sharedInstance] setTunnelRelayProtocol: RELAY_PROTOCOL];
    [_serverInterface sendHandshakeRequest];
}

@end
