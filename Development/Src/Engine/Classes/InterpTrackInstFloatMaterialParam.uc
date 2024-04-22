/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpTrackInstFloatMaterialParam extends InterpTrackInst
	native(Interpolation);

cpptext
{
	virtual void InitTrackInst(UInterpTrack* Track);
	virtual void TermTrackInst(UInterpTrack* Track);
	virtual void SaveActorState(UInterpTrack* Track);
	virtual void RestoreActorState(UInterpTrack* Track);
}

/** list of MICs we are using and optionally also the original value of the parameter we're editing
 * array size should match owner track's Materials array
 */
struct native FloatMaterialParamMICData
{
	/** MICs we're using to set the desired parameter on PrimitiveComponents - size of array should match track's AffectedMaterialRefs */
	var const array<MaterialInstanceConstant> MICs;
	/** saved values for restoring state when exiting Matinee - size of array should match MICs */
	var const array<float> MICResetFloats;
};
var array<FloatMaterialParamMICData> MICInfos;

/** track we are an instance of - used in the editor to propagate changes to the track's Materials array immediately */
var InterpTrackFloatMaterialParam InstancedTrack;
