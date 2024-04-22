/*=============================================================================
	Android.h: Unreal definitions for Android.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*----------------------------------------------------------------------------
	Platform compiler definitions.
----------------------------------------------------------------------------*/

#ifndef __UE3_ANDROID_H__
#define __UE3_ANDROID_H__

#include <string.h>
#include <alloca.h>
#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <limits.h>
#include <sys/time.h>
#include <math.h>

#if FINAL_RELEASE && !FINAL_RELEASE_DEBUGCONSOLE
// don't allow printf in final release
#define printf(...) 
#endif

/*----------------------------------------------------------------------------
	Platform specifics types and defines.
----------------------------------------------------------------------------*/

// Comment this out if you have no need for unicode strings (ie asian languages, etc).
#define UNICODE 1


// Generates GCC version like this:  xxyyzz (eg. 030401)
// xx: major version, yy: minor version, zz: patch level
#ifdef __GNUC__
#	define GCC_VERSION	(__GNUC__*10000 + __GNUC_MINOR__*100 + __GNUC_PATCHLEVEL)
#endif

// Undo any defines.
#undef NULL
#undef BYTE
#undef WORD
#undef DWORD
#undef INT
#undef FLOAT
#undef MAXBYTE
#undef MAXWORD
#undef MAXDWORD
#undef MAXINT
#undef CDECL

// Make sure HANDLE is defined.
#ifndef _WINDOWS_
	#define HANDLE void*
	#define HINSTANCE void*
#endif

#if _DEBUG
	#define STUBBED(x) \
		do { \
			static UBOOL AlreadySeenThisStubbedSection = FALSE; \
			if (!AlreadySeenThisStubbedSection) \
			{ \
				AlreadySeenThisStubbedSection = TRUE; \
				fprintf(stderr, "STUBBED: %s at %s:%d (%s)\n", x, __FILE__, __LINE__, __FUNCTION__); \
			} \
		} while (0)
#else
	#define STUBBED(x)
#endif




// Sizes.
enum {DEFAULT_ALIGNMENT = 8 }; // Default boundary to align memory allocations on.

#define RENDER_DATA_ALIGNMENT 128 // the value to align some renderer bulk data to


// Optimization macros (preceded by #pragma).
#define PRAGMA_DISABLE_OPTIMIZATION _Pragma("optimize(\"\",off)")
#ifdef _DEBUG
	#define PRAGMA_ENABLE_OPTIMIZATION  _Pragma("optimize(\"\",off)")
#else
	#define PRAGMA_ENABLE_OPTIMIZATION  _Pragma("optimize(\"\",on)")
#endif

// Function type macros.
#define VARARGS     					/* Functions with variable arguments */
#define CDECL	    					/* Standard C function */
#define STDCALL							/* Standard calling convention */


#define FORCEINLINE inline __attribute__ ((always_inline))    /* Force code to be inline */

#define FORCENOINLINE __attribute__((noinline))			/* Force code to be NOT inline */
#define ZEROARRAY						/* Zero-length arrays in structs */

// pointer aliasing restricting
#define RESTRICT __restrict

// Hints compiler that expression is true (not supported on Android)
#define ASSUME(expr)

// Compiler name.
#ifdef _DEBUG
	#define COMPILER "Compiled with GCC debug"
#else
	#define COMPILER "Compiled with GCC"
#endif



// Unsigned base types.
typedef uint8_t					BYTE;		// 8-bit  unsigned.
typedef uint16_t				WORD;		// 16-bit unsigned.
typedef uint32_t				UINT;		// 32-bit unsigned.
typedef unsigned long			DWORD;		// 32-bit unsigned.
typedef uint64_t				QWORD;		// 64-bit unsigned.

// Signed base types.
typedef int8_t					SBYTE;		// 8-bit  signed.
typedef int16_t					SWORD;		// 16-bit signed.
typedef int32_t					INT;		// 32-bit signed.
typedef int32_t					LONG;		// 32-bit signed.
typedef int64_t					SQWORD;		// 64-bit unsigned.

// Character types.
typedef char					ANSICHAR;	// An ANSI character. normally a signed type.
typedef int16_t					UNICHAR;	// A unicode character. normally a signed type.
// WCHAR defined below

