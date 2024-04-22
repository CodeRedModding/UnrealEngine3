/*=============================================================================
UnSkeletalMeshSorting.cpp: Static sorting for skeletal mesh triangles
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

#if WITH_EDITOR

#if _WIN64
#pragma pack (push,16)
#endif
#include "../../../External/nvTriStrip/Inc/NvTriStrip.h"
#if _WIN64
#pragma pack (pop)
#endif

/**
* Use NVTriStrip to reorder the indices within a connected strip for cache efficiency
*
* @parm Indices - pointer to the index data
* @parm NumIndices - number of indices to optimize
*/
void CacheOptimizeSortStrip(DWORD* Indices, INT NumIndices)
{
	PrimitiveGroup*	PrimitiveGroups = NULL;
	UINT			NumPrimitiveGroups = 0;
	SetListsOnly(true);
	GenerateStrips((UINT*)Indices,NumIndices,&PrimitiveGroups,&NumPrimitiveGroups);
	// We should have the same number of triangle as we started
	check( NumIndices == PrimitiveGroups->numIndices );
	appMemcpy(Indices, PrimitiveGroups->indices, NumIndices * sizeof(DWORD));
	delete [] PrimitiveGroups;
}

// struct to hold a strip, for sorting by key.
struct FTriStripSortInfo
{
	TArray<INT> Triangles;
	FLOAT SortKey;
};

IMPLEMENT_COMPARE_CONSTREF(FTriStripSortInfo, UnSkeletalMeshSorting, { return A.SortKey > B.SortKey ? 1 : -1; } );

