// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "FTableOfContents.h"
#include "FConfigCacheIni.h"

#if WITH_MANAGED_CODE
#include "NewProjectShared.h"
#endif

#if _WINDOWS
#include "FFileManagerWindows.h"
#endif

#if UDK && !CONSOLE && _WINDOWS
#include <shellapi.h> // For shellexecute
#endif

#include "tinyxml.h"


// These characters cannot be used in proj wiz names.
#define PROJ_WIZ_INVALID_NAME_CHARACTERS	TEXT(" !\"#$%&'()*+,./:;<=>?@[\\]^`{|}~ \n\r\t")

/**
 * Takes an FString and checks to see that it follows the rules that ProjWiz requires.

 * @param	InString		The string to check
 * @param	InvalidChars	The set of invalid characters that the name cannot contain
 *
 * @return	TRUE if the string does not contain invalid characters, false if it does contain invalid characters
 */
UBOOL IsValidString( const FString& InString, FString InvalidChars=PROJ_WIZ_INVALID_NAME_CHARACTERS )
{
	// See if the string contains invalid characters.
	FString Char;
	
	// Make sure the string starts with an alpha character
	if ( !appIsAlpha(InString[0]) )
	{
		return FALSE;
	}

	// Check to make sure the string does not use any characters from the invalid set
	for( INT x = 0; x < InvalidChars.Len() ; ++x )
	{
		Char = InvalidChars.Mid( x, 1 );

		if( InString.InStr( Char ) != INDEX_NONE )
		{
			// A character from the invalid set has been detected in the input string.
			return FALSE;
		}
	}

	return TRUE;
}

FProjWizTemplate::FProjWizTemplate( const FString& InFullPath )
	:	FullPath( InFullPath )
	,	InstallSize( -1 )
	,	InstallSizeInternal( -1 )
{
}

/** 
* Obtains the localized name for the template, if none is found it will return the name of the directory
* the template is located in.
*
* @return Localized template name or directory name if no localized entry is found.
*/
FString FProjWizTemplate::GetName( ) const
{
	FFilename DirectoryPath( FullPath );
	FString DirectoryName = DirectoryPath.GetBaseFilename();

	// Generate the loc lookup name.  For a folder that is named Template1 the loc value is ProjWiz_TemplateName_Template1
	FString LocLookup(TEXT("ProjWiz_TemplateName_"));
	LocLookup += DirectoryName;

	FString OutString = LocalizeUnrealEd( TCHAR_TO_ANSI(*LocLookup) );
	if( OutString.IsEmpty() || OutString[0] == '?' )
	{
		// We didn't find a loc entry for this template so we will return the directory name.
		OutString = DirectoryName;
	}
	
	return OutString;
}

/** 
* Used by external sources to set the template install size.  This represents the actual
* on disk size when this template is installed along with the BaseTemplate and other project
* related files.  This function is needed because  We don't have enough info in the template structure 
* to calculate our own on-disk install size.  The reason is that the  base template along with other files
* are installed with any given template.  For this reason, we rely on the setting of the install size value from
* external sources.  If the install size is not set for us, we will just return our internal calculated size.
*
* @param	InInstallSize	The template install size.
*/
SQWORD FProjWizTemplate::GetInstallSize()
{
	if( InstallSize == -1 )
	{
		return (InstallSizeInternal >= 0 ) ? InstallSizeInternal : CalcInternalInstallSize(); 
	}
	return InstallSize;
}

/** 
* Used by external sources to set the template install size.  This represents the actual
* on disk size when this template is installed along with the BaseTemplate and other project
* related files.  This function is needed because  We don't have enough info in the template structure 
* to calculate our own on-disk install size.  The reason is that the  base template along with other files
* are installed with any given template.  For this reason, we rely on the setting of the install size value from
* external sources.  If the install size is not set for us, we will just return our internal calculated size.
*
* @param	InInstallSize	The template install size.
*/
void FProjWizTemplate::SetInstallSize( SQWORD InInstallSize )
{
	check( InInstallSize >= 0 );
	InstallSize = InInstallSize;
}

/** 
* Calculates the install size for this template and caches the value for future retrieval.
* This value will not be a fully accurate representation of the on disk size of this template when
* used to generate a project.  For that we need the install size to be set externally via SetInstallSize().
*
* @return	Returns the calculated size.
*/
INT FProjWizTemplate::CalcInternalInstallSize()
{
	InstallSizeInternal = 0;
	TArray<FProjWizFileInfo> TemplateFiles;
	GetTemplateFiles( TemplateFiles );
	for (INT FileIndex = 0; FileIndex < TemplateFiles.Num(); FileIndex++)
	{
		InstallSizeInternal += TemplateFiles( FileIndex ).FileSize;
	}
	return InstallSizeInternal;
}


/** 
* Retrieves a list of files that are included in the template.  Will exclude files not used by the game.
*
* @param	OutResult	String array that will store the files list
*
*/
void FProjWizTemplate::GetTemplateFiles( TArray<FString>& OutResult )
{

	OutResult.Empty();

	// If the internal template file list structure is empty we will try to fill it first.
	if( TemplateFiles.Num() == 0 )
	{
		FillTemplateFilesCache();
	}

	for( INT FileIndex = 0; FileIndex < TemplateFiles.Num(); ++FileIndex )
	{
		OutResult.AddItem( TemplateFiles( FileIndex ).SourcePath );
	}
}

/** 
* Retrieves a list of files that are included in the template.  Will exclude files not used by the game.
*
* @param	OutResult	FProjWizFileInfo array that will store the files list
*
*/
void FProjWizTemplate::GetTemplateFiles( TArray<FProjWizFileInfo>& OutResult )
{

	OutResult.Empty();

	// If the internal template file list structure is empty we will try to fill it first.
	if( TemplateFiles.Num() == 0 )
	{
		FillTemplateFilesCache();
	}

	for( INT FileIndex = 0; FileIndex < TemplateFiles.Num(); ++FileIndex )
	{
		OutResult.AddItem( TemplateFiles( FileIndex ) );
	}
}

/** 
* Retrieves a list of advanced settings that the user can set for this template.
*
* @param	OutResult		Empty string array that will store the files list.
*
*/
void FProjWizTemplate::GetTemplateAdvancedSettings( TArray<FUserProjSetting>& OutResult )
{
	OutResult.Empty();

	// If the internal structure is empty we will try to fill it first.
	if( TemplateAdvancedSettings.Num() == 0 )
	{
		FillTemplateAdvancedSettingCache();
	}

	for( INT SettingIndex = 0; SettingIndex < TemplateAdvancedSettings.Num(); ++SettingIndex )
	{
		OutResult.AddItem( TemplateAdvancedSettings( SettingIndex ) );
	}
}

/** 
* Returns a reference to the TemplateInfo.ini config file(). 
*/
FConfigFile* FProjWizTemplate::GetTemplateConfigFile()
{
	// First we check to see if the config file was loaded, if not load it.  There may be better ways of checking if it was loaded,
	//  but for now we assume if it has no entries we never loaded it or attempted to load it.
	if( TemplateConfigFile.Num() == 0 )
	{
		LoadTemplateConfigFile();
	}

	return &TemplateConfigFile;
}

/** 
* Fills the TemplateFiles structure with files we find in the template. 
*/
void FProjWizTemplate::FillTemplateFilesCache()
{
	TArray<FString> DirectoryFiles;
	appFindFilesInDirectory(DirectoryFiles, *(GetFullPath()), TRUE, TRUE);
	for (INT FileIndex = 0; FileIndex < DirectoryFiles.Num(); FileIndex++)
	{
		const FFilename FileSourcePath = DirectoryFiles(FileIndex);

		// Templates have additional files that should not be used when generating a project.  These files store settings and
		//  meta data about the template.  We exclude those files here.
		if( FileSourcePath.EndsWith("TemplateInfo.ini") )
		{
			continue;
		}

		FProjWizFileInfo TemplateFile;
		TemplateFile.SourcePath = FileSourcePath;
		TemplateFile.FileSize = GFileManager->FileSize( *FileSourcePath );
		TemplateFiles.AddItem( TemplateFile );
	}
}

