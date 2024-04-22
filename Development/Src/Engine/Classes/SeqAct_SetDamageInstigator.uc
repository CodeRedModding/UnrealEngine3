/** 
 * sets who gets credit for damage caused by the Target Actor
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_SetDamageInstigator extends SequenceAction;

var Actor DamageInstigator;

defaultproperties
{
	ObjCategory="Actor"
	ObjName="Set Damage Instigator"
	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Damage Instigator",PropertyName=DamageInstigator,MinVars=1,MaxVars=1)
}
