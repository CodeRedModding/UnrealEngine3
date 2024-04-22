/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class FogVolumeConeDensityInfo extends FogVolumeDensityInfo
	showcategories(Movement)
	native(FogVolume)
	abstract;

cpptext
{
#if WITH_EDITOR
	// AActor interface.
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
}

defaultproperties
{
	Begin Object Class=DrawLightConeComponent Name=DrawCone0
		ConeColor=(R=200,G=255,B=255)
	End Object
	Components.Add(DrawCone0)

	Begin Object Class=FogVolumeConeDensityComponent Name=FogVolumeComponent0
		PreviewCone=DrawCone0
	End Object
	DensityComponent=FogVolumeComponent0
	Components.Add(FogVolumeComponent0)
}
