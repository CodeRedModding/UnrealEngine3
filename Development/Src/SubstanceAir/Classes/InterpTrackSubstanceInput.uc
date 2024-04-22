//! @file InterpTrackSubstanceInput.uc
//! @copyright Allegorithmic. All rights reserved.
//!

class InterpTrackSubstanceInput extends InterpTrackLinearColorBase
	native(Interpolation);

cpptext
{
	// InterpTrack interface
	virtual INT AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode);
	virtual void PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst);
	virtual void UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump);
	
	virtual void PreEditChange(UProperty* PropertyThatWillChange);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	INT		GetNumSubCurves() const;
}

/** Name of parameter in the GraphInstance which will be modified over time by this track. */
var() name ParamName;
var int NumSubcurve;

defaultproperties
{
	TrackInstClass=class'SubstanceAir.InterpTrackInstSubstanceInput'
	TrackTitle="Substance Graph Instance Input"
}
