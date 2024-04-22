/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DirectionalLight extends Light
	native(Light)
	ClassGroup(Lights,DirectionalLights)
	placeable;

cpptext
{
public:
	/**
	 * This will determine which icon should be displayed for this light.
	 **/
	virtual void DetermineAndSetEditorIcon();

	/**
	 * Called from within SpawnActor, setting up the default value for the Lightmass light source angle.
	 */
	virtual void Spawned();

	virtual void PostLoad();
}


defaultproperties
{
	Begin Object Name=Sprite
		Sprite=Texture2D'EditorResources.LightIcons.Light_Directional_Stationary_DynamicsAndStatics'
	End Object
	
	Begin Object Class=DirectionalLightComponent Name=DirectionalLightComponent0
	    LightAffectsClassification=LAC_DYNAMIC_AND_STATIC_AFFECTING

	    CastShadows=TRUE
	    CastStaticShadows=TRUE
	    CastDynamicShadows=TRUE
	    bForceDynamicLight=FALSE
	    UseDirectLightMap=TRUE

	    LightingChannels=(BSP=TRUE,Static=TRUE,Dynamic=TRUE,bInitialized=TRUE)
	End Object
	LightComponent=DirectionalLightComponent0
	Components.Add(DirectionalLightComponent0)

	Begin Object Class=ArrowComponent Name=ArrowComponent0
		ArrowColor=(R=150,G=200,B=255)
		bTreatAsASprite=True
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Lighting"
	End Object
	Components.Add(ArrowComponent0)

	Rotation=(Pitch=-16384,Yaw=0,Roll=0)
	RotationRate=(Pitch=0, Yaw=0, Roll=0)
}
