/**
 * An ObjectVolume will replace the normal object references with anything contained within
 * the assigned volume at runtime, allowing designers to quickly reference large areas.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqVar_ObjectVolume extends SeqVar_Object
	native(Sequence);

cpptext
{
	virtual UObject** GetObjectRef(INT Idx);

#if WITH_EDITOR
	virtual void DrawExtraInfo(FCanvas* Canvas, const FVector& CircleCenter);
#endif

	virtual UBOOL SupportsProperty(UProperty *Property)
	{
		return FALSE;
	}
}

/** Last time ContainedObjects was updated */
var float LastUpdateTime;

/** List of objects contained within the volume */
var array<Object> ContainedObjects;

/** List of object types to exclude */
var() array<class<Object> > ExcludeClassList;

/** Should this volume account for non-colliding as well? */
var() bool bCollidingOnly;

defaultproperties
{
	ObjName="Object Volume"
	ObjCategory="Object"

	ExcludeClassList=(class'Trigger',class'Volume')

	bCollidingOnly=TRUE
}
