/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class PrimitiveComponent extends ActorComponent
	dependson(Scene,LightComponent)
	native(Mesh)
	noexport
	abstract;

/** Mirrored from Scene.h */
struct MaterialViewRelevance
{
	var bool bOpaque;
	var bool bTranslucent;
	var bool bDistortion;
	var bool bOneLayerDistortionRelevance;
	var bool bLit;
	var bool bUsesSceneColor;
};

var const native transient int Tag;
// Primitive generated bounds.

var const native transient BoxSphereBounds Bounds;

/** The primitive's scene info. */
var private native transient const pointer SceneInfo{FPrimitiveSceneInfo};

/** A fence to track when the primitive is detached from the scene in the rendering thread. */
var private native const int DetachFence;

// Scene data.

var native transient const float LocalToWorldDeterminant;
var native transient const matrix LocalToWorld;
/**
 *	The index for the primitive component in the MotionBlurInfo array of the scene.
 *	Render-thread usage only.
 *	This assumes that there is only one scene that requires motion blur, as there is only
 *	a single index... If the application requires a primitive component to exist in multiple
 *	scenes and have motion blur in each of them, this can be changed into a mapping of the
 *	scene pointer to the index. (Associated functions would have to be updated as well...)
 */
var native transient const int MotionBlurInfoIndex;

/** Current list of active decals attached to the primitive */
var native private noimport const array<pointer> DecalList{class FDecalInteraction};
/** Decals that are detached from the primitive and need to be reattached */
var private transient const array<DecalComponent> DecalsToReattach;

// Shadow grouping.  An optimization which tells the renderer to use a single shadow for a group of primitive components.

var const PrimitiveComponent ShadowParent;

/** Replacement primitive to draw instead of this one (multiple UPrim's will point to the same Replacement) */
var(Rendering) crosslevelpassive PrimitiveComponent ReplacementPrimitive;

/** Keeps track of which fog component this primitive is using. */
var const transient FogVolumeDensityComponent FogVolumeComponent;

/** If specified, only OverrideLightComponent can affect the primitive. */
var const transient LightComponent OverrideLightComponent;

// Rendering flags.

/** The lighting environment to take the primitive's lighting from. */
var const LightEnvironmentComponent LightEnvironment;

/** Stores the previous light environment if SetLightEnvironment is called while the primitive is attached, so that Detach can notify the previous light environment correctly. */
var transient private const LightEnvironmentComponent PreviousLightEnvironment;

/**
 * The minimum distance at which the primitive should be rendered,
 * measured in world space units from the center of the primitive's bounding sphere to the camera position.
 */
var(Rendering) float MinDrawDistance;

/**
 * The distance at which the renderer will switch from parent (low LOD) to children (high LOD).
 * This is basically the same as MinDrawDistance, except that the low LOD will draw even up close, if there are no children.
 * This is needed so the high lod meshes can be in a streamable sublevel, and if streamed out, the low LOD will draw up close.
 */
var(Rendering) float MassiveLODDistance;

/**
 * Max draw distance exposed to LDs. The real max draw distance is the min (disregarding 0) of this and volumes affecting this object.
 * This is renamed to LDMaxDrawDistance in c++
 */
var(Rendering) const private noexport float MaxDrawDistance;

/**
 * The distance to cull this primitive at.
 * A CachedMaxDrawDistance of 0 indicates that the primitive should not be culled by distance.
 */
var(Rendering) editconst float CachedMaxDrawDistance;

/**
 * Scalar controlling the amount of motion blur to be applied when object moves.
 * 0=none, 1=full instance motion blur(default), value should be 0 or bigger
 */
var(Rendering) float MotionBlurInstanceScale;

/** Legacy, renamed to MaxDrawDistance  deprecated june 2008*/
var const private deprecated noexport float CullDistance;
/** Legacy, renamed to CachedMaxDrawDistance deprecated june 2008*/
var editconst deprecated float CachedCullDistance;

/** The scene depth priority group to draw the primitive in. */
var(Rendering) const ESceneDepthPriorityGroup DepthPriorityGroup;

/** The scene depth priority group to draw the primitive in, if it's being viewed by its owner. */
var const ESceneDepthPriorityGroup ViewOwnerDepthPriorityGroup;

