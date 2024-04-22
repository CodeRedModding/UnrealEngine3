/*=============================================================================
 MacObjCWrapper.mm: Mac wrapper for making ObjC calls from C++ code
 Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#import <Cocoa/Cocoa.h>
#include <pthread.h>
#include <wchar.h>
#include "MacObjCWrapper.h"
#include "UE3AppDelegate.h"

typedef uint32_t				UBOOL;		// Boolean 0 (false) or 1 (true).

extern void* GTempViewport;
int GMacOSXVersion = 0;

// Variables global to all Cocoa-handled windows in the system
static UBOOL				GWasRightClick = false;		// This remembers if mouse left button was pressed with Cmd or without it.
													// With Cmd it should be treated as right mouse button. (Apple standard)

enum ESpecialKeys
{
	SpecialKey_LeftMouseButton		= 1,
	SpecialKey_RightMouseButton		= 2,
	SpecialKey_MiddleMouseButton	= 4,
	SpecialKey_LeftShift			= 8,
	SpecialKey_RightShift			= 16,
	SpecialKey_LeftCtrl				= 32,
	SpecialKey_RightCtrl			= 64,
	SpecialKey_CapsLock				= 128,
	
};

static unsigned long	GSpecialKeysState = 0;		// This remembers current state of mouse buttons, and control keys.

unsigned char			GKeysDown[256] = { 0 };		// This remembers keys pressed by scancodes, and repeats of each. It's used to work around a bug on MacBook Pro laptops (see flagsChanged: method)

static BOOL				GCursorShown = TRUE;

@interface UOpenGLView : NSView <NSWindowDelegate>
{
@private

	NSOpenGLContext		*Context;
	NSOpenGLPixelFormat	*PixelFormat;
	void				*MacViewport;

	BOOL				FirstMouseMoveEvent;
	NSPoint				LastMousePos;

	int					SimulatedMousePosX;
	int					SimulatedMousePosY;
	int					LastVisibleCursorPosX;
	int					LastVisibleCursorPosY;
}

-(id)initWithFrame:(NSRect)FrameRect shareContext:(NSOpenGLContext*)SharedContext;
-(id) initWithFrame:(NSRect)FrameRect;
-(void)dealloc;
-(void)setMacViewport:(void*)Viewport;
-(NSOpenGLContext *)openGLContext;
-(NSOpenGLPixelFormat*) pixelFormat;

// Receivers for keyboard input
-(void)keyDown:(NSEvent*)Event;
-(void)keyUp:(NSEvent*)Event;
-(void)flagsChanged:(NSEvent*)Event;
-(BOOL)canBecomeKeyView;
-(BOOL)acceptsFirstResponder;
-(void)addKeyDownEvent:(int)Scancode withCharacter:(wchar_t)Character repeated:(bool)Repeated;
-(void)addKeyUpEvent:(int)Scancode;

// Receivers for mouse input
-(void)mouseMoved:(NSEvent*)Event;
-(void)mouseDragged:(NSEvent*)Event;
-(void)rightMouseDragged:(NSEvent*)Event;
-(void)otherMouseDragged:(NSEvent*)Event;
-(void)mouseDown:(NSEvent*)Event;
-(void)mouseUp:(NSEvent*)Event;
-(void)rightMouseDown:(NSEvent*)Event;
-(void)rightMouseUp:(NSEvent*)Event;
-(void)otherMouseDown:(NSEvent*)Event;
-(void)otherMouseUp:(NSEvent*)Event;
-(void)scrollWheel:(NSEvent*)Event;
-(void)addMouseMoveEvent:(NSEvent*)Event;

// Hide/show cursor
-(void)showCursor:(BOOL)Show;
-(BOOL)cursorShown;

@end

@implementation UOpenGLView

+ (void)initialize
{
	NSUserDefaults *StandardDefaults = [NSUserDefaults standardUserDefaults];
	NSDictionary *OverrideDefaults = [NSDictionary dictionaryWithObject:@"NO"
																 forKey:@"AppleMomentumScrollSupported"];
	[StandardDefaults registerDefaults:OverrideDefaults];
}

-(id)initWithFrame:(NSRect)FrameRect shareContext:(NSOpenGLContext*)SharedContext
{
	GCursorShown = NO;
	[NSCursor hide];
	FirstMouseMoveEvent = YES;

	LastVisibleCursorPosX = 0;
	LastVisibleCursorPosY = 0;

	NSRect ViewportFrame = [self frame];
	SimulatedMousePosX = (int)(ViewportFrame.size.width/2);
	SimulatedMousePosY = (int)(ViewportFrame.size.height/2);

	NSOpenGLPixelFormatAttribute Attribs[] =
	{
		kCGLPFAAccelerated,
		kCGLPFANoRecovery,
		kCGLPFADoubleBuffer,
		kCGLPFAColorSize, 32,
		0
	};

	PixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:Attribs];
	if (PixelFormat)
	{
		Context = [[NSOpenGLContext alloc] initWithFormat:PixelFormat shareContext:SharedContext];
	}

	if (Context && (self = [super initWithFrame:FrameRect]))
	{
		[Context makeCurrentContext];

		GLint SwapInterval = 1;
		[Context setValues:&SwapInterval forParameter:NSOpenGLCPSwapInterval];
	}

	return self;
}

-(id)initWithFrame:(NSRect)FrameRect
{
	self = [self initWithFrame:FrameRect shareContext:nil];
	return self;
}

-(void)dealloc
{
	if (Context)
	{
		[Context release];
	}

	if (PixelFormat)
	{
		[PixelFormat release];
	}

	[super dealloc];
}

-(void)setMacViewport:(void*)Viewport
{
	MacViewport = GTempViewport;
}

- (NSOpenGLContext *)openGLContext
{
	return Context;
}

-(NSOpenGLPixelFormat*) pixelFormat
{
	return PixelFormat;
}

- (void)lockFocus
{
    [super lockFocus];
    if ([Context view] != self)
	{
        [Context setView:self];
    }
    [Context makeCurrentContext];
}

-(void)keyDown:(NSEvent*)Event
{
	NSString *Chars = [Event characters];
	int Keycode = [Event keyCode];
	bool Repeated = false;
	if( Keycode < 256 )
	{
		Repeated = ( GKeysDown[Keycode] != 0 );
		++GKeysDown[Keycode];
	}

	// For those scancodes Mac gives character information that's enough unlike Windows to confuse the game. So we're suppressing them.
	static int KeysWithBadCharacters[] = {
		51,	// Backspace
		123, 124, 125, 126, // Cursor keys
		122, 120, 99, 118, 96, 97, 98, 100, 101, 109, 103, 111, 105 // F1-F13
	};

	for( int Key = 0; Key < sizeof(KeysWithBadCharacters)/sizeof(int); ++Key )
	{
		if( Keycode == KeysWithBadCharacters[Key] )
		{
			[self addKeyDownEvent: Keycode withCharacter: (wchar_t) 0xFFFF repeated: Repeated];
			return;
		}
	}

	[self addKeyDownEvent: Keycode withCharacter: (wchar_t)( ( [Chars length] ) ? [Chars characterAtIndex:0] : 0xFFFF ) repeated: Repeated];
}

-(void)keyUp:(NSEvent*)Event
{
	int Keycode = [Event keyCode];
	if( Keycode < 256 )
	{
		GKeysDown[ Keycode ] = 0;
	}
	[self addKeyUpEvent: Keycode];
}

-(void)flagsChanged:(NSEvent*)Event
{
	// In this event we have to convert change to flags to keypresses of specific keys

	bool	Pressed;				// was the key pressed, or released?
	int		SpecialKeyValue = 0;	// 0 won't influence the GSpecialKeysState variable.

	unsigned short	Keycode = [Event keyCode];
	unsigned long	Flags = [Event modifierFlags];

	switch( Keycode )
	{
	case 0:		return;	// no idea why this sometimes gets called by the system, ignore it
	case 54:	Pressed = ( ( Flags & (1<<4) ) != 0 );													break;		// Right Cmd
	case 55:	Pressed = ( ( Flags & (1<<3) ) != 0 );													break;		// Left Cmd
	case 56:	Pressed = ( ( Flags & (1<<1) ) != 0 );	SpecialKeyValue = (int)SpecialKey_LeftShift;	break;		// Left Shift
	case 57:	Pressed = ( ( Flags & (1<<16)) != 0 );	SpecialKeyValue = (int)SpecialKey_CapsLock;		break;		// Caps Lock
	case 58:	Pressed = ( ( Flags & (1<<5) ) != 0 );													break;		// Left Alt
	case 59:	Pressed = ( ( Flags & (1<<0) ) != 0 );	SpecialKeyValue = (int)SpecialKey_LeftCtrl;		break;		// Left Ctrl
	case 60:	Pressed = ( ( Flags & (1<<2) ) != 0 );	SpecialKeyValue = (int)SpecialKey_RightShift;	break;		// Right Shift
	case 61:	Pressed = ( ( Flags & (1<<6) ) != 0 );													break;		// Right Alt
	case 62:	Pressed = ( ( Flags & (1<<13) ) != 0 );	SpecialKeyValue = (int)SpecialKey_RightCtrl;	break;		// Right Ctrl

	case 63: // fn key
	{
		// this is workaround for a bug in OS X. If you press function key with fn key, but release
		// fn before function key, system won't send keyUp event for function key when you release it
		Pressed = ( ( Flags & (1<<4) ) != 0 );
		if( !Pressed )
		{
			static int FunctionKeys[] = { 122, 120, 99, 118, 96, 97, 98, 100, 101, 109, 103, 111, 105 };	// Apple scancodes for F1-F13, sorted
			for( int i = 0; i < sizeof(FunctionKeys)/sizeof(int); ++i )
			{
				if( GKeysDown[FunctionKeys[i]] )
				{
					GKeysDown[FunctionKeys[i]] = 0;
					[self addKeyUpEvent: FunctionKeys[i]];
				}
			}
		}
		return;
	}

	default:
		return;
	}

	if( Pressed )
	{
		GSpecialKeysState |= SpecialKeyValue;
		[self addKeyDownEvent: Keycode withCharacter: (wchar_t)0xFFFF repeated: NO];
	}
	else
	{
		GSpecialKeysState &= ~SpecialKeyValue;
		[self addKeyUpEvent: Keycode];
	}

}

-(BOOL)canBecomeKeyView
{
	return YES;
}

-(BOOL)acceptsFirstResponder
{
	return YES;
}

-(void)addKeyDownEvent:(int)Scancode withCharacter:(wchar_t)Character repeated:(bool)Repeated
{
	MacEvent EventData;
	EventData.EventType = MACEVENT_Key;
	EventData.Data.Key.KeyCode = Scancode;
	EventData.Data.Key.KeyPressed = 1;
	EventData.Data.Key.Character = Character;
	EventData.Data.Key.Repeat = ( Repeated ? 1 : 0 );
	AddMacEventFromNSViewToViewportQueue( MacViewport, &EventData );
}

-(void)addKeyUpEvent:(int)Scancode
{
	MacEvent EventData;
	EventData.EventType = MACEVENT_Key;
	EventData.Data.Key.KeyCode = Scancode;
	EventData.Data.Key.KeyPressed = 0;
	EventData.Data.Key.Repeat = 0;
	EventData.Data.Key.Character = 0xFFFF;	// doesn't matter when key is released

	AddMacEventFromNSViewToViewportQueue( MacViewport, &EventData );
}

-(void)mouseMoved:(NSEvent*)Event
{
	[self addMouseMoveEvent: Event];
}

-(void)mouseDragged:(NSEvent*)Event
{
	[self addMouseMoveEvent: Event];
}

-(void)rightMouseDragged:(NSEvent*)Event
{
	[self addMouseMoveEvent: Event];
}

-(void)otherMouseDragged:(NSEvent*)Event
{
	[self addMouseMoveEvent: Event];
}

-(void)mouseDown:(NSEvent*)Event
{
	[self addMouseMoveEvent: Event];
	bool CommandKeyPressed = ( [Event modifierFlags] & NSCommandKeyMask );
	if( CommandKeyPressed )
	{
		// Left click + Cmd key on Macs is right click.
		GSpecialKeysState |= (unsigned long)SpecialKey_RightMouseButton;
		[self addKeyDownEvent: (int)MOUSEBUTTON_Right withCharacter: (wchar_t)0xFFFF repeated: ([Event clickCount] % 2) == 0];
	}
	else
	{
		GSpecialKeysState |= (unsigned long)SpecialKey_LeftMouseButton;
		[self addKeyDownEvent: (int)MOUSEBUTTON_Left withCharacter: (wchar_t)0xFFFF repeated: ([Event clickCount] % 2) == 0];
	}
	GWasRightClick = CommandKeyPressed;
}

-(void)mouseUp:(NSEvent*)Event
{
	[self addMouseMoveEvent: Event];
	if( GWasRightClick )
	{
		GSpecialKeysState &= ~(unsigned long)SpecialKey_RightMouseButton;
		[self addKeyUpEvent: (int)MOUSEBUTTON_Right ];
	}
	else
	{
		GSpecialKeysState &= ~(unsigned long)SpecialKey_LeftMouseButton;
		[self addKeyUpEvent: (int)MOUSEBUTTON_Left ];
	}
	GWasRightClick = false;
}

-(void)rightMouseDown:(NSEvent*)Event
{
	[self addMouseMoveEvent: Event];
	GSpecialKeysState |= (unsigned long)SpecialKey_RightMouseButton;
	[self addKeyDownEvent: (int)MOUSEBUTTON_Right withCharacter: (wchar_t)0xFFFF repeated: ( [Event clickCount] > 1 )];
}

-(void)rightMouseUp:(NSEvent*)Event
{
	[self addMouseMoveEvent: Event];
	GSpecialKeysState &= ~(unsigned long)SpecialKey_RightMouseButton;
	[self addKeyUpEvent: (int)MOUSEBUTTON_Right];
}

-(void)otherMouseDown:(NSEvent*)Event
{
	[self addMouseMoveEvent: Event];
	GSpecialKeysState |= (unsigned long)SpecialKey_MiddleMouseButton;
	[self addKeyDownEvent: (int)MOUSEBUTTON_Middle withCharacter: (wchar_t)0xFFFF repeated: ( [Event clickCount] > 1 )];
}

-(void)otherMouseUp:(NSEvent*)Event
{
	[self addMouseMoveEvent: Event];
	GSpecialKeysState &= ~(unsigned long)SpecialKey_MiddleMouseButton;
	[self addKeyUpEvent: (int)MOUSEBUTTON_Middle];
}

-(void)scrollWheel:(NSEvent*)Event
{
	[self addMouseMoveEvent: Event];

	MacEvent EventData;
	EventData.EventType = MACEVENT_MouseWheel;
	EventData.Data.MouseWheel.MoveAmount = [Event deltaY];
	AddMacEventFromNSViewToViewportQueue( MacViewport, &EventData );
}

-(void)moveMouseToPosition: (CGPoint)Position
{
	LastMousePos.x = Position.x;
	LastMousePos.y = Position.y;
	NSRect ScreenFrame = [[[self window] screen] frame];
	Position.y = ScreenFrame.size.height - Position.y;	// The mouse event takes Y counted from above, like Windows
	CGEventRef MouseMoveCommand = CGEventCreateMouseEvent( NULL, kCGEventMouseMoved, Position, 0 );
	CGEventPost( kCGHIDEventTap, MouseMoveCommand );
	CFRelease( MouseMoveCommand );
}

-(CGPoint)windowCenterInScreenCoords;
{
	NSRect Frame = [[self window] frame];
	NSPoint WindowCenter;
	WindowCenter.x = Frame.size.width/2;
	WindowCenter.y = Frame.size.height/2;
	NSPoint WindowCenterOnScreen = [[self window] convertBaseToScreen: WindowCenter];
	CGPoint WCPosition = { WindowCenterOnScreen.x, WindowCenterOnScreen.y };
	return WCPosition;
}

-(void)moveMouseToWindowCenter
{
	CGPoint WindowCenter = [self windowCenterInScreenCoords];
	[self moveMouseToPosition: WindowCenter];
}

-(void)moveMouseToWindowCenterFrom: (NSPoint) Position
{
	CGPoint WindowCenter = [self windowCenterInScreenCoords];
	if( ( (int)Position.x == (int)WindowCenter.x ) && ( (int)Position.y == (int)WindowCenter.y ) )
		return;
	[self moveMouseToPosition: WindowCenter];
}

-(void)addMouseMoveEvent:(NSEvent*)Event
{
	NSPoint PointInViewport = [self convertPoint: [Event locationInWindow] fromView: nil];
	NSPoint PointOnScreen = [[self window] convertBaseToScreen: [Event locationInWindow]];

	if( FirstMouseMoveEvent )
	{
		// Init LastMousePos
		[self moveMouseToWindowCenterFrom: PointOnScreen];
		FirstMouseMoveEvent = NO;
		return;
	}

	int MouseMoveX = (int)(PointOnScreen.x - LastMousePos.x);
	int MouseMoveY = (int)(PointOnScreen.y - LastMousePos.y);

	if( MouseMoveX || MouseMoveY )
	{
		MacEvent EventData;

		EventData.EventType = MACEVENT_MousePosDiff;
		EventData.Data.MousePosDiff.X = MouseMoveX;
		EventData.Data.MousePosDiff.Y = MouseMoveY;
		AddMacEventFromNSViewToViewportQueue( MacViewport, &EventData );

		NSSize ViewportFrameSize = [self frame].size;
		int ViewportFrameSizeX = (int)ViewportFrameSize.width;
		int ViewportFrameSizeY = (int)ViewportFrameSize.height;

		SimulatedMousePosX += MouseMoveX;
		SimulatedMousePosY -= MouseMoveY;

		if( SimulatedMousePosX < 0 )
		{
			SimulatedMousePosX = 0;
		}
		else if( SimulatedMousePosX >= ViewportFrameSizeX )
		{
			SimulatedMousePosX = ViewportFrameSizeX - 1;
		}

		if( SimulatedMousePosY < 0 )
		{
			SimulatedMousePosY = 0;
		}
		else if( SimulatedMousePosY >= ViewportFrameSizeY )
		{
			SimulatedMousePosY = ViewportFrameSizeY - 1;
		}

		// Mouse position info
		EventData.EventType = MACEVENT_MousePos;
		EventData.Data.MousePos.X = SimulatedMousePosX;
		EventData.Data.MousePos.Y = SimulatedMousePosY;
		AddMacEventFromNSViewToViewportQueue( MacViewport, &EventData );
	}

	[self moveMouseToWindowCenterFrom: PointOnScreen];
}

-(void)showCursor:(BOOL)Show
{
	if( GCursorShown == Show )
		return;

	if( Show )
	{
		SimulatedMousePosX = LastVisibleCursorPosX;
		SimulatedMousePosY = LastVisibleCursorPosY;
		[NSCursor unhide];
	}
	else
	{
		[NSCursor hide];
		LastVisibleCursorPosX = SimulatedMousePosX;
		LastVisibleCursorPosY = SimulatedMousePosY;
	}

	GCursorShown = Show;
}

-(BOOL)cursorShown
{
	return GCursorShown;
}

- (void)windowWillClose:(NSNotification*)Notification
{
	// When the window closes, quit the application cleanly
	MacAppRequestExit( 0 );
}

- (void)windowDidResize:(NSNotification *)Notification
{
	NSWindow *Window = [Notification object];
	
	MacEvent EventData;
	EventData.EventType = MACEVENT_WindowResize;
	NSRect ContentFrame = [Window contentRectForFrameRect:[Window frame]];
	EventData.Data.WindowSize.Width = ContentFrame.size.width;
	EventData.Data.WindowSize.Height = ContentFrame.size.height;
	AddMacEventFromNSViewToViewportQueue( MacViewport, &EventData );
}

@end

/**
 * Returns system version number
 */