/** 
* Fills the advanced setting structure with items we pulled from the template ini and extracts default values from the game config files in the template. 
*/
void FProjWizTemplate::FillTemplateAdvancedSettingCache()
{
	// Note: We don't currently account for the case where a config option has the same key/value pair but a different group or platform.

	check( TemplateAdvancedSettings.Num() == 0 );

	FConfigFile* LocalTemplateConfig = GetTemplateConfigFile();
	FConfigSection* AdvancedOptionsFromIni = LocalTemplateConfig->Find( TEXT("UnrealEd.ProjectWizardAdvancedOptions") );

	// We store off a list of local advanced settings because we are not guaranteed to find all the settings we pulled from the TemplateInfo.ini in 
	//  the game config files.  In that case we don't want to add them to the UI
	TArray<FUserProjSetting> LocalTemplateAdvancedSettings;

	if( AdvancedOptionsFromIni )
	{
		for (FConfigSectionMap::TIterator It(*AdvancedOptionsFromIni); It; ++It)
		{
			FUserProjSetting SettingToAdd;
			SettingToAdd.SettingDisplayGroup = It.Key().ToString();
			SettingToAdd.SettingKey = It.Value();

			LocalTemplateAdvancedSettings.AddItem( SettingToAdd );
		}
	}

	FSystemSettings SysSettings = GSystemSettings;

	// Loop through all the settings we wish to display in the GUI, see if they exist in the system settings, and retrieve 
	//  their values if they do.  We will not display items that don't have a corresponding match in the system settings.
	for( INT SettingIndex = 0; SettingIndex < LocalTemplateAdvancedSettings.Num(); ++SettingIndex )
	{
		FString AdvancedSettingName = *LocalTemplateAdvancedSettings( SettingIndex ).SettingKey;
		FSystemSetting* Setting = SysSettings.FindSystemSetting( AdvancedSettingName, SST_ANY );

		// If the AdvancedSettingName matches one of the system settings, we will retrieve the value and add this 
		//  setting to the list we will display in the GUI
		if( Setting != NULL && Setting->SettingAddress != NULL)
		{
			FString SettingVal;
			switch( Setting->SettingType )
			{
			case SST_BOOL:
				SettingVal = *( UBOOL* )Setting->SettingAddress ? "true" : "false";
				break;

			case SST_INT:
				SettingVal = appItoa(*( INT* )Setting->SettingAddress);
				break;

			case SST_FLOAT:
				SettingVal =  FString::Printf(TEXT("%f"), *( FLOAT* )Setting->SettingAddress);
				break;

			}

			LocalTemplateAdvancedSettings( SettingIndex ).SettingValue = SettingVal;
			TemplateAdvancedSettings.AddItem( LocalTemplateAdvancedSettings( SettingIndex ) );
		}
	}
}

/**
* Fills the cached template config file structure
*/
void FProjWizTemplate::LoadTemplateConfigFile()
{
	FString TemplateConfigFilePath = GetFullPath() * TEXT("TemplateInfo.ini");

	// NOTE: If we ever want our source project to differ from the execution project, we won't be able to use LoadAnIniFile if the ini file uses 'BasedOn'.  The problem is that
	//    LoadAnIniFile always assumes the relative path info provided by BasedOn will be located in the project the current executable is running from.
	LoadAnIniFile( *TemplateConfigFilePath, TemplateConfigFile, FALSE );
}


/** 
* Obtains the localized description for the template, if none is found it will return the empty string.
*
* @return	Localized template description or the empty string if none is found.
*/
FString FProjWizTemplate::GetDescription( ) const
{
	FFilename DirectoryPath( FullPath );
	FString DirectoryName = DirectoryPath.GetBaseFilename();

	// Generate the loc lookup name for the description.  For a folder that is 
	//  named Template1 the loc value is ProjWiz_TemplateDesc_Template1
	FString LocLookup(TEXT("ProjWiz_TemplateDesc_"));
	LocLookup += DirectoryName;

	FString OutString = LocalizeUnrealEd( TCHAR_TO_ANSI(*LocLookup) );
	if( OutString.IsEmpty() || OutString[0] == '?' )
	{
		// We didn't find a loc entry for this template so we will return the directory name.
		OutString = TEXT("");
	}

	return OutString;
}


/** Static: Global new UDK project settings */
FNewUDKProjectSettings FNewUDKProjectSettings::StaticNewUDKProjectSettings;


FNewUDKProjectWizard::FNewUDKProjectWizard( )
	:	LastErrorCode(  EUDKProjWizSettingReturnCodes::CODE_NoError )

{
	GenerateDefaultSettings();
}

FNewUDKProjectWizard::~FNewUDKProjectWizard()
{
}

/** 
* Searches the template folder for a specified project and retrieves all the template names. 
*
* @param	InDirectoryPath		Project directory path.
*/
void FNewUDKProjectWizard::PopulateTemplateList( const FString& InDirectoryPath )
{
	// Cache a temp FString of the root directory
	FString Root(InDirectoryPath);
	Root *= TEXT("UDKGame\\ProjectTemplates");

	// Make a wild card to look for directories in the current directory
	FString Wildcard = Root * TEXT("*.*");
	TArray<FString> SubDirs; 
	GFileManager->FindFiles(SubDirs, *Wildcard, FALSE, TRUE);
	
	for (INT SubDirIndex = 0; SubDirIndex < SubDirs.Num(); SubDirIndex++)
	{
		FString CurSubDir = SubDirs(SubDirIndex);
		FProjWizTemplate& TemplateInfo = TemplateItems.Set( Root * CurSubDir, FProjWizTemplate( Root * CurSubDir ) );
	}
}

/** Sets some appropriate default settings based on the UDK environment */
void FNewUDKProjectWizard::GenerateDefaultSettings()
{
	FString & SourceProject = FNewUDKProjectSettings::Get().SourceProject;

	// If no source project was provided, we use the current application root directory.
	if( SourceProject.IsEmpty() )
	{
		SourceProject = appRootDir();
	}

	// Strip off the last path separator if there is one
	if( SourceProject.Right(1) == PATH_SEPARATOR )
	{
		SourceProject = SourceProject.LeftChop(1);
	}

	FString RootDir =SourceProject;
	FString ProjectDir = FNewUDKProjectSettings::Get().ShortNameSetting;
	FString FinalNewProjectDir = RootDir * FString(TEXT("..\\")) + ProjectDir;
	FinalNewProjectDir = appCollapseRelativeDirectories(FinalNewProjectDir);
	FString ScriptPackageDir =  FNewUDKProjectSettings::Get().ShortNameSetting + TEXT("Game");

	if( appDirectoryExists( *FinalNewProjectDir ) )
	{
		if( FindAvailableDirectoryPath(FinalNewProjectDir, FinalNewProjectDir, 1) == -1 )
		{
			// We failed to generate an appropriate install directory name.
			GWarn->Logf(NAME_Warning, TEXT("Project Wizard failed to generate an appropraite install directory path."));
			FinalNewProjectDir = TEXT("");
		}
	}

	check(FinalNewProjectDir.Len() > 0);
	check(FinalNewProjectDir.Len() < MAX_PATH);
	
	FNewUDKProjectSettings::Get().InstallDirectorySetting = FinalNewProjectDir;
	
}

/**
 * Returns a human readable string from an error code.  If no code is passed in, the last project wizard error code will be used.
 *
 * @param Code	The error code to check
 */
