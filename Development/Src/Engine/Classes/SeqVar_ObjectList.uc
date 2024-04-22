/**
* An ObjectList Var is derived from the SeqVar_Object Variable so it may be used
* by Actions as if it were just another ObjectVar.  For Actions that modify
* a pointed to ObjectVar Kismet object when in fact they are pointing to an
* ObjectList, the behavior will be semi undefined and probably will not do what
* they expect.
*
* For Actions that take a set of pointed to ObjectVars and then do an action
* TO the actual UObject (e.g. an ExampleGamePawn), then the ObjectList will
* return the set of Objects it has and the action will just work.
*
* Also, SeqVar_ObjectList objects are not persistent.  ObjectLists (for now) are
* meant to be runtime list storage.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class SeqVar_ObjectList extends SeqVar_Object
	native(Sequence);


cpptext
{
	virtual void OnCreated();

	// we need to see how to export arrays here, let's look at inventory
	virtual void OnExport();

	virtual UObject** GetObjectRef( INT Idx );

	virtual FString GetValueStr();

#if WITH_EDITOR
	// USequenceVariable interface
	virtual void DrawExtraInfo(FCanvas* Canvas, const FVector& CircleCenter);
#endif

	virtual UBOOL SupportsProperty(UProperty *Property)
	{
		return FALSE;
	}

	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent);
}
/** this is our list of objects which this kismet variable holds **/
 var() array<Object>			ObjList;

function Object GetObjectValue()
{
	return (ObjList.length > 0) ? ObjList[0] : None;
}

function SetObjectValue(Object NewValue)
{
	ObjList[0] = NewValue;
}

defaultproperties
{
	ObjName="Object List"
	ObjCategory="Object"
	ObjColor=(R=102,G=0,B=102,A=255)		// dark purple
}
