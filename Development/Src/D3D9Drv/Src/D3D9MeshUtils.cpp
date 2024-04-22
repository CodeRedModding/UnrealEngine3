/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "D3D9DrvPrivate.h"

#if !UE3_LEAN_AND_MEAN

#include "D3D9MeshUtils.h"

extern IDirect3DDevice9* GLegacyDirect3DDevice9;

namespace D3D9MeshUtilities
{
	#define THRESH_UVS_ARE_SAME (1.0f / 1024.0f)

	// Temporary vertex for utility class
	struct FUtilVertex
	{
		FVector   Position;
		FVector2D UVs[4];
		FColor	  Color;
		DWORD    SmoothingMask;
		INT FragmentIndex;
		UBOOL bOverrideTangentBasis;
		FVector TangentX;
		FVector TangentY;
		FVector TangentZ;
	};

	/**
	 * ConstructD3DVertexElement - acts as a constructor for D3DVERTEXELEMENT9
	 */
	static D3DVERTEXELEMENT9 ConstructD3DVertexElement(WORD Stream, WORD Offset, BYTE Type, BYTE Method, BYTE Usage, BYTE UsageIndex)
	{
		D3DVERTEXELEMENT9 newElement;
		newElement.Stream = Stream;
		newElement.Offset = Offset;
		newElement.Type = Type;
		newElement.Method = Method;
		newElement.Usage = Usage;
		newElement.UsageIndex = UsageIndex;
		return newElement;
	}

	/** Creates a vertex element list for the D3D9MeshUtils vertex format. */
	static void GetD3D9MeshVertexDeclarations(TArray<D3DVERTEXELEMENT9>& OutVertexElements)
	{
		OutVertexElements.Push(ConstructD3DVertexElement(0, STRUCT_OFFSET(FUtilVertex,Position), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0));
		OutVertexElements.Push(ConstructD3DVertexElement(0, STRUCT_OFFSET(FUtilVertex,UVs[0]), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0));
		OutVertexElements.Push(ConstructD3DVertexElement(0, STRUCT_OFFSET(FUtilVertex,UVs[1]), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1));
		OutVertexElements.Push(ConstructD3DVertexElement(0, STRUCT_OFFSET(FUtilVertex,UVs[2]), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2));
		OutVertexElements.Push(ConstructD3DVertexElement(0, STRUCT_OFFSET(FUtilVertex,UVs[3]), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 3));
		OutVertexElements.Push(ConstructD3DVertexElement(0, STRUCT_OFFSET(FUtilVertex,Color), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0));
		OutVertexElements.Push(ConstructD3DVertexElement(0, STRUCT_OFFSET(FUtilVertex,SmoothingMask), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 1));
		OutVertexElements.Push(ConstructD3DVertexElement(0, STRUCT_OFFSET(FUtilVertex,FragmentIndex), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 2));
		OutVertexElements.Push(ConstructD3DVertexElement(0, STRUCT_OFFSET(FUtilVertex,bOverrideTangentBasis), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 1));
		OutVertexElements.Push(ConstructD3DVertexElement(0, STRUCT_OFFSET(FUtilVertex,TangentX), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0));
		OutVertexElements.Push(ConstructD3DVertexElement(0, STRUCT_OFFSET(FUtilVertex,TangentY), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BINORMAL, 0));
		OutVertexElements.Push(ConstructD3DVertexElement(0, STRUCT_OFFSET(FUtilVertex,TangentZ), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0));
		OutVertexElements.Push(ConstructD3DVertexElement(0xFF,0,D3DDECLTYPE_UNUSED,0,0,0));
	}

	/**
	 * Creates a D3DXMESH from a FStaticMeshRenderData
	 * @param Triangles The triangles to create the mesh from.
	 * @param bRemoveDegenerateTriangles True if degenerate triangles should be removed
	 * @param OutD3DMesh Mesh to create
	 * @return Boolean representing success or failure
	*/
	UBOOL ConvertStaticMeshTrianglesToD3DXMesh(
		const TArray<FStaticMeshTriangle>& Triangles,
		const UBOOL bRemoveDegenerateTriangles,	
		TRefCountPtr<ID3DXMesh>& OutD3DMesh
		)
	{
		TArray<D3DVERTEXELEMENT9> VertexElements;
		GetD3D9MeshVertexDeclarations(VertexElements);

		TArray<FUtilVertex> Vertices;
		TArray<WORD> Indices;
		TArray<DWORD> Attributes;
		for(INT TriangleIndex = 0;TriangleIndex < Triangles.Num();TriangleIndex++)
		{
			const FStaticMeshTriangle& Tri = Triangles(TriangleIndex);

			UBOOL bTriangleIsDegenerate = FALSE;

			if( bRemoveDegenerateTriangles )
			{
				// Detect if the triangle is degenerate.
				for(INT EdgeIndex = 0;EdgeIndex < 3;EdgeIndex++)
				{
					const INT I0 = EdgeIndex;
					const INT I1 = (EdgeIndex + 1) % 3;
					if((Tri.Vertices[I0] - Tri.Vertices[I1]).IsNearlyZero(THRESH_POINTS_ARE_SAME * 4.0f))
					{
						bTriangleIsDegenerate = TRUE;
						break;
					}
				}
			}

			if(!bTriangleIsDegenerate)
			{
				Attributes.AddItem(Tri.MaterialIndex);
				for(INT J=0;J<3;J++)
				{
					FUtilVertex* Vertex = new(Vertices) FUtilVertex;
					appMemzero(Vertex,sizeof(FUtilVertex));
					Vertex->Position = Tri.Vertices[J];
					Vertex->Color = Tri.Colors[J];
					//store the smoothing mask per vertex since there is only one per-face attribute that is already being used (materialIndex)
					Vertex->SmoothingMask = Tri.SmoothingMask;
					Vertex->FragmentIndex = Tri.FragmentIndex;
					Vertex->bOverrideTangentBasis = Tri.bOverrideTangentBasis;
					Vertex->TangentX = Tri.TangentX[J];
					Vertex->TangentY = Tri.TangentY[J];
					Vertex->TangentZ = Tri.TangentZ[J];
					const INT NumUVs = Min<INT>(Tri.NumUVs,ARRAY_COUNT(Vertex->UVs));
					for(INT UVIndex = 0; UVIndex < NumUVs; UVIndex++)
					{
						Vertex->UVs[UVIndex] = Tri.UVs[J][UVIndex];
					}
	 
					Indices.AddItem(Vertices.Num() - 1);
				}
			}
		}

		// This code uses the raw triangles. Needs welding, etc.
		const INT NumFaces = Indices.Num() / 3;
		const INT NumVertices = NumFaces*3;

		check(Attributes.Num() == NumFaces);
		check(NumFaces * 3 == Indices.Num());

		// Create mesh for source data
		if (FAILED(D3DXCreateMesh(NumFaces,NumVertices,D3DXMESH_SYSTEMMEM,(D3DVERTEXELEMENT9 *)VertexElements.GetData(),GLegacyDirect3DDevice9,OutD3DMesh.GetInitReference()) ) )
		{
			warnf(TEXT("D3DXCreateMesh() Failed!"));
			return FALSE;
		}

		// Fill D3DMesh mesh
		FUtilVertex* D3DVertices;
		WORD*		 D3DIndices;
		DWORD*		 D3DAttributes;
		OutD3DMesh->LockVertexBuffer(0,(LPVOID*)&D3DVertices);
		OutD3DMesh->LockIndexBuffer(0,(LPVOID*)&D3DIndices);
		OutD3DMesh->LockAttributeBuffer(0, &D3DAttributes);

		appMemcpy(D3DVertices,&Vertices(0),Vertices.Num() * sizeof(FUtilVertex));
		appMemcpy(D3DIndices,&Indices(0),Indices.Num() * sizeof(WORD));
		appMemcpy(D3DAttributes,&Attributes(0),Attributes.Num() * sizeof(DWORD));

		OutD3DMesh->UnlockIndexBuffer();
		OutD3DMesh->UnlockVertexBuffer();
		OutD3DMesh->UnlockAttributeBuffer();

		return TRUE;
	}

