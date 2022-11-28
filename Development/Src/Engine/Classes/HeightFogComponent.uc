class HeightFogComponent extends ActorComponent
	native
	collapsecategories
	hidecategories(Object)
	editinlinenew;

var float	Height;

var() float	Density,
			LightBrightness;
var() color	LightColor;

cpptext
{
	// UActorComponent interface.

	virtual void SetParentToWorld(const FMatrix& ParentToWorld);
	virtual void Created();
	virtual void Destroyed();
}

defaultproperties
{
	Density=0.00005
	LightBrightness=0.1
	LightColor=(R=255,G=255,B=255)
}