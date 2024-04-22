/*=============================================================================
 IPhone.cpp: iPhone main platform glue definitions
 Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#include "Core.h"
#include "FMallocIPhone.h"
#include "FFileManagerIPhone.h"
#include "UnIpDrv.h"
#include "ChartCreation.h"
#include "IPhoneObjCWrapper.h"

#include "IPhoneCrashDefines.h"

#include <execinfo.h>
#include <signal.h>

/** Save off the argv portion of the commandline, so we can re-add it when we reload the UE3CommandLine.txt for networked file loading */
FString GSavedCommandLinePortion;
FString GExecutableName;

void appOutputDebugString( const TCHAR *Message )
{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	printf("%s", TCHAR_TO_UTF8(Message));
#endif
#if WITH_UE3_NETWORKING
	if (GDebugChannel)
	{
		GDebugChannel->SendText(Message);
	}
#endif
}

/** Sends a message to a remote tool. */
void appSendNotificationString( const ANSICHAR *Message )
{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	printf("%s", Message);
#endif
#if WITH_UE3_NETWORKING
	if (GDebugChannel)
	{
		GDebugChannel->SendText(ANSI_TO_TCHAR(Message));
	}
#endif
}

/**
 * Merge the given commandline with GSavedCommandLinePortion, which may start with ?
 * options that need to come after first token
 */
void MergeCommandlineWithSaved(TCHAR CommandLine[16384])
{
	// the saved commandline may be in the format ?opt?opt -opt -opt, so we need to insert it 
	// after the first token on the commandline unless the first token starts with a -, in which 
	// case use it at the start of the command line
	if (CommandLine[0] == '-' || CommandLine[0] == 0)
	{
		// handle the easy - case, just use the saved command line part as the start, in case it
		// started with a ?
		FString CombinedCommandLine = GSavedCommandLinePortion + CommandLine;
		appStrcpy(CommandLine, 16384, *CombinedCommandLine);
	}
	else
	{
		// otherwise, we need to get the first token from the command line and insert after
		TCHAR* Space = appStrchr(CommandLine, ' ');
		if (Space == NULL)
		{
			// if there is only one token (no spaces), just append us after it
			appStrcat(CommandLine, 16384, *GSavedCommandLinePortion);
		}
		else
		{
			// save off what's after the space (include the space for pasting later)
			FString AfterSpace(Space);
			// copy the save part where the space was
			appStrcpy(Space, 16384 - (Space - CommandLine), *GSavedCommandLinePortion);
			// now put back the 2nd and so on token
			appStrcat(CommandLine, 16384, *AfterSpace);
		}
	}
}

/**
 * Rebuild the commandline if needed
 *
 * @param NewCommandLine The commandline to fill out
 *
 * @return TRUE if NewCommandLine should be pushed to GCmdLine
 */
UBOOL appResetCommandLine(TCHAR NewCommandLine[16384])
{
	// we need to switch the GFileManager over to read from the Doc dir. on iPhone, this is known
	// to be okay, but it's really messy.
	// @todo: Come up with a better way to do this, tricky!
	GFileManager->Init(FALSE);

	// load the latest UE3CommandLine.txt
	FString CmdLine;
	appLoadFileToString(CmdLine, *(appGameDir() + TEXT("CookedIPhone\\UE3CommandLine.txt")));

	// put it into the output
	appStrncpy(NewCommandLine, *CmdLine, 16384);

	// merge in the GSavedCommandLinePortion
	MergeCommandlineWithSaved(NewCommandLine);

	return TRUE;
}
		
//
// Return the system time.
//
void appSystemTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec )
{
	// query for calendar time
    struct timeval Time;
    gettimeofday(&Time, NULL);

	// convert it to local time
    struct tm LocalTime;
    localtime_r(&Time.tv_sec, &LocalTime);
	
	// pull out data/time
	Year		= LocalTime.tm_year + 1900;
	Month		= LocalTime.tm_mon + 1;
	DayOfWeek	= LocalTime.tm_wday;
	Day			= LocalTime.tm_mday;
	Hour		= LocalTime.tm_hour;
	Min			= LocalTime.tm_min;
	Sec			= LocalTime.tm_sec;
	MSec		= Time.tv_usec / 1000;
}

//
// Return the UTC time.
//
void appUtcTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec )
{
	// query for calendar time
	struct timeval Time;
	gettimeofday(&Time, NULL);

	// convert it to UTC
	struct tm LocalTime;
	gmtime_r(&Time.tv_sec, &LocalTime);

	// pull out data/time
	Year		= LocalTime.tm_year + 1900;
	Month		= LocalTime.tm_mon + 1;
	DayOfWeek	= LocalTime.tm_wday;
	Day			= LocalTime.tm_mday;
	Hour		= LocalTime.tm_hour;
	Min			= LocalTime.tm_min;
	Sec			= LocalTime.tm_sec;
	MSec		= Time.tv_usec / 1000;
}