int MacGetOSXVersion()
{
	int version;
	if (Gestalt(gestaltSystemVersion, &version) == noErr)
	{
		return version;
	}
	else
	{
		return 0x1060;
	}
}

/**
 * Get the path to the .app where file loading occurs
 * 
 * @param AppDir [out] Return path for the application directory that is the root of file loading
 * @param MaxLen Size of AppDir buffer
 */
void MacGetAppSupportDirectory( char *AppSupportDir, int MaxLen )
{
	// use the API to retrieve where the application is stored
	NSString *dir = [NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES) objectAtIndex: 0];
	[dir getCString: AppSupportDir maxLength: MaxLen encoding: NSASCIIStringEncoding];
}

/**
 * Creates a directory (must be in the Documents directory
 *
 * @param Directory Path to create
 * @param bMakeTree If true, it will create intermediate directory
 *
 * @return true if successful)
 */
bool MacCreateDirectory(char* Directory, bool bMakeTree)
{
	NSAutoreleasePool *autoreleasepool = [[NSAutoreleasePool alloc] init];
	
	// convert to Mac string
	NSFileManager* FileManager = [NSFileManager defaultManager];
	NSString* NSPath = [FileManager stringWithFileSystemRepresentation:Directory length:strlen(Directory)];

	// create the directory (with optional tree)
	BOOL Result = [FileManager createDirectoryAtPath:NSPath withIntermediateDirectories:bMakeTree attributes:nil error:nil];
	
	[autoreleasepool release];
	
	return Result;
}

