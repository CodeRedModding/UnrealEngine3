//=============================================================================
// The Prismatic joint class.
//=============================================================================

class RB_PrismaticActor extends RB_ConstraintActor
    placeable;

defaultproperties
{
	Begin Object Class=RB_PrismaticSetup Name=MyPrismaticSetup
	End Object
	ConstraintSetup=MyPrismaticSetup

	Begin Object Name=Sprite
		Sprite=Texture2D'EngineResources.S_KPrismatic'
		HiddenGame=True
	End Object

	Begin Object Class=ArrowComponent Name=ArrowComponent0
		ArrowColor=(R=255,G=64,B=64)
	End Object
	Components.Add(ArrowComponent0)
}