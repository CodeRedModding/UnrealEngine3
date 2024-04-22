/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class PointLight extends Light
	native(Light)
	ClassGroup(Lights,PointLights)
	placeable;

cpptext
{
#if WITH_EDITOR
	// AActor interface.
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
	/**
	 * This will determine which icon should be displayed for this light.
	 **/
	virtual void DetermineAndSetEditorIcon();

	/**
	 * Called after this actor has been pasted into a level.  Attempts to catch cases where designers are pasting in really old
	 * T3D data that was created when component instancing wasn't working correctly.
	 */
	virtual void PostEditImport();

	virtual void PostLoad();

	/**
	 * Called from within SpawnActor, setting up the default value for the Lightmass light source radius.
	 */
	virtual void Spawned();
}


defaultproperties
{
	Begin Object Name=Sprite
		Sprite=Texture2D'EditorResources.LightIcons.Light_Point_Stationary_Statics'
	End Object

	Begin Object Class=DrawLightRadiusComponent Name=DrawLightRadius0
	End Object
	Components.Add(DrawLightRadius0)

	Begin Object Class=DrawLightRadiusComponent Name=DrawLightSourceRadius0
		SphereColor=(R=231,G=239,B=0,A=255)
	End Object
	Components.Add(DrawLightSourceRadius0)

	Begin Object Class=PointLightComponent Name=PointLightComponent0
	    LightAffectsClassification=LAC_STATIC_AFFECTING
		CastShadows=TRUE
		CastStaticShadows=TRUE
		CastDynamicShadows=FALSE
		bForceDynamicLight=FALSE
		UseDirectLightMap=TRUE
		LightingChannels=(BSP=TRUE,Static=TRUE,Dynamic=FALSE,bInitialized=TRUE)
		PreviewLightRadius=DrawLightRadius0
		PreviewLightSourceRadius=DrawLightSourceRadius0
	End Object
	LightComponent=PointLightComponent0
	Components.Add(PointLightComponent0)
}
