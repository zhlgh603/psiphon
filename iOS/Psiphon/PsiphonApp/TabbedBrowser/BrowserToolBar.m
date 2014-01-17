//
//  BrowserToolBar.m
//  TabbedBrowser
//
//  Created by eugene on 2013-03-04.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import "AppDelegate.h"
#import "BrowserToolBar.h"
#import "Bookmark.h"
#import "BookmarkEditViewController.h"
#import "BookmarksTableViewController.h"

@implementation BrowserToolBar {
    BOOL _searchMaskVisible;
}

- (id)initWithFrame:(CGRect)frame browser:(BrowserViewController *)browser {
    self = [super initWithFrame:frame];
    if (self) {
        _browser = browser;
        self.autoresizingMask = UIViewAutoresizingFlexibleWidth;
        
        CGRect btnRect = CGRectMake(0, 0, 30, 30);
        _backButton = [UIButton buttonWithType:UIButtonTypeCustom];
        _backButton.frame = btnRect;
        _backButton.autoresizingMask = UIViewAutoresizingFlexibleRightMargin;
        _backButton.backgroundColor = [UIColor clearColor];
        _backButton.showsTouchWhenHighlighted = YES;
        [_backButton setImage:[UIImage imageNamed:@"left"] forState:UIControlStateNormal];
        [_backButton addTarget:self.browser action:@selector(goBack) forControlEvents:UIControlEventTouchUpInside];
        [self addSubview:_backButton];
        
        _forwardButton = [UIButton buttonWithType:UIButtonTypeCustom];
        _forwardButton.frame = btnRect;
        _forwardButton.autoresizingMask = UIViewAutoresizingFlexibleRightMargin;
        _forwardButton.backgroundColor = [UIColor clearColor];
        _forwardButton.showsTouchWhenHighlighted = YES;
        [_forwardButton setImage:[UIImage imageNamed:@"right"] forState:UIControlStateNormal];
        [_forwardButton addTarget:self.browser action:@selector(goForward) forControlEvents:UIControlEventTouchUpInside];
        _forwardButton.enabled = self.browser.canGoForward;
        [self addSubview:_forwardButton];
        
        btnRect = CGRectMake(0, (frame.size.height - 36)/2, 36, 36);
        btnRect.origin.x = frame.size.width - 80;
        _optionsButton = [UIButton buttonWithType:UIButtonTypeCustom];
        _optionsButton.frame = btnRect;
        _optionsButton.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin;
        _optionsButton.backgroundColor = [UIColor clearColor];
        [_optionsButton setImage:[UIImage imageNamed:@"grip"] forState:UIControlStateNormal];
        [_optionsButton setImage:[UIImage imageNamed:@"grip-pressed"] forState:UIControlStateHighlighted];
        [_optionsButton addTarget:self action:@selector(showOptions:) forControlEvents:UIControlEventTouchUpInside];
        [self addSubview:_optionsButton];
        
        btnRect.origin.x = frame.size.width - 40;
        _tabsButton = [UIButton buttonWithType:UIButtonTypeCustom];
        _tabsButton.frame = btnRect;
        _tabsButton.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin;
        _tabsButton.backgroundColor = [UIColor clearColor];
        _tabsButton.titleEdgeInsets = UIEdgeInsetsMake(5, 5, 0, 0);
        _tabsButton.titleLabel.font = [UIFont systemFontOfSize:12.5];
        [_tabsButton setBackgroundImage:[UIImage imageNamed:@"expose"] forState:UIControlStateNormal];
        [_tabsButton setBackgroundImage:[UIImage imageNamed:@"expose-pressed"] forState:UIControlStateHighlighted];
        [_tabsButton setTitle:@"0" forState:UIControlStateNormal];
        [_tabsButton setTitleColor:UIColorFromHEX(0x2E2E2E) forState:UIControlStateNormal];
        [_tabsButton addTarget:self action:@selector(pressedTabsButton:) forControlEvents:UIControlEventTouchUpInside];
        [self addSubview:_tabsButton];
        
        btnRect = CGRectMake(0, (frame.size.height - 36)/2, 77, 36);
        btnRect.origin.x = frame.size.width - 80;
        _cancelButton = [UIButton buttonWithType:UIButtonTypeCustom];
        _cancelButton.frame = btnRect;
        _cancelButton.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin;
        _cancelButton.backgroundColor = [UIColor clearColor];
        _cancelButton.titleLabel.font = [UIFont fontWithName:@"HelveticaNeue-Bold" size:18];
        [_cancelButton setTitle:NSLocalizedString(@"Cancel", @"Cancel") forState:UIControlStateNormal];
        [_cancelButton setTitleColor:UIColorFromHEX(0x5E5E5E) forState:UIControlStateNormal];
        [_cancelButton addTarget:self action:@selector(cancelSearchButton:) forControlEvents:UIControlEventTouchUpInside];
        _cancelButton.hidden = YES;
        [self addSubview:_cancelButton];
        
        _searchField = [[AddressField alloc] initWithDelegate:self];
        [self.searchField.stopItem addTarget:self.browser action:@selector(stop) forControlEvents:UIControlEventTouchUpInside];
        [self.searchField.reloadItem addTarget:self.browser action:@selector(reload) forControlEvents:UIControlEventTouchUpInside];
        [self addSubview:_searchField];
        
    }
    return self;
}