/**
 * Shows an alert, blocks until user selects a choice, and then returns the index of the 
 * choice the user has chosen
 *
 * @param Title		The title of the message box
 * @param Message	The text to display
 * @param Button0	Label for Button0
 * @param Button1	Label for Button1 (or NULL for no Button1)
 * @param Button2	Label for Button2 (or NULL for no Button2)
 *
 * @return 0, 1 or 2 depending on which button was clicked
 */
int MacShowBlockingAlert(const char* Title, const char *Message, const char* Button0, const char* Button1, const char* Button2)
{
	NSString *TitleStr = [NSString stringWithUTF8String:Title];
	NSString *MessageStr = [NSString stringWithUTF8String:Message];
	NSString *Button0Str = [NSString stringWithUTF8String:Button0];
	NSString *Button1Str = Button1 ? [NSString stringWithUTF8String:Button1] : nil;
	NSString *Button2Str = Button2 ? [NSString stringWithUTF8String:Button2] : nil;

	NSInteger ButtonClicked = NSRunInformationalAlertPanel(TitleStr, MessageStr, Button0Str, Button1Str, Button2Str);
	switch (ButtonClicked)
	{
	case NSAlertDefaultReturn:
	default:
		return 0;
	case NSAlertAlternateReturn:
		return 1;
	case NSAlertOtherReturn:
		return 2;
	}
}

