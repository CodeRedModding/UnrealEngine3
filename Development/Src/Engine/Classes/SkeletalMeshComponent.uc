/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SkeletalMeshComponent extends MeshComponent
	native(SkeletalMesh)
	noexport
	dependson(AnimNode)
	hidecategories(Object)
	config(Engine)
	editinlinenew;

/** The skeletal mesh used by this component. */
var()	SkeletalMesh			SkeletalMesh;

/** The SkeletalMeshComponent that this one is possibly attached to. */
var		SkeletalMeshComponent	AttachedToSkelComponent;

/**
 *	This should point to the AnimTree in a content package.
 *	BeginPlay on this SkeletalMeshComponent will instance (copy) the tree and assign the instance to the Animations pointer.
 */
var()   const					AnimTree	AnimTreeTemplate;

/**
 *	This is the unique instance of the AnimTree used by this SkeletalMeshComponent.
 *	THIS SHOULD NEVER POINT TO AN ANIMTREE IN A CONTENT PACKAGE.
 */
var()	const editinline export AnimNode Animations <MaxPropertyDepth=3>;

/** Array of all AnimNodes in entire tree, in the order they should be ticked - that is, all parents appear before a child. */
var	const transient array<AnimNode> AnimTickArray;
/** Special Array of nodes that should always be ticked, even when not relevant. */
var const transient array<AnimNode> AnimAlwaysTickArray;
/** Anim nodes relevancy status. Matching AnimTickArray size and indices. */
var const transient array<INT> AnimTickRelevancyArray;
/** Anim nodes weights. Matching AnimTickArray size and indices. */
var const transient array<FLOAT> AnimTickWeightsArray;
/** Linear Array for ticking SkelControls faster */
var const transient Array<SkelControlBase> SkelControlTickArray;

/**
 *	Physics and collision information used for this SkeletalMesh, set up in PhAT.
 *	This is used for per-bone hit detection, accurate bounding box calculation and ragdoll physics for example.
 */
var()	const PhysicsAsset										PhysicsAsset;

/**
 *	Any instanced physics engine information for this SkeletalMeshComponent.
 *	This is only required when you want to run physics or you want physical interaction with this skeletal mesh.
 */
var		const transient editinline export PhysicsAssetInstance	PhysicsAssetInstance;

/**
 * Contains a pointer to the active APEX clothing instance.
 */
var const native transient pointer ApexClothing;

/** 
 *  Enum to define how to scale max distance
 *  @see SetApexClothingMaxDistanceScale
 */
enum EMaxDistanceScaleMode
{
	/** Clothing MaxDistances are reduced by mulitplication with the scale value */
	MDSM_Multiply,
	/** Clothing MaxDistances are reduced by subtracting the value "scale * biggest maxDistance" */
	MDSM_Substract
};

/**
 *	Influence of rigid body physics on the mesh's pose (0.0 == use only animation, 1.0 == use only physics)
 */
var()	interp float			PhysicsWeight;

/** Used to scale speed of all animations on this skeletal mesh. */
var()	float					GlobalAnimRateScale;

/**
 * Allows adjusting the desired streaming distance of streaming textures that uses UV 0.
 * 1.0 is the default, whereas a higher value makes the textures stream in sooner from far away.
 * A lower value (0.0-1.0) makes the textures stream in later (you have to be closer).
 */
var()	float	StreamingDistanceMultiplier;

var native transient const pointer MeshObject;
var() Color						WireframeColor;

/** Temporary array of of component-space bone matrices, update each frame and used for rendering the mesh. */
var native transient const array<AnimNode.BoneAtom>			SpaceBases;

/** Temporary array of local-space (ie relative to parent bone) rotation/translation for each bone. */
var native transient const array<AnimNode.BoneAtom>		    LocalAtoms;

/** Cached Bones, for performance. */
var native transient const array<AnimNode.BoneAtom>		    CachedLocalAtoms;
var native transient const array<AnimNode.BoneAtom>		    CachedSpaceBases;

/** When updated at low frequency, rate of update.
 *  For example if set to 3, animations will be updated once every three frames.
 *  if games runs at 30 FPS, that's 10 FPS.
 *  Not recommended to change during gameplay. */
var const globalconfig INT LowUpdateFrameRate;

/** Temporary array of bone indices required this frame. Filled in by UpdateSkelPose. */
var native const transient Array<byte> RequiredBones;
/** Required Bones array, re-ordered for 3 pass skeleton composing */
var native const transient Array<Byte> ComposeOrderedRequiredBones;

/**
 *	If set, this SkeletalMeshComponent will not use its Animations pointer to do its own animation blending, but will
 *	use the SpaceBases array in the ParentAnimComponent. This is used when constructing a character using multiple skeletal meshes sharing the same
 *	skeleton within the same Actor.
 */
var()	const SkeletalMeshComponent	ParentAnimComponent;

/**
 *	Mapping between bone indices in this component and the parent one. Each element is the index of the bone in the ParentAnimComponent.
 *	Size should be the same as SkeletalMesh.RefSkeleton size (ie number of bones in this skeleton).
 */
var native transient const array<int> ParentBoneMap;

/**
 *	The set of AnimSets that will be looked in to find a particular sequence, specified by name in an AnimNodeSequence.
 *	Array is search from last to first element, so you can replace a particular sequence but putting a set containing the new version later in the array.
 *	You will need to call SetAnim again on nodes that may be affected by any changes you make to this array.
 */
var()	array<AnimSet>			AnimSets;

/**
 *  Temporary array of AnimSets that are used as a backup target when the engine needs to temporarily modify the
 *	actor's animation set list. (e.g. Matinee playback)
 */
var native transient const array<AnimSet> TemporarySavedAnimSets;


// Morph targets

/**
 *	Array of MorphTargetSets that will be looked in to find a particular MorphTarget, specified by name.
 *	It is searched in the same way as the AnimSets array above.
 */
var()	array<MorphTargetSet>	MorphSets;

/** Struct used to indicate one active morph target that should be applied to this SkeletalMesh when rendered. */
struct ActiveMorph
{
	/** The morph target that we want to apply. */
	var	MorphTarget		Target;

	/** Strength of the morph target, between 0.0 and 1.0 */
	var	float			Weight;
};

/** Array indicating all active MorphTargets. This array is updated inside UpdateSkelPose based on the AnimTree's st of MorphNodes. */
var	transient array<ActiveMorph>	ActiveMorphs;

/** Array indicating all active MorphTargets. This array is updated inside UpdateSkelPose based on the AnimTree's st of MorphNodes. */
var	transient array<ActiveMorph>	ActiveCurveMorphs;

/** Map of morph target to name **/
var		const native	map{FName, UMorphTarget*}			MorphTargetIndexMap;

// Attachments.

struct Attachment
{
	var() editinline ActorComponent	Component;
	var() name						BoneName;
	var() vector					RelativeLocation;
	var() rotator					RelativeRotation;
	var() vector					RelativeScale;

	structdefaultproperties
	{
		RelativeScale=(X=1,Y=1,Z=1)
	}
};

var duplicatetransient const array<Attachment> Attachments;

var	transient const array<byte>	SkelControlIndex;
var	transient const array<byte> PostPhysSkelControlIndex;

/** If 0, auto-select LOD level. if >0, force to (ForcedLodModel-1). */
var() int		ForcedLodModel;
/**
 * This is the min LOD that this component will use.  (e.g. if set to 2 then only 2+ LOD Models will be used.) This is useful to set on
 * meshes which are known to be a certain distance away and still want to have better LODs when zoomed in on them.
 **/
var() int		MinLodModel;
var	int			PredictedLODLevel;
var	int			OldPredictedLODLevel; // LOD level from previous frame, so we can detect changes in LOD to recalc required bones

/** If MaxDistanceFactor goes below this value (and it is non 0), start playing animations at a lower frame rate */
var() float     AnimationLODDistanceFactor;

/** Rate of update for skeletal meshes that are below AnimationLODDistanceFactor. For example if set to 3, animations will be updated once every three frames. */
var() int       AnimationLODFrameRate;

/**	High (best) DistanceFactor that was desired for rendering this SkeletalMesh last frame. Represents how big this mesh was in screen space   */
var const float		MaxDistanceFactor;