/**
* Group triangles into connected strips 
*
* @param NumTriangles - The number of triangles to group
* @param Indices - pointer to the index buffer data
* @outparam OutTriSet - an array containing the set number for each triangle.
* @return the maximum set number of any triangle.
*/
INT GetConnectedTriangleSets( INT NumTriangles, const DWORD* Indices, TArray<UINT>& OutTriSet )
{
	// 1. find connected strips of triangles
	union EdgeInfo
	{		
		DWORD Indices[2];
		QWORD QWData;
		EdgeInfo( QWORD InQWData )
			:	QWData(InQWData)
		{}
		EdgeInfo( DWORD Index1, DWORD Index2 )
		{
			Indices[0] = Min<DWORD>(Index1,Index2);
			Indices[1] = Max<DWORD>(Index1,Index2);
		}
	};

	// Map from edge to first triangle using that edge
	TMap<QWORD, INT> EdgeTriMap;
	INT MaxTriSet = 0;
	for( INT TriIndex=0;TriIndex<NumTriangles;TriIndex++ )
	{
		DWORD i1 = Indices[TriIndex*3+0];
		DWORD i2 = Indices[TriIndex*3+1];
		DWORD i3 = Indices[TriIndex*3+2];

		QWORD Edge1 = EdgeInfo(i1,i2).QWData;
		QWORD Edge2 = EdgeInfo(i2,i3).QWData;
		QWORD Edge3 = EdgeInfo(i3,i1).QWData;

		INT* Edge1TriPtr = EdgeTriMap.Find(Edge1);
		INT* Edge2TriPtr = EdgeTriMap.Find(Edge2);
		INT* Edge3TriPtr = EdgeTriMap.Find(Edge3);

		if( !Edge1TriPtr && !Edge2TriPtr && !Edge3TriPtr )
		{
			OutTriSet.AddItem(MaxTriSet++);
			// none of the edges have been found before.
			EdgeTriMap.Set(Edge1, TriIndex);
			EdgeTriMap.Set(Edge2, TriIndex);
			EdgeTriMap.Set(Edge3, TriIndex);
		}
		else
		if( Edge1TriPtr && !Edge2TriPtr && !Edge3TriPtr )
		{
			// triangle belongs to triangle Edge1's set.
			INT Edge1Tri = *Edge1TriPtr;
			INT Edge1TriSet = OutTriSet(Edge1Tri);
			OutTriSet.AddItem(Edge1TriSet);
			EdgeTriMap.Set(Edge2, Edge1Tri);
			EdgeTriMap.Set(Edge3, Edge1Tri);
		}
		else
		if( Edge2TriPtr && !Edge1TriPtr && !Edge3TriPtr )
		{
			// triangle belongs to triangle Edge2's set.
			INT Edge2Tri = *Edge2TriPtr;
			INT Edge2TriSet = OutTriSet(Edge2Tri);
			OutTriSet.AddItem(Edge2TriSet);
			EdgeTriMap.Set(Edge1, Edge2Tri);
			EdgeTriMap.Set(Edge3, Edge2Tri);
		}
		else
		if( Edge3TriPtr && !Edge1TriPtr && !Edge2TriPtr )
		{
			// triangle belongs to triangle Edge3's set.
			INT Edge3Tri = *Edge3TriPtr;
			INT Edge3TriSet = OutTriSet(Edge3Tri);
			OutTriSet.AddItem(Edge3TriSet);
			EdgeTriMap.Set(Edge1, Edge3Tri);
			EdgeTriMap.Set(Edge2, Edge3Tri);
		}
		else
		if( Edge1TriPtr && Edge2TriPtr && !Edge3TriPtr )
		{
			INT Edge1TriSet = OutTriSet(*Edge1TriPtr);
			INT Edge2TriSet = OutTriSet(*Edge2TriPtr);
			OutTriSet.AddItem(Edge1TriSet);
			EdgeTriMap.Set(Edge3, *Edge1TriPtr);

			// triangle belongs to triangle Edge1 and Edge2's set.
			if( Edge1TriSet != Edge2TriSet )
			{
				// merge sets for Edge1 and Edge2
				for( INT TriSetIndex=0;TriSetIndex<OutTriSet.Num();TriSetIndex++ )
				{
					if( OutTriSet(TriSetIndex)==Edge2TriSet )
					{
						OutTriSet(TriSetIndex)=Edge1TriSet;
					}
				}
			}
		}
		else
		if( Edge1TriPtr && Edge3TriPtr && !Edge2TriPtr )
		{
			INT Edge1TriSet = OutTriSet(*Edge1TriPtr);
			INT Edge3TriSet = OutTriSet(*Edge3TriPtr);
			OutTriSet.AddItem(Edge1TriSet);
			EdgeTriMap.Set(Edge2, *Edge1TriPtr);

			// triangle belongs to triangle Edge1 and Edge3's set.
			if( Edge1TriSet != Edge3TriSet )
			{
				// merge sets for Edge1 and Edge3
				for( INT TriSetIndex=0;TriSetIndex<OutTriSet.Num();TriSetIndex++ )
				{
					if( OutTriSet(TriSetIndex)==Edge3TriSet )
					{
						OutTriSet(TriSetIndex)=Edge1TriSet;
					}
				}
			}
		}
		else
		if( Edge2TriPtr && Edge3TriPtr && !Edge1TriPtr )
		{
			INT Edge2TriSet = OutTriSet(*Edge2TriPtr);
			INT Edge3TriSet = OutTriSet(*Edge3TriPtr);
			OutTriSet.AddItem(Edge2TriSet);
			EdgeTriMap.Set(Edge1, *Edge2TriPtr);

			// triangle belongs to triangle Edge2 and Edge3's set.
			if( Edge2TriSet != Edge3TriSet )
			{
				// merge sets for Edge2 and Edge3
				for( INT TriSetIndex=0;TriSetIndex<OutTriSet.Num();TriSetIndex++ )
				{
					if( OutTriSet(TriSetIndex)==Edge3TriSet )
					{
						OutTriSet(TriSetIndex)=Edge2TriSet;
					}
				}
			}
		}
		else
		if( Edge1TriPtr && Edge2TriPtr && Edge3TriPtr )
		{
			INT Edge1TriSet = OutTriSet(*Edge1TriPtr);
			INT Edge2TriSet = OutTriSet(*Edge2TriPtr);
			INT Edge3TriSet = OutTriSet(*Edge3TriPtr);

			// triangle belongs to triangle Edge1, Edge2 and Edge3's set.
			if( Edge1TriSet != Edge2TriSet )
			{
				// merge sets for Edge1 and Edge2
				for( INT TriSetIndex=0;TriSetIndex<OutTriSet.Num();TriSetIndex++ )
				{
					if( OutTriSet(TriSetIndex)==Edge2TriSet )
					{
						OutTriSet(TriSetIndex)=Edge1TriSet;
					}
				}
			}
			if( Edge1TriSet != Edge3TriSet )
			{
				// merge sets for Edge1 and Edge3
				for( INT TriSetIndex=0;TriSetIndex<OutTriSet.Num();TriSetIndex++ )
				{
					if( OutTriSet(TriSetIndex)==Edge3TriSet )
					{
						OutTriSet(TriSetIndex)=Edge1TriSet;
					}
				}
			}
			OutTriSet.AddItem(Edge1TriSet);
		}
	}

	return MaxTriSet;
}

