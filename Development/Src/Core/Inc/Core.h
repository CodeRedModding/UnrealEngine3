/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
/*=============================================================================
	Core.h: Unreal core public header file.
=============================================================================*/

#ifndef _INC_CORE
#define _INC_CORE

// phantom definitions to help VAX parse our VARARG_* macros (VAX build 1440)
#ifdef VISUAL_ASSIST_HACK
	#define VARARG_DECL( FuncRet, StaticFuncRet, Return, FuncName, Pure, FmtType, ExtraDecl, ExtraCall ) FuncRet FuncName( ExtraDecl FmtType Fmt, ... )
	#define VARARG_BODY( FuncRet, FuncName, FmtType, ExtraDecl ) FuncRet FuncName( ExtraDecl FmtType Fmt, ... )
#endif // VISUAL_ASSIST_HACK

/*----------------------------------------------------------------------------
	Low level includes.
----------------------------------------------------------------------------*/

// Unwanted Intel C++ level 4 warnings/ remarks.
#if __ICL
#pragma warning(disable : 873)
#pragma warning(disable : 981)
#pragma warning(disable : 522)
#pragma warning(disable : 271)
#pragma warning(disable : 424)
#pragma warning(disable : 193)
#pragma warning(disable : 444)
#pragma warning(disable : 440)
#pragma warning(disable : 171)
#pragma warning(disable : 1125)
#pragma warning(disable : 488)
#pragma warning(disable : 858)
#pragma warning(disable : 82)
#pragma warning(disable : 1)
#pragma warning(disable : 177)
#pragma warning(disable : 279)
#endif

// note: If you add more platforms here, check out appNetworkNeedsByteSwapping().
// it will probably need to be fixed as well
#if XBOX || PS3 || IPHONE || ANDROID || NGP || WIIU || FLASH
#define CONSOLE					1
#define EXCEPTIONS_DISABLED		1
#define NO_BYTE_ORDER_SERIALIZE 1
#else
#define CONSOLE					0
#endif

// why are these here and not in UnBuild.h
#ifndef WITH_EDITORONLY_DATA
	#if CONSOLE || DEDICATED_SERVER
	#define WITH_EDITORONLY_DATA	0
	#else
	#define WITH_EDITORONLY_DATA	1
	#endif
#endif

// There aren't necessarily reliable platform defines, so build some.
#if FLASH
#define PLATFORM_LINUX   1
#define PLATFORM_UNIX    1
#elif ((defined __APPLE__) && (defined __MACH__)) && !IPHONE
#define PLATFORM_MACOSX  1
#define PLATFORM_UNIX    1
#elif IPHONE
#define PLATFORM_IPHONE  1
#define PLATFORM_UNIX    1
#elif ANDROID
#define PLATFORM_LINUX   1
#define PLATFORM_UNIX    1
#define PLATFORM_ANDROID 1
#elif CONSOLE
#elif _MSC_VER
#elif (defined(__linux__))
#define PLATFORM_LINUX   1
#define PLATFORM_UNIX    1
#elif (defined(unix))
#define PLATFORM_UNIX    1
#else
#error Please define your platform.
#endif

// Set up DESKTOP platforms
#if _WINDOWS || PLATFORM_MACOSX
#define PLATFORM_DESKTOP		1
#else
#define PLATFORM_DESKTOP		0
#endif

#if PLATFORM_MACOSX
#define EXCEPTIONS_DISABLED		1
#endif

/** Compile out some functionality/ book keeping if we don't support D3D10/D3D11 */
#define PLATFORM_SUPPORTS_D3D10_PLUS (PLATFORM_DESKTOP && !CONSOLE)

// set up MOBILE platforms (add any others here)
#if IPHONE || ANDROID || NGP || FLASH
#define MOBILE					1
#else
#define MOBILE					0
#endif

// Build options.
#include "UnBuild.h"

#if USE_AG_PERFMON
unsigned __int16 UnAgPmRegisterEvent(const char *name);
class AgPerfMonTimer
{
	unsigned __int16 PerfEventID;
public:
	AgPerfMonTimer(unsigned __int16 id);

	~AgPerfMonTimer(void);
};
#endif

// secure crt only for use on win32 and VC 8. disable otherwise
#if CONSOLE || _MSC_VER < 1400
#undef USE_SECURE_CRT
#define USE_SECURE_CRT 0
#endif

// GCC supports #pragma pack, but it overrides the crucial GCC_ALIGN(), so don't add it to this #if!
#if _MSC_VER || __ICC
	#define SUPPORTS_PRAGMA_PACK 1
#else
	#define SUPPORTS_PRAGMA_PACK 0
#endif


/**
 * Joins two arguments together even if they are macros
 * themselves (see sec 16.3.1 in the C++ standard).
 */
#define MACRO_JOIN(X,Y)     MACRO_DO_JOIN(X,Y)
#define MACRO_DO_JOIN(X,Y)  MACRO_DO_JOIN2(X,Y)
#define MACRO_DO_JOIN2(X,Y) X##Y

/**
 * Performs a compile-time assertion. Similar in functionality to Loki or Boost STATIC_CHECK.
 * This particular syntax was chosen over others because it allows the assertion to be used 
 * at namespace, class, or block scope (which Loki does not) and supports a custom error 
 * message (which Boost does not).
 *
 * The expression is carefully phrased to produce a compiler error that contains the msg provided.
 * In VC8 the expression checkAtCompileTime(false, MsgHere) will display:
 *
 *     error C2087: 'COMPILE_ERROR_MsgHere' : missing subscript
 *
 * NOTE: This doesn't currently work in gcc as its error message does not display the type.
 * Several variants of Loki or Boost techniques failed to achieve all of the goals desired,
 * so an implementation was chosen that resembles the VC version as closely as possible.
 *
 * For reasons like this static_assert will be added to C++0x:
 *   http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2004/n1720.html
 *
 * @param expr		Must be evaluatable at compile time.
 * @param msg		Message to be displayed. Somewhat obscured because we coerce
 *                  the compiler into displaying an error with msg present.
 */
#if _MSC_VER
	#define checkAtCompileTime(expr, msg)  typedef char COMPILE_ERROR_##msg[1][(expr)]
#else
	// gcc seems to ignore zero-sized arrays (which is non-conforming), but they are necessary 
	// to get VC8 to print out the error msg. gcc won't print out the error msg anyway.
	#define checkAtCompileTime(expr, msg)  typedef char COMPILE_ERROR_##msg[1][(expr)?1:-1]
#endif

/**
 * 32-bit and 64-bit checks
 */
#if defined(__x86_64__) || defined(_M_X64) || defined(__LP64__) || defined(__POWERPC64__) || defined( _WIN64 )
	#define PLATFORM_64BITS 1
	checkAtCompileTime(sizeof (void *) == 8, 64BitPlatformsShouldHave8BytePointers);
#elif defined(__i386__) || defined(_M_IX86) || defined(_M_PPC) || defined(__LP32__) || defined(__POWERPC__) || IPHONE || ANDROID || NGP || WIIU || FLASH
	#define PLATFORM_32BITS 1
	checkAtCompileTime(sizeof (void *) == 4, 32BitPlatformsShouldHave4BytePointers);
#else
	#error Please define your platform.
#endif


#if (_MSC_VER || __INTEL_COMPILER) && (defined(_M_X64) || defined(__x86_64__) || defined(__i386__) || defined(_M_IX86))
	#define __HAS_SSE__ 1
#else
	#define __HAS_SSE__ 0
#endif

#if IPHONE 
	#include <TargetConditionals.h>
#endif

// Enable vector intrinsics if possible
#ifndef ENABLE_VECTORINTRINSICS
	// @todo ngp: Reenable when the compiler bug is fixed, or a fast workaround is discovered
	#if ( PS3 || XBOX || /*NGP ||*/ WITH_IOS_5 || PLATFORM_MACOSX || ( __HAS_SSE__ && !__GNUC__ && !defined( __cplusplus_cli ) && !__INTEL_COMPILER ) )
		#if !FLASH
			/** Turning this on enables the cross-platform intrinsic code path in the engine. */
			#define ENABLE_VECTORINTRINSICS 1
		#endif
	#endif
#endif

// Override with 1 for platforms that supports high precision for thread timing functions such as appSleep().
#ifndef SUPPORTS_HIGH_PRECISION_THREAD_TIMING
#define SUPPORTS_HIGH_PRECISION_THREAD_TIMING 0
#endif

// Platform specific include.
#if XBOX
	#include "UnXenon.h"
#elif _MSC_VER
	#include "UnVcWin32.h"
#elif PS3
	#include "UnPS3.h"
#elif IPHONE
	#include "IPhone.h"
#elif PLATFORM_MACOSX
	#include "Mac.h"
#elif FLASH
	#include "Flash.h"
#elif ANDROID
	#include "Android.h"
#elif NGP
	#include "NGP.h"
#elif WIIU
	#include "WiiU.h"
#elif PLATFORM_UNIX
	#error No Unix support
#elif __MWERKS__
	#error No Metrowerks support
#elif __ICC
	#error No Intel compiler support
#else
	#error Unknown Compiler
#endif

#ifdef _DEBUG
#define FORCEINLINE_DEBUGGABLE inline
#else
#define FORCEINLINE_DEBUGGABLE FORCEINLINE
#endif

// make dummy versions of the inline pragmas if the platform didn't set it
#ifndef PRAGMA_DISABLE_INLINING
#define PRAGMA_DISABLE_INLINING	
#define PRAGMA_ENABLE_INLINING	
#endif


// Check support for variadic macros:
#if (GCC_VERSION > 030401) || (_MSC_VER >= 1400) || NGP || WIIU
	#define SUPPORTS_VARIADIC_MACROS	1
#elif _MSC_VER >= 1300
	//Visual Studio C++ 2003 doesn't support it, but there is a workaround:
	#pragma warning(disable: 4002)		// Warning: too many actual parameters for macro 'ident'
	#pragma warning(disable: 4003)		// Warning: not enough actual parameters for macro 'ident'
	#define SUPPORTS_VARIADIC_MACROS	0
	template <typename T>
	inline const T &		VARG( const T &t )				{ return t; }
	inline const TCHAR *	VARG( )							{ return TEXT(""); }
#else
	#define SUPPORTS_VARIADIC_MACROS	0
#endif
#if !(SUPPORTS_VARIADIC_MACROS || (_MSC_VER >= 1300))
	#error Compiler does not support variable number arguments to #define macros.
#endif

// CPU specific includes.
#if __INTEL__
#pragma warning(disable : 4799)
#include <xmmintrin.h>
#include <fvec.h>
#define PREFETCH(x) _mm_prefetch( (char*)(x), _MM_HINT_T0 );
#define CACHE_LINE_SIZE	128
#endif

#if XBOX
#define __HAS_ALTIVEC__ 1
#include "ppcintrinsics.h"
struct __vector4_c : public __vector4
{
	__vector4_c( FLOAT InX, FLOAT InY, FLOAT InZ, FLOAT InW )
	{
		v[0] = InX; v[1] = InY; v[2] = InZ; v[3] = InW;
	}
	__vector4_c( DWORD InX, DWORD InY, DWORD InZ, DWORD InW )
	{
		u[0] = InX; u[1] = InY; u[2] = InZ; u[3] = InW;
	}

};
#define PREFETCH(x) __dcbt( 0, (char*)(x) );
#define CACHE_LINE_SIZE	128
#endif

#if PS3
#define __HAS_ALTIVEC__ 1
#define PREFETCH(x) __dcbt( (char*)(x) );
#define CACHE_LINE_SIZE	128
#endif

#if ((!PS3) && (PLATFORM_UNIX) && (__POWERPC__ || __POWERPC64__))
#include <ppc_intrinsics.h>
#define __HAS_ALTIVEC__ 1
#define PREFETCH(x) __dcbt( 0, (char*)(x) );
#define FLUSH_CACHE_LINE(x)  __dcbf( 0, (char*)(x) );
#endif

#if NGP
#define PREFETCH(x)	__builtin_pld(x);
#define CACHE_LINE_SIZE	128
#endif


#ifndef PREFETCH
#define PREFETCH(x)
#define CACHE_LINE_SIZE	128
#endif

#define PREFETCH_NEXT_CACHE_LINE(x) PREFETCH( ((BYTE*)(x)) + CACHE_LINE_SIZE )

#if PS3 || XBOX
#define CONSOLE_PREFETCH PREFETCH
#define CONSOLE_PREFETCH_NEXT_CACHE_LINE PREFETCH_NEXT_CACHE_LINE
#else
#define CONSOLE_PREFETCH(x)
#define CONSOLE_PREFETCH_NEXT_CACHE_LINE(x)
#endif

#ifndef FLUSH_CACHE_LINE
#define FLUSH_CACHE_LINE(x)
#endif

#ifndef VTABLE_AT_END_OF_CLASS
#define VTABLE_AT_END_OF_CLASS 0
#endif

// Global constants.
enum {MINBYTE		= 0x00       };
enum {MINWORD		= 0x0000U    };
enum {MINDWORD		= 0x00000000U};
enum {MINSBYTE		= 0x80       };
enum {MINSWORD		= 0x8000     };
enum {MININT		= 0x80000000 };

