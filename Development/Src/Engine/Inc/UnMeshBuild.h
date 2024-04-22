/*=============================================================================
	UnMeshBuild.h: Contains commonly used functions and classes for building
	mesh data into engine usable form.

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_UNMESHBUILDER
#define _INC_UNMESHBUILDER

/**
 * This helper class builds the edge list for a mesh. It uses a hash of vertex
 * positions to edges sharing that vertex to remove the n^2 searching of all
 * previously added edges. This class is templatized so it can be used with
 * either static mesh or skeletal mesh vertices
 */
template <class VertexClass> class TEdgeBuilder
{
protected:
	/**
	 * The list of indices to build the edge data from
	 */
	const TArray<WORD>& Indices;
	/**
	 * The array of verts for vertex position comparison
	 */
	const TArray<VertexClass>& Vertices;
	/**
	 * The array of edges to create
	 */
	TArray<FMeshEdge>& Edges;
	/**
	 * List of edges that start with a given vertex
	 */
	TMultiMap<FVector,FMeshEdge*> VertexToEdgeList;

	/**
	 * This function determines whether a given edge matches or not. It must be
	 * provided by derived classes since they have the specific information that
	 * this class doesn't know about (vertex info, influences, etc)
	 *
	 * @param Index1 The first index of the edge being checked
	 * @param Index2 The second index of the edge
	 * @param OtherEdge The edge to compare. Was found via the map
	 *
	 * @return TRUE if the edge is a match, FALSE otherwise
	 */
	virtual UBOOL DoesEdgeMatch(INT Index1,INT Index2,FMeshEdge* OtherEdge) = 0;

	/**
	 * Searches the list of edges to see if this one matches an existing and
	 * returns a pointer to it if it does
	 *
	 * @param Index1 the first index to check for
	 * @param Index2 the second index to check for
	 *
	 * @return NULL if no edge was found, otherwise the edge that was found
	 */
	inline FMeshEdge* FindOppositeEdge(INT Index1,INT Index2)
	{
		FMeshEdge* Edge = NULL;
		TArray<FMeshEdge*> EdgeList;
		// Search the hash for a corresponding vertex
		VertexToEdgeList.MultiFind(Vertices(Index2).Position,EdgeList);
		// Now search through the array for a match or not
		for (INT EdgeIndex = 0; EdgeIndex < EdgeList.Num() && Edge == NULL;
			EdgeIndex++)
		{
			FMeshEdge* OtherEdge = EdgeList(EdgeIndex);
			// See if this edge matches the passed in edge
			if (OtherEdge != NULL && DoesEdgeMatch(Index1,Index2,OtherEdge))
			{
				// We have a match
				Edge = OtherEdge;
			}
		}
		return Edge;
	}

	/**
	 * Updates an existing edge if found or adds the new edge to the list
	 *
	 * @param Index1 the first index in the edge
	 * @param Index2 the second index in the edge
	 * @param Triangle the triangle that this edge was found in
	 */
	inline void AddEdge(INT Index1,INT Index2,INT Triangle)
	{
		// If this edge matches another then just fill the other triangle
		// otherwise add it
		FMeshEdge* OtherEdge = FindOppositeEdge(Index1,Index2);
		if (OtherEdge == NULL)
		{
			// Add a new edge to the array
			INT EdgeIndex = Edges.AddZeroed();
			Edges(EdgeIndex).Vertices[0] = Index1;
			Edges(EdgeIndex).Vertices[1] = Index2;
			Edges(EdgeIndex).Faces[0] = Triangle;
			Edges(EdgeIndex).Faces[1] = -1;
			// Also add this edge to the hash for faster searches
			// NOTE: This relies on the array never being realloced!
			VertexToEdgeList.Add(Vertices(Index1).Position,&Edges(EdgeIndex));
		}
		else
		{
			OtherEdge->Faces[1] = Triangle;
		}
	}

public:
	/**
	 * Initializes the values for the code that will build the mesh edge list
	 */
	TEdgeBuilder(const TArray<WORD>& InIndices,
		const TArray<VertexClass>& InVertices,
		TArray<FMeshEdge>& OutEdges) :
		Indices(InIndices), Vertices(InVertices), Edges(OutEdges)
	{
		// Presize the array so that there are no extra copies being done
		// when adding edges to it
		Edges.Empty(Indices.Num());
	}

	/**
	* Virtual dtor
	*/
	virtual ~TEdgeBuilder(){}


	/**
	 * Uses a hash of indices to edge lists so that it can avoid the n^2 search
	 * through the full edge list
	 */
	void FindEdges(void)
	{
		// @todo Handle something other than trilists when building edges
		INT TriangleCount = Indices.Num() / 3;
		INT EdgeCount = 0;
		// Work through all triangles building the edges
		for (INT Triangle = 0; Triangle < TriangleCount; Triangle++)
		{
			// Determine the starting index
			INT TriangleIndex = Triangle * 3;
			// Get the indices for the triangle
			INT Index1 = Indices(TriangleIndex);
			INT Index2 = Indices(TriangleIndex + 1);
			INT Index3 = Indices(TriangleIndex + 2);
			// Add the first to second edge
			AddEdge(Index1,Index2,Triangle);
			// Now add the second to third
			AddEdge(Index2,Index3,Triangle);
			// Add the third to first edge
			AddEdge(Index3,Index1,Triangle);
		}
	}
};

