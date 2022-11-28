/*=============================================================================
	UnFile.h: General-purpose file utilities.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Global variables.
-----------------------------------------------------------------------------*/

// Global variables.
extern DWORD GCRCTable[];

/*----------------------------------------------------------------------------
	Byte order conversion.
----------------------------------------------------------------------------*/

// Bitfields.
#ifndef NEXT_BITFIELD
	#if __INTEL_BYTE_ORDER__
		#define NEXT_BITFIELD(b) ((b)<<1)
		#define FIRST_BITFIELD   (1)
	#else
		#define NEXT_BITFIELD(b) ((b)>>1)
		#define FIRST_BITFIELD   (0x80000000)
	#endif
#endif

#if __INTEL_BYTE_ORDER__
	#define INTEL_ORDER16(x)   (x)
	#define INTEL_ORDER32(x)   (x)
	#define INTEL_ORDER64(x)   (x)
#else

    // These macros are not safe to use unless data is UNSIGNED!
	#define INTEL_ORDER16_unsigned(x)   ((((x)>>8)&0xff)+ (((x)<<8)&0xff00))
	#define INTEL_ORDER32_unsigned(x)   (((x)>>24) + (((x)>>8)&0xff00) + (((x)<<8)&0xff0000) + ((x)<<24))

    static inline _WORD INTEL_ORDER16(_WORD val)
    {
        #if MACOSX
        register _WORD retval;
        __asm__ volatile("lhbrx %0,0,%1"
                : "=r" (retval)
                : "r" (&val)
                );
        return retval;
        #else
        return(INTEL_ORDER16_unsigned(val));
        #endif
    }

    static inline SWORD INTEL_ORDER16(SWORD val)
    {
        _WORD uval = *((_WORD *) &val);
        uval = INTEL_ORDER16(uval);
        return( *((SWORD *) &uval) );
    }

    static inline DWORD INTEL_ORDER32(DWORD val)
    {
        #if MACOSX
        register DWORD retval;
        __asm__ __volatile__ (
            "lwbrx %0,0,%1"
                : "=r" (retval)
                : "r" (&val)
        );
        return retval;
        #else
        return(INTEL_ORDER32_unsigned(val));
        #endif
    }

    static inline INT INTEL_ORDER32(INT val)
    {
        DWORD uval = *((DWORD *) &val);
        uval = INTEL_ORDER32(uval);
        return( *((INT *) &uval) );
    }

	static inline QWORD INTEL_ORDER64(QWORD x)
	{
		/* Separate into high and low 32-bit values and swap them */
		DWORD l = (DWORD) (x & 0xFFFFFFFF);
		DWORD h = (DWORD) ((x >> 32) & 0xFFFFFFFF);
	    return( (((QWORD) (INTEL_ORDER32(l))) << 32) |
		         ((QWORD) (INTEL_ORDER32(h))) );
	}
#endif


/*-----------------------------------------------------------------------------
	Stats.
-----------------------------------------------------------------------------*/

#if STATS
	#define STAT(x) x
#else
	#define STAT(x)
#endif

/*-----------------------------------------------------------------------------
	Global init and exit.
-----------------------------------------------------------------------------*/

void appInit( const TCHAR* InCmdLine, FMalloc* InMalloc, FOutputDevice* InLog, FOutputDeviceConsole* InLogConsole, FOutputDeviceError* InError, FFeedbackContext* InWarn, FFileManager* InFileManager, FCallbackDevice* InCallbackDevice, FConfigCache*(*ConfigFactory)() );
void appPreExit();
void appExit();

/*-----------------------------------------------------------------------------
	Logging and critical errors.
-----------------------------------------------------------------------------*/

void appRequestExit( UBOOL Force );

void VARARGS appFailAssert( const ANSICHAR* Expr, const ANSICHAR* File, INT Line );
const TCHAR* appGetSystemErrorMessage( INT Error=0 );
const void appDebugMessagef( const TCHAR* Fmt, ... );
const void appGetLastError( void );
const void EdClearLoadErrors();
VARARG_DECL( const UBOOL, static const UBOOL, return, appMsgf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(INT Type), VARARG_EXTRA(Type) );
VARARG_DECL( const void, static const void, VARARG_NONE, EdLoadErrorf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(INT Type), VARARG_EXTRA(Type) );

void appDebugBreak();
UBOOL appIsDebuggerPresent();

// Define NO_LOGGING to strip out all writing to log files, OutputDebugString(), etc.
// This is needed for consoles that require no logging (Xbox, Xenon)
#ifndef NO_LOGGING
	#define debugf				GLog->Logf
	#define appErrorf			GError->Logf
	#define warnf				GWarn->Logf
