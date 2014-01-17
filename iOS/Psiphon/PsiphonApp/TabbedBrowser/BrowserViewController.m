//
//  BrowserViewController.m
//  TabbedBrowser
//
//  Created by eugene on 2013-03-04.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import "BrowserViewController.h"
#import "BlankController.h"

#define SG_EXPOSED_SCALE (0.76f)
#define SG_EXPOSED_TRANSFORM (CGAffineTransformMakeScale(SG_EXPOSED_SCALE, SG_EXPOSED_SCALE))
#define SG_CONTAINER_EMPTY (_viewControllers.count == 0)

#define SG_DURATION 0.25

@implementation BrowserViewController {
    NSMutableArray *_viewControllers;
    int _dialogResult;
}
@synthesize exposeMode = _exposeMode;
@synthesize bookmarks = _bookmarks;

-(id) initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        _bookmarks = [[Bookmarks alloc] init];
    }
    return self;
}

- (void)loadView {
    CGRect frame = [UIScreen mainScreen].bounds;
    self.view = [[UIView alloc] initWithFrame:frame];
    
    _viewControllers = [NSMutableArray arrayWithCapacity:10];
    _pageControl = [[UIPageControl alloc] initWithFrame:CGRectMake(0, frame.size.height - 25.,
                                                                   frame.size.width, 25.)];
    [self.view addSubview:_pageControl];
    
    _toolbar = [[BrowserToolBar alloc] initWithFrame:CGRectMake(0, 0, frame.size.width, 44.) browser:self];
    [self.view addSubview:_toolbar];
    
    _scrollView = [[UIScrollView alloc] initWithFrame:CGRectMake(0, 45., frame.size.width, frame.size.height-45.)];
    
    
    if (_scrollView) {
        _scrollView.autoresizingMask = (UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight);
        _scrollView.clipsToBounds = YES;
        _scrollView.backgroundColor = [UIColor clearColor];
        _scrollView.pagingEnabled = YES;
        _scrollView.showsHorizontalScrollIndicator = NO;
        _scrollView.showsVerticalScrollIndicator = NO;
        _scrollView.scrollsToTop = NO;
        _scrollView.delaysContentTouches = NO;
    }

    [self.view addSubview:_scrollView];
    
    _closeButton = [UIButton buttonWithType:UIButtonTypeCustom];
    _closeButton.frame = CGRectMake(0, 0, 35, 35);
    [self.view addSubview:_closeButton];
}

