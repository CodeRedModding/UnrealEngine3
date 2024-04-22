/*=============================================================================
	UnSkeletalMesh.h: Unreal skeletal mesh objects.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	USkeletalMeshComponent.
-----------------------------------------------------------------------------*/

#ifndef __UNSKELETALMESH_H__
#define __UNSKELETALMESH_H__

#if WITH_FACEFX
	#include "UnFaceFXSupport.h"
#endif // WITH_FACEFX

#include "GPUSkinPublicDefs.h"

#include "NvApexManager.h"

#include "EnginePhysicsClasses.h"

// Forward references the FIApexClothing interface pointer, which is only valid if WITH_APEX is enabled.
class FIApexClothing; 


// Define that controls showing chart of distance factors for skel meshes during entire run of the game on exit.
#define CHART_DISTANCE_FACTORS 0

//
//	FAttachment
//
struct FAttachment
{
	UActorComponent*	Component;
	FName				BoneName;
	FVector				RelativeLocation;
	FRotator			RelativeRotation;
	FVector				RelativeScale;

	// Constructor.

	FAttachment(UActorComponent* InComponent,FName InBoneName,FVector InRelativeLocation,FRotator InRelativeRotation,FVector InRelativeScale):
		Component(InComponent),
		BoneName(InBoneName),
		RelativeLocation(InRelativeLocation),
		RelativeRotation(InRelativeRotation),
		RelativeScale(InRelativeScale)
	{}
	FAttachment() {}
};
template <> struct TIsPODType<FAttachment> { enum { Value = true }; };

//
//	FBoneRotationControl
//
struct FBoneRotationControl
{
	INT			BoneIndex;
	FName		BoneName;
	FRotator	BoneRotation;
	BYTE		BoneSpace;

	// Constructor.
	FBoneRotationControl(INT InBoneIndex, FName InBoneName, FRotator InBoneRotation, BYTE InBoneSpace):
		BoneIndex(InBoneIndex),
		BoneName(InBoneName),
		BoneRotation(InBoneRotation),
		BoneSpace(InBoneSpace)
	{}
	FBoneRotationControl() {}
};
template <> struct TIsPODType<FBoneRotationControl> { enum { Value = true }; };

//
//	FBoneTranslationControl
//
struct FBoneTranslationControl
{
	INT			BoneIndex;
	FName		BoneName;
	FVector		BoneTranslation;

	FBoneTranslationControl(INT InBoneIndex,  FName InBoneName, const FVector& InBoneTranslation):
		BoneIndex(InBoneIndex),
		BoneName(InBoneName),
		BoneTranslation(InBoneTranslation)
	{}
	FBoneTranslationControl() {}
};
template <> struct TIsPODType<FBoneTranslationControl> { enum { Value = true }; };

/** Struct used to indicate one active morph target that should be applied to this SkeletalMesh when rendered. */
struct FActiveMorph
{
	/** The morph target that we want to apply. */
	class UMorphTarget*	Target;

	/** Strength of the morph target, between 0.0 and 1.0 */
	FLOAT				Weight;

	FActiveMorph(class UMorphTarget* InTarget, FLOAT InWeight):
		Target(InTarget),
		Weight(InWeight)
	{}
	FActiveMorph() {}

	UBOOL operator==(const FActiveMorph& Other) const
	{
		// if target is same, we consider same 
		// any equal operator to check if it's same, 
		// we just check if this is same morph target
		return Target == Other.Target;
	}
};
template <> struct TIsPODType<FActiveMorph> { enum { Value = true }; };


/** A pair of bone names */
struct FBonePair
{
	FName Bones[2];

	UBOOL operator==(const FBonePair& Other) const
	{
		return Bones[0] == Other.Bones[0] && Bones[1] == Other.Bones[1];
	}

	UBOOL IsMatch(const FBonePair& Other) const
	{
		return	(Bones[0] == Other.Bones[0] || Bones[1] == Other.Bones[0]) &&
				(Bones[0] == Other.Bones[1] || Bones[1] == Other.Bones[1]);
	}
};

/** 
* A pair of bone indices
*/
struct FBoneIndexPair
{
	INT BoneIdx[2];

	UBOOL operator==(const FBoneIndexPair& Src) const
	{
		return (BoneIdx[0] == Src.BoneIdx[0]) && (BoneIdx[1] == Src.BoneIdx[1]);
	}

	friend FORCEINLINE DWORD GetTypeHash( const FBoneIndexPair& BonePair )
	{
		return appMemCrc(&BonePair, sizeof(FBoneIndexPair));
	}

	/**
	* Serialize to Archive
	*/
	friend FArchive& operator<<( FArchive& Ar, FBoneIndexPair& W )
	{
		return Ar << W.BoneIdx[0] << W.BoneIdx[1];
	}
};

enum ERootMotionMode
{
	RMM_Translate	= 0,
	RMM_Velocity	= 1,
	RMM_Ignore		= 2,
	RMM_Accel		= 3,
	RMM_Relative	= 4,
	RMM_MAX			= 5,
};

enum ERootMotionRotationMode
{
	RMRM_Ignore			= 0,
	RMRM_RotateActor	= 1,
	RMRM_MAX			= 2,
};

enum EFaceFXBlendMode
{
	FXBM_Overwrite	= 0,
	FXBM_Additive	= 1,
	FXBM_MAX		= 2,
};

enum EFaceFXRegOp
{
	FXRO_Add      = 0,	   
	FXRO_Multiply = 1, 
	FXRO_Replace  = 2,  
};

/** The valid BoneVisibilityStates values; A bone is only visible if it is *exactly* 1 */
enum EBoneVisibilityStatus
{
	BVS_HiddenByParent		= 0,	// Bone is hidden because it's parent is hidden
	BVS_Visible				= 1,	// Bone is visible
	BVS_ExplicitlyHidden	= 2,	// Bone is hidden directly
};

/** PhysicsBody options when bone is hiddne */
enum EPhysBodyOp
{
	PBO_None	= 0, // don't do anything
	PBO_Term	= 1, // terminate - if you terminate, you won't be able to re-init when unhidden
	PBO_Disable	= 2, // disable collision - it will enable collision when unhidden
};

enum EAnimRotationOnly
{
	/** Use settings defined in each AnimSet (default) */
	EARO_AnimSet = 0,
	/** Force AnimRotationOnly enabled on all AnimSets, but for this SkeletalMesh only */
	EARO_ForceEnabled = 1,
	/** Force AnimRotationOnly disabled on all AnimSets, but for this SkeletalMesh only */
	EARO_ForceDisabled = 2,
	EARO_MAX = 3,
};

/** Usage cases for toggling vertex weights */
enum EInstanceWeightUsage
{
	/** Weights are swapped for a subset of vertices. Requires a unique weights vertex buffer per skel component instance. */
	IWU_PartialSwap = 0,
	/** Weights are swapped for ALL vertices.  Shares a weights vertex buffer for all skel component instances. */
	IWU_FullSwap,
	IWU_Max
};

/** Which set of indices to select for TRISORT_CustomLeftRight sections. */
enum ECustomSortAlternateIndexMode
{
	CSAIM_Auto = 0,
	CSAIM_Left = 1,
	CSAIM_Right = 2,
};

/** 
 *  Enum to define how to scale max distance
 *  @see SetApexClothingMaxDistanceScale
 */
enum EMaxDistanceScaleMode
{
	MDSM_Multiply   =0,
	MDSM_Substract  =1,
	MDSM_MAX		=2,
};


/** LOD specific setup for the skeletal mesh component */
struct FSkelMeshComponentLODInfo
{
	/** Material corresponds to section. To show/hide each section, use this **/
	TArrayNoInit<UBOOL> HiddenMaterials;
	/** If TRUE, update the instanced vertex influences for this mesh during the next update */
	BITFIELD bNeedsInstanceWeightUpdate:1;
	/** If TRUE, always use instanced vertex influences for this mesh */
	BITFIELD bAlwaysUseInstanceWeights:1;
	/** Align the following byte */
	SCRIPT_ALIGN;
	/** Whether the instance weights are used for a partial/full swap */
	BYTE InstanceWeightUsage;
	/** Current index into the skeletal mesh VertexInfluences for the current LOD */
	INT InstanceWeightIdx;

	FSkelMeshComponentLODInfo()	 :
		  bNeedsInstanceWeightUpdate(FALSE)
		, bAlwaysUseInstanceWeights(FALSE)
		, InstanceWeightUsage(IWU_PartialSwap)
		, InstanceWeightIdx(INDEX_NONE)
	{ appMemzero(&HiddenMaterials, sizeof(TArray<UBOOL>));}
};

//
//	USkeletalMeshComponent
//
class USkeletalMesh;
class USkeletalMeshComponent : public UMeshComponent
{
	DECLARE_CLASS_NOEXPORT(USkeletalMeshComponent,UMeshComponent,0,Engine)

	USkeletalMesh*						SkeletalMesh;

	/** If this component is attached to another SkeletalMeshComponent, this is the one it's attached to. */
	USkeletalMeshComponent*				AttachedToSkelComponent;

	class UAnimTree*					AnimTreeTemplate;
	class UAnimNode*					Animations;

	/** Array of all AnimNodes in entire tree, in the order they should be ticked - that is, all parents appear before a child. */
	TArray<UAnimNode*>					AnimTickArray;
	/** Special Array of nodes that should always be ticked, even when not relevant. */
	TArray<UAnimNode*>					AnimAlwaysTickArray;
	/** Anim nodes relevancy status. Matching AnimTickArray size and indices. */
	TArray<INT>							AnimTickRelevancyArray;
	/** Anim nodes weights. Matching AnimTickArray size and indices. */
	TArray<FLOAT>						AnimTickWeightsArray;
	/** Linear Array for ticking SkelControls faster */
	TArray<class USkelControlBase*>		SkelControlTickArray;
	class UPhysicsAsset*				PhysicsAsset;
	class UPhysicsAssetInstance*		PhysicsAssetInstance;
	/*** Defines the FIApexClothing interface.  Must exactly match the layout of the corresponding .UC file; so if APEX is unavailable it declares a void pointer. */
	FIApexClothing*						ApexClothing;

	FLOAT								PhysicsWeight;

	/** Used to scale speed of all animations on this skeletal mesh. */
	FLOAT								GlobalAnimRateScale;

	/**
	 * Allows adjusting the desired streaming distance of streaming textures that uses UV 0.
	 * 1.0 is the default, whereas a higher value makes the textures stream in sooner from far away.
	 * A lower value (0.0-1.0) makes the textures stream in later (you have to be closer).
	 */
	FLOAT								StreamingDistanceMultiplier;

	class FSkeletalMeshObject*			MeshObject;
	FColor								WireframeColor;


	// State of bone bases - set up in local mesh space.
	TArray <FBoneAtom>				SpaceBases; 
	/** Temporary array of local-space (ie relative to parent bone) rotation/translation for each bone. */
	TArray <FBoneAtom>					LocalAtoms;
	TArray <FBoneAtom>					CachedLocalAtoms;
	TArray <FBoneAtom>				CachedSpaceBases;
	INT									LowUpdateFrameRate;

	/** Temporary array of bone indices required this frame. Filled in by UpdateSkelPose. */
	TArray<BYTE>						RequiredBones;
	/** Required Bones array for 3 pass skeleton composing */
	TArray<BYTE>						ComposeOrderedRequiredBones;
	USkeletalMeshComponent*				ParentAnimComponent;
	TArrayNoInit<INT>					ParentBoneMap;


	// Array of AnimSets used to find sequences by name.
	TArrayNoInit<class UAnimSet*>			AnimSets;

	/**
	 *  Temporary array of AnimSets that are used as a backup target when the engine needs to temporarily modify the
	 *	actor's animation set list. (e.g. Matinee playback)
	 */
	TArrayNoInit<class UAnimSet*> TemporarySavedAnimSets;


	/** 
	 *	Array of MorphTargetSets that will be looked in to find a particular MorphTarget, specified by name.
	 *	It is searched in the same way as the AnimSets array above.
	 */
	TArrayNoInit<class UMorphTargetSet*>	MorphSets;

	/** Array indicating all active MorphTargets. This array is updated inside UpdateSkelPose based on the AnimTree's st of MorphNodes. */
	TArrayNoInit<FActiveMorph>				ActiveMorphs;

	/** Array indicating all active MorphTargets. This array is updated inside UpdateSkelPose based on the AnimTree's st of MorphNodes. */
	TArrayNoInit<FActiveMorph>				ActiveCurveMorphs;
	/** TMap of MorphTarget Name to MorphTarget **/
	TMap<FName, UMorphTarget*>				MorphTargetIndexMap;

	// Attachments.
	TArrayNoInit<FAttachment>				Attachments;

	/** 
	 *	Array of indices into the Animations->SkelControls array. The size of this array must be equal to the number
	 *	of bones in the skeleton. After the transform for bone index 'i' is calculated, if SkelControlIndex(i) is not 255,
	 *	it will be looked up in Animations->SkelControls and executed.
	 */
	TArrayNoInit<BYTE>						SkelControlIndex;

	/** As SkelControlIndex, but only for controllers flagged with bPostPhysicsController. */
	TArrayNoInit<BYTE>						PostPhysSkelControlIndex;

	// Editor/debugging rendering mode flags.

	/** Force drawing of a specific lodmodel -1 if > 0. */
	INT									ForcedLodModel;
	/** 
	 * This is the min LOD that this component will use.  (e.g. if set to 2 then only 2+ LOD Models will be used.) This is useful to set on
	 * meshes which are known to be a certain distance away and still want to have better LODs when zoomed in on them.
	 **/
	INT									MinLodModel;
	/** 
	 *	Best LOD that was 'predicted' by UpdateSkelPose. 
	 *	This is what bones were updated based on, so we do not allow rendering at a better LOD than this. 
	 */
	INT									PredictedLODLevel;

	/** LOD level from previous frame, so we can detect changes in LOD to recalc required bones. */
	INT									OldPredictedLODLevel;

	/** If MaxDistanceFactor goes below this value (and it is non 0), start playing animations at a lower frame rate */
	FLOAT								AnimationLODDistanceFactor;

	/** Rate of update for skeletal meshes that are below AnimationLODDistanceFactor. For example if set to 3, animations will be updated once every three frames. */
	INT									AnimationLODFrameRate;

	/**	High (best) DistanceFactor that was desired for rendering this SkeletalMesh last frame. Represents how big this mesh was in screen space   */
	FLOAT								MaxDistanceFactor;

#if WITH_EDITORONLY_DATA
	/** Index of the chunk to preview... If set to -1, all chunks will be rendered */
	INT									ChunkIndexPreview;
	/** Index of the section to preview... If set to -1, all section will be rendered */
	INT									SectionIndexPreview;
#endif

	/** Forces the mesh to draw in wireframe mode. */
	UBOOL								bForceWireframe;

	/** If true, force the mesh into the reference pose - is an optimisation. */
	UBOOL								bForceRefpose;

	/** If bForceRefPose was set last tick. */
	UBOOL								bOldForceRefPose;

	/** Skip UpdateSkelPose. */
	UBOOL								bNoSkeletonUpdate;

	/** Draw the skeleton hierarchy for this skel mesh. */
	UBOOL								bDisplayBones;

	/** Bool that enables debug drawing of the skeleton before it is passed to the physics. Useful for debugging animation-driven physics. */
	UBOOL								bShowPrePhysBones;

	/** Don't bother rendering the skin. */
	UBOOL								bHideSkin;

	/** Forces ignoring of the mesh's offset. */
	UBOOL								bForceRawOffset;

	/** Ignore and bone rotation controllers */
	UBOOL								bIgnoreControllers;


	/** 
	 *	Set the LocalToWorld of this component to be the same as the ParentAnimComponent (if there is one). 
	 *	Note that this results in using RotOrigin/Origin from the ParentAnimComponent SkeletalMesh as well, as that is included in the LocalToWorld.
	 */
	UBOOL								bTransformFromAnimParent;

	/** Used to avoid ticking nodes in the tree multiple times. Node will only be ticked if TickTag != NodeTickTag. */
	INT									TickTag;
	/** Used to trigger DeferredInitAnim call on relevant nodes */
	INT									InitTag;
	/** 
	 *	Used to avoid duplicating work when calling GetBoneAtom. 
	 *	If this is equal to a nodes NodeCachedAtomsTag, cache is up-to-date and can be used. 
	 */
	INT									CachedAtomsTag;

	/** 
	 *	If true, create single rigid body physics for this component (like a static mesh) using root bone of PhysicsAsset. 
	 */
	UBOOL								bUseSingleBodyPhysics;

	/** If false, indicates that on the next call to UpdateSkelPose the RequiredBones array should be recalculated. */
	UBOOL								bRequiredBonesUpToDate;

	/** 
	 *	If non-zero, skeletal mesh component will not update kinematic bones and bone springs when distance factor is greater than this (or has not been rendered for a while).
	 *	This also turns off BlockRigidBody, so you do not get collisions with 'left behind' ragdoll setups.
	 */
	FLOAT								MinDistFactorForKinematicUpdate;

	/** Used to keep track of how many frames physics has been asleep for (when using PHYS_RigidBody). */
	INT									FramesPhysicsAsleep;

	/** <2 means no skip, 2 means every other frame, 3 means 1 out of three frames, etc  */
	INT									SkipRateForTickAnimNodesAndGetBoneAtoms;

	/** If TRUE, we will not tick the anim nodes */
	BITFIELD bSkipTickAnimNodes:1;

	/** If TRUE, we will not call GetBonesAtoms, and instead use cached data */
	BITFIELD bSkipGetBoneAtoms:1;

	/** If TRUE, then bSkipGetBoneAtoms is also true; we will interpolate cached data */
	BITFIELD bInterpolateBoneAtoms:1;

	/** If TRUE, there is at least one body in the current PhysicsAsset with a valid bone in the current SkeletalMesh */
	BITFIELD bHasValidBodies:1;

	/** When true, if owned by a PHYS_RigidBody Actor, skip all update (bones and bounds) when physics are asleep. */
	BITFIELD bSkipAllUpdateWhenPhysicsAsleep:1;

	/** When true, skip using the physics asset and always use the fixed bounds defined in the SkeletalMesh. */
	BITFIELD bComponentUseFixedSkelBounds:1;

	/** 
	 * When true, we will just using the bounds from our ParentAnimComponent.  This is useful for when we have a Mesh Parented
	 * to the main SkelMesh (e.g. outline mesh or a full body overdraw effect that is toggled) that is always going to be the same
	 * bounds as parent.  We want to do no calculations in that case.
	 */
	BITFIELD bUseBoundsFromParentAnimComponent:1;

	/** When true, if owned by a PHYS_RigidBody Actor, skip all update (bones and bounds) when physics are asleep. */
	BITFIELD bConsiderAllBodiesForBounds:1;

	/** if true, update skeleton/attachments even when our Owner has not been rendered recently */
	BITFIELD bUpdateSkelWhenNotRendered:1;

	/** If true, do not apply any SkelControls when owner has not been rendered recently. */
	BITFIELD bIgnoreControllersWhenNotRendered:1;

	/** If true, tick anim nodes even when our Owner has not been rendered recently  */
	BITFIELD bTickAnimNodesWhenNotRendered:1;

	/** If this is true, we are not updating kinematic bones and motors based on animation beacause the skeletal mesh is too far from any viewer. */
	BITFIELD bNotUpdatingKinematicDueToDistance:1;

	/** force root motion to be discarded, no matter what the AnimNodeSequence(s) are set to do */
	BITFIELD bForceDiscardRootMotion:1;
	/** Call RootMotionProcessed notification on Owner */
	BITFIELD bNotifyRootMotionProcessed:1;

	/** 
	 * if TRUE, notify owning actor of root motion mode changes.
	 * This calls the Actor.RootMotionModeChanged() event.
	 * This is useful for synchronizing movements. 
	 * For intance, when using RMM_Translate, and the event is called, we know that root motion will kick in on next frame.
	 * It is possible to kill in-game physics, and then use root motion seemlessly.
	 */
	BITFIELD bRootMotionModeChangeNotify:1;
	
	/**
	 * if TRUE, the event RootMotionExtracted() will be called on this owning actor,
	 * after root motion has been extracted, and before it's been used.
	 * This notification can be used to alter extracted root motion before it is forwarded to physics.
	 */
	BITFIELD bRootMotionExtractedNotify:1;
	/** Flag set when processing root motion. */
	BITFIELD bProcessingRootMotion:1;

	/** If true, FaceFX will not automatically create material instances. */
	BITFIELD bDisableFaceFXMaterialInstanceCreation:1;

	/** If true, disable FaceFX entirely for this component */
	BITFIELD bDisableFaceFX:1;

	/** If true, AnimTree has been initialised. */
	BITFIELD bAnimTreeInitialised:1;

	/** If TRUE, UpdateTransform will always result in a call to MeshObject->Update. */
	BITFIELD bForceMeshObjectUpdate:1;

	/** 
	 *	Indicates whether this SkeletalMeshComponent should have a physics engine representation of its state. 
	 *	@see SetHasPhysicsAssetInstance
	 */
	BITFIELD bHasPhysicsAssetInstance:1;

	/** If we are running physics, should we update bFixed bones based on the animation bone positions. */
	BITFIELD bUpdateKinematicBonesFromAnimation:1;

	/** 
	 *	If we should pass joint position to joints each frame, so that they can be used by motorised joints to drive the
	 *	ragdoll based on the animation.
	 */
	BITFIELD bUpdateJointsFromAnimation:1;

	/** Indicates whether this SkeletalMeshComponent is currently considered 'fixed' (ie kinematic) */
	BITFIELD bSkelCompFixed:1;

	/** Used for consistancy checking. Indicates that the results of physics have been blended into SpaceBases this frame. */
	BITFIELD bHasHadPhysicsBlendedIn:1;

	/** 
	 *	If true, attachments will be updated twice a frame - once in Tick and again when UpdateTransform is called. 
	 *	This can resolve some 'frame behind' issues if an attachment need to be in the correct location for it's Tick, but at a cost.
	 */
	BITFIELD bForceUpdateAttachmentsInTick:1;

