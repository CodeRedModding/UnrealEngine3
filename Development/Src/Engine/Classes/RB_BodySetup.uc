class RB_BodySetup extends KMeshProps
	hidecategories(Object)
	noexport
	native;

/** Used in the PhysicsAsset case. Associates this Body with Bone in a skeletal mesh. */
var()	name					BoneName;	

/** No dynamics on this body - fixed relative to the world. */
var()	bool					bFixed; 

/** This body will not collide with anything. */
var()	bool					bNoCollision;

/** When doing line checks against this PhysicsAsset, this body should return hits with zero-extent (ie line) checks. */
var()	bool					bBlockZeroExtent;

/** When doing line checks against this PhysicsAsset, this body should return hits with non-zero-extent (ie swept-box) checks. */
var()	bool					bBlockNonZeroExtent;

/** Physical material class to use for this body. Encodes information about density, friction etc. */
var()   class<PhysicalMaterial>	PhysicalMaterial;

/** 
 *	The mass of a body is calculated automatically based on the volume of the collision geometry and the density specified by the PhysicalMaterial.
 *	This parameters allows you to scale the auto-generated value for this specific body.
 */
var()	float					MassScale;

/** 
 *	The apparent velocity that this body has in the world, defined in the local space of the body.
 *	This only has affect if this is a fixed body.
 */
var()	vector					ApparentVelocity;

/** Cache of physics-engine specific collision shapes at different scales. Physics engines do not normally support per-instance collision shape scaling. */
var		const native array<pointer>	CollisionGeom;

/** Scale factors for each CollisionGeom entry. CollisionGeom.Length == CollisionGeomScale3D.Length. */
var		const native array<vector>	CollisionGeomScale3D;

defaultproperties
{
	PhysicalMaterial=class'Engine.PhysicalMaterial'
	bBlockZeroExtent=true
	bBlockNonZeroExtent=true

	MassScale=1.0
}