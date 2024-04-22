/*=============================================================================
	UCContentCommandlets.cpp: Various commmandlets.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EngineMaterialClasses.h"
#include "EngineSequenceClasses.h"
#include "UnPropertyTag.h"
#include "EngineUIPrivateClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineParticleClasses.h"
#include "LensFlare.h"
#include "EngineAnimClasses.h"
#include "UnTerrain.h"
#include "EngineFoliageClasses.h"
#include "SpeedTree.h"
#include "EnginePrefabClasses.h"
#include "Database.h"
#include "EngineSoundClasses.h"
#include "EngineMeshClasses.h"

#include "SourceControl.h"

#include "PackageHelperFunctions.h"
#include "PackageUtilityWorkers.h"

#include "PerfMem.h"
#include "AnimationEncodingFormat.h"
#include "AnimationUtils.h"

#include "UnFile.h"
#include "UnObjectTools.h"
#include "DiagnosticTable.h"

#include "UnConsoleSupportContainer.h"

#if WITH_MANAGED_CODE
	#include "GameAssetDatabaseShared.h"
#include "..\..\Core\Inc\Array.h"
#endif

/*
Below is a template commandlet than can be used when you want to perform an operation on all packages.

INT UPerformAnOperationOnEveryPackage::Main(const FString& Params)
{
	// Parse command line args.
	TArray<FString> Tokens;
	TArray<FString> Switches;

	const TCHAR* Parms = *Params;
	ParseCommandLine(Parms, Tokens, Switches);

	// Build package file list.
	const TArray<FString> FilesInPath( GPackageFileCache->GetPackageFileList() );
	if( FilesInPath.Num() == 0 )
	{
		warnf( NAME_Warning, TEXT("No packages found") );
		return 1;
	}

	// Iterate over all files doing stuff.
	for( INT FileIndex = 0 ; FileIndex < FilesInPath.Num() ; ++FileIndex )
	{
		const FFilename& Filename = FilesInPath(FileIndex);
		warnf( NAME_Log, TEXT("Loading %s"), *Filename );

		UPackage* Package = UObject::LoadPackage( NULL, *Filename, LOAD_None );
		if( Package == NULL )
		{
			warnf( NAME_Error, TEXT("Error loading %s!"), *Filename );
		}

		/////////////////
		//
		// Do your thing here
		//
		/////////////////

		TObjectIterator<UStaticMesh> It;...

		UStaticMesh* StaticMesh = *It;
		if( StaticMesh->IsIn( Package )




		UObject::CollectGarbage( RF_Native );
	}

	return 0;
}
*/


/**-----------------------------------------------------------------------------
 *	UResavePackages commandlet.
 *
 * This commandlet is meant to resave packages as a default.  We are able to pass in
 * flags to determine which conditions we do NOT want to resave packages. (e.g. not dirty
 * or not older than some version)
 *
 *
----------------------------------------------------------------------------**/

#define CURRENT_PACKAGE_VERSION 0
#define IGNORE_PACKAGE_VERSION INDEX_NONE

/**
 * Evalutes the command-line to determine which maps to check.  By default all maps are checked (except PIE and trash-can maps)
 * Provides child classes with a chance to initialize any variables, parse the command line, etc.
 *
 * @param	Tokens			the list of tokens that were passed to the commandlet
 * @param	Switches		the list of switches that were passed on the commandline
 * @param	PackageNames	receives the list of path names for the packa that will be checked.
 *
 * @return	0 to indicate that the commandlet should continue; otherwise, the error code that should be returned by Main()
 */
INT UResavePackagesCommandlet::InitializeResaveParameters( const TArray<FString>& Tokens, const TArray<FString>& Switches, TArray<FFilename>& PackageNames )
{
	TArray<FString> Unused, PackageExtensions;
	UBOOL bExplicitPackages = FALSE;
	GConfig->GetArray( TEXT("Core.System"), TEXT("Extensions"), PackageExtensions, GEngineIni );

	// Check to see if we have an explicit list of packages
	for( INT SwitchIdx = 0; SwitchIdx < Switches.Num(); SwitchIdx++ )
	{
		FString Package;
		const FString& CurrentSwitch = Switches( SwitchIdx );
		if( Parse( *CurrentSwitch, TEXT( "PACKAGE="), Package ) )
		{
			FFilename PackageFile;
			GPackageFileCache->FindPackageFile( *Package, NULL, PackageFile, NULL );
			PackageNames.AddItem( *PackageFile );
			bExplicitPackages = TRUE;
		}
	}

	// ... if not, load in all packages
	if( !bExplicitPackages )
	{
		BYTE PackageFilter = NORMALIZE_DefaultFlags;
		if ( Switches.ContainsItem(TEXT("SKIPMAPS")) )
		{
			PackageFilter |= NORMALIZE_ExcludeMapPackages;
		}
		else if ( Switches.ContainsItem(TEXT("MAPSONLY")) )
		{
			PackageFilter |= NORMALIZE_ExcludeContentPackages;
		}

		for ( INT ExtensionIndex = 0; ExtensionIndex < PackageExtensions.Num(); ExtensionIndex++ )
		{
			if ( PackageExtensions(ExtensionIndex) != TEXT("U") )
			{
				if ( !NormalizePackageNames(Unused, PackageNames, FString(TEXT("*.")) + PackageExtensions(ExtensionIndex), PackageFilter) )
				{
					return 1;
				}
			}
		}
	}

	// Check for a max package limit
	MaxPackagesToResave = -1;
	for ( INT SwitchIdx = 0; SwitchIdx < Switches.Num(); SwitchIdx++ )
	{
		const FString& CurrentSwitch = Switches(SwitchIdx);
		if( Parse(*CurrentSwitch,TEXT("MAXPACKAGESTORESAVE="),MaxPackagesToResave))
		{
			warnf( NAME_Log, TEXT( "Only resaving a maximum of %d packages." ), MaxPackagesToResave );
			break;
		}
	}

	// Check for the min and max versions
	MinResaveVersion = IGNORE_PACKAGE_VERSION;
	MaxResaveVersion = IGNORE_PACKAGE_VERSION;
	MaxResaveLicenseeVersion = IGNORE_PACKAGE_VERSION;
	if ( Switches.ContainsItem(TEXT("CHECKLICENSEEVER")) )
	{
		// Limits resaving to packages with this licensee package version or lower.
		MaxResaveLicenseeVersion = Max<INT>(GPackageFileLicenseeVersion - 1, 0);
	}
	if ( Switches.ContainsItem(TEXT("CHECKVER")) )
	{
		// Limits resaving to packages with this package version or lower.
		MaxResaveVersion = Max<INT>(GPackageFileVersion - 1, 0);
	}
	else if ( Switches.ContainsItem(TEXT("RESAVEDEPRECATED")) )
	{
		// Limits resaving to packages with this package version or lower.
		MaxResaveVersion = Max<INT>(VER_DEPRECATED_PACKAGE - 1, 0);
	}
	else
	{
		// determine if the resave operation should be constrained to certain package versions
		for ( INT SwitchIdx = 0; SwitchIdx < Switches.Num(); SwitchIdx++ )
		{
			const FString& CurrentSwitch = Switches(SwitchIdx);
			if ( MinResaveVersion == IGNORE_PACKAGE_VERSION && Parse(*CurrentSwitch,TEXT("MINVER="),MinResaveVersion) )
			{
				if ( MinResaveVersion == CURRENT_PACKAGE_VERSION )
				{
					MinResaveVersion = GPackageFileVersion;
				}
			}

			if ( MaxResaveVersion == IGNORE_PACKAGE_VERSION && Parse(*CurrentSwitch,TEXT("MAXVER="),MaxResaveVersion) )
			{
				if ( MaxResaveVersion == CURRENT_PACKAGE_VERSION )
				{
					MaxResaveVersion = GPackageFileVersion;
				}
			}
		}
	}

	if ( Switches.ContainsItem(TEXT("UPDATEKISMET")) )
	{
		bUpdateKismet = TRUE;
	}

	if ( Switches.ContainsItem(TEXT("SOUNDCONVERSIONONLY")) )
	{
		bSoundConversionOnly = TRUE;
	}

	FString ClassList;
	for ( INT SwitchIdx = 0; SwitchIdx < Switches.Num(); SwitchIdx++ )
	{
		const FString& CurrentSwitch = Switches(SwitchIdx);
		if ( Parse(*CurrentSwitch, TEXT("RESAVECLASS="), ClassList, FALSE) )
		{
			TArray<FString> ClassNames;
			ClassList.ParseIntoArray(&ClassNames, TEXT(","), TRUE);
			for ( INT Idx = 0; Idx < ClassNames.Num(); Idx++ )
			{
				ResaveClasses.AddItem(*ClassNames(Idx));
			}

			break;
		}
	}

	return 0;
}

INT UResavePackagesCommandlet::Main( const FString& Params )
{
	const TCHAR* Parms = *Params;
	TArray<FString> Tokens, Switches;
	ParseCommandLine(Parms, Tokens, Switches);

#if HAVE_SCC
	// Ensure source control is initialized and shut down properly
	FScopedSourceControl SourceControl;
#endif

	// skip the assert when a package can not be opened
	const UBOOL bCanIgnoreFails				= Switches.ContainsItem(TEXT("SKIPFAILS"));
	/** load all packages, and display warnings for those packages which would have been resaved but were read-only */
	const UBOOL bVerifyContent				= Switches.ContainsItem(TEXT("VERIFY"));
	/** if we should only save dirty packages **/
	const UBOOL bOnlySaveDirtyPackages		= Switches.ContainsItem(TEXT("OnlySaveDirtyPackages"));
	/** if we should auto checkout packages that need to be saved**/
	const UBOOL bAutoCheckOut				= Switches.ContainsItem(TEXT("AutoCheckOutPackages"));

	TArray<FFilename> PackageNames;
	INT ResultCode = InitializeResaveParameters(Tokens, Switches, PackageNames);
	if ( ResultCode != 0 )
	{
		return ResultCode;
	}

	// Retrieve list of all packages in .ini paths.
	if( !PackageNames.Num() )
	{
		return 0;
	}

#if HAVE_SCC
	TArray <FString> FilesToSubmit;
#endif

	INT GCIndex = 0;
	INT PackagesRequiringResave = 0;

	// allow for an option to restart at a given package name (in case it dies during a run, etc)
	UBOOL bCanProcessPackage = TRUE;
	FString FirstPackageToProcess;
	if (Parse(*Params, TEXT("FirstPackage="), FirstPackageToProcess))
	{
		bCanProcessPackage = FALSE;
	}

	// Iterate over all packages.
	for( INT PackageIndex = 0; PackageIndex < PackageNames.Num(); PackageIndex++ )
	{
		// Make sure we don't rebuild SMs that we're not going to save.
		extern FName GStaticMeshPackageNameToRebuild;
		GStaticMeshPackageNameToRebuild = NAME_None;

		const FFilename& Filename = PackageNames(PackageIndex);

		// skip over packages before the first one allowed, if it was specified
		if (!bCanProcessPackage)
		{
			if (Filename.GetBaseFilename() == FirstPackageToProcess)
			{
				bCanProcessPackage = TRUE;
			}
			else
			{
				warnf(NAME_Log, TEXT("Skipping %s"), *Filename);
				continue;
			}
		}

		// we don't care about trying to resave the various shader caches so just skipz0r them
		if(	Filename.InStr( TEXT("LocalShaderCache") ) != INDEX_NONE
		|| Filename.InStr( TEXT("RefShaderCache") ) != INDEX_NONE )
		{
			continue;
		}

		UBOOL bIsReadOnly = GFileManager->IsReadOnly( *Filename);
		if( Filename.GetExtension() == TEXT("U") )
		{
			warnf(NAME_Log, TEXT("Skipping script file %s"), *Filename);
		}
		else if ( bIsReadOnly && !bVerifyContent && !bAutoCheckOut )
		{
			warnf(NAME_Log, TEXT("Skipping read-only file %s"), *Filename);
		}
#if HAVE_SCC
		else if (bAutoCheckOut && FSourceControl::ForceGetStatus( Filename ) == SCC_NotCurrent)
		{
			warnf(NAME_Log, TEXT("Skipping %s (Not at head source control revision)"), *Filename);
		}
#endif
		else
		{
			warnf(NAME_Log, TEXT("Loading %s"), *Filename);

			INT NumErrorsFromLoading = GWarn->Errors.Num();
			if ( NumErrorsFromLoading > 0 )
			{
				warnf(NAME_Log, TEXT("NumErrorsFromLoading %d"), NumErrorsFromLoading);
			}
			
			// Get the package linker.
			UObject::BeginLoad();
			ULinkerLoad* Linker = UObject::GetPackageLinker(NULL,*Filename,LOAD_NoVerify,NULL,NULL);
			UObject::EndLoad();

			UBOOL bSavePackage = TRUE;
			PerformPreloadOperations(Linker, bSavePackage);

			if(bSavePackage)
			{
				PackagesRequiringResave++;

				// Only rebuild static meshes on load for the to be saved package.
				GStaticMeshPackageNameToRebuild = FName(*Filename.GetBaseFilename());

				// Assert if package couldn't be opened so we have no chance of messing up saving later packages.
				UPackage* Package = UObject::LoadPackage( NULL, *Filename, 0 );
				if (Package == NULL)
				{
					if( bCanIgnoreFails == TRUE )
					{
						continue;
					}
					else
					{
						check(Package);
					}
				}

				// if we are only saving dirty packages and the package is not dirty, then we do not want to save the package (remember the default behavior is to ALWAYS save the package)
				if( ( bOnlySaveDirtyPackages == TRUE ) && ( Package->IsDirty() == FALSE ) )
				{
					bSavePackage = FALSE;
				}

				// here we want to check and see if we have any loading warnings
				// if we do then we want to resave this package
				if( !bSavePackage && ParseParam(appCmdLine(),TEXT("SavePackagesThatHaveFailedLoads")) == TRUE )
				{
					//warnf( TEXT( "NumErrorsFromLoading: %d GWarn->Errors num: %d" ), NumErrorsFromLoading, GWarn->Errors.Num() );

					if( NumErrorsFromLoading != GWarn->Errors.Num() )
					{
						bSavePackage = TRUE;
					}
				}


				// hook to allow performing additional checks without lumping everything into this one function
				PerformAdditionalOperations(Package,bSavePackage);

				// Check for any special per object operations
				bSoundWasDirty = FALSE;
				for( FObjectIterator ObjectIt; ObjectIt; ++ObjectIt )
				{
					if( ObjectIt->IsIn( Package ) )
					{
						PerformAdditionalOperations( *ObjectIt, bSavePackage );
					}
				}

				if (bSoundConversionOnly == TRUE)
				{
					if (bSoundWasDirty == FALSE)
					{
						bSavePackage = FALSE;
					}
				}

				// Now based on the computation above we will see if we should actually attempt
				// to save this package
				if( bSavePackage == TRUE )
				{
					if( bIsReadOnly == TRUE && bVerifyContent == TRUE && bAutoCheckOut == FALSE )
					{
						warnf(NAME_Log,TEXT("Package [%s] is read-only but needs to be resaved (Package Version: %i, Licensee Version: %i  Current Version: %i, Current Licensee Version: %i)"),
							*Filename, Linker->Summary.GetFileVersion(), Linker->Summary.GetFileVersionLicensee(), VER_LATEST_ENGINE, VER_LATEST_ENGINE_LICENSEE );
					}
					else
					{
						// check to see if we need to check this package out
#if HAVE_SCC
						if ( bAutoCheckOut )
						{
							if( FSourceControl::ForceGetStatus( Filename ) == SCC_CheckedOutOther )
							{
								warnf(NAME_Log, TEXT("[REPORT] Skipping deprecated package %s (already checked out by someone else)"), *Filename);
							}
							else if( bIsReadOnly )
							{
								FSourceControl::CheckOut(Package);

								FString PackageName(Filename.GetBaseFilename());
								FilesToSubmit.AddItem(*PackageName);
							}
						}
#endif
						// so now we need to see if we actually were able to check this file out
						// if the file is still read only then we failed and need to emit an error and go to the next package
						if( GFileManager->IsReadOnly( *Filename ) == TRUE )
						{
							warnf( NAME_Error, TEXT("Unable to check out the Package: %s"), *Filename );
							continue;
						}

						warnf(NAME_Log,TEXT("Resaving package [%s] (Package Version: %i, Licensee Version: %i  Saved Version: %i, Saved Licensee Version: %i)"),
							*Filename, Linker->Summary.GetFileVersion(), Linker->Summary.GetFileVersionLicensee(), VER_LATEST_ENGINE, VER_LATEST_ENGINE_LICENSEE );
						if( SavePackageHelper(Package, Filename) )
						{
							warnf( NAME_Log, TEXT("Correctly saved:  [%s]."), *Filename );
						}
					}
				}
			}

			UObject::CollectGarbage(RF_Native);
		}

		// Potentially save the local shader cache to include this packages' shaders.
		SaveLocalShaderCaches();

		// Break out if we've resaved enough packages
		if( MaxPackagesToResave > -1 && PackagesRequiringResave >= MaxPackagesToResave )
		{
			warnf( NAME_Log, TEXT( "Attempting to resave more than MaxPackagesToResave; exiting" ) );
			break;
		}
	}

#if HAVE_SCC
	// Submit the results to source control
	if( bAutoCheckOut )
	{
		FSourceControl::Init();

		// Check in all changed files
		if( FilesToSubmit.Num() > 0 )
		{
			TArray <FString> NoFiles;

			FString Description = FString::Printf( TEXT( "Resave Deprecated Packages" ) );
			FSourceControl::ConvertPackageNamesToSourceControlPaths(FilesToSubmit);
			FSourceControl::CheckIn(NULL,NoFiles,FilesToSubmit,Description);
		}

		// toss the SCC manager
		FSourceControl::Close();
	}
#endif

	warnf( NAME_Log, TEXT( "[REPORT] %d/%d packages required resaving" ), PackagesRequiringResave, PackageNames.Num() );
	return 0;
}

/**
 * Allow the commandlet to perform any operations on the export/import table of the package before all objects in the package are loaded.
 *
 * @param	PackageLinker	the linker for the package about to be loaded
 * @param	bSavePackage	[in]	indicates whether the package is currently going to be saved
 *							[out]	set to TRUE to resave the package
 */
UBOOL UResavePackagesCommandlet::PerformPreloadOperations( ULinkerLoad* PackageLinker, UBOOL& bSavePackage )
{
	UBOOL bResult = FALSE;

	const INT PackageVersion = PackageLinker->Summary.GetFileVersion();
	const INT LicenseePackageVersion = PackageLinker->Summary.GetFileVersionLicensee();

	// validate that this package meets the minimum requirement
	if ( MinResaveVersion != IGNORE_PACKAGE_VERSION && PackageVersion < MinResaveVersion )
	{
		bSavePackage = FALSE;
		bResult = TRUE;
	}

	// Check if this package meets the maximum requirements.
	UBOOL bNoLimitation = MaxResaveVersion == IGNORE_PACKAGE_VERSION && MaxResaveLicenseeVersion == IGNORE_PACKAGE_VERSION;
	UBOOL bAllowResave = bNoLimitation ||
						 (MaxResaveVersion != IGNORE_PACKAGE_VERSION && PackageVersion <= MaxResaveVersion) ||
						 (MaxResaveLicenseeVersion != IGNORE_PACKAGE_VERSION && LicenseePackageVersion <= MaxResaveLicenseeVersion);

	// If not, don't resave it.
	if ( !bAllowResave )
	{
		bSavePackage = FALSE;
		bResult = TRUE;
	}

	// Check if the package contains any instances of the class that needs to be resaved.
	if ( bSavePackage && ResaveClasses.Num() > 0 )
	{
		bSavePackage = FALSE;
		for (INT ExportIndex = 0; ExportIndex < PackageLinker->ExportMap.Num(); ExportIndex++)
		{
			if ( ResaveClasses.HasKey(PackageLinker->GetExportClassName(ExportIndex)) )
			{
				bSavePackage = TRUE;
				break;
			}
		}

		bResult = TRUE;
	}

	return bResult;
}

/**
 * Allows the commandlet to perform any additional operations on the object before it is resaved.
 *
 * @param	Object			the object in the current package that is currently being processed
 * @param	bSavePackage	[in]	indicates whether the package is currently going to be saved
 *							[out]	set to TRUE to resave the package
 */
void UResavePackagesCommandlet::PerformAdditionalOperations( class UObject* Object, UBOOL& bSavePackage )
{
	check( Object );
	UBOOL bShouldSavePackage = FALSE;

	if( Object->IsA( USoundNodeWave::StaticClass() ) )
	{
		bShouldSavePackage = CookSoundNodeWave( ( USoundNodeWave* )Object );
		if (bShouldSavePackage == TRUE)
		{
			bSoundWasDirty = TRUE;
		}
	}
	else if( !bSoundConversionOnly && bUpdateKismet && Object->IsA( USequenceObject::StaticClass() ) )
	{
		USequenceObject* SeqObj = Cast<USequenceObject>(Object);
		USequenceObject* NewObj = NULL;

		if( SeqObj )
		{	
			NewObj = SeqObj->ConvertObject();
			if( NewObj )
			{
				NewObj->UpdateObject();
				Object = NewObj;
				bShouldSavePackage = TRUE;
			}
			else if( SeqObj->eventGetObjClassVersion() != SeqObj->ObjInstanceVersion)
			{
				SeqObj->UpdateObject();
				bShouldSavePackage = TRUE;
			}
		}
	}
	else if (Object->IsA(UTexture2D::StaticClass()))
	{
		// Cache PVRTC Textures if we are cooking for a mobile platform or if we've been asked to always optimize content for mobile
		UE3::EPlatformType Platform = ParsePlatformType( appCmdLine() );

		// Read whether to cache mobile textures from the ini file
		UBOOL bShouldCachePVRTCTextures = FALSE;
		UBOOL bShouldCacheATITCTextures = FALSE;
		UBOOL bShouldCacheETCTextures	= FALSE;
		UBOOL bShouldCacheFlashTextures = FALSE;
		GConfig->GetBool(TEXT("MobileSupport"), TEXT("bShouldCachePVRTCTextures"), bShouldCachePVRTCTextures, GEngineIni);
		GConfig->GetBool(TEXT("MobileSupport"), TEXT("bShouldCacheATITCTextures"), bShouldCacheATITCTextures, GEngineIni);
		GConfig->GetBool(TEXT("MobileSupport"), TEXT("bShouldCacheETCTextures"), bShouldCacheETCTextures, GEngineIni);
		GConfig->GetBool(TEXT("MobileSupport"), TEXT("bShouldCacheFlashTextures"), bShouldCacheFlashTextures, GEngineIni);

		if (bShouldCachePVRTCTextures || bShouldCacheATITCTextures || bShouldCacheFlashTextures)
		{
			UTexture2D* Texture = (UTexture2D*)Object;
			if (bShouldCachePVRTCTextures && ConditionalCachePVRTCTextures(Texture, FALSE))
			{
				bShouldSavePackage = TRUE;
			}

			if (bShouldCacheATITCTextures && ConditionalCacheATITCTextures(Texture, FALSE))
			{
				bShouldSavePackage = TRUE;
			}

			if (bShouldCacheETCTextures && ConditionalCacheETCTextures(Texture))
			{
				bShouldSavePackage = TRUE;
			}

			if (bShouldCacheFlashTextures && ConditionalCacheFlashTextures(Texture, FALSE))
			{
				bShouldSavePackage = TRUE;
			}
		}
	}

	// add additional operations here
	bSavePackage = bSavePackage || bShouldSavePackage;
}

/**
 * Allows the commandlet to perform any additional operations on the package before it is resaved.
 *
 * @param	Package			the package that is currently being processed
 * @param	bSavePackage	[in]	indicates whether the package is currently going to be saved
 *							[out]	set to TRUE to resave the package
 */
void UResavePackagesCommandlet::PerformAdditionalOperations( UPackage* Package, UBOOL& bSavePackage )
{
	check(Package);
	UBOOL bShouldSavePackage = FALSE;
	
	if( ( ParseParam(appCmdLine(), TEXT("CLEANCLASSES")) == TRUE ) && ( CleanClassesFromContentPackages(Package) == TRUE ) )
	{
		bShouldSavePackage = TRUE;
	}
	else if( ( ParseParam(appCmdLine(), TEXT("INSTANCEMISSINGSUBOBJECTS")) == TRUE ) && ( InstanceMissingSubObjects(Package) == TRUE ) )
	{
		bShouldSavePackage = TRUE;
	}

	// add additional operations here

	bSavePackage = bSavePackage || bShouldSavePackage;
}

/**
 * Removes any UClass exports from packages which aren't script packages.
 *
 * @param	Package			the package that is currently being processed
 *
 * @return	TRUE to resave the package
 */
UBOOL UResavePackagesCommandlet::CleanClassesFromContentPackages( UPackage* Package )
{
	check(Package);
	UBOOL bResult = FALSE;

	for ( TObjectIterator<UClass> It; It; ++It )
	{
		if ( It->IsIn(Package) )
		{
			warnf(NAME_Log, TEXT("Removing class '%s' from package [%s]"), *It->GetPathName(), *Package->GetName());

			// mark the class as transient so that it won't be saved into the package
			It->SetFlags(RF_Transient);

			// clear the standalone flag just to be sure :)
			It->ClearFlags(RF_Standalone);
			bResult = TRUE;
		}
	}

	return bResult;
}

/**
 * Instances subobjects for any existing objects with subobject properties pointing to the default object.
 * This is currently the case when a class has an object property and subobject definition added to it --
 * existing instances of such a class will see the new object property refer to the template object.
 *
 * @param	Package			The package that is currently being processed.
 *
 * @return					TRUE to resave the package.
 */
UBOOL UResavePackagesCommandlet::InstanceMissingSubObjects( UPackage* Package )
{
	check( Package );
	UBOOL bResult = FALSE;

	for ( TObjectIterator<UObject> It; It; ++It )
	{
		if ( It->IsIn( Package ) )
		{
			if ( !It->IsTemplate() )
			{
				for ( TFieldIterator<UProperty> ItP(It->GetClass()) ; ItP ; ++ItP )
				{
					UProperty* Property = *ItP;
					if ( Property->PropertyFlags & CPF_NeedCtorLink )
					{
						UBOOL bAlreadyLogged = FALSE;
						for ( INT i = 0 ; i < Property->ArrayDim ; ++i )
						{
							FString	Value;
							if ( Property->ExportText( i, Value, (BYTE*)*It, (BYTE*)*It, *It, PPF_Localized ) )
							{
								UObject* SubObject = UObject::StaticFindObject( UObject::StaticClass(), ANY_PACKAGE, *Value );
								if ( SubObject )
								{
									if ( SubObject->IsTemplate() )
									{
										// The heap memory allocated at this address is owned the source object, so
										// zero out the existing data so that the UProperty code doesn't attempt to
										// deallocate it before allocating the memory for the new value.
										appMemzero( (BYTE*)*It + Property->Offset, Property->GetSize() );
										Property->CopyCompleteValue( (BYTE*)*It + Property->Offset, &SubObject, *It/*NULL*/, *It );
										bResult = TRUE;
										if ( !bAlreadyLogged )
										{
											bAlreadyLogged = TRUE;
											if ( Property->ArrayDim == 1 )
											{
												warnf( NAME_Log, TEXT("Instancing %s::%s"), *It->GetPathName(), *Property->GetName() );
											}
											else
											{
												warnf( NAME_Log, TEXT("Instancing %s::%s[%i]"), *It->GetPathName(), *Property->GetName(), i );
											}
											
										}
									}
								}
							}
						} // Property->ArrayDim
					} // If property NeedCtorLink
				} // Field iterator
			} // if It->IsTemplate
		} // If in package
	} // Object iterator

	return bResult;
}

IMPLEMENT_CLASS(UResavePackagesCommandlet);


/**
 * Allow the commandlet to perform any operations on the export/import table of the package before all objects in the package are loaded.
 *
 * @param	PackageLinker	the linker for the package about to be loaded
 * @param	bSavePackage	[in]	indicates whether the package is currently going to be saved
 *							[out]	set to TRUE to resave the package
 */
UBOOL UChangePrefabSequenceClassCommandlet::PerformPreloadOperations( ULinkerLoad* PackageLinker, UBOOL& bSavePackage )
{
	UBOOL bResult = Super::PerformPreloadOperations(PackageLinker, bSavePackage);
	if ( bResult && bSavePackage )
	{
		bSavePackage = FALSE;

		PACKAGE_INDEX EnginePackageIndex = 0;
		PACKAGE_INDEX SequenceClassIndex = 0;
		PACKAGE_INDEX PrefabSeqContainerClassImportIndex=0;
		PACKAGE_INDEX PrefabSeqClassImportIndex=0;
		PACKAGE_INDEX PrefabClassIndex = 0;

		UClass* PrefabSeqContainerClass = UPrefabSequenceContainer::StaticClass();
		UClass* PrefabSeqClass = UPrefabSequence::StaticClass();

		static const FName SequenceClassName = USequence::StaticClass()->GetFName();
		static const FName PrefabSeqContainerClassName = PrefabSeqContainerClass->GetFName();
		static const FName PrefabSeqClassName = PrefabSeqClass->GetFName();
		static const FName PrefabSequenceName = PREFAB_SEQCONTAINER_NAME;
		static const FName PrefabClassName = UPrefab::StaticClass()->GetFName();

		// if we're here, we know that this package contains at least one prefab instance
		for ( INT ImportIndex = 0; ImportIndex < PackageLinker->ImportMap.Num(); ImportIndex++ )
		{
			FObjectImport& Import = PackageLinker->ImportMap(ImportIndex);
			if ( Import.ClassName == NAME_Class )
			{
				if ( Import.ObjectName == PrefabSeqContainerClassName )
				{
					PrefabSeqContainerClassImportIndex = -ImportIndex-1;
				}
				else if ( Import.ObjectName == PrefabSeqClassName )
				{
					PrefabSeqClassImportIndex = -ImportIndex-1;
				}
				else if ( Import.ObjectName == SequenceClassName )
				{
					SequenceClassIndex = -ImportIndex-1;
				}
				else if ( Import.ObjectName == PrefabClassName )
				{
					PrefabClassIndex = -ImportIndex-1;
				}
			}
			else if ( Import.ClassName == NAME_Package && Import.ObjectName == NAME_Engine )
			{
				EnginePackageIndex = -ImportIndex-1;
			}
		}

		if ( PrefabSeqContainerClassImportIndex == 0 )
		{
			// add it
			check(EnginePackageIndex != 0);
			PrefabSeqContainerClassImportIndex = -PackageLinker->ImportMap.Num()-1;
			FObjectImport* ContainerClassImport = new(PackageLinker->ImportMap) FObjectImport(NULL);
			ContainerClassImport->ObjectName = PrefabSeqContainerClassName;
			ContainerClassImport->OuterIndex = EnginePackageIndex;
			ContainerClassImport->ClassName = NAME_Class;
			ContainerClassImport->ClassPackage = NAME_Core;

			FObjectImport* ContainerCDO = new(PackageLinker->ImportMap) FObjectImport(NULL);
			ContainerCDO->ObjectName = PrefabSeqContainerClass->GetDefaultObject()->GetFName();
			ContainerCDO->OuterIndex = EnginePackageIndex;
			ContainerCDO->ClassName = PrefabSeqContainerClassName;
			ContainerCDO->ClassPackage = NAME_Engine;
			bResult = TRUE;
		}

		if ( PrefabSeqClassImportIndex == 0 )
		{
			check(EnginePackageIndex!=0);
			check(PrefabSeqContainerClassImportIndex!=0);

			PrefabSeqClassImportIndex = -PackageLinker->ImportMap.Num() - 1;
			FObjectImport* SeqClassImport = new(PackageLinker->ImportMap) FObjectImport(NULL);
			SeqClassImport->ObjectName = PrefabSeqClassName;
			SeqClassImport->OuterIndex = EnginePackageIndex;
			SeqClassImport->ClassName = NAME_Class;
			SeqClassImport->ClassPackage = NAME_Core;

			FObjectImport* PrefSeqCDO = new(PackageLinker->ImportMap) FObjectImport(NULL);
			PrefSeqCDO->ObjectName = PrefabSeqClass->GetDefaultObject()->GetFName();
			PrefSeqCDO->OuterIndex = EnginePackageIndex;
			PrefSeqCDO->ClassName = PrefabSeqClassName;
			PrefSeqCDO->ClassPackage = NAME_Engine;
			bResult = TRUE;
		}

		// it's possible that none of the PrefabInstance or Prefabs actually have a sequence, in which case the sequence class might
		// not necessarily be in the import map
		if ( SequenceClassIndex != 0 )
		{
			warnf(TEXT("Performing sequence class fixup for %s"), *PackageLinker->Filename);
			for ( INT ExportIndex = 0; ExportIndex < PackageLinker->ExportMap.Num(); ExportIndex++ )
			{
				FObjectExport& Export = PackageLinker->ExportMap(ExportIndex);
				if ( Export.ClassIndex == SequenceClassIndex )
				{
					if ( Export.ObjectName == PrefabSequenceName )
					{
						warnf(TEXT("\tFound export for Prefabs sequence (%s).  Changing class to PrefabSequenceContainer (%i)."), 
							*PackageLinker->GetExportPathName(ExportIndex), -PrefabSeqContainerClassImportIndex-1);

						Export.ClassIndex = PrefabSeqContainerClassImportIndex;
						bResult = bSavePackage = TRUE;
					}
					else
					{
						FObjectExport& OuterExport = PackageLinker->ExportMap(Export.OuterIndex - 1);
						if ( OuterExport.ObjectName == PrefabSequenceName || OuterExport.ClassIndex == PrefabClassIndex )
						{
							warnf(TEXT("\tFound export for %s sequence (%s).  Changing class to PrefabSequence (%i)."), 
								OuterExport.ClassIndex == PrefabClassIndex ? TEXT("Prefab") : TEXT("PrefabInstance"),
								*PackageLinker->GetExportPathName(ExportIndex), -PrefabSeqClassImportIndex-1);

							Export.ClassIndex = PrefabSeqClassImportIndex;
							bResult = bSavePackage = TRUE;
						}
					}
				}
			}
		}
	}

	return bResult;
}

/**
 * Allows the commandlet to perform any additional operations on the object before it is resaved.
 *
 * @param	Object			the object in the current package that is currently being processed
 * @param	bSavePackage	[in]	indicates whether the package is currently going to be saved
 *							[out]	set to TRUE to resave the package
 */
void UChangePrefabSequenceClassCommandlet::PerformAdditionalOperations( UObject* Object, UBOOL& bSavePackage )
{
	if ( Object->GetClass() == APrefabInstance::StaticClass() )
	{
		APrefabInstance* PrefInst = Cast<APrefabInstance>(Object);
		if ( PrefInst->SequenceInstance != NULL && PrefInst->SequenceInstance->GetOwnerPrefab() == NULL )
		{
			PrefInst->SequenceInstance->SetOwnerPrefab(PrefInst);
			bSavePackage = TRUE;
		}
	}
}

IMPLEMENT_CLASS(UChangePrefabSequenceClassCommandlet);

/*-----------------------------------------------------------------------------
	UScaleAudioVolumeCommandlet commandlet.
-----------------------------------------------------------------------------*/

INT UScaleAudioVolumeCommandlet::Main( const FString& Params )
{
	const TCHAR* Parms = *Params;

	// Retrieve list of all packages in .ini paths.
	TArray<FString> PackageList = GPackageFileCache->GetPackageFileList();
	if( !PackageList.Num() )
		return 0;

	// Read the scale amount from the commandline
	FLOAT VolumeScale = 1.f;
	if ( !Parse(Parms, TEXT("VolumeScale="), VolumeScale) )
	{
		warnf(NAME_Log, TEXT("Failed to parse volume scale"));
		return 0;
	}

	TArray<FString> SoundPackages;
#if HAVE_SCC
	FSourceControl::Init();
#endif

	// Iterate over all packages.
	for( INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++ )
	{
		const FFilename& Filename = PackageList(PackageIndex);

		if( Filename.GetExtension() == TEXT("U") )
		{
			warnf(NAME_Log, TEXT("Skipping script file %s"), *Filename);
		}
		else
		{
			warnf(NAME_Log, TEXT("Loading %s"), *Filename);

			// Assert if package couldn't be opened so we have no chance of messing up saving later packages.
			UPackage* Package = UObject::LoadPackage( NULL, *Filename, 0 );
			if (Package != NULL)
			{
				// Iterate over all SoundNodeWave objects
				UBOOL bHasSounds = FALSE;
				warnf(NAME_Log, TEXT("Looking for wave nodes..."));
				for (TObjectIterator<USoundNodeWave> NodeIt; NodeIt; ++NodeIt)
				{
					if (NodeIt->IsIn(Package))
					{
						bHasSounds = TRUE;
						// scale the volume
						NodeIt->Volume = NodeIt->Volume * VolumeScale;
					}
				}
				if ( bHasSounds )
				{
					if (GFileManager->IsReadOnly(*Filename))
					{
#if HAVE_SCC
						FSourceControl::CheckOut(Package);
#endif

						if (GFileManager->IsReadOnly(*Filename))
						{
							SoundPackages.AddItem(Package->GetFullName());
							continue;
						}
					}

					// resave the package
					SavePackageHelper(Package, Filename);
				}
			}
		}

		UObject::CollectGarbage(RF_Native);
	}

	for (INT Idx = 0; Idx < SoundPackages.Num(); Idx++)
	{
		warnf(TEXT("Failed to save sound package %s"),*SoundPackages(Idx));
	}

#if HAVE_SCC
	FSourceControl::Close(); // clean up our allocated SCC
#endif

	return 0;
}
IMPLEMENT_CLASS(UScaleAudioVolumeCommandlet)

/*-----------------------------------------------------------------------------
	UConformCommandlet commandlet.
-----------------------------------------------------------------------------*/

/**
 * Allows the commandlet to perform any additional operations on the object before it is resaved.
 *
 * @param	Object			the object in the current package that is currently being processed
 * @param	bSavePackage	[in]	indicates whether the package is currently going to be saved
 *							[out]	set to TRUE to resave the package
 */
void UConformCommandlet::PerformAdditionalOperations( class UObject* Object )
{
	check( Object );
	UBOOL bShouldSavePackage = FALSE;

	if( Object->IsA( USoundNodeWave::StaticClass() ) )
	{
		CookSoundNodeWave( ( USoundNodeWave* )Object );
	}
	else if (Object->IsA(UTexture2D::StaticClass()))
	{
		// Cache PVRTC Textures if we are cooking for a mobile platform or if we've been asked to always optimize content for mobile
		UE3::EPlatformType Platform = ParsePlatformType( appCmdLine() );
		UBOOL bShouldCachePVRTCTextures = ( Platform & UE3::PLATFORM_Mobile ) || GAlwaysOptimizeContentForMobile;

		// If in other mode, check the config file for this setting
		if( !bShouldCachePVRTCTextures )
		{
			GConfig->GetBool(TEXT("MobileSupport"), TEXT("bShouldCachePVRTCTextures"), bShouldCachePVRTCTextures, GEngineIni);
		}

		if (bShouldCachePVRTCTextures)
		{
			UTexture2D* Texture = (UTexture2D*)Object;
			if (ConditionalCachePVRTCTextures(Texture, FALSE))
			{
				bShouldSavePackage = TRUE;
			}
		}
	}

	// add additional operations here
}


INT UConformCommandlet::Main( const FString& Params )
{
	GIsRequestingExit = TRUE;

	// Get every entry on the commandline
	TArray<FString> Packages, Switches;
	ParseCommandLine( *Params, Packages, Switches );

	if( Packages.Num() < 2 )
	{
		warnf( NAME_Error, TEXT( "Must have at least two packages to conform" ) );
		return 1;
	}

	GForceSoundRecook = ParseParam( *Params, TEXT( "FORCESOUNDRECOOK" ) );

	GWarn->Log( TEXT( "Checking conformity..." ) );
	UBOOL bAlreadyConformed = TRUE;
	for( INT PackageIndex = 1; PackageIndex < Packages.Num(); PackageIndex++ )
	{
		BeginLoad();
		FFilename OldFilename = FFilename( Packages( PackageIndex - 1 ) );
		UPackage* OldPackage = CreatePackage( NULL, *( OldFilename.GetBaseFilename() + TEXT( "_OLD" ) ) );
		ULinkerLoad* OldLinker = UObject::GetPackageLinker( OldPackage, *Packages( PackageIndex - 1 ), LOAD_NoWarn | LOAD_NoVerify, NULL, NULL );
		EndLoad();

		if( !OldLinker )
		{
			warnf( NAME_Error, TEXT( "Failed to load original file '%s'" ), *Packages( PackageIndex - 1 ) );
			return 1;
		}

		BeginLoad();
		FFilename NewFilename = FFilename( Packages( PackageIndex ) );
		UPackage* NewPackage = CreatePackage( NULL, *( NewFilename.GetBaseFilename() ) );
		ULinkerLoad* NewLinker = UObject::GetPackageLinker( NewPackage, *Packages( PackageIndex ), LOAD_NoWarn | LOAD_NoVerify, NULL, NULL );
		EndLoad();

		if( OldLinker->Summary.Guid != NewLinker->Summary.Guid 
			|| OldLinker->Summary.Generations.Num() >= NewLinker->Summary.Generations.Num() )
		{
			bAlreadyConformed = FALSE;
			break;
		}
	}

	// We can early out if everything is conformed
	if( bAlreadyConformed )
	{
		GWarn->Log( TEXT( "All packages already conformed!" ) );
		return 0;
	}

	// Clear out as much as possible
	UObject::ResetLoaders( NULL );
	UObject::CollectGarbage( RF_Native );

	GWarn->Log( TEXT( "Conforming..." ) );
	TArray<FString> SourceFileNames;

	// Make sure all the potential packages are writable so we can update them
	for( INT PackageIndex = 0; PackageIndex < Packages.Num(); PackageIndex++ )
	{
		FString SourceFileName;
		GPackageFileCache->FindPackageFile( *Packages( PackageIndex ), NULL, SourceFileName );
		SourceFileNames.AddItem( SourceFileName );

		// if it's read only, don't bother
		if( GFileManager->IsReadOnly( *SourceFileNames( PackageIndex ) ) )
		{
			warnf( NAME_Error, TEXT( "Package '%s' is read only" ), *SourceFileNames( PackageIndex ) );
			return 1;
		}
	}

	// Force recook the base package if required
	if( GForceSoundRecook )
	{
		UPackage* NewPackage = LoadPackage( NULL, *Packages( 0 ), LOAD_None );
		if( !NewPackage )
		{
			warnf( NAME_Error, TEXT( "Failed to load file being conformed '%s'" ), *Packages( 0 ) );
			return 1;
		}

		// Perform any necessary cooking
		for( FObjectIterator ObjectIt; ObjectIt; ++ObjectIt )
		{
			if( ObjectIt->IsIn( NewPackage ) )
			{
				PerformAdditionalOperations( *ObjectIt );
			}
		}

		if( SavePackageHelper( NewPackage, SourceFileNames( 0 ), RF_Standalone, GWarn ) )
		{
			warnf( TEXT( "File %s successfully saved..." ), *Packages( 0 ) );
		}
		else
		{
			warnf( TEXT( "File %s could not saved. Please see the log for details."), *Packages( 0 ) );
			return 1;
		}
	}

	// Conform the other packages
	for( INT PackageIndex = 1; PackageIndex < Packages.Num(); PackageIndex++ )
	{
		// Old linker could have changed with a previous operation, so always reload it
		BeginLoad();
		FFilename OldFilename = FFilename( Packages( PackageIndex - 1 ) );
		UPackage* OldPackage = CreatePackage( NULL, *( OldFilename.GetBaseFilename() + TEXT( "_OLD" ) ) );
		ULinkerLoad* OldLinker = UObject::GetPackageLinker( OldPackage, *Packages( PackageIndex - 1 ), LOAD_NoWarn | LOAD_NoVerify, NULL, NULL );
		EndLoad();

		if( !OldLinker )
		{
			warnf( NAME_Error, TEXT( "Failed to load original file '%s'" ), *Packages( PackageIndex - 1 ) );
			return 1;
		}

		UPackage* NewPackage = LoadPackage( NULL, *Packages( PackageIndex ), LOAD_None );
		if( !NewPackage )
		{
			warnf( NAME_Error, TEXT( "Failed to load file being conformed '%s'" ), *Packages( PackageIndex ) );
			return 1;
		}

		// Perform any necessary cooking
		for( FObjectIterator ObjectIt; ObjectIt; ++ObjectIt )
		{
			if( ObjectIt->IsIn( NewPackage ) )
			{
				PerformAdditionalOperations( *ObjectIt );
			}
		}

		if( SavePackageHelper( NewPackage, SourceFileNames( PackageIndex ), RF_Standalone, GWarn, OldLinker ) )
		{
			warnf( TEXT( "File %s successfully saved to %s..."), *Packages( PackageIndex ), *SourceFileNames( PackageIndex ) );
		}
		else
		{
			warnf( NAME_Error, TEXT( "File %s could not saved. Please see the log for details."), *Packages( PackageIndex ) );
			return 1;
		}

		// Clear out as much as possible
		UObject::ResetLoaders( NULL );
		UObject::CollectGarbage( RF_Native );
	}
	
	return 0;
}
IMPLEMENT_CLASS(UConformCommandlet)

/*-----------------------------------------------------------------------------
	UDeleteQuarantinedContent commandlet.
-----------------------------------------------------------------------------*/

INT UDeleteQuarantinedContentCommandlet::Main( const FString & Params )
{

#if !WITH_MANAGED_CODE
	warnf( NAME_Warning, TEXT( "GameAssetDatabase not available unless WITH_MANAGED_CODE is defined" ) );
	return -1;
#else

	// Process the commandline
	const TCHAR* Parms = *Params;
	TArray<FString> Tokens, Switches;
	ParseCommandLine(Parms, Tokens, Switches);

#if HAVE_SCC
	// Ensure source control is initialized and shut down properly
	FScopedSourceControl SourceControl;
#endif

	// Startup the game asset database so we'll have access to the tags
	{
		FGameAssetDatabaseStartupConfig StartupConfig;
		FString InitErrorMessageText;
		FGameAssetDatabase::Init(
			StartupConfig,
			InitErrorMessageText );	// Out
		if( InitErrorMessageText.Len() > 0 )
		{
			warnf( NAME_Warning, TEXT( "GameAssetDatabase: %s" ), *InitErrorMessageText );
		}

		if(!FGameAssetDatabase::IsInitialized())
		{
			warnf( NAME_Warning, TEXT( "GameAssetDatabase could not be initialized." ) );
			return 0;
		}
	}

	const FGameAssetDatabase& GAD = FGameAssetDatabase::Get();

	// Skip the assert when a package can not be opened or saved
	const UBOOL bCanIgnoreFails				= Switches.ContainsItem(TEXT("SkipFails"));
	// Skip auto checkout of readonly package files
	const UBOOL bSkipAutoCheckOut			= Switches.ContainsItem(TEXT("SkipCheckout"));
	// Skip reference checks when deleting objects in a package
	const UBOOL bSkipReferenceChecks		= Switches.ContainsItem(TEXT("SkipRefChecks"));
	// Show references of objects we are trying to delete
	const UBOOL bShowObjReferences			= Switches.ContainsItem(TEXT("ShowReferences"));

	TArray<FString> AssetWithTag;
	TArray<FPackageHelper> PackageInfoList;

	// Query all assets with the Quarantined tag
	GAD.QueryAssetsWithTag( "[Quarantined]", AssetWithTag );
	
	// Display the assets we queried from the GAD and process the returned asset info
	warnf(TEXT("Quarantined assets from GAD:"));
	for(INT AssetIdx = 0; AssetIdx < AssetWithTag.Num(); AssetIdx++)
	{
		// Display the info we got from the GAD
		warnf(TEXT("	%s"), *AssetWithTag(AssetIdx));

		UBOOL bUnique = TRUE;
		FString AssetPackageName = ExtractPackageName(AssetWithTag(AssetIdx));

		// Check to see if this asset's package has already been added to our package list.  If it has we will only add to quarantined object list.
		for(int PackageIdx = 0; PackageIdx < PackageInfoList.Num(); PackageIdx++)
		{
			if(PackageInfoList(PackageIdx).PackageNames == AssetPackageName)
			{
				FString QuarantinedObjectName = ExtractAssetName(AssetWithTag(AssetIdx));
				PackageInfoList(PackageIdx).QuarantinedObjectNames.AddUniqueItem(QuarantinedObjectName);
				bUnique = FALSE;
			}
		}

		// If this is the first time we are processing the package, we add it to the package list.
		if(bUnique)
		{
			new( PackageInfoList ) FPackageHelper();
			FPackageHelper& AddedPackage = PackageInfoList.Last();
			AddedPackage.PackageNames = AssetPackageName;

			// Get the path to the package
			if(!GPackageFileCache->FindPackageFile( *AssetPackageName, NULL, AddedPackage.PackageFilePath ))
			{
				warnf(TEXT("File path for package could not be found %s"), *AssetPackageName );
			}
			FString QuarantinedObjectName = ExtractAssetName(AssetWithTag(AssetIdx));
			AddedPackage.QuarantinedObjectNames.AddUniqueItem(QuarantinedObjectName);
		}
	}

	// Iterate over all packages.
	for( INT PackageIndex = 0; PackageIndex < PackageInfoList.Num(); PackageIndex++ )
	{
		const FFilename& Filename = PackageInfoList(PackageIndex).PackageFilePath;
		warnf(NAME_Log, TEXT("Processing Package: %s"), *Filename);
		
		UBOOL bIsReadOnly = GFileManager->IsReadOnly( *Filename);
		if ( bIsReadOnly && bSkipAutoCheckOut )
		{
			warnf(NAME_Log, TEXT("Auto Check Out is disabled, Skipping read-only file %s."), *Filename);
			continue;
		}
#if HAVE_SCC
		else if ( !bSkipAutoCheckOut && FSourceControl::ForceGetStatus( Filename ) == SCC_NotCurrent )
		{
			warnf(NAME_Log, TEXT("Skipping %s (Not at head source control revision)"), *Filename);
			continue;
		}
#endif
		else
		{
			warnf(NAME_Log, TEXT("Loading %s"), *Filename);

			UBOOL bSavePackage = FALSE;
			INT NumErrorsFromLoading = GWarn->Errors.Num();

			// Assert if package couldn't be opened so we have no chance of messing up saving later packages.
			UPackage* Package = UObject::LoadPackage( NULL, *Filename, 0 );
			if (Package == NULL)
			{
				warnf(NAME_Error, TEXT("Package could not be loaded: %s"),*Filename );
				if( bCanIgnoreFails == TRUE )
				{
					continue;
				}
				else
				{
					return 1;
				}
			}

			for(INT ObjIdx = 0; ObjIdx < PackageInfoList(PackageIndex).QuarantinedObjectNames.Num(); ObjIdx++)
			{
				UObject* Obj = FindObject<UObject>(Package, *PackageInfoList(PackageIndex).QuarantinedObjectNames(ObjIdx));
				if (Obj != NULL)
				{
					UBOOL bIsReferenced = bSkipReferenceChecks ? FALSE : UObject::IsReferenced( Obj, GARBAGE_COLLECTION_KEEPFLAGS );
					UBOOL bMarkObjectForDelete = FALSE;
					if ( bIsReferenced )
					{
						warnf(NAME_Warning, TEXT("Object can not be deleted because it is referenced by other objects: %s"), *PackageInfoList(PackageIndex).QuarantinedObjectNames(ObjIdx));

						if(bShowObjReferences)
						{
							// We cannot safely delete this object. Print out a list of objects referencing this one
							// that prevent us from being able to delete it.
							FStringOutputDevice Ar;
							Obj->OutputReferencers(Ar, FALSE);
							warnf( TEXT("%s"), *Ar );
						}			
					}
					else
					{
						warnf(NAME_Log, TEXT("Marking object for deletion: %s"), *PackageInfoList(PackageIndex).QuarantinedObjectNames(ObjIdx));

						// Mark its package as dirty as we're going to delete the obj.
						Obj->MarkPackageDirty();
						// Remove standalone flag so garbage collection can delete the object.
						Obj->ClearFlags( RF_Standalone );
					}
				}
				else
				{
					warnf(NAME_Log, TEXT("Object not found in package: %s"), *PackageInfoList(PackageIndex).QuarantinedObjectNames(ObjIdx));
				}
			}

			if( Package->IsDirty() == TRUE )
			{
				bSavePackage = TRUE;
			}

			//Save the package
			if( bSavePackage == TRUE )
			{
				if( bIsReadOnly == TRUE && bSkipAutoCheckOut == TRUE )
				{
					warnf(NAME_Error, TEXT("Package is read-only but needs to be resaved. %s"),*Filename );
					if(bCanIgnoreFails)
					{
						continue;
					}
					else
					{
						return 1;
					};
				}
				else
				{
#if HAVE_SCC
					// Check to see if we need to check this package out
					if ( !bSkipAutoCheckOut )
					{
						if( FSourceControl::ForceGetStatus( Filename ) == SCC_CheckedOutOther )
						{
							warnf(NAME_Log, TEXT("Package is already checked out by someone else: %s"), *Filename);
						}
						else if( bIsReadOnly )
						{
							FSourceControl::CheckOut(Package);
						}
					}
#endif
					// Check to see if we were actually able to check out the file.
					//  If the file is still read only then we failed and need to emit an error and
					//  we decide if we need to continue or exit based on the ignore fails flag.
					if( GFileManager->IsReadOnly( *Filename ) == TRUE )
					{
						warnf( NAME_Error, TEXT("Unable to check out the Package: %s"), *Filename );
						if(bCanIgnoreFails)
						{
							continue;
						}
						else
						{
							return 1;
						}
						
					}

					// Attempt to save the package.  If this operation fails we output an error message
					//  and decide if we should continue or exit based on the ignore fails flag.
					if( SavePackageHelper(Package, Filename) )
					{
						warnf( NAME_Log, TEXT("Correctly saved package: %s."), *Filename );
					}
					else
					{
						warnf( NAME_Error, TEXT("Could not properly save package: %s"), *Filename );
						if(bCanIgnoreFails)
						{
							continue;
						}
						else
						{
							return 1;
						}
					}
				}
			}
		}
		UObject::CollectGarbage(RF_Native);
	}

	// Shutdown the game asset database
	FGameAssetDatabase::Destroy();
	return 0;
#endif
	

}


/**
 * Utility method for extracting asset name from from the GAD query data
 *
 * @param	AssetQueryData		The query string returned from the GAD
 *	
 * @return	FString				The asset name
 */
FString UDeleteQuarantinedContentCommandlet::ExtractAssetName( const FString& AssetQueryData )
{
	INT PackageDelimiterPos = AssetQueryData.InStr(TEXT("."));
	if(PackageDelimiterPos != INDEX_NONE)
	{
		return AssetQueryData.Mid(PackageDelimiterPos+1);
	}

	return AssetQueryData;
}

/**
 * Utility method for extracting the package name of an asset from the GAD query data
 *
 * @param	AssetQueryData		The query string returned from the GAD
 *	
 * @return	FString				The asset name
 */
FString UDeleteQuarantinedContentCommandlet::ExtractPackageName( const FString& AssetQueryData )
{
	INT SpaceDelimiterPos = AssetQueryData.InStr(TEXT(" "));
	INT PackageDelimiterPos = AssetQueryData.InStr(TEXT("."));
	if(SpaceDelimiterPos != INDEX_NONE && PackageDelimiterPos != INDEX_NONE)
	{
		return AssetQueryData.Mid(SpaceDelimiterPos+1, PackageDelimiterPos-(SpaceDelimiterPos+1));
	}
	else if(SpaceDelimiterPos != NULL)
	{
		return AssetQueryData.Mid(SpaceDelimiterPos+1);
	}

	return AssetQueryData;
}



IMPLEMENT_CLASS(UDeleteQuarantinedContentCommandlet)



/*-----------------------------------------------------------------------------
	UDumpEmittersCommandlet commandlet.
-----------------------------------------------------------------------------*/
#include "EngineParticleClasses.h"

const TCHAR* GetParticleModuleAcronym(UParticleModule* Module)
{
	if (!Module)
		return TEXT("");

	if (Module->IsA(UParticleModuleAcceleration::StaticClass()))				return TEXT("IA");
	if (Module->IsA(UParticleModuleAccelerationOverLifetime::StaticClass()))	return TEXT("AOL");
	if (Module->IsA(UParticleModuleAttractorLine::StaticClass()))				return TEXT("AL");
	if (Module->IsA(UParticleModuleAttractorParticle::StaticClass()))			return TEXT("AP");
	if (Module->IsA(UParticleModuleAttractorPoint::StaticClass()))				return TEXT("APT");
	if (Module->IsA(UParticleModuleBeamNoise::StaticClass()))					return TEXT("BN");
	if (Module->IsA(UParticleModuleBeamSource::StaticClass()))					return TEXT("BS");
	if (Module->IsA(UParticleModuleBeamTarget::StaticClass()))					return TEXT("BT");
	if (Module->IsA(UParticleModuleCollision::StaticClass()))					return TEXT("CLS");
	if (Module->IsA(UParticleModuleColor::StaticClass()))						return TEXT("IC");
	if (Module->IsA(UParticleModuleColorByParameter::StaticClass()))			return TEXT("CBP");
	if (Module->IsA(UParticleModuleColorOverLife::StaticClass()))				return TEXT("COL");
	if (Module->IsA(UParticleModuleColorScaleOverLife::StaticClass()))			return TEXT("CSL");
	if (Module->IsA(UParticleModuleColorScaleOverDensity::StaticClass()))		return TEXT("CSD");
	if (Module->IsA(UParticleModuleLifetime::StaticClass()))					return TEXT("LT");
	if (Module->IsA(UParticleModuleLocation::StaticClass()))					return TEXT("IL");
	if (Module->IsA(UParticleModuleLocationDirect::StaticClass()))				return TEXT("LD");
	if (Module->IsA(UParticleModuleLocationEmitter::StaticClass()))				return TEXT("LE");
	if (Module->IsA(UParticleModuleLocationPrimitiveCylinder::StaticClass()))	return TEXT("LPC");
	if (Module->IsA(UParticleModuleLocationPrimitiveSphere::StaticClass()))		return TEXT("LPS");
	if (Module->IsA(UParticleModuleMeshRotation::StaticClass()))				return TEXT("IMR");
	if (Module->IsA(UParticleModuleMeshRotationRate::StaticClass()))			return TEXT("IMRR");
	if (Module->IsA(UParticleModuleOrientationAxisLock::StaticClass()))			return TEXT("OAL");
	if (Module->IsA(UParticleModuleRotation::StaticClass()))					return TEXT("IR");
	if (Module->IsA(UParticleModuleRotationOverLifetime::StaticClass()))		return TEXT("ROL");
	if (Module->IsA(UParticleModuleRotationRate::StaticClass()))				return TEXT("IRR");
	if (Module->IsA(UParticleModuleRotationRateMultiplyLife::StaticClass()))	return TEXT("RRML");
	if (Module->IsA(UParticleModuleSize::StaticClass()))						return TEXT("IS");
	if (Module->IsA(UParticleModuleSizeMultiplyLife::StaticClass()))			return TEXT("SML");
	if (Module->IsA(UParticleModuleSizeMultiplyVelocity::StaticClass()))		return TEXT("SMV");
	if (Module->IsA(UParticleModuleSizeScale::StaticClass()))					return TEXT("SS");
	if (Module->IsA(UParticleModuleSizeScaleOverDensity::StaticClass()))		return TEXT("SOD");
	if (Module->IsA(UParticleModuleStoreSpawnTime::StaticClass()))				return TEXT("SST");
	if (Module->IsA(UParticleModuleSubUV::StaticClass()))						return TEXT("SUV");
	if (Module->IsA(UParticleModuleSubUVDirect::StaticClass()))					return TEXT("SUVD");
	if (Module->IsA(UParticleModuleSubUVSelect::StaticClass()))					return TEXT("SUVS");
	if (Module->IsA(UParticleModuleTrailSource::StaticClass()))					return TEXT("TS");
	if (Module->IsA(UParticleModuleTrailSpawn::StaticClass()))					return TEXT("TSP");
	if (Module->IsA(UParticleModuleTrailTaper::StaticClass()))					return TEXT("TT");
	if (Module->IsA(UParticleModuleTypeDataBeam::StaticClass()))				return TEXT("TDB");
	if (Module->IsA(UParticleModuleTypeDataBeam2::StaticClass()))				return TEXT("TDB2");
	if (Module->IsA(UParticleModuleTypeDataMesh::StaticClass()))				return TEXT("TDM");
	if (Module->IsA(UParticleModuleTypeDataTrail::StaticClass()))				return TEXT("TDT");
	if (Module->IsA(UParticleModuleVelocity::StaticClass()))					return TEXT("IV");
	if (Module->IsA(UParticleModuleVelocityInheritParent::StaticClass()))		return TEXT("VIP");
	if (Module->IsA(UParticleModuleVelocityOverLifetime::StaticClass()))		return TEXT("VOL");

	return TEXT("???");
}

INT UDumpEmittersCommandlet::Main(const FString& Params)
{
	const TCHAR* Parms = *Params;

	FString						PackageWildcard;
	TArray<UParticleSystem*>	ParticleSystems;
	TArray<UPackage*>			ParticlePackages;
	TArray<FString>				UberModuleList;

	const TCHAR*	CmdLine		= appCmdLine();
	UBOOL			bAllRefs	= FALSE;

	if (ParseParam(CmdLine, TEXT("ALLREFS")))
	{
		bAllRefs	= TRUE;
	}

	while (ParseToken(Parms, PackageWildcard, 0))
	{
		TArray<FString> FilesInPath;

		if (PackageWildcard.Left(1) == TEXT("-"))
		{
			// this is a command-line parameter - skip it
			continue;
		}

		GFileManager->FindFiles(FilesInPath, *PackageWildcard, 1, 0);
		if( FilesInPath.Num() == 0 )
		{
			// if no files were found, it might be an unqualified path; try prepending the .u output path
			// if one were going to make it so that you could use unqualified paths for package types other
			// than ".u", here is where you would do it
			GFileManager->FindFiles(FilesInPath, *(appScriptOutputDir() * PackageWildcard), 1, 0);
		}

		if (FilesInPath.Num() == 0)
		{
			warnf(TEXT("No packages found using '%s'!"), *PackageWildcard);
			continue;
		}

		for (INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++)
		{
			const FString &Filename = FilesInPath(FileIndex);

			BeginLoad();
			// Assert if package couldn't be opened so we have no chance of messing up saving later packages.
			UPackage* Package = CastChecked<UPackage>(UObject::LoadPackage(NULL, *Filename, 0));
			check(Package);
			EndLoad();

			// Check for particle systems
			if (Package)
			{
				debugf(TEXT("Examining Package %s"), *Package->GetName());

				// Dump the particle systems
				for (TObjectIterator<UParticleSystem> It; It; ++It)
				{
					UParticleSystem* PartSys = Cast<UParticleSystem>(*It);
					check(PartSys->GetOuter());

					// Determine the package it is in...
					UObject*	Outer			= PartSys->GetOuter();
					UPackage*	ParticlePackage = Cast<UPackage>(Outer);
					while (ParticlePackage == NULL)
					{
						Outer			= Outer->GetOuter();
						ParticlePackage = Cast<UPackage>(Outer);
						if (Outer == NULL)
						{
							warnf(TEXT("No package??? %s"), *PartSys->GetName());
							break;
						}
					}

					if (bAllRefs)
					{
						// Find the package the particle is in...
						ParticleSystems.AddUniqueItem(PartSys);
						ParticlePackages.AddUniqueItem(ParticlePackage);
					}
					else
					{
						if (It->IsIn(Package))
						{
							ParticleSystems.AddUniqueItem(PartSys);
							ParticlePackages.AddUniqueItem(Package);
						}
					}
				}
			}

			// Now, dump out the particle systems and the package list.
			debugf(TEXT("**********************************************************************************************\n"));
			debugf(TEXT("                                     PARTICLE SYSTEM LIST                                     \n"));
			debugf(TEXT("**********************************************************************************************\n"));
			debugf(TEXT("ParticleSystem Count %d"), ParticleSystems.Num());
			for (INT PSysIndex = 0; PSysIndex < ParticleSystems.Num(); PSysIndex++)
			{
				UParticleSystem* PSys = ParticleSystems(PSysIndex);
				if (PSys)
				{
					debugf(TEXT("\tParticleSystem %s"), *PSys->GetFullName());

					for (INT EmitterIndex = 0; EmitterIndex < PSys->Emitters.Num(); EmitterIndex++)
					{
						UParticleEmitter* Emitter = PSys->Emitters(EmitterIndex);
						if (Emitter == NULL)
							continue;

						debugf(TEXT("\t\tParticleEmitter %s"), *Emitter->GetName());

						if (Emitter->LODLevels.Num() > 0)
						{
							UParticleLODLevel* LODLevel = Emitter->LODLevels(0);
							if (LODLevel)
							{
								FString	AcronymCollection;

								UParticleModule* Module;
								for (INT ModuleIndex = 0; ModuleIndex < LODLevel->Modules.Num(); ModuleIndex++)
								{
									Module = LODLevel->Modules(ModuleIndex);
									if (Module)
									{
										debugf(TEXT("\t\t\tModule %2d - %48s - %s"), ModuleIndex, *Module->GetName(), GetParticleModuleAcronym(Module));
										AcronymCollection += GetParticleModuleAcronym(Module);
									}
								}

								UberModuleList.AddUniqueItem(AcronymCollection);
							}
						}
					}
				}
			}

			debugf(TEXT("**********************************************************************************************\n"));
			debugf(TEXT("                                         PACKAGE LIST                                         \n"));
			debugf(TEXT("**********************************************************************************************\n"));
			debugf(TEXT("Package Count %d"), ParticlePackages.Num());
			for (INT PackageIndex = 0; PackageIndex < ParticlePackages.Num(); PackageIndex++)
			{
				UPackage* OutPackage	= ParticlePackages(PackageIndex);
				debugf(TEXT("\t%s"), *OutPackage->GetName());
			}

			UObject::CollectGarbage(RF_Native);

			debugf(TEXT("**********************************************************************************************\n"));
			debugf(TEXT("                                       UBER-MODULE LIST                                       \n"));
			debugf(TEXT("**********************************************************************************************\n"));
			debugf(TEXT("UberModule Count %d"), UberModuleList.Num());
			for (INT UberIndex = 0; UberIndex < UberModuleList.Num(); UberIndex++)
			{
				debugf(TEXT("\t%3d - %s"), UberIndex, *UberModuleList(UberIndex));
			}
		}
	}

	return 0;
}

IMPLEMENT_CLASS(UDumpEmittersCommandlet)


/*-----------------------------------------------------------------------------
UMergePackagesCommandlet
-----------------------------------------------------------------------------*/

/**
 * This function will return all objects that were created after a given object index.
 * The idea is that you load some stuff, remember how many objects are in the engine, 
 * and then load a map where you want to track what was loaded. Any objects you load 
 * after that point will come after the original number of objects.
 *
 * @param NewObjects	The array that receives the new objects loaded beginning with index Start
 * @param Start			The first index of object to copy into NewObjects
 */
void GetAllObjectsAfter(TArray<UObject*>& NewObjects, DWORD Start = 0)
{
	// i wish there was a better way
	DWORD NumObjects = 0;
	for (TObjectIterator<UObject> It; It; ++It)
	{
		NumObjects++;
	}

	DWORD ObjIndex = 0;
	for (TObjectIterator<UObject> It; It; ObjIndex++, ++It)
	{
		UObject* Obj = *It;
		if (ObjIndex >= Start)
		{
			NewObjects.AddItem(Obj);
		}
	}
}

INT UMergePackagesCommandlet::Main( const FString& Params )
{
	const TCHAR* Parms = *Params;

	FString	PackageWildcard;

	FString SrcPackageName, DestPackageName;
	ParseToken(Parms,SrcPackageName,0);
	ParseToken(Parms,DestPackageName,1);

	
	// Only moving objects from source package, so find offset of current objects
	INT FirstObjectToMove = 0;
	for (TObjectIterator<UObject> It; It; ++It)
	{
		FirstObjectToMove++;
	}

	UPackage* SrcPackage  = Cast<UPackage>(UObject::LoadPackage( NULL, *SrcPackageName, 0 ));

	// the objects to be moved are the ones in the object array
	TArray<UObject*> DiffObjects;
	GetAllObjectsAfter(DiffObjects, FirstObjectToMove);

	UPackage* DestPackage = Cast<UPackage>(UObject::LoadPackage( NULL, *DestPackageName, 0 ));

	// go through all the new objects and rename all packages that were loaded to their new package name
	for (INT ObjIndex = 0; ObjIndex < DiffObjects.Num(); ObjIndex++)
	{
		UPackage* DiffPackage = Cast<UPackage>(DiffObjects(ObjIndex));

		if (DiffPackage && DiffPackage != UObject::GetTransientPackage() && DiffPackage != DestPackage)
		{
			BeginLoad();
			ULinkerLoad* Linker = GetPackageLinker(DiffPackage, NULL, LOAD_NoWarn, NULL, NULL);
			EndLoad();
			if (Linker && !Linker->ContainsCode() && !Linker->ContainsMap())
			{
				warnf(TEXT("Moving object %s to %s"), *DiffPackage->GetFullName(),*DestPackageName);
				// Only move if won't conflict
				UBOOL Exists = StaticFindObject(DiffPackage->GetClass(), DestPackage, *DiffPackage->GetName(), TRUE)!=0;
				if(Exists)
				{
					warnf(TEXT("Object already exists in package, skipped"));
				}
				else
				{
					DiffPackage->Rename(*DiffPackage->GetName(), DestPackage, REN_ForceNoResetLoaders);
//					warnf(TEXT("Object added to package"));
				}
			}
			else
			{
//				warnf(TEXT("%s didn't match"), *DiffPackage->GetFullName());
			}
		}
	}

	SavePackageHelper(DestPackage, DestPackageName);

	UObject::CollectGarbage(RF_Native);
	return 0;
}
IMPLEMENT_CLASS(UMergePackagesCommandlet)

/*-----------------------------------------------------------------------------
UMergeConflictingPackagesCommandlet
-----------------------------------------------------------------------------*/

INT UMergeConflictingPackagesCommandlet::Main( const FString& Params )
{
	UBOOL bResult = FALSE;
	const TCHAR* Parms = *Params;

	// disable instancing of new components and subobjects so that we don't get lots of false positives
	// GUglyHackFlags |= (HACK_DisableComponentCreation|HACK_DisableSubobjectInstancing);

	// parse the command line into tokens and switches
	TArray<FString> Tokens, Switches;
	ParseCommandLine(Parms, Tokens, Switches);

	if( Tokens.Num() != 2 )
	{
		warnf( NAME_Error, TEXT( "Incorrect parameters passed. Format should be:-\n\tMergeConflictingPackages <SrcFile1> <SrcFile2> <DstFile>\n... making sure to use full paths to each package" ) );
		return 1;
	}

	// Load src package
	FFilename SrcFilename = Tokens(0);
	SET_WARN_COLOR(COLOR_BLUE);
	warnf( TEXT("Loading %s"), *SrcFilename );
	CLEAR_WARN_COLOR();

	UPackage* OuterPackage = CreatePackage( NULL, TEXT("_MERGEOUTER_") );
	UPackage* SrcPackage = LoadPackage( OuterPackage, *SrcFilename, LOAD_None );
	if (NULL == SrcPackage)
	{
		SET_WARN_COLOR(COLOR_RED);
		warnf(TEXT("Failed to load package %s"), *SrcFilename );
		CLEAR_WARN_COLOR();
		return 1;
	}

	// Load dest package
	FFilename DestFilename = Tokens(1);
	SET_WARN_COLOR(COLOR_BLUE);
	warnf( TEXT("Loading %s"), *DestFilename );
	CLEAR_WARN_COLOR();

	UPackage* DestPackage = LoadPackage(NULL, *DestFilename, LOAD_None);	
	if (NULL == DestPackage)
	{
		SET_WARN_COLOR(COLOR_RED);
		warnf(TEXT("Failed to load package %s"), *DestFilename );
		CLEAR_WARN_COLOR();
		return 1;
	}

	// Merge from src to dest
	SET_WARN_COLOR(COLOR_YELLOW);
	TMap<UObject*, UObject*> MatchedObjects;
	MergeConflicting(SrcPackage, DestPackage, MatchedObjects);
	CLEAR_WARN_COLOR();

	// re-point refs in DestPackage to keys in MatchedObjects to the corresponding value
	// fixes refs to objects in SrcPackage to the equivalent in DestPackage
	for(TObjectIterator<UObject> It; It; ++It)
	{
		if (DestPackage == It->GetOutermost())
		{
			FArchiveReplaceObjectRef<UObject> RefFixer(*It, MatchedObjects, FALSE, FALSE, FALSE);
		}
	}

	// Save dest package
	debugf(TEXT("Saving %s"), *DestFilename );
	if (!SavePackageHelper(DestPackage, DestFilename))
	{
		SET_WARN_COLOR(COLOR_RED);
		warnf(TEXT("Failed to save to output package %s"), *DestFilename );
		CLEAR_WARN_COLOR();
		return 1;
	}
	else
	{
		SET_WARN_COLOR(COLOR_GREEN);
		warnf(TEXT("Successfully saved %s"), *DestFilename );
		CLEAR_WARN_COLOR();
	}

	// GC and exit
	UObject::CollectGarbage(RF_Native);
	return 0;
}

void UMergeConflictingPackagesCommandlet::MergeConflicting(UObject* From, UObject* To, TMap<UObject*, UObject*>& Matches)
{
	debugf(TEXT("Merging children of..."));
	debugf(TEXT("%s"), *From->GetFullName() );
	debugf(TEXT("into"));
	debugf(TEXT("%s"), *To->GetFullName() );

	// Build list of child objects of src and dest
	TArray<UObject*> SrcObjects;
	TArray<UObject*> DestObjects;
	for(TObjectIterator<UObject> It; It; ++It)
	{
		if (From == It->GetOuter())
		{
			SrcObjects.AddItem(*It);
		}
		else if (To == It->GetOuter())
		{
			DestObjects.AddItem(*It);
		}
	}

	// For any object in SrcObjects that are not in DestObjects, rename into the matching outer in Dest
	// For any material in SrcObjects that is also in DestObjects, copy Src settings to the object in DestObjects
	for (INT SrcIndex = 0; SrcIndex < SrcObjects.Num(); SrcIndex++)
	{
		UObject* SrcObject = SrcObjects(SrcIndex);
		FString SrcName = SrcObject->GetName();
		UClass* SrcClass = SrcObject->GetClass();

		UBOOL bFound = FALSE;
		for (INT DestIndex = 0; DestIndex < DestObjects.Num(); DestIndex++)
		{
			UObject* DestObject = DestObjects(DestIndex);
			FString DestName = DestObject->GetName();
			UClass* DestClass = DestObject->GetClass();

			// Are the src and dest objects the same?
			if (SrcName == DestName)
			{
				if (SrcClass != DestClass)
				{
					warnf(TEXT("Objects of same name but different class..."));
					warnf(TEXT("Src Object: %s"), *SrcObject->GetFullName());
					warnf(TEXT("Dest Object: %s"), *DestObject->GetFullName());
				}
				else
				{
					Matches.Set(SrcObject, DestObject);

					if (SrcObject->IsA(UMaterial::StaticClass()))
					{
						// Objects are materials - copy settings...
						UMaterial* SrcMaterial = CastChecked<UMaterial>(SrcObject);
						UMaterial* DestMaterial = CastChecked<UMaterial>(DestObject);

						// copy Src settings to the object in DestObjects
						//@TODO
						// the below isn't very future-proof - it could be good if there was a way to copy settings based on the metadata ("Mobile" in this case)...
						debugf(TEXT("Copying material settings..."));
						debugf(TEXT("From: %s"), *SrcMaterial->GetFullName());
						debugf(TEXT("To: %s"), *DestMaterial->GetFullName());

						DestMaterial->bAutoFlattenMobile = SrcMaterial->bAutoFlattenMobile;
						//					DestMaterial->MobileBaseTexture = NULL;//SrcMaterial->MobileBaseTexture;
						DestMaterial->MobileBaseTextureTexCoordsSource = SrcMaterial->MobileBaseTextureTexCoordsSource;
						//					DestMaterial->MobileNormalTexture = SrcMaterial->MobileNormalTexture;
						DestMaterial->MobileAmbientOcclusionSource = SrcMaterial->MobileAmbientOcclusionSource;
						DestMaterial->bMobileAllowFog = SrcMaterial->bMobileAllowFog;
						DestMaterial->bUseMobileSpecular = SrcMaterial->bUseMobileSpecular;
						DestMaterial->bUseMobilePixelSpecular = SrcMaterial->bUseMobilePixelSpecular;
						DestMaterial->MobileSpecularColor = SrcMaterial->MobileSpecularColor;
						DestMaterial->MobileSpecularPower = SrcMaterial->MobileSpecularPower;
						DestMaterial->MobileSpecularMask = SrcMaterial->MobileSpecularMask;
						//					DestMaterial->MobileEmissiveTexture = SrcMaterial->MobileEmissiveTexture;
						DestMaterial->MobileEmissiveColorSource = SrcMaterial->MobileEmissiveColorSource;
						DestMaterial->MobileEmissiveColor = SrcMaterial->MobileEmissiveColor;
						DestMaterial->MobileEmissiveMaskSource = SrcMaterial->MobileEmissiveMaskSource;
						//					DestMaterial->MobileEnvironmentTexture = SrcMaterial->MobileEnvironmentTexture;
						DestMaterial->MobileEnvironmentMaskSource = SrcMaterial->MobileEnvironmentMaskSource;
						DestMaterial->MobileEnvironmentAmount = SrcMaterial->MobileEnvironmentAmount;
						DestMaterial->MobileEnvironmentBlendMode = SrcMaterial->MobileEnvironmentBlendMode;
						DestMaterial->MobileEnvironmentColor = SrcMaterial->MobileEnvironmentColor;
						DestMaterial->MobileEnvironmentFresnelAmount = SrcMaterial->MobileEnvironmentFresnelAmount;
						DestMaterial->MobileEnvironmentFresnelExponent = SrcMaterial->MobileEnvironmentFresnelExponent;
						DestMaterial->MobileRimLightingStrength = SrcMaterial->MobileRimLightingStrength;
						DestMaterial->MobileRimLightingExponent = SrcMaterial->MobileRimLightingExponent;
						DestMaterial->MobileRimLightingMaskSource = SrcMaterial->MobileRimLightingMaskSource;
						DestMaterial->MobileRimLightingColor = SrcMaterial->MobileRimLightingColor;
						DestMaterial->bUseMobileBumpOffset = SrcMaterial->bUseMobileBumpOffset;
						DestMaterial->MobileBumpOffsetReferencePlane = SrcMaterial->MobileBumpOffsetReferencePlane;
						DestMaterial->MobileBumpOffsetHeightRatio = SrcMaterial->MobileBumpOffsetHeightRatio;
						//					DestMaterial->MobileMaskTexture = SrcMaterial->MobileMaskTexture;
						DestMaterial->MobileMaskTextureTexCoordsSource = SrcMaterial->MobileMaskTextureTexCoordsSource;
						DestMaterial->MobileDetailTexture = SrcMaterial->MobileDetailTexture;
						DestMaterial->MobileDetailTexture2 = SrcMaterial->MobileDetailTexture2;
						DestMaterial->MobileDetailTexture3 = SrcMaterial->MobileDetailTexture3;
						DestMaterial->MobileDetailTextureTexCoordsSource = SrcMaterial->MobileDetailTextureTexCoordsSource;
						DestMaterial->MobileOpacityMultiplier = SrcMaterial->MobileOpacityMultiplier;
						DestMaterial->MobileTextureBlendFactorSource = SrcMaterial->MobileTextureBlendFactorSource;
						DestMaterial->bLockColorBlending = SrcMaterial->bLockColorBlending;
						DestMaterial->bUseMobileUniformColorMultiply = SrcMaterial->bUseMobileUniformColorMultiply;
						DestMaterial->MobileDefaultUniformColor = SrcMaterial->MobileDefaultUniformColor;
						DestMaterial->bUseMobileVertexColorMultiply = SrcMaterial->bUseMobileVertexColorMultiply;
						DestMaterial->bBaseTextureTransformed = SrcMaterial->bBaseTextureTransformed;
						DestMaterial->bEmissiveTextureTransformed = SrcMaterial->bEmissiveTextureTransformed;
						DestMaterial->bNormalTextureTransformed = SrcMaterial->bNormalTextureTransformed;
						DestMaterial->bMaskTextureTransformed = SrcMaterial->bMaskTextureTransformed;
						DestMaterial->bDetailTextureTransformed = SrcMaterial->bDetailTextureTransformed;
						DestMaterial->MobileTransformCenterX = SrcMaterial->MobileTransformCenterX;
						DestMaterial->MobileTransformCenterY = SrcMaterial->MobileTransformCenterY;
						DestMaterial->MobilePannerSpeedX = SrcMaterial->MobilePannerSpeedX;
						DestMaterial->MobilePannerSpeedY = SrcMaterial->MobilePannerSpeedY;
						DestMaterial->MobileRotateSpeed = SrcMaterial->MobileRotateSpeed;
						DestMaterial->MobileFixedScaleX = SrcMaterial->MobileFixedScaleX;
						DestMaterial->MobileFixedScaleY = SrcMaterial->MobileFixedScaleY;
						DestMaterial->MobileSineScaleX = SrcMaterial->MobileSineScaleX;
						DestMaterial->MobileSineScaleY = SrcMaterial->MobileSineScaleY;
						DestMaterial->MobileSineScaleFrequencyMultipler = SrcMaterial->MobileSineScaleFrequencyMultipler;
						DestMaterial->MobileFixedOffsetX = SrcMaterial->MobileFixedOffsetX;
						DestMaterial->MobileFixedOffsetY = SrcMaterial->MobileFixedOffsetY;
						DestMaterial->bUseMobileWaveVertexMovement = SrcMaterial->bUseMobileWaveVertexMovement;
						DestMaterial->MobileTangentVertexFrequencyMultiplier = SrcMaterial->MobileTangentVertexFrequencyMultiplier;
						DestMaterial->MobileVerticalFrequencyMultiplier = SrcMaterial->MobileVerticalFrequencyMultiplier;
						DestMaterial->MobileMaxVertexMovementAmplitude = SrcMaterial->MobileMaxVertexMovementAmplitude;
						DestMaterial->MobileSwayFrequencyMultiplier = SrcMaterial->MobileSwayFrequencyMultiplier;
						DestMaterial->MobileSwayMaxAngle = SrcMaterial->MobileSwayMaxAngle;
					}

					// Recurse into objects and merge their children.
					MergeConflicting(SrcObject, DestObject, Matches);
				}
				bFound = TRUE;
				break;

			} // if (SrcName == DestName)

		} // for (INT DestIndex = 0; DestIndex < DestObjects.Num(); DestIndex++)

		// For any object in SrcObjects that are not in DestObjects.
		// Only actually move objects that are direct children of packages.
		if (!bFound && From->IsA(UPackage::StaticClass()))
		{
			if (!StaticFindObject(SrcClass, To, *SrcName, TRUE))
			{
				// rename into the matching outer in Dest
				SrcObject->Rename(*SrcName, To, REN_ForceNoResetLoaders);
				debugf(TEXT("Renaming %s to outer in %s"), *SrcObject->GetFullName(), *To->GetFullName());
			}
			else
			{
				warnf(TEXT("Name comparison of %s didn't find match - but StaticFindObject did!?"), *SrcObject->GetFullName());
			}
		}

	} // for (INT SrcIndex = 0; SrcIndex < SrcObjects.Num(); SrcIndex++)	
}

IMPLEMENT_CLASS(UMergeConflictingPackagesCommandlet)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// UExamineOutersCommandlet
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

INT UExamineOutersCommandlet::Main(const FString& Params)
{
	// Parse command line args.
	TArray<FString> Tokens;
	TArray<FString> Switches;

	const TCHAR* Parms = *Params;
	ParseCommandLine(Parms, Tokens, Switches);

	// Build package file list.
	const TArray<FString> FilesInPath( GPackageFileCache->GetPackageFileList() );
	if( FilesInPath.Num() == 0 )
	{
		warnf( NAME_Warning, TEXT("No packages found") );
		return 1;
	}

	const FString DefaultObjectPrefix( DEFAULT_OBJECT_PREFIX );

	warnf( NAME_Log, TEXT("%s%-7s%-7s%-7s%-7s%-7s%-7s%-7s%-7s%-7s"),
		*FString("Package").RightPad(72),TEXT("Match"), TEXT("Level"), TEXT("Pkg"), TEXT("Class"), TEXT("NULL"), TEXT("DefObj"), TEXT("PSReq"), TEXT("Other"), TEXT("Total") );

	// Iterate over all files doing stuff.
	for( INT FileIndex = 0 ; FileIndex < FilesInPath.Num() ; ++FileIndex )
	{
		const FFilename& Filename = FilesInPath(FileIndex);

		UObject* Package = UObject::LoadPackage( NULL, *Filename, LOAD_None );
		if( Package == NULL )
		{
			warnf( NAME_Error, TEXT("Error loading %s!"), *Filename );
		}

		// Iterate over all the editinline properties of an object and gather outer stats.
		INT NumMatchedOuters = 0;
		INT NumLevelOuters = 0;
		INT NumPackageOuters = 0;
		INT NumClassOuters = 0;
		INT NumNullOuters = 0;
		INT NumDefaultObjectOuters = 0;
		INT NumPSRequiredModule = 0;
		INT NumOther = 0;
		INT NumTotal = 0;

		for ( TObjectIterator<UObject> It ; It ; ++It )
		{
			UObject* Object = *It;
			UClass* ObjectClass = Object->GetClass();

			for ( TFieldIterator<UProperty> PropIt( ObjectClass ) ; PropIt ; ++PropIt )
			{
				UObjectProperty* Property = Cast<UObjectProperty>( *PropIt );

				// Select out editinline object properties.
				if ( (PropIt->PropertyFlags & (CPF_EditInline | CPF_EditInlineUse)) && Property )
				{
					// Get the property value.
					UObject* EditInlineObject;
					Property->CopySingleValue( &EditInlineObject, ((BYTE*)Object) + Property->Offset );

					// If the property value was non-NULL . . .
					if ( EditInlineObject )
					{
						// Check its outer.
						UObject* EditInlineObjectOuter = EditInlineObject->GetOuter();

						if ( EditInlineObjectOuter == NULL )
						{
							++NumNullOuters;
						}
						else if ( EditInlineObjectOuter == Object )
						{
							++NumMatchedOuters;
						}
						else if ( EditInlineObjectOuter->IsA( ULevel::StaticClass() ) )
						{
							++NumLevelOuters;
						}
						else if ( EditInlineObjectOuter->IsA( UPackage::StaticClass() ) )
						{
							++NumPackageOuters;
						}
						else if ( EditInlineObjectOuter->IsA( UClass::StaticClass() ) )
						{
							++NumClassOuters;
						}
						else if ( FString( *EditInlineObjectOuter->GetName() ).StartsWith( DefaultObjectPrefix ) )
						{
							++NumDefaultObjectOuters;
						}
						else if ( appStricmp( *Property->GetName(), TEXT("RequiredModule") ) == 0 )
						{
							++NumPSRequiredModule;
						}
						else
						{
							++NumOther;
						}
						++NumTotal;
					}
				}
			} // property itor
		} // object itor

		warnf( NAME_Log, TEXT("%s%-7i%-7i%-7i%-7i%-7i%-7i%-7i%-7i%-7i"),
			*Filename.RightPad( 72 ), NumMatchedOuters, NumLevelOuters, NumPackageOuters, NumClassOuters, NumNullOuters, NumDefaultObjectOuters, NumPSRequiredModule, NumOther, NumTotal );

		// Clean up.
		UObject::CollectGarbage( RF_Native );
	}

	return 0;
}
IMPLEMENT_CLASS(UExamineOutersCommandlet);


/*-----------------------------------------------------------------------------
	UTestCompressionCommandlet commandlet.
-----------------------------------------------------------------------------*/

/**
 * Run a compression/decompress test with the given package and compression options
 *
 * @param PackageName		The package to compress/decompress
 * @param Flags				The options for compression
 * @param UncompressedSize	The size of the uncompressed package file
 * @param CompressedSize	The options for compressed package file
 */
void UTestCompressionCommandlet::RunTest(const FFilename& PackageName, ECompressionFlags Flags, DWORD& UncompressedSize, DWORD& CompressedSize)
{
	SET_WARN_COLOR(COLOR_WHITE);
	warnf(TEXT(""));
	warnf(TEXT("Running Compression Test..."));

	SET_WARN_COLOR(COLOR_YELLOW);
	warnf(TEXT("Package:"));
	SET_WARN_COLOR(COLOR_GRAY);
	warnf(TEXT("  %s"), *PackageName.GetCleanFilename());

	SET_WARN_COLOR(COLOR_YELLOW);
	warnf(TEXT("Options:"));
	SET_WARN_COLOR(COLOR_GRAY);
	if (Flags & COMPRESS_LZO)
	{
		warnf(TEXT("  LZO"));
	}
	if (Flags & COMPRESS_LZX)
	{
		warnf(TEXT("  LZX"));
	}
	if (Flags & COMPRESS_ZLIB)
	{
		warnf(TEXT("  ZLIB"));
	}
	if (Flags & COMPRESS_BiasMemory)
	{
		warnf(TEXT("  Compressed for memory"));
	}
	if (Flags & COMPRESS_BiasSpeed)
	{
		warnf(TEXT("  Compressed for decompression speed"));
	}
#if _DEBUG
	warnf(TEXT("  Debug build"));
#elif FINAL_RELEASE
	warnf(TEXT("  Final release build"));
#else
	warnf(TEXT("  Release build"));
#endif

	// Generate new filename by swapping out extension.
	FFilename DstPackageFilename = FString::Printf(TEXT("%s%s%s.compressed_%x"), 
		*PackageName.GetPath(),
		PATH_SEPARATOR,
		*PackageName.GetBaseFilename(),
		(DWORD)Flags);

	// Create file reader and writers.
	FArchive* Reader = GFileManager->CreateFileReader( *PackageName );
	FArchive* Writer = GFileManager->CreateFileWriter( *DstPackageFilename );
	check( Reader );
	check( Writer );

	// Figure out filesize.
	INT ReaderTotalSize	= Reader->TotalSize();
	// Create buffers for serialization (Src) and comparison (Dst).
	BYTE* SrcBuffer			= (BYTE*) appMalloc( ReaderTotalSize );
	BYTE* DstBuffer			= (BYTE*) appMalloc( ReaderTotalSize );
	BYTE* DstBufferAsync	= (BYTE*) appMalloc( ReaderTotalSize );

	DOUBLE StartRead = appSeconds();
	Reader->Serialize( SrcBuffer, ReaderTotalSize );
	DOUBLE StopRead = appSeconds();

	DOUBLE StartCompress = appSeconds();
	Writer->SerializeCompressed( SrcBuffer, ReaderTotalSize, Flags );
	DOUBLE StopCompress = appSeconds();

	// Delete (and implicitly flush) reader and writers.
	delete Reader;
	delete Writer;

	// Figure out compressed size (and propagate uncompressed for completeness)
	CompressedSize		= GFileManager->FileSize( *DstPackageFilename );
	UncompressedSize	= ReaderTotalSize;

	// Load compressed via FArchive
	Reader = GFileManager->CreateFileReader( *DstPackageFilename );

	Reader->SerializeCompressed( DstBuffer, 0, Flags );
	delete Reader;
	
	// Load compressed via async IO system.
	FIOSystem* IO = GIOManager->GetIOSystem( IOSYSTEM_GenericAsync );
	check(GIOManager);
	FThreadSafeCounter Counter(1);
	IO->LoadCompressedData( DstPackageFilename, 0, CompressedSize, UncompressedSize, DstBufferAsync, Flags, &Counter, AIOP_Normal );
	// Wait till it completed.
	while( Counter.GetValue() > 0 )
	{
		appSleep(0.05f);
	}
	
	// Compare results and assert if they are different.
	for( DWORD Count=0; Count<UncompressedSize; Count++ )
	{
		BYTE Src		= SrcBuffer[Count];
		BYTE Dst		= DstBuffer[Count];
		BYTE DstAsync	= DstBufferAsync[Count];
		check( Src == Dst );
		check( Src == DstAsync );
	}

	appFree( SrcBuffer );
	appFree( DstBuffer );
	appFree( DstBufferAsync );


	SET_WARN_COLOR(COLOR_WHITE);
	warnf(TEXT(""));
	warnf(TEXT("Complete!"));

	SET_WARN_COLOR(COLOR_YELLOW);
	warnf(TEXT("Size information:"));
	SET_WARN_COLOR(COLOR_GRAY);
	warnf(TEXT("  Uncompressed Size:     %.3fMB"), (FLOAT)UncompressedSize / (1024.0f * 1024.0f));
	warnf(TEXT("  Compressed Size:       %.3fMB"), (FLOAT)CompressedSize / (1024.0f * 1024.0f));
	warnf(TEXT("  Ratio:                 %.3f%%"), ((FLOAT)CompressedSize / (FLOAT)UncompressedSize) * 100.0f);
	SET_WARN_COLOR(COLOR_YELLOW);
	warnf(TEXT("Time information:"));
	SET_WARN_COLOR(COLOR_GRAY);
	warnf(TEXT("  Read time:             %.3fs"), StopRead - StartRead);
	warnf(TEXT("  Compress time:         %.3fs"), StopCompress - StartCompress);
}

INT UTestCompressionCommandlet::Main(const FString& Params)
{
	// Parse command line args.
	TArray<FString> Tokens;
	TArray<FString> Switches;
	ParseCommandLine( *Params, Tokens, Switches);

	// a list of all the different tests to run (as defined by the flags)
	TArray<DWORD> CompressionTests;
	UBOOL bAllTests = ParseParam(*Params, TEXT("alltests"));

	if (bAllTests || ParseParam(*Params, TEXT("zlibtest")))
	{
		CompressionTests.AddItem(COMPRESS_ZLIB);
	}
	if (bAllTests || ParseParam(*Params, TEXT("lzxtest")))
	{
		CompressionTests.AddItem(COMPRESS_LZX);
	}
	if (bAllTests || ParseParam(*Params, TEXT("lzotest")))
	{
		CompressionTests.AddItem(COMPRESS_LZO);
	}
	if (bAllTests || ParseParam(*Params, TEXT("lzospeedtest")))
	{
		CompressionTests.AddItem(COMPRESS_LZO | COMPRESS_BiasSpeed);
	}
	if (bAllTests || ParseParam(*Params, TEXT("lzosizetest")))
	{
		CompressionTests.AddItem(COMPRESS_LZO | COMPRESS_BiasMemory);
	}

	// list of all files to compress
	TArray<FString> FilesToCompress;

	if (ParseParam(*Params, TEXT("allpackages")))
	{
		FilesToCompress = GPackageFileCache->GetPackageFileList();
	}
	else
	{
		// iterate over all passed in tokens looking for packages
		for (INT TokenIndex=0; TokenIndex<Tokens.Num(); TokenIndex++ )
		{
			FFilename PackageFilename;
			// See whether filename was found in cache.		
			if (GPackageFileCache->FindPackageFile(*Tokens(TokenIndex), NULL, PackageFilename))
			{
				FilesToCompress.AddItem(*PackageFilename);
			}
		}
	}

	// keep overall stats
	QWORD TotalUncompressedSize = 0;
	QWORD TotalCompressedSize = 0;

	// Iterate over all files doing stuff.
	for (INT FileIndex = 0; FileIndex < FilesToCompress.Num(); FileIndex++)
	{
		for (INT TestIndex = 0; TestIndex < CompressionTests.Num(); TestIndex++)
		{
			DWORD UncompressedSize = 0;
			DWORD CompressedSize = 0;

			// run compression test
			RunTest(FilesToCompress(FileIndex), (ECompressionFlags)CompressionTests(TestIndex), UncompressedSize, CompressedSize);

			// update stats
			TotalUncompressedSize += UncompressedSize;
			TotalCompressedSize += CompressedSize;
		}
	}

	SET_WARN_COLOR(COLOR_YELLOW);
	warnf(TEXT(""));
	warnf(TEXT(""));
	warnf(TEXT("OVERALL STATS"));
	CLEAR_WARN_COLOR();
	warnf(TEXT("  Total Uncompressed:    %.3fMB"), (FLOAT)TotalUncompressedSize / (1024.0f * 1024.0f));
	warnf(TEXT("  Total Compressed:      %.3fMB"), (FLOAT)TotalCompressedSize / (1024.0f * 1024.0f));
	warnf(TEXT("  Total Ratio:           %.3f%%"), ((FLOAT)TotalCompressedSize / (FLOAT)TotalUncompressedSize) * 100.0f);

	return 0;
}
IMPLEMENT_CLASS(UTestCompressionCommandlet);


/*-----------------------------------------------------------------------------
	UFindDuplicateKismetObjectsCommandlet.
-----------------------------------------------------------------------------*/
INT UFindDuplicateKismetObjectsCommandlet::Main( const FString& Params )
{
	const TCHAR* Parms = *Params;

	// skip the assert when a package can not be opened
	UBOOL bDeleteDuplicates		= ParseParam(Parms,TEXT("clean"));
	UBOOL bMapsOnly				= ParseParam(Parms,TEXT("mapsonly"));
	UBOOL bNonMapsOnly			= ParseParam(Parms,TEXT("nonmapsonly"));

	// Retrieve list of all packages in .ini paths.
	TArray<FString> PackageList = GPackageFileCache->GetPackageFileList();
	if( !PackageList.Num() )
		return 0;

	INT GCIndex = 0;

	// Iterate over all packages.
	for( INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++ )
	{
		const FFilename& Filename = PackageList(PackageIndex);
		// if we only want maps, skip non-maps
		if (bMapsOnly && Filename.GetExtension() != FURL::DefaultMapExt)
		{
			continue;
		}
		if (bNonMapsOnly && Filename.GetExtension() == FURL::DefaultMapExt)
		{
			continue;
		}

		UObject::BeginLoad();
		ULinkerLoad* Linker = UObject::GetPackageLinker( NULL, *Filename, LOAD_Quiet|LOAD_NoWarn|LOAD_NoVerify, NULL, NULL );
		UObject::EndLoad();

		// see if we have any sequences
		UBOOL bHasSequences = FALSE;
		FName SequenceName(TEXT("Sequence"));
		for (INT ExportIndex = 0; ExportIndex < Linker->ExportMap.Num(); ExportIndex++)
		{
			if (Linker->GetExportClassName(ExportIndex) == SequenceName)
			{
				bHasSequences = TRUE;
				break;
			}
		}

		if (!bHasSequences)
		{
			SET_WARN_COLOR(COLOR_DARK_YELLOW);
			warnf(TEXT("Skipping %s (no sequences)"), *Filename);
			CLEAR_WARN_COLOR();
			continue;
		}

		warnf(TEXT("Processing %s"), *Filename);
		// open the package
		UPackage* Package = UObject::LoadPackage( NULL, *Filename, LOAD_NoWarn|LOAD_Quiet );
		if( Package == NULL )
		{
			warnf( NAME_Error, TEXT("Error loading %s!"), *Filename );
		}

		UBOOL bDirty = FALSE;
		// look for kismet sequences
		for (TObjectIterator<USequence> It; It; ++It)
		{
			if (!It->IsIn(Package))
			{
				continue;
			}
			TArray<USequenceObject*>& Objects = It->SequenceObjects;

			// n^2 operation looking for duplicate
			for (INT ObjIndex1 = 0; ObjIndex1 < Objects.Num(); ObjIndex1++)
			{
				USequenceObject* Obj1 = Objects(ObjIndex1);
				for (INT ObjIndex2 = ObjIndex1 + 1; ObjIndex2 < Objects.Num(); ObjIndex2++)
				{
					USequenceObject* Obj2 = Objects(ObjIndex2);
					if (Obj1->ObjPosX == Obj2->ObjPosX && Obj1->ObjPosY == Obj2->ObjPosY && 
						Obj1->DrawWidth == Obj2->DrawWidth && Obj1->DrawHeight == Obj2->DrawHeight &&
						Obj1->GetClass() == Obj2->GetClass())
					{
						SET_WARN_COLOR(COLOR_GREEN);
						warnf(TEXT("Two duplicate sequence objects: '%s' and '%s'"), *Obj1->GetFullName(), *Obj2->GetPathName());
						CLEAR_WARN_COLOR();
						if (bDeleteDuplicates)
						{
							Objects.RemoveItem(Obj2);
							bDirty = TRUE;
						}
					}
				}
			}
		}

		if (bDirty)
		{
			SavePackageHelper(Package, Filename);
		}

//		if (bDirty || (PackageIndex % 10) == 0)
		{
			UObject::CollectGarbage(RF_Native);
		}
	}
	return 0;
}
IMPLEMENT_CLASS(UFindDuplicateKismetObjectsCommandlet);


/*-----------------------------------------------------------------------------
UAnalyzeKismetCommandlet.
-----------------------------------------------------------------------------*/
INT UAnalyzeKismetCommandlet::Main( const FString& Params )
{
	INT PackageCount = 0, KismetObjCount = 0;
	TArray<FFilename> FilesInPath;
	TArray<FString> Tokens, Switches, KismetObjNames, Unused;

	ParseCommandLine(*Params, Tokens, Switches);

	for( INT TokenIndex = 0; TokenIndex < Tokens.Num(); ++TokenIndex )
	{
		TArray<FFilename> TokenFiles;
		if ( !NormalizePackageNames( Unused, TokenFiles, Tokens(TokenIndex) ) )
		{
			warnf(TEXT("No packages found for parameter %i: '%s'"), TokenIndex, *Tokens(TokenIndex));
			continue;
		}
		FilesInPath += TokenFiles;
	}

	for( INT SwitchIndex = 0; SwitchIndex < Switches.Num(); ++SwitchIndex )
	{
		if( Switches(SwitchIndex).Left(4).ToLower() == TEXT("obj=") )
		{
			KismetObjNames.AddItem(Switches(SwitchIndex).Mid(4).ToLower());
		}
		else if( Switches(SwitchIndex).Left(16).ToLower() == TEXT("showunreferenced") )
		{
			// Assemble array of native script packages
			TArray<FString>	AllScriptPackageNames;
			appGetScriptPackageNames(AllScriptPackageNames, SPT_Native, FALSE);

			// Load native script packages, so we can create a list of Kismet object classes
			for( INT PackageNameIdx = 0; PackageNameIdx < AllScriptPackageNames.Num(); ++PackageNameIdx )
			{
				UObject::LoadPackage( NULL, *AllScriptPackageNames(PackageNameIdx), LOAD_NoWarn|LOAD_Quiet );
			}

			// Initialize all Kimset object class reference counts to 0
			for( TObjectIterator<UClass> It; It; ++It )
			{
				if( It->IsChildOf(USequenceObject::StaticClass()) )
				{
					ResourceMap.Set(It->GetFName(), FKismetResourceStat(It->GetFName(), 0));
				}
			}
		}
		else if( Switches(SwitchIndex).Left(13).ToLower() == TEXT("showlevelrefs") )
		{
			bShowLevelRefs = TRUE;
		}
	}

	// If no files / wildcards are specified, look at all maps in the Core.System paths
	if( Tokens.Num() == 0 && FilesInPath.Num() == 0 )
	{
		NormalizePackageNames( Unused, FilesInPath, FString("*.") + *FURL::DefaultMapExt );
	}

	if( FilesInPath.Num() == 0 )
	{
		warnf(TEXT("No packages found"));
	}


	for( INT PackageIndex = 0; PackageIndex < FilesInPath.Num(); ++PackageIndex )
	{
		const FFilename& Filename = FilesInPath(PackageIndex);

		++PackageCount;

		UObject::BeginLoad();
		ULinkerLoad* Linker = UObject::GetPackageLinker( NULL, *Filename, LOAD_Quiet|LOAD_NoWarn|LOAD_NoVerify, NULL, NULL );
		UObject::EndLoad();

		// see if we have any sequences
		UBOOL bHasSequences = FALSE;
		FName SequenceName(TEXT("Sequence"));
		for( INT ExportIndex = 0; ExportIndex < Linker->ExportMap.Num(); ++ExportIndex )
		{
			if (Linker->GetExportClassName(ExportIndex) == SequenceName)
			{
				bHasSequences = TRUE;
				break;
			}
		}

		if( !bHasSequences )
		{
			SET_WARN_COLOR(COLOR_DARK_YELLOW);
			warnf(TEXT("Skipping %s (no sequences)"), *Filename);
			CLEAR_WARN_COLOR();
			continue;
		}

		warnf(TEXT("Processing %s"), *Filename);
		// open the package
		UPackage* Package = UObject::LoadPackage( NULL, *Filename, LOAD_NoWarn|LOAD_Quiet );
		if( Package == NULL )
		{
			warnf( NAME_Error, TEXT("Error loading %s!"), *Filename );
		}

		// look for kismet sequences
		for( TObjectIterator<USequence> It; It; ++It )
		{
			if( !It->IsIn(Package) )
			{
				continue;
			}
			TArray<USequenceObject*>& Objects = It->SequenceObjects;

			// Collect the data
			for( INT ObjIndex = 0; ObjIndex < Objects.Num(); ++ObjIndex )
			{
				USequenceObject* CurObj = Objects(ObjIndex);
				FKismetResourceStat* CurStat = NULL;

				// If Kismet objects are specified on the commandline, only collect stats for those
				if( KismetObjNames.Num() > 0 && !KismetObjNames.ContainsItem(CurObj->GetClass()->GetName().ToLower()) )
				{
					continue;
				}

				CurStat = ResourceMap.Find(CurObj->GetClass()->GetFName());
				if( CurStat )
				{
					++CurStat->ReferenceCount;
				}
				else
				{
					CurStat = &ResourceMap.Set(CurObj->GetClass()->GetFName(), FKismetResourceStat(CurObj->GetClass()->GetFName(), 1));
				}

				if( CurStat->ReferenceCount == 1 )
				{
					++KismetObjCount;
				}

				if( CurStat->ReferenceSources.Num() == 0 || CurStat->ReferenceSources.Top() != Package->GetName() )
				{
					CurStat->ReferenceSources.AddItem( Package->GetName() );
				}
			}
		}

		UObject::CollectGarbage(RF_Native);
	}

	if( ResourceMap.Num() != 0 )
	{
		// Create string with system time to create a unique filename.
		INT Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec;
		appSystemTime( Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec );
		FString	CurrentTime = FString::Printf(TEXT("%i.%02i.%02i-%02i.%02i.%02i"), Year, Month, Day, Hour, Min, Sec );
		FString CSVFilename = FString::Printf(TEXT("%sAnalyzeKismet"), *appGameLogDir());
	
		CSVFilename = FString::Printf(TEXT("%s-%s.csv"), *CSVFilename, *CurrentTime);

		SET_WARN_COLOR(COLOR_GREEN);
		warnf(TEXT(""));
		warnf(TEXT("Analyzed Kismet in: %d packages and found %d Kismet objects in use"), PackageCount, KismetObjCount);
		warnf(TEXT("Writing data to file: %s"), *CSVFilename);
		CLEAR_WARN_COLOR();

		// create a .csv
		CreateReport(CSVFilename);
	}

	return 0;
}

void UAnalyzeKismetCommandlet::CreateReport(const FString& FileName) const
{
	FArchive* CSVFile = GFileManager->CreateFileWriter(*FileName);
	if( !CSVFile )
	{
		warnf(NAME_Error, TEXT("Failed to open output file %s"), *FileName);
	}

	FString CSVLine;
	for( KismetResourceMap::TConstIterator It(ResourceMap); It; ++It )
	{
		const FKismetResourceStat& CurStat = It.Value();

		// dump out a line to the .csv file
		if( bShowLevelRefs )
		{
			CSVLine = FString::Printf(TEXT("%s,%d"), *CurStat.ObjectName.ToString(), CurStat.ReferenceCount);
			for( INT RefSourceIdx = 0; RefSourceIdx < CurStat.ReferenceSources.Num(); ++RefSourceIdx )
			{
				CSVLine = FString::Printf(TEXT("%s,%s"), *CSVLine, *CurStat.ReferenceSources(RefSourceIdx));
			}
			CSVLine = FString::Printf(TEXT("%s,%s"), *CSVLine, LINE_TERMINATOR);
		}
		else
		{
			CSVLine = FString::Printf(TEXT("%s,%d%s"), *CurStat.ObjectName.ToString(), CurStat.ReferenceCount, LINE_TERMINATOR);
		}
		

		CSVFile->Serialize(TCHAR_TO_ANSI(*CSVLine), CSVLine.Len());
	}

	CSVFile->Close();
	delete CSVFile;
}
IMPLEMENT_CLASS(UAnalyzeKismetCommandlet);

/*-----------------------------------------------------------------------------
UUpdateKismetCommandlet.
-----------------------------------------------------------------------------*/
INT UUpdateKismetCommandlet::Main( const FString& Params )
{
	INT KismetObjCount = 0;
	TArray<FFilename> FilesInPath;
	TArray<FString> Tokens, Switches, KismetObjNames, Unused;
	UBOOL bDirty = FALSE;

	ParseCommandLine(*Params, Tokens, Switches);

#if HAVE_SCC
	// Ensure source control is initialized and shut down properly
	FScopedSourceControl SourceControl;
#endif

	for( INT TokenIndex = 0; TokenIndex < Tokens.Num(); ++TokenIndex )
	{
		TArray<FFilename> TokenFiles;
		if ( !NormalizePackageNames( Unused, TokenFiles, Tokens(TokenIndex) ) )
		{
			warnf(TEXT("No packages found for parameter %i: '%s'"), TokenIndex, *Tokens(TokenIndex));
			continue;
		}
		FilesInPath += TokenFiles;
	}

	const UBOOL bAutoCheckOut = Switches.ContainsItem(TEXT("AutoCheckOutPackages"));
	for( INT SwitchIndex = 0; SwitchIndex < Switches.Num(); ++SwitchIndex )
	{
		if( Switches(SwitchIndex).Left(4).ToLower() == TEXT("obj=") )
		{
			KismetObjNames.AddItem(Switches(SwitchIndex).Mid(4).ToLower());
		}
	}

	// If no files / wildcards are specified, look at all maps in the Core.System paths
	if( Tokens.Num() == 0 && FilesInPath.Num() == 0 )
	{
		NormalizePackageNames( Unused, FilesInPath, FString("*.") + *FURL::DefaultMapExt );
	}

	if( FilesInPath.Num() == 0 )
	{
		warnf(TEXT("No packages found"));
	}


	for( INT PackageIndex = 0; PackageIndex < FilesInPath.Num(); ++PackageIndex )
	{
		const FFilename& Filename = FilesInPath(PackageIndex);
		bDirty = FALSE;

#if HAVE_SCC
		if ( bAutoCheckOut && FSourceControl::ForceGetStatus( Filename ) == SCC_NotCurrent )
		{
			warnf( NAME_Log, TEXT("Skipping %s (Not at head source control revision)"), *Filename );
			continue;
		}
#endif

		UObject::BeginLoad();
		ULinkerLoad* Linker = UObject::GetPackageLinker( NULL, *Filename, LOAD_Quiet|LOAD_NoWarn|LOAD_NoVerify, NULL, NULL );
		UObject::EndLoad();

		// see if we have any sequences
		UBOOL bHasSequences = FALSE;
		FName SequenceName(TEXT("Sequence"));
		for( INT ExportIndex = 0; ExportIndex < Linker->ExportMap.Num(); ++ExportIndex )
		{
			if (Linker->GetExportClassName(ExportIndex) == SequenceName)
			{
				bHasSequences = TRUE;
				break;
			}
		}

		if( !bHasSequences )
		{
			SET_WARN_COLOR(COLOR_DARK_YELLOW);
			warnf(TEXT("Skipping %s (no sequences)"), *Filename);
			CLEAR_WARN_COLOR();
			continue;
		}

		warnf(TEXT("Processing %s"), *Filename);
		// open the package
		UPackage* Package = UObject::LoadPackage( NULL, *Filename, LOAD_NoWarn|LOAD_Quiet );
		if( Package == NULL )
		{
			warnf( NAME_Error, TEXT("Error loading %s!"), *Filename );
		}


		// See if the package is read only. If so, and auto-checkout is enabled, check the file out
		UBOOL bIsReadOnly = GFileManager->IsReadOnly( *Filename);
		if( bIsReadOnly == TRUE && bAutoCheckOut == FALSE )
		{
			warnf(NAME_Log,TEXT("Package [%s] is read-only - skipping"), *Filename );
			continue;
		}
		else
		{
			// check to see if we need to check this package out
#if HAVE_SCC
			if( bIsReadOnly == TRUE && bAutoCheckOut == TRUE )
			{
				FSourceControl::CheckOut(Package);
			}
#endif

			// so now we need to see if we actually were able to check this file out
			// if the file is still read only then we failed and need to emit an error and go to the next package
			if( GFileManager->IsReadOnly( *Filename ) == TRUE )
			{
				warnf( NAME_Error, TEXT("Unable to check out the Package: %s"), *Filename );
				continue;
			}
		}


		// look for kismet sequences
		for( TObjectIterator<USequence> It; It; ++It )
		{
			if( !It->IsIn(Package) )
			{
				continue;
			}
			TArray<USequenceObject*>& Objects = It->SequenceObjects;

			// Collect the data
			INT ObjIndex = 0;
			while( ObjIndex < Objects.Num() )
			{
				USequenceObject* CurObj = Objects(ObjIndex);
				USequenceObject* NewObj = NULL;

				// If Kismet objects are specified on the commandline, only update those
				if( KismetObjNames.Num() > 0 && !KismetObjNames.ContainsItem(CurObj->GetClass()->GetName().ToLower()) )
				{
					continue;
				}

				NewObj = CurObj->ConvertObject();
				if( NewObj )
				{
					warnf(TEXT("Converting Kismet Object %s"), *CurObj->GetName());
					NewObj->UpdateObject();
					Objects(ObjIndex) = NewObj;
					bDirty = TRUE;
				}
				else if( CurObj->eventGetObjClassVersion() != CurObj->ObjInstanceVersion)
				{
					warnf(TEXT("Updating Kismet Object %s"), *CurObj->GetName());
					CurObj->UpdateObject();
					bDirty = TRUE;
				}

				++ObjIndex;
			}
		}

		if (bDirty)
		{
			SavePackageHelper(Package, Filename);
		}

		UObject::CollectGarbage(RF_Native);
	}
	return 0;
}
IMPLEMENT_CLASS(UUpdateKismetCommandlet);

/*-----------------------------------------------------------------------------
	UFixupEmitters commandlet.
-----------------------------------------------------------------------------*/
INT UFixupEmittersCommandlet::Main( const FString& Params )
{
	// 
	FString			Token;
	const TCHAR*	CommandLine		= appCmdLine();
	UBOOL			bForceCheckAll	= FALSE;
	FString			FileToFixup(TEXT(""));

	while (ParseToken(CommandLine, Token, 1))
	{
		if (Token == TEXT("-FORCEALL"))
		{
			bForceCheckAll = TRUE;
		}
		else
		{
			FString PackageName;
			if (GPackageFileCache->FindPackageFile(*Token, NULL, PackageName))
			{
				FileToFixup = FFilename(Token).GetBaseFilename();
			}
			else
			{
				debugf(TEXT("Package %s Not Found."), *Token);
				return -1;
			}
		}
	}


	// Retrieve list of all packages in .ini paths.
	TArray<FString> PackageList = GPackageFileCache->GetPackageFileList();
	if (!PackageList.Num())
		return 0;

	// Determine the list of packages we have to touch...
	TArray<FString> RequiredPackageList;
	RequiredPackageList.Empty(PackageList.Num());

	if (FileToFixup != TEXT(""))
	{
		for (INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++)
		{
			const FFilename& Filename = PackageList(PackageIndex);
			if (appStricmp(*Filename.GetBaseFilename(), *FileToFixup) == 0)
			{
				new(RequiredPackageList)FString(PackageList(PackageIndex));
				break;
			}
		}
	}
	else
	{
		// Iterate over all packages.
		for (INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++)
		{
			const FFilename& Filename = PackageList(PackageIndex);
			if (Filename.GetExtension() == TEXT("U"))
			{
				warnf(NAME_Log, TEXT("Skipping script file %s"), *Filename);
				continue;
			}

			// Assert if package couldn't be opened so we have no chance of messing up saving later packages.
			UPackage* Package = CastChecked<UPackage>(UObject::LoadPackage(NULL, *Filename, 0));
			if (Package)
			{
				if (bForceCheckAll)
				{
					new(RequiredPackageList)FString(PackageList(PackageIndex));
				}
				else
				{
					TArray<UPackage*> Packages;
					Packages.AddItem(Package);

					UBOOL bIsDirty = FALSE;
					// Fix up ParticleSystems
					for (TObjectIterator<UParticleSystem> It; It && !bIsDirty; ++It)
					{
						if (It->IsIn(Package))
						{
							UParticleSystem* PartSys = Cast<UParticleSystem>(*It);
							check(PartSys->GetOuter());

							for (INT i = PartSys->Emitters.Num() - 1; (i >= 0) && !bIsDirty; i--)
							{
								UParticleEmitter* Emitter = PartSys->Emitters(i);
								if (Emitter == NULL)
									continue;

								UObject* EmitterOuter = Emitter->GetOuter();
								if (EmitterOuter != PartSys)
								{
									bIsDirty	= TRUE;
								}

								if (!bIsDirty)
								{
									for (INT LODIndex = 0; (LODIndex < Emitter->LODLevels.Num()) && !bIsDirty; LODIndex++)
									{
										UParticleLODLevel* LODLevel = Emitter->LODLevels(LODIndex);
										if (LODLevel == NULL)
										{
											continue;
										}

										UParticleModule* Module;
										Module = LODLevel->TypeDataModule;
										if (Module)
										{
											if (Module->GetOuter() != EmitterOuter)
											{
												bIsDirty	= TRUE;
											}
										}

										if (!bIsDirty)
										{
											for (INT i = 0; i < LODLevel->Modules.Num(); i++)
											{
												Module = LODLevel->Modules(i);
												if (Module)
												{
													if (Module->GetOuter() != EmitterOuter)
													{
														bIsDirty	= TRUE;
														break;
													}
												}
											}
										}
									}
								}
							}

							if (bIsDirty)
							{
								new(RequiredPackageList)FString(PackageList(PackageIndex));
							}
							else
							{
								warnf(NAME_Log, TEXT("ALREADY CONVERTED? Skipping package %32s"), *Filename);
							}
							break;
						}
					}

					// Fix up ParticleEmitters
					for (TObjectIterator<UParticleEmitter> It; It && !bIsDirty; ++It)
					{
						if (It->IsIn(Package))
						{
							UParticleEmitter* Emitter = *It;
							UParticleSystem* PartSys = Cast<UParticleSystem>(Emitter->GetOuter());
							if (PartSys == NULL)
							{
								bIsDirty	= TRUE;
							}

							if (!bIsDirty)
							{
								UObject* EmitterOuter = Emitter->GetOuter();
								if (EmitterOuter != PartSys)
								{
									bIsDirty	= TRUE;
								}

								if (!bIsDirty)
								{
									for (INT LODIndex = 0; (LODIndex < Emitter->LODLevels.Num()) && !bIsDirty; LODIndex++)
									{
										UParticleLODLevel* LODLevel = Emitter->LODLevels(LODIndex);
										if (LODLevel == NULL)
										{
											continue;
										}

										UParticleModule* Module;
										Module = LODLevel->TypeDataModule;
										if (Module)
										{
											if (Module->GetOuter() != EmitterOuter)
											{
												bIsDirty	= TRUE;
											}
										}

										if (!bIsDirty)
										{
											for (INT i = 0; i < LODLevel->Modules.Num(); i++)
											{
												Module = LODLevel->Modules(i);
												if (Module)
												{
													if (Module->GetOuter() != EmitterOuter)
													{
														bIsDirty	= TRUE;
														break;
													}
												}
											}
										}
									}
								}
							}
						}
						if (bIsDirty)
						{
							new(RequiredPackageList)FString(PackageList(PackageIndex));
						}
						else
						{
							warnf(NAME_Log, TEXT("ALREADY CONVERTED? Skipping package %32s"), *Filename);
						}
						break;
					}
				}
			}
			UObject::CollectGarbage(RF_Native);
		}
	}

	if (!RequiredPackageList.Num())
	{
		warnf(NAME_Log, TEXT("No emitter fixups required!"));
		return 0;
	}

	warnf(NAME_Log, TEXT("Emitter fixups required for %d packages..."), RequiredPackageList.Num());

	// source control object
#if HAVE_SCC
	FSourceControl::Init();
#endif

	// Iterate over all packages.
	for (INT PackageIndex = 0; PackageIndex < RequiredPackageList.Num(); PackageIndex++)
	{
		const FFilename& Filename = RequiredPackageList(PackageIndex);

		// Assert if package couldn't be opened so we have no chance of messing up saving later packages.
		UPackage* Package = CastChecked<UPackage>(UObject::LoadPackage(NULL, *Filename, 0));
		check(Package);

		TArray<UPackage*> Packages;
		Packages.AddItem(Package);

		if (GFileManager->IsReadOnly( *Filename))
		{
#if HAVE_SCC
			// update scc status
			FSourceControl::UpdatePackageStatus(Packages);

			// can we check it out?
			INT SCCState = GPackageFileCache->GetSourceControlState(*Package->GetName());
			if (SCCState == SCC_ReadOnly)
			{
				// The package is still available, so do the check out.
				FSourceControl::CheckOut(Package);
			}
#endif

			// if the checkout failed for any reason, this will still be readonly, so we can't clean it up
			const INT	FailCount	= 3;
			INT			iCount		= 0;
			while (GFileManager->IsReadOnly( *Filename) && (iCount < FailCount))
			{
				if (iCount == (FailCount - 1))
				{
					// No warning
				}
				else
				if ((iCount + 1) == (FailCount - 1))
				{
					appMsgf(AMT_OK, TEXT("LAST CHANCE - ABOUT TO SKIP!\nCouldn't check out %s, manually do it, sorry!"), *Filename);
				}
				else
				{
					appMsgf(AMT_OK, TEXT("Couldn't check out %s, manually do it, sorry!"), *Filename);
				}
				iCount++;
			}

			if (iCount >= FailCount)
			{
				appMsgf(AMT_OK, TEXT("Skipping %s"), *Filename);
				continue;
			}
		}
		
		warnf(NAME_Log, TEXT("Loading %s"), *Filename);

		if (Package)
		{
			UBOOL	bIsDirty = FALSE;

			// Fix up ParticleSystems
			for (TObjectIterator<UParticleSystem> It; It; ++It)
			{
				if (It->IsIn(Package))
				{
					UParticleSystem* PartSys = Cast<UParticleSystem>(*It);
					check(PartSys->GetOuter());
					
					UBOOL bResetTabs = FALSE;

					for (INT EmitterIndex = PartSys->Emitters.Num() - 1; EmitterIndex >= 0; EmitterIndex--)
					{
						UParticleEmitter* Emitter = PartSys->Emitters(EmitterIndex);
						if (Emitter == NULL)
							continue;

						UObject* EmitterOuter = Emitter->GetOuter();
						if (EmitterOuter != PartSys)
						{
							// Replace the outer...
							Emitter->Rename(NULL, PartSys, REN_ForceNoResetLoaders);
							debugf(TEXT("Renaming particle emitter %32s from Outer %32s to Outer %32s"),
								*(Emitter->GetName()), *(EmitterOuter->GetName()), *(GetName()));
							bResetTabs = TRUE;
							bIsDirty = TRUE;
						}

						UParticleModule* Module;
						UParticleLODLevel* LODLevel;

						for (INT LODIndex = 0; LODIndex < Emitter->LODLevels.Num(); LODIndex++)
						{
							LODLevel = Emitter->LODLevels(LODIndex);
							if (LODLevel == NULL)
							{
								continue;
							}

							Module = LODLevel->TypeDataModule;
							if (Module)
							{
								UObject* ModuleOuter = Module->GetOuter();
								if (ModuleOuter != EmitterOuter)
								{
									// Replace the outer...
									debugf(TEXT("  Renaming particle module %32s from Outer %32s to Outer %32s"),
										*(Module->GetName()), *(ModuleOuter->GetName()), *(EmitterOuter->GetName()));
									Module->Rename(NULL, EmitterOuter, REN_ForceNoResetLoaders);
									bResetTabs = TRUE;
									bIsDirty = TRUE;
								}
							}

							for (INT ModuleIndex = 0; ModuleIndex < LODLevel->Modules.Num(); ModuleIndex++)
							{
								Module = LODLevel->Modules(ModuleIndex);
								if (Module)
								{
									UObject* ModuleOuter = Module->GetOuter();
									if (ModuleOuter != EmitterOuter)
									{
										// Replace the outer...
										debugf(TEXT("  Renaming particle module %32s from Outer %32s to Outer %32s"),
											*(Module->GetName()), *(ModuleOuter->GetName()), 
											*(EmitterOuter->GetName()));
										Module->Rename(NULL, EmitterOuter, REN_ForceNoResetLoaders);
										bResetTabs = TRUE;
										bIsDirty = TRUE;
									}
								}
							}
						}
					}
					if (bResetTabs)
					{
						if (PartSys->CurveEdSetup)
						{
							PartSys->CurveEdSetup->ResetTabs();
						}
					}
				}
			}

			// Fix up ParticleEmitters
			for (TObjectIterator<UParticleEmitter> It; It; ++It)
			{
				if (It->IsIn(Package))
				{
					UParticleEmitter* Emitter = *It;
					UParticleSystem* PartSys = Cast<UParticleSystem>(Emitter->GetOuter());
					UObject* EmitterOuter = Emitter->GetOuter();

					UParticleModule* Module;
					UParticleLODLevel* LODLevel;
					for (INT LODIndex = 0; LODIndex < Emitter->LODLevels.Num(); LODIndex++)
					{
						LODLevel = Emitter->LODLevels(LODIndex);
						if (LODLevel == NULL)
						{
							continue;
						}

						Module = LODLevel->TypeDataModule;
						if (Module)
						{
							UObject* ModuleOuter = Module->GetOuter();
							if (ModuleOuter != EmitterOuter)
							{
								// Replace the outer...
								debugf(TEXT("  Renaming particle module %32s from Outer %32s to Outer %32s"),
									*(Module->GetName()), *(ModuleOuter->GetName()), *(EmitterOuter->GetName()));
								Module->Rename(NULL, EmitterOuter, REN_ForceNoResetLoaders);
								bIsDirty = TRUE;
							}
						}

						for (INT ModuleIndex = 0; ModuleIndex < LODLevel->Modules.Num(); ModuleIndex++)
						{
							Module = LODLevel->Modules(ModuleIndex);
							if (Module)
							{
								UObject* ModuleOuter = Module->GetOuter();
								if (ModuleOuter != EmitterOuter)
								{
									// Replace the outer...
									debugf(TEXT("  Renaming particle module %32s from Outer %32s to Outer %32s"),
										*(Module->GetName()), *(ModuleOuter->GetName()), 
										*(EmitterOuter->GetName()));
									Module->Rename(NULL, EmitterOuter, REN_ForceNoResetLoaders);
									bIsDirty = TRUE;
								}
							}
						}
					}
				}
			}

			if (bIsDirty)
			{
				UWorld* World = FindObject<UWorld>( Package, TEXT("TheWorld") );
				if( World )
				{	
					UObject::SavePackage( Package, World, 0, *Filename, GWarn );
				}
				else
				{
					UObject::SavePackage( Package, NULL, RF_Standalone, *Filename, GWarn );
				}
			}
		}
		UObject::CollectGarbage(RF_Native);
	}

#if HAVE_SCC
	FSourceControl::Close();
#endif

	return 0;
}
IMPLEMENT_CLASS(UFixupEmittersCommandlet)

/*-----------------------------------------------------------------------------
	UFindEmitterMismatchedLODs commandlet.
	Used to list particle systems containing emitters with LOD level counts 
	that are different than other emitters in the same particle system.
-----------------------------------------------------------------------------*/
void UFindEmitterMismatchedLODsCommandlet::CheckPackageForMismatchedLODs( const FFilename& Filename, UBOOL bFixup )
{
	UPackage* Package = CastChecked<UPackage>(UObject::LoadPackage(NULL, *Filename, 0));
	if (Package)
	{
		UBOOL bSaveIt = FALSE;

		warnf( NAME_Log, TEXT( "Checking package for mismatched LODs: %s" ), *(Package->GetPathName()));
		// Check all ParticleSystems
		for (TObjectIterator<UParticleSystem> It; It; ++It)
		{
			if (It->IsIn(Package))
			{
				UBOOL bIsMismatched = FALSE;
			
				UParticleSystem* PartSys = Cast<UParticleSystem>(*It);

				INT LODLevelCount = -1;
				INT EmitterZeroLevelCount = (PartSys->Emitters.Num() > 0) ? PartSys->Emitters(0)->LODLevels.Num() : 0;
				for (INT i = PartSys->Emitters.Num() - 1; (i >= 0) && (!bIsMismatched || bFixup); i--)
				{
					UParticleEmitter* Emitter = PartSys->Emitters(i);
					if (Emitter == NULL)
					{
						continue;
					}

					INT CheckLODLevelCount = Emitter->LODLevels.Num();
					if (LODLevelCount == -1)
					{
						// First emitter in system...
						LODLevelCount = CheckLODLevelCount;
					}
					else
					{
						if (LODLevelCount != CheckLODLevelCount)
						{
							bIsMismatched = TRUE;
						}
					}
				}

				if (bIsMismatched == TRUE)
				{
					warnf( NAME_Log, TEXT("\t*** PSys with mismatched LODs - %s"), *(PartSys->GetPathName()));
					if ((bFixup == TRUE) && (EmitterZeroLevelCount > 0))
					{
						UBOOL bFixedIt = TRUE;
						for (INT EmitterIndex = 0; EmitterIndex < PartSys->Emitters.Num(); EmitterIndex++)
						{
							UParticleEmitter* Emitter = PartSys->Emitters(EmitterIndex);
							if (Emitter == NULL)
							{
								continue;
							}
							
							if (Emitter->LODLevels.Num() != EmitterZeroLevelCount)
							{
								// Need to fix this up...
								warnf( NAME_Log, TEXT("\t\tEmitter %2d: Has %2d levels, needs %2d"), 
									EmitterIndex, Emitter->LODLevels.Num(), EmitterZeroLevelCount);

								if (Emitter->LODLevels.Num() < EmitterZeroLevelCount)
								{
									for (INT LODInsertIndex = Emitter->LODLevels.Num(); LODInsertIndex < EmitterZeroLevelCount; LODInsertIndex++)
									{
										if (Emitter->CreateLODLevel(LODInsertIndex) != LODInsertIndex)
										{
											bFixedIt = FALSE;
										}
									}
								}
								else
								if (Emitter->LODLevels.Num() > EmitterZeroLevelCount)
								{
									for (INT LODRemoveIndex = Emitter->LODLevels.Num() - 1; LODRemoveIndex >= EmitterZeroLevelCount; LODRemoveIndex--)
									{
										Emitter->LODLevels.Remove(LODRemoveIndex);
									}
								}
							}
						}

						warnf( NAME_Log, TEXT("\t\tFix up - %s"), bFixedIt ? TEXT("COMPLETED") : TEXT("FAILED"));

						bSaveIt = TRUE;
					}
				}
			}
		}

		if (bSaveIt)
		{
			Package->FullyLoad();
			UWorld* World = FindObject<UWorld>( Package, TEXT("TheWorld") );
			if( World )
			{	
				UObject::SavePackage( Package, World, 0, *Filename, GWarn );
			}
			else
			{
				UObject::SavePackage( Package, NULL, RF_Standalone, *Filename, GWarn );
			}
		}

		UObject::CollectGarbage(RF_Native);
	}
}

INT UFindEmitterMismatchedLODsCommandlet::Main( const FString& Params )
{
	// 
	FString			Token;
	const TCHAR*	CommandLine = appCmdLine();
	UBOOL			bForceCheckAll = FALSE;
	FString			FileToFixup(TEXT(""));
	UBOOL			bFixup = FALSE;
	UBOOL			bProcessMaps = FALSE;

	while (ParseToken(CommandLine, Token, 1))
	{
		if (Token == TEXT("-FIXUP"))
		{
			bFixup = TRUE;
		}
		else
		if (Token == TEXT("-MAPS"))
		{
			bProcessMaps = TRUE;
		}
		else
		if (Token == TEXT("-FORCEALL"))
		{
			bForceCheckAll = TRUE;
		}
		else
		{
			FString PackageName;
			if (GPackageFileCache->FindPackageFile(*Token, NULL, PackageName))
			{
				FileToFixup = FFilename(Token).GetBaseFilename();
			}
			else
			{
				debugf(TEXT("Package %s Not Found."), *Token);
				return -1;
			}
		}
	}


	// Retrieve list of all packages in .ini paths.
	TArray<FString> PackageList = GPackageFileCache->GetPackageFileList();
	if (!PackageList.Num())
		return 0;

	// Determine the list of packages we have to touch...
	TArray<FString> RequiredPackageList;
	RequiredPackageList.Empty(PackageList.Num());

	if (FileToFixup != TEXT(""))
	{
		for (INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++)
		{
			const FFilename& Filename = PackageList(PackageIndex);
			if (appStricmp(*Filename.GetBaseFilename(), *FileToFixup) == 0)
			{
				CheckPackageForMismatchedLODs(Filename, bFixup);
			}
		}
	}
	else
	{
		// Iterate over all packages.
		for (INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++)
		{
			const FFilename& Filename = PackageList(PackageIndex);
			if (Filename.GetExtension() == TEXT("U"))
			{
				warnf(NAME_Log, TEXT("Skipping script file %s"), *Filename);
				continue;
			}
			if (appStristr(*(Filename), TEXT("ShaderCache")) != NULL)
			{
				warnf(NAME_Log, TEXT("Skipping ShaderCache file %s"), *Filename);
				continue;
			}
			if (appStristr(*(Filename.GetPath()), TEXT("Autosave")) != NULL)
			{
				warnf(NAME_Log, TEXT("Skipping Autosave file %s"), *Filename);
				continue;
			}
			// if we don't want to load maps 
			if ((bProcessMaps == FALSE) && (Filename.GetExtension() == FURL::DefaultMapExt))
			{
				warnf(NAME_Log, TEXT("Skipping map file %s"), *Filename);
				continue;
			}

			CheckPackageForMismatchedLODs(Filename, bFixup);
		}
	}
	return 0;
}

IMPLEMENT_CLASS(UFindEmitterMismatchedLODsCommandlet)

/*-----------------------------------------------------------------------------
	UFindEmitterModifiedLODs commandlet.
	Used to list particle systems containing emitters with LOD levels that are
	marked as modified. (Indicating they should be examined to ensure they are
	'in-sync' with the high-lod level.)
-----------------------------------------------------------------------------*/
INT UFindEmitterModifiedLODsCommandlet::Main( const FString& Params )
{
	// 
	FString			Token;
	const TCHAR*	CommandLine		= appCmdLine();
	UBOOL			bForceCheckAll	= FALSE;
	FString			FileToFixup(TEXT(""));

	while (ParseToken(CommandLine, Token, 1))
	{
		if (Token == TEXT("-FORCEALL"))
		{
			bForceCheckAll = TRUE;
		}
		else
		{
			FString PackageName;
			if (GPackageFileCache->FindPackageFile(*Token, NULL, PackageName))
			{
				FileToFixup = FFilename(Token).GetBaseFilename();
			}
			else
			{
				debugf(TEXT("Package %s Not Found."), *Token);
				return -1;
			}
		}
	}


	// Retrieve list of all packages in .ini paths.
	TArray<FString> PackageList = GPackageFileCache->GetPackageFileList();
	if (!PackageList.Num())
		return 0;

	// Determine the list of packages we have to touch...
	TArray<FString> RequiredPackageList;
	RequiredPackageList.Empty(PackageList.Num());

	if (FileToFixup != TEXT(""))
	{
		for (INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++)
		{
			const FFilename& Filename = PackageList(PackageIndex);
			if (appStricmp(*Filename.GetBaseFilename(), *FileToFixup) == 0)
			{
				new(RequiredPackageList)FString(PackageList(PackageIndex));
				break;
			}
		}
	}
	else
	{
		// Iterate over all packages.
		for (INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++)
		{
			const FFilename& Filename = PackageList(PackageIndex);
			if (Filename.GetExtension() == TEXT("U"))
			{
				warnf(NAME_Log, TEXT("Skipping script file %s"), *Filename);
				continue;
			}
			if (appStristr(*(Filename), TEXT("ShaderCache")) != NULL)
			{
				warnf(NAME_Log, TEXT("Skipping ShaderCache file %s"), *Filename);
				continue;
			}

			// Assert if package couldn't be opened so we have no chance of messing up saving later packages.
			UPackage* Package = CastChecked<UPackage>(UObject::LoadPackage(NULL, *Filename, 0));
			if (Package)
			{
				// Check all ParticleSystems
				for (TObjectIterator<UParticleSystem> It; It; ++It)
				{
					if (It->IsIn(Package))
					{
						UBOOL bIsEditable = FALSE;
					
						UParticleSystem* PartSys = Cast<UParticleSystem>(*It);

						for (INT i = PartSys->Emitters.Num() - 1; (i >= 0) && !bIsEditable; i--)
						{
							UParticleEmitter* Emitter = PartSys->Emitters(i);
							if (Emitter == NULL)
							{
								continue;
							}

							if (Emitter->LODLevels.Num() > 2)
							{
								bIsEditable = TRUE;
							}

							for (INT LODIndex = 1; (LODIndex < Emitter->LODLevels.Num()) && !bIsEditable; LODIndex++)
							{
								UParticleLODLevel* LODLevel = Emitter->LODLevels(LODIndex);
								if (LODLevel == NULL)
								{
									continue;
								}

								UParticleModule* Module;
								Module = LODLevel->TypeDataModule;
								if (Module && (Module->bEditable == TRUE))
								{
									bIsEditable = TRUE;
									break;
								}

								for (INT i = 0; (i < LODLevel->Modules.Num()) && !bIsEditable; i++)
								{
									Module = LODLevel->Modules(i);
									if (Module && (Module->bEditable == TRUE))
									{
										bIsEditable = TRUE;
										break;
									}
								}
							}
						}
						
						if (bIsEditable == TRUE)
						{
							debugf(TEXT("\t*** PSys with modified LODs - %s"), *(PartSys->GetPathName()));
						}
					}
				}

				UObject::CollectGarbage(RF_Native);
			}
		}
	}
	return 0;
}
IMPLEMENT_CLASS(UFindEmitterModifiedLODsCommandlet)

/*-----------------------------------------------------------------------------
	UFindEmitterModuleLODErrors commandlet.
	Used to list particle systems containing emitters with modules that have
	incorrect LOD validity flags set.
-----------------------------------------------------------------------------*/
UBOOL UFindEmitterModuleLODErrorsCommandlet::CheckModulesForLODErrors( INT LODIndex, INT ModuleIndex, 
	const UParticleEmitter* Emitter, const UParticleModule* CurrModule )
{
	if (CurrModule->LODValidity == 0)
	{
		warnf(NAME_Log, TEXT("\t\tLODValidity Not Set? Module %2d in %2d in PSys %s"), 
			ModuleIndex, LODIndex, *(Emitter->GetOuter()->GetPathName()));
		return FALSE;
	}

	if (CurrModule->IsUsedInLODLevel(LODIndex) == FALSE)
	{
		warnf(NAME_Log, TEXT("\t\tNOT USED? Module %2d in %2d in PSys %s"), 
			ModuleIndex, LODIndex, *(Emitter->GetOuter()->GetPathName()));
		return FALSE;
	}

	UBOOL bResult = TRUE;

	if (LODIndex > 0)
	{
		INT CheckIndex = LODIndex - 1;
		while (CheckIndex >= 0)
		{
			if (CurrModule->IsUsedInLODLevel(CheckIndex))
			{
				// Ensure that it is the same as the one it THINKS it is shared with...
				UParticleLODLevel* CheckLODLevel = Emitter->LODLevels(CheckIndex);

				if (CurrModule->IsA(UParticleModuleSpawn::StaticClass()))
				{
					if (CheckLODLevel->SpawnModule != CurrModule)
					{
						warnf(NAME_Log, TEXT("\t\tMistagged SpawnModule at %2d in PSys %s"), 
							LODIndex, *(Emitter->GetOuter()->GetPathName()));
						bResult = FALSE;
					}
				}
				else
				if (CurrModule->IsA(UParticleModuleRequired::StaticClass()))
				{
					if (CheckLODLevel->RequiredModule != CurrModule)
					{
						warnf(NAME_Log, TEXT("\t\tMistagged RequiredModule at %2d in PSys %s"), 
							LODIndex, *(Emitter->GetOuter()->GetPathName()));
						bResult = FALSE;
					}
				}
				else
				if (CurrModule->IsA(UParticleModuleTypeDataBase::StaticClass()))
				{
					if (CheckLODLevel->TypeDataModule != CurrModule)
					{
						warnf(NAME_Log, TEXT("\t\tMistagged TypeDataModule at %2d in PSys %s"), 
							LODIndex, *(Emitter->GetOuter()->GetPathName()));
						bResult = FALSE;
					}
				}
				else
				{
					if (ModuleIndex >= CheckLODLevel->Modules.Num())
					{
						warnf(NAME_Log, TEXT("\t\tMismatched module count at %2d in PSys %s"), 
							LODIndex, *(Emitter->GetOuter()->GetPathName()));
						bResult = FALSE;
					}
					else
					{
						UParticleModule* CheckModule = CheckLODLevel->Modules(ModuleIndex);
						if (CheckModule != CurrModule)
						{
							warnf(NAME_Log, TEXT("\t\tMistagged module (%2d) at %2d in PSys %s"), 
								ModuleIndex, LODIndex, *(Emitter->GetOuter()->GetPathName()));
							bResult = FALSE;
						}
					}
				}
			}

			CheckIndex--;
		}
	}

	return bResult;
}

void UFindEmitterModuleLODErrorsCommandlet::CheckPackageForModuleLODErrors( const FFilename& Filename, UBOOL bFixup )
{
	UPackage* Package = CastChecked<UPackage>(UObject::LoadPackage(NULL, *Filename, 0));
	if (Package)
	{
		UBOOL bSaveIt = FALSE;

		warnf( NAME_Log, TEXT( "Checking package for module LOD errors: %s" ), *(Package->GetPathName()));
		// Check all ParticleSystems
		for (TObjectIterator<UParticleSystem> It; It; ++It)
		{
			if (It->IsIn(Package))
			{
				UBOOL bIsMismatched = FALSE;
			
				UParticleSystem* PartSys = Cast<UParticleSystem>(*It);

				warnf( NAME_Log, TEXT( "\tChecking particle system: %s" ), *(PartSys->GetPathName()));

				INT EmitterZeroLevelCount = (PartSys->Emitters.Num() > 0) ? PartSys->Emitters(0)->LODLevels.Num() : 0;
				for (INT i = PartSys->Emitters.Num() - 1; (i >= 0) && (!bIsMismatched || bFixup); i--)
				{
					UParticleEmitter* Emitter = PartSys->Emitters(i);
					if (Emitter == NULL)
					{
						continue;
					}

					// For each LOD level, check the validity flags on the modules and
					// verify 'shared' modules are really shared...
					UParticleLODLevel* PrevLOD = NULL;
					UParticleLODLevel* CurrLOD = NULL;

					for (INT LODIndex = 0; LODIndex < Emitter->LODLevels.Num(); LODIndex++)
					{
						CurrLOD = Emitter->LODLevels(LODIndex);
						if (CurrLOD == NULL)
						{
							warnf(NAME_Log, TEXT("\tEmitter %2d, Invalid LOD at %2d in PSys %s"), 
								i, LODIndex, *(PartSys->GetPathName()));
							continue;
						}

						// Check the spawn module
						check(CurrLOD->SpawnModule);
						if (CheckModulesForLODErrors(LODIndex, INDEX_SPAWNMODULE, Emitter, CurrLOD->SpawnModule) == FALSE)
						{
							warnf(NAME_Log, TEXT("\tEmitter %2d, Invalid SpawnModule in LOD %2d: PSys %s"), 
								i, LODIndex, *(PartSys->GetPathName()));
						}

						// Check the required module
						check(CurrLOD->RequiredModule);
						if (CheckModulesForLODErrors(LODIndex, INDEX_REQUIREDMODULE, Emitter, CurrLOD->RequiredModule) == FALSE)
						{
							warnf(NAME_Log, TEXT("\tEmitter %2d, Invalid RequiredModule in LOD %2d: PSys %s"), 
								i, LODIndex, *(PartSys->GetPathName()));
						}

						// Check the type data module (if present)
						if (CurrLOD->TypeDataModule)
						{
							if (PrevLOD)
							{
								if (PrevLOD->TypeDataModule == NULL)
								{
									warnf(NAME_Log, TEXT("\tEmitter %2d, Missing TDM in LOD %2d: PSys %s"), 
										i, PrevLOD->Level, *(PartSys->GetPathName()));
								}

								if (PrevLOD->TypeDataModule->IsA(CurrLOD->TypeDataModule->GetClass()) == FALSE)
								{
									warnf(NAME_Log, TEXT("\tEmitter %2d, Mismatched TDM in LOD %2d: PSys %s"), 
										i, PrevLOD->Level, *(PartSys->GetPathName()));
								}
							}

							if (CheckModulesForLODErrors(LODIndex, INDEX_TYPEDATAMODULE, Emitter, CurrLOD->TypeDataModule) == FALSE)
							{
								warnf(NAME_Log, TEXT("\tEmitter %2d, Invalid TDM in LOD %2d: PSys %s"), 
									i, LODIndex, *(PartSys->GetPathName()));
							}
						}
						else if (PrevLOD && PrevLOD->TypeDataModule)
						{
							warnf(NAME_Log, TEXT("\tEmitter %2d, Missing TDM in LOD %2d: PSys %s"), 
								i, LODIndex, *(PartSys->GetPathName()));
						}

						if (PrevLOD)
						{
							if (PrevLOD->Modules.Num() != CurrLOD->Modules.Num())
							{
								warnf(NAME_Log, TEXT("\tEmitter %2d, Mismatched modules counts in LOD %2d: PSys %s"), 
									i, LODIndex, *(PartSys->GetPathName()));
							}
						}

						// Check the remaining modules
						for (INT ModuleIndex = 0; ModuleIndex < CurrLOD->Modules.Num(); ModuleIndex++)
						{
							UParticleModule* CurrModule = CurrLOD->Modules(ModuleIndex);
							if (PrevLOD)
							{
								if (PrevLOD->Modules.Num() <= ModuleIndex)
								{
									warnf(NAME_Log, TEXT("\tEmitter %2d, Missing module %d in LOD %2d: PSys %s"), 
										i, ModuleIndex, PrevLOD->Level, *(PartSys->GetPathName()));
								}
							}
							if (CheckModulesForLODErrors(LODIndex, ModuleIndex, Emitter, CurrModule) == FALSE)
							{
								warnf(NAME_Log, TEXT("\tEmitter %2d, Invalid module in LOD %2d: PSys %s"), 
									i, LODIndex, *(PartSys->GetPathName()));
							}
						}

						PrevLOD = CurrLOD;
					}
				}
			}
		}

		UObject::CollectGarbage(RF_Native);
	}
}

INT UFindEmitterModuleLODErrorsCommandlet::Main( const FString& Params )
{
	// 
	FString			Token;
	const TCHAR*	CommandLine = appCmdLine();
	UBOOL			bForceCheckAll = FALSE;
	FString			FileToFixup(TEXT(""));
	UBOOL			bFixup = FALSE;
	UBOOL			bProcessMaps = FALSE;

	while (ParseToken(CommandLine, Token, 1))
	{
		if (Token == TEXT("-FIXUP"))
		{
			bFixup = TRUE;
		}
		else
		if (Token == TEXT("-MAPS"))
		{
			bProcessMaps = TRUE;
		}
		else
		if (Token == TEXT("-FORCEALL"))
		{
			bForceCheckAll = TRUE;
		}
		else
		{
			FString PackageName;
			if (GPackageFileCache->FindPackageFile(*Token, NULL, PackageName))
			{
				FileToFixup = FFilename(Token).GetBaseFilename();
			}
			else
			{
				debugf(TEXT("Package %s Not Found."), *Token);
				return -1;
			}
		}
	}


	// Retrieve list of all packages in .ini paths.
	TArray<FString> PackageList = GPackageFileCache->GetPackageFileList();
	if (!PackageList.Num())
		return 0;

	// Determine the list of packages we have to touch...
	TArray<FString> RequiredPackageList;
	RequiredPackageList.Empty(PackageList.Num());

	if (FileToFixup != TEXT(""))
	{
		for (INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++)
		{
			const FFilename& Filename = PackageList(PackageIndex);
			if (appStricmp(*Filename.GetBaseFilename(), *FileToFixup) == 0)
			{
				CheckPackageForModuleLODErrors(Filename, bFixup);
			}
		}
	}
	else
	{
		// Iterate over all packages.
		for (INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++)
		{
			const FFilename& Filename = PackageList(PackageIndex);
			if (Filename.GetExtension() == TEXT("U"))
			{
				warnf(NAME_Log, TEXT("Skipping script file %s"), *Filename);
				continue;
			}
			if (appStristr(*(Filename), TEXT("ShaderCache")) != NULL)
			{
				warnf(NAME_Log, TEXT("Skipping ShaderCache file %s"), *Filename);
				continue;
			}
			if (appStristr(*(Filename.GetPath()), TEXT("Autosave")) != NULL)
			{
				warnf(NAME_Log, TEXT("Skipping Autosave file %s"), *Filename);
				continue;
			}
			// if we don't want to load maps 
			if ((bProcessMaps == FALSE) && (Filename.GetExtension() == FURL::DefaultMapExt))
			{
				warnf(NAME_Log, TEXT("Skipping map file %s"), *Filename);
				continue;
			}

			CheckPackageForModuleLODErrors(Filename, bFixup);
		}
	}
	return 0;
}

IMPLEMENT_CLASS(UFindEmitterModuleLODErrorsCommandlet)

/**
 *	UFixupSourceUVsCommandlet.
 *
 *	Some source UVs in static meshes were corrupted on import - this commandlet attempts to fix them.
 *
 *  The general pattern is the UV coords appear as [0][0*n], [0][1*n] and [0][2*n] when they should be [0][0], [1][0] and [2][0]
 *  I also tried when the UV coords appear as [0][0], [1][1] and [2][0] but this is not fixable
 *
 *  I have tried to write it to not give false positives.
 */
INT UFixupSourceUVsCommandlet::Main( const FString & Params )
{
	// Parse command line args.
	FString			FullyQualified;
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TArray<FString> FilesInPath;
	UBOOL			bResult, bPackageDirty;
	INT				Incorrect, Failed, FixType, i;

	const TCHAR * Parms = *Params;
	ParseCommandLine( Parms, Tokens, Switches );

	if( Tokens.Num() > 0 )
	{
		for( i = 0; i < Tokens.Num(); i++ )
		{
			GPackageFileCache->FindPackageFile( *Tokens( i ), NULL, FullyQualified );
			new( FilesInPath ) FString( FullyQualified );
		}
	}
	else
	{
		FilesInPath = GPackageFileCache->GetPackageFileList();
	}

	if( FilesInPath.Num() == 0 )
	{
		warnf( NAME_Warning, TEXT( "No packages found" ) );
		return 1;
	}

	Incorrect = 0;
	Failed = 0;

	// Iterate over all files checking for incorrect source UVs
	for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
	{
		const FFilename & Filename = FilesInPath( FileIndex );
		warnf( NAME_Log, TEXT( "Loading %s" ), *Filename );
		bPackageDirty = FALSE;

		UPackage * Package = UObject::LoadPackage( NULL, *Filename, LOAD_None );
		if( Package == NULL )
		{
			warnf( NAME_Error, TEXT( "Error loading %s!" ), *Filename );
			continue;
		}

		for( TObjectIterator<UStaticMesh>It; It; ++It )
		{
			UStaticMesh * StaticMesh = *It;
			if( StaticMesh->IsIn( Package ) )
			{
				FStaticMeshRenderData * StaticMeshRenderData = &StaticMesh->LODModels( 0 );
				FStaticMeshTriangle * RawTriangleData = ( FStaticMeshTriangle * )StaticMeshRenderData->RawTriangles.Lock( LOCK_READ_ONLY );

				if( RawTriangleData->NumUVs < 0 || RawTriangleData->NumUVs > 8 )
				{
					warnf( NAME_Warning, TEXT( "Bad number of raw UV channels (%d) in %s" ), RawTriangleData->NumUVs, *StaticMesh->GetName() );
					StaticMeshRenderData->RawTriangles.Unlock();
					continue;	
				}

				bResult = CheckUVs( StaticMeshRenderData, RawTriangleData );
				StaticMeshRenderData->RawTriangles.Unlock();

				if( !bResult )
				{
					warnf( NAME_Log, TEXT( "UV source data incorrect in %s - fixing" ), *StaticMesh->GetName() );
					Incorrect++;

					FStaticMeshTriangle* RawTriangleData = ( FStaticMeshTriangle* )StaticMeshRenderData->RawTriangles.Lock( LOCK_READ_WRITE );
					FixType = CheckFixable( RawTriangleData, StaticMeshRenderData->RawTriangles.GetElementCount() );
					if( FixType < 0 )
					{
						warnf( NAME_Warning, TEXT( "UV source data degenerate or invalid in %s - please reimport" ), *StaticMesh->GetName() );
						Failed++;
					}
					else
					{
						FixupUVs( FixType, RawTriangleData, StaticMeshRenderData->RawTriangles.GetElementCount() );
						bPackageDirty = TRUE;
					}

					StaticMeshRenderData->RawTriangles.Unlock();
				}
			}
		}

		// Write out the package if necessary
		if( bPackageDirty )
		{
			bResult = SavePackageHelper(Package, Filename);

			if( !bResult )
			{
				warnf( NAME_Log, TEXT( "Failed to save: %s" ), *Filename );
			}
		}

		UObject::CollectGarbage( RF_Native );
	}

	warnf( NAME_Log, TEXT( "%d source static mesh(es) with incorrect UV data, %d failed to fix up" ), Incorrect, Failed );
	return( 0 );
}

/**
 *	FixupUVs
 *
 *  Actually remap the UV coords
 */
void UFixupSourceUVsCommandlet::FixupUVs( INT step, FStaticMeshTriangle * RawTriangleData, INT NumRawTriangles )
{
	INT			i, j, k;

	for( i = 0; i < NumRawTriangles; i++ )
	{
		check( RawTriangleData[i].NumUVs < 3 );

		// Copy in the valid UVs
		for( j = 0; j < RawTriangleData[i].NumUVs; j++ )
		{
			for( k = 0; k < 3; k++ )
			{
				RawTriangleData[i].UVs[k][j] = RawTriangleData[i].UVs[0][k * step];
			}
		}

		// Zero out the remainder
		for( j = RawTriangleData[i].NumUVs; j < 8; j++ )
		{
			for( k = 0; k < 3; k++ )
			{
				RawTriangleData[i].UVs[k][j].X = 0.0f;
				RawTriangleData[i].UVs[k][j].Y = 0.0f;
			}
		}
	}
}

/**
 *	ValidateUVChannels
 *
 *  Make sure there are a valid number of UV channels that can be fixed up
 */
UBOOL UFixupSourceUVsCommandlet::ValidateUVChannels( const FStaticMeshTriangle * RawTriangleData, INT NumRawTriangles )
{
	INT			i, UVChannelCount;
	FVector2D	UVsBad[8][3];

	UVChannelCount = RawTriangleData[0].NumUVs;
	check( UVChannelCount > 0 );
	if( UVChannelCount > 2 )
	{
		return( FALSE );
	}

	// Check to make sure all source tris have the same number of UV channels
	for( i = 0; i < NumRawTriangles; i++ )
	{
		if( RawTriangleData[i].NumUVs != UVChannelCount )
		{
			return( FALSE );
		}
	}

	return( TRUE );
}

/**
 *	CheckFixableType
 *
 *  Check to see if the UVs were serialized out incorrectly
 */
UBOOL UFixupSourceUVsCommandlet::CheckFixableType( INT step, const FStaticMeshTriangle * RawTriangleData, INT NumRawTriangles )
{
	INT			i, j, UVChannelCount;
	INT			NullCounts[6];

	UVChannelCount = RawTriangleData[0].NumUVs;
	for( j = 0; j < UVChannelCount * 3; j++ )
	{
		NullCounts[j] = 0;
	}

	for( i = 0; i < NumRawTriangles; i++ )
	{
		// Check to make sure the UVs are all valid
		for( j = 0; j < UVChannelCount * 3; j++ )
		{
			if( appIsNaN( RawTriangleData[i].UVs[0][j * step].X ) || appIsNaN( RawTriangleData[i].UVs[0][j * step].Y ) )
			{
				return( FALSE );
			}

			if( !appIsFinite( RawTriangleData[i].UVs[0][j * step].X ) && !appIsFinite( RawTriangleData[i].UVs[0][j * step].Y ) )
			{
				return( FALSE );
			}

			// Cull 0.0 and #DEN
			if( fabs( RawTriangleData[i].UVs[0][j * step].X ) > SMALL_NUMBER || fabs( RawTriangleData[i].UVs[0][j * step].Y ) > SMALL_NUMBER )
			{
				continue;
			}

			NullCounts[j]++;
		}
	}

	// If any coord is zeroed out more than half the time, it's likley incorrect
	for( j = 0; j < UVChannelCount * 3; j++ )
	{
		if( NullCounts[j] > NumRawTriangles / 2 )
		{
			return( FALSE );
		}
	}

	return( TRUE );
}

/**
 *	CheckFixable
 */
INT UFixupSourceUVsCommandlet::CheckFixable( const FStaticMeshTriangle * RawTriangleData, INT NumRawTriangles )
{
	INT		i;

	if( !ValidateUVChannels( RawTriangleData, NumRawTriangles ) )
	{
		return( -1 );
	}

	for( i = 1; i < 4; i++ )
	{
		if( CheckFixableType( i, RawTriangleData, NumRawTriangles ) )
		{
			return( i );
		}
	}

	return( -1 );
}

/**
 *	FindUV
 *
 *  Find if a UV coord exists anywhere in the source static mesh data
 */
UBOOL UFixupSourceUVsCommandlet::FindUV( const FStaticMeshTriangle * RawTriangleData, INT UVChannel, INT NumRawTriangles, const FVector2D & UV )
{
	INT		i;

	for( i = 0; i < NumRawTriangles; i++ )
	{
		if( RawTriangleData[i].UVs[0][UVChannel] == UV )
		{
			return( TRUE );
		}

		if( RawTriangleData[i].UVs[1][UVChannel] == UV )
		{
			return( TRUE );
		}

		if( RawTriangleData[i].UVs[2][UVChannel] == UV )
		{
			return( TRUE );
		}
	}

	return( FALSE );
}

/**
 *	CheckUVs
 *
 *  Iterate over the final mesh data and check to see if every element of UV data exists somewhere in the source static mesh
 */
UBOOL UFixupSourceUVsCommandlet::CheckUVs( FStaticMeshRenderData * StaticMeshRenderData, const FStaticMeshTriangle * RawTriangleData )
{
	// As the conversion from raw triangles to indexed elements is non trivial and we are checking for a transposition error, just make sure
	// every UV pair in the final data exists somewhere in the raw data.
	INT NumRawTriangles = StaticMeshRenderData->RawTriangles.GetElementCount();

	for( UINT i = 0; i < StaticMeshRenderData->VertexBuffer.GetNumTexCoords(); i++ )
	{
		for( UINT j = 0; j < StaticMeshRenderData->NumVertices; j++ )
		{
			if( !FindUV( RawTriangleData, i, NumRawTriangles, StaticMeshRenderData->VertexBuffer.GetVertexUV(j,i) ) )
			{
				return( FALSE );
			}
		}
	}
	
	return( TRUE );
}

IMPLEMENT_CLASS( UFixupSourceUVsCommandlet );
	
/**
 * Find textures that don't match their MaxLODSize and add them to a shared collection
 *
 * @param Params - the command line arguments used to run the commandlet. A map list is the only expected argument at this time
 *
 * @return 0 - unused
 */
INT UTagSuboptimalTexturesCommandlet::Main(const FString& Params)
{
	// We're a script commandlet that collects garbage, manually put ourselves in the root set.
	AddToRoot();

	// Startup the game asset database so we'll have access to the tags and collections
	{
#if WITH_MANAGED_CODE
		FGameAssetDatabaseStartupConfig StartupConfig;
		FString InitErrorMessageText;
		FGameAssetDatabase::Init(
				StartupConfig,
				InitErrorMessageText );	// Out
		if( InitErrorMessageText.Len() > 0 )
		{
			warnf( NAME_Warning, TEXT( "GameAssetDatabase: %s" ), *InitErrorMessageText );
		}
#else
		warnf( NAME_Warning, TEXT( "GameAssetDatabase not available unless WITH_MANAGED_CODE is defined" ) );
#endif
	}

	// Parse command line args.
	const TCHAR * Parms = *Params;
	TArray<FString> Tokens;
	TArray<FString> Switches;
	ParseCommandLine( Parms, Tokens, Switches );

	TArray<FString> FilesInPath;
	if( Tokens.Num() > 0 )
	{
		for( INT i = 0; i < Tokens.Num(); i++ )
		{
			FString	FullyQualified;
			GPackageFileCache->FindPackageFile( *Tokens( i ), NULL, FullyQualified );
			new( FilesInPath ) FString( FullyQualified );
		}
	}
	else
	{
		FilesInPath = GPackageFileCache->GetPackageFileList();
	}

	if( FilesInPath.Num() == 0 )
	{
		warnf( NAME_Warning, TEXT( "No packages found" ) );
		return 1;
	}

	// Setup our GAD collection
	const FString SuboptimalTexturesCollectionName( TEXT( "Below MaxLODSize" ) );

#if WITH_MANAGED_CODE
	const EGADCollection::Type CollectionType( EGADCollection::Shared );

	if( FGameAssetDatabase::IsInitialized() && !FGameAssetDatabase::Get().IsReadOnly() )
	{
		// Wipe out the previous collections (if they existed at all.)
		FGameAssetDatabase::Get().DestroyCollection( SuboptimalTexturesCollectionName, CollectionType );
	}
#endif

	TArray< FString > AssetsBelowMaxLODSize;

	// Iterate over all files checking for size over MaxLODSize
	for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
	{
		const FFilename & Filename = FilesInPath( FileIndex );
		warnf( NAME_Log, TEXT( "Loading %s" ), *Filename );

		UPackage * Package = UObject::LoadPackage( NULL, *Filename, LOAD_None );
		if( Package == NULL )
		{
			warnf( NAME_Error, TEXT( "Error loading %s!" ), *Filename );
			continue;
		}

		for( TObjectIterator<UTexture2D>It; It; ++It )
		{
			UTexture2D* Texture = *It;

			bool bInGroupToExamine = FALSE;

			// make sure we're in one of the desired texture groups.
			for (int i = 0; i < TextureGroupsToExamine.Num(); i++)
			{
				if (Texture->LODGroup == TextureGroupsToExamine(i))
				{
					bInGroupToExamine = TRUE;
					break;
				}
			}

			if (bInGroupToExamine)
			{
				INT Size = Max(Texture->SizeX, Texture->SizeY);

				if (Size < pow(2.0f, GSystemSettings.TextureLODSettings.GetTextureLODGroup(Texture->LODGroup).MaxLODMipCount))
				{
					warnf(NAME_Log, TEXT( "%s less than MaxLODSize" ), *Texture->GetFullName());
					AssetsBelowMaxLODSize.AddUniqueItem(Texture->GetFullName());
				}
			}
		}

		UObject::CollectGarbage( RF_Native );
	}

#if WITH_MANAGED_CODE
	// Add bad assets to a collection so that artists can find/fix them more easily
	if( FGameAssetDatabase::IsInitialized() && !FGameAssetDatabase::Get().IsReadOnly() )
	{
		if( AssetsBelowMaxLODSize.Num() > 0 )
		{
			warnf(NAME_Log, TEXT( "Writing %s collection to GAD" ), *SuboptimalTexturesCollectionName);
			FGameAssetDatabase::Get().CreateCollection( SuboptimalTexturesCollectionName, CollectionType );
			FGameAssetDatabase::Get().AddAssetsToCollection( SuboptimalTexturesCollectionName, CollectionType, AssetsBelowMaxLODSize );
		}
	}

	// Shutdown the game asset database
	FGameAssetDatabase::Destroy();
#endif

	return 0;
}

IMPLEMENT_CLASS(UTagSuboptimalTexturesCommandlet);

/**
 *	UCheckLightMapUVsCommandlet
 */
INT UCheckLightMapUVsCommandlet::Main( const FString & Params )
{
	// Startup the game asset database so we'll have access to the tags and collections
	{
#if WITH_MANAGED_CODE
		FGameAssetDatabaseStartupConfig StartupConfig;
		FString InitErrorMessageText;
		FGameAssetDatabase::Init(
				StartupConfig,
				InitErrorMessageText );	// Out
		if( InitErrorMessageText.Len() > 0 )
		{
			warnf( NAME_Warning, TEXT( "GameAssetDatabase: %s" ), *InitErrorMessageText );
		}
#else
		warnf( NAME_Warning, TEXT( "GameAssetDatabase not available unless WITH_MANAGED_CODE is defined" ) );
#endif
	}

	// Parse command line args.
	const TCHAR * Parms = *Params;
	TArray<FString> Tokens;
	TArray<FString> Switches;
	ParseCommandLine( Parms, Tokens, Switches );

	TArray<FString> FilesInPath;
	if( Tokens.Num() > 0 )
	{
		for( INT i = 0; i < Tokens.Num(); i++ )
		{
			FString	FullyQualified;
			GPackageFileCache->FindPackageFile( *Tokens( i ), NULL, FullyQualified );
			new( FilesInPath ) FString( FullyQualified );
		}
	}
	else
	{
		FilesInPath = GPackageFileCache->GetPackageFileList();
	}

	if( FilesInPath.Num() == 0 )
	{
		warnf( NAME_Warning, TEXT( "No packages found" ) );
		return 1;
	}

	// Setup our GAD collections
	const FString MissingUVSetsCollectionName( TEXT( "Missing LightMap UVs" ) );
	const FString BadUVSetsCollectionName( TEXT( "Bad LightMap UVs" ) );

#if WITH_MANAGED_CODE
	const EGADCollection::Type CollectionType( EGADCollection::Shared );

	if( FGameAssetDatabase::IsInitialized() && !FGameAssetDatabase::Get().IsReadOnly() )
	{
		if (Switches.ContainsItem(TEXT("CLEARCOLLECTION")) == TRUE)
		{
			// Wipe out the previous collections (if they existed at all.)
			FGameAssetDatabase::Get().DestroyCollection( MissingUVSetsCollectionName, CollectionType );
			FGameAssetDatabase::Get().DestroyCollection( BadUVSetsCollectionName, CollectionType );
		}
		FGameAssetDatabase::Get().CreateCollection( MissingUVSetsCollectionName, CollectionType );
		FGameAssetDatabase::Get().CreateCollection( BadUVSetsCollectionName, CollectionType );
	}

#endif

	TArray< FString > AssetsWithMissingUVSets;
	TArray< FString > AssetsWithBadUVSets;
	TArray< FString > AssetsWithValidUVSets;

	// Iterate over all files checking for incorrect source UVs
	for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
	{
		const FFilename & Filename = FilesInPath( FileIndex );
		warnf( NAME_Log, TEXT( "Loading %s" ), *Filename );
		UBOOL bPackageDirty = FALSE;

		UPackage * Package = UObject::LoadPackage( NULL, *Filename, LOAD_None );
		if( Package == NULL )
		{
			warnf( NAME_Error, TEXT( "Error loading %s!" ), *Filename );
			continue;
		}

		UBOOL bResult = TRUE;

		for( TObjectIterator<UStaticMesh>It; It; ++It )
		{
			UStaticMesh * StaticMesh = *It;
			if( StaticMesh->IsIn( Package ) )
			{
				ProcessStaticMesh( StaticMesh, AssetsWithMissingUVSets, AssetsWithBadUVSets, AssetsWithValidUVSets );
			}
		}

		// Write out the package if necessary
		if( bPackageDirty )
		{
			bResult = SavePackageHelper(Package, Filename);

			if( !bResult )
			{
				warnf( NAME_Log, TEXT( "Failed to save: %s" ), *Filename );
			}
		}

		UObject::CollectGarbage( RF_Native );
	}

	if( (AssetsWithBadUVSets.Num() > 0) || (AssetsWithMissingUVSets.Num() > 0) || (AssetsWithValidUVSets.Num() > 0) )
	{

#if WITH_MANAGED_CODE
		if( FGameAssetDatabase::IsInitialized() && !FGameAssetDatabase::Get().IsReadOnly() )
		{
			TArray<FString> AssetsInCollection;
			FGameAssetDatabase& AssetDatabase = FGameAssetDatabase::Get();

			AssetDatabase.QueryAssetsInCollection(MissingUVSetsCollectionName, CollectionType, AssetsInCollection);
			if(AssetsInCollection.Num() != 0)
			{
				TArray<FString> AddList;
				TArray<FString> RemoveList;

				//find assets that don't exist in the database and add them
				for(INT i = 0; i < AssetsWithMissingUVSets.Num(); ++i)
				{
					const FString& AssetName = AssetsWithMissingUVSets(i);
					if(AssetsInCollection.ContainsItem(AssetName) == FALSE)
					{
						AddList.AddUniqueItem(AssetName);
					}

				}

				//assets that now have valid uv sets should be removed from the database
				for(INT i = 0; i < AssetsWithValidUVSets.Num(); ++i)
				{
					const FString& AssetName = AssetsWithValidUVSets(i);
					if(AssetsInCollection.ContainsItem(AssetName) == TRUE)
					{
						RemoveList.AddUniqueItem(AssetName);
					}
				}

				if(AddList.Num() > 0)
				{
					AssetDatabase.AddAssetsToCollection(MissingUVSetsCollectionName, CollectionType, AddList);
				}

				if(RemoveList.Num() > 0)
				{
					AssetDatabase.RemoveAssetsFromCollection(MissingUVSetsCollectionName, CollectionType, RemoveList);
				}
			}
			else
			{
				//there were no assets in the collection so just add all the new ones
				AssetDatabase.AddAssetsToCollection(MissingUVSetsCollectionName, CollectionType, AssetsWithMissingUVSets);
			}

			AssetsInCollection.Empty();
			AssetDatabase.QueryAssetsInCollection(BadUVSetsCollectionName, CollectionType, AssetsInCollection);
			if(AssetsInCollection.Num() != 0)
			{
				TArray<FString> AddList;
				TArray<FString> RemoveList;

				//find assets that don't exist in the database and add them
				for(INT i = 0; i < AssetsWithBadUVSets.Num(); ++i)
				{
					const FString& AssetName = AssetsWithBadUVSets(i);
					if(AssetsInCollection.ContainsItem(AssetName) == FALSE)
					{
						AddList.AddUniqueItem(AssetName);
					}
				}

				//assets that now have valid uv sets should be removed from the database
				for(INT i = 0; i < AssetsWithValidUVSets.Num(); ++i)
				{
					const FString& AssetName = AssetsWithValidUVSets(i);
					if(AssetsInCollection.ContainsItem(AssetName) == TRUE)
					{
						RemoveList.AddUniqueItem(AssetName);
					}
				}

				if(AddList.Num() > 0)
				{
					AssetDatabase.AddAssetsToCollection(BadUVSetsCollectionName, CollectionType, AddList);
				}

				if(RemoveList.Num() > 0)
				{
					AssetDatabase.RemoveAssetsFromCollection(BadUVSetsCollectionName, CollectionType, RemoveList);
				}
			}
			else
			{
				//there were no assets in the collection so just add all the new ones
				AssetDatabase.AddAssetsToCollection(BadUVSetsCollectionName, CollectionType, AssetsWithBadUVSets);
			}
		}

#else //WITH_MANAGED_CODE
		// print the results out.  The asset database is not available
		debugf(TEXT("Found %d static meshes with missing UV sets"), AssetsWithMissingUVSets.Num());
		for(INT AssetIndex = 0; AssetIndex < AssetsWithMissingUVSets.Num(); ++AssetIndex)
		{
			debugf(TEXT("\t%s"), *(AssetsWithMissingUVSets(AssetIndex)));
		}

		debugf(TEXT("Found %d static meshes with bad UV sets"), AssetsWithBadUVSets.Num());
		for(INT AssetIndex = 0; AssetIndex < AssetsWithBadUVSets.Num(); ++AssetIndex)
		{
			debugf(TEXT("\t%s"), *(AssetsWithBadUVSets(AssetIndex)));
		}
#endif
	}

#if WITH_MANAGED_CODE
	// Shutdown the game asset database
	FGameAssetDatabase::Destroy();
#endif

	return 0;
}


/**
 * Processes the specified static mesh for light map UV problems
 *
 * @param	InStaticMesh					Static mesh to process
 * @param	InOutAssetsWithMissingUVSets	Array of assets that we found with missing UV sets
 * @param	InOutAssetsWithBadUVSets		Array of assets that we found with bad UV sets
 * @param	InOutAssetsWithValidUVSets		Array of assets that we found with valid UV sets
 */
void UCheckLightMapUVsCommandlet::ProcessStaticMesh( UStaticMesh* InStaticMesh, TArray< FString >& InOutAssetsWithMissingUVSets, TArray< FString >& InOutAssetsWithBadUVSets, TArray< FString >& InOutAssetsWithValidUVSets )
{
	UStaticMesh::CheckLightMapUVs( InStaticMesh, InOutAssetsWithMissingUVSets, InOutAssetsWithBadUVSets, InOutAssetsWithValidUVSets );
}


IMPLEMENT_CLASS( UCheckLightMapUVsCommandlet );

/**
 *	UTagCookedReferencedAssetsCommandlet
 */
/**
 *	Initializes the commandlet
 *
 *	@param	Params		The command-line for the app
 *
 *	@return	UBOOL		TRUE if successful, FALSE if not
 */
UBOOL UTagCookedReferencedAssetsCommandlet::Initialize(const FString& Params)
{
	const TCHAR* CmdParams = *Params;
	ParseCommandLine(CmdParams, Tokens, Switches);

	bVerbose = (Switches.FindItemIndex(TEXT("VERBOSE")) != INDEX_NONE);
	bSkipCollection = (Switches.FindItemIndex(TEXT("SKIPCOLLECTION")) != INDEX_NONE);
	bClearCollection = (Switches.FindItemIndex(TEXT("CLEARCOLLECTION")) != INDEX_NONE);

	// There MUST be a switch that identifies the platform
	Platform = ParsePlatformType(CmdParams);
	if (Platform == UE3::PLATFORM_Unknown)
	{
		warnf(NAME_Warning, TEXT("Usage: -PLATFORM=<Xenon/PS3/PC/etc.>"));
		return FALSE;
	}

	// Look for a DLC name to infer user mode for the appropriate platforms
	if ((Platform & UE3::PLATFORM_DLCSupported) != 0)
	{
		Parse(CmdParams, TEXT("DLCName="), DLCName);
		if (DLCName.Len() > 0)
		{
			if (bVerbose == TRUE)
			{
				warnf(NAME_Log, TEXT("Processing DLC %s"), *DLCName);
			}
		}
	}

	// Set compression method based on platform.
	if (((Platform & UE3::PLATFORM_PS3) != 0) || 
		((Platform & UE3::PLATFORM_IPhone) != 0))
	{
		// Zlib uses SPU tasks on PS3.
		GBaseCompressionMethod = COMPRESS_ZLIB;
	}
	else if ((Platform & UE3::PLATFORM_Xbox360) != 0)
	{
		// LZX is best trade-off of perf/ compression size on Xbox 360
		GBaseCompressionMethod = COMPRESS_LZX;
	}
	// Other platforms default to what the PC uses by default.

	// Generate the content directory we are interested in
	if (appCookedContentPath(Platform, DLCName, CookedDataPath) == FALSE)
	{
		warnf(NAME_Warning, TEXT("UTagCookedReferencedAssetsCommandlet: Failed to get CookedData path"));
		return FALSE;
	}

	if (bVerbose == TRUE)
	{
		warnf(NAME_Log, TEXT("Running on %s"), *CookedDataPath);
	}

	FString PlatformString = appPlatformTypeToString(Platform);
	// Setup our GAD collection name
	if (DLCName.Len() > 0)
	{
		CollectionName = TEXT("InDLC");
		CollectionName += DLCName;
	}
	else
	{
		CollectionName = TEXT("OnDVD");
	}
	CollectionName += TEXT("(");
	CollectionName += PlatformString;
	CollectionName += TEXT(")");
	if (bVerbose == TRUE)
	{
		warnf(NAME_Log, TEXT("Collection name = %s"), *CollectionName);
	}

	// Setup the class rejection list...
	//@todo. Move this to an ini file?
	ClassesToSkip.AddItem(TEXT("Class"));
	ClassesToSkip.AddItem(TEXT("ArrayProperty"));
	ClassesToSkip.AddItem(TEXT("BoolProperty"));
	ClassesToSkip.AddItem(TEXT("ByteProperty"));
	ClassesToSkip.AddItem(TEXT("ClassProperty"));
	ClassesToSkip.AddItem(TEXT("ComponentProperty"));
	ClassesToSkip.AddItem(TEXT("FloatProperty"));
	ClassesToSkip.AddItem(TEXT("InterfaceProperty"));
	ClassesToSkip.AddItem(TEXT("IntProperty"));
	ClassesToSkip.AddItem(TEXT("NameProperty"));
	ClassesToSkip.AddItem(TEXT("ObjectProperty"));
	ClassesToSkip.AddItem(TEXT("StrProperty"));
	ClassesToSkip.AddItem(TEXT("StructProperty"));
	ClassesToSkip.AddItem(TEXT("Const"));
	ClassesToSkip.AddItem(TEXT("Enum"));
	ClassesToSkip.AddItem(TEXT("Function"));
	ClassesToSkip.AddItem(TEXT("MetaData"));
	ClassesToSkip.AddItem(TEXT("ScriptStruct"));
	ClassesToSkip.AddItem(TEXT("TextBuffer"));
	ClassesToSkip.AddItem(TEXT("Package"));
	ClassesToSkip.AddItem(TEXT("ShaderCache"));
	ClassesToSkip.AddItem(TEXT("ObjectReferencer"));
	ClassesToSkip.AddItem(TEXT("ObjectRedirector"));
	ClassesToSkip.AddItem(TEXT("GUDBank"));
	ClassesToSkip.AddItem(TEXT("GUDToC"));
	ClassesToSkip.AddItem(TEXT("GuidCache"));
	ClassesToSkip.AddItem(TEXT("World"));
	ClassesToSkip.AddItem(TEXT("LightMapTexture2D"));
	ClassesToSkip.AddItem(TEXT("ShadowMapTexture2D"));

	return TRUE;
}

/**
 *	Generate the list of referenced assets for the given platform
 *	This will utilize the contents of the Cooked<PLATFORM> folder
 */
void UTagCookedReferencedAssetsCommandlet::GenerateReferencedAssetsList()
{
	// Gather the list of items in the cooked folder
	TArray<FString> CookedFiles;
	appFindFilesInDirectory(CookedFiles, *CookedDataPath, TRUE, TRUE);

	for (INT FileIndex = 0; FileIndex < CookedFiles.Num(); FileIndex++)
	{
		FFilename Filename = CookedFiles(FileIndex); 
		if ((Filename.GetExtension() != TEXT("xxx")) &&
			(Filename.GetExtension() != TEXT("upk")))
		{
			if (bVerbose == TRUE)
			{
				warnf(NAME_Log, TEXT("\tSkipping file %s"), *Filename);
			}
			continue;
		}

		ProcessFile(Filename);

		// only GC every 10 packages (A LOT faster this way, and is safe, since we are not 
		// acting on objects that would need to go away or anything)
		if (((FileIndex + 1) % 10) == 0)
		{
			UObject::CollectGarbage(RF_Native);
		}
	}
}

/**
 *	Process the given file, adding it's assets to the list
 *
 *	@param	InFilename		The name of the file to process
 *
 */
void UTagCookedReferencedAssetsCommandlet::ProcessFile(const FFilename& InFilename)
{
	{
		// reset the loaders for the packages we want to load so that we don't find the wrong version of the file
		// (otherwise, attempting to run pkginfo on e.g. Engine.xxx will always return results for Engine.u instead)
		const FString& PackageName = FPackageFileCache::PackageFromPath(*InFilename);
		UPackage* ExistingPackage = FindObject<UPackage>(NULL, *PackageName, TRUE);
		if (ExistingPackage != NULL)
		{
			ResetLoaders(ExistingPackage);
		}
	}

	UObject::BeginLoad();
	ULinkerLoad* Linker = UObject::GetPackageLinker(NULL, *InFilename, LOAD_Quiet|LOAD_NoWarn|LOAD_NoVerify, NULL, NULL);
	UObject::EndLoad();

	if (Linker != NULL)
	{
		if (bVerbose == TRUE)
		{
			warnf(NAME_Log, TEXT("Loaded linker for package: %s"), *InFilename);
		}

		// Walk the export map of the linker. This commandlet assumes SEEKFREE cooking!
		for (INT ExportIdx = 0; ExportIdx < Linker->ExportMap.Num(); ExportIdx++)
		{
			FString Name = Linker->GetExportFullName(ExportIdx);
			// Skip any Default__ objects...
			if (Name.InStr(TEXT("Default__"), FALSE, TRUE) == INDEX_NONE)
			{
				FName ClassName = Linker->GetExportClassName(ExportIdx);
				// Skip classes, etc. that we know shouldn't go into the list
				if (ClassesToSkip.FindItemIndex(ClassName) == INDEX_NONE)
				{
					FString FullName = Linker->GetExportFullName(ExportIdx, NULL, TRUE);
					// Skip 'sub-object' items
					if (FullName.InStr(TEXT(":")) == INDEX_NONE)
					{
						if (bVerbose == TRUE)
						{
							warnf(NAME_Log, TEXT("\t%s"), *FullName);
						}

						AssetAddList.Set(FullName, TRUE);
					}
				}
			}
		}
	}
	else
	{
		warnf(NAME_Warning, TEXT("Failed to get linker for package: %s"), *InFilename);
	}
}

/**
 *	Update the collections
 */
void UTagCookedReferencedAssetsCommandlet::UpdateCollections()
{
	TArray<FString> AssetsToAdd;

	for (TMap<FString,UBOOL>::TIterator It(AssetAddList); It; ++It)
	{
		// We don't need to add unique items as that was the point
		// of using the TMap when gathering the assets!
		AssetsToAdd.AddItem(It.Key());
	}

	if (AssetsToAdd.Num() > 0)
	{
		if (bSkipCollection == FALSE)
		{
			FGADHelper* GADHelper = new FGADHelper();
			if ((GADHelper == NULL) || (GADHelper->Initialize() == FALSE))
			{
				warnf(NAME_Warning, TEXT("UTagCookedReferencedAssetsCommandlet: Failed to initialize GAD!"));
			}
			else
			{
				if (bClearCollection == TRUE)
				{
					GADHelper->ClearCollection(CollectionName, EGADCollection::Shared);
				}
				GADHelper->SetCollection(CollectionName, EGADCollection::Shared, AssetsToAdd);

				// NOTE: "Audit" is the hard-coded tag group for these auto-generated tag names.  If you
				// change this or add other hard-coded groups, you should also update the GAD checkpoint
				// commandlet so that these utility tags are stripped out of UDK builds
				GADHelper->SetTaggedAssets(FString(TEXT("Audit."))+CollectionName, AssetsToAdd); // also tag them so we can use filtering and AND type inclusion
			}
			delete GADHelper;
			GADHelper = NULL;
		}
		else
		{
			// Just log them out
			debugf(TEXT("Found %d assets to add to '%s':"), AssetsToAdd.Num(), *CollectionName);
			for (INT DumpIdx = 0; DumpIdx < AssetsToAdd.Num(); DumpIdx++)
			{
				debugf(TEXT("\t%s"), *(AssetsToAdd(DumpIdx)));
			}

			UBOOL bWriteOutCSV = (Switches.FindItemIndex(TEXT("WRITECSV")) != INDEX_NONE);
			if (bWriteOutCSV == TRUE)
			{
				// Create string with system time to create a unique filename.
				INT Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec;
				appSystemTime( Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec );
				FString	CurrentTime = FString::Printf(TEXT("%i.%02i.%02i-%02i.%02i.%02i"), Year, Month, Day, Hour, Min, Sec );
				FString CSVFilename = FString::Printf(TEXT("%sCookedAssets-%s.csv"), *appGameLogDir(), *CurrentTime);
				FArchive* CSVFile = GFileManager->CreateFileWriter(*CSVFilename);
				if (!CSVFile)
				{
					warnf(NAME_Error, TEXT("Failed to open output file %s"), *CSVFilename);
				}
				else
				{
					for (INT DumpIdx = 0; DumpIdx < AssetsToAdd.Num(); DumpIdx++)
					{
						FString CSVLine = FString::Printf(TEXT("%s%s"), *(AssetsToAdd(DumpIdx)), LINE_TERMINATOR);
						CSVFile->Serialize(TCHAR_TO_ANSI(*CSVLine), CSVLine.Len());
					}
					CSVFile->Close();
					delete CSVFile;
				}
			}
		}
	}
}

/**
 *  Main
 */
INT UTagCookedReferencedAssetsCommandlet::Main(const FString & Params)
{
	// Parse command line args.
	if (Initialize(Params) == TRUE)
	{
		GenerateReferencedAssetsList();
		UpdateCollections();
	}
	else
	{
		warnf(NAME_Warning, TEXT("Failed to initialize..."));
		return -1;
	}
	return 0;
}
IMPLEMENT_CLASS(UTagCookedReferencedAssetsCommandlet);

/**
 *	UTagReferencedAssetsCommandlet
 */
INT UTagReferencedAssetsCommandlet::Main( const FString & Params )
{
#if !WITH_MANAGED_CODE
	warnf( NAME_Warning, TEXT( "GameAssetDatabase not available unless WITH_MANAGED_CODE is defined" ) );
	return -1;
#else
	
	// Setup our GAD collections
	const FString UnreferencedAssetsCollectionName( TEXT( "Unreferenced Assets" ) );
	const EGADCollection::Type CollectionType( EGADCollection::Shared );

	// Startup the game asset database so we'll have access to the tags and collections
	{
		FGameAssetDatabaseStartupConfig StartupConfig;
		FString InitErrorMessageText;
		FGameAssetDatabase::Init(
				StartupConfig,
				InitErrorMessageText );	// Out
		if( InitErrorMessageText.Len() > 0 )
		{
			warnf( NAME_Warning, TEXT( "GameAssetDatabase: %s" ), *InitErrorMessageText );
			return -1;
		}

		if( !FGameAssetDatabase::IsInitialized() || FGameAssetDatabase::Get().IsReadOnly() )
		{
			warnf( NAME_Warning, TEXT("Game Asset Database not initialized or read-only.") );
			return -1;
		}
	}

	// Parse command line args.
	const TCHAR * Parms = *Params;
	TArray<FString> Tokens;
	TArray<FString> Switches;
	ParseCommandLine( Parms, Tokens, Switches );

	TArray<FString> FilesInPath;
	if( Tokens.Num() > 0 )
	{
		for( INT i = 0; i < Tokens.Num(); i++ )
		{
			FString	FullyQualified;
			GPackageFileCache->FindPackageFile( *Tokens( i ), NULL, FullyQualified );
			new( FilesInPath ) FString( FullyQualified );
		}
	}
	else
	{
		FilesInPath = GPackageFileCache->GetPackageFileList();
	}

	if( FilesInPath.Num() == 0 )
	{
		warnf( NAME_Warning, TEXT( "No packages found" ) );
		return 1;
	}

	// Make sure we load all script packages.
	TArray<FString> AllScriptPackageNames;
	appGetScriptPackageNames(AllScriptPackageNames, SPT_AllScript, FALSE);
	for( INT PackageIndex=0; PackageIndex<AllScriptPackageNames.Num(); PackageIndex++ )
	{
		LoadPackage( NULL, *AllScriptPackageNames(PackageIndex), LOAD_NoVerify | LOAD_NoWarn );
	}

	// Iterate over all map files, looking for referenced assets.
	TSet<FString> ReferencedAssets;

	//// Allow for catching script ref'd objects
	for (TObjectIterator<UObject> It; It; ++It)
	{
		FString ObjFullName = It->GetFullName();
		if (ObjFullName.InStr(TEXT(":")) == INDEX_NONE)
		{
			ReferencedAssets.Add(ObjFullName);
		}
	}
	UObject::CollectGarbage(RF_Native);

	INT FileCount = FilesInPath.Num();
	DOUBLE ImpExpStartTime = appSeconds();
	for( INT FileIndex = 0; FileIndex < FileCount; FileIndex++ )
	{
		const FFilename & Filename = FilesInPath( FileIndex );

		// Get the package linker to see whether we have a map file. We can't go by the extension as
		// e.g. the entry level in Engine has a .upkg extension.
		UObject::BeginLoad();
		ULinkerLoad* Linker = UObject::GetPackageLinker(NULL,*Filename,LOAD_NoVerify,NULL,NULL);
		UObject::EndLoad();

		// We only care about content referenced by map packages.
		if (Linker && ((Linker->Summary.PackageFlags & PKG_ContainsMap) == PKG_ContainsMap))
		{
			warnf( NAME_Log, TEXT( "Loading %4d of %4d: %s" ), FileIndex, FileCount, *Filename );
			for (INT ExpIdx = 0; ExpIdx < Linker->ExportMap.Num(); ExpIdx++)
			{
				FString ObjFullName = Linker->GetExportFullName(ExpIdx);
				if (ObjFullName.InStr(TEXT(":")) == INDEX_NONE)
				{
					ReferencedAssets.Add(ObjFullName);
					if (ExpIdx < Linker->DependsMap.Num())
					{
						TArray<INT>& Depends = Linker->DependsMap(ExpIdx);
						if ((Linker->Summary.PackageFlags & PKG_Cooked) == 0)
						{
							TSet<FDependencyRef> AllDepends;
							Linker->GatherExportDependencies(ExpIdx, AllDepends);
							INT DependsIndex = 0;
							for (TSet<FDependencyRef>::TConstIterator It(AllDepends); It; ++It)
							{
								const FDependencyRef& Ref = *It;
								ObjFullName = Ref.Linker->GetExportFullName(Ref.ExportIndex);
								ReferencedAssets.Add(ObjFullName);
							}
						}
					}
				}
			}
		}
		if (((FileIndex + 1) % 10) == 0)
		{
			UObject::CollectGarbage( RF_Native );
		}
	}
	DOUBLE ImpExpTime = appSeconds() - ImpExpStartTime;
	warnf(NAME_Log, TEXT("Found %8d referenced assets in %8.5f seconds."), ReferencedAssets.Num(), ImpExpTime);

	// Retrieve list of all assets, used to find unreferenced ones.
	TArray<FString> UnreferencedAssets;
	TLookupMap<FName> AssetFullNamesLookupMap;
	FGameAssetDatabase::Get().QueryAllAssets( AssetFullNamesLookupMap );
	const TArray<FName>& AssetFullNames = AssetFullNamesLookupMap.GetUniqueElements();

	// Unreferenced assets are assets in the GAD that are NOT in the referenced asset set.
	for( INT AssetIndex=0; AssetIndex<AssetFullNames.Num(); AssetIndex++ )
	{
		FString AssetName = AssetFullNames(AssetIndex).ToString();
		if (ReferencedAssets.Contains(AssetName) == FALSE)
		{
			UnreferencedAssets.AddItem( AssetName );
		}
	}

	FGameAssetDatabase::Get().CreateCollection( UnreferencedAssetsCollectionName, CollectionType );

	// Build lists of assets to add and remove from the collection.
	TArray<FString> AddList;
	TArray<FString> RemoveList;
	TArray<FString> AssetsInCollection;

	FGameAssetDatabase::Get().QueryAssetsInCollection(UnreferencedAssetsCollectionName, CollectionType, AssetsInCollection);

	if (AssetsInCollection.Num() != 0)
	{
		//build the add list, only adding assets not already in the collection
		for (INT i = 0; i < UnreferencedAssets.Num(); ++i)
		{
			const FString& AssetName = UnreferencedAssets(i);
			if (AssetsInCollection.ContainsItem(AssetName) == FALSE)
			{
				AddList.AddUniqueItem(AssetName);
			}
		}

		//build the remove list, removing assets from the collection that are now referenced
		for (INT i = 0; i < AssetsInCollection.Num(); ++i)
		{
			const FString& AssetName = AssetsInCollection(i);
			if (ReferencedAssets.Contains(AssetName) == TRUE)
			{
				RemoveList.AddUniqueItem(AssetName);
			}
		}

		// Add bad assets to a collection so that artists can find/fix them more easily
		if (AddList.Num() > 0)
		{
			FGameAssetDatabase::Get().AddAssetsToCollection(UnreferencedAssetsCollectionName, CollectionType, AddList);
		}

		// Remove referenced assets from the 
		if (RemoveList.Num() > 0)
		{
			FGameAssetDatabase::Get().RemoveAssetsFromCollection(UnreferencedAssetsCollectionName, CollectionType, RemoveList);
		}
	}
	else
	{
		// there were no assets in the collection so just add them all
		FGameAssetDatabase::Get().AddAssetsToCollection(UnreferencedAssetsCollectionName, CollectionType, UnreferencedAssets);
	}


	// Shutdown the game asset database
	FGameAssetDatabase::Destroy();
	return 0;
#endif
}

IMPLEMENT_CLASS( UTagReferencedAssetsCommandlet );


/**
 *	UPIEToNormalCommandlet.
 *
 *	Take a map saved for PIE and make it openable in the editor
 */
INT UPIEToNormalCommandlet::Main( const FString & Params )
{
	// parse the command line
	TArray<FString> Tokens;
	TArray<FString> Switches;
	const TCHAR * Parms = *Params;
	ParseCommandLine( Parms, Tokens, Switches );

	// validate the input
	FString MapPath;
	FString OutputPath;
	if (Tokens.Num() > 0)
	{
		GPackageFileCache->FindPackageFile(*Tokens(0), NULL, MapPath);
	}
	else
	{
		warnf(TEXT("No map specified:\n\tPIEToNormal InputPIEMap [OptionalOutputPath]\n\tNote: If no output specified, the input will be saved over."));
		return 1;
	}

	// get where we want to write to
	if (Tokens.Num() > 1)
	{
		OutputPath = Tokens(1);
	}
	else
	{
		OutputPath = MapPath;
	}

	UPackage* Package = UObject::LoadPackage(NULL, *MapPath, 0);
	if (!Package)
	{
		warnf(TEXT("File %s not found"), *MapPath);
		return 1;
	}

	// make sure there's a world in it (ie a map)
	UWorld* World = FindObject<UWorld>(Package, TEXT("TheWorld"));
	if (World == NULL || (Package->PackageFlags & PKG_PlayInEditor) == 0)
	{
		warnf(TEXT("Package %s was not a map saved via Play In Editor"), *MapPath);
		return 1;
	}

	// first, strip PIE flag
	Package->PackageFlags &= ~PKG_PlayInEditor;

	// next, undo the PIE changes to streaming levels
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
	for (INT LevelIndex=0; LevelIndex<WorldInfo->StreamingLevels.Num(); LevelIndex++)
	{
		ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
		// is it a used streaming level?
		if (StreamingLevel && StreamingLevel->PackageName != NAME_None)
		{
			// cache the package name
			FString StreamingPackageName = StreamingLevel->PackageName.ToString();
			
			// it needs to start with the playworld prefix
			check(StreamingPackageName.StartsWith(PLAYWORLD_PACKAGE_PREFIX));

			// strip off the PIE prefix
			StreamingLevel->PackageName = FName(*StreamingPackageName.Right(StreamingPackageName.Len() - appStrlen(PLAYWORLD_PACKAGE_PREFIX)));
		}
	}


	// delete the play from here teleporter
	for (TObjectIterator<ATeleporter> It; It; ++It)
	{
		// is it the special teleporter?
		if (It->Tag == TEXT("PlayWorldStart"))
		{
			// if so, remove it from nav list and destroy it
			World->PersistentLevel->RemoveFromNavList(*It);
			World->DestroyActor(*It);
			break;
		}
	}

	// save the result
	if (!SavePackageHelper(Package, OutputPath))
	{
		warnf(TEXT("Failed to save to output pacakge %s"), *OutputPath);
	}
	

	return 0;
}
IMPLEMENT_CLASS(UPIEToNormalCommandlet);

/*-----------------------------------------------------------------------------
	UWrangleContent.
-----------------------------------------------------------------------------*/

/** 
 * Helper struct to store information about a unreferenced object
 */
struct FUnreferencedObject
{
	/** Name of package this object resides in */
	FString PackageName;
	/** Full name of object */
	FString ObjectName;
	/** Size on disk as recorded in FObjectExport */
	INT SerialSize;

	/**
	 * Constructor for easy creation in a TArray
	 */
	FUnreferencedObject(const FString& InPackageName, const FString& InObjectName, INT InSerialSize)
	: PackageName(InPackageName)
	, ObjectName(InObjectName)
	, SerialSize(InSerialSize)
	{
	}
};

/**
 * Helper struct to store information about referenced objects insde
 * a package. Stored in TMap<> by package name, so this doesn't need
 * to store the package name 
 */
struct FPackageObjects
{
	/** All objected referenced in this package, and their class */
	TMap<FString, UClass*> ReferencedObjects;

	/** Was this package a fully loaded package, and saved right after being loaded? */
	UBOOL bIsFullyLoadedPackage;

	FPackageObjects()
	: bIsFullyLoadedPackage(FALSE)
	{
	}

};
	
FArchive& operator<<(FArchive& Ar, FPackageObjects& PackageObjects)
{
	Ar << PackageObjects.bIsFullyLoadedPackage;

	if (Ar.IsLoading())
	{
		INT NumObjects;
		FString ObjectName;
		FString ClassName;

		Ar << NumObjects;
		for (INT ObjIndex = 0; ObjIndex < NumObjects; ObjIndex++)
		{
			Ar << ObjectName << ClassName;
			UClass* Class = UObject::StaticLoadClass(UObject::StaticClass(), NULL, *ClassName, NULL, LOAD_None, NULL);
			PackageObjects.ReferencedObjects.Set(*ObjectName, Class);
		}
	}
	else if (Ar.IsSaving())
	{
		INT NumObjects = PackageObjects.ReferencedObjects.Num();
		Ar << NumObjects;
		for (TMap<FString, UClass*>::TIterator It(PackageObjects.ReferencedObjects); It; ++It)
		{
			FString ObjectName, ClassName;
			ObjectName = It.Key();
			ClassName = It.Value()->GetPathName();

			Ar << ObjectName << ClassName;
		}
		
	}

	return Ar;
}

/**
 * Stores the fact that an object (given just a name) was referenced
 *
 * @param PackageName Name of the package the object lives in
 * @param ObjectName FullName of the object
 * @param ObjectClass Class of the object
 * @param ObjectRefs Map to store the object information in
 * @param bIsFullLoadedPackage TRUE if the packge this object is in was fully loaded
 */
void ReferenceObjectInner(const FString& PackageName, const FString& ObjectName, UClass* ObjectClass, TMap<FString, FPackageObjects>& ObjectRefs, UBOOL bIsFullyLoadedPackage)
{
	// look for an existing FPackageObjects
	FPackageObjects* PackageObjs = ObjectRefs.Find(*PackageName);
	// if it wasn't found make a new entry in the map
	if (PackageObjs == NULL)
	{
		PackageObjs = &ObjectRefs.Set(*PackageName, FPackageObjects());
	}

	// if either the package was already marked as fully loaded or it now is fully loaded, then
	// it will be fully loaded
	PackageObjs->bIsFullyLoadedPackage = PackageObjs->bIsFullyLoadedPackage || bIsFullyLoadedPackage;

	// add this referenced object to the map
	PackageObjs->ReferencedObjects.Set(*ObjectName, ObjectClass);

	// make sure the class is in the root set so it doesn't get GC'd, making the pointer we cached invalid
	ObjectClass->AddToRoot();
}

/**
 * Stores the fact that an object was referenced
 *
 * @param Object The object that was referenced
 * @param ObjectRefs Map to store the object information in
 * @param bIsFullLoadedPackage TRUE if the package this object is in was fully loaded
 */
void ReferenceObject(UObject* Object, TMap<FString, FPackageObjects>& ObjectRefs, UBOOL bIsFullyLoadedPackage)
{
	FString PackageName = Object->GetOutermost()->GetName();

	// find the outermost non-upackage object, as it will be loaded later with all its subobjects
	while (Object->GetOuter() && Object->GetOuter()->GetClass() != UPackage::StaticClass())
	{
		Object = Object->GetOuter();
	}

	// make sure this object is valid (it's not in a script or native-only package)
	UBOOL bIsValid = TRUE;
	// can't be in a script packge or be a field/template in a native package, or a top level pacakge, or in the transient package
	if ((Object->GetOutermost()->PackageFlags & PKG_ContainsScript) ||
		Object->IsA(UField::StaticClass()) ||
		Object->IsTemplate(RF_ClassDefaultObject) ||
		Object->GetOuter() == NULL ||
		Object->IsIn(UObject::GetTransientPackage()))
	{
		bIsValid = FALSE;
	}

	if (bIsValid)
	{
		// save the reference
		ReferenceObjectInner(PackageName, Object->GetFullName(), Object->GetClass(), ObjectRefs, bIsFullyLoadedPackage);

		// Get a list of known language extensions
		const TArray<FString>& KnownLanguageExtensions = appGetKnownLanguageExtensions();

		// add the other language versions
	    for (INT LangIndex = 0; LangIndex < KnownLanguageExtensions.Num(); LangIndex++)
		{
			// see if a localized package for this package exists
			FString LocPath;
			FString LocPackageName = PackageName + TEXT("_") + KnownLanguageExtensions(LangIndex);
			if (GPackageFileCache->FindPackageFile(*LocPackageName, NULL, LocPath))
			{
				// make the localized object name (it doesn't even have to exist)
				FString LocObjectName = FString::Printf(TEXT("%s %s.%s"), *Object->GetClass()->GetName(), *LocPackageName, *Object->GetPathName(Object->GetOutermost()));

				// save a reference to the (possibly existing) localized object (these won't have been in a fully loaded package)
				ReferenceObjectInner(LocPackageName, LocObjectName, Object->GetClass(), ObjectRefs, FALSE);
			}
		}
	}
}

/**
 * Take a package pathname and return a path for where to save the cutdown
 * version of the package. Will create the directory if needed.
 *
 * @param Filename Path to a package file
 * @param CutdownDirectoryName Name of the directory to put this package into
 *
 * @return Location to save the cutdown package
 */
FFilename MakeCutdownFilename(const FFilename& Filename, const TCHAR* CutdownDirectoryName=TEXT("CutdownPackages"))
{
	// replace the .. with ..\GAMENAME\CutdownContent
	FFilename CutdownDirectory = Filename.GetPath();
	CutdownDirectory = CutdownDirectory.Replace(TEXT("..\\..\\"), *FString::Printf(TEXT("..\\..\\%sGame\\%s\\"), appGetGameName(), CutdownDirectoryName));

	// make sure it exists
	GFileManager->MakeDirectory(*CutdownDirectory, TRUE);

	// return the full pathname
	return CutdownDirectory * Filename.GetCleanFilename();
}

/**
 * Removes editor-only data for a package
 *
 * @param Package The package to remove editor date from all objects
 * @param PlatformsToKeep A bitfield of platforms that we need to keep the platform-specific data for
 * @param bStripLargeEditorData If TRUE, editor-only, but large enough to bloat download sizes, will be removed
 * @param bStripMips If TRUE, mips larger than the largest specified in SystemSettings will be removed
 */
void WrangleAwayData(UPackage* Package, UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData, UBOOL bStripMips)
{
	for( TObjectIterator<UObject> It; It; ++It )
	{
		if (It->IsIn(Package))
		{
			// let each object decide how to strip itself out
			It->StripData(PlatformsToKeep, bStripLargeEditorData);

			// strip conosle compressed cached data if desired
			UTexture2D* Texture2D = Cast<UTexture2D>(*It);
			if (bStripMips && Texture2D != NULL)
			{
				// Number of mips in texture.
				INT		NumMips				= Texture2D->Mips.Num();								
				// Index of first mip, taking into account LOD bias and settings.
				INT		FirstMipIndex		= Clamp<INT>( GSystemSettings.TextureLODSettings.CalculateLODBias( Texture2D ), 0, NumMips-1 );

				// make sure we load at least the first packed mip level
				FirstMipIndex = Min(FirstMipIndex, Texture2D->MipTailBaseIdx);
				
				// remove any mips that are bigger than the desired biggest size
				if (FirstMipIndex > 0)
				{
					Texture2D->Mips.Remove(0, FirstMipIndex);
					Texture2D->SizeX = Max(Texture2D->SizeX >> FirstMipIndex,1);
					Texture2D->SizeY = Max(Texture2D->SizeY >> FirstMipIndex,1);
					Texture2D->LODBias = 0;
				}

				// Strip out miplevels for UI textures.
				if( Texture2D->LODGroup == TEXTUREGROUP_UI )
				{
					if( Texture2D->Mips.Num() > 1 )
					{
						Texture2D->Mips.Remove( 1, Texture2D->Mips.Num() - 1 );
					}
				}
			}
		}
	}

	// mark the package as dirty
	Package->SetDirtyFlag(TRUE);
}

INT UWrangleContentCommandlet::Main( const FString& Params )
{
	// overall commandlet control options
	UBOOL bShouldRestoreFromPreviousRun = ParseParam(*Params, TEXT("restore"));
	UBOOL bShouldSavePackages = !ParseParam(*Params, TEXT("nosave"));
	UBOOL bShouldSaveUnreferencedContent = !ParseParam(*Params, TEXT("nosaveunreferenced"));
	UBOOL bShouldDumpUnreferencedContent = ParseParam(*Params, TEXT("reportunreferenced"));
	UBOOL bShouldCleanOldDirectories = !ParseParam(*Params, TEXT("noclean"));
	UBOOL bShouldSkipMissingClasses = ParseParam(*Params, TEXT("skipMissingClasses"));

	// what per-object stripping to perform
	UBOOL bShouldStripLargeEditorData = ParseParam(*Params, TEXT("striplargeeditordata"));
	UBOOL bShouldStripMips = ParseParam(*Params, TEXT("stripmips"));

	// package loading options
	UBOOL bShouldLoadAllMaps = ParseParam(*Params, TEXT("allmaps"));
	
	// figure out which exact platforms to keep around
	UINT PlatformsToKeep;

	// Get all supported map extensions
	FString DefaultMapExt;
	TSet<FString> SupportedMapExtensions;
	if( !GConfig->GetString( TEXT("URL"), TEXT("MapExt"), DefaultMapExt, GEngineIni ) )
	{
		appErrorf(*LocalizeUnrealEd("Error_MapExtUndefined"));
	}

	SupportedMapExtensions.Add( DefaultMapExt );

	FString AdditionalMapExt;
	if( GConfig->GetString( TEXT("URL"), TEXT("AdditionalMapExt"), AdditionalMapExt, GEngineIni ) )
	{
		SupportedMapExtensions.Add( AdditionalMapExt );
	}

	// if the user specified to keep only certain platforms, then strip everything else
	FString KeepPlatformsString;
	if (!Parse(*Params, TEXT("PlatformsToKeep="), KeepPlatformsString, FALSE))
	{
		// if no platforms specified, keep them all
		PlatformsToKeep = 0xFFFFFFFF;
		warnf(TEXT("Keeping platform-specific data for ALL platforms"));
	}
	else
	{
		// if specified, start with none
		PlatformsToKeep = 0;

		// parse the platforms string into
		TArray<FString> Platforms;
		KeepPlatformsString.ParseIntoArray(&Platforms, TEXT(","), TRUE);

		warnf(TEXT("Keeping platform-specific data for:"));
		for (INT PlatformIndex = 0; PlatformIndex < Platforms.Num(); PlatformIndex++)
		{
			const FString& PlatformStr = Platforms(PlatformIndex);

			UE3::EPlatformType Platform = appPlatformStringToType(PlatformStr);
			PlatformsToKeep |= Platform;

			warnf(TEXT(" %s"), *appPlatformTypeToString(Platform));
		}
	}

	FString SectionStr;
	Parse( *Params, TEXT( "SECTION=" ), SectionStr );

	// store all referenced objects
	TMap<FString, FPackageObjects> AllReferencedPublicObjects;

	if (bShouldRestoreFromPreviousRun)
	{
		FArchive* Ar = GFileManager->CreateFileReader(*(appGameDir() + TEXT("Wrangle.bin")));
		if( Ar != NULL )
		{
			*Ar << AllReferencedPublicObjects;
			delete Ar;
		}
		else
		{
			warnf(TEXT("Could not read in Wrangle.bin so not restoring and doing a full wrangle") );
		}
	}
	else
	{
		// make name for our ini file to control loading
		FString WrangleContentIniName =	appGameConfigDir() + TEXT("WrangleContent.ini");

		// figure out which section to use to get the packages to fully load
		FString SectionToUse = TEXT("WrangleContent.PackagesToFullyLoad");
		if( SectionStr.Len() > 0 )
		{
			SectionToUse = FString::Printf( TEXT( "WrangleContent.%sPackagesToFullyLoad" ), *SectionStr );
		}

		// get a list of packages to load
		const FConfigSection* PackagesToFullyLoadSection = GConfig->GetSectionPrivate( *SectionToUse, 0, 1, *WrangleContentIniName );
		const FConfigSection* PackagesToAlwaysCook = GConfig->GetSectionPrivate( TEXT("Engine.PackagesToAlwaysCook"), 0, 1, GEngineIni );
		const FConfigSection* StartupPackages = GConfig->GetSectionPrivate( TEXT("Engine.StartupPackages"), 0, 1, GEngineIni );

		// we expect either the .ini to exist, or -allmaps to be specified
		if (!PackagesToFullyLoadSection && !bShouldLoadAllMaps)
		{
			warnf(NAME_Error, TEXT("This commandlet needs a WrangleContent.ini in the Config directory with a [WrangleContent.PackagesToFullyLoad] section"));
			return 1;
		}

		if (bShouldCleanOldDirectories)
		{
			GFileManager->DeleteDirectory(*FString::Printf(TEXT("..\\..\\%sGame\\CutdownPackages"), appGetGameName()), FALSE, TRUE);
			GFileManager->DeleteDirectory(*FString::Printf(TEXT("..\\..\\%sGame\\NFSContent"), appGetGameName()), FALSE, TRUE);
		}

		// copy the packages to load, since we are modifying it
		FConfigSectionMap PackagesToFullyLoad;
		if (PackagesToFullyLoadSection)
		{
			PackagesToFullyLoad = *PackagesToFullyLoadSection;
		}

		// move any always cook packages to list of packages to load
		if (PackagesToAlwaysCook)
		{
			for (FConfigSectionMap::TConstIterator It(*PackagesToAlwaysCook); It; ++It)
			{
				if (It.Key() == TEXT("Package") || It.Key() == TEXT("SeekFreePackage"))
				{
					PackagesToFullyLoad.Add(*It.Key().ToString(), *It.Value());
				}
			}
		}

		// move any startup packages to list of packages to load
		if (StartupPackages)
		{
			for (FConfigSectionMap::TConstIterator It(*StartupPackages); It; ++It)
			{
				if (It.Key() == TEXT("Package"))
				{
					PackagesToFullyLoad.Add(*It.Key().ToString(), *It.Value());
				}
			}
		}

		if (bShouldLoadAllMaps)
		{
			TArray<FString> AllPackages = GPackageFileCache->GetPackageFileList();
			for (INT PackageIndex = 0; PackageIndex < AllPackages.Num(); PackageIndex++)
			{
				const FFilename& PackageName = AllPackages(PackageIndex);
				if (SupportedMapExtensions.Contains(PackageName.GetExtension()))
				{
					PackagesToFullyLoad.Add(TEXT("Package"), PackageName.GetBaseFilename());
				}
			}
		}


		// read in the per-map packages to cook
		TMap<FString, TArray<FString> > PerMapCookPackages;
		GConfig->Parse1ToNSectionOfStrings(TEXT("Engine.PackagesToForceCookPerMap"), TEXT("Map"), TEXT("Package"), PerMapCookPackages, GEngineIni);

		// gather any per map packages for cooking
		TArray<FString> PerMapPackagesToLoad;
		for (FConfigSectionMap::TIterator It(PackagesToFullyLoad); It; ++It)
		{
			// add dependencies for the per-map packages for this map (if any)
			TArray<FString>* Packages = PerMapCookPackages.Find(It.Value());
			if (Packages != NULL)
			{
				for (INT PackageIndex = 0; PackageIndex < Packages->Num(); PackageIndex++)
				{
					PerMapPackagesToLoad.AddItem(*(*Packages)(PackageIndex));
				}
			}
		}

		// now add them to the list of all packges to load
		for (INT PackageIndex = 0; PackageIndex < PerMapPackagesToLoad.Num(); PackageIndex++)
		{
			PackagesToFullyLoad.Add(TEXT("Package"), *PerMapPackagesToLoad(PackageIndex));
		}

		// make sure all possible script packages are loaded
		TArray<FString> PackageNames;
		appGetScriptPackageNames(PackageNames, SPT_AllScript, FALSE);
		for (INT PackageIndex = 0; PackageIndex < PackageNames.Num(); PackageIndex++)
		{
			warnf(TEXT("Loading script package %s..."), *PackageNames(PackageIndex));
			UObject::LoadPackage(NULL, *PackageNames(PackageIndex), LOAD_None);
		}
		
		// all currently loaded public objects were referenced by script code, so mark it as referenced
		for(FObjectIterator ObjectIt;ObjectIt;++ObjectIt)
		{
			UObject* Object = *ObjectIt;

			// record all public referenced objects
//			if (Object->HasAnyFlags(RF_Public))
			{
				ReferenceObject(Object, AllReferencedPublicObjects, FALSE);
			}
		}

		// go over all the packages that we want to fully load
		for (FConfigSectionMap::TIterator It(PackagesToFullyLoad); It; ++It)
		{
			// there may be multiple sublevels to load if this package is a persistent level with sublevels
			TArray<FString> PackagesToLoad;
			// start off just loading this package (more may be added in the loop)
			PackagesToLoad.AddItem(*It.Value());

			for (INT PackageIndex = 0; PackageIndex < PackagesToLoad.Num(); PackageIndex++)
			{
				// save a copy of the packagename (not a reference in case the PackgesToLoad array gets realloced)
				FString PackageName = PackagesToLoad(PackageIndex);
				FFilename PackageFilename;

				if( GPackageFileCache->FindPackageFile( *PackageName, NULL, PackageFilename ) == TRUE )
				{
					SET_WARN_COLOR(COLOR_WHITE);
					warnf(TEXT("Fully loading %s..."), *PackageFilename);
					CLEAR_WARN_COLOR();

	// @todo josh: track redirects in this package and then save the package instead of copy it if there were redirects
	// or make sure that the following redirects marks the package dirty (which maybe it shouldn't do in the editor?)

					// load the package fully
					UPackage* Package = UObject::LoadPackage(NULL, *PackageFilename, LOAD_None);

					UObject::BeginLoad();
					ULinkerLoad* Linker = UObject::GetPackageLinker( NULL, *PackageFilename, LOAD_Quiet|LOAD_NoWarn|LOAD_NoVerify, NULL, NULL );
					UObject::EndLoad();

					// look for special package types
					UBOOL bIsMap = Linker->ContainsMap();
					UBOOL bIsScriptPackage = Linker->ContainsCode();

					// collect all public objects loaded
					for(FObjectIterator ObjectIt; ObjectIt; ++ObjectIt)
					{
						UObject* Object = *ObjectIt;

						// record all public referenced objects (skipping over top level packages)
						if (/*Object->HasAnyFlags(RF_Public) &&*/ Object->GetOuter() != NULL)
						{
							// is this public object in a fully loaded package?
							UBOOL bIsObjectInFullyLoadedPackage = Object->IsIn(Package);

							if (bIsMap && bIsObjectInFullyLoadedPackage && Object->HasAnyFlags(RF_Public))
							{
								warnf(NAME_Warning, TEXT("Clearing public flag on map object %s"), *Object->GetFullName());
								Object->ClearFlags(RF_Public);
								// mark that we need to save the package since we modified it (instead of copying it)
								Object->MarkPackageDirty();
							}
							else
							{
								// record that this object was referenced
								ReferenceObject(Object, AllReferencedPublicObjects, bIsObjectInFullyLoadedPackage);
							}
						}
					}

					// strip out data if desired
					WrangleAwayData(Package, (UE3::EPlatformType)PlatformsToKeep, bShouldStripLargeEditorData, bShouldStripMips);

					// add any sublevels of this world to the list of levels to load
					for( TObjectIterator<UWorld> It; It; ++It )
					{
						UWorld*		World		= *It;
						AWorldInfo* WorldInfo	= World->GetWorldInfo();
						// iterate over streaming level objects loading the levels.
						for( INT LevelIndex=0; LevelIndex<WorldInfo->StreamingLevels.Num(); LevelIndex++ )
						{
							ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
							if( StreamingLevel )
							{
								FString SubLevelName = StreamingLevel->PackageName.ToString();
								// add this sublevel's package to the list of packages to load if it's not already in the master list of packages
								if (PackagesToFullyLoad.FindKey(SubLevelName) == NULL)
								{
									PackagesToLoad.AddUniqueItem(SubLevelName);
								}
							}
						}
					}

					// save/copy the package if desired, and only if it's not a script package (script code is
					// not cutdown, so we always use original script code)
					if (bShouldSavePackages && !bIsScriptPackage)
					{
						// make the name of the location to put the package
						FString CutdownPackageName = MakeCutdownFilename(PackageFilename);
						
						// if the package was modified by loading it, then we should save the package
						if (Package->IsDirty())
						{
							// save the fully load packages
							warnf(TEXT("Saving fully loaded package %s..."), *CutdownPackageName);
							if (!SavePackageHelper(Package, CutdownPackageName))
							{
								warnf(NAME_Error, TEXT("Failed to save package %s..."), *CutdownPackageName);
							}
						}
						else
						{
							warnf(TEXT("Copying fully loaded package %s..."), *CutdownPackageName);
							// copy the unmodified file (faster than saving) (0 is success)
							if (GFileManager->Copy(*CutdownPackageName, *PackageFilename) != 0)
							{
								warnf(NAME_Error, TEXT("Failed to copy package to %s..."), *CutdownPackageName);
							}
						}
					}

					// close this package
					UObject::CollectGarbage(RF_Native);
				}
			}
		}

		// save out the referenced objects so we can restore
		FArchive* Ar = GFileManager->CreateFileWriter(*(appGameDir() + TEXT("Wrangle.bin")));
		*Ar << AllReferencedPublicObjects;
		delete Ar;
	}

	// list of all objects that aren't needed
	TArray<FUnreferencedObject> UnnecessaryPublicObjects;
	TMap<FFilename, FPackageObjects> UnnecessaryObjectsByPackage;
	TMap<FString, UBOOL> UnnecessaryObjects;
	TArray<FFilename> UnnecessaryPackages;

	// now go over all packages, quickly, looking for public objects NOT in the AllNeeded array
	const TArray<FString> AllPackages(GPackageFileCache->GetPackageFileList());

	if (bShouldDumpUnreferencedContent || bShouldSaveUnreferencedContent)
	{
		SET_WARN_COLOR(COLOR_WHITE);
		warnf(TEXT(""));
		warnf(TEXT("Looking for unreferenced objects:"));
		CLEAR_WARN_COLOR();

		// Iterate over all files doing stuff.
		for (INT PackageIndex = 0; PackageIndex < AllPackages.Num(); PackageIndex++)
		{
			FFilename PackageFilename(AllPackages(PackageIndex));
			FString PackageName = PackageFilename.GetBaseFilename();

			// we don't care about trying to wrangle the various shader caches so just skipz0r them
			if(	PackageFilename.GetBaseFilename().InStr( TEXT("LocalShaderCache") )	!= INDEX_NONE
				||	PackageFilename.GetBaseFilename().InStr( TEXT("RefShaderCache") )	!= INDEX_NONE )
			{
				continue;
			}


			// the list of objects in this package
			FPackageObjects* PackageObjs = NULL;

			// this will be set to true if every object in the package is unnecessary
			UBOOL bAreAllObjectsUnnecessary = FALSE;

			if (SupportedMapExtensions.Contains( PackageFilename.GetExtension() ) )
			{
				warnf(TEXT("Skipping map %s..."), *PackageFilename);
				continue;
			}
			else if (PackageFilename.GetExtension() == TEXT("u"))
			{
				warnf(TEXT("Skipping script package %s..."), *PackageFilename);
				continue;
			}
			else if (PackageFilename.ToUpper().InStr(TEXT("SHADERCACHE")) != -1)
			{
				warnf(TEXT("Skipping shader cache package %s..."), *PackageFilename);
				continue;
			}
			else
			{
				// get the objects referenced by this package
				PackageObjs = AllReferencedPublicObjects.Find(*PackageName);

				// if the were no objects referenced in this package, we can just skip it, 
				// and mark the whole package as unreferenced
				if (PackageObjs == NULL)
				{
					warnf(TEXT("No objects in %s were referenced..."), *PackageFilename);
					new(UnnecessaryPublicObjects) FUnreferencedObject(PackageName, 
						TEXT("ENTIRE PACKAGE"), GFileManager->FileSize(*PackageFilename));

					// all objects in this package are unnecasstry
					bAreAllObjectsUnnecessary = TRUE;
				}
				else if (PackageObjs->bIsFullyLoadedPackage)
				{
					warnf(TEXT("Skipping fully loaded package %s..."), *PackageFilename);
					continue;
				}
				else
				{
					warnf(TEXT("Scanning %s..."), *PackageFilename);
				}
			}

			UObject::BeginLoad();
			ULinkerLoad* Linker = UObject::GetPackageLinker( NULL, *PackageFilename, LOAD_Quiet|LOAD_NoWarn|LOAD_NoVerify, NULL, NULL );
			UObject::EndLoad();

			// go through the exports in the package, looking for public objects
			for (INT ExportIndex = 0; ExportIndex < Linker->ExportMap.Num(); ExportIndex++)
			{
				FObjectExport& Export = Linker->ExportMap(ExportIndex);
				FString ExportName = Linker->GetExportFullName(ExportIndex);

				// some packages may have brokenness in them so we want to just continue so we can wrangle
				if( Export.ObjectName == NAME_None )
				{
					warnf( TEXT( "    Export.ObjectName == NAME_None  for Package: %s " ), *PackageFilename );
					continue;
				}

				// make sure its outer is a package, and this isn't a package
				if (Linker->GetExportClassName(ExportIndex) == NAME_Package || 
					(Export.OuterIndex != 0 && Linker->GetExportClassName(Export.OuterIndex - 1) != NAME_Package))
				{
					continue;
				}

				// was it not already referenced?
				// NULL means it wasn't in the reffed public objects map for the package
				if (bAreAllObjectsUnnecessary || PackageObjs->ReferencedObjects.Find(ExportName) == NULL)
				{
					// is it public?
					if ((Export.ObjectFlags & RF_Public) != 0 && !bAreAllObjectsUnnecessary)
					{
						// if so, then add it to list of unused pcreateexportublic items
						new(UnnecessaryPublicObjects) FUnreferencedObject(PackageFilename.GetBaseFilename(), ExportName, Export.SerialSize);
					}

					// look for existing entry
					FPackageObjects* ObjectsInPackage = UnnecessaryObjectsByPackage.Find(*PackageFilename);
					// if not found, make a new one
					if (ObjectsInPackage == NULL)
					{
						ObjectsInPackage = &UnnecessaryObjectsByPackage.Set(*PackageFilename, FPackageObjects());
					}

					// get object's class
					check(IS_IMPORT_INDEX(Export.ClassIndex));
					FString ClassName = Linker->GetImportPathName(- Export.ClassIndex - 1);
					UClass* Class = StaticLoadClass(UObject::StaticClass(), NULL, *ClassName, NULL, LOAD_None, NULL);
					// When wrangling content, you often are loading packages that have not been saved in ages and have a reference to a class
					// that no longer exists.  Instead of asserting, we will just continue
					if( bShouldSkipMissingClasses == TRUE )
					{
						if( Class == NULL )
						{
							continue;
						}
					}
					else
					{
						check(Class);
					}

					// make sure it doesn't get GC'd
					Class->AddToRoot();
				
					// add this referenced object to the map
					ObjectsInPackage->ReferencedObjects.Set(*ExportName, Class);

					// add this to the map of all unnecessary objects
					UnnecessaryObjects.Set(*ExportName, TRUE);
				}
			}

			// collect garbage every 20 packages (we aren't fully loading, so it doesn't need to be often)
			if ((PackageIndex % 20) == 0)
			{
				UObject::CollectGarbage(RF_Native);
			}
		}
	}

	if (bShouldSavePackages)
	{
		INT NumPackages = AllReferencedPublicObjects.Num();

		// go through all packages, and save out referenced objects
		SET_WARN_COLOR(COLOR_WHITE);
		warnf(TEXT(""));
		warnf(TEXT("Saving referenced objects in %d Packages:"), NumPackages);
		CLEAR_WARN_COLOR();
		INT PackageIndex = 0;
		for (TMap<FString, FPackageObjects>::TIterator It(AllReferencedPublicObjects); It; ++It, PackageIndex++ )
		{
			// we don't care about trying to wrangle the various shader caches so just skipz0r them
			if(	It.Key().InStr( TEXT("LocalShaderCache"), FALSE, TRUE ) != INDEX_NONE
				|| It.Key().InStr( TEXT("RefShaderCache"), FALSE, TRUE ) != INDEX_NONE )
			{
				continue;
			}

			// if the package was a fully loaded package, than we already saved it
			if (It.Value().bIsFullyLoadedPackage)
			{
				continue;
			}

			// package for all loaded objects
			UPackage* Package = NULL;
			
			// fully load all the referenced objects in the package
			for (TMap<FString, UClass*>::TIterator It2(It.Value().ReferencedObjects); It2; ++It2)
			{
				// get the full object name
				FString ObjectPathName = It2.Key();

				// skip over the class portion (the It2.Value() has the class pointer already)
				INT Space = ObjectPathName.InStr(TEXT(" "));
				check(Space);

				// get everything after the space
				ObjectPathName = ObjectPathName.Right(ObjectPathName.Len() - (Space + 1));

				// load the referenced object

				UObject* Object = UObject::StaticLoadObject(It2.Value(), NULL, *ObjectPathName, NULL, LOAD_NoWarn, NULL);

				// the object may not exist, because of attempting to load localized content
				if (Object)
				{
					check(Object->GetPathName() == ObjectPathName);

					// set the package if needed
					if (Package == NULL)
					{
						Package = Object->GetOutermost();
					}
					else
					{
						// make sure all packages are the same
						check(Package == Object->GetOutermost());
					}
				}
			}

			// make sure we found some objects in here
			if (Package)
			{
				// mark this package as fully loaded so it can be saved, even though we didn't fully load it
				// (which is the point of this commandlet)
				Package->MarkAsFullyLoaded();

				// get original path of package
				FFilename OriginalPackageFilename;

				//warnf( TEXT( "*It.Key(): %s" ), *It.Key() );

				// we need to be able to find the original package
				//verify(GPackageFileCache->FindPackageFile(*It.Key(), NULL, OriginalPackageFilename) == TRUE);
				if( GPackageFileCache->FindPackageFile(*It.Key(), NULL, OriginalPackageFilename) == FALSE )
				{
					appErrorf( TEXT( "Could not find file in file cache: %s"), *It.Key() );
				}

				// any maps need to be fully referenced
				check( !SupportedMapExtensions.Contains( OriginalPackageFilename.GetExtension() ) );

				// make the filename for the output package
				FString CutdownPackageName = MakeCutdownFilename(OriginalPackageFilename);

				warnf(TEXT("Saving %s... [%d/%d]"), *CutdownPackageName, PackageIndex + 1, NumPackages);

				// strip out data if desired
				WrangleAwayData(Package, (UE3::EPlatformType)PlatformsToKeep, bShouldStripLargeEditorData, bShouldStripMips);

				// save the package now that all needed objects in it are loaded.
				// At this point, any object still around should be saved so we pass all flags so all objects are saved
				SavePackageHelper(Package, *CutdownPackageName, RF_AllFlags);

				// close up this package
				UObject::CollectGarbage(RF_Native);
			}
		}
	}

	if (bShouldDumpUnreferencedContent)
	{
		SET_WARN_COLOR(COLOR_WHITE);
		warnf(TEXT(""));
		warnf(TEXT("Unreferenced Public Objects:"));
		CLEAR_WARN_COLOR();

		// Create string with system time to create a unique filename.
		INT Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec;
		appSystemTime( Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec );
		FString	CurrentTime = FString::Printf(TEXT("%i.%02i.%02i-%02i.%02i.%02i"), Year, Month, Day, Hour, Min, Sec );

		// create a .csv
		FString CSVFilename = FString::Printf(TEXT("%sUnreferencedObjects-%s.csv"), *appGameLogDir(), *CurrentTime);
		FArchive* CSVFile = GFileManager->CreateFileWriter(*CSVFilename);
		if (!CSVFile)
		{
			warnf(NAME_Error, TEXT("Failed to open output file %s"), *CSVFilename);
		}

		for (INT ObjectIndex = 0; ObjectIndex < UnnecessaryPublicObjects.Num(); ObjectIndex++)
		{
			FUnreferencedObject& Object = UnnecessaryPublicObjects(ObjectIndex);
			warnf(*Object.ObjectName);

			// dump out a line to the .csv file
			// @todo: sort by size to Excel's 65536 limit gets the biggest objects
			FString CSVLine = FString::Printf(TEXT("%s,%s,%d%s"), *Object.PackageName, *Object.ObjectName, Object.SerialSize, LINE_TERMINATOR);
			CSVFile->Serialize(TCHAR_TO_ANSI(*CSVLine), CSVLine.Len());
		}
	}

	// load every unnecessary object by package, rename it and any unnecessary objects if uses, to the 
	// an unnecessary package, and save it
	if (bShouldSaveUnreferencedContent)
	{
		INT NumPackages = UnnecessaryObjectsByPackage.Num();
		SET_WARN_COLOR(COLOR_WHITE);
		warnf(TEXT(""));
		warnf(TEXT("Saving unreferenced objects [%d packages]:"), NumPackages);
		CLEAR_WARN_COLOR();

		// go through each package that has unnecessary objects in it
		INT PackageIndex = 0;
		for (TMap<FFilename, FPackageObjects>::TIterator PackageIt(UnnecessaryObjectsByPackage); PackageIt; ++PackageIt, PackageIndex++)
		{
			// we don't care about trying to wrangle the various shader caches so just skipz0r them
			if(	PackageIt.Key().InStr( TEXT("LocalShaderCache") ) != INDEX_NONE
				|| PackageIt.Key().InStr( TEXT("RefShaderCache") ) != INDEX_NONE )
			{
				continue;
			}


			//warnf(TEXT("Processing %s"), *PackageIt.Key());
			UPackage* FullyLoadedPackage = NULL;
			// fully load unnecessary packages with no objects, 
			if (PackageIt.Value().ReferencedObjects.Num() == 0)
			{
				// just load it, and don't need a reference to it
				FullyLoadedPackage = UObject::LoadPackage(NULL, *PackageIt.Key(), LOAD_None);
			}
			else
			{
				// load every unnecessary object in this package
				for (TMap<FString, UClass*>::TIterator ObjectIt(PackageIt.Value().ReferencedObjects); ObjectIt; ++ObjectIt)
				{
					// get the full object name
					FString ObjectPathName = ObjectIt.Key();

					// skip over the class portion (the It2.Value() has the class pointer already)
					INT Space = ObjectPathName.InStr(TEXT(" "));
					check(Space > 0);

					// get everything after the space
					ObjectPathName = ObjectPathName.Right(ObjectPathName.Len() - (Space + 1));

					// load the unnecessary object
					UObject* Object = UObject::StaticLoadObject(ObjectIt.Value(), NULL, *ObjectPathName, NULL, LOAD_NoWarn, NULL);
					
					// this object should exist since it was gotten from a linker
					if (!Object)
					{
						warnf(NAME_Error, TEXT("Failed to load object %s, it will be deleted permanently!"), *ObjectPathName);
					}
				}
			}

			// now find all loaded objects (in any package) that are in marked as unnecessary,
			// and rename them to their destination
			for (TObjectIterator<UObject> It; It; ++It)
			{
				// if was unnecessary...
				if (UnnecessaryObjects.Find(*It->GetFullName()))
				{
					// ... then rename it (its outer needs to be a package, everything else will have to be
					// moved by its outer getting moved)
					if (!It->IsA(UPackage::StaticClass()) &&
						It->GetOuter() &&
						It->GetOuter()->IsA(UPackage::StaticClass()) &&
						It->GetOutermost()->GetName().Left(4) != TEXT("NFS_"))
					{
						UPackage* NewPackage = UObject::CreatePackage(NULL, *(FString(TEXT("NFS_")) + It->GetOuter()->GetPathName()));
						//warnf(TEXT("Renaming object from %s to %s.%s"), *It->GetPathName(), *NewPackage->GetPathName(), *It->GetName());

						// move the object if we can. IF the rename fails, then the object was already renamed to this spot, but not GC'd.
						// that's okay.
						if (It->Rename(*It->GetName(), NewPackage, REN_Test))
						{
							It->Rename(*It->GetName(), NewPackage, REN_None);
						}
					}

				}
			}

			// find the one we moved this packages objects to
			FFilename PackagePath = PackageIt.Key();
			FString PackageName = PackagePath.GetBaseFilename();
			UPackage* MovedPackage = UObject::FindPackage(NULL, *(FString(TEXT("NFS_")) + PackageName));
			check(MovedPackage);

			// convert the new name to a a NFS directory directory
			FFilename MovedFilename = MakeCutdownFilename(FString::Printf(TEXT("%s\\NFS_%s"), *PackagePath.GetPath(), *PackagePath.GetCleanFilename()), TEXT("NFSContent"));
			warnf(TEXT("Saving package %s [%d/%d]"), *MovedFilename, PackageIndex, NumPackages);
			// finally save it out
			SavePackageHelper(MovedPackage, *MovedFilename);

			UObject::CollectGarbage(RF_Native);
		}
	}

	return 0;
}

IMPLEMENT_CLASS(UWrangleContentCommandlet);

/**
 *	UUT3MapStatsCommandlet.
 *
 *	Spits out information on shipping Unreal Tournament 3 maps in CSV format. This assumes that none of the maps are multi-level.
 *
 *	We only care about static mesh actor components to make it easier to gage what needs to be fixed up via major changes.
 */
INT UUT3MapStatsCommandlet::Main( const FString & Params )
{
	// Parse the command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;
	const TCHAR * Parms = *Params;
	ParseCommandLine( Parms, Tokens, Switches );

	// Log at end to avoid other warnings messing up output.
	TArray<FString> GatheredData;
	new(GatheredData) FString( TEXT("Map,Primitive Count,Section Count,Instanced Triangle Count") );

	// Iterate over all packages, weeding out non shipping maps or non- map packages.
	TArray<FString> PackageList = GPackageFileCache->GetPackageFileList();
	for( INT PackageIndex=0; PackageIndex<PackageList.Num(); PackageIndex++ )
	{
		UBOOL bIsShippingMap	= FALSE;
		FFilename Filename		= PackageList(PackageIndex);
		
		if( Filename.GetExtension() == FURL::DefaultMapExt )
		{		
			if( Filename.GetPath().ToUpper().Right( ARRAY_COUNT(TEXT("MAPS"))-1 ) == TEXT("MAPS") )
			{
				bIsShippingMap = TRUE;
			}
			if( Filename.GetPath().ToUpper().Right( ARRAY_COUNT(TEXT("VISUALPROTOTYPES"))-1 ) == TEXT("VISUALPROTOTYPES") )
			{
				bIsShippingMap = TRUE;
			}
			if( Filename.ToUpper().InStr(TEXT("DEMOCONTENT")) != INDEX_NONE )
			{
				bIsShippingMap = FALSE;
			}
			if( Filename.ToUpper().InStr(TEXT("TESTMAPS")) != INDEX_NONE )
			{
				bIsShippingMap = FALSE;
			}
			if( Filename.ToUpper().InStr(TEXT("POC")) != INDEX_NONE )
			{
				bIsShippingMap = FALSE;
			}

			// Gather some information for later. We print it out all at once to not be interrupted by load warnings.
			// @warning: this code assumes that none of the maps are multi-level
			if( bIsShippingMap ) 
			{
				warnf(TEXT("Shipping map: %s"),*Filename);

				UPackage* Package = LoadPackage( NULL, *Filename, LOAD_None );
				if( Package )
				{
					INT SectionCount	= 0;
					INT PrimitiveCount	= 0;
					INT TriangleCount	= 0;

					// Iterate over all static mesh actors (we only care about ones in package).
					for( TObjectIterator<AStaticMeshActor> It; It; ++It )
					{
						// Figure out associate mesh if actor is in level package.
						AStaticMeshActor*	StaticMeshActor = *It;
						UStaticMesh*		StaticMesh		= NULL;
						if( StaticMeshActor 
						&&	StaticMeshActor->IsIn( Package ) 
						&&	StaticMeshActor->StaticMeshComponent )
						{
							StaticMesh = StaticMeshActor->StaticMeshComponent->StaticMesh;
						}
						// Gather stats for mesh.
						if( StaticMesh )
						{
							for( INT ElementIndex=0; ElementIndex<StaticMesh->LODModels(0).Elements.Num(); ElementIndex++ )
							{
								const FStaticMeshElement& StaticMeshElement = StaticMesh->LODModels(0).Elements(ElementIndex);
								TriangleCount += StaticMeshElement.NumTriangles;
								SectionCount++;
							}
							PrimitiveCount++;
						}
					}

					// Update gathered stats.
					new(GatheredData) FString( FString::Printf(TEXT("%s,%i,%i,%i"), *Filename.GetCleanFilename(), PrimitiveCount, SectionCount, TriangleCount) );
				}

				// Purge map again.
				UObject::CollectGarbage( RF_Native, TRUE );
			}
		}
	}

	// Log information in CSV format.
	for( INT LineIndex=0; LineIndex<GatheredData.Num(); LineIndex++ )
	{
		warnf(TEXT(",%s"),*GatheredData(LineIndex));
	}

	return 0;
}

IMPLEMENT_CLASS(UUT3MapStatsCommandlet);

INT UListLoopingEmittersCommandlet::Main(const FString& Params)
{
	// Load all game script files.
	TArray<FString> PackageNames;
	appGetScriptPackageNames(PackageNames, SPT_NonNative, FALSE);
	extern void LoadPackageList(const TArray<FString>& PackageNames);
	LoadPackageList( PackageNames );

	// Find referenced looping emitters.
	for( TObjectIterator<UParticleSystem> It; It; ++It )
	{
		UParticleSystem* ParticleSystem = *It;

		UBOOL bIsInfinitelyLooping = FALSE;
		for( INT EmitterIndex=0; EmitterIndex<ParticleSystem->Emitters.Num(); EmitterIndex++ )
		{
			UParticleEmitter* ParticleEmitter = ParticleSystem->Emitters(EmitterIndex);
			if( ParticleEmitter )
			{
				for( INT LODIndex=0; LODIndex < ParticleEmitter->LODLevels.Num(); LODIndex++ )
				{
					UParticleLODLevel* ParticleLODLevel = ParticleEmitter->LODLevels(LODIndex);
					if( ParticleLODLevel && ParticleLODLevel->RequiredModule && ParticleLODLevel->RequiredModule->EmitterLoops == 0 )
					{
						bIsInfinitelyLooping = TRUE;
					}
				}					
			}
		}

		if( bIsInfinitelyLooping )
		{
			warnf(NAME_Warning,TEXT("%s"),*ParticleSystem->GetFullName());
		}
	}

	return 0;
}

IMPLEMENT_CLASS(UListLoopingEmittersCommandlet);

/** 
 * USoundClassInfoCommandlet
 * 
 * Iterate over all sound cues and do a sanity check on the sound groups
 * Specifically -
 *  - List of Sound Groups assigned to all Sound Cues in Content\Sounds
 *  - List of Sound Cues not assigned to any Sound Group in Content\Sounds
 *  - List of Sound Cues assigned to Sound Group 'Ambient' that do not exist in packages AmbientLoops.upk and AmbientNonLoops
 *  - List of any Sound Wavs or Sound Cues that exist in packages other than Content\Sounds
 */
INT USoundClassInfoCommandlet::Main( const FString& Params )
{
	INT Counter = 0;
	TArray<FName> UniqueClasses;
	TArray<INT> UniqueClassesCounts;
	TArray<FString> MisplacedSoundCues;
	TArray<FString> MisplacedSoundNodeWaves;
	TArray<FString> UnclassedSoundCues;
	TArray<FString> MisplacedAmbientSoundCues;

	// Build package file list.
	const TArray<FString> FilesInPath( GPackageFileCache->GetPackageFileList() );
	if( FilesInPath.Num() == 0 )
	{
		warnf( NAME_Warning, TEXT( "No packages found" ) );
		return( 1 );
	}

	// Iterate over all files doing stuff.
	for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); ++FileIndex )
	{
		const FFilename& Filename = FilesInPath( FileIndex );
		if( Filename.InStr( TEXT( "Game\\Content\\" ) ) > 0 )
		{
			warnf( NAME_Log, TEXT( "Loading %s" ), *Filename );

			UPackage* Package = UObject::LoadPackage( NULL, *Filename, LOAD_None );
			if( Package == NULL )
			{
				warnf( NAME_Error, TEXT( "Error loading %s!" ), *Filename );
			}
			else
			{
				if( Filename.InStr( TEXT( "Game\\Content\\Sounds\\" ) ) > 0 )
				{
					for( TObjectIterator<USoundCue> It; It; ++It )
					{
						USoundCue* Cue = *It;
						if( Cue )
						{
							INT Index;

							if( UniqueClasses.FindItem( Cue->SoundClass, Index ) )
							{
								UniqueClassesCounts( Index )++;
							}
							else
							{
								UniqueClasses.AddItem( Cue->SoundClass );
								UniqueClassesCounts.AddItem( 0 );
							}

							if( Cue->SoundClass == FName( TEXT( "None" ) ) )
							{
								UnclassedSoundCues.AddUniqueItem( Cue->GetFullName() );
							}
							else if( Cue->SoundClass == FName( TEXT( "Ambient" ) ) )
							{
								FString PackageName = Cue->GetOutermost()->GetName();
								if( PackageName.InStr( TEXT( "Ambient_Loop" ) ) < 0 && PackageName.InStr( TEXT( "Ambient_NonLoop" ) ) < 0 )
								{
									MisplacedAmbientSoundCues.AddUniqueItem( Cue->GetFullName() );
								}
							}
						}
					}
				}
				else
				{
					for( TObjectIterator<USoundCue> It; It; ++It )
					{
						USoundCue* Cue = *It;
						if( Cue )
						{
							if( Cue->IsIn( Package ) )
							{
								FString FullName = Cue->GetFullName();
								if( FullName.InStr( TEXT( "AmbientSoundSimple_" ) ) < 0 
									&& FullName.InStr( TEXT( "AmbientSound_" ) ) < 0 
									&& FullName.InStr( TEXT( "AmbientSoundMovable_" ) ) < 0 
									&& FullName.InStr( TEXT( "AmbientSoundNonLoop_" ) ) < 0 
									&& FullName.InStr( TEXT( "AmbientSoundNonLoopingToggleable_" ) ) < 0 
									&& FullName.InStr( TEXT( "AmbientSoundSimpleToggleable_" ) ) < 0 )
								{
									MisplacedSoundCues.AddUniqueItem( FullName );
									warnf( TEXT( "Misplaced sound cue: '%s'" ), *FullName );
								}
							}
						}
					}

					for( TObjectIterator<USoundNodeWave> It; It; ++It )
					{
						USoundNodeWave* Wave = *It;
						if( Wave )
						{
							if( Wave->IsIn( Package ) )
							{
								FString FullName = Wave->GetFullName();
								MisplacedSoundNodeWaves.AddUniqueItem( FullName );
								warnf( TEXT( "Misplaced sound node wave: '%s'" ), *FullName );
							}
						}
					}
				}
			}
		}

		if( ( Counter++ & 0xf ) == 0 )
		{
			UObject::CollectGarbage( RF_Native );
		}
	}

	// List all found sound classes
	warnf( NAME_Log, TEXT( "" ) );
	warnf( NAME_Log, TEXT( "%d sound classes found ..." ), UniqueClasses.Num() );
	for( INT i = 0; i < UniqueClasses.Num(); i++ )
	{
		warnf( NAME_Log, TEXT( " ... %4d : %s" ), UniqueClassesCounts( i ), *UniqueClasses( i ).GetNameString() );
	}

	// List all sound cues not under Content\\Sounds
	warnf( NAME_Log, TEXT( "" ) );
	warnf( NAME_Log, TEXT( "%d misplaced sound cues found ..." ), MisplacedSoundCues.Num() );
	for( TArray<FString>::TIterator It( MisplacedSoundCues ); It; ++It )
	{
		FString CueName = *It;
		warnf( NAME_Log, TEXT( " ... %s" ), *CueName );
	}

	// List all sound mode waves not under Content\\Sounds
	warnf( NAME_Log, TEXT( "" ) );
	warnf( NAME_Log, TEXT( "%d misplaced sound node waves found ..." ), MisplacedSoundNodeWaves.Num() );
	for( TArray<FString>::TIterator It( MisplacedSoundNodeWaves ); It; ++It )
	{
		FString SoundName = *It;
		warnf( NAME_Log, TEXT( " ... %s" ), *SoundName );
	}

	// List all sound cues in class ambient that aren't in the AmbientLoops or AmbientNonLoops packages
	warnf( NAME_Log, TEXT( "" ) );
	warnf( NAME_Log, TEXT( "%d misplaced ambient sound cues found (not in AmbientLoops or AmbientNonLoops) ..." ), MisplacedAmbientSoundCues.Num() );
	for( TArray<FString>::TIterator It( MisplacedAmbientSoundCues ); It; ++It )
	{
		FString CueName = *It;
		warnf( NAME_Log, TEXT( " ... %s" ), *CueName );
	}	

	// List all sound cues in class 'None' (unclassed)
	warnf( NAME_Log, TEXT( "" ) );
	warnf( NAME_Log, TEXT( "%d unclassed sound cues found ..." ), UnclassedSoundCues.Num() );
	for( TArray<FString>::TIterator It( UnclassedSoundCues ); It; ++It )
	{
		FString CueName = *It;
		warnf( NAME_Log, TEXT( " ... %s" ), *CueName );
	}

	return( 0 );
}

IMPLEMENT_CLASS( USoundClassInfoCommandlet );

/** 
 * List all sound cues containing distance cross fade nodes as we can attenuate with a LPF to save memory
 */
INT UListDistanceCrossFadeNodesCommandlet::Main( const FString& Params )
{
	TArray<FString> SoundCues;

	// Build package file list.
	const TArray<FString> FilesInPath( GPackageFileCache->GetPackageFileList() );
	if( FilesInPath.Num() == 0 )
	{
		warnf( NAME_Warning, TEXT( "No packages found" ) );
		return( 1 );
	}

	// Iterate over all files doing stuff.
	for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); ++FileIndex )
	{
		const FFilename& Filename = FilesInPath( FileIndex );
		if( Filename.InStr( TEXT( "Game\\Content\\Sounds" ) ) > 0 )
		{
			warnf( NAME_Log, TEXT( "Loading %s" ), *Filename );

			UPackage* Package = UObject::LoadPackage( NULL, *Filename, LOAD_None );
			if( Package == NULL )
			{
				warnf( NAME_Error, TEXT( "Error loading %s!" ), *Filename );
			}
			else
			{
				for( TObjectIterator<USoundNodeDistanceCrossFade> It; It; ++It )
				{
					USoundNodeDistanceCrossFade* Node = *It;
					if( Node )
					{
						FString CueName = Node->GetOuter()->GetFullName();
						SoundCues.AddUniqueItem( CueName );
					}
				}
			}

			UObject::CollectGarbage( RF_Native );
		}
	}

	// List all sound cues with a distance cross fade node
	warnf( NAME_Log, TEXT( "" ) );
	warnf( NAME_Log, TEXT( "%d sound cues found ..." ), SoundCues.Num() );
	for( TArray<FString>::TIterator It( SoundCues ); It; ++It )
	{
		FString CueName = *It;
		warnf( NAME_Log, TEXT( " ... %s" ), *CueName );
	}

	return( 0 );
}

IMPLEMENT_CLASS( UListDistanceCrossFadeNodesCommandlet );

/** 
 * ULocSoundInfoCommandlet
 * 
 * Sanity check the loc'd sound data
 */
INT ULocSoundInfoCommandlet::Main( const FString& Params )
{
	TArray<FFilename>	INTPackages;
	TArray<FString>		IntObjects;
	TArray<FString>		LocObjects;
	INT					Index;

	// Build package file list.
	const TArray<FString> FilesInPath( GPackageFileCache->GetPackageFileList() );
	if( FilesInPath.Num() == 0 )
	{
		warnf( NAME_Warning, TEXT( "No packages found" ) );
		return( 1 );
	}

	// Iterate over all files grabbing the objects in the INT packages
	for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); ++FileIndex )
	{
		const FFilename& Filename = FilesInPath( FileIndex );
		if( Filename.StartsWith( TEXT( "..\\..\\UDKGame\\Content\\Sounds\\INT\\" ) ) )
		{
			INTPackages.AddItem( Filename );
		}
	}

	// Get a list of known language extensions
	const TArray<FString>& KnownLanguageExtensions = appGetKnownLanguageExtensions();

	INT Count = 1;
	INT MissingLocalizedCount = 0;
	for( INT Count = 1; Count < KnownLanguageExtensions.Num(); ++Count )
	{
		FString LangFolder = FString::Printf( TEXT( "\\%s\\" ), *KnownLanguageExtensions(Count) );
		FString Appendix = FString::Printf( TEXT( "_%s.upk" ), *KnownLanguageExtensions(Count) );

		INT MissingLocalizedCountPerLanguage = 0;
		for( INT PackageIndex = 0; PackageIndex < INTPackages.Num(); PackageIndex++ )
		{
			FFilename IntFilename = INTPackages( PackageIndex );
			FFilename LocFilename = IntFilename.Replace( TEXT( "\\INT\\" ), *LangFolder );
			LocFilename.ReplaceInline( TEXT( ".upk" ), *Appendix );
			if( !FilesInPath.FindItem( LocFilename, Index ) )
			{
				warnf( NAME_Log, TEXT( "Missing loc package: %s" ), *LocFilename );
			}
			else
			{
				// Load in both packages and compare them
				UPackage* IntPackage = UObject::LoadPackage( NULL, *IntFilename, LOAD_None );
				UPackage* LocPackage = UObject::LoadPackage( NULL, *LocFilename, LOAD_None );

				for( TObjectIterator<USoundNodeWave> It; It; ++It )
				{
					USoundNodeWave* Wave = *It;
					if( Wave->IsIn( IntPackage ) )
					{
						IntObjects.AddItem( Wave->GetName() );
					}
					if( Wave->IsIn( LocPackage ) )
					{
						LocObjects.AddItem( Wave->GetName() );
					}
				}

				// Remove common objects
				for( INT i = IntObjects.Num() - 1; i >= 0; i-- )
				{
					if( LocObjects.FindItem( IntObjects( i ), Index ) )
					{
						LocObjects.Remove( Index );
						IntObjects.Remove( i );
					}
				}

				warnf( NAME_Log, TEXT( "Objects only in package '%s' (%d) ... Missing in %s" ), *IntPackage->GetFullName(), IntObjects.Num(), *KnownLanguageExtensions(Count) );
				for( INT i = 0; i < IntObjects.Num(); i++ )
				{
					MissingLocalizedCountPerLanguage++;
					MissingLocalizedCount++;
					warnf( NAME_Log, TEXT( " ... %s" ), *IntObjects( i ) );
				}

				warnf( NAME_Log, TEXT( "Objects only in package '%s' (%d) ... Should be deleted?" ), *LocPackage->GetFullName(), LocObjects.Num() );
				for( INT i = 0; i < LocObjects.Num(); i++ )
				{
					warnf( NAME_Log, TEXT( " ...... %s" ), *LocObjects( i ) );
				}

				IntObjects.Empty();
				LocObjects.Empty();
			}
		}

		warnf( NAME_Log, TEXT( "Missing %d localized sounds for %s." ), MissingLocalizedCountPerLanguage, *KnownLanguageExtensions(Count) );
		UObject::CollectGarbage( RF_Native );
	}

	warnf( NAME_Log, TEXT( "Missing a total number of %d localized sounds." ), MissingLocalizedCount );
	return( 0 );
}

IMPLEMENT_CLASS( ULocSoundInfoCommandlet );

/** 
 * UDumpPropertiesCommandlet
 * 
 * Dump the properties to a file
 */
INT UDumpPropertiesCommandlet::Main( const FString& Params )
{
	FString FileName = appEngineDir() + TEXT("Localization") + PATH_SEPARATOR + TEXT("AutoGenerated.Properties");
	debugf(*FileName);

	FArchive* DumpFile = GFileManager->CreateFileWriter( *(FileName));
	if( !DumpFile )
	{
		warnf(NAME_Error, TEXT("Failed to open output file %s"), *FileName);
	}
	else
	{
		// Always save as UTF-16 so we can copy it and submit directly for other languages
		UNICHAR BOM = UNICODE_BOM;
		DumpFile->Serialize( &BOM, sizeof(BOM) );

		FString DumpLine;
		TArray<FName> Categories;
		BOOL ContainsProperty = FALSE; // if the class contains a property that we care about

		//Go through all classes and dump their properties
		for( TObjectIterator<UClass> It; It; ++It )
		{
			UClass* Class = *It;
			ContainsProperty = FALSE;

			//Go through all of the properties of this class
			for( TFieldIterator<UProperty> It(Class,FALSE); It; ++It)
			{
				UProperty* Property = *It;
				
				//Is this property editable
				if ( Property->HasAnyPropertyFlags( CPF_Edit ) && !Class->HideCategories.ContainsItem( Property->Category ) && It->GetOuter() == Class )
				{
					// if its the first property that we are adding to the file, add the section
					if (ContainsProperty == FALSE)
					{
						ContainsProperty = TRUE;
						DumpLine = FString::Printf( TEXT("[%s]%s"), *Class->GetName(), LINE_TERMINATOR);
						DumpFile->Serialize(const_cast<TCHAR*>(*DumpLine), DumpLine.Len()*sizeof(TCHAR));
					}

					//Keep track of all of the unique categories so that we can add them at the end
					Categories.AddUniqueItem( Property->Category );

					//Add the property to the file
					UBOOL bIsBoolProperty = ConstCast<UBoolProperty>(Property) != NULL;
					FString PropertyDisplayName = Property->GetName();
					SanitizePropertyDisplayName( PropertyDisplayName, bIsBoolProperty );
					DumpLine = FString::Printf( TEXT("%s=%s%s"), *Property->GetName(), *PropertyDisplayName, LINE_TERMINATOR);
					DumpFile->Serialize(const_cast<TCHAR*>(*DumpLine), DumpLine.Len()*sizeof(TCHAR));

					//Add a tooltip to the file, if any
					if (Property->HasMetaData(TEXT("tooltip")) )
					{
						DumpLine = FString::Printf( TEXT("%s.tooltip=%s%s"), *Property->GetName(), *Property->GetMetaData(TEXT("tooltip")).Replace(TEXT("\n"), TEXT("\\n")).Replace(TEXT("\r"), TEXT("")), LINE_TERMINATOR);
						DumpFile->Serialize(const_cast<TCHAR*>(*DumpLine), DumpLine.Len()*sizeof(TCHAR));
					}
				}
			}

			//Add an empty line if we have added properties to this section
			if (ContainsProperty == TRUE)
			{
				DumpLine = FString::Printf( TEXT("%s"), LINE_TERMINATOR);
				DumpFile->Serialize(const_cast<TCHAR*>(*DumpLine), DumpLine.Len()*sizeof(TCHAR));
			}


			// Dump any structs
			for( TFieldIterator<UStruct> StructIt(Class, FALSE); StructIt; ++StructIt )
			{
				if( StructIt->GetOuter() == Class )
				{
					UBOOL bDumpedMember = FALSE;
					for( TFieldIterator<UProperty> It(*StructIt,FALSE); It; ++It)
					{
						UProperty* StructMember = *It;
						if( !It->HasAnyPropertyFlags(CPF_Edit) )
						{
							continue;
						}

						if( !bDumpedMember )
						{
							DumpLine = FString::Printf( TEXT("[%s]%s"), *((*StructIt)->GetFullGroupName(FALSE)), LINE_TERMINATOR);
							DumpFile->Serialize(const_cast<TCHAR*>(*DumpLine), DumpLine.Len()*sizeof(TCHAR));
							bDumpedMember = TRUE;
						}

						UBOOL bIsStructMemberBoolProperty = ConstCast<UBoolProperty>(StructMember) != NULL;
						FString StructMemberDisplayName = StructMember->GetName();
						SanitizePropertyDisplayName( StructMemberDisplayName, bIsStructMemberBoolProperty );
						DumpLine = FString::Printf( TEXT("%s=%s%s"), *StructMember->GetName(), *StructMemberDisplayName, LINE_TERMINATOR);
						DumpFile->Serialize(const_cast<TCHAR*>(*DumpLine), DumpLine.Len()*sizeof(TCHAR));

						//Add a tooltip to the file, if any
						if (StructMember->HasMetaData(TEXT("tooltip")) )
						{
							DumpLine = FString::Printf( TEXT("%s.tooltip=%s%s"), *StructMember->GetName(), *StructMember->GetMetaData(TEXT("tooltip")).Replace(TEXT("\n"), TEXT("\\n")).Replace(TEXT("\r"), TEXT("")), LINE_TERMINATOR);
							DumpFile->Serialize(const_cast<TCHAR*>(*DumpLine), DumpLine.Len()*sizeof(TCHAR));
						}
					}
					if( bDumpedMember )
					{
						DumpLine = FString::Printf( TEXT("%s"), LINE_TERMINATOR);
						DumpFile->Serialize(const_cast<TCHAR*>(*DumpLine), DumpLine.Len()*sizeof(TCHAR));
					}
				}
			}
		}

		// Write Category names
		DumpLine = FString::Printf( TEXT("[%s]%s"), TEXT("Category"), LINE_TERMINATOR);
		DumpFile->Serialize(const_cast<TCHAR*>(*DumpLine), DumpLine.Len()*sizeof(TCHAR));
		for( INT x = 0 ; x < Categories.Num() ; ++x )
		{
			FString CategoryName = Categories(x).ToString();
			FString CategoryDisplayName = CategoryName;
			const UBOOL bIsBoolProperty = FALSE;
			SanitizePropertyDisplayName( CategoryDisplayName, bIsBoolProperty );
			DumpLine = FString::Printf( TEXT("%s=%s%s"), *CategoryName, *CategoryDisplayName, LINE_TERMINATOR);
			DumpFile->Serialize(const_cast<TCHAR*>(*DumpLine), DumpLine.Len()*sizeof(TCHAR));
		}
		DumpLine = FString::Printf( TEXT("%s"), LINE_TERMINATOR);
		DumpFile->Serialize(const_cast<TCHAR*>(*DumpLine), DumpLine.Len()*sizeof(TCHAR));

		DumpFile->Close();
		delete DumpFile;
	}

	return( 0 );
}

IMPLEMENT_CLASS( UDumpPropertiesCommandlet );


INT UCheckpointGameAssetDatabaseCommandlet::Main( const FString& Params )
{
#if WITH_MANAGED_CODE

	FGameAssetDatabaseStartupConfig StartupConfig;

	// Tell the GAD that we're in "commandlet mode"
	StartupConfig.bIsCommandlet = TRUE;


	// "-Verify" param launches in integrity-check mode.  Otherwise we'll be in Checkpoint mode!
	if( ParseParam( appCmdLine(), TEXT( "Verify" ) ) )
	{
		warnf( NAME_Log, TEXT( "Verifying integrity of game asset database..." ) );

		// Just verify existing data and don't checkpoint or repair anything
		StartupConfig.bShouldVerifyIntegrity = TRUE;
		StartupConfig.bShouldCheckpoint = FALSE;
		StartupConfig.bShouldRepairIfNeeded = FALSE;

		// Never delete (or save) anything while in verify mode
		StartupConfig.bAllowServerDeletesAfterCheckpoint = FALSE;
	}
	// "-ShowAllOld" shows old tags from ALL branches and games
	else if (ParseParam(appCmdLine(), TEXT("ShowAllOld")))
	{
		warnf( NAME_Log, TEXT( "Showing Old Content Tags" ) );

		StartupConfig.bShowAllOldContent = TRUE;
		StartupConfig.bAllowServerDeletesAfterCheckpoint = FALSE;

		// Do not checkpoint or repair
		StartupConfig.bShouldCheckpoint = FALSE;
		StartupConfig.bShouldRepairIfNeeded = FALSE;
	}
	// "-PurgeAllOld" shows old tags from ALL branches and games, then deletes them
	else if (ParseParam(appCmdLine(), TEXT("PurgeAllOld")))
	{
		warnf( NAME_Log, TEXT( "Purging Old Content Tags" ) );
		
		StartupConfig.bShowAllOldContent = TRUE;
		StartupConfig.bPurgeAllOldContent = TRUE;
		StartupConfig.bAllowServerDeletesAfterCheckpoint = TRUE;

		// Do not checkpoint or repair
		StartupConfig.bShouldCheckpoint = FALSE;
		StartupConfig.bShouldRepairIfNeeded = FALSE;
	}
	else
	{
		warnf( NAME_Log, TEXT( "Checkpointing game asset database..." ) );

		// Checkpoint the database.  Also verify everything and repair if needed!
		StartupConfig.bShouldVerifyIntegrity = TRUE;
		StartupConfig.bShouldCheckpoint = TRUE;


		// "-Repair" will attempt to fix any problems detected in the database while checkpointing
		StartupConfig.bShouldRepairIfNeeded = ParseParam( appCmdLine(), TEXT( "Repair" ) );


		// "-NoDeletes" will disable purging of outdated journal entries on the SQL server
		StartupConfig.bAllowServerDeletesAfterCheckpoint = !ParseParam( appCmdLine(), TEXT( "NoDeletes" ) );


		// "-PurgeGhosts" will purge any assets in the database that don't actually exist on disk
		StartupConfig.bPurgeNonExistentAssets = ParseParam( appCmdLine(), TEXT( "PurgeGhosts" ) );


		// "-Reset" param will effectively replace all existing tags by not loading the current checkpoint
		// data or downloading server entries before updating the file
		if( ParseParam( appCmdLine(), TEXT( "Reset" ) ) )
		{
			StartupConfig.bShouldLoadCheckpointFile = FALSE;
			StartupConfig.bShouldLoadJournalEntries = FALSE;
		}

		// "DeletePrivateCollections" will remove private collections LOCALLY before saving the checkpoint file
		StartupConfig.bDeletePrivateCollections = ParseParam( appCmdLine(), TEXT( "DeletePrivateCollections" ) );

		// "DeleteNonUDKCollections" will remove shared collections that do not start with "UDK"
		// LOCALLY before saving the checkpoint file
		StartupConfig.bDeleteNonUDKCollections = ParseParam( appCmdLine(), TEXT( "DeleteNonUDKCollections" ) );
	}

	// "-Dump" will print the contents of the game asset database; however, we should not change anything in the journal.
	StartupConfig.bShouldDumpDatabase = ParseParam( appCmdLine(), TEXT("Dump"));
	if ( StartupConfig.bShouldDumpDatabase )
	{
		StartupConfig.bAllowServerDeletesAfterCheckpoint = FALSE;
	}

	GEngine->Exec( TEXT("UNSUPPRESS DevAssetDataBase") );



	// Init the game asset database and checkpoint all journal data immediately
	FString InitErrorMessageText;
	FGameAssetDatabase::Init(
			StartupConfig,
			InitErrorMessageText );	// Out
	
	if( InitErrorMessageText.Len() > 0 )
	{
		warnf( NAME_Error, TEXT( "Error: %s" ), *InitErrorMessageText );
	}
	else
	{
		warnf( NAME_Log, TEXT( "Game asset database maintenance completed successfully." ) );
	}

	// Shutdown the game asset database
	FGameAssetDatabase::Destroy();
#else
	warnf( NAME_Error, TEXT( "CheckpointGameAssetDatabase commandlet not available unless WITH_MANAGED_CODE is defined" ) );
#endif


	return 0;	// NOTE: 0 == success
}

IMPLEMENT_CLASS( UCheckpointGameAssetDatabaseCommandlet );



/*-----------------------------------------------------------------------------
	UListCustomMaterialExpressions
-----------------------------------------------------------------------------*/

/**
 * List all materials using a custom node and the kind of code that is being used.
 **/
INT UListCustomMaterialExpressionsCommandlet::Main( const FString& Params )
{
	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	const UBOOL bLoadAllPackages = Switches.ContainsItem(TEXT("ALL"));

	TArray<FFilename> FilesInPath;
	if ( bLoadAllPackages )
	{
		TArray<FString> PackageExtensions;
		GConfig->GetArray( TEXT("Core.System"), TEXT("Extensions"), PackageExtensions, GEngineIni );

		Tokens.Empty(PackageExtensions.Num());
		for ( INT ExtensionIndex = 0; ExtensionIndex < PackageExtensions.Num(); ExtensionIndex++ )
		{
			Tokens.AddItem(FString(TEXT("*.")) + PackageExtensions(ExtensionIndex));
		}
	}

	if ( Tokens.Num() == 0 )
	{
		warnf(TEXT("You must specify a package name (multiple files can be delimited by spaces) or wild-card, or specify -all to include all registered packages"));
		return 1;
	}

	BYTE PackageFilter = NORMALIZE_DefaultFlags;
	if ( Switches.ContainsItem(TEXT("MAPSONLY")) )
	{
		PackageFilter |= NORMALIZE_ExcludeContentPackages;
	}

	// assume the first token is the map wildcard/pathname
	TArray<FString> Unused;
	for ( INT TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++ )
	{
		TArray<FFilename> TokenFiles;
		if ( !NormalizePackageNames( Unused, TokenFiles, Tokens(TokenIndex), PackageFilter) )
		{
			debugf(TEXT("No packages found for parameter %i: '%s'"), TokenIndex, *Tokens(TokenIndex));
			continue;
		}

		FilesInPath += TokenFiles;
	}

	if ( FilesInPath.Num() == 0 )
	{
		warnf(TEXT("No files found."));
		return 1;
	}

	FArchive* Ar = GFileManager->CreateFileWriter( *(appGameLogDir() + TEXT("CustomMaterials.log")) );

	for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
	{
		const FFilename& Filename = FilesInPath(FileIndex);

		// we don't care about trying to load the various shader caches so just skip them
		if(	Filename.InStr( TEXT("LocalShaderCache") ) != INDEX_NONE
		||	Filename.InStr( TEXT("RefShaderCache") ) != INDEX_NONE )
		{
			continue;
		}

		warnf( NAME_Log, TEXT("Loading %s"), *Filename );

		const FString& PackageName = FPackageFileCache::PackageFromPath(*Filename);

		UPackage* Package = UObject::LoadPackage( NULL, *Filename, LOAD_NoWarn | LOAD_Quiet );
		if( Package == NULL )
		{
			warnf( NAME_Error, TEXT("Error loading %s!"), *Filename );
		}
		else
		{
			for( TObjectIterator<UMaterialExpressionCustom> It; It; ++It )
			{
				UMaterialExpressionCustom* CustomExpression = *It;
				if( CustomExpression->IsIn( Package ) )
				{					
					Ar->Logf( TEXT("Material: %s"), *CustomExpression->GetOuter()->GetPathName() );
					Ar->Logf( TEXT("%s") LINE_TERMINATOR, *CustomExpression->Code );
				}
			}
		}

		UObject::CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
	}

	Ar->Close();
	delete Ar;

	return 0;
}
IMPLEMENT_CLASS(UListCustomMaterialExpressionsCommandlet)


/*-----------------------------------------------------------------------------
	UAnalyzeShaderCachesCommandlet
-----------------------------------------------------------------------------*/

/**
 * Analyzes shader caches and their content.
 */
INT UAnalyzeShaderCachesCommandlet::Main( const FString& Params )
{
	EShaderPlatform Platform = SP_PCD3D_SM3;

	// Parse passed in platform string.
	FString PlatformString;
	if( Parse(*Params, TEXT("PLATFORM="), PlatformString) )
	{
		PlatformString = PlatformString.ToUpper();
		if( PlatformString == TEXT("SM3") )
		{
			Platform = SP_PCD3D_SM3;
		}
		else if( PlatformString == TEXT("SM5") )
		{
			Platform = SP_PCD3D_SM5;
		}
		else if( PlatformString == TEXT("OPENGL") )
		{
			Platform = SP_PCOGL;
		}
		else if( PlatformString == TEXT("PS3") )
		{
			Platform = SP_PS3;
		}
		else if( PlatformString == TEXT("XENON") || PlatformString == TEXT("XBOX360") )
		{
			Platform = SP_XBOXD3D;
		}
	}

	// Ensure proper shader cache is loaded.
	GetReferenceShaderCache( Platform );

	// Dump vertex and pixel shader information in same XLS
	if( ParseParam(*Params,TEXT("MERGED")) )
	{
		DumpShaderStats( Platform, SF_NumFrequencies );
	}
	// Dump vertex and pixel shader information separately for platform.
	else
	{
		DumpShaderStats( Platform, SF_Pixel );
		DumpShaderStats( Platform, SF_Domain );
		DumpShaderStats( Platform, SF_Hull );
		DumpShaderStats( Platform, SF_Vertex );
	}

	// Dump per material stats.
	DumpMaterialStats( Platform );

	return 0;
}
IMPLEMENT_CLASS(UAnalyzeShaderCachesCommandlet)

/*-----------------------------------------------------------------------------
	UAnalyzeCookedTextureUsageCommandlet
-----------------------------------------------------------------------------*/

/**
 * Analyzes shader caches and their content.
 */
INT UAnalyzeCookedTextureUsageCommandlet::Main( const FString& Params )
{
	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	if( Tokens.Num() )
	{
		UPersistentCookerData* CookerData = LoadObject<UPersistentCookerData>( NULL, TEXT("GlobalPersistentCookerData.PersistentCookerData"), *Tokens(0), LOAD_None, NULL );
		if( CookerData )
		{
			// Global per texture stats.
			{
				// Create diagnostic table in log folder, automatically viewed at exit.
				FDiagnosticTableViewer Table(*FDiagnosticTableViewer::GetUniqueTemporaryFilePath(TEXT("CookedTextureUsage")));

				// Write out header.
				Table.AddColumn( TEXT("Texture Name") );
				Table.AddColumn( TEXT("Unique Packages") );
				Table.AddColumn( TEXT("LODGroup") );
				Table.AddColumn( TEXT("Format") );
				Table.AddColumn( TEXT("SizeX") );
				Table.AddColumn( TEXT("SizeY") );
				Table.AddColumn( TEXT("Stored Once Mip Size") );
				Table.AddColumn( TEXT("Duplicated Mip Size") );
				Table.AddColumn( TEXT("Package Names") );
				Table.CycleRow();

				// Iterate over texture info and create a new row per texture. Number of columns is variable size as we
				// log a new column for each unique package name.
				TArray<FString> TextureGroupNames = FTextureLODSettings::GetTextureGroupNames();
				for( TMap<FString,FCookedTextureUsageInfo>::TConstIterator It( CookerData->TextureUsageInfos ); It; ++It )
				{
					const FString& TextureName = It.Key();
					const FCookedTextureUsageInfo& UsageInfo = It.Value();
					Table.AddColumn( TEXT("%s"), *TextureName );
					Table.AddColumn( TEXT("%i"), UsageInfo.PackageNames.Num() );
					Table.AddColumn( TEXT("%s"), *TextureGroupNames( UsageInfo.LODGroup ) );
					Table.AddColumn( TEXT("%i"), UsageInfo.Format );
					Table.AddColumn( TEXT("%i"), UsageInfo.SizeX );
					Table.AddColumn( TEXT("%i"), UsageInfo.SizeY );
					Table.AddColumn( TEXT("%i"), UsageInfo.StoredOnceMipSize );
					Table.AddColumn( TEXT("%i"), UsageInfo.DuplicatedMipSize );
					for( TSet<FString>::TConstIterator It(UsageInfo.PackageNames); It; ++It )
					{
						FString PackageName = *It;
						Table.AddColumn( TEXT("%s"), *PackageName );
					}
					Table.CycleRow();
				}

				Table.Close();
			}

			// Per package/ LOD group summary
			{ 
				// Create diagnostic table in log folder, automatically viewed at exit.
				FDiagnosticTableViewer Table(*FDiagnosticTableViewer::GetUniqueTemporaryFilePath(TEXT("CookedTextureUsage-PackageSummary")));

				// Write out header.
				Table.AddColumn( TEXT("Package Name") );
				Table.AddColumn( TEXT("LODGroup") );
				Table.AddColumn( TEXT("Count") );
				Table.AddColumn( TEXT("Stored Once Mip Size") );
				Table.AddColumn( TEXT("Duplicated Mip Size") );
				Table.CycleRow();

				// Mapping from PackageName_LODGroup to summary. E.g. Package_1.
				TMap<FString,FCookedTextureUsageInfo> PackageLODSummaries;
				const TCHAR* SplitChar = TEXT("^");

				// Iterate over texture info and collate info per package and per LOD
				TArray<FString> TextureGroupNames = FTextureLODSettings::GetTextureGroupNames();
				for( TMap<FString,FCookedTextureUsageInfo>::TConstIterator It( CookerData->TextureUsageInfos ); It; ++It )
				{
					const FString& TextureName = It.Key();
					const FCookedTextureUsageInfo& UsageInfo = It.Value();

					// Update all packages using this texture with its info.
					for( TSet<FString>::TConstIterator It(UsageInfo.PackageNames); It; ++It )
					{						
						// Generate unique name for package and LOD combo. Code printing it needs to separate/ unmangle.
						FString PackageLOD = *It + SplitChar + appItoa(UsageInfo.LODGroup);
						if( PackageLODSummaries.Find( PackageLOD ) )
						{
							FCookedTextureUsageInfo* SummaryInfo = PackageLODSummaries.Find( PackageLOD );
							SummaryInfo->DuplicatedMipSize += UsageInfo.DuplicatedMipSize;
							SummaryInfo->StoredOnceMipSize += UsageInfo.StoredOnceMipSize;
							check(SummaryInfo->LODGroup == UsageInfo.LODGroup);
							// Overloading SizeX as count.
							SummaryInfo->SizeX++;
						}
						else
						{
							FCookedTextureUsageInfo SummaryInfo;
							SummaryInfo.DuplicatedMipSize += UsageInfo.DuplicatedMipSize;
							SummaryInfo.StoredOnceMipSize += UsageInfo.StoredOnceMipSize;
							// Overloading SizeX as count.
							SummaryInfo.SizeX = 1;
							SummaryInfo.LODGroup = UsageInfo.LODGroup;
							PackageLODSummaries.Set( PackageLOD, SummaryInfo );
						}
					}
				}

				// Dump stats
				for( TMap<FString,FCookedTextureUsageInfo>::TConstIterator It( PackageLODSummaries ); It; ++It )
				{
					const FString& PackageLOD = It.Key();
					const FCookedTextureUsageInfo& SummaryInfo = It.Value();

					// Unmangle the name.
					FString PackageName;
					FString LODGroup;
					PackageLOD.Split( SplitChar, &PackageName, &LODGroup );

					Table.AddColumn( TEXT("%s"), *PackageName );
					Table.AddColumn( TEXT("%s"), *TextureGroupNames( SummaryInfo.LODGroup ) );					
					Table.AddColumn( TEXT("%i"), SummaryInfo.SizeX ); // Count					
					Table.AddColumn( TEXT("%i"), SummaryInfo.StoredOnceMipSize );
					Table.AddColumn( TEXT("%i"), SummaryInfo.DuplicatedMipSize );
					Table.CycleRow();
				}
				
				Table.Close();
			}
		}
		else
		{
			warnf(TEXT("Could not find persistent cooker data object in file '%s'"),*Tokens(0));
		}
	}
	else
	{
		warnf(TEXT("Please specify full path to persistent cooker info to analyze."));
	}

	return 0;
}
IMPLEMENT_CLASS(UAnalyzeCookedTextureUsageCommandlet)

/*-----------------------------------------------------------------------------
	UAnalyzeCookedTextureDXT5UsageCommandlet
-----------------------------------------------------------------------------*/
/**
 *	Generate the list of DXT5 textures for the given CookerData
 *
 *	@param	CookerData		The cooker data to generate the list from
 */
void UAnalyzeCookedTextureDXT5UsageCommandlet::GenerateDXT5TextureList(const UPersistentCookerData* CookerData)
{
	if (CookerData != NULL)
	{
		// Iterate over texture info and find all single use textures
		for (TMap<FString,FCookedTextureUsageInfo>::TConstIterator It(CookerData->TextureUsageInfos); It; ++It)
		{
			const FString& TextureName = It.Key();
			const FCookedTextureUsageInfo& UsageInfo = It.Value();
			// Ignore various textures we don't care about
			if ((UsageInfo.LODGroup != TEXTUREGROUP_Lightmap) &&
				(UsageInfo.LODGroup != TEXTUREGROUP_RenderTarget) &&
				(UsageInfo.LODGroup != TEXTUREGROUP_MobileFlattened) &&
				(UsageInfo.LODGroup != TEXTUREGROUP_ProcBuilding_Face) &&
				(UsageInfo.LODGroup != TEXTUREGROUP_ProcBuilding_LightMap) &&
				(UsageInfo.LODGroup != TEXTUREGROUP_Shadowmap))
			{
				if (EPixelFormat(UsageInfo.Format) == PF_DXT5)
				{
					// Cheat here, as we know only Texture2D objects are in this list...
					DXT5TextureList.AddUniqueItem(TextureName);
					if (bVerbose == TRUE)
					{
						warnf(NAME_Log, TEXT("Found DXT5 texture: %s"), *TextureName);
					}

					if (bWhiteAlpha == TRUE)
					{
						for (TSet<FString>::TConstIterator PackageIt(UsageInfo.PackageNames); PackageIt; ++PackageIt)
						{
							FString PackageName = *PackageIt;
							TArray<FString>* TextureList = PackageToDXT5TexturesMap.Find(PackageName);
							if (TextureList == NULL)
							{
								TArray<FString> TempList;
								PackageToDXT5TexturesMap.Set(PackageName, TempList);
								TextureList = PackageToDXT5TexturesMap.Find(PackageName);
							}
							check(TextureList);
							TextureList->AddUniqueItem(TextureName);
							if (bVerbose == TRUE)
							{
								warnf(NAME_Log, TEXT("Package %32s, Texture %s"), *PackageName, *TextureName);
							}
						}
					}
				}
			}
		}
	}
}

/**
 *	Process the DXT5 texture list that was generated from the cooker data
 */
#include "UnTextureLayout.h"
void UAnalyzeCookedTextureDXT5UsageCommandlet::ProcessDXT5TextureList()
{
	if (bWhiteAlpha == FALSE)
	{
		for (INT AddIdx = 0; AddIdx < DXT5TextureList.Num(); AddIdx++)
		{
			// Need to tag the 'full' path name on it for collections
			FString FullName = TEXT("Texture2D ");
			FullName += DXT5TextureList(AddIdx);
			TextureAddList.AddUniqueItem(FullName);
		}
	}
	else
	{
		TArray<FString> ProcessedTextures;
		for (TMap<FString,TArray<FString>>::TIterator It(PackageToDXT5TexturesMap); It; ++It)
		{
			FString& PackageName = It.Key();
			TArray<FString>& TextureList = It.Value();

			// Load the package
			warnf(TEXT("Loading %s...."), *PackageName);
			UPackage* Package = LoadPackage(NULL, *PackageName, LOAD_None);
			if (Package != NULL)
			{
				UBOOL bProcess = TRUE;
				if (bMapsOnly == TRUE)
				{
					if (Package->ContainsMap() == FALSE)
					{
						bProcess = FALSE;
					}
				}

				if (bProcess == TRUE)
				{
					for (INT TextureIdx = 0; TextureIdx < TextureList.Num(); TextureIdx++)
					{
						FString TextureName = TextureList(TextureIdx);
						UTexture2D* Texture = FindObject<UTexture2D>(NULL, *TextureName);
						if (Texture != NULL)
						{
							FString TextureFullName = Texture->GetFullName();
							if (bVerbose == TRUE)
							{
								debugf(TEXT("\tLoaded texture %s"), *(Texture->GetPathName()));
							}
							TextureAddList.AddUniqueItem(TextureFullName);

							// Copy the alpha values
							UBOOL bAllWhite = TRUE;
							if (Texture->HasSourceArt() == TRUE)
							{
								TArray<BYTE> SourceArt;
								Texture->GetUncompressedSourceArt(SourceArt);
								check((SourceArt.Num() % 4) == 0);

								TArray<BYTE> SrcAlphas;
								TArray<BYTE> DiffAlphas;
								for (INT Idx = 3; (Idx < SourceArt.Num()) && bAllWhite; Idx += 4)
								{
									SrcAlphas.AddItem(SourceArt(Idx));
								}
								// We will diff against an all white image
								DiffAlphas.AddZeroed(SrcAlphas.Num());
								BYTE* DiffData = DiffAlphas.GetTypedData();
								appMemset(DiffData, 255, SrcAlphas.Num());

								// Compute the difference between the two images
								// @todo: Should compute RMSD per channel and use a separate max RMSD threshold for each
								static TArray< DOUBLE > ValueDifferences;	// Static to reduce heap alloc thrashing

								// Find all white alphas
								ValueDifferences.Reset();
								const BYTE* BytesArrayA = (BYTE*)SrcAlphas.GetData();
								const BYTE* BytesArrayB = (BYTE*)DiffAlphas.GetData();
								const INT ByteCount = SrcAlphas.GetTypeSize() * SrcAlphas.Num();
								TextureLayoutTools::ComputeDifferenceArray(BytesArrayA, BytesArrayB, ByteCount, ValueDifferences);
								// Compute the root mean square deviation for the difference image
								const DOUBLE RMSD = TextureLayoutTools::ComputeRootMeanSquareDeviation(ValueDifferences.GetData(), ValueDifferences.Num());
								if (RMSD > AllowedRMSDForWhite)
								{
									bAllWhite = FALSE;
								}
							}
							else
							{
								//@todo. Add support for this?
								bAllWhite = FALSE;
								warnf(NAME_Log, TEXT("Texture has no source art: %s"), *(Texture->GetPathName()));
							}

							if (bAllWhite == TRUE)
							{
								WhiteAlphaAddList.AddUniqueItem(TextureFullName);
								if (bVerbose == TRUE)
								{
									warnf(NAME_Log, TEXT("\tAllWhite: %s"), *TextureName);
								}
							}
						}
						else
						{
							warnf(NAME_Warning, TEXT("\tFailed to find texture %s"), *TextureName);
						}
					}
				}
				UObject::CollectGarbage(RF_Native);
			}
			else
			{
				warnf(NAME_Warning, TEXT("Failed to load package!"));
			}
		}
	}
}

/**
 *	Update the collections
 */
void UAnalyzeCookedTextureDXT5UsageCommandlet::UpdateCollections()
{
	// If there are textures to add, setup the collection and add them to it
	if (bSkipCollections == FALSE)
	{
		FGADHelper GADHelper;
		GADHelper.Initialize();

		FString CollectionName = TEXT("DXT5 Ref'd Textures");
		if (bRetainCollections == FALSE)
		{
			GADHelper.ClearCollection(CollectionName, EGADCollection::Shared);
		}
		GADHelper.SetCollection(CollectionName, EGADCollection::Shared, TextureAddList);

		FString WhiteAlphaCollectionName = TEXT("DXT5 Ref'd WhiteAlpha Textures");
		if (bRetainCollections == FALSE)
		{
			GADHelper.ClearCollection(WhiteAlphaCollectionName, EGADCollection::Shared);
		}
		GADHelper.SetCollection(WhiteAlphaCollectionName, EGADCollection::Shared, WhiteAlphaAddList);
	}
}

/**
 * Analyzes cooked textures that are only used a single time
 */
INT UAnalyzeCookedTextureDXT5UsageCommandlet::Main( const FString& Params )
{
	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	bWhiteAlpha = Switches.ContainsItem(TEXT("WHITEALPHA"));
	bMapsOnly = Switches.ContainsItem(TEXT("MAPSONLY"));
	bVerbose = Switches.ContainsItem(TEXT("VERBOSE"));
	bRetainCollections = Switches.ContainsItem(TEXT("NOCLEARCOLLECTION"));
	bSkipCollections = Switches.ContainsItem(TEXT("SKIPCOLLECTIONS"));

	// Check to see if we have an explicit RMSD value passed in
	AllowedRMSDForWhite = 1.1;
	for (INT SwitchIdx = 0; SwitchIdx < Switches.Num(); SwitchIdx++)
	{
		FString RMSDVal;
		const FString& CurrentSwitch = Switches( SwitchIdx );
		if (Parse(*CurrentSwitch, TEXT("RMSD="), RMSDVal))
		{
			AllowedRMSDForWhite = appAtof(*RMSDVal);
		}
	}

	PackageToDXT5TexturesMap.Empty();
	DXT5TextureList.Empty();
	TextureAddList.Empty();
	WhiteAlphaAddList.Empty();

	if (Tokens.Num())
	{
		UPersistentCookerData* CookerData = LoadObject<UPersistentCookerData>( NULL, TEXT("GlobalPersistentCookerData.PersistentCookerData"), *Tokens(0), LOAD_None, NULL );
		if (CookerData != NULL)
		{
			GenerateDXT5TextureList(CookerData);
			ProcessDXT5TextureList();
			UpdateCollections();
		}
		else
		{
			warnf(NAME_Warning, TEXT("Could not find persistent cooker data object in file '%s'"),*Tokens(0));
		}
	}
	else
	{
		warnf(NAME_Warning, TEXT("Please specify full path to persistent cooker info to analyze."));
	}

	return 0;
}
IMPLEMENT_CLASS(UAnalyzeCookedTextureDXT5UsageCommandlet);
/*-----------------------------------------------------------------------------
	UAnalyzeCookedTextureSingleUsageCommandlet
-----------------------------------------------------------------------------*/
/**
 *	Generate the list of single use textures for the given CookerData
 *
 *	@param	CookerData		The cooker data to generate the list from
 */
void UAnalyzeCookedTextureSingleUsageCommandlet::GenerateSingleUseTextureList(const UPersistentCookerData* CookerData)
{
	if (CookerData != NULL)
	{
		// Iterate over texture info and find all single use textures
		for (TMap<FString,FCookedTextureUsageInfo>::TConstIterator It(CookerData->TextureUsageInfos); It; ++It)
		{
			const FString& TextureName = It.Key();
			const FCookedTextureUsageInfo& UsageInfo = It.Value();
			if (UsageInfo.PackageNames.Num() == 1)
			{
				// Ignore various textures we don't care about
				if ((UsageInfo.LODGroup != TEXTUREGROUP_Lightmap) &&
					(UsageInfo.LODGroup != TEXTUREGROUP_RenderTarget) &&
					(UsageInfo.LODGroup != TEXTUREGROUP_MobileFlattened) &&
					(UsageInfo.LODGroup != TEXTUREGROUP_ProcBuilding_Face) &&
					(UsageInfo.LODGroup != TEXTUREGROUP_ProcBuilding_LightMap) &&
					(UsageInfo.LODGroup != TEXTUREGROUP_Shadowmap))
				{
					// Insert the texture in the Package to SingleTextures map for processing
					// later. (This is so that the package only has to be loaded once, and
					// all assets of interest in it can be processed at that time.)
					for (TSet<FString>::TConstIterator InnerIt(UsageInfo.PackageNames); InnerIt; ++InnerIt)
					{
						FString PackageName = *InnerIt;
						TArray<FString>* TextureList = PackageToSingleTexturesMap.Find(PackageName);
						if (TextureList == NULL)
						{
							TArray<FString> TempTextureList;
							PackageToSingleTexturesMap.Set(PackageName, TempTextureList);
							TextureList = PackageToSingleTexturesMap.Find(PackageName);
						}
						check(TextureList);
						TextureList->AddUniqueItem(TextureName);
					}
				}
			}
		}
	}
}

/**
 *	Process the single use texture list that was generated from the cooker data
 */
void UAnalyzeCookedTextureSingleUsageCommandlet::ProcessSingleUseTextureList()
{
	// If there are single-usage cases, load them and find the referencers
	for (TMap<FString,TArray<FString>>::TIterator It(PackageToSingleTexturesMap); It; ++It)
	{
		FString PackageName = It.Key();
		TArray<FString>& TextureList = It.Value();

		if (bVerbose == TRUE)
		{
			debugf(TEXT("%s"), *PackageName);
			for (INT TextureIdx = 0; TextureIdx < TextureList.Num(); TextureIdx++)
			{
				debugf(TEXT("\t%s"), *(TextureList(TextureIdx)));
			}
		}

		// Load the package
		warnf(TEXT("Loading %s...."), *PackageName);
		UPackage* Package = LoadPackage(NULL, *PackageName, LOAD_None);
		if (Package != NULL)
		{
			UBOOL bProcess = TRUE;
			if (bMapsOnly == TRUE)
			{
				if (Package->ContainsMap() == FALSE)
				{
					bProcess = FALSE;
				}
			}

			if (bProcess == TRUE)
			{
				for (INT TextureIdx = 0; TextureIdx < TextureList.Num(); TextureIdx++)
				{
					FString TextureName = TextureList(TextureIdx);
					UTexture* Texture = FindObject<UTexture>(NULL, *TextureName);
					if (Texture != NULL)
					{
						// Add the full name to the TextureAddList that will be used
						// to update the 'Single Use Textures' collection
						FString TextureFullName = Texture->GetFullName();
						TextureAddList.AddUniqueItem(TextureFullName);

						if (bVerbose == TRUE)
						{
							debugf(TEXT("\t%s"), *(Texture->GetPathName()));
						}

						// Generate the referencer list for the single use texture
						TArray<FString>* ReferencerList = SingleTextureToReferencersMap.Find(TextureName);
						if (ReferencerList == NULL)
						{
							TArray<FString> Temp;
							SingleTextureToReferencersMap.Set(TextureName, Temp);
							ReferencerList = SingleTextureToReferencersMap.Find(TextureName);
						}
						check(ReferencerList);

						TArray<FReferencerInformation> OutInternalReferencers;
						TArray<FReferencerInformation> OutExternalReferencers;
						Texture->RetrieveReferencers(&OutInternalReferencers, &OutExternalReferencers, FALSE);
						for (INT OIRefIdx = 0; OIRefIdx < OutInternalReferencers.Num(); OIRefIdx++)
						{
							FReferencerInformation& InternalReferencer = OutInternalReferencers(OIRefIdx);
							if (InternalReferencer.Referencer != NULL)
							{
								if (bVerbose == TRUE)
								{
									debugf(TEXT("\t\tINT: %s"), *(InternalReferencer.Referencer->GetFullName()));
								}
								ProcessReferencer(InternalReferencer.Referencer, ReferencerList);
							}
						}

						for (INT OERefIdx = 0; OERefIdx < OutExternalReferencers.Num(); OERefIdx++)
						{
							FReferencerInformation& ExternalReferencer = OutExternalReferencers(OERefIdx);
							if (ExternalReferencer.Referencer != NULL)
							{
								if (bVerbose == TRUE)
								{
									debugf(TEXT("\t\tEXT: %s"), *(ExternalReferencer.Referencer->GetFullName()));
								}
								ProcessReferencer(ExternalReferencer.Referencer, ReferencerList);
							}
						}
					}
					else
					{
						warnf(NAME_Warning, TEXT("\tFailed to find texture %s"), *TextureName);
					}
				}
			}
			UObject::CollectGarbage(RF_Native);
		}
		else
		{
			warnf(NAME_Warning, TEXT("Failed to load package!"));
		}
	}
}

/**
 *	Process the given single use texture referencer
 *
 *	@param	InReferencer		The referencer object
 *	@param	ReferencerList		The SUT-->Referencer list for the current SUT being processed
 */
void UAnalyzeCookedTextureSingleUsageCommandlet::ProcessReferencer(UObject* InReferencer, TArray<FString>* ReferencerList)
{
	FString RefName = InReferencer->GetName();
	// We don't care about material expressions, as the material that contains
	// them will also show up in the list...
	// We also don't want to list the level and/or package as a referencer.
	if ((RefName.InStr(TEXT("MaterialExpression"), FALSE, TRUE) == INDEX_NONE) &&
		(InReferencer->IsA(ULevel::StaticClass()) == FALSE) &&
		(InReferencer->IsA(UPackage::StaticClass()) == FALSE))
	{
		// Add it to the Texture-->Referencers map
		ReferencerList->AddUniqueItem(RefName);
		// Add the full name to the list that will be added to the
		// 'Single Use Textures Referencers' collection
		ReferencerAddList.AddUniqueItem(InReferencer->GetFullName());
		if (bFullRefCsv == TRUE)
		{
			// If the full ref csv file was requested, generate the referencer list
			// for the single use texture referencer
			TArray<FReferencerInformation> RefInternalReferencers;
			TArray<FReferencerInformation> RefExternalReferencers;
			InReferencer->RetrieveReferencers(&RefInternalReferencers, &RefExternalReferencers, FALSE);
			ProcessReferencerReferencerList(RefName, RefInternalReferencers);
			ProcessReferencerReferencerList(RefName, RefExternalReferencers);
		}
	}
}

/**
 *	Process the given list of referencers of the given referencer of a 
 *	single use texture.
 *
 *	@param	InRefName			The name of the single use texture referencer
 *	@param	InReferencersList	The list of referencers of that reference.
 */
void UAnalyzeCookedTextureSingleUsageCommandlet::ProcessReferencerReferencerList(
	const FString& InRefName, const TArray<FReferencerInformation>& InReferencersList)
{
	for (INT RefIdx = 0; RefIdx < InReferencersList.Num(); RefIdx++)
	{
		const FReferencerInformation& TheReferencer = InReferencersList(RefIdx);
		if (TheReferencer.Referencer != NULL)
		{
			FString TheRefName = TheReferencer.Referencer->GetPathName();
			TArray<FString>* FullRefs = FullReferencerMap.Find(InRefName);
			if (FullRefs == NULL)
			{
				TArray<FString> Temp;
				FullReferencerMap.Set(InRefName, Temp);
				FullRefs = FullReferencerMap.Find(InRefName);
			}
			check(FullRefs);
			FullRefs->AddUniqueItem(TheRefName);
		}
	}
}

/**
 *	Update the collections
 */
void UAnalyzeCookedTextureSingleUsageCommandlet::UpdateCollections()
{
	// If there are textures to add, setup the collection and add them to it
	if (bSkipCollections == FALSE)
	{
		FGADHelper GADHelper;
		GADHelper.Initialize();
		FString SingleUseTextureCollectionName = TEXT("Single Use Textures");
		FString SingleUseTextureReferencersCollectionName = TEXT("Single Use Textures Referencers");

		if (bRetainCollections == FALSE)
		{
			GADHelper.ClearCollection(SingleUseTextureCollectionName, EGADCollection::Shared);
		}
		GADHelper.SetCollection(SingleUseTextureCollectionName, EGADCollection::Shared, TextureAddList);
		if (bRetainCollections == FALSE)
		{
			GADHelper.ClearCollection(SingleUseTextureReferencersCollectionName, EGADCollection::Shared);
		}
		GADHelper.SetCollection(SingleUseTextureReferencersCollectionName, EGADCollection::Shared, ReferencerAddList);
	}
}

/**
 *	Generate the CSV file of single use texture referencer referencers
 */
void UAnalyzeCookedTextureSingleUsageCommandlet::GenerateCSVFile()
{
	// Generate the CSV file of the SUT referencer referencers...
	if ((bFullRefCsv == TRUE) && (FullReferencerMap.Num() > 0))
	{
		FString CSVLine;
		// Create string with system time to create a unique filename.
		INT Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec;
		appSystemTime( Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec );
		FString	CurrentTime = FString::Printf(TEXT("%i.%02i.%02i-%02i.%02i.%02i"), Year, Month, Day, Hour, Min, Sec );
		FString CSVFilename = FString::Printf(TEXT("%s%s_SingleUseRefList_%s.csv"), *appGameLogDir(), appGetGameName(), *CurrentTime);
		FArchive* CSVFile = GFileManager->CreateFileWriter(*CSVFilename);

		if (bVerbose == TRUE)
		{
			debugf(TEXT("Ref CSV File generation............"));
		}
		for (TMap<FString,TArray<FString>>::TIterator It(FullReferencerMap); It; ++It)
		{
			FString Referencee = It.Key();
			TArray<FString>& Referencers = It.Value();

			CSVLine = FString::Printf(TEXT("%s"), *Referencee);
			for (INT RefIdx = 0; RefIdx < Referencers.Num(); RefIdx++)
			{
				CSVLine += FString::Printf(TEXT(",%s"), *(Referencers(RefIdx)));
			}
			if (bVerbose == TRUE)
			{
				debugf(*CSVLine);
			}
			CSVLine += LINE_TERMINATOR;
			if (CSVFile)
			{
				CSVFile->Serialize(TCHAR_TO_ANSI(*CSVLine), CSVLine.Len());
			}
		}

		if (CSVFile)
		{
			CSVFile->Close();
			delete CSVFile;
		}
	}
}

/**
 * Analyzes cooked textures that are only used a single time
 */
INT UAnalyzeCookedTextureSingleUsageCommandlet::Main( const FString& Params )
{
	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	bFullRefCsv = Switches.ContainsItem(TEXT("FULLREF"));
	bMapsOnly = Switches.ContainsItem(TEXT("MAPSONLY"));
	bVerbose = Switches.ContainsItem(TEXT("VERBOSE"));
	bRetainCollections = Switches.ContainsItem(TEXT("NOCLEARCOLLECTION"));
	bSkipCollections = Switches.ContainsItem(TEXT("SKIPCOLLECTIONS"));

	if (Tokens.Num())
	{
		UPersistentCookerData* CookerData = LoadObject<UPersistentCookerData>( NULL, TEXT("GlobalPersistentCookerData.PersistentCookerData"), *Tokens(0), LOAD_None, NULL );
		if (CookerData != NULL)
		{
			GenerateSingleUseTextureList(CookerData);
			ProcessSingleUseTextureList();
			UpdateCollections();
			GenerateCSVFile();
		}
		else
		{
			warnf(NAME_Warning, TEXT("Could not find persistent cooker data object in file '%s'"),*Tokens(0));
		}
	}
	else
	{
		warnf(NAME_Warning, TEXT("Please specify full path to persistent cooker info to analyze."));
	}

	return 0;
}
IMPLEMENT_CLASS(UAnalyzeCookedTextureSingleUsageCommandlet)

/*-----------------------------------------------------------------------------
	UTestTextureCompressionCommandlet
-----------------------------------------------------------------------------*/

void TestCompression( const FString& Name, void* IntermediateData, INT Size )
{
	// Temporary archive used to serialize to.
	FBufferArchive BufferArchive( TRUE );

	// LZO
	BufferArchive.Seek(0);
	BufferArchive.SerializeCompressed( IntermediateData, Size, (ECompressionFlags) (COMPRESS_LZO | COMPRESS_BiasMemory) );
	INT CompressedSizeLZO	= BufferArchive.Tell();
	// LZX
	BufferArchive.Seek(0);
	BufferArchive.SerializeCompressed( IntermediateData, Size, COMPRESS_LZX );
	INT CompressedSizeLZX	= BufferArchive.Tell();
	// ZLIB
	BufferArchive.Seek(0);
	BufferArchive.SerializeCompressed( IntermediateData, Size, COMPRESS_ZLIB );
	INT CompressedSizeZLIB	= BufferArchive.Tell();

	// Log data for compression as single block without data reorg.
	debugf(TEXT("Texture: '%s'"),*Name);
	debugf(TEXT("Uncompressed: %i"),Size);
	debugf(TEXT("LZO (size)  : %i"),CompressedSizeLZO);
	debugf(TEXT("LZX (360)   : %i"),CompressedSizeLZX);
	debugf(TEXT("ZLIB        : %i"),CompressedSizeZLIB);			
}

/**
 * Analyzes shader caches and their content.
 */
INT UTestTextureCompressionCommandlet::Main( const FString& Params )
{
	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	// Load in the console support container for Xbox 360.
	FConsoleSupportContainer::GetConsoleSupportContainer()->LoadAllConsoleSupportModules();
	FConsoleSupport* ConsoleSupport = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport(CONSOLESUPPORT_NAME_360);
	if( !ConsoleSupport )
	{
		warnf(TEXT("Couldn't bind to console support DLL."));
		return 0;
	}
	// Create texture cooker object.
	FConsoleTextureCooker* TextureCooker = ConsoleSupport->GetGlobalTextureCooker();
	check(TextureCooker);

	for( INT i=0; i<Tokens.Num(); i++ )
	{
		// Load the texture. Command line token needs to be fully qualified name including package.
		UTexture2D* Texture2D = LoadObject<UTexture2D>( NULL, *Tokens(i), NULL, LOAD_None, NULL );
		if( Texture2D )
		{			
			if( Texture2D->Format != PF_DXT1 )
			{
				debugf( TEXT("Skipping texture '%s' as it is not DXT1"), *Texture2D->GetPathName() );
			}

			// Initialize texture cooker for given format and size. Don't bother with PWL remapping.
			TextureCooker->Init(Texture2D->Format, Texture2D->SizeX, Texture2D->SizeY, Texture2D->Mips.Num(), 0);

			// Ideally would pick top miplevel used on Xbox 360. Hardcoded for now.
			INT MipLevel = Clamp(1,0,Texture2D->Mips.Num()-1);			
			FTexture2DMipMap& Mip = Texture2D->Mips(MipLevel);

			// Allocate enough memory for cooked miplevel.
			UINT MipSize = TextureCooker->GetMipSize( MipLevel );
			// a size of 0 means to use original data size as dest size
			if (MipSize == 0)
			{
				MipSize = Mip.Data.GetBulkDataSize();
			}

			// Allocate cooked/ swizzled/ tiled data.
			void* IntermediateData = appMalloc( MipSize );
			appMemzero(IntermediateData, MipSize);
					
			// Resize upfront to new size to work around issue in Xbox 360 texture cooker reading memory out of bounds.
			// zero-out the newly allocated block of memory as we may not end up using it all.
			Mip.Data.Lock(LOCK_READ_WRITE);
			INT		SizeBeforeRealloc	= Mip.Data.GetBulkDataSize();
			void*	MipData				= Mip.Data.Realloc( MipSize );
			INT		SizeOfReallocRegion = Mip.Data.GetBulkDataSize() - SizeBeforeRealloc;
			if( SizeOfReallocRegion > 0 )
			{
				appMemzero((BYTE*)MipData + SizeBeforeRealloc, SizeOfReallocRegion);
			}

			// Cook the miplevel into the intermediate memory.
			UINT SrcRowPitch = Max<UINT>( 1, (Texture2D->SizeX >> MipLevel) / GPixelFormats[Texture2D->Format].BlockSizeX ) * GPixelFormats[Texture2D->Format].BlockBytes;
			TextureCooker->CookMip( MipLevel, MipData, IntermediateData, SrcRowPitch );

			debugf(TEXT("Regular/ unmodified"));
			TestCompression( *Texture2D->GetPathName(), IntermediateData, MipSize );

			BYTE* Dummy = (BYTE*) appMalloc(MipSize + 100);
			INT DummySize = MipSize + 100;
			UINT Offset = 0;
			{
				// Separate indices from colors and arrange colors in bit planes per component
				TBitArray<> PlaneR[2][5];
				TBitArray<> PlaneG[2][6];
				TBitArray<> PlaneB[2][5];
				TArray<DWORD> Indices;

				FDXT1* DXT1Data = (FDXT1*) IntermediateData;
				for( UINT BlockIndex=0; BlockIndex<(MipSize/sizeof(FDXT1)); BlockIndex++ )
				{
					const FDXT1& SrcDXT1 = DXT1Data[BlockIndex];
					FDXT1 DXT1 = SrcDXT1;
					DXT1.Color[0].Value = BYTESWAP_ORDER16(DXT1.Color[0].Value);
					DXT1.Color[1].Value = BYTESWAP_ORDER16(DXT1.Color[1].Value);
									
					for( INT ColorIndex=0; ColorIndex<2; ColorIndex++ )
					{
						for( INT PlaneIndex=0; PlaneIndex<5; PlaneIndex++ )
						{
							PlaneR[ColorIndex][PlaneIndex].AddItem( ((DXT1.Color[ColorIndex].Color565.R >> PlaneIndex) & 1) == 1 );
							PlaneG[ColorIndex][PlaneIndex].AddItem( ((DXT1.Color[ColorIndex].Color565.G >> PlaneIndex) & 1) == 1 );
							PlaneB[ColorIndex][PlaneIndex].AddItem( ((DXT1.Color[ColorIndex].Color565.B >> PlaneIndex) & 1) == 1 );
						}
						PlaneG[ColorIndex][5].AddItem( ((DXT1.Color[ColorIndex].Color565.G >> 5) & 1) == 1 );
					}

					Indices.AddItem(DXT1.Indices);
				}

				Offset = 0;
				appMemzero(Dummy,DummySize);
				appMemcpy( Dummy + Offset, Indices.GetData(), Indices.Num() * 4 );
				Offset += Indices.Num() * 4;

				for( INT ColorIndex=0; ColorIndex<2; ColorIndex++ )
				{
					for( INT PlaneIndex=0; PlaneIndex<5; PlaneIndex++ )
					{
						appMemcpy( Dummy + Offset, PlaneR[ColorIndex][PlaneIndex].GetData(), PlaneR[ColorIndex][PlaneIndex].Num() / 8 );
						Offset += PlaneR[ColorIndex][PlaneIndex].Num() / 8;

						appMemcpy( Dummy + Offset, PlaneG[ColorIndex][PlaneIndex].GetData(), PlaneG[ColorIndex][PlaneIndex].Num() / 8 );
						Offset += PlaneG[ColorIndex][PlaneIndex].Num() / 8;

						appMemcpy( Dummy + Offset, PlaneB[ColorIndex][PlaneIndex].GetData(), PlaneB[ColorIndex][PlaneIndex].Num() / 8 );
						Offset += PlaneB[ColorIndex][PlaneIndex].Num() / 8;

					}
					appMemcpy( Dummy + Offset, PlaneG[ColorIndex][5].GetData(), PlaneG[ColorIndex][5].Num() / 8 );
					Offset += PlaneG[ColorIndex][5].Num() / 8;
				}
				check(Offset < MipSize + 100);
				check(Offset >= MipSize);
			}
			debugf(TEXT("Separating indices and storing colors as bit planes."));
			TestCompression( *Texture2D->GetPathName(), Dummy, MipSize );

			{
				// Separate indices from colors.
				TArray<DWORD> Colors;
				TArray<DWORD> Indices;

				FDXT1* DXT1Data = (FDXT1*) IntermediateData;
				for( UINT BlockIndex=0; BlockIndex<(MipSize/sizeof(FDXT1)); BlockIndex++ )
				{
					const FDXT1& DXT1 = DXT1Data[BlockIndex];
					Colors.AddItem(DXT1.Colors);
					Indices.AddItem(DXT1.Indices);
				}

				Offset = 0;
				appMemzero(Dummy,DummySize);
				appMemcpy( Dummy + Offset, Indices.GetData(), Indices.Num() * 4 );
				Offset += Indices.Num() * 4;
				appMemcpy( Dummy + Offset, Colors.GetData(), Colors.Num() * 4 );
				Offset += Colors.Num() * 4;
			}
			debugf(TEXT("Only separating indices"));
			TestCompression( *Texture2D->GetPathName(), Dummy, MipSize );

			// Clean up pointers/ unlock bulk data.
			appFree( Dummy );
			appFree( IntermediateData );
			Mip.Data.Unlock();
		}
		else
		{
			warnf(TEXT("Could not find texture '%s'"),*Tokens(i));
		}
	}

	return 0;
}
IMPLEMENT_CLASS(UTestTextureCompressionCommandlet)



/*-----------------------------------------------------------------------------
	UFindUnreferencedFunctionsCommandlet
-----------------------------------------------------------------------------*/

/**
 * Dummy archive for use by UByteCodeSerializer
 */
class FArchiveFunctionMarker : public FArchive
{
public:
	FArchiveFunctionMarker( class UByteCodeSerializer* InByteCodeSerializer, INT& iCode )
	: FArchive(), CurrentCodePosition(iCode), ByteCodeSerializer(InByteCodeSerializer)
	{}

	/**
  	 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	 * is in when a loading error occurs.
	 *
	 * This is overridden for the specific Archive Types
	 **/
	virtual FString GetArchiveName() const { return TEXT("FArchiveFunctionMarker"); }

	virtual FArchive& operator<<( class FName& N )
	{
		// only function names should be sent to this archive
		return *this;
	}
	virtual FArchive& operator<<( class UObject*& Res )
	{
		return *this;
	}
	virtual INT Tell()
	{
		return CurrentCodePosition;
	}
	virtual INT TotalSize()
	{
		return ByteCodeSerializer->Script.Num();
	}
	virtual void Seek( INT InPos )
	{
		CurrentCodePosition = InPos;
	}

private:

	INT& CurrentCodePosition;
	class UByteCodeSerializer* ByteCodeSerializer;
};


/**
 * Entry point for processing a single struct's bytecode array.  Calls SerializeExpr to process the struct's bytecode.
 *
 * @param	InParentStruct	the struct containing the bytecode to be processed.
 */
void UByteCodeSerializer::ProcessStructBytecode( UStruct* InParentStruct )
{
	/*
	Unhandled references:
	- Parameter types (delegate<MyTestActor.OnWriteProfileSettingsComplete)
	- default properties
	- references through named timers
	*/
	ParentStruct = InParentStruct;
	if ( ParentStruct != NULL && ParentStruct != this )
	{
		// make ParentStruct our Outer to make searching through scopes easier
		Rename(NULL, ParentStruct, REN_ForceNoResetLoaders);
		// copy the struct's script to a local copy so we don't have to keep referencing the ParentStruct
		Script = ParentStruct->Script;
	
// 
// 		if ( ParentStruct->GetName() == TEXT("TestIt")
// 		&&	ParentStruct->GetOuter()->GetName() == TEXT("MyTestActor2") )
// 		{
// 			int i = 0;
// 		}

		INT iCode = 0;
		FArchiveFunctionMarker FunctionMarkerAr(this, iCode);
		while ( iCode < Script.Num() )
		{
			CurrentContext = NULL;
			SerializeExpr( iCode, FunctionMarkerAr );
		}
	}

	CurrentContext = NULL;
}

/**
 * Processes compiled bytecode for UStructs, marking any UFunctions encountered as referenced.
 *
 * @param	iCode	the current position in the struct's bytecode array.
 * @param	Ar		the archive used for serializing the bytecode; not really necessary for this purpose.
 *
 * @return	the token that was parsed from the bytecode stream.
 */
EExprToken UByteCodeSerializer::SerializeExpr( INT& iCode, FArchive& Ar )
{
	FName NextFunctionName;
	UFunction* NextFunction=NULL;

	#define XFER(Type)				{ iCode += sizeof(Type); }
	#define XFER_FUNC_NAME			{ NextFunctionName = *(FName*)&Script(iCode); XFER(FName) }
	#define XFER_FUNC_POINTER		{ ScriptPointerType TempCode = *(ScriptPointerType*)&Script(iCode); XFER(ScriptPointerType) NextFunction = (UFunction*)appSPtrToPointer(TempCode); }
	#define XFER_PROP_POINTER		{ ScriptPointerType TempCode = *(ScriptPointerType*)&Script(iCode); XFER(ScriptPointerType) GProperty	 = (UProperty*)appSPtrToPointer(TempCode); SetCurrentContext(); }
	#define XFER_OBJECT_POINTER(T)	{ ScriptPointerType TempCode = *(ScriptPointerType*)&Script(iCode); XFER(ScriptPointerType) GPropObject	 = (UObject*)appSPtrToPointer(TempCode); CurrentContext = Cast<UClass>(GPropObject); if ( CurrentContext == NULL ) { CurrentContext = GPropObject->GetClass(); } }

	#define	XFERPTR(T) XFER(ScriptPointerType)
	#define XFERNAME() XFER(FName)

	// include this file to define any remaining symbols
	#include "ScriptSerialization.h"

	EExprToken ExpressionToken = EX_Max;

	// Get expression token.
	XFER(BYTE);
	ExpressionToken = (EExprToken)Script(iCode-1);
	if( ExpressionToken >= EX_FirstNative )
	{
		UFunction* NextFunction = GetIndexedNative(ExpressionToken);
		check(NextFunction);
		MarkFunctionAsReferenced(NextFunction);

		// Native final function with id 1-127.
		UClass* Context = CurrentContext;
		while( SerializeExpr( iCode, Ar ) != EX_EndFunctionParms )
		{
			// these native functions are the only ones where we want to restore the original context
			CurrentContext = Context;
		}
		HANDLE_OPTIONAL_DEBUG_INFO;
	}
	else if( ExpressionToken >= EX_ExtendedNative )
	{
		// Native final function with id 256-16383.
		BYTE B = Script(iCode);
		XFER(BYTE);

		// this is the formula for calculating the function's actual function index
		INT FuncIndex = (ExpressionToken - EX_ExtendedNative) * 0x100 + B;

		// find the function with this value for iNative
		UFunction* NextFunction = GetIndexedNative(FuncIndex);
		check(NextFunction);
		MarkFunctionAsReferenced(NextFunction);

		// only keep the current context if this function is an operator
		UClass* Context = NextFunction->HasAnyFunctionFlags(FUNC_PreOperator|FUNC_Operator) ? CurrentContext : NULL;

		// the current context doesn't matter to the parameters (since each one must be evaluated starting with a context of the current class), but
		// the code can attempt to call a function through the return value of a function (i.e. SomeFuncWithObjectReturnValue().SomeOtherFunction()),
		// so we need to set the current context to the function's return value so that it will be restored when we exit (due to TGuardValue)
		SetCurrentContext(NextFunction->GetReturnProperty());
		NextFunction = NULL;

		TGuardValue<UClass*> RestoreContext(CurrentContext, NULL);
		while( SerializeExpr( iCode, Ar ) != EX_EndFunctionParms )
		{
			CurrentContext = Context;
		}
		HANDLE_OPTIONAL_DEBUG_INFO;
	}
	else switch( ExpressionToken )
	{
		case EX_MetaCast:
		{
			XFER_OBJECT_POINTER(UClass*);
			TGuardValue<UClass*> RestoreContext(CurrentContext, NULL);
			SerializeExpr( iCode, Ar );
			break;
		}
		case EX_DynamicCast:
		{
			XFER_OBJECT_POINTER(UClass*);
			TGuardValue<UClass*> RestoreContext(CurrentContext, NULL);
			SerializeExpr( iCode, Ar );
			break;
		}
		case EX_FinalFunction:
		{
			XFER_FUNC_POINTER;											// Function pointer
			check(NextFunction);

			MarkFunctionAsReferenced(NextFunction);

			// the current context doesn't matter to the parameters (since each one must be evaluated starting with a context of the current class), but
			// the code can attempt to call a function through the return value of a function (i.e. SomeFuncWithObjectReturnValue().SomeOtherFunction()),
			// so we need to set the current context to the function's return value so that it will be restored when we exit (due to TGuardValue)
			SetCurrentContext(NextFunction->GetReturnProperty());
			NextFunction = NULL;

			// the current context must be restored each time we begin processing a new parameter so that object variable parameters aren't mistakenly used as the context for the
			// the next parameter's value (which might contain a function call)
			TGuardValue<UClass*> RestoreContext(CurrentContext, NULL);
			while( SerializeExpr( iCode, Ar ) != EX_EndFunctionParms )
			{
				CurrentContext = NULL;
			}
			HANDLE_OPTIONAL_DEBUG_INFO;

			break;
		}
		case EX_VirtualFunction:
		case EX_GlobalFunction:
		{
			XFER_FUNC_NAME;												// Virtual function name.
			if ( NextFunctionName != NAME_None )
			{
				UStruct* Context = CurrentContext;
				if ( Context == NULL )
				{
					Context = ParentStruct;
				}
				NextFunction = FindFieldChecked<UFunction>(Context, NextFunctionName);
				MarkFunctionAsReferenced(NextFunction);

				// the current context doesn't matter to the parameters (since each one must be evaluated starting with a context of the current class), but
				// the code can attempt to call a function through the return value of a function (i.e. SomeFuncWithObjectReturnValue().SomeOtherFunction()),
				// so we need to set the current context to the function's return value so that it will be restored when we exit (due to TGuardValue)
				SetCurrentContext(NextFunction->GetReturnProperty());
			}

			// the current context must be restored each time we begin processing a new parameter so that object variable parameters aren't mistakenly used as the context for the
			// the next parameter's value (which might contain a function call)
			TGuardValue<UClass*> RestoreContext(CurrentContext, NULL);
			while( SerializeExpr( iCode, Ar ) != EX_EndFunctionParms )
			{
				// each time SerializeExpr returns, it means we've finished parsing one complete parameter value
				CurrentContext = NULL;
			}
			HANDLE_OPTIONAL_DEBUG_INFO;

			break;
		}
		// script call to a delegate by invoking the delegate function directly
		case EX_DelegateFunction:
		{
			// indicates whether this is a local property
			XFER(BYTE);
			// Delegate property
			XFER_PROP_POINTER;

			// Name of the delegate function (in case no functions are assigned to the delegate)
			XFER_FUNC_NAME;
			if ( NextFunctionName != NAME_None )
			{
				UStruct* Context = CurrentContext;
				if ( Context == NULL )
				{
					Context = ParentStruct;
				}
				NextFunction = FindFieldChecked<UFunction>(Context, NextFunctionName);
				MarkFunctionAsReferenced(NextFunction);

				// the current context doesn't matter to the parameters (since each one must be evaluated starting with a context of the current class), but
				// the code can attempt to call a function through the return value of a function (i.e. SomeFuncWithObjectReturnValue().SomeOtherFunction()),
				// so we need to set the current context to the function's return value so that it will be restored when we exit (due to TGuardValue)
				SetCurrentContext(NextFunction->GetReturnProperty());
			}

			// the current context must be restored each time we begin processing a new parameter so that object variable parameters aren't mistakenly used as the context for the
			// the next parameter's value (which might contain a function call)
			TGuardValue<UClass*> RestoreContext(CurrentContext, NULL);
			while( SerializeExpr( iCode, Ar ) != EX_EndFunctionParms )
			{
				CurrentContext = NULL;
			}
			HANDLE_OPTIONAL_DEBUG_INFO;

			break;
		}
		case EX_DelegateProperty:
		{
            // Name of function we're assigning to the delegate.
            XFER_FUNC_NAME;
           
            UClass* OldContext = CurrentContext;
            XFER_PROP_POINTER;    // delegate property corresponding to the function we're assigning (if any)
            // discard the incorrect context from XFER_PROP_POINTER
            CurrentContext = OldContext;

            if ( NextFunctionName != NAME_None )
            {
                UStruct* Context = CurrentContext;
                if ( Context == NULL )
                {
                    Context = ParentStruct;
                }

                NextFunction = FindFieldChecked<UFunction>(Context, NextFunctionName);
                MarkFunctionAsReferenced(NextFunction);
            }
            break;
        }
		case EX_InstanceDelegate:
		{
			// Name of function we're assigning to the delegate.
			XFER_FUNC_NAME;
			if ( NextFunctionName != NAME_None )
			{
				UStruct* Context = CurrentContext;
				if ( Context == NULL )
				{
					Context = ParentStruct;
				}

				NextFunction = FindFieldChecked<UFunction>(Context, NextFunctionName);
				MarkFunctionAsReferenced(NextFunction);
			}
			break;
		}

		case EX_Let:
		case EX_LetBool:
		case EX_LetDelegate:
		{
			SerializeExpr( iCode, Ar ); // l-value

			// be sure to clear out the current context before evaluating the next expression
			CurrentContext = NULL;
			SerializeExpr( iCode, Ar ); // r-value
			break;
		}
		case EX_StructMember:
		{
			XFER_PROP_POINTER;			// the struct property we're accessing
			XFERPTR(UStruct*);			// the struct which contains the property (allows derived structs to be used in expressions with base structs)
			XFER(BYTE);					// byte indicating whether a local copy of the struct must be created in order to access the member property
			XFER(BYTE);					// byte indicating whether the struct member will be modified by the expression it's being used in

			// need to restore context because this call to SerializeExpr will process the part of the expression that came BEFORE the struct member property
			// i.e. SomeStruct.SomeMember =>  XFER_PROP_POINTER processed "SomeMember", this next call to SerializeExpr will process "SomeStruct."
			TGuardValue<UClass*> RestoreContext(CurrentContext, CurrentContext);
			SerializeExpr( iCode, Ar );	// expression corresponding to the struct member property.
			break;
		}
		case EX_EqualEqual_DelDel:
		case EX_NotEqual_DelDel:
		case EX_EqualEqual_DelFunc:
		case EX_NotEqual_DelFunc:
		{
			TGuardValue<UClass*> RestoreContext(CurrentContext, NULL);
			while( SerializeExpr( iCode, Ar ) != EX_EndFunctionParms )
			{
				CurrentContext = NULL;
			}
			HANDLE_OPTIONAL_DEBUG_INFO;
			break;
		}
		case EX_DynArrayAddItem:
        case EX_DynArrayRemoveItem:
        {
            SerializeExpr( iCode, Ar ); // Array property expression
            XFER(CodeSkipSizeType);        // Number of bytes to skip if NULL context encountered

            TGuardValue<UClass*> RestoreContext(CurrentContext, NULL);
            SerializeExpr( iCode, Ar ); // Item
            break;
        }
		default:
		{
			// back up the code pointer so that we can leave the standard code as is
			iCode -= sizeof(BYTE);

#define SERIALIZEEXPR_INC
			#include "ScriptSerialization.h"
#undef SERIALIZEEXPR_INC
			
			ExpressionToken = Expr;
			break;
		}
	}

	return ExpressionToken;

#undef XFER
#undef XFERPTR
#undef XFERNAME
#undef XFER_LABELTABLE
#undef HANDLE_OPTIONAL_DEBUG_INFO
#undef XFER_FUNC_POINTER
#undef XFER_FUNC_NAME
#undef XFER_PROP_POINTER
}

/**
 * Determines the correct class context based on the specified property.
 *
 * @param	ContextProperty		a property which potentially changes the current lookup context, such as object or interface property
 *
 * @return	if ContextProperty corresponds to an object, interface, or delegate property, result is a pointer to the class associated
 *			with the property (e.g. for UObjectProperty, would be PropertyClass); NULL if no context switch will happen as a result of
 *			processing ContextProperty
 */
UClass* UByteCodeSerializer::DetermineCurrentContext( UProperty* ContextProperty ) const
{
	UClass* NewContext = NULL;

	if ( ContextProperty != NULL )
	{
		UInterfaceProperty* InterfaceProp = SmartCastProperty<UInterfaceProperty>(ContextProperty);
		if ( InterfaceProp != NULL )
		{
			NewContext = InterfaceProp->InterfaceClass;
		}
		else
		{
			UObjectProperty* ObjectProp = SmartCastProperty<UObjectProperty>(ContextProperty);
			if ( ObjectProp != NULL )
			{
				// special case for Outer variable
				if ( ObjectProp->GetFName() == NAME_Outer )
				{
					UClass* Context = CurrentContext;
					if ( Context == NULL )
					{
						Context = ParentStruct->GetOwnerClass();
					}
					if ( Context != NULL && Context->ClassWithin != NULL && Context->ClassWithin != UObject::StaticClass() )
					{
						NewContext = Context->ClassWithin;
					}
					else
					{
						NewContext = NULL;
					}
				}
				else
				{
					UClassProperty* ClassProp = Cast<UClassProperty>(ObjectProp);
					NewContext = ClassProp != NULL ? ClassProp->MetaClass : ObjectProp->PropertyClass;
				}
			}
			else
			{
				UDelegateProperty* DelProp = SmartCastProperty<UDelegateProperty>(ContextProperty);
				if ( DelProp != NULL )
				{
					if ( DelProp->SourceDelegate == NULL )
					{
						// if this delegate property doesn't have a SourceDelegate set, then this property is the compiler-generated
						// property for an actual delegate function (i.e. __SomeDelegate__Delegate)
						check(DelProp->Function);
						NewContext = DelProp->Function->GetOwnerClass();
					}
					else
					{
						NewContext = DelProp->SourceDelegate->GetOwnerClass();
					}
				}
				else
				{
					NewContext = NULL;
				}
			}
		}
	}
	else
	{
		NewContext = NULL;
	}

	return NewContext;
}

/**
 * Sets the value of CurrentContext based on the results of calling DetermineCurrentContext()
 *
 * @param	ContextProperty		a property which potentially changes the current lookup context, such as object or interface property
 */
void UByteCodeSerializer::SetCurrentContext( UProperty* ContextProperty/*=GProperty*/ )
{
	CurrentContext = DetermineCurrentContext(ContextProperty);
}

/**
 * Marks the specified function as referenced by finding the UFunction corresponding to its original declaration.
 *
 * @param	ReferencedFunction	the function that should be marked referenced
 */
void UByteCodeSerializer::MarkFunctionAsReferenced( UFunction* ReferencedFunction )
{
	FindOriginalDeclaration(ReferencedFunction)->SetFlags(RF_Marked);
}

/**
 * Wrapper for checking whether the specified function is referenced.  Uses the UFunction corresponding to
 * the function's original declaration.
 */
UBOOL UByteCodeSerializer::IsFunctionReferenced( UFunction* FunctionToCheck )
{
	return FindOriginalDeclaration(FunctionToCheck)->HasAnyFlags(RF_Marked);
}

/**
 * Searches for the UFunction corresponding to the original declaration of the specified function.
 *
 * @param	FunctionToCheck		the function to lookup
 *
 * @return	a pointer to the original declaration of the specified function.
 */
UFunction* UByteCodeSerializer::FindOriginalDeclaration( UFunction* FunctionToCheck )
{
	UFunction* Result = NULL;

	if ( FunctionToCheck != NULL )
	{
		UFunction* SuperFunc = FunctionToCheck->GetSuperFunction();
		while ( SuperFunc != NULL )
		{
			Result = SuperFunc;
			SuperFunc = SuperFunc->GetSuperFunction();
		}

		if ( Result == NULL )
		{
			Result = FunctionToCheck;
		}

		UState* OuterScope = Result->GetOuterUState();
		if ( Cast<UClass>(OuterScope) == NULL )
		{
			// this function is declared within a state; see if we have a global version
			UClass* OwnerClass = Result->GetOwnerClass();
			UFunction* GlobalFunc = FindField<UFunction>(OwnerClass, Result->GetFName());
			if ( GlobalFunc != NULL )
			{
				check(GlobalFunc != Result);
				SuperFunc = GlobalFunc->GetSuperFunction();
				while ( SuperFunc != NULL )
				{
					Result = SuperFunc;
					SuperFunc = SuperFunc->GetSuperFunction();
				}
			}
		}
	}

	return Result;
}

/**
 * Find the function associated with the native index specified.
 *
 * @param	iNative		the native function index to search for
 *
 * @return	a pointer to the UFunction using the specified native function index.
 */
UFunction* UByteCodeSerializer::GetIndexedNative( INT iNative )
{
	UFunction* Result = NativeFunctionIndexMap.FindRef(iNative);
	if ( Result == NULL )
	{
		for ( TObjectIterator<UFunction> It; It; ++It )
		{
			if ( It->iNative == iNative )
			{
				Result = *It;
				NativeFunctionIndexMap.Set(iNative, Result);
				break;
			}
		}
	}

	return Result;
}


/**
 * Find the original function declaration from an interface class implemented by FunctionOwnerClass.
 *
 * @param	FunctionOwnerClass	the class containing the function being looked up.
 * @param	Function			the function being looked up
 *
 * @return	if Function is an implementation of a function declared in an interface class implemented by FunctionOwnerClass,
 *			returns a pointer to the function from the interface class; NULL if Function isn't an implementation of an interface
 *			function
 */
UFunction* UFindUnreferencedFunctionsCommandlet::GetInterfaceFunctionDeclaration( UClass* FunctionOwnerClass, UFunction* Function )
{
	check(FunctionOwnerClass);
	check(Function);

	UFunction* Result = NULL;

	for (UClass* CurrentClass = FunctionOwnerClass; Result == NULL && CurrentClass; CurrentClass = CurrentClass->GetSuperClass())
	{
		for (TArray<FImplementedInterface>::TIterator It(CurrentClass->Interfaces); It; ++It)
		{
			UClass* ImplementedInterfaceClass = It->Class;
			Result = FindField<UFunction>(ImplementedInterfaceClass, Function->GetFName());
			if (Result != NULL)
			{
				break;
			}
		}
	}

	return Result;
}

/**
 * This class is used to find which objects reference any element from a list of "TargetObjects".  When invoked,
 * it will generate a mapping of each target object to an array of objects referencing that target object.
 *
 * Each key corresponds to an element of the input TargetObjects array which was referenced
 * by some other object.  The values for these keys are the objects which are referencing them.
 */
class FArchiveFunctionReferenceCollector : public FArchive
{
public:

	/**
	 * Default constructor
	 *
	 * @param	TargetObjects	the list of objects to find references to
	 * @param	PackageToCheck	if specified, only objects contained in this package will be searched
	 *							for references to 
	 */
	FArchiveFunctionReferenceCollector()
	: bSerializingDelegateProperty(FALSE), CurrentObj(NULL), CurrentContext(NULL)
	{
		AllowEliminatingReferences(FALSE);
		ArIgnoreArchetypeRef = TRUE;
		ArIgnoreClassRef = TRUE;
		ArIgnoreOuterRef = TRUE;

		ArIsObjectReferenceCollector = TRUE;
	}

	void ProcessTemplateObjects()
	{
		// find all CDOs and subobject templates
		for ( FObjectIterator It; It; ++It )
		{
			if ( It->IsTemplate(RF_ClassDefaultObject) )
			{
				TemplateObjects.AddItem(*It);
			}
		}

		// serialize each template object
		for ( INT ObjIndex = 0; ObjIndex < TemplateObjects.Num(); ObjIndex++ )
		{
			CurrentObj = TemplateObjects(ObjIndex);
			CurrentObj->Serialize(*this);
		}

		// now mark all functions we encountered as referenced
		for ( INT FuncIndex = 0; FuncIndex < ReferencedFunctions.Num(); FuncIndex++ )
		{
			UFunction* FunctionObject = ReferencedFunctions(FuncIndex);
			if ( !UByteCodeSerializer::IsFunctionReferenced(FunctionObject) )
			{
				UByteCodeSerializer::MarkFunctionAsReferenced(FunctionObject);
			}
		}
	}

	/* === FArchive interface === */
	virtual FArchive& operator<<( FName& Name )
	{
		if ( bSerializingDelegateProperty )
		{
			if ( Name != NAME_None )
			{
				UObject* ContextObj = CurrentContext != NULL ? CurrentContext : CurrentObj;
				UClass* Scope = Cast<UClass>(ContextObj);
				if ( Scope == NULL )
				{
					Scope = ContextObj->GetClass();
				}

				UFunction* FunctionObj = FindFieldChecked<UFunction>(Scope, Name);
				ReferencedFunctions.AddItem(FunctionObj);
			}

			CurrentContext = NULL;
			bSerializingDelegateProperty = FALSE;
		}
		return *this;
	}

	/**
	 * Serializes the reference to the object
	 */
	virtual FArchive& operator<<( UObject*& Obj )
	{
		UFunction* FunctionObject = Cast<UFunction>(Obj);
		if ( FunctionObject != NULL )
		{
			ReferencedFunctions.AddItem(FunctionObject);
		}
		else if ( GSerializedProperty != NULL )
		{
			UDelegateProperty* DelProp = Cast<UDelegateProperty>(GSerializedProperty);
			if ( DelProp != NULL )
			{
				bSerializingDelegateProperty = TRUE;
				CurrentContext = Obj;
			}
		}

		return *this;
	}

	/**
  	 * Returns the name of this archive.
	 **/
	virtual FString GetArchiveName() const { return TEXT("FArchiveFunctionReferenceCollector"); }

private:
	UBOOL bSerializingDelegateProperty;

	/** the object currently being serialized */
	UObject* CurrentObj;

	/** the object that contains the function reference (usually relevant for delegates) */
	UObject* CurrentContext;

	/** The list of objects which are either class default objects, or contained within class default objects */
	TArray<UObject*> TemplateObjects;

	/** the list of functions encountered while processing the template objects */
	TLookupMap<UFunction*> ReferencedFunctions;
};

/**
 * Commandlet entry point
 *
 * @param	Params	the command line parameters that were passed in.
 *
 * @return	0 if the commandlet succeeded; otherwise, an error code defined by the commandlet.
 */
INT UFindUnreferencedFunctionsCommandlet::Main( const FString& Params )
{
	const TCHAR* CmdLine = *Params;

	TArray<FString> Tokens, Switches;
	ParseCommandLine(CmdLine, Tokens, Switches);

	Serializer = ConstructObject<UByteCodeSerializer>(UByteCodeSerializer::StaticClass());

	TArray<FString> PackageNames;
	TArray<FFilename> PackageFilenames;
	if ( !NormalizePackageNames(PackageNames, PackageFilenames, TEXT("*.u"), 0) )
	{
		warnf(TEXT("No files found using wildcard *.u"));
		return 0;
	}

	warnf(TEXT("Found %i files"), PackageFilenames.Num());
	for ( INT FileIndex = 0; FileIndex < PackageFilenames.Num(); FileIndex++ )
	{
		warnf(TEXT("Loading %s...."), *PackageFilenames(FileIndex));
		LoadPackage(NULL, *PackageFilenames(FileIndex), LOAD_None);
	}

	for ( TObjectIterator<UStruct> It; It; ++It )
	{
		Serializer->ProcessStructBytecode(*It);
	}

	// now gather references from class defaults
	FArchiveFunctionReferenceCollector FunctionReferenceCollector;
	FunctionReferenceCollector.ProcessTemplateObjects();

	//@todo - add support for switches
	/*
	- shownatives (include native functions in results)
	- nativesonly (only include native functions in results)
	- showdelegates
	- delegatesonly
	- showevents
	- eventsonly
	*/

	warnf(TEXT("Unreferenced functions:"));
	INT UnreferencedFunctionCount = 0;
	INT NativeFunctionCount = 0, UnreferencedNativeFunctionCount=0;
	INT EventCount = 0, UnreferencedEventCount=0;
	INT DelegateCount = 0, UnreferencedDelegateCount=0;
	INT ExecFunctionCount = 0;
	for ( TObjectIterator<UFunction> It; It; ++It )
	{
		UFunction* Function = *It;
		UClass* FunctionOwnerClass = Function->GetOwnerClass();
		if ( !FunctionOwnerClass->HasAnyClassFlags(CLASS_Interface) )
		{
			if ( Function->HasAnyFunctionFlags(FUNC_Event|FUNC_Native|FUNC_Delegate|FUNC_Operator|FUNC_PreOperator|FUNC_Exec) )
			{
				UBOOL bIsReferencedByScript = TRUE;
				if ( !Serializer->IsFunctionReferenced(Function) )
				{
					UFunction* InterfaceParent = GetInterfaceFunctionDeclaration(FunctionOwnerClass, Function);
					if ( InterfaceParent == NULL || !Serializer->IsFunctionReferenced(InterfaceParent) )
					{
						bIsReferencedByScript = FALSE;
					}
				}

				if ( Function->HasAnyFunctionFlags(FUNC_Native) )
				{
					NativeFunctionCount++;
					if ( !bIsReferencedByScript )
					{
						UnreferencedNativeFunctionCount++;
					}
				}
				else if ( Function->HasAnyFunctionFlags(FUNC_Event) )
				{
					EventCount++;
					if ( !bIsReferencedByScript )
					{
						UnreferencedEventCount++;
					}
				}
				else if ( Function->HasAnyFunctionFlags(FUNC_Delegate) )
				{
					DelegateCount++;
					if ( !bIsReferencedByScript )
					{
						UnreferencedDelegateCount++;
					}
				}
				else if ( Function->HasAnyFunctionFlags(FUNC_Exec) )
				{
					ExecFunctionCount++;
				}
			}
			else
			{
				if ( !Serializer->IsFunctionReferenced(Function) )
				{
					UFunction* InterfaceParent = GetInterfaceFunctionDeclaration(FunctionOwnerClass, Function);
					if ( InterfaceParent == NULL || !Serializer->IsFunctionReferenced(InterfaceParent) )
					{
						UnreferencedFunctionCount++;
						warnf(TEXT("    %s"), *It->GetFullName());
					}
				}
			}
		}
	}

	warnf(TEXT("%i functions unreferenced by this game"), UnreferencedFunctionCount);
	warnf(TEXT("%i unreported native functions (%i not referenced by script)"), NativeFunctionCount, UnreferencedNativeFunctionCount);
	warnf(TEXT("%i unreported events (%i not referenced by script)"), EventCount, UnreferencedEventCount);
	warnf(TEXT("%i unreported delegates (%i not referenced by script)"), DelegateCount, UnreferencedDelegateCount);
	warnf(TEXT("%i unreported exec functions"), ExecFunctionCount);

	warnf(TEXT("%i total functions not referenced by script"), UnreferencedFunctionCount + UnreferencedNativeFunctionCount + UnreferencedEventCount + UnreferencedDelegateCount);

	return 0;
}

IMPLEMENT_CLASS(UFindUnreferencedFunctionsCommandlet);
IMPLEMENT_CLASS(UByteCodeSerializer);

//
//
//
/**
 *	Base commandlet for processing objects contained in the collection
 *	generated by the TagCookedReferencedAssets commandlet (ie, object
 *	that are known to be 'OnDVD')
 */
IMPLEMENT_CLASS(UBaseCollectionProcessingCommandlet);

// This is also in UnContentCookers... should consolidate them somewhere
IMPLEMENT_COMPARE_CONSTREF(FString, UContentCommandletsFullNameSort, \
	{ \
		FString TempA = A; \
		FString TempB = B; \
		INT SpaceIdx = TempA.InStr(TEXT(" ")); \
		if (SpaceIdx != INDEX_NONE) \
		{ \
			TempA = TempA.Right(TempA.Len() - (SpaceIdx + 1)); \
		} \
		SpaceIdx = TempB.InStr(TEXT(" ")); \
		if (SpaceIdx != INDEX_NONE) \
		{ \
			TempB = TempB.Right(TempB.Len() - (SpaceIdx + 1)); \
		} \
		return appStricmp(*TempA, *TempB); \
	})

/**
 *	Startup the commandlet.
 *
 *	@return	UBOOL	TRUE if successful, FALSE if not
 */
UBOOL UBaseCollectionProcessingCommandlet::Startup()
{
	// There MUST be a switch that identifies the platform
	PlatformType = ParsePlatformType(appCmdLine());
	if (PlatformType == UE3::PLATFORM_Unknown)
	{
		warnf(NAME_Warning, TEXT("Unknown platform specified. Use: -PLATFORM=<Xbox360/PS3/PC/etc.>"));
		return FALSE;
	}

	bSaveShaderCaches = (Switches.FindItemIndex(TEXT("SAVESHADERCACHES")) != INDEX_NONE);
	warnf(NAME_Log, TEXT("SaveShaderCaches is %s..."), bSaveShaderCaches ? TEXT("ENABLED") : TEXT("DISABLED"));

	GCRate = 10;
	FString UpdateRate(TEXT(""));
	for (INT TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++)
	{
		FString& Token = Tokens(TokenIndex);
		if (Parse(*Token, TEXT("GCRate="), UpdateRate))
		{
			break;
		}
	}
	if (UpdateRate.Len() > 0)
	{
		INT CheckInt = appAtoi(*UpdateRate);
		if (CheckInt > 0)
		{
			GCRate = CheckInt;
		}
	}
	warnf(NAME_Log, TEXT("Garbage collection rate set to %3d packages..."), GCRate);

	return TRUE;
}

/**
 *	Shutdown the commandlet
 */
void UBaseCollectionProcessingCommandlet::Shutdown()
{
	if (GADHelper != NULL)
	{
		GADHelper->Shutdown();
		delete GADHelper;
		GADHelper = NULL;
	}
}

/**
 *	Fill in the collection names and classes of interest
 */
UBOOL UBaseCollectionProcessingCommandlet::Initialize()
{
	// By default, the commandlet will look at 'OnDVD (<platform>)'
	// Override this via '-COLLECTION=<name>' on the commandline
	FString CommandLineCollectionName;
	Parse(appCmdLine(), TEXT("COLLECTION="), CommandLineCollectionName);
	if (CommandLineCollectionName.Len() > 0)
	{
		CollectionName = CommandLineCollectionName;
	}
	else
	{
		CollectionName = FString::Printf(TEXT("OnDVD(%s)"), *(appPlatformTypeToString(PlatformType)));
	}
	warnf(NAME_Log, TEXT("Processing objects in collection: %s"), *CollectionName);

	bPrivateCollection = (Switches.FindItemIndex(TEXT("PRIVATE")) != INDEX_NONE);
	bLocalCollection = (Switches.FindItemIndex(TEXT("LOCAL")) != INDEX_NONE);
	warnf(NAME_Log, TEXT("Collection is %s..."), 
		bPrivateCollection ? TEXT("PRIVATE") : (bLocalCollection ? TEXT("LOCAL") : TEXT("PUBLIC")));

	// Construct the GADHelper
	GADHelper = new FGADHelper();
	if (GADHelper != NULL)
	{
		if (GADHelper->Initialize() == FALSE)
		{
			warnf(NAME_Error, TEXT("Failed to initialize GAD helper... exiting."));
			return FALSE;
		}
	}

	// Derived commandlets should fill in this array now...
	//ObjectClassesOfInterest
	return TRUE;
}

/**
 *	Gather the list of assets in the collection
 */
UBOOL UBaseCollectionProcessingCommandlet::GatherAssetNamesFromCollection()
{
	// Retrieve the collection list
	if (GADHelper != NULL)
	{
		if (GADHelper->QueryAssetsInCollection(
			CollectionName, 
			bPrivateCollection ? EGADCollection::Private : (bLocalCollection ? EGADCollection::Local : EGADCollection::Shared),
			FoundObjectsOfInterest) == FALSE)
		{
			warnf(NAME_Error, TEXT("GatherAssetNamesFromCollection(): Failed to query of assets in collection %s"), *CollectionName);
			return FALSE;
		}
	}
	else
	{
		warnf(NAME_Error, TEXT("GatherAssetNamesFromCollection(): Invalid GADHelper!"));
		return FALSE;
	}
	return TRUE;
}

/**
 *	Process the list of objects
 */
void UBaseCollectionProcessingCommandlet::ProcessFoundObjectList()
{
	if (FoundObjectsOfInterest.Num() > 0)
	{
		// Sort the objects so they are in package order...
		TArray<FString> SortedObjectList = FoundObjectsOfInterest;
		Sort<USE_COMPARE_CONSTREF(FString,UContentCommandletsFullNameSort)>(SortedObjectList.GetData(),SortedObjectList.Num());

		FString LastPackageName = TEXT("");
		INT PackageSwitches = 0;
		UPackage* CurrentPackage = NULL;
		for (INT ObjIdx = 0; ObjIdx < SortedObjectList.Num(); ObjIdx++)
		{
			FString ObjClassName;
			FString ObjFullName = SortedObjectList(ObjIdx);
			FString ObjPathName = ObjFullName;
			INT PackageDotIdx = ObjFullName.InStr(TEXT(" "));
			if (PackageDotIdx != INDEX_NONE)
			{
				// Parse off the class name...
				ObjClassName = ObjFullName.Left(PackageDotIdx);
				ObjPathName = ObjFullName.Right(ObjFullName.Len() - (PackageDotIdx + 1));
			}

			if (ObjClassName.Len() > 0)
			{
				if (ObjectClassesOfInterest.Find(ObjClassName) != NULL)
				{
					INT FirstDotIdx = ObjPathName.InStr(TEXT("."));
					if (FirstDotIdx != INDEX_NONE)
					{
						FString PackageName = ObjPathName.Left(FirstDotIdx);
						if (PackageName != LastPackageName)
						{
							if (PackageSwitches > GCRate)
							{
								UObject::CollectGarbage(RF_Native);
								PackageSwitches = 0;
							}

							UPackage* Package = UObject::LoadPackage(NULL, *PackageName, LOAD_None);
							if (Package != NULL)
							{
								LastPackageName = PackageName;
								Package->FullyLoad();
								CurrentPackage = Package;
							}
							else
							{
								warnf(NAME_Warning, TEXT("Failed to load package %s processing %s"), *PackageName, *ObjFullName);
								CurrentPackage = NULL;
							}
						}

						FString ShorterObjName = ObjPathName.Right(ObjPathName.Len() - FirstDotIdx - 1);
						UObject* Obj = FindObject<UObject>(CurrentPackage, *ShorterObjName);
						if (Obj != NULL)
						{
							if (LastPackageName.Len() > 0)
							{
								if (LastPackageName != Obj->GetOutermost()->GetName())
								{
									LastPackageName = Obj->GetOutermost()->GetName();
									PackageSwitches++;
								}
							}
							else
							{
								LastPackageName = Obj->GetOutermost()->GetName();
							}

							// Process the object
							ProcessObject(Obj, CurrentPackage);
						}
						else
						{
							warnf(NAME_Warning, TEXT("Failed to load object %s"), *ObjFullName);
						}
					}
				}
			}
			else
			{
				warnf(NAME_Warning, TEXT("Incomplete path name for %s"), *ObjFullName);
			}
		}

		// Probably don't need to do this, but just in case we have any 'hanging' packages 
		// and more processing steps are added later, let's clean up everything...
		UObject::CollectGarbage(RF_Native);
	}
}

/**
 *	Process the given object
 *
 *	@param	InObject	The object to process
 *	@param	InPackage	The package being processed
 */
void UBaseCollectionProcessingCommandlet::ProcessObject(UObject* InObject, UPackage* InPackage)
{
	// Process your objects of interest here...
}

/**
 *	Dump the results of the commandlet
 */
void UBaseCollectionProcessingCommandlet::DumpResults()
{
	// Dump out the results of the processing here...
}

/**
 *	Helper function for getting a diagnostic table writer
 *	CALLER MUST SHUT IT DOWN/DELETE IT PROPERLY!!!!!
 *
 *	@param	InFilename						The name of the file to write the table to
 *	
 *	@return	FDiagnosticTableWriterCSV*		The diagnostic table writer if successful, NULL if not.
 */
FDiagnosticTableWriterCSV* UBaseCollectionProcessingCommandlet::GetTableWriter(const TCHAR* InFilename)
{
	// Place in the <UE3>\<GAME>\Logs\Audit folder
	FString Filename = FString::Printf(TEXT("%sLogs%s%s%s%s-%s.csv"),*appGameDir(),PATH_SEPARATOR,TEXT("Audit"),PATH_SEPARATOR,InFilename,*appSystemTimeString());
	FDiagnosticTableWriterCSV* DumpTable = new FDiagnosticTableWriterCSV(GFileManager->CreateDebugFileWriter(*Filename));
	return DumpTable;
}

INT UBaseCollectionProcessingCommandlet::Main(const FString& Params)
{
	// Parse the commandline and store off the entries
	ParseCommandLine(*Params, Tokens, Switches);

	if (Startup() == FALSE)
	{
		warnf(NAME_Error, TEXT("Startup() failed..."));
		Shutdown();
		return -1;
	}

	if (Initialize() == FALSE)
	{
		warnf(NAME_Error, TEXT("Initialize() failed..."));
		Shutdown();
		return -2;
	}

	if (GatherAssetNamesFromCollection() == FALSE)
	{
		warnf(NAME_Error, TEXT("GatherAssetNamesFromCollection() failed..."));
		Shutdown();
		return -3;
	}

	ProcessFoundObjectList();
	DumpResults();
	Shutdown();

	return 0;
}

/**
 *	Find particle systems w/ collision enabled.
 */
IMPLEMENT_CLASS(UFindOnDVDPSysWithCollisionEnabledCommandlet);

/**
 *	Fill in the collection names and classes of interest
 */
UBOOL UFindOnDVDPSysWithCollisionEnabledCommandlet::Initialize()
{
	if (Super::Initialize() == TRUE)
	{
		ObjectClassesOfInterest.Set(TEXT("ParticleSystem"), TRUE);
		return TRUE;
	}
	return FALSE;
}

/**
 *	Process the given object
 *
 *	@param	InObject	The object to process
 *	@param	InPackage	The package being processed
 */
void UFindOnDVDPSysWithCollisionEnabledCommandlet::ProcessObject(UObject* InObject, UPackage* InPackage)
{
	UParticleSystem* PSys = Cast<UParticleSystem>(InObject);
	if (PSys != NULL)
	{
		UBOOL bHasCollisionEnabled = FALSE;
		UBOOL bHasCollisionActorEnabled = FALSE;
		for (INT EmitterIdx = 0; EmitterIdx < PSys->Emitters.Num(); EmitterIdx++)
		{
			UParticleEmitter* Emitter = PSys->Emitters(EmitterIdx);
			if (Emitter != NULL)
			{
				for (INT LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
				{
					UParticleLODLevel* LODLevel = Emitter->LODLevels(LODIdx);
					if (LODLevel != NULL)
					{
						for (INT ModuleIdx = 0; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
						{
							UParticleModule* Module = LODLevel->Modules(ModuleIdx);
							UParticleModuleCollision* CollisionModule = Cast<UParticleModuleCollision>(Module);
							UParticleModuleCollisionActor* CollisionActorModule = Cast<UParticleModuleCollisionActor>(Module);
							if (CollisionActorModule != NULL)
							{
								bHasCollisionActorEnabled |= CollisionActorModule->bEnabled;
							}
							else if (CollisionModule != NULL)
							{
								bHasCollisionEnabled |= CollisionModule->bEnabled;
							}
						}
					}
				}
			}
		}

		if ((bHasCollisionEnabled == TRUE) || (bHasCollisionActorEnabled == TRUE))
		{
			ParticleSystemsWithCollisionEnabled.Set(PSys->GetPathName(), TRUE);
		}
	}
}

/**
 *	Dump the results of the commandlet
 */
void UFindOnDVDPSysWithCollisionEnabledCommandlet::DumpResults()
{
	warnf(NAME_Log, TEXT("Found %d particle systems with collision enabled:"), ParticleSystemsWithCollisionEnabled.Num());
	for (TMap<FString,UBOOL>::TIterator DumpIt(ParticleSystemsWithCollisionEnabled); DumpIt; ++DumpIt)
	{
		warnf(NAME_Log, TEXT("\t%s"), *(DumpIt.Key()));
	}
}

INT UFindOnDVDPSysWithCollisionEnabledCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}

//
//
//
IMPLEMENT_CLASS(UFindOnDVDPSysWithMedDetailSpawnRateCommandlet);
/**
 *	Find particle systems w/ MediumDetailSpawnRate set to something other than 1.0.
 */
/**
 *	Fill in the collection names and classes of interest
 */
UBOOL UFindOnDVDPSysWithMedDetailSpawnRateCommandlet::Initialize()
{
	if (Super::Initialize() == TRUE)
	{
		ObjectClassesOfInterest.Set(TEXT("ParticleSystem"), TRUE);
		bTagPartialValuesOnly = (Switches.FindItemIndex(TEXT("PARTIAL")) != INDEX_NONE);
		return TRUE;
	}
	return FALSE;
}

/**
 *	Process the given object
 *
 *	@param	InObject	The object to process
 *	@param	InPackage	The package being processed
 */
void UFindOnDVDPSysWithMedDetailSpawnRateCommandlet::ProcessObject(UObject* InObject, UPackage* InPackage)
{
	UParticleSystem* PSys = Cast<UParticleSystem>(InObject);
	if ((PSys != NULL) && (PSys->Emitters.Num() > 0))
	{
		TArray<FLOAT> MediumDetailSpawnRateScaleValues;
		MediumDetailSpawnRateScaleValues.Empty(PSys->Emitters.Num());
		MediumDetailSpawnRateScaleValues.AddZeroed(PSys->Emitters.Num());
		UBOOL bHasMedDetailSpawnRateScale = FALSE;
		for (INT EmitterIdx = 0; EmitterIdx < PSys->Emitters.Num(); EmitterIdx++)
		{
			UParticleEmitter* Emitter = PSys->Emitters(EmitterIdx);
			if (Emitter != NULL)
			{
				MediumDetailSpawnRateScaleValues(EmitterIdx) = Emitter->MediumDetailSpawnRateScale;
				bHasMedDetailSpawnRateScale |= 
					((Emitter->MediumDetailSpawnRateScale != 1.0f) && 
					(bTagPartialValuesOnly ? (Emitter->MediumDetailSpawnRateScale != 0.0f) : 1));
			}
		}

		if (bHasMedDetailSpawnRateScale == TRUE)
		{
			ParticleSystemsWithMedDetailSpawnRate.Set(PSys->GetPathName(), MediumDetailSpawnRateScaleValues);
		}
	}
}

/**
 *	Dump the results of the commandlet
 */
void UFindOnDVDPSysWithMedDetailSpawnRateCommandlet::DumpResults()
{
	warnf(NAME_Log, TEXT("Found %d particle systems with MediumDetailSpawnRate:"), ParticleSystemsWithMedDetailSpawnRate.Num());
	FDiagnosticTableWriterCSV* DumpTable = GetTableWriter(TEXT("PSysMedDetailSpawnRate"));
	if (DumpTable != NULL)
	{
		DumpTable->AddColumn(TEXT("ParticleSystem"));
		DumpTable->AddColumn(TEXT("Emitter MediumDetailSpawnRate"));
		DumpTable->AddColumn(TEXT("..."));
		DumpTable->CycleRow();
		for (TMap<FString,TArray<FLOAT> >::TIterator DumpIt(ParticleSystemsWithMedDetailSpawnRate); DumpIt; ++DumpIt)
		{
			FString PSysName = DumpIt.Key();
			TArray<FLOAT>& MedDetailList = DumpIt.Value();

			DumpTable->AddColumn(*PSysName);
			for (INT DumpIdx = 0; DumpIdx < MedDetailList.Num(); DumpIdx++)
			{
				DumpTable->AddColumn(TEXT("%3.2f"), MedDetailList(DumpIdx));
			}
			DumpTable->CycleRow();
		}
		DumpTable->Close();
		delete DumpTable;
	}
}

INT UFindOnDVDPSysWithMedDetailSpawnRateCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}

//
//
//
IMPLEMENT_CLASS(UFindOnDVDCinematicTexturesCommandlet);
/**
 *	Fill in the collection names and classes of interest
 */
UBOOL UFindOnDVDCinematicTexturesCommandlet::Initialize()
{
	if (Super::Initialize() == TRUE)
	{
		ObjectClassesOfInterest.Set(TEXT("Texture2D"), TRUE);
		return TRUE;
	}
	return FALSE;
}

/**
 *	Process the given object
 *
 *	@param	InObject	The object to process
 *	@param	InPackage	The package being processed
 */
void UFindOnDVDCinematicTexturesCommandlet::ProcessObject(UObject* InObject, UPackage* InPackage)
{
	UTexture2D* Texture = Cast<UTexture2D>(InObject);
	if (Texture != NULL)
	{
		if (Texture->LODGroup == TEXTUREGROUP_Cinematic)
		{
			CinematicCookedTextures.Set(Texture->GetPathName(), TRUE);
		}
	}
}

/**
 *	Dump the results of the commandlet
 */
void UFindOnDVDCinematicTexturesCommandlet::DumpResults()
{
	warnf(NAME_Log, TEXT("Found %d cinematic textures on DVD:"), CinematicCookedTextures.Num());
	FDiagnosticTableWriterCSV* DumpTable = GetTableWriter(TEXT("CinematicTexturesOnDVD"));
	if (DumpTable != NULL)
	{
		DumpTable->AddColumn(TEXT("Texture"));
		DumpTable->CycleRow();
		for (TMap<FString,UBOOL>::TIterator DumpIt(CinematicCookedTextures); DumpIt; ++DumpIt)
		{
			FString TextureName = DumpIt.Key();
			DumpTable->AddColumn(*TextureName);
			DumpTable->CycleRow();
		}
		DumpTable->Close();
	}
}

INT UFindOnDVDCinematicTexturesCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}

//
//
//
IMPLEMENT_CLASS(UFindOnDVDPSystemsCommandlet);
/**
 *	Find particle systems w/ MediumDetailSpawnRate set to something other than 1.0.
 */
/**
 *	Fill in the collection names and classes of interest
 */
UBOOL UFindOnDVDPSystemsCommandlet::Initialize()
{
	if (Super::Initialize() == TRUE)
	{
		ObjectClassesOfInterest.Set(TEXT("ParticleSystem"), TRUE);
		return TRUE;
	}
	return FALSE;
}

/**
 *	Process the given object
 *
 *	@param	InObject	The object to process
 *	@param	InPackage	The package being processed
 */
void UFindOnDVDPSystemsCommandlet::ProcessObject(UObject* InObject, UPackage* InPackage)
{
	UParticleSystem* PSys = Cast<UParticleSystem>(InObject);
	if (PSys != NULL)
	{
		ParticleSystemsOnDVD.Set(PSys->GetPathName(), TRUE);
	}
}

/**
 *	Dump the results of the commandlet
 */
void UFindOnDVDPSystemsCommandlet::DumpResults()
{
	warnf(NAME_Log, TEXT("Found %d particle systems on DVD:"), ParticleSystemsOnDVD.Num());
	FDiagnosticTableWriterCSV* DumpTable = GetTableWriter(TEXT("PSystemsOnDVD"));
	if (DumpTable != NULL)
	{
		DumpTable->AddColumn(TEXT("ParticleSystem"));
		DumpTable->CycleRow();
		for (TMap<FString,UBOOL>::TIterator DumpIt(ParticleSystemsOnDVD); DumpIt; ++DumpIt)
		{
			FString PSysName = DumpIt.Key();
			DumpTable->AddColumn(*PSysName);
			DumpTable->CycleRow();
		}
		DumpTable->Close();
		delete DumpTable;
	}
}

INT UFindOnDVDPSystemsCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}

//
//
//
IMPLEMENT_CLASS(UFindOnDVDPSysWithNoFixedBoundsCommandlet);
/**
 *	Find particle systems w/ no fixed bounds set.
 */
/**
 *	Fill in the collection names and classes of interest
 */
UBOOL UFindOnDVDPSysWithNoFixedBoundsCommandlet::Initialize()
{
	if (Super::Initialize() == TRUE)
	{
		ObjectClassesOfInterest.Set(TEXT("ParticleSystem"), TRUE);
		return TRUE;
	}
	return FALSE;
}

/**
 *	Process the given object
 *
 *	@param	InObject	The object to process
 *	@param	InPackage	The package being processed
 */
void UFindOnDVDPSysWithNoFixedBoundsCommandlet::ProcessObject(UObject* InObject, UPackage* InPackage)
{
	UParticleSystem* PSys = Cast<UParticleSystem>(InObject);
	if (PSys != NULL)
	{
		if (PSys->bUseFixedRelativeBoundingBox == FALSE)
		{
			ParticleSystemsOnDVDWithNoFixedBounds.Set(PSys->GetFullName(), TRUE);
		}
	}
}

/**
 *	Dump the results of the commandlet
 */
void UFindOnDVDPSysWithNoFixedBoundsCommandlet::DumpResults()
{
	warnf(NAME_Log, TEXT("Found %d particle systems with NoFixedBounds:"), ParticleSystemsOnDVDWithNoFixedBounds.Num());

	TMap<FString,UBOOL> SortedObjectMap = ParticleSystemsOnDVDWithNoFixedBounds;
	SortedObjectMap.KeySort<COMPARE_CONSTREF_CLASS(FString,UContentCommandletsFullNameSort)>();

	FDiagnosticTableWriterCSV* DumpTable = GetTableWriter(TEXT("PSysOnDVDNoFixedBounds"));
	if (DumpTable != NULL)
	{
		DumpTable->AddColumn(TEXT("ParticleSystem w/ NoFixedBounds"));
		DumpTable->CycleRow();
		for (TMap<FString,UBOOL>::TIterator DumpIt(SortedObjectMap); DumpIt; ++DumpIt)
		{
			FString PSysName = DumpIt.Key();
			DumpTable->AddColumn(*PSysName);
			DumpTable->CycleRow();
		}
		DumpTable->Close();
		delete DumpTable;
	}
}

INT UFindOnDVDPSysWithNoFixedBoundsCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}

//
//
//
IMPLEMENT_CLASS(UFindOnDVDPSysDynamicParameterCommandlet);
/**
 *	particle systems DynamicParameter audit
 */
/**
 *	Fill in the collection names and classes of interest
 */
UBOOL UFindOnDVDPSysDynamicParameterCommandlet::Initialize()
{
	if (Super::Initialize() == TRUE)
	{
		bNoMaterialUsageOnly = (Switches.FindItemIndex(TEXT("NOMATERIALUSAGE")) != INDEX_NONE);
		if (bNoMaterialUsageOnly == TRUE)
		{
			warnf(NAME_Warning, TEXT("Running w/ NoMaterialUsageOnly set!"));
		}

		ObjectClassesOfInterest.Set(TEXT("ParticleSystem"), TRUE);
		return TRUE;
	}
	return FALSE;
}

/**
 *	Process the given object
 *
 *	@param	InObject	The object to process
 *	@param	InPackage	The package being processed
 */
void UFindOnDVDPSysDynamicParameterCommandlet::ProcessObject(UObject* InObject, UPackage* InPackage)
{
	UParticleSystem* PSys = Cast<UParticleSystem>(InObject);
	if (PSys != NULL)
	{
		UBOOL bHadDynamicParameter = FALSE;
		UBOOL bHadNoMaterialUsage = FALSE;
		TArray<FDynParamInfoHelper> CurrentDynParamInfo;
		CurrentDynParamInfo.Empty(PSys->Emitters.Num());
		CurrentDynParamInfo.AddZeroed(PSys->Emitters.Num());
		for (INT EmitterIdx = 0; EmitterIdx < PSys->Emitters.Num(); EmitterIdx++)
		{
			UParticleEmitter* Emitter = PSys->Emitters(EmitterIdx);
			if (Emitter != NULL)
			{
				if (Emitter->LODLevels.Num() > 0)
				{
					UParticleLODLevel* LODLevel = Emitter->LODLevels(0);
					if (LODLevel != NULL)
					{
						for (INT ModuleIdx = 0; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
						{
							UParticleModuleParameterDynamic* DynamicParamModule = Cast<UParticleModuleParameterDynamic>(LODLevel->Modules(ModuleIdx));
							if (DynamicParamModule != NULL)
							{
								bHadDynamicParameter = TRUE;
								FDynParamInfoHelper& Info = CurrentDynParamInfo(EmitterIdx);
								Info.bUsesDynamicParameter = TRUE;

								for (INT ParamIdx = 0; ParamIdx < 4; ParamIdx++)
								{
									FEmitterDynamicParameter& DynParam = DynamicParamModule->DynamicParams(ParamIdx);

									// Set the spawn time only flag if used
									if (DynParam.bSpawnTimeOnly == TRUE)
									{
										Info.SpawnTimeOnlyFlags |= (1 << ParamIdx);
									}

									// Set the velocity usage info, if used
									if (DynParam.ValueMethod != EDPV_UserSet)
									{
										Info.VelocityUsageFlags |= (1 << ParamIdx);
									}

									// Set the constant distribution flags
									UDistributionFloatConstant* DistConstant = Cast<UDistributionFloatConstant>(DynParam.ParamValue.Distribution);
									if (DistConstant != NULL)
									{
										Info.ConstantValueFlags |= (1 << ParamIdx);
									}
								}

								UMaterialInterface* MtrlInterface = LODLevel->RequiredModule->Material;
								if (MtrlInterface != NULL)
								{
									Info.MaterialName = MtrlInterface->GetFullName();

									FStaticParameterSet* StaticParameterSet = NULL;
									FStaticParameterSet TempStaticParameterSet;
									
									UMaterial* Mtrl = Cast<UMaterial>(MtrlInterface);
									UMaterialInstance* MtrlInst = Cast<UMaterialInstance>(MtrlInterface);
									if (MtrlInst != NULL)
									{
										FMaterialResource* MtrlRes = MtrlInst->GetMaterialResource();
										if ((MtrlRes != NULL) && (MtrlRes->GetShaderMap() != NULL))
										{
											TempStaticParameterSet = MtrlRes->GetShaderMap()->GetMaterialId();
											StaticParameterSet = &TempStaticParameterSet;
										}

										UMaterialInstance* ParentMtrlInst = Cast<UMaterialInstance>(MtrlInst->Parent);
										while (ParentMtrlInst != NULL)
										{
											MtrlInst = ParentMtrlInst;
											ParentMtrlInst = Cast<UMaterialInstance>(MtrlInst->Parent);
										}

										Mtrl = Cast<UMaterial>(MtrlInst->Parent);
									}

									if (Mtrl != NULL)
									{
										TArray<UMaterialExpression*> Expressions;
										if (Mtrl->GetAllReferencedExpressions(Expressions, StaticParameterSet) == TRUE)
										{
											TMap<UMaterialExpression*,UBOOL> DynParamExpressions;
											UBOOL bHasDynamicParameterExpressions = FALSE;

											// See if there are *any* referenced dynamic parameter expressions
											for (INT ExpressionIdx = 0; ExpressionIdx < Expressions.Num(); ExpressionIdx++)
											{
												UMaterialExpressionDynamicParameter* DynParamExp = Cast<UMaterialExpressionDynamicParameter>(Expressions(ExpressionIdx));
												if (DynParamExp != NULL)
												{
													bHasDynamicParameterExpressions = TRUE;
													DynParamExpressions.Set(DynParamExp, TRUE);
												}
											}

											if (bHasDynamicParameterExpressions == TRUE)
											{
												// See if any expressions have a dynamic parameter as the input
												for (INT ExpressionIdx = 0; ExpressionIdx < Expressions.Num(); ExpressionIdx++)
												{
													UMaterialExpression* Expression = Expressions(ExpressionIdx);
													if (Expression != NULL)
													{
														if (DynParamExpressions.Find(Expression) == NULL)
														{
															const TArray<FExpressionInput*> Inputs = Expression->GetInputs();
															for (INT InputIdx = 0; InputIdx < Inputs.Num(); InputIdx++)
															{
																FExpressionInput* Input = Inputs(InputIdx);
																if (DynParamExpressions.Find(Input->Expression))
																{
																	// The input is a dynamic parameter...
																	if (Input->MaskR == TRUE)
																	{
																		Info.MtrlUsageFlags |= 0x01;
																	}
																	if (Input->MaskG == TRUE)
																	{
																		Info.MtrlUsageFlags |= 0x02;
																	}
																	if (Input->MaskB == TRUE)
																	{
																		Info.MtrlUsageFlags |= 0x04;
																	}
																	if (Input->MaskA == TRUE)
																	{
																		Info.MtrlUsageFlags |= 0x08;
																	}
																}
															}
														}
													}
												}
											}
										}
										else
										{
											warnf(NAME_Warning, TEXT("Failed to get expressions for material %s"), *(MtrlInterface->GetFullName()));
										}
									}
									else
									{
										warnf(NAME_Warning, TEXT("Failed to get material for %s"), *(MtrlInterface->GetFullName()));
									}
								}
								else
								{
									Info.MaterialName = TEXT("NULL");
								}

								if (Info.MtrlUsageFlags == 0)
								{
									bHadNoMaterialUsage = TRUE;
								}
							}
						}
					}
				}
			}
		}

		if (bHadDynamicParameter == TRUE)
		{
			if ((bNoMaterialUsageOnly == FALSE) || (bHadNoMaterialUsage == TRUE))
			{
				ParticleSystemsOnDVDDynamicParameterUsage.Set(PSys->GetPathName(), CurrentDynParamInfo);
			}
		}
	}
}

/**
 *	Dump the results of the commandlet
 */
void UFindOnDVDPSysDynamicParameterCommandlet::DumpResults()
{
	warnf(NAME_Log, TEXT("Found %d particle systems with DynamicParameter:"), ParticleSystemsOnDVDDynamicParameterUsage.Num());

	FDiagnosticTableWriterCSV* DumpTable = GetTableWriter(TEXT("PSysOnDVDDynamicParameter"));
	if (DumpTable != NULL)
	{
		TMap<FString,TArray<FDynParamInfoHelper> > SortedObjectMap = ParticleSystemsOnDVDDynamicParameterUsage;
		SortedObjectMap.KeySort<COMPARE_CONSTREF_CLASS(FString,UContentCommandletsFullNameSort)>();

		DumpTable->AddColumn(TEXT("ParticleSystem"));
		DumpTable->AddColumn(TEXT("Emitter"));
		DumpTable->AddColumn(TEXT("Material"));
		DumpTable->AddColumn(TEXT("MtrlUsage"));
		DumpTable->AddColumn(TEXT("."));
		DumpTable->AddColumn(TEXT("."));
		DumpTable->AddColumn(TEXT("."));
		DumpTable->AddColumn(TEXT("SpawnTimeOnly"));
		DumpTable->AddColumn(TEXT("."));
		DumpTable->AddColumn(TEXT("."));
		DumpTable->AddColumn(TEXT("."));
		DumpTable->AddColumn(TEXT("Constants"));
		DumpTable->AddColumn(TEXT("."));
		DumpTable->AddColumn(TEXT("."));
		DumpTable->AddColumn(TEXT("."));
		DumpTable->CycleRow();
		DumpTable->CycleRow();

		for (TMap<FString,TArray<FDynParamInfoHelper> >::TIterator DumpIt(SortedObjectMap); DumpIt; ++DumpIt)
		{
			FString PSysName = DumpIt.Key();
			TArray<FDynParamInfoHelper>& DynParamInfoArray = DumpIt.Value();

			DumpTable->AddColumn(*PSysName);
			DumpTable->CycleRow();
			for (INT DumpIdx = 0; DumpIdx < DynParamInfoArray.Num(); DumpIdx++)
			{
				FDynParamInfoHelper& DynParamInfo = DynParamInfoArray(DumpIdx);
				if (DynParamInfo.bUsesDynamicParameter == TRUE)
				{
					if ((bNoMaterialUsageOnly == FALSE) || (DynParamInfo.MtrlUsageFlags == 0))
					{
						DumpTable->AddColumn(TEXT(""));
						DumpTable->AddColumn(TEXT("%d"), DumpIdx);
						DumpTable->AddColumn(TEXT("%s"), *(DynParamInfo.MaterialName));
						DumpTable->AddColumn(TEXT("%d"), (DynParamInfo.MtrlUsageFlags & 0x01) ? 1 : 0);
						DumpTable->AddColumn(TEXT("%d"), (DynParamInfo.MtrlUsageFlags & 0x02) ? 1 : 0);
						DumpTable->AddColumn(TEXT("%d"), (DynParamInfo.MtrlUsageFlags & 0x04) ? 1 : 0);
						DumpTable->AddColumn(TEXT("%d"), (DynParamInfo.MtrlUsageFlags & 0x08) ? 1 : 0);
						DumpTable->AddColumn(TEXT("%d"), (DynParamInfo.SpawnTimeOnlyFlags & 0x01) ? 1 : 0);
						DumpTable->AddColumn(TEXT("%d"), (DynParamInfo.SpawnTimeOnlyFlags & 0x02) ? 1 : 0);
						DumpTable->AddColumn(TEXT("%d"), (DynParamInfo.SpawnTimeOnlyFlags & 0x04) ? 1 : 0);
						DumpTable->AddColumn(TEXT("%d"), (DynParamInfo.SpawnTimeOnlyFlags & 0x08) ? 1 : 0);
						DumpTable->AddColumn(TEXT("%d"), (DynParamInfo.ConstantValueFlags & 0x01) ? 1 : 0);
						DumpTable->AddColumn(TEXT("%d"), (DynParamInfo.ConstantValueFlags & 0x02) ? 1 : 0);
						DumpTable->AddColumn(TEXT("%d"), (DynParamInfo.ConstantValueFlags & 0x04) ? 1 : 0);
						DumpTable->AddColumn(TEXT("%d"), (DynParamInfo.ConstantValueFlags & 0x08) ? 1 : 0);
						DumpTable->CycleRow();
					}
				}
			}
		}
		DumpTable->Close();
		delete DumpTable;
	}
}

INT UFindOnDVDPSysDynamicParameterCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}

//
//
//
IMPLEMENT_CLASS(UFindBadAnimNotifyCommandlet);
/**
 *	Find AnimNotifies that have a different outer than the animseq they are ref'd by
 */
/**
 *	Fill in the collection names and classes of interest
 */
UBOOL UFindBadAnimNotifyCommandlet::Initialize()
{
	if (Super::Initialize() == TRUE)
	{
		ObjectClassesOfInterest.Set(TEXT("AnimSet"), TRUE);
		return TRUE;
	}
	return FALSE;
}

/**
 *	Process the given object
 *
 *	@param	InObject	The object to process
 *	@param	InPackage	The package being processed
 */
void UFindBadAnimNotifyCommandlet::ProcessObject(UObject* InObject, UPackage* InPackage)
{
	UAnimSet* AnimSet = Cast<UAnimSet>(InObject);
	if (AnimSet != NULL)
	{
		for (INT J=0; J<AnimSet->Sequences.Num(); ++J)
		{
			UAnimSequence * AnimSeq = AnimSet->Sequences(J);
			// iterate over all animnotifiers
			// if any animnotifier outer != current animsequence
			// then add to map
			for (INT I=0; I<AnimSeq->Notifies.Num(); ++I)
			{
				if (AnimSeq->Notifies(I).Notify && AnimSeq->Notifies(I).Notify->GetOuter()!=AnimSeq)
				{
					BadAnimNotifiers.Set(AnimSeq->GetPathName(), AnimSeq->Notifies(I).Notify->GetPathName());
				}
			}
		}
	}
}

/**
 *	Dump the results of the commandlet
 */
void UFindBadAnimNotifyCommandlet::DumpResults()
{
	warnf(NAME_Log, TEXT("Found %d bad animnotifies on DVD:"), BadAnimNotifiers.Num());
	// Place in the <UE3>\<GAME>\Logs\Audit folder
	FString Filename = FString::Printf(TEXT("%sLogs%s%s%s%s-%s.csv"),*appGameDir(),PATH_SEPARATOR,TEXT("Audit"),
		PATH_SEPARATOR,TEXT("BadAnimNotifies"),*appSystemTimeString());
	FDiagnosticTableWriterCSV DumpTable(GFileManager->CreateDebugFileWriter(*Filename));

	DumpTable.AddColumn(TEXT("BadAnimNotifies"));
	DumpTable.CycleRow();
	for (TMap<FString,FString>::TIterator DumpIt(BadAnimNotifiers); DumpIt; ++DumpIt)
	{
		DumpTable.AddColumn(*DumpIt.Key());
		DumpTable.AddColumn(*DumpIt.Value());
		DumpTable.CycleRow();
	}
	DumpTable.Close();
}

INT UFindBadAnimNotifyCommandlet::Main(const FString& Params)
{
	return Super::Main(Params);
}


	static INT PackageNameNum = 0;	
struct BreakApartPackageFunctor
{


	void CleanUpGADTags()
	{
	}


	template< typename OBJECTYPE >
	void DoIt( UCommandlet* Commandlet, UPackage* Package, TArray<FString>& Tokens, TArray<FString>& Switches )
	{
		PackageNameNum++;

		UBOOL bModifiedPackage = FALSE;
		UINT ObjectCount = 0;
		UINT ObjectCountMoved = 0;

		const FString PathToMoveTo = FString( TEXT("..\\..\\GearGame\\Content2\\") );

		FString EarlyOutFilename;
		GPackageFileCache->FindPackageFile( *Package->GetName(), NULL, EarlyOutFilename );

		// do nothing
		if( EarlyOutFilename.InStr( *PathToMoveTo ) != INDEX_NONE )
		{
			warnf( TEXT( "BLARGH early out %s" ), *Package->GetName() );
			return;
		}

		for( TObjectIterator<OBJECTYPE> It; It; ++It )
		{
			ObjectCount++;
	
			OBJECTYPE* AnObject = *It;

			if( AnObject->IsIn( Package ) == FALSE )
			{
				continue;
			}

			// do not try to move any ThumbnailTexture
			if( (AnObject->GetName().Right(16)).InStr( TEXT( "ThumbnailTexture" )) != INDEX_NONE )
			{
				continue;
			}

			// if our object name has a : in it then we are inside a prefab or some other meta object and we don't want rename that
			if( (AnObject->GetFullName()).InStr( TEXT( ":" )) != INDEX_NONE )
			{
				continue;
			}

			// externally linked
			if( (AnObject->GetName()).InStr( TEXT( "P_Ceiling_dust" )) != INDEX_NONE )
			{
				continue;
			}

			// externally linked
			if( (AnObject->GetName()).InStr( TEXT( "P_ambient_FlyThroughFog_Dense" )) != INDEX_NONE )
			{
				continue;
			}

			// externally linked
			if( (AnObject->GetName()).InStr( TEXT( "P_ambient_FlyThroughFog_Light" )) != INDEX_NONE )
			{
				continue;
			}




			if( Cast<UTextureFlipBook>(AnObject) != NULL )
			{
				continue;
			}

			const UBOOL bIsSomethingWeCareAbout = ( Cast<UTexture2D>(AnObject) != NULL )	
	|| ( Cast<UFaceFXAsset>(AnObject) != NULL )
	|| ( Cast<UAnimSet>(AnObject) != NULL )
	|| ( Cast<UAnimTree>(AnObject) != NULL )
	|| ( Cast<UCameraAnim>(AnObject) != NULL )
	|| ( Cast<UFaceFXAnimSet>(AnObject) != NULL )
	|| ( Cast<UFont>(AnObject) != NULL )
	|| ( Cast<UMaterial>(AnObject) != NULL )
	|| ( Cast<UMaterialInstanceConstant>(AnObject) != NULL )
	|| ( Cast<UMaterialInstanceTimeVarying>(AnObject) != NULL )
	|| ( Cast<UMorphTargetSet>(AnObject) != NULL )
	|| ( Cast<UMorphWeightSequence>(AnObject) != NULL )
	|| ( Cast<UParticleSystem>(AnObject) != NULL )
	|| ( Cast<UPhysicalMaterial>(AnObject) != NULL )
	|| ( Cast<UPhysicsAsset>(AnObject) != NULL )
	|| ( Cast<UPostProcessChain>(AnObject) != NULL )
	|| ( Cast<USkeletalMesh>(AnObject) != NULL )
	|| ( Cast<USoundCue>(AnObject) != NULL )
	|| ( Cast<USoundNodeWave>(AnObject) != NULL )
	|| ( Cast<USpeedTree>(AnObject) != NULL )
 	|| ( Cast<UStaticMesh>(AnObject) != NULL )
	;

	if( bIsSomethingWeCareAbout == FALSE )
	{
		continue;
	}


	ObjectCountMoved++;

			bModifiedPackage = TRUE;


			FString TheName = AnObject->GetName();
			FString FullName = AnObject->GetFullName();
			FString FullGroupName = AnObject->GetFullGroupName( TRUE );
			FString FullGroupName2 = AnObject->GetFullGroupName( FALSE );
			FString PathName = AnObject->GetPathName();

			//warnf( TEXT( "TheName: %s  FullName: %s  FullGroupName: %s  FullGroupName2: %s PathName: %s" ), *TheName, *FullName, *FullGroupName, *FullGroupName2, *PathName );

			// TheName: T_Hairmask01  
			// FullName: Texture2D Hair_Tests.Textures.T_Hairmask01  
			// FullGroupName: Textures  
			// FullGroupName2: Textures.T_Hairmask01 
			// PathName: Hair_Tests.Textures.T_Hairmask01


			FString PackageName = Package->GetName();
			FString GroupName = AnObject->GetFullGroupName( TRUE );
			FString ObjectName = AnObject->GetName();;

			//warnf( TEXT( "PackageName: %s  GroupNameTRUE: %s  GroupNameFALSE: %s ObjectName: %s" ), *PackageName, *GroupName, *AnObject->GetFullGroupName( FALSE ), *ObjectName );

			// Rename
			//FString NewPackageName = FString::Printf( TEXT( "%s_%s_%s" ), *PackageName, *GroupName, *ObjectName );
			
			
			FString NewPackageName = FString::Printf( TEXT( "%s_%s_%s" ), *FString::Printf( TEXT("%u"), PackageNameNum), *FString::Printf( TEXT("%u"), ObjectCountMoved), *ObjectName );

			NewPackageName.ReplaceInline( TEXT("."), TEXT("_") );


			UPackage* NewPackage = UObject::CreatePackage( NULL, *NewPackageName );

			FString Filename;
			GPackageFileCache->FindPackageFile( *PackageName, NULL, Filename );

			Filename.ReplaceInline( TEXT("..\\..\\GearGame\\Content\\"), *PathToMoveTo );

			FFilename NewFileName(Filename);

			FString NewPath = (NewFileName.GetPath() + PATH_SEPARATOR + PackageName + PATH_SEPARATOR + NewPackageName + FString(TEXT(".upk")));

			warnf( TEXT( "NewPath: %s  NewPackageName: %s" ), *NewPath, *NewPackageName);

			AnObject->Rename( *ObjectName, NewPackage );


			SavePackageHelper( NewPackage, NewPath );
		}

		//bModifiedPackage = FALSE;

		if( bModifiedPackage == TRUE )
		{
			// save the orig package here that now has redirectors
			FString OrigFilename;
			GPackageFileCache->FindPackageFile( *Package->GetName(), NULL, OrigFilename );
			SavePackageHelper( Package, OrigFilename );
		}

		warnf( TEXT( "    NumObjects Looked at: %u   Num Moved: %u"), ObjectCount, ObjectCountMoved );
	}
};



INT UBreakApartPackagesCommandlet::Main( const FString& Params )
{
	DoActionToAllPackages<UObject, BreakApartPackageFunctor>(this, Params);



	return 0;
}
IMPLEMENT_CLASS(UBreakApartPackagesCommandlet)





struct ListPhysAssetsNumBodiesFunctor
{
	void CleanUpGADTags()
	{
	}

	template< typename OBJECTYPE >
	void DoIt( UCommandlet* Commandlet, UPackage* Package, TArray<FString>& Tokens, TArray<FString>& Switches )
	{
		TArray<FString> AllAssetsTagged;

		// if we don't have managed code we can't access the Tags.  So, without managed code we will return all assets
#if WITH_MANAGED_CODE
		TArray<FString> OnDVDPlusTag;
		OnDVDPlusTag.AddItem( FString( TEXT( "Audit.OnDVD(Xbox360)" ) ) );

		CommandletGADHelper.QueryAssetsWithAllTags( OnDVDPlusTag, AllAssetsTagged );
		//warnf( TEXT( "AllAssetsTaggedSize: %u"), AllAssetsTagged.Num() );
#endif

		for( TObjectIterator<OBJECTYPE> It; It; ++It )
		{
			OBJECTYPE* ThePhysAsset = *It;

			if( ThePhysAsset->IsIn( Package ) == FALSE )
			{
				continue;
			}

			const FString& PhysAssetName = ThePhysAsset->GetFullName();
			UBOOL bOnDVD = FALSE;
			INT Tmp = -1;
			//warnf( TEXT( "AllAssetsTaggedSize: %s   %s"), *PhysAssetName, *AllAssetsTagged(0) );


			if( AllAssetsTagged.FindItem( PhysAssetName, Tmp ) == TRUE )
			{
				bOnDVD = TRUE;
			}



			INT NumConsiderForBounds = 0;
			for( INT Idx = 0; Idx < ThePhysAsset->BodySetup.Num(); ++Idx )
			{
				if( ThePhysAsset->BodySetup(Idx)->bConsiderForBounds == TRUE )
				{
					NumConsiderForBounds++;
				}
			}

			warnf( TEXT("Name: %s Bodies: %u NumConsiderForBounds: %u Ratio: %f OnDVD: %u"), *ThePhysAsset->GetName(), ThePhysAsset->BodySetup.Num(), NumConsiderForBounds, (static_cast<FLOAT>(NumConsiderForBounds)/static_cast<FLOAT>(ThePhysAsset->BodySetup.Num())), bOnDVD );
		}
	}

};



INT UListPhysAssetsNumBodiesCommandlet::Main( const FString& Params )
{
	DoActionToAllPackages<UPhysicsAsset, ListPhysAssetsNumBodiesFunctor>(this, Params);


	return 0;
}

IMPLEMENT_CLASS(UListPhysAssetsNumBodiesCommandlet)
