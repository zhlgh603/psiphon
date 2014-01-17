#import "BookmarksTableViewController.h"
#import "Bookmark.h"
#import "BookmarkEditViewController.h"
#import "AppDelegate.h"


@implementation BookmarksTableViewController
@synthesize addButton;
@synthesize editButton;
@synthesize backButton;
@synthesize editDoneButton;

- (id)initWithStyle:(UITableViewStyle)style
{
    self = [super initWithStyle:style];
    if (self) {
        // Custom initialization
    }
    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    [self.tableView setAllowsSelectionDuringEditing:YES];
    
    self.title = @"Bookmarks";
    
    self.navigationItem.leftBarButtonItem = self.editButtonItem;
    
    addButton = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemAdd
                                                              target:self action:@selector(addBookmark)];
    editButton = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemEdit
                                                               target:self action:@selector(startEditing)];
    editDoneButton = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                                                                   target:self action:@selector(stopEditing)];
    backButton = [[UIBarButtonItem alloc] initWithTitle:@"Back" style:UIBarButtonItemStyleDone target:self action:@selector(goBack)];
    self.navigationItem.leftBarButtonItem = editButton;
    self.navigationItem.rightBarButtonItem = backButton;
    [self reloadView];
}
- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    [self reloadView];
}
-(void)reloadView {
    [self.tableView reloadData];
}

- (void)viewDidUnload
{
    [super viewDidUnload];
    self.addButton = nil;
}


#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    return 1;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section{
    return nil;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
        return [self.browser.bookmarks count];
}

- (BOOL)tableView:(UITableView *)tableView canEditRowAtIndexPath:(NSIndexPath *)indexPath {
    if (indexPath.section == 0)
        return YES;
    else
        return NO;
}
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    static NSString *CellIdentifier = @"Cell";
    
    // Dequeue or create a new cell.
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];
    if (cell == nil) {
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:CellIdentifier];
        [cell setEditingAccessoryType:UITableViewCellAccessoryDisclosureIndicator];
    }
    
        Bookmark *bookmark = (Bookmark *)[self.browser.bookmarks objectAtIndex:indexPath.row];
        cell.textLabel.text = bookmark.title;
        cell.detailTextLabel.text = bookmark.url;
        return cell;
}

- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath {
    if (editingStyle == UITableViewCellEditingStyleDelete) {
        
        [self.browser.bookmarks deleteBookmark:indexPath.row];
        [tableView deleteRowsAtIndexPaths:[NSArray arrayWithObject:indexPath] withRowAnimation:YES];
    }
}


// Override to support rearranging the table view.
- (void)tableView:(UITableView *)tableView moveRowAtIndexPath:(NSIndexPath *)fromIndexPath toIndexPath:(NSIndexPath *)toIndexPath {
    
}

#pragma mark - Table view delegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    if (tableView.isEditing) {
            // Open an editing pane
            Bookmark *bookmark = (Bookmark *)[self.browser.bookmarks objectAtIndex:indexPath.row];
            BookmarkEditViewController *editController =
                [[BookmarkEditViewController alloc] initWithDelegate:self.browser andBookmark:bookmark];
            [self presentModalViewController:editController animated:YES];
    } else {
        Bookmark *bookmark = (Bookmark *)[self.browser.bookmarks objectAtIndex:indexPath.row];
        [self.browser handleURLString:bookmark.url title:nil];
        [self goBack];
    }
}

- (void)addBookmark {
    Bookmark *bookmark = (Bookmark *)[NSEntityDescription insertNewObjectForEntityForName:@"Bookmark" inManagedObjectContext:[appDelegate managedObjectContext]];
    
    [bookmark setTitle:@"Title"];
    [bookmark setUrl:@"http://example.com/"];
    
    
    BookmarkEditViewController *editController = [[BookmarkEditViewController alloc] initWithDelegate:self.browser andBookmark:bookmark];
    [self presentModalViewController:editController animated:YES];
}

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
    if (editing) {
        self.navigationItem.leftBarButtonItem = editDoneButton;
        self.navigationItem.rightBarButtonItem = addButton;
    } else {
        self.navigationItem.leftBarButtonItem = editButton;
        self.navigationItem.rightBarButtonItem = backButton;
    }
    [super setEditing:editing animated:animated];
}

- (void)startEditing {
    [self setEditing:YES];
}
- (void)stopEditing {
    [self setEditing:NO];
}
- (void)goBack {
    [self dismissModalViewControllerAnimated:YES];
}
@end
