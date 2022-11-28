/*=============================================================================
	FOutputDeviceFile.h: ANSI file output device.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

//
// ANSI file output device.
//
class FOutputDeviceFile : public FOutputDevice
{
public:
	FOutputDeviceFile()
	: LogAr( NULL )
	, Opened( 0 )
	, Dead( 0 )
	{
		Filename[0]		= 0;
		DebuggerPresent = appIsDebuggerPresent();
	}
	/**
	 * Closes output device and cleans up. This can't happen in the destructor
	 * as we have to call "delete" which cannot be done for static/ global
	 * objects.
	 */
	void TearDown()
	{
		if( LogAr )
		{
			Logf( NAME_Log, TEXT("Log file closed, %s"), appTimestamp() );
			delete LogAr;
			LogAr = NULL;
		}
	}
	void Flush()
	{
		if( LogAr )
			LogAr->Flush();
	}
	void Serialize( const TCHAR* Data, enum EName Event )
	{
		static UBOOL Entry=0;
		if( !GIsCriticalError || Entry )
		{
			if( !FName::SafeSuppressed(Event) )
			{
				if( !LogAr && !Dead )
				{
					// Make log filename.
					if( !Filename[0] )
					{
						// The Editor requires a fully qualified directory to not end up putting the log in various directories.
						appStrcpy( Filename, appBaseDir() );
						appStrcat( Filename, *appGameLogDir() );

						if(	!Parse(appCmdLine(), TEXT("LOG="), Filename+appStrlen(Filename), ARRAY_COUNT(Filename)-appStrlen(Filename) )
						&&	!Parse(appCmdLine(), TEXT("ABSLOG="), Filename, ARRAY_COUNT(Filename) ) )
						{
							appStrcat( Filename, GPackage );
							appStrcat( Filename, TEXT(".log") );
						}
					}

					// Open log file.
					LogAr = GFileManager->CreateFileWriter( Filename, FILEWRITE_AllowRead|FILEWRITE_Unbuffered|(Opened?FILEWRITE_Append:0));
					if( LogAr )
					{
						Opened = 1;
#if UNICODE && !FORCE_ANSI_LOG
                        #if __LINUX__
                        if (GUnicodeOS) {
                        #endif
						_WORD UnicodeBOM = UNICODE_BOM;
						LogAr->Serialize( &UnicodeBOM, 2 );
                        #if __LINUX__
                        }
                        #endif
#endif
						Logf( NAME_Log, TEXT("Log file open, %s"), appTimestamp() );
					}
					else Dead = 1;
				}
				if( LogAr && Event!=NAME_Title )
				{
#if ((FORCE_ANSI_LOG && UNICODE) || (__LINUX__))
                    #if __LINUX__
                    if (!GUnicodeOS) {
                    #endif

					TCHAR Ch[65536] = TEXT("");
					ANSICHAR ACh[65536];
					appStrcat(Ch,FName::SafeString(Event));
					appStrcat(Ch,TEXT(": "));
					appStrcat(Ch,Data);
					appStrcat(Ch,LINE_TERMINATOR);
					INT i;
					for( i=0; Ch[i]; i++ )
						ACh[i] = ToAnsi(Ch[i] );
					ACh[i] = 0;
					LogAr->Serialize( ACh, i );

                    #if __LINUX__
                    } else {
					WriteRaw( FName::SafeString(Event) );
					WriteRaw( TEXT(": ") );
					WriteRaw( Data );
					WriteRaw( LINE_TERMINATOR );
                    }
                    #endif
#else
					WriteRaw( FName::SafeString(Event) );
					WriteRaw( TEXT(": ") );
					WriteRaw( Data );
					WriteRaw( LINE_TERMINATOR );
#endif
#if _MSC_VER
					if( DebuggerPresent )
					{
						TCHAR Temp[1024];
						appSprintf( Temp, TEXT("%s: %s%s"), FName::SafeString(Event), Data, LINE_TERMINATOR );
						OutputDebugString(Temp);
					}
#endif
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
	FArchive* LogAr;
	TCHAR Filename[1024];
private:
	UBOOL Opened, Dead, DebuggerPresent;
	void WriteRaw( const TCHAR* C )
	{
		LogAr->Serialize( const_cast<TCHAR*>(C), appStrlen(C)*sizeof(TCHAR) );
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

