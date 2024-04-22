//! @file SubstanceAirEdImportCommandlet.cpp
//! @brief Substance Air commandlets implementation
//! @contact antoine.gonzalez@allegorithmic.com
//! @copyright Allegorithmic. All rights reserved.

#include <UnrealEd.h>
#include <Factories.h>

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirHelpers.h"
#include "SubstanceAirPackage.h"
#include "SubstanceAirTextureClasses.h"
#include "SubstanceAirInstanceFactoryClasses.h"

#include "SubstanceAirEdFactoryClasses.h"
#include "SubstanceAirEdCommandlet.h"

IMPLEMENT_CLASS(UImportSbsCommandlet);


FString GetTextureNameFromPath(const FString & TexturePath) 
{
	FString Filename = TexturePath.Mid(TexturePath.InStr(PATH_SEPARATOR, TRUE)+1);
	return Filename.Left((Filename.InStr((TEXT(".")))));
}


INT UImportSbsCommandlet::Main(const FString& Params)
{
	// Command line parse
	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	// use the first parameter as package name
	FString PackageName = Tokens(0);
	Tokens.Remove(0);

	// the rest of tokens are the paths of the substances to import
	TArray<FString> AssetsToImport = Tokens;

	UBOOL bReimportOnly = FALSE;

	TArray<UPackage*> PackagesNeedingResave;

	TArray<FString> UpToDateAssets;
	TArray<FString> ReimportedAssets;
	TArray<FString> NewlyImportedAssets;

	USubstanceAirImportFactory* TextureFact = new USubstanceAirImportFactory;

	for( TArray<FString>::TConstIterator AssetIt(AssetsToImport) ; AssetIt ; ++AssetIt )
	{	
		FString TextureName = GetTextureNameFromPath( *AssetIt );

		// Attempt to find the texture package
		USubstanceAirInstanceFactory* ExistingTexturePackage = 
			Cast<USubstanceAirInstanceFactory>(
				UObject::StaticLoadObject(
					USubstanceAirInstanceFactory::StaticClass(), 
						NULL, *TextureName, NULL, LOAD_NoWarn|LOAD_Quiet, NULL));

		if (ExistingTexturePackage != NULL)
		{
			warnf( NAME_Log, TEXT("Texture already existing %s"), *(*AssetIt) );
		}
		else
		{
			// Texture does not exist, import it
			warnf( NAME_Log, TEXT("Importing %s"), *(*AssetIt) );

			UPackage* Package = CreatePackage( NULL, *(PackageName) );

			if ( Package != NULL )
			{
				UObject* ImportedObject = UFactory::StaticImportObject(
					USubstanceAirInstanceFactory::StaticClass(),
					Package,
					*TextureName,
					RF_Public|RF_Standalone,
					*(*AssetIt),
					NULL,
					TextureFact);

				USubstanceAirInstanceFactory* ImportedTexturePackage = Cast<USubstanceAirInstanceFactory>( ImportedObject );

				if (ImportedTexturePackage != NULL)
				{
					PackagesNeedingResave.AddUniqueItem( ImportedTexturePackage->GetOutermost() );	
					NewlyImportedAssets.AddItem( *AssetIt );
				}
				else
				{
					warnf( NAME_Error, TEXT("Error importing %s."), *(*AssetIt) );
				}
			}
			else
			{
				warnf( NAME_Error, TEXT("Failed to create package for %s"), *(*AssetIt) );
			}
		}
	}

	// Save all the packages that we modified.
	for (TArray<UPackage*>::TConstIterator PackageIterator(PackagesNeedingResave); PackageIterator; ++PackageIterator  )
	{
		FString PackageName = (*PackageIterator)->GetName();
		FString PackageFilename;

		// We have the package in memory, but it may not be fully loaded.
		if ( !(*PackageIterator)->IsFullyLoaded() )
		{
			(*PackageIterator)->FullyLoad();
		}

		UBOOL bPackageExistsOnDisk = GPackageFileCache->FindPackageFile( *PackageName, NULL, PackageFilename );

		// If the package does not yet exist, figure out what its filename should be.
		if (!bPackageExistsOnDisk)
		{
			FString GfxPackageLocation = appGameDir() + TEXT("Content") + PATH_SEPARATOR;
			PackageFilename = GfxPackageLocation + PackageName + TEXT(".upk");
		}

		warnf( NAME_Log, TEXT("Saving %s"), *PackageFilename );

		// An array version of the package filename (used by Source Control methods);
		TArray <FString> PackageFilenames;
		PackageFilenames.AddItem( PackageFilename );

		// Actually save the package
		UObject::SavePackage( *PackageIterator, NULL, RF_Standalone, *PackageFilename, GWarn );
	}

	warnf( NAME_Log, TEXT("%d new assets imported"), NewlyImportedAssets.Num() );
	for( INT AssetIndex = 0; AssetIndex < NewlyImportedAssets.Num(); ++AssetIndex )
	{
		warnf( NAME_Log, TEXT("\t%s"), *NewlyImportedAssets(AssetIndex) );
	}

	return 0; //success
}
