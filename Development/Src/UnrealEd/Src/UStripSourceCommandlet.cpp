/*=============================================================================
	UStripSourceCommandlet.cpp: Load a .u file and remove the script text from
	all classes.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"

/*-----------------------------------------------------------------------------
	UStripSourceCommandlet
-----------------------------------------------------------------------------*/

INT UStripSourceCommandlet::Main( const FString& Params )
{
	const TCHAR* Parms = *Params;

	FString PackageName, FullPackageName;
	if( !ParseToken(Parms, PackageName, 0) )
		appErrorf( TEXT("A .u package file must be specified.") );

	UBOOL bCppTextOnly = FALSE;
	if (ParseParam(Parms,TEXT("cpponly")))
	{
		bCppTextOnly = TRUE;
	}

	warnf( TEXT("Loading package %s..."), *PackageName );
	warnf(TEXT(""));

	// resolve the package name so that it is saved into the correct location
	GPackageFileCache->FindPackageFile(*PackageName, NULL, FullPackageName);

	UPackage* Package = LoadPackage( NULL, *FullPackageName, LOAD_NoWarn );
	if( !Package )
	{
		appErrorf( TEXT("Unable to load %s"), *PackageName );
	}

	for( TObjectIterator<UStruct> It; It; ++It )
	{
		if (!bCppTextOnly)
		{
			if( It->GetOutermost() == Package && It->ScriptText )
			{
				warnf( TEXT("  Stripping source code from struct %s"), *It->GetName() );
				It->ScriptText->Text = FString(TEXT(" "));
				It->ScriptText->Pos = 0;
				It->ScriptText->Top = 0;
			}
		}

		if( It->GetOutermost() == Package && It->CppText )
		{
			warnf( TEXT("  Stripping cpptext from struct %s"), *It->GetName() );
			It->CppText->Text = FString(TEXT(" "));
			It->CppText->Pos = 0;
			It->CppText->Top = 0;
		}
	}

	Package->PackageFlags |= PKG_StrippedSource;

	warnf(TEXT(""));
	warnf(TEXT("Saving %s..."), *FullPackageName );
	SavePackage( Package, NULL, RF_Standalone, *FullPackageName, GWarn );

	GIsRequestingExit=1;
	return 0;
}
IMPLEMENT_CLASS(UStripSourceCommandlet)

