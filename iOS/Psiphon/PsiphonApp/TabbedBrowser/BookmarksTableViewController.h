
#import <UIKit/UIKit.h>
#import "Bookmarks.h"
#import "BrowserViewController.h"

@interface BookmarksTableViewController : UITableViewController

@property (nonatomic, retain) UIBarButtonItem *editButton;
@property (nonatomic, retain) UIBarButtonItem *addButton;
@property (nonatomic, retain) UIBarButtonItem *backButton;
@property (nonatomic, retain) UIBarButtonItem *editDoneButton;
@property (weak, nonatomic) BrowserViewController *browser;


- (void)reloadView;
- (void)addBookmark;
- (void)startEditing;
- (void)stopEditing;
- (void)goBack;
@end
    