/**
 * Gets the language the user has selected
 *
 * @param Language String to receive the Language into
 * @param MaxLen Size of the Language string
 */
void MacGetUserLanguage(char* Language, int MaxLen)
{
	// get the set of languages
	NSArray* Languages = [[NSUserDefaults standardUserDefaults] objectForKey:@"AppleLanguages"];

	// get the language the user would like (first element is selected)
	NSString* PreferredLanguage = [Languages objectAtIndex:0];

	// convert to C string to pass back
	[PreferredLanguage getCString:Language maxLength:MaxLen encoding:NSASCIIStringEncoding];
}

/**
 *	Pumps Mac OS X messages.
 */
void appMacPumpMessages()
{
	NSAutoreleasePool *Pool = [[NSAutoreleasePool alloc] init];

	while (TRUE)
	{
		NSEvent *Event = [NSApp nextEventMatchingMask: NSAnyEventMask untilDate: nil inMode: NSDefaultRunLoopMode dequeue: TRUE];
		if( Event )
		{
			[NSApp sendEvent: Event];
		}
		else
		{
			break;
		}
	}

	[Pool release];

	// In some situations the system can make the cursor visible (for example, when the dock overlaps the window and the mouse is moved over the dock),
	// so we hide it back here
	if (!GCursorShown && CGCursorIsVisible())
	{
		[NSCursor hide];
	}
}

