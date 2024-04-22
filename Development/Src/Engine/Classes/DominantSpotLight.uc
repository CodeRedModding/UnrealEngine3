/**
 * Dominant version of SpotLight that generates static shadowmaps.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DominantSpotLight extends SpotLight
	native(Light)
	ClassGroup(Lights,SpotLights)
	placeable;

cpptext
{
	/**
	 * Returns true if the light supports being toggled off and on on-the-fly
	 **/
	virtual UBOOL IsToggleable() const
	{
		return TRUE;
	}
}

defaultproperties
{
	Begin Object Name=Sprite
		Sprite=Texture2D'EditorResources.LightIcons.Light_Spot_Toggleable_Statics'
	End Object

	// Light component.
	Begin Object Class=DominantSpotLightComponent Name=DominantSpotLightComponent0
	    LightAffectsClassification=LAC_DYNAMIC_AND_STATIC_AFFECTING
	    CastShadows=TRUE
	    CastStaticShadows=TRUE
	    CastDynamicShadows=TRUE
	    bForceDynamicLight=FALSE
	    UseDirectLightMap=FALSE
        bAllowPreShadow=TRUE
	    LightingChannels=(BSP=TRUE,Static=TRUE,Dynamic=TRUE,bInitialized=TRUE)
        PreviewLightRadius=DrawLightRadius0
		PreviewInnerCone=DrawInnerCone0
		PreviewOuterCone=DrawOuterCone0
		PreviewLightSourceRadius=DrawLightSourceRadius0
	End Object
    Components.Remove(SpotLightComponent0)
    LightComponent=DominantSpotLightComponent0
	Components.Add(DominantSpotLightComponent0)

	bMovable=FALSE
	bStatic=FALSE
	bHardAttach=TRUE
}
