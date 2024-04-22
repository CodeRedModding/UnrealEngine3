/*=============================================================================
	FullscreenMovie.cpp: Fullscreen movie playback implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "FullScreenMovieIPhone.h"
#include "UnSubtitleManager.h"
#include "SubtitleStorage.h"
#import <AudioToolbox/AudioToolbox.h>
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import "IPhoneAppDelegate.h"
#import "IPhoneObjCWrapper.h"
#include "IPhoneInput.h"

extern UBOOL bIPhonePortraitMode;

static NSAutoreleasePool* GAutoreleasePool = NULL;

/**
 * Strip off the ".mp4" or "m4v" extension from filename.
 * e.g. GetFilenameWithoutMP4Extension("File.mp4/m4v") -> "File"
 *      GetFilenameWithoutMP4Extension("File.Name") -> "File.Name"
 *
 * @param InPath - full filename
 * @param bRemovePath - remove path from final returned string
 *
 */
static FString GetFilenameWithoutMovieExtension( const FString & InPath, bool bRemovePath)
{
	if (InPath.EndsWith(TEXT(".mp4")) || InPath.EndsWith(TEXT(".m4v")))
	{
		return FFilename(InPath).GetBaseFilename(bRemovePath);
	}

	return InPath;
}

/**
 * Create a string that has playback options prepended to it
 *
 * @param MovieName Name of the movie on disk
 * @param bIsLooping
 *
 * @param String with options prepended
 */
NSString* MakeMovieNameWithOptions(NSString* MovieName, UBOOL bIsLooping)
{
	NSString* Option = bIsLooping ? @"l," : @"s,";
	return [Option stringByAppendingString:MovieName];
}

NSString* GetPathForResource(NSString* resourceName, NSString* extension, UBOOL* bLoadedNonDeviceAspectResource=nil)
{
	EIOSDevice DeviceType = IPhoneGetDeviceType();

	NSString* ResourcePath = nil;
	switch (DeviceType)
	{
		case IOS_IPhone3GS:
		case IOS_IPhone4:
		case IOS_IPhone4S:
		case IOS_IPodTouch4:
			ResourcePath = [[NSBundle mainBundle] pathForResource:resourceName ofType:extension];
			break;
		case IOS_IPhone5:
		case IOS_IPodTouch5:
			ResourcePath = [[NSBundle mainBundle] pathForResource:[NSString stringWithFormat:@"%@-568h@2x", resourceName] ofType:extension];
			break;
		case IOS_IPad:
		case IOS_IPad2:
		case IOS_IPad3:
			ResourcePath = [[NSBundle mainBundle] pathForResource:[NSString stringWithFormat:@"IPad%@", resourceName] ofType:extension];
			break;
	};

	// Default back to a resource that isn't specific to the device
	if (ResourcePath == nil)
	{
		if (bLoadedNonDeviceAspectResource != nil)
			*bLoadedNonDeviceAspectResource = true;
		ResourcePath = [[NSBundle mainBundle] pathForResource:resourceName ofType:extension];
	}

	return ResourcePath;
}

/**
 * Notification when user changes audio-routing (plugs in a headset, etc)
 */
void AudioSessionPropListener(void* inUserData, AudioSessionPropertyID inPropertyID, UInt32 inPropertyValueSize, const void* inPropertyValue);

/************************************************
 * FMovieTouchOverlay (interface, implementation below)
 ***********************************************/


 /**
 * Simple view, used just so that we can process touches while showing to an external display
 */
@interface FMovieTouchOverlay : UIView 
{
}

@end


/************************************************
 * FMoviePlayerView
 ***********************************************/

/**
 * This is the view in the view hierarchy that the AVPlayer will push it's content into
 */
@interface FMoviePlayerView : UIView

- (void)SetPlayer:(AVPlayer*)player;

@end

@implementation FMoviePlayerView

/**
 * This view wraps the AVPlayerLayer layer type
 */
+ (Class)layerClass
{
    return [AVPlayerLayer class];
}

/**
 * Destructor
 */
- (void)dealloc
{
    AVPlayerLayer* PlayerLayer = (AVPlayerLayer*)[self layer];
	[PlayerLayer setPlayer:nil];

	[super dealloc];
}

- (void)SetPlayer:(AVPlayer*)Player
{
	// push the player to the layer
    AVPlayerLayer* PlayerLayer = (AVPlayerLayer*)[self layer];
	[PlayerLayer setPlayer:Player];
	PlayerLayer.videoGravity = AVLayerVideoGravityResizeAspect;
}
 
@end


/************************************************
 * FMovieHelper
 ***********************************************/

// context objects, value is unimportant, just needed to separate from any other context
static void *AVPlayerObservationContext = &AVPlayerObservationContext;

/**
 * Objective C helper class to play movies on the main thread, and communicate with game thread as needed
 */
@interface FMovieHelper : NSObject
{
	/** Where are we in the starutp movie progression */
	INT CurrentStartupMovie;

	/** TRUE if the current movie is a looping movie (in which case we ignore the paused playback state) */
	UBOOL bIsPlayingLoopingMovie;

	/** If TRUE, the game thread has requested to stop the startup movies, but the required ones aren't done yet */
	UBOOL bIsWaitingForEndOfRequiredMovies;

	/** If TRUE, a UIActivityIndicatorView (spinner) will be drawn on top of static images */
	UBOOL bShouldDisplaySpinnerOnImages;

	/** If no splash time was set for a particular splash, leave it up for this long (unless it's looping) */
	FLOAT DefaultSplashTime;



	NSMutableArray* StartupMovies;
	NSMutableSet* SkippableMovies;
	AVPlayer* Player;
	AVPlayerItem* PlayerItem;
	FMoviePlayerView* PlayerView;
	NSMutableArray* TextOverlays;
	FMovieTouchOverlay* TouchOverlay;
	@public FSubtitleStorage* SubtitleStorage;
	NSString* CachedMovieName;
	UIImageView* SplashView;
	UIImageView* ClearBackgroundView;
	NSTimer* SplashTimer;
	NSMutableDictionary* SplashTimeMap;
	UIActivityIndicatorView* Spinner;
	UBOOL bIsPlaying;
	UBOOL bHasRequestedPlay;
	INT TicksUntilStop;
	@public DOUBLE MovieElapsedTime;
}

/** Constructor */
- (id)init;

