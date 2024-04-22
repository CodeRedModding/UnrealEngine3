/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpTrackSkelControlScale extends InterpTrackFloatBase
	native(Interpolation);

cpptext
{
	// InterpTrack interface
	virtual INT AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode);
	virtual void PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst);
	virtual void UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump);
}

/** Name of property in Group Actor which this track mill modify over time. */
var()	name	SkelControlName;

defaultproperties
{
	TrackInstClass=class'Engine.InterpTrackInstSkelControlScale'
	TrackTitle="SkelControl Scale"
	bIsAnimControlTrack=true
}
