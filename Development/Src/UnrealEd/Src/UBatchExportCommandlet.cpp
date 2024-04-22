/*=============================================================================
	UBatchExportCommandlet.cpp: Unreal file exporting commandlet.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "LocalizationExport.h"

/*-----------------------------------------------------------------------------
	UBatchExportCommandlet.
-----------------------------------------------------------------------------*/

INT UBatchExportCommandlet::Main( const FString& Params )
{
	const TCHAR* Parms = *Params;
	
	FString Pkg, Cls, Ext, Path;
	if( !ParseToken(Parms,Pkg,0) )
	{
		appErrorf(*LocalizeUnrealEd("Error_PackageFileNotSpecified"));
	}
	if( !ParseToken(Parms,Cls,0) )
	{
		appErrorf(*LocalizeUnrealEd("Error_ExporterNotSpecified"));
	}
	if( !ParseToken(Parms,Ext,0) )
	{
		appErrorf(*LocalizeUnrealEd("Error_FileExtensionNotSpecified"));
	}
	if( !ParseToken(Parms,Path,0) )
	{
		Path=TEXT(".");
	}
	if( Ext.Left(1)==TEXT(".") )
	{
		Ext = Ext.Mid(1);
	}

	UClass* Class = FindObjectChecked<UClass>( ANY_PACKAGE, *Cls );

	warnf( TEXT("Loading package %s..."), *Pkg );
	UObject* Package = LoadPackage(NULL,*Pkg,LOAD_None);
	if ( !Package )
	{
		appErrorf( LocalizeSecure(LocalizeUnrealEd("Error_FailedToLoadPackage"), *Pkg) );
	}

	if( !GFileManager->MakeDirectory( *Path, 1 ) )
	{
		warnf( NAME_Warning, LocalizeSecure(LocalizeUnrealEd("Error_FailedToMakeDirectory"), *Path) );
		return 1;
	}

	for( TObjectIterator<UObject> It; It; ++It )
	{
		if( It->IsA(Class) && It->IsIn(Package) )
		{
			const FString Filename = Path * *It->GetName() + TEXT(".") + Ext;
			const INT Result = UExporter::ExportToFile(*It, NULL, *Filename, 1, 0);
			if ( Result > 0 )
			{
				warnf( TEXT("Exported %s to %s"), *It->GetFullName(), *Filename );
			}
			else if ( Result == 0 )
			{
				appErrorf(LocalizeSecure(LocalizeUnrealEd("Error_CantExportNameToFile"), *It->GetFullName(),*Filename));
			}
		}
	}
	GIsRequestingExit=1;
	return 0;
}
IMPLEMENT_CLASS(UBatchExportCommandlet)
