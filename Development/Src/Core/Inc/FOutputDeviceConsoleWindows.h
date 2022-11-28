/*=============================================================================
	FOutputDeviceConsoleWindows.h: Windows console text output device.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

/**
 * Windows implementation of console log window, utilizing the Win32 console API
 */
class FOutputDeviceConsoleWindows : public FOutputDeviceConsole
{
private:
	HANDLE ConsoleHandle;

	/**
	 * Handler called for console events like closure, CTRL-C, ...
	 *
	 * @param Type	unused
	 */
	static BOOL WINAPI ConsoleCtrlHandler( DWORD /*Type*/ )
	{
		if( !GIsRequestingExit )
		{
			PostQuitMessage( 0 );
			GIsRequestingExit = 1;
		}
		else
		{
			ExitProcess(0);
		}
		return TRUE;
	}
public:

	/** 
	 * Constructor, setting console control handler.
	 */
	FOutputDeviceConsoleWindows()
	{
		// Set console control handler so we can exit if requested.
		SetConsoleCtrlHandler( ConsoleCtrlHandler, TRUE );
	}

	/**
	 * Shows or hides the console window. 
	 *
	 * @param ShowWindow	Whether to show (TRUE) or hide (FALSE) the console window.
	 */
	virtual void Show( UBOOL ShowWindow )
	{
		if( ShowWindow )
		{
			if( !ConsoleHandle )
			{
				AllocConsole();
				ConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

				if( ConsoleHandle )
				{
					COORD Size;
					Size.X = 160;
					Size.Y = 4000;
					SetConsoleScreenBufferSize( ConsoleHandle, Size );

					HWND ConsoleWindow = GetConsoleWindow();
					LONG Style;

					//@todo static linking: this currently fails with "access denied" (error 5)
					Style = GetClassLong( ConsoleWindow, GCL_STYLE );
					if( SetClassLong( ConsoleWindow, GCL_STYLE, Style | CS_NOCLOSE ) == 0 )
					Style = GetWindowLong( ConsoleWindow, GWL_STYLE );
					SetWindowLong( ConsoleWindow, GWL_STYLE, Style & ~WS_SYSMENU );
				}
			}
		}
		else if( ConsoleHandle )
		{
			ConsoleHandle = NULL;
			FreeConsole();
		}
	}

	/** 
	 * Returns whether console is currently shown or not
	 *
	 * @return TRUE if console is shown, FALSE otherwise
	 */
	virtual UBOOL IsShown()
	{
		return ConsoleHandle != NULL;
	}

	/**
	 * Displays text on the console and scrolls if necessary.
	 *
	 * @param Data	Text to display
	 * @param Event	Event type, used for filtering/ suppression
	 */
	void Serialize( const TCHAR* Data, enum EName Event )
	{
		if( ConsoleHandle )
		{
			static UBOOL Entry=0;
			if( !GIsCriticalError || Entry )
			{
				if( !FName::SafeSuppressed(Event) )
				{
					if( Event == NAME_Title )
					{
						SetConsoleTitle( Data );
					}
					else
					{
						TCHAR OutputString[1024]; //@warning: this is safe as appSprintf only use 1024 characters max
						if( Event == NAME_None )
						{
							appSprintf(OutputString,TEXT("%s%s"),Data,LINE_TERMINATOR);
						}
						else
						{
							appSprintf(OutputString,TEXT("%s: %s%s"),FName::SafeString(Event),Data,LINE_TERMINATOR);
						}

						CONSOLE_SCREEN_BUFFER_INFO	ConsoleInfo; 
						SMALL_RECT					WindowRect; 

						if( GetConsoleScreenBufferInfo( ConsoleHandle, &ConsoleInfo) && ConsoleInfo.srWindow.Top > 0 ) 
						{ 
							WindowRect.Top		= -1;	// Move top up by one row 
							WindowRect.Bottom	= -1;	// Move bottom up by one row 
							WindowRect.Left		= 0;	// No change 
							WindowRect.Right	= 0;	// No change 

							SetConsoleWindowInfo( ConsoleHandle, FALSE, &WindowRect );
						}
	  
						DWORD Written;
						WriteConsole( ConsoleHandle, OutputString, appStrlen(OutputString), &Written, NULL );
					}
				}
			}
			else
			{
				Entry=1;
				try
				{
					// Ignore errors to prevent infinite-recursive exception reporting.
					Serialize( Data, Event );
				}
				catch( ... )
				{}
				Entry=0;
			}
		}
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

