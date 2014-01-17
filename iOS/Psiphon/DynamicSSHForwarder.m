//
//  DynamicSSHForwarder.m
//  Psiphon
//
//  Created by eugene on 2013-10-29.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import "DynamicSSHForwarder.h"

@implementation DynamicSSHForwarder

@synthesize delegate, isStopped = _isStopped;

-(void) start
{
    _forwarderThread = [[NSThread alloc] initWithTarget:self
                                               selector:@selector(runForwarder)
                                                 object:nil];
    [_forwarderThread start];
}

-(void) stop
{
    if(_forwarderThread != nil)
    {
        [_forwarderThread cancel];
    }
}

-(id) initWithSession:(LIBSSH2_SESSION *)session andPort:(int)port
{
    self = [super init];
    if (self) {
        _localPort = port;
        _session = session;
    }
    return self;
}

-(void) runForwarder
{
    int i = 0;
    struct psi_connection* connections;
    int max_conn = 256;
    
    int listensock = psi_start_local_listen(_localPort, max_conn);
    if(INVALID_SOCKET == listensock)
    {
        psi_shutdown(listensock, _session);
        [self performSelectorOnMainThread:@selector(sshStopped)
                                      withObject:nil
                                   waitUntilDone:NO];
        return;
    }
    
    /*Init connections*/
    connections = (struct psi_connection*)(malloc(max_conn * sizeof(struct psi_connection)));
    for (i=0; i<max_conn; i++)
    {
        connections[i].sock = INVALID_SOCKET;
        connections[i].channel = NULL;
        connections[i].data = NULL;
        connections[i].data_size = 0;
        connections[i].direction = -1;
    }
    
    [self performSelectorOnMainThread:@selector(sshReady)
                           withObject:nil
                        waitUntilDone:NO];

    
    //loop  until signalled or disconnected
    while(psi_poll_connections(listensock, &connections, max_conn, _session) &&
          [[NSThread currentThread] isCancelled] == NO);
    
    free(connections);
    psi_shutdown(listensock, _session);
    
    [self performSelectorOnMainThread:@selector(sshStopped)
                                  withObject:nil
                               waitUntilDone:NO];
}


-(void) sshStopped
{
    [delegate disconnectedSSH];
}

-(void) sshReady
{
    [delegate readySSHSocks];
}


@end