- (void)viewDidLoad {
    [super viewDidLoad];
    
    self.view.backgroundColor = [UIColor scrollViewTexturedBackgroundColor];
    self.scrollView.delegate = self;
    
    self.pageControl.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleTopMargin;
    [self.pageControl addTarget:self action:@selector(updatePage) forControlEvents:UIControlEventValueChanged];
    
    NSString *text = NSLocalizedString(@"New Tab", @"New Tab");
    UIFont *font = [UIFont fontWithName:@"HelveticaNeue-Bold" size:20];
    CGSize size = [text sizeWithFont:font];
    
    UIButton *button  = [UIButton buttonWithType:UIButtonTypeCustom];
    button.frame = CGRectMake(5, 10, 40 + size.width, size.height);
    button.autoresizingMask = UIViewAutoresizingFlexibleBottomMargin | UIViewAutoresizingFlexibleRightMargin;
    button.backgroundColor  = [UIColor clearColor];
    button.titleLabel.font = font;
    button.showsTouchWhenHighlighted = YES;
    [button setTitle:text forState:UIControlStateNormal];
    [button setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    [button setImage:[UIImage imageNamed:@"plus-white"] forState:UIControlStateNormal];
    button.titleEdgeInsets = UIEdgeInsetsMake(0, 10, 0, 0);
    [button addTarget:self action:@selector(addTab) forControlEvents:UIControlEventTouchUpInside];

    [self.view insertSubview:button belowSubview:self.toolbar];
    
    button = [UIButton buttonWithType:UIButtonTypeCustom];
    button.frame = CGRectMake(self.view.bounds.size.width - 80, 4, 36, 36);
    button.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin;
    button.backgroundColor = [UIColor clearColor];
    [button setImage:[UIImage imageNamed:@"grip-white"] forState:UIControlStateNormal];
    [button setImage:[UIImage imageNamed:@"grip-white-pressed"] forState:UIControlStateHighlighted];
    [button addTarget:self.toolbar action:@selector(showOptions:) forControlEvents:UIControlEventTouchUpInside];
    [self.view insertSubview:button belowSubview:self.toolbar];
    
    _tabsButton = [UIButton buttonWithType:UIButtonTypeCustom];
    _tabsButton.frame = CGRectMake(self.view.bounds.size.width - 40, 4, 36, 36);
    _tabsButton.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin;
    _tabsButton.backgroundColor = [UIColor clearColor];
    _tabsButton.titleEdgeInsets = UIEdgeInsetsMake(5, 5, 0, 0);
    _tabsButton.titleLabel.font = [UIFont systemFontOfSize:12.5];
    [_tabsButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    [_tabsButton setTitle:@"0" forState:UIControlStateNormal];
    [_tabsButton setBackgroundImage:[UIImage imageNamed:@"expose-white"] forState:UIControlStateNormal];
    [_tabsButton setBackgroundImage:[UIImage imageNamed:@"expose-white-pressed"] forState:UIControlStateHighlighted];
    [_tabsButton addTarget:self action:@selector(pressedTabsButton:) forControlEvents:UIControlEventTouchUpInside];
    [self.view insertSubview:_tabsButton belowSubview:self.toolbar];
    
    font = [UIFont fontWithName:@"HelveticaNeue" size:16];
    _titleLabel = [[UILabel alloc] initWithFrame:CGRectMake(5, 45, self.view.bounds.size.width - 5, font.lineHeight)];
    _titleLabel.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleBottomMargin;
    _titleLabel.textAlignment = NSTextAlignmentCenter;
    _titleLabel.backgroundColor = [UIColor clearColor];
    _titleLabel.textColor = [UIColor whiteColor];
    _titleLabel.lineBreakMode = UILineBreakModeMiddleTruncation;
    [self.view insertSubview:_titleLabel belowSubview:self.scrollView];
    
    _closeButton.hidden = YES;
    _closeButton.autoresizingMask = UIViewAutoresizingFlexibleBottomMargin | UIViewAutoresizingFlexibleRightMargin | UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleLeftMargin;
    _closeButton.backgroundColor  = [UIColor clearColor];
    [_closeButton setImage:[UIImage imageNamed:@"close_x"] forState:UIControlStateNormal];
    [_closeButton addTarget:self action:@selector(closeTabButton:) forControlEvents:UIControlEventTouchDown];
    
    self.toolbar.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleBottomMargin;
    
    [self showBlank];
    
}

- (void)viewDidUnload {
    [super viewDidUnload];
    _toolbar = nil;
    _scrollView = nil;
    _pageControl = nil;
    _closeButton = nil;
}

- (void)willRotateToInterfaceOrientation:(UIInterfaceOrientation)toInterfaceOrientation duration:(NSTimeInterval)duration {
    self.scrollView.delegate = nil;
}

- (void)willAnimateRotationToInterfaceOrientation:(UIInterfaceOrientation)toInterfaceOrientation
                                         duration:(NSTimeInterval)duration {
    [self arrangeChildViewControllers];
    
    self.scrollView.contentSize = CGSizeMake(self.scrollView.bounds.size.width * _viewControllers.count, 1);
    self.scrollView.contentOffset = CGPointMake(self.scrollView.bounds.size.width * self.pageControl.currentPage, 0);
    [self setCloseButtonHidden:NO];
}

- (void)didRotateFromInterfaceOrientation:(UIInterfaceOrientation)fromInterfaceOrientation {
    [self arrangeChildViewControllers];
    self.scrollView.delegate = self;
}

- (UIView *)rotatingHeaderView {
    return self.toolbar;
}

- (NSUInteger)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskAllButUpsideDown;
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)toInterfaceOrientation {
    return toInterfaceOrientation != UIInterfaceOrientationPortraitUpsideDown;
}

- (BOOL)prefersStatusBarHidden {
    return YES;
}

#pragma mark - Methods

- (void)addViewController:(UIViewController *)childController {
    NSInteger index = 0;
    if (_viewControllers.count == 0) {
        [_viewControllers addObject:childController];
        self.closeButton.hidden = !self.exposeMode;
    } else {
        index = self.pageControl.currentPage+1;
        [_viewControllers insertObject:childController atIndex:index];
    }
    
    [self addChildViewController:childController];
    
    childController.view.clipsToBounds = YES;
    childController.view.transform = CGAffineTransformIdentity;
    childController.view.frame = CGRectMake(self.scrollView.frame.size.width * index,
                                            0,
                                            self.scrollView.frame.size.width,
                                            self.scrollView.frame.size.height);
    if (_exposeMode) {
        childController.view.userInteractionEnabled = NO;
        childController.view.transform = SG_EXPOSED_TRANSFORM;
    }
    
    NSUInteger count = _viewControllers.count;
    self.pageControl.numberOfPages = count;
    self.scrollView.contentSize = CGSizeMake(self.scrollView.frame.size.width * count, 1);
    
    for (NSUInteger i = index+1; i < count; i++) {
        UIViewController *vC = _viewControllers[i];
        vC.view.center = CGPointMake(vC.view.center.x + self.scrollView.frame.size.width,
                                     vC.view.center.y);
    }
    [childController didMoveToParentViewController:self];
    [self arrangeChildViewControllers];
    
    if (_viewControllers.count > 1 && [childController isKindOfClass:[WebViewController class]])
        [((WebViewController *)childController).webView.scrollView setScrollsToTop:NO];
}

- (void)showViewController:(UIViewController *)viewController {
    NSUInteger index = [_viewControllers indexOfObject:viewController];
    if (index != NSNotFound) {
        self.pageControl.currentPage = index;
        
        // self.count != 1 as a workaround when there is just 1 view and no scrolling animation
        [self setCloseButtonHidden:self.count != 1];
        [self.scrollView setContentOffset:CGPointMake(self.scrollView.frame.size.width*index, 0)
                                 animated:YES];
        
        if (!_exposeMode)
            [self enableScrollsToTop];
    }
}

- (void)removeViewController:(UIViewController *)childController withIndex:(NSUInteger)index {
    if (!childController || index == NSNotFound)
        return;
    
    [self setCloseButtonHidden:YES];
    [childController willMoveToParentViewController:nil];
    [_viewControllers removeObjectAtIndex:index];
    
    NSUInteger count = _viewControllers.count;
    [UIView animateWithDuration:SG_DURATION
                     animations:^{
                         for (NSUInteger i = index; i < count; i++) {
                             UIViewController *vC = _viewControllers[i];
                             vC.view.center = CGPointMake(vC.view.center.x - self.scrollView.frame.size.width,
                                                          vC.view.center.y);
                         }
                         [childController.view removeFromSuperview];
                         self.pageControl.numberOfPages = count;
                         self.scrollView.contentSize = CGSizeMake(self.scrollView.frame.size.width * count, 1);
                     }
                     completion:^(BOOL done){
                         [childController removeFromParentViewController];
                         [self arrangeChildViewControllers];
                         [self updateChrome];
                         
                         if (_exposeMode)
                             self.closeButton.hidden = SG_CONTAINER_EMPTY;
                         else
                             [self enableScrollsToTop];
                     }];
}

- (void)removeViewController:(UIViewController *)childController {
    [self removeViewController:childController withIndex:[_viewControllers indexOfObject:childController]];
}

- (void)removeIndex:(NSUInteger)index {
    [self removeViewController:_viewControllers[index] withIndex:index];
}

- (void)swapCurrentViewControllerWith:(UIViewController *)viewController {
    UIViewController *old = [self selectedViewController];
    
    if (!old)
        [self addViewController:viewController];
    else if (![_viewControllers containsObject:viewController]) {
        [self addChildViewController:viewController];
        
        NSUInteger index = [self selectedIndex];
        
        viewController.view.frame = old.view.frame;
        
        [old willMoveToParentViewController:nil];
        [self transitionFromViewController:old
                          toViewController:viewController
                                  duration:0
                                   options:UIViewAnimationOptionAllowAnimatedContent
                                animations:NULL
                                completion:^(BOOL finished){
                                    [old removeFromParentViewController];
                                    _viewControllers[index] = viewController;
                                    [viewController didMoveToParentViewController:self];
                                    [self arrangeChildViewControllers];
                                    [self updateChrome];
                                    
                                    if (!_exposeMode)
                                        [self enableScrollsToTop];
                                }];
    }
}

- (void)updateChrome {
    [[UIApplication sharedApplication] setNetworkActivityIndicatorVisible:self.isLoading];
    
    self.titleLabel.text = self.selectedViewController.title;
    
    _tabsButton.enabled = !SG_CONTAINER_EMPTY;
    NSString *text = [NSString stringWithFormat:@"%d", self.count];
    [_tabsButton setTitle:text forState:UIControlStateNormal];
    
    [self.toolbar updateChrome];
};

- (UIViewController *)selectedViewController {
    return _viewControllers.count > 0 ? _viewControllers[self.pageControl.currentPage] : nil;
}

- (NSUInteger)selectedIndex {
    return self.pageControl.currentPage;
}

- (NSUInteger)count {
    return _viewControllers.count;
}

- (NSUInteger)maxCount {
    return 50;
}

#pragma mark - Utility

- (void)arrangeChildViewControllers {
    NSInteger current = self.pageControl.currentPage;
    
    for (NSInteger i = 0; i < _viewControllers.count; i++) {
        UIViewController *viewController = _viewControllers[i];
        if (current - 1  <= i && i <= current + 1) {
            viewController.view.transform = CGAffineTransformIdentity;
            viewController.view.frame = CGRectMake(self.scrollView.frame.size.width * i,
                                                   0,
                                                   self.scrollView.frame.size.width,
                                                   self.scrollView.frame.size.height);
            if (_exposeMode)
                viewController.view.transform = SG_EXPOSED_TRANSFORM;
            
            if (!viewController.view.superview)
                [self.scrollView addSubview:viewController.view];
        } else if (viewController.view.superview)
            [viewController.view removeFromSuperview];
    }
}

- (void)scaleChildViewControllers {
    CGFloat offset = self.scrollView.contentOffset.x;
    CGFloat width = self.scrollView.frame.size.width;
    
    for (NSUInteger i = 0; i < _viewControllers.count; i++) {
        UIViewController *viewController = _viewControllers[i];
        if (_exposeMode) {
            viewController.view.userInteractionEnabled = NO;
            viewController.view.transform = SG_EXPOSED_TRANSFORM;
        } else {
            viewController.view.userInteractionEnabled = YES;
            
            CGFloat y = i * width;
            CGFloat value = (offset-y)/width;
            CGFloat scale = 1.f-fabs(value);
            if (scale > 1.f) scale = 1.f;
            if (scale < SG_EXPOSED_SCALE) scale = SG_EXPOSED_SCALE;
            
            viewController.view.transform = CGAffineTransformMakeScale(scale, scale);
        }
    }
}

- (void)setCloseButtonHidden:(BOOL)hidden {
    if (self.count > 0) {
        CGPoint point = self.selectedViewController.view.frame.origin;
        self.closeButton.center = [self.view convertPoint:point fromView:self.scrollView];
        self.closeButton.hidden = !_exposeMode  || SG_CONTAINER_EMPTY || hidden;
    } else
        self.closeButton.hidden = YES;
}

// Setting the currently selecte view as the one which receives the scrolls to top gesture
- (void)enableScrollsToTop {
    for (NSUInteger i = 0; i < _viewControllers.count; i++) {
        BOOL enable = i == self.pageControl.currentPage;
        UIViewController *viewController = _viewControllers[i];
        if ([viewController isKindOfClass:[WebViewController class]])
            [((WebViewController *)viewController).webView.scrollView setScrollsToTop:enable];
    }
}

// Disabling the gesture for everyone
- (void)disableScrollsToTop {
    for (UIViewController *viewController in _viewControllers) {
        if ([viewController isKindOfClass:[WebViewController class]])
            [((WebViewController *)viewController).webView.scrollView setScrollsToTop:NO];
    }
}

#pragma mark UIScrollViewDelegate

- (void)scrollViewWillBeginDragging:(UIScrollView *)scrollView {
    if (!_exposeMode)
        [self disableScrollsToTop];
    
    [self setCloseButtonHidden:YES];
}

- (void)scrollViewDidEndDecelerating:(UIScrollView *)scrollView {
    [self setCloseButtonHidden:NO];
    if (!_exposeMode)
        [self enableScrollsToTop];
    
    [self arrangeChildViewControllers];
    [self updateChrome];
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView *)scrollView {
    [self setCloseButtonHidden:NO];
    [self arrangeChildViewControllers];
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView {
    CGFloat offset = scrollView.contentOffset.x;
    CGFloat width = scrollView.frame.size.width;
    
    NSInteger nextPage = (NSInteger)fabsf((offset+(width/2))/width);
    if (nextPage != self.pageControl.currentPage) {
        self.pageControl.currentPage = nextPage;
        [self arrangeChildViewControllers];
        [self updateChrome];
    }
    
    [self scaleChildViewControllers];
}

- (void)setExposeMode:(BOOL)exposeMode animated:(BOOL)animated {
    _exposeMode = exposeMode;
    CGFloat duration = animated ? SG_DURATION : 0;
    if (exposeMode) {
        [self.toolbar.searchField resignFirstResponder];
        [self disableScrollsToTop];
        
        [UIView animateWithDuration:duration
                         animations:^{
                             self.toolbar.frame = CGRectMake(0, -self.toolbar.frame.size.height,
                                                             self.view.bounds.size.width, self.toolbar.frame.size.height);
                             [self scaleChildViewControllers];
                         }
                         completion:^(BOOL finished) {
                             UITapGestureRecognizer *rec = [[UITapGestureRecognizer alloc] initWithTarget:self
                                                                                                   action:@selector(tappedViewController:)];
                             rec.cancelsTouchesInView = NO;
                             [self.view addGestureRecognizer:rec];
                             [self arrangeChildViewControllers];
                             [self setCloseButtonHidden:NO];
                         }];
        
    } else {
        [self enableScrollsToTop];
        self.closeButton.hidden = YES;
        [UIView animateWithDuration:duration
                         animations:^{
                             self.toolbar.frame = CGRectMake(0, 0, self.view.bounds.size.width, self.toolbar.frame.size.height);
                             [self scaleChildViewControllers];
                         }
         ];
    }
}

- (void)setExposeMode:(BOOL)exposeMode {
    [self setExposeMode:exposeMode animated:NO];
}


- (void)addTab; {
    if (self.count >= self.maxCount) {
        return;
    }
    BlankController *latest = [BlankController new];
    [self addViewController:latest];
    [self showViewController:latest];
    [self updateChrome];
}

- (void)addTabWithURL:(NSURL *)url withTitle:(NSString *)title;{
    WebViewController *webC = [[WebViewController alloc] initWithNibName:nil bundle:nil];
    webC.title = title;
    [webC openURL:url];
    [self addViewController:webC];
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"org.graetzer.tabs.foreground"])
        [self showViewController:webC];
    
    if (self.count >= self.maxCount) {
        if ([self selectedIndex] != 0)
            [self removeIndex:0];
        else
            [self removeIndex:1];
    }
    [self updateChrome];
}