/**
 * Does per platform initialization of timing information and returns the current time. This function is
 * called before the execution of main as GStartTime is statically initialized by it. The function also
 * internally sets GSecondsPerCycle, which is safe to do as static initialization order enforces complex
 * initialization after the initial 0 initialization of the value.
 *
 * @return	current time
 */
DOUBLE appInitTiming()
{
	// Time base is in nano seconds.
	mach_timebase_info_data_t Info;
	verify( mach_timebase_info( &Info ) == 0 );
	GSecondsPerCycle = 1e-9 * (DOUBLE) Info.numer / (DOUBLE) Info.denom;
	return appSeconds();
}

/**
 * Wide stricmp replacement
 */
int wgccstrcasecmp(const TCHAR *StrA, const TCHAR *StrB)
{
	// walk the strings, comparing them case insensitively
	for (; *StrA || *StrB; StrA++, StrB++)
	{
		TCHAR A = towupper(*StrA), B = towupper(*StrB);
		if (A != B)
		{
			return A - B;
		}
	}
	return 0;
}

/**
 * Wide strnicmp replacement
 */
int wgccstrncasecmp(const TCHAR *StrA, const TCHAR *StrB, size_t Size)
{
	// walk the strings, comparing them case insensitively, up to a max size
	for (; (*StrA || *StrB) && Size; StrA++, StrB++, Size--)
	{
		TCHAR A = towupper(*StrA), B = towupper(*StrB);
		if (A != B)
		{
			return A - B;
		}
	}
	return 0;
}

/**
 * Replacement for the builtin vswprintf, which doesn't handle actual wide characters
 * in either the format or parameters
 * This was taken from UE2 MacOS code, found here:
 *    //depot/CodeDrops/UE2/UE-2.5-v3369/Core/Src/UnUnix.cpp
 */
