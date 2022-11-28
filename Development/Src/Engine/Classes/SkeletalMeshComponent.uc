class SkeletalMeshComponent extends MeshComponent
	native
	noexport
	collapsecategories
	hidecategories(Object)
	editinlinenew;
	
var()	SkeletalMesh			SkeletalMesh;

var()   const					AnimTree	AnimTreeTemplate;
var()	const editinline export	AnimNode	Animations;

// Used for ragdolls, mattresses etc.
var()	PhysicsAsset							PhysicsAsset;
var		editinline export PhysicsAssetInstance	PhysicsAssetInstance;

var()	float					PhysicsWeight;

var native const pointer		MeshObject;

var native const array<matrix>	SpaceBases;

// The set of AnimSets that will be looked in to find a particular sequence, specified by name in an AnimNodeSequence.
// Array is search from last to first element, so you can replace a particular sequence but putting a set containing the new version later in the array.
// You will need to call SetAnim again on nodes that may be affected by any changes you make to this array.
var()	array<AnimSet>			AnimSets;

// Attachments.

struct Attachment
{
	var() editinline ActorComponent	Component;
	var() name						BoneName;
	var() vector					RelativeLocation;
	var() rotator					RelativeRotation;
	var() vector					RelativeScale;

	structdefaults
	{
		RelativeScale=(X=1,Y=1,Z=1)
	};
};

var() const array<Attachment>	Attachments;

struct BoneRotationControl
{
	var	int		BoneIndex;
	var name	BoneName;
	var Rotator	BoneRotation;
	var byte	BoneSpace;
};
var const array<BoneRotationControl>	BoneRotationControls;

struct BoneTranslationControl
{
	var	int		BoneIndex;
	var	name	BoneName;
	var	vector	BoneTranslation;
};
var const array<BoneTranslationControl>	BoneTranslationControls;

var int			ForcedLodModel; // if 0, auto-select LOD level. if >0, force to (ForcedLodModel-1)
var int			bForceRefpose;
var int			bDisplayBones;
var int			bHideSkin;
var int			bForceRawOffset;
var int			bIgnoreControllers;

var	material	LimitMaterial;


// Root bone options.

/** In addition to controlling the translation, we can also discard the rotation part of the root bone. */
var() int					bDiscardRootRotation;

/** 
 *	This will actually call MoveActor to move the Actor owning this SkeletalMeshComponent.
 *	You can specify the behaviour for each axis (mesh space).
 *	Doing this for multiple skeletal meshes on the same Actor does not make much sense!
 */
enum ERootBoneAxis
{
	/** Do the default behaviour - apply root translation from animation and do no affect owning Actor location. */
	RBA_Default,

	/** Discard any movement of the root bone, pinning the animation to origin of the component.  */
	RBA_Discard,

	/** Discard movement to root from animation, but apply the delta translation to the movement of the Actor instead. */
	RBA_Translate
};

var() ERootBoneAxis		RootBoneOption[3]; // [X, Y, Z] axes

// Internal
var transient int		bOldRootInitialized;
var transient vector	OldRootLocation;

//=============================================================================
// Animation.

// Attachment functions.
native final function AttachComponent(ActorComponent Component,name BoneName,optional vector RelativeLocation,optional rotator RelativeRotation,optional vector RelativeScale);
native final function DetachComponent(ActorComponent Component);

// Skeletal animation.
simulated native final function SetSkeletalMesh( SkeletalMesh NewMesh );

/**
 * Find a named AnimSequence from the AnimSets array in the SkeletalMeshComponent. 
 * This searches array from end to start, so specific sequence can be replaced by putting a set containing a sequence with the same name later in the array.
 * 
 * @param AnimSeqName Name of AnimSequence to look for.
 * 
 * @return Pointer to found AnimSequence. Returns NULL if could not find sequence with that name.
 */
native final function AnimSequence FindAnimSequence( Name AnimSeqName );

/* SetBoneRotation
	Control a bone's Rotation.
	Input:	BoneName
			BoneRotation
			Space	0 = Relative (Component Space)
					1 = Relative (Actor Space)
					2 = Lock Bone to World Rotation
*/
native final function SetBoneRotation( name BoneName, Rotator BoneRotation, optional byte BoneSpace );

/**
 * Clears out all current bone rotations.
 */
native final function ClearBoneRotations();

native final function DrawAnimDebug( Canvas canvas );

native final function quat GetBoneQuaternion( name BoneName );
native final function vector GetBoneLocation( name BoneName );

native final function SetAnimTreeTemplate(AnimTree NewTemplate);

defaultproperties
{
	RootBoneOption[0] = RBA_Default
	RootBoneOption[1] = RBA_Default
	RootBoneOption[2] = RBA_Default
}