/*=============================================================================
	EAGLView.mm: IPhone window wrapper for a GL view
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/EAGLDrawable.h>
#import <CoreFoundation/CFRunLoop.h>
#import <UIKit/UIKit.h>

#import "Engine.h"
#import "EAGLView.h"
#import "IPhoneAppDelegate.h"
#import "IPhoneInput.h"
#import "ES2RHIDebug.h"
#import "UnIpDrv.h"
#import "IPhoneObjCWrapper.h"

#include <pthread.h>


@implementation EAGLView

/** Properties */
@synthesize SwapCount;
@synthesize OnScreenColorRenderBuffer;
@synthesize OnScreenColorRenderBufferMSAA;

extern UBOOL bIPhonePortraitMode;

#if USE_DETAILED_IPHONE_MEM_TRACKING
UINT IPhoneBackBufferMemSize;
#endif

/**
 * @return The Layer Class for the window
 */
+ (Class)layerClass
{
	return [CAEAGLLayer class];
}

-(BOOL)becomeFirstResponder
{
	return YES;
}

/**
 * Main initialization that is called from both versions of the constructor
 */
- (id)InternalInitialize
{
		// Get the layer
		CAEAGLLayer *EaglLayer = (CAEAGLLayer *)self.layer;
		EaglLayer.opaque = YES;
		NSMutableDictionary* Dict = [NSMutableDictionary dictionary];
		[Dict setValue:[NSNumber numberWithBool:NO] forKey:kEAGLDrawablePropertyRetainedBacking];
		[Dict setValue:kEAGLColorFormatRGBA8 forKey:kEAGLDrawablePropertyColorFormat];
		EaglLayer.drawableProperties = Dict;
		
		// Initialize a single, static OpenGL ES 2.0 context, shared by all EAGLView objects
		static EAGLContext* SharedContext = nil;
		if (SharedContext == nil)
		{
			SharedContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
		}
		Context = SharedContext;
		
		// delete this on failure
		if (!Context || ![EAGLContext setCurrentContext:Context]) 
		{
			[self release];
			return nil;
		}

		// Initialize some variables
		SwapCount = 0;

	return self;
}

/**
 * Handle initializing the window after the GL view has been loaded from the .nib file (this
 * is a Obj-C style constructor
 *
 * @param coder The nib loader object
 * 
 * @return the constructed View object
 */
- (id)initWithCoder:(NSCoder*)coder 
{
	bInitialized = NO;

	// call base class constructor 
	if ((self = [super initWithCoder:coder])) 
	{
		self = [self InternalInitialize];
	}
		
	// return ourself, the constructed object
	return self;
}

- (id)initWithFrame:(CGRect)Frame
{
	bInitialized = NO;

	if ((self = [super initWithFrame:Frame]))
	{
		self = [self InternalInitialize];
	}
	return self;
}

/**
 * Make sure that the OpenGL context is the current context for rendering (on the current thread)
 */
- (void)MakeCurrent 
{
#if WITH_ES2_RHI
	// handle null context debug mode (this basically disables OpenGL)
	if ( GThreeTouchMode == ThreeTouchMode_NullContext ) 
	{
		[EAGLContext setCurrentContext:nil];
	}
	// otherwise, set the context to be current, and bind the frame buffer set
	else
	{
		[EAGLContext setCurrentContext:Context];
	}
#endif
}

/**
 * Release the context (disabled currently on iPhone, no need to use it)
 */
- (void)UnmakeCurrent
{
#if WITH_ES2_RHI
	[EAGLContext setCurrentContext:nil];
#endif
}

/**
 * Swap the back buffer at the end of each frame
 */
- (void)SwapBuffers 
{
	// if this is running on a secondary thread, then setup an autorelease pool and flush it each frame
//	if (!IsInGameThread())
//	{
//		// @todo: find out why the EAGLView is being autoreleased each frame - ie, why is this needed?
// 		static NSAutoreleasePool* AutoreleasePool = [[NSAutoreleasePool alloc] init];
// 		[AutoreleasePool release];
// 		AutoreleasePool = [[NSAutoreleasePool alloc] init];
//	}
#if WITH_ES2_RHI
	// In the "No Swap" debug mode, we don't swap buffers
	if ( GThreeTouchMode == ThreeTouchMode_NoSwap ) 
	{
		glFlush();
	}
	// Otherwise, bind the back buffer, and present it
	else
	{
		// We may need this in the MSAA case
		GLint CurrentFramebuffer = 0;
		if( GMSAAAllowed && GMSAAEnabled )
		{
			// Get the currently bound FBO
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &CurrentFramebuffer);

			// Set up and perform the resolve (the READ is already set)
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER_APPLE, ResolveFrameBuffer);
			glResolveMultisampleFramebufferAPPLE();

			// After the resolve, we can discard the old attachments
			GLenum attachments[] = { GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT };
			glDiscardFramebufferEXT( GL_READ_FRAMEBUFFER_APPLE, 3, attachments );
		}
		else
		{
			// Discard the now-unncessary depth buffer
			GLenum attachments[] = { GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT };
			glDiscardFramebufferEXT( GL_READ_FRAMEBUFFER_APPLE, 2, attachments );
		}

		// Perform the actual present with the on-screen renderbuffer
		glBindRenderbuffer(GL_RENDERBUFFER, OnScreenColorRenderBuffer);
		[Context presentRenderbuffer:GL_RENDERBUFFER];

		if( GMSAAAllowed && GMSAAEnabled )
		{
			// Restore the DRAW framebuffer object
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER_APPLE, CurrentFramebuffer);
		}
	}
	// increment our swap counter
	SwapCount++;
