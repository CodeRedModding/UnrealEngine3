class InterpTrackFade extends InterpTrackFloatBase
	native(Interpolation);

/** 
 * InterpTrackFade
 *
 * Special float property track that controls camera fading over time.
 * Should live in a Director group.
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
	
	virtual class UMaterial* GetTrackIcon() const;

	// InterpTrackFade interface
	FLOAT GetFadeAmountAtTime(FLOAT Time);
}

var() bool bPersistFade;

defaultproperties
{
	bOnePerGroup=true
	bDirGroupOnly=true
	TrackInstClass=class'Engine.InterpTrackInstFade'
	TrackTitle="Fade"
}
