/*=============================================================================
	DynamicMeshBuilder.cpp: Dynamic mesh builder implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

/** The index buffer type used for dynamic meshes. */
class FDynamicMeshIndexBuffer : public FDynamicPrimitiveResource, public FIndexBuffer
{
public:

	TArray<INT> Indices;

	// FRenderResource interface.
	virtual void InitRHI()
	{
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(INT),Indices.Num() * sizeof(INT),NULL,RUF_Static);

		// Write the indices to the index buffer.
		void* Buffer = RHILockIndexBuffer(IndexBufferRHI,0,Indices.Num() * sizeof(INT));
		appMemcpy(Buffer,&Indices(0),Indices.Num() * sizeof(INT));
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}

	// FDynamicPrimitiveResource interface.
	virtual void InitPrimitiveResource()
	{
		InitResource();
	}
	virtual void ReleasePrimitiveResource()
	{
		ReleaseResource();
		delete this;
	}
};

/** The vertex buffer type used for dynamic meshes. */
class FDynamicMeshVertexBuffer : public FDynamicPrimitiveResource, public FVertexBuffer
{
public:

	TArray<FDynamicMeshVertex> Vertices;

	// FResourceResource interface.
	virtual void InitRHI()
	{
		VertexBufferRHI = RHICreateVertexBuffer(Vertices.Num() * sizeof(FDynamicMeshVertex),NULL,RUF_Static);

		// Copy the vertex data into the vertex buffer.
		void* VertexBufferData = RHILockVertexBuffer(VertexBufferRHI,0,Vertices.Num() * sizeof(FDynamicMeshVertex),FALSE);
		appMemcpy(VertexBufferData,&Vertices(0),Vertices.Num() * sizeof(FDynamicMeshVertex));
		RHIUnlockVertexBuffer(VertexBufferRHI);
	}

	// FDynamicPrimitiveResource interface.
	virtual void InitPrimitiveResource()
	{
		InitResource();
	}
	virtual void ReleasePrimitiveResource()
	{
		ReleaseResource();
		delete this;
	}
};

/** The vertex factory type used for dynamic meshes. */
class FDynamicMeshVertexFactory : public FDynamicPrimitiveResource, public FLocalVertexFactory
{
public:

	/** Initialization constructor. */
	FDynamicMeshVertexFactory(const FDynamicMeshVertexBuffer* VertexBuffer)
	{
		// Initialize the vertex factory's stream components.
		if(IsInRenderingThread())
		{
			DataType TheData;
			TheData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,Position,VET_Float3);
			TheData.TextureCoordinates.AddItem(
				FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FDynamicMeshVertex,TextureCoordinate),sizeof(FDynamicMeshVertex),VET_Float2)
				);
			TheData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentX,VET_PackedNormal);
			TheData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentZ,VET_PackedNormal);
			TheData.ColorComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,Color,VET_Color);
			SetData(TheData);
		}
		else
		{
		    ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			    InitVertexFactory,
			    FLocalVertexFactory*,VertexFactory,this,
			    const FDynamicMeshVertexBuffer*,VertexBuffer,VertexBuffer,
			    {
				    DataType TheData;
				    TheData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,Position,VET_Float3);
				    TheData.TextureCoordinates.AddItem(
					    FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FDynamicMeshVertex,TextureCoordinate),sizeof(FDynamicMeshVertex),VET_Float2)
					    );
				    TheData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentX,VET_PackedNormal);
				    TheData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentZ,VET_PackedNormal);
					TheData.ColorComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,Color,VET_Color);
				    VertexFactory->SetData(TheData);
			    });
		}
	}

	// FDynamicPrimitiveResource interface.
	virtual void InitPrimitiveResource()
	{
		InitResource();
	}
	virtual void ReleasePrimitiveResource()
	{
		ReleaseResource();
		delete this;
	}
};

FDynamicMeshBuilder::FDynamicMeshBuilder()
{
	VertexBuffer = new FDynamicMeshVertexBuffer;
	IndexBuffer = new FDynamicMeshIndexBuffer;
}

FDynamicMeshBuilder::~FDynamicMeshBuilder()
{
	//Delete the resources if they have not been already. At this point they are only valid if Draw() has never been called,
	//so the resources have not been passed to the rendering thread.  Also they do not need to be released,
	//since they are only initialized when Draw() is called.
	delete VertexBuffer;
	delete IndexBuffer;
}

