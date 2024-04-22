/*=============================================================================
	UnMorphTools.cpp: Morph target creation helper classes.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"


/** compare based on base mesh source vertex indices */
static INT CDECL CompareMorphVertex(const void *A, const void *B)
{
	return( ((FMorphTargetVertex*)A)->SourceIdx - ((FMorphTargetVertex*)B)->SourceIdx );
}

/**
* Generate the streams for this morph target mesh using
* a base mesh and a target mesh to find the positon differences
* and other vertex attributes.
*
* @param	BaseSource - source mesh for comparing position differences
* @param	TargetSource - final target vertex positions/attributes 
* @param	LODIndex - level of detail to use for the geometry
*/
void UMorphTarget::CreateMorphMeshStreams( const FMorphMeshRawSource& BaseSource, const FMorphMeshRawSource& TargetSource, INT LODIndex )
{
	check(BaseSource.IsValidTarget(TargetSource));

	const FLOAT CLOSE_TO_ZERO_DELTA = THRESH_POINTS_ARE_SAME * 4.f;

	// create the LOD entry if it doesn't already exist
	if( LODIndex == MorphLODModels.Num() )
	{
		new(MorphLODModels) FMorphTargetLODModel();
	}

	// morph mesh data to modify
	FMorphTargetLODModel& MorphModel = MorphLODModels(LODIndex);
	// copy the wedge point indices
	// for now just keep every thing 

	// set the original number of vertices
	MorphModel.NumBaseMeshVerts = BaseSource.Vertices.Num();

	// empty morph mesh vertices first
	MorphModel.Vertices.Empty();

	// array to mark processed base vertices
	TArray<UBOOL> WasProcessed;
	WasProcessed.Empty(BaseSource.Vertices.Num());
	WasProcessed.AddZeroed(BaseSource.Vertices.Num());


	TMap<DWORD,DWORD> WedgePointToVertexIndexMap;
	// Build a mapping of wedge point indices to vertex indices for fast lookup later.
	for( INT Idx=0; Idx < TargetSource.WedgePointIndices.Num(); Idx++ )
	{
		WedgePointToVertexIndexMap.Set( TargetSource.WedgePointIndices(Idx), Idx );
	}

	// iterate over all the base mesh indices
    for( INT Idx=0; Idx < BaseSource.Indices.Num(); Idx++ )
	{
		DWORD BaseVertIdx = BaseSource.Indices(Idx);

		// check for duplicate processing
		if( !WasProcessed(BaseVertIdx) )
		{
			// mark this base vertex as already processed
			WasProcessed(BaseVertIdx) = TRUE;

			// get base mesh vertex using its index buffer
			const FMorphMeshVertexRaw& VBase = BaseSource.Vertices(BaseVertIdx);
			// get the base mesh's original wedge point index
			DWORD BasePointIdx = BaseSource.WedgePointIndices(BaseVertIdx);

			// find the matching target vertex by searching for one
			// that has the same wedge point index
			DWORD* TargetVertIdx = WedgePointToVertexIndexMap.Find( BasePointIdx );

			// only add the vertex if the source point was found
			if( TargetVertIdx != NULL )
			{
				// get target mesh vertex using its index buffer
				const FMorphMeshVertexRaw& VTarget = TargetSource.Vertices(*TargetVertIdx);

				// change in position from base to target
				FVector PositionDelta( VTarget.Position - VBase.Position );

				// check if position actually changed much
				if( PositionDelta.Size() > CLOSE_TO_ZERO_DELTA )
				{
					// create a new entry
					FMorphTargetVertex NewVertex;
					// position delta
					NewVertex.PositionDelta = PositionDelta;
					// normal delta
					NewVertex.TangentZDelta = VTarget.TanZ - VBase.TanZ;
					// index of base mesh vert this entry is to modify
					NewVertex.SourceIdx = BaseVertIdx;

					// add it to the list of changed verts
					MorphModel.Vertices.AddItem( NewVertex );				
				}
			}	
		}
	}

	// sort the array of vertices for this morph target based on the base mesh indices 
	// that each vertex is associated with. This allows us to sequentially traverse the list
	// when applying the morph blends to each vertex.
	appQsort( &MorphModel.Vertices(0), MorphModel.Vertices.Num(), sizeof(FMorphTargetVertex), CompareMorphVertex );

	// remove array slack
	MorphModel.Vertices.Shrink();    
}
// remap vertex indices with input base mesh
void UMorphTarget::RemapVertexIndices( USkeletalMesh* InBaseMesh, const TArray< TArray<DWORD> > & BasedWedgePointIndices )
{
	// make sure base wedge point indices have more than what this morph target has
	// any morph target import needs base mesh (correct LOD index if it belongs to LOD)
	check ( BasedWedgePointIndices.Num() >= MorphLODModels.Num() );
	check ( InBaseMesh );

	// for each LOD
	for ( INT LODIndex=0; LODIndex<MorphLODModels.Num(); ++LODIndex )
	{
		if ( !InBaseMesh->LODModels.IsValidIndex( LODIndex ) )
		{
			debugf(TEXT("MorphTarget has more LODs (%d) that SkeletalMesh (%d) - Mesh render may appear to have corrupt blend"), LODIndex, InBaseMesh->LODModels.Num() );
			continue;
		}

		FStaticLODModel & BaseLODModel = InBaseMesh->LODModels(LODIndex);
		FMorphTargetLODModel& MorphLODModel = MorphLODModels(LODIndex);
		const TArray<DWORD> & LODWedgePointIndices = BasedWedgePointIndices(LODIndex);
		TArray<DWORD> NewWedgePointIndices;

		// If the LOD has been simplified, don't remap vertex indices else the data will be useless if the mesh is unsimplified.
		check( LODIndex < InBaseMesh->LODInfo.Num() );
		if ( InBaseMesh->LODInfo( LODIndex ).bHasBeenSimplified  )
		{
			continue;
		}

		// copy the wedge point indices - it makes easy to find
		if( BaseLODModel.RawPointIndices.GetBulkDataSize() )
		{
			NewWedgePointIndices.Empty( BaseLODModel.RawPointIndices.GetElementCount() );
			NewWedgePointIndices.Add( BaseLODModel.RawPointIndices.GetElementCount() );
			appMemcpy( NewWedgePointIndices.GetData(), BaseLODModel.RawPointIndices.Lock(LOCK_READ_ONLY), BaseLODModel.RawPointIndices.GetBulkDataSize() );
			BaseLODModel.RawPointIndices.Unlock();
		
			// Source Indices used : Save it so that you don't use it twice
			TArray<DWORD> SourceIndicesUsed;
			SourceIndicesUsed.Empty(MorphLODModel.Vertices.Num());

			// go through all vertices
			UBOOL bNumVerticesError = FALSE;
			for ( INT VertIdx=0; VertIdx<MorphLODModel.Vertices.Num(); ++VertIdx )
			{	
				// Get Old Base Vertex ID
				DWORD OldVertIdx = MorphLODModel.Vertices(VertIdx).SourceIdx;
				if ( !LODWedgePointIndices.IsValidIndex( OldVertIdx ) )
				{
					bNumVerticesError = TRUE;
					continue;
				}
				// find PointIndices from the old list
				DWORD BasePointIndex = LODWedgePointIndices(OldVertIdx);

				// Find the PointIndices from new list
				INT NewVertIdx = NewWedgePointIndices.FindItemIndex(BasePointIndex);
				// See if it's already used
				if ( SourceIndicesUsed.FindItemIndex(NewVertIdx) != INDEX_NONE )
				{
					// if used look for next right vertex index
					for ( INT Iter = NewVertIdx + 1; Iter<NewWedgePointIndices.Num(); ++Iter )
					{
						// found one
						if (NewWedgePointIndices(Iter) == BasePointIndex)
						{
							// see if used again
							if (SourceIndicesUsed.FindItemIndex(Iter) == INDEX_NONE)
							{
								// if not, this slot is available 
								// update new value
								MorphLODModel.Vertices(VertIdx).SourceIdx = Iter;
								SourceIndicesUsed.AddItem(Iter);									
								break;
							}
						}
					}
				}
				else
				{
					// update new value
					MorphLODModel.Vertices(VertIdx).SourceIdx = NewVertIdx;
					SourceIndicesUsed.AddItem(NewVertIdx);
				}
			}
			if ( bNumVerticesError )
			{
				debugf(TEXT("MorphLODModel (%d) references vertices higher than the vertex count at this BasedWedgePointIndices NumVertices (%d) - Mesh render may appear to have corrupt blend"), LODIndex, LODWedgePointIndices.Num() );
			}
			appQsort( &MorphLODModel.Vertices(0), MorphLODModel.Vertices.Num(), sizeof(FMorphTargetVertex), CompareMorphVertex );
		}
	}
}
/**
* Constructor. 
* Converts a skeletal mesh to raw vertex data
* needed for creating a morph target mesh
*
* @param	SrcMesh - source skeletal mesh to convert
* @param	LODIndex - level of detail to use for the geometry
*/
FMorphMeshRawSource::FMorphMeshRawSource( USkeletalMesh* SrcMesh, INT LODIndex ) :
	SourceMesh(SrcMesh)
{
    check(SrcMesh);
	check(SrcMesh->LODModels.IsValidIndex(LODIndex));

	// get the mesh data for the given lod
	FStaticLODModel& LODModel = SrcMesh->LODModels(LODIndex);

	// vertices are packed in this order iot stay consistent
	// with the indexing used by the FStaticLODModel vertex buffer
	//
	//	Chunk0
	//		Rigid0
	//		Rigid1
	//		Soft0
	//		Soft1
	//	Chunk1
	//		Rigid0
	//		Rigid1
	//		Soft0
	//		Soft1
    
	// iterate over the chunks for the skeletal mesh
	for( INT ChunkIdx=0; ChunkIdx < LODModel.Chunks.Num(); ChunkIdx++ )
	{
		// each chunk has both rigid and smooth vertices
		const FSkelMeshChunk& Chunk = LODModel.Chunks(ChunkIdx);
		// rigid vertices should always be added first so that we are
		// consistent with the way vertices are packed in the FStaticLODModel vertex buffer
		for( INT VertexIdx=0; VertexIdx < Chunk.RigidVertices.Num(); VertexIdx++ )
		{
			const FRigidSkinVertex& SourceVertex = Chunk.RigidVertices(VertexIdx);
			FMorphMeshVertexRaw RawVertex = 
			{
                SourceVertex.Position,
				SourceVertex.TangentX,
				SourceVertex.TangentY,
				SourceVertex.TangentZ
			};
			Vertices.AddItem( RawVertex );			
		}
		// smooth vertices are added next. The resulting Vertices[] array should
		// match the FStaticLODModel vertex buffer when indexing vertices
		for( INT VertexIdx=0; VertexIdx < Chunk.SoftVertices.Num(); VertexIdx++ )
		{
			const FSoftSkinVertex& SourceVertex = Chunk.SoftVertices(VertexIdx);
			FMorphMeshVertexRaw RawVertex = 
			{
                SourceVertex.Position,
				SourceVertex.TangentX,
				SourceVertex.TangentY,
				SourceVertex.TangentZ
			};
			Vertices.AddItem( RawVertex );			
		}		
	}

    // Copy the indices manually, since the LODModel's index buffer may have a different alignment.
	Indices.Empty(LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Num());
	for(INT Index = 0;Index < LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Num();Index++)
	{
		Indices.AddItem(LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Get(Index));
	}
    
	// copy the wedge point indices
	if( LODModel.RawPointIndices.GetBulkDataSize() )
	{
		WedgePointIndices.Empty( LODModel.RawPointIndices.GetElementCount() );
		WedgePointIndices.Add( LODModel.RawPointIndices.GetElementCount() );
		appMemcpy( WedgePointIndices.GetData(), LODModel.RawPointIndices.Lock(LOCK_READ_ONLY), LODModel.RawPointIndices.GetBulkDataSize() );
		LODModel.RawPointIndices.Unlock();
	}
}

/**
* Constructor. 
* Converts a static mesh to raw vertex data
* needed for creating a morph target mesh
*
* @param	SrcMesh - source static mesh to convert
* @param	LODIndex - level of detail to use for the geometry
*/
FMorphMeshRawSource::FMorphMeshRawSource( UStaticMesh* SrcMesh, INT LODIndex ) :
	SourceMesh(SrcMesh)
{
	// @todo - not implemented
	// not sure if we will support static mesh morphing yet
}

/**
* Return true if current vertex data can be morphed to the target vertex data
* 
*/
UBOOL FMorphMeshRawSource::IsValidTarget( const FMorphMeshRawSource& Target ) const
{
	//@todo sz -
	// heuristic is to check for the same number of original points
	//return( WedgePointIndices.Num() == Target.WedgePointIndices.Num() );
	return TRUE;
}

