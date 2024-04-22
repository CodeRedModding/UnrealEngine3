/*=============================================================================
	UnCDKey.cpp: CD Key validation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	Internal MD5 Stuff
-----------------------------------------------------------------------------*/

//!! TEMP - need to load cdkey from disk.
#define CDKEY TEXT("54321")

static FString GetDigestString( BYTE* Digest )
{
	FString MD5;
	for( INT i=0; i<16; i++ )
		MD5 += FString::Printf(TEXT("%02x"), Digest[i]);	
	return MD5;
}

FString MD5HashAnsiString( const TCHAR* String )
{
	BYTE Digest[16];
	FMD5Context Context;
	appMD5Init( &Context );
	appMD5Update( &Context, (unsigned char*)TCHAR_TO_ANSI( String ), appStrlen( String ) );
	appMD5Final( Digest, &Context );
	return GetDigestString( Digest );
}

/*-----------------------------------------------------------------------------
	Global CD Key functions
-----------------------------------------------------------------------------*/

FString GetCDKeyHash()
{
	return MD5HashAnsiString(CDKEY);
}

FString GetCDKeyResponse( const TCHAR* Challenge )
{
	FString CDKey;
	
	// Get real CD Key
	CDKey = CDKEY;
    
	// Append challenge
	CDKey += Challenge;

	// MD5
	return MD5HashAnsiString( *CDKey );
}