/**
 * Play a movie on main thread 
 * 
 * @param MovieNameWithOptions Name of a movie, no path or extension, with options prepended (l: for looping, s: for single playback)
 */
- (void)PlayMovie:(NSString*)MovieNameWithOptions;

/**
 * Stop any playing movie
 *
 * @param bForceStop If TRUE, this will stop a movie even if it's not marked skippable
 */
- (void)StopMovie:(NSNumber*)bForceStop;

/**
 * Kickoff the startup movie sequence 
 */
- (void)InitiateStartupSequence;

/**
 * Notification when movie finishes playing
 */
- (void)OnMovieFinished:(NSNotification *)Notification;

/**
 * Remove all the labels on the movie
 */
- (void)ClearTextOverlays;

/**
 * Add a label to the movie
 */
- (void)AddTextOverlay:(NSArray*)Params;

/**
 * Add a label to the movie, removing all previous labels in the process
 */
- (void)AddTextSingleOverlay:(NSArray*)Params;

/**
 * Timer function to close the current splash
 */
- (void)OnSplashTimer;

/**
 * Called when a movie ends, or when a splashes time us up, to move to next one in sequence
 */
- (void)OnMovieStopped;



/** List of startup movies (NSString*) */
@property (nonatomic, retain) NSMutableArray* StartupMovies;

/** List of movies that can be skipped (NSString*) */
@property (nonatomic, retain) NSMutableSet* SkippableMovies;

/** The current AVPlayer object */
@property (retain) AVPlayer* Player;

/** The item the AVPlayer is playing */
@property (retain) AVPlayerItem* PlayerItem;

/** The view to play the movie on */
@property (retain) FMoviePlayerView* PlayerView;

/** The set of text labels that we are drawing on top of the movie */
@property (retain) NSMutableArray* TextOverlays;

/** View on top of the movie player to handle touches */
@property (retain) FMovieTouchOverlay* TouchOverlay;

/** Total elapsed time for current movie */
@property DOUBLE MovieElapsedTime;

/** The name of the movie most recently played (may have already finished) */
@property (retain) NSString* CachedMovieName;

/** Image view for the current splash image */
@property (retain) UIImageView* SplashView;

/** Image view for the black border image, if any */
@property (retain) UIImageView* ClearBackgroundView;

/** Timer used to end a timed splash image */
@property (retain) NSTimer* SplashTimer;

/** A mapping of a splash "movie" name to how long to display it */
@property (retain) NSMutableDictionary* SplashTimeMap;

/** A spinner view */
@property (retain) UIActivityIndicatorView* Spinner;

/** TRUE when a movie is actively playing */
@property UBOOL bIsPlaying;

/** TRUE when game thread has requested a movie, but it hasn't started yet */
@property UBOOL bHasRequestedPlay;

/** Number of ticks remaining until the movie will be stopped during it's Tick() update */
@property INT TicksUntilStop;

@end

@implementation FMovieHelper

@synthesize StartupMovies;
@synthesize SkippableMovies;
@synthesize PlayerView;
@synthesize Player;
@synthesize PlayerItem;
@synthesize TextOverlays;
@synthesize TouchOverlay;
@synthesize MovieElapsedTime;
@synthesize CachedMovieName;
@synthesize SplashTimer;
@synthesize SplashView;
@synthesize ClearBackgroundView;
@synthesize SplashTimeMap;
@synthesize Spinner;
@synthesize bIsPlaying;
@synthesize bHasRequestedPlay;
@synthesize TicksUntilStop;

/**
 * Constructor
 */
- (id)init
{
	self = [super init];

	if (self)
	{
		self.StartupMovies = [NSMutableArray arrayWithCapacity:2];
		self.SkippableMovies = [NSMutableArray arrayWithCapacity:2];
		self.TextOverlays = [NSMutableArray arrayWithCapacity:2];
		self.SplashTimeMap = [NSMutableDictionary dictionaryWithCapacity:2];

		SubtitleStorage = new FSubtitleStorage();

		// default to 3 seconds for the startup splash
		DefaultSplashTime = 3.0f;

		// to start playing a movie early, we always attempt to play a movie called Startup
		CurrentStartupMovie = 0;

		// @todo ib2merge: Chair had this logic:
#if 0
		if (IPhoneGetNumCores() >= 2)
		{
			// Play a quick version for newer devices (ones with multi-cores load faster)
			[self.StartupMovies addObject:@"Startup"];
			[self PlayMovie:MakeMovieNameWithOptions(@"Startup", FALSE)];
		}
		else
		{
			// Play a long version for slow devices
			[self.StartupMovies addObject:@"Startup_Long"];
			[self PlayMovie:MakeMovieNameWithOptions(@"Startup_Long", FALSE)];
		}
#else
		[self.StartupMovies addObject:@"Startup"];
		[self PlayMovie:MakeMovieNameWithOptions(@"Startup", FALSE)];
#endif


		// listen for audio route changes, so we can keep the movie from stopping when you unplig a headset
		AudioSessionAddPropertyListener(kAudioSessionProperty_AudioRouteChange, AudioSessionPropListener, self);
	}

	return self;
}


 
- (void)observeValueForKeyPath:(NSString*) path 
            ofObject:(id)object 
            change:(NSDictionary*)change 
            context:(void*)context
{
    if (context == AVPlayerObservationContext)
    {
        AVPlayerStatus Status = [[change objectForKey:NSKeyValueChangeNewKey] integerValue];
        switch (Status)
        {
            case AVPlayerStatusReadyToPlay:
				{
					INT InsertIndex = 1;
					if (self.SplashView != nil)
						InsertIndex++;
					if (self.ClearBackgroundView != nil)
						InsertIndex++;
					// we are ready to play, so put this view into the view hierarchy (on top of temp transition image if it was there)
					[[IPhoneAppDelegate GetDelegate].RootView insertSubview:self.PlayerView atIndex:InsertIndex];

					// now that it's ready, start it up!
					[self.Player play];
				}
				break;
			default:
				NSLog(@"Unknown movie state %d\n", Status);
				break;
        }
    }
    else
    {
        [super observeValueForKeyPath:path ofObject:object change:change context:context];
    }
}

/**
 * Play a movie on main thread 
 * 
 * @param MovieNameWithOptions Name of a movie, no path or extension, with options prepended (l: for looping, s: for single playback)
 */
