/*=============================================================================
	ABrush.h.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Constructors.
ABrush() {}

// UObject interface.
void PostLoad();

void PostEditChange(UProperty* PropertyThatChanged)
{
	Super::PostEditChange(PropertyThatChanged);

	check(BrushComponent);

	if(Brush)
		Brush->BuildBound();
}

virtual UBOOL IsABrush() {return true;}

// ABrush interface.
virtual void CopyPosRotScaleFrom( ABrush* Other )
{
	check(BrushComponent);
	check(Brush);
	check(Other);
	check(Other->BrushComponent);
	check(Other->Brush);

	Location    = Other->Location;
	Rotation    = Other->Rotation;
	PrePivot	= Other->PrePivot;

	Brush->BuildBound();
	ClearComponents();
	UpdateComponents();

}
virtual void InitPosRotScale();

void CheckForErrors();

/**
 * Figures out the best color to use for this brushes wireframe drawing.
 */

virtual FColor GetWireColor();

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