- (void)reload; {
    if ([[self selectedViewController] isKindOfClass:[WebViewController class]]) {
        WebViewController *webC = (WebViewController *)[self selectedViewController];
        [webC reload];
        [self updateChrome];
    }
}

- (void)stop {
    if ([[self selectedViewController] isKindOfClass:[WebViewController class]]) {
        WebViewController *webC = (WebViewController *)[self selectedViewController];
        [webC.webView stopLoading];
        [self updateChrome];
    }
}

- (BOOL)isLoading {
    if ([[self selectedViewController] isKindOfClass:[WebViewController class]]) {
        WebViewController *webC = (WebViewController *)[self selectedViewController];
        return webC.loading;
    }
    return NO;
}

- (void)goBack; {
    if ([[self selectedViewController] isKindOfClass:[WebViewController class]]) {
        WebViewController *webC = (WebViewController *)[self selectedViewController];
        [webC.webView goBack];
        [self updateChrome];
    }
}

- (void)goForward; {
    if ([[self selectedViewController] isKindOfClass:[WebViewController class]]) {
        WebViewController *webC = (WebViewController *)[self selectedViewController];
        [webC.webView goForward];
        [self updateChrome];
    }
}

- (BOOL)canGoBack; {
    if ([[self selectedViewController] isKindOfClass:[WebViewController class]]) {
        WebViewController *webC = (WebViewController *)[self selectedViewController];
        return [webC.webView canGoBack];
    }
    return NO;
}