// Other base types.
typedef UINT					UBOOL;		// Boolean 0 (false) or 1 (true).
typedef float					FLOAT;		// 32-bit IEEE floating point.
typedef double					DOUBLE;		// 64-bit IEEE double.
typedef uintptr_t				SIZE_T;     // Should be size_t, but windows uses this
typedef intptr_t				PTRINT;		// Integer large enough to hold a pointer.
typedef uintptr_t				UPTRINT;	// Unsigned integer large enough to hold a pointer.

// Bitfield type.
typedef unsigned int			BITFIELD;	// For bitfields.

/** Represents a serializable object pointer in UnrealScript.  This is always 64-bits, even on 32-bit platforms. */
typedef	QWORD				ScriptPointerType;

#define DECLARE_UINT64(x)	x##ULL


// Make sure characters are unsigned.
#ifdef _CHAR_UNSIGNED
	#error "Bad VC++ option: Characters must be signed"
#endif

// No asm if not compiling for x86.
#define ASM_X86 0

#define __INTEL_BYTE_ORDER__ 1

#define PLATFORM_64BITS 0
#define PLATFORM_32BITS 1

// DLL file extension.
#define DLLEXT TEXT(".dll")

// Pathnames.
#define PATH(s) s

// NULL.
#define NULL 0

#define FALSE 0
#define TRUE 1

// Platform support options.
#define FORCE_ANSI_LOG           1

// OS unicode function calling.
typedef char TCHAR;
#define TCHAR_IS_1_BYTE 1
#define _TCHAR_DEFINED

#define TEXT(s) s
#define _TEXT_DEFINED 


// defines for the "standard" unicode-safe string functions
#define _tcscpy wide_cpy
#define _tcslen wide_len
#define _tcsstr wide_str
#define _tcschr wide_chr
#define _tcsrchr wide_rchr
#define _tcscat wide_cat
#define _tcscmp wide_cmp
#define _stricmp strcasecmp
#define _tcsicmp strcasecmp
#define _tcsncmp wide_ncmp
#define _tcsupr wide_upr
#define _tcstoul(s,e,b) wide_toul(s, e, b)
#define _tcstoui64(s,e,b) wide_toull(s, e, b)
#define _tcsnicmp strncasecmp
#define _tstoi(s) wide_toul(s)
#define _tstoi64(s) wide_toull(s)
#define _tstof(s) wide_tod(s)
#define _tcstod(s,d) wide_tod(s)
#define _tcsncpy wide_ncpy
#define _stscanf swscanf

// wide string android madness
bool iswspace( TCHAR c );
bool iswpunct( TCHAR c );
TCHAR towupper( TCHAR c );
TCHAR * wide_upr( TCHAR *str );
void wide_to_narrow( char * n, const TCHAR * w );
void narrow_to_wide( TCHAR * w, const char * n );
TCHAR *wide_cpy( TCHAR *dst, const TCHAR *src );
TCHAR *wide_ncpy( TCHAR *dst, const TCHAR *src, int n );
size_t wide_len( const TCHAR *dst );
TCHAR *wide_cat( TCHAR *dst, const TCHAR *src );
TCHAR *wide_chr( const TCHAR *s, TCHAR c );
TCHAR *wide_rchr( const TCHAR *s, TCHAR c );
TCHAR *wide_str( const TCHAR *big, const TCHAR *little );
int wide_cmp( const TCHAR *a, const TCHAR *b );
int wide_ncmp( const TCHAR *a, const TCHAR *b, int n );
int wide_toul( const TCHAR * wstr, TCHAR **end = NULL, int base = 10 );
unsigned long long wide_toull( const TCHAR * wstr, TCHAR **end = NULL, int base = 10 );
double wide_tod( const TCHAR * wstr );
// This won't handle TCHAR * replacement; need to scan the fmt for %s and do surgery on
// the arg list. Ugh.
int vswprintf( TCHAR *dst, int count, const TCHAR *fmt, va_list arg );
int wprintf( const TCHAR *fmt, ... );
int swscanf( const TCHAR *buffer, const TCHAR *fmt, ... );

#define CP_OEMCP 1
#define CP_ACP 1

//#include <wchar.h>
//#include <wctype.h>

// String conversion classes
#include "UnStringConv.h"


/**
* NOTE: The objects these macros declare have very short lifetimes. They are
* meant to be used as parameters to functions. You cannot assign a variable
* to the contents of the converted string as the object will go out of
* scope and the string released.
*
* NOTE: The parameter you pass in MUST be a proper string, as the parameter
* is typecast to a pointer. If you pass in a char, not char* it will crash.
*
* Usage:
*
*		SomeApi(TCHAR_TO_ANSI(SomeUnicodeString));
*
*		const char* SomePointer = TCHAR_TO_ANSI(SomeUnicodeString); <--- Bad!!!
*/