enum {MAXBYTE		= 0xff       };
enum {MAXWORD		= 0xffffU    };
enum {MAXDWORD		= 0xffffffffU};
enum {MAXSBYTE		= 0x7f       };
enum {MAXSWORD		= 0x7fff     };
enum {MAXINT		= 0x7fffffff };
enum {INDEX_NONE	= -1         };
enum {UNICODE_BOM   = 0xfeff     };
enum ENoInit {E_NoInit = 0};
enum EForceInit {E_ForceInit = 0};

// Aspect ratio constants
const FLOAT AspectRatio4x3 = 1.33333f;
const FLOAT AspectRatio5x4 = 1.25f;
const FLOAT AspectRatio16x9 = 1.77778f;
const FLOAT InvAspectRatio4x3 = 0.75f;
const FLOAT InvAspectRatio5x4 = 0.8f;
const FLOAT InvAspectRatio16x9 = 0.56249f;

// Unicode character set mappings.
#ifndef _TCHAR_DEFINED
	typedef UNICHAR  TCHAR;
#endif

#ifndef _TEXT_DEFINED
#undef TEXT
#define TEXT(s) L##s
#endif

#ifndef _US_DEFINED
#undef US
#define US FString(TEXT(""))
#endif

// Does vswprintf want %ls instead of %s when displaying wide strings?
#define USE_LS_SPEC_FOR_UNICODE (PS3 || NGP || (PLATFORM_UNIX && !IPHONE) || WIIU)



inline TCHAR    FromAnsi   ( ANSICHAR In ) { return (BYTE)In;                        }
inline TCHAR    FromUnicode( UNICHAR In  ) { return In;                              }
inline ANSICHAR ToAnsi     ( TCHAR In    ) { 
// @todo flash: Is this really needed with 1 byte tchars?
    #ifdef FLASH
    return In;
    #else
    return (WORD)In<0x100 ? In : MAXSBYTE;
    #endif
}
inline UNICHAR  ToUnicode  ( TCHAR In    ) { return In;                              }

/*----------------------------------------------------------------------------
	Forward declarations.
----------------------------------------------------------------------------*/

// Objects.
class	UObject;
class		UComponent;
class		UExporter;
class		UFactory;
class		UField;
class			UConst;
class			UEnum;
class			UProperty;
class				UByteProperty;
class				UIntProperty;
class				UBoolProperty;
class				UFloatProperty;
class				UObjectProperty;
class					UComponentProperty;
class					UClassProperty;
class					UInterfaceProperty;
class				UNameProperty;
class				UStructProperty;
class               UStrProperty;
class               UArrayProperty;
class				UMapProperty;
class				UDelegateProperty;
class			UStruct;
class				UFunction;
class				UState;
class					UClass;
class				UScriptStruct;
class		ULinker;
class			ULinkerLoad;
class			ULinkerSave;
class		UPackage;
class		USubsystem;
class			USystem;
class		UTextBuffer;
class		UPackageMap;
class		UDebugger; //DEBUGGER
class		UObjectRedirector;

// Structs.
class FName;
class FArchive;
class FCompactIndex;
class FExec;
class FGuid;
class FMemStack;
class FPackageInfo;
class FTransactionBase;
class FUnknown;
class FRepLink;
class FString;
class FMalloc;
class FFilename;

// Globals.
extern class FOutputDevice*					GNull;
extern class FOutputDeviceRedirectorBase*	GLog;
extern class FOutputDevice*					GThrow;
extern class FOutputDeviceError*			GError;
extern class FFeedbackContext*				GWarn;

// EName definition.
#include "UnNames.h"


/** Breaks into the debugger.  Forces a GPF in non-debug builds.  Does nothing in shipping version. */

#if SHIPPING_PC_GAME || (FINAL_RELEASE && !FINAL_RELEASE_DEBUGCONSOLE)
		#define appIsDebuggerPresent()	FALSE
		#define appDebugBreak()			(void)1
#elif defined(_DEBUG)
	#if defined(XBOX)
		#define appIsDebuggerPresent()	TRUE
		#define appDebugBreak()			DebugBreak()
	#elif PS3
		#if PS3_SNC && !FINAL_RELEASE
			#define appIsDebuggerPresent	snIsDebuggerRunning
		#else
		#define appIsDebuggerPresent()	TRUE
		#endif // PS3_SNC
		#define appDebugBreak()			( *((INT*)3) = 13 )
	#elif IPHONE
		#define appIsDebuggerPresent()  TRUE
		#define appDebugBreak()			( *((INT*)3) = 13 )
	#elif PLATFORM_MACOSX
		#define appIsDebuggerPresent()  TRUE
		#define appDebugBreak()			( *((INT*)3) = 13 )
	#elif ANDROID
		#define appIsDebuggerPresent()  TRUE
		#define appDebugBreak()			( *((INT*)3) = 13 )
	#elif FLASH
		#define appIsDebuggerPresent()  TRUE
		#define appDebugBreak()			( *((INT*)3) = 13 )
	#elif _MSC_VER
		#define appIsDebuggerPresent	IsDebuggerPresent
		#define appDebugBreak()			( appIsDebuggerPresent() ? (DebugBreak(),1) : 1 )
	#elif PLATFORM_UNIX
		#define appIsDebuggerPresent	appUnixIsDebuggerPresent
		#define appDebugBreak()			( appUnixIsDebuggerPresent() ? appUnixDebugBreak() : 1 )
	#else
		#define appIsDebuggerPresent()	TRUE
		#define appDebugBreak()			( *((INT*)3) = 13 )
	#endif
#else
	#if defined(XBOX)
        #define appIsDebuggerPresent()	TRUE
		#define appDebugBreak()			( *((INT*)3) = 13 )
	#elif PS3
        #if PS3_SNC && !FINAL_RELEASE
			#define appIsDebuggerPresent	snIsDebuggerRunning
		#else
        #define appIsDebuggerPresent()	TRUE
		#endif // PS3_SNC
		#define appDebugBreak()			( *((INT*)3) = 13 )
	#elif IPHONE
		#define appIsDebuggerPresent()	TRUE
		#define appDebugBreak()			( *((INT*)3) = 13 )
	#elif PLATFORM_MACOSX
		#define appIsDebuggerPresent()  TRUE
		#define appDebugBreak()			( *((INT*)3) = 13 )
	#elif ANDROID
		#define appIsDebuggerPresent()	TRUE
		#define appDebugBreak()			( *((INT*)3) = 13 )
	#elif FLASH
		#define appIsDebuggerPresent()	TRUE
		#define appDebugBreak()			( *((INT*)3) = 13 )
	#elif _MSC_VER
		#define appIsDebuggerPresent	IsDebuggerPresent
		#define appDebugBreak()			( appIsDebuggerPresent() ? (DebugBreak(),1) : 1 )
	#elif PLATFORM_UNIX
		#define appIsDebuggerPresent	appUnixIsDebuggerPresent
		#define appDebugBreak()			( appUnixIsDebuggerPresent() ? *((INT*)3)=13 : 1 )
	#else
		#define appIsDebuggerPresent()	TRUE
		#define appDebugBreak()			( *((INT*)3) = 13 )
	#endif
#endif

#if PS3
// debug callstack helper function
extern void PS3Callstack(FThreadContext* Context = NULL);
#endif

/** This enables the new MouseLock and MouseCapture method */
#define USE_NEW_MOUSECAPTURE 1


// use a light-weight version of the PURE_VIRTUAL macro for FOutputDevice, necessary
// because of dependency issues
#if CHECK_PUREVIRTUALS
#define _PURE_VIRTUAL =0;
#else
#define _PURE_VIRTUAL { appDebugBreak(); }
#endif

// enable once UMapProperty is implemented
#define TMAPS_IMPLEMENTED 0

// whether object's ObjectArchetype must be the same class as the object
#define REQUIRES_SAMECLASS_ARCHETYPE	1

/*-----------------------------------------------------------------------------
	VarArgs helper macros.
-----------------------------------------------------------------------------*/
// @todo flash: huh?
#if !FLASH
#define GET_VARARGS(msg, msgsize, len, lastarg, fmt) \
{ \
	va_list ap; \
	va_start(ap, lastarg); \
	appGetVarArgs(msg, msgsize, len, fmt, ap); \
}
#define GET_VARARGS_ANSI(msg, msgsize, len, lastarg, fmt) \
{ \
	va_list ap; \
	va_start(ap, lastarg); \
	appGetVarArgsAnsi(msg, msgsize, len, fmt, ap); \
}
#define GET_VARARGS_RESULT(msg, msgsize, len, lastarg, fmt, result) \
{ \
	va_list ap; \
	va_start(ap, lastarg); \
	result = appGetVarArgs(msg, msgsize, len, fmt, ap); \
	if (result >= msgsize) \
	{ \
		result = -1; \
	} \
}
#define GET_VARARGS_RESULT_ANSI(msg, msgsize, len, lastarg, fmt, result) \
{ \
	va_list ap; \
	va_start(ap, lastarg); \
	result = appGetVarArgsAnsi(msg, msgsize, len, fmt, ap); \
	if (result >= msgsize) \
	{ \
		result = -1; \
	} \
}
#endif

/*-----------------------------------------------------------------------------
	Ugly VarArgs type checking (debug builds only).
-----------------------------------------------------------------------------*/

#define VARARG_EXTRA(A) A,
#define VARARG_NONE
#define VARARG_PURE =0

#if _MSC_VER

static inline DWORD			CheckVA(DWORD dw)		{ return dw; }
static inline BYTE			CheckVA(BYTE b)			{ return b; }
static inline UINT			CheckVA(UINT ui)		{ return ui; }
static inline INT			CheckVA(INT i)			{ return i; }
static inline QWORD			CheckVA(QWORD qw)		{ return qw; }
static inline SQWORD		CheckVA(SQWORD sqw)		{ return sqw; }
static inline DOUBLE		CheckVA(DOUBLE d)		{ return d; }
static inline TCHAR			CheckVA(TCHAR c)		{ return c; }
static inline void*			CheckVA(ANSICHAR* s)	{ return (void*)s; }
template<class T> T*		CheckVA(T* p)			{ return p; }
template<class T> const T*	CheckVA(const T* p)		{ return p; }

#define VARARG_DECL( FuncRet, StaticFuncRet, Return, FuncName, Pure, FmtType, ExtraDecl, ExtraCall )	\
	FuncRet FuncName##__VA( ExtraDecl FmtType Fmt, ... ) Pure;  \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt) {Return FuncName##__VA(ExtraCall (Fmt));} \
	template<class T1> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1) {T1 v1=CheckVA(V1);Return FuncName##__VA(ExtraCall (Fmt),(v1));} \
	template<class T1,class T2> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2));} \
	template<class T1,class T2,class T3> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3));} \
	template<class T1,class T2,class T3,class T4> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4));} \
	template<class T1,class T2,class T3,class T4,class T5> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15,class T16> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15,T16 V16) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);T16 v16=CheckVA(V16);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15),(v16));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15,class T16,class T17> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15,T16 V16,T17 V17) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);T16 v16=CheckVA(V16);T17 v17=CheckVA(V17);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15),(v16),(v17));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15,class T16,class T17,class T18> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15,T16 V16,T17 V17,T18 V18) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);T16 v16=CheckVA(V16);T17 v17=CheckVA(V17);T18 v18=CheckVA(V18);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15),(v16),(v17),(v18));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15,class T16,class T17,class T18,class T19> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15,T16 V16,T17 V17,T18 V18,T19 V19) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);T16 v16=CheckVA(V16);T17 v17=CheckVA(V17);T18 v18=CheckVA(V18);T19 v19=CheckVA(V19);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15),(v16),(v17),(v18),(v19));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15,class T16,class T17,class T18,class T19,class T20> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15,T16 V16,T17 V17,T18 V18,T19 V19,T20 V20) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);T16 v16=CheckVA(V16);T17 v17=CheckVA(V17);T18 v18=CheckVA(V18);T19 v19=CheckVA(V19);T20 v20=CheckVA(V20);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15),(v16),(v17),(v18),(v19),(v20));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15,class T16,class T17,class T18,class T19,class T20,class T21> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15,T16 V16,T17 V17,T18 V18,T19 V19,T20 V20, T21 V21) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);T16 v16=CheckVA(V16);T17 v17=CheckVA(V17);T18 v18=CheckVA(V18);T19 v19=CheckVA(V19);T20 v20=CheckVA(V20);T21 v21=CheckVA(V21);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15),(v16),(v17),(v18),(v19),(v20),(v21));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15,class T16,class T17,class T18,class T19,class T20,class T21,class T22> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15,T16 V16,T17 V17,T18 V18,T19 V19,T20 V20, T21 V21, T22 V22) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);T16 v16=CheckVA(V16);T17 v17=CheckVA(V17);T18 v18=CheckVA(V18);T19 v19=CheckVA(V19);T20 v20=CheckVA(V20);T21 v21=CheckVA(V21);T22 v22=CheckVA(V22);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15),(v16),(v17),(v18),(v19),(v20),(v21),(v22));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15,class T16,class T17,class T18,class T19,class T20,class T21,class T22,class T23> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15,T16 V16,T17 V17,T18 V18,T19 V19,T20 V20, T21 V21, T22 V22, T23 V23) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);T16 v16=CheckVA(V16);T17 v17=CheckVA(V17);T18 v18=CheckVA(V18);T19 v19=CheckVA(V19);T20 v20=CheckVA(V20);T21 v21=CheckVA(V21);T22 v22=CheckVA(V22);T23 v23=CheckVA(V23);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15),(v16),(v17),(v18),(v19),(v20),(v21),(v22),(v23));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15,class T16,class T17,class T18,class T19,class T20,class T21,class T22,class T23,class T24> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15,T16 V16,T17 V17,T18 V18,T19 V19,T20 V20, T21 V21, T22 V22, T23 V23, T24 V24) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);T16 v16=CheckVA(V16);T17 v17=CheckVA(V17);T18 v18=CheckVA(V18);T19 v19=CheckVA(V19);T20 v20=CheckVA(V20);T21 v21=CheckVA(V21);T22 v22=CheckVA(V22);T23 v23=CheckVA(V23);T24 v24=CheckVA(V24);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15),(v16),(v17),(v18),(v19),(v20),(v21),(v22),(v23),(v24));}



