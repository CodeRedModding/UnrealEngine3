class SpotLightComponent extends PointLightComponent
	native
	collapsecategories
	hidecategories(Object)
	editinlinenew;

var() float	InnerConeAngle,
			OuterConeAngle;

cpptext
{
	virtual UBOOL AffectsSphere(const FSphere& Sphere);
}

defaultproperties
{
	InnerConeAngle=0
	OuterConeAngle=44
}