- (id)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        // Initialization code
    }
    return self;
}

- (void)layoutSubviews {
    const CGFloat margin = 5;
    CGFloat width = self.bounds.size.width;
    CGFloat height = self.bounds.size.height;
    
    CGFloat posX = margin;
    
    if (!_searchMaskVisible) {
        _backButton.frame = CGRectMake(posX, (height - _backButton.frame.size.height)/2,
                                       _backButton.frame.size.width, _backButton.frame.size.height);
        
        posX += _backButton.frame.size.width + margin;
        _forwardButton.frame = CGRectMake(posX, (height - _forwardButton.frame.size.height)/2,
                                          _forwardButton.frame.size.width, _forwardButton.frame.size.height);
        
        if (_forwardButton.enabled)
            posX += _forwardButton.frame.size.width + margin;
    }
    
    CGFloat searchWidth = width - posX - _tabsButton.frame.size.width - _optionsButton.frame.size.width - 3*margin;
    _searchField.frame = CGRectMake(posX, (height - _searchField.frame.size.height)/2,
                                    searchWidth, _searchField.frame.size.height);
}

- (void)drawRect:(CGRect)rect {
    CGContextRef context = UIGraphicsGetCurrentContext();
    CGPoint topCenter = CGPointMake(CGRectGetMidX(self.bounds), 0.0f);
    CGPoint bottomCenter = CGPointMake(CGRectGetMidX(self.bounds), self.bounds.size.height);
    CGFloat locations[2] = {0.00, 1.0};
    
    //Two colour components, the start and end colour both set to opaque. Red Green Blue Alpha
    CGFloat components[8] = { 244./255., 245./255., 247./255., 1.0, 204./255., 205./255., 207./255., 1.0};
    
    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
    
    CGGradientRef gradient = CGGradientCreateWithColorComponents(colorspace, components, locations, 2);
    CGContextDrawLinearGradient(context, gradient, topCenter, bottomCenter, 0);
    CGGradientRelease(gradient);
    CGColorSpaceRelease(colorspace);
}


- (void)updateChrome {
    if (!([self.searchField isFirstResponder] || _searchMaskVisible))
        self.searchField.text = [self.browser URL].absoluteString;
    
    NSString *text = [NSString stringWithFormat:@"%d", self.browser.count];
    [_tabsButton setTitle:text forState:UIControlStateNormal];
    
    self.backButton.enabled = self.browser.canGoBack;
    if (_forwardButton.enabled != self.browser.canGoForward)
        [UIView animateWithDuration:0.2 animations:^{
            _forwardButton.enabled = self.browser.canGoForward;
            [self layoutSubviews];
        }];
    
    if (self.browser.canStopOrReload) {
        if ([self.browser isLoading]) {
            self.searchField.state = SearchFieldStateStop;
        } else {
            self.searchField.state = SearchFieldStateReload;
        }
    } else {
        self.searchField.state = SearchFieldStateDisabled;
    }
}

- (IBAction)pressedTabsButton:(id)sender {
    [self.browser setExposeMode:YES animated:YES];
}
- (IBAction)cancelSearchButton:(id)sender {
    [self dismissSearchController];
    [self.searchField resignFirstResponder];
}