#define VARARG_BODY( FuncRet, FuncName, FmtType, ExtraDecl )		\
	FuncRet FuncName##__VA( ExtraDecl  FmtType Fmt, ... )

#else  // !_MSC_VER

#define VARARG_DECL( FuncRet, StaticFuncRet, Return, FuncName, Pure, FmtType, ExtraDecl, ExtraCall )	\
	FuncRet FuncName( ExtraDecl FmtType Fmt, ... ) Pure
#define VARARG_BODY( FuncRet, FuncName, FmtType, ExtraDecl )		\
	FuncRet FuncName( ExtraDecl FmtType Fmt, ... )

#endif // _MSC_VER



/*-----------------------------------------------------------------------------
	Abstract interfaces.
-----------------------------------------------------------------------------*/


// An output device.
class FOutputDevice
{
public:
	FOutputDevice(UBOOL bInAllowSuppression = TRUE)
		: bAllowSuppression(bInAllowSuppression),
		  bSuppressEventTag(FALSE),
		  bAutoEmitLineTerminator(TRUE)
	{}
	virtual ~FOutputDevice(){}

	// FOutputDevice interface.
	virtual void Serialize( const TCHAR* V, EName Event ) _PURE_VIRTUAL;
	virtual void Flush(){};

	/**
	 * Closes output device and cleans up. This can't happen in the destructor
	 * as we might have to call "delete" which cannot be done for static/ global
	 * objects.
	 */
	virtual void TearDown(){};

	void SetSuppressEventTag(UBOOL bInSuppressEventTag)
	{
		bSuppressEventTag = bInSuppressEventTag;
	}
	FORCEINLINE UBOOL GetSuppressEventTag()	{	return bSuppressEventTag;	}
	void SetAutoEmitLineTerminator(UBOOL bInAutoEmitLineTerminator)
	{
		bAutoEmitLineTerminator = bInAutoEmitLineTerminator;
	}
	FORCEINLINE UBOOL GetAutoEmitLineTerminator()	{	return bAutoEmitLineTerminator;	}

	// Simple text printing.
	void Log( const TCHAR* S );
	void Log( enum EName Type, const TCHAR* S );
	void Log( const FString& S );
	void Log( enum EName Type, const FString& S );
	VARARG_DECL( void, void, {}, Logf, VARARG_NONE, const TCHAR*, VARARG_NONE, VARARG_NONE );
	VARARG_DECL( void, void, {}, Logf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(enum EName E), VARARG_EXTRA(E) );
private:
	/** whether text sent through this output device can be suppressed (ignored) if the name passed for Type has the RF_Suppress flag set */
	UBOOL bAllowSuppression;

protected:
	/** Whether to output the 'Log: ' type front... */
	UBOOL bSuppressEventTag;
	/** Whether to output a line-terminator after each log call... */
	UBOOL bAutoEmitLineTerminator;
};

/**
 * Abstract base version of FOutputDeviceRedirector, needed due to order of dependencies.
 */
class FOutputDeviceRedirectorBase : public FOutputDevice
{
public:
	/**
	 * Adds an output device to the chain of redirections.	
	 *
	 * @param OutputDevice	output device to add
	 */
	virtual void AddOutputDevice( FOutputDevice* OutputDevice ) = 0;
	/**
	 * Removes an output device from the chain of redirections.	
	 *
	 * @param OutputDevice	output device to remove
	 */
	virtual void RemoveOutputDevice( FOutputDevice* OutputDevice ) = 0;
	/**
	 * Returns whether an output device is currently in the list of redirectors.
	 *
	 * @param	OutputDevice	output device to check the list against
	 * @return	TRUE if messages are currently redirected to the the passed in output device, FALSE otherwise
	 */
	virtual UBOOL IsRedirectingTo( FOutputDevice* OutputDevice ) = 0;

	/** Flushes lines buffered by secondary threads. */
	virtual void FlushThreadedLogs() = 0;

	/**
	 * Serializes the current backlog to the specified output device.
	 * @param OutputDevice	- Output device that will receive the current backlog
	 */
	virtual void SerializeBacklog( FOutputDevice* OutputDevice )
	{
	}

	/**
	 * Enables or disables the backlog.
	 * @param bEnable	- Starts saving a backlog if TRUE, disables and discards any backlog if FALSE
	 */
	virtual void EnableBacklog( UBOOL bEnable )
	{
	}

	/**
	 * Sets the current thread to be the master thread that prints directly
	 * (isn't queued up)
	 */
	virtual void SetCurrentThreadAsMasterThread() = 0;
};

// Error device.
class FOutputDeviceError : public FOutputDevice
{
public:
	virtual void HandleError()=0;
};

/**
 * This class servers as the base class for console window output.
 */
class FOutputDeviceConsole : public FOutputDevice
{
public:
	/**
	 * Shows or hides the console window. 
	 *
	 * @param ShowWindow	Whether to show (TRUE) or hide (FALSE) the console window.
	 */
	virtual void Show( UBOOL ShowWindow )=0;

	/** 
	 * Returns whether console is currently shown or not
	 *
	 * @return TRUE if console is shown, FALSE otherwise
	 */
	virtual UBOOL IsShown()=0;

	/**
	 * Returns whether the console has been inherited (TRUE) or created (FALSE)
	 *
	 * @return TRUE if console is inherited, FALSE if it was created
     */
	virtual UBOOL IsInherited() const { return FALSE; }
};

/*-----------------------------------------------------------------------------
	Logging and critical errors.
-----------------------------------------------------------------------------*/

/** @name Logging and critical errors */
//@{
/**
 * Requests application exit.
 *
 * @param	Force	If true, perform immediate exit (dangerous because config code isn't flushed, etc).
 *                  If false, request clean main-loop exit from the platform specific code.
 */
void appRequestExit( UBOOL Force );

/** Sends a message to the debugging output. */
void appOutputDebugString( const TCHAR *Message );

/** Sends a formatted message to the debugging output. */
void VARARGS appOutputDebugStringf( const TCHAR *Format, ... );

/** Sends a message to a remote tool. */
void appSendNotificationString( const ANSICHAR *Message );

/** Sends a message to a remote tool. */
void appSendNotificationString( const TCHAR* Message );

/** Sends a formatted message to a remote tool. */
void VARARGS appSendNotificationStringf( const ANSICHAR *Format, ... );

/** Failed assertion handler.  Warning: May be called at library startup time. */
void VARARGS appFailAssertFunc( const ANSICHAR* Expr, const ANSICHAR* File, INT Line, const TCHAR* Format=TEXT(""), ... );

/** Failed assertion handler.  This version only calls appOutputDebugString. */
void VARARGS appFailAssertFuncDebug( const ANSICHAR* Expr, const ANSICHAR* File, INT Line, const TCHAR* Format=TEXT(""), ... );
void VARARGS appFailAssertFuncDebug( const ANSICHAR* Expr, const ANSICHAR* File, INT Line, enum EName Type, const TCHAR* Format=TEXT(""), ... );

#if SUPPORTS_VARIADIC_MACROS
#if __INTEL_COMPILER
	#define appFailAssert(expr,file,line,...)				{ if (appIsDebuggerPresent()) appFailAssertFuncDebug(expr, file, line, __VA_ARGS__); appDebugBreak(); appFailAssertFunc(expr, file, line, __VA_ARGS__); }
#else
	#define appFailAssert(expr,file,line,...)				{ if (appIsDebuggerPresent()) appFailAssertFuncDebug(expr, file, line, ##__VA_ARGS__); appDebugBreak(); appFailAssertFunc(expr, file, line, ##__VA_ARGS__); }
#endif
#elif _MSC_VER >= 1300
	#define appFailAssert(expr,file,line,a,b,c,d,e,f,g,h)	{ if (appIsDebuggerPresent()) appFailAssertFuncDebug(expr,file,line,VARG(a),VARG(b),VARG(c),VARG(d),VARG(e),VARG(f),VARG(g),VARG(h)); appDebugBreak(); appFailAssertFunc(expr,file,line,VARG(a),VARG(b),VARG(c),VARG(d),VARG(e),VARG(f),VARG(g),VARG(h)); }
#endif


/**
 * Called when an 'ensure' assertion fails; gathers stack data and generates and error report.
 *
 * @param	Expr	Code expression ANSI string (#code)
 * @param	File	File name ANSI string (__FILE__)
 * @param	Line	Line number (__LINE__)
 * @param	Msg		Informative error message text
 */
void appFailEnsure( const ANSICHAR* Expr, const ANSICHAR* File, INT Line, const TCHAR* Msg );


/**
 * Triggers a (potentially) non-fatal, non-intrusive error if bExpressionResult is zero
 *
 * @param	bExpressionResult	Expression that will trigger an error if resolves to zero
 * @param	Expr	Code expression ANSI string (#code)
 * @param	File	File name ANSI string (__FILE__)
 * @param	Line	Line number (__LINE__)
 * @param	Msg		Optional informative error message text
 */
inline static UBOOL appEnsureNotFalse( UBOOL bExpressionResult, const ANSICHAR* Expr, const ANSICHAR* File, INT Line, const TCHAR* Msg = TEXT( "" ) )
{
	if( bExpressionResult == 0 )
	{
		appFailEnsure( Expr, File, Line, Msg );
	}
	
	return bExpressionResult;
}


/**
 * Triggers a (potentially) non-fatal, non-intrusive error if bExpressionResult is zero.  Takes a formatted string.
 *
 * @param	bExpressionResult	Expression that will trigger an error if resolves to zero
 * @param	Expr	Code expression ANSI string (#code)
 * @param	File	File name ANSI string (__FILE__)
 * @param	Line	Line number (__LINE__)
 * @param	FormattedMsg	Informative error message text with variable args
 */
inline static UBOOL VARARGS appEnsureNotFalseFormatted( UBOOL bExpressionResult, const ANSICHAR* Expr, const ANSICHAR* File, INT Line, const TCHAR* FormattedMsg, ... )
{
	const INT TempStrSize = 4096;
	TCHAR TempStr[ TempStrSize ];
	GET_VARARGS( TempStr, TempStrSize, TempStrSize - 1, FormattedMsg, FormattedMsg );

	if( bExpressionResult == 0 )
	{
		appFailEnsure( Expr, File, Line, TempStr );
	}
	
	return bExpressionResult;
}



/**
 * Returns the last system error code in string form.  NOTE: Only one return value is valid at a time!
 *
 * @param OutBuffer the buffer to be filled with the error message
 * @param BufferLength the size of the buffer in character count
 * @param Error the error code to convert to string form
 */
const TCHAR* appGetSystemErrorMessage(TCHAR* OutBuffer,INT BufferCount,INT Error = 0);
/** Pops up a message dialog box containing the input string. */
void appDebugMessagef( const TCHAR* Fmt, ... );
/** Pops up a message dialog box containing the last system error code in string form. */
void appGetLastError( void );

/** Wrapper for appDebugBreak() - used while debugging for calls that should be removed prior to checking code in */
inline void DebugRemoveMe() { appDebugBreak(); }

/** Returns a unique Runtime ID, always > 0. */
QWORD appCreateRuntimeUID();

/** A function that does nothing. Allows for a default behavior for callback function pointers. */
void appNoop();

// Define NO_LOGGING to strip out all writing to log files, OutputDebugString(), etc.
// This is needed for consoles that require no logging (Xbox, Xenon)
#if NO_LOGGING || (FINAL_RELEASE && !PLATFORM_DESKTOP)
	#if COMPILER_SUPPORTS_NOOP
		// MS compilers support noop which discards everything inside the parens
		#define debugf			__noop
		#define warnf			__noop
		#define KISMET_LOG		__noop
		#define KISMET_WARN		__noop
		#define KISMET_LOG_REF(obj)		__noop
		#define KISMET_WARN_REF(obj)	__noop
	#else
		#pragma message("Logging can only be disabled on MS compilers")
		#define debugf(...)
		#define warnf(...)
		#define KISMET_LOG(...)
		#define KISMET_WARN(...)
		#define KISMET_LOG_REF(...)
		#define KISMET_WARN_REF(...)
	#endif
