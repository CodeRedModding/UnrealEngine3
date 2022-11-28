class WarScout extends Scout
	native;

cpptext
{
	virtual void InitForPathing()
	{
		Super::InitForPathing();
		bJumpCapable = 0;
		bCanJump = 0;
	}
};

defaultproperties
{
	Begin Object NAME=CollisionCylinder
		CollisionRadius=+0044.000000
	End Object

	PathSizes.Empty
	PathSizes(0)=(Desc=Human,Radius=48,Height=80)
}