/**
 * This is the static mesh specific version for finding edges
 */
class FStaticMeshEdgeBuilder :
	public TEdgeBuilder<FStaticMeshBuildVertex>
{
public:
	/**
	 * Constructor that passes all work to the parent class
	 */
	FStaticMeshEdgeBuilder(const TArray<WORD>& InIndices,
		const TArray<FStaticMeshBuildVertex>& InVertices,
		TArray<FMeshEdge>& OutEdges) :
		TEdgeBuilder<FStaticMeshBuildVertex>(InIndices,InVertices,OutEdges)
	{
	}

	/**
	 * This function determines whether a given edge matches or not for a static mesh
	 *
	 * @param Index1 The first index of the edge being checked
	 * @param Index2 The second index of the edge
	 * @param OtherEdge The edge to compare. Was found via the map
	 *
	 * @return TRUE if the edge is a match, FALSE otherwise
	 */
	UBOOL DoesEdgeMatch(INT Index1,INT Index2,FMeshEdge* OtherEdge)
	{
		return Vertices(OtherEdge->Vertices[1]).Position == Vertices(Index1).Position &&
			OtherEdge->Faces[1] == -1;
	}
};

/**
 * This is the skeletal mesh specific version for finding edges
 */
class FSkeletalMeshEdgeBuilder :
	public TEdgeBuilder<FSoftSkinVertex>
{
public:
	/**
	 * Constructor that passes all work to the parent class
	 */
	FSkeletalMeshEdgeBuilder(const TArray<WORD>& InIndices,
		const TArray<FSoftSkinVertex>& InVertices,
		TArray<FMeshEdge>& OutEdges) :
		TEdgeBuilder<FSoftSkinVertex>(InIndices,InVertices,OutEdges)
	{
	}

	/**
	 * This function determines whether a given edge matches or not for a skeletal mesh
	 *
	 * @param EdgeIndex1 The first index of the edge being checked
	 * @param EdgeIndex2 The second index of the edge
	 * @param OtherEdge The edge to compare. Was found via the map
	 *
	 * @return TRUE if the edge is a match, FALSE otherwise
	 */
	UBOOL DoesEdgeMatch(INT EdgeIndex1,INT EdgeIndex2,FMeshEdge* OtherEdge)
	{
		UBOOL DoesMatch = FALSE;
		// Don't bother checking influences if these aren't true
		if (Vertices(OtherEdge->Vertices[1]).Position == Vertices(EdgeIndex1).Position &&
			OtherEdge->Faces[1] == -1)
		{
			INT Index = 0;
            UBOOL DoAllInfluencesMatch = TRUE;
			// Verify that all the bone influences/weights match
			while (Index < 4 && DoAllInfluencesMatch == TRUE)
			{
				DoAllInfluencesMatch =
					Vertices(OtherEdge->Vertices[0]).InfluenceBones[Index] == Vertices(EdgeIndex2).InfluenceBones[Index] &&
					Vertices(OtherEdge->Vertices[0]).InfluenceWeights[Index] == Vertices(EdgeIndex2).InfluenceWeights[Index] &&
					Vertices(OtherEdge->Vertices[1]).InfluenceBones[Index] == Vertices(EdgeIndex1).InfluenceBones[Index] &&
					Vertices(OtherEdge->Vertices[1]).InfluenceWeights[Index] == Vertices(EdgeIndex1).InfluenceWeights[Index];
				Index++;
			}
			// Copy over whether or not they matched
			DoesMatch = DoAllInfluencesMatch;
		}
		return DoesMatch;
	}
};

/**
 * Returns TRUE if the specified points are about equal
 */
inline UBOOL PointsEqual(const FVector& V1,const FVector& V2, UBOOL bUseEpsilonCompare = TRUE )
{
	const FLOAT Epsilon = bUseEpsilonCompare ? THRESH_POINTS_ARE_SAME * 4.0f : 0.0f;	// Extra 4.0 multiplier (legacy?)
	if( Abs(V1.X - V2.X) > Epsilon || Abs(V1.Y - V2.Y) > Epsilon || Abs(V1.Z - V2.Z) > Epsilon )
	{
		return FALSE;
	}

	return TRUE;
}


/**
 * Returns TRUE if the specified normal vectors are about equal
 */
inline UBOOL NormalsEqual(const FVector& V1,const FVector& V2)
{
	const FLOAT Epsilon = THRESH_NORMALS_ARE_SAME * 4.0f;	// Extra 4.0 multiplier (legacy?)
	if( Abs(V1.X - V2.X) > Epsilon || Abs(V1.Y - V2.Y) > Epsilon || Abs(V1.Z - V2.Z) > Epsilon )
	{
		return FALSE;
	}

	return TRUE;
}

#endif
