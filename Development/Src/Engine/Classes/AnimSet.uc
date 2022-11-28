/** 
 * This is a set of AnimSequences
 * All sequence have the same number of tracks, and they relate to the same bone names.
 */

class AnimSet extends Object
	native(Anim)
	hidecategories(Object);


/** This is a mapping table between each bone in a particular skeletal mesh and the tracks of this animation set. */
struct native AnimSetMeshLinkup
{
	/** Skeletal mesh that this linkup relates to. */
	var SkeletalMesh	SkelMesh;

	/** 
	 * Mapping table. Size must be same as size of SkelMesh reference skeleton. 
	 * No index should be more than the number of tracks in this AnimSet.
	 * -1 indicates no track for this bone - will use reference pose instead.
	 */
	var array<INT>		BoneToTrackTable;
};




/** Bone name that each track relates to. TrackBoneName.Num() == Number of tracks. */
var array<name>			TrackBoneNames;

/** Actual animation sequence information. */
var	array<AnimSequence>	Sequences;

/** Non-serialised cache of linkups between different skeletal meshes and this AnimSet. */
var transient array<AnimSetMeshLinkup>	LinkupCache;

cpptext
{
	// UObject interface

	void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz );
	FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz );
	INT GetThumbnailLabels( TArray<FString>* InLabels );


	// UAnimSet interface

	/** 
	 * See if we can play sequences from this AnimSet on the provided SkeletalMesh.
	 * Returns true if there is a bone in SkelMesh for every track in the AnimSet - so will allow animation to play if mesh has too many bones.
	 */
	UBOOL CanPlayOnSkeletalMesh(USkeletalMesh* SkelMesh);

	/** Find an AnimSequence with the given name in this set. */
	UAnimSequence* FindAnimSequence(FName SequenceName);

	/** 
	 * Find a mesh linkup table (mapping of sequence tracks to bone indices) for a particular SkeletalMesh
	 * If one does not already exist, create it now.
	 */
	FAnimSetMeshLinkup* GetMeshLinkup(USkeletalMesh* SkelMesh);

	/** Find the track index for the bone with the supplied name. Returns INDEX_NONE if no track exists for that bone. */
	INT FindTrackWithName(FName BoneName);

	/** Size of total animation data in this AnimSet in bytes. */
	INT GetAnimSetMemory(); 

	/** Clear all sequences and reset the TrackBoneNames table. */
	void ResetAnimSet();
}