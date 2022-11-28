class LightComponent extends ActorComponent
	native
	noexport
	abstract
	collapsecategories;

var native const pointer FirstDynamicPrimitiveLink;
var native const pointer FirstStaticPrimitiveLink;
var native const matrix WorldToLight;
var native const matrix LightToWorld;

var() interp float	Brightness;
var() color	Color;

var() editinline LightFunction	Function;

var() bool	CastShadows;
var() bool	RequireDynamicShadows;
var() bool	bEnabled;

defaultproperties
{
	Brightness=1.0
	Color=(R=255,G=255,B=255)
	CastShadows=True
	RequireDynamicShadows=False
	bEnabled=true
}