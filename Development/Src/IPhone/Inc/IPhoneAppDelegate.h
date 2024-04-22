/*=============================================================================
	IPhoneAppDelegate.h: IPhone application class / main loop
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#import <UIKit/UIKit.h>
#import <CoreLocation/CoreLocation.h>
#import <CoreLocation/CLHeading.h>
#import <UIKit/UIAccelerometer.h>
#import <MediaPlayer/MediaPlayer.h>
#import <AVFoundation/AVFoundation.h>
#import <iAd/ADBannerView.h>
#if defined(__IPHONE_6_0)
#import <iAd/ADBannerView_Deprecated.h>
#endif
#import "IPhoneHome.h"

@class EAGLView;
@class IPhoneViewController;

@interface IPhoneAppDelegate : NSObject <UIApplicationDelegate, CLLocationManagerDelegate, 
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	UIGestureRecognizerDelegate,
#endif
	UITextFieldDelegate, UITextViewDelegate, UIAccelerometerDelegate, AVAudioPlayerDelegate, UIAlertViewDelegate, ADBannerViewDelegate> 
{
@private

	/** Global view scale for iOS platforms */
	float GlobalViewScale;

	/** Can the view autorotate when user turns the device? */
	BOOL bCanAutoRotate;

	/** How much we have faded (if it's positive, we are fading in, if it's negative we are fading out) */
	float FadeAlpha;

	/** Time last fade update happened (ie DeltaSeconds) */
	double LastFadeTime;
}

/** Scalar applied to the GL view (for iPhone 4 double-res screen, etc) */
@property (assign) float GlobalViewScale;

/** Scalar for MusicPlayer volume */
@property (assign) float VolumeMultiplier;

/** TRUE if the game wants to allow the controller to autorotate when the device tilts */
@property (assign) BOOL bCanAutoRotate;

/** Song that was playing when we switched to another song so we can resume playing this one when we're done with the other one */
@property (copy) NSURL* PreviousSongURL;

/** The playback point, in seconds, within the timeline of sound when the previous song was interrupted */
@property (assign) NSTimeInterval PreviousSongPauseTime;

/** If we are fading out a previous song, this will be the name of the song to fade in to */
@property (copy) NSString* NextSongToPlay;

/** Version of the OS we are running on (NOT compiled with) */
@property (readonly) float OSVersion;

/** The controller to handle rotation of the view */
@property (retain) IPhoneViewController* Controller;

/** Main GL View */
@property (retain) EAGLView *GLView;

/** Main GL View initialization handshaking boolean */
@property (assign) BOOL bMainGLViewReadyToInitialize;
@property (assign) BOOL bMainGLViewInitialized;

/** Window object */
@property (retain) UIWindow* Window;

/** Secondary gl view (when using output cable or AirPlay) */
@property (retain) EAGLView *SecondaryGLView;

/** External window */
@property (retain) UIWindow* SecondaryWindow;

/** MP3 playback object */
@property (retain) AVAudioPlayer* MusicPlayer;

/** Fade timer */
@property (retain) NSTimer* FadeTimer;

/** UE3 init timer */
@property (retain) NSTimer* UE3InitCheckTimer;

/** The value of the alert response (atomically set since main thread and game thread use it */
@property (assign) int AlertResponse;

/** The view controlled by the auto-rotating controller */
@property (retain) UIView* RootView;

/** iAd banner view, if open */
@property (retain) ADBannerView* BannerView;

/** TRUE if the device is playing background music and we want to allow that */
@property (assign) BOOL bUsingBackgroundMusic;
//External link support
@property (assign) NSURL* iTunesURL;

@property (nonatomic, retain) UIAlertView*		UserInputAlert;
@property (nonatomic, retain) UITextField*		UserInputField;

/*** For Multi line input **/
@property (nonatomic, retain) UITextView*		UserInputView;
@property (nonatomic, retain) UIView*			UserView;

@property (copy) NSString*						UserInputExec;
@property (copy) NSString*						UserInputCancel;
@property (assign) int							UserInputCharacterLimit;


#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	/** Properties for managing the console */
	@property (nonatomic, retain) UIAlertView*		ConsoleAlert;
	@property (nonatomic, retain) UITextField*		ConsoleTextField;
	@property (nonatomic, retain) NSMutableArray*	ConsoleHistoryValues;
	@property (nonatomic, assign) int				ConsoleHistoryValuesIndex;
#endif

/**
 * @return the single app delegate object
 */
+ (IPhoneAppDelegate*)GetDelegate;


/**
 * Sets whether or not the view can autorotate
 */
- (void)SetRotationEnabled:(BOOL)bEnabled;

/**
 * Updates the volume of the media player
 */
- (void)UpdatePlayerVolume;

/**
 * Sets the VolumeMultiplier and adjusts the MusicPlayer's current volume
 */
- (void)ScaleMusicVolume:(NSString*)VolumeScale;

/**
 * Plays a hardware mp3 stream
 */
- (void)PlaySong:(NSString*)SongName;

/**
 * Stops the hardware mp3 stream
 */
- (void)StopSong;

/**
 * Pauses the hardware mp3 stream
 */
- (void)PauseSong;

/**
 * Resumes a paused song on the hardware mp3 stream
 */
- (void)ResumeSong;

/**
 * Resumes the previous song from the point in playback where it was paused before the current song started playing
 */
- (void)ResumePreviousSong;

/**
 * Disables looping for playing songs on the hardware mp3 stream
 */
- (void)DisableSongLooping;

/**
 * Shows an alert with up to 3 buttons. A delegate callback will later set AlertResponse property
 */
- (void)ShowAlert:(NSMutableArray*)StringArray;

/**
 * Will show an iAd on the top or bottom of screen, on top of the GL view (doesn't resize
 * the view)
 * 
 * @param bShowOnBottomOfScreen If true, the iAd will be shown at the bottom of the screen, top otherwise
 */
- (void)ShowAdBanner:(NSNumber*)bShowOnBottomOfScreen;

/**
 * Hides the iAd banner shows with ShowAdBanner. Will force close the ad if it's open
 */
- (void)HideAdBanner;

/**
 * Forces closed any displayed ad. Can lead to loss of revenue
 */
- (void)CloseAd;

/** 
* Brings up an on-screen keyboard for input.
*/
- (void)GetUserInput:(NSDictionary *)info;

/** 
* Brings up an on-screen keyboard for input in multi line dialog.
*/
- (void)GetUserInputMulti:(NSDictionary *)info;

/** 
 * Determine whether the specified text should be changed or not upon input
 */
- (BOOL)textField:(UITextField *)textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)string;

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
/**
 * Shows the console input dialog
 */
- (void)ShowConsole;
#endif

static void interruptionListener(void *inClientData, UInt32 inInterruption);

//Apple sample code http://developer.apple.com/library/ios/#qa/qa2008/qa1629.html so we can avoid opening safari when we only want itunes
// Process a LinkShare/TradeDoubler/DGM URL to something iPhone can handle
- (void)openReferralURL:(NSURL *)referralURL;

// Save the most recent URL in case multiple redirects occur
// "iTunesURL" is an NSURL property in your class declaration
- (NSURLRequest *)connection:(NSURLConnection *)connection willSendRequest:(NSURLRequest *)request redirectResponse:(NSURLResponse *)response;

// No more redirects; use the last URL saved
- (void)connectionDidFinishLoading:(NSURLConnection *)connection;

// Shows the system SMS view controller
- (void)ShowMessageController:(NSString*)InitialMessage;

// Shows the system email view controller
- (void)ShowMailController:(NSArray*)Params;

@end