void *MacCreateWindow(const wchar_t *Name, int Fullscreen)
{
	NSRect ViewRect = NSMakeRect(0,0,100,100);	// doesn't matter, the viewport will dictate this
	NSWindow *Window;

	if( Fullscreen )
	{
		Window = [[NSWindow alloc] initWithContentRect:ViewRect
								   styleMask: NSBorderlessWindowMask
								   backing:NSBackingStoreBuffered
								   defer:YES];
		[Window setIgnoresMouseEvents:YES];
		[Window setLevel: NSMainMenuWindowLevel+1];
	}
	else
	{
		Window = [[NSWindow alloc] initWithContentRect:ViewRect
								   styleMask: (NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask)
								   backing:NSBackingStoreBuffered
								   defer:YES];
		[Window setAcceptsMouseMovedEvents:YES];

		if( GMacOSXVersion >= MacOSXVersion_Lion )
		{
			[Window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
		}

		size_t NameLength = wcslen(Name);
		CFStringRef CFName = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)Name, NameLength * sizeof(UInt32), kCFStringEncodingUTF32LE, false);
		[Window setTitle:(NSString *)CFName];
		CFRelease(CFName);
	}
	[Window setOpaque:YES];

	return Window;
}

void MacDestroyWindow(void *Window)
{
	[(NSWindow *)Window release];
}

void *MacCreateOpenGLView(void *MacViewport, int SizeX, int SizeY)
{
	NSRect Bounds = NSMakeRect( 0, 0, SizeX, SizeY );
	UOpenGLView *View = [[UOpenGLView alloc] initWithFrame:Bounds];
	[View setMacViewport: MacViewport];
	return View;
}

void *MacCreateFullscreenOpenGLView(void)
{
	NSRect Bounds = [[NSScreen mainScreen] frame];
	NSView *View = [[NSView alloc] initWithFrame:Bounds];
	return View;
}

void MacResizeOpenGLView(void *OpenGLView, int SizeX, int SizeY)
{
	UOpenGLView *View = (UOpenGLView*)OpenGLView;
	NSWindow* Window = [View window];
	[View setFrameSize: NSMakeSize(SizeX,SizeY)];
	extern bool GIsUserResizingWindow;
	if (!GIsUserResizingWindow)
	{
		[Window setFrame: [Window frameRectForContentRect: [View frame]] display: TRUE];	// resize window to fit the view
		[Window center];
	}
}

void MacReleaseOpenGLView(void *OpenGLView)
{
	[(NSView*)OpenGLView release];
}

