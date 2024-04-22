/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#ifndef __BUILD_AND_SUBMIT_WINDOW_H__
#define __BUILD_AND_SUBMIT_WINDOW_H__

#include "SourceControl.h"

#if HAVE_SCC
namespace BuildWindows
{
	/**
	 * Prompt the user with the build all and submit dialog, allowing the user
	 * to enter a changelist description to use for a source control submission before
	 * kicking off a full build and submitting the map files to source control
	 *
	 * @param	InEventListener	Event listener to be updated to source control commands
	 */
	void PromptForBuildAndSubmit( FSourceControlEventListener* InEventListener );	
}
#endif // #if HAVE_SCC

#endif // #ifndef __BUILD_AND_SUBMIT_WINDOW_H__
