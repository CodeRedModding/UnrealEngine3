/*=============================================================================
 MacObjCWrapper.h: Mac wrapper for making ObjC calls from C++ code
 Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#ifndef __MAC_OBJC_WRAPPER_H__
#define __MAC_OBJC_WRAPPER_H__

#define MAC_PATH_MAX 1024

struct DoubleWindow
{
	void*	MainWindow;
	void*	FullscreenWindow;

	DoubleWindow() : MainWindow(NULL), FullscreenWindow(NULL) {}
};

/** Mac OS X version												*/
extern int GMacOSXVersion;
const int MacOSXVersion_Lion = 0x1070;

/**
 * Returns system version number
 */
int MacGetOSXVersion();

/**
 * Get the path to the user's Application Support directory where file saving occurs
 * 
 * @param AppSupportDir [out] Return path for the application directory that is the root of file saving
 * @param MaxLen Size of AppSupportDir buffer
 */
void MacGetAppSupportDirectory( char *AppSupportDir, int MaxLen );

/**
 * Creates a directory (must be in the Documents directory
 *
 * @param Directory Path to create
 * @param bMakeTree If true, it will create intermediate directory
 *
 * @return true if successful)
 */
bool MacCreateDirectory(char* Directory, bool bMakeTree);

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
int MacShowBlockingAlert(const char* Title, const char *Message, const char* Button0, const char* Button1=0, const char* Button2=0);

/**
 * Gets the language the user has selected
 *
 * @param Language String to receive the Language into
 * @param MaxLen Size of the Language string
 */
void MacGetUserLanguage(char* Language, int MaxLen);

void *MacCreateWindow(const wchar_t *Name, int Fullscreen);
void MacDestroyWindow(void *Window);
void *MacCreateOpenGLView(void *MacViewport, int SizeX, int SizeY);
void MacResizeOpenGLView(void *OpenGLView, int SizeX, int SizeY);	// don't use this with fullscreen view, it stays at display resolution
void MacReleaseOpenGLView(void *OpenGLView);
void *MacCreateFullscreenOpenGLView(void);
void MacShowWindow(void *Window, int Show);
void MacShowWindowByView(void *OpenGLView, int Show);
void MacUpdateGLContextByView(void *OpenGLView);
void MacAttachOpenGLViewToWindow(void *Window, void *OpenGLView);
void MacLionToggleFullScreen(void *Window);
void MacShowCursorInViewport(void *OpenGLView, int Show);
void *MacCreateOpenGLContext(void *OpenGLView, void* FullscreenOpenGLView, int SizeX, int SizeY, int Fullscreen);
void MacDestroyOpenGLContext(void *OpenGLView, void *Context);
void *MacGetOpenGLContextFromView(void *OpenGLView);
void MacFlushGLBuffers(void *Context);
void MacMakeOpenGLContextCurrent(void *Context);
void MacGetDisplaySize(float& DisplaySizeX, float& DisplaySizeY);
const void *MacGetDisplayModesArray();
unsigned int MacGetDisplayModesCount(const void *Array);
unsigned int MacGetDisplayModeWidth(const void *Array, unsigned int Index);
unsigned int MacGetDisplayModeHeight(const void *Array, unsigned int Index);
unsigned int MacGetDisplayModeRefreshRate(const void *Array, unsigned int Index);
void MacReleaseDisplayModesArray(const void *Array);

void *MacCreateCocoaAutoreleasePool();
void MacReleaseCocoaAutoreleasePool(void* Pool);

void MacIssueMouseEventDoingNothing();

enum EMacEventType
{
	MACEVENT_Key = 0,
	MACEVENT_MousePos = 1,
	MACEVENT_MousePosDiff = 2,
	MACEVENT_MouseWheel = 3,
	MACEVENT_WindowResize = 4
};

enum EMouseButtonCodes
{
	MOUSEBUTTON_Left = -1,
	MOUSEBUTTON_Right = -2,
	MOUSEBUTTON_Middle = -3,
};

struct MacEvent
{
	EMacEventType	EventType;	// Only relevant fields are valid in all cases. Mouse buttons are keys.

	union DataUnion
	{
		struct KeyStruct
		{
			short	KeyCode;		// this is Mac keyboard scancode, so a separate table will be needed on client side to translate it to UnrealEngine key names.
									// Also, left mouse button is -1, right mouse button is -2, middle mouse button is -3.
			wchar_t	Character;		// Valid character for the key, or 0xFFFF otherwise.
			int		KeyPressed:1;	// if 1, key was pressed, if 0 - released.
			int		Repeat:1;		// if 1, key was already pressed before (double-click for mouse buttons, repeat keypress for keys)
		} Key;

		struct MousePosStruct
		{
			int	X;	// 0 is on the left side of the viewport.
			int	Y;	// 0 is on the lower edge of the viewport.
		} MousePos;

		struct MousePosDiffStruct
		{
			int	X;	// >0 is movement to the right side of the view.
			int	Y;	// >0 is movement down, like on Windows.
		} MousePosDiff;
		
		struct MouseWheelStruct
		{
			float MoveAmount;
		} MouseWheel;

		struct WindowSizeStruct
		{
			int Width;
			int Height;
		} WindowSize;

		struct AppActiveStruct
		{
			bool bIsActive;
		} AppActive;
	} Data;
};

void AddMacEventFromNSViewToViewportQueue( void* MacViewport, MacEvent* Event );

void MacAppRequestExit( int ForceExit );

#endif
