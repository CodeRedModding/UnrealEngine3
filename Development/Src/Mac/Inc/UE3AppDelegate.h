/*=============================================================================
	UE3AppDelegate.h: Mac application class and main loop.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#import <Cocoa/Cocoa.h>

@interface UE3AppDelegate : NSObject <NSApplicationDelegate>
{
	IBOutlet NSWindow *EULAWindow;
	IBOutlet NSTextView *EULAView;
	AuthorizationRef Authorization;
}

+ (void)DummyThreadRoutine:(id)AnObject;
- (IBAction)toggleFullScreen:(id)Sender;
- (BOOL)ShowEULA:(const char *)RTFPath;
- (IBAction)OnEULAAccepted:(id)Sender;
- (IBAction)OnEULADeclined:(id)Sender;
- (void)CheckInstallationFolder;
- (BOOL)MoveAppBundle:(NSString *)BundlePath ToAppsFolder:(NSString *)DestPath;
- (BOOL)AuthorizeExtendedPrivileges;

@end
