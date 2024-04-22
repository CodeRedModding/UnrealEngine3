/*=============================================================================
	LocalizationCommandlets.cpp: Class implementations for commandlets related to localization.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "FConfigCacheIni.h"
#include "LocalizationExport.h"

/*-----------------------------------------------------------------------------
	UExportLocCommandlet.
-----------------------------------------------------------------------------*/
INT UExportLocCommandlet::Main( const FString& Params )
{
	const TCHAR* Parms = *Params;
	const TCHAR* LangExt = UObject::GetLanguage();

	TArray<FString> Tokens, Switches;
	ParseCommandLine(Parms, Tokens, Switches);

	INT OutputPathTokenIndex=1;

	//@todo ronp - support exporting all possible loc data by using the 
	TArray<FString> PackageNames;
	if ( Switches.ContainsItem(TEXT("FULL")) )
	{
		OutputPathTokenIndex = 0;

		// build the list of package names to load from the names of the existing INT files
		TArray<FString> ExistingLocFiles;
		for( INT PathIndex=0; PathIndex < GSys->LocalizationPaths.Num(); PathIndex++ )
		{
			const FString PathToSearch = FString::Printf( TEXT("%s") PATH_SEPARATOR TEXT("%s") PATH_SEPARATOR TEXT("*.%s"), 
				*GSys->LocalizationPaths(PathIndex), LangExt, LangExt );

			GFileManager->FindFiles(ExistingLocFiles, *PathToSearch, TRUE, FALSE);
		}

		// now strip the extension from the list of files found and use those as the package names
		for ( INT FileIndex = 0; FileIndex < ExistingLocFiles.Num(); FileIndex++ )
		{
			FFilename LocFilename = ExistingLocFiles(FileIndex);
			PackageNames.AddUniqueItem(LocFilename.GetBaseFilename());
		}
	}
	else
	{
		if ( Tokens.Num() == 0 )
		{
			warnf(NAME_Error, *LocalizeUnrealEd("Error_PackageFileNotSpecified"));
			return 1;
		}

		PackageNames.AddItem(Tokens(0));
	}

	// parse the OutputPath from the command-line, or use the default if none was specified
	FString OutputPath;
	if ( Tokens.Num() > OutputPathTokenIndex )
	{
		OutputPath = Tokens(OutputPathTokenIndex);
	}
	else
	{
		OutputPath = FString::Printf(TEXT("ExportedLocFiles") PATH_SEPARATOR TEXT("%s"), LangExt);
	}

	if ( !GFileManager->MakeDirectory(*OutputPath) )
	{
		warnf(NAME_Error, TEXT("Failed to create or find %s directory path: %s"), Tokens.Num() <= OutputPathTokenIndex ? TEXT("default") : TEXT(""), *OutputPath);
		return 1;
	}

	INT GCIndex=0;
	for ( INT PackageIndex = 0; PackageIndex < PackageNames.Num(); PackageIndex++ )
	{
		FString& PackageName = PackageNames(PackageIndex);

		warnf( NAME_Log, TEXT("Loading %s"), *PackageName );
		UPackage* Package = LoadPackage( NULL, *PackageName, LOAD_None );

		// skip over script files
		if( Package && (Package->PackageFlags&PKG_ContainsScript) == 0 )
		{
			FFilename LocFilename;
			FLocalizationExport::GenerateIntNameFromPackageName( *PackageName, LocFilename );

			LocFilename = OutputPath * (LocFilename.GetBaseFilename() + TEXT(".") + LangExt);

			warnf(NAME_Log, TEXT("Exporting localized data to %s"), *LocFilename);
			FLocalizationExport::ExportPackage( Package, *LocFilename, FALSE, TRUE );
		}

		// only GC every 10 packages (A LOT faster this way, and is safe, since we are not 
		// acting on objects that would need to go away or anything)
		if ((++GCIndex % 10) == 0)
		{
			UObject::CollectGarbage(RF_Native);
		}
	}

	return 0;
}
IMPLEMENT_CLASS(UExportLocCommandlet)


/*-----------------------------------------------------------------------------
	UCompareLocCommandlet.
-----------------------------------------------------------------------------*/
/**
 * Returns the index of the loc file pair that contains the english version of the specified filename, or INDEX_NONE if it isn't found
 */
INT UCompareLocCommandlet::FindEnglishIndex( const FString& Filename )
{
	INT Result = INDEX_NONE;
	if ( Filename.Len() > 0 )
	{
		for ( INT i = 0; i < LocPairs.Num(); i++ )
		{
			if ( LocPairs(i).HasEnglishFile(Filename) )
			{
				Result = i;
				break;
			}
		}
	}
	return Result;
}

