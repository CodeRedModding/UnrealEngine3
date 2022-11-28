/*=============================================================================
	FOutputDeviceAnsiError.h: Ansi stdout error output device.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

//
// ANSI stdout output device.
//
class FOutputDeviceAnsiError : public FOutputDeviceError
{
	INT ErrorPos;
	EName ErrorType;
	void LocalPrint( const TCHAR* Str )
	{
#if UNICODE
		wprintf(TEXT("%s"),Str);
#else
		printf(TEXT("%s"),Str);
#endif
	}
public:
	FOutputDeviceAnsiError()
	: ErrorPos(0)
	, ErrorType(NAME_None)
	{}
	void Serialize( const TCHAR* Msg, enum EName Event )
	{

#if _DEBUG
		if( appIsDebuggerPresent() )
		{	
			// Just display info and break the debugger.
  			debugf( NAME_Critical, TEXT("appError called while debugging:") );
			debugf( NAME_Critical, Msg );
			debugf( NAME_Critical, TEXT("Breaking debugger") );
			UObject::StaticShutdownAfterError();
			appDebugBreak();
		}
		else
		{
			// Display the error and exit.
  			LocalPrint( TEXT("\nappError called: \n") );
			LocalPrint( Msg );
  			LocalPrint( TEXT("\n") );
			if( !GIsCriticalError )
			{
				GIsCriticalError = 1;
				UObject::StaticShutdownAfterError();
				appExit();
			}
			appRequestExit( 1 );
		}
#else
		if( !GIsCriticalError )
		{
			// First appError.
			GIsCriticalError = 1;
			ErrorType        = Event;
			debugf( NAME_Critical, TEXT("appError called:") );
			debugf( NAME_Critical, Msg );

			// Shut down.
			UObject::StaticShutdownAfterError();
			appStrncpy( GErrorHist, Msg, ARRAY_COUNT(GErrorHist) );
			appStrncat( GErrorHist, TEXT("\r\n\r\n"), ARRAY_COUNT(GErrorHist) );
			ErrorPos = appStrlen(GErrorHist);
			if( GIsGuarded )
			{
				appStrncat( GErrorHist, *LocalizeError("History",TEXT("Core")), ARRAY_COUNT(GErrorHist) );
				appStrncat( GErrorHist, TEXT(": "), ARRAY_COUNT(GErrorHist) );
			}
			else
			{
				HandleError();
			}
		}
		else debugf( NAME_Critical, TEXT("Error reentered: %s"), Msg );

#ifdef XBOX
		GMalloc->DumpAllocs();
#endif

		// Propagate the error or exit.
		if( GIsGuarded )
			throw( 1 );
		else
			appRequestExit( 1 );
#endif
	}
	void HandleError()
	{
		try
		{
			GIsGuarded			= 0;
			GIsRunning			= 0;
			GIsCriticalError	= 1;
			GLogConsole			= NULL;
			UObject::StaticShutdownAfterError();
			GErrorHist[ErrorType==NAME_FriendlyError ? ErrorPos : ARRAY_COUNT(GErrorHist)-1]=0;
			LocalPrint( GErrorHist );
			LocalPrint( TEXT("\n\nExiting due to error\n") );
		}
		catch( ... )
		{}
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

