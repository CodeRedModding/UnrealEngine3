//=============================================================================
// Emitter actor class.
//=============================================================================
class Emitter extends Actor
	native(Particle)
	placeable;

var()	editconst const	ParticleSystemComponent ParticleSystemComponent;
var						bool					bDestroyOnSystemFinish;

native final function	SetTemplate(ParticleSystem NewTemplate, optional bool bDestroyOnFinish);

event OnParticleSystemFinished(ParticleSystemComponent FinishedComponent)
{
	if(bDestroyOnSystemFinish)
	{
		Lifespan = 0.0001f;
	}
}

/**
 * Handling Toggle event from Kismet.
 */
simulated function OnToggle(SeqAct_Toggle action)
{
	// Turn ON
	if (action.InputLinks[0].bHasImpulse)
	{
		ParticleSystemComponent.ActivateSystem();
	}
	// Turn OFF
	else if (action.InputLinks[1].bHasImpulse)
	{
		ParticleSystemComponent.DeactivateSystem();
	}
	// Toggle
	else if (action.InputLinks[2].bHasImpulse)
	{
		// If spawning is suppressed or we aren't turned on at all, activate.
		if(ParticleSystemComponent.bSuppressSpawning || ParticleSystemComponent.EmitterInstances.Length == 0)
		{
			ParticleSystemComponent.ActivateSystem();
		}
		else
		{
			ParticleSystemComponent.DeactivateSystem();
		}
	}
}

cpptext
{
	void SetTemplate(UParticleSystem* NewTemplate, UBOOL bDestroyOnFinish=false);
}

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EngineResources.S_Emitter'
	End Object

	Begin Object Class=ParticleSystemComponent Name=ParticleSystemComponent0
	End Object
	ParticleSystemComponent=ParticleSystemComponent0
	Components.Add(ParticleSystemComponent0)

	Begin Object Class=ArrowComponent Name=ArrowComponent0
		ArrowColor=(R=0,G=255,B=128)
		ArrowSize=1.5
	End Object
	Components.Add(ArrowComponent0)

	bEdShouldSnap=true
}