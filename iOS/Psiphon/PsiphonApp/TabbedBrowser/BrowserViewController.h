//
//  BrowserViewController.h
//  TabbedBrowser
//
//  Created by eugene on 2013-03-04.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//
#import <UIKit/UIKit.h>
#import "BrowserToolBar.h"
#import "WebViewController.h"
#import "Bookmarks.h"
#import "BookmarkEditViewControllerDelegate.h"

@class BrowserToolBar;

@interface BrowserViewController : UIViewController<UIScrollViewDelegate, UIGestureRecognizerDelegate, BookmarkEditViewControllerDelegate>
@property (readonly, nonatomic) BrowserToolBar *toolbar;
@property (readonly, nonatomic) UIScrollView *scrollView;
@property (readonly, nonatomic) UIPageControl *pageControl;
@property (readonly, nonatomic) UIButton *closeButton;
@property (readonly, nonatomic) UIButton *tabsButton;
@property (readonly, nonatomic) UILabel *titleLabel;
@property (assign, nonatomic) BOOL exposeMode;
@property (nonatomic,retain) Bookmarks *bookmarks;

- (void)setExposeMode:(BOOL)exposeMode animated:(BOOL)animated;


/// Adds a tab, don't add the same instance twice!
- (void)addViewController:(UIViewController *)childController;

/// Bring a tab to the frontpage
- (void)showViewController:(UIViewController *)viewController;

// Remove a tap
- (void)removeViewController:(UIViewController *)childController;
- (void)removeIndex:(NSUInteger)index;

// Swap the current view controller. Used to replace the blankView with the webView
- (void)swapCurrentViewControllerWith:(UIViewController *)viewController;

- (void)updateChrome;

@property (readonly, nonatomic) UIViewController *selectedViewController;
@property (readonly, nonatomic) NSUInteger selectedIndex;
@property (readonly, nonatomic) NSUInteger count;
@property (readonly, nonatomic) NSUInteger maxCount;



// Add and show a BlankViewController
- (void)addTab;

- (void)addTabWithURL:(NSURL *)url withTitle:(NSString *)title;

- (void)reload;
- (void)stop;

- (BOOL)isLoading;

- (void)goBack;
- (void)goForward;

- (BOOL)canGoBack;
- (BOOL)canGoForward;

- (BOOL)canStopOrReload;
- (BOOL)canRemoveTab:(UIViewController *)viewController;

- (NSURL *)URL;
- (void)openURL:(NSURL *)url title:(NSString *)title;
- (void)handleURLString:(NSString*)input title:(NSString *)title;

@end
