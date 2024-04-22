/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


//=============================================================================
// GeomModifier_Edit: Maniupalating selected objects with the widget
//=============================================================================

class GeomModifier_Edit
	extends GeomModifier
	native;

cpptext
{
	/**
	 * @return		TRUE if the delta was handled by this editor mode tool.
	 */
	virtual UBOOL InputDelta(struct FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale);	
}

defaultproperties
{
	Description="Edit"
}
