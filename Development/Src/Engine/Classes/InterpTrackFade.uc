class InterpTrackFade extends InterpTrackFloatBase
	native(Interpolation);

/** 
 * InterpTrackFade
 *
 * Special float property track that controls camera fading over time.
 * Should live in a Director group.
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

	// InterpTrackFade interface
	FLOAT GetFadeAmountAtTime(FLOAT Time);
}

defaultproperties
{
	bOnePerGroup=true
	bDirGroupOnly=true
	TrackInstClass=class'Engine.InterpTrackInstFade'
	TrackTitle="Fade"
}