- (BOOL)canGoForward; {
    if ([[self selectedViewController] isKindOfClass:[WebViewController class]]) {
        WebViewController *webC = (WebViewController *)[self selectedViewController];
        return [webC.webView canGoForward];
    }
    return NO;
}

- (BOOL)canStopOrReload {
    if ([[self selectedViewController] isKindOfClass:[WebViewController class]]) {
        return YES;
    }
    return NO;
}

- (BOOL)canRemoveTab:(UIViewController *)viewController {
    if ([viewController isKindOfClass:[BlankController class]] && self.count == 1) {
        return NO;
    }
    return YES;
}

- (NSURL *)URL {
    if ([[self selectedViewController] isKindOfClass:[WebViewController class]]) {
        WebViewController *webC = (WebViewController *)[self selectedViewController];
        return webC.location;
    }
    return nil;
}

- (void) showBlank
{
    BlankController *latest = [BlankController new];
    [self addViewController:latest];
    [self updateChrome];
}

- (void)openURL:(NSURL *)url title:(NSString *)title {
    if (!url)
        return;
    
    if (!title)
        title = url.host;
    
    if ([self.selectedViewController isKindOfClass:[WebViewController class]]) {
        WebViewController *webC = (WebViewController *)[self selectedViewController];
        webC.title = title;
        [webC openURL:url];
    } else {
        WebViewController *webC = [[WebViewController alloc] initWithNibName:nil bundle:nil];
        webC.title = title;
        [webC openURL:url];
        [self swapCurrentViewControllerWith:webC];
    }
}

