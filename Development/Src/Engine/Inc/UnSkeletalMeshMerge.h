/*=============================================================================
	UnSkeletalMeshMerge.h: Merging of unreal skeletal mesh objects.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
FSkeletalMeshMerge
-----------------------------------------------------------------------------*/

#ifndef __UNSKELETALMESHMERGE_H__
#define __UNSKELETALMESHMERGE_H__

/** 
* Info to map all the sections from a single source skeletal mesh to 
* a final section entry int he merged skeletal mesh
*/
struct FSkelMeshMergeSectionMapping
{
	/** indices to final section entries of the merged skel mesh */
	TArray<INT> SectionIDs;
};

/** 
* Utility for merging a list of skeletal meshes into a single mesh
*/
class FSkeletalMeshMerge
{
public:
	/**
	* Constructor
	* @param InMergeMesh - destination mesh to merge to
	* @param InSrcMeshList - array of source meshes to merge
	* @param InForceSectionMapping - optional array to map sections from the source meshes to merged section entries
	* @param StripTopLODs - number of high LODs to remove from input meshes
	*/
	FSkeletalMeshMerge( 
		USkeletalMesh* InMergeMesh, 
		const TArray<USkeletalMesh*>& InSrcMeshList, 
		const TArray<FSkelMeshMergeSectionMapping>& InForceSectionMapping,
		INT StripTopLODs
		);

	/**
	* Merge/Composite the list of source meshes onto the merge one
	* @return TRUE if succeeded
	*/
	UBOOL DoMerge();

private:
	/** Destination merged mesh */
	USkeletalMesh* MergeMesh;
	/** Array of source skeletal meshes  */
	const TArray<USkeletalMesh*>& SrcMeshList;

	/** Number of high LODs to remove from input meshes. */
	INT StripTopLODs;

	/** Info about source mesh used in merge. */
	struct FMergeMeshInfo
	{
		/** Mapping from RefSkeleton bone index in source mesh to output bone index. */
		TArray<INT> SrcToDestRefSkeletonMap;
	};

	/** Array of source mesh info structs. */
	TArray<FMergeMeshInfo> SrcMeshInfo;

	/** New reference skeleton, made from creating union of each part's skeleton. */
	TArray<FMeshBone> NewRefSkeleton;

	/** array to map sections from the source meshes to merged section entries */
	const TArray<FSkelMeshMergeSectionMapping>& ForceSectionMapping;

	/** Matches the Materials array in the final mesh - used for creating the right number of Material slots. */
	TArray<INT>	MaterialIds;

	/** keeps track of an existing section that need to be merged with another */
	struct FMergeSectionInfo
	{
		/** ptr to source skeletal mesh for this section */
		const USkeletalMesh* SkelMesh;
		/** ptr to source section for merging */
		const FSkelMeshSection* Section;
		/** ptr to source chunk for this section */
		const FSkelMeshChunk* Chunk;
		/** mapping from the original BoneMap for this sections chunk to the new MergedBoneMap */
		TArray<WORD> BoneMapToMergedBoneMap;

		FMergeSectionInfo( const USkeletalMesh* InSkelMesh,const FSkelMeshSection* InSection, const FSkelMeshChunk* InChunk )
			:	SkelMesh(InSkelMesh)
			,	Section(InSection)
			,	Chunk(InChunk)
		{}
	};

	/** info needed to create a new merged section */
	struct FNewSectionInfo
	{
		/** array of existing sections to merge */
		TArray<FMergeSectionInfo> MergeSections;
		/** merged bonemap */
		TArray<WORD> MergedBoneMap;
		/** material for use by this section */
		UMaterialInterface* Material;
		/** 
		* if -1 then we use the Material* to match new section entries
		* otherwise the MaterialId is used to find new section entries
		*/
		INT MaterialId;

		FNewSectionInfo( UMaterialInterface* InMaterial, INT InMaterialId )
			:	Material(InMaterial)
			,	MaterialId(InMaterialId)
		{}
	};

	/**
	* Merge a bonemap with an existing bonemap and keep track of remapping
	* (a bonemap is a list of indices of bones in the USkeletalMesh::RefSkeleton array)
	* @param MergedBoneMap - out merged bonemap
	* @param BoneMapToMergedBoneMap - out of mapping from original bonemap to new merged bonemap 
	* @param BoneMap - input bonemap to merge
	*/
	void MergeBoneMap( TArray<WORD>& MergedBoneMap, TArray<WORD>& BoneMapToMergedBoneMap, const TArray<WORD>& BoneMap );

	/**
	* Creates a new LOD model and adds the new merged sections to it. Modifies the MergedMesh.
	* @param LODIdx - current LOD to process
	*/
	template<typename VertexDataType>
	void GenerateLODModel( INT LODIdx );

	/**
	* Generate the list of sections that need to be created along with info needed to merge sections
	* @param NewSectionArray - out array to populate
	* @param LODIdx - current LOD to process
	*/
	void GenerateNewSectionArray( TArray<FNewSectionInfo>& NewSectionArray, INT LODIdx );

	/**
	* (Re)initialize and merge skeletal mesh info from the list of source meshes to the merge mesh
	* @return TRUE if succeeded
	*/
	UBOOL ProcessMergeMesh();
};

#endif // __UNSKELETALMESH_H__