#else
	#if _MSC_VER
		// MS compilers support noop which discards everything inside the parens
		#define debugf			__noop
		#define appErrorf		__noop
		#define warnf			__noop
	#else
		#pragma message("Logging can only be disabled on MS compilers")
		#define debugf			GLog->Logf
		#define appErrorf		GError->Logf
		#define warnf			GWarn->Logf
	#endif
#endif

#if DO_GUARD_SLOW
	#define debugfSlow			GLog->Logf
	#define appErrorfSlow		GError->Logf
#else
	#if _MSC_VER
		// MS compilers support noop which discards everything inside the parens
		#define debugfSlow		__noop
		#define appErrorfSlow	__noop
	#else
		#define debugfSlow		GNull->Logf
		#define appErrorfSlow	GNull->Logf
	#endif
#endif

/*-----------------------------------------------------------------------------
	Misc.
-----------------------------------------------------------------------------*/
void  appInitTimeing( void );
void* appGetDllHandle( const TCHAR* DllName );
void appFreeDllHandle( void* DllHandle );
void* appGetDllExport( void* DllHandle, const TCHAR* ExportName );
void appLaunchURL( const TCHAR* URL, const TCHAR* Parms=NULL, FString* Error=NULL );
void* appCreateProc( const TCHAR* URL, const TCHAR* Parms );
UBOOL appGetProcReturnCode( void* ProcHandle, INT* ReturnCode );
class FGuid appCreateGuid();
void appCreateTempFilename( const TCHAR* Path, TCHAR* Result256 );
void appCleanFileCache();
UBOOL appCreateBitmap( const TCHAR* Pattern, INT Width, INT Height, class FColor* Data, FFileManager* FileManager = GFileManager );
/* ============================================================================
 * appAssembleIni
 * 
 * Given a Destination and Source filename, this function will recurse the
 * Destination till it hits the root and then merge ini settings from there
 * till the leaf (Source) and write out the result to the file specified by 
 * Destination.
 *
 * ============================================================================
 */
void appAssembleIni( const TCHAR* Destination, const TCHAR* Source );


/*-----------------------------------------------------------------------------
	Package file cache.
-----------------------------------------------------------------------------*/

struct FPackageFileCache
{
	FString PackageFromPath( const TCHAR* InPathName );
	void SplitPath( const TCHAR* InPathName, FString& Path, FString& Filename, FString& Extension );
	virtual void CachePaths()=0;
	virtual UBOOL CachePackage( const TCHAR* InPathName, UBOOL InOverrideDupe=0, UBOOL WarnIfExists=1 )=0;
	virtual UBOOL FindPackageFile( const TCHAR* InName, const FGuid* Guid, FString& OutFileName )=0;
	virtual TArray<FString> GetPackageFileList() = 0;
};

extern FPackageFileCache* GPackageFileCache;


/*-----------------------------------------------------------------------------
	Clipboard.
-----------------------------------------------------------------------------*/

void appClipboardCopy( const TCHAR* Str );
FString appClipboardPaste();

/*-----------------------------------------------------------------------------
	Exception handling.
-----------------------------------------------------------------------------*/

//
// For throwing string-exceptions which safely propagate through guard/unguard.
//
VARARG_DECL( void VARARGS, static void, VARARG_NONE, appThrowf, VARARG_NONE, const TCHAR*, VARARG_NONE, VARARG_NONE );

/*-----------------------------------------------------------------------------
	Check macros for assertions.
-----------------------------------------------------------------------------*/

//
// "check" expressions are only evaluated if enabled.
// "verify" expressions are always evaluated, but only cause an error if enabled.
//
#if DO_CHECK
	#define check(expr)  {if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__ );}
	#define verify(expr) {if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__ );}
#else
#if _MSC_VER
	// MS compilers support noop which discards everything inside the parens
	#define check __noop
#else
	#define check(expr) {}
#endif
	#define verify(expr) if(expr){}
#endif

//
// Check for development only.
//
#if DO_GUARD_SLOW
	#define checkSlow(expr)  {if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__ );}
	#define verifySlow(expr) {if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__ );}
#else
	#if _MSC_VER
		// MS compilers support noop which discards everything inside the parens
		#define checkSlow __noop
	#else
		#define checkSlow(expr) {}
	#endif
	#define verifySlow(expr) if(expr){}
#endif

/*-----------------------------------------------------------------------------
	Timing macros.
-----------------------------------------------------------------------------*/