/** If detail mode is > system detail mode, primitive won't be rendered. */
var(Rendering) const EDetailMode DetailMode;

/** Enum indicating different type of objects for rigid-body collision purposes. */
enum ERBCollisionChannel
{
	RBCC_Default,
	RBCC_Nothing, // Special channel that nothing should request collision with.
	RBCC_Pawn,
	RBCC_Vehicle,
	RBCC_Water,
	RBCC_GameplayPhysics,
	RBCC_EffectPhysics,
	RBCC_Untitled1,
	RBCC_Untitled2,
	RBCC_Untitled3,
	RBCC_Untitled4,
	RBCC_Cloth,
	RBCC_FluidDrain,
	RBCC_SoftBody,
	RBCC_FracturedMeshPart,
	RBCC_BlockingVolume,
	RBCC_DeadPawn,
	RBCC_Clothing,
	RBCC_ClothingCollision
};

/** Enum indicating what type of object this should be considered for rigid body collision. */
var(Collision)	const ERBCollisionChannel	RBChannel;

/**
 *	Used for creating one-way physics interactions (via constraints or contacts)
 *	Groups with lower RBDominanceGroup push around higher values in a 'one way' fashion. Must be <32.
 */
var(Physics)	byte		RBDominanceGroup;

/** Environment shadow factor used when previewing unbuilt lighting on this primitive. */
var				byte		PreviewEnvironmentShadowing;

/** True if the primitive should be rendered using ViewOwnerDepthPriorityGroup if viewed by its owner. */
var const bool bUseViewOwnerDepthPriorityGroup;

/** Whether to accept cull distance volumes to modify cached cull distance. */
var(Rendering) const bool	bAllowCullDistanceVolume;

var(Rendering) const bool	HiddenGame;
var(Rendering) const bool	HiddenEditor;

/** If this is True, this component won't be visible when the view actor is the component's owner, directly or indirectly. */
var(Rendering) const bool bOwnerNoSee;

/** If this is True, this component will only be visible when the view actor is the component's owner, directly or indirectly. */
var(Rendering) const bool bOnlyOwnerSee;

/** If true, bHidden on the Owner of this component will be ignored. */
var(Rendering) const bool bIgnoreOwnerHidden;

/**
 * Whether to render the primitive in the depth only pass.
 * Setting this to FALSE will cause artifacts with dominant light shadows and potentially large performance loss,
 * So it should be TRUE on all lit objects, setting it to FALSE is mostly only useful for debugging.
 */
var bool bUseAsOccluder;

/** If this is True, this component doesn't need exact occlusion info. */
var(Rendering) bool bAllowApproximateOcclusion;

/** If this is True, this component will return 0.0f as their occlusion when first rendered. */
var bool bFirstFrameOcclusion;

/** If True, this component will still be queried for occlusion even when it intersects the near plane. */
var bool bIgnoreNearPlaneIntersection;

/** If this is True, this component can be selected in the editor. */
var bool bSelectable;

/** If TRUE, forces mips for textures used by this component to be resident when this component's level is loaded. */
var(Rendering) const bool bForceMipStreaming;

/** replaced with bAcceptsStaticDecals,bAcceptsDynamicDecals Deprecated April 2008*/
var deprecated const bool bAcceptsDecals;
var deprecated const bool bAcceptsDecalsDuringGameplay;

/** If TRUE, this primitive accepts static level placed decals in the editor. */
var(Rendering) const bool bAcceptsStaticDecals;

/** If TRUE, this primitive accepts dynamic decals spawned during gameplay.  */
var(Rendering) const bool bAcceptsDynamicDecals;

var native transient const bool bIsRefreshingDecals;

var transient bool bAllowDecalAutomaticReAttach;

/** If true a hit-proxy will be generated for each instance of instanced static meshes */
var bool bUsePerInstanceHitProxies;

// Lighting flags

/**
 * Whether to cast any shadows or not
 *
 * controls whether the primitive component should cast a shadow or not. Currently dynamic primitives will not receive shadows from static objects unless both this flag and bCastDynamicSahdow are enabled.
 **/
var(Lighting)	bool		CastShadow;

