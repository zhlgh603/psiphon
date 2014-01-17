#import "PsiphonData.h"
#import "Utils.h"
#import "TunnelCore.h"


@interface PsiphonData()

@end

@implementation PsiphonData

+ (PsiphonData *)sharedInstance
{
    
    //singleton pattern using GCD
    
    static PsiphonData *sharedInstance = nil;
    static dispatch_once_t onceToken = 0;
    dispatch_once(&onceToken, ^{
        sharedInstance = [[self alloc] init];
        // Do any other initialisation stuff here
        sharedInstance->_homePages = [[NSMutableArray alloc]init];
        sharedInstance->_nextFetchRemoteServerListDate = nil;
    });
    
    return sharedInstance;
}

#pragma mark -
#pragma mark PsiphonData setters & getters

- (void) setHomePages:(NSArray *)homePages
{
    @synchronized(self)
    {
        _homePages = homePages;
    }
}

- (NSArray *) homePages
{
    NSArray *retVal;
    
    @synchronized(self)
    {
        retVal = _homePages;
    }
    
    return retVal;
}

- (void) setNextFetchRemoteServerListDate:(NSDate*)date
{
    @synchronized(self)
    {
        _nextFetchRemoteServerListDate = date;
    }
}

- (NSDate*) nextFetchRemoteServerListDate
{
    NSDate* retVal;
    
    @synchronized(self)
    {
        retVal = _nextFetchRemoteServerListDate;
    }
    
    return retVal;
}

- (void) setClientSessionID:(NSString *)clientSessionID
{
    @synchronized(self)
    {
        _clientSessionID = clientSessionID;
    }
}

- (NSString *) clientSessionID
{
    NSString *retVal;
    
    @synchronized(self)
    {
        retVal = _clientSessionID;
    }
    
    return retVal;
}

- (void) setTunnelSessionID:(NSString *)tunnelSessionID
{
    @synchronized(self)
    {
        _tunnelSessionID = tunnelSessionID;
    }
}

- (NSString *) tunnelSessionID
{
    NSString *retVal;
    
    @synchronized(self)
    {
        retVal = _tunnelSessionID;
    }
    
    return retVal;
}

- (void) setTunnelRelayProtocol:(NSString *)tunnelRelayProtocol
{
    @synchronized(self)
    {
        _tunnelRelayProtocol = tunnelRelayProtocol;
    }
}

- (NSString *) tunnelRelayProtocol
{
    NSString *retVal;
    
    @synchronized(self)
    {
        retVal = _tunnelRelayProtocol;
    }
    
    return retVal;
}

- (void) setSocksPort:(int)socksPort
{
    @synchronized(self)
    {
        _socksPort = socksPort;
    }
}

- (int) socksPort
{
    int retVal;
    
    @synchronized(self)
    {
        retVal = _socksPort;
    }
    
    return retVal;
}

- (void) setCurrentTunnelCore:(TunnelCore *)currentTunnelCore
{
    @synchronized(self)
    {
        _currentTunnelCore = currentTunnelCore;
    }
}

- (TunnelCore *) currentTunnelCore
{
    TunnelCore *retVal;
    
    @synchronized(self)
    {
        retVal = _currentTunnelCore;
    }
    
    return retVal;
}

#pragma mark -


@end
