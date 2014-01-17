//
//  WebViewController.h
//  TabbedBrowser
//
//  Created by eugene on 2013-03-05.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <Foundation/Foundation.h>

@interface WebViewController : UIViewController<UIWebViewDelegate, UIGestureRecognizerDelegate,
UIActionSheetDelegate, UIAlertViewDelegate>

@property (weak, nonatomic) UIWebView *webView;
@property (weak, nonatomic) UIToolbar *searchToolbar;

@property (strong, nonatomic) NSURL *location;
@property (assign, nonatomic) BOOL loading;

- (void)openURL:(NSURL *)url;
- (void)reload;
- (BOOL)handleURL:(NSURL *)url;
@end
