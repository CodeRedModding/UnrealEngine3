/*=============================================================================
UnSkeletalMeshSorting.h: Static sorting for skeletal mesh triangles
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/


/**
* Group triangles into connected strips 
*
* @param NumTriangles - The number of triangles to group
* @param Indices - pointer to the index buffer data
* @outparam OutTriSet - an array containing the set number for each triangle.
* @return the maximum set number of any triangle.
*/
INT GetConnectedTriangleSets( INT NumTriangles, const DWORD* Indices, TArray<UINT>& OutTriSet );

/**
* Use NVTriStrip to reorder the indices within a connected strip for cache efficiency
*
* @parm Indices - pointer to the index data
* @parm NumIndices - number of indices to optimize
*/
void CacheOptimizeSortStrip(DWORD* Indices, INT NumIndices);

/**
* Remove all sorting and reset the index buffer order to that returned by NVTriStrip.
*
* @param SkelMesh - Skeletal mesh
* @param NumTriangles - Number of triangles
* @param Vertices - pointer to the skeletal mesh's FSoftSkinVertex array.
* @param Indices - pointer to the index buffer data to be reordered
*/
void SortTriangles_None( USkeletalMesh* SkelMesh, INT NumTriangles, const FSoftSkinVertex* Vertices, DWORD* Indices );


/**
* Sort connected strips by distance from the center of each strip to the center of all vertices.
*
* @param SkelMesh - Skeletal mesh
* @param NumTriangles - Number of triangles
* @param Vertices - pointer to the skeletal mesh's FSoftSkinVertex array.
* @param Indices - pointer to the index buffer data to be reordered
*/
void SortTriangles_CenterRadialDistance( USkeletalMesh* SkelMesh, INT NumTriangles, const FSoftSkinVertex* Vertices, DWORD* Indices );

/**
* Sort triangles randomly (for testing)
*
* @param SkelMesh - Skeletal mesh
* @param NumTriangles - Number of triangles
* @param Vertices - pointer to the skeletal mesh's FSoftSkinVertex array.
* @param Indices - pointer to the index buffer data to be reordered
*/
void SortTriangles_Random( INT NumTriangles, const FSoftSkinVertex* Vertices, DWORD* Indices );

/**
* Sort triangles by merging contiguous strips but otherwise retaining draw order
*
* @param SkelMesh - Skeletal mesh
* @param NumTriangles - Number of triangles
* @param Vertices - pointer to the skeletal mesh's FSoftSkinVertex array.
* @param Indices - pointer to the index buffer data to be reordered
*/
void SortTriangles_MergeContiguous( INT NumTriangles, INT NumVertices, const FSoftSkinVertex* Vertices, DWORD* Indices );