INT vswprintf_Replacement( TCHAR *buf, INT max, const TCHAR *fmt, va_list args )
{
	if (fmt == NULL)
	{
		if ((max > 0) && (buf != NULL))
			*buf = 0;
		return(0);
	}

	TCHAR *src = (TCHAR *) appAlloca((wcslen(fmt) + 1) * sizeof (TCHAR));
	wcscpy(src, fmt);

	TCHAR *dst = buf;
	TCHAR *enddst = dst + (max - 1);

	//printf("printf by-hand formatting "); unicode_str_to_stdout(src);

	while ((*src) && (dst < enddst))
	{
		if (*src != '%')
		{
			*dst = *src;
			dst++;
			src++;
			continue;
		}

		TCHAR *percent_ptr = src;
		INT fieldlen = 0;
		INT precisionlen = -1;

		src++; // skip the '%' char...

		while (*src == ' ')
		{
			*dst = ' ';
			dst++;
			src++;
		}

		// check for field width requests...
		if ((*src == '-') || ((*src >= '0') && (*src <= '9')))
		{
			TCHAR *ptr = src + 1;
			while ((*ptr >= '0') && (*ptr <= '9'))
				ptr++;

			TCHAR ch = *ptr;
			*ptr = '\0';
			fieldlen = atoi(TCHAR_TO_ANSI(src));
			*ptr = ch;

			src = ptr;
		}

		if (*src == '.')
		{
			TCHAR *ptr = src + 1;
			while ((*ptr >= '0') && (*ptr <= '9'))
				ptr++;

			TCHAR ch = *ptr;
			*ptr = '\0';
			precisionlen = atoi(TCHAR_TO_ANSI(src + 1));
			*ptr = ch;
			src = ptr;
		}

		// Check for 'ls' field, change to 's'
		if ((src[0] == 'l' && src[1] == 's'))
		{
			src++;
		}

		switch (*src)
		{
		case '%':
			{
				src++;
				*dst = '%';
				dst++;
			}
			break;

		case 'c':
			{
				TCHAR val = (TCHAR) va_arg(args, int);
				src++;
				*dst = val;
				dst++;
			}
			break;

		case 'd':
		case 'i':
		case 'X':
		case 'x':
		case 'u':
		case 'p':
			{
				src++;
				int val = va_arg(args, int);
				ANSICHAR ansinum[30];
				ANSICHAR fmtbuf[30];

				// Yes, this is lame.
				INT cpyidx = 0;
				while (percent_ptr < src)
				{
					fmtbuf[cpyidx] = (ANSICHAR) *percent_ptr;
					percent_ptr++;
					cpyidx++;
				}
				fmtbuf[cpyidx] = 0;

				int rc = snprintf(ansinum, sizeof (ansinum), fmtbuf, val);
				if ((dst + rc) > enddst)
					return -1;	// Fail - the app needs to create a larger buffer and try again
				for (int i = 0; i < rc; i++)
				{
					*dst = (TCHAR) ansinum[i];
					dst++;
				}
			}
			break;

		case 'l':
		case 'I':
			{
				if ((src[0] == 'l' && src[1] != 'l') ||
					(src[0] == 'I' && (src[1] != '6' || src[2] != '4')))
				{
					printf("Unknown percent [%lc%lc%lc] in IPhone.cpp::wvsnprintf() [%s]\n.", src[0], src[1], src[2], TCHAR_TO_ANSI(fmt));
					src++;  // skip it, I guess.
					break;
				}

				// Yes, this is lame.
				INT cpyidx = 0;
				QWORD val = va_arg(args, QWORD);
				ANSICHAR ansinum[60];
				ANSICHAR fmtbuf[30];
				if (src[0] == 'l')
				{
					src += 3;
				}
				else
				{
					src += 4;
					strcpy(fmtbuf, "%L");
					percent_ptr += 4;
					cpyidx = 2;
				}

				while (percent_ptr < src)
				{
					fmtbuf[cpyidx] = (ANSICHAR) *percent_ptr;
					percent_ptr++;
					cpyidx++;
				}
				fmtbuf[cpyidx] = 0;

				int rc = snprintf(ansinum, sizeof (ansinum), fmtbuf, val);
				if ((dst + rc) > enddst)
					return -1;	// Fail - the app needs to create a larger buffer and try again
				for (int i = 0; i < rc; i++)
				{
					*dst = (TCHAR) ansinum[i];
					dst++;
				}
			}
			break;

		case 'f':
		case 'e':
		case 'g':
			{
				src++;
				double val = va_arg(args, double);
				ANSICHAR ansinum[30];
				ANSICHAR fmtbuf[30];

				// Yes, this is lame.
				INT cpyidx = 0;
				while (percent_ptr < src)
				{
					fmtbuf[cpyidx] = (ANSICHAR) *percent_ptr;
					percent_ptr++;
					cpyidx++;
				}
				fmtbuf[cpyidx] = 0;

				int rc = snprintf(ansinum, sizeof (ansinum), fmtbuf, val);
				if ((dst + rc) > enddst)
					return -1;	// Fail - the app needs to create a larger buffer and try again
				for (int i = 0; i < rc; i++)
				{
					*dst = (TCHAR) ansinum[i];
					dst++;
				}
			}
			break;

		case 's':
			{
				src++;
				static const TCHAR* Null = TEXT("(null)");
				const TCHAR *val = va_arg(args, TCHAR *);
				if (val == NULL)
					val = Null;

				int rc = wcslen(val);
				int spaces = Max<INT>(Abs(fieldlen) - rc, 0);
				if ((dst + rc + spaces) > enddst)
					return -1;	// Fail - the app needs to create a larger buffer and try again
				if (spaces > 0 && fieldlen > 0)
				{
					for (int i = 0; i < spaces; i++)
					{
						*dst = TEXT(' ');
						dst++;
					}
				}
				for (int i = 0; i < rc; i++)
				{
					*dst = *val;
					dst++;
					val++;
				}
				if (spaces > 0 && fieldlen < 0)
				{
					for (int i = 0; i < spaces; i++)
					{
						*dst = TEXT(' ');
						dst++;
					}
				}
			}
			break;

		default:
			printf("Unknown percent [%%%c] in IPhone.cpp::wvsnprintf().\n", *src);
			src++;  // skip char, I guess.
			break;
		}
	}

	// Check if we were able to finish the entire format string
	// If not, the app needs to create a larger buffer and try again
	if(*src)
		return -1;

	*dst = 0;  // null terminate the new string.
	return(dst - buf);
}

/**
 * Helper function to write formatted output using an argument list
 *
 * @param Dest - destination string buffer
 * @param DestSize - size of destination buffer
 * @param Count - number of characters to write (not including null terminating character)
 * @param Fmt - string to print
 * @param Args - argument list
 * @return number of characters written or -1 if truncated
 */
INT appGetVarArgs( TCHAR* Dest, SIZE_T DestSize, INT Count, const TCHAR*& Fmt, va_list ArgPtr )
{
#if UNICODE
	INT Result = vswprintf_Replacement(Dest, Count, Fmt, ArgPtr);
#else
	INT Result = vsnprintf(Dest,Count,Fmt,ArgPtr);
#endif
	va_end( ArgPtr );
	return Result;
}

/**
 * Helper function to write formatted output using an argument list
 * ASCII version
 *
 * @param Dest - destination string buffer
 * @param DestSize - size of destination buffer
 * @param Count - number of characters to write (not including null terminating character)
 * @param Fmt - string to print
 * @param Args - argument list
 * @return number of characters written or -1 if truncated
 */
