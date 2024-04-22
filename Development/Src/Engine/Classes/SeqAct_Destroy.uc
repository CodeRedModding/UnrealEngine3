/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_Destroy extends SequenceAction;

var() bool bDestroyBasedActors;
var() array< class<Actor> > IgnoreBasedClasses;

defaultproperties
{
	ObjName="Destroy"
	ObjCategory="Actor"
}
