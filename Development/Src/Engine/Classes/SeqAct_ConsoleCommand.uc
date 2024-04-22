/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_ConsoleCommand extends SequenceAction;

/** deprecated. Use Commands array. */
var string Command;

/** Array of console commands to issue, in the order to execute them. */
var() array<string> Commands;

function VersionUpdated(int OldVersion, int NewVersion)
{
	// if user hasn't filled in the new commands array, auto-migrate
	// the old Command.
	if ( (OldVersion < 2) &&
		 ( (Commands.length == 0) || (Commands[0] == "") )
		)
	{
		Commands[0] = Command;
	}
}

/**
 * Return the version number for this class.  Child classes should increment this method by calling Super then adding
 * a individual class version to the result.  When a class is first created, the number should be 0; each time one of the
 * link arrays is modified (VariableLinks, OutputLinks, InputLinks, etc.), the number that is added to the result of
 * Super.GetObjClassVersion() should be incremented by 1.
 *
 * @return	the version number for this specific class.
 */
static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 1;
}

defaultproperties
{
	Commands(0)=""

	ObjName="Console Command"
	ObjCategory="Misc"
}