#pragma mark - UITextFieldDelegate

- (void)textFieldDidBeginEditing:(UITextField *)textField {
    [self presentSearchController];
}


- (BOOL)textFieldShouldReturn:(UITextField *)textField {
    // When the search button is tapped, add the search term to recents and conduct the search.
    NSString *searchString = [textField text];
    [self.browser handleURLString:searchString  title:nil];
    [self dismissSearchController];
    [self.searchField resignFirstResponder];

    return YES;
}



- (void)presentSearchController {
    if (!_searchMaskVisible) {
        _searchMaskVisible = YES;
/*
 TODO        self.searchController.view.frame = CGRectMake(0, self.frame.size.height,
                                                      self.frame.size.width, self.superview.bounds.size.height - self.frame.size.height);
 */
        [UIView animateWithDuration:0.25
                         animations:^{
                             [self layoutSubviews];
                             _cancelButton.alpha = 1.0;
                             _optionsButton.alpha = 0;
                             _tabsButton.alpha = 0;
//TODO                             [self.superview addSubview:self.searchController.view];
                         } completion:^(BOOL finished){
                             _optionsButton.hidden = YES;
                             _tabsButton.hidden = YES;
                             _cancelButton.hidden = NO;
                         }];
    }
}

- (void)dismissSearchController {
    if (_searchMaskVisible) {
        _searchMaskVisible = NO;
        // If the user finishes editing text in the search bar by, for example:
        // tapping away rather than selecting from the recents list, then just dismiss the popover
        self.searchField.text = [self.browser URL].absoluteString;
        [UIView animateWithDuration:0.25
                         animations:^{
                             [self layoutSubviews];
                             _optionsButton.alpha = 1.0;
                             _tabsButton.alpha = 1.0;
                             _cancelButton.alpha = 0;
                             //TODO [self.searchController.view removeFromSuperview];
                         } completion:^(BOOL finished){
                             _optionsButton.hidden = NO;
                             _tabsButton.hidden = NO;
                             _cancelButton.hidden = YES;
                         }];
    }
}


- (IBAction)showOptions:(UIButton *)sender {
        self.actionSheet = [[UIActionSheet alloc] initWithTitle:nil
                                                       delegate:self
                                              cancelButtonTitle:NSLocalizedString(@"Cancel", nil)
                                         destructiveButtonTitle:nil
                                              otherButtonTitles:
                            NSLocalizedString(@"Add bookmark", @"Add bookmark"),
                            NSLocalizedString(@"Bookmarks", @"Bookmarks"),
                            nil];
    [self.actionSheet showInView:self];
}

- (void)actionSheet:(UIActionSheet *)actionSheet clickedButtonAtIndex:(NSInteger)buttonIndex {
    [self dismissSearchController];
    NSURL *url = [self.browser URL];
    NSString *title = self.browser.titleLabel.text;
    
    
    
    if (buttonIndex == 0) {
        //add current page to bookmarks
        Bookmark *bookmark = (Bookmark *)[NSEntityDescription insertNewObjectForEntityForName:@"Bookmark"
                                                                       inManagedObjectContext:[appDelegate managedObjectContext]];
        
        if(url )
        {
            [bookmark setUrl:[url absoluteString]];
            [bookmark setTitle:title];
        }
        else {
            [bookmark setUrl:@"http://example.com/"];
            [bookmark setTitle:@"Title"];
        }
        
        [bookmark setOrder:[NSNumber numberWithInt:-1]]; //indicates new bookmark
        
        
        BookmarkEditViewController *editController = [[BookmarkEditViewController alloc] initWithDelegate:self.browser andBookmark:bookmark];

        [self.browser presentViewController:editController animated:YES completion:NULL];
    }
    if (buttonIndex == 1) {
        BookmarksTableViewController *bookmarksVC = [[BookmarksTableViewController alloc] initWithStyle:UITableViewStylePlain];
        bookmarksVC.browser = self.browser;
        UINavigationController *bookmarkNavController = [[UINavigationController alloc]
                                                         initWithRootViewController:bookmarksVC];
        
        
        [self.browser presentModalViewController:bookmarkNavController animated:YES];
    }
    
}

@end
