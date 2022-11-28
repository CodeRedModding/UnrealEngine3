/*=============================================================================
	URebuildCommandlet.cpp: Loads all packages and saves them again, allowing
	PostLoad content updating to do it's work.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Andrew Scheidecker
=============================================================================*/

#include "EditorPrivate.h"

void URebuildCommandlet::StaticConstructor()
{
	IsClient        = 1;
	IsEditor        = 1;
	IsServer        = 1;
	LazyLoad        = 0;
	ShowErrorCount  = 1;
}
INT URebuildCommandlet::Main(const TCHAR *Parms)
{
	UClass* EditorEngineClass = UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail, NULL );
	GEngine = GEditor  = ConstructObject<UEditorEngine>( EditorEngineClass );
	GEditor->UseSound = 0;
    GEditor->InitEditor();
	GIsRequestingExit = 1; // Causes ctrl-c to immediately exit.

	TArray<FString>	PackageFiles = GPackageFileCache->GetPackageFileList();
	for(UINT FileIndex = 0;FileIndex < (UINT)PackageFiles.Num();FileIndex++)
	{
		const TCHAR*	Path = *PackageFiles(FileIndex);

		if(GFileManager->IsReadOnly(Path))
		{
			warnf(NAME_Log,TEXT("Skipping %s (read-only)"),Path);
			continue;
		}

		warnf (NAME_Log, TEXT("Loading %s..."), Path );
		UPackage* Package = CastChecked<UPackage>( LoadPackage( NULL, Path, LOAD_NoWarn ) );

		if(Package->bDirty)
		{
			warnf (NAME_Log, TEXT("Saving...") );
			SavePackage( Package, NULL, RF_Standalone, Path, GError, NULL );
		}

		warnf (NAME_Log, TEXT("Cleaning up...") );

		UObject::CollectGarbage( RF_Native );
	}
	return 0;
}
IMPLEMENT_CLASS(URebuildCommandlet)
