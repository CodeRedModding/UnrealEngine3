/**
 * Movable version of PointLight.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class PointLightMovable extends PointLight
	native(Light)
	ClassGroup(Lights, PointLights)
	placeable;

cpptext
{
public:
	/**
	 * This will determine which icon should be displayed for this light.
	 **/
	virtual void DetermineAndSetEditorIcon();

	/**
	 * Returns true if the light supports being toggled off and on on-the-fly
	 *
	 * @return For 'toggleable' lights, returns true
	 **/
	virtual UBOOL IsToggleable() const
	{
		// PointLightMovable supports being toggled on the fly!
		return TRUE;
	}
}


defaultproperties
{
	// Visual things should be ticked in parallel with physics
	TickGroup=TG_DuringAsyncWork

	Begin Object Name=Sprite
		Sprite=Texture2D'EditorResources.LightIcons.Light_Point_Moveable_DynamicsAndStatics'
	End Object

	// Light component.
	Begin Object Name=PointLightComponent0
	    LightAffectsClassification=LAC_DYNAMIC_AND_STATIC_AFFECTING

	    CastShadows=TRUE
	    CastStaticShadows=TRUE
	    CastDynamicShadows=TRUE
	    bForceDynamicLight=FALSE
	    UseDirectLightMap=FALSE

	    LightingChannels=(BSP=TRUE,Static=TRUE,Dynamic=TRUE,bInitialized=TRUE)
	End Object


	bMovable=TRUE
	bStatic=FALSE
	bHardAttach=TRUE
}