/**
 * If true, forces all static lights to use light-maps for direct lighting on this primitive, regardless of the light's UseDirectLightMap property.
 *
 * forces the use of lightmaps for all static lights affecting this primitive even though the light might not be set to use light maps. This means that the primitive will not receive any shadows from dynamic objects obstructing static lights. It will correctly shadow in the case of dynamic lights
 */
var	const bool	bForceDirectLightMap;

/** If false, primitive does not cast dynamic shadows.
 *
 * controls whether the primitive should cast shadows in the case of non precomputed shadowing like e.g. the primitive being in between a light and a dynamic object. This flag is only used if CastShadow is TRUE. Currently dynamic primitives will not receive shadows from static objects unless both this flag and CastShadow are enabled.
 *
 **/
var(Lighting)	bool		bCastDynamicShadow;

/** Whether the object should cast a static shadow from shadow casting lights.  Also requires Cast Shadow to be set to True. */
var(Lighting)	bool		bCastStaticShadow;

/**
 * If true, the primitive will only shadow itself and will not cast a shadow on other primitives.
 * This can be used as an optimization when the shadow on other primitives won't be noticeable.
 */
var(Lighting)	bool		bSelfShadowOnly;

/**
 * For mobile platforms only! If true, the primitive will not receive projected mod shadows, not from itself nor any other mod shadow caster.
 * This can be used to avoid self-shadowing artifacts.
 */
var(Lighting)	bool		bNoModSelfShadow;

/**
 * Optimization for objects which don't need to receive dominant light shadows.
 * This is useful for objects which eat up a lot of GPU time and are heavily texture bound yet never receive noticeable shadows from dominant lights like trees.
 */
var(Lighting)	bool		bAcceptsDynamicDominantLightShadows;

/**
 *	If TRUE, the primitive will cast shadows even if bHidden is TRUE.
 *
 *	Controls whether the primitive should cast shadows when hidden.
 *	This flag is only used if CastShadow is TRUE.
 *
 */
var(Lighting)	bool		bCastHiddenShadow;

/** Whether this primitive should cast dynamic shadows as if it were a two sided material. */
var(Lighting)	bool		bCastShadowAsTwoSided;

/**
 * Does this primitive accept lights?
 *
 * controls whether the primitive accepts any lights. Should be set to FALSE for e.g. unlit objects as its a nice optimization - especially for larger objects.
 **/
var(Lighting)	const bool	bAcceptsLights;

/**
 * Whether this primitives accepts dynamic lights
 *
 * controls whether the object should be affected by dynamic lights.
 **/
var(Lighting)	const bool	bAcceptsDynamicLights;

/**
 * If TRUE, dynamically lit translucency on this primitive will render in one pass,
 * Which is cheaper and ensures correct blending but approximates lighting using one directional light and all other lights in an unshadowed SH environment.
 * If FALSE, dynamically lit translucency will render in multiple passes which uses more shader instructions and results in incorrect blending.
 */
var(Lighting)	const bool bUseOnePassLightingOnTranslucency;

/** Whether to allow the primitive to use precomputed shadows or lighting. */
var(Lighting)	const bool	bUsePrecomputedShadows;

/**
* TRUE if ShadowParent was set through SetShadowParent,
* FALSE if ShadowParent is set automatically based on Owner->bShadowParented.
*/
var private transient const bool bHasExplicitShadowParent;

var deprecated bool bAllowAmbientOcclusion;

// Collision flags.

var(Collision)	const bool	CollideActors;

/** when this is on, this primitive component get collision tests even if it isn't the actor's collision component */
var const bool  AlwaysCheckCollision;

var(Collision)	const bool	BlockActors;
var(Collision)	const bool	BlockZeroExtent;
var(Collision)	const bool	BlockNonZeroExtent;
/** TRUE if this primitive is eligible to block camera traces, FALSE if the camera should ignore it. */
var(Collision)	const bool	CanBlockCamera;
var(Collision)	const bool	BlockRigidBody;
/** If TRUE will block foot placement line checks (default). FALSE will skip right through. */
var(Collision)  const bool  bBlockFootPlacement;

/** Never create any physics engine representation for this body. */
var(Physics) const bool bDisableAllRigidBody;

/** When creating rigid body, will skip normal geometry creation step, and will rely on ModifyNxActorDesc to fill in geometry. */
var(Physics) const bool	bSkipRBGeomCreation;

