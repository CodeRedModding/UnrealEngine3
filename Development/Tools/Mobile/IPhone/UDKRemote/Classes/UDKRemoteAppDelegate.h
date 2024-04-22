//
//  UDKRemoteAppDelegate.h
//  UDKRemote
//
//  Created by jadams on 7/28/10.
//  Copyright __MyCompanyName__ 2010. All rights reserved.
//

#import <UIKit/UIKit.h>

@class MainViewController;

@interface UDKRemoteAppDelegate : NSObject <UIApplicationDelegate> 
{
	UIWindow *window;
	MainViewController *mainViewController;
}

@property (nonatomic, retain) IBOutlet UIWindow *window;
@property (nonatomic, retain) MainViewController *mainViewController;

/** Properties set in the settings view */
@property (retain) NSString* PCAddress;
@property int Port;
@property BOOL bShouldIgnoreTilt;
@property BOOL bShouldIgnoreTouch;
@property BOOL bLockOrientation;
@property UIInterfaceOrientation LockedOrientation;
@property (retain) NSMutableArray* RecentComputers;


@end