- (void)PlayMovie:(NSString*)MovieNameWithOptions
{
	IPhoneAppDelegate* AppDelegate = [IPhoneAppDelegate GetDelegate];

	// get the options
	bIsPlayingLoopingMovie = [MovieNameWithOptions characterAtIndex:0] == 'l';

	// skip over the options
	NSString* MovieName = [MovieNameWithOptions substringFromIndex:2];

	// Now look to see if there's a movie with this name
	UBOOL bDifferentAspectRatio = FALSE;
	NSString* MoviePath = GetPathForResource(MovieName, @"m4v", &bDifferentAspectRatio);

	// If we are on iPad, but did *not* find an iPad movie (or iPhone5 and trying to use normal aspect ratio movie)
	// then make sure we clear the display before showing it to cover anything in the borders.
	UBOOL bClearDisplay = bDifferentAspectRatio;

	// track if a movie succeeded in playing
	UBOOL bMovieWasStarted = FALSE;

	// cache the rect to use
	CGRect Frame = AppDelegate.RootView.bounds;

	UBOOL bDisableGameRenderingFullScreenMovie = FALSE;
	if (GConfig != NULL)
	{
		GConfig->GetBool(TEXT("SystemSettingsIPhone"), TEXT("bDisableGameRenderingFullScreenMovie"), bDisableGameRenderingFullScreenMovie, GSystemSettingsIni);
	}

	UBOOL bIsStartupMovie = [MovieName compare:@"Startup"] == NSOrderedSame;

	// load subtitle text file
	FString NativeMovieName = [MovieName UTF8String];
	FString NativeMoviePath = [MoviePath UTF8String];
	UBOOL bRemovePath = FALSE;
	FString BaseMovieName = GetFilenameWithoutMovieExtension(NativeMoviePath, bRemovePath);
	FString SubtitlePath = BaseMovieName + TEXT(".txt");
	if (!bIsStartupMovie)
	{
		SubtitleStorage->Load(SubtitlePath);
	}
	MovieElapsedTime = 0.0;

	// if it exists, use that on top of the splash instead of a still image with spinner
	if (MoviePath != nil)
	{
		// setup the subtitles for this movie
		if (!bIsStartupMovie)
		{
			SubtitleStorage->ActivateMovie(BaseMovieName);
		}

		// convert it to a URL (with OS4 weak reference workaround to the class)
		Class NSURLClass = NSClassFromString(@"NSURL");
		NSURL* URL = [NSURLClass fileURLWithPath:MoviePath];

		// Always use AVPlayer
		{
			// make AVPlayer asset and item from the url
			AVURLAsset* Asset = [AVURLAsset URLAssetWithURL:URL options:nil];
			self.PlayerItem = [AVPlayerItem playerItemWithAsset:Asset];


		    // observe on status to find when it's ready to play
			[self.PlayerItem addObserver:self 
							  forKeyPath:@"status" 
								 options:NSKeyValueObservingOptionInitial | NSKeyValueObservingOptionNew
								 context:AVPlayerObservationContext];
    
			// get a notification when then movie ends to we can move to the next one
			[[NSNotificationCenter defaultCenter] addObserver:self
													 selector:@selector(OnMovieFinished:)
														 name:AVPlayerItemDidPlayToEndTimeNotification
													   object:self.PlayerItem];


			// create a view, we can put it into the hierarchy later, when we are ready to play
			self.PlayerView = [[FMoviePlayerView alloc] initWithFrame:Frame];
			[self.PlayerView release];

	        // tell the view about the player for this item
			self.Player = [AVPlayer playerWithPlayerItem:self.PlayerItem];
	        [self.PlayerView SetPlayer:self.Player];
			if ( bIsPlayingLoopingMovie )
			{
				self.Player.actionAtItemEnd = AVPlayerActionAtItemEndNone;
			}
		}

		UIImage* ImageBehindMovie = nil;
		INT InsertIndex = 1;

		// Add a pure black background to keep the screen cleared underneath movies that are not the same aspect ratio as the screen (black bars that cover any rendering artifacts during loads)
		if (bClearDisplay)
		{
			ImageBehindMovie = [UIImage imageNamed:@"BlackSquare.png"];

			UIImageView* ImageView = [[UIImageView alloc] initWithImage: ImageBehindMovie];
			ImageView.contentMode = UIViewContentModeScaleToFill;
			ImageView.frame = Frame;

			// add the view on top of GL (but underneath any added text labels or splash image)
			[AppDelegate.RootView insertSubview:ImageView atIndex:InsertIndex++];

			// hold on to the splash view so we can remove it at the end of the movie
			self.ClearBackgroundView = ImageView;
			ImageBehindMovie = nil;
			[ImageView release];
		}

		// for the initial movie, we want to keep the Default image shown, and the only way is to draw
		// something before the movie gets itself going, so 
		UBOOL bUsedDefault = FALSE;
		if (bIsStartupMovie)
		{
			// re-display the Default.png, since the OS is about to clear the screen to black
			// iPad can have various default images to pick from
			if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad)
			{
				// find the properly rotated image
				if (bIPhonePortraitMode)
				{
					ImageBehindMovie = [UIImage imageNamed:@"Default-Portrait.png"];
				}
				else
				{
					ImageBehindMovie = [UIImage imageNamed:@"Default-Landscape.png"];
				}
			}

			// non-iPad just has Default (Default@2x.png will be found automatically)
			if (!ImageBehindMovie)
			{
				if( [UIScreen mainScreen].bounds.size.height == 568.0f )
				{
					ImageBehindMovie = [UIImage imageNamed:@"Default-568h@2x.png"];
				}
				else
				{
					ImageBehindMovie = [UIImage imageNamed:@"Default.png"];
				}
				bUsedDefault = TRUE;
			}
		}

		if (ImageBehindMovie)
		{
			UIImageView* ImageView = [[UIImageView alloc] initWithImage: ImageBehindMovie];
			ImageView.contentMode = UIViewContentModeScaleAspectFit;

			CGRect ImageFrame = Frame;
			// rotate if needed (for landscape apps that had to use the fake-rotated portrait Default.png)
			if (!bIPhonePortraitMode && bUsedDefault)
			{
				Swap<float>(Frame.size.width, Frame.size.height);
				ImageView.center = CGPointMake(0,0);
				ImageView.transform = CGAffineTransformRotate(ImageView.transform, -PI / 2);
			}
			ImageView.frame = ImageFrame;

			// add the view on top of GL (but underneath any added text labels)
			[AppDelegate.RootView insertSubview:ImageView atIndex:InsertIndex++];

			// hold on to the splash view so we can remove it at the end of the movie
			self.SplashView = ImageView;

			// hide this temporary splash for just a few frames, then hide it so it doesn't ever show at the end of the movie
			self.SplashTimer = [NSTimer scheduledTimerWithTimeInterval:0.5 target:self selector:@selector(OnSplashTimer) userInfo:nil repeats:NO];

			// property is now the owner
			[ImageView release];
		}

		// we are now playing a movie
		self.bIsPlaying = TRUE;

		// the request has been fulfilled
		self.bHasRequestedPlay = FALSE;

		// make sure game isn't rendered while movie is playing
		if (bDisableGameRenderingFullScreenMovie == TRUE)
		{
			FViewport::SetGameRenderingEnabled(FALSE);	
		}

		bMovieWasStarted = TRUE;
	}
	else
	{
		NSString* ImagePath = GetPathForResource(MovieName, @"png", &bDifferentAspectRatio);
		UIImage* SplashImage = [UIImage imageWithContentsOfFile:ImagePath];

		if (SplashImage != nil)
		{
			// load the image and view
			self.SplashView = [[UIImageView alloc] initWithFrame:Frame];
			self.SplashView.contentMode = UIViewContentModeScaleAspectFit;
			self.SplashView.backgroundColor = [UIColor blackColor];
			self.SplashView.image = SplashImage;

			// property is now the owner
			[self.SplashView release];

			// insert the image under any text labels
			[AppDelegate.RootView addSubview:self.SplashView];

			// show a spinning indicator
			self.Spinner = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleWhite];
			self.Spinner.frame = CGRectMake(8, Frame.size.height - 40, 32, 32);
			[AppDelegate.RootView addSubview:self.Spinner];
			[self.Spinner startAnimating];

			// property is now the owner
			[self.Spinner release];

			// if it's not a "looping" movie, then set a timer to end it after a certain time
			if (!bIsPlayingLoopingMovie)
			{
				// start with the default splash time
				FLOAT SplashTime = DefaultSplashTime;
				// override if there is an entry in the SplashTimeMap
				NSNumber* SplashTimeObj = [self.SplashTimeMap valueForKey:MovieName];
				if (SplashTimeObj)
				{
					SplashTime = [SplashTimeObj floatValue];
				}

				// hide this splash in the given amount of time
				self.SplashTimer = [NSTimer scheduledTimerWithTimeInterval:SplashTime target:self selector:@selector(OnSplashTimer) userInfo:nil repeats:NO];
			}

			// we have handled the request
			self.bIsPlaying = TRUE;
			self.bHasRequestedPlay = FALSE;

			bMovieWasStarted = TRUE;
		}
		else
		{
			// if movie fails to play, note that we handled the request
			self.bHasRequestedPlay = FALSE;
		}
	}

	if (bMovieWasStarted)
	{
		// remember the movie name
		self.CachedMovieName = MovieName;

		// add a touch overlay view
		self.TouchOverlay = [[FMovieTouchOverlay alloc] initWithFrame:Frame];
		[AppDelegate.RootView addSubview:self.TouchOverlay];

		// make sure game isn't rendered while movie is playing
		if (bDisableGameRenderingFullScreenMovie == TRUE)
		{
			FViewport::SetGameRenderingEnabled(FALSE);	
		}	
	}
	else
	{
		// go to next movie
		[self OnMovieStopped];
	}
}

