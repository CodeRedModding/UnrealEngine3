/**********************************************************************

Copyright   :   (c) 2006-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING 
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/
class GFxEvent_FSCommand extends SequenceEvent
	dependson(GFxFSCmdHandler_Kismet)
	native(UISequence);

/** Swf Movie data to use. */
var() SwfMovie              Movie;

// name of fscommand to trigger on
var()   string              FsCommand;

/**Command handler to route events through*/
var		GFxFSCmdHandler_Kismet				Handler;

cpptext
{
	virtual void FinishDestroy();
	virtual UBOOL RegisterEvent();

	/**
	 * Adds an error message to the map check dialog if this SequenceEvent's EventActivator is bStatic
	 */
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif
}

defaultproperties
{
	ObjName="FsCommand"
	ObjCategory="GFx UI"

	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_String',LinkDesc="Argument",bWriteable=false)

	bPlayerOnly=false
	MaxTriggerCount=0
}
