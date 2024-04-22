/**
 * This is a version of the comment box which wraps the comment text within the box region.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SequenceFrameWrapped extends SequenceFrame
	native(Sequence);

cpptext
{
#if WITH_EDITOR
	virtual void DrawSeqObj(FCanvas* Canvas, UBOOL bSelected, UBOOL bMouseOver, INT MouseOverConnType, INT MouseOverConnIndex, FLOAT MouseOverTime);
#endif
}

defaultproperties
{
	ObjName="Sequence Comment Wrapped"
	bDrawBox=true
}