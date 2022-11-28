class PhysicalMaterial extends Object
	native(Physics)
	collapsecategories
	hidecategories(Object);

// Surface properties
var()	float	Friction;
var()	float	Restitution;

// Object properties
var()	float	Density;
var()	float	AngularDamping;
var()	float	LinearDamping;
var()	float	MagneticResponse;
var()	float	WindResponse;

// Used internally by physics engine.
var	transient int	MaterialIndex;

defaultproperties
{
	Friction=0.7
	Restitution=0.3

	Density=1.0
	AngularDamping=0.0
	LinearDamping=0.01
	MagneticResponse=0.0
	WindResponse=0.0
}