/**
 * Returns the index of the loc file pair that contains the english version of the specified filename, or INDEX_NONE if it isn't found
 */
INT UCompareLocCommandlet::FindForeignIndex( const FString& Filename )
{
	INT Result = INDEX_NONE;
	if ( Filename.Len() > 0 )
	{
		for ( INT i = 0; i < LocPairs.Num(); i++ )
		{
			if ( LocPairs(i).HasForeignFile(Filename) )
			{
				Result = i;
				break;
			}
		}
	}

	return Result;
}

/**
 * Adds the specified file as the english version for a loc file pair
 */
void UCompareLocCommandlet::AddEnglishFile( const FString& Filename )
{
	if ( Filename.Len() > 0 )
	{
		FLocalizationFile locFile(Filename);

		// attempt to find the matching foreign file for this english file
		INT Index = FindForeignIndex(locFile.GetFilename());
		if ( Index == INDEX_NONE )
		{
			Index = LocPairs.AddZeroed();
		}

		LocPairs(Index).SetEnglishFile(Filename);
	}
}

/**
 * Adds the specified file as the foreign version for a loc file pair
 */
void UCompareLocCommandlet::AddForeignFile( const FString& Filename )
{
	if ( Filename.Len() > 0 )
	{
		FLocalizationFile locFile(Filename);

		// attempt to find the matching foreign file for this english file
		INT Index = FindEnglishIndex(locFile.GetFilename());
		if ( Index == INDEX_NONE )
		{
			Index = LocPairs.AddZeroed();
		}

		LocPairs(Index).SetForeignFile(Filename);
	}
}

/**
 * Initializes the LocPairs arrays using the list of filenames provided.
 */
void UCompareLocCommandlet::ReadLocFiles( const TArray<FString>& EnglishFilenames, TArray<FString>& ForeignFilenames )
{
	for ( INT i = 0; i < EnglishFilenames.Num(); i++ )
	{
		AddEnglishFile(*EnglishFilenames(i));
	}

	for ( INT i = 0; i < ForeignFilenames.Num(); i++ )
	{
		AddForeignFile(*ForeignFilenames(i));
	}
}

