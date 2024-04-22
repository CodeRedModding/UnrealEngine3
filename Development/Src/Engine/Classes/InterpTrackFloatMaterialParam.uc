class InterpTrackFloatMaterialParam extends InterpTrackFloatBase
	native(Interpolation);

/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

cpptext
{
	virtual void PreSave();
	virtual void PostLoad();
	virtual void PreEditChange(UProperty* PropertyThatWillChange);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PostDuplicate();

	// InterpTrack interface
	virtual INT AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode);
	virtual void PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst);
	virtual void UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump);

	//virtual class UMaterial* GetTrackIcon() const;
}

/** materials whose parameters we want to change and the references to those materials
 * that need to be given MICs in the same level, compiled at save time
 */
var() const array<MaterialReferenceList> Materials;
var deprecated const MaterialInterface Material;
/** Name of parameter in the MaterialInstance which this track will modify over time. */
var() name ParamName;

/** @compatibility: indicates we need to gather material references on first use
 * (can't do in PostLoad() because Actors initialize components array in their own PostLoad() which might not have been called yet)
 */
var transient bool bNeedsMaterialRefsUpdate;

defaultproperties
{
	TrackInstClass=class'Engine.InterpTrackInstFloatMaterialParam'
	TrackTitle="Float Material Param"
}
