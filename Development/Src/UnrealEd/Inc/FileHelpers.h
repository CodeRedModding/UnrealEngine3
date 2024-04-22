/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __FILEHELPERS_H__
#define __FILEHELPERS_H__

// Forward declarations.
class FFileName;
class FString;

enum EFileInteraction
{
	FI_Load,
	FI_Save,
	FI_Import,
	FI_Export
};

/**
 * For saving map files through the main editor frame.
 */
class FEditorFileUtils
{
public:

	/** Used to decide how to handle garbage collection. */
	enum EGarbageCollectionOption
	{
		GCO_SkipGarbageCollection	= 0,
		GCO_CollectGarbage			= 1,
	};

	// Maximum length of any filename.  To ensure files have compatability with all platforms
	static const UINT MAX_UNREAL_FILENAME_LENGTH = 30;
	////////////////////////////////////////////////////////////////////////////
	// New

	/**
	 * Prompts the user to save the current map if necessary, then creates a new (blank) map.
	 */
	static void NewMap();

	////////////////////////////////////////////////////////////////////////////
	// NewMapInteractive

	/**
	 * Shows the new map screen/dialog. Gives the user a choice of new blank or
	 * templated maps and deals with overwrite prompts etc.
	 */
	static void NewMapInteractive();

	////////////////////////////////////////////////////////////////////////////
	// NewProjectInteractive

	/**
	 * Shows the new project screen/dialog. Guides the user through a series of
	 * required to setup and customize a new project.
	 */
	static void NewProjectInteractive();

	////////////////////////////////////////////////////////////////////////////
	// ResetLevelFilenames

	/**
	 * Clears current level filename so that the user must SaveAs on next Save.
	 * Called by NewMap() after the contents of the map are cleared.
	 * Also called after loading a map template so that the template isn't overwritten.
	 */
	static void ResetLevelFilenames();	

	////////////////////////////////////////////////////////////////////////////
	// Loading

	/**
	 * Prompts the user to save the current map if necessary, the presents a load dialog and
	 * loads a new map if selected by the user.
	 */
	static void LoadMap();

	/**
	 * Loads the specified map.  Does not prompt the user to save the current map.
	 *
	 * @param	Filename		Map package filename, including path.
	 *
	 * @param	LoadAsTemplate	Forces the map to load into an untitled outermost package
	 *							preventing the map saving over the original file.
	 */
	static void LoadMap(const FFilename& Filename, UBOOL LoadAsTemplate = FALSE);

	////////////////////////////////////////////////////////////////////////////
	// Saving

	/**
	 * Saves the specified map package, returning TRUE on success.
	 *
	 * @param	World			The world to save.
	 * @param	Filename		Map package filename, including path.
	 * @param	bAddToMRUList	True if the level should be added to the editor's MRU level list
	 *
	 * @return					TRUE if the map was saved successfully.
	 */
	static UBOOL SaveMap(UWorld* World, const FFilename& Filename, const UBOOL bAddToMRUList );

	/**
	 * Saves the specified level.  SaveAs is performed as necessary.
	 *
	 * @param	Level				The level to be saved.
	 * @param	DefaultFilename		File name to use for this level if it doesn't have one yet (or empty string to prompt)
	 *
	 * @return				TRUE if the level was saved.
	 */
	static UBOOL SaveLevel(ULevel* Level, const FFilename& DefaultFilename = TEXT( "" ) );

	/**
	 * Does a saveAs for the specified level.
	 *
	 * @param	Level		The level to be SaveAs'd.
	 * @return				TRUE if the level was saved.
	 */
	static UBOOL SaveAs(UObject* LevelObject);

	/**
	 * Checks to see if GWorld's package is dirty and if so, asks the user if they want to save it.
	 * If the user selects yes, does a save or SaveAs, as necessary.
	 */
	static UBOOL AskSaveChanges();

	/**
	 * Saves all writable levels associated with GWorld.
	 *
	 * @param	bCheckDirty		If TRUE, don't save level packages that aren't dirty.
	 */
	static void SaveAllWritableLevels(UBOOL bCheckDirty);

	/**
	 * Saves all levels to the specified directory.
	 *
	 * @param	AbsoluteAutosaveDir		Autosave directory.
	 * @param	AutosaveIndex			Integer prepended to autosave filenames..
	 */
	static UBOOL AutosaveMap(const FString& AbsoluteAutosaveDir, INT AutosaveIndex);

