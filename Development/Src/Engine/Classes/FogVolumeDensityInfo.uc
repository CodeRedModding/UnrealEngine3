/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class FogVolumeDensityInfo extends Info
	showcategories(Movement)
	AutoExpandCategories(FogVolumeDensityInfo)
	ClassGroup(Fog)
	native(FogVolume)
	abstract;

/** The fog component which stores data specific to each density function. */
var() FogVolumeDensityComponent	DensityComponent;

/**
 * The automatic mesh component, which sizes with this fog volume actor.  This mesh component gets rendered with FogMaterial.
 * It is optional, and individual actors can be specified using the FogVolumeActors array instead.
 */
var() StaticMeshComponent AutomaticMeshComponent;

/** replicated copy of HeightFogComponent's bEnabled property */
var repnotify bool bEnabled;

struct CheckpointRecord
{
	var bool bEnabled;
};

replication
{
	if (Role == ROLE_Authority)
		bEnabled;
}

cpptext
{
public:
	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();
	virtual void PostEditImport();
	virtual void SetupDefaultFogVolume();
}

event PostBeginPlay()
{
	Super.PostBeginPlay();

	if( DensityComponent != none )
	{
		bEnabled = DensityComponent.bEnabled;
	}
}

simulated event ReplicatedEvent(name VarName)
{
	if (VarName == 'bEnabled')
	{
		DensityComponent.SetEnabled(bEnabled);
	}
	else
	{
		Super.ReplicatedEvent(VarName);
	}
}

/* epic ===============================================
* ::OnToggle
*
* Scripted support for toggling height fog, checks which
* operation to perform by looking at the action input.
*
* Input 1: turn on
* Input 2: turn off
* Input 3: toggle
*
* =====================================================
*/
simulated function OnToggle(SeqAct_Toggle action)
{
	if (action.InputLinks[0].bHasImpulse)
	{
		// turn on
		DensityComponent.SetEnabled(TRUE);
	}
	else if (action.InputLinks[1].bHasImpulse)
	{
		// turn off
		DensityComponent.SetEnabled(FALSE);
	}
	else if (action.InputLinks[2].bHasImpulse)
	{
		// toggle
		DensityComponent.SetEnabled(!DensityComponent.bEnabled);
	}
	bEnabled = DensityComponent.bEnabled;
	ForceNetRelevant();
	SetForcedInitialReplicatedProperty(Property'Engine.FogVolumeDensityInfo.bEnabled', (bEnabled == default.bEnabled));
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
	DensityComponent.SetEnabled(bEnabled);
	ForceNetRelevant();
	SetForcedInitialReplicatedProperty(Property'Engine.FogVolumeDensityInfo.bEnabled', (bEnabled == default.bEnabled));
}

defaultproperties
{
	bStatic=FALSE
	bNoDelete=true

	Begin Object Class=StaticMeshComponent Name=AutomaticMeshComponent0
		StaticMesh=StaticMesh'EngineMeshes.Cube'
		bCastDynamicShadow=FALSE
		BlockRigidBody=FALSE
		bForceDirectLightMap=FALSE
		bAcceptsDynamicLights=FALSE
		bAcceptsLights=FALSE
		CastShadow=FALSE
		bUsePrecomputedShadows=FALSE
		bAcceptsStaticDecals=FALSE
		bAcceptsDynamicDecals=FALSE
		bUseAsOccluder=FALSE
		bSelectable=FALSE
		bIgnoreOwnerHidden=TRUE
		WireframeColor=(R=100,G=100,B=200,A=255)
	End Object

	AutomaticMeshComponent=AutomaticMeshComponent0
	Components.Add(AutomaticMeshComponent0)
	
	Begin Object Name=Sprite
		SpriteCategoryName="Fog"
	End Object
}
