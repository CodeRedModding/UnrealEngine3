//=============================================================================
// A sticky note.  Level designers can place these in the level and then
// view them as a batch in the error/warnings window.
//=============================================================================

class Note extends Actor
	placeable
	native;

var() string Text;

cpptext
{
	void CheckForErrors();
}

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EngineResources.S_Note'
	End Object

	bStatic=true
	bHidden=true
	bNoDelete=true
	bMovable=false
}