INT UCompareLocCommandlet::Main(const FString& Params)
{
	INT Result = 0;

	const TCHAR* Parms = *Params;

	// get the extension that we want to check
	if( !ParseToken(Parms, LangExt, FALSE) )
	{
		warnf(NAME_Error, TEXT("Example: ucc compareint <lang_ext>"));
		return 1;
	}

	TArray<FString> EnglishFilenames;
	TArray<FString> ForeignFilenames;

	for ( INT LocPathIndex = 0; LocPathIndex < GSys->LocalizationPaths.Num(); LocPathIndex++ )
	{
		TArray<FString> PathEnglishFilenames;
		TArray<FString> PathForeignFilenames;

		FString EnglishLocDirectory = GSys->LocalizationPaths(LocPathIndex) * TEXT("INT") + PATH_SEPARATOR;
		FString ForeignLocDirectory = GSys->LocalizationPaths(LocPathIndex) * LangExt + PATH_SEPARATOR;

		FString EnglishWildcardName = EnglishLocDirectory + TEXT("*.INT");
		FString ForeignWildcardName = ForeignLocDirectory + TEXT("*.") + LangExt;

		// grab the list of english loc files
		GFileManager->FindFiles(PathEnglishFilenames, *EnglishWildcardName, 1, 0);

		// get a list of foreign loc files
		GFileManager->FindFiles(PathForeignFilenames, *ForeignWildcardName, 1, 0);

		// since the FLocalizationFile keeps a pointer to the FConfigFile, we must preload all loc files so that the FConfigFile pointer
		// doesn't become invalid when the FConfigCacheIni reallocs as it expands
		for ( INT FileIndex = 0; FileIndex < PathEnglishFilenames.Num(); FileIndex++ )
		{
			FString* CompleteFilename = new(EnglishFilenames) FString(EnglishLocDirectory + PathEnglishFilenames(FileIndex));
			static_cast<FConfigCacheIni*>(GConfig)->Find(**CompleteFilename,FALSE);
		}
		for ( INT FileIndex = 0; FileIndex < PathForeignFilenames.Num(); FileIndex++ )
		{
			FString* CompleteFilename = new(ForeignFilenames) FString(ForeignLocDirectory + PathForeignFilenames(FileIndex));
			static_cast<FConfigCacheIni*>(GConfig)->Find(**CompleteFilename,FALSE);
		}
	}

	if ( EnglishFilenames.Num() == 0 )
	{
		warnf(NAME_Error, TEXT("No english loc files found!"));
		return 1;
	}

	if ( ForeignFilenames.Num() == 0 )
	{
		warnf(NAME_Error, TEXT("No foreign loc files found using language extension '%s'"), *LangExt);
		return 1;
	}

	ReadLocFiles(EnglishFilenames, ForeignFilenames);

	// show the results
	TArray<FString> MissingFiles, ObsoleteFiles;

	// for each file in the list, 
	for ( INT i = 0; i < LocPairs.Num(); i++ )
	{
		TArray<FString> MissingSections, ObsoleteSections;
		TArray<FString> MissingProperties, ObsoleteProperties, UntranslatedProperties;

		FLocalizationFilePair& Pair = LocPairs(i);

		Pair.CompareFiles();

		// first, search for any english files that don't have corresponding foreign files,
		// and add them to the "missing foreign file" list
		Pair.GetMissingLocFiles(MissingFiles);

		// next, search for any foreign files that don't have corresponding english files,
		// and add them to the "obsolete foreign file" list
		Pair.GetObsoleteLocFiles(ObsoleteFiles);

		// search for any sections that exist only in the english version, and add these section to
		// the "section missing from foreign file" list
		Pair.GetMissingSections(MissingSections);

		// search for any sections that exist only in the foreign version, and add these sections to
		// the "obsolete foreign sections" list
		Pair.GetObsoleteSections(ObsoleteSections);

		// for each section, search for any properties that only exist in the english version, and add these to the
		// "properties missing from foreign" section list
		Pair.GetMissingProperties(MissingProperties);

		// next, for each section, search for any properties that only exist in the foreign version, and
		// add these to the "obsolete properties in foreign section" list
		Pair.GetObsoleteProperties(ObsoleteProperties);

		// finally, find all properties that have identical values in both the english and foreigh versions,
		// and add these properties to the "haven't been localized" list
		Pair.GetUntranslatedProperties(UntranslatedProperties);

		if ( MissingSections.Num() || ObsoleteSections.Num() ||
			MissingProperties.Num() || ObsoleteProperties.Num() ||
			UntranslatedProperties.Num() )
		{
			warnf(TEXT("\r\n======== %s ========"), *Pair.GetFilename());
		}


		// display the results of our findings.
		if ( MissingSections.Num() )
		{
			warnf(TEXT("\r\n    MISSING SECTIONS:"));
			for ( INT i = 0; i < MissingSections.Num(); i++ )
			{
				warnf(TEXT("        %s"), *MissingSections(i));
			}
		}

		if ( ObsoleteSections.Num() )
		{
			warnf(TEXT("\r\n    OBSOLETE SECTIONS:"));
			for ( INT i = 0; i < ObsoleteSections.Num(); i++ )
			{
				warnf(TEXT("        %s"), *ObsoleteSections(i));
			}
		}

		if ( MissingProperties.Num() )
		{
			warnf(TEXT("\r\n    MISSING PROPERTIES:"));
			for ( INT i = 0; i < MissingProperties.Num(); i++ )
			{
				warnf(TEXT("        %s"), *MissingProperties(i));
			}
		}

		if ( ObsoleteProperties.Num() )
		{
			warnf(TEXT("\r\n    OBSOLETE PROPERTIES:"));
			for ( INT i = 0; i < ObsoleteProperties.Num(); i++ )
			{
				warnf(TEXT("        %s"), *ObsoleteProperties(i));
			}
		}

		if ( UntranslatedProperties.Num() )
		{
			warnf(TEXT("\r\n    UNTRANSLATED PROPERTIES:"));
			for ( INT i = 0; i < UntranslatedProperties.Num(); i++ )
			{
				warnf(TEXT("        %s"), *UntranslatedProperties(i));
			}
		}

	}
	if ( MissingFiles.Num() )
	{
		warnf(TEXT("\r\nEnglish files with no matching %s file:"), *LangExt);
		for ( INT i = 0; i < MissingFiles.Num(); i++ )
		{
			warnf(TEXT("  %s"), *MissingFiles(i));
		}
	}

	if ( ObsoleteFiles.Num() )
	{
		warnf(TEXT("\r\n%s files with no matching english file:"), *LangExt);
		for ( INT i = 0; i < ObsoleteFiles.Num(); i++ )
		{
			warnf(TEXT("  %s"), *ObsoleteFiles(i));
		}
	}

	return Result;
}
IMPLEMENT_CLASS(UCompareLocCommandlet);

