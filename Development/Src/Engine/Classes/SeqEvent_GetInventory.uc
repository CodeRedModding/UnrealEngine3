/**
 * Event which is activated by gameplay code when a new item is added to a controller's inventory list.
 * Orignator: the pawn that owns the InventoryList that the item was added to
 * Instigator: the inventory item that was added.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_GetInventory extends SequenceEvent
	native(Sequence);

cpptext
{
	virtual UBOOL CheckActivate(AActor *InOriginator, AActor *InInstigator, UBOOL bTest=FALSE, TArray<INT>* ActivateIndices = NULL, UBOOL bPushTop = FALSE);
};

defaultproperties
{
	ObjName="Get Inventory"
	ObjCategory="Pawn"
	bPlayerOnly=false

	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="Inventory",bWriteable=true)
}
