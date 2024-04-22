// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#ifndef __NEW_PROJECT_H__
#define __NEW_PROJECT_H__

#include "InteropShared.h"

#ifdef __cplusplus_cli

// Forward declarations
ref class MWPFFrame;
ref class MNewProjectPanel;

#endif // #ifdef __cplusplus_cli

class FNewUDKProjectWizard;

/** Non-managed singleton wrapper around the new project screen */
class FNewProjectScreen
{
public:
	/** Enumeration representing the possible results returned from the project wizard screen */
	enum NewProjectResults
	{
		PROJWIZ_Cancel,
		PROJWIZ_Finish
	};

	/** 
	* Display the new project screen
	*
	* @return	TRUE if the user has made valid setting selections, FALSE if the user canceled
	*
	*/
	static BOOL DisplayNewProjectScreen( FNewUDKProjectWizard* InProjWizSystem, UBOOL bShowSuccessScreen = FALSE );

	/** Shut down the new project screen singleton */
	static void Shutdown();

private:
	/** Constructor */
	FNewProjectScreen();

	/** Destructor */
	~FNewProjectScreen();

	// Copy constructor and assignment operator intentionally left unimplemented
	FNewProjectScreen( const FNewProjectScreen& );
	FNewProjectScreen& operator=( const FNewProjectScreen& );

	/**
	 * Return internal singleton instance of the class
	 *
	 * @return	Reference to the internal singleton instance of the class
	 */
	static FNewProjectScreen& GetInternalInstance();

	/** Frame used for the new project screen */
	GCRoot( MWPFFrame^ ) NewProjectScreenFrame;

	/** Panel used for the project screen */
	GCRoot( MNewProjectPanel^ ) NewProjectPanel;

	/** Singleton instance of the class */
	static FNewProjectScreen* Instance;
};

#endif // #ifndef __NEW_PROJECT_H__
