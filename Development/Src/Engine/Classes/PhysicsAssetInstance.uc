class PhysicsAssetInstance extends Object
	hidecategories(Object)
	noexport
	native;

/**
 *	Per-instance asset variables
 *	You only need one of these when you are actually running physics for the asset. 
 *	One is created for a skeletal mesh inside AActor::InitArticulated.
 *	You can perform line-checks etc. with just a PhysicsAsset.
 */


/**
 *	Actor that owns this PhysicsAsset instance.
 *	Filled in by InitInstance, so we don't need to save it.
 */
var 	const transient Actor							Owner; 

/** 
 *	Index of the 'Root Body', or top body in the asset heirarchy. Used by PHYS_Articulated to get new location for Actor.
 *	Filled in by InitInstance, so we don't need to save it.
 */
var		const transient int								RootBodyIndex;

/** Array of RB_BodyInstance objects, storing per-instance state about about each body. */
var		const editinline export array<RB_BodyInstance>				Bodies;

/** Array of RB_ConstraintInstance structs, storing per-instance state about each constraint. */
var		const editinline export array<RB_ConstraintInstance>		Constraints;

/** Table indicating which pairs of bodies have collision disabled between them. Used internally. */
var		const native Map_Mirror							CollisionDisableTable;