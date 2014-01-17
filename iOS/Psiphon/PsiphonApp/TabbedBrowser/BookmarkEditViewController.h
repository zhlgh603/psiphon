//
//  BookmarkEditViewController.h
//  OnionBrowser
//
//  Created by Mike Tigas on 9/7/12.
//
//

#import <UIKit/UIKit.h>
#import "Bookmark.h"
#import "BookmarkEditViewControllerDelegate.h"

@interface BookmarkEditViewController : UITableViewController <UITextFieldDelegate> {
    Bookmark *bookmark;
}

@property (nonatomic, retain) Bookmark *bookmark;
@property (weak) id<BookmarkEditViewControllerDelegate> delegate;

- (id)initWithDelegate:(id<BookmarkEditViewControllerDelegate>)aDelegate andBookmark:(Bookmark *)bookmarkToEdit;
-(void)saveAndDismiss;
@end
