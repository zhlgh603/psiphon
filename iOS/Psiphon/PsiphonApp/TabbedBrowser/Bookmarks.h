//
//  BookmarksArray.h
//  TabbedBrowser
//
//  Created by eugene on 2013-03-26.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Bookmark.h"

@interface Bookmarks:NSObject
@property(retain) NSMutableArray* bookmarksArray;
- (void)deleteBookmark:(NSInteger )index;
- (void)addBookmark:(Bookmark*)bookmark;
-(void)updateBookmark:(Bookmark*)bookmark;

-(NSUInteger)count;
-(id)objectAtIndex:(NSUInteger)index;


@end
