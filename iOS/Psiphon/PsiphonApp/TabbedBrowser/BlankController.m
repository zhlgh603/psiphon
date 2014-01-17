//
//  BlankController.m
//  TabbedBrowser
//
//  Created by eugene on 2013-03-05.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import "BlankController.h"
#import "BrowserViewController.h"

@interface BlankController ()

@end

@implementation BlankController

- (BrowserViewController *)getBrowserViewController {
    UIViewController *parent = self.parentViewController;
    return [parent isKindOfClass:[BrowserViewController class]] ?(BrowserViewController *)parent : nil;
}
- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        // Custom initialization
    }
    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    self.title = NSLocalizedString(@"Untitled", @"Untitled tab");
    self.view.backgroundColor = [UIColor whiteColor];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

@end
