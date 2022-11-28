class InterpTrackFloatProp extends InterpTrackFloatBase
	native(Interpolation);

/** 
 * InterpTrackFloatProp
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
}

/** Name of property in Group Actor which this track mill modify over time. */
var		name				PropertyName;

defaultproperties
{
	TrackInstClass=class'Engine.InterpTrackInstFloatProp'
	TrackTitle="Float Property"
}