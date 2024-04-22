/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __SOURCE_CONTROL_WINDOWS_H__
#define __SOURCE_CONTROL_WINDOWS_H__

#include "SourceControl.h"

#if HAVE_SCC

namespace SourceControlWindows
{
	UBOOL PromptForCheckin(FSourceControlEventListener* InEventListener, const TArray<FString>& InPackageNames);

	/**
	 * Display file revision history for the provided packages
	 *
	 * @param	InPackageNames	Names of packages to display file revision history for
	 */
	void DisplayRevisionHistory( const TArray<FString>& InPackagesNames );

	/**
	 * Prompt the user with a revert files dialog, allowing them to specify which packages, if any, should be reverted.
	 *
	 * @param	InEventListener	Object which should receive the source control callback
	 * @param	InPackageNames	Names of the packages to consider for reverting
	 *
	 * @param	TRUE if the files were reverted; FALSE if the user canceled out of the dialog
	 */
	UBOOL PromptForRevert( FSourceControlEventListener* InEventListener, const TArray<FString>& InPackageNames );
};


#endif // HAVE_SCC
#endif // #define __SOURCE_CONTROL_WINDOWS_H__