- (BOOL)isSearchTerm:(NSString*)string
{
    if ([string hasPrefix:@"http://"] || [string hasPrefix:@"https://"])
        return NO;
    
    // Can be open by any app?
    if ([[UIApplication sharedApplication] canOpenURL:[NSURL URLWithString:string]])
        return NO;
    
    // If more than one word?
    if ([string rangeOfString:@" "].location != NSNotFound)
        return YES;
    
    NSURL *url = [NSURL URLWithString:[NSString stringWithFormat:@"http://%@", string]];
    
    //single word that can't be parsed?
    if (!url)
        return YES;
    
    // localhost - special case
    if ([url.host isEqualToString:@"localhost"])
        return NO;
    
    // dots mean user wanted to enter an URL
    if ([url.host rangeOfString:@"."].location != NSNotFound)
        return NO;
    
    return YES;
}

- (void)handleURLString:(NSString*)input title:(NSString *)title
{
    NSURL *url = nil;
    
    input = [input stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    if([input isEqualToString:@""])
    {
        return;
    }
    
    if([self isSearchTerm:input])
    {
        NSString *searchEngineFormatStr = [[NSUserDefaults standardUserDefaults] stringForKey:@"search_engine_url"];
        NSString *locale = [NSLocale preferredLanguages][0];

        //using http://stackoverflow.com/questions/8086584/objective-c-url-encoding
        //for URL encoding
        NSString *escapedInput =
        (NSString *)CFBridgingRelease(CFURLCreateStringByAddingPercentEscapes(
                                                                              NULL,
                                                                              (__bridge CFStringRef) input,
                                                                              NULL,
                                                                              CFSTR("!*'();:@&=+$,/?%#[]"),
                                                                              kCFStringEncodingUTF8));
        
        @try {
            url = [NSURL URLWithString:[NSString stringWithFormat:searchEngineFormatStr, escapedInput, locale]];
        }
        @catch (NSException *e) {
            if ([e.name isEqualToString:NSInvalidArgumentException])
            {
                url = [NSURL URLWithString:[NSString stringWithFormat:@"http://%@", input]];
            }
            else {
                [e raise];
            }
        }
    }
    else
    {
        url = [NSURL URLWithString:input];
        if(!url.host)
        {
            url = [NSURL URLWithString:[NSString stringWithFormat:@"http://%@", input]];
        }
    }
    if (!title)
    {
        title = url.host;
    }
    
    [self openURL:url title:title];
}


- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex {
    _dialogResult = buttonIndex;
}
#pragma mark - IBAction

- (IBAction)updatePage {
    [self.scrollView setContentOffset:CGPointMake(self.scrollView.frame.size.width*self.pageControl.currentPage, 0)
                             animated:YES];
}

- (IBAction)tappedViewController:(UITapGestureRecognizer *)recognizer {
    if (recognizer.state == UIGestureRecognizerStateEnded) {
        UIView *view = [self selectedViewController].view;
        CGPoint pos = [recognizer locationInView:view];
        if (CGRectContainsPoint(view.bounds, pos)) {
            [self setExposeMode:NO animated:YES];
            [self.view removeGestureRecognizer:recognizer];
        }
    }
}

- (IBAction)pressedTabsButton:(id)sender {
    if (!SG_CONTAINER_EMPTY)
        [self setExposeMode:NO animated:YES];
}

- (IBAction)closeTabButton:(UIButton *)button {
    [self removeViewController:self.selectedViewController];
}



#pragma mark - BookmarkEditViewControllerDelegate methods

-(void) bookmarkEditingDone:(Bookmark*)bookmark
{
    if(bookmark.order.intValue == -1)
    {
        [self.bookmarks addBookmark:bookmark];
    }
    else
    {
        [self.bookmarks updateBookmark:bookmark];
    }
    
}

@end
