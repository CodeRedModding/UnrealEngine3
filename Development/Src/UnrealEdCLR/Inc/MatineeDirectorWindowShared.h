/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __MATINEE_DIRECTOR_WINDOWS_H__
#define __MATINEE_DIRECTOR_WINDOWS_H__

class WxInterpEd;

namespace MatineeWindows
{
	/**Gateway function to launch the director interface window*/
	void LaunchDirectorWindow(WxInterpEd* InInterpEd);

	/**Close Director Window if one has been opened*/
	void CloseDirectorWindow(WxInterpEd* InInterpEd);
};


#endif // #define __MATINEE_DIRECTOR_WINDOWS_H__

