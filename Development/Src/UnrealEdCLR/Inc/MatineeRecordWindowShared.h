/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __MATINEE_RECORD_WINDOW_H__
#define __MATINEE_RECORD_WINDOW_H__

class WxInterpEd;

namespace MatineeWindows
{
	/**Gateway function to launch the Recording interface window*/
	void LaunchRecordWindow(WxInterpEd* InInterpEd);

	/**Close Recording Window if one has been opened*/
	void CloseRecordWindow(WxInterpEd* InInterpEd);

	/**Gives focus to the Record Panel so it can receive input*/
	void FocusRecordWindow();

};


#endif // #define __MATINEE_RECORD_WINDOW_H__