FString FNewUDKProjectWizard::GetErrorString( INT Code )
{
	if ( Code == -1 )
	{
		Code = GetLastErrorCode();
	}
	switch ( Code )
	{

		case EUDKProjWizSettingReturnCodes::CODE_NoError:					return FString(*LocalizeUnrealEd("NewProject_Error_NoError"));
		case EUDKProjWizSettingReturnCodes::CODE_InvalidTargetDirectory:	return FString(*LocalizeUnrealEd("NewProject_Error_InstallDir"));
		case EUDKProjWizSettingReturnCodes::CODE_InvalidProjectName:		return FString(*LocalizeUnrealEd("NewProject_Error_ProjName"));
		case EUDKProjWizSettingReturnCodes::CODE_InvalidSourceDirectory:	return FString(*LocalizeUnrealEd("NewProject_Error_SourceDir"));
		case EUDKProjWizSettingReturnCodes::CODE_InvalidShortName:			return FString(*LocalizeUnrealEd("NewProject_Error_ShortName"));
		case EUDKProjWizSettingReturnCodes::CODE_CantCreateInstallDir:		return FString(*LocalizeUnrealEd("NewProject_Error_CreateDir"));
		case EUDKProjWizSettingReturnCodes::CODE_FailedFileRead:			return FString(*LocalizeUnrealEd("NewProject_Error_ReadFile"));
		case EUDKProjWizSettingReturnCodes::CODE_FailedFileWrite:			return FString(*LocalizeUnrealEd("NewProject_Error_WriteFile"));
		case EUDKProjWizSettingReturnCodes::CODE_FailedFileCopy:			return FString(*LocalizeUnrealEd("NewProject_Error_CopyFile"));
		case EUDKProjWizSettingReturnCodes::CODE_InvalidManifestFile:		return FString(*LocalizeUnrealEd("NewProject_Error_Manifest"));
		case EUDKProjWizSettingReturnCodes::CODE_FailedToLaunchNewEditor:	return FString(*LocalizeUnrealEd("NewProject_Error_LaunchProj"));
		case EUDKProjWizSettingReturnCodes::CODE_MissingProjectTemplate:	return FString(*LocalizeUnrealEd("NewProject_Error_FindTemplate"));
		case EUDKProjWizSettingReturnCodes::CODE_FailedToBuildScripts:		return FString(*LocalizeUnrealEd("NewProject_Error_BuildScript"));
		case EUDKProjWizSettingReturnCodes::CODE_InsufficientDiskSpace:		return FString(*LocalizeUnrealEd("NewProject_Error_DiskSpace"));


		case EUDKProjWizSettingReturnCodes::CODE_Unknown: return FString(TEXT("Unknown Error"));
		default: return FString(TEXT("Unknown Error"));

	};

}

/**
* Takes an error code, which can store multiple errors, and displays a dialog with the error info.
* Will use the LastErrorCode value if no code is provided.  If there are no valid error codes, no dialog will be shown.
*
* @param Code	The error code used to generate the dialog contents, if none is provided the 
*/
void FNewUDKProjectWizard::ShowErrorDialog( INT Code )
{
	if ( FNewUDKProjectSettings::Get().ProjectWizardMode != EUDKProjWizMode::Interactive )
	{
		return;
	}

	if ( Code == -1 )
	{
		Code = GetLastErrorCode();
	}

	// We won't display a dialog in the case of no errors
	if( Code == EUDKProjWizSettingReturnCodes::CODE_NoError )
	{
		return;
	}

	FString OutString(  TEXT("\nThe following errors were encountered:\r\n") );
	FString ErrorString;

	for( INT Shift = 0; Shift < 32; Shift++ )
	{
		INT BitSet = ( Code >> Shift ) & 1;
		if( BitSet == 1 )
		{
			FString ErrorText = GetErrorString( 1<<Shift );

			if( !ErrorText.IsEmpty() )
			{
				// Add the string for this particular error code.
				OutString += FString::Printf( TEXT("\t%s\r\n"), *ErrorText );
			}
		}
	}

	// Display warning
	WxLongChoiceDialog ErrorDlg(
		*OutString,
		 TEXT(" Project Wizard Error "),
		WxChoiceDialogBase::Choice( AMT_OK, *LocalizeUnrealEd("OK"), WxChoiceDialogBase::DCT_DefaultCancel ) );

	ErrorDlg.ShowModal();
}

/** 
* Used to validate the UDK Project Wizard settings.
*
* @return	Returns an error code in the EUDKProjWizSettingReturnCodes format
*/
INT FNewUDKProjectWizard::ValidateSettings()
{
	INT ErrorCode = EUDKProjWizSettingReturnCodes::CODE_NoError;

	// Evaluate the source project, due to some issues with 'basedon' functionality of ini files, this should be the same as the root folder for the exe that is currently running.
	if( FNewUDKProjectSettings::Get().SourceProject.IsEmpty() && 
		!appDirectoryExists( *FNewUDKProjectSettings::Get().SourceProject )
		)
	{
		ErrorCode |= EUDKProjWizSettingReturnCodes::CODE_InvalidSourceDirectory;
	}

	// Evaluate the target directory setting which represents the file system path to the install directory.
	if( FNewUDKProjectSettings::Get().InstallDirectorySetting.IsEmpty() )
	{
		ErrorCode |= EUDKProjWizSettingReturnCodes::CODE_InvalidTargetDirectory;
	}

	FString Spec = FNewUDKProjectSettings::Get().InstallDirectorySetting * TEXT("*");
	TArray<FString> List;
	appFindFilesInDirectory( List, *Spec, 1, 1 );
	if( appDirectoryExists( *FNewUDKProjectSettings::Get().InstallDirectorySetting ) && !FNewUDKProjectSettings::Get().bOverwriteExisting && List.Num() > 0 )
	{
		// The directory exists and there are files inside but the user has not set the overwrite flag.
		ErrorCode |= EUDKProjWizSettingReturnCodes::CODE_InvalidTargetDirectory;
	}

	// Evaluate the Project name setting which represents the user readable name for the game (ex. Unreal Development Kit, My Custom Game, etc)
	if( FNewUDKProjectSettings::Get().ProjectNameSetting.IsEmpty() )
	{
		ErrorCode |= EUDKProjWizSettingReturnCodes::CODE_InvalidProjectName;
	}

	// Evaluate the project short name setting which represents the abbreviated name for the game (ex. UDK, MCG, etc)
	if( FNewUDKProjectSettings::Get().ShortNameSetting.IsEmpty() )
	{
		ErrorCode |= EUDKProjWizSettingReturnCodes::CODE_InvalidShortName;
	}

	if( !IsValidString( FNewUDKProjectSettings::Get().ShortNameSetting, PROJ_WIZ_INVALID_NAME_CHARACTERS ) )
	{
		// An invalid character was found in the short name string.  Since this string is used for a directory name in the dev folder
		//  later, we restrict the allowed character set.
		ErrorCode |= EUDKProjWizSettingReturnCodes::CODE_InvalidShortName;
	}

	// The short name will be used to generate the custom project script package folder in the Development\src directory.  We don't 
	//  want any conflicts between existing script package names and this string.  So, if the short name is valid at this point and
	//  our source path is valid, we will check the base template and target template directories for script packages that may conflict.
	if( !( ErrorCode & EUDKProjWizSettingReturnCodes::CODE_InvalidShortName )	&&
		!( ErrorCode & EUDKProjWizSettingReturnCodes::CODE_InvalidSourceDirectory ) 
		)
	{

		FProjWizTemplate* BaseTemplate = GetBaseTemplate();
		FProjWizTemplate* SelectedTemplate = GetSelectedTemplate();

		// Check the base template for conflicting script package names
		FString DirectoryToCheck( BaseTemplate->GetFullPath() * FString(TEXT("Development")) * FString(TEXT("Src")) );

		// Make a wild card to look for directories in the current directory
		FString Wildcard = DirectoryToCheck * TEXT("*.*");
		TArray<FString> SubDirs; 
		GFileManager->FindFiles(SubDirs, *Wildcard, FALSE, TRUE);

		for (INT SubDirIndex = 0; SubDirIndex < SubDirs.Num(); SubDirIndex++)
		{
			FString CurSubDir = SubDirs(SubDirIndex);
			if( CurSubDir.ToLower() == FNewUDKProjectSettings::Get().ShortNameSetting.ToLower() )
			{
				ErrorCode |= EUDKProjWizSettingReturnCodes::CODE_InvalidShortName;
			}
		}

		// Check the selected template for conflicting script package names
		DirectoryToCheck = SelectedTemplate->GetFullPath() * FString(TEXT("Development")) * FString(TEXT("Src"));
		Wildcard = DirectoryToCheck * TEXT("*.*");
		SubDirs.Empty();
		GFileManager->FindFiles(SubDirs, *Wildcard, FALSE, TRUE);
		for (INT SubDirIndex = 0; SubDirIndex < SubDirs.Num(); SubDirIndex++)
		{
			FString CurSubDir = SubDirs(SubDirIndex);
			if( CurSubDir.ToLower() == FNewUDKProjectSettings::Get().ShortNameSetting.ToLower() )
			{
				ErrorCode |= EUDKProjWizSettingReturnCodes::CODE_InvalidShortName;
			}
		}
	}

	return ErrorCode;
}

