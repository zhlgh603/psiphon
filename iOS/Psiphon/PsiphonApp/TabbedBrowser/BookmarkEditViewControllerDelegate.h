//
//  BrowserViewControllerDelegate.h
//  TabbedBrowser
//
//  Created by eugene on 2013-11-01.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol BookmarkEditViewControllerDelegate <NSObject>
@required
-(void) bookmarkEditingDone:(Bookmark*) bookmark;
@end
