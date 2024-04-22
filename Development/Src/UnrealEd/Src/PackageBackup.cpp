/*=============================================================================
	PackageBackup.cpp: Utility class for backing up a package.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "PackageBackup.h"

#if !CONSOLE && WITH_EDITOR

/**
 * Helper struct to hold information on backup files to prevent redundant checks
 */
struct FBackupFileInfo
{
	INT FileSize;								/** Size of the file */
	FString FileName;							/** Name of the file */
	FFileManager::FTimeStamp FileTimeStamp;		/** Timestamp of the file */
};

/** Implement the sorting criteria for the FBackupFileInfo struct */
IMPLEMENT_COMPARE_CONSTREF( FBackupFileInfo, PackageBackup, { return ( A.FileTimeStamp > B.FileTimeStamp ) ? 1 : ( B.FileTimeStamp > A.FileTimeStamp ) ? -1 : 0; } )

/**
 * Create a backup of the specified package. A backup is only created if the specified
 * package meets specific criteria, as outlined in the comments for ShouldBackupPackage().
 *
 * @param	InPackage	Package which should be backed up
 *
 * @see		FAutoPackageBackup::ShouldBackupPackage()
 *
 * @return	TRUE if the package was successfully backed up; FALSE if it was not
 */
UBOOL FAutoPackageBackup::BackupPackage( const UPackage& InPackage )
{
	UBOOL bPackageBackedUp = FALSE;
	FFilename OriginalFileName;
	
	// Check if the package is valid for being backed up
	if ( ShouldBackupPackage( InPackage, OriginalFileName ) )
	{
		GWarn->StatusUpdatef( -1, -1, *LocalizeUnrealEd("PackageBackup_Warning") );

		// Construct the backup file name by appending a timestamp in between the base file name and extension
		FFilename DestinationFileName = GetBackupDirectory() * OriginalFileName.GetBaseFilename();
		DestinationFileName += TEXT("_");

		// Replace the time string's periods with -'s so as not to confuse the package cache
		FString TimeStampString = appSystemTimeString();
		TimeStampString.ReplaceInline( TEXT("."), TEXT("-") );
		DestinationFileName += TimeStampString;
		DestinationFileName += OriginalFileName.GetExtension( TRUE );

		// Copy the file to the backup file name
		GFileManager->Copy( *DestinationFileName, *OriginalFileName );

		// Update the file's timestamp to the current time
		GFileManager->TouchFile( *DestinationFileName );

		bPackageBackedUp = TRUE;
	}
	
	return bPackageBackedUp;
}

/**
 * Create a backup of the specified packages. A backup is only created if a specified
 * package meets specific criteria, as outlined in the comments for ShouldBackupPackage().
 *
 * @param	InPackage	Package which should be backed up
 *
 * @see		FAutoPackageBackup::ShouldBackupPackage()
 *
 * @return	TRUE if all provided packages were successfully backed up; FALSE if one or more
 *			were not
 */
UBOOL FAutoPackageBackup::BackupPackages( const TArray<UPackage*>& InPackages )
{
	UBOOL bAllPackagesBackedUp = TRUE;
	for ( TArray<UPackage*>::TConstIterator PackageIter( InPackages ); PackageIter; ++PackageIter )
	{
		UPackage* CurPackage = *PackageIter;
		check( CurPackage );

		if ( !BackupPackage( *CurPackage ) )
		{
			bAllPackagesBackedUp = FALSE;
		}
	}
	return bAllPackagesBackedUp;
}

/**
 * Helper function designed to determine if the provided package should be backed up or not.
 * The function checks for many conditions, such as if the package is too large to backup,
 * if the package has a particular attribute that should prevent it from being backed up (such
 * as being marked for PIE-use), if cooking is in progress, etc.
 *
 * @param	InPackage		Package which should be checked to see if its valid for backing-up
 * @param	OutFileName		File name of the package on disk if the function determines the package
 *							already existed
 *
 * @return	TRUE if the package is valid for backing-up; FALSE otherwise
 */