/**
 * Stop any playing movie
 *
 * @param bForceStop If TRUE, this will stop a movie even if it's not marked skippable
 */
- (void)StopMovie:(NSNumber*)bForceStop
{
	// are we in a movie currently?
	if (self.bIsPlaying)
	{
		// convert object back to a bool
		UBOOL bForce = [bForceStop intValue] != 0;

		// only allow a stop if we aren't waiting for the end of the startup sequence
		// two stops will really stop
		if (!bForce &&
			// @todo ib2merge: Chair had commented out the next line
			!bIsWaitingForEndOfRequiredMovies &&
			CurrentStartupMovie != INDEX_NONE &&
			CurrentStartupMovie < ([self.StartupMovies count] - 1))
		{
			bIsWaitingForEndOfRequiredMovies = TRUE;
		}
		else
		{
			if (self.Player)
			{
				// movie is no longer looping (make sure it is cleaned up in notification)
				bIsPlayingLoopingMovie = FALSE;
				// @todo ib2merge: Chair  added this line, with this comment: when we stop, we really want to stop the whole sequence
				// CurrentStartupMovie = INDEX_NONE;

				// stop the movie (there is no [AVPlayer stop] function, so we just pretend it finished
				// and then tear it all down)
				[self OnMovieFinished:nil];
			}
			else
			{
				// call the timer function (there may not have been one, for looping mode, so we can't fire the timer
				[self.SplashTimer invalidate];
				[self OnSplashTimer];
			}
		}
	}
	else
	{
		// let game render again
		FViewport::SetGameRenderingEnabled(TRUE);
	}
}

/**
 * Attempts to skip a movie (will do nothing if movie is not skippable)
 */
- (void)SkipMovie
{
	// skip the movie if we can
	if ([SkippableMovies containsObject:self.CachedMovieName])
	{
		[self StopMovie:nil];
	}
}

/**
 * Called when a movie ends, or when a splashes time us up, to move to next one in sequence
 */
- (void)OnMovieStopped
{
	// cache number of startup movies
	INT NumStartupMovies = [self.StartupMovies count];

	// touch view no longer needed
	[self.TouchOverlay removeFromSuperview];
	self.TouchOverlay = nil;

	UBOOL bHasPlayedMovie = FALSE;

	// if we are in startup sequence, and we didn't just play the last one, go to the next one
	if (CurrentStartupMovie != INDEX_NONE && CurrentStartupMovie < NumStartupMovies - 1)
	{
		// move to next in the seqeuce
		CurrentStartupMovie++;

		// is this the last in the sequence?
		UBOOL bIsLastMovie = (CurrentStartupMovie == NumStartupMovies - 1);

		// if the game thread already requested a stop, we can skip the final (should be looping) movie
		if (bIsLastMovie && bIsWaitingForEndOfRequiredMovies)
		{
			// reset the flag
			bIsWaitingForEndOfRequiredMovies = FALSE;
		}
		else
		{
			bHasPlayedMovie = TRUE;

			NSString* MovieName = [self.StartupMovies objectAtIndex:CurrentStartupMovie];
			// choose looping for the last movie in the startup sequence
 			[self performSelectorOnMainThread:@selector(PlayMovie:) 
 								   withObject:MakeMovieNameWithOptions(MovieName, bIsLastMovie)
 								waitUntilDone:NO];
		}
	}

	// if we didn't play a movie, we need to let the engine render again
	if (!bHasPlayedMovie)
	{
		// reset startup
		CurrentStartupMovie = INDEX_NONE;

		// let game render again
		FViewport::SetGameRenderingEnabled(TRUE);

		// we are no longer playing a movie
		self.bIsPlaying = FALSE;

		// remove all the overlaysnow that the movies are over
		[self ClearTextOverlays];
	}
}