	/** Enables blending in of physics bodies with the bAlwaysFullAnimWeight flag set. */
	BITFIELD bEnableFullAnimWeightBodies:1;

	/** 
	 *	If true, when this skeletal mesh overlaps a physics volume, each body of it will be tested against the volume, so only limbs 
	 *	actually in the volume will be affected. Useful when gibbing bodies.
	 */
	BITFIELD bPerBoneVolumeEffects:1;

	/** 
	 *	If true, use per-bone motion blur on this skeletal mesh.
	 */
	BITFIELD bPerBoneMotionBlur:1;

	/** If true, will move the Actors Location to match the root rigid body location when in PHYS_RigidBody. */
	BITFIELD bSyncActorLocationToRootRigidBody:1;

	/** If TRUE, force usage of raw animation data when animating this skeltal mesh; if FALSE, use compressed data. */
	BITFIELD bUseRawData:1;

	/** Disable warning when an AnimSequence is not found. FALSE by default. */
	BITFIELD bDisableWarningWhenAnimNotFound:1;

	/** if set, components that are attached to us have their bOwnerNoSee and bOnlyOwnerSee properties overridden by ours */
	BITFIELD bOverrideAttachmentOwnerVisibility:1;

	/** if TRUE, when detach, send message to renderthread to delete this component from hit mask list **/
	BITFIELD bNeedsToDeleteHitMask:1;

	/** pauses animations (doesn't tick them) */
	BITFIELD bPauseAnims:1;

	/** If true, DistanceFactor for this SkeletalMeshComponent will be added to global chart. */
	BITFIELD bChartDistanceFactor:1;

	/** If TRUE, line checks will test against the bounding box of this skeletal mesh component and return a hit if there is a collision. */
	BITFIELD bEnableLineCheckWithBounds:1;

	/** Whether or not we can highlight selected sections - this should really only be done in the editor */
	BITFIELD bCanHighlightSelectedSections:1;

	/** Whether or not we can highlight selected sections - this should really only be done in the editor */
	BITFIELD bUpdateMorphWhenParentAnimComponentExists:1;

	/** If bEnableLineCheckWithBounds is TRUE, scale the bounds by this value before doing line check. */
	FVector LineCheckBoundsScale;

	// CLOTH

	/** 
	 *	Whether cloth simulation should currently be used on this SkeletalMeshComponent.
	 *	@see SetEnableClothSimulation
	 */
	BITFIELD bEnableClothSimulation:1;

	/** Turns off all cloth collision so not checks are done (improves performance). */
	BITFIELD bDisableClothCollision:1;

	/** If true, cloth is 'frozen' and no simulation is taking place for it, though it will keep its shape. */
	BITFIELD bClothFrozen:1;

	/** If true, cloth will automatically have bClothFrozen set when it is not rendered, and have it turned off when it is seen. */
	BITFIELD bAutoFreezeClothWhenNotRendered:1;

	/** If true, cloth will be awake when a level is started, otherwise it will be instantly put to sleep. */
	BITFIELD bClothAwakeOnStartup:1;

	/** It true, clamp velocity of cloth particles to be within ClothOwnerVelClampRange of Base velocity. */
	BITFIELD bClothBaseVelClamp:1;

	/** It true, interp velocity of cloth particles towards Base velocity, using ClothBaseVelClampRange as the interp rate (0..1). */
	BITFIELD bClothBaseVelInterp:1;

	/** If true, fixed verts of the cloth are attached in the physics to the physics body that this components actor is attached to. */
	BITFIELD bAttachClothVertsToBaseBody:1;

	/** Whether this cloth is on a non-animating static object. */
	BITFIELD bIsClothOnStaticObject:1;
	/** Whether we've updated fixed cloth verts since last attachment. */
	BITFIELD bUpdatedFixedClothVerts:1;

	/** Whether should do positional box dampening */
	BITFIELD bClothPositionalDampening:1;
	/** Whether wind direction is relative to owner rotation or not */
	BITFIELD bClothWindRelativeToOwner:1;

	/** TRUE if mesh has been recently rendered, FALSE otherwise */
	BITFIELD bRecentlyRendered:1;

	BITFIELD bCacheAnimSequenceNodes:1;

	/** TRUE if it needs to rebuild the required bones array for multi pass compose */
	BITFIELD bUpdateComposeSkeletonPasses:1;
	/** Flag to remember if cache saved is valid or not to make sure Save/Restore always happens with a pair **/
	BITFIELD bValidTemporarySavedAnimSets:1;

	/** 
	 * Set of bones which will be used to find vertices to switch to using instanced influence weights
	 * instead of the default skeletal mesh weighting.
	 */
	TArrayNoInit<FBonePair> InstanceVertexWeightBones;	

	/** LOD specific setup for the skeletal mesh component */
	TArrayNoInit<FSkelMeshComponentLODInfo> LODInfo;
	
	/** The state of the LocalToWorld pos at the point the cloth was frozen. */
	FVector FrozenLocalToWorldPos;

	/** The state of the LocalToWorld rotation at the point the cloth was frozen. */
	FRotator FrozenLocalToWorldRot;

	/** Constant force applied to all vertices in the cloth. */
	FVector ClothExternalForce;

	/** 'Wind' force applied to cloth. Force on each vertex is based on the dot product between the wind vector and the surface normal. */
	FVector	ClothWind;

	/** Amount of variance from base's velocity the cloth is allowed. */
	FVector	ClothBaseVelClampRange;

	/** How much to blend in results from cloth simulation with results from regular skinning. */
	FLOAT	ClothBlendWeight;

	/** Cloth blend weight, controlled by distance from camera. */
	FLOAT	ClothDynamicBlendWeight;

	/** Distance factor below which cloth should be fully animated. -1.0 indicates always physics. */
	FLOAT	ClothBlendMinDistanceFactor;

	/** Distance factor above which cloth should be fully simulated. */
	FLOAT	ClothBlendMaxDistanceFactor;

	/** Distance from the owner in relative frame (max == pos XYZ, min == neg XYZ) */
	FVector	MinPosDampRange;
	FVector	MaxPosDampRange;
	/** Dampening scale applied to cloth particle velocity when approaching boundaries of *PosDampRange */
	FVector MinPosDampScale;
	FVector	MaxPosDampScale;

	/** Pointer to internal simulation object for cloth on this skeletal mesh. */
	FPointer ClothSim;

	/** Index of physics scene that this components cloth simulation is taking place in. */
	INT SceneIndex;

	/** Output vertex position data. Filled in by simulation engine when fetching results */
	TArray<FVector> ClothMeshPosData;

	/** Output vertex normal data. Filled in by simulation engine when fetching results */
	TArray<FVector> ClothMeshNormalData;

	/** Output index buffer. Filled in by simulation engine when fetching results */
	TArray<INT> 	ClothMeshIndexData;

	/** Output number of verts in cloth mesh. Filled in by simulation engine when fetching results */
	INT	NumClothMeshVerts;

	/** Output number of indices in buffer. Filled in by simulation engine when fetching results */
	INT	NumClothMeshIndices;


	/** Cloth parent indices contain the index of the original vertex when a vertex is created during tearing.
	If it is an original vertex then the parent index is the same as the vertex index. 
	*/
	TArray<INT>		ClothMeshParentData;

	/** Number of cloth parent indices provided by the physics SDK */
	INT				NumClothMeshParentIndices;


	/** Replacement Output vertex position data if welding needs to be used. Data is filled into ClothMeshPosData during rendering */
	TArray<FVector> ClothMeshWeldedPosData;

	/** Replacement Output vertex normal data if welding needs to be used. Data is filled into ClothMeshPosData during rendering */
	TArray<FVector> ClothMeshWeldedNormalData;

	/** Replacement  Output index buffer. Since tearing is not supported these do not change anyways*/
	TArray<INT> 	ClothMeshWeldedIndexData;

	INT ClothDirtyBufferFlag;

	/** Enum indicating what type of object this cloth should be considered for rigid body collision. */
	BYTE ClothRBChannel;

	/** Types of objects that this cloth will collide with. */
	FRBCollisionChannelContainer ClothRBCollideWithChannels;

	/** How much force to apply to cloth, in relation to the force applied to rigid bodies(zero applies no force to cloth, 1 applies the same) */
	FLOAT			ClothForceScale;

	/** Amount to scale impulses applied to cloth simulation. */ 
	FLOAT			ClothImpulseScale;

	/** 
     * The cloth tear factor for this SkeletalMeshComponent, negative values take the tear factor from the SkeletalMesh.
     * Note: UpdateClothParams() should be called after modification so that the changes are reflected in the simulation.
	 */
	FLOAT			ClothAttachmentTearFactor;

	/** If TRUE, cloth uses compartment in physics scene (usually with fixed timstep for better behaviour) */
	BITFIELD		bClothUseCompartment:1;

	/** If the distance traveled between frames exceeds this value the vertices will be reset to avoid stretching. */
	FLOAT MinDistanceForClothReset;
	FVector LastClothLocation;

	/** Enum indicating what type of object this apex clothing should be considered for rigid body collision. */
	BYTE		ApexClothingRBChannel;

	/** Types of objects that this clothing will collide with. */
	FRBCollisionChannelContainer	ApexClothingRBCollideWithChannels;

	/** Align the following byte */
	SCRIPT_ALIGN;

	/** Enum indicating what channel the apex clothing collision shapes should be placed in */
	BYTE		ApexClothingCollisionRBChannel;

	/** Align the following bitfields */
	SCRIPT_ALIGN;

	/** If true, the clothing actor will stop simulating when it is not rendered */
	BITFIELD						bAutoFreezeApexClothingWhenNotRendered:1;

	/** If TRUE, WindVelocity is applied in the local space of the component, rather than world space. */
	BITFIELD						bLocalSpaceWind:1;

	/** The Wind Velocity applied to Apex Clothing */
	FVector							WindVelocity;

	/** Time taken for ApexClothing to reach WindVelocity */
	FLOAT							WindVelocityBlendTime;

    /** Don't attempt to initialize clothing when component is attached */
	BITFIELD						bSkipInitClothing:1;

	/** Pointer to the simulated NxSoftBody object. */
	FPointer						SoftBodySim;

    /** Index of the Novodex scene the soft-body resides in. */
	INT								SoftBodySceneIndex;

    /** Whether soft-body simulation should currently be used on this SkeletalMeshComponent. */
	BITFIELD						bEnableSoftBodySimulation:1;

    /** Buffer of the updated tetrahedron-vertex positions. */
	TArray<FVector>					SoftBodyTetraPosData;

    /** Buffer of the updated tetrahedron-indices. */
	TArray<INT>						SoftBodyTetraIndexData;

    /** Number of tetrahedron vertices of the soft-body mesh. */
	INT								NumSoftBodyTetraVerts;

    /** Number of tetrahedron indices of the soft-body mesh (equal to four times the number of tetrahedra). */
	INT								NumSoftBodyTetraIndices;

	/** Number of tetrahedron indices of the soft-body mesh (equal to four times the number of tetrahedra). */
	FLOAT							SoftBodyImpulseScale;

	/** If true, the soft-body is 'frozen' and no simulation is taking place for it, though it will keep its shape. */
	BITFIELD						bSoftBodyFrozen:1;

	/** If true, the soft-body will automatically have bSoftBodyFrozen set when it is not rendered, and have it turned off when it is seen. */
	BITFIELD						bAutoFreezeSoftBodyWhenNotRendered:1;

	/** If true, the soft-body will be awake when a level is started, otherwise it will be instantly put to sleep. */
	BITFIELD						bSoftBodyAwakeOnStartup:1;

	/** If TRUE, soft body uses compartment in physics scene (usually with fixed timstep for better behaviour) */
	BITFIELD						bSoftBodyUseCompartment:1;

	/** Align the following byte */
	SCRIPT_ALIGN;

    /** Enum indicating what type of object this soft-body should be considered for rigid body collision. */
	ERBCollisionChannel				SoftBodyRBChannel;

    /** Types of objects that this soft-body will collide with. */
	FRBCollisionChannelContainer	SoftBodyRBCollideWithChannels;

    /** Pointer to the Novodex plane-actor used when previewing the soft-body in the AnimSet Editor. */
	FPointer						SoftBodyASVPlane;


	/** For rendering physics limits. TODO remove! */
	UMaterialInterface*					LimitMaterial;
		
	/** Root Motion extracted from animation. */
	FBoneAtom	RootMotionDelta;
	/** Root Motion velocity */
	FVector		RootMotionVelocity;

	/** Root Bone offset */
	FVector		RootBoneTranslation;

	/** Scale applied in physics when RootMotionMode == RMM_Accel */
	FVector		RootMotionAccelScale;

	/** Determines whether motion should be applied immediately or... (uses ERootMotionMode) */
	BYTE	RootMotionMode;
	/** Previous Root Motion Mode, to catch changes */
	BYTE	PreviousRMM;
	BYTE	PendingRMM;
	BYTE	OldPendingRMM;
	INT		bRMMOneFrameDelay;
	/** Root Motion Rotation mode (uses ERootMotionRotationMode) */
	BYTE	RootMotionRotationMode;
	/** SkeletalMeshComponent settings for AnimRotationOnly (EAnimRotationOnly) */
	BYTE AnimRotationOnly;

	/** How should be blend FaceFX animations? */
	BYTE	FaceFXBlendMode;

#if WITH_FACEFX
	// The FaceFX actor instance associated with the skeletal mesh component.
	OC3Ent::Face::FxActorInstance* FaceFXActorInstance;
#else
	void* FaceFXActorInstance;
#endif

	/** 
	 *	The audio component that we are using to play audio for a facial animation. 
	 *	Assigned in PlayFaceFXAnim and cleared in StopFaceFXAnim.
	 */
	UAudioComponent* CachedFaceFXAudioComp;

	/** Array of bone visibilities (containing one of the values in EBoneVisibilityStatus for each bone).  A bone is only visible if it is *exactly* 1 (BVS_Visible) */
	TArrayNoInit <BYTE>	BoneVisibilityStates;

	/* To cache it rather than re-calculating all the time : have guard for stale data*/
	FBoneAtom LocalToWorldBoneAtom;

	/** Editor only. Used for visualizing drawing order in Animset Viewer. If < 1.0,
	* only the specified fraction of triangles will be rendered
	*/
	float ProgressiveDrawingFraction;

	/** Editor only. Used for manually selecting the alternate indices for
	  * TRISORT_CustomLeftRight sections.
	  */
	BYTE CustomSortAlternateIndexMode;

	/** Editor only. Used to keep track of the morph targets we've reported 
	  * the user as having bad LODs (to prevent LOD spam)
	  */
	TArrayNoInit <FName>	MorphTargetsQueried;

	/*-----------------------------------------------------------------------------
		Tick time optimization.
	  -----------------------------------------------------------------------------*/

	/** Whether to use tick optimization. */
	BITFIELD	bUseTickOptimization:1;

	/** How many times this component was ticked. */
	INT			TickCount;

	/** Last drop rate [0-2]. */
	INT			LastDropRate;

	/** Time when LastDropRate changes, used to avoid 'flickering' when drop rates changes very frequently. */
	FLOAT		LastDropRateChange;

	/** Accumulated delta time when frames were dropped. */
	FLOAT		AccumulatedDroppedDeltaTime;

	/** Dropped Delta Time when component skipped ticking for optimizations */
	FLOAT		ComponentDroppedDeltaTime;

	// USkeletalMeshComponent interface
	void DeleteAnimTree();

	// FaceFX functions.
	void	UpdateFaceFX( TArray<FBoneAtom>& LocalTransforms, UBOOL bTickFaceFX );
	UBOOL	PlayFaceFXAnim(UFaceFXAnimSet* FaceFXAnimSetRef, const FString& AnimName, const FString& GroupName,class USoundCue* SoundCueToPlay);
	void	StopFaceFXAnim( void );
	UBOOL	IsPlayingFaceFXAnim();
	void	DeclareFaceFXRegister( const FString& RegName );
	FLOAT	GetFaceFXRegister( const FString& RegName );
	void	SetFaceFXRegister( const FString& RegName, FLOAT RegVal, BYTE RegOp, FLOAT InterpDuration );
	void	SetFaceFXRegisterEx( const FString& RegName, BYTE RegOp, FLOAT FirstValue, FLOAT FirstInterpDuration, FLOAT NextValue, FLOAT NextInterpDuration );

	/** Update the PredictedLODLevel and MaxDistanceFactor in the component from its MeshObject. */
	UBOOL UpdateLODStatus();
	/** Initialize the LOD entries for the component */
	void InitLODInfos();

	void UpdateSkelPose( FLOAT DeltaTime = 0.f, UBOOL bTickFaceFX = TRUE );
	void UpdateMorph( FLOAT DeltaTime = 0.f, UBOOL bTickFaceFX = TRUE );
	void ProcessRootMotion( FLOAT DeltaTime, FBoneAtom& ExtractedRootMotionDelta, INT& bHasRootMotion );
	void ComposeSkeleton();
	void ApplyControllersForBoneIndex(INT BoneIndex, UBOOL bPrePhysControls, UBOOL bPostPhysControls, const UAnimTree* Tree, UBOOL bRenderedRecently, const BYTE* BoneProcessed);
	void UpdateActiveMorphs();
	/** Builds required bones for 3 pass Compose Skeleton. */
	void BuildComposeSkeletonPasses();
	void BlendPhysicsBones( TArray<BYTE>& RequiredBones, FLOAT PhysicsWeight );
	void BlendInPhysics();
	UBOOL DoesBlendPhysics();

	/** Update bEnableFullAnimWeightBodies Flag - it does not turn off if already on **/
	void UpdateFullAnimWeightBodiesFlag();

	void RecalcRequiredBones(INT LODIndex);
	void RebuildVisibilityArray();

	void SetSkeletalMesh(USkeletalMesh* InSkelMesh, UBOOL bKeepSpaceBases = FALSE);
	void SetPhysicsAsset(UPhysicsAsset* InPhysicsAsset, UBOOL bForceReInit = FALSE);
	void SetForceRefPose(UBOOL bNewForceRefPose);
	virtual void SetMaterial(INT ElementIndex, UMaterialInterface* InMaterial);
	void SetParentAnimComponent(USkeletalMeshComponent* NewParentAnimComp);
	
	/**
	 *	Sets the value of the bForceWireframe flag and reattaches the component as necessary.
	 *
	 *	@param	InForceWireframe		New value of bForceWireframe.
	 */
	void SetForceWireframe(UBOOL InForceWireframe);

	/**
	 *	Set value of bHasPhysicsAssetInstance flag.
	 *	Will create/destroy PhysicsAssetInstance as desired.
	 *
	 *  @param bHasInstance - Sets value of flag
	 *  @param bUseCurrentPosition - If true, skip the skeletal update and use current positions
	 */
	void SetHasPhysicsAssetInstance(UBOOL bHasInstance, UBOOL bUseCurrentPosition = FALSE);

	/** Find a BodyInstance by BoneName */
	URB_BodyInstance* FindBodyInstanceNamed(FName BoneName);

	// Search through AnimSets to find an animation with the given name
	class UAnimSequence* FindAnimSequence(FName AnimSeqName) const;

	// Search through MorphSets to find a morph target with the given name
	class UMorphTarget* FindMorphTarget(FName MorphTargetName);
	class UMorphNodeBase* FindMorphNode(FName InNodeName);

	FMatrix	GetBoneMatrix(DWORD BoneIdx) const;

	/**
	 * returns the bone atom for the bone at the specified index
	 * @param BoneIdx - index of the bone you want an atom for
	 */
	FBoneAtom GetBoneAtom(DWORD BoneIdx) const;

	// Controller Interface
	INT	MatchRefBone(FName StartBoneName) const;
	FName GetParentBone( FName BoneName ) const;

	/** 
	 * Returns bone name linked to a given named socket on the skeletal mesh component.
	 * If you're unsure to deal with sockets or bones names, you can use this function to filter through, and always return the bone name.
	 * @input	bone name or socket name
	 * @output	bone name
	 */
	FName GetSocketBoneName(FName InSocketName);

	FQuat	GetBoneQuaternion(FName BoneName, INT Space=0) const;
	FVector GetBoneLocation(FName BoneName, INT Space=0) const;
	FVector GetBoneAxis( FName BoneName, BYTE Axis ) const;

	void TransformToBoneSpace(FName BoneName, const FVector & InPosition, const FRotator & InRotation, FVector & OutPosition, FRotator & OutRotation);
	void TransformFromBoneSpace(FName BoneName, const FVector & InPosition, const FRotator & InRotation, FVector & OutPosition, FRotator & OutRotation);

	FMatrix GetAttachmentLocalToWorld(const FAttachment& Attachment);

	FMatrix GetTransformMatrix();
	
	virtual void InitArticulated(UBOOL bFixed);
	virtual void TermArticulated(FRBPhysScene* Scene);


	void SetAnimTreeTemplate(UAnimTree* NewTemplate);

	void UpdateParentBoneMap();

	/** Update bHasValidBodies flag */
	void UpdateHasValidBodies();

	/** forces an update to the mesh's skeleton/attachments, even if bUpdateSkelWhenNotRendered is false and it has not been recently rendered
	* @note if bUpdateSkelWhenNotRendered is true, there is no reason to call this function (but doing so anyway will have no effect)
	*/
	void ForceSkelUpdate();

	/** 
	 * Force AnimTree to recache all animations.
	 * Call this when the AnimSets array has been changed.
	 */
	void UpdateAnimations();
	UBOOL GetBonesWithinRadius( const FVector& Origin, FLOAT Radius, DWORD TraceFlags, TArray<FName>& out_Bones );

	// SkelControls.

	FBoneAtom CalcComponentToFrameMatrix( INT BoneIndex, BYTE Space, FName OtherBoneName );
	void CalcBothComponentFrameMatrix(const INT BoneIndex, const BYTE Space, const FName OtherBoneName, FBoneAtom &ComponentToFrame, FBoneAtom& FrameToComponent) const;