	/**
	 * Creates a FStaticMeshRenderData from a D3DXMesh
	 * @param DestMesh Destination mesh to extract to
	 * @param NumUVs Number of UVs
	 * @param Elements Elements array
	 * @return Boolean representing success or failure
	 */
	UBOOL ConvertD3DXMeshToStaticMesh(
		TRefCountPtr<ID3DXMesh>& D3DMesh, 									  
		FStaticMeshRenderData& DestMesh, 				  
		INT NumUVs, 
		TArray<FStaticMeshElement>& Elements, 
		UBOOL bClearDestElements
		)
	{

		//DestMesh.RawTriangles.RemoveBulkData();
		//DestMesh.Elements.Empty();

		// Extract simplified data to LOD
		FUtilVertex* D3DVertices;
		WORD*		 D3DIndices;
		DWORD*		 D3DAttributes;
		D3DMesh->LockVertexBuffer(D3DLOCK_READONLY,(LPVOID*)&D3DVertices);
		D3DMesh->LockIndexBuffer(D3DLOCK_READONLY,(LPVOID*)&D3DIndices);
		D3DMesh->LockAttributeBuffer(D3DLOCK_READONLY,&D3DAttributes);

		DestMesh.RawTriangles.Lock(LOCK_READ_WRITE);
		FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*) DestMesh.RawTriangles.Realloc( D3DMesh->GetNumFaces() );

		for(UINT I=0;I<D3DMesh->GetNumFaces();I++)
		{
			FStaticMeshTriangle& Tri = RawTriangleData[I];
			// Copy smoothing mask and index from any vertex into this triangle
			Tri.SmoothingMask = D3DVertices[D3DIndices[I*3+0]].SmoothingMask;
			Tri.MaterialIndex = D3DAttributes[I];
			Tri.FragmentIndex = D3DVertices[D3DIndices[I*3+0]].FragmentIndex;
			Tri.bOverrideTangentBasis = D3DVertices[D3DIndices[I*3+0]].bOverrideTangentBasis;

			Tri.NumUVs = NumUVs;
			for(int UVs=0;UVs<Tri.NumUVs;UVs++)
			{
				Tri.UVs[0][UVs] = D3DVertices[D3DIndices[I*3+0]].UVs[UVs];
				Tri.UVs[1][UVs] = D3DVertices[D3DIndices[I*3+1]].UVs[UVs];
				Tri.UVs[2][UVs] = D3DVertices[D3DIndices[I*3+2]].UVs[UVs];
			}

			for(INT K=0;K<3;K++)
			{
				Tri.Vertices[K]   = D3DVertices[D3DIndices[I*3+K]].Position;
				Tri.Colors[K]   = D3DVertices[D3DIndices[I*3+K]].Color;
				Tri.TangentX[K] = D3DVertices[D3DIndices[I*3+K]].TangentX;
				Tri.TangentY[K] = D3DVertices[D3DIndices[I*3+K]].TangentY;
				Tri.TangentZ[K] = D3DVertices[D3DIndices[I*3+K]].TangentZ;
			}
		}

		DestMesh.RawTriangles.Unlock();

		DestMesh.Elements = Elements;
		if (bClearDestElements)
		{
			for(INT I=0;I<DestMesh.Elements.Num();I++)
			{
				DestMesh.Elements(I).MaxVertexIndex = 0;
				DestMesh.Elements(I).MinVertexIndex = 0;
				DestMesh.Elements(I).NumTriangles   = 0;
				DestMesh.Elements(I).FirstIndex     = 0;
			}
		}

		D3DMesh->UnlockIndexBuffer();
		D3DMesh->UnlockVertexBuffer();
		D3DMesh->UnlockAttributeBuffer();

