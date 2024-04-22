/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_DrawText extends SequenceAction
	dependson(HUD)
	native(Sequence);


var() float DisplayTimeSeconds;	
var() bool	bDisplayOnObject;

cpptext
{
	UBOOL UpdateOp(FLOAT deltaTime);
	virtual void Activated();
};

var() HUD.KismetDrawTextInfo DrawTextInfo;

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
	return Super.GetObjClassVersion() + 2;
}


defaultproperties
{
	ObjName="Draw Text"
	ObjCategory="Misc"

	DrawTextInfo=(MessageText="", MessageFontScale=(X=1,Y=1), MessageOffset=(X=0,Y=0), MessageColor=(R=255,G=255,B=255,A=255),MessageEndTime=-1)
	DisplayTimeSeconds=-1
	bDisplayOnObject=false

	InputLinks(0)=(LinkDesc="Show")
	InputLinks(1)=(LinkDesc="Hide")

	VariableLinks(1)=(ExpectedType=class'SeqVar_String',LinkDesc="String",MinVars=0,bHidden=TRUE)

	bLatentExecution=TRUE
	bAutoActivateOutputLinks=FALSE
}
