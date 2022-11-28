#ifndef NX_FOUNDATION_NX
#define NX_FOUNDATION_NX
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/
/**
DLL export macros
*/
//@epic_sas_xenon start
#if !defined(_XBOX)
//@epic_sas_xenon stop
#ifndef NXF_DLL_EXPORT
	#ifdef NX_FOUNDATION_DLL

		#define NXF_DLL_EXPORT __declspec(dllexport)

	#elif defined NX_FOUNDATION_STATICLIB

		#define NXF_DLL_EXPORT

	#elif defined NX_USE_SDK_DLLS

		#define NXF_DLL_EXPORT __declspec(dllimport)

	#elif defined NX_USE_SDK_STATICLIBS

		#define NXF_DLL_EXPORT

	#else

		//#error Please define either NX_USE_SDK_DLLS or NX_USE_SDK_STATICLIBS in your project settings depending on the kind of libraries you use!
		#define NXF_DLL_EXPORT __declspec(dllimport)
	
	#endif
#endif
//@epic_sas_xenon start
#else	//#if !defined(_XBOX)
	#define NXF_DLL_EXPORT
#endif	//#if !defined(_XBOX)
//@epic_sas_xenon stop

#ifndef NX_C_EXPORT
	#define NX_C_EXPORT extern "C"
#endif

#ifndef NX_CALL_CONV
	#if defined WIN32
		#define NX_CALL_CONV __cdecl
	#elif defined LINUX
		#define NX_CALL_CONV
    #elif defined __APPLE__
        #define NX_CALL_CONV
//@epic_sas_xenon start
    #elif defined _XBOX
        #define NX_CALL_CONV
//@epic_sas_xenon stop
	#else
	#error custom definition of NX_CALL_CONV for your OS needed!
	#endif
#endif

#if	  defined NX32
#elif defined NX64
#elif defined WIN32
#ifdef NX64
#error NovodeX SDK: Platforms pointer size ambiguous!  The defines WIN32 and NX64 are in conflict.  
#endif
#define NX32
#elif defined WIN64
#ifdef NX32
#error NovodeX SDK: Platforms pointer size ambiguous!  The defines WIN64 and NX32 are in conflict.  
#endif
#define NX64
//@epic_sas_xenon start
#elif defined _XBOX
#ifdef NX64
#error NovodeX SDK: Platforms pointer size ambiguous!  The defines WIN32 and NX64 are in conflict.  
#endif
#define NX32
//@epic_sas_xenon stop
#else
#error NovodeX SDK: Platforms pointer size ambiguous.  Please define NX32 or Nx64 in the compiler settings!
#endif

#define NX_COMPILE_TIME_ASSERT(exp)	extern char NX_CompileTimeAssert[ (exp) ? 1 : -1 ]

#if _MSC_VER
	#define NX_MSVC		// Compiling with VC++
	#if _MSC_VER > 1300
		#define NX_VC7		// Compiling with VC7
	#else
		#define NX_VC6		// Compiling with VC6
		#define __FUNCTION__	"Undefined"
	#endif
#endif

/**
 Nx SDK misc defines.
*/

//NX_INLINE
#if (_MSC_VER>=1000)
	#define NX_INLINE __forceinline	//alternative is simple inline
	#pragma inline_depth( 255 )

	#include <string.h>
	#include <stdlib.h>
	#pragma intrinsic(memcmp)
	#pragma intrinsic(memcpy)
	#pragma intrinsic(memset)
	#pragma intrinsic(strcat)
	#pragma intrinsic(strcmp)
	#pragma intrinsic(strcpy)
	#pragma intrinsic(strlen)
	#pragma intrinsic(abs)
	#pragma intrinsic(labs)
#elif defined(__MWERKS__) 
	//optional: #pragma always_inline on
	#define NX_INLINE inline
#else
	#define NX_INLINE inline
#endif

	#define NX_DELETE(x)	delete x
	#define NX_DELETE_SINGLE(x)	if (x) { delete x;		x = NULL; }
	#define NX_DELETE_ARRAY(x)	if (x) { delete []x;	x = NULL; }

	template<class Type> NX_INLINE void NX_Swap(Type& a, Type& b)
		{
		const Type c = a; a = b; b = c;
		}

/**
Error codes
*/

enum NxErrorCode
	{
	NXE_NO_ERROR			= 0,	//no error
	NXE_INVALID_PARAMETER	= 1,	//method called with invalid parameter(s)
	NXE_INVALID_OPERATION	= 2,	//method was called at a time when an operation is not possible
	NXE_OUT_OF_MEMORY		= 3,	//method failed to allocate some memory
	NXE_INTERNAL_ERROR		= 4,	//the library failed for some reason (usually because you have passed invalid values like NaNs into the system, which are not checked for.)

	NXE_ASSERTION			= 107,//an assertion failed.

	//messages only emitted when NX_USER_DEBUG_MODE is defined:
	NXE_DB_INFO				= 205,//an information message for the user to help with debugging
	NXE_DB_WARNING			= 206,//a warning message for the user to help with debugging
	NXE_DB_PRINT			= 208 //the message should simply be printed without any additional infos (line number, etc)

	};

//get rid of browser info warnings 
#pragma warning( disable : 4786 )  //identifier was truncated to '255' characters in the debug information
#pragma warning( disable : 4251 )  //class needs to have dll-interface to be used by clients of class

//files to always include:
#include "NxSimpleTypes.h"
#include "NxAssert.h"
#ifdef LINUX
#include <time.h>
#elif __APPLE__
	#include <time.h>
//@epic_sas_xenon start
#elif _XBOX
	#include <time.h>
//@epic_sas_xenon stop
#endif

#define NX_DEBUG_MALLOC 0

// Don't use inline for alloca !!!
#ifdef WIN32
    #include <malloc.h>
	#define NxAlloca(x)	_alloca(x)
#elif LINUX
    #include <malloc.h>
	#define NxAlloca(x)	alloca(x)
#elif __APPLE__
    #include <alloca.h>
    #define NxAlloca(x)	alloca(x)
//@epic_sas_xenon start
#elif _XBOX
	#define NOMINMAX
	#include <Xtl.h>
	#include <malloc.h>
    #define NxAlloca(x)		alloca(x)
//@epic_sas_xenon stop
#endif

//#define TRANSPOSED_MAT33
//#define COMPILE_CCD

#endif
