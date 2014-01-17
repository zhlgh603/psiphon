//
//  WebViewController.m
//  TabbedBrowser
//
//  Created by eugene on 2013-03-05.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import "WebViewController.h"
#import "AppDelegate.h"
//#import "BrowserViewController.h"


@interface WebViewController ()

@end

@implementation WebViewController

- (void)loadView {
    __strong UIWebView *webView = [[UIWebView alloc] initWithFrame:CGRectZero];
    self.view = webView;
    self.webView = webView;
}

- (void)viewWillAppear:(BOOL)animated {
    if (!self.webView.request) {
        [self openURL:nil];
    }
}


- (void)viewDidLoad
{
    [super viewDidLoad];
	// Do any additional setup after loading the view.
    
    self.webView.frame = self.view.bounds;
    self.webView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.webView.backgroundColor = [UIColor colorWithWhite:1 alpha:0.1];
    UILongPressGestureRecognizer *gr = [[UILongPressGestureRecognizer alloc]
                                        initWithTarget:self action:@selector(handleLongPress:)];
    [self.webView addGestureRecognizer:gr];
    gr.delegate = self;
    self.webView.delegate = self;
    self.webView.scalesPageToFit = YES;
    
}
- (void) dealloc
{
    self.webView.delegate = nil;
}

- (void)openURL:(NSURL *)url {
    if (url)
        self.location = url;
    
    if (![self isViewLoaded])
        return;
    
    // In case the webView is empty, show the error on the site
    /*
     TODO
    if (![appDelegate canConnectToInternet] && ![self.webView isEmpty]) {
        UIAlertView *alert = [[UIAlertView alloc] initWithTitle:NSLocalizedString(@"Cannot Load Page", @"unable to load page")
                                                        message:NSLocalizedString(@"No internet connection available", nil)
                                                       delegate:nil cancelButtonTitle:NSLocalizedString(@"OK", nil) otherButtonTitles:nil];
        [alert show];
    } else {
        NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:self.location];
        [self.webView loadRequest:request];
    }
     */
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:self.location];
    [self.webView loadRequest:request];

}

- (void)reload {
    [self openURL:nil];
}

- (BOOL) handleURL:(NSURL*) url {
    
    //Prohibit opening local files and javascript links
    if( [url.scheme isEqualToString: @"file"] || [url.scheme isEqualToString: @"javascript"])
        return NO;
    

    //handle special URLs such as mailto, store links etc.
    BOOL openNatively = NO;
    if (url != nil)
	{
        
        // Basic case where it is a link to one of the native apps that is the only handler.
        
        static NSSet* nativeSchemes = nil;
        if (nativeSchemes == nil) {
            nativeSchemes = [NSSet setWithObjects: @"mailto", @"tel", @"sms", @"itms", nil];
        }
        
        if ([nativeSchemes containsObject: [url scheme]]) {
            openNatively = YES;
        }
        
        // Special case for handling links to the app store. See  http://developer.apple.com/library/ios/#qa/qa2008/qa1629.html
        // and http://developer.apple.com/library/ios/#qa/qa2008/qa1633.html for more info. Note that we do this even is
        // Use Native Apps is turned off. I think that is the right choice here since there is no web alternative for the
        // store.
        
        else if ([[url scheme] isEqual:@"http"] || [[url scheme] isEqual:@"https"])
        {
            if ([[url host] isEqualToString: @"itunes.com"])
            {
                if ([[url path] hasPrefix: @"/apps/"])
                {
                    openNatively = YES;
                }
            }
            else if ([[url host] isEqualToString: @"phobos.apple.com"] || [[url host] isEqualToString: @"itunes.apple.com"])
            {
                openNatively = YES;
            }
        }
	}
	
    if(openNatively) {
        [[UIApplication sharedApplication] openURL:url];
        return NO;
    }
    return YES;
}


#pragma mark - UIWebViewDelegate

-  (BOOL)webView:(UIWebView *)webView shouldStartLoadWithRequest:(NSURLRequest *)request navigationType:(UIWebViewNavigationType)navigationType {
    
    UIViewController *parent = self.parentViewController;
    BrowserViewController *browserViewController = [parent isKindOfClass:[BrowserViewController class]] ?(BrowserViewController *)parent : nil;
    if ([request.URL.scheme isEqualToString:@"newtab"]) {
        NSString *source = [request.URL resourceSpecifier];
        NSString *urlString = [source stringByReplacingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
        NSURL *url = [NSURL URLWithString:urlString relativeToURL:self.location];
        

        
        [browserViewController addTabWithURL:url withTitle:url.absoluteString];
        return NO;
    }
    
    if (navigationType != UIWebViewNavigationTypeOther) {
        self.location = request.mainDocumentURL;
        [browserViewController updateChrome];
    }
    return [self handleURL:request.URL];
}

- (void)webViewDidStartLoad:(UIWebView *)webView {
//    [self dismissSearchToolbar];
    self.loading = YES;
    UIViewController *parent = self.parentViewController;
    BrowserViewController *browserViewController = [parent isKindOfClass:[BrowserViewController class]] ?(BrowserViewController *)parent : nil;
    [browserViewController updateChrome];
}

- (void)webViewDidFinishLoad:(UIWebView *)webView {
    UIViewController *parent = self.parentViewController;
    BrowserViewController *browserViewController = [parent isKindOfClass:[BrowserViewController class]] ?(BrowserViewController *)parent : nil;
    self.title = [webView stringByEvaluatingJavaScriptFromString:@"document.title"];
    self.loading = NO;
    
    [self.webView stringByEvaluatingJavaScriptFromString:@"document.body.style.webkitTouchCallout='none';"];

    
    [browserViewController updateChrome];
}

//there are too many spurious warnings, so I'm going to just ignore or log them all for now.
- (void)webView:(UIWebView *)webView didFailLoadWithError:(NSError *)error {
    self.loading = NO;
    UIViewController *parent = self.parentViewController;
    BrowserViewController *browserViewController = [parent isKindOfClass:[BrowserViewController class]] ?(BrowserViewController *)parent : nil;
    [browserViewController updateChrome];
    
    DLog(@"WebView error code: %d", error.code);
    //ignore these
    if (error.code == NSURLErrorCancelled || [error.domain isEqualToString:@"WebKitErrorDomain"]) return;
    
    NSString *title = NSLocalizedString(@"Error Loading Page", @"error loading page");

    UIAlertView *alert = [[UIAlertView alloc] initWithTitle:title
                                                        message:error.localizedDescription
                                                       delegate:nil
                                              cancelButtonTitle:NSLocalizedString(@"OK", @"ok")
                                              otherButtonTitles: nil];
        [alert show];
}


#pragma mark - UILongPressGesture
- (IBAction)handleLongPress:(UILongPressGestureRecognizer *)sender {
    if (sender.state == UIGestureRecognizerStateBegan) {
        /*
        CGPoint at = [sender locationInView:self.webView];
        CGPoint pt = at;
        
        // convert point from view to HTML coordinate system
        //CGPoint offset  = [self.webView scrollOffset];
        CGSize viewSize = [self.webView frame].size;
        CGSize windowSize = [self.webView windowSize];
        
        CGFloat f = windowSize.width / viewSize.width;
        pt.x = pt.x * f ;//+ offset.x;
        pt.y = pt.y * f ;//+ offset.y;
        
        [self contextMenuFor:pt showAt:at];
         */
    }
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer {
    return YES;
}


@end