#else
	#define debugf				GLog->Logf
	#define warnf				GWarn->Logf

	#if CONSOLE
		#if COMPILER_SUPPORTS_NOOP
			#define KISMET_LOG	__noop
			#define KISMET_WARN	__noop
			#define KISMET_LOG_REF(obj)		__noop
			#define KISMET_WARN_REF(obj)	__noop
		#else
			#define KISMET_LOG(...)
			#define KISMET_WARN(...)
			#define KISMET_LOG_REF(...)
			#define KISMET_WARN_REF(...)
		#endif
	#else
		#define KISMET_LOG		GetRootSequence()->ScriptLogf
		#define KISMET_WARN		GetRootSequence()->ScriptWarnf
		#define KISMET_LOG_REF(obj) obj->GetRootSequence()->ScriptLogf
		#define KISMET_WARN_REF(obj) obj->GetRootSequence()->ScriptWarnf
	#endif
#endif

#if DO_LOG_SLOW
	#define debugfSlow			GLog->Logf
#else
	#if COMPILER_SUPPORTS_NOOP
		// MS compilers support noop which discards everything inside the parens
		#define debugfSlow		__noop
	#else
		#define debugfSlow(...)
	#endif
#endif

#if SUPPORTS_VARIADIC_MACROS
#if __INTEL_COMPILER
#define appErrorf(...)				( (appIsDebuggerPresent() ? appFailAssertFuncDebug("appErrorf", __FILE__, __LINE__, __VA_ARGS__),1 : 1), appDebugBreak(), GError->Logf(__VA_ARGS__), 1 )
#else
#define appErrorf(...)				( (appIsDebuggerPresent() ? appFailAssertFuncDebug("appErrorf", __FILE__, __LINE__, ##__VA_ARGS__),1 : 1), appDebugBreak(), GError->Logf(__VA_ARGS__), 1 )
#endif
#elif _MSC_VER >= 1300
#define appErrorf(a,b,c,d,e,f,g,h)	( (appIsDebuggerPresent() ? appFailAssertFuncDebug("appErrorf", __FILE__, __LINE__, VARG(a),VARG(b),VARG(c),VARG(d),VARG(e),VARG(f),VARG(g),VARG(h)),1 : 1), appDebugBreak(), GError->Logf(VARG(a),VARG(b),VARG(c),VARG(d),VARG(e),VARG(f),VARG(g),VARG(h)), 1 )
#else
#error need to define appErrorf
#endif

#if DO_GUARD_SLOW
	#define appErrorfDebug		GError->Logf
#else
	#if COMPILER_SUPPORTS_NOOP
		// MS compilers support noop which discards everything inside the parens
		#define appErrorfDebug	__noop
	#else
		#define appErrorfDebug(...)
	#endif
#endif

#if SUPPORT_SUPPRESSED_LOGGING
	#define debugfSuppressed		GLog->Logf
	#define appErrorfSuppressed		GError->Logf
#else
	#if COMPILER_SUPPORTS_NOOP
		// MS compilers support noop which discards everything inside the parens
		#define debugfSuppressed	__noop
		#define appErrorfSuppressed	__noop
	#else
		#define debugfSuppressed(...)
		#define appErrorfSuppressed(...)
	#endif
#endif
//@}

#if NO_LOGGING || FINAL_RELEASE || !defined(_DEBUG)
#define ENABLE_SCRIPT_TRACING 0
#else
#define ENABLE_SCRIPT_TRACING 1
#endif


// This define must come after the class declaration for FOutputDevice, since the macro uses GError->Logf
#if CHECK_PUREVIRTUALS
	#define PURE_VIRTUAL(func,extra) =0;
#else
	#define PURE_VIRTUAL(func,extra) { appErrorf(TEXT("Pure virtual not implemented (%s)"), TEXT(#func)); extra }
#endif

// undefine the simpler version of the PURE_VIRTUAL macro so that
// no one accidentally uses the wrong version
#ifdef _PURE_VIRTUAL
	#undef _PURE_VIRTUAL
#endif

// Any object that is capable of taking commands.
class FExec
{
public:
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar ) PURE_VIRTUAL(FExec::Exec,return FALSE;)
};

//
// Container forward declarations.
//

/** branchless pointer selection
* return A ? A : B;
**/
template<typename ReferencedType>
ReferencedType* IfAThenAElseB(ReferencedType* A,ReferencedType* B);

/** branchless pointer selection based on predicate
* return PTRINT(Predicate) ? A : B;
**/
template<typename PredicateType,typename ReferencedType>
ReferencedType* IfPThenAElseB(PredicateType Predicate,ReferencedType* A,ReferencedType* B);

FORCEINLINE UINT appRoundUpToPowerOfTwo( UINT Minimum );

#include "ContainerAllocationPolicies.h"

template<typename ElementType,typename Allocator = FDefaultAllocator>
class TBaseArray;

template<typename T,typename Allocator = FDefaultAllocator>
class TArray;

template<typename T>
class TArrayNoInit;

template<typename T>
class TTransArray;

template<typename KeyType,typename ValueType,typename SetAllocator = FDefaultSetAllocator >
class TMap;

template<typename KeyType,typename ValueType,typename SetAllocator = FDefaultSetAllocator >
class TMultiMap;

/** The type that's used to access dynamic array from UnrealScript. */
class FScriptArray;

/**
 * Exec handler that registers itself and is being routed via UObject::StaticExec
 */
class FSelfRegisteringExec : public FExec
{
public:
	/** Constructor, registering this instance. */
	FSelfRegisteringExec();
	/** Destructor, unregistering this instance. */
	virtual ~FSelfRegisteringExec();

private:
	friend class UObject;
	/** Array of registered exec's routed via UObject::StaticExec. */
	static TArrayNoInit<FSelfRegisteringExec*> RegisteredExecs;
};


#define CONFIG_NO_USER -1

// Configuration database cache.
// Forward declarations - find in FConfigCacheIni.h
class FConfigCacheIni;
class FConfigFile;

// Notification hook.
class FNotifyHook
{
public:
	virtual void NotifyDestroy( void* Src ) {}
	virtual void NotifyPreChange( void* Src, UProperty* PropertyAboutToChange ) {}
	virtual void NotifyPostChange( void* Src, UProperty* PropertyThatChanged ) {}
	virtual void NotifyPreChange( void* Src, class FEditPropertyChain* PropertyAboutToChange );
	virtual void NotifyPostChange( void* Src, class FEditPropertyChain* PropertyThatChanged );
	virtual void NotifyExec( void* Src, const TCHAR* Cmd ) {}
};

// Interface for returning a context string.
class FContextSupplier
{
public:
	virtual FString GetContext()=0;
};

// Class for handling undo/redo transactions among objects.
typedef void( *STRUCT_AR )( FArchive& Ar, void* TPtr );
typedef void( *STRUCT_DTOR )( void* TPtr );

/**
 * Baseclass for transactions.
 */
class FTransactionBase
{
public:
	virtual void SaveObject( UObject* Object )=0;
	virtual void SaveArray( UObject* Object, FScriptArray* Array, INT Index, INT Count, INT Oper, INT ElementSize, STRUCT_AR Serializer, STRUCT_DTOR Destructor )=0;

	/**
	 * Enacts the transaction.
	 *
	 */
	virtual void Apply()=0;
};

// File manager.
enum EFileTimes
{
	FILETIME_Create				= 0,
	FILETIME_LastAccess			= 1,
	FILETIME_LastWrite			= 2,
};
enum EFileWrite
{
	FILEWRITE_NoFail            = 0x01,
	FILEWRITE_NoReplaceExisting = 0x02,
	FILEWRITE_EvenIfReadOnly    = 0x04,
	FILEWRITE_Append			= 0x08,
	FILEWRITE_AllowRead         = 0x10,
	FILEWRITE_SaveGame			= 0x20,		// The platform needs to create a platform-specific save game file. The passed in path is relative to the "base" of savegames for that platform
	FILEWRITE_Async				= 0x40,		// Attempt to create an async file archive (use the FArchive::IsCloseComplete to tell if it's completed). Supported in only a VERY small set of cases
};
enum EFileRead
{
	FILEREAD_NoFail             = 0x01,
	FILEREAD_SaveGame			= 0x02,		// The platform needs to load a platform-specific save game file. The passed in path is relative to the "base" of savegames for that platform
	FILEREAD_Silent				= 0x04,
};
enum ECopyResult
{
	COPY_OK						= 0x00,
	COPY_MiscFail				= 0x01,
	COPY_ReadFail				= 0x02,
	COPY_WriteFail				= 0x03,
	COPY_Canceled				= 0x06,
};
#define COMPRESSED_EXTENSION	TEXT(".uz2")

struct FCopyProgress
{
	virtual UBOOL Poll( FLOAT Fraction )=0;
};


/** Helper structure encapsulating file and stats handle. */
struct FFileHandle
{
	/** Default constructor, initializing all member variables. */
	FFileHandle()
	:	Handle( PTRINT(INDEX_NONE) )
	,	Info( PTRINT(INDEX_NONE) )
	{}
	/** Constructor. */
	FFileHandle( PTRINT InHandle, PTRINT InInfo )
	:	Handle( InHandle )
	,	Info( InInfo )
	{}

	/** Platform-specific file handle. */
	PTRINT	Handle;
	/** Optional payload for additional information about the file. */
	PTRINT	Info;

	/** Returns TRUE if this is a valid handle to an opened file. */
	UBOOL IsValid()
	{
		return Handle != PTRINT(INDEX_NONE);
	}

	/** Resets the handle to an invalid state. */
	void Invalidate()
	{
		Handle = PTRINT(INDEX_NONE);
		Info = PTRINT(INDEX_NONE);
	}
};

enum EFileOpenFlags
{
	IO_READ			= 0x01,					// Open for reading
	IO_WRITE		= 0x02,					// Open for writing
	IO_READWRITE	= IO_READ | IO_WRITE,	// Combination of reading and writing
	IO_APPEND		= 0x40,					// When writing, keep the existing data, set the filepointer to the end of the existing data
};

enum EFileSeekFlags
{
	IO_SEEK_BEGIN,
	IO_SEEK_CURRENT,
	IO_SEEK_END
};

class FFileManager
{
private:
	/** Set to TRUE if the file manager has been initialized. */
	UBOOL bIsInitialized;

public:

	/** Timestamp structure */
	struct FTimeStamp
	{
		// Time is in UTC
		INT     Year;           /* year                             */
		INT     Month;          /* months since January - [0,11]    */
		INT     Day;            /* day of the month - [1,31]        */
		INT     Hour;           /* hours since midnight - [0,23]    */
		INT     Minute;         /* minutes after the hour - [0,59]  */
		INT     Second;         /* seconds after the minute - [0,59]*/
		INT     DayOfWeek;      /* days since Sunday - [0,6]        */

		INT     GetJulian     ( void )         const;
		INT     GetSecondOfDay( void )         const;
		UBOOL   operator< ( const FTimeStamp& Other ) const;
		UBOOL   operator> ( const FTimeStamp& Other ) const;        
		UBOOL   operator==( const FTimeStamp& Other ) const;        
		UBOOL   operator>=( const FTimeStamp& Other ) const;        
		UBOOL   operator<=( const FTimeStamp& Other ) const;        
		UBOOL   operator!=( const FTimeStamp& Other ) const;        

		/**
		  * Converts a timestamp to a string.
		  *
		  * @param Timestamp The timestamp to convert to a string
		  * @param [out] Output The FString to place the formatted timestamp in
		  */
		static void TimestampToFString(const FTimeStamp& Timestamp, FString& Output);

		/**
		  * Converts a string to a timestamp.
		  * Note: No error checking is done and this function should only be called on the output from TimestampToFString.
		  *
		  * @param TimestampAsString A string containing a timestamp, created by TimestampToFString()
		  * @param [out] Output	The timestamp represented by the input string
		  */
		static void FStringToTimestamp(const FString& TimestampAsString, FTimeStamp& Output);

		/** Generate an FTimestamp that represents the current system time. */
		static void GetTimestampFromCurrentTime(FTimeStamp& Output);
	};

	/** Construtor. */
	FFileManager() : bIsInitialized(FALSE) {}

	/** Gets a value indicating whether or not the file manager has been initialized. */
	inline UBOOL IsInitialized() const { return bIsInitialized; }
	/**
	 * Initialize the file manager _before_ the commandline has been set up
	 */
	virtual void PreInit() {}
	virtual void Init(UBOOL Startup) { bIsInitialized = TRUE; }
	virtual FArchive* CreateFileReader( const TCHAR* Filename, DWORD ReadFlags=0, FOutputDevice* Error=GNull )=0;

	virtual FArchive* CreateFileWriter( const TCHAR* Filename, DWORD WriteFlags=0, FOutputDevice* Error=GNull, INT MaxFileSize=0 )=0;