//
// Normal timing.
//
#define clock(Timer)   {Timer -= appCycles();}
#define unclock(Timer) {Timer += appCycles()-12;}

/*-----------------------------------------------------------------------------
	Text format.
-----------------------------------------------------------------------------*/

FString appFormat( FString Src, const TMultiMap<FString,FString>& Map );

/*-----------------------------------------------------------------------------
	Localization.
-----------------------------------------------------------------------------*/

FString Localize( const TCHAR* Section, const TCHAR* Key, const TCHAR* Package=GPackage, const TCHAR* LangExt=NULL, UBOOL Optional=0 );
FString LocalizeError( const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
FString LocalizeProgress( const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
FString LocalizeQuery( const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
FString LocalizeGeneral( const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );

#if UNICODE
	FString Localize( const ANSICHAR* Section, const ANSICHAR* Key, const TCHAR* Package=GPackage, const TCHAR* LangExt=NULL, UBOOL Optional=0 );
	FString LocalizeError( const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
	FString LocalizeProgress( const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
	FString LocalizeQuery( const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
	FString LocalizeGeneral( const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
#endif

/*-----------------------------------------------------------------------------
	File functions.
-----------------------------------------------------------------------------*/

// File utilities.
const TCHAR* appFExt( const TCHAR* Filename );
UBOOL appUpdateFileModTime( TCHAR* Filename );

/*-----------------------------------------------------------------------------
	OS functions.
-----------------------------------------------------------------------------*/

const TCHAR* appCmdLine();
const TCHAR* appBaseDir();
const TCHAR* appComputerName();
const TCHAR* appUserName();

/*-----------------------------------------------------------------------------
	Game/ mod specific directories.
-----------------------------------------------------------------------------*/

FString appEngineDir();
FString appEngineConfigDir();

FString appGameDir();
FString appGameConfigDir();
FString appGameLogDir();

void appAssembleIni( const TCHAR* Destination, const TCHAR* Source );

/*-----------------------------------------------------------------------------
	Timing functions.
-----------------------------------------------------------------------------*/

#if !DEFINED_appCycles
DWORD appCycles();
#endif

#if !DEFINED_appSeconds
DOUBLE appSeconds();
#endif

void appSystemTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec );
SQWORD appSystemTime64();
const TCHAR* appTimestamp();
DOUBLE appSecondsSlow();
void appSleep( FLOAT Seconds );

/*-----------------------------------------------------------------------------
	Character type functions.
-----------------------------------------------------------------------------*/

inline TCHAR appToUpper( TCHAR c )
{
	return (c<'a' || c>'z') ? (c) : (c+'A'-'a');
}
inline TCHAR appToLower( TCHAR c )
{
	return (c<'A' || c>'Z') ? (c) : (c+'a'-'A');
}
inline UBOOL appIsUpper( TCHAR c )
{
	return (c>='A' && c<='Z');
}
inline UBOOL appIsLower( TCHAR c )
{
	return (c>='a' && c<='z');
}
inline UBOOL appIsAlpha( TCHAR c )
{
	return (c>='a' && c<='z') || (c>='A' && c<='Z');
}
inline UBOOL appIsDigit( TCHAR c )
{
	return c>='0' && c<='9';
}
inline UBOOL appIsAlnum( TCHAR c )
{
	return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9');
}

#include <ctype.h>
inline UBOOL appIsSpace( TCHAR c )
{
#if UNICODE
    return( iswspace(c) );
#else
    return( isspace(c) );
#endif
}

/*-----------------------------------------------------------------------------
	String functions.
-----------------------------------------------------------------------------*/

UBOOL appIsPureAnsi( const TCHAR* Str );

inline TCHAR* appStrcpy( TCHAR* Dest, const TCHAR* Src ) { return (TCHAR*)_tcscpy( Dest, Src ); }
inline INT appStrlen( const TCHAR* String ) { return _tcslen( String ); }
inline TCHAR* appStrstr( const TCHAR* String, const TCHAR* Find ) { return (TCHAR*)_tcsstr( String, Find ); }
inline TCHAR* appStrchr( const TCHAR* String, INT c ) { return (TCHAR*)_tcschr( String, c ); }
inline TCHAR* appStrcat( TCHAR* Dest, const TCHAR* Src ) { return (TCHAR*)_tcscat( Dest, Src ); }
inline INT appStrcmp( const TCHAR* String1, const TCHAR* String2 ) { return _tcscmp( String1, String2 ); }
inline INT appStricmp( const TCHAR* String1, const TCHAR* String2 )  { return _tcsicmp( String1, String2 ); }
inline INT appStrncmp( const TCHAR* String1, const TCHAR* String2, INT Count ) { return _tcsncmp( String1, String2, Count ); }
inline TCHAR* appStrupr( TCHAR* String ) { return (TCHAR*)_tcsupr( String ); }
inline INT appAtoi( const TCHAR* String ) { return atoi( TCHAR_TO_ANSI(String) ); }
inline FLOAT appAtof( const TCHAR* String ) { return atof( TCHAR_TO_ANSI(String) ); }
inline INT appStrtoi( const TCHAR* Start, TCHAR** End, INT Base ) { return _tcstoul( Start, End, Base ); }
inline INT appStrnicmp( const TCHAR* A, const TCHAR* B, INT Count ) { return _tcsnicmp( A, B, Count ); }

const TCHAR* appSpc( int Num );
TCHAR* appStrncpy( TCHAR* Dest, const TCHAR* Src, int Max);
TCHAR* appStrncat( TCHAR* Dest, const TCHAR* Src, int Max);
const TCHAR* appStrfind(const TCHAR* Str, const TCHAR* Find);
DWORD appStrCrc( const TCHAR* Data );
DWORD appStrCrcCaps( const TCHAR* Data );
TCHAR* appItoa( const INT Num );
VARARG_DECL( INT, static INT, return, appSprintf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(TCHAR* Dest), VARARG_EXTRA(Dest) );
void appTrimSpaces( ANSICHAR* String);

#if UNICODE
	#define appSSCANF	swscanf
#else
	#define appSSCANF	sscanf
#endif

#if _MSC_VER
	INT appGetVarArgs( TCHAR* Dest, INT Count, const TCHAR*& Fmt, va_list ArgPtr );
	INT appGetVarArgsAnsi( ANSICHAR* Dest, INT Count, const ANSICHAR*& Fmt, va_list ArgPtr );
#else
	#include <stdio.h>
	#include <stdarg.h>
#endif

typedef int QSORT_RETURN;
typedef QSORT_RETURN(CDECL* QSORT_COMPARE)( const void* A, const void* B );
void appQsort( void* Base, INT Num, INT Width, QSORT_COMPARE Compare );

//
// Case insensitive string hash function.
//
inline DWORD appStrihash( const TCHAR* Data )
{
	DWORD Hash=0;
	while( *Data )
	{
		TCHAR Ch = appToUpper(*Data++);
		BYTE  B  = Ch;
		Hash     = ((Hash >> 8) & 0x00FFFFFF) ^ GCRCTable[(Hash ^ B) & 0x000000FF];
#if UNICODE
		B        = Ch>>8;
		Hash     = ((Hash >> 8) & 0x00FFFFFF) ^ GCRCTable[(Hash ^ B) & 0x000000FF];
#endif
	}
	return Hash;
}

/*-----------------------------------------------------------------------------
	Parsing functions.
-----------------------------------------------------------------------------*/

UBOOL ParseCommand( const TCHAR** Stream, const TCHAR* Match );
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, class FName& Name );
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, DWORD& Value );
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, class FGuid& Guid );
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, TCHAR* Value, INT MaxLen );
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, BYTE& Value );
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, SBYTE& Value );
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, _WORD& Value );
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, SWORD& Value );
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, FLOAT& Value );
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, INT& Value );
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, FString& Value );
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, QWORD& Value );
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, SQWORD& Value );
UBOOL ParseUBOOL( const TCHAR* Stream, const TCHAR* Match, UBOOL& OnOff );
UBOOL ParseObject( const TCHAR* Stream, const TCHAR* Match, class UClass* Type, class UObject*& DestRes, class UObject* InParent );
UBOOL ParseLine( const TCHAR** Stream, TCHAR* Result, INT MaxLen, UBOOL Exact=0 );
UBOOL ParseLine( const TCHAR** Stream, FString& Resultd, UBOOL Exact=0 );
UBOOL ParseToken( const TCHAR*& Str, TCHAR* Result, INT MaxLen, UBOOL UseEscape );
UBOOL ParseToken( const TCHAR*& Str, FString& Arg, UBOOL UseEscape );
FString ParseToken( const TCHAR*& Str, UBOOL UseEscape );
void ParseNext( const TCHAR** Stream );
UBOOL ParseParam( const TCHAR* Stream, const TCHAR* Param );

