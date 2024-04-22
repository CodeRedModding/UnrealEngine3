// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#ifndef __NEWPROJECT_H__
#define __NEWPROJECT_H__

#ifdef _MSC_VER
#pragma once
#endif

// Forward declaration.
class FConfigFile;

/** Project wizard modes */
namespace EUDKProjWizMode
{
	enum Type
	{
		/** User is interacting with the project wizard through a UI */
		Interactive,

		/** Don't show dialogs */
		NonInteractive
	};
}


/** Return flags used when validating settings */
namespace EUDKProjWizSettingReturnCodes
{
	enum Type
	{
		CODE_NoError					= 0x00000000,

		// Error codes for invalid settings
		CODE_InvalidTargetDirectory		= 0x00000001,	// The target directory is invalid
		CODE_InvalidProjectName			= 0x00000002,	// The project name is invalid
		CODE_InvalidSourceDirectory		= 0x00000004,	// The source directory is invalid
		CODE_InvalidShortName			= 0x00000008,	// The project short name is invalid
	
		// Other Error codes 
		CODE_CantCreateInstallDir		= 0x00010000,	// The target install directory can't be created
		CODE_FailedFileRead				= 0x00020000,	// Failed to read a file
		CODE_FailedFileWrite			= 0x00040000,	// Failed to write a file
		CODE_FailedFileCopy				= 0x00080000,	// Failed to copy a file
		CODE_InvalidManifestFile		= 0x00100000,	// Missing or corrupt manifest file
		CODE_FailedToLaunchNewEditor	= 0x00200000,	// Could not launch new project editor
		CODE_MissingProjectTemplate		= 0x00400000,	// Could not find one of the expected project templates	
		CODE_FailedToBuildScripts		= 0x00800000,	// The new projects scripts failed to build
		CODE_InsufficientDiskSpace		= 0x01000000,	// The user has run out of disk space

		CODE_MAX						= 0x40000000,
		CODE_Unknown					= CODE_MAX
	};
}


/**
 * FNewUDKProjectSettings.
 * Class to control the global project settings.
 */
class FNewUDKProjectSettings
{
protected:

	/** Static: Global new UDK project settings */
	static FNewUDKProjectSettings StaticNewUDKProjectSettings;

public:

	/** Static: Returns global new UDK project settings */
	static FNewUDKProjectSettings& Get()
	{
		return StaticNewUDKProjectSettings;
	}

	/** Constructor */
	FNewUDKProjectSettings()
		:	SourceProject( TEXT("") )
		,	ProjectWizardMode( EUDKProjWizMode::NonInteractive )
		,	ProjectNameSetting( TEXT("My Custom Game") )
		,	ShortNameSetting( TEXT("Custom") )
		,	InstallDirectorySetting( TEXT("") )
		,	bOverwriteExisting( FALSE )
	{
	}

	FString SourceProject;
	EUDKProjWizMode::Type ProjectWizardMode;
	FString ProjectNameSetting;
	FString ShortNameSetting;
	FString InstallDirectorySetting;
	UBOOL bOverwriteExisting;

};

/**  Template struct used to store the settings that a user can change. */
struct FUserProjSetting
{
	FString SettingKey;
	FString SettingValue;
	FString SettingDisplayGroup;

	FString GetSettingDisplayName() 
	{
		FString DisplayName = SettingKey;
		SanitizePropertyDisplayName( DisplayName, FALSE );
		
		return DisplayName;
	}
};


/**  Stores the info associated with a project wizard file. */
struct FProjWizFileInfo
{
	FFilename SourcePath;
	INT FileSize;

	FProjWizFileInfo()
		:	FileSize(-1)
	{}
	FProjWizFileInfo(const FFilename& InSourceFile, const INT InFileSize)
		:	SourcePath( InSourceFile )
		,	FileSize( InFileSize )
	{}
};