/**
* Remove all sorting and reset the index buffer order to that returned by NVTriStrip.
*
* @param SkelMesh - Skeletal mesh
* @param NumTriangles - Number of triangles
* @param Vertices - pointer to the skeletal mesh's FSoftSkinVertex array.
* @param Indices - pointer to the index buffer data to be reordered
*/
void SortTriangles_None( USkeletalMesh* SkelMesh, INT NumTriangles, const FSoftSkinVertex* Vertices, DWORD* Indices )
{
	CacheOptimizeSortStrip( Indices, NumTriangles*3 );
}

/**
* Sort connected strips by distance from the center of each strip to the center of all vertices.
*
* @param SkelMesh - Skeletal mesh
* @param NumTriangles - Number of triangles
* @param Vertices - pointer to the skeletal mesh's FSoftSkinVertex array.
* @param Indices - pointer to the index buffer data to be reordered
*/
void SortTriangles_CenterRadialDistance( USkeletalMesh* SkelMesh, INT NumTriangles, const FSoftSkinVertex* Vertices, DWORD* Indices )
{
	FVector SortCenter(0,0,0);
	UBOOL FoundCenter = FALSE;

	USkeletalMeshSocket* Socket = SkelMesh->FindSocket( FName(TEXT("SortCenter")) );
	if( Socket )
	{
		INT BoneIndex = SkelMesh->MatchRefBone(Socket->BoneName);
		if( BoneIndex >= 0 && BoneIndex < SkelMesh->RefSkeleton.Num() )
		{
			FoundCenter = TRUE;
			SortCenter = SkelMesh->RefSkeleton(BoneIndex).BonePos.Position + Socket->RelativeLocation;
		}
	}

	if( !FoundCenter )
	{
		// find average location of entire model and use that as the sorting center
		TSet<FVector> AllVertsSet;
		INT AllVertsCount=0;

		for( INT TriIndex=0;TriIndex<NumTriangles;TriIndex++ )
		{
			DWORD i1 = Indices[TriIndex*3+0];
			DWORD i2 = Indices[TriIndex*3+1];
			DWORD i3 = Indices[TriIndex*3+2];

			UBOOL bDuplicate=FALSE;
			AllVertsSet.Add(Vertices[i1].Position,&bDuplicate);
			if( !bDuplicate )
			{
				SortCenter += Vertices[i1].Position;
				AllVertsCount++;
			}
			AllVertsSet.Add(Vertices[i2].Position,&bDuplicate);
			if( !bDuplicate )
			{
				SortCenter += Vertices[i2].Position;
				AllVertsCount++;
			}
			AllVertsSet.Add(Vertices[i3].Position,&bDuplicate);
			if( !bDuplicate )
			{
				SortCenter += Vertices[i3].Position;
				AllVertsCount++;
			}
		}

		// Calc center of all verts.
		SortCenter /= AllVertsCount;
	}

	// Get the set number for each triangle
	TArray<UINT> TriSet;
	INT MaxTriSet = GetConnectedTriangleSets( NumTriangles, Indices, TriSet );

	TArray<FTriStripSortInfo> Strips;
	Strips.AddZeroed(MaxTriSet);
	for( INT TriIndex=0;TriIndex<TriSet.Num();TriIndex++ )
	{
		Strips(TriSet(TriIndex)).Triangles.AddItem(TriIndex);
	}

	for( INT s=0;s<Strips.Num();s++ )
	{
		if( Strips(s).Triangles.Num() == 0 )
		{
			Strips.Remove(s);
			s--;
			continue;
		}
		
		FVector StripCenter(0,0,0);
		for( int TriIndex=0;TriIndex<Strips(s).Triangles.Num();TriIndex++ )
		{
			INT tri = Strips(s).Triangles(TriIndex);
			DWORD i1 = Indices[tri*3+0];
			DWORD i2 = Indices[tri*3+1];
			DWORD i3 = Indices[tri*3+2];
			FVector TriCenter = (Vertices[i1].Position + Vertices[i2].Position + Vertices[i3].Position) / 3.f;
			StripCenter += TriCenter;
		}

		StripCenter /= Strips(s).Triangles.Num();
		Strips(s).SortKey = (StripCenter-SortCenter).SizeSquared();
	}
	Sort<USE_COMPARE_CONSTREF(FTriStripSortInfo, UnSkeletalMeshSorting)>(&Strips(0),Strips.Num());

	// export new draw order
	TArray<DWORD> NewIndices;
	NewIndices.Empty(NumTriangles*3);
	for( INT s=0;s<Strips.Num();s++ )
	{
		INT StripStartIndex = NewIndices.Num();
		for( int TriIndex=0;TriIndex<Strips(s).Triangles.Num();TriIndex++ )
		{
			INT tri = Strips(s).Triangles(TriIndex);
			NewIndices.AddItem(Indices[tri*3+0]);
			NewIndices.AddItem(Indices[tri*3+1]);
			NewIndices.AddItem(Indices[tri*3+2]);	
		}
		CacheOptimizeSortStrip( &NewIndices(StripStartIndex), Strips(s).Triangles.Num()*3 );
	}
	appMemcpy( Indices, &NewIndices(0), NewIndices.Num() * sizeof(DWORD) );
}


