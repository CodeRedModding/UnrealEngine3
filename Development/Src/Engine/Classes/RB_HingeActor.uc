//=============================================================================
// The Hinge joint class.
//=============================================================================

class RB_HingeActor extends RB_ConstraintActor
    placeable;

defaultproperties
{
	Begin Object Class=RB_HingeSetup Name=MyHingeSetup
	End Object
	ConstraintSetup=MyHingeSetup

	
	Begin Object Name=Sprite
		Sprite=Texture2D'EngineResources.S_KHinge'
		HiddenGame=True
	End Object

	Begin Object Class=ArrowComponent Name=ArrowComponent0
		ArrowColor=(R=255,G=64,B=64)
	End Object
	Components.Add(ArrowComponent0)
}