/**
 * Timer function to close the current splash
 */
- (void)OnSplashTimer
{
	// hide the a spinning indicator
	if (self.Spinner)
	{
		[self.Spinner removeFromSuperview];
		self.Spinner = nil;
	}

	// hide and release the image view
	if (self.SplashView)
	{
		[self.SplashView removeFromSuperview];
		self.SplashView = nil;
	}

	[self.SplashTimer invalidate];
	self.SplashTimer = nil;

	// move on to next movie, or end the sequence
	// unless we have a real movie playing, in which case we just want to hide the temporary splash image
	if (self.Player == nil)
	{
		[self OnMovieStopped];
	}
}

/**
 * Notification when movie finishes playing
 */
- (void)OnMovieFinished:(NSNotification *)Notification
{
	// hide and release the image view (if it exists)
	if (self.SplashView)
	{
		[self.SplashView removeFromSuperview];
		self.SplashView = nil;
	}

	// hide and release the black image view
	if (self.ClearBackgroundView)
	{
		[self.ClearBackgroundView removeFromSuperview];
		self.ClearBackgroundView = nil;
	}

	if (self.Player != nil)
	{
		if ( bIsPlayingLoopingMovie )
		{
			 [self.PlayerItem seekToTime:kCMTimeZero]; 
		}
		else
		{
			// clean up the movie player observations
			[self.PlayerItem removeObserver:self forKeyPath:@"status"];            
			[[NSNotificationCenter defaultCenter] removeObserver:self
													name:AVPlayerItemDidPlayToEndTimeNotification
													object:self.PlayerItem];
		
			// clean up the movie player objects
			[self.Player pause];
			[self.PlayerView removeFromSuperview];
			self.PlayerView = nil;
			self.Player = nil;
			self.PlayerItem = nil;

			// move on to next movie, or end the sequence
			[self OnMovieStopped];
		}
	}
}


/**
 * Kickoff the startup movie sequence 
 */
- (void)InitiateStartupSequence
{
	// now that the GConfig system has been loaded, we can read from the .inis
	FConfigSection* MovieIni = GConfig->GetSectionPrivate(TEXT("FullScreenMovie"), FALSE, TRUE, GEngineIni);
	if (MovieIni)
	{
		// get the time to use for splashes that don't have a per-splash time
		GConfig->GetFloat(TEXT("FullScreenMovie"), TEXT("DefaultSplashTime"), DefaultSplashTime, GEngineIni);

		for (FConfigSectionMap::TIterator It(*MovieIni); It; ++It)
		{
			// add to list of startup movies
			if( It.Key() == TEXT("StartupMovies") )
			{
				NSString* MovieName = [NSString stringWithUTF8String:TCHAR_TO_UTF8(*It.Value())];
				[StartupMovies addObject:MovieName];

				// in case it's an splash image, look up how long to show the splash
				FString Key = FString(TEXT("SplashTime_")) + It.Value();
				FLOAT SplashTime;
				if (GConfig->GetFloat(TEXT("FullScreenMovie"), *Key, SplashTime, GEngineIni))
				{
					[self.SplashTimeMap setValue:[NSNumber numberWithFloat:SplashTime] forKey:MovieName];
				}

			}
			// add to list of movies that are skippable
			else if( It.Key() == TEXT("SkippableMovies") )
			{
				[SkippableMovies addObject:[NSString stringWithUTF8String:TCHAR_TO_UTF8(*It.Value())]];
			}
		}
	}
	// start the first movie if we have a set to play
	INT NumStartupMovies = [self.StartupMovies count];
	if (NumStartupMovies > 0)
	{
		// if we aren't already playing the Startup movie (see init), then start playing now
		// if we are already playing Startup, then when it ends, it will just continue on with the next
		// startup movie in the sequence
		if (!self.bIsPlaying)
		{
			// start at the beginning
			CurrentStartupMovie = 0;
			NSString* MovieName = [self.StartupMovies objectAtIndex:CurrentStartupMovie];
			// choose looping for the last movie in the startup sequence
			[self PlayMovie:MakeMovieNameWithOptions(MovieName, CurrentStartupMovie == NumStartupMovies - 1)];
		}
	}
}

/**
 * Remove all the labels on the movie
 */
- (void)ClearTextOverlays
{
	// remove all the labels
	for (UILabel* Label in self.TextOverlays)
	{
		[Label removeFromSuperview];
	}

	// empty the array
	[TextOverlays removeAllObjects];
}

/**
 * Add a label to the movie
 */