INT FDynamicMeshBuilder::AddVertex(
	const FVector& InPosition,
	const FVector2D& InTextureCoordinate,
	const FVector& InTangentX,
	const FVector& InTangentY,
	const FVector& InTangentZ,
	const FColor& InColor
	)
{
	INT VertexIndex = VertexBuffer->Vertices.Num();
	FDynamicMeshVertex* Vertex = new(VertexBuffer->Vertices) FDynamicMeshVertex;
	Vertex->Position = InPosition;
	Vertex->TextureCoordinate = InTextureCoordinate;
	Vertex->TangentX = InTangentX;
	Vertex->TangentZ = InTangentZ;
	// store the sign of the determinant in TangentZ.W (-1=0,+1=255)
	Vertex->TangentZ.Vector.W = GetBasisDeterminantSign( InTangentX, InTangentY, InTangentZ ) < 0 ? 0 : 255;
	Vertex->Color = InColor;

	return VertexIndex;
}

/** Adds a vertex to the mesh. */
INT FDynamicMeshBuilder::AddVertex(const FDynamicMeshVertex &InVertex)
{
	INT VertexIndex = VertexBuffer->Vertices.Num();
	FDynamicMeshVertex* Vertex = new(VertexBuffer->Vertices) FDynamicMeshVertex(InVertex);

	return VertexIndex;
}

/** Adds a triangle to the mesh. */
void FDynamicMeshBuilder::AddTriangle(INT V0,INT V1,INT V2)
{
	IndexBuffer->Indices.AddItem(V0);
	IndexBuffer->Indices.AddItem(V1);
	IndexBuffer->Indices.AddItem(V2);
}

/** Adds many vertices to the mesh. Returns start index of verts in the overall array. */
INT FDynamicMeshBuilder::AddVertices(const TArray<FDynamicMeshVertex> &InVertices)
{
	INT StartIndex = VertexBuffer->Vertices.Num();
	VertexBuffer->Vertices.Append(InVertices);
	return StartIndex;
}

/** Add many indices to the mesh. */
void FDynamicMeshBuilder::AddTriangles(const TArray<INT> &InIndices)
{
	IndexBuffer->Indices.Append(InIndices);
}

void FDynamicMeshBuilder::Draw(FPrimitiveDrawInterface* PDI,const FMatrix& LocalToWorld,const FMaterialRenderProxy* MaterialRenderProxy,BYTE DepthPriorityGroup,FLOAT DepthBias,UBOOL bDisableBackfaceCulling)
{
	// Only draw non-empty meshes.
	if(VertexBuffer->Vertices.Num() > 0 && IndexBuffer->Indices.Num() > 0)
	{
		// Register the dynamic resources with the PDI.
		PDI->RegisterDynamicResource(VertexBuffer);
		PDI->RegisterDynamicResource(IndexBuffer);

		// Create the vertex factory.
		FDynamicMeshVertexFactory* VertexFactory = new FDynamicMeshVertexFactory(VertexBuffer);
		PDI->RegisterDynamicResource(VertexFactory);

		// Draw the mesh.
		FMeshBatch Mesh;
		FMeshBatchElement& BatchElement = Mesh.Elements(0);
		BatchElement.IndexBuffer = IndexBuffer;
		Mesh.VertexFactory = VertexFactory;
		Mesh.MaterialRenderProxy = MaterialRenderProxy;
		BatchElement.LocalToWorld = LocalToWorld;
		BatchElement.WorldToLocal = LocalToWorld.Inverse();
		// previous l2w not used so treat as static
		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = IndexBuffer->Indices.Num() / 3;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = VertexBuffer->Vertices.Num() - 1;
		Mesh.ReverseCulling = LocalToWorld.Determinant() < 0.0f ? TRUE : FALSE;
		Mesh.bDisableBackfaceCulling = bDisableBackfaceCulling;
		Mesh.Type = PT_TriangleList;
		Mesh.DepthPriorityGroup = DepthPriorityGroup;
		Mesh.DepthBias = DepthBias;
		Mesh.bUsePreVertexShaderCulling = FALSE;
		Mesh.PlatformMeshData = NULL;
		PDI->DrawMesh(Mesh);

		// Clear the resource pointers so they cannot be overwritten accidentally.
		// These resources will be released by the PDI.
		VertexBuffer = NULL;
		IndexBuffer = NULL;
	}
}