#endif
}

/**
 * Create the back/depth buffers for the view
 *
 * @return success
 */
- (BOOL)CreateFramebuffer:(BOOL)bIsForOnDevice 
{
#if WITH_ES2_RHI
	if (bInitialized == NO)
	{
		// make sure this is current
		[self MakeCurrent];

		// Get any command line overrides
		UBOOL bCmdLineForceEnableMSAA = ParseParam(appCmdLine(),TEXT("IPHONEFORCEMSAA"));
		UBOOL bCmdLineForceDisableMSAA = ParseParam(appCmdLine(),TEXT("IPHONEDISABLEMSAA"));

		FLOAT OverrideScaleFactor = 0.0;
		Parse(appCmdLine(), TEXT("ScaleFactor="), OverrideScaleFactor);

		// If we want to enable MSAA, either by force or request, make sure it's on,
		// as long as we're not forcibly disabling MSAA
		if( ( bCmdLineForceDisableMSAA == FALSE ) &&
			( bCmdLineForceEnableMSAA == TRUE ||
			  GSystemSettings.bEnableMSAA ) )
		{
			debugf(TEXT("MSAA is enabled, by choice"));
			GMSAAAllowed = TRUE;
			GMSAAEnabled = TRUE;
		}
		else
		{
			debugf(TEXT("MSAA is disabled, by choice"));
			GMSAAAllowed = FALSE;
			GMSAAEnabled = FALSE;
		}

		if ( GSystemSettings.bAllowDynamicShadows && GSystemSettings.bMobileModShadows && GMSAAAllowed )
		{
			debugf(TEXT("Disabling MSAA because projected mod shadows are enabled."));
			GMSAAAllowed = FALSE;
			GMSAAEnabled = FALSE;
		}

		// for TV screens, always use scale factor of 1
		CGFloat RequestedContentScaleFactor = OverrideScaleFactor ? OverrideScaleFactor : GSystemSettings.MobileContentScaleFactor;
		self.contentScaleFactor = bIsForOnDevice ? RequestedContentScaleFactor : 1.0f;
		debugf(TEXT("Setting contentScaleFactor to %0.4f (optimal = %0.4f)"), self.contentScaleFactor, [[UIScreen mainScreen] scale] );

		// Create our standard displayable surface
		glGenRenderbuffers(1, &OnScreenColorRenderBuffer);
		glBindRenderbuffer(GL_RENDERBUFFER, OnScreenColorRenderBuffer);
		[Context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer*)self.layer];

		// Get the size of the surface
		GLint OnScreenWidth, OnScreenHeight;
		glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &OnScreenWidth);
		glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &OnScreenHeight);

		// NOTE: This resolve FBO is necessary even if we don't plan on using MSAA because otherwise
		// the shaders will not warm properly. Future investigation as to why; it seems unnecessary.

		// Create an FBO used to target the resolve surface
		glGenFramebuffers(1, &ResolveFrameBuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, ResolveFrameBuffer);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, OnScreenColorRenderBuffer);

#if USE_DETAILED_IPHONE_MEM_TRACKING
		//This value is used to allow the engine to track gl allocated memory see GetIPhoneOpenGLBackBufferSize
		UINT singleBufferSize = OnScreenWidth * OnScreenHeight *  4/*rgba8*/;
		IPhoneBackBufferMemSize = singleBufferSize *3/*iphone back buffer system is tripple buffered*/;
#endif

		if( GMSAAAllowed )
		{
			// Create the MSAA surface to use for rendering and another FBO for it
			glGenRenderbuffers(1, &OnScreenColorRenderBufferMSAA);
			glBindRenderbuffer(GL_RENDERBUFFER, OnScreenColorRenderBufferMSAA);
			glRenderbufferStorageMultisampleAPPLE(GL_RENDERBUFFER, 4, GL_RGBA8_OES, OnScreenWidth, OnScreenHeight);
#if USE_DETAILED_IPHONE_MEM_TRACKING
			//For AA  one buffer is needs the extra pixels but not all buffers
			IPhoneBackBufferMemSize += singleBufferSize * 2;
#endif
		}

