//
//  CheckServerReachabilityOperation.m
//  Psiphon
//
//  Created by eugene on 2013-11-15.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import "CheckServerReachabilityOperation.h"
#import "ServerEntry.h"

static void socketConnectCallbackFunction (CFSocketRef sref, CFSocketCallBackType type, CFDataRef inAddress, const void *pData, void *pInfo);

@implementation CheckServerReachabilityOperation

@synthesize responded, completed, responseTime, serverEntry;

- (id) initWithServerEntry:(ServerEntry*)entry
{
    self = [super init];
    if (self) {
        serverEntry = entry;
        responded = NO;
        completed = NO;
        responseTime = - 1;
    }
    return self;
}

- (void)main {
    if([self isCancelled])
    {
        return;
    }
    @autoreleasepool {
        
        NSDate* startDate = [NSDate date];
        
        CFSocketContext context = {
			.version = 0,
			.info = (__bridge void *)(self),
			.retain = NULL,
			.release = NULL,
			.copyDescription = NULL
		};
        
        CFSocketRef socket = CFSocketCreate(kCFAllocatorDefault, PF_INET, SOCK_STREAM, IPPROTO_TCP,
                                            kCFSocketConnectCallBack, socketConnectCallbackFunction, &context);
        
        CFSocketSetSocketFlags(socket, CFSocketGetSocketFlags(socket) & ~kCFSocketCloseOnInvalidate);
        
        int fd = CFSocketGetNative(socket);
        int yes = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&yes, sizeof(yes));

        if(socket)
        {
            
            struct sockaddr_in addr4;
            memset(&addr4, 0, sizeof(addr4));
            addr4.sin_len = sizeof(addr4);
            addr4.sin_family = AF_INET;
            addr4.sin_port = htons([serverEntry getPreferredReachablityTestPort]);
            inet_pton(AF_INET, [[serverEntry ipAddress] UTF8String], &addr4.sin_addr);
            
            NSData *address4 = [NSData dataWithBytes:&addr4 length:sizeof(addr4)];
            
            if([self isCancelled])
            {
                return;
            }
            CFSocketConnectToAddress(socket, (__bridge CFDataRef)address4, -1);
            
            CFRunLoopSourceRef sourceRef = CFSocketCreateRunLoopSource(kCFAllocatorDefault, socket, 0);
            CFRunLoopAddSource(CFRunLoopGetCurrent(), sourceRef, kCFRunLoopCommonModes);
            CFRelease(sourceRef);
            CFRunLoopRun();

            if(responded == YES)
            {
                responseTime = -[startDate timeIntervalSinceNow];
            }
        }
    }
}

-(void) cancel
{
    [super cancel];
    CFRunLoopStop(CFRunLoopGetCurrent());
}

@end

static void socketConnectCallbackFunction (CFSocketRef sref, CFSocketCallBackType type, CFDataRef inAddress, const void *pData, void *pInfo)
{
    @autoreleasepool {
        CheckServerReachabilityOperation *op = (__bridge CheckServerReachabilityOperation *)pInfo;
        
        //In this case the data argument is either NULL, or a pointer to
        //an SInt32 error code if the connect failed.
        if(pData == NULL)
        {
            op.responded = YES;
        }
        
        op.completed = YES;
        
        CFSocketInvalidate(sref);
        CFRelease(sref);
    }
}


