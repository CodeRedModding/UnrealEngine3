/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_ToggleHidden extends SeqAct_Toggle;

var() bool bToggleBasedActors;
var() array< class<Actor> > IgnoreBasedClasses;


defaultproperties
{
	ObjName="Toggle Hidden"
	ObjCategory="Toggle"

	InputLinks(0)=(LinkDesc="Hide")
	InputLinks(1)=(LinkDesc="UnHide")
}