	// If you're writing to a debug file, you should use CreateDebugFileWriter, and wrap the calling code in #if ALLOW_DEBUG_FILES.
#if ALLOW_DEBUG_FILES
	FArchive* CreateDebugFileWriter(const TCHAR* Filename, DWORD WriteFlags=0, FOutputDevice* Error=GNull, INT MaxFileSize=0 )
	{
		return CreateFileWriter(Filename,WriteFlags,Error,MaxFileSize);
	}
#endif

	/**
	 * If the given file is compressed, this will return the size of the uncompressed file,
	 * if the platform supports it.
	 * @param Filename Name of the file to get information about
	 * @return Uncompressed size if known, otherwise -1
	 */
	virtual INT UncompressedFileSize( const TCHAR* Filename )=0;

	virtual UBOOL IsReadOnly( const TCHAR* Filename )=0;
	virtual UBOOL Delete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 )=0;
	virtual DWORD Copy( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0, FCopyProgress* Progress=NULL )=0;
	virtual UBOOL Move( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0 )=0;

	/**
	 * Updates the modification time of the file on disk to right now, just like the unix touch command
	 * @param Filename Path to the file to touch
	 * @return TRUE if successful
	 */
	virtual UBOOL TouchFile(const TCHAR* Filename)=0;

	/**
	 * Creates filenames belonging to the set  "Base####.Extension" where #### is a 4-digit number in [0000-9999] and
	 * no file of that name currently exists.  The return value is the index of first empty filename, or -1 if none
	 * could be found.  Clients that call FindAvailableFilename repeatedly will want to cache the result and pass it
	 * in to the next call thorugh the StartVal argument.  Base and Extension must valid pointers.
	 * Example usage:
	 * \verbatim
	   // Get a free filename of form <appGameDir>/SomeFolder/SomeFilename####.txt
	   FString Output;
	   FindAvailableFilename( *(appGameDir() * TEXT("SomeFolder") * TEXT("SomeFilename")), TEXT("txt"), Output );
	   \enverbatim
	 *
	 * @param	Base			Filename base, optionally including a path.
	 * @param	Extension		File extension.
	 * @param	OutFilename		[out] A free filename (untouched on fail).
	 * @param	StartVal		[opt] Can be used to hint beginning of index search.
	 * @return					The index of the created file, or -1 if no free file with index (StartVal, 9999] was found.
	 */
	virtual INT FindAvailableFilename( const TCHAR* Base, const TCHAR* Extension, FString& OutFilename, INT StartVal=-1 )=0;

	virtual UBOOL MakeDirectory( const TCHAR* Path, UBOOL Tree=0 )=0;
	virtual UBOOL DeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 )=0;
	virtual void FindFiles( TArray<FString>& FileNames, const TCHAR* Filename, UBOOL Files, UBOOL Directories )=0;
	virtual DOUBLE GetFileAgeSeconds( const TCHAR* Filename )=0;
	virtual DOUBLE GetFileTimestamp( const TCHAR* Filename )=0;
	virtual UBOOL SetDefaultDirectory()=0;
	virtual UBOOL SetCurDirectory( const TCHAR* Directory )=0;
	virtual FString GetCurrentDirectory()=0;
	/** 
	 * Get the timestamp for a file
	 * @param Path Path for file
	 * @TimeStamp Output time stamp
	 * @return success code
	 */
	virtual UBOOL GetTimestamp( const TCHAR* Path, FTimeStamp& TimeStamp )=0;

	/**
	 * Converts passed in filename to use a relative path.
	 *
	 * @param	Filename	filename to convert to use a relative path
	 * 
	 * @return	filename using relative path
	 */
	virtual FString ConvertToRelativePath( const TCHAR* Filename );

	/**
	 * Converts passed in filename to use an absolute path.
	 *
	 * @param	Filename	filename to convert to use an absolute path, safe to pass in already using absolute path
	 * 
	 * @return	filename using absolute path
	 */
	virtual FString ConvertToAbsolutePath( const TCHAR* Filename );
	/**
	 * Converts a path pointing into the installed directory to a path that a least-privileged user can write to 
	 *
	 * @param AbsolutePath Source path to convert
	 *
	 * @return Path to the user directory
	 */
	virtual FString ConvertAbsolutePathToUserPath(const TCHAR* AbsolutePath);

	/**
	 *	Converts the platform-independent Unreal filename into platform-specific full path. (Thread-safe)
	 *
	 *	@param Filename		Platform-independent Unreal filename
	 *	@return				Platform-dependent full filepath
	 **/
	virtual FString GetPlatformFilepath( const TCHAR* Filename );

	/**
	 *	Returns the size of a file. (Thread-safe)
	 *
	 *	@param Filename		Platform-independent Unreal filename.
	 *	@return				File size in bytes or INDEX_NONE if the file didn't exist.
	 **/
	virtual INT FileSize( const TCHAR* Filename )=0;

	/**Enables file system drive addressing where appropriate (360)*/
	virtual void EnableLogging (void) {};

	/**
	 * Copies given UE3 path filename from the network host to the local disk, if 
	 * it is newer on the host
	 *
	 * @param Filename Filename as passed from UE3
	 *
	 * @return TRUE if the file is up to date and usable on the local disk
	 */
	virtual UBOOL EnsureFileIsLocal(const TCHAR* Filename)
	{ return TRUE; }
};

class FEdLoadError;

// NOTE: These are copied out of UnConsoleTools.h and must be identical for object propagation to work properly
// Core.h can't be included by UnConsoleTools.h because our external Tools*.dll's include that header file
// so we have to define it in 2 places =(

#ifndef INVALID_TARGETHANDLE
typedef void* TARGETHANDLE;
#define INVALID_TARGETHANDLE ((TARGETHANDLE)-1)
#endif

/**
 * Base abstract class for an object propagator. Specific subclasses will be written
 * for propagating object property changes from the editor into either the 
 * Play From Here window or a console connected across the network.
 */
class FObjectPropagator : public FExec
{
public:
	virtual UBOOL Connect() { return true; }
	virtual void Disconnect() {}
	virtual void OnPropertyChange(UObject* Object, UProperty* Property, INT PropertyOffset) {};
	virtual void OnObjectRename(UObject* Object, const TCHAR* NewName) {}
	virtual void OnActorMove(class AActor* Actor) {}
	virtual void OnActorCreate(class AActor* Actor) {}
	virtual void OnActorDelete(class AActor* Actor) {}

	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar ) { return 0; }
	virtual void Tick(FLOAT DeltaTime) {}

	virtual void AddTarget(TARGETHANDLE Target, DWORD RemoteIPAddress, UBOOL bIntelByteOrder) {}
	virtual void RemoveTarget(TARGETHANDLE Target) {}
	virtual UINT GetTargetCount() const { return 0; }
	virtual void ClearTargets() {}

	static void SetPropagator(FObjectPropagator* InPropagator);
	static void ClearPropagator();

	static void Pause();
	static void Unpause();

	/**
	 * Send an entire actor over the propagator, including any special properties
	 * @param Actor The actor to propagate
	 */
	virtual void PropagateActor(class AActor* Actor) {}

protected:
	static DWORD Paused;
};

/*----------------------------------------------------------------------------
	Global variables.
----------------------------------------------------------------------------*/

class FOutputDeviceRedirector;

// Core globals.
extern FMemStack				GMainThreadMemStack;
extern FConfigCacheIni*			GConfig;
extern FTransactionBase*		GUndo;
extern FOutputDeviceConsole*	GLogConsole;
extern FFileManager*			GFileManager;
extern USystem*					GSys;
extern UProperty*				GProperty;
/** Points to the UProperty currently being serialized */
extern UProperty*				GSerializedProperty;
/** Points to the main UObject currently being serialized */
extern UObject*					GSerializedObject;
/** Points to the main Linker currently being used for serialization by CreateImport() */
extern ULinkerLoad*				GSerializedImportLinker;
/** The main Import Index currently being used for serialization by CreateImport() */
extern INT						GSerializedImportIndex;
/** Points to the most recently used Linker for serialization by CreateExport() */
extern ULinkerLoad*				GSerializedExportLinker;
/** The most recently used export Index for serialization by CreateExport() */
extern INT						GSerializedExportIndex;
/** Points to the main PackageLinker currently being serialized */
extern ULinkerLoad*				GSerializedPackageLinker;
extern BYTE*					GPropAddr;
extern UObject*					GPropObject;
extern DWORD					GRuntimeUCFlags;
extern class UPropertyWindowManager*	GPropertyWindowManager;
//extern USubsystem*				GPropertyWindowManager;
#if !CONSOLE
extern TCHAR				    GErrorHist[16384];
#endif
extern TCHAR					GTrue[64], GFalse[64], GYes[64], GNo[64], GNone[64];
extern DOUBLE					GSecondsPerCycle;
extern DWORD					GUglyHackFlags;


/** Helper function to flush resource streaming. */
extern void						(*GFlushStreamingFunc)(void);

enum EUglyHackFlags
{
	HACK_DisableSubobjectInstancing		=	0x01,
	HACK_ClassLoadingDisabled			=	0x02,	// indicates that classes aren't being loaded from disk, thus all classes should be treated as intrinsic classes.
	HACK_UpdateArchetypeFromInstance	=	0x04,
	HACK_KeepSequenceObject				=	0x08,	// prevents the code that removes sequences objects from sequences from marking the object pending kill
	HACK_IsReloadObjArc					=	0x10,

	HACK_ForceLoadEditorOnly			=	0x20,	// force loading of editor-only property data, even in game

	/**
	 * prevents component instancing code from instancing new components from templates;
	 * useful in certain commandlets [like diffpackage] that want to look at data from disk only.
	 * does not prevent existing instanced components from being loaded from disk.
	 */
	HACK_DisableComponentCreation		=	0x40,

	/** Disable loading of objects not contained within script files; used during script compilation */
	HACK_VerifyObjectReferencesOnly		=	0x80,

	/** Bypassses an assert in StaticDuplicateObjectEx() */
	HACK_AllowDifferingClassSizesWhenDuplicatingObjects	= 0x100,

	HACK_SkipCopyDuringRegistration = 0x200, // when performing native registration, skip copying property values from the template when constructing default objects

	/** when constructing objects, use the fast path on consoles... */
	HACK_FastPathUniqueNameGeneration = 0x400, 

	/** When importing textures, don't require any size restrictions such as must be power of two or multiple of 4. (Used for creating texture atlases) */
	HACK_AllowAnySizeTextureImport = 0x800,
};


extern class FCallbackEventObserver*	GCallbackEvent;
extern class FCallbackQueryDevice*	GCallbackQuery;

/** 
 *	True if we are in the editor. 
 *	Note that this is still true when using Play In Editor. You may want to use GWorld->HasBegunPlay in that case.
 */
#if !CONSOLE
extern UBOOL					GIsEditor;
extern UBOOL					GIsImportingT3D;
extern UBOOL					GIsUCC;
extern UBOOL					GIsUCCMake;
extern UBOOL					GIsWatchingEndLoad;	/* Whether we should fire notification events when objects are loaded (see GCallbackEvent). */
extern UBOOL					GIsTransacting;
extern FLOAT					GVolumeMultiplier;
#else
#define GIsEditor				0
#define GIsUCC					0
#define GIsUCCMake				0
#define GVolumeMultiplier		( 1.0f )
#endif // CONSOLE

extern UBOOL					GEdSelectionLock;
extern UBOOL					GIsClient;
extern UBOOL					GIsServer;
extern UBOOL					GIsCriticalError;
extern UBOOL					GIsStarted;
extern UBOOL					GIsRunning;
extern UBOOL					GIsGarbageCollecting;
extern UBOOL					GIsReplacingObject;

/**
 * These are set when the engine first starts up.
 **/

#if XBOX
/** On the xbox setting thread names messes up the XDK COM API that UnrealConsole uses. Have them off by default. */
extern UBOOL					GSetThreadNames;
#endif

/**
 * This determines if we should pop up any dialogs.  If Yes then no popping up dialogs.
 **/
extern UBOOL					GIsUnattended;

/**
* This specifies whether the engine was launched as a build machine process.
**/
extern UBOOL					GIsBuildMachine;

/**
 * This determines if we should output any log text.  If Yes then no log text should be emitted.
 **/
extern UBOOL					GIsSilent;

/**
 * Used by non-UObject constructors of UObjects with multiple inheritance to
 * determine whether we're constructing the class default object
 */
extern UBOOL					GIsAffectingClassDefaultObject;
extern UBOOL					GIsSlowTask;
extern UBOOL					GSlowTaskOccurred;
extern UBOOL					GIsGuarded;
extern UBOOL					GIsRequestingExit;
extern class FGlobalMath		GMath;
extern class FArchive*			GDummySave;
/** Archive for serializing arbitrary data to and from memory						*/
extern class FReloadObjectArc*	GMemoryArchive;
extern TArray<FEdLoadError>		GEdLoadErrors;
extern UDebugger*				GDebugger; //DEBUGGER
/** Optionally force a assert on accessed none */
extern UBOOL					GTreatScriptWarningsFatal;
extern UBOOL					GScriptStackForScriptWarning;
extern UBOOL					GIsBenchmarking;
/**
 *	Global value indicating on-screen warnings/message should be displayed.
 *	Disabled via console command "DISABLEALLSCREENMESSAGES"
 *	Enabled via console command "ENABLEALLSCREENMESSAGES"
 *	Toggled via console command "TOGGLEALLSCREENMESSAGES"
 */
