/*=============================================================================
	DebugToolExec.h: Game debug tool implementation.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/ 

/**
 * This class servers as an interface to debugging tools like "EDITOBJECT" which
 * can be invoked by console commands which are parsed by the exec function.
 */
class FDebugToolExec : public FExec
{
protected:
	/**
	 * Brings up a property window to edit the passed in object.
	 *
	 * @param Object	property to edit
	 */
	void EditObject( UObject* Object );

public:
	/**
	 * Exec handler, parsing the passed in command
	 *
	 * @param Cmd	Command to parse
	 * @param Ar	output device used for logging
	 */
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar );
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