INT appGetVarArgsAnsi( ANSICHAR* Dest, SIZE_T DestSize, INT Count, const ANSICHAR*& Fmt, va_list ArgPtr )
{
	INT Result = vsnprintf(Dest,Count,Fmt,ArgPtr);
	va_end( ArgPtr );
	return Result;
}



void appSleep( FLOAT Seconds )
{
	const DOUBLE MilliSeconds = Seconds * 1000.0f;
	if ( MilliSeconds > 0.05 )
	{
		// Note: mach_wait_until() is a little bit more efficient than usleep().
		mach_timebase_info_data_t TimeBaseInfo;
		mach_timebase_info( &TimeBaseInfo );
		DOUBLE MsToAbs = ((DOUBLE)TimeBaseInfo.denom / (DOUBLE)TimeBaseInfo.numer) * 1000000.0;
		QWORD TimeNow = mach_absolute_time();
		QWORD TimeLater = TimeNow + MilliSeconds * MsToAbs;
		mach_wait_until( TimeLater );
	}
	else
	{
		sched_yield();
	}
}

void appSleepInfinite()
{
	// stop this thread forever
	pause();
}

const TCHAR* appBaseDir() 
{
	return TEXT("");
}

const TCHAR* appShaderDir() 
{
	return TEXT("");
}

void appRequestExit( UBOOL bForceExit )
{
	debugf( TEXT("appRequestExit(%i)"), bForceExit );
	if( bForceExit )
	{
		// Abort allows signal handler to know we aborted.
		abort();
	}
	else
	{
		// Tell the platform specific code we want to exit cleanly from the main loop.
		GIsRequestingExit = 1;
	}
}

void appPlatformPreInit()
{
}

void appPlatformInit()
{
	GNumHardwareThreads = IPhoneGetNumCores();
	debugf(TEXT("This device has %d core(s)"), GNumHardwareThreads);
}

void appPlatformPostInit()
{
#if WITH_GAMECENTER
	// start up game center now (it needs GConfig so it can be disabled for UDK users)
	IPhoneStartGameCenter();
#endif
#if STATS && WITH_MOBILE_RHI
	DWORD MaxMemMB = GSystemSettings.MobileMaxMemory * (1024*1024);
	GStatManager.SetAvailableMemory(MCR_Physical, MaxMemMB);
	debugf(TEXT("This device has %d max memory"), MaxMemMB);
#endif 
	// enable high resolution timing if possible on this device
#if SUPPORTS_HIGH_PRECISION_THREAD_TIMING && WITH_MOBILE_RHI
	GIsHighPrecisionThreadingEnabled = GSystemSettings.bMobileUsingHighResolutionTiming;
#endif

	debugf(TEXT("High resolution timing is %s"), GIsHighPrecisionThreadingEnabled ? TEXT("Enabled") : TEXT("Disabled"));

}

void appHandleIOFailure( const TCHAR* ) 
{
}

/** Returns the language setting that is configured for the platform */
FString appGetLanguageExt(void)
{
	static FString UserLanguage;
	if (UserLanguage.Len() == 0)
	{
		// ask Obj-C lang for the user's language
		ANSICHAR ObjCLanguage[128];
		IPhoneGetUserLanguage(ObjCLanguage, ARRAY_COUNT(ObjCLanguage));

		// lookup table for two letter code returned by iOS to UE3 extension
		static const ANSICHAR* LanguageRemap[] = 
		{
			"en", "INT",
			"ja", "JPN",
			"de", "DEU",
			"fr", "FRA",
			"es", "ESN",
			"??", "ESM",
			"it", "ITA",
			"ko", "KOR",
			"zh", "CHN",
			"ru", "RUS",
			"pl", "POL",
			"hu", "HUN",
			"cs", "CZE",
			"sk", "SLO",
			"sv", "SWE",
			"nl", "DUT",
			"pt_PT", "POR",
			"pt_BR", "BRA",
			"pt", "POR",			// Catch all for any other variation of portuguese
		};

		// now translate the Apple language into a UE3 language
		for (INT LookupIndex = 0; LookupIndex < ARRAY_COUNT(LanguageRemap); LookupIndex += 2)
		{
			// if we match the first column, use the second column string
			if (strncmp(LanguageRemap[LookupIndex], ObjCLanguage, strlen(LanguageRemap[LookupIndex])) == 0)
			{
				UserLanguage = LanguageRemap[LookupIndex + 1];
				break;
			}
		}

		// default to INT if the language wasn't found
		if (UserLanguage.Len() == 0)
		{
			UserLanguage = TEXT("INT");
		}

		// Allow for overriding the language settings via the commandline
		FString CmdLineLang;
		if (Parse(appCmdLine(), TEXT("LANGUAGE="), CmdLineLang))
		{
			debugf(NAME_Log, TEXT("Overriding lang %s w/ command-line option of %s"), *UserLanguage, *CmdLineLang);
			UserLanguage = CmdLineLang;
		}
		UserLanguage = UserLanguage.ToUpper();

		// make sure the language is one that is known (GKnownLanguages)
		if (appIsKnownLanguageExt(UserLanguage) == FALSE)
		{
			// default back to INT if the language wasn't known
			debugf(NAME_Warning, TEXT("Unknown language extension %s. Defaulting to INT"), *UserLanguage);
			UserLanguage = TEXT("INT");
		}
	}

	return UserLanguage;
}


