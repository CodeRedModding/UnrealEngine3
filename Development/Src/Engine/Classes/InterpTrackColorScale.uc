class InterpTrackColorScale extends InterpTrackVectorBase
	native(Interpolation);

/** 
 * InterpTrackColorScale
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

cpptext
{
	// InterpTrack interface
	virtual INT AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode);
	virtual void UpdateKeyframe(INT KeyIndex, UInterpTrackInst* TrInst);
	virtual void PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst);
	virtual void UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump);
	virtual void SetTrackToSensibleDefault();

	virtual class UMaterial* GetTrackIcon() const;

	// InterpTrackColorScale interface
	FVector GetColorScaleAtTime(FLOAT Time);
}

defaultproperties
{
	bOnePerGroup=true
	bDirGroupOnly=true
	TrackInstClass=class'Engine.InterpTrackInstColorScale'
	TrackTitle="Color Scale"
}