/** Index of the chunk to preview... If set to -1, all chunks will be rendered */
var editoronly transient int	ChunkIndexPreview;
/** Index of the section to preview... If set to -1, all section will be rendered */
var editoronly transient int	SectionIndexPreview;

var int			bForceWireframe;	// Forces the mesh to draw in wireframe mode.

/** If true, force the mesh into the reference pose - is an optimization. */
var int			bForceRefpose;
/** If bForceRefPose was set last tick. */
var int			bOldForceRefPose;

/** Skip UpdateSkelPose. */
var() bool		bNoSkeletonUpdate;

/** Draw the skeleton hierarchy for this skel mesh. */
var int			bDisplayBones;

/** Bool that enables debug drawing of the skeleton before it is passed to the physics. Useful for debugging animation-driven physics. */
var int			bShowPrePhysBones;

var int			bHideSkin;
var int			bForceRawOffset;
var int			bIgnoreControllers;
var	int			bTransformFromAnimParent;
var	const transient int	TickTag;
var	const transient int	InitTag;
var const transient int	CachedAtomsTag;

/**
 * Only instance Root Bone rigid body for physics. Mostly used by Vehicles.
 * Other Rigid Bodies are ignored for physics, but still considered for traces.
 */
var	const int	bUseSingleBodyPhysics;

var transient int	bRequiredBonesUpToDate;

/**
 *	If non-zero, skeletal mesh component will not update kinematic bones and bone springs when distance factor is greater than this (or has not been rendered for a while).
 *	This also turns off BlockRigidBody, so you do not get collisions with 'left behind' ragdoll setups.  Items will fall through
 * the world if you move too far away from them and they are in RigigBody.
 */
var float		MinDistFactorForKinematicUpdate;

/** Used to keep track of how many frames physics has been asleep for (when using PHYS_RigidBody). */
var transient int		FramesPhysicsAsleep;

/** <2 means no skip, 2 means every other frame, 3 means 1 out of three frames, etc  */
var const transient int SkipRateForTickAnimNodesAndGetBoneAtoms;

/** If TRUE, we will not tick the anim nodes */
var const transient bool bSkipTickAnimNodes;

/** If TRUE, we will not call GetBonesAtoms, and instead use cached data */
var const transient bool bSkipGetBoneAtoms;

/** If TRUE, then bSkipGetBoneAtoms is also true; we will interpolate cached data */
var const transient bool bInterpolateBoneAtoms;

/** If TRUE, there is at least one body in the current PhysicsAsset with a valid bone in the current SkeletalMesh */
var const transient bool bHasValidBodies;

/**
 * When true, if owned by a PHYS_RigidBody Actor, skip all update (bones and bounds) when physics are asleep.   This is a very top level
 * optimization flag for things we know are just physics (e.g. Kassets).  Lots of things have physics on them that are asleep while the
 * actor moves around the level and then are woken up.  Setting this flag to true will stop those actors from having any updates which is not
 * what we want in the general case.
 */
var bool		bSkipAllUpdateWhenPhysicsAsleep;

/** When true, skip using the physics asset etc. and always use the fixed bounds defined in the SkeletalMesh. */
var() bool		bComponentUseFixedSkelBounds;

/** 
 * When true, we will just using the bounds from our ParentAnimComponent.  This is useful for when we have a Mesh Parented
 * to the main SkelMesh (e.g. outline mesh or a full body overdraw effect that is toggled) that is always going to be the same
 * bounds as parent.  We want to do no calculations in that case.
 */
var() bool		bUseBoundsFromParentAnimComponent;

/** If TRUE, when updating bounds from a PhysicsAsset, consider _all_ BodySetups, not just those flagged with bConsiderForBounds. */
var() bool		bConsiderAllBodiesForBounds;

/** If true, update skeleton/attachments even when our Owner has not been rendered recently
 * @note if this is false, bone information may not be accurate, so be careful setting this to false if bone info is relevant to gameplay
 * @note you can use ForceSkelUpdate() to force an update
 * @note: In the output from SHOWSKELCOMPTICKTIME you want UpdatePoseTotal to be 0 when this is FALSE for a specific component
 */
var() bool bUpdateSkelWhenNotRendered;

/** If true, do not apply any SkelControls when owner has not been rendered recently. */
var bool bIgnoreControllersWhenNotRendered;

/** If true, tick anim nodes even when our Owner has not been rendered recently  */
var bool bTickAnimNodesWhenNotRendered;

/** If this is true, we are not updating kinematic bones and motors based on animation because the skeletal mesh is too far from any viewer. */
var	const bool bNotUpdatingKinematicDueToDistance;

/** force root motion to be discarded, no matter what the AnimNodeSequence(s) are set to do */
var() bool bForceDiscardRootMotion;
/** Call RootMotionProcessed notification on Owner */
var() bool bNotifyRootMotionProcessed;

/**
 * if TRUE, notify owning actor of root motion mode changes.
 * This calls the Actor.RootMotionModeChanged() event.
 * This is useful for synchronizing movements.
 * For intance, when using RMM_Translate, and the event is called, we know that root motion will kick in on next frame.
 * It is possible to kill in-game physics, and then use root motion seemlessly.
 */
var bool bRootMotionModeChangeNotify;

/**
 * if TRUE, the event RootMotionExtracted() will be called on this owning actor,
 * after root motion has been extracted, and before it's been used.
 * This notification can be used to alter extracted root motion before it is forwarded to physics.
 */
var bool bRootMotionExtractedNotify;

/** Flag set when processing root motion. */
var transient bool bProcessingRootMotion;

/** If true, FaceFX will not automatically create material instances. */
var() bool bDisableFaceFXMaterialInstanceCreation;

/** If true, disable FaceFX entirely for this component */
var() bool bDisableFaceFX;

/** If true, AnimTree has been initialised. */
var const transient bool bAnimTreeInitialised;

/** If TRUE, UpdateTransform will always result in a call to MeshObject->Update. */
var private transient bool	bForceMeshObjectUpdate;

/**
 *	Indicates whether this SkeletalMeshComponent should have a physics engine representation of its state.
 *	@see SetHasPhysicsAssetInstance
 */
var() const bool bHasPhysicsAssetInstance;

/** If we are running physics, should we update bFixed bones based on the animation bone positions. */
var() bool	bUpdateKinematicBonesFromAnimation;

/**
 *	If we should pass joint position to joints each frame, so that they can be used by motorized joints to drive the
 *	ragdoll based on the animation.
 */
var() bool	bUpdateJointsFromAnimation;

/** Indicates whether this SkeletalMeshComponent is currently considered 'fixed' (ie kinematic) */
var const bool	bSkelCompFixed;

/** Used for consistency checking. Indicates that the results of physics have been blended into SpaceBases this frame. */
var	const bool	bHasHadPhysicsBlendedIn;

/**
 *	If true, attachments will be updated twice a frame - once in Tick and again when UpdateTransform is called.
 *	This can resolve some 'frame behind' issues if an attachment need to be in the correct location for it's Tick, but at a cost.
 */
var() bool	bForceUpdateAttachmentsInTick;

/** Enables blending in of physics bodies with the bAlwaysFullAnimWeight flag set. (e.g. hair and other flappy bits!)*/
var transient bool	bEnableFullAnimWeightBodies;

/**
 *	If true, when this skeletal mesh overlaps a physics volume, each body of it will be tested against the volume, so only limbs
 *	actually in the volume will be affected. Useful when gibbing bodies.
 */
var() bool		bPerBoneVolumeEffects;

/**
 *	If true, use per-bone motion blur on this skeletal mesh.
 */
var() bool		bPerBoneMotionBlur;

/** If true, will move the Actors Location to match the root rigid body location when in PHYS_RigidBody. */
var() bool		bSyncActorLocationToRootRigidBody;

/** If TRUE, force usage of raw animation data when animating this skeletal mesh; if FALSE, use compressed data. */
var const bool	bUseRawData;

/** Disable warning when an AnimSequence is not found. FALSE by default. */
var bool		bDisableWarningWhenAnimNotFound;

/** if set, components that are attached to us have their bOwnerNoSee and bOnlyOwnerSee properties overridden by ours */
var bool bOverrideAttachmentOwnerVisibility;

/** if TRUE, when detach, send message to renderthread to delete this component from hit mask list **/
var const transient bool bNeedsToDeleteHitMask;