// Create a new globally unique identifier.
FGuid appCreateGuid()
{
	INT Year=0, Month=0, DayOfWeek=0, Day=0, Hour=0, Min=0, Sec=0, MSec=0;
	appSystemTime( Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec );
	
	FGuid   GUID;
	GUID.A = Day | (Hour << 16);
	GUID.B = Month | (Sec << 16);
	GUID.C = MSec | (Min << 16);
	GUID.D = Year ^ appCycles();
	return GUID;
}

// Get computer name.
const TCHAR* appComputerName()
{
	static TCHAR CompName[256] = { 0 };

	// get the name of this iphone
	if (CompName[0] == 0)
	{
		ANSICHAR AnsiCompName[ARRAY_COUNT(CompName)];
		gethostname(AnsiCompName, ARRAY_COUNT(CompName));
		appStrcpy(CompName, ANSI_TO_TCHAR(AnsiCompName));
	}

	return CompName;
}

// Get user name.
const TCHAR* appUserName()
{
	return TEXT("iPhoneUser");
}

/**
 * Converts the passed in program counter address to a human readable string and appends it to the passed in one.
 * @warning: The code assumes that HumanReadableString is large enough to contain the information.
 *
 * @param	ProgramCounter			Address to look symbol information up for
 * @param	HumanReadableString		String to concatenate information with
 * @param	HumanReadableStringSize size of string in characters
 * @param	VerbosityFlags			Bit field of requested data for output. -1 for all output.
 */ 
void appProgramCounterToHumanReadableString( QWORD ProgramCounter, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, EVerbosityFlags VerbosityFlags )
{
}

/**
 * Capture a stack backtrace and optionally use the passed in exception pointers.
 *
 * @param	BackTrace			[out] Pointer to array to take backtrace
 * @param	MaxDepth			Entries in BackTrace array
 * @return	Number of function pointers captured
 */
DWORD appCaptureStackBackTrace( QWORD* BackTrace, DWORD MaxDepth )
{
	void* OsBackTrace[128];
	MaxDepth = Min( ARRAY_COUNT(OsBackTrace), MaxDepth );
	INT CapturedDepth = backtrace(OsBackTrace, MaxDepth);

	// Convert 32 bit pointers 64 bit QWORD
	for( INT i = 0; i < MaxDepth; i++ )
	{
		BackTrace[i] = i < CapturedDepth ? (QWORD) OsBackTrace[i] : 0;
	}

	return CapturedDepth;
}

/**
 *	Capture (and display) the call stack w/ the given error message
 *
 *	@param	InErrorMessage		The error message to display
 *	@param	InSleepTime			The time to sleep the app (0.0 = do not sleep)
 *	@param	InExitCode			The exit code to exit with (0 = do not exit)
 */
