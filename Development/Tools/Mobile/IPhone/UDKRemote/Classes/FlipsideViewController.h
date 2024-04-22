//
//  FlipsideViewController.h
//  UDKRemote
//
//  Created by jadams on 7/28/10.
//  Copyright __MyCompanyName__ 2010. All rights reserved.
//

#import <UIKit/UIKit.h>

@protocol FlipsideViewControllerDelegate;
@class UDKRemoteAppDelegate;

@interface FlipsideViewController : UINavigationController <UITextFieldDelegate> 
{
	id <FlipsideViewControllerDelegate> delegate;
	
	/** Cached app delegate */
	UDKRemoteAppDelegate* AppDelegate;
}

@property (nonatomic, assign) id <FlipsideViewControllerDelegate> delegate;

/** Table cell types */
@property (nonatomic, retain) IBOutlet UITableViewCell* DestIPCell;
@property (nonatomic, retain) IBOutlet UITableViewCell* PortCell;
@property (nonatomic, retain) IBOutlet UITableViewCell* TiltCell;
@property (nonatomic, retain) IBOutlet UITableViewCell* TouchCell;
@property (nonatomic, retain) IBOutlet UITableViewCell* LockCell;

/** Useful objects inside the table view cells */
@property (nonatomic, assign) IBOutlet UITextField* SubIPTextField;
@property (nonatomic, assign) IBOutlet UITextField* PortTextField;
@property (nonatomic, assign) IBOutlet UISwitch* SubTiltSwitch;
@property (nonatomic, assign) IBOutlet UISwitch* SubTouchSwitch;
@property (nonatomic, assign) IBOutlet UISwitch* SubLockSwitch;

/** The table views in the hierarchy */
@property (nonatomic, retain) IBOutlet UITableView* MainSettingsTable;
@property (nonatomic, retain) IBOutlet UITableView* RecentComputersTable;

@property (nonatomic, retain) IBOutlet UIViewController* RecentComputersController;
@property (nonatomic, assign) UITextField* NewComputerTextField;
@property (nonatomic, assign) IBOutlet UIBarButtonItem* ComputerListEdit;
@property (nonatomic, retain) UIAlertView* TextAlert;


- (IBAction)done;
- (IBAction)AddComputer;
- (IBAction)EditList;
- (IBAction)CalibrateTilt;

@end


@protocol FlipsideViewControllerDelegate
- (void)flipsideViewControllerDidFinish:(FlipsideViewController *)controller;
@end