/** pauses this component's animations (doesn't tick them) */
var bool bPauseAnims;
/** If true, DistanceFactor for this SkeletalMeshComponent will be added to global chart. */
var bool	bChartDistanceFactor;
/** If TRUE, line checks will test against the bounding box of this skeletal mesh component and return a hit if there is a collision. */
var bool	bEnableLineCheckWithBounds;

/** Whether or not we can highlight selected sections - this should really only be done in the editor */
var transient bool bCanHighlightSelectedSections;

/** Update Morph when ParentAnimComponent exists **/
var() bool bUpdateMorphWhenParentAnimComponentExists;

/** If bEnableLineCheckWithBounds is TRUE, scale the bounds by this value before doing line check. */
var	vector	LineCheckBoundsScale;

// CLOTH bools

/**
 *	Whether cloth simulation should currently be used on this SkeletalMeshComponent.
 *	@see SetEnableClothSimulation
 */
var(Cloth)	const bool		bEnableClothSimulation;

/** Turns off all cloth collision so not checks are done (improves performance). */
var(Cloth)	const bool		bDisableClothCollision;

/** If true, cloth is 'frozen' and no simulation is taking place for it, though it will keep its shape. */
var(Cloth)	const bool		bClothFrozen;

/** If true, cloth will automatically have bClothFrozen set when it is not rendered, and have it turned off when it is seen. */
var(Cloth)	bool			bAutoFreezeClothWhenNotRendered;

/** If true, cloth will be awake when a level is started, otherwise it will be instantly put to sleep. */
var(Cloth)	bool			bClothAwakeOnStartup;

/** It true, clamp velocity of cloth particles to be within ClothBaseVelClampRange of Base velocity. */
var(Cloth)	bool			bClothBaseVelClamp;

/** It true, interp velocity of cloth particles towards Base velocity, using ClothBaseVelClampRange as the interp rate (0..1). */
var(Cloth)	bool			bClothBaseVelInterp;

/** If true, fixed verts of the cloth are attached in the physics to the physics body that this components actor is attached to. */
var(Cloth)	bool			bAttachClothVertsToBaseBody;

/** Whether this cloth is on a non-animating static object. */
var(Cloth)	bool			bIsClothOnStaticObject;
/** Whether we've updated fixed cloth verts since last attachment. */
var			bool			bUpdatedFixedClothVerts;

/** Whether should do positional box dampening */
var(Cloth)	bool			bClothPositionalDampening;
/** Whether wind direction is relative to owner rotation or not */
var(Cloth)	bool			bClothWindRelativeToOwner;

/**
 * TRUE if mesh has been recently rendered, FALSE otherwise
 */
var	transient bool bRecentlyRendered;

/** Should anim sequence nodes cache the calculated values when not actually playing an animation? */
var bool bCacheAnimSequenceNodes;

/** If TRUE, update the instanced vertex influences for this mesh during the next update */
var const transient bool bNeedsInstanceWeightUpdate;
/** If TRUE, always use instanced vertex influences for this mesh */
var const transient bool bAlwaysUseInstanceWeights;
/** TRUE if it needs to rebuild the required bones array for multi pass compose */
var const transient bool    bUpdateComposeSkeletonPasses;
/** Flag to remember if cache saved is valid or not to make sure Save/Restore always happens with a pair **/
var native transient const bool             bValidTemporarySavedAnimSets;

/** Usage cases for toggling vertex weights */
enum EInstanceWeightUsage
{
	/** Weights are swapped for a subset of vertices. Requires a unique weights vertex buffer per skel component instance. */
	IWU_PartialSwap,
	/** Weights are swapped for ALL vertices.  Shares a weights vertex buffer for all skel component instances. */
	IWU_FullSwap
};

/**
* Set of bones which will be used to find vertices to switch to using instanced influence weights
* instead of the default skeletal mesh weighting.
*/
struct BonePair
{
	var name Bones[2];
};
var native transient const array<BonePair> InstanceVertexWeightBones;

/** LOD specific setup for the skeletal mesh component */
struct SkelMeshComponentLODInfo
{
	/** Material corresponds to section. To show/hide each section, use this **/
	var const array<bool> HiddenMaterials;
	var const bool bNeedsInstanceWeightUpdate;
	var const bool bAlwaysUseInstanceWeights;
	/** Whether the instance weights are used for a partial/full swap */
	var const transient EInstanceWeightUsage InstanceWeightUsage;
	/** Current index into the skeletal mesh VertexInfluences for the current LOD */
	var const transient int InstanceWeightIdx;
};
var const transient array<SkelMeshComponentLODInfo> LODInfo;

// CLOTH

/** The state of the LocalToWorld pos at the point the cloth was frozen. */
var const vector			FrozenLocalToWorldPos;

/** The state of the LocalToWorld rotation at the point the cloth was frozen. */
var const rotator			FrozenLocalToWorldRot;

/** Constant force applied to all vertices in the cloth. */
var(Cloth)	const vector	ClothExternalForce;

/** 'Wind' force applied to cloth. Force on each vertex is based on the dot product between the wind vector and the surface normal. */
var(Cloth)	vector			ClothWind;

/**
 *	If bClothBaseVelClamp is TRUE, amount of variance from base's velocity the cloth is allowed.
 *	If bClothBaseVelInterp is TRUE, how fast cloth verts are pushed towards base velocity (0..1)
 */
var(Cloth)	vector			ClothBaseVelClampRange;

/** How much to blend in results from cloth simulation with results from regular skinning. */
var(Cloth)	float			ClothBlendWeight;

/** Cloth blend weight, controlled by distance from camera. */
var	float ClothDynamicBlendWeight;

/** Distance factor below which cloth should be fully animated. -1.0 indicates always physics. */
var(Cloth)	float			ClothBlendMinDistanceFactor;

/** Distance factor above which cloth should be fully simulated. */
var(Cloth)	float			ClothBlendMaxDistanceFactor;

/** Distance from the owner in relative frame (max == pos XYZ, min == neg XYZ) */
var(Cloth)	Vector			MinPosDampRange, MaxPosDampRange;
/** Dampening scale applied to cloth particle velocity when approaching boundaries of *PosDampRange */
var(Cloth)	Vector			MinPosDampScale, MaxPosDampScale;

var const native transient pointer	ClothSim;
var const native transient int		SceneIndex;

var const array<vector> ClothMeshPosData;
var const array<vector> ClothMeshNormalData;
var const array<int> ClothMeshIndexData;
var int NumClothMeshVerts;
var int NumClothMeshIndices;

/** Cloth parent indices contain the index of the original vertex when a vertex is created during tearing.
 *  If it is an original vertex then the parent index is the same as the vertex index.
 */
var const array<int> ClothMeshParentData;
var int NumClothMeshParentIndices;

/** buffers used for reverse lookups to unweld vertices to support wrapped UVs. */
var const native transient array<vector>	ClothMeshWeldedPosData;
var const native transient array<vector>	ClothMeshWeldedNormalData;
var const native transient array<int>		ClothMeshWeldedIndexData;

/** flags to indicate which buffers were recently updated by the cloth simulation. */
var int ClothDirtyBufferFlag;

/** Enum indicating what type of object this cloth should be considered for rigid body collision. */
var(Cloth)	const ERBCollisionChannel			ClothRBChannel;

/** Types of objects that this cloth will collide with. */
var(Cloth)	const RBCollisionChannelContainer	ClothRBCollideWithChannels;

/** How much force to apply to cloth, in relation to the force(from a force field) applied to rigid bodies(zero applies no force to cloth, 1 applies the same) */
var(Cloth)	const float				ClothForceScale;

/** Amount to scale impulses applied to cloth simulation. */
var(Cloth)	float					ClothImpulseScale;

/**
    The cloth tear factor for this SkeletalMeshComponent, negative values take the tear factor from the SkeletalMesh.
    Note: UpdateClothParams() should be called after modification so that the changes are reflected in the simulation.
*/
var(Cloth)	const float				ClothAttachmentTearFactor;

/** If TRUE, soft body uses compartment in physics scene (usually with fixed timstep for better behaviour) */
var(Cloth)	const bool				bClothUseCompartment;

/** If the distance traveled between frames exceeds this value the vertices will be reset to avoid stretching. */
var(Cloth)	const float				MinDistanceForClothReset;

/** Last location of our owner/base for checking MinDistanceForClothReset. */
var const transient vector LastClothLocation;