extern void ClearSignalHandling();
void appCaptureCrashCallStack(const TCHAR* InErrorMessage, FLOAT InSleepTime, INT InExitCode)
{
	static UBOOL s_bCapturingCallstack = FALSE;

	if (s_bCapturingCallstack == FALSE)
	{
		s_bCapturingCallstack = TRUE;

		FString CallStackCrash;
		FString CallStackCrashMsg;
		CallStackCrash += FString::Printf(iPhoneSysCrashString, InErrorMessage);
		CallStackCrash += TEXT("\n");
		CallStackCrash += FString::Printf(iPhoneSysGameString, GGameName);
		CallStackCrash += TEXT("\n");
		CallStackCrash += FString::Printf(iPhoneSysEngineVersionString, GEngineVersion);
		CallStackCrash += TEXT("\n");
		CallStackCrash += FString::Printf(iPhoneSysChangelistString, GBuiltFromChangeList);
		CallStackCrash += TEXT("\n");
		CallStackCrash += FString::Printf(iPhoneSysConfigurationString, 
#if _DEBUG
			TEXT("Debug")
#elif FINAL_RELEASE_DEBUGCONSOLE
			TEXT("ShippingDebugConsole")
#elif FINAL_RELEASE
			TEXT("Shipping")
#else
			TEXT("Release")
#endif
			);
		CallStackCrash += TEXT("\n");
		CallStackCrashMsg = CallStackCrash;

		int FrameCount = 0;
		void* Frames[128];

		// Frame collection
		FrameCount = backtrace(Frames, 128);
		CallStackCrash += FString::Printf(iPhoneSysCallstackStartString, FrameCount);
		CallStackCrash += TEXT("\n");
		for (int backtraceIdx = 0; backtraceIdx < FrameCount; backtraceIdx++)
		{
			CallStackCrash += FString::Printf(iPhoneSysCallstackEntryString, Frames[backtraceIdx]);
			CallStackCrash += TEXT("\n");
		}
		CallStackCrash += FString::Printf(iPhoneSysCallstackEndString);
		CallStackCrash += TEXT("\n");
		CallStackCrash += FString::Printf(iPhoneSysEndCrashString);
		CallStackCrash += TEXT("\n");
		appOutputDebugString(*CallStackCrash);

#if WITH_UE3_NETWORKING
		// Sleep to allow the debug channel to flush to UnrealConsole...
		if (GDebugChannel != NULL)
		{
			GDebugChannel->Tick();
		}
#endif

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
		{
			appSleep(2.5f);
			char** symbols = backtrace_symbols(Frames, FrameCount);
			CallStackCrashMsg += TEXT("CallStack:\n");
			for (int backtraceIdx = 0; backtraceIdx < FrameCount; backtraceIdx++)
			{
				CallStackCrashMsg += FString::Printf(TEXT("\t%s\n"), ANSI_TO_TCHAR(symbols[backtraceIdx]));
			}
			GFullScreenMovie->GameThreadStopMovie(0, FALSE, TRUE);
			appMsgf(AMT_Crash, *CallStackCrashMsg);
		}
#endif

		if (InSleepTime > 0.0f)
		{
			appSleep(InSleepTime);
		}
		s_bCapturingCallstack = FALSE;

		if (InExitCode != 0)
		{
			ClearSignalHandling();
			raise(SIGABRT);
		}
	}
}

/**
 * Returns the number of modules loaded by the currently running process.
 */
INT appGetProcessModuleCount()
{
	return 0;
}

/**
 * Gets the signature for every module loaded by the currently running process.
 *
 * @param	ModuleSignatures		An array to retrieve the module signatures.
 * @param	ModuleSignaturesSize	The size of the array pointed to by ModuleSignatures.
 *
 * @return	The number of modules copied into ModuleSignatures
 */
INT appGetProcessModuleSignatures(FModuleInfo *ModuleSignatures, const INT ModuleSignaturesSize)
{
	return 0;
}

void appBeginNamedEvent(FColor const&, wchar_t const*) 
{
}

void appEndNamedEvent() 
{
}

void appCleanFileCache()
{
	GSys->PerformPeriodicCacheCleanup();
}

void appUpdateMemoryChartStats(FMemoryChartEntry& MemoryEntry)
{
#if DO_CHARTING
	// track what memory info we hav

	uint64_t FreeMem, UsedMem;
	IPhoneGetPhysicalMemoryInfo( FreeMem, UsedMem );
	MemoryEntry.PhysicalMemUsed = UsedMem;
	MemoryEntry.PhysicalTotal = FreeMem + UsedMem;
#endif // DO_CHARTING
}

/**
 * Return the system settings section name to use for the current running platform
 */
const TCHAR* appGetMobileSystemSettingsSectionName()
{
	// Look for an override on the commandline
	FString OverrideSubName;
	if (Parse(appCmdLine(), TEXT("-SystemSettings="), OverrideSubName))
	{
		// Append it to SystemSettings, unless it starts with SystemSettings
		static TCHAR ReturnedOverrideName[256] = TEXT("SystemSettings");
		static INT SystemSettingsStrLen = appStrlen(ReturnedOverrideName);
		if (appStrnicmp(*OverrideSubName, TEXT("SystemSettings"), SystemSettingsStrLen) == 0)
		{
			appStrcpy(ReturnedOverrideName + SystemSettingsStrLen, ARRAY_COUNT(ReturnedOverrideName), (*OverrideSubName + SystemSettingsStrLen));
		}
		else
		{
			appStrcpy(ReturnedOverrideName + SystemSettingsStrLen, ARRAY_COUNT(ReturnedOverrideName), *OverrideSubName);
		}
		return ReturnedOverrideName;
	}

	static EIOSDevice DeviceType = IPhoneGetDeviceType();
	
	// return the string depending on the device type
	switch (DeviceType)
	{
		case IOS_IPhone3GS:
			return TEXT("SystemSettingsIPhone3GS");
		case IOS_IPhone4:
			return TEXT("SystemSettingsIPhone4");
		case IOS_IPhone4S:
			return TEXT("SystemSettingsIPhone4S");
		case IOS_IPhone5:
			return TEXT("SystemSettingsIPhone5");
		case IOS_IPodTouch4:
			return TEXT("SystemSettingsIPodTouch4");
		case IOS_IPodTouch5:
			return TEXT("SystemSettingsIPodTouch5");
		case IOS_IPad:
			return TEXT("SystemSettingsIPad");
		case IOS_IPad2:
			return TEXT("SystemSettingsIPad2");
		case IOS_IPad3:
			return TEXT("SystemSettingsIPad3");
		case IOS_IPad4:
			return TEXT("SystemSettingsIPad4");
		case IOS_IPadMini:
			return TEXT("SystemSettingsIPadMini");
		// default to lowest settings, which is the iPhone 3GS
		default:
			return TEXT("SystemSettingsIPhone3GS");
	}
}