#define TCHAR_TO_ANSI(str) (ANSICHAR*)FTCHARToANSI((const TCHAR*)str)
#define TCHAR_TO_UTF8(str) (ANSICHAR*)FTCHARToUTF8((const TCHAR*)str)
#define UTF8_TO_TCHAR(str) (TCHAR*)FUTF8ToTCHAR((const ANSICHAR*)str)
#define TCHAR_TO_OEM(str) (ANSICHAR*)FTCHARToUTF8((const TCHAR*)str)
#define ANSI_TO_TCHAR(str) (TCHAR*)FANSIToTCHAR((const ANSICHAR*)str)
#define TCHAR_TO_UCS2(str) (UNICHAR*)FTCHARToUCS2((const TCHAR*)str)
#define UCS2_TO_TCHAR(str) (TCHAR*)FUCS2ToTCHAR((const UNICHAR*)str)
#undef CP_OEMCP
#undef CP_ACP

#define TCHAR_CALL_OS(funcW,funcA) (GUnicodeOS ? (funcW) : (funcA))

// Strings.
#define LINE_TERMINATOR TEXT("\n")
#define PATH_SEPARATOR TEXT("\\")
#define appIsPathSeparator( Ch )	((Ch) == TEXT('/') || (Ch) == TEXT('\\'))

// Alignment.
#define GCC_PACK(n) __attribute__((packed,aligned(n)))
#define GCC_ALIGN(n) __attribute__((aligned(n)))
#define MS_ALIGN(n)

#define REQUIRES_ALIGNED_ACCESS 1
#define REQUIRES_ALIGNED_INT_ACCESS 1

// GCC doesn't support __noop, but there is a workaround :)
#define COMPILER_SUPPORTS_NOOP 1
#define __noop(...)

/*----------------------------------------------------------------------------
	Globals.
----------------------------------------------------------------------------*/

/** Device Model													*/
class FString;
extern FString		GAndroidDeviceModel;

/** Screen width													*/
extern INT			GScreenWidth;
/** Screen height													*/
extern INT			GScreenHeight;

// The total device memory in bytes
extern UINT			GAndroidDeviceMemory;

extern FLOAT		GAndroidResolutionScale;

/**
 * Shader manager instance
 */
#if WITH_ES2_RHI
extern FES2ShaderManager GShaderManager;
#endif

/** Thread context */
typedef DWORD		FThreadContext;

// Enumeration for performance levels to enable/disable features
enum EAndroidPerformanceLevel
{
	ANDROID_Performance_1,
	ANDROID_Performance_2
};
extern EAndroidPerformanceLevel GAndroidPerformanceLevel;

// Enumeration for memory cutoff levels to enable/disable features
enum EAndroidMemoryLevel
{
	Android_Memory_Low,
	Android_Memory_1024		// >= 1024 detected MB of device memory
};
extern EAndroidMemoryLevel GAndroidMemoryLevel;

/*----------------------------------------------------------------------------
Stack walking.
----------------------------------------------------------------------------*/

/** @name ObjectFlags
* Flags used to control the output from stack tracing
*/
typedef DWORD EVerbosityFlags;
#define VF_DISPLAY_BASIC		0x00000000
#define VF_DISPLAY_FILENAME		0x00000001
#define VF_DISPLAY_MODULE		0x00000002
#define VF_DISPLAY_ALL			0xffffffff
                               
/*----------------------------------------------------------------------------
	Initialization.
----------------------------------------------------------------------------*/

//extern void appAndroidInit(int argc, char* const argv[]);

/*----------------------------------------------------------------------------
	Math functions.
----------------------------------------------------------------------------*/

extern INT GSRandSeed;

inline INT appTrunc( FLOAT F )
{
	return (INT)F;
//	return (INT)truncf(F);
}
inline FLOAT appTruncFloat( FLOAT F )
{
//	return __fcfid(__fctidz(F));
	return (FLOAT)appTrunc(F);
}

