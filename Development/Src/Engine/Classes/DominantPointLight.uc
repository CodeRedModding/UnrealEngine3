/**
 * Dominant version of PointLight that generates static shadowmaps.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DominantPointLight extends PointLight
	native(Light)
	ClassGroup(Lights, PointLights)
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
		Sprite=Texture2D'EditorResources.LightIcons.Light_Point_Toggleable_Statics'
	End Object

	// Light component.
	Begin Object Class=DominantPointLightComponent Name=DominantPointLightComponent0
	    LightAffectsClassification=LAC_DYNAMIC_AND_STATIC_AFFECTING
	    CastShadows=TRUE
	    CastStaticShadows=TRUE
	    CastDynamicShadows=TRUE
	    bForceDynamicLight=FALSE
	    UseDirectLightMap=FALSE
        bAllowPreShadow=TRUE
	    LightingChannels=(BSP=TRUE,Static=TRUE,Dynamic=TRUE,bInitialized=TRUE)
        PreviewLightRadius=DrawLightRadius0
		PreviewLightSourceRadius=DrawLightSourceRadius0
	End Object
    Components.Remove(PointLightComponent0)
    LightComponent=DominantPointLightComponent0
	Components.Add(DominantPointLightComponent0)

	bMovable=FALSE
	bStatic=FALSE
	bHardAttach=TRUE
}
