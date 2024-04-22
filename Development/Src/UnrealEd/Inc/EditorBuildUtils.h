/*=============================================================================
	EditorBuildUtils.h: Utilities for building in the editor
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __EditorBuildUtils_h__
#define __EditorBuildUtils_h__

/** Utility class to hold functionality for building within the editor */
class FEditorBuildUtils
{
public:
	/** Enumeration representing automated build behavior in the event of an error */
	enum EAutomatedBuildBehavior
	{
		ABB_PromptOnError,	// Modally prompt the user about the error and ask if the build should proceed
		ABB_FailOnError,	// Fail and terminate the automated build in response to the error
		ABB_ProceedOnError	// Acknowledge the error but continue with the automated build in spite of it
	};

	/** Helper struct to specify settings for an automated editor build */
	struct FEditorAutomatedBuildSettings
	{
		/** Constructor */
		FEditorAutomatedBuildSettings();

		/** Behavior to take when a map build results in map check errors */
		EAutomatedBuildBehavior BuildErrorBehavior;

		/** Behavior to take when a map file cannot be checked out for some reason */
		EAutomatedBuildBehavior UnableToCheckoutFilesBehavior;

		/** Behavior to take when a map is discovered which has never been saved before */
		EAutomatedBuildBehavior NewMapBehavior;

		/** Behavior to take when a saveable map fails to save correctly */
		EAutomatedBuildBehavior FailedToSaveBehavior;

		/** If TRUE, built map files not already in the source control depot will be added */
		UBOOL bAutoAddNewFiles;

		/** If TRUE, the editor will shut itself down upon completion of the automated build */
		UBOOL bShutdownEditorOnCompletion;

		/** If TRUE, the editor will check in all checked out packages */
		UBOOL bCheckInPackages;

		/** Populate list with selected packages to check in */
		TArray<FString> PackagesToCheckIn;

		/** SCC event listener to notify of any SCC action (can be NULL) */
		class FSourceControlEventListener* SCCEventListener;

		/** Changelist description to use for the submission of the automated build */
		FString ChangeDescription;
	};

	/**
	 * Start an automated build of all current maps in the editor. Upon successful conclusion of the build, the newly
	 * built maps will be submitted to source control.
	 *
	 * @param	BuildSettings		Build settings used to dictate the behavior of the automated build
	 * @param	OutErrorMessages	Error messages accumulated during the build process, if any
	 *
	 * @return	TRUE if the build/submission process executed successfully; FALSE if it did not
	 */
	static UBOOL EditorAutomatedBuildAndSubmit( const FEditorAutomatedBuildSettings& BuildSettings, FString& OutErrorMessages );

	/**
	 * Perform an editor build with behavior dependent upon the specified id
	 *
	 * @param	Id	Action Id specifying what kind of build is requested
	 *
	 * @return	TRUE if the build completed successfully; FALSE if it did not (or was manually canceled)
	 */
	static UBOOL EditorBuild( INT Id );

	/**
	 * Helper method to submit packages to source control outside of the automated build process
	 *
	 * @param	InPkgsToSubmit		Set of packages which should be submitted to source control
	 * @param	ChangeDescription	Description (already localized) to be attached to the check in.
	 */
	static void SaveAndCheckInPackages( const TSet<UPackage*>& InPkgsToSubmit, const FString ChangeDescription );

private:

	/**
	 * Private helper method to log an error both to GWarn and to the build's list of accumulated errors
	 *
	 * @param	InErrorMessage			Message to log to GWarn/add to list of errors
	 * @param	OutAccumulatedErrors	List of errors accumulated during a build process so far
	 */
	static void LogErrorMessage( const FString& InErrorMessage, FString& OutAccumulatedErrors );

	/**
	 * Helper method to handle automated build behavior in the event of an error. Depending on the specified behavior, one of three
	 * results are possible:
	 *	a) User is prompted on whether to proceed with the automated build or not,
	 *	b) The error is regarded as a build-stopper and the method returns failure,
	 *	or
	 *	c) The error is acknowledged but not regarded as a build-stopper, and the method returns success.
	 * In any event, the error is logged for the user's information.
	 *
	 * @param	InBehavior				Behavior to use to respond to the error
	 * @param	InErrorMsg				Error to log
	 * @param	OutAccumulatedErrors	List of errors accumulated from the build process so far; InErrorMsg will be added to the list
	 *
	 * @return	TRUE if the build should proceed after processing the error behavior; FALSE if it should not
	 */
	static UBOOL ProcessAutomatedBuildBehavior( EAutomatedBuildBehavior InBehavior, const FString& InErrorMsg, FString& OutAccumulatedErrors );

	/**
	 * Helper method designed to perform the necessary preparations required to complete an automated editor build
	 *
	 * @param	BuildSettings		Build settings that will be used for the editor build
	 * @param	OutPkgsToSubmit		Set of packages that need to be saved and submitted after a successful build
	 * @param	OutErrorMessages	Errors that resulted from the preparation (may or may not force the build to stop, depending on build settings)
	 *
	 * @return	TRUE if the preparation was successful and the build should continue; FALSE if the preparation failed and the build should be aborted
	 */
	static UBOOL PrepForAutomatedBuild( const FEditorAutomatedBuildSettings& BuildSettings, TSet<UPackage*>& OutPkgsToSubmit, FString& OutErrorMessages );

	/**
	 * Helper method to submit packages to source control as part of the automated build process
	 *
	 * @param	InPkgsToSubmit	Set of packages which should be submitted to source control
	 * @param	BuildSettings	Build settings used during the automated build
	 */
	static void SubmitPackagesForAutomatedBuild( const TSet<UPackage*>& InPkgsToSubmit, const FEditorAutomatedBuildSettings& BuildSettings );
	/** Intentionally hide constructors, etc. to prevent instantiation */
	FEditorBuildUtils();
	~FEditorBuildUtils();
	FEditorBuildUtils( const FEditorBuildUtils& );
	FEditorBuildUtils operator=( const FEditorBuildUtils& );
};

#endif // #ifndef __EditorBuildUtils_h__
