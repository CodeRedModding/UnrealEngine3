/**
 * Toggleable version of PointLight.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class PointLightToggleable extends PointLight
	native(Light)
	ClassGroup(Lights,PointLights)
	placeable;

struct CheckpointRecord
{
	var bool bEnabled;
};

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
		// PointLightToggleable supports being toggled on the fly!
		return TRUE;
	}
}

function bool ShouldSaveForCheckpoint()
{
	return (RemoteRole != ROLE_None);
}

function CreateCheckpointRecord(out CheckpointRecord Record)
{
	Record.bEnabled = bEnabled;
}

function ApplyCheckpointRecord(const out CheckpointRecord Record)
{
	bEnabled = Record.bEnabled;
	LightComponent.SetEnabled(bEnabled);
	ForceNetRelevant();
}

defaultproperties
{
	// Visual things should be ticked in parallel with physics
	TickGroup=TG_DuringAsyncWork

	Begin Object Name=Sprite
		Sprite=Texture2D'EditorResources.LightIcons.Light_Point_Toggleable_Statics'
	End Object

	// Light component.
	Begin Object Name=PointLightComponent0
	    LightAffectsClassification=LAC_STATIC_AFFECTING
	    CastShadows=TRUE
	    CastStaticShadows=TRUE
	    CastDynamicShadows=FALSE
	    bForceDynamicLight=FALSE
	    UseDirectLightMap=FALSE
	    LightingChannels=(BSP=TRUE,Static=TRUE,Dynamic=FALSE,bInitialized=TRUE)
        // By default indirect light from toggleable lights won't be put into lightmaps, since it can't be toggled in-game
        LightmassSettings=(IndirectLightingScale=0)
	End Object


	bMovable=FALSE
	bStatic=FALSE
	bHardAttach=TRUE
}
