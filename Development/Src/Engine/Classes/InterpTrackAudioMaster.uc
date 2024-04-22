/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpTrackAudioMaster extends InterpTrackVectorBase
	native(Interpolation);

cpptext
{
	// InterpTrack interface
	virtual INT AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode);
	virtual void UpdateKeyframe(INT KeyIndex, UInterpTrackInst* TrInst);
	virtual void PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst);
	virtual void UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump);
	virtual void SetTrackToSensibleDefault();

	/** Return the sound volume scale for the specified time */
	FLOAT GetVolumeScaleForTime( FLOAT Time ) const;

	/** Return the sound pitch scale for the specified time */
	FLOAT GetPitchScaleForTime( FLOAT Time ) const;

	virtual class UMaterial* GetTrackIcon() const;
}

defaultproperties
{
	bOnePerGroup=true
	bDirGroupOnly=true
	TrackInstClass=class'Engine.InterpTrackInstAudioMaster'
	TrackTitle="Audio Master"
}
