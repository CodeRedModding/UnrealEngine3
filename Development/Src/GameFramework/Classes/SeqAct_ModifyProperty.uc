/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


class SeqAct_ModifyProperty extends SequenceAction
	native;

cpptext
{
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif
	virtual void Activated();
};

/**
 * Struct used to figure out which properties to modify.
 */
struct native PropertyInfo
{
	/** Name of the property to modify */
	var() Name PropertyName;

	/** Should this property be modified? */
	var() bool bModifyProperty;

	/** New value to apply to the property */
	var() string PropertyValue;
};

/** List of properties that can be modified */
var() editinline array<PropertyInfo> Properties;

defaultproperties
{
	ObjName="Modify Property"
	ObjCategory="Object Property"
}
