/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class FaceFXAnimSet extends Object
	hidecategories(Object)
	native;

/** 
 *  Default FaceFXAsset to use when editing this FaceFXAnimSet etc. 
 *  Is the one that was used as the basis for creating this AnimSet.
 */
var() editoronly const FaceFXAsset DefaultFaceFXAsset;

/** Internal use.  FaceFX representation of this AnimSet. */
var const native pointer InternalFaceFXAnimSet;
/** 
 *  Internal use.  Raw bytes of the FaceFX AnimSet. 
 *  This only stays loaded in the editor.
 */
var const native array<byte> RawFaceFXAnimSetBytes;
/** 
 *  Internal use.  Raw bytes of the FaceFX Studio mini session for this AnimSet. 
 *  This only stays loaded in the editor.
 */
var const native array<byte> RawFaceFXMiniSessionBytes;

/**
 *  Array of SoundCue objects that the FaceFXAnimSet references.
 */
var editoronly notforconsole array<SoundCue> ReferencedSoundCues;

/**
 *  Internal use.  The number of errors generated during load.
 */
var int NumLoadErrors;

cpptext
{
	/** Creates a new FaceFX AnimSet for the given FaceFX Asset.  This is only called from within the editor. */
	void CreateFxAnimSet( class UFaceFXAsset* FaceFXAsset );

	/** Get list of FaceFX animations in this AnimSet. Names are in the form GroupName.AnimName.*/
	void GetSequenceNames(TArray<FString>& OutNames);

#if WITH_FACEFX
	/** Returns the internal FaceFX representation of this FaceFX AnimSet. */
	class OC3Ent::Face::FxAnimSet* GetFxAnimSet( void );
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