/** Launches the project wizard UI */
UBOOL FNewUDKProjectWizard::RunUDKProjectWizard()
{

	LastErrorCode = EUDKProjWizSettingReturnCodes::CODE_NoError;

#if WITH_MANAGED_CODE
	// Fill in the template list.
	PopulateTemplateList( FNewUDKProjectSettings::Get().SourceProject );

	// Make sure we have at least the base template.  If it is missing we will prompt and exit.
	if( !GetBaseTemplate() )
	{
		LastErrorCode = EUDKProjWizSettingReturnCodes::CODE_MissingProjectTemplate;
		ShowErrorDialog();
		return false;
	}

	// We require a manifest.xml file to know which files were part of the original UDK install.  We use this file to drive file copy
	//  into the new project folder.  This file will be part of every UDK install but will need to be added when running in debug.
	const FString ManifestRelativePath( FNewUDKProjectSettings::Get().SourceProject * TEXT("Binaries\\InstallData\\Manifest.xml") );
	const UBOOL bManifestFileExists = ( GFileManager->FileSize( * ManifestRelativePath ) > 0 );
	if( bManifestFileExists == FALSE )
	{
		LastErrorCode = EUDKProjWizSettingReturnCodes::CODE_InvalidManifestFile;
		ShowErrorDialog();
		return false;
	}

	UpdateTemplateTargetList();

	UpdateUserSettingsList();

	UBOOL DoProjectGen = FALSE;

	if( FNewUDKProjectSettings::Get().ProjectWizardMode == EUDKProjWizMode::NonInteractive )
	{
		DoProjectGen = ( ValidateSettings() == EUDKProjWizSettingReturnCodes::CODE_NoError );
	}
	else if (FNewUDKProjectSettings::Get().ProjectWizardMode == EUDKProjWizMode::Interactive )
	{
		UBOOL bTryAgain;
		do 
		{
			bTryAgain = FALSE;

			// Show the project wizard screen - will return FALSE if the user cancels
			if( FNewProjectScreen::DisplayNewProjectScreen( this ) )
			{	
				// The user dismissed the settings screen with the Finish button, here we validate the user provided settings
				INT ReturnCode = ValidateSettings();

				// Set the last error code we encountered.
				LastErrorCode = ReturnCode;

				if( ReturnCode == EUDKProjWizSettingReturnCodes::CODE_NoError )
				{
					DoProjectGen = TRUE;
					bTryAgain = FALSE;
				}
				else
				{
					// At this point, there is no error we can't recover from so we prompt and continue the loop which will bring up the proj wiz settings screen again( previous settings are preserved )
					ShowErrorDialog( ReturnCode );
					DoProjectGen = FALSE;
					bTryAgain = TRUE;
				}
			}
			else
			{
				// The user hit cancel so we won't be doing the project generation and we don't need to continue getting settings from the user.
				DoProjectGen = FALSE;
				bTryAgain = FALSE;
			}

		} while( bTryAgain );
	}


	if( DoProjectGen )
	{
		LastErrorCode = Exec();
		ShowErrorDialog( LastErrorCode );
	}


#else
	// We don't currently support project creation without managed code.
	GWarn->Logf(NAME_Warning, TEXT("Project Wizard not available in this mode."));
#endif

	return (UBOOL)( LastErrorCode == EUDKProjWizSettingReturnCodes::CODE_NoError );
}

/**
* Will return the list of items that will appear in the UI 
*/
TArray<FTemplateTargetListInfo>* FNewUDKProjectWizard::GetTemplateTargetList()
{
	return &UITemplateItems;
}

/** 
* Will update the list of items that will appear in the UI 
*/
void FNewUDKProjectWizard::UpdateTemplateTargetList()
{
	// This should only be called from interactive mode.
	check( FNewUDKProjectSettings::Get().ProjectWizardMode == EUDKProjWizMode::Interactive );
	UITemplateItems.Empty();

	// Iterate through our local structure(map) and pull out all the juicy templates.  Currently this includes
	//  everything except for BaseTemplate which is always selected(applied).
	for( TMap<FString, FProjWizTemplate>::TIterator It( TemplateItems ); It; ++It )
	{
		FProjWizTemplate* CurrentTemplate = &It.Value();

		if( CurrentTemplate->GetName() != TEXT("BaseTemplate") )
		{
			new(UITemplateItems) FTemplateTargetListInfo(CurrentTemplate);
		}
	}

	if( UITemplateItems.Num() > 0 )
	{
		// Just set the first one as selected.
		UITemplateItems(0).bIsSelected = TRUE;
	}
}

/** 
* Will return the list of user setting items that will appear in the UI 
*/
TArray<FUserProjectSettingsListInfo>* FNewUDKProjectWizard::GetUserSettingsList()
{
	return &UIUserSettingItems;
}

/** 
* Will update the list of user settings that will appear in the UI 
*/
void FNewUDKProjectWizard::UpdateUserSettingsList()
{
	// This should only be called from interactive mode.
	check( FNewUDKProjectSettings::Get().ProjectWizardMode == EUDKProjWizMode::Interactive );
	UIUserSettingItems.Empty();

	if( UserSettingItems.Num() == 0 )
	{
		// Fill in the advanced settings
		TArray<FUserProjSetting> AdvancedTemplateSettings;
		FProjWizTemplate* BaseTemplate = GetBaseTemplate();
		BaseTemplate->GetTemplateAdvancedSettings( AdvancedTemplateSettings );
		for( INT SettingIndex = 0; SettingIndex < AdvancedTemplateSettings.Num(); ++SettingIndex )
		{
			UserSettingItems.AddItem( AdvancedTemplateSettings( SettingIndex ) );
		}
		AdvancedTemplateSettings.Empty();

		FProjWizTemplate* SelectedTemplate = GetSelectedTemplate();
		SelectedTemplate->GetTemplateAdvancedSettings( AdvancedTemplateSettings );
		for( INT SettingIndex = 0; SettingIndex < AdvancedTemplateSettings.Num(); ++SettingIndex )
		{
			UBOOL bAddSetting = TRUE;
			for( INT i = 0; i < UserSettingItems.Num(); ++i )
			{
				// If the setting already exists, we will overwrite the value and continue.
				if( UserSettingItems( i ).SettingKey == AdvancedTemplateSettings( SettingIndex ).SettingKey )
				{
					UserSettingItems( i ).SettingValue = AdvancedTemplateSettings( SettingIndex ).SettingValue;
					bAddSetting = FALSE;
					break;
				}
			}

			if( bAddSetting )
			{
				UserSettingItems.AddItem( AdvancedTemplateSettings( SettingIndex ) );
			}
		}
	}

	// Iterate through all the settings that a user could set, and add the items to the list that will drive the UI
	for(INT i = 0; i < UserSettingItems.Num(); ++i )
	{
		new(UIUserSettingItems) FUserProjectSettingsListInfo( &UserSettingItems(i) );
	}
}