/**
 *	Flag that indicates if OnRigidBodyCollision function should be called for physics collisions involving this PrimitiveComponent.
 */
var(Physics) const bool	bNotifyRigidBodyCollision;

// Novodex fluids

/** Whether this object should act as a 'drain' for fluid, and destroy fluid particles when they contact it. */
var(Physics) const bool	bFluidDrain;

/** Indicates that fluid interaction with this object should be 'two-way' - that is, force should be applied to both fluid and object. */
var(Physics) const bool	bFluidTwoWay;

// Physics

/** Will ignore radial impulses applied to this component. */
var(Physics)	bool		bIgnoreRadialImpulse;

/** Will ignore radial forces applied to this component. */
var(Physics)	bool		bIgnoreRadialForce;

/** Will ignore force field applied to this component. */
var(Physics)	bool		bIgnoreForceField;

/** Place into a NxCompartment that will run in parallel with the primary scene's physics with potentially different simulation parameters.
 *  If double buffering is enabled in the WorldInfo then physics will run in parallel with the entire game for this component. */
var(Physics)	const bool		bUseCompartment;

// General flags.

/** If this is True, this component must always be loaded on clients, even if HiddenGame && !CollideActors. */
var private const bool AlwaysLoadOnClient;

/** If this is True, this component must always be loaded on servers, even if !CollideActors. */
var private const bool AlwaysLoadOnServer;

/** Allow certain components to render even if the parent actor is part of the camera's HiddenActors array. */
var() bool bIgnoreHiddenActorsMembership;

var() const bool			AbsoluteTranslation;
var() const bool			AbsoluteRotation;
var() const bool			AbsoluteScale;

/** Determines whether or not we allow shadowing fading.  Some objects (especially in cinematics) having the shadow fade/pop out looks really bad. **/
var bool bAllowShadowFade;

/** Whether or not this primitive type is supported on mobile. For the emulate mobile rendering editor feature. */
var bool bSupportedOnMobile;

// Internal scene data.

var const native transient bool bWasSNFiltered;
var const native transient array<int> OctreeNodes;


/**
 * Translucent objects with a lower sort priority draw behind objects with a higher priority.
 * Translucent objects with the same priority are rendered from back-to-front based on their bounds origin.
 *
 * Ignored if the object is not translucent.  The default priority is zero.
 * Warning: This should never be set to a non-default value unless you know what you are doing, as it will prevent the renderer from sorting correctly.
 * It is especially problematic on dynamic gameplay effects.
 **/
var(Rendering) int TranslucencySortPriority;

/** Used for precomputed visibility */
var duplicatetransient int VisibilityId;

/**
 * Lighting channels controlling light/ primitive interaction. Only allows interaction if at least one channel is shared
 *
 */
var(Lighting)	const LightingChannelContainer	LightingChannels;

/**
 *	Container for indicating a set of collision channel that this object will collide with.
 *	Mirrored manually in UnPhysPublic.h
 */
struct RBCollisionChannelContainer
{
	var()	const bool	Default;
	var		const bool	Nothing; // This is reserved to allow an object to opt-out of all collisions, and should not be set.
	var()	const bool	Pawn;
	var()	const bool	Vehicle;
	var()	const bool	Water;
	var()	const bool	GameplayPhysics;
	var()	const bool	EffectPhysics;
	var()	const bool	Untitled1;
	var()	const bool	Untitled2;
	var()	const bool	Untitled3;
	var()	const bool	Untitled4;
	var()	const bool	Cloth;
	var()	const bool	FluidDrain;
	var()	const bool	SoftBody;
	var()	const bool	FracturedMeshPart;
	var()	const bool	BlockingVolume;
	var()	const bool	DeadPawn;
	var()	const bool	Clothing;
	var()	const bool	ClothingCollision;
};

/** Types of objects that this physics objects will collide with. */
var(Collision) const RBCollisionChannelContainer	RBCollideWithChannels;

/** Return codes for ClosestPointToPrimitive functions */
enum GJKResult
{
	GJK_Intersect,      //two primitives overlap (results invalid)
	GJK_NoIntersection, //two primitives don't overlap (results valid)
	GJK_Fail            //failed to find result in max iteration time (results valid but unoptimal)
};