	void InitAnimTree(UBOOL bForceReInit=TRUE);
	void InitSkelControls();
	/**
	*	Initialize MorphSets look up table : MorphTargetIndexMap
	*/
	void InitMorphTargets();
	void UpdateMorphTargetMaterial(const UMorphTarget* MorphTarget, const FLOAT Weight);

	/** 
	* Add Curve Keys to ActiveMorph Sets 
	*/
	void ApplyCurveKeys(FCurveKeyArray& CurveKeys);

	class UAnimNode*		FindAnimNode(FName InNodeName);
	class USkelControlBase* FindSkelControl(FName InControlName);
	void TickSkelControls(FLOAT DeltaSeconds);
	virtual UBOOL LegLineCheck(const FVector& Start, const FVector& End, FVector& HitLocation, FVector& HitNormal, const FVector& Extent = FVector(0.f));

	// Attachment interface.
	void AttachComponent(UActorComponent* Component,FName BoneName,FVector RelativeLocation = FVector(0,0,0),FRotator RelativeRotation = FRotator(0,0,0),FVector RelativeScale = FVector(1,1,1));
	void DetachComponent(UActorComponent* Component);

	UBOOL GetSocketWorldLocationAndRotation(FName InSocketName, FVector& OutLocation, FRotator* OutRotation, INT Space=0);
	void AttachComponentToSocket(UActorComponent* Component,FName SocketName);

	/**
	 * Detach any component that's attached if class of the component == ClassOfComponentToDetach or child
	 */
	void DetachAnyOf(UClass * ClassOfComponentToDetach);

    /**
     * Function returns whether or not CPU skinning should be applied
     * Allows the editor to override the skinning state for editor tools
     */
	virtual UBOOL ShouldCPUSkin();

    /** 
     * Function to operate on mesh object after its created, 
     * but before it's attached.
     * @param MeshObject - Mesh Object owned by this component
	 */
	virtual void PostInitMeshObject(class FSkeletalMeshObject* MeshObject) {}

#if USE_GAMEPLAY_PROFILER
    /** 
     * This function actually does the work for the GetProfilerAssetObject and is virtual.  
     * It should only be called from GetProfilerAssetObject as GetProfilerAssetObject is safe to call on NULL object pointers
     **/
	virtual UObject* GetProfilerAssetObjectInternal() const;
#endif

	/**
	 * This will return detail info about this specific object. (e.g. AudioComponent will return the name of the cue,
	 * ParticleSystemComponent will return the name of the ParticleSystem)  The idea here is that in many places
	 * you have a component of interest but what you really want is some characteristic that you can use to track
	 * down where it came from.  
	 *
	 */
	virtual FString GetDetailedInfoInternal() const;

	/** if bOverrideAttachmentOwnerVisibility is true, overrides the owner visibility values in the specified attachment with our own
	 * @param Component the attached primitive whose settings to override
	 */
	void SetAttachmentOwnerVisibility(UActorComponent* Component);

	/** finds the closest bone to the given location
	 * @param TestLocation the location to test against
	 * @param BoneLocation (optional, out) if specified, set to the world space location of the bone that was found, or (0,0,0) if no bone was found
	 * @param IgnoreScale (optional) if specified, only bones with scaling larger than the specified factor are considered
	 * @return the name of the bone that was found, or 'None' if no bone was found
	 */
	FName FindClosestBone(FVector TestLocation, FVector* BoneLocation = NULL, FLOAT IgnoreScale = -1.0f);

	/** Calculate the up-to-date transform of the supplied SkeletalMeshComponent, which should be attached to this component. */
	FMatrix CalcAttachedSkelCompMatrix(const USkeletalMeshComponent* AttachedComp);

	/** 
	 * Update the instanced vertex influences (weights/bones) 
	 * Uses the cached list of bones to find vertices that need to use instanced influences
	 * instead of the defaults from the skeletal mesh 
     * @param LODIdx - The LOD to update the weights for
	 */
	void UpdateInstanceVertexWeights(INT LODIdx);

	/** 
	 * Add a new bone to the list of instance vertex weight bones
	 *
	 * @param BoneNames - set of bones (implicitly parented) to use for finding vertices
	 */
	void AddInstanceVertexWeightBoneParented(FName BoneName, UBOOL bPairWithParent = TRUE);

	/** 
	 * Remove a new bone to the list of instance vertex weight bones
	 *
	 * @param BoneNames - set of bones (implicitly parented) to use for finding vertices
	 */
	void RemoveInstanceVertexWeightBoneParented(FName BoneName);

	/** 
	 * Find an existing bone pair entry in the list of InstanceVertexWeightBones
	 *
	 * @param BonePair - pair of bones to search for
	 * @return index of entry found or -1 if not found
	 */
	INT FindInstanceVertexweightBonePair(const FBonePair& BonePair) const;
	
	/** 
	 * Update the bones that specify which vertices will use instanced influences
	 * This will also trigger an update of the vertex weights.
	 *
	 * @param BonePairs - set of bone pairs to use for finding vertices
	 */
	void UpdateInstanceVertexWeightBones( const TArray<FBonePair>& BonePairs );
	
	/**
	 * Enabled or disable the instanced vertex weights buffer for the skeletal mesh object
	 *
	 * @param bEnable - TRUE to enable, FALSE to disable
	 * @param LODIdx - LOD to enable
	 */
	void ToggleInstanceVertexWeights( UBOOL bEnabled, INT LODIdx);

	/**
	 * Verify FaceFX Bone indices match SkeletalMesh bone indices
	 */
	void DebugVerifyFaceFXBoneList();

	/**
	 * Debug function that traces FaceFX bone list
	 * to see if more than one mesh is referencing/relinking master bone list
	 */
	void TraceFaceFX(UBOOL bOutput = FALSE);

	/**
	 * Checks/updates material usage on proxy based on current morph target usage
	 */
	void UpdateMorphMaterialUsageOnProxy();

	// UObject interface - to count memory
	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	INT GetResourceSize();

	/**
	* Returns the FIApexClothing interface pointer.
	**/
	FIApexClothing * GetApexClothing(void) const { return ApexClothing; };
	/** 
	* Returns the number of clothing vertices
	**/
	INT GetApexClothingNumVertices(int LodIndex, int SectionIndex);
	/**
	*  Initialize Apex clothing, if this skeletal mesh has any materials marked as being used with clothing.
	*/
	void           InitApexClothing(FRBPhysScene* RBPhysScene);

	/**
	* Releases the Apex clothing interface if one was created.
	*/
	void           ReleaseApexClothing();

	/**
	* 'Ticks' the associated Apex clothing if there is one.  
	* Primarily detects if the underlying asset has changed and re-creates the clothing interface if needed.
    * Give up a time slice to synchronize apex clothing
	*/
	void           TickApexClothing(FLOAT DeltaTime);

	/**
	* Forward the MaxDistance Scale Notifications from Animations to Apex Clothing.
	*/
	void           SetApexClothingMaxDistanceScale(FLOAT StartScale, FLOAT EndScale, INT ScaleMode, FLOAT Duration);

	/**
	 *  Retrieve various actor metrics depending on the provided type.  All of
	 *  these will total the values for this component.
	 *
	 *  @param MetricsType The type of metric to calculate.
	 *
	 *  METRICS_VERTS    - Get the number of vertices.
	 *  METRICS_TRIS     - Get the number of triangles.
	 *  METRICS_SECTIONS - Get the number of sections.
	 *
	 *  @return INT The total of the given type for this component.
	 */
	virtual INT GetActorMetrics(EActorMetricsType MetricsType);

	// UActorComponent interface.
protected:
	virtual void SetParentToWorld(const FMatrix& ParentToWorld);
	virtual void Attach();
	virtual void UpdateTransform();
	virtual void UpdateChildComponents();
	virtual void Detach( UBOOL bWillReattach = FALSE );
	virtual void BeginPlay();
	virtual void Tick(FLOAT DeltaTime);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);	

	/**
	 * Called by the editor to query whether a property of this object is allowed to be modified.
	 * The property editor uses this to disable controls for properties that should not be changed.
	 * When overriding this function you should always call the parent implementation first.
	 *
	 * @param	InProperty	The property to query
	 *
	 * @return	TRUE if the property can be modified in the editor, otherwise FALSE
	 */
	virtual UBOOL CanEditChange( const UProperty* InProperty ) const;

	/**
	 * Called to finish destroying the object.  After UObject::FinishDestroy is called, the object's memory should no longer be accessed.
	 */
	virtual void FinishDestroy();

	/** Internal function that updates physics objects to match the RBChannel/RBCollidesWithChannel info. */
	virtual void UpdatePhysicsToRBChannels();
public:
	void TickAnimNodes(FLOAT DeltaTime);

	virtual void InitComponentRBPhys(UBOOL bFixed);
	virtual void SetComponentRBFixed(UBOOL bFixed);
	virtual void TermComponentRBPhys(FRBPhysScene* InScene);
	virtual class URB_BodySetup* GetRBBodySetup();

	/** 
	 * Retrieves the materials used in this component 
	 * 
	 * @param OutMaterials	The list of used materials.
	 */
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const;

	/** Initialize physx cloth support. */
	void InitClothSim(FRBPhysScene* Scene);

	/** Destructs and frees cloth support. */
	void TermClothSim(FRBPhysScene* Scene);

	/** Initialize breakable cloth vertices. */
	void InitClothBreakableAttachments();

	/** Initialize "metal" cloth support... stiff cloth that is associated with rigid-bodies to simulate denting. */
	void InitClothMetal();

	/** Initialize physx soft-body support. */
	void InitSoftBodySim(FRBPhysScene* Scene, UBOOL bRunsInAnimSetViewer = false);

	/** Destructs and frees soft-body support. */
	void TermSoftBodySim(FRBPhysScene* Scene);

	/** Updates soft-body parameters from the UActor to the simulation. */
	void UpdateSoftBodyParams();

	/** Initialize soft-body to rigid-body attach points. */
	void InitSoftBodyAttachments();

	/** Freeze or unfreeze soft-body simulation. */
	void SetSoftBodyFrozen(UBOOL bNewFrozen);

	/** Force awake any soft body simulation on this component */
	void WakeSoftBody();

	/** Inialize internal soft-body memory buffers. */
	void InitSoftBodySimBuffers();

	virtual UBOOL IsValidComponent() const;

	// UPrimitiveComponent interface.
	virtual void SetTransformedToWorld();

	virtual void GetStreamingTextureInfo(TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const;

	virtual UBOOL PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent,DWORD TraceFlags);
	virtual UBOOL LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags);

	virtual void UpdateBounds();
	void UpdateClothBounds();
	void UpdateApexClothingBounds();

	virtual void AddImpulse(FVector Impulse, FVector Position = FVector(0,0,0), FName BoneName = NAME_None, UBOOL bVelChange=false);
	virtual void AddRadialImpulse(const FVector& Origin, FLOAT Radius, FLOAT Strength, BYTE Falloff, UBOOL bVelChange=false);
	virtual void AddRadialForce(const FVector& Origin, FLOAT Radius, FLOAT Strength, BYTE Falloff);
	virtual void AddForceField(FForceApplicator* Applicator, const FBox& FieldBoundingBox, UBOOL bApplyToCloth, UBOOL bApplyToRigidBody);
	virtual void WakeRigidBody( FName BoneName = NAME_None );
	virtual void PutRigidBodyToSleep( FName BoneName = NAME_None );
	virtual UBOOL RigidBodyIsAwake( FName BoneName = NAME_None );
	virtual void SetRBLinearVelocity(const FVector& NewVel, UBOOL bAddToCurrent=false);
	virtual void SetRBAngularVelocity(const FVector& NewVel, UBOOL bAddToCurrent=false);
	virtual void RetardRBLinearVelocity(const FVector& RetardDir, FLOAT VelScale);
	virtual void SetRBPosition(const FVector& NewPos, FName BoneName = NAME_None);
	virtual void SetRBRotation(const FRotator& NewRot, FName BoneName = NAME_None);
	virtual void SetBlockRigidBody(UBOOL bNewBlockRigidBody);
	virtual void SetNotifyRigidBodyCollision(UBOOL bNewNotifyRigidBodyCollision);
	virtual void SetPhysMaterialOverride(UPhysicalMaterial* NewPhysMaterial);
	virtual URB_BodyInstance* GetRootBodyInstance();

	/** 
	 *	Used for creating one-way physics interactions.
	 *	@see RBDominanceGroup
	 */
	virtual void SetRBDominanceGroup(BYTE InDomGroup);

	/** Utility for calculating the current LocalToWorld matrix of this SkelMeshComp, given its parent transform. */
	FMatrix CalcCurrentLocalToWorld(const FMatrix& ParentMatrix);

	/** Simple, CPU evaluation of a vertex's skinned position (returned in component space) */
	FVector GetSkinnedVertexPosition(INT VertexIndex) const;

	void UpdateRBBonesFromSpaceBases(const FMatrix& CompLocalToWorld, UBOOL bMoveUnfixedBodies, UBOOL bTeleport);
	void UpdateRBJointMotors();
	void UpdateFixedClothVerts();

	/** Update forces applied to each cloth particle based on the ClothWind parameter. */
	void UpdateClothWindForces(FLOAT DeltaSeconds);

	/** Move all vertices in the cloth to the reference pose and zero their velocity. */
	void ResetClothVertsToRefPose();

	/** Forces apex clothing to use 'teleport and reset' for the next update */
	void ForceApexClothingTeleportAndReset();
	/** Forces apex clothing to use 'teleport' for the next update */
	void ForceApexClothingTeleport();

	/**
	* Looks up all bodies for broken constraints.
	* Makes sure child bodies of a broken constraints are not fixed and using bone springs, and child joints not motorized.
	*/
	void UpdateMeshForBrokenConstraints();

	/** 
	 *  Show/Hide Material - technical correct name for this is Section, but seems Material is mostly used
	 *  This disable rendering of certain Material ID (Section)
	 *
	 * @param MaterialID - id of the material to match a section on and to show/hide
	 * @param bShow - TRUE to show the section, otherwise hide it
	 * @param LODIndex - index of the lod entry since material mapping is unique to each LOD
	 */
	void ShowMaterialSection(INT MaterialID, UBOOL bShow, INT LODIndex);

	/** Enables or disables Gore Mesh Mode 
	 *  Disable only works in editor
	 */
	void EnableAltBoneWeighting(UBOOL bEnable, INT LOD=0);

	//Some get*() APIs
	FLOAT GetClothAttachmentResponseCoefficient();
	FLOAT GetClothAttachmentTearFactor();
	FLOAT GetClothBendingStiffness();
	FLOAT GetClothCollisionResponseCoefficient();
	FLOAT GetClothDampingCoefficient();
	INT GetClothFlags();
	FLOAT GetClothFriction();
	FLOAT GetClothPressure();
	FLOAT GetClothSleepLinearVelocity();
	INT GetClothSolverIterations();
	FLOAT GetClothStretchingStiffness();
	FLOAT GetClothTearFactor();
	FLOAT GetClothThickness();
	//some set*() APIs
	void SetClothAttachmentResponseCoefficient(FLOAT ClothAttachmentResponseCoefficient);
	void SetClothAttachmentTearFactor(FLOAT ClothAttachmentTearFactor);
	void SetClothBendingStiffness(FLOAT ClothBendingStiffness);
	void SetClothCollisionResponseCoefficient(FLOAT ClothCollisionResponseCoefficient);
	void SetClothDampingCoefficient(FLOAT ClothDampingCoefficient);
	void SetClothFlags(INT ClothFlags);
	void SetClothFriction(FLOAT ClothFriction);
	void SetClothPressure(FLOAT ClothPressure);
	void SetClothSleepLinearVelocity(FLOAT ClothSleepLinearVelocity);
	void SetClothSolverIterations(INT ClothSolverIterations);
	void SetClothStretchingStiffness(FLOAT ClothStretchingStiffness);
	void SetClothTearFactor(FLOAT ClothTearFactor);
	void SetClothThickness(FLOAT ClothThickness);
	//Other APIs
	void SetClothSleep(UBOOL IfClothSleep);
	void SetClothPosition(const FVector& ClothOffSet);
	void SetClothVelocity(const FVector& VelocityOffSet);
	//Attachment API
	void AttachClothToCollidingShapes(UBOOL AttatchTwoWay, UBOOL AttachTearable);
	//ValidBounds APIs
	void EnableClothValidBounds(UBOOL IfEnableClothValidBounds);
	void SetClothValidBounds(const FVector& ClothValidBoundsMin, const FVector& ClothValidBoundsMax);

	virtual void UpdateRBKinematicData();
	void SetEnableClothSimulation(UBOOL bInEnable);

	/** Toggle active simulation of cloth. Cheaper than doing SetEnableClothSimulation, and keeps its shape while frozen. */
	void SetClothFrozen(UBOOL bNewFrozen);

	/** Toggle active simulation of clothing and keeps its shape while frozen. */
	void SetEnableClothingSimulation(UBOOL bInEnable);

	void UpdateClothParams();
	void SetClothExternalForce(const FVector& InForce);

	/** Attach/detach verts from physics body that this components actor is attached to. */
	void SetAttachClothVertsToBaseBody(UBOOL bAttachVerts);

	/**
	 * Saves the skeletal component's current AnimSets to a temporary buffer.  You can restore them later by calling
	 * RestoreSavedAnimSets().  This is the C++ version of the method.  The script version just calls this one.
	 */
	void SaveAnimSets();

	/**
	 * Restores saved AnimSets to the master list of AnimSets and clears the temporary saved list of AnimSets.  This
	 * is the C++ version of the method.  The script version just calls this one.
	 */
	void RestoreSavedAnimSets();

	virtual void GenerateDecalRenderData(class FDecalState* Decal, TArray< FDecalRenderData* >& OutDecalRenderDatas) const;

	/**
	 * Transforms the specified decal info into reference pose space.
	 *
	 * @param	Decal			Info of decal to transform.
	 * @param	BoneIndex		The index of the bone hit by the decal.
	 * @return					A reference to the transformed decal info, or NULL if the operation failed.
	 */
	FDecalState* TransformDecalToRefPoseSpace(FDecalState* Decal, INT BoneIndex) const;

	/** 
	* @return TRUE if the primitive component can render decals
	*/
	virtual UBOOL SupportsDecalRendering() const;

	virtual FPrimitiveSceneProxy* CreateSceneProxy();	

#if WITH_NOVODEX
	virtual class NxActor* GetNxActor(FName BoneName = NAME_None);
	virtual class NxActor* GetIndexedNxActor(INT BodyIndex = INDEX_NONE);

	/** Utility for getting all physics bodies contained within this component. */
	virtual void GetAllNxActors(TArray<class NxActor*>& OutActors);

	virtual FVector NxGetPointVelocity(FVector LocationInWorldSpace);
