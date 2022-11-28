/**
 * An actor used to generate collision events (touch/untouch), and
 * interactions events (ue) as inputs into the scripting system.
 */
class Trigger extends Actor
	placeable
	native;

/** Base cylinder component for collision */
var() editconst const CylinderComponent	CylinderComponent;

defaultproperties
{
	Begin Object NAME=Sprite LegacyClassName=Trigger_TriggerSprite_Class
		Sprite=Texture2D'EngineResources.S_Trigger'
		HiddenGame=False
	End Object

	Begin Object NAME=CollisionCylinder LegacyClassName=Trigger_TriggerCylinderComponent_Class
		CollideActors=true
		CollisionRadius=+0040.000000
		CollisionHeight=+0040.000000
	End Object
	CylinderComponent=CollisionCylinder

	bHidden=true
	bCollideActors=true
	bStatic=true

	SupportedEvents.Add(class'SeqEvent_Used')
}