UBOOL FAutoPackageBackup::ShouldBackupPackage( const UPackage& InPackage, FString& OutFilename )
{
	// Check various conditions to see if the package is a valid candidate for backing up
	UBOOL bShouldBackup =
		GIsEditor																			// Backing up packages only makes sense in the editor
		&& !GIsUCC																			// Don't backup saves resulting from commandlets
		&& IsPackageBackupEnabled()															// Ensure that the package backup is enabled in the first place
		&& !GIsCooking																		// Don't back up packages from cooking
		&& ( InPackage.PackageFlags & PKG_PlayInEditor ) == 0								// Don't back up PIE packages
		&& ( InPackage.PackageFlags & PKG_ContainsScript ) == 0;							// Don't back up script packages

	if( bShouldBackup )
	{
		GWarn->StatusUpdatef( -1, -1, *LocalizeUnrealEd("PackageBackup_ValidityWarning") );

		bShouldBackup =	GPackageFileCache->FindPackageFile( *InPackage.GetName(), NULL, OutFilename );	// Make sure the file already exists (no sense in backing up a new package)
	}
	
	// If the package passed the initial backup checks, proceed to check more specific conditions
	// that might disqualify the package from being backed up
	const INT FileSizeOfBackup = GFileManager->FileSize( *OutFilename );
	if ( bShouldBackup )
	{
		// Ensure that the size the backup would require is less than that of the maximum allowed
		// space for backups
		bShouldBackup = FileSizeOfBackup <= GetMaxAllowedBackupSpace();
	}

	// Ensure that the package is not one of the shader caches
	if ( bShouldBackup )
	{
		for( INT PlatformIndex = 0; PlatformIndex < SP_NumPlatforms && bShouldBackup; ++PlatformIndex )
		{
			EShaderPlatform CurPlatform = (EShaderPlatform)PlatformIndex;
			bShouldBackup = 
				OutFilename != GetLocalShaderCacheFilename( CurPlatform )
				&& OutFilename != GetGlobalShaderCacheFilename( CurPlatform )
				&& OutFilename != GetReferenceShaderCacheFilename( CurPlatform );
		}
	}

	// If all of the prior checks have passed, now see if the package has been backed up
	// too recently to be considered for an additional backup
	if ( bShouldBackup )
	{
		// Ensure that the autosave/backup directory exists
		const FString& FullAutoSaveDir = GetBackupDirectory();
		GFileManager->MakeDirectory( *FullAutoSaveDir, 1 );

		// Find all of the files in the backup directory
		TArray<FString> FilesInBackupDir;
		appFindFilesInDirectory( FilesInBackupDir, *FullAutoSaveDir, TRUE, TRUE );

		// Extract the base file name and extension from the passed-in package file name
		FFilename ExistingFileName( OutFilename );
		FString ExistingBaseFileName = ExistingFileName.GetBaseFilename();
		FString ExistingFileNameExtension = ExistingFileName.GetExtension();

		UBOOL bFoundExistingBackup = FALSE;
		INT DirectorySize = 0;
		FFileManager::FTimeStamp LastBackupTimeStamp;
		appMemzero( &LastBackupTimeStamp, sizeof( FFileManager::FTimeStamp ) );

		TArray<FBackupFileInfo> BackupFileInfoArray;
		
		// Check every file in the backup directory for matches against the passed-in package
		// (Additionally keep statistics on all backup files for potential maintenance)
		for ( TArray<FString>::TConstIterator FileIter( FilesInBackupDir ); FileIter; ++FileIter )
		{
			const FFilename CurBackupFileName = FFilename( *FileIter );
			
			// Create a new backup file info struct for keeping information about each backup file
			const INT FileInfoIndex = BackupFileInfoArray.AddZeroed();
			FBackupFileInfo& CurBackupFileInfo = BackupFileInfoArray( FileInfoIndex );
			
			// Record the backup file's name, size, and timestamp
			CurBackupFileInfo.FileName = CurBackupFileName;
			CurBackupFileInfo.FileSize = GFileManager->FileSize( *CurBackupFileName );
			
			// If we failed to get a timestamp or a valid size, something has happened to the file and it shouldn't be considered
			if ( !( GFileManager->GetTimestamp( *CurBackupFileName, CurBackupFileInfo.FileTimeStamp ) ) || CurBackupFileInfo.FileSize == -1 )
			{
				BackupFileInfoArray.Remove( BackupFileInfoArray.Num() - 1 );
				continue;
			}

			// Calculate total directory size by adding the size of this backup file
			DirectorySize += CurBackupFileInfo.FileSize;

			FString CurBackupBaseFileName =  CurBackupFileName.GetBaseFilename();
			FString CurBackupFileNameExtension = CurBackupFileName.GetExtension();

			// The base file name of the backup file is going to include an underscore followed by a timestamp, so they must be removed for comparison's sake
			CurBackupBaseFileName = CurBackupBaseFileName.Left( CurBackupBaseFileName.InStr( TEXT("_"), TRUE ) );
					
			// If the base file names and extensions match, we've found a backup
			if ( CurBackupBaseFileName == ExistingBaseFileName &&  CurBackupFileNameExtension == ExistingFileNameExtension )
			{
				bFoundExistingBackup = TRUE;

				// Keep track of the most recent matching time stamp so we can check if the passed-in package
				// has been backed up too recently
				if ( CurBackupFileInfo.FileTimeStamp > LastBackupTimeStamp )
				{
					LastBackupTimeStamp = CurBackupFileInfo.FileTimeStamp;
				}
			}
		}

		// If there was an existing backup, check to see if it was created too recently to allow another
		// backup
		if ( bFoundExistingBackup )
		{
			// appSystemTime and the like use the local system time, whereas the file manager's timestamps
			// use gmtime, so we are forced to use gmtime here too for comparison

			// Get the current time in FTimeStamp form
			time_t CurTime;
			time ( &CurTime );

#if USE_SECURE_CRT
			tm CurTimeStruct;
			gmtime_s( &CurTimeStruct, &CurTime );
#else
			tm& CurTimeStruct = *gmtime( &CurTime );
#endif // #if USE_SECURE_CRT
			
			FFileManager::FTimeStamp CurTimeStamp;
			appMemzero( &CurTimeStamp, sizeof( FFileManager::FTimeStamp ) );
			CurTimeStamp.Day       = CurTimeStruct.tm_mday;
			CurTimeStamp.Month     = CurTimeStruct.tm_mon;
			CurTimeStamp.DayOfWeek = CurTimeStruct.tm_wday;
			CurTimeStamp.Hour      = CurTimeStruct.tm_hour;
			CurTimeStamp.Minute    = CurTimeStruct.tm_min;
			CurTimeStamp.Second    = CurTimeStruct.tm_sec;
			CurTimeStamp.Year      = CurTimeStruct.tm_year + 1900;

			// Check the difference in timestamp seconds against the backup interval; if not enough time has elapsed since
			// the last backup, we don't want to make another one
			if ( ( CurTimeStamp.GetJulian() == LastBackupTimeStamp.GetJulian() ) 
			&& ( ( CurTimeStamp.GetSecondOfDay() - LastBackupTimeStamp.GetSecondOfDay() ) < ( GetBackupInterval() ) ) )
			{
				bShouldBackup = FALSE;
			}
		}

		// If every other check against the package has succeeded for backup purposes, ensure there is enough directory space
		// available in the backup directory, as adding the new backup might use more space than the user allowed for backups.
		// If the backup file size + the current directory size exceeds the max allowed space, deleted old backups until there
		// is sufficient space. If enough space can't be freed for whatever reason, then no back-up will be created.
		if ( bShouldBackup && ( FileSizeOfBackup + DirectorySize > GetMaxAllowedBackupSpace() ) )
		{
			bShouldBackup = PerformBackupSpaceMaintenance( BackupFileInfoArray, DirectorySize, FileSizeOfBackup );
		}
	}
	
	return bShouldBackup;
}