/** Enum indicating what type of object this apex clothing should be considered for rigid body collision. */
var(ApexClothing) const ERBCollisionChannel		ApexClothingRBChannel;

/** Types of objects that this clothing will collide with. */
var(ApexClothing)	const RBCollisionChannelContainer	ApexClothingRBCollideWithChannels;

/** Enum indicating what channel the apex clothing collision shapes should be placed in */
var(ApexClothing) const ERBCollisionChannel		ApexClothingCollisionRBChannel;

/** If true, the clothing actor will stop simulating when it is not rendered */
var(ApexClothing) bool								bAutoFreezeApexClothingWhenNotRendered;

/** If TRUE, WindVelocity is applied in the local space of the component, rather than world space. */
var(ApexClothing) bool                  bLocalSpaceWind;

/** The Wind Velocity applied to Apex Clothing */
var(ApexClothing) interp vector			WindVelocity;

/** Time taken for ApexClothing to reach WindVelocity */
var(ApexClothing) interp float			WindVelocityBlendTime;

/** Don't attempt to initialize clothing when component is attached */
var const transient bool							bSkipInitClothing;

/** Pointer to the simulated NxSoftBody object. */
var const native transient pointer					SoftBodySim;

/** Index of the Novodex scene the soft-body resides in. */
var const native transient int						SoftBodySceneIndex;

/** Whether soft-body simulation should currently be used on this SkeletalMeshComponent. */
var(Softbody) const bool							bEnableSoftBodySimulation;

/** Buffer of the updated tetrahedron-vertex positions. */
var const array<vector>								SoftBodyTetraPosData;

/** Buffer of the updated tetrahedron-indices. */
var const array<int>								SoftBodyTetraIndexData;

/** Number of tetrahedron vertices of the soft-body mesh. */
var int												NumSoftBodyTetraVerts;

/** Number of tetrahedron indices of the soft-body mesh (equal to four times the number of tetrahedra). */
var int												NumSoftBodyTetraIndices;

/** Amount to scale impulses applied to soft body simulation. */
var(SoftBody)	float								SoftBodyImpulseScale;

/** If true, the soft-body is 'frozen' and no simulation is taking place for it, though it will keep its shape. */
var(SoftBody)	const bool							bSoftBodyFrozen;

/** If true, the soft-body will automatically have bSoftBodyFrozen set when it is not rendered, and have it turned off when it is seen. */
var(SoftBody)	bool								bAutoFreezeSoftBodyWhenNotRendered;

/** If true, the soft-body will be awake when a level is started, otherwise it will be instantly put to sleep. */
var(SoftBody)	bool								bSoftBodyAwakeOnStartup;

/** If TRUE, soft body uses compartment in physics scene (usually with fixed timstep for better behaviour) */
var(SoftBody)	const bool							bSoftBodyUseCompartment;

/** Enum indicating what type of object this soft-body should be considered for rigid body collision. */
var(SoftBody)	const ERBCollisionChannel			SoftBodyRBChannel;

/** Types of objects that this soft-body will collide with. */
var(SoftBody)	const RBCollisionChannelContainer	SoftBodyRBCollideWithChannels;

/** Pointer to the Novodex plane-actor used when previewing the soft-body in the AnimSet Editor. */
var const native transient pointer					SoftBodyASVPlane;


var	material	LimitMaterial;

/** Root Motion extracted from animation. */
var	transient	BoneAtom	RootMotionDelta;
/** Root Motion velocity for this frame, set from RootMotionDelta. */
var			transient	Vector		RootMotionVelocity;

/**
 * Offset of the root bone from the reference pose.
 * Used to offset bounding box.
 */
var const transient Vector	RootBoneTranslation;

/** Scale applied in physics when RootMotionMode == RMM_Accel */
var vector RootMotionAccelScale;

enum ERootMotionMode
{
	RMM_Translate,	// move actor with root motion
	RMM_Velocity,	// extract magnitude from root motion, and limit max Actor velocity with it.
	RMM_Ignore,		// do nothing
	RMM_Accel,		// extract velocity from root motion and use it to derive acceleration of the Actor
	RMM_Relative,	// if bHardAttach is used, then affect relative location instead of location.
};
var() ERootMotionMode		RootMotionMode;
/** Previous Root Motion Mode, to catch changes */
var	const ERootMotionMode	PreviousRMM;

var		ERootMotionMode		PendingRMM;
var		ERootMotionMode		OldPendingRMM;

/** Handle one frame delay with PendingRMM */
var	const INT				bRMMOneFrameDelay;

/** Root Motion Rotation mode */
enum ERootMotionRotationMode
{
	/** Ignore rotation delta passed from animation. */
	RMRM_Ignore,
	/** Apply rotation delta to actor */
	RMRM_RotateActor,
};
var() ERootMotionRotationMode RootMotionRotationMode;

enum EAnimRotationOnly
{
	/** Use settings defined in each AnimSet (default) */
	EARO_AnimSet,
	/** Force AnimRotationOnly enabled on all AnimSets, but for this SkeletalMesh only */
	EARO_ForceEnabled,
	/** Force AnimRotationOnly disabled on all AnimSets, but for this SkeletalMesh only */
	EARO_ForceDisabled
};

/** SkeletalMeshComponent settings for AnimRotationOnly */
var() EAnimRotationOnly AnimRotationOnly;

enum EFaceFXBlendMode
{
	/**
	 * Face FX overwrites relevant bones on skeletal mesh.
	 * Default.
	 */
	FXBM_Overwrite,
	/**
	 * Face FX transforms are relative to ref skeleton and added
	 * in parent bone space.
	 */
	FXBM_Additive,
};

/** How FaceFX transforms should be blended with skeletal mesh */
var()	EFaceFXBlendMode	FaceFXBlendMode;

/** The valid FaceFX register operations. */
enum EFaceFXRegOp
{
	FXRO_Add,	   // Add the register value with the Face Graph node value.
	FXRO_Multiply, // Multiply the register value with the Face Graph node value.
	FXRO_Replace,  // Replace the Face Graph node value with the register value.
};

/** The valid BoneVisibilityStates values; A bone is only visible if it is *exactly* 1 */
enum EBoneVisibilityStatus
{
	BVS_HiddenByParent,      // Bone is hidden because it's parent is hidden
	BVS_Visible,             // Bone is visible
	BVS_ExplicitlyHidden,    // Bone is hidden directly
};

/** The FaceFX actor instance associated with the skeletal mesh component. */
var transient native pointer FaceFXActorInstance;

/**
 *	The audio component that we are using to play audio for a facial animation.
 *	Assigned in PlayFaceFXAnim and cleared in StopFaceFXAnim.
 */
var AudioComponent	CachedFaceFXAudioComp;

/** Array of bone visibilities (containing one of the values in EBoneVisibilityStatus for each bone).  A bone is only visible if it is *exactly* 1 (BVS_Visible) */
var	transient const array<byte>	BoneVisibilityStates;

/** Cache of LocalToWorld BoneAtom. */
var	transient const boneatom	LocalToWorldBoneAtom;

/** Editor only. Used for visualizing drawing order in Animset Viewer. If < 1.0,
  * only the specified fraction of triangles will be rendered
  */
var transient float ProgressiveDrawingFraction;

/** Editor only. Used for manually selecting the alternate indices for
  * TRISORT_CustomLeftRight sections.
  */
var transient byte CustomSortAlternateIndexMode;

/** Editor only. Used to keep track of the morph targets we've reported 
  * the user as having bad LODs (to prevent LOD spam)
  */
var transient array<Name>		MorphTargetsQueried;

/** PhysicsBody options when bone is hiddne */
enum EPhysBodyOp
{
	PBO_None, // don't do anything
	PBO_Term, // terminate - if you terminate, you won't be able to re-init when unhidden
	PBO_Disable, // disable collision - it will enable collision when unhidden
};

/*-----------------------------------------------------------------------------
	Tick time optimization.
  -----------------------------------------------------------------------------*/

/** Whether to use based on distance factor tick optimization. */
var(Optimization) const bool	bUseTickOptimization;

/** How many times this component was ticked. */
var const int					TickCount;

/** Last drop rate [0-2]. */
var transient const int			LastDropRate;

/** Time when LastDropRate changes, used to avoid 'flickering' when drop rates changes very frequently. */
var transient const float		LastDropRateChange;

