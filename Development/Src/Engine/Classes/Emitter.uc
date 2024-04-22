//=============================================================================
// Emitter actor class.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class Emitter extends Actor
	native
	placeable;

var()	editconst const	ParticleSystemComponent ParticleSystemComponent;
var()	editconst const DynamicLightEnvironmentComponent LightEnvironment;
var						bool					bDestroyOnSystemFinish;

var()					bool					bPostUpdateTickGroup;

/** used to update status of toggleable level placed emitters on clients */
var repnotify bool bCurrentlyActive;

struct CheckpointRecord
{
	var bool bIsActive;
};

replication
{
	if (bNoDelete)
		bCurrentlyActive;
}

cpptext
{
	void SetTemplate(UParticleSystem* NewTemplate, UBOOL bDestroyOnFinish=false);
	void AutoPopulateInstanceProperties();

	// AActor interface.
	virtual void Spawned();
	virtual void PostBeginPlay();
	/**
	 *	ticks the actor
	 *	@param	DeltaTime	The time slice of this tick
	 *	@param	TickType	The type of tick that is happening
	 *
	 *	@return	TRUE if the actor was ticked, FALSE if it was aborted (e.g. because it's in stasis)
	 */
	virtual UBOOL Tick( FLOAT DeltaTime, enum ELevelTick TickType );

	/**
	 * Function that gets called from within Map_Check to allow this actor to check itself
	 * for any potential errors and register them with map check dialog.
	 */
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif

#if USE_GAMEPLAY_PROFILER
    /** 
     * This function actually does the work for the GetProfilerAssetObject and is virtual.  
     * It should only be called from GetProfilerAssetObject as GetProfilerAssetObject is safe to call on NULL object pointers
     */
	virtual UObject* GetProfilerAssetObjectInternal() const;
#endif

	/**
	 * This will return detail info about this specific object. (e.g. AudioComponent will return the name of the cue,
	 * ParticleSystemComponent will return the name of the ParticleSystem)  The idea here is that in many places
	 * you have a component of interest but what you really want is some characteristic that you can use to track
	 * down where it came from.
	 *
	 */
	virtual FString GetDetailedInfoInternal() const;

	/**
	 *	Called to reset the emitter actor in the level.
	 *	Intended for use in editor only
	 */
	void ResetInLevel();
}

native noexport event SetTemplate(ParticleSystem NewTemplate, optional bool bDestroyOnFinish);

simulated event PostBeginPlay()
{
	Super.PostBeginPlay();

	// Let them die quickly on a dedicated server
 	if (WorldInfo.NetMode == NM_DedicatedServer && (RemoteRole == ROLE_None || bNetTemporary))
 	{
 		LifeSpan = 0.2;
 	}

	// Set Notification Delegate
	if (ParticleSystemComponent != None)
	{
		ParticleSystemComponent.OnSystemFinished = OnParticleSystemFinished;
		bCurrentlyActive = ParticleSystemComponent.bAutoActivate;
	}
}

simulated event ReplicatedEvent(name VarName)
{
	if (VarName == 'bCurrentlyActive')
	{
		ParticleSystemComponent.SetActive(bCurrentlyActive);
	}
	else
	{
		Super.ReplicatedEvent(VarName);
	}
}

simulated function OnParticleSystemFinished(ParticleSystemComponent FinishedComponent)
{
	if (bDestroyOnSystemFinish)
	{
		Lifespan = 0.0001f;
	}
	bCurrentlyActive = false;
}

/**
 * Handling Toggle event from Kismet.
 */
function OnToggle(SeqAct_Toggle action)
{
	// Turn ON
	if (action.InputLinks[0].bHasImpulse)
	{
		ParticleSystemComponent.ActivateSystem();
		bCurrentlyActive = TRUE;
	}
	// Turn OFF
	else if (action.InputLinks[1].bHasImpulse)
	{
		ParticleSystemComponent.DeactivateSystem();
		bCurrentlyActive = FALSE;
	}
	// Toggle
	else if (action.InputLinks[2].bHasImpulse)
	{
		// If spawning is suppressed or we aren't turned on at all, activate.
		if (ParticleSystemComponent.bSuppressSpawning || !bCurrentlyActive)
		{
			ParticleSystemComponent.ActivateSystem();
			bCurrentlyActive = TRUE;
		}
		else
		{
			ParticleSystemComponent.DeactivateSystem();
			bCurrentlyActive = FALSE;
		}
	}
	ParticleSystemComponent.LastRenderTime = WorldInfo.TimeSeconds;
	ForceNetRelevant();

	if (RemoteRole != ROLE_None)
	{
		// force replicate flag if necessary
		SetForcedInitialReplicatedProperty(Property'Engine.Emitter.bCurrentlyActive', (bCurrentlyActive == default.bCurrentlyActive));
	}
}

