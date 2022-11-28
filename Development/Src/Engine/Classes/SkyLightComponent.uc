class SkyLightComponent extends LightComponent
	native
	collapsecategories
	hidecategories(Object)
	editinlinenew;

cpptext
{
	virtual FPlane GetPosition() const;
}

defaultproperties
{
	CastShadows=False
}