/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class FaceFXAsset extends Object
	hidecategories(Object)
	native;

/** 
 *  Default skeletal mesh to use when previewing this FaceFXAsset etc. 
 *  Is the one that was used as the basis for creating this Asset.
 */
var const editoronly SkeletalMesh DefaultSkelMesh;

/** Internal use.  FaceFX representation of this asset. */
var const native pointer FaceFXActor;
/** 
 *  Internal use.  Raw bytes of the FaceFX Actor for this asset. 
 *  This only stays loaded in the editor.
 */
var const native array<byte> RawFaceFXActorBytes;
/** 
 *  Internal use.  Raw bytes of the FaceFX Studio session for this asset. 
 *  This only stays loaded in the editor.
 */
var const native array<byte> RawFaceFXSessionBytes;

/**
 *	MorphTargetSets used when previewing this FaceFXAsset in FaceFX Studio.
 *  Note that these are only valid in the editor.
 */
var() editoronly array<MorphTargetSet>	PreviewMorphSets;

/**
 *  Array of currently mounted FaceFXAnimSets.
 *	We only track this if GIsEditor!
 */
var transient array<FaceFXAnimSet> MountedFaceFXAnimSets;

/**
 *  Array of SoundCue objects that the FaceFXAsset references.
 */
var editoronly notforconsole array<SoundCue> ReferencedSoundCues;

/**
 *  Internal use.  The number of errors generated during load.
 */
var int NumLoadErrors;

/**
 *  Mounts the specified FaceFXAnimSet into this FaceFXAsset.
 */
native final function MountFaceFXAnimSet( FaceFXAnimSet AnimSet );

/**
 *  Internal use.  Unmounts the specified FaceFXAnimSet from this FaceFXAsset.
 */
native final function UnmountFaceFXAnimSet( FaceFXAnimSet AnimSet );

cpptext
{
	/** Creates a new FaceFX Actor for this FaceFX Asset.  This is only called from within the editor. */
	void CreateFxActor( class USkeletalMesh* SkelMesh );

	/** 
	 *	Get list of FaceFX animations in this Asset. Names are in the form GroupName.AnimName.
	 *	@param bExcludeMountedGroups	If true, do not show animations that are in separate FaceFXAnimSets currently mounted to the Asset.
	 */
	void GetSequenceNames(UBOOL bExcludeMountedGroups, TArray<FString>& OutNames);

#if WITH_FACEFX
	/** Returns the internal FaceFX representation of this FaceFX Asset. */
	class OC3Ent::Face::FxActor* GetFxActor( void );
#endif

	/** Fixes up the ReferencedSoundCue stuff. */
	void FixupReferencedSoundCues();

	// UObject interface.

	/** 
	 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
	 */
	virtual FString GetDesc();
	virtual void PostLoad();
	virtual void FinishDestroy();
	virtual void Serialize(FArchive& Ar);

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	INT GetResourceSize();

	/**
	 * Used by various commandlets to purge editor only and platform-specific data from various objects
	 * 
	 * @param PlatformsToKeep Platforms for which to keep platform-specific data
	 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
	 */
	virtual void StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData);
}