/**
 * Handling ParticleEventGenerator event from Kismet.
 * - Does nothing... just here to stop Kismet from complaining
 */
function OnParticleEventGenerator(SeqAct_ParticleEventGenerator action)
{
}

simulated function ShutDown()
{
	Super.ShutDown();

	bCurrentlyActive = false;
}

simulated function SetFloatParameter(name ParameterName, float Param)
{
	if (ParticleSystemComponent != none)
		ParticleSystemComponent.SetFloatParameter(ParameterName, Param);
	else
		`log("Warning: Attempting to set a parameter on "$self$" when the PSC does not exist");
}

simulated function SetVectorParameter(name ParameterName, vector Param)
{
	if (ParticleSystemComponent != none)
		ParticleSystemComponent.SetVectorParameter(ParameterName, Param);
	else
		`log("Warning: Attempting to set a parameter on "$self$" when the PSC does not exist");
}

simulated function SetColorParameter(name ParameterName, color Param)
{
	if (ParticleSystemComponent != none)
		ParticleSystemComponent.SetColorParameter(ParameterName, Param);
	else
		`log("Warning: Attempting to set a parameter on "$self$" when the PSC does not exist");
}

simulated function SetExtColorParameter(name ParameterName, byte Red, byte Green, byte Blue, byte Alpha)
{
	local color c;

	if (ParticleSystemComponent != none)
	{
		c.r = Red;
		c.g = Green;
		c.b = Blue;
		c.a = Alpha;
		ParticleSystemComponent.SetColorParameter(ParameterName, C);
	}
	else
		`log("Warning: Attempting to set a parameter on "$self$" when the PSC does not exist");
}


simulated function SetActorParameter(name ParameterName, actor Param)
{
	if (ParticleSystemComponent != none)
		ParticleSystemComponent.SetActorParameter(ParameterName, Param);
	else
		`log("Warning: Attempting to set a parameter on "$self$" when the PSC does not exist");
}

/**
 * Kismet handler for setting particle instance parameters.
 */
simulated function OnSetParticleSysParam(SeqAct_SetParticleSysParam Action)
{
	local int Idx, ParamIdx;
	if (ParticleSystemComponent != None &&
		Action.InstanceParameters.Length > 0)
	{
		for (Idx = 0; Idx < Action.InstanceParameters.Length; Idx++)
		{
			if (Action.InstanceParameters[Idx].ParamType != PSPT_None)
			{
				// look for an existing entry
				ParamIdx = ParticleSystemComponent.InstanceParameters.Find('Name',Action.InstanceParameters[Idx].Name);
				// create one if necessary
				if (ParamIdx == -1)
				{
					ParamIdx = ParticleSystemComponent.InstanceParameters.Length;
					ParticleSystemComponent.InstanceParameters.Length = ParamIdx + 1;
				}
				// update the instance parm
				ParticleSystemComponent.InstanceParameters[ParamIdx] = Action.InstanceParameters[Idx];
				if (Action.bOverrideScalar)
				{
					ParticleSystemComponent.InstanceParameters[ParamIdx].Scalar = Action.ScalarValue;
				}
			}
		}
	}
}

function bool ShouldSaveForCheckpoint()
{
	return (bNoDelete && RemoteRole != ROLE_None);
}

function CreateCheckpointRecord(out CheckpointRecord Record)
{
	Record.bIsActive = bCurrentlyActive;
}

function ApplyCheckpointRecord(const out CheckpointRecord Record)
{
	bCurrentlyActive = Record.bIsActive;
	if (bCurrentlyActive)
	{
		ParticleSystemComponent.ActivateSystem();
	}
	else
	{
		ParticleSystemComponent.DeactivateSystem();
	}
	ForceNetRelevant();
}

/** Function used to have the emitter hide itself and put itself into stasis **/
simulated function HideSelf();


defaultproperties
{
	// Visual things should be ticked in parallel with physics
	TickGroup=TG_DuringAsyncWork

	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.S_Emitter'
		HiddenGame=True
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		bIsScreenSizeScaled=True
		ScreenSize=0.0025
		SpriteCategoryName="Effects"
	End Object
	Components.Add(Sprite)

	Begin Object Class=ParticleSystemComponent Name=ParticleSystemComponent0
		SecondsBeforeInactive=1
	End Object
	ParticleSystemComponent=ParticleSystemComponent0
	Components.Add(ParticleSystemComponent0)

	Begin Object Class=ArrowComponent Name=ArrowComponent0
		ArrowColor=(R=0,G=255,B=128)
		ArrowSize=1.5
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		bTreatAsASprite=True
		SpriteCategoryName="Effects"
	End Object
	Components.Add(ArrowComponent0)

	bEdShouldSnap=true
	bHardAttach=true
	bGameRelevant=true
	bNoDelete=true

	SupportedEvents.Add(class'SeqEvent_ParticleEvent')
}
