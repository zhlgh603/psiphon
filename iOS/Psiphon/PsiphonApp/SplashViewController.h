//
//  SplashViewController.h
//  Psiphon
//
//  Created by eugene on 2013-08-22.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface SplashViewController : UIViewController
@property (strong, nonatomic) UILabel *messageLabel;

-(void) showMesage:(NSString*) message;


@end