void MacShowWindow(void *Window, int Show)
{
	NSWindow* Wind = (NSWindow *)Window;
	if( Show )
	{
		if( [Wind styleMask] == NSBorderlessWindowMask )
		{
			[Wind orderFront:nil];
		}
		else
		{
			[Wind makeKeyAndOrderFront:nil];
		}
	}
	else
	{
		[Wind orderOut:nil];
	}
}

void MacShowWindowByView(void *OpenGLView, int Show)
{
	MacShowWindow( [(NSView*)OpenGLView window], Show );
}

void MacUpdateGLContextByView(void *OpenGLView)
{
	NSOpenGLContext *Context = [(UOpenGLView *)OpenGLView openGLContext];
	[Context update];
}

void MacAttachOpenGLViewToWindow(void *Window, void *OpenGLView)
{
	NSWindow* Wind = (NSWindow *)Window;
	[Wind setFrame: [(NSWindow*)Window frameRectForContentRect: [(NSView*)OpenGLView frame]] display: FALSE];	// resize window to fit the view
	[Wind setContentView:(NSView*)OpenGLView];
	[Wind setDelegate:(UOpenGLView*)OpenGLView];
	if( [Wind styleMask] == NSBorderlessWindowMask )
	{
		[Wind setFrameOrigin: NSMakePoint(0,0)];	// if we center it, it'll be below menu
	}
	else
	{
		[Wind center];
	}
}

void MacLionToggleFullScreen(void *Window)
{
	[(NSWindow *)Window toggleFullScreen:nil];
}

void MacShowCursorInViewport(void *OpenGLView, int Show)
{
	[(UOpenGLView *)OpenGLView showCursor: ( Show != 0 ) ? YES : NO ];
}

void *MacCreateOpenGLContext(void *OpenGLView, void *FullscreenOpenGLView, int SizeX, int SizeY, int Fullscreen)
{
	NSOpenGLContext *SharedContext = [(UOpenGLView *)OpenGLView openGLContext];
	NSOpenGLPixelFormat *PixelFormat = [(UOpenGLView *)OpenGLView pixelFormat];
//	NSOpenGLContext *Context = [[NSOpenGLContext alloc] initWithFormat:PixelFormat shareContext:SharedContext];
	NSOpenGLContext *Context = SharedContext;

	CGLContextObj CGLContext = (CGLContextObj)[Context CGLContextObj];
	if( Fullscreen && FullscreenOpenGLView )
	{
//		GLint Dimensions[2] = { SizeX, SizeY };
//		CGLSetParameter( CGLContext, kCGLCPSurfaceBackingSize, Dimensions );
//		CGLEnable( CGLContext, kCGLCESurfaceBackingSize );
		[Context setView:(NSView*)FullscreenOpenGLView];
	}
	else
	{
//		CGLDisable( CGLContext, kCGLCESurfaceBackingSize );
		[Context setView:(UOpenGLView *)OpenGLView];
	}
	[Context update];
//	[Context copyAttributesFromContext: SharedContext withMask: GL_ALL_ATTRIB_BITS];
	return Context;
}

void MacDestroyOpenGLContext(void *OpenGLView, void *Context)
{
//	NSOpenGLContext *SharedContext = [(UOpenGLView *)OpenGLView openGLContext];
//	if( [NSOpenGLContext currentContext] == (NSOpenGLContext *)Context )
//	{
//		[SharedContext makeCurrentContext];
//	}
//	[SharedContext copyAttributesFromContext: (NSOpenGLContext *)Context withMask: GL_ALL_ATTRIB_BITS];
	[(NSOpenGLContext *)Context clearDrawable];
//	[(NSOpenGLContext *)Context release];
}

void *MacGetOpenGLContextFromView(void *OpenGLView)
{
	return [(UOpenGLView *)OpenGLView openGLContext];
}

void MacFlushGLBuffers(void *Context)
{
	[(NSOpenGLContext *)Context flushBuffer];
}

void MacMakeOpenGLContextCurrent(void *Context)
{
	[(NSOpenGLContext *)Context makeCurrentContext];
}

void *MacCreateCocoaAutoreleasePool()
{
	NSAutoreleasePool *autoreleasepool = [[NSAutoreleasePool alloc] init];
	return (void*)autoreleasepool;
}

void MacReleaseCocoaAutoreleasePool(void* Pool)
{
	[(NSAutoreleasePool*)Pool release];
}

void MacIssueMouseEventDoingNothing()
{
	CGEventRef LocationEvent = CGEventCreate( NULL );
	CGPoint MouseLocation = CGEventGetLocation( LocationEvent );
	CFRelease( LocationEvent );
	CGEventRef MouseMoveCommand = CGEventCreateMouseEvent( NULL, kCGEventMouseMoved, MouseLocation, 0 );
	CGEventPost( kCGHIDEventTap, MouseMoveCommand );
	CFRelease( MouseMoveCommand );
}

void MacGetDisplaySize(float& DisplaySizeX, float& DisplaySizeY)
{
	NSRect Bounds = [[NSScreen mainScreen] frame];
	DisplaySizeX = Bounds.size.width;
	DisplaySizeY = Bounds.size.height;
}

const void *MacGetDisplayModesArray()
{
	return CGDisplayCopyAllDisplayModes(CGMainDisplayID(), NULL);
}

unsigned int MacGetDisplayModesCount(const void *Array)
{
	return CFArrayGetCount((CFArrayRef)Array);
}