// Internal physics engine data.

/** Allows you to override the PhysicalMaterial to use for this PrimitiveComponent. */
var(Physics)	const PhysicalMaterial			PhysMaterialOverride;

var	duplicatetransient	const native RB_BodyInstance	BodyInstance;

// Properties moved from TransformComponent
var native transient const matrix CachedParentToWorld; //@todo please remove me if possible

var() const vector			Translation;
var() const rotator			Rotation;
var() const float			Scale <UIMin=0.0 | UIMax=4.0>;
var() const vector			Scale3D;
/**
 * Scales the bounds of the object.
 * This is useful when using World Position Offset to animate the vertices of the object outside of its bounds.
 * Warning: Increasing the bounds of an object will reduce performance and shadow quality!
 * Currently only used by StaticMeshComponent and SkeletalMeshComponent.
 */
var() const float			BoundsScale <UIMin=1 | UIMax=10.0>;

/** Last time the component was submitted for rendering (called FScene::AddPrimitive). */
var const transient float	LastSubmitTime;

/**
 * The value of WorldInfo->TimeSeconds for the frame when this actor was last rendered.  This is written
 * from the render thread, which is up to a frame behind the game thread, so you should allow this time to
 * be at least a frame behind the game thread's world time before you consider the actor non-visible.
 * There's an equivalent variable in PrimitiveComponent.
 */
var transient float	LastRenderTime;

//=============================================================================
// Physics.

/** if > 0, the script RigidBodyCollision() event will be called on our Owner when a physics collision involving
 * this PrimitiveComponent occurs and the relative velocity is greater than or equal to this
 */
var float ScriptRigidBodyCollisionThreshold;

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
 * Good for zero time.  One time insta burst.
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

/**
 *	Add a force to this component.
 *
 * This is like a thruster. Good for adding a burst over some (non zero) time.
 *
 *	@param Force		Force vector to apply. Magnitude indicates strength of force.
 *	@param Position		Position on object to apply force. If (0,0,0), force is applied at center of mass.
 *	@param BoneName		Used in the skeletal case to apply a force to a single body.
 */
native final function AddForce(vector Force, optional vector Position, optional name BoneName);

/**
 *	Add a force originating from the supplied world-space location.
 *
 *	@param Origin		Origin of force in world space.
 *	@param Radius		Radius within which to apply the force.
 *	@param Strength		Strength of force to apply.
 *  @param Falloff		Allows you to control the strength of the force as a function of distance from Origin.
 */
native final function AddRadialForce(vector Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff);

/**
*	Add a torque to this component.
*	@param Torque		Force vector to apply. Magnitude indicates strength of force.
*	@param BoneName		Used in the skeletal case to apply a force to a single body.
*/
native final function AddTorque(vector Torque, optional name BoneName);

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
 * Set the angular velocity of the rigid body physics of this PrimitiveComponent. If no rigid-body physics is active, will do nothing.
 * In the case of a SkeletalMeshComponent will affect all bones - and will apply the linear velocity necessary to get all bones to rotate around the root.
 * This should be used cautiously - it may be better to use AddForce or AddImpulse.
 *
 * @param	NewAngVel		New angular velocity to apply to physics.
 * @param	bAddToCurrent	If true, NewAngVel is added to the existing velocity of the body.
 */
native final function SetRBAngularVelocity(vector NewAngVel, optional bool bAddToCurrent);

/**
 *	Reduce velocity of rigid body physics in the direction supplied. This decomposes body velocity into that along supplied vector and that perpendicular to the vector.
 *	That along vector, if in same direction as vector, is scale by VelScale. If it is moving in the opposite direction to supplied vector it is not affected.
 *
 *	@param	RetardDir		Unit vector indicating direction to check velocity of physics against
 *	@param	VelScale		Value from 0.0 to 1.0 - 1.0 will stop all motion along RetardDir
 */
native final function RetardRBLinearVelocity(vector RetardDir, float VelScale);