/** Accumulated delta time when frames were dropped. */
var transient const float		AccumulatedDroppedDeltaTime;

/** Accumulated delta time when frames were dropped. */
var transient const float		ComponentDroppedDeltaTime;


//=============================================================================
// Animation.

// Attachment functions.
native final function AttachComponent(ActorComponent Component,name BoneName,optional vector RelativeLocation,optional rotator RelativeRotation,optional vector RelativeScale);
native final function DetachComponent(ActorComponent Component);


/**
 * Attach an ActorComponent to a Socket.
 */

native final function AttachComponentToSocket(ActorComponent Component, name SocketName);

/**
 *	Find the current world space location and rotation of a named socket on the skeletal mesh component.
 *	If the socket is not found, then it returns false and does not change the OutLocation/OutRotation variables.
 *	@param InSocketName the name of the socket to find
 *	@param OutLocation (out) set to the world space location of the socket
 *	@param OutRotation (out) if specified, set to the world space rotation of the socket
 *	@return whether or not the socket was found
 */
native final function bool GetSocketWorldLocationAndRotation(name InSocketName, out vector OutLocation, optional out rotator OutRotation, optional int Space ); // 0 == World, 1 == Local (Component)


/**
 * Returns SkeletalMeshSocket of named socket on the skeletal mesh component.
 * Returns None if not found.
 */

native final function SkeletalMeshSocket GetSocketByName( Name InSocketName );

/**
 * Returns bone name linked to a given named socket on the skeletal mesh component.
 * If you're unsure to deal with sockets or bones names, you can use this function to filter through, and always return the bone name.
 * @input	bone name or socket name
 * @output	bone name
 */
native final function Name GetSocketBoneName(Name InSocketName);

/**
 * Returns component attached to specified BoneName. (returns the first entry found).
 * @param	BoneName		Bone Name to look up.
 * @return	First ActorComponent found, attached to BoneName, if it exists.
 */

native final function ActorComponent FindComponentAttachedToBone( name InBoneName );


/**
 * Returns true if component is attached to skeletal mesh.
 * @param	Component	ActorComponent to check for.
 * @return	true if Component is attached to SkeletalMesh.
 */
native final function bool IsComponentAttached( ActorComponent Component, optional Name BoneName );

/** returns all attached components that are of the specified class or a subclass
 * @param BaseClass the base class of ActorComponent to return
 * @param (out) OutComponent the returned ActorComponent for each iteration
 */
native final iterator function AttachedComponents(class<ActorComponent> BaseClass, out ActorComponent OutComponent);

/**
 * Return Transform Matrix for SkeletalMeshComponent considering root motion setups
 * 
 * @param SkelComp SkeletalMeshComponent to get transform matrix from
 */
native final function Matrix GetTransformMatrix();

// Skeletal animation.

/** Change the SkeletalMesh that is rendered for this Component. Will re-initialize the animation tree etc. */
simulated native final function SetSkeletalMesh(SkeletalMesh NewMesh, optional bool bKeepSpaceBases);

/** Change the Physics Asset of the mesh */
simulated native final function SetPhysicsAsset(PhysicsAsset NewPhysicsAsset, optional bool bForceReInit);

/** Change whether to force mesh into ref pose (and use cheaper vertex shader) */
simulated native final function SetForceRefPose(bool bNewForceRefPose);

// Cloth

/** Turn on and off cloth simulation for this skeletal mesh. */
simulated native final function SetEnableClothSimulation(bool bInEnable);

/** Toggle active simulation of cloth. Cheaper than doing SetEnableClothSimulation, and keeps its shape while frozen. */
simulated native final function SetClothFrozen(bool bNewFrozen);

/** Toggle active simulation of clothing and keeps its shape while frozen. */
simulated native final function SetEnableClothingSimulation(bool bInEnable);

/** Update params of the this components internal cloth sim from the SkeletalMesh properties. */
simulated native final function UpdateClothParams();

/** Modify the external force that is applied to the cloth. Will continue to be applied until it is changed. */
simulated native final function SetClothExternalForce(vector InForce);

/** Attach/detach verts from physics body that this components actor is attached to. */
simulated native final function SetAttachClothVertsToBaseBody(bool bAttachVerts);

/** Move all vertices in the cloth to the reference pose and zero their velocity. */
simulated native final function ResetClothVertsToRefPose();

/** Forces apex clothing to use 'teleport and reset' for the next update */
simulated native final function ForceApexClothingTeleportAndReset();
/** Forces apex clothing to use 'teleport' for the next update */
simulated native final function ForceApexClothingTeleport();

//Some get*() APIs
simulated native final function float GetClothAttachmentResponseCoefficient();
simulated native final function float GetClothAttachmentTearFactor();
simulated native final function float GetClothBendingStiffness();
simulated native final function float GetClothCollisionResponseCoefficient();
simulated native final function float GetClothDampingCoefficient();
simulated native final function int GetClothFlags();
simulated native final function float GetClothFriction();
simulated native final function float GetClothPressure();
simulated native final function float GetClothSleepLinearVelocity();
simulated native final function int GetClothSolverIterations();
simulated native final function float GetClothStretchingStiffness();
simulated native final function float GetClothTearFactor();
simulated native final function float GetClothThickness();
//some set*() APIs
simulated native final function SetClothAttachmentResponseCoefficient(float ClothAttachmentResponseCoefficient);
simulated native final function SetClothAttachmentTearFactor(float ClothAttachTearFactor);
simulated native final function SetClothBendingStiffness(float ClothBendingStiffness);
simulated native final function SetClothCollisionResponseCoefficient(float ClothCollisionResponseCoefficient);
simulated native final function SetClothDampingCoefficient(float ClothDampingCoefficient);
simulated native final function SetClothFlags(int ClothFlags);
simulated native final function SetClothFriction(float ClothFriction);
simulated native final function SetClothPressure(float ClothPressure);
simulated native final function SetClothSleepLinearVelocity(float ClothSleepLinearVelocity);
simulated native final function SetClothSolverIterations(int ClothSolverIterations);
simulated native final function SetClothStretchingStiffness(float ClothStretchingStiffness);
simulated native final function SetClothTearFactor(float ClothTearFactor);
simulated native final function SetClothThickness(float ClothThickness);
//Other APIs
simulated native final function SetClothSleep(bool IfClothSleep);
simulated native final function SetClothPosition(vector ClothOffSet);
simulated native final function SetClothVelocity(vector VelocityOffSet);
//Attachment API
simulated native final function AttachClothToCollidingShapes(bool AttatchTwoWay, bool AttachTearable);
//ValidBounds APIs
simulated native final function EnableClothValidBounds(bool IfEnableClothValidBounds);
simulated native final function SetClothValidBounds(vector ClothValidBoundsMin, vector ClothValidBoundsMax);

//SoftBody

/** Update soft-body simulation from components params. */
simulated native final function UpdateSoftBodyParams();

/** Toggle active simulation of the soft-body. */
simulated native final function SetSoftBodyFrozen(bool bNewFrozen);

/** Force awake any soft body simulation on this component */
simulated native final function WakeSoftBody();

/**
 * Find a named AnimSequence from the AnimSets array in the SkeletalMeshComponent.
 * This searches array from end to start, so specific sequence can be replaced by putting a set containing a sequence with the same name later in the array.
 *
 * @param AnimSeqName Name of AnimSequence to look for.
 *
 * @return Pointer to found AnimSequence. Returns NULL if could not find sequence with that name.
 */
native final function AnimSequence FindAnimSequence( Name AnimSeqName );


/**
 * Saves the skeletal component's current AnimSets to a temporary buffer.  You can restore them later by calling
 * RestoreSavedAnimSets().
 */
native final function SaveAnimSets();

/**
 * Restores saved AnimSets to the master list of AnimSets and clears the temporary saved list of AnimSets.
 */
native final function RestoreSavedAnimSets();


/**
 * Finds play Rate for a named AnimSequence to match a specified Duration in seconds.
 *
 * @param	AnimSeqName	Name of AnimSequence to look for.
 * @param	Duration	in seconds to match
 *
 * @return	play rate of animation, so it plays in <duration> seconds.
 */
