/*=============================================================================
	UConformCommandlet.cpp: Unreal file conformer.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#include "EditorPrivate.h"
/*-----------------------------------------------------------------------------
	UConformCommandlet.
-----------------------------------------------------------------------------*/

void UConformCommandlet::StaticConstructor()
{
	IsClient        = 1;
	IsEditor        = 1;
	IsServer        = 1;
	LazyLoad        = 1;
	ShowErrorCount  = 1;

}
INT UConformCommandlet::Main( const TCHAR* Parms )
{
	FString Src, Old;
	if( !ParseToken(Parms,Src,0) )
		appErrorf(TEXT("Source file not specified"));
	if( !ParseToken(Parms,Old,0) )
		appErrorf(TEXT("Old file not specified"));
	GWarn->Log( TEXT("Loading...") );
	BeginLoad();
	ULinkerLoad* OldLinker = UObject::GetPackageLinker( CreatePackage(NULL,*(Old+FString(TEXT("_OLD")))), *Old, LOAD_NoWarn|LOAD_NoVerify, NULL, NULL );
	EndLoad();
	UObject* NewPackage = LoadPackage( NULL, *Src, LOAD_NoFail );
	if( !OldLinker )
		appErrorf( TEXT("Old file '%s' load failed"), *Old );
	if( !NewPackage )
		appErrorf( TEXT("New file '%s' load failed"), *Src );
	GWarn->Log( TEXT("Saving...") );
	SavePackage( NewPackage, NULL, RF_Standalone, *Src, GError, OldLinker );
	warnf( TEXT("File %s successfully conformed to %s..."), *Src, *Old );
	GIsRequestingExit=1;
	return 0;
}
IMPLEMENT_CLASS(UConformCommandlet)

/*-----------------------------------------------------------------------------
	UCheckUnicodeCommandlet.
-----------------------------------------------------------------------------*/

void UCheckUnicodeCommandlet::StaticConstructor()
{
	IsClient        = 1;
	IsEditor        = 1;
	IsServer        = 1;
	LazyLoad        = 1;
	ShowErrorCount  = 1;

}
INT UCheckUnicodeCommandlet::Main( const TCHAR* Parms )
{
	FString Path, Wildcard;
	if( !ParseToken(Parms,Path,0) )
		appErrorf(TEXT("Missing path"));
	if( !ParseToken(Parms,Wildcard,0) )
		appErrorf(TEXT("Missing wildcard"));
	GWarn->Log( TEXT("Files:") );
	TArray<FString> Files;
	GFileManager->FindFiles(Files,*(Path*Wildcard),1,0);
	INT Pages[256];
	BYTE* Chars = (BYTE*)appMalloc(65536*sizeof(BYTE));
	appMemzero(Pages,sizeof(Pages));
	appMemzero(Chars,65536*sizeof(BYTE));
	for( TArray<FString>::TIterator i(Files); i; ++i )
	{
		FString S;
		warnf( TEXT("Checking: %s"),*(Path * *i));
		verify(appLoadFileToString(S,*(Path * *i)));
		for( INT i=0; i<S.Len(); i++ )
		{
			warnf(TEXT("Found Character: %i"), (*S)[i]);
			if( Chars[(*S)[i]] == 0 )
			{
				Pages[(*S)[i]/256]++;
				Chars[(*S)[i]] = 1;
			}
		}

	}
	INT T=0, P=0;
	{for( INT i=0; i<254; i++ )
		if( Pages[i] )
		{
			warnf(TEXT("%x: %i"), i, Pages[i]);
			T += Pages[i];
			P++;
		}
	}
	warnf(TEXT("Total Characters: %i"), T);
	warnf(TEXT("Total Pages: %i"), P);
	
	GIsRequestingExit=1;
	return 0;
}
IMPLEMENT_CLASS(UCheckUnicodeCommandlet)

/*-----------------------------------------------------------------------------
	UPackageFlagCommandlet.
-----------------------------------------------------------------------------*/

void UPackageFlagCommandlet::StaticConstructor()
{
	IsClient        = 1;
	IsEditor        = 1;
	IsServer        = 1;
	LazyLoad        = 1;
	ShowErrorCount  = 1;
}
INT UPackageFlagCommandlet::Main( const TCHAR* Parms )
{
	TCHAR* FlagNames[] = 
				{
					TEXT("AllowDownload"),
					TEXT("ClientOptional"),
					TEXT("ServerSideOnly"),
					TEXT("Unsecure"),
					TEXT("Need")
				};
	DWORD Flags[] = 
				{
					PKG_AllowDownload,
					PKG_ClientOptional,
					PKG_ServerSideOnly,
					PKG_Unsecure,
					PKG_Need
				};
	check(ARRAY_COUNT(FlagNames)==ARRAY_COUNT(Flags));
	INT NumFlags = ARRAY_COUNT(Flags);
	FString Src, Dest;
	if( !ParseToken(Parms,Src,0) )
		appErrorf(TEXT("Source Package file not specified"));
	BeginLoad();
	ULinkerLoad* OldLinker = UObject::GetPackageLinker( CreatePackage(NULL,*(Src+FString(TEXT("_OLD")))), *Src, LOAD_NoWarn|LOAD_NoVerify, NULL, NULL );
	EndLoad();

	UPackage* Package = Cast<UPackage>(LoadPackage( NULL, *Src, LOAD_NoFail ));
	if( !Package )
		appErrorf( TEXT("Source package '%s' load failed"), *Src );

	warnf( TEXT("Loaded %s."), *Src );
	warnf( TEXT("Current flags: %d"), (INT)Package->PackageFlags );
	for( INT i=0;i<NumFlags;i++ )
		if( Package->PackageFlags & Flags[i] )
			warnf( TEXT(" %s"), FlagNames[i]);
	GWarn->Log( TEXT("") );
	if( ParseToken(Parms,Dest,0) )
	{
		FString Flag;
		while( ParseToken(Parms,Flag,0) )
		{
			for( INT i=0;i<NumFlags;i++ )
			{
				if( !appStricmp( &(*Flag)[1], FlagNames[i] ) )
				{
					switch((*Flag)[0])
					{
					case '+':
						Package->PackageFlags |= Flags[i];
						break;
					case '-':
						Package->PackageFlags &= ~Flags[i];
						break;
					}
				}
			}
		}	

		if( !SavePackage( Package, NULL, RF_Standalone, *Dest, GError, OldLinker ) )
			appErrorf( TEXT("Saving package '%s' failed"), *Dest );

		warnf( TEXT("Saved %s."), *Dest );
		warnf( TEXT("New flags: %d"), (INT)Package->PackageFlags );
		for( INT i=0;i<NumFlags;i++ )
			if( Package->PackageFlags & Flags[i] )
				warnf( TEXT(" %s"), FlagNames[i]);
	}
	GIsRequestingExit=1;
	return 0;
}
IMPLEMENT_CLASS(UPackageFlagCommandlet)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