/*-----------------------------------------------------------------------------
	Array functions.
-----------------------------------------------------------------------------*/

// Core functions depending on TArray and FString.
UBOOL appLoadFileToArray( TArray<BYTE>& Result, const TCHAR* Filename, FFileManager* FileManager=GFileManager );
UBOOL appLoadFileToString( FString& Result, const TCHAR* Filename, FFileManager* FileManager=GFileManager );
UBOOL appSaveArrayToFile( const TArray<BYTE>& Array, const TCHAR* Filename, FFileManager* FileManager=GFileManager );
UBOOL appSaveStringToFile( const FString& String, const TCHAR* Filename, FFileManager* FileManager=GFileManager );

/*-----------------------------------------------------------------------------
	Memory functions.
-----------------------------------------------------------------------------*/

void* appMemmove( void* Dest, const void* Src, INT Count );
INT appMemcmp( const void* Buf1, const void* Buf2, INT Count );
UBOOL appMemIsZero( const void* V, int Count );
DWORD appMemCrc( const void* Data, INT Length, DWORD CRC=0 );
void appMemswap( void* Ptr1, void* Ptr2, DWORD Size );
void appMemset( void* Dest, INT C, INT Count );

#ifndef DEFINED_appMemcpy
void appMemcpy( void* Dest, const void* Src, INT Count );
#endif

