/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __WPFWindowWrapperCLR_h__
#define __WPFWindowWrapperCLR_h__

#ifdef _MSC_VER
	#pragma once
#endif

#ifndef __cplusplus_cli
	#error "This file must be compiled as managed code using the /clr compiler option."
#endif

#include "UnrealEdCLR.h"
#include "ManagedCodeSupportCLR.h"

using namespace System;
using namespace System::Windows;
using namespace System::Windows::Controls;	   


/** Helper macro to declare a "simple" property that fires notifications when changed.  You can use this for
    properties that you want to be able to bind in WPF but don't need any special accessor handling. */
#define DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( InPropertyType, InPropertyName ) \
	private: \
		InPropertyType InPropertyName##Value; \
	public: \
		property InPropertyType InPropertyName \
		{ \
			InPropertyType get() { return InPropertyName##Value; } \
			void set( InPropertyType Value ) \
			{ \
				if( InPropertyName##Value != Value ) \
				{ \
					InPropertyName##Value = Value; \
					OnPropertyChanged( #InPropertyName ); \
				} \
			} \
		}


/** Helper macro to declare a property that maps to an existing value. */
#define DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY( InPropertyName, MappedVariable ) \
		property bool InPropertyName \
		{ \
			bool get() { return ( ( MappedVariable ) ? true : false ); } \
			void set( bool Value ) \
			{ \
				if( (UBOOL)Value != MappedVariable ) \
				{ \
					MappedVariable = (UBOOL)Value; \
					OnPropertyChanged( #InPropertyName ); \
				} \
			} \
		}


/** Helper macro to declare a property that maps to an existing value. */
#define DECLARE_MAPPED_NOTIFY_PROPERTY( InPropertyType, InPropertyName, MappedVariableType, MappedVariable ) \
		property InPropertyType InPropertyName \
		{ \
			InPropertyType get() { return static_cast<InPropertyType>( MappedVariable ); } \
			void set( InPropertyType Value ) \
			{ \
				if( (MappedVariableType)Value != MappedVariable ) \
				{ \
					MappedVariable = (MappedVariableType)Value; \
					OnPropertyChanged( #InPropertyName ); \
				} \
			} \
		}


/** Helper macro to declare a property that maps to a single value in an enum. */
#define DECLARE_ENUM_NOTIFY_PROPERTY( InPropertyType, InPropertyName, EnumType, EnumVariable, EnumValue ) \
		property InPropertyType InPropertyName \
		{ \
			InPropertyType get() { return EnumVariable == EnumValue ? true : false; } \
			void set( InPropertyType Value ) \
			{ \
				if( Value != InPropertyName ) \
				{ \
					EnumVariable = Value == true ? EnumValue : (EnumType)-1; \
					OnPropertyChanged( #InPropertyName ); \
				} \
			} \
		}


/**
 * WPF window wrapper
 */
ref class MWPFWindowWrapper
{

public:

	/** Constructor */
	MWPFWindowWrapper()
		: FakeTitleBarHeight( 0 ),
		  FakeTitleBarButtonWidth (0)
	{
	}


	/** Destructor (deterministic; effectively IDisposable.Dispose() to the CLR) */
	virtual ~MWPFWindowWrapper()
	{
		// Note: Because we're using an auto_handle, this is probably redundant
		DisposeOfInteropWindow();
	}


protected:

	/** Finalizer (non-deterministic destructor) */
	!MWPFWindowWrapper()
	{
	}



	/** Clean up the interop window if it still exists */
	void DisposeOfInteropWindow()
	{
		// Dispose of the window.  This also cancels our various event handlers and message hooks.
		InteropWindow.reset();
	}



public:

	/**
	 * Initialize the window & Finalizes the layout
	 *
	 * @param	InParentWindowHandle	Parent window handle
	 * @param	InWindowTitle			Window title
	 * @param	InWPFXamlFileName		Filename of the WPF .xaml file
	 * @param	InPositionX				X position of window (-1 for default)
	 * @param	InPositionY				Y position of window (-1 for default)
	 * @param	InWidth					Width of window (0 for default)
	 * @param	InHeight				Height of window (0 for default)
	 * @param	bCenterWindow			True to center the window in the app (incoming position is ignored)
	 * @param	InFakeTitleBarHeight	Height of fake title bar (or 0)
	 *
	 * @return	TRUE if successful
	 */
	UBOOL InitWindow( const HWND InParentWindowHandle,
					  String^ InWindowTitle,
					  String^ InWPFXamlFileName,
					  int InPositionX,
					  int InPositionY,
					  int InWidth,
					  int InHeight,
					  bool bCenterWindow,
					  int InFakeTitleBarHeight,
					  int bInIsTopmost)
	{
		UBOOL bShowInTaskBar = FALSE;
		UBOOL bAllowFrameDraw = TRUE;
		UBOOL bCreationSuccessful = CreateWindow(InParentWindowHandle, InWindowTitle, InWPFXamlFileName, InPositionX, InPositionY, InWidth, InHeight, InFakeTitleBarHeight, bShowInTaskBar, bAllowFrameDraw, bInIsTopmost);
		FinalizeLayout(bCenterWindow);

		return bCreationSuccessful;
	}

	/**
	 * Initialize the window & Finalizes the layout
	 *
	 * @param	InParentWindowHandle	Parent window handle
	 * @param	InWindowTitle			Window title
	 * @param	InWPFXamlFileName		Filename of the WPF .xaml file
	 * @param	InPositionX				X position of window (-1 for default)
	 * @param	InPositionY				Y position of window (-1 for default)
	 * @param	bCenterWindow			True to center the window in the app (incoming position is ignored)
	 * @param	InFakeTitleBarHeight	Height of fake title bar (or 0)
	 *
	 * @return	TRUE if successful
	 */
	UBOOL InitWindow( const HWND InParentWindowHandle,
					  String^ InWindowTitle,
					  String^ InWPFXamlFileName,
					  int InPositionX,
					  int InPositionY,
					  bool bCenterWindow,
					  int InFakeTitleBarHeight,
					  int bInIsTopmost)
	{
		return InitWindow( InParentWindowHandle, InWindowTitle, InWPFXamlFileName, InPositionX, InPositionY, 0, 0, bCenterWindow, InFakeTitleBarHeight, bInIsTopmost );
	}

	/** Call to hide or show the window */
	void ShowWindow( bool bShouldShow )
	{
		const HWND WPFWindowHandle = GetWindowHandle();

		// Show the window!
		::ShowWindow( WPFWindowHandle, bShouldShow ? SW_SHOW : SW_HIDE );
	}

	/** Call to enable/disable the window */
	void EnableWindow( bool bEnable )
	{
		const HWND WPFWindowHandle = GetWindowHandle();

		::EnableWindow(WPFWindowHandle, bEnable ? TRUE : FALSE);
	}

	/** Returns the Win32 handle for this window */
	HWND GetWindowHandle()
	{
		return ( HWND )InteropWindow->Handle.ToPointer();
	}


	/** Returns the root visual of this window */
	System::Windows::Media::Visual^ GetRootVisual()
	{
		check( InteropWindow.get() != nullptr );
		return InteropWindow->RootVisual;
	}



protected:

	/**
	 * Creates the window but does not finalize the layout
	 *
	 * @param	InParentWindowHandle	Parent window handle
	 * @param	InWindowTitle			Window title
	 * @param	InWPFXamlFileName		Filename of the WPF .xaml file
	 * @param	InPositionX				X position of window (-1 for default)
	 * @param	InPositionY				Y position of window (-1 for default)
	 * @param	InWidth					Width of window (0 for default)
	 * @param	InHeight				Height of window (0 for default)
	 * @param	InFakeTitleBarHeight	Height of fake title bar (or 0)
	 *
	 * @return	TRUE if successful
	 */
	UBOOL CreateWindow( const HWND InParentWindowHandle,
					  String^ InWindowTitle,
					  String^ InWPFXamlFileName,
					  int InPositionX,
					  int InPositionY,
					  int InWidth, 
					  int InHeight,
					  int InFakeTitleBarHeight,
					  int InShowInTaskBar,
					  int InAllowFrameDraw,
					  int bInIsTopMost)
	{
		FakeTitleBarHeight = InFakeTitleBarHeight;


		String^ WPFXamlPathAndFileName =
			String::Format( "{0}WPF\\Controls\\{1}", CLRTools::ToString( GetEditorResourcesDir() ), InWPFXamlFileName );


		// NOTE: We'll start off invisible and display the window after the WPF elements are laid out

		int WindowStyle = 0;
		if (InParentWindowHandle) {
			WindowStyle |= WS_CHILD;
		}
		if(( InFakeTitleBarHeight > 0 ) || (!InAllowFrameDraw))
		{
		//	WS_POPUP | WS_BORDER;			// Fixed size window, no caption
		//	WS_POPUP;						// Fixed size window, no caption, no border
			WindowStyle |= WS_POPUP;
		}
		else
		{
		//	WS_OVERLAPPEDWINDOW;			// Resiable window with caption, system menu and close box
		//	WS_THICKFRAME | WS_BORDER;		// Resizable window with caption
		//	WS_DLGFRAME | WS_BORDER;		// Fixed size window with caption (same as WS_CAPTION)
		//	WS_OVERLAPPEDWINDOW;	// WS_POPUPWINDOW | WS_VISIBLE | WS_CHILD | WS_SIZEBOX
			WindowStyle |= WS_DLGFRAME | WS_BORDER;
		}

		
		int ExtendedWindowStyle = 0;
		if (InShowInTaskBar)
		{
			ExtendedWindowStyle |= WS_EX_APPWINDOW;
		}
		else
		{
			ExtendedWindowStyle |= WS_EX_TOOLWINDOW;
		}

		if (bInIsTopMost)
		{
			ExtendedWindowStyle |= WS_EX_TOPMOST;
		}
		//	WS_EX_TOPMOST;					// Makes sure the window is on top of other windows
		//	WS_EX_DLGMODALFRAME;			// Enables modal dialog frame
		//	WS_EX_TRANSPARENT;				// Enables back to front painting


		// @todo WPF: Per-pixel effects outside of the window region cause AIRSPACE contention with DirectX
		const int WindowClassStyle =
			0;
		//	CS_DROPSHADOW;					// Enables funky Windows XP drop shadow on the window


		const UBOOL bUsesPerPixelOpacity = FALSE;

		int WindowXPos = InPositionX;
		int WindowYPos = InPositionY;
		int WindowWidth = InWidth;
		int WindowHeight = InHeight;

		const bool bSizeToContent = (WindowHeight==0 || WindowWidth==0);

		if( InPositionX == -1 )
		{
			WindowXPos = CW_USEDEFAULT;
		}
		if( InPositionY == -1 )
		{
			WindowYPos = CW_USEDEFAULT;
		}


		// Create the actual window!
		InteropWindow.reset(
			CLRTools::CreateWPFWindowFromXaml(
				InParentWindowHandle,					// Parent window handle
				WPFXamlPathAndFileName,					// WPF .xaml file
				InWindowTitle,							// Window title
				WindowXPos,								// X
				WindowYPos,								// Y
				WindowWidth,							// Width
				WindowHeight,							// Height
				WindowStyle,							// Style
				ExtendedWindowStyle,					// Extended window style
				WindowClassStyle,						// Window class style
				bUsesPerPixelOpacity,					// Use per-pixel opacity?
				bSizeToContent ) );						// Size to content?

		return TRUE;
	}

	/**
	 * Finalizes the layout
	 *
	 * @param	bCenterWindow			True to center the window in the app (incoming position is ignored)
	 *
	 */
	void FinalizeLayout( bool bCenterWindow )
	{
		// Attach our message hook routine.  This is only for catching messages that do not
		// have WPF equivalents.  The WPF messages can be handled through the HwndSource
		// object itself!
		InteropWindow->AddHook(
			gcnew Interop::HwndSourceHook( this, &MWPFWindowWrapper::StaticMessageHookFunction ) );


		// Setup root visual event handlers
		Visual^ MyVisual = InteropWindow->RootVisual;
		FrameworkElement^ MyFrameworkElement = safe_cast< FrameworkElement^ >( MyVisual );

		// Attach event handler for CloseButton Click
		Button^ CloseButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( MyVisual, "CloseButton" ) );
		if( CloseButton != nullptr )
		{
			CloseButton->Click +=
				gcnew RoutedEventHandler( this, &MWPFWindowWrapper::Callback_CloseButtonClicked );
		}

		if( 0 )
		{
			// Localize the window's contents
			CLRTools::LocalizeContentRecursively( MyFrameworkElement );
		}

		// Make sure the window layout is finalized before we show the window.  The localization
		// process may have caused content dimensions to changes, etc.
		MyFrameworkElement->UpdateLayout();

		const HWND WPFWindowHandle = ( HWND )InteropWindow->Handle.ToPointer();

		// Now that the window's size is final, we'll set the position of the window
		if( bCenterWindow )
		{
			// Grab the size of our window
			RECT WindowRect;
			::GetWindowRect( WPFWindowHandle, &WindowRect );

			INT WindowWidth = WindowRect.right - WindowRect.left;
			INT WindowHeight = WindowRect.bottom - WindowRect.top;

			// Grab the position and size of the main editor window
			wxRect FrameWindowRect;
			if (GApp && GApp->EditorFrame)
			{
				FrameWindowRect = GApp->EditorFrame->GetRect();
			}
			else
			{
				::GetClientRect(::GetDesktopWindow(), &WindowRect);
				FrameWindowRect = wxRect(WindowRect.left, WindowRect.top, WindowRect.right-WindowRect.left, WindowRect.bottom-WindowRect.top);
			}


			// Center the window within it's parent
			INT WindowXPos =
				FrameWindowRect.GetX() +
				( FrameWindowRect.GetWidth() - WindowWidth ) / 2;
			INT WindowYPos =
				FrameWindowRect.GetY() +
				( FrameWindowRect.GetHeight() - WindowHeight ) / 2;


			// Apply the window position change
			::SetWindowPos(
				WPFWindowHandle,
				0,
				WindowXPos,
				WindowYPos,
				0,
				0,
				SWP_NOSIZE | SWP_NOZORDER );
		}
	}

	/** Called when the HwndSource receives a windows message */
	IntPtr StaticMessageHookFunction( IntPtr HWnd, int Msg, IntPtr WParam, IntPtr LParam, bool% OutHandled )
	{
		return VirtualMessageHookFunction(HWnd, Msg, WParam, LParam, OutHandled);
	}

	virtual IntPtr VirtualMessageHookFunction( IntPtr HWnd, int Msg, IntPtr WParam, IntPtr LParam, bool% OutHandled )
	{
		OutHandled = false;

		int RetVal = 0;
		
		if( Msg == WM_CLOSE )
		{
			DisposeOfInteropWindow();
			OutHandled = true;
		}


		if( Msg == WM_GETDLGCODE )
		{
			// This tells Windows (and Wx) that we'll need keyboard events for this control
			RetVal = DLGC_WANTALLKEYS;
			OutHandled = true;
		}
 

		// Tablet PC software (e.g. Table PC Input Panel) sends WM_GETOBJECT events which
		// trigger a massive performance bug in WPF (http://wpf.codeplex.com/Thread/View.aspx?ThreadId=41964)
		// We'll intercept this message and skip it.
		if( Msg == WM_GETOBJECT )
		{
			RetVal = 0;
			OutHandled = true;
		}


		if( FakeTitleBarHeight > 0 && Msg == WM_NCHITTEST )
		{
			// If the left mouse button is down, override the client area detection.  We only do this
			// while the mouse button is down so that our window will still receive mouse move messages
			// and other events for this region of the window, *until* the user clicks (to move the window)
			// Second check is for Left handed mice.
			if( ( ( GetAsyncKeyState( VK_LBUTTON ) & 0x8000 ) != 0 && !GetSystemMetrics(SM_SWAPBUTTON) ) ||
				( ( GetAsyncKeyState( VK_RBUTTON ) & 0x8000 ) != 0 && GetSystemMetrics(SM_SWAPBUTTON) ) )
			{
				const HWND NativeHWnd = ( HWND )( PTRINT )HWnd;
				const LPARAM NativeWParam = ( WPARAM )( PTRINT )WParam;
				const LPARAM NativeLParam = ( WPARAM )( PTRINT )LParam;

				// Let Windows perform the true hit test
				RetVal = DefWindowProc( NativeHWnd, Msg, NativeWParam, NativeLParam );
				OutHandled = true;

		
				// Did the user click in the client area?
				if( RetVal == HTCLIENT )
				{
					// Grab the size of our window
					RECT WindowRect;
					::GetWindowRect( NativeHWnd, &WindowRect );

					int CursorXPos = GET_X_LPARAM( NativeLParam );
					int CursorYPos = GET_Y_LPARAM( NativeLParam );

					if( CursorXPos >= WindowRect.left && CursorXPos < WindowRect.right - FakeTitleBarButtonWidth &&
						CursorYPos >= WindowRect.top && CursorYPos < WindowRect.top + FakeTitleBarHeight )
					{
						// Trick Windows into thinking the user interacted with the caption
						RetVal = HTCAPTION;				
					}
				}
			}

		}

		if( (Msg == WM_KEYDOWN) || (Msg == WM_SYSKEYDOWN ) )
		{// Search for global key presses
			const LPARAM NativeWParam = ( WPARAM )( PTRINT )WParam;
			UINT KeyCode = NativeWParam;

			UBOOL bCtrlPressed = (	(System::Windows::Input::Keyboard::IsKeyDown(System::Windows::Input::Key::RightCtrl)) ||	
				(System::Windows::Input::Keyboard::IsKeyDown(System::Windows::Input::Key::LeftCtrl)) );

			UBOOL bShiftPressed = ( (System::Windows::Input::Keyboard::IsKeyDown(System::Windows::Input::Key::RightShift)) ||	
				(System::Windows::Input::Keyboard::IsKeyDown(System::Windows::Input::Key::LeftShift)) );

			UBOOL bAltPressed = ( (System::Windows::Input::Keyboard::IsKeyDown(System::Windows::Input::Key::RightAlt)) ||	
				(System::Windows::Input::Keyboard::IsKeyDown(System::Windows::Input::Key::LeftAlt)) );

			//assume a hot key has been pressed
			UBOOL bHotKeyPressed = TRUE;
			FName KeyName;

			// Map the keycode to a name that GApp can recognize
			switch( KeyCode )
			{
				case 9:// Tab == 9
				{
					KeyName = FName(TEXT("Tab"));
					break;
				}
				case 70:// F == 70
					{
						KeyName = FName(TEXT("F"));
						break;
					}
				case 122:// F11 == 122
					{
						KeyName = FName(TEXT("F11"));
						break;
					}
				case 13:// Enter == 13
					{
						KeyName = FName(TEXT("Enter"));
						break;
					}
				default:
					bHotKeyPressed = FALSE;
					break;
			}

			if( bHotKeyPressed )
			{// Process global hotkey
				GApp->CheckIfGlobalHotkey( KeyName, bCtrlPressed, bShiftPressed, bAltPressed );
			}
		}

		return (IntPtr)RetVal;
	}




	/** Called when the Close button is clicked */
	void Callback_CloseButtonClicked( Object^ Sender, RoutedEventArgs^ Args )
	{
		// Close button was clicked
		Button^ CloseButton = safe_cast< Button^ >( Sender );

		// Kill the window
		DisposeOfInteropWindow();
	}




protected:

	/** If greater than zero, the height in pixels of the user-rendered title bar */
	int FakeTitleBarHeight;
	/** If greater than zero, the width of the title bar "moveable" area is that much smaller.*/
	int FakeTitleBarButtonWidth;

	/** WPF interop wrapper around the HWND and WPF controls */
	auto_handle< Interop::HwndSource > InteropWindow;

};



#endif	// __WPFWindowWrapperCLR_h__