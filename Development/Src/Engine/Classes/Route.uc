class Route extends Actor
	native;

enum ERouteType
{
	/** Move from beginning to end, then stop */
	ERT_Linear,
	/** Move from beginning to end and then reverse */
	ERT_Loop,
	/** Move from beginning to end, then start at beginning again */
	ERT_Circle,
};
var() ERouteType RouteType;

/** List of move targets in order */
var() array<NavigationPoint> MoveList;

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EngineResources.S_Route'
		HiddenGame=True
	End Object
}
