/*=============================================================================
	UStripSourceCommandlet.cpp: Load a .u file and remove the script text from
	all classes.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Jack Porter

=============================================================================*/

#include "EditorPrivate.h"

/*-----------------------------------------------------------------------------
	UStripSourceCommandlet
-----------------------------------------------------------------------------*/

void UStripSourceCommandlet::StaticConstructor()
{
	IsClient        = 1;
	IsEditor        = 1;
	IsServer        = 1;
	LazyLoad        = 0;
	ShowErrorCount  = 1;
}
INT UStripSourceCommandlet::Main( const TCHAR* Parms )
{
	FString PackageName;
	if( !ParseToken(Parms, PackageName, 0) )
		appErrorf( TEXT("A .u package file must be specified.") );

	warnf( TEXT("Loading package %s..."), *PackageName );
	warnf(TEXT(""));
	UObject* Package = LoadPackage( NULL, *PackageName, LOAD_NoWarn );
	if( !Package )
		appErrorf( TEXT("Unable to load %s"), *PackageName );


	for( TObjectIterator<UClass> It; It; ++It )
	{
		if( It->GetOuter() == Package && It->ScriptText )
		{
			warnf( TEXT("  Stripping source code from class %s"), It->GetName() );
			It->ScriptText->Text = FString(TEXT(" "));
			It->ScriptText->Pos = 0;
			It->ScriptText->Top = 0;
		}
	}

	warnf(TEXT(""));
	warnf(TEXT("Saving %s..."), *PackageName );
	SavePackage( Package, NULL, RF_Standalone, *PackageName, GWarn );

	GIsRequestingExit=1;
	return 0;
}
IMPLEMENT_CLASS(UStripSourceCommandlet)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