- (void)AddTextOverlay:(NSArray*)Params
{
	if (!self.bIsPlaying)
	{
		// done with params
		[Params release];
		return;
	}

	// get the params (see GameThreadAddOverlay)
	NSString* Text = [Params objectAtIndex:0];
	NSNumber* Param;
	Param = [Params objectAtIndex:1]; FLOAT X = [Param floatValue];
	Param = [Params objectAtIndex:2]; FLOAT Y = [Param floatValue];
	Param = [Params objectAtIndex:3]; FLOAT Width = [Param floatValue];
	Param = [Params objectAtIndex:4]; FLOAT Height = [Param floatValue];
	Param = [Params objectAtIndex:5]; FLOAT FontSize = [Param floatValue];
	Param = [Params objectAtIndex:6]; UBOOL bIsCentered = [Param boolValue];
	Param = [Params objectAtIndex:7]; UBOOL bIsWrapped = [Param boolValue];
	// done with params
	[Params release];

	// make a font of the appropriate size
	UIFont* LabelFont = [UIFont systemFontOfSize:FontSize];

	// we need to move the label's frame up because the label will always vertically
	// center the text
	CGSize TextSize = [Text sizeWithFont:LabelFont constrainedToSize:CGSizeMake(Width, Height)];
	Y -= ((Height - TextSize.height) / 2);

	UILabel* NewLabel = [[UILabel alloc] initWithFrame:CGRectMake(X, Y, Width, Height)];
	NewLabel.opaque = NO;
#if defined(__IPHONE_6_0)
	NewLabel.textAlignment = (bIsCentered ? NSTextAlignmentCenter : NSTextAlignmentLeft);
#else
	NewLabel.textAlignment = (bIsCentered ? UITextAlignmentCenter : UITextAlignmentLeft);
#endif
	NewLabel.textColor = [UIColor whiteColor];
	NewLabel.shadowColor = [UIColor blackColor];
	NewLabel.shadowOffset = CGSizeMake(1, 1);
	NewLabel.backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.0];
	// set the text string
	NewLabel.text = Text;
	NewLabel.font = LabelFont;

	// if we want to wrap the text, 
 	if (bIsWrapped)
 	{
 		NewLabel.numberOfLines = 0;
 	}

	// add the label to the screen
	[[IPhoneAppDelegate GetDelegate].RootView addSubview:NewLabel];

	// cache the overlay
	[self.TextOverlays addObject:NewLabel];

	// the array is now the owner
	[NewLabel release];
}

/**
 * Add a label to the movie, clearing all previous labels in the process
 */
- (void)AddTextSingleOverlay:(NSArray*)Params
{
	[self ClearTextOverlays];
	[self AddTextOverlay: Params];
}

@end

/** Movie helper singleton */
FMovieHelper* GMovieHelper;

void AudioSessionPropListener(void* inUserData, AudioSessionPropertyID inPropertyID, UInt32 inPropertyValueSize, const void* inPropertyValue)
{
	// unpause the AVPlayer
	if (GMovieHelper.Player)
	{
		NSLog(@"Player rate = %f\n", GMovieHelper.Player.rate);
		[GMovieHelper.Player play];
		NSLog(@" After play, Player rate = %f\n", GMovieHelper.Player.rate);
	}
}


/************************************************
 * FMovieTouchOverlay (implementation)
 ***********************************************/

@implementation FMovieTouchOverlay

/**
 * Handle the various touch types from the OS
 *
 * @param touches Array of touch events
 * @param event Event information
 */
- (void) touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event 
{
}

- (void) touchesMoved:(NSSet*)touches withEvent:(UIEvent*)event
{
}

- (void) touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event
{
	if (GIsRunning)
	{
		// attempt to skip the movie if allowed
		[GMovieHelper SkipMovie];
	}
}

- (void) touchesCancelled:(NSSet*)touches withEvent:(UIEvent*)event
{
}

@end


/*-----------------------------------------------------------------------------
FFullScreenMovieIPhone
-----------------------------------------------------------------------------*/

/** 
* Constructor
*/
FFullScreenMovieIPhone::FFullScreenMovieIPhone(UBOOL bUseSound)
	: bEnableInputProcessing(FALSE)
{
	GMovieHelper = [[FMovieHelper alloc] init];
}

/** 
* Perform one-time initialization and create instance
*
* @param bUseSound - TRUE if sound should be enabled for movie playback
* @return new instance if successful
*/
FFullScreenMovieSupport* FFullScreenMovieIPhone::StaticInitialize(UBOOL bUseSound)
{
	static FFullScreenMovieIPhone* StaticInstance = NULL;
	if( !StaticInstance )
	{
		StaticInstance = new FFullScreenMovieIPhone(bUseSound);
	}
	return StaticInstance;
}

/**
* Pure virtual that must be overloaded by the inheriting class. It will
* be called from within UnLevTick.cpp after ticking all actors.
*
* @param DeltaTime	Game time passed since the last call.
*/
void FFullScreenMovieIPhone::Tick(FLOAT DeltaTime)
{
	if (GMovieHelper.TicksUntilStop > 0)
	{
		GMovieHelper.TicksUntilStop--;
		if (GMovieHelper.TicksUntilStop == 0)
		{
			GameThreadStopMovie(0, FALSE, TRUE);
            return;
		}
	}

	// handle subtitles
	FString Subtitle;
	GMovieHelper->MovieElapsedTime += DeltaTime;

	FString SubtitleID = GMovieHelper->SubtitleStorage->LookupSubtitle(GMovieHelper->MovieElapsedTime);
	if( SubtitleID.Len() )
	{
		Subtitle = Localize(TEXT("Subtitles"), *SubtitleID, TEXT("Subtitles"), NULL, TRUE);
		// if it wasn't found, then just use the Key
		if( Subtitle.Len() == 0 )
		{
			Subtitle = SubtitleID;
		}
	}

	if (Subtitle.Len() != 0)
	{
		FLOAT XPos = 0.0f; //unused due to centering
		FLOAT YPos = 0.9f;  //percentage of screen height
		FLOAT ScaleX = 1.0f;
		FLOAT ScaleY = 1.0f;
		UBOOL bIsCentered = TRUE;
		UBOOL bIsWrapped = TRUE;
		CGRect Frame = [[UIScreen mainScreen] bounds];
		FLOAT WrapWidth = Frame.size.width * 0.9f;
		UFont *FontToUse = NULL;
		if (GEngine)
		{
			FontToUse = GEngine->GetSubtitleFont();
		}
		GameThreadAddSingleOverlay(FontToUse, Subtitle, XPos, YPos, ScaleX, ScaleY, bIsCentered, bIsWrapped, WrapWidth);
	}

	FIPhoneInputManager::QueueType QueuedEvents;
	GIPhoneInputManager.GetAllEvents(QueuedEvents);
	if (1)
	{
		// look for a touch down event
		UBOOL bSkipMovie = FALSE;
		for (INT QueueIndex = 0; QueueIndex < QueuedEvents.Num(); QueueIndex++)
		{
			TArray<FIPhoneTouchEvent>& Events = QueuedEvents(QueueIndex);
			for (INT EventIndex = 0; EventIndex < Events.Num(); EventIndex++)
			{
				if (Events(EventIndex).Type == Touch_Began)
				{
					bSkipMovie = TRUE;
					break;
				}
			}
		}

		// attempt to skip the movie if it's able
		if (bSkipMovie)
		{
			[GMovieHelper performSelectorOnMainThread:@selector(SkipMovie) withObject:nil waitUntilDone:NO];
		}
	}
}