/**
 * Helper function that returns whether the user has package backups enabled or not. The value
 * is determined by a configuration INI setting.
 *
 * @return	TRUE if package backups are enabled; FALSE otherwise
 */
UBOOL FAutoPackageBackup::IsPackageBackupEnabled()
{
	UBOOL bEnabled = FALSE;
	GConfig->GetBool( TEXT("FAutoPackageBackup"), TEXT("Enabled"), bEnabled, GEditorUserSettingsIni );
	return bEnabled;
}

/**
 * Helper function that returns the maximum amount of space the user has designated to allow for
 * package backups. This value is determined by a configuration INI setting.
 *
 * @return	The maximum amount of space allowed, in bytes
 */
INT FAutoPackageBackup::GetMaxAllowedBackupSpace()
{
	INT MaxSpaceAllowed = 0;
	if ( GConfig->GetInt( TEXT("FAutoPackageBackup"), TEXT("MaxAllowedSpaceInMB"), MaxSpaceAllowed, GEditorUserSettingsIni ) )
	{
		// Convert the user stored value from megabytes to bytes; <<= 20 is the same as *= 1024 * 1024
		MaxSpaceAllowed <<= 20;
	}
	return MaxSpaceAllowed;
}

/**
 * Helper function that returns the time in between backups of a package before another backup of
 * the same package should be considered valid. This value is determined by a configuration INI setting,
 * and prevents a package from being backed-up over and over again in a small time frame.
 *
 * @return	The interval to wait before allowing another backup of the same package, in seconds
 */
