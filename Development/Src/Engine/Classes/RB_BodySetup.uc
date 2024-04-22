/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class RB_BodySetup extends KMeshProps
	hidecategories(Object)
	native(Physics);

/** Presets of values used in considering when put this body to sleep. */
enum ESleepFamily
{
	/** Engine defaults. */
	SF_Normal,
	/** A family of values with a lower sleep threshold; good for slower pendulum-like physics. */
	SF_Sensitive,
};

/** The set of values used in considering when put this body to sleep. */
var() ESleepFamily				SleepFamily;

/** Used in the PhysicsAsset case. Associates this Body with Bone in a skeletal mesh. */
var()	editconst name			BoneName;	

/** No dynamics on this body - fixed relative to the world. */
var()	bool					bFixed; 

/** This body will not collide with anything. */
var()	bool					bNoCollision;

/** When doing line checks against this PhysicsAsset, this body should return hits with zero-extent (ie line) checks. */
var()	bool					bBlockZeroExtent;

/** When doing line checks against this PhysicsAsset, this body should return hits with non-zero-extent (ie swept-box) checks. */
var()	bool					bBlockNonZeroExtent;

/** 
 *	Turn on continuous collision detection for this body.
 *	This should avoid it passing through other objects when moving quickly.
 */
var()	bool					bEnableContinuousCollisionDetection;

/** 
 *	If true (and bEnableFullAnimWeightBodies in SkelMeshComp is true), the physics of this bone will always be blended into the skeletal mesh, regardless of what PhysicsWeight of the SkelMeshComp is. 
 *	This is useful for bones that should always be physics, even when blending physics in and out for hit reactions (eg cloth or pony-tails).
 */
var()	bool					bAlwaysFullAnimWeight;

/** 
 *	Should this BodySetup be considered for the bounding box of the PhysicsAsset (and hence SkeletalMeshComponent).
 *	There is a speed improvement from having less BodySetups processed each frame when updating the bounds.
 */
var()	bool					bConsiderForBounds;

/** Physical material to use for this body. Encodes information about density, friction etc. */
var()   PhysicalMaterial		PhysMaterial;

/** 
 *	The mass of a body is calculated automatically based on the volume of the collision geometry and the density specified by the PhysicalMaterial.
 *	This parameters allows you to scale the auto-generated value for this specific body.
 */
var()	float					MassScale;

/** Cache of physics-engine specific collision shapes at different scales. Physics engines do not normally support per-instance collision shape scaling. */
var		const native array<pointer>	CollisionGeom;

/** Scale factors for each CollisionGeom entry. CollisionGeom.Length == CollisionGeomScale3D.Length. */
var		const native array<vector>	CollisionGeomScale3D;


// PRECOOKED COLLISION

/** Scales to pre-cache physics data for this collision at. */
var()	const array<vector>						PreCachedPhysScale;

/** Script mirror of cached pre-cooked physics data for one convex hull */
struct KCachedConvexDataElement
{
	var native array<byte>						ConvexElementData;
};

/** Script mittot of cached pre-cooked physics data for this simplified collision */
struct KCachedConvexData
{
	var native array<KCachedConvexDataElement>	CachedConvexElements;
};

/** Array of cached convex physics data. */
var		const native array<KCachedConvexData>	PreCachedPhysData;

/** Version of cached physics data. */
var		const int								PreCachedPhysDataVersion;

cpptext
{
	// UObject interface.
	virtual void PreSave();
	virtual void Serialize(FArchive& Ar);
	virtual void BeginDestroy();
	virtual void FinishDestroy();
	virtual void PostLoad();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	// URB_BodySetup interface.
	void	CopyBodyPropertiesFrom(class URB_BodySetup* fromSetup);
	void	ClearShapeCache();

	/** Pre-cache this mesh at all desired scales. */
	void PreCachePhysicsData();

	/*
	* Create and add a new physics collision representation to this body setup
	* @param Scale3D    - Scale at which this geometry should be created at
	* @param CachedData - Unreal description of the collision geometry to create
	* @param DebugName  - debug name to output
	* returns TRUE if successful
	*/
	UBOOL AddCollisionFromCachedData(const FVector& Scale3D, FKCachedConvexData* CachedData, const FString& DebugName);
}

defaultproperties
{
	SleepFamily=SF_Normal
	bBlockZeroExtent=true
	bBlockNonZeroExtent=true

	bConsiderForBounds=TRUE

	MassScale=1.0
}
