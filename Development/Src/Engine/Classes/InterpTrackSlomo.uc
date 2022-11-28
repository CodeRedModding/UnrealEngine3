class InterpTrackSlomo extends InterpTrackFloatBase
	native(Interpolation);

/** 
 * InterpTrackSlomo
 *
 * Created by:	James Golding
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

cpptext
{
	// InterpTrack interface
	virtual INT AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst);
	virtual void UpdateKeyframe(INT KeyIndex, UInterpTrackInst* TrInst);
	virtual void PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst);
	virtual void UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump=false);
	virtual void SetTrackToSensibleDefault();

	// InterpTrackSlomo interface
	FLOAT GetSlomoFactorAtTime(FLOAT Time);
}

defaultproperties
{
	bOnePerGroup=true
	bDirGroupOnly=true
	TrackInstClass=class'Engine.InterpTrackInstSlomo'
	TrackTitle="Slomo"
}