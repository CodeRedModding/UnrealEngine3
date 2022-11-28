class RB_RadialImpulseActor extends Actor
	placeable;

/** 
 *	Encapsulates a RB_RadialImpulseComponent to let a level designer place one in a level.
 *	When toggled from Kismet, will apply a kick to surrounding physics objects within blast radius.
 *	@see AddRadialImpulse
 */

var						DrawSphereComponent			RenderComponent;
var() const editconst	RB_RadialImpulseComponent	ImpulseComponent;

/** Handling Toggle event from Kismet. */
simulated function onToggle(SeqAct_Toggle inAction)
{
	if (inAction.InputLinks[0].bHasImpulse)
	{
		ImpulseComponent.FireImpulse();
	}
}

defaultproperties
{
	Begin Object Class=DrawSphereComponent Name=DrawSphere0
	End Object
	RenderComponent=DrawSphere0
	Components.Add(DrawSphere0)

	Begin Object Class=RB_RadialImpulseComponent Name=ImpulseComponent0
		PreviewSphere=DrawSphere0
	End Object
	ImpulseComponent=ImpulseComponent0
	Components.Add(ImpulseComponent0)

	Begin Object Name=Sprite 
		Sprite=Texture2D'EngineResources.S_RadImpulse'
	End Object

	bEdShouldSnap=true
}