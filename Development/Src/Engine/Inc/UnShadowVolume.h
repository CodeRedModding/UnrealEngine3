/*=============================================================================
	UnShadowVolume.h: Shadow volume definitions.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

//
//	FShadowVertex
//

struct FShadowVertex
{
	FVector	Position;
	FLOAT	Extrusion;

	FShadowVertex() {}
	FShadowVertex(FVector InPosition,FLOAT InExtrusion): Position(InPosition), Extrusion(InExtrusion) {}

	friend FArchive& operator<<(FArchive& Ar,FShadowVertex& V)
	{
		return Ar << V.Position << V.Extrusion;
	}
};

//
//	FShadowVertexBuffer
//

struct FShadowVertexBuffer: FVertexBuffer
{
	TArray<FVector>	Vertices;

	FShadowVertexBuffer()
	{
		Size = 0;
		Stride = sizeof(FShadowVertex);
	}

	virtual void GetData(void* Buffer)
	{
		FShadowVertex*	DestVertex = (FShadowVertex*)Buffer;
		for(UINT VertexIndex = 0;VertexIndex < (UINT)Vertices.Num();VertexIndex++)
			*DestVertex++ = FShadowVertex(Vertices(VertexIndex),0.0f);
		for(UINT VertexIndex = 0;VertexIndex < (UINT)Vertices.Num();VertexIndex++)
			*DestVertex++ = FShadowVertex(Vertices(VertexIndex),1.0f);
	}

	friend FArchive& operator<<(FArchive& Ar,FShadowVertexBuffer& V)
	{
		return Ar << V.Vertices << V.Size;
	}
};

//
//	FShadowIndexBuffer
//

struct FShadowIndexBuffer: FIndexBuffer
{
	TArray<_WORD>	Indices;

	// Constructor.

	FShadowIndexBuffer()
	{
		Stride = sizeof(_WORD);
		Size = 0;
		Dynamic = 1;
	}

	// CalcSize

	void CalcSize()
	{
		Size = Indices.Num() * Stride;
		GResourceManager->UpdateResource(this);
	}

	// AddFace

	FORCEINLINE void AddFace(_WORD V1,_WORD V2,_WORD V3)
	{
		INT	BaseIndex = Indices.Add(3);
		Indices(BaseIndex + 0) = V1;
		Indices(BaseIndex + 1) = V2;
		Indices(BaseIndex + 2) = V3;
	}

	// AddEdge

	FORCEINLINE void AddEdge(_WORD V1,_WORD V2,_WORD ExtrudeOffset)
	{
		INT	BaseIndex = Indices.Add(6);

		Indices(BaseIndex + 0) = V1;
		Indices(BaseIndex + 1) = V1 + ExtrudeOffset;
		Indices(BaseIndex + 2) = V2 + ExtrudeOffset;

		Indices(BaseIndex + 3) = V2 + ExtrudeOffset;
		Indices(BaseIndex + 4) = V2;
		Indices(BaseIndex + 5) = V1;
	}

	// FIndexBuffer interface.

	virtual void GetData(void* Buffer)
	{
		appMemcpy(Buffer,&Indices(0),Size);
	}
};

//
//	FMeshEdge
//

struct FMeshEdge
{
	INT	Vertices[2];
	INT	Faces[2];
	
	// Constructor.

	FMeshEdge() {}

	friend FArchive& operator<<(FArchive& Ar,FMeshEdge& E)
	{
		return Ar << E.Vertices[0] << E.Vertices[1] << E.Faces[0] << E.Faces[1];
	}
};
