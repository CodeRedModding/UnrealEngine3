/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_SetSkelControlTarget extends SequenceAction;

/** Name of SkelControl to set target of */
var()	name			SkelControlName;
/** List of objects to call the handler function on */
var()	array<Object>	TargetActors;

defaultproperties
{
	ObjName="Set SkelControl Target"
	ObjCategory="Actor"

	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="SkelMesh",PropertyName=Targets)
	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="TargetActor",PropertyName=TargetActors)
}