INT FAutoPackageBackup::GetBackupInterval()
{
	INT BackupInterval = 0;
	if ( GConfig->GetInt( TEXT("FAutoPackageBackup"), TEXT("BackupIntervalInMinutes"), BackupInterval, GEditorUserSettingsIni ) )
	{
		// Convert the user stored value from minutes to seconds
		BackupInterval *= 60;
	}
	return BackupInterval;
}

/**
 * Helper function that returns the directory to store package backups in.
 *
 * @return	String containing the directory to store package backups in.
 */
FString FAutoPackageBackup::GetBackupDirectory()
{
	FString Directory = FString( appBaseDir() ) * GEditor->AutoSaveDir * TEXT("Backup");
	return Directory;
}

/**
 * Deletes old backed-up package files until the provided amount of space (in bytes)
 * is available to use in the backup directory. Fails if the provided amount of space
 * is more than the amount of space the user has allowed for backups or if enough space
 * could not be made.
 *
 * @param	InBackupFiles		File info of the files in the backup directory
 * @param	InSpaceUsed			The amount of space, in bytes, the files in the provided array take up
 * @param	InSpaceRequired		The amount of space, in bytes, to assure is available
 *								for the purposes of package backups
 *
 * @return	TRUE if the space was successfully provided, FALSE if not or if the requested space was
 *			greater than the max allowed space by the user
 */
UBOOL FAutoPackageBackup::PerformBackupSpaceMaintenance( TArray<FBackupFileInfo>& InBackupFiles, INT InSpaceUsed, INT InSpaceRequired )
{
	UBOOL bSpaceFreed = FALSE;

	const INT MaxAllowedSpace = GetMaxAllowedBackupSpace();
	
	// We can only free up enough space if the required space is less than the maximum allowed space to begin with
	if ( InSpaceRequired < MaxAllowedSpace )
	{
		GWarn->StatusUpdatef( -1, -1, *LocalizeUnrealEd("PackageBackup_MaintenanceWarning") );
		
		// Sort the backup files in order of their timestamps; we want to naively delete the oldest files first
		Sort< USE_COMPARE_CONSTREF( FBackupFileInfo, PackageBackup ) >( &InBackupFiles( 0 ), InBackupFiles.Num() );
		
		INT CurSpaceUsed = InSpaceUsed;
		TArray<FBackupFileInfo>::TConstIterator BackupFileIter( InBackupFiles );
		
		// Iterate through the backup files until all of the files have been deleted or enough space has been freed
		while ( ( InSpaceRequired + CurSpaceUsed > MaxAllowedSpace ) && BackupFileIter )
		{
			const FBackupFileInfo& CurBackupFileInfo = *BackupFileIter;
			
			// Delete the file; this could potentially fail, but not because of a read-only flag, so if it fails
			// it's likely because the file was removed by the user
			GFileManager->Delete( *CurBackupFileInfo.FileName, TRUE, TRUE );
			CurSpaceUsed -= CurBackupFileInfo.FileSize;
			++BackupFileIter;
		}
		if ( InSpaceRequired + CurSpaceUsed <= MaxAllowedSpace )
		{
			bSpaceFreed = TRUE;
		}
	}

	return bSpaceFreed;
}

#endif // #if !CONSOLE && WITH_EDITOR
