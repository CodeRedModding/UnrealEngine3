/*=============================================================================
	EAGLView.h: IPhone window wrapper for a GL view
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/


#import <UIKit/UIKit.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>
#import <GameKit/GameKit.h>
#import <MessageUI/MessageUI.h>
#import <MessageUI/MFMailComposeViewController.h>

/**
 * This class wraps the CAEAGLLayer from CoreAnimation into a convenient UIView subclass.
 * The view content is basically an EAGL surface you render your OpenGL scene into.
 * Note that setting the view non-opaque will only work if the EAGL surface has an alpha channel.
 */
@interface EAGLView : UIView <GKPeerPickerControllerDelegate, GKSessionDelegate>
{
@public
	BOOL bInitialized;

@private
	/** Count of how many times */
	GLuint SwapCount;

	/** Back buffer render buffer names */
	GLuint OnScreenColorRenderBuffer;
	GLuint OnScreenColorRenderBufferMSAA;
    
	/** OpenGL context for the view */
    EAGLContext *Context;

	/** The internal MSAA FBO used to resolve the color buffer at present-time */
	GLuint ResolveFrameBuffer;
}

@property (nonatomic) GLuint SwapCount;
@property (nonatomic) GLuint OnScreenColorRenderBuffer;
@property (nonatomic) GLuint OnScreenColorRenderBufferMSAA;

- (BOOL)CreateFramebuffer:(BOOL)bIsForOnDevice;
- (void)DestroyFramebuffer;

- (void)MakeCurrent;
- (void)UnmakeCurrent;
- (void)SwapBuffers;

@end


/**
 * A view controller subclass that handles loading our GL view as well as autorotation
 */
@interface IPhoneViewController : UIViewController <MFMailComposeViewControllerDelegate, MFMessageComposeViewControllerDelegate>
{
}
@end


/**
 * A simple UIImageView that responds to touches
 */
@interface StartButtonView : UIImageView
{
}
@end