extern UBOOL					GAreScreenMessagesEnabled;
extern UBOOL					GScreenMessagesRestoreState;
/** Whether we are currently rendering stats or not.										*/
// @todo ib2merge: This seems kind of dirty to be in Core for how its only used in ES2 - worth it?
extern UBOOL					GIsDrawingStats;
// "ColorGrading" console variable state
extern INT						GColorGrading;
// see "BloomWeightSmall" console command help
extern FLOAT					GBloomWeightSmall;
// see "BloomWeightLarge" console command help
extern FLOAT					GBloomWeightLarge;
extern UBOOL                    GDrawGFx;
/* Whether we are dumping screenshots */
extern UBOOL					GIsDumpingMovie;
extern UBOOL					GIsHighResScreenshot;
extern UBOOL                    GIsDumpingTileShotMovie;
extern UBOOL		            GIsTiledScreenshot;
extern INT						GGameScreenshotCounter;
extern INT			            GScreenshotResolutionMultiplier;
extern INT			            GScreenshotMargin ;
extern UBOOL					GForceLogFlush;
extern UBOOL					GForceSoundRecook;
extern QWORD					GMakeCacheIDIndex;
#if UDK
extern FGuid					GModGUID;
#endif // UDK
extern TCHAR					GConfigSubDirectory[1024];
extern TCHAR					GEngineIni[1024];
extern TCHAR					GEditorIni[1024];
extern TCHAR					GEditorUserSettingsIni[1024];
extern TCHAR					GSystemSettingsIni[1024];
extern TCHAR					GLightmassIni[1024];
extern TCHAR					GInputIni[1024];
extern TCHAR					GGameIni[1024];
extern TCHAR					GUIIni[1024];
extern TCHAR					GDefaultEngineIni[1024];
extern TCHAR					GDefaultEditorIni[1024];
extern TCHAR					GDefaultEditorUserSettingsIni[1024];
extern TCHAR					GDefaultSystemSettingsIni[1024];
extern TCHAR					GDefaultLightmassIni[1024];
extern TCHAR					GDefaultInputIni[1024];
extern TCHAR					GDefaultGameIni[1024];
extern TCHAR					GDefaultUIIni[1024];

extern FLOAT					GNearClippingPlane;
/** Timestep if a fixed delta time is wanted.										*/
extern DOUBLE					GFixedDeltaTime;
/** Current delta time in seconds.													*/
extern DOUBLE					GDeltaTime;
/** Unclamped delta time in seconds.												*/
extern DOUBLE					GUnclampedDeltaTime;
extern DOUBLE					GCurrentTime;
extern DOUBLE					GLastTime;
extern INT						GSRandSeed;
/* Counter assigning runtime unique IDs */
extern QWORD					GRuntimeUIDCounter;						
extern UBOOL					GExitPurge;
extern TCHAR					GGameName[64];
/** The current SentinelRunID														*/
extern INT                      GSentinelRunID;
//@{
//@script patcher
/** whether we're currently generating a script patch								*/
extern UBOOL					GIsScriptPatcherActive;
/** suffix to append to top-level package names when running the script patcher		*/
extern class FString			GScriptPatchPackageSuffix;
//@}
/** Exec handler for game debugging tool, allowing commands like "editactor", ...	*/
extern FExec*					GDebugToolExec;
/** Whether we are currently cooking.												*/
extern UBOOL					GIsCooking;
/** Whether we are currently cooking for a demo build.								*/
extern UBOOL					GIsCookingForDemo;
/** Whether we're currently in the async loading codepath or not					*/
extern UBOOL					GIsAsyncLoading;
/** Whether the editor is currently loading a map or not													*/
extern UBOOL					GIsEditorLoadingMap;
/** The global object property propagator											*/
extern FObjectPropagator*		GObjectPropagator;
/** Whether to allow execution of Epic internal code like e.g. TTP integration, ... */
extern UBOOL					GIsEpicInternal;
/** Whether GWorld points to the play in editor world								*/
extern UBOOL					GIsPlayInEditorWorld;
/** 
 * TRUE if a normal or PIE game is active (basically !GIsEditor || GIsPlayInEditorWorld) 
 * This must not be accessed on threads other than the game thread!  
 * Use View->Family->ShowFlags & SHOW_Game on the rendering thread.
 */
extern UBOOL					GIsGame;
/** TRUE if running PC w/ -simmobile												*/
extern UBOOL					GIsSimMobile;
/** TRUE if using fast PIE world/level duplication instead of serializing out to disk */
extern UBOOL					GUseFastPIE;
/** TRUE if the runtime needs textures to be powers of two							*/
extern UBOOL					GPlatformNeedsPowerOfTwoTextures;
/** TRUE if we're associating a level with the world, FALSE otherwise				*/
extern UBOOL					GIsAssociatingLevel;
/** Global IO manager																*/
extern struct FIOManager*		GIOManager;
/** Time at which appSeconds() was first initialized (very early on)				*/
extern DOUBLE					GStartTime;
/** System time at engine init.														*/
extern FString					GSystemStartTime;
/** Whether to use the seekfree package map over the regular linker based one.		*/
extern UBOOL					GUseSeekFreePackageMap;
/** Whether to run as PLATFORM_WindowsConsole or not.  Enabled with "-seekfreeloadingpcconsole"	*/
extern UBOOL					GIsSeekFreePCConsole;
/** Whether to run as PLATFORM_WindowsServer or not.  Enabled with "-seekfreeloadingserver"	*/
extern UBOOL					GIsSeekFreePCServer;
/** Whether we are currently precaching or not.										*/
extern UBOOL					GIsPrecaching;
/** Whether we are still in the initial loading proces.								*/
extern UBOOL					GIsInitialLoad;
/** Whether we are currently purging an object in the GC purge pass.				*/
extern UBOOL					GIsPurgingObject;
/** TRUE when we are routing ConditionalPostLoad/PostLoad to objects				*/
extern UBOOL					GIsRoutingPostLoad;
/** Steadily increasing frame counter.												*/
extern QWORD					GFrameCounter;
/** Incremented once per frame before the scene is being rendered. In split screen mode this is incremented once for all views (not for each view). */
extern UINT						GFrameNumber;
/** Render Thread copy of the frame number. */
extern UINT						GFrameNumberRenderThread;

#if !SHIPPING_PC_GAME
// We cannot count on this variable to be accurate in a shipped game, so make sure no code tries to use it
/** Whether we are the first instance of the game running.							*/
extern UBOOL					GIsFirstInstance;
#endif
/** Whether to always use the compression resulting in the smallest size.			*/
extern UBOOL					GAlwaysBiasCompressionForSize;
/** The number of full speed hardware threads the system has.						*/
extern UINT						GNumHardwareThreads;
/** The number of unused threads to have for SerializeCompressed tasks				*/
extern UINT                     GNumUnusedThreads_SerializeCompressed;
/** Approximate physical RAM in GB; 1 on everything except PC. Used for "course tuning", like GNumHardwareThreads.*/
extern UINT						GPhysicalGBRam;
/** This flag signals that the rendering code should throw up an appropriate error.	*/
extern UBOOL					GHandleDirtyDiscError;
/** Whether to forcefully enable capturing of scoped script stats (if > 0).			*/
extern INT						GForceEnableScopedCycleStats;
/** Size to break up data into when saving compressed data							*/
extern INT						GSavingCompressionChunkSize;
/** Total amount of calls to appSeconds and appCycles.								*/
extern QWORD					GNumTimingCodeCalls;
/** Whether we are using the seekfree/ cooked loading codepath.						*/
extern UBOOL					GUseSeekFreeLoading;

#if WITH_SUBSTANCE_AIR
extern UBOOL					GUseSubstanceInstallTimeCache;
#endif

/** Thread ID of the main/game thread												*/
extern DWORD					GGameThreadId;
/** Has GGameThreadId been set yet?													*/
extern UBOOL					GIsGameThreadIdInitialized;
#if ENABLE_SCRIPT_TRACING
/** Whether we are tracing script to a log file */
extern UBOOL					GIsUTracing;
#endif
/** Whether to emit begin/ end draw events.											*/
extern UBOOL					GEmitDrawEvents;
/** Whether we are using software rendering or not.									*/
extern UBOOL					GUseSoftwareRendering;
/** Whether COM is already initialized or not.										*/
extern UBOOL					GIsCOMInitialized;
/** Whether we want the rendering thread to be suspended, used e.g. for tracing.    */
extern UBOOL					GShouldSuspendRenderingThread;
/** Whether we want the game thread to be suspended, used e.g. for tracing.			*/
extern UBOOL					GShouldSuspendGameThread;
/** Whether we want to use a fixed time step or not.								*/
extern UBOOL					GUseFixedTimeStep;
/** Determines what kind of trace should occur, NAME_None for none.					*/
extern FName					GCurrentTraceName;
/** whether to print time since GStartTime in log output							*/
extern UBOOL					GPrintLogTimes;
/** Global screen shot index to avoid overwriting ScreenShots						*/
extern INT                      GScreenshotBitmapIndex;
/** Whether stats should emit named events for e.g. PIX.							*/
extern UBOOL					GCycleStatsShouldEmitNamedEvents;
/** Whether stats should also generate data for gameplay profiler					*/
extern UBOOL					GCycleStatsWithGameplayProfiling;
/** Whether or not we should log out all of the PlaySound calls.  Makes it much easier to debug what sound is playing when you have the name. */
extern UBOOL					GShouldLogAllPlaySoundCalls;
/** Whether to convert net index errors to warnings (for fixing up redirects)		*/
extern UBOOL					GConvertNetIndexErrorsToWarnings;

/** Whether or not we should log out all of the PlaySound calls.  Makes it much easier to debug what sound is playing when you have the name. */
extern UBOOL					GShouldLogAllParticleActivateSystemCalls;

/** Whether the title is running in lowgore mode									*/
extern UBOOL					GForceLowGore;

/** Render Mode																		*/
enum ERenderMode
{
	RENDER_MODE_NONE			= -1,	
	RENDER_MODE_DX9				= 0,
	RENDER_MODE_DX11			= 1,
};
extern ERenderMode				GRenderMode;
extern ERenderMode				GForcedRenderMode;

/** True if we're emulating mobile rendering on a non-mobile renderer */
extern UBOOL GEmulateMobileRendering;

/** Sometimes we need to temporarily disallow mobile materials (eg. in editor MeshPaint mode) - so we use this */
extern UBOOL GForceDisableEmulateMobileRendering;

/** Whether we are forcing simple lightmaps for the purpose of mobile emulation on a non-mobile renderer. */
extern UBOOL GUseSimpleLightmapsForMobileEmulation;

/** True if gamma correction should be used when emulating mobile rendering */
extern UBOOL GUseGammaCorrectionForMobileEmulation;

/** True if we should always cache PVRTC textures and/or flatten textures for mobile when working in the editor */
extern UBOOL GAlwaysOptimizeContentForMobile;

/** True if we're emulating mobile input on a non-mobile platform */
extern UBOOL GEmulateMobileInput;

/** Global access point to register, find and deal with console variables. */
extern struct IConsoleManager*	GConsoleManager;

/** Whether or not we are running OpenAutomate */
extern class FOpenAutomate* GOpenAutomate;

/** Whether or not a unit test is currently being run */
extern UBOOL GIsUnitTesting;

// Per module globals.
extern "C" TCHAR GPackage[];