/** Will return the selected target template. */
FProjWizTemplate* FNewUDKProjectWizard::GetSelectedTemplate()
{
	FProjWizTemplate* RetVal = NULL;

	if( FNewUDKProjectSettings::Get().ProjectWizardMode == EUDKProjWizMode::Interactive )
	{
		// Loop through our list of templates and see which one the user has selected
		for( INT targetIndex = 0; targetIndex < UITemplateItems.Num(); targetIndex++ )
		{
			if(UITemplateItems(targetIndex).bIsSelected)
			{
				RetVal = UITemplateItems(targetIndex).TemplateData;
				break;
			}
		}
	}
	else
	{
		// In non-interactive mode we just return the first template.
		FString TemplateDir = TEXT("UDKGame\\ProjectTemplates\\Template1");
		RetVal = TemplateItems.Find( FNewUDKProjectSettings::Get().SourceProject * TemplateDir );
	}

	// Should always return a valid value.
	check(RetVal);

	return RetVal;
}

/** Will return the base template. */
FProjWizTemplate* FNewUDKProjectWizard::GetBaseTemplate()
{
	FString TemplateBaseDir = TEXT("UDKGame\\ProjectTemplates\\BaseTemplate");
	FString TemplateDir = FNewUDKProjectSettings::Get().SourceProject * TemplateBaseDir;
	FProjWizTemplate* BaseTemplate = TemplateItems.Find( TemplateDir );

	return BaseTemplate;
}

/** Recursivly steps through manifest xml nodes and pulls out the file info. */
void RecursiveGenFileList( TiXmlElement* Node, FString& ParentPath, TMap<FFilename, INT>& OutFiles )
{
	check(Node);
	FString FolderName = Node->Attribute( "FolderName" );
	FString Path = ParentPath * FolderName;

	TiXmlElement* FileListNode = Node->FirstChildElement( "Files" );
	for( TiXmlElement* FilesPropertiesNode = FileListNode->FirstChildElement( "FileProperties"); FilesPropertiesNode != NULL; FilesPropertiesNode = FilesPropertiesNode->NextSiblingElement() )
	{
		FString FileName = FilesPropertiesNode->Attribute( "FileName" );
		FString FilePath =  Path * FileName;

		FString FileSizeStr = FilesPropertiesNode->Attribute( "Size" );
		OutFiles.Set( FilePath, appAtoi( *FileSizeStr ) );
	}
	
	TiXmlElement* FoldersListNode = Node->FirstChildElement( "Folders" );
	for( TiXmlElement * FolderPropertiesNode = FoldersListNode->FirstChildElement( "FolderProperties" ); FolderPropertiesNode != NULL; FolderPropertiesNode = FolderPropertiesNode->NextSiblingElement() )
	{
		RecursiveGenFileList( FolderPropertiesNode, Path, OutFiles ); 
	}
}

/**
 * Removes files entries from a map based on a search string.
 *	
 * @param	Entries					Structure to remove file entries from.
 * @param	SearchStr				Search string used to find entries that need to be removed.
 * @param	bShouldFindFiles		If files should be removed.
 * @param	bShouldFindDirectories	If directories should be removed.
 *
 * @return							The number of entries that were removed.
 */
INT RemoveFileEntries(TMap<FFilename, FProjWizFileInfo>& Entries, const FString& SearchStr, UBOOL bShouldFindFiles, UBOOL bShouldFindDirectories)
{
	// Will store the number of elements removed from the list.
	INT RetVal = 0;

	// cache a filename version of the search string
	FFilename FullSearchStr( SearchStr );

	// Find the path info for the search string
	FFilename BasePath = FullSearchStr.GetPath();
	const INT BasePathLen = BasePath.Len();

	for( TMap<FFilename, FProjWizFileInfo>::TIterator It( Entries ); It; ++It )
	{
		const FFilename& Filename = It.Key();

		if(bShouldFindDirectories)
		{
			if( appStrnicmp( *Filename, *BasePath, BasePathLen ) == 0 )
			{
				It.RemoveCurrent();
				RetVal++;
				continue;
			}
		}
		
		if(bShouldFindFiles)
		{
			if( appStricmp( *Filename, *FullSearchStr ) == 0 ) 
			{
				It.RemoveCurrent();
				RetVal++;
				continue;
			}
		}
	}
	return RetVal;
}

/** 
 * Loads a manifest file, parses the contents and returns a list of files that are found inside
 *
 * @param	ManifestFile	Manifest file path.
 * @param	OutFiles		Structure to contain the resulting file list.
 *
 * @return	TRUE if the operation was successful, FALSE otherwise. 
 */
UBOOL GetManifestFiles( const FFilename& ManifestFile, TMap<FFilename, INT>& OutFiles  )
{
	check(ManifestFile.Len() > 0);
	check(OutFiles.Num() == 0);

	TiXmlDocument InstallManifest( TCHAR_TO_ANSI(*ManifestFile) );

	if( InstallManifest.LoadFile() )
	{
		TiXmlElement* RootNode = InstallManifest.FirstChildElement();
		if( RootNode != NULL )
		{
			FString Path;
			RecursiveGenFileList(RootNode, Path, OutFiles);
		}
	}
	else
	{
		GWarn->Logf(NAME_Warning, TEXT("Project wizard failed to find required file: %s"), TEXT("Manifest.xml") );
		return FALSE;
	}

	
	return TRUE;
	
}

