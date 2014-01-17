//
//  AppDelegate.h
//  Psiphon
//
//  Created by eugene on 2013-08-07.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "BrowserViewController.h"
#import "SplashViewController.h"
#import "TunnelCore.h"
#import "PsiphonConstants.h"

@interface AppDelegate : UIResponder <UIApplicationDelegate, EventsDelegate>

@property (strong, nonatomic) UIWindow *window;

@property (readonly, strong, nonatomic) NSManagedObjectContext *managedObjectContext;
@property (readonly, strong, nonatomic) NSManagedObjectModel *managedObjectModel;
@property (readonly, strong, nonatomic) NSPersistentStoreCoordinator *persistentStoreCoordinator;

@property (strong, nonatomic) BrowserViewController *browserViewController;
@property (strong, nonatomic)  SplashViewController *splashViewConroller;
@property (strong, nonatomic)  UIViewController *currentViewContoller;



@property (strong, nonatomic) TunnelCore *tunnelCore;
@property (strong, nonatomic) NSMutableArray* logQueue;





- (void)saveContext;
- (NSURL *)applicationDocumentsDirectory;

@end

extern AppDelegate *appDelegate;

