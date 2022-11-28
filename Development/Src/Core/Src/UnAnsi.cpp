/*=============================================================================
	UnFile.cpp: ANSI C core.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "CorePrivate.h"

#undef clock
#include <time.h>
#include <stdio.h>
#include <stdarg.h>

/*-----------------------------------------------------------------------------
	Time.
-----------------------------------------------------------------------------*/

//
// String timestamp.
// !! Note to self: Move to UnVcWin32.cpp
// !! Make Linux version.
//
#if _MSC_VER
const TCHAR* appTimestamp()
{
	static TCHAR Result[1024];
	*Result = 0;
#if UNICODE
	if( GUnicodeOS )
	{
		_wstrdate( Result );
		appStrcat( Result, TEXT(" ") );
		_wstrtime( Result + appStrlen(Result) );
	}
	else
#endif
	{
		ANSICHAR Temp[1024]="";
		_strdate( Temp );
		appStrcpy( Result, ANSI_TO_TCHAR(Temp) );
		appStrcat( Result, TEXT(" ") );
		_strtime( Temp );
		appStrcat( Result, ANSI_TO_TCHAR(Temp) );
	}
	return Result;
}
#endif


/*-----------------------------------------------------------------------------
	Memory functions.
-----------------------------------------------------------------------------*/

INT appMemcmp( const void* Buf1, const void* Buf2, INT Count )
{
	return memcmp( Buf1, Buf2, Count );
}

UBOOL appMemIsZero( const void* V, int Count )
{
	BYTE* B = (BYTE*)V;
	while( Count-- > 0 )
		if( *B++ != 0 )
			return 0;
	return 1;
}

void* appMemmove( void* Dest, const void* Src, INT Count )
{
	return memmove( Dest, Src, Count );
}

void appMemset( void* Dest, INT C, INT Count )
{
	memset( Dest, C, Count );
}

#ifndef DEFINED_appMemzero
void appMemzero( void* Dest, INT Count )
{
	memset( Dest, 0, Count );
}
#endif

#ifndef DEFINED_appMemcpy
void appMemcpy( void* Dest, const void* Src, INT Count )
{
	memcpy( Dest, Src, Count );
}
#endif

/*-----------------------------------------------------------------------------
	String functions.
-----------------------------------------------------------------------------*/

//
// Copy a string with length checking.
//warning: Behavior differs from strncpy; last character is zeroed.
//
TCHAR* appStrncpy( TCHAR* Dest, const TCHAR* Src, INT MaxLen )
{
#if UNICODE
	wcsncpy( Dest, Src, MaxLen );
#else
	strncpy( Dest, Src, MaxLen );
#endif
	Dest[MaxLen-1]=0;
	return Dest;
}

//
// Concatenate a string with length checking
//
TCHAR* appStrncat( TCHAR* Dest, const TCHAR* Src, INT MaxLen )
{
	INT Len = appStrlen(Dest);
	TCHAR* NewDest = Dest + Len;
	if( (MaxLen-=Len) > 0 )
	{
		appStrncpy( NewDest, Src, MaxLen );
		NewDest[MaxLen-1] = 0;
	}
	return Dest;
}

//
// Standard string functions.
//
VARARG_BODY( INT, appSprintf, const TCHAR*, VARARG_EXTRA(TCHAR* Dest) )
{
	INT	Result = -1;
	va_list ap;
	va_start(ap, Fmt);
	//@warning: make sure code using appSprintf allocates enough memory if the below 1024 is ever changed.
	GET_VARARGS_RESULT(Dest,1024/*!!*/,Fmt,Fmt,Result);
	return Result;
}

#if _MSC_VER
INT appGetVarArgs( TCHAR* Dest, INT Count, const TCHAR*& Fmt, va_list ArgPtr )
{
#if UNICODE
	INT Result = _vsnwprintf( Dest, Count, Fmt, ArgPtr );
#else
	INT Result = _vsnprintf( Dest, Count, Fmt, ArgPtr );
#endif
	va_end( ArgPtr );
	return Result;
}

INT appGetVarArgsAnsi( ANSICHAR* Dest, INT Count, const ANSICHAR*& Fmt, va_list ArgPtr)
{
	INT Result = _vsnprintf( Dest, Count, Fmt, ArgPtr );
	va_end( ArgPtr );
	return Result;
}
#endif

/*-----------------------------------------------------------------------------
	Sorting.
-----------------------------------------------------------------------------*/

void appQsort( void* Base, INT Num, INT Width, int(CDECL *Compare)(const void* A, const void* B ) )
{
	qsort( Base, Num, Width, Compare );
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

