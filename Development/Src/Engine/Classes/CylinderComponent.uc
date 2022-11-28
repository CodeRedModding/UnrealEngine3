class CylinderComponent extends PrimitiveComponent
	native
	noexport
	collapsecategories
	editinlinenew;

var() const export float	CollisionHeight;
var() const export float	CollisionRadius;

native final function SetSize(float NewHeight, float NewRadius);

defaultproperties
{
	HiddenGame=True
	BlockZeroExtent=true
	BlockNonZeroExtent=true
	CollisionRadius=+00022.000000
	CollisionHeight=+00022.000000
}