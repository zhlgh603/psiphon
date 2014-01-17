//
//  SplashViewController.m
//  Psiphon
//
//  Created by eugene on 2013-08-22.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import "SplashViewController.h"
#import "Utils.h"

@interface SplashViewController ()


@end

@implementation SplashViewController

@synthesize messageLabel;

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        CGRect splashViewFrame = [[UIScreen mainScreen] bounds];
        UIView *splashView = [[UIView alloc] initWithFrame:splashViewFrame];
        
        UIImageView *imgView = [[UIImageView alloc] initWithImage:[UIImage imageNamed:@"splash_image.png"]];
        
        imgView.contentMode = UIViewContentModeScaleAspectFit;
        imgView.frame = CGRectMake(0, 60,
                                   splashViewFrame.size.width,
                                   splashViewFrame.size.height/3);
        
        [splashView addSubview:imgView];
        
        splashView.backgroundColor = [UIColor blackColor];
        /*
         or pick color from the image
        splashView.backgroundColor = [Utils getRGBAsFromImage:imgView.image atX:0 andY:0];
         */
        
        messageLabel = [ [UILabel alloc ] initWithFrame:CGRectMake(0, splashViewFrame.size.height/2,
                                                                                  splashViewFrame.size.width,
                                                                                  70) ];
        messageLabel.textAlignment =  NSTextAlignmentCenter;
        messageLabel.textColor = [UIColor whiteColor];
        messageLabel.backgroundColor = splashView.backgroundColor;
        //textView.font = [UIFont fontWithName:@"Arial Rounded MT Bold" size:(12.0)];
        [splashView addSubview:messageLabel];
        
        [self.view addSubview:splashView];
        
        messageLabel.text= @"This is a test message";
    }
    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
	// Do any additional setup after loading the view.
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

/*
- (BOOL)prefersStatusBarHidden {
    return YES;
}
 */

-(void) showMesage:(NSString*) message
{
    self.messageLabel.text = message;
}

@end