/** Template class used to store info about a particular project template. */
class FProjWizTemplate
{
private:
	/** Stores the full path to the template folder */
	FString FullPath;
	/** Stores the computed install size for the template as it would appear installed on disk along with BaseTemplate.  This value is passed in from external source */
	SQWORD InstallSize;
	/** Stores the computed install size for the template files only.  No other templates or files are included with this. */
	INT InstallSizeInternal;
	/** Stores the files available in the template.  This is cached so we don't have to hit the disk to get this info more than once */
	TArray<FProjWizFileInfo> TemplateFiles;
	/** Store all of the project specific settings the user can change */
	TArray<FUserProjSetting> TemplateAdvancedSettings;
	/** Stores the contents of the TemplateInfo.ini file.  We cache this so we don't have to hit the disk each time to retrieve info */
	FConfigFile TemplateConfigFile;

public:
	FProjWizTemplate( const FString& InFullPath );

	/** 
	* Obtains the localized name for the template, if none is found it will return the name of the directory
	* the template is located in.
	*
	* @return Localized template name or directory name if no localized entry is found.
	*/
	FString GetName() const;

	/** 
	* Obtains the full path to the template folder.
	*
	* @return The template folder path.
	*/
	FString GetFullPath() const  { return FullPath; }

	/** 
	* Obtains the localized description for the template, if none is found it will return the empty string.
	*
	* @return	Localized template description or the empty string if none is found.
	*/
	FString GetDescription() const;

	/** 
	* Retrieves the cached install size. 
	*
	* @return The calculated install size for this template
	*/
	SQWORD GetInstallSize();

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
	void SetInstallSize( SQWORD InInstallSize );

	/** 
	* Retrieves a list of files that are included in the template.  Will exclude files not used by the game.
	*
	* @param	OutResult		Empty string array that will store the files list.
	*
	*/
	void GetTemplateFiles( TArray<FString>& OutResult );

	/** 
	* Retrieves a list of advanced settings that the user can set for this template.
	*
	* @param	OutResult		Empty string array that will store the files list.
	*
	*/
	void GetTemplateAdvancedSettings(TArray<FUserProjSetting>& OutResult);

	/** 
	* Retrieves a list of files that are included in the template.  Will exclude files not used by the game.
	*
	* @param	OutResult	FProjWizFileInfo array that will store the files list
	*
	*/
	void GetTemplateFiles( TArray<FProjWizFileInfo>& OutResult );

	/** 
	* Returns a reference to the TemplateInfo.ini config file for this template. 
	*/
	FConfigFile* GetTemplateConfigFile();
	

private:

	/** 
	* Calculates the install size for this template and caches the value for future retrieval.
	* This value will not be a fully accurate representation of the on disk size of this template when
	* used to generate a project.  For that we need the install size to be set externally via SetInstallSize().
	*
	* @return	Returns the calculated size.
	*/
	INT CalcInternalInstallSize();

	/** 
	* Fills the TemplateFiles structure with files we find in the template. 
	*/
	void FillTemplateFilesCache();

	/** 
	* Fills the advanced setting structure with items we pulled from the template ini and crosschecked with the template settings files. 
	*/
	void FillTemplateAdvancedSettingCache();

	/**
	* Fills the cached template config file structure
	*/
	void LoadTemplateConfigFile();

};

/**
 *  Wrapper to expose project settings to WPF code.
 */
struct FUserProjectSettingsListInfo
{
	FUserProjSetting* SettingData;
	UBOOL bIsSelected;

	FUserProjectSettingsListInfo(FUserProjSetting* InSettingData, UBOOL InbIsSelected = FALSE)
		:	SettingData(InSettingData)
		,	bIsSelected(InbIsSelected)
	{}
};

/**
 *  Wrapper to expose template targets to WPF code.
 */
struct FTemplateTargetListInfo
{
	FProjWizTemplate* TemplateData;
	UBOOL bIsSelected;
	
	FTemplateTargetListInfo(FProjWizTemplate* InTemplateData, UBOOL InbIsSelected = FALSE)
		:	TemplateData(InTemplateData)
		,	bIsSelected(InbIsSelected)
	{}
};


/**
 * FNewUDKProjectWizard.
 * This class drives the interactive process associated with creating a
 * new custom UDK project.
 */
class FNewUDKProjectWizard
{
public:

	/** Constructor */
	FNewUDKProjectWizard();
	~FNewUDKProjectWizard();

	/**
	* Searches the template folder for a specified project and sets all the template info. 
	*
	* @param	InDirectoryPath		Project directory path.
	*/
	void PopulateTemplateList( const FString& InDirectoryPath );