unsigned int MacGetDisplayModeWidth(const void *Array, unsigned int Index)
{
	CGDisplayModeRef Mode = (CGDisplayModeRef)CFArrayGetValueAtIndex((CFArrayRef)Array, Index);
	return CGDisplayModeGetWidth(Mode);
}

unsigned int MacGetDisplayModeHeight(const void *Array, unsigned int Index)
{
	CGDisplayModeRef Mode = (CGDisplayModeRef)CFArrayGetValueAtIndex((CFArrayRef)Array, Index);
	return CGDisplayModeGetHeight(Mode);
}

unsigned int MacGetDisplayModeRefreshRate(const void *Array, unsigned int Index)
{
	CGDisplayModeRef Mode = (CGDisplayModeRef)CFArrayGetValueAtIndex((CFArrayRef)Array, Index);
	return (unsigned int)CGDisplayModeGetRefreshRate(Mode);
}

void MacReleaseDisplayModesArray(const void *Array)
{
	CFRelease((CFArrayRef)Array);
}

wchar_t*			GMacSplashBitmapPath = 0;
wchar_t*			GMacSplashStartupProgress = 0;
wchar_t*			GMacSplashVersionInfo = 0;
wchar_t*			GMacSplashCopyrightInfo = 0;

extern uint32_t		GIsEditor;

static NSImage* BackgroundImage = nil;

@interface MySplashView : NSView
{
}

- (void)mouseDown: (NSEvent*) event;
- (BOOL)acceptsFirstResponder;
- (void)drawRect: (NSRect) dirtyRect;

@end

@implementation MySplashView