/*-----------------------------------------------------------------------------
 Unimplemented dummies.
 -----------------------------------------------------------------------------*/

void appClipboardCopy( const TCHAR* Str )
{
}
FString appClipboardPaste()
{
	return TEXT("");
}

/**
 * Returns the last system error code in string form.  NOTE: Only one return value is valid at a time!
 *
 * @param OutBuffer the buffer to be filled with the error message
 * @param BufferLength the size of the buffer in character count
 * @param Error the error code to convert to string form
 */
const TCHAR* appGetSystemErrorMessage(TCHAR* OutBuffer,INT BufferCount,INT Error)
{
	check(OutBuffer && BufferCount >= MAX_SPRINTF);
	appSprintf(OutBuffer,TEXT("appGetSystemErrorMessage not supported.  Error: %d"),Error);
	warnf(OutBuffer);
	return OutBuffer;
}

void* appGetDllHandle( const TCHAR* Filename )
{
	return NULL;
}
void appFreeDllHandle( void* DllHandle )
{
}
void* appGetDllExport( void* DllHandle, const TCHAR* ProcName )
{
	return NULL;
}
void appDebugMessagef( const TCHAR* Fmt, ... )
{
	appErrorf(TEXT("Not supported"));
}
VARARG_BODY( UBOOL, appMsgf, const TCHAR*, VARARG_EXTRA(EAppMsgType Type) )
{
	TCHAR TempStr[4096]=TEXT("");
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Fmt, Fmt );

	switch (Type)
	{
		case AMT_OK:
			return IPhoneShowBlockingAlert("Message", TCHAR_TO_ANSI(TempStr), "OK");
		case AMT_YesNo:
			return IPhoneShowBlockingAlert("Message", TCHAR_TO_ANSI(TempStr), "No", "Yes");
		case AMT_OKCancel:
			return IPhoneShowBlockingAlert("Message", TCHAR_TO_ANSI(TempStr), "Cancel", "OK");
		case AMT_YesNoCancel:
			return IPhoneShowBlockingAlert("Message", TCHAR_TO_ANSI(TempStr), "No", "Yes", "Cancel");
		case AMT_Crash:
			debugf(TEXT("Game is crashing: %s"), TempStr);
			return IPhoneShowBlockingAlert("CRASH", TCHAR_TO_ANSI(TempStr), "OK");

		default:
			warnf(TEXT("Unsupported appMsgf type [%d]: %s"), TempStr);
	}

	return 1;
}
void appGetLastError( void )
{
	appErrorf(TEXT("Not supported"));
}
void EdClearLoadErrors()
{
	appErrorf(TEXT("Not supported"));
}
VARARG_BODY( void VARARGS, EdLoadErrorf, const TCHAR*, VARARG_EXTRA(INT Type) )
{
	appErrorf(TEXT("Not supported"));
}

void appLaunchURL( const TCHAR* URL, const TCHAR* Parms, FString* Error )
{
	if (Parms && appStrlen(Parms) > 0)
	{
		appErrorf(TEXT("appLaunchURL with Parms is not supported"));
	}
	IPhoneLaunchURL(TCHAR_TO_UTF8(URL), FALSE);
}

/**
 * Creates a new process and its primary thread. The new process runs the
 * specified executable file in the security context of the calling process.
 * @param URL					executable name
 * @param Parms					command line arguments
 * @param bLaunchDetached		if TRUE, the new process will have its own window
 * @param bLaunchHidded			if TRUE, the new process will be minimized in the task bar
 * @param bLaunchReallyHidden	if TRUE, the new process will not have a window or be in the task bar
 * @param OutProcessId			if non-NULL, this will be filled in with the ProcessId
 * @param PriorityModifier		-2 idle, -1 low, 0 normal, 1 high, 2 higher
 * @return	The process handle for use in other process functions
 */
void *appCreateProc( const TCHAR* URL, const TCHAR* Parms, UBOOL bLaunchDetached, UBOOL bLaunchHidden, UBOOL bLaunchReallyHidden, DWORD* OutProcessID, INT PriorityModifier )
{
	appErrorf(TEXT("appCreateProc not supported"));
	return NULL;
}

