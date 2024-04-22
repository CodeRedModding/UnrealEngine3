/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MorphTargetSet extends Object
	native(Anim);
	
/** Array of pointers to MorphTarget objects, containing vertex deformation information. */ 
var	array<MorphTarget>		Targets;

/** SkeletalMesh that this MorphTargetSet works on. */
var	SkeletalMesh			BaseSkelMesh;

/** morph mesh original wedge point indices for each LOD - used to remap*/
var	const native			Array_Mirror	RawWedgePointIndices{TArrayNoInit< TArray<DWORD> >}; 

/** Find a morph target by name in this MorphTargetSet. */ 
native final function MorphTarget FindMorphTarget( Name MorphTargetName );

cpptext
{
	/** 
	* Returns a one line description of an object for viewing in the thumbnail view of the generic browser
	*/
	virtual FString GetDesc();

	/**
	* Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	*
	* @return size of resource as to be displayed to artists/ LDs in the Editor.
	*/
	INT GetResourceSize();

	/** 
	* Verify if the current BaseSkelMesh is still valid
	* 
	* @return TRUE if so. FALSE otherwise. 
	*/
	UBOOL IsValidBaseMesh();

	/** 
	* Update vertex indices for all morph targets from the base mesh
	*
	*/
	void UpdateMorphTargetsFromBaseMesh();

	/**
	* Refill data assuming current base mesh exactly works
	* This is only for compatibility to support old morph targets to re-fill base mesh information 
	* 
	* @param DoNotOverwriteIfExists:	Set to TRUE if you'd like to not overwrite if exists
	*							This can be used for filling up LOD data when morph target doesn't have LOD data imported yet
	*
	*/
	void FillBaseMeshData(UBOOL DoNotOverwriteIfExists);

	/** Serialize **/
	virtual void Serialize(FArchive& Ar);
};