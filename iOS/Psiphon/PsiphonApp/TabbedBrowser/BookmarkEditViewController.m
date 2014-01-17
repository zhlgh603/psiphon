//
//  BookmarkEditViewController.m
//  OnionBrowser
//
//  Created by Mike Tigas on 9/7/12.
//
//

#import "BookmarkEditViewController.h"
#import "AppDelegate.h"

@interface BookmarkEditViewController ()

@end

@implementation BookmarkEditViewController
@synthesize bookmark;
@synthesize delegate;

- (id)initWithStyle:(UITableViewStyle)style
{
    self = [super initWithStyle:style];
    if (self) {
        // Custom initialization
    }
    return self;
}

- (id)initWithDelegate:(id<BookmarkEditViewControllerDelegate>)aDelegate andBookmark:(Bookmark *)bookmarkToEdit {
    self.delegate = aDelegate;
    self = [super initWithStyle:UITableViewStyleGrouped];
    if (self) {
        self.bookmark = bookmarkToEdit;
    }
    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    // Uncomment the following line to preserve selection between presentations.
    // self.clearsSelectionOnViewWillAppear = NO;
    
    // Uncomment the following line to display an Edit button in the navigation bar for this view controller.
    // self.navigationItem.rightBarButtonItem = self.editButtonItem;
}

- (void)viewDidUnload
{
    [super viewDidUnload];
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}


#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    return 4;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section{
    if(section == 0)
        return @"Bookmark Title";
    else if (section == 1)
        return @"Bookmark URL";
    else
        return nil;
}


- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    return 1;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    static NSString *CellIdentifier = @"Cell";
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];
    if (cell == nil) {
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:CellIdentifier];
    }
    
    if ((indexPath.section == 0)||(indexPath.section == 1)) {
        cell.selectionStyle = UITableViewCellSelectionStyleNone;
        
        CGRect textFrame;
        textFrame = CGRectMake(20, 10,
                                   cell.contentView.frame.size.width-40, cell.contentView.frame.size.height-20);
        UITextField *editField = [[UITextField alloc]
                                  initWithFrame:textFrame];
        editField.adjustsFontSizeToFitWidth = YES;
        editField.autoresizingMask = UIViewAutoresizingFlexibleWidth;
        
        /*
         editField.textColor = [UIColor blackColor];
         editField.backgroundColor = [UIColor whiteColor];
         */
        editField.textAlignment = UITextAlignmentLeft;
        editField.clearButtonMode = UITextFieldViewModeNever; // no clear 'x' button to the right
        [editField setEnabled: YES];
        editField.delegate = self;
        
        if (indexPath.section == 0) {
            editField.autocorrectionType = UITextAutocorrectionTypeYes;
            editField.autocapitalizationType = UITextAutocapitalizationTypeWords;
            editField.text = bookmark.title;
            editField.returnKeyType = UIReturnKeyNext;
            editField.tag = 100;
        } else {
            editField.autocorrectionType = UITextAutocorrectionTypeNo;
            editField.autocapitalizationType = UITextAutocapitalizationTypeNone;
            editField.text = bookmark.url;
            editField.returnKeyType = UIReturnKeyDone;
            editField.tag = 101;
        }
        
        [cell addSubview:editField];
        
    } else {
        cell.selectionStyle = UITableViewCellSelectionStyleBlue;
        
        cell.textLabel.textAlignment = UITextAlignmentCenter;
        
        if (indexPath.section == 2) {
            cell.textLabel.text = @"Done";
        }
        else {
            cell.textLabel.text = @"Cancel";
        }
    }
    
    return cell;
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField {
    if (textField.tag == 100)
        [[self.view viewWithTag:101] becomeFirstResponder];
    else if (textField.tag == 101) {
        [self saveAndDismiss];
    }
    return YES;
}

// Override to support conditional editing of the table view.
- (BOOL)tableView:(UITableView *)tableView canEditRowAtIndexPath:(NSIndexPath *)indexPath {
    return NO;
}

#pragma mark - Table view delegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    if (indexPath.section == 2) {
        [self saveAndDismiss];   
    }
    if (indexPath.section == 3) {
        [self dismissModalViewControllerAnimated:YES];
    }
}

-(void)saveAndDismiss {
    NSUInteger titlePathInt[2] = {0,0};
    NSIndexPath* titlePath = [[NSIndexPath alloc] initWithIndexes:titlePathInt length:2];
    UITableViewCell *titleCell = [self.tableView cellForRowAtIndexPath:titlePath];
    UITextField *titleEditField = (UITextField*)[titleCell viewWithTag:100];
    bookmark.title = titleEditField.text;
    
    NSUInteger urlPathInt[2] = {1,0};
    NSIndexPath* urlPath = [[NSIndexPath alloc] initWithIndexes:urlPathInt length:2];
    UITableViewCell *urlCell = [self.tableView cellForRowAtIndexPath:urlPath];
    UITextField *urlEditField = (UITextField*)[urlCell viewWithTag:101];
    bookmark.url = urlEditField.text;
    
    [self.delegate bookmarkEditingDone:bookmark];
    
    
    [self dismissModalViewControllerAnimated:YES];
}

@end