#ifndef DEFINED_appMemzero
void appMemzero( void* Dest, INT Count );
#endif

//
// C style memory allocation stubs.
//
#define appMalloc			GMalloc->Malloc
#define appFree				GMalloc->Free
#define appRealloc			GMalloc->Realloc
#define appPhysicalAlloc	GMalloc->PhysicalAlloc
#define appPhysicalFree		GMalloc->PhysicalFree
//
// C style memory allocation stubs that are safe to call before GMalloc.Init
//
#define appSystemMalloc		malloc
#define appSystemFree		free
#define appSystemRealloc	realloc

//
// C++ style memory allocation.
//
inline void* operator new( size_t Size )
{
	return appMalloc( Size );
}
inline void operator delete( void* Ptr )
{
	appFree( Ptr );
}

inline void* operator new[]( size_t Size )
{
	return appMalloc( Size );
}
inline void operator delete[]( void* Ptr )
{
	appFree( Ptr );
}

/**
 * Inheriting from this base class will use the system allocators for
 * new and delete which renders it safe to create and destroy inherited
 * objects before GMalloc->Init has been called. Another usage scenario
 * is objects that need to persist across memory image level transitions
 * like e.g. FCriticalSection and therefore can't use the default 
 * implementation of operator new/ delete.
 */
struct FSystemAllocatorNewDelete
{
	/**
	 * Overloaded new operator using the system allocator.
	 *
	 * @param	Size	Amount of memory to allocate (in bytes)
	 * @return			A pointer to a block of memory with size Size or NULL
	 */
	inline void* operator new( size_t Size )
	{
		return appSystemMalloc( Size );
	}

	/**
	 * Overloaded delete operator using the system allocator
	 *
	 * @param	Ptr		Pointer to delete
	 */
	inline void operator delete( void* Ptr )
	{
		appSystemFree( Ptr );
	}

	/**
	 * Overloaded array new operator using the system allocator.
	 *
	 * @param	Size	Amount of memory to allocate (in bytes)
	 * @return			A pointer to a block of memory with size Size or NULL
	 */
	inline void* operator new[]( size_t Size )
	{
		return appSystemMalloc( Size );
	}

	/**
	 * Overloaded array delete operator using the system allocator
	 *
	 * @param	Ptr		Pointer to delete
	 */
	inline void operator delete[]( void* Ptr )
	{
		appSystemFree( Ptr );
	}
};

/*-----------------------------------------------------------------------------
	Math.
-----------------------------------------------------------------------------*/

BYTE appCeilLogTwo( DWORD Arg );

/*-----------------------------------------------------------------------------
	MD5 functions.
-----------------------------------------------------------------------------*/

//
// MD5 Context.
//
struct FMD5Context
{
	DWORD state[4];
	DWORD count[2];
	BYTE buffer[64];
};

//
// MD5 functions.
//!!it would be cool if these were implemented as subclasses of
// FArchive.
//
void appMD5Init( FMD5Context* context );
void appMD5Update( FMD5Context* context, BYTE* input, INT inputLen );
void appMD5Final( BYTE* digest, FMD5Context* context );
void appMD5Transform( DWORD* state, BYTE* block );
void appMD5Encode( BYTE* output, DWORD* input, INT len );
void appMD5Decode( DWORD* output, BYTE* input, INT len );

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

