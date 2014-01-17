//
//  Utils.m
//  Psiphon
//
//  Created by eugene on 2013-08-21.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import "Utils.h"
#import "PsiphonConstants.h"

@implementation Utils
+ (UIColor*)getRGBAsFromImage:(UIImage*)image atX:(int)xx andY:(int)yy
{
    
    // First get the image into your data buffer
    CGImageRef imageRef = [image CGImage];
    NSUInteger width = CGImageGetWidth(imageRef);
    NSUInteger height = CGImageGetHeight(imageRef);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    unsigned char *rawData = (unsigned char*) calloc(height * width * 4, sizeof(unsigned char));
    NSUInteger bytesPerPixel = 4;
    NSUInteger bytesPerRow = bytesPerPixel * width;
    NSUInteger bitsPerComponent = 8;
    CGContextRef context = CGBitmapContextCreate(rawData, width, height,
                                                 bitsPerComponent, bytesPerRow, colorSpace,
                                                 kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(colorSpace);
    
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), imageRef);
    CGContextRelease(context);
    
    // Now your rawData contains the image data in the RGBA8888 pixel format.
    int byteIndex = (bytesPerRow * yy) + xx * bytesPerPixel;
    CGFloat red   = (rawData[byteIndex]     * 1.0) / 255.0;
    CGFloat green = (rawData[byteIndex + 1] * 1.0) / 255.0;
    CGFloat blue  = (rawData[byteIndex + 2] * 1.0) / 255.0;
    CGFloat alpha = (rawData[byteIndex + 3] * 1.0) / 255.0;
//    byteIndex += 4;
    
    UIColor *resColor = [UIColor colorWithRed:red green:green blue:blue alpha:alpha];
    
    free(rawData);
    
    return resColor;
}

+(NSString*) dehexString:(NSString *)hexString {
    NSMutableData* data = [NSMutableData data];
    int idx;
    int length = [hexString length];
    for (idx = 0; idx+2 <= length; idx+=2) {
        NSRange range = NSMakeRange(idx, 2);
        NSString* hexStr = [hexString substringWithRange:range];
        NSScanner* scanner = [NSScanner scannerWithString:hexStr];
        unsigned int intValue;
        [scanner scanHexInt:&intValue];
        [data appendBytes:&intValue length:1];
    }
    return [NSString stringWithUTF8String:[data bytes]];
}

+(NSString*) NSDataToHex:(NSData*)data
{
    const unsigned char *dbytes = [data bytes];
    NSMutableString *hexStr =
    [NSMutableString stringWithCapacity:[data length]*2];
    int i;
    for (i = 0; i < [data length]; i++) {
        [hexStr appendFormat:@"%02x", dbytes[i]];
    }
    return [NSString stringWithString: hexStr];
}

+ (NSString *)urlEncode:(NSString*) myString{
    NSMutableString *output = [NSMutableString string];
    const unsigned char *source = (const unsigned char *)[myString UTF8String];
    int sourceLen = strlen((const char *)source);
    for (int i = 0; i < sourceLen; ++i) {
        const unsigned char thisChar = source[i];
        if (thisChar == ' '){
            [output appendString:@"+"];
        } else if (thisChar == '.' || thisChar == '-' || thisChar == '_' || thisChar == '~' ||
                   (thisChar >= 'a' && thisChar <= 'z') ||
                   (thisChar >= 'A' && thisChar <= 'Z') ||
                   (thisChar >= '0' && thisChar <= '9')) {
            [output appendFormat:@"%c", thisChar];
        } else {
            [output appendFormat:@"%%%02X", thisChar];
        }
    }
    return output;
}

+(bool)isPortAvailable:(int)port
{
    @try {
        
        CFSocketRef socket ;
        
        socket = CFSocketCreate(kCFAllocatorDefault,
                                PF_INET,SOCK_STREAM,IPPROTO_TCP,
                                0,NULL,NULL);
        struct sockaddr_in sin;
        
        memset(&sin, 0, sizeof(sin));
        sin.sin_len = sizeof(sin);
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port); // port we want to connect on
        sin.sin_addr.s_addr = inet_addr("127.0.0.1");
        
        CFDataRef address = CFDataCreate(NULL,(unsigned char*)&sin,sizeof(sin));
        
        CFTimeInterval timeout = 1 ; // 1 second timeout
        
        CFSocketError e = CFSocketConnectToAddress(socket,address,timeout);
        CFSocketInvalidate(socket) ;
        
        CFRelease(address);
        CFRelease(socket);
        
        if(e == kCFSocketSuccess || e == kCFSocketTimeout)
        {
            return NO;
        }
        else
        {
            return YES;
        }
        
    } @catch (id SocketException) {
        
        return YES ;
        
    }
}

+(int) findAvailablePort:(int) startPort withMaxIncrement:(int)maxIncement
{
    for(int port = startPort; port < (startPort + maxIncement); port++)
    {
        if ([self isPortAvailable:port])
        {
            return port;
        }
    }
    return 0;
}

+ (void) postLogEntryNotification:(NSString*)format, ...
{
    va_list args;
    va_start(args, format);
    NSString *logEntry = [[NSString alloc] initWithFormat:format arguments:args];
    
    dispatch_async(dispatch_get_main_queue(),^{
        NSDictionary* userInfo = [NSDictionary dictionaryWithObject: logEntry forKey:LOG_ENTRY_NOTIFICATION_KEY];
        NSNotification *notification = [NSNotification notificationWithName:NEW_LOG_ENTRY_POSTED object:nil userInfo:userInfo];
        [[NSNotificationQueue defaultQueue] enqueueNotification:notification postingStyle:NSPostNow
                                                   coalesceMask:NSNotificationNoCoalescing forModes:nil];
    });
}

+ (void) shuffleSubArray:(NSArray**) array from:(NSInteger)startIndex to:(NSInteger) endIndex
{
    assert(startIndex <= endIndex && startIndex >= 0 && endIndex <= [(*array) count] - 1);

    NSMutableArray* tempArray = [[NSMutableArray alloc]initWithArray:*array];
    
    NSInteger count = endIndex - startIndex;
    for (NSInteger i = startIndex; i < count; ++i) {
        NSInteger nElements = count - i;
        NSInteger n = (arc4random() % nElements) + i;
        [tempArray exchangeObjectAtIndex:i withObjectAtIndex:n];
    }
    
    *array = (NSArray*)tempArray;
}

+ (void) shuffle:(NSArray**) array
{
    if([*array count] == 1)
        return;
    NSUInteger lastIndex = [*array count] - 1;
    [self shuffleSubArray: array from:0 to:lastIndex];
}



@end