- (void)mouseDown: (NSEvent*) event
{
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (void)drawRect: (NSRect) dirtyRect
{
	NSAutoreleasePool *autoreleasepool = [[NSAutoreleasePool alloc] init];

	// Draw background
	[BackgroundImage drawAtPoint: NSMakePoint(0,0) fromRect: NSZeroRect operation: NSCompositeCopy fraction: 1.0];

	// Draw copyright info string(if it exists)
	if( GMacSplashCopyrightInfo )
	{
		int Len = 0;
		for( wchar_t* Ptr = GMacSplashCopyrightInfo; *Ptr; ++Ptr )
			++Len;

		if( Len )
		{
			NSString* ControlText = [[NSString alloc] initWithBytes: GMacSplashCopyrightInfo length: (Len+1)*sizeof(wchar_t) encoding: NSUTF16LittleEndianStringEncoding];
			if( ControlText )
			{
				NSPoint ControlAnchor;
				ControlAnchor.x = 10;
				if( GIsEditor )
					ControlAnchor.y = 34;
				else
					ControlAnchor.y = 6;
				NSDictionary* Dict =
					[NSDictionary dictionaryWithObjects:
						[NSArray arrayWithObjects:
							[NSColor colorWithDeviceRed: 240.0/255.0 green: 240.0/255.0 blue: 240.0/255.0 alpha: 1.0 ],
							[NSFont fontWithName:@"Helvetica-Bold" size:11],
							[NSColor colorWithDeviceRed: 0.0 green: 0.0 blue: 0.0 alpha: 1.0],
							[NSNumber numberWithFloat:-4.0],
							nil ]
					forKeys:
						[NSArray arrayWithObjects:
							NSForegroundColorAttributeName,
							NSFontAttributeName,
							NSStrokeColorAttributeName,
							NSStrokeWidthAttributeName,
							nil]
					];
				[ControlText drawAtPoint: ControlAnchor withAttributes: Dict];
				[ControlText release];
			}
		}
	}

	// Draw version info string(if it exists)
	if( GMacSplashVersionInfo )
	{
		int Len = 0;
		for( wchar_t* Ptr = GMacSplashVersionInfo; *Ptr; ++Ptr )
			++Len;

		if( Len )
		{
			NSString* ControlText = [[NSString alloc] initWithBytes: GMacSplashVersionInfo length: (Len+1)*sizeof(wchar_t) encoding: NSUTF16LittleEndianStringEncoding];
			if( ControlText )
			{
				NSDictionary* Dict =
					[NSDictionary dictionaryWithObjects:
						[NSArray arrayWithObjects:
							[NSColor colorWithDeviceRed: 240.0/255.0 green: 240.0/255.0 blue: 240.0/255.0 alpha: 1.0 ],
							[NSFont fontWithName:@"Helvetica-Bold" size:11],
							[NSColor colorWithDeviceRed: 0.0 green: 0.0 blue: 0.0 alpha: 1.0],
							[NSNumber numberWithFloat:-4.0],
							nil ]
					forKeys:
						[NSArray arrayWithObjects:
							NSForegroundColorAttributeName,
							NSFontAttributeName,
							NSStrokeColorAttributeName,
							NSStrokeWidthAttributeName,
							nil]
					];
				[ControlText drawAtPoint: NSMakePoint( 10, 20 ) withAttributes: Dict];
				[ControlText release];
			}
		}
	}

	// Draw startup progress string(if it exists)
	if( GMacSplashStartupProgress )
	{
		int Len = 0;
		for( wchar_t* Ptr = GMacSplashStartupProgress; *Ptr; ++Ptr )
			++Len;

		if( Len )
		{
			NSString* ControlText = [[NSString alloc] initWithBytes: GMacSplashStartupProgress length: (Len+1)*sizeof(wchar_t) encoding: NSUTF16LittleEndianStringEncoding];
			if( ControlText )
			{
				NSDictionary* Dict =
					[NSDictionary dictionaryWithObjects:
						[NSArray arrayWithObjects:
							[NSColor colorWithDeviceRed: 240.0/255.0 green: 240.0/255.0 blue: 240.0/255.0 alpha: 1.0 ],
							[NSFont fontWithName:@"Helvetica-Bold" size:11],
							[NSColor colorWithDeviceRed: 0.0 green: 0.0 blue: 0.0 alpha: 1.0],
							[NSNumber numberWithFloat:-4.0],
							nil ]
					forKeys:
						[NSArray arrayWithObjects:
							NSForegroundColorAttributeName,
							NSFontAttributeName,
							NSStrokeColorAttributeName,
							NSStrokeWidthAttributeName,
							nil]
					];
				[ControlText drawAtPoint: NSMakePoint( 10, 0 ) withAttributes: Dict];
				[ControlText release];
			}
		}
	}

	[autoreleasepool release];
}

@end

@interface MySplashWindow : NSWindow
{
}

- (BOOL)canBecomeKeyWindow;

@end

@implementation MySplashWindow

- (BOOL)canBecomeKeyWindow
{
	return YES;
}

@end

static MySplashWindow* SplashWindow = nil;

void appShowSplashMac( void )
{
	NSAutoreleasePool *autoreleasepool = [[NSAutoreleasePool alloc] init];

	// Convert path to background file from wchar_t to NSString
	int Len = 0;
	for( wchar_t* Ptr = GMacSplashBitmapPath; *Ptr; ++Ptr )
	{
		if( *Ptr == '\\' )
			*Ptr = '/';
		++Len;
	}

	NSString* BackgroundPath = [[NSString alloc] initWithBytes: GMacSplashBitmapPath length: (Len+1)*sizeof(wchar_t) encoding: NSUTF32LittleEndianStringEncoding];
	if( BackgroundPath )
	{
		// Load bitmap into NSImage
		BackgroundImage = [[NSImage alloc] initWithContentsOfFile: BackgroundPath];
		[BackgroundPath release];
	}

	if( BackgroundImage )
	{
		// Now get image size in pixels. Guess what - as an example of Apple retardedness, you need to make a copy of the whole image to be able to get this information.
		NSBitmapImageRep* BitmapRep = [NSBitmapImageRep imageRepWithData: [BackgroundImage TIFFRepresentation]];
		float PixelsWide = [BitmapRep pixelsWide];
		float PixelsHigh = [BitmapRep pixelsHigh];

		NSRect ContentRect;
		ContentRect.origin.x = 0;
		ContentRect.origin.y = 0;
		ContentRect.size.width = PixelsWide;
		ContentRect.size.height = PixelsHigh;

		[BackgroundImage setSize: ContentRect.size];

		// Create bordeless window with size from NSImage
		SplashWindow = [[MySplashWindow alloc] initWithContentRect: ContentRect styleMask: 0 backing: NSBackingStoreBuffered defer: NO];
		[SplashWindow setAcceptsMouseMovedEvents: TRUE];
		[SplashWindow setContentView: [[MySplashView alloc] initWithFrame: ContentRect] ];

		if( SplashWindow )
		{
			// Show window
//			[SplashWindow setLevel: NSStatusWindowLevel];
			[SplashWindow center];
			[SplashWindow orderFront: nil];
		}
	}

	[autoreleasepool release];

	appMacPumpMessages();
}

void appHideSplashMac( void )
{
	NSAutoreleasePool *autoreleasepool = [[NSAutoreleasePool alloc] init];

	if( SplashWindow )
	{
		// Hide window
		[SplashWindow orderOut: nil];
			
		// Delete window
		[SplashWindow close];

		// Forget about window
		SplashWindow = nil;
	}

	if( BackgroundImage )
	{
		// Delete background image
		[BackgroundImage release];

		// Forget about background image
		BackgroundImage= nil;
	}

	[autoreleasepool release];
}

/**
 * Checks EULA acceptance in UDK
 * return - TRUE if user has accepted the EULA and it is stored in the registry
 */
bool MacIsEULAAccepted(const char *RTFPath)
{
	NSUserDefaults *UserDefaults = [NSUserDefaults standardUserDefaults];
	if (UserDefaults && [UserDefaults boolForKey:@"EULA Accepted"])
	{
		return true;
	}

	bool ReturnValue = [(UE3AppDelegate *)[NSApp delegate] ShowEULA:RTFPath];
	if (ReturnValue)
	{
		[UserDefaults setBool:YES forKey:@"EULA Accepted"];
	}

	return ReturnValue;
}

UBOOL MacLoadRadioEffectComponent()
{
	UBOOL bLoaded = FALSE;

	CFBundleRef MainBundleRef = CFBundleGetMainBundle();
	if( MainBundleRef )
	{
		CFURLRef ComponentURLRef = CFBundleCopyResourceURL( MainBundleRef, CFSTR("RadioEffectUnit"), CFSTR("component"), 0 );
		FSRef ComponentFSRef;
		if( ComponentURLRef )
		{
			if( CFURLGetFSRef( ComponentURLRef, &ComponentFSRef ) )
			{
				OSStatus Status = RegisterComponentFileRef( &ComponentFSRef, false );
				bLoaded = Status == noErr;
			}

			CFRelease( ComponentURLRef );
		}
	}

	return bLoaded;
}