inline FLOAT	appExp( FLOAT Value )			{ return expf(Value); }
inline FLOAT	appLoge( FLOAT Value )			{ return logf(Value); }
inline FLOAT	appFmod( FLOAT Y, FLOAT X )		{ return fmodf(Y,X); }
inline FLOAT	appSin( FLOAT Value )			{ return sinf(Value); }
inline FLOAT 	appAsin( FLOAT Value ) 			{ return asinf( (Value<-1.f) ? -1.f : ((Value<1.f) ? Value : 1.f) ); }
inline FLOAT 	appCos( FLOAT Value ) 			{ return cosf(Value); }
inline FLOAT 	appAcos( FLOAT Value ) 			{ return acosf( (Value<-1.f) ? -1.f : ((Value<1.f) ? Value : 1.f) ); }
inline FLOAT	appTan( FLOAT Value )			{ return tanf(Value); }
inline FLOAT	appAtan( FLOAT Value )			{ return atanf(Value); }
inline FLOAT	appAtan2( FLOAT Y, FLOAT X )	{ return atan2f(Y,X); }
inline FLOAT	appSqrt( FLOAT Value );
inline FLOAT	appPow( FLOAT A, FLOAT B )		{ return powf(A,B); }
inline UBOOL	appIsNaN( FLOAT A )				{ return isnan(A) != 0; }
inline UBOOL	appIsFinite( FLOAT A )			{ return isfinite(A) != 0; }
inline INT		appFloor( FLOAT F );
inline INT		appCeil( FLOAT Value )			{ return appTrunc(ceilf(Value)); }
inline INT		appRand()						{ return rand(); }
inline FLOAT	appCopySign( FLOAT A, FLOAT B ) { return copysignf(A,B); }
inline void		appRandInit(INT Seed)			{ srand( Seed ); }
inline void		appSRandInit( INT Seed )		{ GSRandSeed = Seed; }
inline FLOAT	appFractional( FLOAT Value )	{ return Value - appTruncFloat( Value ); }

/**
 * Counts the number of leading zeros in the bit representation of the value
 *
 * @param Value the value to determine the number of leading zeros for
 *
 * @return the number of zeros before the first "on" bit
 */
FORCEINLINE DWORD appCountLeadingZeros(DWORD Value)
{
	if (Value == 0)
	{
		return 32;
	}
	
	DWORD NumZeros = 0;
	
	while ((Value & 0x80000000) == 0)
	{
		++NumZeros;
		Value <<= 1;
	}
	
	return NumZeros;
}

/**
 * Computes the base 2 logarithm for an integer value that is greater than 0.
 * The result is rounded down to the nearest integer.
 *
 * @param Value the value to compute the log of
 */
inline DWORD appFloorLog2(DWORD Value)
{
	return 31 - appCountLeadingZeros(Value);
}

inline INT appRound( FLOAT F )
{
	return appTrunc(roundf(F));
}

inline INT appFloor( FLOAT F )
{
	return appTrunc(floorf(F));
}

inline FLOAT appInvSqrt( FLOAT F )
{
	return 1.0f / sqrtf(F);
}

/**
 * Fast inverse square root using the estimate intrinsic with Newton-Raphson refinement
 * Accurate to at least 0.00000001 of appInvSqrt() and 2.45x faster
 *
 * @param F the float to estimate the inverse square root of
 *
 * @return the inverse square root
 */
inline FLOAT appInvSqrtEst(FLOAT F)
{
	// !!! FIXME?  Can Android do this faster?
    return appInvSqrt(F);
}

inline FLOAT appSqrt( FLOAT F )
{
	return sqrtf(F);
}

//#define appAlloca(size) _alloca((size+7)&~7)
#define appAlloca(size) ((size==0) ? 0 : alloca((size+7)&~7))

#define DEFINED_appSeconds 1

extern DOUBLE GSecondsPerCycle;
extern QWORD GTicksPerSeconds;
extern QWORD GNumTimingCodeCalls;

inline DOUBLE appSeconds()
{
#if !FINAL_RELEASE
	GNumTimingCodeCalls++;
#endif
	struct timeval tv;
	gettimeofday( &tv, NULL );
	return ((DOUBLE) tv.tv_sec) + (((DOUBLE) tv.tv_usec) / 1000000.0);
}


inline DWORD appCycles()
{
#if !FINAL_RELEASE
	GNumTimingCodeCalls++;
#endif
	struct timeval tv;
	gettimeofday( &tv, NULL );
	return (DWORD) ((((QWORD)tv.tv_sec) * 1000000ULL) + (((QWORD)tv.tv_usec)));
}

/*----------------------------------------------------------------------------
	Misc functions.
----------------------------------------------------------------------------*/