/**
 * Called if you want to move the physics of a component which has dynamics running (ie actor is in PHYS_RigidBody).
 * Be careful calling this when this is jointed to something else, or when it does not fit in the destination (no checking is done).
 * @param NewPos new position of the body
 * @param BoneName (SkeletalMeshComponent only) if specified, the bone to change position of
 * 			if not specified for a SkeletalMeshComponent, all bodies are moved by the delta
 * 			between the desired location and that of the root body.
 */
native final function SetRBPosition(vector NewPos, optional name BoneName);

/**
 * Called if you want to rotate the physics of a component which has dynamics running (ie actor is in PHYS_RigidBody).
 * @param NewRot new rotation of the body
 * @param BoneName (SkeletalMeshComponent only) if specified, the bone to change rotation of
 * 			if not specified for a SkeletalMeshComponent, all bodies are moved by the delta
 * 			between the desired rotation and that of the root body.
 */
native final function SetRBRotation(rotator NewRot, optional name BoneName);

/**
 *	Ensure simulation is running for this component.
 *	If a SkeletalMeshComponent and no BoneName is specified, will wake all bones in the PhysicsAsset.
 */
native final function WakeRigidBody(optional name BoneName);

/**
 * Put a simulation back to sleep.
 */
native final function PutRigidBodyToSleep(optional name BoneName);

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
 *	Changes a member of the RBCollideWithChannels container for this PrimitiveComponent.
 *
 * @param bNewCollides whether or not to collide with passed in channel
 */
final native function SetRBCollidesWithChannel(ERBCollisionChannel Channel, bool bNewCollides);

/**
 *	Sets the collision channels based on the settings in the Channel container.
 *
 * @param Channels is a list of channels with which the component should collide
 */
final native function SetRBCollisionChannels(RBCollisionChannelContainer Channels);

/**
 *	Changes the rigid-body channel that this object is defined in.
 */
final native function SetRBChannel(ERBCollisionChannel Channel);

/** Changes the value of bNotifyRigidBodyCollision
 * @param bNewNotifyRigidBodyCollision - The value to assign to bNotifyRigidBodyCollision
 */
native final function SetNotifyRigidBodyCollision(bool bNewNotifyRigidBodyCollision);

/** initializes rigid body physics for this component
 * this is done automatically for PrimitiveComponents attached via Actor defaults,
 * but if a component is attached at runtime you may need to call this function to set it up
 * @note: this function does nothing if not attached or bDisableAllRigidBody is set
 */
native final function InitRBPhys();

/**
 *	Changes the current PhysMaterialOverride for this component.
 *	Note that if physics is already running on this component, this will _not_ alter its mass/inertia etc, it will only change its
 *	surface properties like friction and the damping.
 */
native final function SetPhysMaterialOverride(PhysicalMaterial NewPhysMaterial);

/** returns the physics RB_BodyInstance for the root body of this component (if any) */
native final function RB_BodyInstance GetRootBodyInstance();

/**
 *	Used for creating one-way physics interactions.
 *	@see RBDominanceGroup
 */
native final function SetRBDominanceGroup(BYTE InDomGroup);

/**
 *  Looking at various values of the component, determines if this
 *  component should be added to the scene
 * @return TRUE if the component is visible and should be added to the scene, FALSE otherwise
 */
native final function bool ShouldComponentAddToScene();

/**
 * Changes the value of HiddenGame.
 *
 * @param NewHidden	- The value to assign to HiddenGame.
 */
native final function SetHidden(bool NewHidden);

/**
 * Changes the value of bOwnerNoSee.
 */
native final function SetOwnerNoSee(bool bNewOwnerNoSee);

/**
 * Changes the value of bOnlyOwnerSee.
 */
native final function SetOnlyOwnerSee(bool bNewOnlyOwnerSee);

/**
* Changes the value of bIgnoreOwnerHidden.
*/
native final function SetIgnoreOwnerHidden(bool bNewIgnoreOwnerHidden);

/**
 * Changes the value of ShadowParent.
 * @param NewShadowParent - The value to assign to ShadowParent.
 */
native final function SetShadowParent(PrimitiveComponent NewShadowParent);

/**
 * Changes the value of LightEnvironment.
 * @param NewLightEnvironment - The value to assign to LightEnvironment.
 */
native final function SetLightEnvironment(LightEnvironmentComponent NewLightEnvironment);

/**
 * Changes the value of CullDistance.
 * @param NewCullDistance - The value to assign to CullDistance.
 */