	/**
	 * Saves all asset packages to the specified directory.
	 *
	 * @param	AbsoluteAutosaveDir		Autosave directory.
	 * @param	AutosaveIndex					Integer prepended to autosave filenames.
	 * @param PackagesToSave				List of packages to be saved, if empty saves all
	 *
	 * @return	TRUE if one or more packages were autosaved; FALSE otherwise
	 */
	static UBOOL AutosaveContentPackages(const FString& AbsoluteAutosaveDir, INT AutosaveIndex, const TArrayNoInit<FString>* PackagesToSave);

	/**
	 * Looks at all currently loaded packages and saves them if their "bDirty" flag is set, optionally prompting the user to select which packages to save)
	 * 
	 * @param	bPromptUserToSave			TRUE if we should prompt the user to save dirty packages we found. FALSE to assume all dirty packages should be saved.  Regardless of this setting the user will be prompted for checkout(if needed) unless bFastSave is set
	 * @param	bSaveMapPackages			TRUE if map packages should be saved
	 * @param	bSaveContentPackages		TRUE if we should save content packages. 
	 * @param	bFastSave					TRUE if we should do a fast save. (I.E dont prompt the user to save, dont prompt for checkout, and only save packages that are currently writable).  Note: Still prompts for SaveAs if a package needs a filename
	 * @return								TRUE on success, FALSE on fail.
	 */
	static UBOOL SaveDirtyPackages(const UBOOL bPromptUserToSave, const UBOOL bSaveMapPackages, const UBOOL bSaveContentPackages, const UBOOL bFastSave = FALSE);

	/** Enum used for prompt returns */
	enum EPromptReturnCode
	{
		PR_Success,		/** The user has answered in the affirmative to all prompts, and execution succeeded */
		PR_Failure,		/** The user has answered in the affirmative to prompts, but an operation(s) has failed during execution */
		PR_Declined,	/** The user has declined out of the prompt; the caller should continue whatever it was doing */
		PR_Cancelled	/** The user has cancelled out of a prompt; the caller should abort whatever it was doing */
	};

	/**
	 * Optionally prompts the user for which of the provided packages should be saved, and then additionally prompts the user to check-out any of
	 * the provided packages which are under source control. If the user cancels their way out of either dialog, no packages are saved. It is possible the user
	 * will be prompted again, if the saving process fails for any reason. In that case, the user will be prompted on a package-by-package basis, allowing them
	 * to retry saving, skip trying to save the current package, or to again cancel out of the entire dialog. If the user skips saving a package that failed to save,
	 * the package will be added to the optional OutFailedPackages array, and execution will continue. After all packages are saved (or not), the user is provided with
	 * a warning about any packages that were writable on disk but not in source control, as well as a warning about which packages failed to save.
	 *
	 * @param		PackagesToSave				The list of packages to save.  Both map and content packages are supported 
	 * @param		bCheckDirty					If TRUE, only packages that are dirty in PackagesToSave will be saved	
	 * @param		bPromptToSave				If TRUE the user will be prompted with a list of packages to save, otherwise all passed in packages are saved
	 * @param		OutFailedPackages			[out] If specified, will be filled in with all of the packages that failed to save successfully
	 *
	 * @return		An enum value signifying success, failure, user declined, or cancellation. If any packages at all failed to save during execution, the return code will be 
	 *				failure, even if other packages successfully saved. If the user cancels at any point during any prompt, the return code will be cancellation, even though it
	 *				is possible some packages have been successfully saved (if the cancel comes on a later package that can't be saved for some reason). If the user opts the "Don't
	 *				Save" option on the dialog, the return code will indicate the user has declined out of the prompt. This way calling code can distinguish between a decline and a cancel
	 *				and then proceed as planned, or abort its operation accordingly.
	 */
	static EPromptReturnCode PromptForCheckoutAndSave( const TArray<UPackage*>& PackagesToSave, UBOOL bCheckDirty, UBOOL bPromptToSave, TArray<UPackage*>* OutFailedPackages = NULL );

	////////////////////////////////////////////////////////////////////////////
	// Import/Export

	/**
	 * Presents the user with a file dialog for importing.
	 * If the import is not a merge (bMerging is FALSE), AskSaveChanges() is called first.
	 *
	 * @param	bMerge	If TRUE, merge the file into this map.  If FALSE, merge the map into a blank map.
	 */
	static void Import(UBOOL bMerge);								// prompts user for file etc.
	static void Import(const FFilename& InFilename, UBOOL bMerge);	// no prompts
	static void Export(UBOOL bExportSelectedActorsOnly);			// prompts user for file etc.