/** @return True if called from the rendering thread. */
extern UBOOL IsInRenderingThread();

/** @return True if called from the game thread. */
extern UBOOL IsInGameThread();

/** Used to cause runtime feature level change */
extern void appUpdateFeatureLevelChangeFromMainThread();

/** Used to cause a shader recompile if the eglContext was destroyed by the OS */
extern void appRecompilePreprocessedShaders();

#include "UMemoryDefines.h"

/**
* Converts the passed in program counter address to a human readable string and appends it to the passed in one.
* @warning: The code assumes that HumanReadableString is large enough to contain the information.
*
* @param	ProgramCounter			Address to look symbol information up for
* @param	HumanReadableString		String to concatenate information with
* @param	HumanReadableStringSize size of string in characters
* @param	VerbosityFlags			Bit field of requested data for output. -1 for all output.
*/ 
void appProgramCounterToHumanReadableString( QWORD ProgramCounter, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, EVerbosityFlags VerbosityFlags = VF_DISPLAY_ALL );

/**
 * Capture a stack backtrace and optionally use the passed in exception pointers.
 *
 * @param	BackTrace			[out] Pointer to array to take backtrace
 * @param	MaxDepth			Entries in BackTrace array
 * @param	Context				Optional thread context information
 * @return	Number of function pointers captured
 */
DWORD appCaptureStackBackTrace( QWORD* BackTrace, DWORD MaxDepth, FThreadContext* Context = NULL );

/**
 * Handles IO failure by ending gameplay.
 *
 * @param Filename	If not NULL, name of the file the I/O error occurred with
 */
void appHandleIOFailure( const TCHAR* Filename );

/** 
 * Returns whether the line can be broken between these two characters
 */
UBOOL appCanBreakLineAt( TCHAR Previous, TCHAR Current );

/**
 * Enforces strict memory load/store ordering across the memory barrier call.
 */
FORCEINLINE void appMemoryBarrier()
{
	__sync_synchronize();
}

/** 
 * Support functions for overlaying an object/name pointer onto an index (like in script code
 */
inline ScriptPointerType appPointerToSPtr(void* Pointer)
{
	return (ScriptPointerType)Pointer;
}

inline void* appSPtrToPointer(ScriptPointerType Value)
{
	return (void*)(PTRINT)Value;
}

/**
 * Retrieve a environment variable from the system
 *
 * @param VariableName The name of the variable (ie "Path")
 * @param Result The string to copy the value of the variable into
 * @param ResultLength The size of the Result string
 */
inline void appGetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, INT ResultLength)
{
	// !!! FIXME: Is this ok?
	// return an empty string
	*Result = 0;
}

/**
 * Push a marker for external profilers.
 *
 * @param MarkerName A descriptive name for the marker to display in the profiler
 */
inline void appPushMarker( const TCHAR* MarkerName )
{
}

/**
 * Pop the previous marker for external profilers.
 */
inline void appPopMarker( )
{
}

// Variable arguments.
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
INT appGetVarArgs( TCHAR* Dest, SIZE_T DestSize, INT Count, const TCHAR*& Fmt, va_list ArgPtr );

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
INT appGetVarArgsAnsi( ANSICHAR* Dest, SIZE_T DestSize, INT Count, const ANSICHAR*& Fmt, va_list ArgPtr );

/**
 * Return the system settings section name to use for the current running platform
 */
const TCHAR* appGetMobileSystemSettingsSectionName();

/**
 * Sets up feature levels based on queried device metrics
 */ 
void appDetermineDeviceFeatureLevels();

/*
 * Rechecks feature levels and adjusts accordingly 
 */
void appHandleFeatureLevelChange(int PerformanceLevel, float ResolutionScale);

/*----------------------------------------------------------------------------
	Extras
 ----------------------------------------------------------------------------*/

/**
 * Super early Android initialization
 */
void appAndroidInit(int argc, char* argv[]);

/**
 * Return the name of the texture format extension for thisd device
 */
const TCHAR* appGetAndroidTextureFormatName();

/**
 * Return the texture format used on this device
 */
DWORD appGetAndroidTextureFormat();

// URL that phone home stats are sent to.
// Internally, we send it to "et.epicgames.com". If you set up your own service, define it's URL here.
#if !defined(PHONE_HOME_URL)
#define PHONE_HOME_URL TEXT("tempuri.org")
#endif

const TCHAR* appGetAndroidPhoneHomeURL();

#endif // __UE3_ANDROID_H__
