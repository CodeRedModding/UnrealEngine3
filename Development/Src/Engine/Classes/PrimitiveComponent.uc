class PrimitiveComponent extends ActorComponent
	native
	noexport
	abstract;

var private native noexport pointer	ViewVTable; // FPrimitiveViewInterface virtual function table.

// Scene data.

var native const float			LocalToWorldDeterminant;
var native const matrix			LocalToWorld;
var native const array<pointer>	Lights;
var native const array<int>		CachedShadowVolumes;

// Shadow grouping.  An optimization which tells the renderer to use a single shadow for a group of primitive components.

var PrimitiveComponent				ShadowParent;
var native const PrimitiveComponent	FirstShadowChild,
									NextShadowChild;

// Primitive generated bounds.

var const native BoxSphereBounds	Bounds;

/**
 * The distance to cull this primitive at.  A CullDistance of 0 indicates that the primitive should not be culled by distance.
 * The distance calculation uses the center of the primitive's bounding sphere.
 */
var(Rendering) float CullDistance;

// Rendering flags.

var(Rendering) const bool	HiddenGame,
							HiddenEditor;
var(Rendering) bool			CastShadow;

/** If this is True, this component won't be visible when the view actor is the component's owner, directly or indirectly. */
var(Rendering) bool bOwnerNoSee;

/** If this is True, this component will only be visible when the view actor is the component's owner, directly or indirectly. */
var(Rendering) bool bOnlyOwnerSee;

// Collision flags.

var(Collision) const bool	CollideActors,
							BlockActors,
							BlockZeroExtent,
							BlockNonZeroExtent,
							BlockRigidBody;

// General flags.

/** If this is True, this component must always be loaded on clients, even if HiddenGame && !CollideActors. */
var private const bool AlwaysLoadOnClient;

/** If this is True, this component must always be loaded on servers, even if !CollideActors. */
var private const bool AlwaysLoadOnServer;

// Internal scene data.

var const native bool		bWasSNFiltered;
var const native array<int>	OctreeNodes;
var const native int		OctreeTag;
var const native qword		ZoneMask;

// Internal physics engine data.

var(Physics)	class<PhysicalMaterial>			PhysicalMaterialOverride;
var				const native RB_BodyInstance	BodyInstance;

//=============================================================================
// Physics.


/** Enum for controlling the falloff of strength of a radial impulse as a function of distance from Origin. */
enum ERadialImpulseFalloff
{
	/** Impulse is a constant strength, up to the limit of its range. */
	RIF_Constant,

	/** Impulse should get linearly weaker the further from origin. */
	RIF_Linear
};

/**
 *	Add an impulse to the physics of this PrimitiveComponent.
 *
 *	@param	Impulse		Magnitude and direction of impulse to apply.
 *	@param	Position	Point in world space to apply impulse at. If Position is (0,0,0), impulse is applied at center of mass ie. no rotation.
 *	@param	BoneName	If a SkeletalMeshComponent, name of bone to apply impulse to.
 *	@param	bVelChange	If true, the Strength is taken as a change in velocity instead of an impulse (ie. mass will have no affect).
 */
native final function AddImpulse(vector Impulse, optional vector Position, optional name BoneName, optional bool bVelChange);

/**
 * Add an impulse to this component, radiating out from the specified position.
 * In the case of a skeletal mesh, may affect each bone of the mesh.
 * 
 * @param Origin		Point of origin for the radial impulse blast
 * @param Radius		Size of radial impulse. Beyond this distance from Origin, there will be no affect.
 * @param Strength		Maximum strength of impulse applied to body.
 * @param Falloff		Allows you to control the strength of the impulse as a function of distance from Origin.
 * @param bVelChange	If true, the Strength is taken as a change in velocity instead of an impulse (ie. mass will have no affect).
 */
native final function AddRadialImpulse(vector Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff, optional bool bVelChange);

native final function AddForce(vector Force, optional vector Position, optional name BoneName);

/**
 * Set the linear velocity of the rigid body physics of this PrimitiveComponent. If no rigid-body physics is active, will do nothing.
 * In the case of a SkeletalMeshComponent will affect all bones.
 * This should be used cautiously - it may be better to use AddForce or AddImpulse.
 *
 * @param	NewVel			New linear velocity to apply to physics.
 * @param	bAddToCurrent	If true, NewVel is added to the existing velocity of the body.
 */
native final function SetRBLinearVelocity(vector NewVel, optional bool bAddToCurrent);

/** 
 *	Called if you want to move the physics of a component which has dynamics running (ie actor is in PHYS_RigidBody). 
 *	Be careful calling this when this is jointed to something else, or when it does not fit in the destination (no checking is done).
 */
native final function SetRBPosition(vector NewPos, rotator NewRot);

/** 
 *	Ensure simulation is running for this component. 
 *	If a SkeletalMeshComponent and no BoneName is specified, will wake all bones in the PhysicsAsset.
 */
native final function WakeRigidBody(optional name BoneName);

native final function UpdateWindForces();

/** 
 *	Returns if the body is currently awake and simulating.
 *	If a SkeletalMeshComponent, and no BoneName is specified, will pick a random bone - 
 *	so does not make much sense if not all bones are jointed together.
 */
native final function bool RigidBodyIsAwake(optional name BoneName);

/** 
 *	Change the value of BlockRigidBody.
 *
 *	@param NewBlockRigidBody - The value to assign to BlockRigidBody.
 */
native final function SetBlockRigidBody(bool bNewBlockRigidBody);

/**
 * Changes the value of HiddenGame.
 *
 * @param NewHidden	- The value to assign to HiddenGame.
 */
native final function SetHidden(bool NewHidden);

defaultproperties
{
	CastShadow=False
}