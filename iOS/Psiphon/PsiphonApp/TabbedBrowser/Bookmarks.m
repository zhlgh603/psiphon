//
//  BookmarksArray.m
//  TabbedBrowser
//
//  Created by eugene on 2013-03-26.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import "Bookmarks.h"
#import "AppDelegate.h"


@implementation Bookmarks
@synthesize bookmarksArray;

-(id) init {
    self = [super init];
    if (self != nil)
    {
        NSFetchRequest *request = [[NSFetchRequest alloc] init];
        NSEntityDescription *entity = [NSEntityDescription entityForName:@"Bookmark" inManagedObjectContext:[appDelegate managedObjectContext]];
        [request setEntity:entity];
        NSSortDescriptor *sortDescriptor = [[NSSortDescriptor alloc] initWithKey:@"order" ascending:YES];
        NSArray *sortDescriptors = [[NSArray alloc] initWithObjects:sortDescriptor, nil];
        [request setSortDescriptors:sortDescriptors];
        
        NSError *error = nil;
        NSMutableArray *mutableFetchResults = [[[appDelegate managedObjectContext] executeFetchRequest:request error:&error] mutableCopy];
        if (mutableFetchResults == nil) {
            // Handle the error.
        }
        [self setBookmarksArray:mutableFetchResults];
    }
    return self;
}

- (void)addBookmark:(Bookmark*)bookmark {
    [bookmarksArray addObject:bookmark];
    [self saveAndResetBookmarkOrder];
}

-(void)updateBookmark:(Bookmark*)bookmark {
    [self saveAndResetBookmarkOrder];
}


- (void)deleteBookmark:(NSInteger )index {
    NSManagedObject *bookmarkToDelete = [bookmarksArray objectAtIndex:index];
    [[appDelegate managedObjectContext] deleteObject:bookmarkToDelete];
    [bookmarksArray removeObjectAtIndex:index];
    [self saveAndResetBookmarkOrder];
}

- (void)saveAndResetBookmarkOrder {
    int16_t i = 0;
    for (Bookmark *bookmark in bookmarksArray) {
        [bookmark setOrder:[NSNumber numberWithInt:i]];
        i++;
    }
    NSError *error = nil;
    if (![[appDelegate managedObjectContext] save:&error]) {
        NSLog(@"Error updating bookmark order: %@", error);
    }
}

-(NSUInteger)count
{
    return [bookmarksArray count];
}

-(id)objectAtIndex:(NSUInteger)index
{
    return [bookmarksArray objectAtIndex:index];
}


@end
