//
//  BrowserToolBar.h
//  TabbedBrowser
//
//  Created by eugene on 2013-03-04.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "BrowserViewController.h"
#import "AddressField.h"

@class BrowserViewController;

@interface BrowserToolBar : UIView <UITextFieldDelegate, UIActionSheetDelegate>
@property (readonly, nonatomic) AddressField *searchField;

@property (readonly, nonatomic) UIButton *backButton;
@property (readonly, nonatomic) UIButton *forwardButton;
@property (readonly, nonatomic) UIButton *optionsButton;
@property (readonly, nonatomic) UIButton *tabsButton;
@property (readonly, nonatomic) UIButton *cancelButton;

@property (strong, nonatomic) UINavigationController *bookmarks;
@property (strong, nonatomic) UIActionSheet *actionSheet;
@property (weak, nonatomic) BrowserViewController *browser;

- (id)initWithFrame:(CGRect)frame browser:(BrowserViewController *)browser;
- (void)updateChrome;

@end
