/**
 *	LensFlare source actor class.
 *	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class LensFlareSource extends Actor
	native(LensFlare)
	placeable;

var()	editconst const	LensFlareComponent		LensFlareComp;

/** used to update status of toggleable level placed lens flares on clients */
var repnotify bool bCurrentlyActive;

replication
{
	if (bNoDelete)
		bCurrentlyActive;
}

cpptext
{
	void AutoPopulateInstanceProperties();

	// AActor interface.
	/**
	 * Function that gets called from within Map_Check to allow this actor to check itself
	 * for any potential errors and register them with map check dialog.
	 */
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif
}

//native noexport event SetTemplate(LensFlare NewTemplate);
native final function SetTemplate(LensFlare NewTemplate);

simulated event PostBeginPlay()
{
	bCurrentlyActive = LensFlareComp.bIsActive;
}

/**
 * Handling Toggle event from Kismet.
 */
simulated function OnToggle(SeqAct_Toggle action)
{
	// Turn ON
	if (action.InputLinks[0].bHasImpulse)
	{
		LensFlareComp.SetIsActive(TRUE);
		bCurrentlyActive = TRUE;
	}
	// Turn OFF
	else if (action.InputLinks[1].bHasImpulse)
	{
		LensFlareComp.SetIsActive(FALSE);
		bCurrentlyActive = FALSE;
	}
	// Toggle
	else if (action.InputLinks[2].bHasImpulse)
	{
		// If spawning is suppressed or we aren't turned on at all, activate.
		if (!bCurrentlyActive)
		{
			LensFlareComp.SetIsActive(TRUE);
			bCurrentlyActive = TRUE;
		}
		else
		{
			LensFlareComp.SetIsActive(FALSE);
			bCurrentlyActive = FALSE;
		}
	}
	LensFlareComp.LastRenderTime = WorldInfo.TimeSeconds;
	ForceNetRelevant();
}

simulated event ReplicatedEvent(name VarName)
{
	if (VarName == nameof(bCurrentlyActive))
	{
		LensFlareComp.SetIsActive(bCurrentlyActive);
		LensFlareComp.LastRenderTime = WorldInfo.TimeSeconds;
	}
}

simulated function SetFloatParameter(name ParameterName, float Param)
{
/***
	if (LensFlareComp != none)
	{
		LensFlareComp.SetFloatParameter(ParameterName, Param);
	}
	else
	{
		`log("Warning: Attempting to set a parameter on "$self$" when the PSC does not exist");
	}
***/
}

simulated function SetVectorParameter(name ParameterName, vector Param)
{
/***
	if (LensFlareComp != none)
	{
		LensFlareComp.SetVectorParameter(ParameterName, Param);
	}
	else
	{
		`log("Warning: Attempting to set a parameter on "$self$" when the PSC does not exist");
	}
***/
}

simulated function SetColorParameter(name ParameterName, linearcolor Param)
{
/***
	if (LensFlareComp != none)
	{
		LensFlareComp.SetColorParameter(ParameterName, Param);
	}
	else
	{
		`log("Warning: Attempting to set a parameter on "$self$" when the PSC does not exist");
	}
***/
}

simulated function SetExtColorParameter(name ParameterName, float Red, float Green, float Blue, float Alpha)
{
/***
	local linearcolor c;

	if (LensFlareComp != none)
	{
		c.r = Red;
		c.g = Green;
		c.b = Blue;
		c.a = Alpha;
		LensFlareComp.SetColorParameter(ParameterName, C);
	}
	else
	{
		`log("Warning: Attempting to set a parameter on "$self$" when the PSC does not exist");
	}
***/
}


simulated function SetActorParameter(name ParameterName, actor Param)
{
/***
	if (LensFlareComp != none)
	{
		LensFlareComp.SetActorParameter(ParameterName, Param);
	}
	else
	{
		`log("Warning: Attempting to set a parameter on "$self$" when the PSC does not exist");
	}
***/
}

/**
 * Kismet handler for setting particle instance parameters.
 */
/***
simulated function OnSetLensFlareParam(SeqAct_SetLensFlareParam Action)
{
	local int Idx, ParamIdx;
	if ((LensFlareComp != None) && (Action.InstanceParameters.Length > 0))
	{
		for (Idx = 0; Idx < Action.InstanceParameters.Length; Idx++)
		{
			if (Action.InstanceParameters[Idx].ParamType != PSPT_None)
			{
				// look for an existing entry
				ParamIdx = LensFlareComp.InstanceParameters.Find('Name',Action.InstanceParameters[Idx].Name);
				// create one if necessary
				if (ParamIdx == -1)
				{
					ParamIdx = LensFlareComp.InstanceParameters.Length;
					LensFlareComp.InstanceParameters.Length = ParamIdx + 1;
				}
				// update the instance parm
				LensFlareComp.InstanceParameters[ParamIdx] = Action.InstanceParameters[Idx];
				if (Action.bOverrideScalar)
				{
					LensFlareComp.InstanceParameters[ParamIdx].Scalar = Action.ScalarValue;
				}
			}
		}
	}
}
***/
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
		SpriteCategoryName="LensFlares"
	End Object
	Components.Add(Sprite)

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

	// Light radius visualization.
	Begin Object Class=DrawLightRadiusComponent Name=DrawRadius0
	End Object
	Components.Add(DrawRadius0)

	Begin Object Class=LensFlareComponent Name=LensFlareComponent0
		PreviewInnerCone=DrawInnerCone0
		PreviewOuterCone=DrawOuterCone0
		PreviewRadius=DrawRadius0
	End Object
	LensFlareComp=LensFlareComponent0
	Components.Add(LensFlareComponent0)

	Begin Object Class=ArrowComponent Name=ArrowComponent0
		ArrowColor=(R=0,G=255,B=128)
		ArrowSize=1.5
		bTreatAsASprite=True
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="LensFlares"
	End Object
	Components.Add(ArrowComponent0)

	bEdShouldSnap=true
	bHardAttach=true
	bGameRelevant=true
	bNoDelete=true
}