		return TRUE;
	}

	/**
	 * Checks whether two points are within a given threshold distance of each other.
	 */
	template<typename PointType>
	UBOOL ArePointsWithinThresholdDistance(const PointType& A,const PointType& B,FLOAT InThresholdDistance)
	{
		return (A - B).IsNearlyZero(InThresholdDistance);
	}

	/**
	 * An adjacency filter that disallows adjacency between exterior and interior triangles, and between interior triangles that
	 * aren't in the same fragment.
	 */
	class FFragmentedAdjacencyFilter
	{
	public:

		UBOOL AreEdgesAdjacent(const FUtilVertex& V0,const FUtilVertex& V1,const FUtilVertex& OtherV0,const FUtilVertex& OtherV1) const
		{
			const INT TriangleFragmentIndex = V0.FragmentIndex;
			const UBOOL bIsFragmentExteriorTriangle = V0.bOverrideTangentBasis;
			const INT OtherTriangleFragmentIndex = OtherV0.FragmentIndex;
			const UBOOL bOtherIsFragmentExteriorTriangle = OtherV0.bOverrideTangentBasis;

			const UBOOL bBothAreFragmentExteriorTriangles = bIsFragmentExteriorTriangle && bOtherIsFragmentExteriorTriangle;
			const UBOOL bBothAreFragmentInteriorTriangles = !bIsFragmentExteriorTriangle && !bOtherIsFragmentExteriorTriangle;
			const UBOOL bBothTrianglesInSameFragment = TriangleFragmentIndex == OtherTriangleFragmentIndex;
			const UBOOL bBothTrianglesAreInteriorTrianglesInSameFragment = bBothAreFragmentInteriorTriangles && bBothTrianglesInSameFragment;
			const UBOOL bEdgesMatch =
				ArePointsWithinThresholdDistance(V0.Position,OtherV1.Position,THRESH_POINTS_ARE_SAME * 4.0f) &&
				ArePointsWithinThresholdDistance(V1.Position,OtherV0.Position,THRESH_POINTS_ARE_SAME * 4.0f);

			return bEdgesMatch && (bBothAreFragmentExteriorTriangles || bBothTrianglesAreInteriorTrianglesInSameFragment);
		}
	};

	/** An adjacency filter that derives adjacency from a UV mapping. */
	class FUVChartAdjacencyFilter
	{
	public:

		/** Initialization constructor. */
		FUVChartAdjacencyFilter(INT InUVIndex)
		:	UVIndex(InUVIndex)
		{}

		UBOOL AreEdgesAdjacent(const FUtilVertex& V0,const FUtilVertex& V1,const FUtilVertex& OtherV0,const FUtilVertex& OtherV1) const
		{
			const UBOOL bEdgesMatch =
				ArePointsWithinThresholdDistance(V0.UVs[UVIndex],OtherV1.UVs[UVIndex],THRESH_UVS_ARE_SAME) &&
				ArePointsWithinThresholdDistance(V1.UVs[UVIndex],OtherV0.UVs[UVIndex],THRESH_UVS_ARE_SAME);
			return bEdgesMatch;
		}

	private:

		INT UVIndex;
	};

	// Helper class for generating uv mapping winding info. Use to prevent creating charts from with inconsistent mapping triangle windings.
	class FLayoutUVWindingInfo
	{
	public:
		FLayoutUVWindingInfo(TRefCountPtr<ID3DXMesh> & Mesh, INT TexCoordIndex)
		{
			D3DMesh = Mesh;
			FUtilVertex* D3DVertices;
			WORD*		 D3DIndices;
			DWORD*		 D3DAttributes;
			D3DMesh->LockVertexBuffer(D3DLOCK_READONLY,(LPVOID*)&D3DVertices);
			D3DMesh->LockIndexBuffer(D3DLOCK_READONLY,(LPVOID*)&D3DIndices);
			D3DMesh->LockAttributeBuffer(D3DLOCK_READONLY,&D3DAttributes);

			for(UINT I=0;I<D3DMesh->GetNumFaces();I++)
			{


				FUtilVertex & vert0 = D3DVertices[D3DIndices[I*3+0]];
				FUtilVertex & vert1 = D3DVertices[D3DIndices[I*3+1]];
				FUtilVertex & vert2 = D3DVertices[D3DIndices[I*3+2]];

				const FVector2D uve1= vert0.UVs[TexCoordIndex] - vert1.UVs[TexCoordIndex];
				const FVector2D uve2= vert0.UVs[TexCoordIndex] - vert2.UVs[TexCoordIndex];

				FVector vec1(uve1 , 0);
				FVector vec2(uve2, 0);	
				vec1.Normalize();
				vec2.Normalize();

				FVector Sidedness = vec1 ^ vec2;
				BOOL Side = FALSE;

				if(Sidedness.Z > 0 )
				{
					Side = TRUE;
				}
				Windings.Push(Side);
			}
			D3DMesh->UnlockVertexBuffer();
			D3DMesh->UnlockVertexBuffer();
			D3DMesh->UnlockVertexBuffer();

		}

		// check if 2 triangles have same winding
		UBOOL HaveSameWinding(UINT tr1, UINT tri2)
		{
			if (Windings(tr1) == Windings(tri2))
			{
				return TRUE;
			}
			else
			{
				return FALSE;
			}
		};

	private:
		TRefCountPtr<ID3DXMesh> D3DMesh;
		TArray<BOOL> Windings;
	};

	/**
	 * Generates adjacency for a D3DXMesh
	 * @param SourceMesh - The mesh to generate adjacency for.
	 * @param OutAdjacency - An array that the adjacency info is written to in the expected D3DX format.
	 * @param Filter - A filter that determines which edge pairs are adjacent.
	 */
	template<typename FilterType>
	void GenerateAdjacency(ID3DXMesh* SourceMesh,TArray<UINT>& OutAdjacency,const FilterType& Filter, FLayoutUVWindingInfo * WindingInfo = NULL) 
	{
		const UINT NumTriangles = SourceMesh->GetNumFaces();

		// Initialize the adjacency array.
		OutAdjacency.Empty(NumTriangles * 3);
		for(UINT AdjacencyIndex = 0;AdjacencyIndex < NumTriangles * 3;AdjacencyIndex++)
		{
			OutAdjacency.AddItem(INDEX_NONE);
		}

		// Lock the D3DX mesh's vertex and index data.
		FUtilVertex* D3DVertices;
		WORD*		 D3DIndices;
		SourceMesh->LockVertexBuffer(D3DLOCK_READONLY,(LPVOID*)&D3DVertices);
		SourceMesh->LockIndexBuffer(D3DLOCK_READONLY,(LPVOID*)&D3DIndices);

		for(UINT TriangleIndex = 0;TriangleIndex < NumTriangles;TriangleIndex++)
		{
			const WORD* TriangleVertexIndices = D3DIndices + TriangleIndex * 3;

			// Find other triangles in the mesh that have a matching edge.
			for(UINT OtherTriangleIndex = TriangleIndex + 1;OtherTriangleIndex < NumTriangles;OtherTriangleIndex++)
			{
				const WORD* OtherTriangleVertexIndices = D3DIndices + OtherTriangleIndex * 3;

				for(INT EdgeIndex = 0;EdgeIndex < 3;EdgeIndex++)
				{
					if(OutAdjacency(TriangleIndex * 3 + EdgeIndex) == INDEX_NONE)
					{
						for(INT OtherEdgeIndex = 0;OtherEdgeIndex < 3;OtherEdgeIndex++)
						{
							if(OutAdjacency(OtherTriangleIndex * 3 + OtherEdgeIndex) == INDEX_NONE)
							{
								const FUtilVertex& V0 = D3DVertices[TriangleVertexIndices[EdgeIndex]];
								const FUtilVertex& V1 = D3DVertices[TriangleVertexIndices[(EdgeIndex + 1) % 3]];
								const FUtilVertex& OtherV0 = D3DVertices[OtherTriangleVertexIndices[OtherEdgeIndex]];
								const FUtilVertex& OtherV1 = D3DVertices[OtherTriangleVertexIndices[(OtherEdgeIndex + 1) % 3]];
								// added check when separating  mirrored, overlapped chunks for "Layout using 0 chunks" mode
								if(Filter.AreEdgesAdjacent(V0,V1,OtherV0,OtherV1) && (WindingInfo ? WindingInfo->HaveSameWinding(TriangleIndex, OtherTriangleIndex) : TRUE)) 
								{
									OutAdjacency(TriangleIndex * 3 + EdgeIndex) = OtherTriangleIndex;
									OutAdjacency(OtherTriangleIndex * 3 + OtherEdgeIndex) = TriangleIndex;
									break;
								}
							}
						}
					}
				}
			}
		}

		// Unlock the D3DX mesh's vertex and index data.
		SourceMesh->UnlockVertexBuffer();
		SourceMesh->UnlockIndexBuffer();
	}

	/** Merges a set of D3DXMeshes. */
	static void MergeD3DXMeshes(TRefCountPtr<ID3DXMesh>& OutMesh,TArray<INT>& OutBaseFaceIndex,const TArray<ID3DXMesh*>& Meshes)
	{
		TArray<D3DVERTEXELEMENT9> VertexElements;
		GetD3D9MeshVertexDeclarations(VertexElements);
		
		// Count the number of faces and vertices in the input meshes.
		INT NumFaces = 0;
		INT NumVertices = 0;
		for(INT MeshIndex = 0;MeshIndex < Meshes.Num();MeshIndex++)
		{
			NumFaces += Meshes(MeshIndex)->GetNumFaces();
			NumVertices += Meshes(MeshIndex)->GetNumVertices();
		}

		// Create mesh for source data
		VERIFYD3D9RESULT(D3DXCreateMesh(
			NumFaces,
			NumVertices,
			D3DXMESH_SYSTEMMEM,
			(D3DVERTEXELEMENT9*)VertexElements.GetData(),
			GLegacyDirect3DDevice9,
			OutMesh.GetInitReference()
			) );

		// Fill D3DXMesh
		FUtilVertex* ResultVertices;
		WORD*		 ResultIndices;
		DWORD*		 ResultAttributes;
		OutMesh->LockVertexBuffer(0,(LPVOID*)&ResultVertices);
		OutMesh->LockIndexBuffer(0,(LPVOID*)&ResultIndices);
		OutMesh->LockAttributeBuffer(0, &ResultAttributes);

		INT BaseVertexIndex = 0;
		INT BaseFaceIndex = 0;
		for(INT MeshIndex = 0;MeshIndex < Meshes.Num();MeshIndex++)
		{
			ID3DXMesh* Mesh = Meshes(MeshIndex);
				
			FUtilVertex* Vertices;
			WORD*		 Indices;
			DWORD*		 Attributes;
			Mesh->LockVertexBuffer(0,(LPVOID*)&Vertices);
			Mesh->LockIndexBuffer(0,(LPVOID*)&Indices);
			Mesh->LockAttributeBuffer(0, &Attributes);

			for(UINT FaceIndex = 0;FaceIndex < Mesh->GetNumFaces();FaceIndex++)
			{
				for(UINT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
				{
					*ResultIndices++ = BaseVertexIndex + *Indices++;
				}
			}
			OutBaseFaceIndex.AddItem(BaseFaceIndex);
			BaseFaceIndex += Mesh->GetNumFaces();

			appMemcpy(ResultVertices,Vertices,Mesh->GetNumVertices() * sizeof(FUtilVertex));
			ResultVertices += Mesh->GetNumVertices();
			BaseVertexIndex += Mesh->GetNumVertices();

			appMemcpy(ResultAttributes,Attributes,Mesh->GetNumFaces() * sizeof(DWORD));
			ResultAttributes += Mesh->GetNumFaces();

			Mesh->UnlockIndexBuffer();
			Mesh->UnlockVertexBuffer();
			Mesh->UnlockAttributeBuffer();
		}

		OutMesh->UnlockIndexBuffer();
		OutMesh->UnlockVertexBuffer();
		OutMesh->UnlockAttributeBuffer();
	}

	/**
	 * Assigns a group index to each triangle such that it's the same group as its adjacent triangles.
	 * The group indices are between zero and the minimum number of indices needed.
	 * @return The number of groups used.
	 */
	static UINT AssignMinimalAdjacencyGroups(const TArray<UINT>& Adjacency,TArray<INT>& OutTriangleGroups)
	{
		const UINT NumTriangles = Adjacency.Num() / 3;
		check(Adjacency.Num() == NumTriangles * 3);

		// Initialize the triangle group array.
		OutTriangleGroups.Empty(NumTriangles);
		for(UINT TriangleIndex = 0;TriangleIndex < NumTriangles;TriangleIndex++)
		{
			OutTriangleGroups.AddItem(INDEX_NONE);
		}

		UINT NumGroups = 0;
		while(TRUE)
		{
			const UINT CurrentGroupIndex = NumGroups;
			TArray<UINT> PendingGroupTriangles;

			// Find the next ungrouped triangle to start the group with.
			for(UINT TriangleIndex = 0;TriangleIndex < NumTriangles;TriangleIndex++)
			{
				if(OutTriangleGroups(TriangleIndex) == INDEX_NONE)
				{
					PendingGroupTriangles.Push(TriangleIndex);
					break;
				}
			}

			if(!PendingGroupTriangles.Num())
			{
				break;
			}
			else
			{
				// Recursively expand the group to include all triangles adjacent to the group's triangles.
				while(PendingGroupTriangles.Num())
				{
					const UINT TriangleIndex = PendingGroupTriangles.Pop();

					OutTriangleGroups(TriangleIndex) = CurrentGroupIndex;

					for(INT EdgeIndex = 0;EdgeIndex < 3;EdgeIndex++)
					{
						const INT AdjacentTriangleIndex = Adjacency(TriangleIndex * 3 + EdgeIndex);
						if(AdjacentTriangleIndex != INDEX_NONE)
						{
							const INT AdjacentTriangleGroupIndex = OutTriangleGroups(AdjacentTriangleIndex);
							check(AdjacentTriangleGroupIndex == INDEX_NONE || AdjacentTriangleGroupIndex == CurrentGroupIndex);
							if(AdjacentTriangleGroupIndex == INDEX_NONE)
							{
								PendingGroupTriangles.Push(AdjacentTriangleIndex);
							}
						}
					}
				};

				NumGroups++;
			}
		};

		return NumGroups;
	}

	/**
	 * Called during unique UV set generation to allow us to update status in the GUI
	 *
	 * @param	InPercentDone	Scalar (0-1) of percent currently complete
	 * @param	InUserData		User data pointer
	 */
	static HRESULT __stdcall GenerateUVsStatusCallback( FLOAT InPercentDone, LPVOID InUserData )
	{
		GWarn->UpdateProgress( InPercentDone * 100, 100 );

		// NOTE: Returning anything other than S_OK will abort the operation
		return S_OK;
	}

	/**
	* For quick generating lightmap uvs.
	* It copies charts from 0 uv channel and layouts them without making new charts (keeps edge splits). Additionally separates folded triangles automatically
	* Use when DXD generates ugly cuts and degenerates charts 
	* @param StaticMesh - The input/output mesh
	* @param LODIndex - The LOD level
	* @param TexCoordIndex - Index of the uv channel to overwrite or create
	* @return TRUE if successful
	*/
	UBOOL LayoutUVs(	
		UStaticMesh* StaticMesh,
		UINT LODIndex, 
		UINT TexCoordIndex
		)
	{
		FStaticMeshRenderData& LOD = StaticMesh->LODModels(LODIndex);

		// Sort the mesh's triangles by whether they need to be charted, or just to be packed into the atlas.
		FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*) LOD.RawTriangles.Lock(LOCK_READ_ONLY);
		TArray<FStaticMeshTriangle> TrianglesToChartAndAtlas;
		TArray<FStaticMeshTriangle> TrianglesToAtlas;
		for(INT TriangleIndex = 0;TriangleIndex < LOD.RawTriangles.GetElementCount();TriangleIndex++)
		{
			FStaticMeshTriangle& Triangle = RawTriangleData[TriangleIndex];
			if(TexCoordIndex >0)
			{
				Triangle.UVs[0][TexCoordIndex] =		Triangle.UVs[0][0];
				Triangle.UVs[1][TexCoordIndex] =		Triangle.UVs[1][0];
				Triangle.UVs[2][TexCoordIndex] =		Triangle.UVs[2][0];

			}
			if (INT(TexCoordIndex +1) >= (INT)Triangle.NumUVs)
			{
				Triangle.NumUVs = UINT(TexCoordIndex +1);
			}
			TrianglesToAtlas.AddItem(Triangle);
		}

		LOD.RawTriangles.Unlock();
		TRefCountPtr<ID3DXMesh> ChartMesh;
		TArray<UINT> AtlasAndChartAdjacency;
		TArray<INT> AtlasAndChartTriangleCharts;
		TRefCountPtr<ID3DXMesh> MergedMesh;
		TArray<UINT> MergedAdjacency;
		TArray<INT> MergedTriangleCharts;
		TRefCountPtr<ID3DXMesh> AtlasOnlyMesh;
		TArray<UINT> AtlasOnlyAdjacency;
		TArray<INT> AtlasOnlyTriangleCharts;

		if(TrianglesToAtlas.Num())
		{
			// Create a D3DXMesh for the triangles that only need to be atlassed.
			const UBOOL bRemoveDegenerateTriangles = TRUE;
			if (!ConvertStaticMeshTrianglesToD3DXMesh(TrianglesToAtlas,bRemoveDegenerateTriangles,AtlasOnlyMesh))
			{
				appDebugMessagef(TEXT("GenerateUVs failed, couldn't convert to a D3DXMesh."));
				return FALSE;
			}
			// generate mapping orientations info 
			FLayoutUVWindingInfo WindingInfo(AtlasOnlyMesh, TexCoordIndex);
			// Generate adjacency for the pre-charted triangles based on their input charts.
			GenerateAdjacency(AtlasOnlyMesh,AtlasOnlyAdjacency,FUVChartAdjacencyFilter(TexCoordIndex), &WindingInfo);


			////clean the mesh
			TRefCountPtr<ID3DXMesh> TempMesh;
			TArray<UINT> CleanedAdjacency(AtlasOnlyMesh->GetNumFaces() * 3);
			if( FAILED(D3DXCleanMesh( D3DXCLEAN_SIMPLIFICATION, 
				AtlasOnlyMesh, 
				(DWORD*)AtlasOnlyAdjacency.GetTypedData(), 
				TempMesh.GetInitReference(), 
				(DWORD*)CleanedAdjacency.GetTypedData(), 
				NULL ) ) )
			{
				appDebugMessagef(TEXT("GenerateUVs failed, couldn't clean mesh."));
				return FALSE;
			}

			// Group the pre-charted triangles into indexed charts based on their adjacency in the chart.
			AssignMinimalAdjacencyGroups(CleanedAdjacency,AtlasOnlyTriangleCharts);

			MergedMesh = TempMesh;
			MergedAdjacency = CleanedAdjacency;
			MergedTriangleCharts = AtlasOnlyTriangleCharts;
		}

		if(MergedMesh)
		{
			// Create a buffer to hold the triangle chart data.
			TRefCountPtr<ID3DXBuffer> MergedTriangleChartsBuffer;
			VERIFYD3D9RESULT(D3DXCreateBuffer(
				MergedTriangleCharts.Num() * sizeof(INT),
				MergedTriangleChartsBuffer.GetInitReference()
				));
			DWORD* MergedTriangleChartsBufferPointer = (DWORD*)MergedTriangleChartsBuffer->GetBufferPointer();
			for(INT TriangleIndex = 0;TriangleIndex < MergedTriangleCharts.Num();TriangleIndex++)
			{
				*MergedTriangleChartsBufferPointer++ = MergedTriangleCharts(TriangleIndex);
			}
			UINT LightMapResolution = StaticMesh->LightMapResolution ? StaticMesh->LightMapResolution : 256;
			const FLOAT GutterSize = 2.0f;
			// Pack the charts into a unified atlas.
			HRESULT Result = D3DXUVAtlasPack(
				MergedMesh,
				LightMapResolution,
				LightMapResolution,
				GutterSize,
				TexCoordIndex,
				(DWORD*)MergedAdjacency.GetTypedData(),
				NULL,
				0,
				NULL,
				0,
				MergedTriangleChartsBuffer
				);
			if (FAILED(Result))
			{
				warnf(
					TEXT("D3DXUVAtlasPack() returned %u."),
					Result
					);
				appDebugMessagef(TEXT("GenerateUVs failed, D3DXUVAtlasPack failed."));
				return FALSE;
			}

			INT NewNumTexCoords = LOD.VertexBuffer.GetNumTexCoords();
			//if the selected index doesn't exist yet, create it
			if (TexCoordIndex == LOD.VertexBuffer.GetNumTexCoords())
			{
				NewNumTexCoords++;
			}

			//convert back to UStaticMesh
			if (!ConvertD3DXMeshToStaticMesh(MergedMesh, LOD, NewNumTexCoords, LOD.Elements, FALSE))
			{
				appDebugMessagef(TEXT("GenerateUVs failed, couldn't convert the simplified D3DXMesh back to a UStaticMesh."));
				return FALSE;
			}
			// Re-build the static mesh
			// todo - only rebuild the altered LOD
			StaticMesh->Build();
		}
		return TRUE;
	}

	/**
	* Generates a unique UV layout for a static mesh
	* @param StaticMesh - The input/output mesh
	* @param LODIndex - The LOD level
	* @param TexCoordIndex - Index of the uv channel to overwrite or create
	* @param bKeepExistingCoordinates - True to preserve the existing charts when packing
	* @param MinChartSpacingPercent - Minimum distance between two packed charts (0.0-100.0)
	* @param BorderSpacingPercent - Spacing between UV border and charts (0.0-100.0)
	* @param bUseMaxStretch - True if "MaxDesiredStretch" should be used; otherwise "MaxCharts" is used
	* @param InFalseEdgeIndices - Optional list of raw face edge indices to be ignored when creating UV seams
	* @param MaxCharts - In: Max number of charts to allow; Out:Number of charts generated by the uv unwrap algorithm.
	* @param MaxDesiredStretch - The amount of stretching allowed. 0 means no stretching is allowed, 1 means any amount of stretching can be used. 
	* @return TRUE if successful
	*/
	UBOOL GenerateUVs(
		UStaticMesh* StaticMesh,
		UINT LODIndex, 
		UINT TexCoordIndex,
		UBOOL bKeepExistingCoordinates,
		FLOAT MinChartSpacingPercent,
		FLOAT BorderSpacingPercent,
		UBOOL bUseMaxStretch,
		const TArray< INT >* InFalseEdgeIndices,
		UINT& MaxCharts,
		FLOAT& MaxDesiredStretch
		)
	{
		FStaticMeshRenderData& LOD = StaticMesh->LODModels(LODIndex);
		if( LOD.RawTriangles.GetElementCount() == 0 )
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd( TEXT( "GenerateUVs_Error_NoSourceDataAvailable" ) ) );
			return FALSE;
		}
		
		// Sort the mesh's triangles by whether they need to be charted, or just to be packed into the atlas.
		FStaticMeshRenderData& BaseLOD = StaticMesh->LODModels(LODIndex);
		const FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*) BaseLOD.RawTriangles.Lock(LOCK_READ_ONLY);
		TArray<FStaticMeshTriangle> TrianglesToChartAndAtlas;
		TArray<FStaticMeshTriangle> TrianglesToAtlas;
		for(INT TriangleIndex = 0;TriangleIndex < BaseLOD.RawTriangles.GetElementCount();TriangleIndex++)
		{
			const FStaticMeshTriangle& Triangle = RawTriangleData[TriangleIndex];

			// Determine whether the triangle has a mapping for the specified UV channel.
			UBOOL bHasMapping =
				(INT)TexCoordIndex < Triangle.NumUVs &&
				Triangle.UVs[0][TexCoordIndex] != Triangle.UVs[1][TexCoordIndex] &&
				Triangle.UVs[1][TexCoordIndex] != Triangle.UVs[2][TexCoordIndex] &&
				Triangle.UVs[2][TexCoordIndex] != Triangle.UVs[0][TexCoordIndex];

			if(!bHasMapping || !bKeepExistingCoordinates)
			{
				TrianglesToChartAndAtlas.AddItem(Triangle);
			}
			else
			{
				TrianglesToAtlas.AddItem(Triangle);
			}
		}
		BaseLOD.RawTriangles.Unlock();

		TRefCountPtr<ID3DXMesh> ChartMesh;
		TArray<UINT> AtlasAndChartAdjacency;
		TArray<INT> AtlasAndChartTriangleCharts;
		if(TrianglesToChartAndAtlas.Num())
		{
			const UBOOL bUseFalseEdges = !bKeepExistingCoordinates && InFalseEdgeIndices != NULL;

			// When using false edges we don't remove degenerates as we want our incoming selected edge list to map
			// correctly to the D3DXMesh.
			const UBOOL bRemoveDegenerateTriangles = !bUseFalseEdges;

			// Create a D3DXMesh for the triangles being charted.
			TRefCountPtr<ID3DXMesh> SourceMesh;
			if (!ConvertStaticMeshTrianglesToD3DXMesh(TrianglesToChartAndAtlas,bRemoveDegenerateTriangles,SourceMesh))
			{
				appDebugMessagef(TEXT("GenerateUVs failed, couldn't convert to a D3DXMesh."));
				return FALSE;
			}

			//generate adjacency info for the mesh, which is needed later
			TArray<UINT> Adjacency;
			GenerateAdjacency(SourceMesh,Adjacency,FFragmentedAdjacencyFilter());

			// We don't clean the mesh as this can collapse vertices or delete degenerate triangles, and
			// we want our incoming selected edge list to map correctly to the D3DXMesh.
			if( !bUseFalseEdges )
			{
				//clean the mesh
				TRefCountPtr<ID3DXMesh> TempMesh;
				TArray<UINT> CleanedAdjacency(SourceMesh->GetNumFaces() * 3);
				if( FAILED(D3DXCleanMesh( D3DXCLEAN_SIMPLIFICATION, SourceMesh, (DWORD*)Adjacency.GetTypedData(), TempMesh.GetInitReference(), 
					(DWORD*)CleanedAdjacency.GetTypedData(), NULL ) ) )
				{
					appDebugMessagef(TEXT("GenerateUVs failed, couldn't clean mesh."));
					return FALSE;
				}
				SourceMesh = TempMesh;
				Adjacency = CleanedAdjacency;
			}


			// Setup the D3DX "false edge" array.  This is three DWORDS per face that define properties of the
			// face's edges.  Values of -1 indicates that the edge may be used as a UV seam in a the chart.  Any
			// other value indicates that the edge should never be a UV seam.  This essentially allows us to
			// provide a precise list of edges to be used as UV seams in the new charts.
			DWORD* FalseEdgeArray = NULL;
			TArray<UINT> FalseEdges;
			if( bUseFalseEdges )
			{
				// Make sure our incoming edge indices will match the structure of this buffer
				check( TrianglesToChartAndAtlas.Num() == LOD.RawTriangles.GetElementCount() );
				check( TrianglesToChartAndAtlas.Num() == SourceMesh->GetNumFaces() );
				check( TrianglesToChartAndAtlas.Num() * 3 == Adjacency.Num() );

				// -1 means "always use this edge as a chart UV seam" to D3DX
 				FalseEdges.Add( SourceMesh->GetNumFaces() * 3 );
				for( INT CurFalseEdgeIndex = 0; CurFalseEdgeIndex < (INT)SourceMesh->GetNumFaces() * 3; ++CurFalseEdgeIndex )
				{
					FalseEdges( CurFalseEdgeIndex ) = -1;
				}

				// For each tagged edge
				for( INT CurTaggedEdgeIndex = 0; CurTaggedEdgeIndex < InFalseEdgeIndices->Num(); ++CurTaggedEdgeIndex )
				{
					const INT EdgeIndex = ( *InFalseEdgeIndices )( CurTaggedEdgeIndex );
					
	/*
					const INT TriangleIndex = EdgeIndex / 3;
					const INT TriangleEdgeIndex = EdgeIndex % 3;
					const INT MeshTriangleEdge = TriangleIndex * 3 + TriangleEdgeIndex;	// EdgeIndex
	*/

					// Mark this as a false edge by setting it to a value other than negative one
					FalseEdges( EdgeIndex ) = Adjacency( CurTaggedEdgeIndex );
				}

				FalseEdgeArray = (DWORD*)FalseEdges.GetTypedData();
			}

			
			// Partition the mesh's triangles into charts.
			TRefCountPtr<ID3DXBuffer> PartitionResultAdjacencyBuffer;
			TRefCountPtr<ID3DXBuffer> FacePartitionBuffer;
			HRESULT Result = D3DXUVAtlasPartition(
				SourceMesh,
				bUseMaxStretch ? 0 : MaxCharts,				// Max charts (0 = use max stretch instead)
				MaxDesiredStretch,
				TexCoordIndex,
				(DWORD*)Adjacency.GetTypedData(),
				FalseEdgeArray,	// False edges
				NULL,		// IMT data
				&GenerateUVsStatusCallback,
				0.01f,		// Callback frequency
				NULL,			// Callback user data
				D3DXUVATLAS_GEODESIC_QUALITY,
				ChartMesh.GetInitReference(),
				FacePartitionBuffer.GetInitReference(),
				NULL,
				PartitionResultAdjacencyBuffer.GetInitReference(),
				&MaxDesiredStretch,
				&MaxCharts
				);
			if (FAILED(Result))
			{
				warnf(
					TEXT("D3DXUVAtlasPartition() returned %u with MaxDesiredStretch=%.2f, TexCoordIndex=%u."),
					Result,
					MaxDesiredStretch,
					TexCoordIndex
					);
				appDebugMessagef(TEXT("GenerateUVs failed, D3DXUVAtlasPartition failed."));
				return FALSE;
			}

			// Extract the chart adjacency data from the D3DX buffer into an array.
			for(UINT TriangleIndex = 0;TriangleIndex < ChartMesh->GetNumFaces();TriangleIndex++)
			{
				for(INT EdgeIndex = 0;EdgeIndex < 3;EdgeIndex++)
				{
					AtlasAndChartAdjacency.AddItem(*((DWORD*)PartitionResultAdjacencyBuffer->GetBufferPointer()+TriangleIndex*3+EdgeIndex));
				}
			}

			// Extract the triangle chart data from the D3DX buffer into an array.
			DWORD* FacePartitionBufferPointer = (DWORD*)FacePartitionBuffer->GetBufferPointer();
			for(UINT TriangleIndex = 0;TriangleIndex < ChartMesh->GetNumFaces();TriangleIndex++)
			{
				AtlasAndChartTriangleCharts.AddItem(*FacePartitionBufferPointer++);
			}

			// Scale the partitioned UVs down.
			FUtilVertex* LockedVertices;
			ChartMesh->LockVertexBuffer(0,(LPVOID*)&LockedVertices);
			for(UINT VertexIndex = 0;VertexIndex < ChartMesh->GetNumVertices();VertexIndex++)
			{
				LockedVertices[VertexIndex].UVs[TexCoordIndex] /= 2048.0f;
			}
			ChartMesh->UnlockVertexBuffer();
		}

		TRefCountPtr<ID3DXMesh> AtlasOnlyMesh;
		TArray<UINT> AtlasOnlyAdjacency;
		TArray<INT> AtlasOnlyTriangleCharts;
		if(TrianglesToAtlas.Num())
		{
			// Create a D3DXMesh for the triangles that only need to be atlassed.
			const UBOOL bRemoveDegenerateTriangles = TRUE;
			if (!ConvertStaticMeshTrianglesToD3DXMesh(TrianglesToAtlas,bRemoveDegenerateTriangles,AtlasOnlyMesh))
			{
				appDebugMessagef(TEXT("GenerateUVs failed, couldn't convert to a D3DXMesh."));
				return FALSE;
			}

			// Generate adjacency for the pre-charted triangles based on their input charts.
			GenerateAdjacency(AtlasOnlyMesh,AtlasOnlyAdjacency,FUVChartAdjacencyFilter(TexCoordIndex));

			//clean the mesh
			TRefCountPtr<ID3DXMesh> TempMesh;
			TArray<UINT> CleanedAdjacency(AtlasOnlyMesh->GetNumFaces() * 3);
			if( FAILED(D3DXCleanMesh( D3DXCLEAN_SIMPLIFICATION, AtlasOnlyMesh, (DWORD*)AtlasOnlyAdjacency.GetTypedData(), TempMesh.GetInitReference(), 
				(DWORD*)CleanedAdjacency.GetTypedData(), NULL ) ) )
			{
				appDebugMessagef(TEXT("GenerateUVs failed, couldn't clean mesh."));
				return FALSE;
			}
			AtlasOnlyMesh = TempMesh;
			AtlasOnlyAdjacency = CleanedAdjacency;

			// Group the pre-charted triangles into indexed charts based on their adjacency in the chart.
			AssignMinimalAdjacencyGroups(AtlasOnlyAdjacency,AtlasOnlyTriangleCharts);
		}

		TRefCountPtr<ID3DXMesh> MergedMesh;
		TArray<UINT> MergedAdjacency;
		TArray<INT> MergedTriangleCharts;
		if(TrianglesToChartAndAtlas.Num() && TrianglesToAtlas.Num())
		{
			// Merge the newly charted triangles with the triangles that had a chart we wanted to keep.
			TArray<ID3DXMesh*> MergeMeshes;
			TArray<INT> MergedBaseFaceIndices;
			MergeMeshes.AddItem(ChartMesh);
			MergeMeshes.AddItem(AtlasOnlyMesh);
			MergeD3DXMeshes(MergedMesh,MergedBaseFaceIndices,MergeMeshes);

			// Merge the adjacency data for the pre-charted and newly charted meshes.
			for(INT Index = 0;Index < AtlasAndChartAdjacency.Num();Index++)
			{
				MergedAdjacency.AddItem(MergedBaseFaceIndices(0) + AtlasAndChartAdjacency(Index));
			}
			for(INT Index = 0;Index < AtlasOnlyAdjacency.Num();Index++)
			{
				const INT RemappedAdjacentTriangleIndex =
					AtlasOnlyAdjacency(Index) == INDEX_NONE ?
						INDEX_NONE :
						MergedBaseFaceIndices(1) + AtlasOnlyAdjacency(Index);
				MergedAdjacency.AddItem(RemappedAdjacentTriangleIndex);
			}

			// Merge the chart indices for the pre-charted and newly charted meshes.
			INT BaseTriangleChartIndex = 0;
			for(INT TriangleIndex = 0;TriangleIndex < AtlasAndChartTriangleCharts.Num();TriangleIndex++)
			{
				const INT TriangleChartIndex = AtlasAndChartTriangleCharts(TriangleIndex);
				MergedTriangleCharts.AddItem(TriangleChartIndex);
				BaseTriangleChartIndex = Max(BaseTriangleChartIndex,TriangleChartIndex) + 1;
			}
			for(INT TriangleIndex = 0;TriangleIndex < AtlasOnlyTriangleCharts.Num();TriangleIndex++)
			{
				const INT TriangleChartIndex = BaseTriangleChartIndex + AtlasOnlyTriangleCharts(TriangleIndex);
				MergedTriangleCharts.AddItem(TriangleChartIndex);
			}
		}
		else if(TrianglesToChartAndAtlas.Num())
		{
			MergedMesh = ChartMesh;
			MergedAdjacency = AtlasAndChartAdjacency;
			MergedTriangleCharts = AtlasAndChartTriangleCharts;
		}
		else if(TrianglesToAtlas.Num())
		{
			MergedMesh = AtlasOnlyMesh;
			MergedAdjacency = AtlasOnlyAdjacency;
			MergedTriangleCharts = AtlasOnlyTriangleCharts;
		}

		if(MergedMesh)
		{
			// Create a buffer to hold the triangle chart data.
			TRefCountPtr<ID3DXBuffer> MergedTriangleChartsBuffer;
			VERIFYD3D9RESULT(D3DXCreateBuffer(
				MergedTriangleCharts.Num() * sizeof(INT),
				MergedTriangleChartsBuffer.GetInitReference()
				));
			DWORD* MergedTriangleChartsBufferPointer = (DWORD*)MergedTriangleChartsBuffer->GetBufferPointer();
			for(INT TriangleIndex = 0;TriangleIndex < MergedTriangleCharts.Num();TriangleIndex++)
			{
				*MergedTriangleChartsBufferPointer++ = MergedTriangleCharts(TriangleIndex);
			}

			const UINT FakeTexSize = 1024;
			const FLOAT GutterSize = ( FLOAT )FakeTexSize * MinChartSpacingPercent * 0.01f;

			// Pack the charts into a unified atlas.
			HRESULT Result = D3DXUVAtlasPack(
				MergedMesh,
				FakeTexSize,
				FakeTexSize,
				GutterSize,
				TexCoordIndex,
				(DWORD*)MergedAdjacency.GetTypedData(),
				&GenerateUVsStatusCallback,
				0.01f,		// Callback frequency
				NULL,
				0,
				MergedTriangleChartsBuffer
				);
			if (FAILED(Result))
			{
				warnf(
					TEXT("D3DXUVAtlasPack() returned %u."),
					Result
					);
				appDebugMessagef(TEXT("GenerateUVs failed, D3DXUVAtlasPack failed."));
				return FALSE;
			}

			INT NewNumTexCoords = LOD.VertexBuffer.GetNumTexCoords();
			//if the selected index doesn't exist yet, create it
			if (TexCoordIndex == LOD.VertexBuffer.GetNumTexCoords())
			{
				NewNumTexCoords++;
			}

			//convert back to UStaticMesh
			if (!ConvertD3DXMeshToStaticMesh(MergedMesh, LOD, NewNumTexCoords, LOD.Elements, FALSE))
			{
				appDebugMessagef(TEXT("GenerateUVs failed, couldn't convert the simplified D3DXMesh back to a UStaticMesh."));
				return FALSE;
			}


			// Scale/offset the UVs appropriately to ensure there is empty space around the border
			{
				const FLOAT BorderSize = BorderSpacingPercent * 0.01f;
				const FLOAT ScaleAmount = 1.0f - BorderSize * 2.0f;

				const INT TriangleCount = LOD.RawTriangles.GetElementCount();
				FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*)LOD.RawTriangles.Lock(LOCK_READ_WRITE);
				for( INT CurTriangleIndex = 0; CurTriangleIndex < TriangleCount; ++CurTriangleIndex )
				{
					FStaticMeshTriangle& CurTriangle = RawTriangleData[ CurTriangleIndex ];

					for( INT CurVertIndex = 0; CurVertIndex < 3; ++CurVertIndex )
					{
						for( INT CurUVIndex = 0; CurUVIndex < CurTriangle.NumUVs; ++CurUVIndex )
						{
							CurTriangle.UVs[ CurVertIndex ][ CurUVIndex ].X = BorderSize + CurTriangle.UVs[ CurVertIndex ][ CurUVIndex ].X * ScaleAmount;
							CurTriangle.UVs[ CurVertIndex ][ CurUVIndex ].Y = BorderSize + CurTriangle.UVs[ CurVertIndex ][ CurUVIndex ].Y * ScaleAmount;
						}
					}
				}

				LOD.RawTriangles.Unlock();
			}


			// Re-build the static mesh
			// todo - only rebuild the altered LOD
			StaticMesh->Build();
		}

		return TRUE; 

	}
}

#endif