#endif // WITH_NOVODEX

	void HideBone( INT BoneIndex, EPhysBodyOp PhysBodyOption );
	void UnHideBone( INT BoneIndex );
	UBOOL IsBoneHidden( INT BoneIndex );

	void HideBoneByName( FName BoneName, EPhysBodyOp PhysBodyOption);
	void UnHideBoneByName( FName BoneName );

	virtual FKCachedConvexData* GetBoneCachedPhysConvexData(const FVector& InScale3D, const FName& BoneName);

	// UMeshComponent interface.

	virtual UMaterialInterface* GetMaterial(INT MaterialIndex) const;
	virtual INT GetNumElements() const;

	/**
	 * Called by AnimNotify_PlayParticleEffect
	 * Looks for a socket name first then bone name
 	 *
	 * @param AnimNotifyData The AnimNotify_PlayParticleEffect which will have all of the various params on it
	 */
	 UBOOL eventPlayParticleEffect(const class UAnimNotify_PlayParticleEffect* AnimNotifyData)
	 {
	 	 Actor_eventPlayParticleEffect_Parms Parms(EC_EventParm);
	 	 Parms.AnimNotifyData=AnimNotifyData;
	 	 ProcessEvent(FindFunctionChecked(ENGINE_PlayParticleEffect),&Parms);
		return Parms.ReturnValue;
	 }

	 UBOOL eventCreateForceField(const class UAnimNotify_ForceField* AnimNotifyData)
	 {
		 Actor_eventCreateForceField_Parms Parms(EC_EventParm);
		 Parms.ReturnValue=FALSE;
		 Parms.AnimNotifyData=AnimNotifyData;
		 ProcessEvent(FindFunctionChecked(ENGINE_CreateForceField),&Parms);
		 return Parms.ReturnValue;
	 }

	UBOOL ExtractRootMotionCurve( FName AnimName, FLOAT SampleRate, FRootMotionCurve& out_RootMotionInterpCurve );

	// Script functions.
	DECLARE_FUNCTION(execSetMaterial);
	DECLARE_FUNCTION(execAttachComponent);
	DECLARE_FUNCTION(execDetachComponent);
	DECLARE_FUNCTION(execAttachComponentToSocket);
	DECLARE_FUNCTION(execGetSocketWorldLocationAndRotation);
	DECLARE_FUNCTION(execGetSocketByName);
	DECLARE_FUNCTION(execGetSocketBoneName);
	DECLARE_FUNCTION(execFindComponentAttachedToBone);
	DECLARE_FUNCTION(execIsComponentAttached);
	DECLARE_FUNCTION(execAttachedComponents);
	DECLARE_FUNCTION(execGetTransformMatrix);
	DECLARE_FUNCTION(execSetSkeletalMesh);
	DECLARE_FUNCTION(execSetPhysicsAsset);
	DECLARE_FUNCTION(execSetForceRefPose);
	DECLARE_FUNCTION(execSetParentAnimComponent);
	DECLARE_FUNCTION(execFindAnimSequence);
	DECLARE_FUNCTION(execFindMorphTarget);
	DECLARE_FUNCTION(execGetBoneQuaternion);
	DECLARE_FUNCTION(execGetBoneLocation);
	DECLARE_FUNCTION(execGetBoneAxis);
	DECLARE_FUNCTION(execTransformToBoneSpace);
	DECLARE_FUNCTION(execTransformFromBoneSpace);
	DECLARE_FUNCTION(execFindClosestBone);
	DECLARE_FUNCTION(execGetClosestCollidingBoneLocation);
	DECLARE_FUNCTION(execSetAnimTreeTemplate);
	DECLARE_FUNCTION(execUpdateParentBoneMap);
	DECLARE_FUNCTION(execInitSkelControls);
	DECLARE_FUNCTION(execInitMorphTargets);
	DECLARE_FUNCTION(execFindAnimNode);
	DECLARE_FUNCTION(execAllAnimNodes);
	DECLARE_FUNCTION(execFindSkelControl);
	DECLARE_FUNCTION(execFindMorphNode);
	DECLARE_FUNCTION(execFindConstraintIndex);
	DECLARE_FUNCTION(execFindConstraintBoneName);
	DECLARE_FUNCTION(execFindBodyInstanceNamed);
	DECLARE_FUNCTION(execForceSkelUpdate);
	DECLARE_FUNCTION(execUpdateAnimations);
	DECLARE_FUNCTION(execGetBonesWithinRadius);
	DECLARE_FUNCTION(execAddInstanceVertexWeightBoneParented);
	DECLARE_FUNCTION(execRemoveInstanceVertexWeightBoneParented);
	DECLARE_FUNCTION(execFindInstanceVertexweightBonePair);
	DECLARE_FUNCTION(execUpdateInstanceVertexWeightBones);
	DECLARE_FUNCTION(execToggleInstanceVertexWeights);
	DECLARE_FUNCTION(execSetHasPhysicsAssetInstance);
	DECLARE_FUNCTION(execUpdateRBBonesFromSpaceBases);
	DECLARE_FUNCTION(execPlayFaceFXAnim);
	DECLARE_FUNCTION(execStopFaceFXAnim);
	DECLARE_FUNCTION(execIsPlayingFaceFXAnim);
	DECLARE_FUNCTION(execDeclareFaceFXRegister);
	DECLARE_FUNCTION(execGetFaceFXRegister);
	DECLARE_FUNCTION(execSetFaceFXRegister);
	DECLARE_FUNCTION(execSetFaceFXRegisterEx);
	DECLARE_FUNCTION(execSetEnableClothingSimulation);
	DECLARE_FUNCTION(execSetEnableClothSimulation);
	DECLARE_FUNCTION(execSetClothFrozen);
	DECLARE_FUNCTION(execUpdateClothParams);
	DECLARE_FUNCTION(execSetClothExternalForce);
	DECLARE_FUNCTION(execSetAttachClothVertsToBaseBody);
	DECLARE_FUNCTION(execResetClothVertsToRefPose);
	DECLARE_FUNCTION(execForceApexClothingTeleportAndReset);
	DECLARE_FUNCTION(execForceApexClothingTeleport);
	DECLARE_FUNCTION(execUpdateMeshForBrokenConstraints);
	DECLARE_FUNCTION(execShowMaterialSection);

	//Some get*() APIs
	DECLARE_FUNCTION(execGetClothAttachmentResponseCoefficient);
	DECLARE_FUNCTION(execGetClothAttachmentTearFactor);
	DECLARE_FUNCTION(execGetClothBendingStiffness);
	DECLARE_FUNCTION(execGetClothCollisionResponseCoefficient);
	DECLARE_FUNCTION(execGetClothDampingCoefficient);
	DECLARE_FUNCTION(execGetClothFlags);
	DECLARE_FUNCTION(execGetClothFriction);
	DECLARE_FUNCTION(execGetClothPressure);
	DECLARE_FUNCTION(execGetClothSleepLinearVelocity);
	DECLARE_FUNCTION(execGetClothSolverIterations);
	DECLARE_FUNCTION(execGetClothStretchingStiffness);
	DECLARE_FUNCTION(execGetClothTearFactor);
	DECLARE_FUNCTION(execGetClothThickness);
	//some set*() APIs
	DECLARE_FUNCTION(execSetClothAttachmentResponseCoefficient);
	DECLARE_FUNCTION(execSetClothAttachmentTearFactor);
	DECLARE_FUNCTION(execSetClothBendingStiffness);
	DECLARE_FUNCTION(execSetClothCollisionResponseCoefficient);
	DECLARE_FUNCTION(execSetClothDampingCoefficient);
	DECLARE_FUNCTION(execSetClothFlags);
	DECLARE_FUNCTION(execSetClothFriction);
	DECLARE_FUNCTION(execSetClothPressure);
	DECLARE_FUNCTION(execSetClothSleepLinearVelocity);
	DECLARE_FUNCTION(execSetClothSolverIterations);
	DECLARE_FUNCTION(execSetClothStretchingStiffness);
	DECLARE_FUNCTION(execSetClothTearFactor);
	DECLARE_FUNCTION(execSetClothThickness);
	//Other APIs
	DECLARE_FUNCTION(execSetClothSleep);
	DECLARE_FUNCTION(execSetClothPosition);
	DECLARE_FUNCTION(execSetClothVelocity);
	//Attachment API
	DECLARE_FUNCTION(execAttachClothToCollidingShapes);
	//ValidBounds APIs
	DECLARE_FUNCTION(execEnableClothValidBounds);
	DECLARE_FUNCTION(execSetClothValidBounds);

	DECLARE_FUNCTION(execSaveAnimSets);
	DECLARE_FUNCTION(execRestoreSavedAnimSets);
	DECLARE_FUNCTION(execGetBoneMatrix);
	DECLARE_FUNCTION(execMatchRefBone);
	DECLARE_FUNCTION(execGetBoneName);
	DECLARE_FUNCTION(execGetParentBone);
	DECLARE_FUNCTION(execGetBoneNames);
	DECLARE_FUNCTION(execBoneIsChildOf);
	DECLARE_FUNCTION(execGetRefPosePosition);

	DECLARE_FUNCTION(execUpdateSoftBodyParams);
	DECLARE_FUNCTION(execSetSoftBodyFrozen);
	DECLARE_FUNCTION(execWakeSoftBody);

	DECLARE_FUNCTION(execHideBone);
	DECLARE_FUNCTION(execUnHideBone);
	DECLARE_FUNCTION(execIsBoneHidden);

	DECLARE_FUNCTION(execHideBoneByName);
	DECLARE_FUNCTION(execUnHideBoneByName);

	DECLARE_FUNCTION(execGetPosition);
	DECLARE_FUNCTION(execGetRotation);
};

class FSkeletalMeshComponentReattachContext
{
public:

	/** Initialization constructor. */
	FSkeletalMeshComponentReattachContext( class USkeletalMesh* SkeletalMesh )
	{
		for( TObjectIterator<USkeletalMeshComponent> It; It; ++It )
		{
			if ( It->SkeletalMesh == SkeletalMesh )
			{
				new(ReattachContexts) FComponentReattachContext( *It );
			}
		}

		// Flush the rendering commands generated by the detachments.
		FlushRenderingCommands();
	}

private:
	TIndirectArray<FComponentReattachContext> ReattachContexts;
};


/*-----------------------------------------------------------------------------
	USkeletalMesh.
-----------------------------------------------------------------------------*/

struct FMeshWedge
{
	DWORD			iVertex;			// Vertex index.
	FVector2D		UVs[MAX_TEXCOORDS];	// UVs.
	FColor			Color;			// Vertex color.
	friend FArchive &operator<<( FArchive& Ar, FMeshWedge& T )
	{
		if (Ar.Ver() < VER_DWORD_SKELETAL_MESH_INDICES)
		{
			WORD LegacyVert;
			Ar << LegacyVert;
			T.iVertex = LegacyVert;
		}
		else
		{
			Ar << T.iVertex;
		}
		
		if( Ar.Ver() < VER_ADDED_MULTIPLE_UVS_TO_SKELETAL_MESH )
		{
			// This package is older, just serialize the first set of UV's
			Ar << T.UVs[0].X << T.UVs[0].Y;
		}
		else
		{
			// This package has multiple UV's so serialize them all
			for( INT UVIdx = 0; UVIdx < MAX_TEXCOORDS; ++UVIdx )
			{
				Ar << T.UVs[UVIdx];
			}
		}

		if( Ar.Ver() < VER_ADDED_SKELETAL_MESH_VERTEX_COLORS )
		{
			// Initialize color to white.
			T.Color = FColor(255,255,255);
		}
		else
		{
			Ar << T.Color;
		}

		return Ar;
	}
};
template <> struct TIsPODType<FMeshWedge> { enum { Value = true }; };

struct FMeshFace
{
	DWORD		iWedge[3];			// Textured Vertex indices.
	WORD		MeshMaterialIndex;	// Source Material (= texture plus unique flags) index.

    FVector	TangentX[3];
    FVector	TangentY[3];
    FVector	TangentZ[3];
    UBOOL   bOverrideTangentBasis;  //override tangents data of unreal
	friend FArchive &operator<<( FArchive& Ar, FMeshFace& F )
	{
		if (Ar.Ver() < VER_DWORD_SKELETAL_MESH_INDICES)
		{
			WORD LegacyVertIdx[3];
			Ar << LegacyVertIdx[0] << LegacyVertIdx[1] << LegacyVertIdx[2];
			F.iWedge[0] = LegacyVertIdx[0];
			F.iWedge[1] = LegacyVertIdx[1];
			F.iWedge[2] = LegacyVertIdx[2];
		}
		else
		{
			Ar << F.iWedge[0] << F.iWedge[1] << F.iWedge[2];
		}
		Ar << F.MeshMaterialIndex;
		Ar << F.TangentX[0] << F.TangentX[1] << F.TangentX[2];
        Ar << F.TangentY[0] << F.TangentY[1] << F.TangentY[2];
        Ar << F.TangentZ[0] << F.TangentZ[1] << F.TangentZ[2];
        Ar << F.bOverrideTangentBasis;
		return Ar;
	}
};
template <> struct TIsPODType<FMeshFace> { enum { Value = true }; };

// A bone: an orientation, and a position, all relative to their parent.
struct VJointPos
{
	FQuat   	Orientation;  //
	FVector		Position;     //  needed or not ?

	FLOAT       Length;       //  For collision testing / debugging drawing...
	FLOAT       XSize;
	FLOAT       YSize;
	FLOAT       ZSize;

	friend FArchive &operator<<( FArchive& Ar, VJointPos& V )
	{
		return Ar << V.Orientation << V.Position;
	}
};
template <> struct TIsPODType<VJointPos> { enum { Value = true }; };

/*
This class is to keep compatibility with ActorX FQuat
*/
struct FQuatNoAlign
{
	// Variables.
	FLOAT X,Y,Z,W;

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FQuatNoAlign& F )
	{
		return Ar << F.X << F.Y << F.Z << F.W;
	}
};

// NoAlign VJointPos - To keep alignment working with ActorX
struct VJointPosNoAlign
{
	FQuatNoAlign   	Orientation;  //
	FVector			Position;     //  needed or not ?

	FLOAT       Length;       //  For collision testing / debugging drawing...
	FLOAT       XSize;
	FLOAT       YSize;
	FLOAT       ZSize;
};

// Reference-skeleton bone, the package-serializable version.
struct FMeshBone
{
	FName 		Name;		  // Bone's name.
	DWORD		Flags;        // reserved
	VJointPos	BonePos;      // reference position
	INT         ParentIndex;  // 0/NULL if this is the root bone.  
	INT 		NumChildren;  // children  // only needed in animation ?
	INT         Depth;        // Number of steps to root in the skeletal hierarcy; root=0.

	// DEBUG rendering
	FColor		BoneColor;		// Color to use when drawing bone on screen.

	UBOOL operator==( const FMeshBone& B ) const
	{
		return( Name == B.Name );
	}
	
	friend FArchive &operator<<( FArchive& Ar, FMeshBone& F)
	{
		Ar << F.Name << F.Flags << F.BonePos << F.NumChildren << F.ParentIndex;

		if( Ar.IsLoading() && Ar.Ver() < VER_SKELMESH_DRAWSKELTREEMANAGER )
		{
			F.BoneColor = FColor(255, 255, 255, 255);
		}
		else
		{
			Ar << F.BoneColor;
		}

		return Ar;
	}
};
template <> struct TIsPODType<FMeshBone> { enum { Value = true }; };

// Textured triangle.
struct VTriangle
{
	DWORD   WedgeIndex[3];	 // Point to three vertices in the vertex list.
	BYTE    MatIndex;	     // Materials can be anything.
	BYTE    AuxMatIndex;     // Second material from exporter (unused)
	DWORD   SmoothingGroups; // 32-bit flag for smoothing groups.

 FVector	TangentX[3];
    FVector	TangentY[3];
    FVector	TangentZ[3];
    UBOOL   bOverrideTangentBasis;  //override tangents data of unreal

	friend FArchive &operator<<( FArchive& Ar, VTriangle& V )
	{
		if (Ar.Ver() < VER_DWORD_SKELETAL_MESH_INDICES)
		{
			WORD LegacyVertIdx[3];
			Ar << LegacyVertIdx[0] << LegacyVertIdx[1] << LegacyVertIdx[2];
			V.WedgeIndex[0] = LegacyVertIdx[0];
			V.WedgeIndex[1] = LegacyVertIdx[1];
			V.WedgeIndex[2] = LegacyVertIdx[2];
		}
		else
		{
			Ar << V.WedgeIndex[0] << V.WedgeIndex[1] << V.WedgeIndex[2];
		}
		Ar << V.MatIndex << V.AuxMatIndex;
		Ar << V.SmoothingGroups;
        Ar << V.TangentX[0] << V.TangentX[1] << V.TangentX[2];
        Ar << V.TangentY[0] << V.TangentY[1] << V.TangentY[2];
        Ar << V.TangentZ[0] << V.TangentZ[1] << V.TangentZ[2];
		Ar << V.bOverrideTangentBasis;
		return Ar;
	}

	VTriangle& operator=( const VTriangle& Other)
	{
		this->AuxMatIndex = Other.AuxMatIndex;
		this->MatIndex        =  Other.MatIndex;
		this->SmoothingGroups =  Other.SmoothingGroups;
		this->WedgeIndex[0]   =  Other.WedgeIndex[0];
		this->WedgeIndex[1]   =  Other.WedgeIndex[1];
		this->WedgeIndex[2]   =  Other.WedgeIndex[2];
        this->TangentX[0]   =  Other.TangentX[0];
        this->TangentX[1]   =  Other.TangentX[1];
        this->TangentX[2]   =  Other.TangentX[2];

        this->TangentY[0]   =  Other.TangentY[0];
        this->TangentY[1]   =  Other.TangentY[1];
        this->TangentY[2]   =  Other.TangentY[2];

        this->TangentZ[0]   =  Other.TangentZ[0];
        this->TangentZ[1]   =  Other.TangentZ[1];
        this->TangentZ[2]   =  Other.TangentZ[2];

        this->bOverrideTangentBasis   =  Other.bOverrideTangentBasis;
		return *this;
	}
};
template <> struct TIsPODType<VTriangle> { enum { Value = true }; };

struct FVertInfluence 
{
	FLOAT Weight;
	DWORD VertIndex;
	WORD BoneIndex;
	friend FArchive &operator<<( FArchive& Ar, FVertInfluence& F )
	{
		if (Ar.Ver() < VER_DWORD_SKELETAL_MESH_INDICES)
		{
			WORD LegacyVertIdx;
			Ar << F.Weight << LegacyVertIdx << F.BoneIndex;
			F.VertIndex = LegacyVertIdx;
		}
		else
		{
			Ar << F.Weight << F.VertIndex << F.BoneIndex;
		}
		return Ar;
	}
};
template <> struct TIsPODType<FVertInfluence> { enum { Value = true }; };

/**
* Data needed for importing an extra set of vertex influences
*/
struct FSkelMeshExtraInfluenceImportData
{
	TArray<FMeshBone> RefSkeleton;
	TArray<FVertInfluence> Influences;
	TArray<FMeshWedge> Wedges;
	TArray<FMeshFace> Faces;
	TArray<FVector> Points;
	EInstanceWeightUsage Usage;
	INT MaxBoneCountPerChunk;
};

//
//	FSoftSkinVertex
//

struct FSoftSkinVertex
{
	FVector			Position;
	FPackedNormal	TangentX,	// Tangent, U-direction
					TangentY,	// Binormal, V-direction
					TangentZ;	// Normal
	FVector2D		UVs[MAX_TEXCOORDS]; // UVs
	FColor			Color;		// VertexColor
	BYTE			InfluenceBones[MAX_INFLUENCES];
	BYTE			InfluenceWeights[MAX_INFLUENCES];

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	* @param V - vertex to serialize
	* @return archive that was used
	*/
	friend FArchive& operator<<(FArchive& Ar,FSoftSkinVertex& V);
};

//
//	FRigidSkinVertex
//

struct FRigidSkinVertex
{
	FVector			Position;
	FPackedNormal	TangentX,	// Tangent, U-direction
					TangentY,	// Binormal, V-direction
					TangentZ;	// Normal
	FVector2D		UVs[MAX_TEXCOORDS]; // UVs
	FColor			Color;		// Vertex color.
	BYTE			Bone;

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	* @param V - vertex to serialize
	* @return archive that was used
	*/
	friend FArchive& operator<<(FArchive& Ar,FRigidSkinVertex& V);
};

/**
 * A set of the skeletal mesh vertices which use the same set of <MAX_GPUSKIN_BONES bones.
 * In practice, there is a 1:1 mapping between chunks and sections, but for meshes which
 * were imported before chunking was implemented, there will be a single chunk for all
 * sections.
 */
struct FSkelMeshChunk
{
	/** The offset into the LOD's vertex buffer of this chunk's vertices. */
	UINT BaseVertexIndex;

	/** The rigid vertices of this chunk. */
	TArray<FRigidSkinVertex> RigidVertices;

	/** The soft vertices of this chunk. */
	TArray<FSoftSkinVertex> SoftVertices;

	/** The bones which are used by the vertices of this chunk. Indices of bones in the USkeletalMesh::RefSkeleton array */
	TArray<WORD> BoneMap;

	/** The number of rigid vertices in this chunk */
	INT NumRigidVertices;
	/** The number of soft vertices in this chunk */
	INT NumSoftVertices;

	/** max # of bones used to skin the vertices in this chunk */
	INT MaxBoneInfluences;

	FSkelMeshChunk()
		: BaseVertexIndex(0)
		, NumRigidVertices(0)
		, NumSoftVertices(0)
		, MaxBoneInfluences(4)
	{}

	FSkelMeshChunk(const FSkelMeshChunk& Other)
	{
		BaseVertexIndex = Other.BaseVertexIndex;
		RigidVertices = Other.RigidVertices;
		SoftVertices = Other.SoftVertices;
		BoneMap = Other.BoneMap;
		NumRigidVertices = Other.NumRigidVertices;
		NumSoftVertices = Other.NumSoftVertices;
		MaxBoneInfluences = Other.MaxBoneInfluences;
	}

	/**
	* @return total num rigid verts for this chunk
	*/
	FORCEINLINE INT GetNumRigidVertices() const
	{
		return NumRigidVertices;
	}

	/**
	* @return total num soft verts for this chunk
	*/
	FORCEINLINE INT GetNumSoftVertices() const
	{
		return NumSoftVertices;
	}

	/**
	* @return total number of soft and rigid verts for this chunk
	*/
	FORCEINLINE INT GetNumVertices() const
	{
		return GetNumRigidVertices() + GetNumSoftVertices();
	}

	/**
	* @return starting index for rigid verts for this chunk in the LOD vertex buffer
	*/
	FORCEINLINE INT GetRigidVertexBufferIndex() const
	{
        return BaseVertexIndex;
	}

	/**
	* @return starting index for soft verts for this chunk in the LOD vertex buffer
	*/
	FORCEINLINE INT GetSoftVertexBufferIndex() const
	{
        return BaseVertexIndex + NumRigidVertices;
	}

	/**
	* Calculate max # of bone influences used by this skel mesh chunk
	*/
	void CalcMaxBoneInfluences();

	/**
	* Serialize this class
	* @param Ar - archive to serialize to
	* @param C - skel mesh chunk to serialize
	*/
	friend FArchive& operator<<(FArchive& Ar,FSkelMeshChunk& C)
	{
		Ar << C.BaseVertexIndex;
		Ar << C.RigidVertices;
		Ar << C.SoftVertices;
		Ar << C.BoneMap;
		Ar << C.NumRigidVertices;
		Ar << C.NumSoftVertices;
		Ar << C.MaxBoneInfluences;
		return Ar;
	}
};

enum EBoneBreakOption
{
	BONEBREAK_SoftPreferred		=0, 
	BONEBREAK_AutoDetect		=1,
	BONEBREAK_RigidPreferred	=2
};

enum ETriangleSortOption
{
	TRISORT_None						= 0,
	TRISORT_CenterRadialDistance		= 1,
	TRISORT_Random						= 2,
	TRISORT_MergeContiguous				= 3,
	TRISORT_Custom						= 4,
	TRISORT_CustomLeftRight				= 5,
};

/** Helper to convert the above enum to string */
static const TCHAR* TriangleSortOptionToString(ETriangleSortOption Option)
{
	switch(Option)
	{
		case TRISORT_CenterRadialDistance:
			return TEXT("CenterRadialDistance");
		case TRISORT_Random:
			return TEXT("Random");
		case TRISORT_MergeContiguous:
			return TEXT("MergeContiguous");
		case TRISORT_Custom:
			return TEXT("Custom");
		case TRISORT_CustomLeftRight:
			return TEXT("CustomLeftRight");
	}
	return TEXT("None");
}


/** Enum indicating which method to use to generate per-vertex cloth vert movement scale (ClothMovementScale) */
enum EClothMovementScaleGen
{
	ECMDM_DistToFixedVert				= 0,
	ECMDM_VertexBoneWeight				= 1,
	ECMDM_Empty							= 2,
};

/**
 * A set of skeletal mesh triangles which use the same material and chunk.
 */
struct FSkelMeshSection
{
	/** Material (texture) used for this section. */
	WORD MaterialIndex;

	/** The chunk that vertices for this section are from. */
	WORD ChunkIndex;

	/** The offset of this section's indices in the LOD's index buffer. */
	DWORD BaseIndex;

