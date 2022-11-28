//=============================================================================
// Abstract light class.
//=============================================================================
class Light extends Actor;

var() editconst const LightComponent	LightComponent;

/* epic ===============================================	
* ::OnToggle
*
* Scripted support for toggling a light, checks which
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
		LightComponent.bEnabled = true;
	}
	else
	if (action.InputLinks[1].bHasImpulse)
	{
		// turn off
		LightComponent.bEnabled = false;
	}
	else
	if (action.InputLinks[2].bHasImpulse)
	{
		// toggle
		LightComponent.bEnabled = !LightComponent.bEnabled;
	}
}

defaultproperties
{
	Begin Object Name=Sprite LegacyClassName=Light_LightSprite_Class
		Sprite=Texture2D'EngineResources.S_Light'
	End Object

	bStatic=true
	bHidden=true
	bNoDelete=true
	bMovable=false
}
