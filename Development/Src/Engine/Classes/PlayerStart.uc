//=============================================================================
// Player start location.
//=============================================================================
class PlayerStart extends NavigationPoint 
	placeable
	native;

cpptext
{
	void addReachSpecs(AScout *Scout, UBOOL bOnlyChanged=0);
}

// Players on different teams are not spawned in areas with the
// same TeamNumber unless there are more teams in the level than
// team numbers.
var() byte TeamNumber;			// what team can spawn at this start
var() bool bSinglePlayerStart;	// use first start encountered with this true for single player
var() bool bCoopStart;			// start can be used in coop games	
var() bool bEnabled; 
var() bool bPrimaryStart;		// None primary starts used only if no primary start available
var() float LastSpawnCampTime;	// last time a pawn starting from this spot died within 5 seconds

/* epic ===============================================	
* ::OnToggle
*
* Scripted support for toggling a playerstart, checks which
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
		bEnabled = true;
	}
	else
	if (action.InputLinks[1].bHasImpulse)
	{
		// turn off
		bEnabled = false;
	}
	else
	if (action.InputLinks[2].bHasImpulse)
	{
		// toggle
		bEnabled = !bEnabled;
	}
}


defaultproperties
{
	Begin Object NAME=CollisionCylinder
		CollisionRadius=+00040.000000
		CollisionHeight=+00080.000000
	End Object

	Begin Object NAME=Sprite LegacyClassName=PlayerStart_PlayerStartSprite_Class
		Sprite=Texture2D'EngineResources.S_Player'
	End Object

	bPrimaryStart=true
 	bEnabled=true
    bSinglePlayerStart=True
    bCoopStart=True

	Begin Object Class=ArrowComponent Name=ArrowComponent0
		HiddenGame=true
	End Object
	Components.Add(ArrowComponent0)
}