	/** The number of triangles in this section. */
	DWORD NumTriangles;

	/** Current triangle sorting method */
	BYTE TriangleSorting;

	/** Is this mesh selected? */
	BYTE bSelected:1;

	FSkelMeshSection()
		: MaterialIndex(0)
		, ChunkIndex(0)
		, BaseIndex(0)
		, NumTriangles(0)
		, TriangleSorting(0)
		, bSelected(0)
	{}

	// Serialization.
	friend FArchive& operator<<(FArchive& Ar,FSkelMeshSection& S)
	{		
		Ar << S.MaterialIndex;
		Ar << S.ChunkIndex;
		Ar << S.BaseIndex;
		
		if (Ar.Ver() < VER_DWORD_SKELETAL_MESH_INDICES)
		{
			WORD NumTriangles;
			Ar << NumTriangles;
			S.NumTriangles = NumTriangles;
		}
		else
		{
			Ar << S.NumTriangles;
		}
		
		if( Ar.Ver() >= VER_SKELETAL_MESH_SORTING_OPTIONS )
		{
			Ar << S.TriangleSorting;
		}
		else if( Ar.IsLoading() )
		{
			S.TriangleSorting = TRISORT_None;
		}

		return Ar;
	}
};
template <> struct TIsPODType<FSkelMeshSection> { enum { Value = true }; };

/**
* Base vertex data for GPU skinned skeletal meshes
*/
struct FGPUSkinVertexBase
{
	FPackedNormal	TangentX,	// Tangent, U-direction
					TangentZ;	// Normal	
	BYTE			InfluenceBones[MAX_INFLUENCES];
	BYTE			InfluenceWeights[MAX_INFLUENCES];

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	*/
	void Serialize(FArchive& Ar, FVector & OutPosition);
	void Serialize(FArchive& Ar);
};

/** 
* 16 bit UV version of skeletal mesh vertex
*/
template<UINT NumTexCoords=1>
struct TGPUSkinVertexFloat16Uvs : public FGPUSkinVertexBase
{
	/** full float position **/
	FVector			Position;
	/** half float UVs */
	FVector2DHalf	UVs[NumTexCoords];

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	* @param V - vertex to serialize
	* @return archive that was used
	*/
	friend FArchive& operator<<(FArchive& Ar,TGPUSkinVertexFloat16Uvs& V)
	{
		// If prior to VER_SKELETAL_MESH_SUPPORT_PACKED_POSITION, then 
		// you need to get position from parent class
		if( Ar.Ver() < VER_SKELETAL_MESH_SUPPORT_PACKED_POSITION )
		{
			V.Serialize(Ar, V.Position);
		}
		else
		{
			V.Serialize(Ar);
			Ar << V.Position;
		}

		for(UINT UVIndex = 0;UVIndex < NumTexCoords;UVIndex++)
		{
			Ar << V.UVs[UVIndex];
		}
		return Ar;
	}
};

/** 
* 32 bit UV version of skeletal mesh vertex
*/
template<UINT NumTexCoords=1>
struct TGPUSkinVertexFloat32Uvs : public FGPUSkinVertexBase
{
	/** full float position **/
	FVector			Position;
	/** full float UVs */
	FVector2D UVs[NumTexCoords];

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	* @param V - vertex to serialize
	* @return archive that was used
	*/
	friend FArchive& operator<<(FArchive& Ar,TGPUSkinVertexFloat32Uvs& V)
	{
		// If prior to VER_SKELETAL_MESH_SUPPORT_PACKED_POSITION, then 
		// you need to get position from parent class
		if( Ar.Ver() < VER_SKELETAL_MESH_SUPPORT_PACKED_POSITION )
		{
			V.Serialize(Ar, V.Position);
		}
		else
		{
			V.Serialize(Ar);
			Ar << V.Position;
		}

		for(UINT UVIndex = 0;UVIndex < NumTexCoords;UVIndex++)
		{
			Ar << V.UVs[UVIndex];
		}
		return Ar;
	}
};

/** 
* 16 bit XYZ/16 bit UV version of skeletal mesh vertex
*/
template<UINT NumTexCoords=1>
struct TGPUSkinVertexFloat16Uvs32Xyz : public FGPUSkinVertexBase
{
	FPackedPosition Position;
	/** half float UVs */
	FVector2DHalf	UVs[NumTexCoords];
	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	* @param V - vertex to serialize
	* @return archive that was used
	*/
	friend FArchive& operator<<(FArchive& Ar,TGPUSkinVertexFloat16Uvs32Xyz& V)
	{
		V.Serialize(Ar);
		
		// If after VER_SKELETAL_MESH_SUPPORT_PACKED_POSITION, then 
		// you need to serialize position 
		// Otherwise, don't serialize. Just by any reason, serialize happens with this struct
		if( Ar.Ver() >= VER_SKELETAL_MESH_SUPPORT_PACKED_POSITION )
		{
			Ar << V.Position;
		}

		for(UINT UVIndex = 0;UVIndex < NumTexCoords;UVIndex++)
		{
			Ar << V.UVs[UVIndex];
		}
		return Ar;
	}
};

/** 
* 16 bit XYZ/32 bit UV version of skeletal mesh vertex
*/
template<UINT NumTexCoords=1>
struct TGPUSkinVertexFloat32Uvs32Xyz : public FGPUSkinVertexBase
{
	FPackedPosition Position;
	/** full float UVs */
	FVector2D	UVs[NumTexCoords];

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	* @param V - vertex to serialize
	* @return archive that was used
	*/
	friend FArchive& operator<<(FArchive& Ar,TGPUSkinVertexFloat32Uvs32Xyz& V)
	{
		V.Serialize(Ar);
		// If after VER_SKELETAL_MESH_SUPPORT_PACKED_POSITION, then 
		// you need to serialize position 
		// Otherwise, don't serialize. Just by any reason, serialize happens with this struct		
		if( Ar.Ver() >= VER_SKELETAL_MESH_SUPPORT_PACKED_POSITION )
		{
			Ar << V.Position;
		}

		for(UINT UVIndex = 0;UVIndex < NumTexCoords;UVIndex++)
		{
			Ar << V.UVs[UVIndex];
		}
		return Ar;
	}
};

/**
 * A structure for holding a skeletal mesh vertex color
 */
struct FGPUSkinVertexColor
{
	/** VertexColor */
	FColor VertexColor;

	/**
	 * Serializer
	 *
	 * @param Ar - archive to serialize with
	 * @param V - vertex to serialize
	 * @return archive that was used
	 */
	friend FArchive& operator<<(FArchive& Ar, FGPUSkinVertexColor& V)
	{
		// Serialization really shouldn't get this far if our package version is older
		// Just in case, we should guard around it
		if( Ar.Ver() >= VER_ADDED_SKELETAL_MESH_VERTEX_COLORS )
		{
			Ar << V.VertexColor;
		}

		return Ar;
	}
};

/** An interface to the skel-mesh vertex data storage type. */
class FSkeletalMeshVertexDataInterface
{
public:

	/** Virtual destructor. */
	virtual ~FSkeletalMeshVertexDataInterface() {}

	/**
	* Resizes the vertex data buffer, discarding any data which no longer fits.
	* @param NumVertices - The number of vertices to allocate the buffer for.
	*/
	virtual void ResizeBuffer(UINT NumVertices) = 0;

	/** @return The stride of the vertex data in the buffer. */
	virtual UINT GetStride() const = 0;

	/** @return A pointer to the data in the buffer. */
	virtual BYTE* GetDataPointer() = 0;

	/** @return number of vertices in the buffer */
	virtual UINT GetNumVertices() = 0;

	/** @return A pointer to the FResourceArrayInterface for the vertex data. */
	virtual FResourceArrayInterface* GetResourceArray() = 0;

	/** Serializer. */
	virtual void Serialize(FArchive& Ar) = 0;
};


/** The implementation of the skeletal mesh vertex data storage type. */
template<typename VertexDataType>
class TSkeletalMeshVertexData :
	public FSkeletalMeshVertexDataInterface,
	public TResourceArray<VertexDataType,VERTEXBUFFER_ALIGNMENT>
{
public:
	typedef TResourceArray<VertexDataType,VERTEXBUFFER_ALIGNMENT> ArrayType;

	/**
	* Constructor
	* @param InNeedsCPUAccess - TRUE if resource array data should be CPU accessible
	*/
	TSkeletalMeshVertexData(UBOOL InNeedsCPUAccess=FALSE)
		:	TResourceArray<VertexDataType,VERTEXBUFFER_ALIGNMENT>(InNeedsCPUAccess)
	{
	}
	
	/**
	* Resizes the vertex data buffer, discarding any data which no longer fits.
	*
	* @param NumVertices - The number of vertices to allocate the buffer for.
	*/
	virtual void ResizeBuffer(UINT NumVertices)
	{
		if((UINT)ArrayType::Num() < NumVertices)
		{
			// Enlarge the array.
			ArrayType::Add(NumVertices - ArrayType::Num());
		}
		else if((UINT)ArrayType::Num() > NumVertices)
		{
			// Shrink the array.
			ArrayType::Remove(NumVertices,ArrayType::Num() - NumVertices);
		}
	}
	/**
	* @return stride of the vertex type stored in the resource data array
	*/
	virtual UINT GetStride() const
	{
		return sizeof(VertexDataType);
	}
	/**
	* @return BYTE pointer to the resource data array
	*/
	virtual BYTE* GetDataPointer()
	{
		return (BYTE*)&(*this)(0);
	}
	/**
	* @return number of vertices stored in the resource data array
	*/
	virtual UINT GetNumVertices()
	{
		return ArrayType::Num();
	}
	/**
	* @return resource array interface access
	*/
	virtual FResourceArrayInterface* GetResourceArray()
	{
		return this;
	}
	/**
	* Serializer for this class
	*
	* @param Ar - archive to serialize to
	* @param B - data to serialize
	*/
	virtual void Serialize(FArchive& Ar)
	{
		ArrayType::BulkSerialize(Ar);
	}
	/**
	* Assignment operator. This is currently the only method which allows for 
	* modifying an existing resource array
	*/
	TSkeletalMeshVertexData<VertexDataType>& operator=(const TArray<VertexDataType>& Other)
	{
		ArrayType::operator=(Other);
		return *this;
	}
};

/** 
* Vertex buffer with static lod chunk vertices for use with GPU skinning 
*/
class FSkeletalMeshVertexBuffer : public FVertexBuffer
{
public:
	/**
	* Constructor
	*/
	FSkeletalMeshVertexBuffer();

	/**
	* Destructor
	*/
	virtual ~FSkeletalMeshVertexBuffer();

	/**
	* Assignment. Assumes that vertex buffer will be rebuilt 
	*/
	FSkeletalMeshVertexBuffer& operator=(const FSkeletalMeshVertexBuffer& Other);
	/**
	* Constructor (copy)
	*/
	FSkeletalMeshVertexBuffer(const FSkeletalMeshVertexBuffer& Other);

	/** 
	* Delete existing resources 
	*/
	void CleanUp();

	/**
	* Initializes the buffer with the given vertices.
	* @param InVertices - The vertices to initialize the buffer with.
	*/
	void Init(const TArray<FSoftSkinVertex>& InVertices);

	/**
	* Serializer for this class
	* @param Ar - archive to serialize to
	* @param B - data to serialize
	*/
	friend FArchive& operator<<(FArchive& Ar,FSkeletalMeshVertexBuffer& VertexBuffer);

	// FRenderResource interface.

	/**
	* Initialize the RHI resource for this vertex buffer
	*/
	virtual void InitRHI();

	/**
	* @return text description for the resource type
	*/
	virtual FString GetFriendlyName() const;

	// Vertex data accessors.

	/** 
	* Const access to entry in vertex data array
	*
	* @param VertexIndex - index into the vertex buffer
	* @return pointer to vertex data cast to base vertex type
	*/
	FORCEINLINE const FGPUSkinVertexBase* GetVertexPtr(UINT VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return (FGPUSkinVertexBase*)(Data + VertexIndex * Stride);
	}
	/** 
	* Non=Const access to entry in vertex data array
	*
	* @param VertexIndex - index into the vertex buffer
	* @return pointer to vertex data cast to base vertex type
	*/
	FORCEINLINE FGPUSkinVertexBase* GetVertexPtr(UINT VertexIndex)
	{
		checkSlow(VertexIndex < GetNumVertices());
		return (FGPUSkinVertexBase*)(Data + VertexIndex * Stride);
	}
	/**
	* Get the vertex UV values at the given index in the vertex buffer
	*
	* @param VertexIndex - index into the vertex buffer
	* @param UVIndex - [0,MAX_TEXCOORDS] value to index into UVs array
	* @return 2D UV values
	*/
	FORCEINLINE FVector2D GetVertexUV(UINT VertexIndex,UINT UVIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		if( !bUseFullPrecisionUVs )
		{
#if CONSOLE
			if (GetUsePackedPosition())
			{
				return ((TGPUSkinVertexFloat16Uvs32Xyz<MAX_TEXCOORDS>*)(Data + VertexIndex * Stride))->UVs[UVIndex];
			}
			else
#endif
			{
				return ((TGPUSkinVertexFloat16Uvs<MAX_TEXCOORDS>*)(Data + VertexIndex * Stride))->UVs[UVIndex];
			}
		}
		else
		{
#if CONSOLE
			if (GetUsePackedPosition())
			{
				return ((TGPUSkinVertexFloat32Uvs32Xyz<MAX_TEXCOORDS>*)(Data + VertexIndex * Stride))->UVs[UVIndex];
			}
			else
#endif
			{
				return ((TGPUSkinVertexFloat32Uvs<MAX_TEXCOORDS>*)(Data + VertexIndex * Stride))->UVs[UVIndex];
			}
		}		
	}
	
	/**
	* Get the vertex XYZ values at the given index in the vertex buffer
	*
	* @param VertexIndex - index into the vertex buffer
	* @return FVector 3D position
	*/
	FORCEINLINE FVector GetVertexPosition(UINT VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return GetVertexPosition((const FGPUSkinVertexBase*)(Data + VertexIndex * Stride));
	}

	/**
	* Get the vertex XYZ values of the given SrcVertex
	*
	* @param FGPUSkinVertexBase *
	* @return FVector 3D position
	*/
	FORCEINLINE FVector GetVertexPosition(const FGPUSkinVertexBase* SrcVertex) const
	{
		if( !bUseFullPrecisionUVs )
		{
#if CONSOLE
			if (GetUsePackedPosition())
			{
				return (FVector)(((TGPUSkinVertexFloat16Uvs32Xyz<MAX_TEXCOORDS>*)(SrcVertex))->Position)* MeshExtension + MeshOrigin;
			}
			else
#endif
			{
				return ((TGPUSkinVertexFloat16Uvs<MAX_TEXCOORDS>*)(SrcVertex))->Position;
			}
		}
		else
		{
#if CONSOLE
			if (GetUsePackedPosition())
			{
				return (FVector)(((TGPUSkinVertexFloat32Uvs32Xyz<MAX_TEXCOORDS>*)(SrcVertex))->Position)* MeshExtension + MeshOrigin;
			}
			else
#endif
			{
				return ((TGPUSkinVertexFloat32Uvs<MAX_TEXCOORDS>*)(SrcVertex))->Position;
			}
		}		
	}
	// Other accessors.

	/** 
	* @return TRUE if using 32 bit floats for UVs 
	*/
	FORCEINLINE UBOOL GetUseFullPrecisionUVs() const
	{
		return bUseFullPrecisionUVs;
	}
	/** 
	* @param UseFull - set to TRUE if using 32 bit floats for UVs 
	*/
	FORCEINLINE void SetUseFullPrecisionUVs(UBOOL UseFull)
	{
		bUseFullPrecisionUVs = UseFull;
	}
	/** 
	* @return TRUE if using 16 bit floats for XYZs 
	*/
	FORCEINLINE UBOOL GetUsePackedPosition() const
	{
#if CONSOLE
	#if WITH_ES2_RHI
			return GUsingES2RHI ? FALSE : bUsePackedPosition;
	#else
		return bUsePackedPosition;
	#endif
#else
	#if SUPPORTS_SCRIPTPATCH_CREATION
		//@script patcher
		// during script patch creation, we can potentially read data cooked for console
		// so we need to honor this flag to make sure serialization works
		return GIsScriptPatcherActive ? bUsePackedPosition : FALSE;
	#else
		return FALSE;
	#endif	// SUPPORTS_SCRIPTPATCH_CREATION
#endif
	}
	/** 
	* @return TRUE if saved using 16 bit floats for XYZs 
	*/
	FORCEINLINE UBOOL GetSavedPackedPosition() const
	{
		return bUsePackedPosition;
	}
	/** 
	* @param UseFull - set to TRUE if using 32 bit floats for UVs 
	*/
	FORCEINLINE void SetUsePackedPosition(UBOOL InUsePackedPosition)
	{
		bUsePackedPosition = InUsePackedPosition;
	}
	/** 
	* @return number of vertices in this vertex buffer
	*/
	FORCEINLINE UINT GetNumVertices() const
	{
		return NumVertices;
	}
	/** 
	* @return cached stride for vertex data type for this vertex buffer
	*/
	FORCEINLINE UINT GetStride() const
	{
		return Stride;
	}
	/** 
	* @return total size of data in resource array
	*/
	FORCEINLINE DWORD GetVertexDataSize() const
	{
		return NumVertices * Stride;
	}
	/** 
	* @return Mesh Origin 
	*/
	FORCEINLINE const FVector& GetMeshOrigin() const 		
	{ 
		return MeshOrigin; 
	}

	/** 
	* @return Mesh Extension
	*/
	FORCEINLINE const FVector& GetMeshExtension() const
	{ 
		return MeshExtension;
	}

	/**
	 * @return the number of texture coordinate sets in this buffer
	 */
	FORCEINLINE UINT GetNumTexCoords() const 
	{
		return NumTexCoords;
	}

	/** 
	* @param UseCPUSkinning - set to TRUE if using cpu skinning with this vertex buffer
	*/
	void SetUseCPUSkinning(UBOOL UseCPUSkinning);

	/**
	 * @param InNumTexCoords	The number of texture coordinate sets that should be in this mesh
	 */
	FORCEINLINE void SetNumTexCoords( UINT InNumTexCoords ) 
	{
		NumTexCoords = InNumTexCoords;
	}

	/**
	 * Assignment operator. 
	 */
	template <UINT NumTexCoordsT>
	FSkeletalMeshVertexBuffer& operator=(const TArray< TGPUSkinVertexFloat32Uvs32Xyz<NumTexCoordsT> >& InVertices)
	{
		check(bUseFullPrecisionUVs);
		check(bUsePackedPosition);

		AllocateData();

		*(TSkeletalMeshVertexData< TGPUSkinVertexFloat32Uvs32Xyz<NumTexCoordsT> >*)VertexData = InVertices;

		Data = VertexData->GetDataPointer();
		Stride = VertexData->GetStride();
		NumVertices = VertexData->GetNumVertices();

		return *this;
	}

	
	/**
	 * Assignment operator. 
	 */
	template <UINT NumTexCoordsT>
	FSkeletalMeshVertexBuffer& operator=(const TArray< TGPUSkinVertexFloat16Uvs<NumTexCoordsT> >& InVertices)
	{
		check(!bUseFullPrecisionUVs);
	#if CONSOLE
		check(!bUsePackedPosition);
	#endif

		AllocateData();

		*(TSkeletalMeshVertexData< TGPUSkinVertexFloat16Uvs<NumTexCoordsT> >*)VertexData = InVertices;


		Data = VertexData->GetDataPointer();
		Stride = VertexData->GetStride();
		NumVertices = VertexData->GetNumVertices();

		return *this;
	}

	/**
	 * Assignment operator.  
	 */
	template <UINT NumTexCoordsT>
	FSkeletalMeshVertexBuffer& operator=(const TArray< TGPUSkinVertexFloat32Uvs<NumTexCoordsT> >& InVertices)
	{
		check(bUseFullPrecisionUVs);
	#if CONSOLE
		check(!bUsePackedPosition);
	#endif

		AllocateData();

		*(TSkeletalMeshVertexData< TGPUSkinVertexFloat32Uvs<NumTexCoordsT> >*)VertexData = InVertices;

		Data = VertexData->GetDataPointer();
		Stride = VertexData->GetStride();
		NumVertices = VertexData->GetNumVertices();

		return *this;
	}

	/** 
	 * Assignment operator. 
	 */
	template <UINT NumTexCoordsT>
	FSkeletalMeshVertexBuffer& operator=(const TArray< TGPUSkinVertexFloat16Uvs32Xyz<NumTexCoordsT> >& InVertices)
	{
		check(!bUseFullPrecisionUVs);
		check(bUsePackedPosition);

		AllocateData();

		*(TSkeletalMeshVertexData< TGPUSkinVertexFloat16Uvs32Xyz<NumTexCoordsT> >*)VertexData = InVertices;

		Data = VertexData->GetDataPointer();
		Stride = VertexData->GetStride();
		NumVertices = VertexData->GetNumVertices();

		return *this;
	}



	/**
	* Convert the existing data in this mesh from 16 bit to 32 bit UVs.
	* Without rebuilding the mesh (loss of precision)
	*/
	template<UINT NumTexCoordsT>
	void ConvertToFullPrecisionUVs();

	/**
	* Convert the existing data in this mesh from 12 bytes to 4 bytes XYZs.
	*/
	template<UINT NumTexCoordsT>
	void ConvertToPackedPosition();

private:
	/** InfluenceBones/InfluenceWeights byte order has been swapped */
	UBOOL bInflucencesByteSwapped;
	/** Corresponds to USkeletalMesh::bUseFullPrecisionUVs. if TRUE then 32 bit UVs are used */
	UBOOL bUseFullPrecisionUVs;
	/** TRUE if this vertex buffer will be used with CPU skinning. Resource arrays are set to cpu accessible if this is TRUE */
	UBOOL bUseCPUSkinning;
	/** Corresponds to USkeletalMesh::bUsePackedPosition. if TRUE then 4 byte XYZs are used */
	UBOOL bUsePackedPosition;
	/** Position data has already been packed. Used during cooking to avoid packing twice. */
	UBOOL bProcessedPackedPositions;
	/** The vertex data storage type */
	FSkeletalMeshVertexDataInterface* VertexData;
	/** The cached vertex data pointer. */
	BYTE* Data;
	/** The cached vertex stride. */
	UINT Stride;
	/** The cached number of vertices. */
	UINT NumVertices;
	/** The number of unique texture coordinate sets in this buffer */
	UINT NumTexCoords;

