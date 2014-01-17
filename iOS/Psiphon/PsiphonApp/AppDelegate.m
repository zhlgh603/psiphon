//
//  AppDelegate.m
//  Psiphon
//
//  Created by eugene on 2013-08-07.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import "AppDelegate.h"
#import "ProxyURLProtocol.h"

AppDelegate* appDelegate = nil;


@implementation AppDelegate

@synthesize managedObjectContext = _managedObjectContext;
@synthesize managedObjectModel = _managedObjectModel;
@synthesize persistentStoreCoordinator = _persistentStoreCoordinator;
@synthesize browserViewController = _browserViewController;
@synthesize splashViewConroller = _splashViewController;
@synthesize currentViewContoller = _currentViewContoller;
@synthesize tunnelCore = _tunnelCore;
@synthesize logQueue = _logQueue;

+ (void)initialize
{
    if (![[NSUserDefaults standardUserDefaults] objectForKey:@"search_engine_url"])  {
        
        NSString  *mainBundlePath = [[NSBundle mainBundle] bundlePath];
        NSString  *settingsPropertyListPath = [mainBundlePath
                                               stringByAppendingPathComponent:@"Settings.bundle/Browser.plist"];
        
        NSDictionary *settingsPropertyList = [NSDictionary
                                              dictionaryWithContentsOfFile:settingsPropertyListPath];
        
        NSMutableArray      *preferenceArray = [settingsPropertyList objectForKey:@"PreferenceSpecifiers"];
        NSMutableDictionary *registerableDictionary = [NSMutableDictionary dictionary];
        
        for (int i = 0; i < [preferenceArray count]; i++)  {
            NSString  *key = [[preferenceArray objectAtIndex:i] objectForKey:@"Key"];
            
            if (key)  {
                id  value = [[preferenceArray objectAtIndex:i] objectForKey:@"DefaultValue"];
                [registerableDictionary setObject:value forKey:key];
            }
        }
        
        [[NSUserDefaults standardUserDefaults] registerDefaults:registerableDictionary];
        [[NSUserDefaults standardUserDefaults] synchronize];
    }
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    appDelegate = self;
    
    signal(SIGPIPE, SIG_IGN);
    
    _tunnelCore = [[TunnelCore alloc] init];
    _tunnelCore.eventsDelegate = self;
    
    CGRect frame = [[UIScreen mainScreen] bounds];

    self.window = [[UIWindow alloc] initWithFrame:frame];
    

    self.window.backgroundColor = [UIColor whiteColor];
    
    _browserViewController = [[BrowserViewController alloc] initWithNibName:nil bundle:nil];
    _browserViewController.definesPresentationContext = YES;
    _browserViewController.providesPresentationContextTransitionStyle = YES;
    _browserViewController.modalTransitionStyle = UIModalTransitionStyleCrossDissolve;
    _browserViewController.modalPresentationStyle = UIModalPresentationCurrentContext;

    _splashViewController = [[SplashViewController alloc] initWithNibName:nil bundle:nil];
    
    self.window.rootViewController = _splashViewController;
    _currentViewContoller = _splashViewController;
    [self.window makeKeyAndVisible];
    
    //setup logging observer
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(receivedNewLogEntryNotification:)
                                                 name:NEW_LOG_ENTRY_POSTED
                                               object:nil];
    
    [NSURLProtocol registerClass:[ProxyURLProtocol class]];
    
    //setup logging timer & queue
    _logQueue = [[NSMutableArray alloc] init];
    [NSTimer scheduledTimerWithTimeInterval:0.3f
                                     target:self
                                   selector:@selector(showNextLogEntry)
                                   userInfo:nil
                                    repeats:YES];
    
    [_tunnelCore startTunnel];
    
    return YES;
}

-(void) addLogEntry:(NSString*) logEntry
{
    [_logQueue addObject:logEntry];
}

-(void) showNextLogEntry
{
    NSString* logEntry = [_logQueue firstObject];
    if(logEntry)
    {
        [_logQueue removeObjectAtIndex:0];
        _splashViewController.messageLabel.text = logEntry;
        [_splashViewController.messageLabel setNeedsDisplay];
    }
}

-(void) receivedNewLogEntryNotification:(NSNotification*) aNotification
{
    if ([[aNotification name] isEqualToString: NEW_LOG_ENTRY_POSTED])
    {
        NSDictionary *userInfo = aNotification.userInfo;
        if( userInfo )
        {
            NSString* logEntry = [userInfo objectForKey:LOG_ENTRY_NOTIFICATION_KEY];
            if(logEntry)
            {
                [self addLogEntry: logEntry];
            }
        }
    }
}

