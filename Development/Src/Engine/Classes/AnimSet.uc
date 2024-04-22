/** 
 * This is a set of AnimSequences
 * All sequence have the same number of tracks, and they relate to the same bone names.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimSet extends Object
	native(Anim)
	hidecategories(Object);


/** This is a mapping table between each bone in a particular skeletal mesh and the tracks of this animation set. */
struct native AnimSetMeshLinkup
{
	/** 
	 * Mapping table. Size must be same as size of SkelMesh reference skeleton. 
	 * No index should be more than the number of tracks in this AnimSet.
	 * -1 indicates no track for this bone - will use reference pose instead.
	 */
	var array<INT> BoneToTrackTable;

	structcpptext
	{
		/** Reset this linkup and re-create between the provided skeletal mesh and anim set. */
		void BuildLinkup(USkeletalMesh* InSkelMesh, UAnimSet* InAnimSet);
	}
};

/** 
 *	Indicates that only the rotation should be taken from the animation sequence and the translation should come from the SkeletalMesh ref pose. 
 *	Note that the root bone always takes translation from the animation, even if this flag is set.
 *	You can use the UseTranslationBoneNames array to specify other bones that should use translation with this flag set.
 */
var() bool				bAnimRotationOnly;

/** Bone name that each track relates to. TrackBoneName.Num() == Number of tracks. */
var array<name>			TrackBoneNames;

/** Actual animation sequence information. */
var	array<AnimSequence> Sequences;
/** Lookup-cache, populated in PostLoad. */
var	native transient Map{FName,INT} SequenceCache;

/** Non-serialised cache of linkups between different skeletal meshes and this AnimSet. */
var transient array<AnimSetMeshLinkup>	LinkupCache;
/** Runtime built mapping table between SkeletalMeshes, and LinkupCache array indices. */
var native transient Map{FName,INT} SkelMesh2LinkupCache;
/** 
 *	Array of booleans that indicate whether or not to read the translation of a bone from animation or ref skeleton.
 *	This is basically a cooked down version of UseTranslationBoneNames for speed.
 *	Size matches the number of tracks.
 */
var transient Array<byte> BoneUseAnimTranslation;
/** Cooked down version of ForceMeshTranslationBoneNames */
var transient Array<byte> ForceUseMeshTranslation;

/** Names of bones that should use translation from the animation, if bAnimRotationOnly is set. */
var() Array<name>		UseTranslationBoneNames;
/** List of bones which are ALWAYS going to use their translation from the mesh and not the animation. */
var() Array<Name>		ForceMeshTranslationBoneNames;
/** In the AnimSetEditor, when you switch to this AnimSet, it sees if this skeletal mesh is loaded and if so switches to it. */
var	name				PreviewSkelMeshName;
/** Holds the name of the skeletal mesh whose reference skeleton best matches the TrackBoneName array. */
var name				BestRatioSkelMeshName;

cpptext
{
	// UObject interface
	virtual void PreSave();
	virtual void PostLoad();
	virtual void BeginDestroy();
	
	// UAnimSet interface
	/**
	 * See if we can play sequences from this AnimSet on the provided SkeletalMesh.
	 * Returns true if there is a bone in SkelMesh for every track in the AnimSet,
	 * or there is a track of animation for every bone of the SkelMesh.
	 * 
	 * @param	SkelMesh	SkeletalMesh to compare the AnimSet against.
	 * @return				TRUE if animation set can play on supplied SkeletalMesh, FALSE if not.
	 */
	UBOOL CanPlayOnSkeletalMesh(USkeletalMesh* SkelMesh) const;

	/** Get Ratio of how much that mesh fits that animation set */
	FLOAT GetSkeletalMeshMatchRatio(USkeletalMesh* SkelMesh) const;

	/**
	 * Returns the AnimSequence with the specified name in this set.
	 * 
	 * @param		SequenceName	Name of sequence to find.
	 * @return						Pointer to AnimSequence with desired name, or NULL if sequence was not found.
	 */
	UAnimSequence* FindAnimSequence(FName SequenceName);

	/** 
	 * Find a mesh linkup table (mapping of sequence tracks to bone indices) for a particular SkeletalMesh
	 * If one does not already exist, create it now.
	 */
	INT GetMeshLinkupIndex(USkeletalMesh* SkelMesh);

	/**
	 * @return		The track index for the bone with the supplied name, or INDEX_NONE if no track exists for that bone.
	 */
	INT FindTrackWithName(FName BoneName) const
	{
		return TrackBoneNames.FindItemIndex( BoneName );
	}

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	INT GetResourceSize();

	/**
	 * Clears all sequences and resets the TrackBoneNames table.
	 */
	void ResetAnimSet();
	/** 
	 * Properly remove an AnimSequence from an AnimSet, and updates references it might have.
	 * @return TRUE if AnimSequence was properly removed, FALSE if it wasn't found.
	 */
	UBOOL RemoveAnimSequenceFromAnimSet(UAnimSequence* AnimSeq);

	/** Util that find all AnimSets and flushes their LinkupCache, then calls InitAnimTree on all SkeletalMeshComponents. */
	static void ClearAllAnimSetLinkupCaches();
	
	/**
	 * Animation Usage Tracking
	 */
	void	TraceAnimationUsage();
	void	RecordAnimationUsage();	

	static void OutputAnimationUsage();	
	static void CleanUpAnimationUsage();	
	static void TickAnimationUsage();
}

defaultproperties
{
	bAnimRotationOnly=true
}
