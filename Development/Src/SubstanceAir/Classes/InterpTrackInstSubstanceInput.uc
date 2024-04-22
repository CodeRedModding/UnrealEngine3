//! @file InterpTrackInstSubstanceInput.uc
//! @copyright Allegorithmic. All rights reserved.
//!

class InterpTrackInstSubstanceInput extends InterpTrackInst
	native(Interpolation);

cpptext
{
	virtual void InitTrackInst(UInterpTrack* Track);
	virtual void SaveActorState(UInterpTrack* Track);
	virtual void RestoreActorState(UInterpTrack* Track);
}

/** Saved value for restoring state when exiting Matinee. */
var	array<int> ResetValue;

var InterpTrackSubstanceInput InstancedTrack;