	/** The origin of Mesh **/
	FVector MeshOrigin;
	/** The scale of Mesh **/
	FVector MeshExtension;

	/** 
	* Allocates the vertex data storage type. Based on UV precision needed
	*/
	void AllocateData();	

	/** 
	* Allocates the vertex data to packed position type. 
	* This is to avoid confusion from using AllocateData
	* This only happens during cooking and other than that this won't be used
	*/

	template<UINT NumTexCoordsT>
	void AllocatePackedData(const TArray< TGPUSkinVertexFloat16Uvs32Xyz<NumTexCoordsT> >& InVertices);	
	
	template<UINT NumTexCoordsT>
	void AllocatePackedData(const TArray< TGPUSkinVertexFloat32Uvs32Xyz<NumTexCoordsT> >& InVertices);	

	/** 
	* Copy the contents of the source vertex to the destination vertex in the buffer 
	*
	* @param VertexIndex - index into the vertex buffer
	* @param SrcVertex - source vertex to copy from
	*/
	void SetVertex(UINT VertexIndex,const FSoftSkinVertex& SrcVertex);
};

/** 
 * A vertex buffer for holding skeletal mesh per vertex color information only. 
 * This buffer sits along side FSkeletalMeshVertexBuffer in each skeletal mesh lod
 */
class FSkeletalMeshVertexColorBuffer : public FVertexBuffer
{
public:
	/**
	 * Constructor
	 */
	FSkeletalMeshVertexColorBuffer();

	/**
	 * Destructor
	 */
	virtual ~FSkeletalMeshVertexColorBuffer();

	/**
	 * Assignment. Assumes that vertex buffer will be rebuilt 
	 */
	FSkeletalMeshVertexColorBuffer& operator=(const FSkeletalMeshVertexColorBuffer& Other);
	
	/**
	 * Constructor (copy)
	 */
	FSkeletalMeshVertexColorBuffer(const FSkeletalMeshVertexColorBuffer& Other);

	/** 
	 * Delete existing resources 
	 */
	void CleanUp();

	/**
	 * Initializes the buffer with the given vertices.
	 * @param InVertices - The vertices to initialize the buffer with.
	 */
	void Init(const TArray<FSoftSkinVertex>& InVertices);

	/**
	 * Serializer for this class
	 * @param Ar - archive to serialize to
	 * @param B - data to serialize
	 */
	friend FArchive& operator<<(FArchive& Ar,FSkeletalMeshVertexColorBuffer& VertexBuffer);

	// FRenderResource interface.

	/**
	 * Initialize the RHI resource for this vertex buffer
	 */
	virtual void InitRHI();

	/**
	 * @return text description for the resource type
	 */
	virtual FString GetFriendlyName() const;

	/** 
	 * @return number of vertices in this vertex buffer
	 */
	FORCEINLINE UINT GetNumVertices() const
	{
		return NumVertices;
	}

	/** 
	* @return cached stride for vertex data type for this vertex buffer
	*/
	FORCEINLINE UINT GetStride() const
	{
		return Stride;
	}
	/** 
	* @return total size of data in resource array
	*/
	FORCEINLINE DWORD GetVertexDataSize() const
	{
		return NumVertices * Stride;
	}

	/**
	 * @return the vertex color for the specified index
	 */
	FORCEINLINE const FColor& VertexColor( UINT VertexIndex ) const
	{
		checkSlow( VertexIndex < GetNumVertices() );
		BYTE* VertBase = Data + VertexIndex * Stride;
		return ((FGPUSkinVertexColor*)(VertBase))->VertexColor;
	}
private:
	/** The vertex data storage type */
	FSkeletalMeshVertexDataInterface* VertexData;
	/** The cached vertex data pointer. */
	BYTE* Data;
	/** The cached vertex stride. */
	UINT Stride;
	/** The cached number of vertices. */
	UINT NumVertices;

	/** 
	 * Allocates the vertex data storage type
	 */
	void AllocateData();	

	/** 
	 * Copy the contents of the source color to the destination vertex in the buffer 
	 *
	 * @param VertexIndex - index into the vertex buffer
	 * @param SrcColor - source color to copy from
	 */
	void SetColor(UINT VertexIndex,const FColor& SrcColor);
};

/**
* Vertex influence weights
*/
struct FInfluenceWeights
{
	union 
	{ 
		struct
		{ 
			BYTE InfluenceWeights[MAX_INFLUENCES];
		};
		// for byte-swapped serialization
		DWORD InfluenceWeightsDWORD; 
	};

	/**
	* Serialize to Archive
	*/
	friend FArchive& operator<<( FArchive& Ar, FInfluenceWeights& W )
	{
		return Ar << W.InfluenceWeightsDWORD;
	}
};

/**
* Vertex influence bones
*/
struct FInfluenceBones
{
	union 
	{ 
		struct
		{ 
			BYTE InfluenceBones[MAX_INFLUENCES];
		};
		// for byte-swapped serialization
		DWORD InfluenceBonesDWORD; 
	};

	/**
	* Serialize to Archive
	*/
	friend FArchive& operator<<( FArchive& Ar, FInfluenceBones& W )
	{
		return Ar << W.InfluenceBonesDWORD;
	}

};

/**
* Vertex influence weights and bones
*/
struct FVertexInfluence
{
	FInfluenceWeights Weights;
	FInfluenceBones Bones;

	friend FArchive& operator<<( FArchive& Ar, FVertexInfluence& W )
	{
		return Ar << W.Weights << W.Bones;
	}
};

/**
 * Array of vertex influences
 */
class FSkeletalMeshVertexInfluences : public FVertexBuffer
{
public:
	TResourceArray<FVertexInfluence, VERTEXBUFFER_ALIGNMENT> Influences;

	/** Array of vertex indices to swap by bone pair */
	TMap<struct FBoneIndexPair, TArray<DWORD> > VertexInfluenceMapping;

	/** Sections to swap to when vertex weights usage is IWU_FullSwap */
	TArray<FSkelMeshSection> Sections;

	/** Chunks to swap to when vertex weights usage is IWU_FullSwap */
	TArray<FSkelMeshChunk> Chunks;

	/** Array of all bones used by this alternate weighting */
	TArray<BYTE> RequiredBones;

	/** Usage type specified at import time */
	EInstanceWeightUsage Usage;

	/** A mapping of influence sections to regular sections, only used with CustomLeftRight triangle sorting, not serialized */
	TArray<INT> CustomLeftRightSectionMap;

	FSkeletalMeshVertexInfluences() : Influences(TRUE) {}

	/**
     * Initialize the RHI resource for this vertex buffer
     */
	void InitRHI();

	friend FArchive& operator<<( FArchive& Ar, FSkeletalMeshVertexInfluences& W )
	{
		Ar << W.Influences;

		if (Ar.Ver() >= VER_ADDED_EXTRA_SKELMESH_VERTEX_INFLUENCE_MAPPING)
		{
			if( Ar.Ver() < VER_DWORD_SKELETAL_MESH_INDICES_FIXUP )
			{
				if( Ar.Ver() >= VER_DWORD_SKELETAL_MESH_INDICES )
				{
					BYTE IndexSize;
					Ar << IndexSize;
				}

				TMap<struct FBoneIndexPair, TArray<WORD> > TempMapping;
				Ar << TempMapping;

				for( TMap<struct FBoneIndexPair, TArray<WORD> >::TConstIterator It(TempMapping); It; ++It )
				{
					const TArray<WORD>& TempArray = It.Value();
					TArray<DWORD> NewArray;
					for( INT I = 0; I < TempArray.Num(); ++I )
					{
						NewArray.AddItem( TempArray(I) );
					}

					W.VertexInfluenceMapping.Set( It.Key(), NewArray );
				}
			}
			else
			{
				Ar << W.VertexInfluenceMapping;
			}
		}
		if (Ar.Ver() >= VER_ADDED_CHUNKS_SECTIONS_VERTEX_INFLUENCE)
		{
			Ar << W.Sections;
			Ar << W.Chunks;
		}
		if (Ar.Ver() >= VER_ADDED_REQUIRED_BONES_VERTEX_INFLUENCE)
		{
			Ar << W.RequiredBones;
		}
		if (Ar.Ver() < VER_ADDED_USAGE_VERTEX_INFLUENCE)
		{
			W.Usage = IWU_PartialSwap;
		}
		else
		{
			BYTE Usage;
			if (Ar.IsLoading())
			{
				Ar << Usage;
				W.Usage = (EInstanceWeightUsage)Usage;
			}
			else
			{
				Usage = (BYTE)W.Usage;
				Ar << Usage;
			}
		}				  

		return Ar;
	}
};

struct FMultiSizeIndexContainerData
{
	TArray<DWORD> Indices;
	UINT DataTypeSize;
	UINT NumVertsPerInstance;
	UBOOL bNeedsCPUAccess;
	UBOOL bSetUpForInstancing;
};

/**
 * Skeletal mesh index buffers are 16 bit by default and 32 bit when called for.
 * This class adds a level of abstraction on top of the index buffers so that we can treat them all as 32 bit.
 */
class FMultiSizeIndexContainer
{
public:
	FMultiSizeIndexContainer(UBOOL bNeedsCPUAccess = FALSE)
	: NeedsCPUAccess(bNeedsCPUAccess)
	, DataTypeSize(sizeof(WORD))
	, IndexBuffer(NULL)
	{
	}

	~FMultiSizeIndexContainer();
	
	/**
	 * Initialize the index buffer's render resources.
	 */
	void InitResources();

	/**
	 * Releases the index buffer's render resources.
	 */	
	void ReleaseResources();

	/**
	 * Creates a new index buffer
	 */
	void CreateIndexBuffer(BYTE DataTypeSize);

	/**
	 * Repopulates the index buffer
	 */
	void RebuildIndexBuffer( const FMultiSizeIndexContainerData& InData );

	/**
	 * Returns a 32 bit version of the index buffer
	 */
	void GetIndexBuffer( TArray<DWORD>& OutArray ) const;

	/**
	 * Populates the index buffer with a new set of indices
	 */
	void CopyIndexBuffer(const TArray<DWORD>& NewArray);

	UBOOL IsIndexBufferValid() const { return IndexBuffer != NULL; }

	/**
	 * Accessors
	 */
	UBOOL GetNeedsCPUAccess() const { return NeedsCPUAccess; }
	BYTE GetDataTypeSize() const { return DataTypeSize; }
	FRawStaticIndexBuffer16or32Interface* GetIndexBuffer() 
	{ 
		check( IndexBuffer != NULL );
		return IndexBuffer; 
	}
	const FRawStaticIndexBuffer16or32Interface* GetIndexBuffer() const
	{ 
		check( IndexBuffer != NULL );
		return IndexBuffer; 
	}
	
#if WITH_EDITOR
	/**
	 * Retrieves index buffer related data
	 */
	void GetIndexBufferData( FMultiSizeIndexContainerData& OutData ) const;
	
	FMultiSizeIndexContainer(const FMultiSizeIndexContainer& Other);
	FMultiSizeIndexContainer& operator=(const FMultiSizeIndexContainer& Buffer);
#endif

#if WITH_EDITORONLY_DATA
	/**
	 * Strips all data from the index buffer.
	 */
	void StripData();
#endif // #if WITH_EDITORONLY_DATA

	friend FArchive& operator<<(FArchive& Ar, FMultiSizeIndexContainer& Buffer);

private:
	/** Specifies whether, or not, the index buffer array needs CPU access */
	UBOOL NeedsCPUAccess;
	/** Size of the index buffer's index type (should be 2 or 4 bytes) */
	BYTE DataTypeSize;
	/** The vertex index buffer */
	FRawStaticIndexBuffer16or32Interface* IndexBuffer;
};

/**
* All data to define a certain LOD model for a skeletal mesh.
* All necessary data to render smooth-parts is in SkinningStream, SmoothVerts, SmoothSections and SmoothIndexbuffer.
* For rigid parts: RigidVertexStream, RigidIndexBuffer, and RigidSections.
*/
class FStaticLODModel
{
public:
	/** Sections. */
	TArray<FSkelMeshSection> Sections;

	/** The vertex chunks which make up this LOD. */
	TArray<FSkelMeshChunk> Chunks;

	/** 
	* Bone hierarchy subset active for this chunk.
	* This is a map between the bones index of this LOD (as used by the vertex structs) and the bone index in the reference skeleton of this SkeletalMesh.
	*/
	TArray<WORD> ActiveBoneIndices;  
	
	/** 
	* Bones that should be updated when rendering this LOD. This may include bones that are not required for rendering.
	* All parents for bones in this array should be present as well - that is, a complete path from the root to each bone.
	* For bone LOD code to work, this array must be in strictly increasing order, to allow easy merging of other required bones.
	*/
	TArray<BYTE> RequiredBones;

	/** 
	* Rendering data.
	*/
	FMultiSizeIndexContainer	MultiSizeIndexContainer; 
	UINT						Size;
	UINT						NumVertices;
	/** The number of unique texture coordinate sets in this lod */
	UINT						NumTexCoords;

	/** Resources needed to render the model using PN-AEN */
	FMultiSizeIndexContainer	AdjacencyMultiSizeIndexContainer;

	/** static vertices from chunks for skinning on GPU */
	FSkeletalMeshVertexBuffer	VertexBufferGPUSkin;
	
	/** A buffer for vertex colors */
	FSkeletalMeshVertexColorBuffer	ColorVertexBuffer;

	/** Optional array of weight/bone influences that can be used by this mesh. Defaults are in VertexBufferGPUSkin */
	TArray<FSkeletalMeshVertexInfluences> VertexInfluences;
	
	/** Editor only data: array of the original point (wedge) indices for each of the vertices in a FStaticLODModel */
	FIntBulkData				RawPointIndices;
	FWordBulkData				LegacyRawPointIndices;

	/**
	* Initialize the LOD's render resources.
	*
	* @param Parent Parent mesh
	*/
	void InitResources(class USkeletalMesh* Parent);

	/**
	* Releases the LOD's render resources.
	*/
	void ReleaseResources();

	/** Constructor (default) */
	FStaticLODModel()
	:	MultiSizeIndexContainer(TRUE)	// needs to be CPU accessible for CPU-skinned decals.
	,	Size(0)
	,	NumVertices(0)
	,	AdjacencyMultiSizeIndexContainer(TRUE)
	{}

	/**
	 * Special serialize function passing the owning UObject along as required by FUnytpedBulkData
	 * serialization.
	 *
	 * @param	Ar		Archive to serialize with
	 * @param	Owner	UObject this structure is serialized within
	 * @param	Idx		Index of current array entry being serialized
	 */
	void Serialize( FArchive& Ar, UObject* Owner, INT Idx );

	/**
	* Fill array with vertex position and tangent data from skel mesh chunks.
	*
	* @param Vertices Array to fill.
	*/
	void GetVertices(TArray<FSoftSkinVertex>& Vertices) const;

	/**
	* Initialize postion and tangent vertex buffers from skel mesh chunks
	*
	* @param Mesh Parent mesh
	*/
	void BuildVertexBuffers(const class USkeletalMesh* Mesh, UBOOL bUsePackedPosition);

	/** Utility function for returning total number of faces in this LOD. */
	INT GetTotalFaces();

	/** Utility for finding the chunk that a particular vertex is in. */
	void GetChunkAndSkinType(INT InVertIndex, INT& OutChunkIndex, INT& OutVertIndex, UBOOL& bOutSoftVert) const;

	/** Sort the triangles with the specified sorting method */
	void SortTriangles( USkeletalMesh* SkelMesh, INT SectionIndex, ETriangleSortOption NewTriangleSorting );

	/** Ensures triangle sorting modes are set up properly for alternate vertex blend weight sections */
	void UpdateTriangleSortingForAltVertexInfluences();
};

/**
 * FSkeletalMeshSourceData - Source triangles and render data, editor-only.
 */
class FSkeletalMeshSourceData
{
public:
	FSkeletalMeshSourceData();
	~FSkeletalMeshSourceData();

#if WITH_EDITOR
	/** Initialize from static mesh render data. */
	void Init( const class USkeletalMesh* SkeletalMesh, FStaticLODModel& LODModel );

	/** Retrieve render data. */
	FORCEINLINE FStaticLODModel* GetModel() { return LODModel; }
#endif // #if WITH_EDITOR

#if WITH_EDITORONLY_DATA
	/** Free source data. */
	void Clear();
#endif // WITH_EDITORONLY_DATA

	/** Returns TRUE if the source data has been initialized. */
	FORCEINLINE UBOOL IsInitialized() const { return LODModel != NULL; }

	/** Serialization. */
	void Serialize( FArchive& Ar, USkeletalMesh* SkeletalMesh );

private:
	FStaticLODModel* LODModel;
};

enum ESkeletalMeshOptimizationImportance
{
	SMOI_Off		=0,
	SMOI_Lowest		=1,
	SMOI_Low		=2,
	SMOI_Normal		=3,
	SMOI_High		=4,
	SMOI_Highest	=5,
	SMOI_Max
};

/** Enum specifying the reduction type to use when simplifying skeletal meshes. */
enum SkeletalMeshOptimizationType
{
	SMOT_NumOfTriangles = 0,
	SMOT_MaxDeviation   = 1,
	SMOT_MAX,
};

enum ESkeletalMeshOptimizationNormalMode
{
	SMONM_RecalculateNormals		=0,
	SMONM_RecalculateNormalsSmooth	=1,
	SMONM_RecalculateNormalsHard	=2,
	SMONM_Max
};

/**
 * FSkeletalMeshOptimizationSettings - The settings used to optimize a skeletal mesh LOD.
 */
struct FSkeletalMeshOptimizationSettings
{
	/** Maximum deviation from the base mesh as a percentage of the bounding sphere. */
	FLOAT MaxDeviationPercentage;
	/** How important the shape of the geometry is (ESkeletalMeshOptimizationImportance). */
	BYTE SilhouetteImportance;
	/** How important texture density is (ESkeletalMeshOptimizationImportance). */
	BYTE TextureImportance;
	/** How important shading quality is. */
	BYTE ShadingImportance;
	/** How important skinning quality is (ESkeletalMeshOptimizationImportance). */
	BYTE SkinningImportance;
	/** How to compute normals for the optimized mesh (ESkeletalMeshOptimizationNormalMode). */
	BYTE NormalMode_DEPRECATED;
	/** The ratio of bones that will be removed from the mesh */
	FLOAT BoneReductionRatio;
	/** Maximum number of bones that can be assigned to each vertex. */
	INT MaxBonesPerVertex;
	/** The method to use when optimizing the skeletal mesh LOD */
	BYTE ReductionMethod;
	/** If ReductionMethod equals SMOT_NumOfTriangles this value is the ratio of triangles [0-1] to remove from the mesh */
	FLOAT NumOfTrianglesPercentage;
	/** The welding threshold distance. Vertices under this distance will be welded. */
	FLOAT WeldingThreshold; 
	/** Whether Normal smoothing groups should be preserved. If false then NormalsThreshold is used **/
	UBOOL bRecalcNormals;
	/** If the angle between two triangles are above this value, the normals will not be
	smooth over the edge between those two triangles. Set in degrees. This is only used when PreserveNormals is set to false*/
	FLOAT NormalsThreshold;

	FSkeletalMeshOptimizationSettings()
		: ReductionMethod(SMOT_MaxDeviation)
		, MaxDeviationPercentage( 0.0f )
		, NumOfTrianglesPercentage(1.0f)
		, WeldingThreshold(0.1f)
		, bRecalcNormals(TRUE)
		, NormalsThreshold(60.0f)
		, SilhouetteImportance( SMOI_Normal )
		, TextureImportance( SMOI_Normal )
		, ShadingImportance( SMOI_Normal )
		, SkinningImportance( SMOI_Normal )
		, NormalMode_DEPRECATED( SMONM_RecalculateNormals )
		, BoneReductionRatio(100.0f)
		, MaxBonesPerVertex(4)
	{
	}
};

/**
 *	Contains the vertices that are most dominated by that bone. Vertices are in Bone space.
 *	Not used at runtime, but useful for fitting physics assets etc.
 */
struct FBoneVertInfo
{
	// Invariant: Arrays should be same length!
	TArray<FVector>	Positions;
	TArray<FVector>	Normals;
};

/** Struct containing triangle sort settings for a particular section */
struct FTriangleSortSettings
{
	BYTE TriangleSorting;
	BYTE CustomLeftRightAxis;
	FName CustomLeftRightBoneName;
};

/** Struct containing information for a particular LOD level, such as materials and info for when to use it. */
struct FSkeletalMeshLODInfo
{
	/**	Indicates when to use this LOD. A smaller number means use this LOD when further away. */
	FLOAT								DisplayFactor;

	/**	Used to avoid 'flickering' when on LOD boundary. Only taken into account when moving from complex->simple. */
	FLOAT								LODHysteresis;

	/** Mapping table from this LOD's materials to the SkeletalMesh materials array. */
	TArray<INT>							LODMaterialMap;

	/** Per-section control over whether to enable shadow casting. */
	TArray<UBOOL>						bEnableShadowCasting;

	/** Per-section sorting options */
	TArray<BYTE>						OLD_TriangleSorting;	// deprecated
	TArray<FTriangleSortSettings>		TriangleSortSettings;

	/** If true, use 16 bit XYZs to save memory. If false, use 32 bit XYZs */
	BITFIELD							bDisableCompression:1;

	/** Whether to disable morph targets for this LOD. */
	BITFIELD							bHasBeenSimplified:1;
};