/*-----------------------------------------------------------------------------
	UCompareLocCommandlet::FLocalizationFile.
-----------------------------------------------------------------------------*/

/**
 * Standard constructor
 */
UCompareLocCommandlet::FLocalizationFile::FLocalizationFile( const FString& InPath )
: LocFile(NULL)
{
	LocFilename = InPath;
	LocFile = static_cast<FConfigCacheIni*>(GConfig)->Find(*InPath, FALSE);
}

/** Copy ctor */
UCompareLocCommandlet::FLocalizationFile::FLocalizationFile( const FLocalizationFile& Other )
{
	LocFilename = Other.GetFullName();
	LocFile = Other.GetFile();

	//@todo ronp - should we also copy over the tracking arrays?
}

/** Dtor */
UCompareLocCommandlet::FLocalizationFile::~FLocalizationFile()
{
	LocFile = NULL;
}

/**
 * Determines whether this file is the counterpart for the loc file specified
 */
UBOOL UCompareLocCommandlet::FLocalizationFile::IsCounterpartFor( const FLocalizationFile& Other ) const
{
	UBOOL bResult = FALSE;

	FString OtherFilename = Other.GetFilename();
	if ( OtherFilename.Len() > 0 )
	{
		bResult = (OtherFilename == GetFilename());
	}

	return bResult;
}

/**
 * Compares the data in this loc file against the data in the specified counterpart file, placing the results in the various tracking arrays.
 */
void UCompareLocCommandlet::FLocalizationFile::CompareToCounterpart( FLocalizationFile* Other )
{
	check(Other);

	FConfigFile* OtherFile = Other->GetFile();
	check(OtherFile);

	// iterate through all sections in the loc file
	for ( FConfigFile::TIterator It(*LocFile); It; ++It )
	{
		const FString& SectionName = It.Key();
		FConfigSection& MySection = It.Value();

		// find this section in the counterpart loc file
		FConfigSection* OtherSection = OtherFile->Find(SectionName);
		if ( OtherSection != NULL )
		{
			// iterate through all keys in this section
			for ( FConfigSection::TIterator It(MySection); It; ++It )
			{
				const FName Propname = It.Key();
				const FString& PropValue = It.Value();

				// find this key in the counterpart loc file
				FString* OtherValue = OtherSection->Find(Propname);
				if ( OtherValue != NULL )
				{
					// if the counterpart has the same value as we do, the value is untranslated
					if ( PropValue == *OtherValue )
					{
						new(IdenticalProperties) FString(SectionName + TEXT(".") + Propname.ToString());
					}
				}
				else
				{
					// the counterpart didn't contain this key
					new(UnmatchedProperties) FString(SectionName + TEXT(".") + Propname.ToString());
				}
			}
		}
		else
		{
			// the counterpart didn't contain this section
			new(UnmatchedSections) FString(LocFilename.GetBaseFilename() + TEXT(".") + SectionName);
		}
	}
}

/** Accessors */
void UCompareLocCommandlet::FLocalizationFile::GetMissingSections( TArray<FString>& out_Sections ) const
{
	out_Sections += UnmatchedSections;
}
void UCompareLocCommandlet::FLocalizationFile::GetMissingProperties( TArray<FString>& out_Properties ) const
{
	out_Properties += UnmatchedProperties;
}
void UCompareLocCommandlet::FLocalizationFile::GetIdenticalProperties( TArray<FString>& out_Properties ) const
{
	out_Properties += IdenticalProperties;
}


/*-----------------------------------------------------------------------------
	UCompareLocCommandlet::FLocalizationFilePair.
-----------------------------------------------------------------------------*/
UCompareLocCommandlet::FLocalizationFilePair::~FLocalizationFilePair()
{
	if ( EnglishFile != NULL )
	{
		delete EnglishFile;
		EnglishFile = NULL;
	}

	if ( ForeignFile != NULL )
	{
		delete ForeignFile;
		ForeignFile = NULL;
	}
}
/**
 * Compares the two loc files against each other.
 */
void UCompareLocCommandlet::FLocalizationFilePair::CompareFiles()
{
	verify( HasEnglishFile() || HasForeignFile() );

	if ( HasEnglishFile() && HasForeignFile() )
	{
		EnglishFile->CompareToCounterpart(ForeignFile);
		ForeignFile->CompareToCounterpart(EnglishFile);			
	}
}

