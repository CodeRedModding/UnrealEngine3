/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SpotLight extends Light
	native(Light)
	ClassGroup(Lights,SpotLights)
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
		Sprite=Texture2D'EditorResources.LightIcons.Light_Spot_Stationary_Statics'
	End Object

	// Light radius visualization.
	Begin Object Class=DrawLightRadiusComponent Name=DrawLightRadius0
	End Object
	Components.Add(DrawLightRadius0)

	// Inner cone visualization.
	Begin Object Class=DrawLightConeComponent Name=DrawInnerCone0
		ConeColor=(R=150,G=200,B=255)
	End Object
	Components.Add(DrawInnerCone0)

	// Outer cone visualization.
	Begin Object Class=DrawLightConeComponent Name=DrawOuterCone0
		ConeColor=(R=200,G=255,B=255)
	End Object
	Components.Add(DrawOuterCone0)

	Begin Object Class=DrawLightRadiusComponent Name=DrawLightSourceRadius0
		SphereColor=(R=231,G=239,B=0,A=255)
	End Object
	Components.Add(DrawLightSourceRadius0)

	// Light component.
	Begin Object Class=SpotLightComponent Name=SpotLightComponent0
	    LightAffectsClassification=LAC_STATIC_AFFECTING
		CastShadows=TRUE
		CastStaticShadows=TRUE
		CastDynamicShadows=FALSE
		bForceDynamicLight=FALSE
		UseDirectLightMap=TRUE
		LightingChannels=(BSP=TRUE,Static=TRUE,Dynamic=FALSE,bInitialized=TRUE)
	    PreviewLightRadius=DrawLightRadius0
		PreviewInnerCone=DrawInnerCone0
		PreviewOuterCone=DrawOuterCone0
		PreviewLightSourceRadius=DrawLightSourceRadius0
	End Object
	LightComponent=SpotLightComponent0
	Components.Add(SpotLightComponent0)

	Begin Object Class=ArrowComponent Name=ArrowComponent0
		ArrowColor=(R=150,G=200,B=255)
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		bTreatAsASprite=True
		SpriteCategoryName="Lighting"
	End Object
	Components.Add(ArrowComponent0)

	Rotation=(Pitch=-16384,Yaw=0,Roll=0)

}
