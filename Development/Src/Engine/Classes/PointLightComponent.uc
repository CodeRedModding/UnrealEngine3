class PointLightComponent extends LightComponent
	native
	collapsecategories
	hidecategories(Object)
	editinlinenew;

var() float	Radius;

cpptext
{
	virtual UBOOL AffectsSphere(const FSphere& Sphere);
	virtual FPlane GetPosition() const;
	virtual FBox GetBoundingBox() const;
}

defaultproperties
{
	Radius=1024.0
}