native final function SetCullDistance(float NewCullDistance);

/**
 * Changes the value of LightingChannels.
 * @param NewLightingChannels - The value to assign to LightingChannels.
 */
native final function SetLightingChannels(LightingChannelContainer NewLightingChannels);

/**
 * Changes the value of DepthPriorityGroup.
 * @param NewDepthPriorityGroup - The value to assign to DepthPriorityGroup.
 */
native final function SetDepthPriorityGroup(ESceneDepthPriorityGroup NewDepthPriorityGroup);

/**
 * Changes the value of bUseViewOwnerDepthPriorityGroup and ViewOwnerDepthPriorityGroup.
 * @param bNewUseViewOwnerDepthPriorityGroup - The value to assign to bUseViewOwnerDepthPriorityGroup.
 * @param NewViewOwnerDepthPriorityGroup - The value to assign to ViewOwnerDepthPriorityGroup.
 */
native final function SetViewOwnerDepthPriorityGroup(
	bool bNewUseViewOwnerDepthPriorityGroup,
	ESceneDepthPriorityGroup NewViewOwnerDepthPriorityGroup
	);

native final function SetTraceBlocking(bool NewBlockZeroExtent, bool NewBlockNonZeroExtent);

native final function SetActorCollision(bool NewCollideActors, bool NewBlockActors, optional bool NewAlwaysCheckCollision);

// Copied from TransformComponent
native function SetTranslation(vector NewTranslation);
native function SetRotation(rotator NewRotation);
native function SetScale(float NewScale);
native function SetScale3D(vector NewScale3D);
native function SetAbsolute(optional bool NewAbsoluteTranslation,optional bool NewAbsoluteRotation,optional bool NewAbsoluteScale);

/** Returns position of the component, in world space. */
native function vector GetPosition();

/** Returns rotation of the component, in world space. */
native function rotator GetRotation();

/**
* Calculates the closest point on this primitive to a point given
* @param POI - Point in world space to determine closest point to
* @param Extent - Convex primitive
* @param OutPointA - The point closest on the extent box
* @param OutPointB - Point on this primitive closest to the extent box
*
* @return An enumeration indicating the result of the query (intersection/non-intersection/failure)
*/
native final function GJKResult ClosestPointOnComponentToPoint(out vector POI, out vector Extent, out vector OutPointA, out vector OutPointB);

/**
* Calculates the closest point this component to another component
* @param PrimitiveComponent - Another Primitive Component
* @param PointOnComponentA - Point on this primitive closest to other primitive
* @param PointOnComponentB - Point on other primitive closest to this primitive
*
* @return An enumeration indicating the result of the query (intersection/non-intersection/failure)
*/
native function GJKResult ClosestPointOnComponentToComponent(out PrimitiveComponent OtherComponent, out vector PointOnComponentA, out vector PointOnComponentB);

defaultproperties
{
	LastRenderTime=-1000
	Scale=1.0
	MotionBlurInstanceScale=1.0
	Scale3D=(X=1.0,Y=1.0,Z=1.0)
	BoundsScale=1
	MinDrawDistance=0
	DepthPriorityGroup=SDPG_World
	bAllowCullDistanceVolume=TRUE
	bUseAsOccluder=FALSE
	CastShadow=FALSE
	bCastDynamicShadow=TRUE
	bCastStaticShadow=TRUE
	bAcceptsDynamicDominantLightShadows=TRUE
	bAcceptsLights=FALSE
	bAcceptsDynamicLights=TRUE
	bSelectable=TRUE
	bAcceptsStaticDecals=FALSE
	bAcceptsDynamicDecals=TRUE
	bAllowDecalAutomaticReAttach=TRUE
	bAcceptsDecalsDuringGameplay=TRUE
	AlwaysLoadOnClient=TRUE
	AlwaysLoadOnServer=TRUE
	bAllowShadowFade=TRUE
	bSupportedOnMobile=TRUE
	RBChannel=RBCC_Default
	RBDominanceGroup=15
	PreviewEnvironmentShadowing=180
	CanBlockCamera=TRUE
	bBlockFootPlacement=TRUE
	bAllowAmbientOcclusion=TRUE
	VisibilityId=-1
}