/** Generates the new UDK project based on the current settings 
*
* @return A return code representing the error encountered in the EUDKProjWizSettingReturnCodes format
*/
INT FNewUDKProjectWizard::Exec()
{
	check( LastErrorCode == EUDKProjWizSettingReturnCodes::CODE_NoError );

	// Create the destination directory if it does not exist
	if( !appDirectoryExists( *FNewUDKProjectSettings::Get().InstallDirectorySetting ) )
	{
		if( !GFileManager->MakeDirectory( *FNewUDKProjectSettings::Get().InstallDirectorySetting ) )
		{
			LastErrorCode |= EUDKProjWizSettingReturnCodes::CODE_CantCreateInstallDir;
			GWarn->Logf(NAME_Warning, TEXT("Failed to create the install folder: %s."), *FNewUDKProjectSettings::Get().InstallDirectorySetting );
			return LastErrorCode;
		}
	}
	
	const FString DestProjectPath = FNewUDKProjectSettings::Get().InstallDirectorySetting;
	
	GWarn->BeginSlowTask( TEXT("Copying new project files"), TRUE);

	FProjWizTemplate* SelectedTemplate = GetSelectedTemplate();
	//Generate a list of destination / src files.  This will be manipulated a few times before it is ready to be used.
	TMap< FFilename, FProjWizFileInfo > FilesToProcess;

	if ( !GetFilesToProcessForTemplate( FilesToProcess, SelectedTemplate ) )
	{
		return LastErrorCode;
	}

	INT NumFilesToCopy = FilesToProcess.Num();
	INT ProcessedFileCount = 0;

	const FString UDKConfigFolderEnding = FString::Printf(TEXT("%s%s%s"), TEXT("UDKGame"), PATH_SEPARATOR, TEXT("Config"));
	const FString TemplateScriptFolderEnding = FString::Printf(TEXT("%s%s%s"), TEXT("TemplateGame"), PATH_SEPARATOR, TEXT("Classes"));

#if _WINDOWS
	QWORD FreeBytesToCaller = 0;
	QWORD TotalBytes = 0;
	QWORD FreeBytes = 0;
	FFileManagerWindows* FileManagerWindows = static_cast<FFileManagerWindows*>(GFileManager);
	if( FileManagerWindows->GetDiskFreeSpace(*FNewUDKProjectSettings::Get().InstallDirectorySetting, FreeBytesToCaller, TotalBytes, FreeBytes))
	{
		// If we are able to retrieve disk space info, we will check to make sure there is enough space for the new project prior to copy.
		QWORD ProjectedProjectBytes = 0;
		for( TMap<FFilename, FProjWizFileInfo>::TConstIterator It( FilesToProcess ); It; ++It )
		{
			const INT& FileSize = It.Value().FileSize;
			if(FileSize == -1)
			{
				// We found a file that we don't have size info for, this can happen for files that were not in the manifest.
				const FFilename& SourcePath = It.Value().SourcePath;
				ProjectedProjectBytes += GFileManager->FileSize(*SourcePath);
			}
			else
			{
				ProjectedProjectBytes += FileSize;
			}
		}

		// Add some padding just to be on the safe side.
		ProjectedProjectBytes += 10 * 1024;

		if( ProjectedProjectBytes > FreeBytesToCaller )
		{
			LastErrorCode |= EUDKProjWizSettingReturnCodes::CODE_InsufficientDiskSpace;
			GWarn->Logf(NAME_Warning, TEXT("Insufficient disk space to create new project."));
		}
	}
#endif // _WINDOWS

	if(LastErrorCode == EUDKProjWizSettingReturnCodes::CODE_NoError)
	{
		// We now have a set of files that needs to be copied from the source project to the destination project.  We do that copy here.  Ini and script in the template directories get special treatment.
		for( TMap<FFilename, FProjWizFileInfo>::TConstIterator It( FilesToProcess ); It; ProcessedFileCount++, ++It )
		{
			const FFilename& DestPath = It.Key();
			const FFilename& SourcePath = It.Value().SourcePath;

			GWarn->StatusUpdatef( ProcessedFileCount, NumFilesToCopy, TEXT("Copying new project files") );

			if( SourcePath.GetExtension() == TEXT("ini") && appStrstr( *SourcePath.GetPath(),  *UDKConfigFolderEnding ) != NULL )
			{
			
				// Handle ini files in a special way.  Here we read them in, change all instances of the template name to the user provided game name.
				FString Data;
				if( appLoadFileToString( Data, *SourcePath ) )
				{
					// Replace the game name
					Data = Data.Replace(TEXT("TEMPLATE_FULL_NAME"), *FNewUDKProjectSettings::Get().ProjectNameSetting);
					// Replace the short name
					Data = Data.Replace(TEXT("TEMPLATE_SHORT_NAME"), *FNewUDKProjectSettings::Get().ShortNameSetting);
					if ( !appSaveStringToFile(Data, *DestPath) )
					{
						LastErrorCode |= EUDKProjWizSettingReturnCodes::CODE_FailedFileWrite;
						GWarn->Logf(NAME_Warning, TEXT("Failed to write file %s."), *SourcePath);
					}
				}
				else
				{
					LastErrorCode |= EUDKProjWizSettingReturnCodes::CODE_FailedFileRead;
					GWarn->Logf(NAME_Warning, TEXT("Failed to load file %s."), *SourcePath);
				}
			}
			else if( SourcePath.GetExtension() == TEXT("uc") && appStrstr( *SourcePath.GetPath(),  *TemplateScriptFolderEnding ) != NULL )
			{
				// Handle script files.  We want to change the folder name of the template script package.  Also want to change the file name of template files.  Finally,
				//  we want to change the contents of the template files.
				FFilename ScriptDestPath = DestPath;
				FString TemplateGameStr( TEXT("TemplateGame") );
				TemplateGameStr+= PATH_SEPARATOR;
				// If the script source folder contains TemplateGame\ we know we need to process it a special way.
				if( SourcePath.InStr( TemplateGameStr ) != INDEX_NONE )
				{
					ScriptDestPath = ScriptDestPath.Replace( *TemplateGameStr, *(FNewUDKProjectSettings::Get().ShortNameSetting + TEXT("Game") + PATH_SEPARATOR));
					FString DestCleanFileName = ScriptDestPath.GetCleanFilename();
					if(DestCleanFileName.InStr(TEXT("Template")) != INDEX_NONE)
					{
						FString NewDestCleanFileName = DestCleanFileName.Replace( TEXT("Template"), *FNewUDKProjectSettings::Get().ShortNameSetting );
						ScriptDestPath = ScriptDestPath.Replace( *DestCleanFileName, *NewDestCleanFileName );
					}
			
					// Read in the file, modify and write out.
					FString Data;
					if( appLoadFileToString( Data, *SourcePath ) )
					{
						// Replace the short name
						Data = Data.Replace( TEXT("TEMPLATE_SHORT_NAME"), *FNewUDKProjectSettings::Get().ShortNameSetting );
						if( !appSaveStringToFile(Data, *ScriptDestPath) )
						{
							LastErrorCode |= EUDKProjWizSettingReturnCodes::CODE_FailedFileWrite;
							GWarn->Logf(NAME_Warning, TEXT("Failed to write file %s."), *ScriptDestPath);
						}
					}
					else
					{
						LastErrorCode |= EUDKProjWizSettingReturnCodes::CODE_FailedFileRead;
						GWarn->Logf(NAME_Warning, TEXT("Failed to load file %s."), *SourcePath);
					}
				}
				else
				{
					// If we don't need special handling, just copy it.
					const DWORD CopyResult = GFileManager->Copy( *ScriptDestPath, *SourcePath, TRUE );
					if( CopyResult != COPY_OK )
					{
						LastErrorCode |= EUDKProjWizSettingReturnCodes::CODE_FailedFileCopy;
						GWarn->Logf(NAME_Warning, TEXT("Failed to copy file %s to %s with copy result %u!"), *SourcePath, *ScriptDestPath, CopyResult);
					}
				}
			}
			else
			{
				// Just copy any other file.
				const DWORD CopyResult = GFileManager->Copy(*DestPath, *SourcePath, TRUE);
				if( CopyResult != COPY_OK )
				{
					LastErrorCode |= EUDKProjWizSettingReturnCodes::CODE_FailedFileCopy;
					GWarn->Logf(NAME_Warning, TEXT("Failed to copy file %s to %s with copy result %u!"), *SourcePath, *DestPath, CopyResult);
				}
			}
		}
	}


	if(LastErrorCode == EUDKProjWizSettingReturnCodes::CODE_NoError)
	{
		// Run game to generate the proper ini files
		TriggerGenerateINI();
	}

	UBOOL bScriptsBuilt = FALSE;
	if(LastErrorCode == EUDKProjWizSettingReturnCodes::CODE_NoError)
	{
		// Build the scripts for the new project
		GWarn->StatusUpdatef( ProcessedFileCount, NumFilesToCopy, TEXT("Building new project scripts") );
		bScriptsBuilt = TriggerScriptBuild();
	}
	
	if(LastErrorCode == EUDKProjWizSettingReturnCodes::CODE_NoError)
	{
		GWarn->StatusUpdatef( ProcessedFileCount, NumFilesToCopy, TEXT("Creating shortcuts") );
		CreateShortcuts();
	}

	GWarn->EndSlowTask();

	if(LastErrorCode == EUDKProjWizSettingReturnCodes::CODE_NoError && bScriptsBuilt)
	{
		if( FNewUDKProjectSettings::Get().ProjectWizardMode == EUDKProjWizMode::Interactive )
		{
#if WITH_MANAGED_CODE
			// Show the project wizard successful install info screen - will return FALSE if the user cancels
			if( FNewProjectScreen::DisplayNewProjectScreen( this, TRUE ) )
			{
				TriggerEditorSwitch();
			}
#endif // WITH_MANAGED_CODE
		}
		else
		{
			FString PromptMessage(TEXT("Project installed successfully."));
			PromptMessage += TEXT("\n\n  Project Name: ");
			PromptMessage += FNewUDKProjectSettings::Get().ProjectNameSetting + TEXT("\n  Project Path: ") + DestProjectPath;
			GWarn->Logf(NAME_Log, *PromptMessage);
		}
	}

	return LastErrorCode;
}


