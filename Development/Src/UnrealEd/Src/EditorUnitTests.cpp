// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "UnObjectTools.h"

/**
 * FGenericImportAssetsUnitTest
 * Simple unit test that attempts to import every file (except .txt files) within the unit test directory in a sub-folder
 * named "GenericImport." Used to test the basic codepath that would execute if a user imported a file using the interface
 * in the Content Browser (does not allow for specific settings to be made per import factory). Cannot be run in a commandlet
 * as it executes code that routes through wxWidgets.
 */
IMPLEMENT_UNIT_TEST( FGenericImportAssetsUnitTest, UTF_Editor | UTF_PC | UTF_RequiresNonNullRHI )

/** 
 * Execute the generic import test
 *
 * @return	TRUE if the test was successful, FALSE otherwise
 */
UBOOL FGenericImportAssetsUnitTest::RunTest()
{
	// Find all files in the GenericImport directory
	TArray<FString> FilesInDirectory;
	FString ImportDirectory = FUnitTestFramework::GetUnitTestDirectory() * TEXT("GenericImport");
	appFindFilesInDirectory( FilesInDirectory, *ImportDirectory, FALSE, TRUE );

	// Scan all the found files, ignoring .txt files which are likely P4 placeholders for creating directories
	wxArrayString FilesToImport;
	for ( TArray<FString>::TConstIterator FileIter( FilesInDirectory ); FileIter; ++FileIter )
	{
		if ( FFilename( *FileIter ).GetExtension() != TEXT("txt") )
		{
			FilesToImport.Add( **FileIter );
		}
	}
	
	UBOOL bAllSuccessful = TRUE;

	if ( FilesToImport.Count() > 0 )
	{
		TArray<UFactory*> Factories;		

		// Needed by AssembleListofImportFactories but unused here
		FString FileTypes, AllExtensions;
		TMultiMap<INT, UFactory*> FilterToFactory;

		// Obtain a list of factories to use for importing
		ObjectTools::AssembleListOfImportFactories( Factories, FileTypes, AllExtensions, FilterToFactory );

		INT CurErrorIndex = 0, CurWarningIndex = 0, CurLogItemIndex = 0;
		
		// For each file found, attempt to import it
		for ( UINT FileIndex = 0; FileIndex < FilesToImport.Count(); ++FileIndex )
		{
			wxArrayString CurFileToImport;
			CurFileToImport.Add( FilesToImport[FileIndex] );
			FString CleanFilename = FFilename( FilesToImport[FileIndex].c_str() ).GetCleanFilename();

			const UBOOL CurTestSuccessful = ObjectTools::ImportFiles( CurFileToImport, Factories, NULL, FUnitTestFramework::GetUnitTestPackageName() );
			bAllSuccessful = bAllSuccessful && CurTestSuccessful;
			
			// Any errors, warnings, or log items that are caught during this unit test aren't guaranteed to include the name of the file that generated them,
			// which can be confusing when reading results. Alleviate the issue by injecting the file name for each error, warning, or log item, where appropriate.
			for ( INT ErrorStartIndex = CurErrorIndex; ErrorStartIndex < ExecutionInfo.Errors.Num(); ++ErrorStartIndex )
			{
				ExecutionInfo.Errors(ErrorStartIndex) = FString::Printf( TEXT("%s: %s"), *CleanFilename, *ExecutionInfo.Errors(ErrorStartIndex) );
			}
			for ( INT WarningStartIndex = CurWarningIndex; WarningStartIndex < ExecutionInfo.Warnings.Num(); ++WarningStartIndex )
			{
				ExecutionInfo.Warnings(WarningStartIndex) = FString::Printf( TEXT("%s: %s"), *CleanFilename, *ExecutionInfo.Warnings(WarningStartIndex) );
			}
			for ( INT LogItemStartIndex = CurLogItemIndex; LogItemStartIndex < ExecutionInfo.LogItems.Num(); ++LogItemStartIndex )
			{
				ExecutionInfo.LogItems(LogItemStartIndex) = FString::Printf( TEXT("%s: %s"), *CleanFilename, *ExecutionInfo.LogItems(LogItemStartIndex) );
			}
			CurErrorIndex = ExecutionInfo.Errors.Num();
			CurWarningIndex = ExecutionInfo.Warnings.Num();
			CurLogItemIndex = ExecutionInfo.LogItems.Num();
		}
	}
	else
	{
		GWarn->Logf( NAME_Warning, *FString::Printf( LocalizeSecure( LocalizeUnrealEd("UnitTest_GenericImport_NoFilesFoundToImport"), *ImportDirectory ) ) );
	}
	
	return bAllSuccessful;
}