/**
* Pure virtual that must be overloaded by the inheriting class. It is
* used to determine whether an object is ready to be ticked. This is 
* required for example for all UObject derived classes as they might be
* loaded async and therefore won't be ready immediately.
*
* @return	TRUE if class is ready to be ticked, FALSE otherwise.
*/
UBOOL FFullScreenMovieIPhone::IsTickable() const
{
	if (!GMovieHelper)
	{
	    return FALSE;
        }

	return (GMovieHelper->bIsPlaying || (GMovieHelper.TicksUntilStop > 0));
}

/**
* Kick off a movie play from the game thread
*
* @param InMovieMode How to play the movie (usually MM_PlayOnceFromStream or MM_LoopFromMemory).
* @param MovieFilename Path of the movie to play in its entirety
* @param StartFrame Optional frame number to start on
* @param InStartOfRenderingMovieFrame When the fading in from just audio to audio and video should occur
* @param InEndOfRenderingMovieFrame When the fading from audio and video to just audio should occur
*/
void FFullScreenMovieIPhone::GameThreadPlayMovie(EMovieMode InMovieMode, const TCHAR* InMovieFilename, INT StartFrame, INT InStartOfRenderingMovieFrame, INT InEndOfRenderingMovieFrame)
{
	FFilename BaseMovieName = FFilename(InMovieFilename).GetBaseFilename();

	// stop any movie that is currently playing before starting the new one
	GameThreadStopMovie ( 0, TRUE, TRUE );

	checkf(StartFrame == 0 && InStartOfRenderingMovieFrame == -1 && InEndOfRenderingMovieFrame == -1, 
		TEXT("Delayed start video playback not supported on IPhone"));

	NSString* MovieName = [NSString stringWithUTF8String:TCHAR_TO_UTF8(*BaseMovieName)];

	// note that we ahve requested a movie to play (so IsPlaying is accurate)
	GMovieHelper.bHasRequestedPlay = TRUE;

	// remember the movie name
	GMovieHelper.CachedMovieName = MovieName;

	// pass the message to the main thread
	[GMovieHelper performSelectorOnMainThread:@selector(PlayMovie:) 
								   withObject:MakeMovieNameWithOptions(MovieName, (InMovieMode & MF_LoopPlayback))
								waitUntilDone:NO];
}

/**
* Stops the currently playing movie
*
* @param DelayInSeconds Will delay the stopping of the movie for this many seconds. If zero, this function will wait until the movie stops before returning.
* @param bWaitForMovie if TRUE then wait until the movie finish event triggers
* @param bForceStop if TRUE then non-skippable movies and startup movies are forced to stop
*/
void FFullScreenMovieIPhone::GameThreadStopMovie(FLOAT DelayInSeconds,UBOOL bWaitForMovie,UBOOL bForceStop)
{
	// pass the message to the main thread
	[GMovieHelper performSelectorOnMainThread:@selector(StopMovie:) withObject:[NSNumber numberWithBool:bForceStop] waitUntilDone:NO];

	// wait for the movie to finish if desired
	if( bWaitForMovie )
	{
		GameThreadWaitForMovie();
	}

}


/**
* Block game thread until movie is complete (must have been started
* with GameThreadPlayMovie or it may never return)
*/
void FFullScreenMovieIPhone::GameThreadWaitForMovie()
{
	while (GMovieHelper.bIsPlaying || GMovieHelper.bHasRequestedPlay)
	{
		appSleep(0.1f);
	}
}

/**
* Checks to see if the movie has finished playing. Will return immediately
*
* @param MovieFilename MovieFilename to check against (should match the name passed to GameThreadPlayMovie). Empty string will match any movie
* 
* @return TRUE if the named movie has finished playing
*/
UBOOL FFullScreenMovieIPhone::GameThreadIsMovieFinished(const TCHAR* MovieFilename)
{
	// match the moviefilename
	if (MovieFilename[0] || 
		GMovieHelper.CachedMovieName == nil ||
		appStricmp(MovieFilename, (TCHAR*)[GMovieHelper.CachedMovieName cStringUsingEncoding:NSUTF32StringEncoding]) == 0)
	{
		// return if the movie has finished playing
		return GMovieHelper.bIsPlaying == FALSE && GMovieHelper.bHasRequestedPlay == FALSE;
	}

	return FALSE;
}

/**
* Checks to see if the movie is playing. Will return immediately
*
* @param MovieFilename MovieFilename to check against (should match the name passed to GameThreadPlayMovie). Empty string will match any movie
* 
* @return TRUE if the named movie is playing
*/
UBOOL FFullScreenMovieIPhone::GameThreadIsMoviePlaying(const TCHAR* MovieFilename)
{
	if (MovieFilename[0] || 
		GMovieHelper.CachedMovieName == nil ||
		appStricmp(MovieFilename, (TCHAR*)[GMovieHelper.CachedMovieName cStringUsingEncoding:NSUTF32StringEncoding]) == 0)
	{
		// return if the movie has still playing, or a request has been entered, but hasn't started yet
		return GMovieHelper.bIsPlaying || GMovieHelper.bHasRequestedPlay;
	}

	return FALSE;
}

/**
* Get the name of the most recent movie played
*
* @return Name of the movie that was most recently played, or empty string if a movie hadn't been played
*/
FString FFullScreenMovieIPhone::GameThreadGetLastMovieName()
{
	return FString((TCHAR*)[GMovieHelper.CachedMovieName cStringUsingEncoding:NSUTF32StringEncoding]);
}

/**
* Kicks off a thread to control the startup movie sequence
*/
void FFullScreenMovieIPhone::GameThreadInitiateStartupSequence()
{
	if ( !ParseParam(appCmdLine(), TEXT("nostartupmovies")) )
	{
		[GMovieHelper performSelectorOnMainThread:@selector(InitiateStartupSequence) withObject:nil waitUntilDone:NO];
	}
}

/**
 * Tells the movie player to allow game rendering for a frame while still keeping the movie
 * playing. This will cover delays from render caching the first frame
 */
void FFullScreenMovieIPhone::GameThreadRequestDelayedStopMovie()
{
	// let game render again, but wait 2 frames before game presents
	FViewport::SetGameRenderingEnabled(TRUE, 2);
	GMovieHelper.TicksUntilStop = 2;
}

