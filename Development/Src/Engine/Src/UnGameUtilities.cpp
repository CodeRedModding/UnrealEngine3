/*=============================================================================
	UnGameUtilities.cpp: APawn AI implementation

  This contains game utilities used by script for accessing files

	Copyright 2000 Epic MegaGames, Inc. This software is a trade secret.

	Revision history:
		* Created by Steven Polge 4/00
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"
#include "FConfigCacheIni.h"

void AActor::execGetCacheEntry( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(Num);
	P_GET_STR_REF(GUID);
	P_GET_STR_REF(Filename);
	P_FINISH;

	*GUID = TEXT("");
	*Filename = TEXT("");
	TCHAR IniName[256];
	FConfigCacheIni CacheIni;
	appSprintf( IniName, TEXT("%s") PATH_SEPARATOR TEXT("cache.ini"), *GSys->CachePath );
	TMultiMap<FString,FString>* Sec = CacheIni.GetSectionPrivate( TEXT("Cache"), 0, 1, IniName );
	if( Sec )
	{
		INT i = 0;
		for( TMultiMap<FString,FString>::TIterator It(*Sec); It; ++It )
		{
			if( *(*It.Value()) )
			{
				if( i == Num )
				{
					*GUID = *It.Key();
					*Filename = *It.Value();
					*(UBOOL*)Result = 1;
					return;
				}
				i++;
			}
		}
	}
	*(UBOOL*)Result = 0;
}

void AActor::execMoveCacheEntry( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(GUID);
	P_GET_STR_OPTX(NewFilename,TEXT(""));
	P_FINISH;

	*(UBOOL*)Result = 0;
	FConfigCacheIni CacheIni;
	FString IniName = FString::Printf( TEXT("%s") PATH_SEPARATOR TEXT("cache.ini"), *GSys->CachePath );
	FString OrigFilename;
	if( !CacheIni.GetString( TEXT("Cache"), *GUID, OrigFilename, *IniName ) )
		return;
	if( !*(*NewFilename) )
		NewFilename = OrigFilename;
	INT i;
	while( (i=NewFilename.InStr(TEXT("/"))) != -1 )
		NewFilename = NewFilename.Mid(i+1);
	while( (i=NewFilename.InStr(TEXT("\\"))) != -1 )
		NewFilename = NewFilename.Mid(i+1);
	while( (i=NewFilename.InStr(TEXT(":"))) != -1 )
		NewFilename = NewFilename.Mid(i+1);

	// get file extension
	FString FileExt = NewFilename;
	i = FileExt.InStr( TEXT(".") );
	if( i == -1 )
	{
		debugf(TEXT("MoveCacheEntry: No extension: %s"), *NewFilename);
		return;
	}
	while( i != -1 )
	{
		FileExt = FileExt.Mid( i + 1 );
		i = FileExt.InStr( TEXT(".") );
	}
	FileExt = FString(TEXT(".")) + FileExt;
	
	// get the destination path from a Paths array entry such as ../Maps/*.unr
	for( i=0; i<GSys->Paths.Num(); i++ )
	{
		if( GSys->Paths(i).Right( FileExt.Len() ) == FileExt )
		{
			const TCHAR* p;
			for( p = *GSys->Paths(i) + GSys->Paths(i).Len() - 1; p >= *GSys->Paths(i); p-- )
				if( *p=='/' || *p=='\\' || *p==':' )
					break;

			FString DestPath = GSys->Paths(i).Left( p - *GSys->Paths(i) + 1 );
			debugf( TEXT("MoveCacheEntry: %s -> %s"), *(GSys->CachePath + FString(PATH_SEPARATOR) + GUID + GSys->CacheExt), *(DestPath + NewFilename) );
			if( GFileManager->Move( *(DestPath + NewFilename), *(GSys->CachePath + FString(PATH_SEPARATOR) + GUID + GSys->CacheExt) ) )
			{
				CacheIni.SetString( TEXT("Cache"), *GUID, TEXT(""), *IniName );
				*(UBOOL*)Result = 1;
			}
			return;
		}
	}
}

void AActor::execGetURLMap( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	*(FString*)Result = ((UGameEngine*)GetLevel()->Engine)->LastURL.Map;
}