/**
 * Figures out which files will be used for a given template.  Will set the LastErrorCode if a problem was encountered.
 *
 * @param	OutFilesToProcess	Structure used to return info about the files that we will need to process for a given template.
 * @param	InTemplate			The project template to use.
 *
 * @return						TRUE if the operation completed successfully, FALSE otherwise(Will set LastErrorCode).
 */
UBOOL FNewUDKProjectWizard::GetFilesToProcessForTemplate( TMap< FFilename, FProjWizFileInfo >& OutFilesToProcess, FProjWizTemplate* InTemplate )
{
	check( InTemplate );
	OutFilesToProcess.Empty();

	const FString SourceProjectPath = FNewUDKProjectSettings::Get().SourceProject;
	const FString DestProjectPath = FNewUDKProjectSettings::Get().InstallDirectorySetting;
	const FString ManifestRelativeFilePath( TEXT("Binaries\\InstallData\\Manifest.xml") );
	
	FProjWizTemplate* BaseTemplate = GetBaseTemplate();

	// Get files from manifest
	TMap< FFilename, INT > ManifestFiles;
	if( !GetManifestFiles( SourceProjectPath * ManifestRelativeFilePath, ManifestFiles ) )
	{
		GWarn->Logf(NAME_Warning, TEXT("Failed to find install manifest: %s."), *(SourceProjectPath * ManifestRelativeFilePath) );
		LastErrorCode |= EUDKProjWizSettingReturnCodes::CODE_InvalidManifestFile;
		return FALSE;
	}

	// Manually add any files that might not appear in the manifest file.  ex. The manifest file itself and the contents of the BaseTemplate 
	ManifestFiles.Set( FString( TEXT(".\\") ) + ManifestRelativeFilePath, GFileManager->FileSize( *( SourceProjectPath * ManifestRelativeFilePath ) ));

	// Manually add the contents of the BaseTemplate folder.  We include this folder since the UDK installer copies a pristine data set into there.
	//  This pristine data set represents a backup copy of key UDK files that we are reasonably sure the user won't modify.  This
	//  set of files does not appear in the manifest.
	TArray<FString> BaseTemplateFiles;
	BaseTemplate->GetTemplateFiles( BaseTemplateFiles );
	for (INT FileIndex = 0; FileIndex < BaseTemplateFiles.Num(); FileIndex++)
	{
		const FFilename FileSourcePath = BaseTemplateFiles(FileIndex);
		const FFilename FileRelativePath = FileSourcePath.Replace(*SourceProjectPath, TEXT("."));
		ManifestFiles.Set( FileRelativePath, GFileManager->FileSize( *FileSourcePath ));
	}

	// Add all files we find in the UDK install manifest to our process list.
	for( TMap<FFilename, INT>::TConstIterator It( ManifestFiles ); It; ++It )
	{
		const FFilename FileRelativePath = It.Key().Right( It.Key().Len() - 2 );
		const FFilename FileSourcePath = SourceProjectPath * FileRelativePath;
		const FFilename FileDestPath = DestProjectPath * FileRelativePath;

		FProjWizFileInfo Val( FileSourcePath, It.Value() );
		OutFilesToProcess.Set( FileDestPath, Val );
	}

	// Done with the manifest file list.
	ManifestFiles.Empty();

	// Get a list of items to exclude from the settings and remove those entries from the DB.  Format
	//	ex: Directory=.\\Engine    <-- this will exclude the entire engine directory
	//	ex: Directory=.\\Binaries\\Win32  <-- This will exclude the entire binaries\win32 directory
	//	ex: File= .\\Binaries\\InstallInfo.xml  <-- Removes specific file
	FConfigFile ConfigFile;
	FString ConfigFilePath = InTemplate->GetFullPath() * TEXT("TemplateInfo.ini");
	INT ExcludeCount = 0;

	// Read in the project template info ini file and exclude any items we find in the ProjectWizardExcludes section
	LoadAnIniFile( *ConfigFilePath, ConfigFile, FALSE );
	FConfigSection* ExcludeFiles = ConfigFile.Find( TEXT("UnrealEd.ProjectWizardExcludes") );
	if( ExcludeFiles )
	{
		for (FConfigSectionMap::TIterator It(*ExcludeFiles); It; ++It)
		{
			const FName ExcludeKey = It.Key();
			FString ExcludeValue = It.Value();
			if ( ExcludeValue.StartsWith( TEXT("./") ) || ExcludeValue.StartsWith( TEXT(".\\") ) )
			{
				ExcludeValue = ExcludeValue.Right( ExcludeValue.Len() - 2 );
			}
			if( ExcludeKey == TEXT("File") || ExcludeKey == TEXT("+File") )
			{
				ExcludeCount += RemoveFileEntries( OutFilesToProcess, *( DestProjectPath * ExcludeValue ), TRUE, FALSE );
			}
			else if( ExcludeKey == TEXT("Directory") || ExcludeKey == TEXT("+Directory"))
			{
				ExcludeCount += RemoveFileEntries( OutFilesToProcess,*( DestProjectPath * ExcludeValue ), FALSE, TRUE);
			}
		}
	}

	if( ExcludeCount > 0 )
	{
		OutFilesToProcess.Compact();
	}

	// Add/overwrite entries in our process list based on what is in the BaseTemplate folder.  This folder represents a pristine dataset
	//  and we assume that it was not modified by the user after UDK was installed.
	for (INT FileIndex = 0; FileIndex < BaseTemplateFiles.Num(); FileIndex++)
	{
		const FFilename FileSourcePath = BaseTemplateFiles( FileIndex );

		// Each Template file maps to a specific destination project folder, this variable stores that computed relative path info.
		const FFilename FileRelativePath = FileSourcePath.Right( FileSourcePath.Len() - (BaseTemplate->GetFullPath().Len() + 1) );
		const FFilename FileDestPath = DestProjectPath * FileRelativePath;

		INT FileSize = GFileManager->FileSize( *FileSourcePath );
		FProjWizFileInfo Val( FileSourcePath, FileSize );

		// If the FileDestPath key already exists in the map we will overwrite the source info
		OutFilesToProcess.Set( FileDestPath, Val );
	}

	// Add/overwrite entries in our process list based on what is in the template folder the user selected.  This folder represents data specific to the
	//  particular template.  Note, we also assume this folder has not been modified by the user since UDK install.
	TArray<FString> TemplateFiles;
	InTemplate->GetTemplateFiles( TemplateFiles );
	for (INT FileIndex = 0; FileIndex < TemplateFiles.Num(); FileIndex++)
	{
		const FFilename FileSourcePath = TemplateFiles( FileIndex );

		// Each Template file maps to a specific destination project folder, this variable stores that computed relative path info.
		const FFilename FileRelativePath = FileSourcePath.Right( FileSourcePath.Len() - (InTemplate->GetFullPath().Len() + 1) );
		const FFilename FileDestPath = DestProjectPath * FileRelativePath;

		INT FileSize = GFileManager->FileSize( *FileSourcePath );
		FProjWizFileInfo Val( FileSourcePath, FileSize );

		// If the FileDestPath key already exists in the map we will overwrite the source info
		OutFilesToProcess.Set( FileDestPath, Val );
	}
	TemplateFiles.Empty();

	return TRUE;

}

/**
 * Creates a directory path that belongs to the set "InDirectoryPath##" where ## is a 2-digit number
 * in [0-99] and no directory of that name currently exists.  The return value is the index of first
 * empty filename, or -1 if none could be found.
 * 
 *
 * @param	InDirectoryPath		Directory path.
 * @param	OutDirectoryPath	Used to return an available directory path (untouched on fail).
 * @param	StartVal			Optional parameter that can be used to hint beginning of index search.
 * @return						The index of the available directory path, or -1 if no free path with index [StartVal, 99] was found.
 */
