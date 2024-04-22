/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_Trace extends SequenceAction
	native(Sequence);

cpptext
{
	virtual void Activated();
	virtual void DeActivated()
	{
	}
}

/** Should actors be traced against? */
var() bool bTraceActors;

/** Should the world be traced against? */
var() bool bTraceWorld;

/** What extent should be used for the trace? */
var() vector TraceExtent;

var() vector StartOffset, EndOffset;

var() editconst Object HitObject;
var() editconst float  Distance;
var() editconst	Vector HitLocation;

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
	ObjName="Trace"
	ObjCategory="Misc"

	bTraceWorld=TRUE

	VariableLinks.Empty
	VariableLinks(0)=(LinkDesc="Start",ExpectedType=class'SeqVar_Object')
	VariableLinks(1)=(LinkDesc="End",ExpectedType=class'SeqVar_Object')
	VariableLinks(2)=(LinkDesc="HitObject",ExpectedType=class'SeqVar_Object',bWriteable=TRUE,PropertyName=HitObject)
	VariableLinks(3)=(LinkDesc="Distance",ExpectedType=class'SeqVar_Float',bWriteable=TRUE,PropertyName=Distance)
	VariableLinks(4)=(LinkDesc="HitLoc",ExpectedType=class'SeqVar_Vector',bWriteable=TRUE,PropertyName=HitLocation)

	OutputLinks(0)=(LinkDesc="Not Obstructed")
	OutputLinks(1)=(LinkDesc="Obstructed")
}