final function float GetAnimRateByDuration( Name AnimSeqName, float Duration )
{
	local AnimSequence AnimSeq;

	AnimSeq = FindAnimSequence( AnimSeqName );
	if( AnimSeq == None || AnimSeq.RateScale <= 0.f )
	{
		return 1.f;
	}

	return (AnimSeq.SequenceLength / (Duration * AnimSeq.RateScale));
}


/** Returns the duration (in seconds) for a named AnimSequence. Returns 0.f if no animation. */
final function float GetAnimLength(Name AnimSeqName)
{
	local AnimSequence AnimSeq;

	AnimSeq = FindAnimSequence(AnimSeqName);
	if( AnimSeq == None || AnimSeq.RateScale <= 0.f )
	{
		return 0.f;
	}

	return (AnimSeq.SequenceLength / AnimSeq.RateScale);
}


/**
 * Find a named MorphTarget from the MorphSets array in the SkeletalMeshComponent.
 * This searches the array in the same way as FindAnimSequence
 *
 * @param AnimSeqName Name of MorphTarget to look for.
 *
 * @return Pointer to found MorphTarget. Returns NULL if could not find target with that name.
 */
native final function MorphTarget FindMorphTarget( Name MorphTargetName );

/**
 * Find an Animation Node in the Animation Tree whose NodeName matches InNodeName.
 * Warning: The search is O(n), so for large AnimTrees, cache result.
 */
native final function	AnimNode			FindAnimNode(name InNodeName);

/** returns all AnimNodes in the animation tree that are the specfied class or a subclass
 * @param BaseClass base class to return
 * @param Node (out) the returned AnimNode for each iteration
 */
native final iterator function AllAnimNodes(class<AnimNode> BaseClass, out AnimNode Node);

native final function	SkelControlBase		FindSkelControl( name InControlName );

native final function	MorphNodeBase		FindMorphNode( name InNodeName );

native final function quat GetBoneQuaternion( name BoneName, optional int Space ); // 0 == World, 1 == Local (Component)
native final function vector GetBoneLocation( name BoneName, optional int Space ); // 0 == World, 1 == Local (Component)

/** returns the bone index of the specified bone, or INDEX_NONE if it couldn't be found */
native final function int MatchRefBone( name BoneName );
/** @return the name of the bone at the specified index */
native final function name GetBoneName(int BoneIndex);

/** returns the matrix of the bone at the specified index */
native final function matrix GetBoneMatrix( int BoneIndex );

/** returns the name of the parent bone for the specified bone. Returns 'None' if the bone does not exist or it is the root bone */
native final function name GetParentBone(name BoneName);

/** fills the given array with the names of all the bones in this component's current SkeletalMesh */
native final function GetBoneNames(out array<name> BoneNames);

/**
 * Tests if BoneName is child of (or equal to) ParentBoneName.
 * Note - will return FALSE if ChildBoneIndex is the same as ParentBoneIndex ie. must be strictly a child.
 */
native final function bool BoneIsChildOf(name BoneName, name ParentBoneName);

/** Gets the local-space position of a bone in the reference pose. */
native final function vector GetRefPosePosition(int BoneIndex);

/** finds a vector pointing along the given axis of the given bone
 * @param BoneName the name of the bone to find
 * @param Axis the axis of that bone to return
 * @return the direction of the specified axis, or (0,0,0) if the specified bone was not found
 */
native final function vector GetBoneAxis(name BoneName, EAxis Axis);

/**
 *	Transform a location/rotation from world space to bone relative space.
 *	This is handy if you know the location in world space for a bone attachment, as AttachComponent takes location/rotation in bone-relative space.
 */
native final function TransformToBoneSpace( name BoneName, vector InPosition, rotator InRotation, out vector OutPosition, out rotator OutRotation );

/**
 *	Transform a location/rotation in bone relative space to world space.
 */
native final function TransformFromBoneSpace( name BoneName, vector InPosition, rotator InRotation, out vector OutPosition, out rotator OutRotation );

/** finds the closest bone to the given location
 * @param TestLocation the location to test against
 * @param BoneLocation (optional, out) if specified, set to the world space location of the bone that was found, or (0,0,0) if no bone was found
 * @param IgnoreScale (optional) if specified, only bones with scaling larger than the specified factor are considered
 * @return the name of the bone that was found, or 'None' if no bone was found
 */
native final function name FindClosestBone(vector TestLocation, optional out vector BoneLocation, optional float IgnoreScale);

/** iterates through all bodies in our PhysicsAsset and returns the location of the closest bone associated
 * with a body that blocks the specified kind of traces
 * @note: only the collision flags on the PhysicsAsset are checked; the collision flags on the component are ignored
 * @param TestLocation - location to check against
 * @param bCheckZeroExtent - consider bodies that block zero extent traces
 * @param bCheckNonZeroExtent - consider bodies that block nonzero extent traces
 * @return location of closest colliding bone, or (0,0,0) if there were no bodies to test
 */
native final function vector GetClosestCollidingBoneLocation(vector TestLocation, bool bCheckZeroExtent, bool bCheckNonZeroExtent);

native final function SetAnimTreeTemplate(AnimTree NewTemplate);
native final function SetParentAnimComponent(SkeletalMeshComponent NewParentAnimComp);

native final function UpdateParentBoneMap();
native final function InitSkelControls();
/**
*	Initialize MorphSets look up table : MorphTargetIndexMap
*/
native final function InitMorphTargets();

final native function int	FindConstraintIndex(name ConstraintName);
final native function name	FindConstraintBoneName(int ConstraintIndex);

/** Find a BodyInstance by BoneName */
final native function	RB_BodyInstance	FindBodyInstanceNamed(Name BoneName);

/**
 *	Set value of bHasPhysicsAssetInstance flag.
 *	Will create/destroy PhysicsAssetInstance as desired.
 *	
 * @param bHasInstance - Sets value of flag
 * @param bUseCurrentPosition - If true, skip the skeletal update and use current positions
 */
final native function SetHasPhysicsAssetInstance(bool bHasInstance, optional bool bUseCurrentPosition);

/** Force an update of this meshes kinematic bodies and springs. */
native final function UpdateRBBonesFromSpaceBases(bool bMoveUnfixedBodies, bool bTeleport);

/** forces an update to the mesh's skeleton/attachments, even if bUpdateSkelWhenNotRendered is false and it has not been recently rendered
 * @note if bUpdateSkelWhenNotRendered is true, there is no reason to call this function (but doing so anyway will have no effect)
 */
native final function ForceSkelUpdate();

/**
 * Force AnimTree to recache all animations.
 * Call this when the AnimSets array has been changed.
 */
native final function UpdateAnimations();

/**
 *	Find all bones by name within given radius
 */
native final function bool GetBonesWithinRadius( Vector Origin, FLOAT Radius, INT TraceFlags, out array< Name > out_Bones );

/**
 * Add a new bone to the list of instance vertex weight bones
 *
 * @param BoneNames - set of bones (implicitly parented) to use for finding vertices
 */
native final function AddInstanceVertexWeightBoneParented(name BoneName, optional bool bPairWithParent = TRUE);

/**
 * Remove a new bone to the list of instance vertex weight bones
 *
 * @param BoneNames - set of bones (implicitly parented) to use for finding vertices
 */
native final function RemoveInstanceVertexWeightBoneParented(name BoneName);

/**
 * Find an existing bone pair entry in the list of InstanceVertexWeightBones
 *
 * @param Bones - pair of bones to search for
 * @return index of entry found or -1 if not found
 */
native final function int FindInstanceVertexweightBonePair(BonePair Bones);

/**
 * Update the bones that specify which vertices will use instanced influences
 * This will also trigger an update of the vertex weights.
 *
 * @param BonePairs - set of bone pairs to use for finding vertices.
 * A bone can be paired with None bone name to only match up a single bone.
 */
native final function UpdateInstanceVertexWeightBones(array<BonePair> BonePairs);

/**
 * Enabled or disable the instanced vertex weights buffer for the skeletal mesh object
 *
 * @param bEnable - TRUE to enable, FALSE to disable
 * @param LODIdx - LOD to enable
 */
native final function ToggleInstanceVertexWeights(bool bEnable, INT LODIdx);

// FaceFX.

/**
 * Play the specified FaceFX animation.
 * Returns TRUE if successful.
 * If animation couldn't be found, a log warning will be issued.
 */
native final function bool PlayFaceFXAnim(FaceFXAnimSet FaceFXAnimSetRef, string AnimName, string GroupName, SoundCue SoundCueToPlay);