- (void)applicationWillResignActive:(UIApplication *)application
{
    // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
    // Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    // Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later. 
    // If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
    // Called as part of the transition from the background to the inactive state; here you can undo many of the changes made on entering the background.
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
    // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    // Saves changes in the application's managed object context before the application terminates.
    [self saveContext];
}

- (void)saveContext
{
    NSError *error = nil;
    NSManagedObjectContext *managedObjectContext = self.managedObjectContext;
    if (managedObjectContext != nil) {
        if ([managedObjectContext hasChanges] && ![managedObjectContext save:&error]) {
             // Replace this implementation with code to handle the error appropriately.
             // abort() causes the application to generate a crash log and terminate. You should not use this function in a shipping application, although it may be useful during development. 
            NSLog(@"Unresolved error %@, %@", error, [error userInfo]);
            abort();
        } 
    }
}

#pragma mark - Core Data stack

// Returns the managed object context for the application.
// If the context doesn't already exist, it is created and bound to the persistent store coordinator for the application.
- (NSManagedObjectContext *)managedObjectContext
{
    if (_managedObjectContext != nil) {
        return _managedObjectContext;
    }
    
    NSPersistentStoreCoordinator *coordinator = [self persistentStoreCoordinator];
    if (coordinator != nil) {
        _managedObjectContext = [[NSManagedObjectContext alloc] init];
        [_managedObjectContext setPersistentStoreCoordinator:coordinator];
    }
    return _managedObjectContext;
}

// Returns the managed object model for the application.
// If the model doesn't already exist, it is created from the application's model.
- (NSManagedObjectModel *)managedObjectModel
{
    if (_managedObjectModel != nil) {
        return _managedObjectModel;
    }
    NSURL *modelURL = [[NSBundle mainBundle] URLForResource:@"BrowserData" withExtension:@"momd"];
    _managedObjectModel = [[NSManagedObjectModel alloc] initWithContentsOfURL:modelURL];
    return _managedObjectModel;
}

// Returns the persistent store coordinator for the application.
// If the coordinator doesn't already exist, it is created and the application's store added to it.
- (NSPersistentStoreCoordinator *)persistentStoreCoordinator
{
    if (_persistentStoreCoordinator != nil) {
        return _persistentStoreCoordinator;
    }
    
    NSURL *storeURL = [[self applicationDocumentsDirectory] URLByAppendingPathComponent:@"Psiphon.sqlite"];
    
    NSError *error = nil;
    _persistentStoreCoordinator = [[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:[self managedObjectModel]];
    if (![_persistentStoreCoordinator addPersistentStoreWithType:NSSQLiteStoreType configuration:nil URL:storeURL options:nil error:&error]) {
        /*
         Replace this implementation with code to handle the error appropriately.
         
         abort() causes the application to generate a crash log and terminate. You should not use this function in a shipping application, although it may be useful during development. 
         
         Typical reasons for an error here include:
         * The persistent store is not accessible;
         * The schema for the persistent store is incompatible with current managed object model.
         Check the error message to determine what the actual problem was.
         
         
         If the persistent store is not accessible, there is typically something wrong with the file path. Often, a file URL is pointing into the application's resources directory instead of a writeable directory.
         
         If you encounter schema incompatibility errors during development, you can reduce their frequency by:
         * Simply deleting the existing store:
         [[NSFileManager defaultManager] removeItemAtURL:storeURL error:nil]
         
         * Performing automatic lightweight migration by passing the following dictionary as the options parameter:
         @{NSMigratePersistentStoresAutomaticallyOption:@YES, NSInferMappingModelAutomaticallyOption:@YES}
         
         Lightweight migration will only work for a limited set of schema changes; consult "Core Data Model Versioning and Data Migration Programming Guide" for details.
         
         */
        NSLog(@"Unresolved error %@, %@", error, [error userInfo]);
        abort();
    }    
    
    return _persistentStoreCoordinator;
}

#pragma mark - Application's Documents directory

// Returns the URL to the application's Documents directory.
- (NSURL *)applicationDocumentsDirectory
{
    return [[[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask] lastObject];
}


#pragma mark -
#pragma mark - EventsDelegate methods

-(void) signalUnexpectedDisconnect
{
    //stop webviews if loading, show splashview
    
//    [_browserViewController stopLoading];
    
    if(_currentViewContoller == _browserViewController)
        [_currentViewContoller dismissViewControllerAnimated:NO completion:^{
            _currentViewContoller = _splashViewController;
}];
}

-(void) signalHandshakeSuccess
{
    //load home pages
    if(_currentViewContoller != _browserViewController)
    {
        [self.window.rootViewController presentViewController:_browserViewController animated:NO completion:^{
            _currentViewContoller = _browserViewController;
        }];
    }
    NSString* homePage = [[[PsiphonData sharedInstance] homePages]objectAtIndex:0];
    
    [_browserViewController handleURLString:homePage title:nil];
}

#pragma mark -

@end