// Normal includes.
#include <new>
#include "ScopedDebugInfo.h"			// Scoped debug info.
#include "ProfilerIntegration.h"		// AQtime/ VTune integration.
#include "UnFile.h"						// Low level utility code.
#include "UnObjVer.h"					// Object version info.
#include "UnArc.h"						// Archive class.
#include "UnTypeTraits.h"				// Type Traits
#include "UnTemplate.h"					// Common template definitions.
#include "ScopedPointer.h"				// Scoped pointer definitions.
#include "Sorting.h"					// Sorting definitions.
#include "Array.h"						// Dynamic array definitions.
#include "BitArray.h"					// Bit array definition.
#include "SparseArray.h"				// Sparse array definitions.
#include "ChunkedArray.h"				// Chunked array definitions.
#include "UnString.h"					// Dynamic string definitions.
#include "StaticArray.h"                // Static array definition.
#include "StaticBitArray.h"             // Static bit array definition.
#include "Set.h"						// Set definitions.
#include "Map.h"						// Dynamic map definitions.
#include "List.h"						// Dynamic list definitions.
#include "ScriptInterface.h"			// Script interface definitions.
#include "ResourceArray.h"				// Resource array definitions.
#include "RefCounting.h"				// Reference counting definitions.
#include "RingBuffer.h"					// Ring buffer definitions.
#include "UnName.h"						// Global name subsystem.
#include "UnStack.h"					// Script stack definition.
#include "UnScriptMacros.h"				// Script macro definitions
#include "UnObjBas.h"					// Object base class.
#include "UnMath.h"						// Vector math functions.
#include "SHMath.h"						// SH math functions.
#include "Random.h"						// Random math functions.
#include "CoreClasses.h"				// Low level utility code.
#include "ObjectThumbnail.h"			// Object thumbnails
#include "UnCoreNet.h"					// Core networking.
#include "UnCorObj.h"					// Core object class definitions.
#include "UnObjGC.h"					// Realtime garbage collection helpers
#include "UnClass.h"					// Class definition.
#include "Casts.h"                      // Templated casts
#include "UnType.h"						// Base property type.
#include "UnScript.h"					// Script class.
#include "UFactory.h"					// Factory definition.
#include "UExporter.h"					// Exporter definition.
#include "UnMem.h"						// Stack based memory management.
#include "UnCId.h"						// Cache ID's.
#include "UnBits.h"						// Bitstream archiver.
#include "FCallbackDevice.h"			// Base class for callback devices.
#include "UnThreadingBase.h"			// Non-platform specific multi-threaded support.
#include "UnAsyncWork.h"				// Async threaded work
#include "UnOutputDevices.h"			// Output devices
#include "UnObjectRedirector.h"			// Cross-package object redirector
#include "UnArchive.h"					// Utility archive classes
#include "UnBulkData.h"					// Bulk data classes
#include "PerfCounter.h"				// Serialization performance tracking classes
#include "UnLinker.h"					// Linker.
#include "FSerializableObject.h"		// non- UObject object serializer
#include "UnIOBase.h"					// base IO declarations, FIOManager, FIOSystem
#include "UnAsyncLoading.h"				// FAsyncPackage definition
#include "FConfigCacheIni.h"			// The configuration cache declarations
#include "UnMisc.h"						// Misc helpers that don't fit anywhere else

#if PERF_TRACK_SERIALIZATION_PERFORMANCE || LOOKING_FOR_PERF_ISSUES
/** Tracks time spent serializing UObject data, per object type */
extern FStructEventMap* GObjectSerializationPerfTracker;

/** Tracks the amount of time spent in UClass serialization, per class */
extern FStructEventMap* GClassSerializationPerfTracker;

#endif

/** Container class for analytics API Keys */
struct FAnalyticsConfig
{
	FString ApsalarAPIKeyDev;
	FString ApsalarAPIKeyRelease;
	FString ApsalarSecretDev;
	FString ApsalarSecretRelease;	
};

/** Identifier for specifying the type of map check message */
enum MapCheckType
{
	MCTYPE_NONE					= 0,
	MCTYPE_CRITICALERROR		= ( 1 << 0 ),
	MCTYPE_ERROR				= ( 1 << 1 ),
	MCTYPE_PERFORMANCEWARNING	= ( 1 << 2 ),
	MCTYPE_WARNING				= ( 1 << 3 ),
	MCTYPE_NOTE					= ( 1 << 4 ),
	MCTYPE_INFO					= ( 1 << 5 ),
};
enum MapCheckGroup
{
	MCGROUP_DEFAULT			= 0,
	MCGROUP_KISMET			= ( 1 << 0 ),
	MCGROUP_MOBILEPLATFORM	= ( 1 << 1 ),
};

/** A context for displaying modal warning messages. */
class FFeedbackContext : public FOutputDevice
{
public:
	VARARG_DECL( virtual UBOOL, UBOOL, return, YesNof, VARARG_PURE, const TCHAR*, VARARG_NONE, VARARG_NONE );
	virtual void BeginSlowTask( const TCHAR* Task, UBOOL ShowProgressDialog, UBOOL bShowCancelButton=FALSE )=0;
	virtual void EndSlowTask()=0;
	virtual UBOOL ReceivedUserCancel() { return FALSE; };
	VARARG_DECL( virtual UBOOL, UBOOL VARARGS, return, StatusUpdatef, VARARG_PURE, const TCHAR*, VARARG_EXTRA(INT Numerator) VARARG_EXTRA(INT Denominator), VARARG_EXTRA(Numerator) VARARG_EXTRA(Denominator) );


	/**
	 * Updates the progress amount without changing the status message text.  Override this in derived classes.
	 *
	 * @param Numerator		New progress numerator
	 * @param Denominator	New progress denominator
	 */
	virtual void UpdateProgress( INT Numerator, INT Denominator )
	{
	}

	/** Pushes the current status message/progress onto the stack so it can be restored later.  Override this
	    in derived classes. */
	virtual void PushStatus()
	{
	}

	/** Restores the previously pushed status message/progress.  Override this in derived classes. */
	virtual void PopStatus()
	{
	}

	virtual FContextSupplier* GetContext() const { return NULL; }
	virtual void SetContext( FContextSupplier* InSupplier ) {}


	/** Map checking interface */

	/**
	 * @return	TRUE if a map check is currently active.
	 */
	virtual UBOOL MapCheck_IsActive() const { return FALSE; }

	/**
	 * Shows the map check dialog.
	 */
	virtual void MapCheck_Show() {};

	/**
	 * Same as MapCheck_Show, except it won't display the map check dialog if there are no errors in it.
	 */
	virtual void MapCheck_ShowConditionally() {};

	/**
	 * Hides the map check dialog.
	 */
	virtual void MapCheck_Hide() {};

	/**
	 * Clears out all errors/warnings.
	 */
	virtual void MapCheck_Clear() {};

	/** Called around bulk MapCheck_Add calls. */
	virtual void MapCheck_BeginBulkAdd() {};

	/**
	 * Adds a message to the map check dialog, to be displayed when the dialog is shown.
	 *
	 * @param	InType					The	type of message.
	 * @param	InActor					Actor associated with the message; can be NULL.
	 * @param	InMessage				The message to display.
	 * @param	InUDNPage				UDN Page to visit if the user needs more info on the warning.  This will send the user to https://udn.epicgames.com/Three/MapErrors#InUDNPage. 
	 * @param	InGroup					The message group (kismet/mobile/...).
	 */
	virtual void MapCheck_Add(MapCheckType InType, UObject* InActor, const TCHAR* InMessage, const TCHAR* InUDNPage, MapCheckGroup InGroup=MCGROUP_DEFAULT) {};

	/** Called around bulk MapCheck_Add calls. */
	virtual void MapCheck_EndBulkAdd() {};

	/**
	 *	Lighting Build handlers
	 */
	/**
	 * @return	TRUE if a lighting build is currently active.
	 */
	virtual UBOOL LightingBuild_IsActive() const { return FALSE; }
	virtual void LightingBuild_Show() {}

	/**
	 * Shows the dialog only if there are warnings or errors to display.
	 */
	virtual void LightingBuild_ShowConditionally() {}

	/** Hides the LightingBuild dialog. */
	virtual void LightingBuild_Hide() {}
	/** Clears out all errors/warnings.*/
	virtual void LightingBuild_Clear() {}
	/** Called around bulk LightingBuild_Add calls.*/
	virtual void LightingBuild_BeginBulkAdd() {}

	/**
	 * Adds a message to the map check dialog, to be displayed when the dialog is shown.
	 *
	 * @param	InType					The message type (error/warning/...).
	 * @param	InActor					Actor associated with the message; can be NULL.
	 * @param	InMessage				The message to display.
	 * @param	InUDNPage				UDN Page to visit if the user needs more info on the warning.  This will send the user to https://udn.epicgames.com/Three/MapErrors#InUDNPage.
	 * @param	InGroup					The message group (kismet/mobile/...).
	 */
	virtual void LightingBuild_Add( MapCheckType InType, UObject* InObject, const TCHAR* InMessage, const TCHAR* InUDNPage=TEXT(""), MapCheckGroup InGroup = MCGROUP_DEFAULT ) {}

	/** Called around bulk MapCheck_Add calls. */
	virtual void LightingBuild_EndBulkAdd() {}

	/** Refresh the lighting times list. */
	virtual void LightingBuild_Refresh() {}

	/**
	 *	Lighting Build Info handlers
	 */
	/**
	 * @return	TRUE if the LightingBuildInfo is currently active
	 */
	virtual UBOOL LightingBuildInfo_IsActive() const { return FALSE; }
	virtual void LightingBuildInfo_Show() {}

	/** Hides the LightingBuildInfo dialog. */
	virtual void LightingBuildInfo_Hide() {}
	/** Clears out all errors/warnings.*/
	virtual void LightingBuildInfo_Clear() {}
	/** Called around bulk *_Add calls.*/
	virtual void LightingBuildInfo_BeginBulkAdd() {}

	/**
	 * Adds a message to the LightingBuildInfo dialog, to be displayed when the dialog is shown.
	 *
	 *	@param	InObject				Actor associated with the message; can be NULL.
	 *	@param	InTime					The time taken to light this object.
	 *	@param	InUnmappedPercentage	The unmapped texel percentage for this object, -1.f for 
	 *	@param	InUnmappedMemory		The memory taken up by unmapped texels for this object
	 *	@param	InTotalTexelMemory				The memory consumed by all texels for this object.
	 */
	virtual void LightingBuildInfo_Add(UObject* InObject, DOUBLE InTime, FLOAT InUnmappedPercentage, INT InUnmappedMemory, INT InTotalTexelMemory) {}

	/** Called around bulk *_Add calls. */
	virtual void LightingBuildInfo_EndBulkAdd() {}

	/** Refresh the lighting times list. */
	virtual void LightingBuildInfo_Refresh() {}

	/** Clears out all static mesh lighting info.*/
	virtual void LightingBuildInfoList_Clear() {}

	TArray<FString> Warnings;
	TArray<FString> Errors;

	UBOOL	TreatWarningsAsErrors;

	/**
	* A pointer to the editors frame window.  This gives you the ability to parent windows
	* correctly in projects that are at a lower level than UnrealEd.
	*/
	PTRINT	winEditorFrame;				
	PTRINT	hWndEditorFrame;				

	FFeedbackContext() :
		 TreatWarningsAsErrors( 0 )
		, winEditorFrame( 0 )
		, hWndEditorFrame( 0 )
	{}
};

// Worker class for tracking loading errors in the editor
class FEdLoadError
{
public:
	FEdLoadError()
	{}
	FEdLoadError( INT InType, TCHAR* InDesc )
	{
		Type = InType;
		Desc = InDesc;
	}
	~FEdLoadError()
	{}

	// The types of things that could be missing.
	enum
	{
		TYPE_FILE		= 0,	// A totally missing file
		TYPE_RESOURCE	= 1,	// Texture/Sound/StaticMesh/etc
	};

	INT Type;		// TYPE_
	FString Desc;	// Description of the error

	UBOOL operator==( const FEdLoadError& LE ) const
	{
		return Type==LE.Type && Desc==LE.Desc;
	}
	FEdLoadError& operator=( const FEdLoadError Other )
	{
		Type = Other.Type;
		Desc = Other.Desc;
		return *this;
	}
};

/** Coordinate system identifiers. */
enum ECoordSystem
{
	COORD_None	= -1,
	COORD_World,
	COORD_Local,
	COORD_Max,
};

/** Very basic abstract debugger class. */
class UDebugger
{
public:
	virtual ~UDebugger() {};

	virtual void  DebugInfo( const UObject* Debugee, const FFrame* Stack, BYTE OpCode, INT LineNumber, INT InputPos )=0;

	virtual void  NotifyBeginTick()=0;
	virtual void  NotifyGC()=0;
	virtual void  NotifyAccessedNone()=0;
	virtual UBOOL NotifyAssertionFailed( const INT LineNumber )=0;
	virtual UBOOL NotifyInfiniteLoop()=0;

	/**
	 * Detach the UDebugger from the engine.
	 *
	 * @param	bUnbindInterfaceImmediately		specify TRUE to cleanup and unbind the interface .dll as well.  If calling this method
	 *											from the interface .dll's code, should NEVER specify TRUE.
	 */
	virtual void Close( UBOOL bUnbindInterfaceImmediately=FALSE )=0;
};

#include "UnCoreNative.h"


/**
 * This struct holds information about a cached package
 */
struct FDLCInfo
{
	/** Full path to the package */
	FString Path;
	/** Which user this content is associated with, or NO_USER_SPECIFIED if no user */
	INT UserIndex;

	/** Initialization constructor */
	FDLCInfo(FString InPath, INT InUserIndex)
		: Path(InPath)
		, UserIndex(InUserIndex)
	{
	}
};

struct FMapPackageFileCache : public FPackageFileCache
{
private:
	TMap<FString, FString> FileLookup;		// find a package file by lowercase filename
	TMap<FString, FDLCInfo> DownloadedFileLookup;		// find a downloaded package file by lowercase filename
	TMap<FString, INT> SourceControlStateLookup;		// find a package file state by lowercase filename

	/** 
	 * Cache all packages in this path and any directories under it
	 * 
	 * @param InPath The path to look in for packages
	 */
	void CachePath( const TCHAR* InPath);
public:
	virtual void CachePaths();
	virtual UBOOL CachePackage( const TCHAR* InPathName, UBOOL InOverrideDupe=0, UBOOL WarnIfExists=1 );
	virtual UBOOL FindPackageFile( const TCHAR* InName, const FGuid* Guid, FString& OutFileName, const TCHAR* Language=NULL );

