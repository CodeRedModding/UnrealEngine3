/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Revolves selected polygons around the pivot point.
 */
class GeomModifier_Lathe
	extends GeomModifier_Edit
	native;
	
var(Settings)	int		TotalSegments;
var(Settings)	int		Segments;
var(Settings)	bool	AlignToSide;

/** The axis of rotation to use when creating the brush.  This is automatically determined from the current ortho viewport. */
var				EAxis	Axis;

cpptext
{
	/**
	 * @return		TRUE if this modifier will work on the currently selected sub objects.
	 */
	virtual UBOOL Supports();

	/**
	 * Gives the individual modifiers a chance to do something the first time they are activated.
	 */
	virtual void Initialize();

protected:
	/**
	 * Implements the modifier application.
	 */
 	virtual UBOOL OnApply();
 	
private:
 	void Apply( INT InTotalSegments, INT InSegments, EAxis InAxis );
}
	
defaultproperties
{
	Description="Lathe"
	Axis=AXIS_Y
	TotalSegments=16
	Segments=4
	AlignToSide=FALSE
}