/** Stop any currently playing FaceFX animation. */
native final function StopFaceFXAnim();

/** Is playing a FaceFX animation. */
native final function bool IsPlayingFaceFXAnim();

/** Declare a new register in the FaceFX register system.  This is required
  * before using the register name in GetRegister() or SetRegister(). */
native final function DeclareFaceFXRegister( string RegName );

/** Retrieve the value of the specified FaceFX register. */
native final function float GetFaceFXRegister( string RegName );

/** Set the value and operation of the specified FaceFX register. */
native final function SetFaceFXRegister( string RegName, float RegVal, EFaceFXRegOp RegOp, optional float InterpDuration );
/** Set the value and operation of the specified FaceFX register. */
native final function SetFaceFXRegisterEx( string RegName, EFaceFXRegOp RegOp, float FirstValue, float FirstInterpDuration, float NextValue, float NextInterpDuration );

/**
 *	Hides the specified bone.  Currently this just enforces a scale of 0 for the hidden bones.
 *	@param	PhysBodyOption		Option for physics bodies that attach to the bones to be hidden
 */
native final function HideBone( int BoneIndex, EPhysBodyOp PhysBodyOption );
/** Unhides the specified bone. */
native final function UnHideBone( int BoneIndex );
/** Determines if the specified bone is hidden. */
native final function bool IsBoneHidden( int BoneIndex );

/**
 *	Hides the specified bone with name.  Currently this just enforces a scale of 0 for the hidden bones.
 *	Compoared to HideBone By Index - This keeps track of list of bones and update when LOD changes
 *	@param  BoneName            Name of bone to hide
 *	@param	PhysBodyOption		Option for physics bodies that attach to the bones to be hidden
 */
native final function HideBoneByName( name BoneName, EPhysBodyOp PhysBodyOption );

/**
 *	UnHide the specified bone with name.  Currently this just enforces a scale of 0 for the hidden bones.
 *	Compoared to HideBone By Index - This keeps track of list of bones and update when LOD changes
 *	@param  BoneName            Name of bone to unhide
 */
native final function UnHideBoneByName( name BoneName );

/**
* Looks up all bodies for broken constraints.
* Makes sure child bodies of a broken constraints are not fixed and using bone springs, and child joints not motorized.
*/
simulated final native function UpdateMeshForBrokenConstraints();

/**
 *  Show/Hide Material - technical correct name for this is Section, but seems Material is mostly used
 *  This disable rendering of certain Material ID (Section)
 *
 * @param MaterialID - id of the material to match a section on and to show/hide
 * @param bShow - TRUE to show the section, otherwise hide it
 * @param LODIndex - index of the lod entry since material mapping is unique to each LOD
 */
simulated final native function ShowMaterialSection(int MaterialID, bool bShow, int LODIndex);


/** simple generic case animation player
 * requires that the one and only animation node in the AnimTree is an AnimNodeSequence
 * @param AnimName name of the animation to play
 * @param Duration (optional) override duration for the animation
 * @param bLoop (optional) whether the animation should loop
 * @param bRestartIfAlreadyPlaying whether or not to restart the animation if the specified anim is already playing
 * @param StartTime (optional) What time to start the animation at
 * @param bPlayBackwards (optional) Play this animation backwards
 */
function PlayAnim(name AnimName, optional float Duration, optional bool bLoop, optional bool bRestartIfAlreadyPlaying = true, optional float StartTime=0.0f, optional bool bPlayBackwards=false)
{
	local AnimNodeSequence AnimNode;
	local float DesiredRate;

	AnimNode = AnimNodeSequence(Animations);
	if (AnimNode == None && Animations.IsA('AnimTree'))
	{
		AnimNode = AnimNodeSequence(AnimTree(Animations).Children[0].Anim);
	}
	if (AnimNode == None)
	{
		`warn("Base animation node is not an AnimNodeSequence (Owner:" @ Owner $ ")");
	}
	else
	{
		if (AnimNode.AnimSeq != None && AnimNode.AnimSeq.SequenceName == AnimName)
		{
			DesiredRate = (Duration > 0.0) ? (AnimNode.AnimSeq.SequenceLength / (Duration * AnimNode.AnimSeq.RateScale)) : 1.0;
			DesiredRate = (bPlayBackwards) ? -DesiredRate : DesiredRate;
			if (bRestartIfAlreadyPlaying || !AnimNode.bPlaying)
			{
				AnimNode.PlayAnim(bLoop, DesiredRate, StartTime);
			}
			else
			{
				AnimNode.Rate = DesiredRate;
				AnimNode.bLooping = bLoop;
			}
		}
		else
		{
			AnimNode.SetAnim(AnimName);
			if (AnimNode.AnimSeq != None)
			{
				DesiredRate = (Duration > 0.0) ? (AnimNode.AnimSeq.SequenceLength / (Duration * AnimNode.AnimSeq.RateScale)) : 1.0;
				DesiredRate = (bPlayBackwards) ? -DesiredRate : DesiredRate;
				AnimNode.PlayAnim(bLoop, DesiredRate, StartTime);
			}
		}
	}
}

/** simple generic case animation stopper
 * requires that the one and only animation node in the AnimTree is an AnimNodeSequence
 */
function StopAnim()
{
	local AnimNodeSequence AnimNode;

	AnimNode = AnimNodeSequence(Animations);
	if (AnimNode == None && Animations.IsA('AnimTree'))
	{
		AnimNode = AnimNodeSequence(AnimTree(Animations).Children[0].Anim);
	}
	if (AnimNode == None)
	{
		`warn("Base animation node is not an AnimNodeSequence (Owner:" @ Owner $ ")");
	}
	else
	{
		AnimNode.StopAnim();
	}
}

/**
 * Called by AnimNotify_ForceField
 * Looks for a socket name first then bone name
 *
 * @param AnimNotifyData The AnimNotify_ForceField which will have all of the various params on it
 *
 *	@return	bool		true if the forcefield was created, false if not;
 */

event bool CreateForceField( const AnimNotify_Forcefield AnimNotifyData )
{
	local NxForceFieldComponent NewForceFieldComponent;
	
	NewForceFieldComponent = 
		new(self) AnimNotifyData.ForceFieldComponent.Class(AnimNotifyData.ForceFieldComponent);
		
	NewForceFieldComponent.DoInitRBPhys();
	if( AnimNotifyData.SocketName != '' )
	{
		AttachComponentToSocket(  NewForceFieldComponent, AnimNotifyData.SocketName );
	}
	else if( AnimNotifyData.BoneName != '' )
	{
		AttachComponent(  NewForceFieldComponent, AnimNotifyData.BoneName );
	}

	return true;
}