	////////////////////////////////////////////////////////////////////////////
	// Source Control

	/**
	 * Prompt the user with a check-box dialog allowing him/her to check out the provided packages
	 * from source control, if desired
	 *
	 * @param	bCheckDirty						If TRUE, non-dirty packages won't be added to the dialog
	 * @param	PackagesToCheckOut				Reference to array of packages to prompt the user with for possible check out
	 * @param	OutPackagesNotNeedingCheckout	If not NULL, this array will be populated with packages that the user was not prompted about and do not need to be checked out to save.  Useful for saving packages even if the user canceled the checkout dialog.
	 * @param	bPromptingAfterModify			If TRUE, we are prompting the user after an object has been modified, which changes the cancel button to "Ask me later".
	 * @param	bPromptingForDependentMaterialPackages If TRUE, this is being launched after loading a material with a dependency change.  This changes the button options, but the functionality is maintained.
	 *
	 * @return	TRUE if the user did not cancel out of the dialog and has potentially checked out some files
	 *			(or if there is no source control integration); FALSE if the user cancelled the dialog
	 */
	static UBOOL PromptToCheckoutPackages(UBOOL bCheckDirty, const TArray<UPackage*>& PackagesToCheckOut, TArray< UPackage* >* OutPackagesNotNeedingCheckout = NULL, const UBOOL bPromptingAfterModify = FALSE, const UBOOL bPromptingForDependentMaterialPackages = FALSE );

	/**
	 * Prompt the user with a check-box dialog allowing him/her to check out relevant level packages 
	 * from source control
	 *
	 * @param	bCheckDirty					If TRUE, non-dirty packages won't be added to the dialog
	 * @param	SpecificLevelsToCheckOut	If specified, only the provided levels' packages will display in the
	 *										dialog if they are under source control; If nothing is specified, all levels
	 *										referenced by GWorld whose packages are under source control will be displayed
	 * @param	OutPackagesNotNeedingCheckout	If not null, this array will be populated with packages that the user was not prompted about and do not need to be checked out to save.  Useful for saving packages even if the user canceled the checkout dialog.
	 *
	 * @return	TRUE if the user did not cancel out of the dialog and has potentially checked out some files (or if there is
	 *			no source control integration); FALSE if the user cancelled the dialog
	 */
	static UBOOL PromptToCheckoutLevels(UBOOL bCheckDirty, const TArray<ULevel*>* const SpecificLevelsToCheckOut = NULL, TArray<UPackage*>* OutPackagesNotNeedingCheckout = NULL);

	/**
	 * Overloaded version of PromptToCheckOutLevels which prompts the user with a check-box dialog allowing
	 * him/her to check out the relevant level package if necessary
	 *
	 * @param	bCheckDirty				If TRUE, non-dirty packages won't be added to the dialog
	 * @param	SpecificLevelToCheckOut	The level whose package will display in the dialog if it is
	 *									under source control
	 *
	 * @return	TRUE if the user did not cancel out of the dialog and has potentially checked out some files (or if there is
	 *			no source control integration); FALSE if the user cancelled the dialog
	 */
	static UBOOL PromptToCheckoutLevels(UBOOL bCheckDirty, ULevel* SpecificLevelToCheckOut);

	/**
	 * Checks to see if a filename is valid for saving.
	 * A filename must be under MAX_UNREAL_FILENAME_LENGTH to be saved
	 *
	 * @param Filename	Filename, with or without path information, to check.
	 * @param OutError	If an error occurs, this is the reason why
	 */
	static UBOOL IsFilenameValidForSaving( const FString& Filename, FString& OutError );

	/**
	 * Static: Returns the simple map file name, or an empty string if no simple map is set for this game
	 *
	 * @return	The name of the simple map
	 */
	static FString GetSimpleMapName();

	/** Loads the simple map for UDK */
	static void LoadSimpleMapAtStartup ();

	/** Whether or not we're in the middle of loading the simple startup map */
	static UBOOL IsLoadingSimpleStartupMap() {return bIsLoadingSimpleStartupMap;}

	/**
	 * Returns a file filter string appropriate for a specific file interaction.
	 *
	 * @param	Interaction		A file interaction to get a filter string for.
	 * @return					A filter string.
	 */
	static FString GetFilterString(EFileInteraction Interaction);

private:
	static UBOOL bIsLoadingSimpleStartupMap;
};

#endif
