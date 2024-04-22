/**
 * Toggleable version of DirectionalLight.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DirectionalLightToggleable extends DirectionalLight
	native(Light)
	ClassGroup(Lights, DirectionalLights)
	placeable;


cpptext
{
public:
	/**
	 * This will determine which icon should be displayed for this light.
	 **/
	virtual void DetermineAndSetEditorIcon();

	/** 
	 * Static affecting Toggleables can't have UseDirectLightmaps=TRUE  So even tho they are not "free" 
	 * lightmapped data, they still are classified as static as it is the best they can be.
	 **/
	virtual void SetValuesForLight_StaticAffecting();

	/**
	 * Returns true if the light supports being toggled off and on on-the-fly
	 *
	 * @return For 'toggleable' lights, returns true
	 **/
	virtual UBOOL IsToggleable() const
	{
		// DirectionalLightToggleable supports being toggled on the fly!
		return TRUE;
	}
}


defaultproperties
{
	// Visual things should be ticked in parallel with physics
	TickGroup=TG_DuringAsyncWork

	Begin Object Name=Sprite
		Sprite=Texture2D'EditorResources.LightIcons.Light_Directional_Toggleable_DynamicsAndStatics'
	End Object

	// Light component.
	Begin Object Name=DirectionalLightComponent0
	    LightAffectsClassification=LAC_DYNAMIC_AND_STATIC_AFFECTING

	    CastShadows=TRUE
	    CastStaticShadows=TRUE
	    CastDynamicShadows=TRUE
	    bForceDynamicLight=FALSE
	    UseDirectLightMap=FALSE

	    LightingChannels=(BSP=TRUE,Static=TRUE,Dynamic=TRUE,bInitialized=TRUE)
        // By default indirect light from toggleable lights won't be put into lightmaps, since it can't be toggled in-game
        LightmassSettings=(IndirectLightingScale=0)
	End Object


	bMovable=FALSE
	bStatic=FALSE
	bHardAttach=TRUE
}