/**
* Sort triangles randomly (for testing)
*
* @param SkelMesh - Skeletal mesh
* @param NumTriangles - Number of triangles
* @param Vertices - pointer to the skeletal mesh's FSoftSkinVertex array.
* @param Indices - pointer to the index buffer data to be reordered
*/
void SortTriangles_Random( INT NumTriangles, const FSoftSkinVertex* Vertices, DWORD* Indices )
{
	TArray<INT> Triangles;
	for( INT i=0;i<NumTriangles;i++ )
	{
		Triangles.InsertItem(i, i > 0 ? appRand() % i : 0);
	}

	// export new draw order
	TArray<DWORD> NewIndices;
	NewIndices.Empty(NumTriangles*3);
	for( int TriIndex=0;TriIndex<NumTriangles;TriIndex++ )
	{
		INT tri = Triangles(TriIndex);
		NewIndices.AddItem(Indices[tri*3+0]);
		NewIndices.AddItem(Indices[tri*3+1]);
		NewIndices.AddItem(Indices[tri*3+2]);	
	}

	appMemcpy( Indices, &NewIndices(0), NewIndices.Num() * sizeof(DWORD) );
}


/**
* Sort triangles by merging contiguous strips but otherwise retain the draw order
*
* @param SkelMesh - Skeletal mesh
* @param NumTriangles - Number of triangles
* @param Vertices - pointer to the skeletal mesh's FSoftSkinVertex array.
* @param Indices - pointer to the index buffer data to be reordered
*/
void SortTriangles_MergeContiguous( INT NumTriangles, INT NumVertices, const FSoftSkinVertex* Vertices, DWORD* Indices )
{
	// Build the list of triangle sets
	TArray<UINT> TriSet;
	GetConnectedTriangleSets( NumTriangles, Indices, TriSet );

	// Mapping from triangle set number to the array of indices that make up the contiguous strip.
	TMap<UINT, TArray<DWORD> > Strips;

	INT Index=0;
	for( INT s=0;s<TriSet.Num();s++ )
	{
		// Store the indices for this triangle in the appropriate contiguous set.
		TArray<DWORD>* ThisStrip = Strips.Find(TriSet(s));
		if( !ThisStrip )
		{
			ThisStrip = &Strips.Set(TriSet(s),TArray<DWORD>());
		}

		// Add the three indices for this triangle.
		ThisStrip->AddItem(Indices[Index++]);
		ThisStrip->AddItem(Indices[Index++]);
		ThisStrip->AddItem(Indices[Index++]);
	}

	// Export the indices in the same order.
	Index = 0;
	INT PrevSet = INDEX_NONE;
	for( INT s=0;s<TriSet.Num();s++ )
	{
		// The first time we see a triangle in a new set, export all the indices from that set.
		if( TriSet(s) != PrevSet )
		{
			TArray<DWORD>* ThisStrip = Strips.Find(TriSet(s));
			check(ThisStrip);

			if( ThisStrip->Num() > 0 )
			{
				check(Index < NumTriangles*3);
				appMemcpy( &Indices[Index], &(*ThisStrip)(0), ThisStrip->Num() * sizeof(DWORD) );
				Index += ThisStrip->Num();

				// We want to export the whole strip contiguously, so we empty it so we don't export the
				// indices again when we see the same TriSet later.
				ThisStrip->Empty();
			}
		}
		PrevSet = TriSet(s);
	}
	check(Index == NumTriangles*3);
}


#endif