struct FBoneMirrorInfo
{
	/** The bone to mirror. */
	INT		SourceIndex;
	/** Axis the bone is mirrored across. */
	BYTE	BoneFlipAxis;
};
template <> struct TIsPODType<FBoneMirrorInfo> { enum { Value = true }; };

struct FBoneMirrorExport
{
	FName	BoneName;
	FName	SourceBoneName;
	BYTE	BoneFlipAxis;
};

/** Used to specify special properties for cloth vertices */
enum ClothBoneType
{
	/** The cloth Vertex is attached to the physics asset if available */
	CLOTHBONE_Fixed					= 0,

	/** The cloth Vertex is attached to the physics asset if available and made breakable */
	CLOTHBONE_BreakableAttachment	= 1,

	/** The Cloth Vertex is marked as a tearable vert */
	CLOTHBONE_TearLine				= 2,
};

struct FClothSpecialBoneInfo
{
	/** The bone name to attach to a cloth vertex */
	FName BoneName;

	/** The type of attachment */
	BYTE BoneType;

	/** Array used to cache cloth indices which will be attached to this bone, created in BuildClothMapping(),
	 * Note: These are welded indices.
	 */
	TArray<INT> AttachedVertexIndices;
};


struct FSoftBodyTetraLink
{
	INT TetIndex;
	FVector Bary;
};

struct FSoftBodyTetraLinkArray
{
	FSoftBodyTetraLinkArray(TArray<FSoftBodyTetraLink>& TL) : TetraLinks(TL)
	{
	}

	TArray<FSoftBodyTetraLink> TetraLinks;
};

/** Used to specify special properties for cloth vertices */
enum SoftBodyBoneType
{
	/** The softbody Vertex is attached to the physics asset if available */
	SOFTBODYBONE_Fixed					= 0,
	SOFTBODYBONE_BreakableAttachment	= 1,
	SOFTBODYBONE_TwoWayAttachment		= 2,
};

/**
* Used in editor only when showing tangents/normals/bone weights of information
*/
enum SkinColorRenderMode
{
	ESCRM_None,
	ESCRM_VertexTangent, 
	ESCRM_VertexNormal,
	ESCRM_VertexMirror,
	ESCRM_BoneWeights, 
	ESCRM_Max,
};

struct FSoftBodySpecialBoneInfo
{
	/** The bone name to attach to a softbody vertex */
	FName BoneName;

	/** The type of attachment */
	BYTE BoneType;

	/** Array used to cache softbody indices which will be attached to this bone, created in BuildSoftBodyMapping(),
	 * Note: These are welded indices.
	 */
	TArray<INT> AttachedVertexIndices;
};

/**
 * Used to store temporary data when building a soft-body mesh instead of overwriting existing data.
 */
struct FSoftBodyMeshInfo
{
public:

	/** Mapping between each vertex of the simulated soft-body's surface-mesh and the graphics mesh. */ 
	TArray<INT>					SurfaceToGraphicsVertMap; 

	/** Index buffer of the triangles of the soft-body's surface mesh. */
	TArray<INT>					SurfaceIndices; 

	/** Index buffer of the tetrahedral of the soft-body's tetra-mesh. */	
	TArray<INT>					TetraIndices; 

	/** Mapping between each vertex of the surface-mesh and its tetrahedron, with local positions given in barycentric coordinates. */
	TArray<FSoftBodyTetraLink>	TetraLinks; 

	/** Base array of tetrahedron vertex positions, used to generate the scaled versions from. */
	TArray<FVector>				TetraVertsUnscaled; 

	/** Array used to cache soft-body welded indices which will be attached to a bone. */
	TArray<TArray<INT> >		SpecialBoneAttachedVertexIndicies;

	/**
	 * Determines if the soft-body info contains valid data.
	 *
	 * To be valid, the mesh info must have entries for:
	 *		- SurfaceToGraphicsVertMap
	 *		- SurfaceIndices
	 *		- TetraIndices
	 *		- TetraLinks
	 *		- TetraVertsUnscaled
	 *
	 * Optional entries are:
	 *		- SpecialBoneAttachedVertexIndicies
	 *
	 * @return	TRUE if all the data contained in this structure is valid.
	 */
	UBOOL IsValid() const
	{
		return	(	(SurfaceToGraphicsVertMap.Num() > 0)
				&&	(SurfaceIndices.Num() > 0)
				&&	(TetraIndices.Num() > 0)
				&&	(TetraLinks.Num() > 0)
				&&	(TetraVertsUnscaled.Num() > 0)		);
	}
};
struct FApexClothingLodInfo
{
	FApexClothingLodInfo(INT NumSection)
	{
		ClothingSectionInfo.Empty(NumSection);
	}
	/** The mesh section that the clothing submesh will override. */
	TArray<INT>	ClothingSectionInfo;
};

struct FApexClothingAssetInfo
{
	FApexClothingAssetInfo(INT NumLOD, FName NameOfAsset)
	{
		ClothingLODInfo.Empty(NumLOD);
		ClothingAssetName = NameOfAsset;
	}
	/** Graphical Lod Info for the clothing asset */
	TArray<FApexClothingLodInfo>	ClothingLODInfo;
	/** Clothing Asset Name */
	FName							ClothingAssetName;		
};
/**
* Skeletal mesh.
*/
class USkeletalMesh : public UObject
{
	DECLARE_CLASS_NOEXPORT(USkeletalMesh, UObject, CLASS_SafeReplace | 0, Engine)

	FBoxSphereBounds				Bounds;
	/** List of materials applied to this mesh. */
	TArray<UMaterialInterface*>		Materials;
	/** List of clothing assets associated with each corresponding material */	
	TArray<class UApexClothingAsset *>	ClothingAssets;
	/** List of Clothing LOD and the mapping of clothing to skeletal mesh section */
	TArray<FApexClothingAssetInfo>		ClothingLodMap;
	/** Origin in original coordinate system */
	FVector 						Origin;				
	/** Amount to rotate when importing (mostly for yawing) */
	FRotator						RotOrigin;			
	/** Reference skeleton */
	TArray<FMeshBone>				RefSkeleton;
	/** The max hierarchy depth. */
	INT								SkeletalDepth;

	/** Map from bone name to bone index. Used to accelerate MatchRefBone. */
	TMap<FName,INT>					NameIndexMap;

	/** Static LOD models */
	TIndirectArray<FStaticLODModel>	LODModels;
	/** Source data. */
	FSkeletalMeshSourceData			SourceData;
	/** Reference skeleton precomputed bases. */
	TArray<FBoneAtom>					RefBasesInvMatrix;	// @todo: wasteful ?! 
	/** List of bones that should be mirrored. */
	TArray<FBoneMirrorInfo>			SkelMirrorTable;
	BYTE							SkelMirrorAxis;
	BYTE							SkelMirrorFlipAxis;
	
	/** 
	 *	Array of named socket locations, set up in editor and used as a shortcut instead of specifying 
	 *	everything explicitly to AttachComponent in the SkeletalMeshComponent. 
	 */
	TArray<USkeletalMeshSocket*>	Sockets;

	/**
	 *   Array of bone names that are breakable, used to auto create bone break vertex weight tracks on mesh import
	 */
	TArray<FString>					BoneBreakNames;

	/** Array of options that break bones for use in game/editor */
	/** Match with BoneBreakNames array **/
	TArray<BYTE>					BoneBreakOptions;

	/** Array of information for each LOD level. */	
	TArray<FSkeletalMeshLODInfo>	LODInfo;

	/** Optimization settings used to simplify LODs of this mesh. */
	TArray<FSkeletalMeshOptimizationSettings> OptimizationSettings;

	/** For each bone specified here, all triangles rigidly weighted to that bone are entered into a kDOP, allowing per-poly collision checks. */
	TArray<FName>					PerPolyCollisionBones;
	
	/** For each of these bones, find the parent that is in PerPolyCollisionBones and add its polys to that bone. */
	TArray<FName>					AddToParentPerPolyCollisionBone;

	/** KDOP tree's used for storing rigid triangle information for a subset of bones. */
	TArray<struct FPerPolyBoneCollisionData> PerPolyBoneKDOPs;

	/** If true, include triangles that are soft weighted to bones. */
	BITFIELD						bPerPolyUseSoftWeighting:1;

	/** If true, use PhysicsAsset for line collision checks. If false, use per-poly bone collision (if present). */
	BITFIELD						bUseSimpleLineCollision:1;

	/** If true, use PhysicsAsset for extent (swept box) collision checks. If false, use per-poly bone collision (if present). */
	BITFIELD						bUseSimpleBoxCollision:1;

	/** All meshes default to GPU skinning. Set to True to enable CPU skinning */
	BITFIELD						bForceCPUSkinning:1;

	/** If true, use 32 bit UVs. If false, use 16 bit UVs to save memory */
	BITFIELD						bUseFullPrecisionUVs:1;

	/** TRUE if this mesh has ever been simplified with Simplygon. */
	BITFIELD						bHasBeenSimplified:1;

	/** FaceFX animation asset */
	UFaceFXAsset*					FaceFXAsset;

#if WITH_EDITORONLY_DATA
	/** Asset used for previewing bounds in AnimSetViewer. Makes setting up LOD distance factors more reliable. */
	UPhysicsAsset*					BoundsPreviewAsset;

	/** Asset used for previewing morph target animations in AnimSetViewer. Only for editor. */
	TArray<UMorphTargetSet*>		PreviewMorphSets;
#endif // WITH_EDITORONLY_DATA

	/** LOD bias to use for PC.						*/
	INT								LODBiasPC;
	/** LOD bias to use for PS3.					*/
	INT								LODBiasPS3;
	/** LOD bias to use for Xbox 360.				*/
	INT								LODBiasXbox360;

#if WITH_EDITORONLY_DATA
	/** Path to the resource used to construct this skeletal mesh */
	FStringNoInit					SourceFilePath;

	/** Date/Time-stamp of the file from the last import */
	FStringNoInit					SourceFileTimestamp;
#endif // WITH_EDITORONLY_DATA

	// CLOTH
	// Under Development! Not a fully supported feature at the moment.

	/** Cache of ClothMesh objects at different scales. */
	TArray<FPointer>				ClothMesh;

	/** Scale of each of the ClothMesh objects in cache. This array is same size as ClothMesh. */
	TArray<FLOAT>					ClothMeshScale;

	/** 
	 *	Mapping between each vertex in the simulation mesh and the graphics mesh. 
	 *	This is ordered so that 'free' vertices are first, and then after NumFreeClothVerts they are 'fixed' to the skinned mesh.
	 */
	TArray<INT>						ClothToGraphicsVertMap;

	/** Scaling (per vertex) for how far cloth vert can move from its animated position  */
	TArray<FLOAT>					ClothMovementScale;

	/** Method to use to generate the ClothMovementScale table */
	BYTE							ClothMovementScaleGenMode;

	/** How far a simulated vertex can move from its animated location */
	FLOAT							ClothToAnimMeshMaxDist;

	/** If TRUE, simulated verts are limited to a certain distance from */
	BITFIELD						bLimitClothToAnimMesh:1;

	/**
	 * Mapping from index of rendered mesh to index of simulated mesh.
	 * This mapping applies before ClothToGraphicsVertMap which can then operate normally
	 * The reason for this mapping is to weld several vertices with the same position but different texture coordinates into one
	 * simulated vertex which makes it possible to run closed meshes for cloth.
	 */
	TArray<INT>						ClothWeldingMap;

	/**
	 * This is the highest value stored in ClothWeldingMap
	 */
	INT								ClothWeldingDomain;

	/**
	 * This will hold the indices to the reduced number of cloth vertices used for cooking the NxClothMesh.
	 */
	TArray<INT>						ClothWeldedIndices;

	/**
	 * This will hold the indices to the reduced number of cloth vertices used for cooking the NxClothMesh.
	 */
	BITFIELD						bForceNoWelding:1;

	/** Point in the simulation cloth vertex array where the free verts finish and we start having 'fixed' verts. */
	INT								NumFreeClothVerts;

	/** Index buffer for simulation cloth. */
	TArray<INT>						ClothIndexBuffer;

	/** Vertices with any weight to these bones are considered 'cloth'. */
	TArray<FName>					ClothBones;

	/** If greater than 1, will generate smaller meshes internally, used to improve simulation time and reduce stretching. */
	INT								ClothHierarchyLevels;

	/** Enable constraints that attempt to minimize curvature or folding of the cloth. */
	BITFIELD 						bEnableClothBendConstraints:1;

	/** Enable damping forces on the cloth. */
	BITFIELD 						bEnableClothDamping:1;

	/** Enable center of mass damping of cloth internal velocities.  */
	BITFIELD						bUseClothCOMDamping:1;

	/** Controls strength of springs that attempts to keep particles in the cloth together. */
	FLOAT 							ClothStretchStiffness;

	/** 
	 *	Controls strength of springs that stop the cloth from bending. 
	 *	bEnableClothBendConstraints must be true to take affect. 
	 */
	FLOAT 							ClothBendStiffness;

	/** 
	 *	This is multiplied by the size of triangles sharing a point to calculate the points mass.
	 *	This cannot be modified after the cloth has been created.
	 */
	FLOAT 							ClothDensity;

	/** How thick the cloth is considered when doing collision detection. */
	FLOAT 							ClothThickness;

	/** 
	 *	Controls how much damping force is applied to cloth particles.
	 *	bEnableClothDamping must be true to take affect.
	 */
	FLOAT 							ClothDamping;

	/** Increasing the number of solver iterations improves how accurately the cloth is simulated, but will also slow down simulation. */
	INT 							ClothIterations;

	/** If ClothHierarchyLevels is more than 0, this number controls the number of iterations of the hierarchical solver. */
	INT								ClothHierarchicalIterations;

	/** Controls movement of cloth when in contact with other bodies. */
	FLOAT 							ClothFriction;

	/** 
	 * Controls the size of the grid cells a cloth is divided into when performing broadphase collision. 
	 * The cell size is relative to the AABB of the cloth.
	 */
	FLOAT							ClothRelativeGridSpacing;

	/** Adjusts the internal "air" pressure of the cloth. Only has affect when bEnableClothPressure. */
	FLOAT							ClothPressure;

	/** Response coefficient for cloth/rb collision */
	FLOAT							ClothCollisionResponseCoefficient;

	/** How much an attachment to a rigid body influences the cloth */
	FLOAT							ClothAttachmentResponseCoefficient;

	/** How much extension an attachment can undergo before it tears/breaks */
	FLOAT							ClothAttachmentTearFactor;

	/**
	 * Maximum linear velocity at which cloth can go to sleep.
	 * If negative, the global default will be used.
	 */
	FLOAT							ClothSleepLinearVelocity;

	/** If bHardStretchLimit is TRUE, how much stretch is allowed in the cloth. 1.0 is no stretch (but will cause jitter) */
	FLOAT							HardStretchLimitFactor;

	/** 
	 *	If TRUE, limit the total amount of stretch that is allowed in the cloth, based on HardStretchLimitFactor. 
	 *	Note that bLimitClothToAnimMesh must be TRUE on the SkeletalMeshComponent for this to work.
	 */
	BITFIELD						bHardStretchLimit:1;

	/** Enable orthogonal bending resistance to minimize curvature or folding of the cloth. 
	 *  This technique uses angular springs instead of distance springs as used in 
	 *  'bEnableClothBendConstraints'. This mode is slower but independent of stretching resistance.
	 */
	BITFIELD						bEnableClothOrthoBendConstraints : 1;

	/** Enables cloth self collision. */
	BITFIELD						bEnableClothSelfCollision : 1;

	/** Enables pressure support. Simulates inflated objects like balloons. */
	BITFIELD						bEnableClothPressure : 1;

	/** Enables two way collision with rigid-bodies. */
	BITFIELD						bEnableClothTwoWayCollision : 1;

	/** 
	 * Vertices with any weight to these bones are considered cloth with special behavoir, currently
	 * they are attached to the physics asset with fixed or breakable attachments or tearlines.
	 */
	TArray<FClothSpecialBoneInfo>	ClothSpecialBones; //ClothBones could probably be eliminated, but that requires and interface change

/** 
 * Enable cloth line/extent/point checks. 
 * Note: line checks are performed with a raycast against the cloth, but point and swept extent checks are performed against the cloth AABB 
 */
	BITFIELD						bEnableClothLineChecks : 1;

	/**
	 *  Whether cloth simulation should be wrapped inside a Rigid Body and only be used upon impact
	 */
	BITFIELD						bClothMetal : 1;

	/** Threshold for when deformation is allowed */
	FLOAT							ClothMetalImpulseThreshold;
	/** Amount by which colliding objects are brought closer to the cloth */
	FLOAT							ClothMetalPenetrationDepth;
	/** Maximum deviation of cloth particles from initial position */
	FLOAT							ClothMetalMaxDeformationDistance;

/** Used to enable cloth tearing. Note, extra vertices/indices must be reserved using ClothTearReserve */
	BITFIELD						bEnableClothTearing : 1;

/** Stretch factor beyond which a cloth edge/vertex will tear. Should be greater than 1. */
	FLOAT							ClothTearFactor;

/** Number of vertices/indices to set aside to accomodate new triangles created as a result of tearing */
	INT								ClothTearReserve;

	/** Any cloth vertex that exceeds its valid bounds will be deleted if bEnableValidBounds is set. */
	BITFIELD						bEnableValidBounds	:	1;
	/** The minimum coordinates triplet of the cloth valid bound */
	FVector							ValidBoundsMin;

	/** The maximum coordinates triplet of the cloth valid bound */
	FVector							ValidBoundsMax;

/** Map which maps from a set of 3 triangle indices packet in a 64bit to the location in the index buffer,
 *  Used to update indices for torn triangles. Generated in InitClothSim().
 */
	TMap<QWORD,INT>					ClothTornTriMap;

	/** Mapping between each vertex of the simulated soft-body's surface-mesh and the graphics mesh. */ 	
	TArray<INT>								SoftBodySurfaceToGraphicsVertMap;

	/** Index buffer of the triangles of the soft-body's surface mesh. */
	TArray<INT>								SoftBodySurfaceIndices;

	/** Base array of tetrahedron vertex positions, used to generate the scaled versions from. */
	TArray<FVector>							SoftBodyTetraVertsUnscaled;

	/** Index buffer of the tetrahedra of the soft-body's tetra-mesh. */	
	TArray<INT>								SoftBodyTetraIndices;

	/** Mapping between each vertex of the surface-mesh and its tetrahedron, with local positions given in barycentric coordinates. */
	TArray<FSoftBodyTetraLink>				SoftBodyTetraLinks;

	/** Cache of pointers to NxSoftBodyMesh objects at different scales. */
	TArray<FPointer>						CachedSoftBodyMeshes;

	/** Scale of each of the NxSoftBodyMesh objects in cache. This array is same size as CachedSoftBodyMeshes. */
	TArray<FLOAT>							CachedSoftBodyMeshScales;

	/** Vertices with any weight to these bones are considered 'soft-body'. */
	TArray<FName>							SoftBodyBones;

	/** 
	 * Vertices with any weight to these bones are considered softbody with special behavoir, currently
	 * they are attached to the physics asset with fixed attachments.
	 */
	TArray<FSoftBodySpecialBoneInfo>		SoftBodySpecialBones; //SoftBodyBones could probably be eliminated, but that requires and interface change

	FLOAT							SoftBodyVolumeStiffness;
	FLOAT							SoftBodyStretchingStiffness;
	FLOAT							SoftBodyDensity;
	FLOAT							SoftBodyParticleRadius;
	FLOAT							SoftBodyDamping;
	INT								SoftBodySolverIterations;
	FLOAT							SoftBodyFriction;
	FLOAT							SoftBodyRelativeGridSpacing;
	FLOAT							SoftBodySleepLinearVelocity;
	BITFIELD						bEnableSoftBodySelfCollision : 1;
	FLOAT							SoftBodyAttachmentResponse;
	FLOAT							SoftBodyCollisionResponse;
	FLOAT							SoftBodyDetailLevel;
	INT								SoftBodySubdivisionLevel;
	BITFIELD						bSoftBodyIsoSurface : 1;
	BITFIELD						bEnableSoftBodyDamping : 1;
	BITFIELD						bUseSoftBodyCOMDamping : 1;
	FLOAT							SoftBodyAttachmentThreshold;
	BITFIELD						bEnableSoftBodyTwoWayCollision : 1;
	FLOAT							SoftBodyAttachmentTearFactor;

	/** Enable soft body line checks. */
	BITFIELD						bEnableSoftBodyLineChecks : 1;
	/** If TRUE, this skeletal mesh has vertex colors and we should set up the vertex buffer accordingly. */
	BITFIELD						bHasVertexColors : 1;

	/** Array to mark a graphics vertex is cloth */
	TArray<UBOOL>					GraphicsIndexIsCloth;

	/** The cached streaming texture factors.  If the array doesn't have MAX_TEXCOORDS entries in it, the cache is outdated. */
	TArray<FLOAT>					CachedStreamingTextureFactors;

	/**
	 * Allows artists to adjust the distance where textures using UV 0 are streamed in/out.
	 * 1.0 is the default, whereas a higher value increases the streamed-in resolution.
	 */
	FLOAT							StreamingDistanceMultiplier;

	/** A fence which is used to keep track of the rendering thread releasing the static mesh resources. */
	FRenderCommandFence				ReleaseResourcesFence;

	/** Runtime UID for this SkeletalMeshm, used when linking meshes to AnimSets. */
	QWORD							SkelMeshRUID;

	/** When enabled the material in the APEX clothing asset will override the skeletal mesh material. */
	BITFIELD						bUseClothingAssetMaterial : 1;

	/**
	* Initialize the mesh's render resources.
	*/
	void InitResources();

	/**
	* Releases the mesh's render resources.
	*/
	void ReleaseResources();

	/**
	 * Returns the scale dependent texture factor used by the texture streaming code.	
	 *
	 * @param RequestedUVIndex UVIndex to look at
	 * @return scale dependent texture factor
	 */
	FLOAT GetStreamingTextureFactor( INT RequestedUVIndex );