INT FNewUDKProjectWizard::FindAvailableDirectoryPath( const FString& InDirectoryPath, FString& OutFilename, INT StartVal )
{
	check( !InDirectoryPath.IsEmpty() );

	FString FullPath;

	// Iterate over indices, searching for a directory that doesn't exist.
	for ( INT i = StartVal; i < 100 ; ++i )
	{
		FullPath = FString::Printf( TEXT("%s%d"), *InDirectoryPath, i );

		if ( !appDirectoryExists( *FullPath ) )
		{
			// The directory doesn't exist; output success.
			OutFilename = FullPath;
			return  i;
		}
	}

	// Can't find an available directory path with index in [StartVal, 99].
	return -1;
}

FString FNewUDKProjectWizard::GetNewProjectExePath()
{
	// Figure out the proper exe to switch to
	FString ProjectName(GGameName);
	FString Game = ProjectName + TEXT( "Game" );
	FString Separator = TEXT( "-" );
	FString Extension = TEXT( ".exe" );

	FString BinariesFolder = FNewUDKProjectSettings::Get().InstallDirectorySetting * FString( TEXT( "Binaries" ) );

	// If we are running in 64 bit, launch the 64 bit process
#if _WIN64
	FString PlatformConfig = TEXT( "Win64" );
#else
	FString PlatformConfig = TEXT( "Win32" );
#endif

	FString PendingProjectExe;

#if _DEBUG
	FString Config = TEXT( "Debug" );
	PendingProjectExe = BinariesFolder * PlatformConfig * Game + Separator + PlatformConfig + Separator + Config + Extension;
#elif SHIPPING_PC_GAME
	Game = TEXT( "UDK" );
	PendingProjectExe = BinariesFolder * PlatformConfig * Game + Extension;
#else
	PendingProjectExe = BinariesFolder * PlatformConfig * Game + Extension;
#endif

	return PendingProjectExe;
}

UBOOL FNewUDKProjectWizard::TriggerScriptBuild()
{
	FString NewProjectExe = GetNewProjectExePath();

	const UBOOL bNewProjectExeExists = ( GFileManager->FileSize( *NewProjectExe) > 0 );
	if( bNewProjectExeExists == FALSE )
	{
		GWarn->Logf(NAME_Warning, TEXT("Could not find new project main executable: %s"), *NewProjectExe);
		LastErrorCode |= EUDKProjWizSettingReturnCodes::CODE_FailedToBuildScripts;
		return false;
	}

	FString LaunchEditorCmdLine(TEXT("make -full -nopauseonsuccess -silent"));

	// Launch the target executable with a cmdline that will build scripts
	void* Handle = appCreateProc( *NewProjectExe, *LaunchEditorCmdLine );
	if( !Handle )
	{
		// We were not able to spawn the new project exe. Skip shutting down the editor if this happens
		GWarn->Logf(NAME_Warning, TEXT("Failed to build new project scripts because we could not launch the new project executable: %s"), *NewProjectExe);
		LastErrorCode |= EUDKProjWizSettingReturnCodes::CODE_FailedToBuildScripts;
		return false;
	}

	INT ReturnCode;
	// Wait for the process to finish and get return code
	while (!appGetProcReturnCode(Handle, &ReturnCode))
	{
		appSleep(0);
	}

	return (ReturnCode == 0);

}

void FNewUDKProjectWizard::TriggerEditorSwitch()
{
	FString NewProjectExe = GetNewProjectExePath();

	const UBOOL bNewProjectExeExists = ( GFileManager->FileSize( *NewProjectExe) > 0 );
	if( bNewProjectExeExists == FALSE )
	{
		GWarn->Logf(NAME_Warning, TEXT("Could not find new project main executable: %s"), *NewProjectExe);
		LastErrorCode |= EUDKProjWizSettingReturnCodes::CODE_FailedToLaunchNewEditor;
		return;
	}

	FString LaunchEditorCmdLine = FString::Printf( TEXT( "editor %s" ), appCmdLine() );

	// Try to launch the new editor with the same cmdline that the current editor was launched with.
	void* Handle = appCreateProc( *NewProjectExe, *LaunchEditorCmdLine );
	if( !Handle )
	{
		// We were not able to spawn the new project exe. Skip shutting down the editor if this happens
		GWarn->Logf(NAME_Warning, TEXT("Failed to launch the new project editor: %s"), *NewProjectExe);
		LastErrorCode |= EUDKProjWizSettingReturnCodes::CODE_FailedToLaunchNewEditor;
		return;
	}

	// Close the current editor
	GApp->EditorFrame->Close();
	
}

void FNewUDKProjectWizard::TriggerGenerateINI()
{
	
	FString NewProjectExe = GetNewProjectExePath();
	const UBOOL bNewProjectExeExists = ( GFileManager->FileSize( *NewProjectExe) > 0 );
	if( bNewProjectExeExists == FALSE )
	{
		GWarn->Logf(NAME_Warning, TEXT("Could not find new project main executable: %s"), *NewProjectExe);
		LastErrorCode |= EUDKProjWizSettingReturnCodes::CODE_FailedToLaunchNewEditor;
		return;
	}
	
	// Prepare the advanced setting commandline which needs to follow this form: // -ss:name1=val1,name2=val2
	FString AdvancedSettingsCmdLine = UserSettingItems.Num() > 0 ? TEXT("-ss:") : TEXT("");
	for( INT SettingIndex = 0; SettingIndex < UserSettingItems.Num(); ++SettingIndex )
	{
		const FUserProjSetting& UserSetting = UserSettingItems(SettingIndex);
		AdvancedSettingsCmdLine+= FString::Printf(TEXT("%s=%s"), *UserSetting.SettingKey, *UserSetting.SettingValue);
		if(SettingIndex < UserSettingItems.Num() - 1 )
		{
			AdvancedSettingsCmdLine+= TEXT(",");
		}
	}

	FString CmdLine = FString::Printf( TEXT( "-firstinstall -LanguageForCooking=%s" ), *appGetLanguageExt() );
	if( !AdvancedSettingsCmdLine.IsEmpty() )
	{
		CmdLine = CmdLine + TEXT(" ") + AdvancedSettingsCmdLine;
	}

	void* Handle = appCreateProc( *NewProjectExe, *CmdLine );
	if( !Handle )
	{
		// We were not able to spawn the new project exe.
		// Its likely that the exe doesnt exist.
		warnf( TEXT("Failed to launch project to generate ini files: %s"), *NewProjectExe );
		LastErrorCode |= EUDKProjWizSettingReturnCodes::CODE_FailedFileWrite;
		return;
	}
	
	// Make sure the process exits before we return from the function
	INT ReturnCode;
	while (!appGetProcReturnCode(Handle, &ReturnCode))
	{
		appSleep(0);
	}

}

void FNewUDKProjectWizard::CreateShortcuts()
{
#if UDK && !CONSOLE && _WINDOWS
	// Current Directory Storage
	FString CurrentDirectory = GFileManager->GetCurrentDirectory() + TEXT("..\\");
	// Setup Executable String
	FString ShortcutExecutableString = CurrentDirectory + TEXT("UnSetup.exe");
	// Setup Parameters for shortcut creation
	FString ShortcutCommandParams = TEXT("/MakeShortcuts");

	SHELLEXECUTEINFO Info = { sizeof(SHELLEXECUTEINFO), 0 };

	Info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_UNICODE;
	Info.lpVerb = TEXT("runas");
	Info.lpFile = *ShortcutExecutableString;
	Info.lpParameters = *ShortcutCommandParams;
	Info.lpDirectory = *CurrentDirectory;

	Info.nShow = SW_HIDE;

	ShellExecuteEx(&Info);

	if (!Info.hProcess)
	{
		debugf(TEXT("Failed to launch UnSetup.exe to create start menu shortcuts."));
		// Failure
		return;
	}
	// wait for it to finish and get return code
	INT ReturnCode;
	while (!appGetProcReturnCode(Info.hProcess, &ReturnCode))
	{
		appSleep(0);
	}

#endif

}