/**
 * Removes all overlays from displaying
 */
void FFullScreenMovieIPhone::GameThreadRemoveAllOverlays()
{
	[GMovieHelper performSelectorOnMainThread:@selector(ClearTextOverlays) withObject:nil waitUntilDone:NO];
}

/**
 * Adds a text overlay to the movie
 *
 * @param Font Font to use to display (must be in the root set so this will work during loads)
 * @param Text Text to display
 * @param X X location in resolution-independent coordinates (ignored if centered)
 * @param Y Y location in resolution-independent coordinates
 * @param ScaleX Text horizontal scale
 * @param ScaleY Text vertical scale
 * @param bIsCentered TRUE if the text should be centered
 * @param bIsWrapped TRUE if the text should be wrapped at WrapWidth
 * @param WrapWidth Number of pixels before text should wrap
 */
void FFullScreenMovieIPhone::GameThreadAddOverlay(UFont* Font, const FString& Text, FLOAT X, FLOAT Y, FLOAT ScaleX, FLOAT ScaleY, UBOOL bIsCentered, UBOOL bIsWrapped, FLOAT WrapWidth )
{
	// throw the parameters into an array to pass to main thread
	NSMutableArray* ParamArray = reinterpret_cast<NSMutableArray*>(FillOverlayParams(Font, Text, X, Y, ScaleX, ScaleY, bIsCentered, bIsWrapped, WrapWidth));

	// add the label on the main thread
	[GMovieHelper performSelectorOnMainThread:@selector(AddTextOverlay:) withObject:ParamArray waitUntilDone:NO];
}

/**
 * Adds a text overlay to the movie, clearing previous overlays in the process
 *
 * @param Font Font to use to display (must be in the root set so this will work during loads)
 * @param Text Text to display
 * @param X X location in resolution-independent coordinates (ignored if centered)
 * @param Y Y location in resolution-independent coordinates
 * @param ScaleX Text horizontal scale
 * @param ScaleY Text vertical scale
 * @param bIsCentered TRUE if the text should be centered
 * @param bIsWrapped TRUE if the text should be wrapped at WrapWidth
 * @param WrapWidth Number of pixels before text should wrap
 */
void FFullScreenMovieIPhone::GameThreadAddSingleOverlay(UFont* Font, const FString& Text, FLOAT X, FLOAT Y, FLOAT ScaleX, FLOAT ScaleY, UBOOL bIsCentered, UBOOL bIsWrapped, FLOAT WrapWidth )
{
	// throw the parameters into an array to pass to main thread
	NSMutableArray* ParamArray = reinterpret_cast<NSMutableArray*>(FillOverlayParams(Font, Text, X, Y, ScaleX, ScaleY, bIsCentered, bIsWrapped, WrapWidth));

	// add the label on the main thread
	[GMovieHelper performSelectorOnMainThread:@selector(AddTextSingleOverlay:) withObject:ParamArray waitUntilDone:NO];
}










/**
 * Controls whether the movie player processes input.
 *
 * @param	bShouldMovieProcessInput	whether the movie should process input.
 */
void FFullScreenMovieIPhone::GameThreadToggleInputProcessing(UBOOL bShouldMovieProcessInput)
{
	bEnableInputProcessing = bShouldMovieProcessInput;
}

/**
 * Controls whether the movie  is hidden and if input will forcibly stop the movie from playing when hidden/
 *
 * @param	bHidden	whether the movie should be hidden
 */
void FFullScreenMovieIPhone::GameThreadSetMovieHidden(UBOOL bInHidden)
{
// 	if (bInHidden)
// 	{
// 		// if it should be hidden, remove it from the view hierarchy
// 		[[self.Player view] removeFromSuperview];
// 	}
// 	else
// 	{
// 		// otherwise, put it into the view hierarchy
// 		[[IPhoneAppDelegate GetDelegate].view addSubview:[self.Player view]];
// 	}
// 
// 	FViewport::SetGameRenderingEnabled( bInHidden );	
}

/**
* Returns the current frame number of the movie (not thred synchronized in anyway, but it's okay 
* if it's a little off
*/
INT FFullScreenMovieIPhone::GameThreadGetCurrentFrame()
{
	// we can get the current time, but i can't see a way to get frame number 
	// @todo: could use time and pretend a framerate
	return 0;
}

/**
* Releases any dynamic resources. This is needed for flushing resources during device reset on d3d.
*/
void FFullScreenMovieIPhone::ReleaseDynamicResources()
{

}

/** 
* Returns an NSMutableArray* filled with appropriate params for passing to AddTextOverlay functions
*/
void* FFullScreenMovieIPhone::FillOverlayParams(UFont* Font, const FString& Text, FLOAT X, FLOAT Y, FLOAT ScaleX, FLOAT ScaleY, UBOOL bIsCentered, UBOOL bIsWrapped, FLOAT WrapWidth )
{
	// attempt to match UE3's font
	FLOAT FontSize = (Font ? Font->GetMaxCharHeight() : 12.0f) * ScaleY;
	
	CGRect Frame = [[UIScreen mainScreen] bounds];
	if (!bIPhonePortraitMode)
	{
		Swap<float>(Frame.size.width, Frame.size.height);
	}

	FLOAT ViewWidth = Frame.size.width;
	FLOAT ViewHeight = Frame.size.height;
	FLOAT LabelX = ViewWidth * (bIsCentered ? 0 : X);
	FLOAT LabelY = ViewHeight * Y;

	// we need to adjust the view up, since the text is always vertically centered in the lab

	// throw the parameters into an array to pass to main thread
	NSMutableArray* ParamArray = [[NSMutableArray alloc] initWithObjects:
	/*0*/	[NSString stringWithUTF8String:TCHAR_TO_UTF8(*Text)],
	/*1*/	[NSNumber numberWithFloat:LabelX],
	/*2*/	[NSNumber numberWithFloat:LabelY],
	/*3*/	[NSNumber numberWithFloat:(bIsWrapped ? WrapWidth : ViewWidth)],
	/*4*/	[NSNumber numberWithFloat:ViewHeight],
	/*5*/	[NSNumber numberWithFloat:FontSize],
	/*6*/	[NSNumber numberWithBool:bIsCentered],
	/*7*/	[NSNumber numberWithBool:bIsWrapped],
			nil
	];

	return reinterpret_cast<void*>(ParamArray);
}