	/**
	 * Sets the source control status for a package
	 *
	 * @param	InName			a string representing the filename of an Unreal package; may include path and/or extension.
	 * @param	InState			the new source control state
	 */
	virtual UBOOL SetSourceControlState ( const TCHAR* InName, INT InNewState);
	/**
	 * Gets the source control status for a package
	 *
	 * @param	InName			a string representing the filename of an Unreal package; may include path and/or extension.
	 */
	virtual INT GetSourceControlState ( const TCHAR* InName);

	virtual TArray<FString> GetPackageFileList();

	/**
	 * Add a downloaded content package to the list of known packages.
	 * Can be assigned to a particular ID for removing with ClearDownloadadPackages.
	 *
	 * @param InPlatformPathName The platform-specific full path name to a package (will be passed directly to CreateFileReader)
	 * @param UserIndex Optional user to associate with the package so that it can be flushed later
	 *
	 * @return TRUE if successful
	 */
	virtual UBOOL CacheDownloadedPackage(const TCHAR* InPlatformPathName, INT UserIndex);

	/**
	 * Clears all entries from the package file cache.
	 *
	 * @script patcher
	 */
	virtual void ClearPackageCache();

	/**
	 * Remove all downloaded packages from the package file cache.
	 */
	virtual void ClearDownloadedPackages();
};

#if _WINDOWS
static inline TCHAR GetFormatType(UINT)			{ return TEXT('u'); }
#endif
static inline TCHAR	GetFormatType(DWORD)		{ return TEXT('u'); }
static inline TCHAR	GetFormatType(BYTE)			{ return TEXT('X'); }
static inline TCHAR	GetFormatType(INT)			{ return TEXT('i'); }
static inline TCHAR	GetFormatType(DOUBLE)		{ return TEXT('f'); }
static inline TCHAR	GetFormatType(TCHAR)		{ return TEXT('c'); }
static inline TCHAR GetFormatType(TCHAR*)		{ return TEXT('s'); }
static inline TCHAR GetFormatType(const TCHAR*)	{ return TEXT('s'); }
#if !TCHAR_IS_1_BYTE
static inline TCHAR	GetFormatType(ANSICHAR*)		{ return TEXT('s'); }
static inline TCHAR	GetFormatType(const ANSICHAR*)	{ return TEXT('s'); }
#endif

#define INITSTRING TCHAR* pString=String
#define RETURNSTRING return String
#define REPLACE_PARM(parm)													\
	if ( pString != NULL )													\
	{																		\
		pString = appStrchr(pString, TEXT('`'));							\
		if ( pString )														\
		{																	\
			if ((pString>String) && (*(pString-1)==TEXT('\\')))				\
			{																\
				if  ( *(pString+1) != 0 )									\
				{															\
					pString = appStrchr(pString+1,TEXT('`'));				\
					if ( pString==NULL ) RETURNSTRING;						\
				}															\
			}																\
																			\
			*pString=TEXT('%');												\
			pString=appStrchr(pString, TEXT('~'));							\
			if ( pString )													\
			{																\
				*pString=GetFormatType(parm);								\
			}																\
		}																	\
	}																		\
	else																	\
	{																		\
		return TEXT("");													\
	}

template<typename T1> const TCHAR* FormatLocalizedString(TCHAR* String, T1 V1)
{
	INITSTRING;REPLACE_PARM(V1);RETURNSTRING;
}
template<typename T1,typename T2> const TCHAR* FormatLocalizedString(TCHAR* String,T1 V1,T2 V2)
{
	INITSTRING;REPLACE_PARM(V1);REPLACE_PARM(V2);RETURNSTRING;
}
template<typename T1,typename T2,typename T3> const TCHAR* FormatLocalizedString(TCHAR* String,T1 V1,T2 V2,T3 V3)
{
	INITSTRING;REPLACE_PARM(V1);REPLACE_PARM(V2);REPLACE_PARM(V3);RETURNSTRING;
}
template<typename T1,typename T2,typename T3,typename T4> const TCHAR* FormatLocalizedString(TCHAR* String,T1 V1,T2 V2,T3 V3,T4 V4)
{
	INITSTRING;REPLACE_PARM(V1);REPLACE_PARM(V2);REPLACE_PARM(V3);REPLACE_PARM(V4);RETURNSTRING;
}
template<typename T1,typename T2,typename T3,typename T4,typename T5> const TCHAR* FormatLocalizedString(TCHAR* String,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5)
{
	INITSTRING;REPLACE_PARM(V1);REPLACE_PARM(V2);REPLACE_PARM(V3);REPLACE_PARM(V4);REPLACE_PARM(V5);RETURNSTRING;
}
template<typename T1,typename T2,typename T3,typename T4,typename T5,typename T6> const TCHAR* FormatLocalizedString(TCHAR* String,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6)
{
	INITSTRING;REPLACE_PARM(V1);REPLACE_PARM(V2);REPLACE_PARM(V3);REPLACE_PARM(V4);REPLACE_PARM(V5);REPLACE_PARM(V6);RETURNSTRING;
}

#if SUPPORTS_VARIADIC_MACROS
	#define LocalizeSecure( string, ... )			FormatLocalizedString( (TCHAR*)string.GetCharArray().GetData(),__VA_ARGS__ ), __VA_ARGS__
#elif (_MSC_VER >= 1300)
	#define LocalizeSecure( string,a,b,c,d,e,f )	FormatLocalizedString( (TCHAR*)string.GetCharArray().GetData(),VARG(a),VARG(b),VARG(c),VARG(d),VARG(e),VARG(f) ), VARG(a),VARG(b),VARG(c),VARG(d),VARG(e),VARG(f)
#endif

#include "UnThreadingBase.h"

#include "UnitTest.h"					// Unit testing functionality for game, editor, etc.
#include "GameplayProfiler.h"
#include "UnStats.h"

/** Memory stats */
enum EMemoryStats
{
	STAT_AudioMemory = STAT_MemoryFirstStat,
	STAT_PhysicalAllocSize,
#if PS3
	STAT_GPUAllocSize,
	STAT_HostAllocSize,
#else
	STAT_VirtualAllocSize,
#endif
	STAT_StreamingAllocSize,
	STAT_AnimationMemory,
	STAT_MemoryNovodexTotalAllocationSize,
	STAT_VertexLightingAndShadowingMemory,
	STAT_PrecomputedVisibilityMemory,
	STAT_PrecomputedLightVolumeMemory,
	STAT_DominantShadowTransitionMemory,
	STAT_StaticMeshTotalMemory,
	STAT_FracturedMeshIndexMemory,
	STAT_SkeletalMeshVertexMemory,
	STAT_SkeletalMeshIndexMemory,
	STAT_SkeletalMeshMotionBlurSkinningMemory,
	STAT_DecalVertexMemory,
	STAT_DecalIndexMemory,
	STAT_DecalInteractionMemory,
	STAT_VertexShaderMemory,
	STAT_PixelShaderMemory,

	STAT_StaticMeshTotalMemory2,
	STAT_StaticMeshkDOPMemory,
	STAT_StaticMeshVertexMemory,
	STAT_ResourceVertexColorMemory,
	STAT_InstVertexColorMemory,
	STAT_StaticMeshIndexMemory,

#if PS3
	STAT_StaticMeshVideoMemory,
	STAT_ResourceVertexColorVideoMemory,
	STAT_StaticMeshIndexVideoMemory,
#endif // PS3

	STAT_FaceFXPeakAllocSize,
	STAT_FaceFXCurrentAllocSize,

	STAT_RendertargetMemory,
	STAT_TextureMemory,
	STAT_TextureLightmapMemory,
	STAT_TextureShadowmapMemory,
#if _WINDOWS // The TextureMemory stat will be what is used on Xbox...
	STAT_XboxTextureMemory,
	STAT_XboxTextureLightmapMemory,
	STAT_XboxTextureShadowmapMemory,
#endif
	STAT_TrimMemoryTime,

	STAT_GameToRendererMallocTotal,
	STAT_GameToRendererMalloc,
	STAT_GameToRendererFree,
	STAT_GameToRendererNet,

#if PS3 || NGP
	// NOTE: These enums must match EAllocationType (PSGcmMalloc.h / GXMMalloc.h)!
	STAT_FirstGPUStat,
	
	STAT_LocalCommandBufferSize,
	STAT_LocalFrameBufferSize,
	STAT_LocalZBufferSize,
	STAT_LocalRenderTargetSize,
	STAT_LocalTextureSize,
	STAT_LocalVertexShaderSize,
	STAT_LocalPixelShaderSize,
	STAT_LocalVertexBufferSize,
	STAT_LocalIndexBufferSize,
	STAT_LocalRingBufferSize,
	STAT_LocalCompressionTagSize,
	STAT_LocalResourceArraySize,
	STAT_LocalOcclusionQueries,
	STAT_HostCommandBufferSize,
	STAT_HostFrameBufferSize,
	STAT_HostZBufferSize,
	STAT_HostRenderTargetSize,
	STAT_HostTextureSize,
	STAT_HostVertexShaderSize,
	STAT_HostPixelShaderSize,
	STAT_HostVertexBufferSize,
	STAT_HostIndexBufferSize,
	STAT_HostRingBufferSize,
	STAT_HostCompressionTagSize,
	STAT_HostResourceArraySize,
	STAT_HostOcclusionQueries,

	STAT_LastGPUStat,
#endif
};

/** Memory churn stats. */
enum EMemoryChurnStats
{
	STAT_MallocCalls = STAT_MemoryChurnFirstStat,
	STAT_FreeCalls,
	STAT_ReallocCalls,
	STAT_PhysicalAllocCalls,
	STAT_PhysicalFreeCalls,
	STAT_TotalAllocatorCalls,
};

/**
 * Holds the list of stat ids for object stat gathering
 */
enum EObjectStats
{
	STAT_ConstructObject = STAT_ObjectFirstStat,
	STAT_LoadConfig,
	STAT_LoadLocalized,
	STAT_LoadObject,
	STAT_FindObject,
	STAT_FindObjectFast,
	STAT_InitProperties,
	STAT_NameTableEntries,
	STAT_NameTableAnsiEntries,
	STAT_NameTableUnicodeEntries,
	STAT_NameTableMemorySize,
	STAT_DestroyObject,
};

/** Threading stats */
enum EThreadingStats
{
	STAT_RenderingIdleTime = STAT_ThreadingFirstStat,
	STAT_RenderingBusyTime,
	STAT_GameIdleTime,
	STAT_GameTickWaitTime,
	STAT_GameTickWantedWaitTime,
	STAT_GameTickAdditionalWaitTime,
	STAT_GPUWaitingOnCPU,
	STAT_CPUWaitingOnGPU,
};

/**
 * Async IO stats - STATGROUP_AsyncIO
 */
enum EAsyncIOStats
{
	STAT_AsyncIO_FulfilledReadCount	= STAT_AsyncIOFirstStat,
	STAT_AsyncIO_FulfilledReadSize,
	STAT_AsyncIO_CanceledReadCount,
	STAT_AsyncIO_CanceledReadSize,
	STAT_AsyncIO_OutstandingReadCount,
	STAT_AsyncIO_OutstandingReadSize,
	STAT_AsyncIO_PlatformReadTime,
	STAT_AsyncIO_UncompressorWaitTime,
	STAT_AsyncIO_MainThreadBlockTime,
	STAT_AsyncIO_AsyncPackagePrecacheWaitTime,
	STAT_AsyncIO_Bandwidth,
	STAT_UncompressorTime,
};

/**
 * Network stats counters
 */
enum ENetStats
{
	STAT_Ping = STAT_NetFirstStat,
	STAT_Channels,
	STAT_InRate,
	STAT_OutRate,
	STAT_InPackets,
	STAT_OutPackets,
	STAT_InBunches,
	STAT_OutBunches,
	STAT_OutLoss,
	STAT_InLoss,
	STAT_VoiceBytesSent,
	STAT_VoiceBytesRecv,
	STAT_VoicePacketsSent,
	STAT_VoicePacketsRecv,
	STAT_PercentInVoice,
	STAT_PercentOutVoice
};

/**
 * Network stats counters (specific to peer connections)
 */
enum ENetPeerStats
{
	STAT_PeerPing = STAT_NetPeerFirstStat,
	STAT_PeerChannels,
	STAT_PeerInRate,
	STAT_PeerOutRate,
	STAT_PeerInPackets,
	STAT_PeerOutPackets,
	STAT_PeerInBunches,
	STAT_PeerOutBunches,
	STAT_PeerOutLoss,
	STAT_PeerInLoss,
	STAT_PeerVoiceBytesSent,
	STAT_PeerVoiceBytesRecv,
	STAT_PeerVoicePacketsSent,
	STAT_PeerVoicePacketsRecv,
	STAT_PeerPercentInVoice,
	STAT_PeerPercentOutVoice
};

/*-----------------------------------------------------------------------------
	Seekfree defines.
-----------------------------------------------------------------------------*/

#define STANDALONE_SEEKFREE_SUFFIX	TEXT("_SF")


#endif