/**
 * Builds a list of files which exist in the english directory but don't have a counterpart in the foreign directory.
 */
void UCompareLocCommandlet::FLocalizationFilePair::GetMissingLocFiles( TArray<FString>& Files )
{
	if ( !HasForeignFile() )
	{
		new(Files) FString(EnglishFile->GetFilename());
	}
}

/**
 * Builds a list of files which no longer exist in the english loc directories.
 */
void UCompareLocCommandlet::FLocalizationFilePair::GetObsoleteLocFiles( TArray<FString>& Files )
{
	if ( !HasEnglishFile() )
	{
		new(Files) FString(ForeignFile->GetFilename());
	}
}

/**
 * Builds a list of section names which exist in the english version of the file but don't exist in the foreign version.
 */
void UCompareLocCommandlet::FLocalizationFilePair::GetMissingSections( TArray<FString>& Sections )
{
	if ( HasEnglishFile() && HasForeignFile() )
	{
		EnglishFile->GetMissingSections(Sections);
	}
}

/**
 * Builds a list of section names which exist in the foreign version but no longer exist in the english version.
 */
void UCompareLocCommandlet::FLocalizationFilePair::GetObsoleteSections( TArray<FString>& Sections )
{
	if ( HasEnglishFile() && HasForeignFile() )
	{
		ForeignFile->GetMissingSections(Sections);
	}
}

/**
 * Builds a list of key names which exist in the english version of the file but don't exist in the foreign version.
 */
void UCompareLocCommandlet::FLocalizationFilePair::GetMissingProperties( TArray<FString>& Properties )
{
	if ( HasEnglishFile() && HasForeignFile() )
	{
		EnglishFile->GetMissingProperties(Properties);
	}
}

/**
 * Builds a list of section names which exist in the foreign version but no longer exist in the english version.
 */
void UCompareLocCommandlet::FLocalizationFilePair::GetObsoleteProperties( TArray<FString>& Properties )
{
	if ( HasEnglishFile() && HasForeignFile() )
	{
		ForeignFile->GetMissingProperties(Properties);
	}
}

/**
 * Builds a list of property names which have the same value in the english and localized version of the file, indicating that the value isn't translated.
 */
void UCompareLocCommandlet::FLocalizationFilePair::GetUntranslatedProperties( TArray<FString>& Properties )
{
	if ( HasEnglishFile() && HasForeignFile() )
	{
		EnglishFile->GetIdenticalProperties(Properties);
	}
}

/**
 * Assigns the english version of the loc file pair.
 */
UBOOL UCompareLocCommandlet::FLocalizationFilePair::SetEnglishFile( const FString& EnglishFilename )
{
	if ( EnglishFilename.Len() == 0 )
	{
		return FALSE;
	}

	if ( EnglishFile )
	{
		delete EnglishFile;
		EnglishFile = NULL;
	}

	EnglishFile = new FLocalizationFile(EnglishFilename);
	return EnglishFile != NULL && EnglishFile->GetFile() != NULL;
}

/**
 * Assigns the foreign version of this loc file pair.
 */
UBOOL UCompareLocCommandlet::FLocalizationFilePair::SetForeignFile( const FString& ForeignFilename )
{
	if ( ForeignFilename.Len() == 0 )
	{
		return FALSE;
	}

	if ( ForeignFile )
	{
		delete ForeignFile;
		ForeignFile = NULL;
	}

	ForeignFile = new FLocalizationFile(ForeignFilename);
	return ForeignFile != NULL && ForeignFile->GetFile() != NULL;
}

const FString UCompareLocCommandlet::FLocalizationFilePair::GetFilename()
{
	return HasEnglishFile()
		? EnglishFile->GetFilename()
		: ForeignFile->GetFilename();
}

/** returns the filename (without path or extension info) for this file pair */
UBOOL UCompareLocCommandlet::FLocalizationFilePair::HasEnglishFile()
{
	return EnglishFile != NULL && EnglishFile->GetFile() != NULL;
}
UBOOL UCompareLocCommandlet::FLocalizationFilePair::HasForeignFile()
{
	return ForeignFile != NULL && ForeignFile->GetFile() != NULL;
}
UBOOL UCompareLocCommandlet::FLocalizationFilePair::HasEnglishFile( const FString& Filename )
{
	return HasEnglishFile() && Filename == EnglishFile->GetFilename();
}
UBOOL UCompareLocCommandlet::FLocalizationFilePair::HasForeignFile( const FString& Filename )
{
	return HasForeignFile() && Filename == ForeignFile->GetFilename();
}