#if USE_DETAILED_IPHONE_MEM_TRACKING
		debugf(TEXT("IPhone Back Buffer Size: %i MB"), (IPhoneBackBufferMemSize/1024)/1024.f);
#endif

		
		// Set the global size (on the first view made)
		if (GScreenWidth == 0)
		{
			GScreenWidth = OnScreenWidth;
			GScreenHeight = OnScreenHeight;
		}

		bInitialized = YES;
	}    
#endif
	return YES;
}

/** 
 * Tear down the frame buffers
 */
- (void)DestroyFramebuffer 
{
#if WITH_ES2_RHI
	if(bInitialized)
	{
		if(ResolveFrameBuffer)
		{
			glDeleteFramebuffers(1, &ResolveFrameBuffer);
		}
		if(OnScreenColorRenderBuffer)
		{
			glDeleteRenderbuffers(1, &OnScreenColorRenderBuffer);
			OnScreenColorRenderBuffer = 0;
		}
		if( GMSAAAllowed )
		{
			if(OnScreenColorRenderBufferMSAA)
			{
				glDeleteRenderbuffers(1, &OnScreenColorRenderBufferMSAA);
				OnScreenColorRenderBufferMSAA = 0;
			}
		}

		bInitialized = NO;
	}
#endif
}

/**
 * Destructor
 */
- (void)dealloc 
{
	[self DestroyFramebuffer];
	[super dealloc];
}

/**
 * Pass iPhone touch events to the UE3-side FIPhoneInputManager
 *
 * @param View The view the event happened in
 * @param Touches Set of touch events from the OS
 */
void HandleTouches(UIView* View, NSSet* Touches )
{
	// ignore touches until game is booted
	EAGLView* GLView = static_cast<EAGLView*>( View );
		
	INT Count = 0;
	TArray<FIPhoneTouchEvent> TouchesArray;
	for (UITouch* Touch in Touches)
	{ 
		// count active touches
		if ( Touch.phase != UITouchPhaseEnded && Touch.phase != UITouchPhaseCancelled ) 
		{
			Count++;
		}
		
		// get info from the touch
		CGPoint Loc = [Touch locationInView:View];

		// Apply any hardware UI scaling that may be active to support high res displays
		const float GlobalViewScale = ([IPhoneAppDelegate GetDelegate].GlobalViewScale);
		Loc.x *= GlobalViewScale;
		Loc.y *= GlobalViewScale;
		
		// make a new UE3 touch event struct
		INT Handle = INT(Touch);
/*
		if (Touch.phase == UITouchPhaseBegan)      debugf(TEXT("HandleTouches() is Adding an Event with Handle %i [TOUCH]"),Handle);
		if (Touch.phase == UITouchPhaseMoved)      debugf(TEXT("HandleTouches() is Adding an Event with Handle %i [MOVED]"),Handle);
		if (Touch.phase == UITouchPhaseStationary) debugf(TEXT("HandleTouches() is Adding an Event with Handle %i [STAT]"),Handle);
		if (Touch.phase == UITouchPhaseEnded)      debugf(TEXT("HandleTouches() is Adding an Event with Handle %i [ENDED]"),Handle);
		if (Touch.phase == UITouchPhaseCancelled)  debugf(TEXT("HandleTouches() is Adding an Event with Handle %i [CANCEL]"),Handle);
*/
		
		new(TouchesArray) FIPhoneTouchEvent( Handle, FIntPoint(Loc.x, Loc.y), (ETouchType)Touch.phase, (DOUBLE)Touch.timestamp);
	}

#if ENABLE_THREE_TOUCH_MODES
	// if there are 3 active touches, then toggle the debug mode
	if ( Count == 3 )
	{
		GThreeTouchMode = (EThreeTouchMode)((GThreeTouchMode + 1 ) % ThreeTouchMode_Max );
		const TCHAR *Modes[] = { TEXT("None"), TEXT("NullContext"), TEXT("TinyViewport"), TEXT("SingleTriangle"), TEXT("NoSwap") }; 
		debugf(TEXT("ThreeTouchMode %s"), Modes[ GThreeTouchMode ] );
	}
#endif
	
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	// If there are 4 active touches, bring up the console
	if( Count == 4 )
	{
		// Route the command to the main iPhone thread (all UI must go to the main thread)
		[[IPhoneAppDelegate GetDelegate] performSelectorOnMainThread:@selector(ShowConsole) withObject:nil waitUntilDone:NO];
		for (INT i = 0; i < TouchesArray.Num(); i++)
		{
			TouchesArray(i).Type = Touch_Cancelled;
		}
	}
#endif

	GIPhoneInputManager.AddTouchEvents(TouchesArray);
}


