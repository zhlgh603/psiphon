#import <Foundation/Foundation.h>


@class PsiphonConstants;
@class TunnelCore;

@interface PsiphonData : NSObject
{
    NSArray* _homePages;
    NSDate* _nextFetchRemoteServerListDate;
    NSString* _clientSessionID;
    NSString* _tunnelSessionID;
    NSString* _tunnelRelayProtocol;
    int _socksPort;
    TunnelCore* _currentTunnelCore;
}

+ (id) sharedInstance;

- (void) setHomePages:(NSArray *)homePages;
- (NSMutableArray *) homePages;
- (void) setNextFetchRemoteServerListDate:(NSDate*)date;
- (NSDate*) nextFetchRemoteServerListDate;
- (void) setClientSessionID:(NSString *)clientSessionID;
- (NSString *) clientSessionID;
- (void) setTunnelSessionID:(NSString *)tunnelSessionID;
- (NSString *) tunnelSessionID;
- (void) setTunnelRelayProtocol:(NSString *)tunnelRelayProtocol;
- (NSString *) tunnelRelayProtocol;
- (void) setSocksPort:(int)socksPort;
- (int) socksPort;
- (void) setCurrentTunnelCore:(TunnelCore *)currentTunnelCore;
- (TunnelCore *) currentTunnelCore;

@end