	/** Sets some appropriate default settings based on the UDK environment */
	void GenerateDefaultSettings();

	/**
	 * Used to retrieve the last error encountered by the project wizard.
	 *
	 * @return	Error code associated with the last set of errors encountered by the project wizard in the EUDKProjWizSettingReturnCodes format.
	 */
	INT GetLastErrorCode(void)
	{
		return LastErrorCode;
	}

	/**
	* Returns a human readable string for an individual error code.  If no code is provided this function will attempt to retrieve
	* the last error code encountered by the project wizard.
	*
	* @param Code	The error code to check
	*/
	FString GetErrorString( INT Code = -1 );

	/**
	* Takes an error code, which can store multiple errors, and displays a dialog with the error info.
	* Will use the LastErrorCode value if no code is provided.  If there are no valid error codes, no dialog will be shown.
	*
	* @param Code	The error code used to generate the dialog contents, if none is provided the 
	*/
	void ShowErrorDialog( INT Code = -1 );

	/** 
	* Used to validate the UDK Project Wizard settings.
	*
	* @return	Returns an error code in the EUDKProjWizSettingReturnCodes format
	*/
	INT ValidateSettings();

	/** Launches the project wizard UI */
	UBOOL RunUDKProjectWizard();

	/** Will return the list of items that will appear in the UI */
	TArray<FTemplateTargetListInfo>* GetTemplateTargetList();

	/** Will update the list of items that will appear in the UI */
	void UpdateTemplateTargetList();

	/** Will return the list of user setting items that will appear in the UI */
	TArray<FUserProjectSettingsListInfo>* GetUserSettingsList();

	/** Will update the list of user settings that will appear in the UI */
	void UpdateUserSettingsList();
	
	/** Will return the selected target template. */
	FProjWizTemplate* GetSelectedTemplate();

	/** Will return the base template. */
	FProjWizTemplate* GetBaseTemplate();

protected:

	/** Generates the new UDK project based on the current settings 
	*
	* @return	A return code representing the error encountered in the EUDKProjWizSettingReturnCodes format
	*/
	INT Exec();


	UBOOL GetFilesToProcessForTemplate( TMap< FFilename, FProjWizFileInfo >& OutFilesToProcess, FProjWizTemplate* SelectedTemplate );

	/**
	 * Creates a directory path that belongs to the set "InDirectoryPath##" where ## is a 2-digit number
	 * in [0-99] and no directory of that name currently exists.  The return value is the index of first
	 * empty filename, or -1 if none could be found.
	 * 
	 *
	 * @param InDirectoryPath	Directory path.
	 * @param OutDirectoryPath	Used to return an available directory path (untouched on fail).
	 * @param StartVal			Optional parameter that can be used to hint beginning of index search.
	 * @return					The index of the available directory path, or -1 if no free path with index [StartVal, 99] was found.
	 */
	INT FindAvailableDirectoryPath( const FString& InDirectoryPath, FString& OutFilename, INT StartVal = 0 );

	/* Calculates and returns the generated project's executable. */
	FString GetNewProjectExePath();

	/** Will close the current instance of the editor and start the editor in the new generated project */
	UBOOL TriggerScriptBuild();

	/** Will close the current instance of the editor and start the editor in the new generated project */
	void TriggerEditorSwitch();

	/** Will generate ini files for the new project and make sure that we set the appropriate advanced settings */
	void TriggerGenerateINI();

	/** Will generate game, editor, tool, and documentation shortcuts in the start menu. */
	void CreateShortcuts();

	/** Stores the last error code we encountered */
	INT LastErrorCode;

	/** Store all of our template info.  Key: TemplatePath */
	TMap<FString, FProjWizTemplate> TemplateItems;

	/** List of templates displayed in the UI. */
	TArray<FTemplateTargetListInfo> UITemplateItems;

	/** Store all of the project specific settings the user can change */
	TArray<FUserProjSetting> UserSettingItems;

	/** List of UI displayable project specific settings the user can change. */
	TArray<FUserProjectSettingsListInfo> UIUserSettingItems;

};

#endif // __NEWPROJECT_H__