/**
 * Handle the various touch types from the OS
 *
 * @param touches Array of touch events
 * @param event Event information
 */
- (void) touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event 
{
	HandleTouches(self, touches);
}

- (void) touchesMoved:(NSSet*)touches withEvent:(UIEvent*)event
{
	HandleTouches(self, touches);
}

- (void) touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event
{
	HandleTouches(self, touches);
}

- (void) touchesCancelled:(NSSet*)touches withEvent:(UIEvent*)event
{
	HandleTouches(self, touches);
}

@end





@implementation IPhoneViewController

/**
 * The ViewController was created, so now we need to create our view to be controlled (an EAGLView)
 */
- (void) loadView
{
	// get the landcape size of the screen
	CGRect Frame = [[UIScreen mainScreen] bounds];
	if (!bIPhonePortraitMode)
	{
		Swap<float>(Frame.size.width, Frame.size.height);
	}

	self.view = [[UIView alloc] initWithFrame:Frame];

	// settings copied from InterfaceBuilder
	self.wantsFullScreenLayout = YES;
	self.view.clearsContextBeforeDrawing = NO;
	self.view.multipleTouchEnabled = NO;
}

/**
 * View was unloaded from us
 */ 
- (void) viewDidUnload
{
	// make sure the app delegate doesn't have apointer to the view anymore
	if ([IPhoneAppDelegate GetDelegate].GLView == self.view)
	{
		[IPhoneAppDelegate GetDelegate].GLView = nil;
	}
}

/**
 * Tell the OS that our view controller can auto-rotate between the two landscape modes
 */
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
	BOOL bIsValidOrientation;

	// will the app let us rotate?
	BOOL bCanRotate = [IPhoneAppDelegate GetDelegate].bCanAutoRotate;
	
	// is it one of the valid orientations to rotate to?
	bIsValidOrientation = bCanRotate && 
		(((interfaceOrientation == UIInterfaceOrientationLandscapeLeft) && !bIPhonePortraitMode) || 
		((interfaceOrientation == UIInterfaceOrientationLandscapeRight) && !bIPhonePortraitMode) || 
		((interfaceOrientation == UIInterfaceOrientationPortrait) && bIPhonePortraitMode) ||
		((interfaceOrientation == UIInterfaceOrientationPortraitUpsideDown) && bIPhonePortraitMode));
		
	return bIsValidOrientation;	
}

#if defined(__IPHONE_6_0)
- (NSUInteger)supportedInterfaceOrientationsForWindow:(UIWindow *)window
{
	return bIPhonePortraitMode ? UIInterfaceOrientationMaskPortrait | UIInterfaceOrientationMaskPortraitUpsideDown : UIInterfaceOrientationMaskLandscape;
}

- (NSUInteger)supportedInterfaceOrientations {
	return bIPhonePortraitMode ? UIInterfaceOrientationMaskPortrait | UIInterfaceOrientationMaskPortraitUpsideDown : UIInterfaceOrientationMaskLandscape;
}

- (BOOL)shouldAutorotate
{
	return [IPhoneAppDelegate GetDelegate].bCanAutoRotate;	
}
#endif

- (void)mailComposeController:(MFMailComposeViewController*)controller 
			didFinishWithResult:(MFMailComposeResult)result error:(NSError*)error 
{
	switch (result)
	{
		case MFMailComposeResultCancelled:
			NSLog(@"Result: canceled");
			break;
		case MFMailComposeResultSaved:
			NSLog(@"Result: saved");
			break;
		case MFMailComposeResultSent:
			NSLog(@"Result: sent");
			break;
		case MFMailComposeResultFailed:
			NSLog(@"Result: failed");
			break;
		default:
			NSLog(@"Result: not sent");
			break;
	}

	//TODO, hook this into the PlatformInterfaceBase delegate system... works fine without it now though
	[self dismissModalViewControllerAnimated:YES];
}

- (void)messageComposeViewController:(MFMessageComposeViewController *)controller 
			didFinishWithResult:(MessageComposeResult)result 
{
	switch (result) {
		case MessageComposeResultCancelled:
			NSLog(@"Cancelled");
			break;
		case MessageComposeResultFailed:
			NSLog(@"FAILED");
			break;
		case MessageComposeResultSent:
			NSLog(@"Sent");	 
			break;
		default:
			break;
	}
	
	//TODO, hook this into the PlatformInterfaceBase delegate system... works fine without it now though
	[self dismissModalViewControllerAnimated:YES];
}

@end