/**
* Called by AnimNotify_PlayParticleEffect
* Looks for a socket name first then bone name
*
* @param AnimNotifyData The AnimNotify_PlayParticleEffect which will have all of the various params on it
*/
event bool PlayParticleEffect( const AnimNotify_PlayParticleEffect AnimNotifyData )
{
	local vector Loc;
	local rotator Rot;
	local WorldInfo WI;
	local ParticleSystemComponent PSC;
	local bool bPlayNonExtreme;

	WI = class'WorldInfo'.static.GetWorldInfo();
	if (WI.NetMode == NM_DedicatedServer)
	{
		`Log("(SkeletalMeshComponent): PlayParticleEffect on dedicated server!");
		return true;
	}

	if (AnimNotifyData.PSTemplate == none)
	{
		return false;
	}

	// should I play non extreme content?
	bPlayNonExtreme = ( AnimNotifyData.bIsExtremeContent && class'Engine'.static.IsGame() && !WI.GRI.ShouldShowGore() ) ;


	// if we should not respond to anim notifies OR if this is extreme content and we can't show extreme content then return
	if ( bPlayNonExtreme && AnimNotifyData.PSNonExtremeContentTemplate==None )
	{
		return false;
	}

	// now go ahead and spawn the particle system based on whether we need to attach it or not
	if (AnimNotifyData.bAttach)
	{
		PSC = new(self) class'ParticleSystemComponent';  // move this to the object pool once it can support attached to bone/socket and relative translation/rotation
		if ( bPlayNonExtreme )
		{
			PSC.SetTemplate( AnimNotifyData.PSNonExtremeContentTemplate );
		}
		else
		{
			PSC.SetTemplate( AnimNotifyData.PSTemplate );
		}

		if( AnimNotifyData.SocketName != '' )
		{
			//`log( "attaching AnimNotifyData.SocketName" );
			AttachComponentToSocket( PSC, AnimNotifyData.SocketName );
		}
		else if( AnimNotifyData.BoneName != '' )
		{
			//`log( "attaching AnimNotifyData.BoneName" );
			AttachComponent( PSC, AnimNotifyData.BoneName );
		}

		PSC.ActivateSystem(true);
		PSC.OnSystemFinished = SkelMeshCompOnParticleSystemFinished;
	}
	else
	{
		// find the location
		if( AnimNotifyData.SocketName != '' )
		{
			GetSocketWorldLocationAndRotation( AnimNotifyData.SocketName, Loc, Rot );
		}
		else if( AnimNotifyData.BoneName != '' )
		{
			Loc = GetBoneLocation( AnimNotifyData.BoneName );
			Rot = rot(0,0,1);
		}
		else
		{
			Loc = GetPosition();
			Rot = rot(0,0,1);
		}

		if (Owner != None && Owner.WorldInfo != None && Owner.WorldInfo.MyEmitterPool != None)
		{

			if ( bPlayNonExtreme )
			{
				Owner.WorldInfo.MyEmitterPool.SpawnEmitter( AnimNotifyData.PSNonExtremeContentTemplate, Loc, Rot );
			}
			else
			{
				Owner.WorldInfo.MyEmitterPool.SpawnEmitter( AnimNotifyData.PSTemplate, Loc, Rot );
			}
		}
		else
		{
			if (class'Engine'.static.IsGame())
			{
				PSC = new(self) class'ParticleSystemComponent';  // move this to the object pool once it can support attached to bone/socket and relative translation/rotation
				PSC.SetTemplate( AnimNotifyData.PSTemplate );
				PSC.SetAbsolute(true, true, true);
				PSC.SetTranslation( Loc );
				PSC.SetRotation( Rot );
				PSC.ActivateSystem(true);
				PSC.OnSystemFinished = SkelMeshCompOnParticleSystemFinished;
			}
			else if (class'Engine'.static.IsEditor())
			{
				PSC = new(self) class'ParticleSystemComponent';  // move this to the object pool once it can support attached to bone/socket and relative translation/rotation
				PSC.SetTemplate( AnimNotifyData.PSTemplate );
				PSC.SetAbsolute(true, true, true);
				PSC.SetTranslation( Loc );
				PSC.SetRotation( Rot );

				if( AnimNotifyData.SocketName != '' )
				{
					//`log( "attaching AnimNotifyData.SocketName" );
					AttachComponentToSocket( PSC, AnimNotifyData.SocketName );
				}
				else if( AnimNotifyData.BoneName != '' )
				{
					//`log( "attaching AnimNotifyData.BoneName" );
					AttachComponent( PSC, AnimNotifyData.BoneName );
				}

				PSC.ActivateSystem(true);
				PSC.OnSystemFinished = SkelMeshCompOnParticleSystemFinished;
			}
			else
			{
				// It should *never* get in here... but just in case
				return false;
			}
		}
	}

	if( PSC != None && AnimNotifyData.BoneSocketModuleActorName != '' && Owner != None )
	{
		PSC.SetActorParameter(AnimNotifyData.BoneSocketModuleActorName, Owner);
	}

	return true;
}


/** We so we detach the Component once we are done playing it **/
simulated function SkelMeshCompOnParticleSystemFinished( ParticleSystemComponent PSC )
{
	DetachComponent( PSC );
}

/** Break a constraint off a Gore mesh. */
simulated final function BreakConstraint(Vector Impulse, Vector HitLocation, Name InBoneName, optional bool bVelChange)
{
	local int					ConstraintIndex, LODIdx;
	local RB_ConstraintInstance	Constraint;
	local RB_ConstraintSetup	ConstraintSetup;
	local RB_BodyInstance		Body;


	// you can enable/disable the instanced weights by calling
	ConstraintIndex = FindConstraintIndex(InBoneName);
	if( ConstraintIndex == INDEX_NONE )
	{
		return;
	}

	Constraint = PhysicsAssetInstance.Constraints[ConstraintIndex];
	// If already broken, our job has already been done. Bail!
	if( Constraint.bTerminated )
	{
		return;
	}

	// you can enable/disable the instanced weights by calling
	for (LODIdx=0; LODIdx<LODInfo.length;LODIdx++)
	{
		if (LODInfo[LODIdx].InstanceWeightUsage == IWU_PartialSwap)
		{
		   ToggleInstanceVertexWeights( TRUE, LODIdx );
		}
	}

	AddInstanceVertexWeightBoneParented( InBoneName );

	// Figure out if Body is fixed or not
	ConstraintSetup = PhysicsAsset.ConstraintSetup[Constraint.ConstraintIndex];
	Body = FindBodyInstanceNamed(ConstraintSetup.JointName);


	if( Body != None && Body.IsFixed() )
	{
		// Unfix body so it can be broken.
		Body.SetFixed(FALSE);
	}

	// Break Constraint
	Constraint.TermConstraint();
	// Make sure child bodies and constraints are released and turned to physics.
	UpdateMeshForBrokenConstraints();
	// Add impulse to broken limb
	AddImpulse(Impulse, HitLocation, InBoneName, bVelChange);
}

/** Returns position of the component, in world space. */
native function vector GetPosition();
/** Returns rotation of the component, in world space. */
native function rotator GetRotation();
native virtual function SetMaterial(int ElementIndex, MaterialInterface Material);

defaultproperties
{
	GlobalAnimRateScale=1.0

	// Various physics related items need to be ticked pre physics update
	TickGroup=TG_PreAsyncWork

	RootMotionMode=RMM_Ignore
	PreviousRMM=RMM_Ignore
	RootMotionRotationMode=RMRM_Ignore
	FaceFXBlendMode=FXBM_Additive

	AnimRotationOnly=EARO_AnimSet

	ChunkIndexPreview=-1
	SectionIndexPreview=-1
	
	WireframeColor=(R=221,G=221,B=28,A=255)
	bTransformFromAnimParent=1
	// by default, update kinematic when the mesh is far in the distance as things falling out of the world and are hard to track down
	MinDistFactorForKinematicUpdate=0.0f
	bNoSkeletonUpdate=FALSE
	bUpdateSkelWhenNotRendered=TRUE
	bTickAnimNodesWhenNotRendered=TRUE
	bAutoFreezeClothWhenNotRendered=TRUE
	bUpdateKinematicBonesFromAnimation=TRUE
	bSyncActorLocationToRootRigidBody=TRUE

	TickCount=0
	bUseTickOptimization=TRUE

	// re-rendering mesh for each decal is slow, and sometimes can't even be seen
 	bAcceptsStaticDecals=FALSE
 	bAcceptsDynamicDecals=FALSE

	RootMotionAccelScale=(X=1.f,Y=1.f,Z=1.f)

	StreamingDistanceMultiplier=1.0

	bClothUseCompartment=FALSE
	bClothAwakeOnStartup=FALSE
	ClothRBChannel=RBCC_Cloth
	ClothBlendWeight=1.0
	ClothImpulseScale=1.0

	ClothBlendMinDistanceFactor=-1.0

	ClothAttachmentTearFactor=-1.0
	MinDistanceForClothReset=256.f

	ApexClothingRBChannel=RBCC_Clothing
	ApexClothingCollisionRBChannel=RBCC_ClothingCollision
	ApexClothingRBCollideWithChannels=(Default=TRUE,BlockingVolume=TRUE,GameplayPhysics=TRUE,EffectPhysics=TRUE,ClothingCollision=TRUE)
	bAutoFreezeApexClothingWhenNotRendered=TRUE
	bSoftBodyAwakeOnStartup=FALSE
	SoftBodyRBChannel=RBCC_SoftBody
	SoftBodyImpulseScale=1.0
	bSoftBodyUseCompartment=TRUE

    bCacheAnimSequenceNodes=TRUE

	LineCheckBoundsScale=(X=1,Y=1,Z=1)

	ProgressiveDrawingFraction=1.0
	
	bCanHighlightSelectedSections=false;

	AnimationLODFrameRate=2

	//debug
	//bDisplayBones=1
}