/** Returns TRUE if the specified process is running 
*
* @param ProcessHandle handle returned from appCreateProc
* @return TRUE if the process is still running
*/
UBOOL appIsProcRunning( void* )
{
	appErrorf(TEXT("appIsProcRunning not supported"));
	return FALSE;
}

/** Waits for a process to stop
*
* @param ProcessHandle handle returned from appCreateProc
*/
void appWaitForProc( void* )
{
	appErrorf(TEXT("appWaitForProc not supported"));
}

/** Terminates a process
*
* @param ProcessHandle handle returned from appCreateProc
*/
void appTerminateProc( void* )
{
	appErrorf(TEXT("appTerminateProc not supported"));
}

/** Retrieves the ProcessId of this process
*
* @return the ProcessId of this process
*/
DWORD appGetCurrentProcessId()
{
	appErrorf(TEXT("appGetCurrentProcessId not supported"));
	return 0;
}

UBOOL appGetProcReturnCode( void* ProcHandle, INT* ReturnCode )
{
	appErrorf(TEXT("appGetProcReturnCode not supported"));
	return false;
}
/** Returns TRUE if the specified application is running */
UBOOL appIsApplicationRunning( DWORD ProcessId )
{
	appErrorf(TEXT("appIsApplicationRunning not implemented."));
	return FALSE;
}

/*----------------------------------------------------------------------------
	Extras
 ----------------------------------------------------------------------------*/

time_t GAppInvokeTime;

/**
 * Super early IPhone initialization
 */
void appIPhoneInit(int argc, char* argv[], const char* SettingsCommandline)
{
	// record the time the app was invoked
	GAppInvokeTime = time(NULL);

	// set a bit in the profile so we can tell if we crashed
	if (IPhoneLoadUserSettingU64("IPhoneHome::LastRunCrashed") != 0)
	{
		// increment the numcrashes stat
		IPhoneIncrementUserSettingU64("IPhoneHome::NumCrashes");
	}
	else
	{
		// set this (we'll unset when we exit cleanly)
		IPhoneSaveUserSettingU64("IPhoneHome::LastRunCrashed", 1);
	}
	// increment num invocations stat
	IPhoneIncrementUserSettingU64("IPhoneHome::NumInvocations");

	// iphone file manager early initialization (needed for GetApplicationDirectory below)
	FFileManagerIPhone::StaticInit();
	
	extern TCHAR GCmdLine[16384];
	GCmdLine[0] = 0;

	// read in the command line text file (coming from UnrealFrontend) if it exists
	FString CommandLineFilePath = FFileManagerIPhone::GetApplicationDirectory() + ("/CookedIPhone/UE3CommandLine.txt");
	FILE* CommandLineFile = fopen(TCHAR_TO_UTF8(*CommandLineFilePath), "r");
	if (CommandLineFile)
	{
		char CommandLine[16384];
		fgets(CommandLine, ARRAY_COUNT(CommandLine) - 1, CommandLineFile);
		appStrcpy(GCmdLine, ANSI_TO_TCHAR(CommandLine));
	}

	// start the saved commandline with the Settings
	GSavedCommandLinePortion = ANSI_TO_TCHAR(SettingsCommandline);

	// append any commandline options coming from Xcode (in general, ? options from Xcode will fail)
	for (INT Option = 1; Option < argc; Option++)
	{
		GSavedCommandLinePortion += TEXT(" ");
		GSavedCommandLinePortion += ANSI_TO_TCHAR(argv[Option]);
	}

	// now merge the GSavedCommandLine with the rest
	MergeCommandlineWithSaved(GCmdLine);

	GExecutableName = FString::Printf( TEXT( "%sGame-IPhone-%s" ), GGameName, *appGetConfigurationString() );

	appOutputDebugStringf(TEXT("Combined iPhone Commandline: %s") LINE_TERMINATOR, GCmdLine);
}


/**
 * Allow debugger to show FStrings
 */
const char* __FString(const FString& Input)
{
	return TCHAR_TO_ANSI(*Input);
}

/**
 * Allow debugger to show TCHAR* strings
 */
const char* __Str(TCHAR* Input)
{
	return TCHAR_TO_ANSI(Input);
}

/**
 * Allow debugger to show FNames
 */
const char* __FName(FName Input)
{
	return TCHAR_TO_ANSI(*Input.ToString());
}

/**
 * Allow debugger to show UObject names
 */
const char* __Obj(const UObject* Input)
{
	return TCHAR_TO_ANSI(*Input->GetFullName());
}

/**
 * Reads the mac address for the computer
 *
 * @return the MAC address as a string (or empty if MAC address could not be read).
 */
FString appGetMacAddress()
{
	return IPhoneGetMacAddress();
}

/**
 * Return the name of the currently running executable
 *
 * @return Name of the currently running executable
 */
const TCHAR* appExecutableName()
{
	return *GExecutableName;
}
