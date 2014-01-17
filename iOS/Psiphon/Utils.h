//
//  Utils.h
//  Psiphon
//
//  Created by eugene on 2013-08-21.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface Utils : NSObject

+ (UIColor*)getRGBAsFromImage:(UIImage*)image atX:(int)xx andY:(int)yy;
+ (NSString*) dehexString:(NSString *)hexString;
+ (NSString*) NSDataToHex:(NSData*)data;
+ (NSString *)urlEncode:(NSString*) myString;

+ (bool)isPortAvailable:(int)port;
+ (int) findAvailablePort:(int) startPort withMaxIncrement:(int)maxIncement;
+ (void) postLogEntryNotification:(NSString*)format, ...;
+ (void) shuffle:(NSArray**) array;
+ (void) shuffleSubArray:(NSArray**) array from:(NSInteger)startIndex to:(NSInteger) endIndex;


@end