	// Object interface.
	virtual void PreEditChange(UProperty* PropertyAboutToChange);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void BeginDestroy();
	virtual UBOOL IsReadyForFinishDestroy();
	void Serialize( FArchive& Ar );
	virtual void PostLoad();
	virtual void PreSave();
	virtual void FinishDestroy();
	virtual void PostDuplicate();

	/**
	 * Used by various commandlets to purge editor only and platform-specific data from various objects
	 * 
	 * @param PlatformsToKeep Platforms for which to keep platform-specific data
	 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
	 */
	virtual void StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData);
	
	/** Re-generate the per-poly collision data in the PerPolyBoneKDOPs array, based on names in the PerPolyCollisionBones array. */
	void UpdatePerPolyKDOPs();

	/** 
	 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
	 */
	virtual FString GetDesc();

	/** 
	 * Returns detailed info to populate listview columns
	 */
	virtual FString GetDetailedDescription( INT InIndex );


	/**
	 * This will return detail info about this specific object. (e.g. AudioComponent will return the name of the cue,
	 * ParticleSystemComponent will return the name of the ParticleSystem)  The idea here is that in many places
	 * you have a component of interest but what you really want is some characteristic that you can use to track
	 * down where it came from.  
	 *
	*/
	virtual FString GetDetailedInfoInternal() const;

	/** Setup-only routines - not concerned with the instance. */
	
	/**
	 * Create all render specific (but serializable) data like e.g. the 'compiled' rendering stream,
	 * mesh sections and index buffer.
	 *
	 * @todo: currently only handles LOD level 0.
	 */
	UBOOL CreateSkinningStreams( 
		const TArray<FVertInfluence>& Influences, 
		const TArray<FMeshWedge>& Wedges, 
		const TArray<FMeshFace>& Faces, 
		const TArray<FVector>& Points,
		const FSkelMeshExtraInfluenceImportData* ExtraInfluenceData=NULL
		);

	/** Calculate the required bones for this LOD, including possible extra influences */
	void CalculateRequiredBones(INT LODIdx);

	void CalculateInvRefMatrices();
	void CalcBoneVertInfos( TArray<FBoneVertInfo>& Infos, UBOOL bOnlyDominant );

	/** Clear and create the NameIndexMap of bone name to bone index. */
	void InitNameIndexMap();

	UBOOL	IsCPUSkinned() const;
	INT		MatchRefBone( FName StartBoneName) const;
	UBOOL	BoneIsChildOf( INT ChildBoneIndex, INT ParentBoneIndex ) const;
	class USkeletalMeshSocket* FindSocket(FName InSocketName);

	FMatrix	GetRefPoseMatrix( INT BoneIndex ) const;

	/** Allocate and initialise bone mirroring table for this skeletal mesh. Default is source = destination for each bone. */
	void InitBoneMirrorInfo();

	/** Utility for copying and converting a mirroring table from another SkeletalMesh. */
	void CopyMirrorTableFrom(USkeletalMesh* SrcMesh);
	void ExportMirrorTable(TArray<FBoneMirrorExport> &MirrorExportInfo);
	void ImportMirrorTable(TArray<FBoneMirrorExport> &MirrorExportInfo);

	/** 
	 *	Utility for checking that the bone mirroring table of this mesh is good.
	 *	Return TRUE if mirror table is OK, false if there are problems.
	 *	@param	ProblemBones	Output string containing information on bones that are currently bad.
	 */
	UBOOL MirrorTableIsGood(FString& ProblemBones);

#if WITH_EDITOR
	/**
	 * Retrieves the source model for this skeletal mesh.
	 */
	FStaticLODModel& GetSourceModel();

	/**
	 * Copies off the source model for this skeletal mesh if necessary and returns it. This function should always be called before
	 * making destructive changes to the mesh's geometry, e.g. simplification.
	 */
	FStaticLODModel& PreModifyMesh();
#endif // #if WITH_EDITOR

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	INT GetResourceSize();

	/** Uses the ClothBones array to analyze the graphics mesh and generate informaton needed to construct simulation mesh (ClothToGraphicsVertMap etc). */
	void BuildClothMapping();

	void BuildClothTornTriMap();

	/** Using whatever method is specified with ClothMovementScaleGenMode, regen the ClothMovementScale table */
	void GenerateClothMovementScale();

	/** Util to fill in the ClothMovementScale automatically based on how far a vertex is from a fixed one  */
	void GenerateClothMovementScaleFromDistToFixed();

	/** Util to fill in the ClothMovementScale automatically based on how verts are weighted to cloth bones  */
	void GenerateClothMovementScaleFromBoneWeight();

	/** Determines if this mesh is only cloth. */
	UBOOL IsOnlyClothMesh() const;

	/** Reset the store of cooked cloth meshes. Need to make sure you are not actually using any when you call this. */
	void ClearClothMeshCache();

#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	/** Get the cooked NxClothMesh for this mesh at the given scale. */
	class NxClothMesh* GetClothMeshForScale(FLOAT InScale);

	/** Pull the cloth mesh positions from the SkeletalMesh skinning data. */
	UBOOL ComputeClothSectionVertices(TArray<FVector>& ClothSectionVerts, FLOAT InScale, UBOOL ForceNoWelding=FALSE);
#endif

	/** Clears internal soft-body buffers. */
	void ClearSoftBodyMeshCache();

	/** 
	* Replaces the given data with current data to recreate the soft body representation for the mesh
	*/
	void RecreateSoftBody(FSoftBodyMeshInfo& MeshInfo);

#if WITH_NOVODEX && !NX_DISABLE_SOFTBODY

	class NxSoftBodyMesh* GetSoftBodyMeshForScale(FLOAT InScale);

#endif //WITH_NOVODEX && !NX_DISABLE_SOFTBODY

	/**
	* Verify SkeletalMeshLOD is set up correctly	
	*/
	void DebugVerifySkeletalMeshLOD();

	/**
	 * Returns TRUE if the mesh has optimizations stored for the specified LOD.
	 * @param LODIndex - LOD index for which to look for optimization settings.
	 * @returns TRUE if the mesh has optimizations stored for the specified LOD.
	 */
	UBOOL HasOptimizationSettings( INT LODIndex ) const
	{
		return LODIndex >= 0 && LODIndex < OptimizationSettings.Num();
	}

	/**
	 * Retrieves the settings with which the LOD was optimized.
	 * @param LODIndex - LOD index for which to look up the optimization settings.
	 * @returns the optimization settings for the specified LOD.
	 */
	const FSkeletalMeshOptimizationSettings& GetOptimizationSettings( INT LODIndex ) const
	{
		check( LODIndex >= 0 && LODIndex < OptimizationSettings.Num() );
		return OptimizationSettings( LODIndex );
	}

	/**
	 * Stores the settings with which the LOD was optimized.
	 * @param LODIndex - LOD index for which to store optimization settings.
	 * @param Settings - Optimization settings for the specified LOD index.
	 */
	void SetOptimizationSettings( INT LODIndex, const FSkeletalMeshOptimizationSettings& Settings )
	{
		if ( LODIndex >= OptimizationSettings.Num() )
		{
			FSkeletalMeshOptimizationSettings DefaultSettings;
			const FSkeletalMeshOptimizationSettings& SettingsToCopy = OptimizationSettings.Num() ? OptimizationSettings.Last() : DefaultSettings;
			while ( LODIndex >= OptimizationSettings.Num() )
			{
				OptimizationSettings.AddItem( SettingsToCopy );
			}
		}
		check( LODIndex < OptimizationSettings.Num() );
		OptimizationSettings( LODIndex ) = Settings;
	}

	/**
	 * Removes an entry from the list of optimization settings.
	 * @param LODIndex - LOD index for which to remove optimization settings.
	 */
	void RemoveOptimizationSettings( INT LODIndex )
	{
		if ( LODIndex < OptimizationSettings.Num() )
		{
			OptimizationSettings.Remove( LODIndex );
		}
	}

	/**
	 * Clears max deviations.
	 */
	void ClearOptimizationSettings()
	{
		OptimizationSettings.Empty();
	}
	/**
	 * Initializes Lod information for clothing 
	 */
	void InitClothingLod();
};


#include "UnkDOP.h"

typedef TkDOPTreeCompact<class FSkelMeshCollisionDataProvider,WORD>	TSkeletalKDOPTree;
typedef TkDOPTree<class FSkelMeshCollisionDataProvider,WORD>	TSkeletalKDOPTreeLegacy;


/** Data used for doing line checks against triangles rigidly weighted to a specific bone. */
struct FPerPolyBoneCollisionData
{
	/** KDOP tree spacial data structure used for collision checks. */
	TSkeletalKDOPTree								KDOPTree;

	/** Collision vertices, in local bone space */
	TArray<FVector>									CollisionVerts;

	FPerPolyBoneCollisionData() {}

	friend FArchive& operator<<(FArchive& Ar,FPerPolyBoneCollisionData& Data)
	{
		TSkeletalKDOPTreeLegacy LegacykDOPTree;
		UBOOL bNeedsKDopConversion = FALSE;
		if( !Ar.IsLoading() || Ar.Ver() >= VER_COMPACTKDOPSTATICMESH )		
		{
			Ar << Data.KDOPTree;
		}
		else if (Ar.IsLoading())
		{
			Ar << LegacykDOPTree; 
			bNeedsKDopConversion = TRUE;
		}
		Ar << Data.CollisionVerts;

		if (bNeedsKDopConversion)
		{
			TArray<FkDOPBuildCollisionTriangle<WORD> > kDOPBuildTriangles;
			for (INT TriangleIndex = 0; TriangleIndex < LegacykDOPTree.Triangles.Num(); TriangleIndex++)
			{
				FkDOPCollisionTriangle<WORD>& OldTriangle = LegacykDOPTree.Triangles(TriangleIndex);
				new (kDOPBuildTriangles) FkDOPBuildCollisionTriangle<WORD>(
					OldTriangle.v1,
					OldTriangle.v2,
					OldTriangle.v3,
					OldTriangle.MaterialIndex,
					Data.CollisionVerts(OldTriangle.v1),
					Data.CollisionVerts(OldTriangle.v2),
					Data.CollisionVerts(OldTriangle.v3));
			}
			Data.KDOPTree.Build(kDOPBuildTriangles);
		}
		else if (Ar.IsLoading() && Ar.Ver() < VER_KDOP_ONE_NODE_FIX && Data.KDOPTree.Nodes.Num() == 2 )
		{
			TArray<FkDOPBuildCollisionTriangle<WORD> > kDOPBuildTriangles;
			for (INT TriangleIndex = 0; TriangleIndex < Data.KDOPTree.Triangles.Num(); TriangleIndex++)
			{
				FkDOPCollisionTriangle<WORD>& OldTriangle = Data.KDOPTree.Triangles(TriangleIndex);
				new (kDOPBuildTriangles) FkDOPBuildCollisionTriangle<WORD>(
					OldTriangle.v1,
					OldTriangle.v2,
					OldTriangle.v3,
					OldTriangle.MaterialIndex,
					Data.CollisionVerts(OldTriangle.v1),
					Data.CollisionVerts(OldTriangle.v2),
					Data.CollisionVerts(OldTriangle.v3));
			}
			Data.KDOPTree.Build(kDOPBuildTriangles);
		}
		return Ar;
	}
};

/** This struct provides the interface into the skeletal mesh collision data */
class FSkelMeshCollisionDataProvider
{
	/** The component this mesh is attached to */
	const USkeletalMeshComponent* Component;
	/** The mesh that is being collided against */
	class USkeletalMesh* Mesh;
	/** Index into PerPolyBoneKDOPs array within SkeletalMesh */
	INT BoneCollisionIndex;
	/** Index of bone that this collision is for within the skel mesh. */
	INT BoneIndex;
	/** Cached calculated bone transform. Includes scaling. */
	FMatrix BoneToWorld;
	/** Cached calculated inverse bone transform. Includes scaling. */
	FMatrix WorldToBone;

	/** Hide default ctor */
	FSkelMeshCollisionDataProvider(void)
	{
	}

public:
	/** Sets the component and mesh members */
	FORCEINLINE FSkelMeshCollisionDataProvider(const USkeletalMeshComponent* InComponent, USkeletalMesh* InMesh, INT InBoneIndex, INT InBoneCollisionIndex) :
		Component(InComponent),
		Mesh(InComponent->SkeletalMesh),
		BoneCollisionIndex(InBoneCollisionIndex),
		BoneIndex(InBoneIndex)
		{
			BoneToWorld = Component->GetBoneMatrix(BoneIndex);
			WorldToBone = BoneToWorld.InverseSafe();
		}

	FORCEINLINE const FVector& GetVertex(WORD Index) const
	{
		return Mesh->PerPolyBoneKDOPs(BoneCollisionIndex).CollisionVerts(Index);
	}

	FORCEINLINE UMaterialInterface* GetMaterial(WORD MaterialIndex) const
	{
		return Component->GetMaterial(MaterialIndex);
	}

	FORCEINLINE INT GetItemIndex(WORD MaterialIndex) const
	{
		return 0;
	}

	FORCEINLINE UBOOL ShouldCheckMaterial(INT MaterialIndex) const
	{
		return TRUE;
	}

	FORCEINLINE const TSkeletalKDOPTree& GetkDOPTree(void) const
	{
		return Mesh->PerPolyBoneKDOPs(BoneCollisionIndex).KDOPTree;
	}

	FORCEINLINE const FMatrix& GetLocalToWorld(void) const
	{
		return BoneToWorld;
	}

	FORCEINLINE const FMatrix& GetWorldToLocal(void) const
	{
		return WorldToBone;
	}

	FORCEINLINE FMatrix GetLocalToWorldTransposeAdjoint(void) const
	{
		return GetLocalToWorld().TransposeAdjoint();
	}

	FORCEINLINE FLOAT GetDeterminant(void) const
	{
		return GetLocalToWorld().Determinant();
	}
};




/*-----------------------------------------------------------------------------
FSkeletalMeshSceneProxy
-----------------------------------------------------------------------------*/

/**
 * A skeletal mesh component scene proxy.
 */
class FSkeletalMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	/** 
	 * Constructor. 
	 * @param	Component - skeletal mesh primitive being added
	 */
	FSkeletalMeshSceneProxy(const USkeletalMeshComponent* Component, const FColor& InBoneColor = FColor(230, 230, 255));

	// FPrimitiveSceneProxy interface.

	/** 
	* Draw the scene proxy as a dynamic element
	*
	* @param	PDI - draw interface to render to
	* @param	View - current view
	* @param	DPGIndex - current depth priority 
	* @param	Flags - optional set of flags from EDrawDynamicElementFlags
	*/
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags);

	/**
	* Draw only the secion of the material ID given of the scene proxy as a dynamic element
	*
	* @param	PDI - draw interface to render to
	* @param	View - current view
	* @param	DPGIndex - current depth priority 
	* @param	Flags - optional set of flags from EDrawDynamicElementFlags
	* @param 	ForceLOD - Force this LOD. If -1, use current LOD of mesh. 
	* @param	InMaterial - which material section to draw
	*/
	virtual void DrawDynamicElementsByMaterial(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags, INT ForceLOD, INT InMaterial);

	/**
	* Draws the primitive's dynamic decal elements.  This is called from the rendering thread for each frame of each view.
	* The dynamic elements will only be rendered if GetViewRelevance declares dynamic relevance.
	* Called in the rendering thread.
	*
	* @param	PDI						The interface which receives the primitive elements.
	* @param	View					The view which is being rendered.
	* @param	InDepthPriorityGroup	The DPG which is being rendered.
	* @param	bDynamicLightingPass	TRUE if drawing dynamic lights, FALSE if drawing static lights.
	* @param	bDrawOpaqueDecals		TRUE if we want to draw opaque decals
	* @param	bDrawTransparentDecals	TRUE if we want to draw transparent decals
	* @param	bTranslucentReceiverPass	TRUE during the decal pass for translucent receivers, FALSE for opaque receivers.
	*/
	virtual void DrawDynamicDecalElements(
		FPrimitiveDrawInterface* PDI,
		const FSceneView* View,
		UINT InDepthPriorityGroup,
		UBOOL bDynamicLightingPass,
		UBOOL bDrawOpaqueDecals,
		UBOOL bDrawTransparentDecals,
		UBOOL bTranslucentReceiverPass
		);

	/**
	 * Adds a decal interaction to the primitive.  This is called in the rendering thread by AddDecalInteraction_GameThread.
	 */
	virtual void AddDecalInteraction_RenderingThread(const FDecalInteraction& DecalInteraction);

	/**
	 * Removes a decal interaction from the primitive.  This is called in the rendering thread by RemoveDecalInteraction_GameThread.
	 */
	virtual void RemoveDecalInteraction_RenderingThread(UDecalComponent* DecalComponent);

	/**
	 * Returns the world transform to use for drawing.
	 * @param View - Current view
	 * @param OutLocalToWorld - Will contain the local-to-world transform when the function returns.
	 * @param OutWorldToLocal - Will contain the world-to-local transform when the function returns.
	 */
	virtual void GetWorldMatrices( const FSceneView* View, FMatrix& OutLocalToWorld, FMatrix& OutWorldToLocal );

	/**
	 * Relevance is always dynamic for skel meshes unless they are disabled
	 */
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View);

	/**
	 *	Called during FSceneRenderer::InitViews for view processing on scene proxies before rendering them
	 *  Only called for primitives that are visible and have bDynamicRelevance
 	 *
	 *	@param	ViewFamily		The ViewFamily to pre-render for
	 *	@param	VisibilityMap	A BitArray that indicates whether the primitive was visible in that view (index)
	 *	@param	FrameNumber		The frame number of this pre-render
	 */
	virtual void PreRenderView(const FSceneViewFamily* ViewFamily, const DWORD VisibilityMap, INT FrameNumber);

	/** Util for getting LOD index currently used by this SceneProxy. */
	INT GetCurrentLODIndex();

	/** 
	 * Render bones for debug display
	 */
	void DebugDrawBones(FPrimitiveDrawInterface* PDI,const FSceneView* View, const TArray<FBoneAtom>& InSpaceBases, const class FStaticLODModel& LODModel, const FColor& LineColor, INT ChunkIndexPreview);

	/** 
	 * Render physics asset for debug display
	 */
	void DebugDrawPhysicsAsset(FPrimitiveDrawInterface* PDI,const FSceneView* View);

	/** Render any per-poly collision data for tri's rigidly weighted to bones. */
	void DebugDrawPerPolyCollision(FPrimitiveDrawInterface* PDI, const TArray<FBoneAtom>& InSpaceBases);

	/** 
	 * Render soft body tetrahedra for debug display
	 */
	void DebugDrawSoftBodyTetras(FPrimitiveDrawInterface* PDI, const FSceneView* View);

	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() + LODSections.GetAllocatedSize() ); }

	/**
	* Updates morph material usage for materials referenced by each LOD entry
	*
	* @param bNeedsMorphUsage - TRUE if the materials used by this skeletal mesh need morph target usage
	*/
	void UpdateMorphMaterialUsage(UBOOL bNeedsMorphUsage);

	friend class FSkeletalMeshSectionIter;

protected:
	AActor* Owner;
	const USkeletalMesh* SkeletalMesh;
	FSkeletalMeshObject* MeshObject;
	UPhysicsAsset* PhysicsAsset;

	/** data copied for rendering */
	FColor LevelColor;
	FColor PropertyColor;
	BITFIELD bCastShadow : 1;
	BITFIELD bShouldCollide : 1;
	BITFIELD bDisplayBones : 1;
	BITFIELD bForceWireframe : 1;
	BITFIELD bMaterialsNeedMorphUsage : 1;
	BITFIELD bIsCPUSkinned : 1;
	FMaterialViewRelevance MaterialViewRelevance;

	/** info for section element in an LOD */
	struct FSectionElementInfo
	{
		/*
		FSectionElementInfo() 
		:	Material(NULL)
		,	bEnableShadowCasting(TRUE)
		{}
		*/
		FSectionElementInfo(UMaterialInterface* InMaterial, UBOOL bInEnableShadowCasting, INT InUseMaterialIndex, INT InClothingAssetIndex)
		:	Material( InMaterial )
		,	bEnableShadowCasting( bInEnableShadowCasting )
		,	UseMaterialIndex( InUseMaterialIndex )
		,	ClothingAssetIndex( InClothingAssetIndex )
		{}
		UMaterialInterface* Material;
		/** Whether shadow casting is enabled for this section. */
		UBOOL bEnableShadowCasting;
		/** Index into the materials array of the skel mesh or the component after LOD mapping */
		INT UseMaterialIndex;
		/** Index of the Clothing Asset to use */
		INT ClothingAssetIndex;
	};

	/** Section elements for a particular LOD */
	struct FLODSectionElements
	{
		TArray<FSectionElementInfo> SectionElements;
		// mapping from new sections used when swapping to instance weights to the base SectionElements
		TArray< TArray<INT> > InstanceWeightsSectionElementsMapping;
	};
	
	/** Array of section elements for each LOD */
	TArray<FLODSectionElements> LODSections;
	
	/** This is the color used to render bones if bDisplayBones is set to TRUE */
	FColor BoneColor;

	/** The color used by the wireframe mesh overlay mode */
	FColor WireframeOverlayColor;

	/**
	* Draw only the section of the scene proxy as a dynamic element
	* This is to avoid redundant code of two functions (DrawDynamicElementsByMaterial & DrawDynamicElements)
	* 
	* @param	PDI - draw interface to render to
	* @param	View - current view
	* @param	DPGIndex - current depth priority 
	* @param	const FStaticLODModel& LODModel - LODModel 
	* @param	const FSkelMeshSection& Section - Section
	* @param	const FSkelMeshChunk& Chunk - Chunk
	* @param	const FSectionElementInfo& SectionElementInfo - SectionElementInfo - material ID
	*/
	void DrawDynamicElementsSection(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,
		const FStaticLODModel& LODModel, const INT LODIndex, const FSkelMeshSection& Section, 
		const FSkelMeshChunk& Chunk, const FSectionElementInfo& SectionElementInfo, const FTwoVectors& CustomLeftRightVectors );

};


#endif // __UNSKELETALMESH_H__
