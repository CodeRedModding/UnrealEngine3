/*=============================================================================
	UnModelRender.cpp: Unreal model rendering
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"

//
//	FModelVertex
//

struct FModelVertex
{
	FVector			Position;
	FPackedNormal	TangentX,
					TangentY,
					TangentZ;
	FLOAT			U,
					V;
};

//
//	FNodeVertexIndices
//

struct FNodeVertexIndices
{
	UINT	SurfaceIndex;
	UINT	BaseVertexIndex;
	_WORD	NumVertices;

	FNodeVertexIndices(UINT InSurfaceIndex,UINT InBaseVertexIndex,DWORD InNumVertices):
		SurfaceIndex(InSurfaceIndex),
		BaseVertexIndex(InBaseVertexIndex),
		NumVertices(InNumVertices)
	{}
};

//
//	FModelVertexBuffer
//

struct FModelVertexBuffer: FVertexBuffer
{
	TArray<UINT>	NodeBaseVertexIndices;
	TMap<INT,UINT>	BackSideVertexIndices;
	UModel*			Model;

	// Constructor.

	FModelVertexBuffer(UModel* InModel):
		Model(InModel)
	{
		Stride = sizeof(FModelVertex);
		Size = 0;

		for(INT NodeIndex = 0;NodeIndex < Model->Nodes.Num();NodeIndex++)
		{
			FBspNode&	Node = Model->Nodes(NodeIndex);
			FBspSurf&	Surf = Model->Surfs(Node.iSurf);

			NodeBaseVertexIndices.AddItem(Num());

			if(Node.NumVertices)
			{
				Size += Stride * Node.NumVertices;

				if(Surf.PolyFlags & PF_TwoSided)
				{
					BackSideVertexIndices.Set(NodeIndex,Num());
					Size += Stride * Node.NumVertices;
				}
			}
		}

	}

	// GetData

	virtual void GetData(void* Buffer)
	{
		FModelVertex*	DestVertex = (FModelVertex*)Buffer;

		for(UINT NodeIndex = 0;NodeIndex < (UINT)Model->Nodes.Num();NodeIndex++)
		{
			FBspNode&	Node = Model->Nodes(NodeIndex);
			FBspSurf&	Surf = Model->Surfs(Node.iSurf);

			if(Node.NumVertices)
			{
				FVector	TextureBase = Model->Points(Surf.pBase),
						TextureX = Model->Vectors(Surf.vTextureU),
						TextureY = Model->Vectors(Surf.vTextureV),
						Normal = Model->Vectors(Surf.vNormal);

				for(UINT VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
				{
					const FVector& Position = Model->Points(Model->Verts(Node.iVertPool + VertexIndex).pVertex);
					DestVertex->Position = Position;
					DestVertex->U = ((Position - TextureBase) | TextureX) / 128.0f;
					DestVertex->V = ((Position - TextureBase) | TextureY) / 128.0f;
					DestVertex->TangentX = TextureX.SafeNormal();
					DestVertex->TangentY = TextureY.SafeNormal();
					DestVertex->TangentZ = Normal.SafeNormal();
					DestVertex++;
				}

				if(Surf.PolyFlags & PF_TwoSided)
				{
					for(INT VertexIndex = Node.NumVertices - 1;VertexIndex >= 0;VertexIndex--)
					{
						const FVector& Position = Model->Points(Model->Verts(Node.iVertPool + VertexIndex).pVertex);
						DestVertex->Position = Position;
						DestVertex->U = ((Position - TextureBase) | TextureX) / 128.0f;
						DestVertex->V = ((Position - TextureBase) | TextureY) / 128.0f;
						DestVertex->TangentX = TextureX.SafeNormal();
						DestVertex->TangentY = TextureY.SafeNormal();
						DestVertex->TangentZ = -Normal.SafeNormal();
						DestVertex++;
					}
				}
			}
		}

	}
};

//
//	FModelLightVertex
//

struct FModelLightVertex
{
	FVector			Position;
	FPackedNormal	TangentX,
					TangentY,
					TangentZ;
	FVector2D		TexCoord,
					LightMapTexCoord;
};

//
//	FStaticModelLightVertexBuffer
//

struct FStaticModelLightVertexBuffer : FVertexBuffer
{
	UModel*				Model;
	FStaticModelLight*	Light;
	TArray<UINT>		NodeVertexMap; // Maps a node index to the base vertex index.
	TMap<INT,UINT>		BackSideVertexMap;

	// Constructor.

	FStaticModelLightVertexBuffer() {}
	FStaticModelLightVertexBuffer(UModel* InModel,FStaticModelLight* InLight):
		Model(InModel),
		Light(InLight)
	{
		Stride = sizeof(FModelLightVertex);
		Size = 0;

		NodeVertexMap.Empty(Model->Nodes.Num());
		for(UINT NodeIndex = 0;NodeIndex < (UINT)Model->Nodes.Num();NodeIndex++)
            NodeVertexMap.AddItem(INDEX_NONE);

		for(UINT TextureIndex = 0;TextureIndex < (UINT)Light->Textures.Num();TextureIndex++)
		{
			FStaticModelLightTexture&	Texture = Light->Textures(TextureIndex);

			for(UINT SurfaceIndex = 0;SurfaceIndex < (UINT)Texture.Surfaces.Num();SurfaceIndex++)
			{
				FStaticModelLightSurface&	LightSurface = Texture.Surfaces(SurfaceIndex);

				for(UINT NodeIndex = 0;NodeIndex < (UINT)LightSurface.Nodes.Num();NodeIndex++)
				{
					FBspNode&	Node = Model->Nodes(LightSurface.Nodes(NodeIndex));
					FBspSurf&	Surf = Model->Surfs(Node.iSurf);

					NodeVertexMap(LightSurface.Nodes(NodeIndex)) = Num();
					Size += Node.NumVertices * Stride;

					if(Surf.PolyFlags & PF_TwoSided)
					{
						BackSideVertexMap.Set(LightSurface.Nodes(NodeIndex),Num());
						Size += Node.NumVertices * Stride;
					}
				}
			}
		}
	}

	// FVertexBuffer interface.

	virtual void GetData(void* Buffer)
	{
		FModelLightVertex*	DestVertex = (FModelLightVertex*)Buffer;

		for(UINT TextureIndex = 0;TextureIndex < (UINT)Light->Textures.Num();TextureIndex++)
		{
			FStaticModelLightTexture&	Texture = Light->Textures(TextureIndex);

			for(UINT SurfaceIndex = 0;SurfaceIndex < (UINT)Texture.Surfaces.Num();SurfaceIndex++)
			{
				FStaticModelLightSurface&	LightSurface = Texture.Surfaces(SurfaceIndex);
				FMatrix						WorldToLightMap = LightSurface.WorldToTexture();
				FLOAT						SurfaceBaseU = ((FLOAT)LightSurface.BaseX + 0.5f) / (FLOAT)Texture.SizeX,
											SurfaceBaseV = ((FLOAT)LightSurface.BaseY + 0.5f) / (FLOAT)Texture.SizeY,
											SurfaceScaleU = (FLOAT)(LightSurface.SizeX - 1) / (FLOAT)Texture.SizeX,
											SurfaceScaleV = (FLOAT)(LightSurface.SizeY - 1) / (FLOAT)Texture.SizeY;

				for(UINT NodeIndex = 0;NodeIndex < (UINT)LightSurface.Nodes.Num();NodeIndex++)
				{
					FBspNode&	Node = Model->Nodes(LightSurface.Nodes(NodeIndex));
					FBspSurf&	Surf = Model->Surfs(Node.iSurf);
					FVector		TextureBase = Model->Points(Surf.pBase),
								TextureX = Model->Vectors(Surf.vTextureU),
								TextureY = Model->Vectors(Surf.vTextureV),
								Normal = Model->Vectors(Surf.vNormal);

					for(UINT VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
					{
						const FVector& Position = Model->Points(Model->Verts(Node.iVertPool + VertexIndex).pVertex);
						DestVertex->Position = Position;
						DestVertex->TangentX = TextureX.SafeNormal();
						DestVertex->TangentY = TextureY.SafeNormal();
						DestVertex->TangentZ = Normal.SafeNormal();
						DestVertex->TexCoord.X = ((Position - TextureBase) | TextureX) / 128.0f;
						DestVertex->TexCoord.Y = ((Position - TextureBase) | TextureY) / 128.0f;
						FVector LightMapTexCoord = WorldToLightMap.TransformFVector(Position);
						DestVertex->LightMapTexCoord = FVector2D(SurfaceBaseU + LightMapTexCoord.X * SurfaceScaleU,SurfaceBaseV + LightMapTexCoord.Y * SurfaceScaleV);
						DestVertex++;
					}

					if(Surf.PolyFlags & PF_TwoSided)
					{
						for(INT VertexIndex = Node.NumVertices - 1;VertexIndex >= 0;VertexIndex--)
						{
							const FVector& Position = Model->Points(Model->Verts(Node.iVertPool + VertexIndex).pVertex);
							DestVertex->Position = Position;
							DestVertex->TangentX = TextureX.SafeNormal();
							DestVertex->TangentY = TextureY.SafeNormal();
							DestVertex->TangentZ = -Normal.SafeNormal();
							DestVertex->TexCoord.X = ((Position - TextureBase) | TextureX) / 128.0f;
							DestVertex->TexCoord.Y = ((Position - TextureBase) | TextureY) / 128.0f;
							FVector LightMapTexCoord = WorldToLightMap.TransformFVector(Position);
							DestVertex->LightMapTexCoord = FVector2D(SurfaceBaseU + LightMapTexCoord.X * SurfaceScaleU,SurfaceBaseV + LightMapTexCoord.Y * SurfaceScaleV);
							DestVertex++;
						}
					}
				}
			}
		}
	}
};

//
//	FStaticModelLightIndexBuffer
//

struct FStaticModelLightIndexBuffer: FIndexBuffer
{
	UModel*							Model;
	TArray<FNodeVertexIndices>		Nodes;
	UINT							NumTriangles;
	FStaticModelLightVertexBuffer*	VertexBuffer;

	// Constructor.

	FStaticModelLightIndexBuffer(UModel* InModel,FStaticModelLightVertexBuffer* InVertexBuffer):
		Model(InModel),
		VertexBuffer(InVertexBuffer),
		NumTriangles(0)
	{
		Stride = sizeof(UINT);
		Size = 0;
		Dynamic = 1;
	}

	// AddNode

	void AddNode(const FNodeVertexIndices& Node)
	{
		Nodes.AddItem(Node);
		Size += (Node.NumVertices - 2) * 3 * Stride;
		NumTriangles += (Node.NumVertices - 2);
	}

	// FIndexBuffer interface.

	virtual void GetData(void* Buffer)
	{
		UINT*	DestIndex = (UINT*)Buffer;

		for(INT NodeIndex = 0;NodeIndex < Nodes.Num();NodeIndex++)
		{
			FNodeVertexIndices&	Node = Nodes(NodeIndex);

			for(UINT VertexIndex = 2;VertexIndex < Node.NumVertices;VertexIndex++)
			{
				*DestIndex++ = Node.BaseVertexIndex;
				*DestIndex++ = Node.BaseVertexIndex + VertexIndex;
				*DestIndex++ = Node.BaseVertexIndex + VertexIndex - 1;
			}
		}

		check((BYTE*)DestIndex - (BYTE*)Buffer == Size);
	}
};

//
//	FStaticModelLightBatch
//

struct FStaticModelLightBatch
{
	UMaterialInstance*			Material;
	UINT						TextureIndex;
	TArray<UINT>				Nodes;
	TArray<FNodeVertexIndices>	PortalNodes;
	FRawIndexBuffer32				IndexBuffer;
};

//
//	FStaticModelLightPrimitive
//

struct FStaticModelLightPrimitive: FStaticLightPrimitive
{
	UModelComponent*				Component;
	FStaticModelLight*				Light;
	FLocalVertexFactory				VertexFactory;
	FStaticModelLightVertexBuffer*	VertexBuffer;
	TArray<FStaticModelLightBatch>	Batches;
	UBOOL							InvalidSurfaceData;

	// Constructor.

	FStaticModelLightPrimitive() {}
	FStaticModelLightPrimitive(UModelComponent* InComponent,FStaticModelLight* InLight):
		Component(InComponent),
		Light(InLight),
		VertexBuffer(NULL),
		InvalidSurfaceData(1)
	{}

	// InvalidateSurfaceData

	void InvalidateSurfaceData()
	{
		InvalidSurfaceData = 1;
		Batches.Empty();
		delete VertexBuffer;
		VertexBuffer = NULL;
	}

	// GetBatch

	FStaticModelLightBatch* GetBatch(UMaterialInstance* SurfMaterial,UINT TextureIndex)
	{
		for(INT BatchIndex = 0;BatchIndex < Batches.Num();BatchIndex++)
		{
			if(Batches(BatchIndex).Material == SurfMaterial && Batches(BatchIndex).TextureIndex == TextureIndex)
			{
				return &Batches(BatchIndex);
			}
		}

		FStaticModelLightBatch* Batch = new(Batches) FStaticModelLightBatch;
		Batch->Material = SurfMaterial;
		Batch->TextureIndex = TextureIndex;
		return Batch;
	}

	// BuildSurfaceData

	void BuildSurfaceData()
	{
		// Create the vertex buffer.

		VertexBuffer = new FStaticModelLightVertexBuffer(Component->Model,Light);

		VertexFactory.PositionComponent = FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FModelLightVertex,Position),VCT_Float3);
		VertexFactory.TangentBasisComponents[0] = FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FModelLightVertex,TangentX),VCT_PackedNormal);
		VertexFactory.TangentBasisComponents[1] = FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FModelLightVertex,TangentY),VCT_PackedNormal);
		VertexFactory.TangentBasisComponents[2] = FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FModelLightVertex,TangentZ),VCT_PackedNormal);
		VertexFactory.TextureCoordinates.Num = 0;
		VertexFactory.TextureCoordinates.AddItem(FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FModelLightVertex,TexCoord),VCT_Float2));
		VertexFactory.LightMapCoordinateComponent = FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FModelLightVertex,LightMapTexCoord),VCT_Float2);
		GResourceManager->UpdateResource(&VertexFactory);

		// Build batches from groups of nodes with matching materials.

		for(UINT TextureIndex = 0;TextureIndex < (UINT)Light->Textures.Num();TextureIndex++)
		{
			FStaticModelLightTexture&	Texture = Light->Textures(TextureIndex);

			for(UINT SurfaceIndex = 0;SurfaceIndex < (UINT)Texture.Surfaces.Num();SurfaceIndex++)
			{
				FStaticModelLightSurface&	LightSurface = Texture.Surfaces(SurfaceIndex);
				FStaticModelLightBatch*		Batch = GetBatch(Component->Model->Surfs(LightSurface.SurfaceIndex).Material,TextureIndex);

				for(UINT NodeIndex = 0;NodeIndex < (UINT)LightSurface.Nodes.Num();NodeIndex++)
				{
					FBspNode&	Node = Component->Model->Nodes(LightSurface.Nodes(NodeIndex));
					FBspSurf&	Surf = Component->Model->Surfs(Node.iSurf);

					if(!(Surf.PolyFlags & PF_Portal))
					{
						Batch->Nodes.AddItem(LightSurface.Nodes(NodeIndex));

						UINT	BaseVertexIndex = VertexBuffer->NodeVertexMap(LightSurface.Nodes(NodeIndex));
						for(UINT VertexIndex = 2;VertexIndex < Node.NumVertices;VertexIndex++)
						{
							Batch->IndexBuffer.Indices.AddItem(BaseVertexIndex);
							Batch->IndexBuffer.Indices.AddItem(BaseVertexIndex + VertexIndex);
							Batch->IndexBuffer.Indices.AddItem(BaseVertexIndex + VertexIndex - 1);
							Batch->IndexBuffer.Size += Batch->IndexBuffer.Stride * 3;
						}
					}
					else
					{
						UBOOL	IsFrontVisible = Node.iZone[1] == Component->ZoneIndex,
								IsBackVisible = Node.iZone[0] == Component->ZoneIndex;

						if(IsFrontVisible)
						{
							Batch->PortalNodes.AddItem(FNodeVertexIndices(Node.iSurf,VertexBuffer->NodeVertexMap(LightSurface.Nodes(NodeIndex)),Node.NumVertices));
						}
						if(IsBackVisible)
						{
							Batch->PortalNodes.AddItem(FNodeVertexIndices(Node.iSurf,VertexBuffer->BackSideVertexMap.FindRef(LightSurface.Nodes(NodeIndex)),Node.NumVertices));
						}
					}
				}
			}
		}

		InvalidSurfaceData = 0;
	}

	// FStaticLightPrimitive interface.

	virtual void Render(const FSceneContext& Context,FPrimitiveViewInterface* PVI,FStaticLightPrimitiveRenderInterface* PRI)
	{
		if(Context.View->ShowFlags & SHOW_BSP)
		{
			if(InvalidSurfaceData)
				BuildSurfaceData();

			if(Context.View->ShowFlags & SHOW_Selection)
			{
				for(INT Selected = 0;Selected < 2;Selected++)
				{
					for(UINT BatchIndex = 0;BatchIndex < (UINT)Batches.Num();BatchIndex++)
					{
						FStaticModelLightBatch&	Batch = Batches(BatchIndex);
						FMaterialInstance*		MaterialInstances[2];
						FMaterial*				Materials[2] =
						{
							Component->GetMaterialResources(Context,Batch.Material,0,MaterialInstances[0]),
							Component->GetMaterialResources(Context,Batch.Material,1,MaterialInstances[1]),
						};

						// Build an index buffer containing only the nodes in the batch which are either selected or unselected.

						FStaticModelLightIndexBuffer	IndexBuffer(Component->Model,VertexBuffer);

						for(UINT NodeIndex = 0;NodeIndex < (UINT)Batch.Nodes.Num();NodeIndex++)
						{
							FBspNode&	Node = Component->Model->Nodes(Batch.Nodes(NodeIndex));
							FBspSurf&	Surf = Component->Model->Surfs(Node.iSurf);
							UBOOL		SurfSelected = (Surf.PolyFlags & PF_Selected) ? 1 : 0;

							if(SurfSelected == Selected)
								IndexBuffer.AddNode(FNodeVertexIndices(Node.iSurf,VertexBuffer->NodeVertexMap(Batch.Nodes(NodeIndex)),Node.NumVertices));
						}

						if(!(Context.View->ShowFlags & SHOW_Portals))
						{
							for(INT NodeIndex = 0;NodeIndex < Batch.PortalNodes.Num();NodeIndex++)
							{
								FBspSurf&	Surf = Component->Model->Surfs(Batch.PortalNodes(NodeIndex).SurfaceIndex);
								UBOOL		SurfSelected = (Surf.PolyFlags & PF_Selected) ? 1 : 0;

								if(SurfSelected == Selected)
									IndexBuffer.AddNode(Batch.PortalNodes(NodeIndex));
							}
						}

						// Render the nodes.

						if(IndexBuffer.NumTriangles)
						{
							PRI->DrawVisibilityTexture(
								&VertexFactory,
								&IndexBuffer,
								Materials[Selected],
								MaterialInstances[Selected],
								&Light->Textures(Batch.TextureIndex),
								0,
								IndexBuffer.NumTriangles,
								0,
								VertexBuffer->Num() - 1
								);
						}
					}
				}
			}
			else
			{
				for(UINT BatchIndex = 0;BatchIndex < (UINT)Batches.Num();BatchIndex++)
				{
					FMaterialInstance*	MaterialInstance;
					FMaterial*			Material = Component->GetMaterialResources(Context,Batches(BatchIndex).Material,0,MaterialInstance);
					if(Batches(BatchIndex).IndexBuffer.Num())
					{
						PRI->DrawVisibilityTexture(
							&VertexFactory,
							&Batches(BatchIndex).IndexBuffer,
							Material,
							MaterialInstance,
							&Light->Textures(Batches(BatchIndex).TextureIndex),
							0,
							Batches(BatchIndex).IndexBuffer.Num() / 3,
							0,
							VertexBuffer->Num() - 1
							);
					}
					if(!(Context.View->ShowFlags & SHOW_Portals))
					{
						FStaticModelLightIndexBuffer	IndexBuffer(Component->Model,VertexBuffer);

						for(INT NodeIndex = 0;NodeIndex < Batches(BatchIndex).PortalNodes.Num();NodeIndex++)
						{
							IndexBuffer.AddNode(Batches(BatchIndex).PortalNodes(NodeIndex));
						}

						// Render the nodes.

						if(IndexBuffer.Num() && IndexBuffer.NumTriangles)
						{
							PRI->DrawVisibilityTexture(
								&VertexFactory,
								&IndexBuffer,
								Material,
								MaterialInstance,
								&Light->Textures(Batches(BatchIndex).TextureIndex),
								0,
								IndexBuffer.NumTriangles,
								0,
								VertexBuffer->Num() - 1
								);
						}
					}
				}
			}
		}
	}

	virtual UPrimitiveComponent* GetSource() { return Component; }

	void Precache(FPrecacheInterface* P)
	{
		if(InvalidSurfaceData)
			BuildSurfaceData();

		if(VertexBuffer->Num())
			P->CacheResource(&VertexFactory);

		for(UINT BatchIndex = 0;BatchIndex < (UINT)Batches.Num();BatchIndex++)
			if(Batches(BatchIndex).IndexBuffer.Num())
				P->CacheResource(&Batches(BatchIndex).IndexBuffer);

		for(UINT TextureIndex = 0;TextureIndex < (UINT)Light->Textures.Num();TextureIndex++)
			P->CacheResource(&Light->Textures(TextureIndex));
	}
};

//
//	FModelNodeIndexBuffer
//

struct FModelNodeIndexBuffer: FIndexBuffer
{
	UModel*				Model;
	FModelVertexBuffer*	VertexBuffer;

	TArray<FNodeVertexIndices>	Nodes;
	UINT						NumTriangles;

	// Constructor.

	FModelNodeIndexBuffer(UModel* InModel,FModelVertexBuffer* InVertexBuffer):
		Model(InModel),
		VertexBuffer(InVertexBuffer),
		NumTriangles(0)
	{
		Stride = sizeof(UINT);
		Size = 0;
		Dynamic = 1;
	}

	// AddNode

	void AddNode(const FNodeVertexIndices& Node)
	{
		Nodes.AddItem(Node);
		Size += (Node.NumVertices - 2) * 3 * Stride;
		NumTriangles += Node.NumVertices - 2;
	}

	// FIndexBuffer interface.

	virtual void GetData(void* Buffer)
	{
		UINT*	DestIndex = (UINT*)Buffer;

		for(UINT NodeIndex = 0;NodeIndex < (UINT)Nodes.Num();NodeIndex++)
		{
			FNodeVertexIndices&	Node = Nodes(NodeIndex);
			for(UINT VertexIndex = 2;VertexIndex < Node.NumVertices;VertexIndex++)
			{
				*DestIndex++ = Node.BaseVertexIndex;
				*DestIndex++ = Node.BaseVertexIndex + VertexIndex;
				*DestIndex++ = Node.BaseVertexIndex + VertexIndex - 1;
			}
		}

		check((BYTE*)DestIndex - (BYTE*)Buffer == Size);
	}
};

//
//	FindEdgeIndex
//

INT FindEdgeIndex(TArray<FMeshEdge>& Edges,UModel* Model,FMeshEdge& Edge)
{
	for(INT EdgeIndex = 0;EdgeIndex < Edges.Num();EdgeIndex++)
	{
		FMeshEdge&	OtherEdge = Edges(EdgeIndex);

		if(OtherEdge.Vertices[0] != Edge.Vertices[1])
			continue;

		if(OtherEdge.Vertices[1] != Edge.Vertices[0])
			continue;

		if(OtherEdge.Faces[1] != INDEX_NONE)
			continue;

		OtherEdge.Faces[1] = Edge.Faces[0];
		return EdgeIndex;
	}

	new(Edges) FMeshEdge(Edge);

	return Edges.Num() - 1;

}

//
//	FModelBatch
//

struct FModelBatch
{
	UMaterialInstance*			Material;
	TArray<FNodeVertexIndices>	Nodes;
	TArray<FNodeVertexIndices>	PortalNodes;
	FRawIndexBuffer32			IndexBuffer;
};

//
//	FModelMeshObject
//

struct FModelMeshObject
{
	UModel*				Model;
	UModelComponent*	ModelComponent;
	FLocalVertexFactory	VertexFactory;
	FModelVertexBuffer*	VertexBuffer;

	TArray<FModelBatch>	Batches;

	TIndirectArray<FStaticModelLightPrimitive>	StaticLightPrimitives;

	FShadowVertexBuffer			ShadowVertexBuffer;
	FLocalShadowVertexFactory	ShadowVertexFactory;
	TArray<FMeshEdge>			ShadowEdges;

	UINT	NumVertices;
	UBOOL	InvalidSurfaceData;

	// Constructor.

	FModelMeshObject(UModelComponent* InModelComponent):
		Model(InModelComponent->Model),
		ModelComponent(InModelComponent),
		VertexBuffer(NULL),
		NumVertices(0),
		InvalidSurfaceData(1)
	{
		ShadowVertexFactory.LocalToWorld = InModelComponent->LocalToWorld;
		ShadowVertexFactory.PositionComponent = FVertexStreamComponent(&ShadowVertexBuffer,STRUCT_OFFSET(FShadowVertex,Position),VCT_Float3);
		ShadowVertexFactory.ExtrusionComponent = FVertexStreamComponent(&ShadowVertexBuffer,STRUCT_OFFSET(FShadowVertex,Extrusion),VCT_Float1);
		GResourceManager->UpdateResource(&ShadowVertexFactory);

		// Create the static light primitives.

		for(INT LightIndex = 0;LightIndex < ModelComponent->StaticLights.Num();LightIndex++)
			new(StaticLightPrimitives) FStaticModelLightPrimitive(ModelComponent,&ModelComponent->StaticLights(LightIndex));

		// Build shadow data.

		for(UINT NodeIndex = 0;NodeIndex < (UINT)Model->Nodes.Num();NodeIndex++)
		{
			FBspNode&	Node = Model->Nodes(NodeIndex);
			FBspSurf&	Surf = Model->Surfs(Node.iSurf);

			if((Node.iZone[1] == ModelComponent->ZoneIndex || ModelComponent->ZoneIndex == INDEX_NONE) && Node.NumVertices && !(Surf.PolyFlags & PF_TwoSided))
			{
				for(UINT VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
				{
					FMeshEdge	Edge;

					Edge.Faces[0] = NodeIndex;
					Edge.Faces[1] = INDEX_NONE;

					Edge.Vertices[0] = Model->Verts(Node.iVertPool + VertexIndex).pVertex;
					Edge.Vertices[1] = Model->Verts(Node.iVertPool + ((VertexIndex + 1) % Node.NumVertices)).pVertex;

					FindEdgeIndex(ShadowEdges,Model,Edge);
				}
			}
		}

		for(UINT PointIndex = 0;PointIndex < (UINT)Model->Points.Num();PointIndex++)
			ShadowVertexBuffer.Vertices.AddItem(Model->Points(PointIndex));

		ShadowVertexBuffer.Size = ShadowVertexBuffer.Stride * ShadowVertexBuffer.Vertices.Num() * 2;
		GResourceManager->UpdateResource(&ShadowVertexBuffer);
	}

	// InvalidateSurfaceData

	void InvalidateSurfaceData()
	{
		InvalidSurfaceData = 1;
		Batches.Empty();
		delete VertexBuffer;
		VertexBuffer = NULL;
		for(UINT LightIndex = 0;LightIndex < (UINT)StaticLightPrimitives.Num();LightIndex++)
			StaticLightPrimitives(LightIndex).InvalidateSurfaceData();
	}

	// GetBatch

	FModelBatch* GetBatch(UMaterialInstance* SurfMaterial)
	{
		for(INT BatchIndex = 0;BatchIndex < Batches.Num();BatchIndex++)
		{
			if(Batches(BatchIndex).Material == SurfMaterial)
			{
				return &Batches(BatchIndex);
			}
		}

		FModelBatch* Batch = new(Batches) FModelBatch;
		Batch->Material = SurfMaterial;
		return Batch;
	}

	// BuildSurfaceData

	void BuildSurfaceData()
	{
		// Build a vertex buffer for the model.

		VertexBuffer = new FModelVertexBuffer(ModelComponent->Model);
		NumVertices = VertexBuffer->Num();

		VertexFactory.LocalToWorld = ModelComponent->LocalToWorld;
		VertexFactory.WorldToLocal = ModelComponent->LocalToWorld.Inverse();
		VertexFactory.PositionComponent = FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FModelVertex,Position),VCT_Float3);
		VertexFactory.TangentBasisComponents[0] = FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FModelVertex,TangentX),VCT_PackedNormal);
		VertexFactory.TangentBasisComponents[1] = FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FModelVertex,TangentY),VCT_PackedNormal);
		VertexFactory.TangentBasisComponents[2] = FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FModelVertex,TangentZ),VCT_PackedNormal);
		VertexFactory.TextureCoordinates.Num = 0;
		VertexFactory.TextureCoordinates.AddItem(FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FModelVertex,U),VCT_Float2));
		GResourceManager->UpdateResource(&VertexFactory);

		// Find all unique materials on the model.

		for(INT NodeIndex = 0;NodeIndex < Model->Nodes.Num();NodeIndex++)
		{
			FBspNode&	Node = Model->Nodes(NodeIndex);
			FBspSurf&	Surf = Model->Surfs(Node.iSurf);

			if(Node.NumVertices)
			{
				if(Surf.PolyFlags & PF_Portal)
				{
					UBOOL	IsFrontVisible = Node.iZone[1] == ModelComponent->ZoneIndex,
							IsBackVisible = Node.iZone[0] == ModelComponent->ZoneIndex;

					if(IsFrontVisible)
					{
						FModelBatch* Batch = GetBatch(Surf.Material);
						UINT BaseVertexIndex = VertexBuffer->NodeBaseVertexIndices(NodeIndex);

						Batch->PortalNodes.AddItem(FNodeVertexIndices(Node.iSurf,BaseVertexIndex,Node.NumVertices));
					}

					if(IsBackVisible)
					{
						FModelBatch* Batch = GetBatch(Surf.Material);
						UINT BaseVertexIndex = VertexBuffer->BackSideVertexIndices.FindRef(NodeIndex);

						Batch->PortalNodes.AddItem(FNodeVertexIndices(Node.iSurf,BaseVertexIndex,Node.NumVertices));
					}
				}
				else if((Node.iZone[1] == ModelComponent->ZoneIndex || ModelComponent->ZoneIndex == INDEX_NONE) && !(Surf.PolyFlags & PF_Invisible))
				{
					FModelBatch* Batch = GetBatch(Surf.Material);
					UINT BaseVertexIndex = VertexBuffer->NodeBaseVertexIndices(NodeIndex);

					Batch->Nodes.AddItem(FNodeVertexIndices(Node.iSurf,BaseVertexIndex,Node.NumVertices));

					for(UINT VertexIndex = 2;VertexIndex < Node.NumVertices;VertexIndex++)
					{
						Batch->IndexBuffer.Indices.AddItem(BaseVertexIndex);
						Batch->IndexBuffer.Indices.AddItem(BaseVertexIndex + VertexIndex);
						Batch->IndexBuffer.Indices.AddItem(BaseVertexIndex + VertexIndex - 1);
						Batch->IndexBuffer.Size += Batch->IndexBuffer.Stride * 3;
					}
				}
			}
		}

		InvalidSurfaceData = 0;
	}
};

//
//	UModelComponent::GetLayerMask
//

DWORD UModelComponent::GetLayerMask(const FSceneContext& Context) const
{
	DWORD	LayerMask = 0;
	if((Context.View->ShowFlags & SHOW_BSP) && MeshObject)
	{
		if(MeshObject->InvalidSurfaceData)
			MeshObject->BuildSurfaceData();

		for(INT BatchIndex = 0;BatchIndex < MeshObject->Batches.Num();BatchIndex++)
		{
			FModelBatch&	Batch = MeshObject->Batches(BatchIndex);
			if(Batch.IndexBuffer.Num() || Batch.PortalNodes.Num())
			{
				if(Batch.Material)
					LayerMask |= Batch.Material->GetLayerMask();
				else
					LayerMask |= GEngine->DefaultMaterial->GetLayerMask();
			}
		}
	}
	return LayerMask;
}

FMaterial* UModelComponent::GetMaterialResources(const FSceneContext& Context,UMaterialInstance* SurfMaterial,UBOOL Selected,FMaterialInstance*& OutMaterialInstance) const
{
	if(Context.View->ShowFlags & SHOW_ZoneColors)
	{
		static FMaterialInstance ZoneColorInstance;

		FLinearColor ZoneColor;
		if(ZoneIndex == 0)
			ZoneColor = FColor(255,255,255);
		else
			ZoneColor = FColor((ZoneIndex * 67) & 255,(ZoneIndex * 1371) & 255,(ZoneIndex * 1991) & 255);

		ZoneColorInstance.VectorValueMap.Set(NAME_Color,ZoneColor);

		OutMaterialInstance = &ZoneColorInstance;
		return GEngine->ColoredNodeMaterial->GetMaterialInterface(Selected);
	}
	else if(SurfMaterial)
	{
		OutMaterialInstance = SurfMaterial->GetInstanceInterface();
		return SurfMaterial->GetMaterialInterface(Selected);
	}
	else
	{
		OutMaterialInstance = GEngine->DefaultMaterial->GetInstanceInterface();
		return GEngine->DefaultMaterial->GetMaterialInterface(Selected);
	}
}

//
//	UModelComponent::Render
//

void UModelComponent::Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	if((Context.View->ShowFlags & SHOW_BSP) && Context.View->ViewMode != SVM_BrushWireframe && MeshObject)
	{
		if(MeshObject->InvalidSurfaceData)
			MeshObject->BuildSurfaceData();

		for(UINT BatchIndex = 0;BatchIndex < (UINT)MeshObject->Batches.Num();BatchIndex++)
		{
			FModelBatch&		Batch = MeshObject->Batches(BatchIndex);	
			FMaterialInstance*	MaterialInstances[2];
			FMaterial*			Materials[2] =
			{
				GetMaterialResources(Context,Batch.Material,0,MaterialInstances[0]),
				GetMaterialResources(Context,Batch.Material,1,MaterialInstances[1]),
			};

			if(!PRI->IsMaterialRelevant(Materials[0]))
				continue;


			if(Context.View->ViewMode == SVM_Wireframe)
			{
				if(Batch.IndexBuffer.Num())
				{
					PRI->DrawWireframe(
						&MeshObject->VertexFactory,
						&Batch.IndexBuffer,
						WT_TriList,
						FColor(255,255,255),
						0,
						Batch.IndexBuffer.Num() / 3,
						0,
						MeshObject->NumVertices - 1,
						0
						);

					GEngineStats.BSPTriangles.Value += Batch.IndexBuffer.Num() / 3;
				}
			}
			else if(Context.View->ShowFlags & SHOW_Selection)
			{
				for(INT Selected = 0;Selected < ((Context.View->ShowFlags & SHOW_Selection) ? 2 : 1);Selected++)
				{
					FModelNodeIndexBuffer IndexBuffer(Model,MeshObject->VertexBuffer);

					for(INT NodeIndex = 0;NodeIndex < Batch.Nodes.Num();NodeIndex++)
					{
						UBOOL NodeSelected = (Model->Surfs(Batch.Nodes(NodeIndex).SurfaceIndex).PolyFlags & PF_Selected) ? 1 : 0;
						if(NodeSelected == Selected)
							IndexBuffer.AddNode(Batch.Nodes(NodeIndex));
					}

					if(!(Context.View->ShowFlags & SHOW_Portals))
					{
						for(INT NodeIndex = 0;NodeIndex < Batch.PortalNodes.Num();NodeIndex++)
						{
							UBOOL NodeSelected = (Model->Surfs(Batch.PortalNodes(NodeIndex).SurfaceIndex).PolyFlags & PF_Selected) ? 1 : 0;
							if(NodeSelected == Selected)
								IndexBuffer.AddNode(Batch.PortalNodes(NodeIndex));
						}
					}

					if(IndexBuffer.NumTriangles && MeshObject->NumVertices)
					{
						PRI->DrawMesh(
							&MeshObject->VertexFactory,
							&IndexBuffer,
							Materials[Selected],
							MaterialInstances[Selected],
							0,
							IndexBuffer.NumTriangles,
							0,
							MeshObject->NumVertices - 1
							);

						GEngineStats.BSPTriangles.Value += IndexBuffer.NumTriangles;
					}
				}
			}
			else
			{
				if(Batch.IndexBuffer.Num())
				{
				    PRI->DrawMesh(
					    &MeshObject->VertexFactory,
					    &Batch.IndexBuffer,
					    Materials[0],
					    MaterialInstances[0],
					    0,
					    Batch.IndexBuffer.Num() / 3,
					    0,
					    MeshObject->NumVertices - 1
					    );

					GEngineStats.BSPTriangles.Value += Batch.IndexBuffer.Num() / 3;
				}
				if(!(Context.View->ShowFlags & SHOW_Portals))
				{
					FModelNodeIndexBuffer IndexBuffer(Model,MeshObject->VertexBuffer);
					for(INT NodeIndex = 0;NodeIndex < Batch.PortalNodes.Num();NodeIndex++)
					{
						IndexBuffer.AddNode(Batch.PortalNodes(NodeIndex));
					}
					if(IndexBuffer.NumTriangles && MeshObject->NumVertices)
					{
						PRI->DrawMesh(
							&MeshObject->VertexFactory,
							&IndexBuffer,
							Materials[0],
							MaterialInstances[0],
							0,
							IndexBuffer.NumTriangles,
							0,
							MeshObject->NumVertices - 1
							);

						GEngineStats.BSPTriangles.Value += IndexBuffer.NumTriangles;
					}
				}
			}
		}
	}
}

//
//	UModelComponent::RenderShadowVolume
//

void UModelComponent::RenderShadowVolume(const FSceneContext& Context,FShadowRenderInterface* SRI,ULightComponent* Light)
{
	if((Context.View->ShowFlags & SHOW_BSP) && MeshObject && MeshObject->NumVertices)
	{
		FCycleCounterSection	CycleCounter(GEngineStats.ShadowTime);
		EShadowStencilMode		Mode = SSM_ZFail;
		FCachedShadowVolume*	CachedShadowVolume = GetCachedShadowVolume(Light);

		if(!CachedShadowVolume)
		{
			CachedShadowVolume = new FCachedShadowVolume;
			CachedShadowVolume->Light = Light;
			CachedShadowVolumes.AddItem(CachedShadowVolume);

			FPlane	LightPosition = LocalToWorld.Inverse().TransformFPlane(Light->GetPosition());
			_WORD	FirstExtrudedVertex = MeshObject->ShadowVertexBuffer.Vertices.Num();
			FLOAT*	PlaneDots = new FLOAT[Model->Nodes.Num()];

			for(INT NodeIndex = 0;NodeIndex < Model->Nodes.Num();NodeIndex++)
				PlaneDots[NodeIndex] = Model->Nodes(NodeIndex).Plane | FPlane(LightPosition.X,LightPosition.Y,LightPosition.Z,-LightPosition.W);

			for(INT NodeIndex = 0;NodeIndex < Model->Nodes.Num();NodeIndex++)
			{
				FBspNode&	Node = Model->Nodes(NodeIndex);
				FBspSurf&	Surf = Model->Surfs(Node.iSurf);
				if((Node.iZone[1] == ZoneIndex || ZoneIndex == INDEX_NONE) && Node.NumVertices && !(Surf.PolyFlags & PF_TwoSided))
				{
					if(IsNegativeFloat(PlaneDots[NodeIndex]))
					{
						for(UINT VertexIndex = 2;VertexIndex < Node.NumVertices;VertexIndex++)
						{
							CachedShadowVolume->IndexBuffer.AddFace(
								Model->Verts(Node.iVertPool).pVertex,
								Model->Verts(Node.iVertPool + VertexIndex - 1).pVertex,
								Model->Verts(Node.iVertPool + VertexIndex).pVertex
								);
							CachedShadowVolume->IndexBuffer.AddFace(
								FirstExtrudedVertex + Model->Verts(Node.iVertPool).pVertex,
								FirstExtrudedVertex + Model->Verts(Node.iVertPool + VertexIndex).pVertex,
								FirstExtrudedVertex + Model->Verts(Node.iVertPool + VertexIndex - 1).pVertex
								);
						}
					}
				}
			}

			for(UINT EdgeIndex = 0;EdgeIndex < (UINT)MeshObject->ShadowEdges.Num();EdgeIndex++)
			{
				FMeshEdge&	Edge = MeshObject->ShadowEdges(EdgeIndex);
				if(Edge.Faces[1] != INDEX_NONE)
				{
					if(IsNegativeFloat(PlaneDots[Edge.Faces[0]]) == IsNegativeFloat(PlaneDots[Edge.Faces[1]]))
						continue;
				}
				else if(!IsNegativeFloat(PlaneDots[Edge.Faces[0]]))
					continue;

				CachedShadowVolume->IndexBuffer.AddEdge(
					IsNegativeFloat(PlaneDots[Edge.Faces[0]]) ? Edge.Vertices[0] : Edge.Vertices[1],
					IsNegativeFloat(PlaneDots[Edge.Faces[0]]) ? Edge.Vertices[1] : Edge.Vertices[0],
					FirstExtrudedVertex
					);
			}

			delete [] PlaneDots;

			CachedShadowVolume->IndexBuffer.CalcSize();
		}

		GEngineStats.ShadowTriangles.Value += CachedShadowVolume->IndexBuffer.Indices.Num() / 3;
		if(CachedShadowVolume->IndexBuffer.Indices.Num())
			SRI->DrawShadowVolume(&MeshObject->ShadowVertexFactory,&CachedShadowVolume->IndexBuffer,0,CachedShadowVolume->IndexBuffer.Indices.Num() / 3,0,MeshObject->ShadowVertexBuffer.Vertices.Num() * 2 - 1,Mode);
	}
}

//
//	UModelComponent::RenderShadowDepth
//

void UModelComponent::RenderShadowDepth(const FSceneContext& Context,struct FPrimitiveRenderInterface* PRI)
{
	if((Context.View->ShowFlags & SHOW_BSP) && MeshObject && MeshObject->NumVertices)
	{
		for(UINT BatchIndex = 0;BatchIndex < (UINT)MeshObject->Batches.Num();BatchIndex++)
		{
			FModelBatch&	Batch = MeshObject->Batches(BatchIndex);
			if(Batch.IndexBuffer.Num())
			{
				PRI->DrawMesh(
					&MeshObject->VertexFactory,
					&Batch.IndexBuffer,
					GEngine->DefaultMaterial->GetMaterialInterface(0),
					GEngine->DefaultMaterial->GetInstanceInterface(),
					0,
					Batch.IndexBuffer.Num() / 3,
					0,
					MeshObject->NumVertices - 1
					);
			}
		}
	}
}

//
//	UModelComponent::RenderHitTest
//

void UModelComponent::RenderHitTest(const FSceneContext& Context,struct FPrimitiveRenderInterface* PRI)
{
	if((Context.View->ShowFlags & SHOW_BSP) && Context.View->ViewMode != SVM_BrushWireframe && MeshObject && MeshObject->NumVertices)
	{
		for(UINT BatchIndex = 0;BatchIndex < (UINT)MeshObject->Batches.Num();BatchIndex++)
		{
			FModelBatch&	Batch = MeshObject->Batches(BatchIndex);
			if(Context.View->ViewMode == SVM_Wireframe)
			{
				PRI->SetHitProxy(NULL);
				if(Batch.IndexBuffer.Num())
				{
					PRI->DrawWireframe(
						&MeshObject->VertexFactory,
						&Batch.IndexBuffer,
						WT_TriList,
						FColor(255,255,255),
						0,
						Batch.IndexBuffer.Num() / 3,
						0,
						MeshObject->NumVertices - 1,
						0
						);
				}
			}
			else
			{
				for(INT NodeIndex = 0;NodeIndex < Batch.Nodes.Num();NodeIndex++)
				{
					FBspSurf&				Surf = Model->Surfs(Batch.Nodes(NodeIndex).SurfaceIndex);
					FMaterialInstance*		MaterialInstance;
					FMaterial*				Material = GetMaterialResources(Context,Batch.Material,0,MaterialInstance);
					FModelNodeIndexBuffer	IndexBuffer(Model,MeshObject->VertexBuffer);

					IndexBuffer.AddNode(Batch.Nodes(NodeIndex));

					PRI->SetHitProxy(new HBspSurf(Model,Batch.Nodes(NodeIndex).SurfaceIndex,0));
					PRI->DrawMesh(
						&MeshObject->VertexFactory,
						&IndexBuffer,
						Material,
						MaterialInstance,
						0,
						IndexBuffer.NumTriangles,
						0,
						MeshObject->NumVertices - 1
						);
				}
				if(!(Context.View->ShowFlags & SHOW_Portals))
				{
					for(INT NodeIndex = 0;NodeIndex < Batch.PortalNodes.Num();NodeIndex++)
					{
						FBspSurf&				Surf = Model->Surfs(Batch.PortalNodes(NodeIndex).SurfaceIndex);
						FMaterialInstance*		MaterialInstance;
						FMaterial*				Material = GetMaterialResources(Context,Batch.Material,0,MaterialInstance);
						FModelNodeIndexBuffer	IndexBuffer(Model,MeshObject->VertexBuffer);

						IndexBuffer.AddNode(Batch.PortalNodes(NodeIndex));

						PRI->SetHitProxy(new HBspSurf(Model,Batch.PortalNodes(NodeIndex).SurfaceIndex,0));
						PRI->DrawMesh(
							&MeshObject->VertexFactory,
							&IndexBuffer,
							Material,
							MaterialInstance,
							0,
							IndexBuffer.NumTriangles,
							0,
							MeshObject->NumVertices - 1
							);
					}
				}
			}
		}

		PRI->SetHitProxy(NULL);
	}
}

EStaticLightingAvailability UModelComponent::GetStaticLightPrimitive(ULightComponent* Light,FStaticLightPrimitive*& OutStaticLightPrimitive)
{
	if(MeshObject && Light->HasStaticShadowing())
	{
		for(INT LightIndex = 0;LightIndex < MeshObject->StaticLightPrimitives.Num();LightIndex++)
		{
			FStaticModelLight* StaticLight = &StaticLights(LightIndex);
			if(StaticLight->Light == Light)
			{
				if(StaticLight->Textures.Num())
				{
					OutStaticLightPrimitive = &MeshObject->StaticLightPrimitives(LightIndex);
					return SLA_Available;
				}
				return SLA_Shadowed;
			}
		}
	}

	return SLA_Unavailable;
}

//
//	UModelComponent::UpdateBounds
//

void UModelComponent::UpdateBounds()
{
	if(Model)
	{
		FBox	BoundingBox(0);
		for(UINT NodeIndex = 0;NodeIndex < (UINT)Model->Nodes.Num();NodeIndex++)
		{
			if(Model->Nodes(NodeIndex).iZone[1] == ZoneIndex || ZoneIndex == INDEX_NONE)
			{
				for(UINT VertexIndex = 0;VertexIndex < (UINT)Model->Nodes(NodeIndex).NumVertices;VertexIndex++)
					BoundingBox += Model->Points(Model->Verts(Model->Nodes(NodeIndex).iVertPool + VertexIndex).pVertex);
			}
		}
		Bounds = FBoxSphereBounds(BoundingBox.TransformBy(LocalToWorld));
	}
	else
		Super::UpdateBounds();
}

void UModelComponent::GetZoneMask(UModel* Model)
{
	if(ZoneIndex != INDEX_NONE)
		ZoneMask = FZoneSet::IndividualZone(ZoneIndex);
	else
		ZoneMask = FZoneSet::AllZones();
}

//
//	UModelComponent::Created
//

void UModelComponent::Created()
{
	if(Model)
		MeshObject = new FModelMeshObject(this);

	Super::Created();

}

//
//	UModelComponent::Update
//

void UModelComponent::Update()
{
	Super::Update();

	if(MeshObject)
	{
		MeshObject->VertexFactory.LocalToWorld = LocalToWorld;
		GResourceManager->UpdateResource(&MeshObject->VertexFactory);
	}
}

//
//	UModelComponent::Destroyed
//

void UModelComponent::Destroyed()
{
	Super::Destroyed();

	if(MeshObject)
	{
		delete MeshObject;
		MeshObject = NULL;
	}
}

//
//	UModelComponent::Precache
//

void UModelComponent::Precache(FPrecacheInterface* P)
{
	if(MeshObject)
	{
		for(UINT LightIndex = 0;LightIndex < (UINT)MeshObject->StaticLightPrimitives.Num();LightIndex++)
			MeshObject->StaticLightPrimitives(LightIndex).Precache(P);

		if(MeshObject->InvalidSurfaceData)
			MeshObject->BuildSurfaceData();

		for(UINT BatchIndex = 0;BatchIndex < (UINT)MeshObject->Batches.Num();BatchIndex++)
		{
			if(MeshObject->Batches(BatchIndex).IndexBuffer.Num())
				P->CacheResource(&MeshObject->Batches(BatchIndex).IndexBuffer);
			if( MeshObject->Batches(BatchIndex).Material )
				P->CacheResource(MeshObject->Batches(BatchIndex).Material->GetMaterialInterface(0));
		}

		if(MeshObject->NumVertices)
		{
			P->CacheResource(&MeshObject->VertexFactory);
			P->CacheResource(&MeshObject->ShadowVertexFactory);
		}
	}
}

//
//	UModelComponent::InvalidateSurfaces
//

void UModelComponent::InvalidateSurfaces()
{
	if(Initialized)
	{
		check(MeshObject);
		MeshObject->InvalidateSurfaceData();
	}

}

//
//	FModelWireVertexBuffer
//

struct FModelWireVertexBuffer: FVertexBuffer
{
	UModel*	Model;

	// Constructor.

	FModelWireVertexBuffer(UModel* InModel):
		Model(InModel)
	{
		Stride = sizeof(FVector);
		Size = 0;

		for(UINT PolyIndex = 0;PolyIndex < (UINT)Model->Polys->Element.Num();PolyIndex++)
			Size += Stride * Model->Polys->Element(PolyIndex).NumVertices;
	}

	// GetData

	virtual void GetData(void* Buffer)
	{
		FVector*	DestVertex = (FVector*)Buffer;

		for(UINT PolyIndex = 0;PolyIndex < (UINT)Model->Polys->Element.Num();PolyIndex++)
		{
			FPoly&	Poly = Model->Polys->Element(PolyIndex);
			for(UINT VertexIndex = 0;VertexIndex < (UINT)Poly.NumVertices;VertexIndex++)
				*DestVertex++ = Poly.Vertex[VertexIndex];
		}
	}
};

//
//	FModelWireIndexBuffer
//

struct FModelWireIndexBuffer: FIndexBuffer
{
	UModel*	Model;

	// Constructor.

	FModelWireIndexBuffer(UModel* InModel):
		Model(InModel)
	{
		Stride = sizeof(_WORD);
		Size = 0;
		for(UINT PolyIndex = 0;PolyIndex < (UINT)Model->Polys->Element.Num();PolyIndex++)
			Size += Stride * Model->Polys->Element(PolyIndex).NumVertices * 2;
	}

	// GetData

	virtual void GetData(void* Buffer)
	{
		_WORD*	DestIndex = (_WORD*)Buffer;
		_WORD	BaseIndex = 0;

        for(UINT PolyIndex = 0;PolyIndex < (UINT)Model->Polys->Element.Num();PolyIndex++)
		{
			FPoly&	Poly = Model->Polys->Element(PolyIndex);
			for(UINT VertexIndex = 0;VertexIndex < (UINT)Poly.NumVertices;VertexIndex++)
			{
				*DestIndex++ = BaseIndex + VertexIndex;
				*DestIndex++ = BaseIndex + ((VertexIndex + 1) % Poly.NumVertices);
			}
			BaseIndex += Poly.NumVertices;
		}
	}
};

//
//	FBrushWireObject
//

struct FBrushWireObject
{
	FLocalVertexFactory		VertexFactory;
	FModelWireIndexBuffer	WireIndexBuffer;
	FModelWireVertexBuffer	WireVertexBuffer;

	UINT	NumTriangles,
			NumVertices;

	// Constructor.

	FBrushWireObject(UModel* InModel):
		WireIndexBuffer(InModel),
		WireVertexBuffer(InModel)
	{
		VertexFactory.PositionComponent = FVertexStreamComponent(&WireVertexBuffer,0,VCT_Float3);

		NumTriangles = WireIndexBuffer.Num() / 2;
		NumVertices = WireVertexBuffer.Num();
	}
};

//
//	UBrushComponent::Created
//

void UBrushComponent::Created()
{
	Super::Created();

	WireObject = new FBrushWireObject(Brush);
	WireObject->VertexFactory.LocalToWorld = LocalToWorld;
	WireObject->VertexFactory.WorldToLocal = LocalToWorld.Inverse();
	GResourceManager->UpdateResource(&WireObject->VertexFactory);

}

//
//	UBrushComponent::Update
//

void UBrushComponent::Update()
{
	Super::Update();

	WireObject->VertexFactory.LocalToWorld = LocalToWorld;
	GResourceManager->UpdateResource(&WireObject->VertexFactory);

}

//
//	UBrushComponent::Destroyed
//

void UBrushComponent::Destroyed()
{
	Super::Destroyed();

	delete WireObject;
	WireObject = NULL;

}

static UBOOL OwnerIsActiveBrush(const UBrushComponent* BrushComp)
{
	if(!BrushComp->Owner)
	{
		return false;
	}

	check(BrushComp->Owner->XLevel);

	// In the Editor, Actors(1) is the active brush. If this BrushComponent belongs to it, then mark it as being in all zones.
	AActor* ActiveBrush = NULL;
	if( BrushComp->Owner->XLevel->Actors(1) )
	{
		ActiveBrush = BrushComp->Owner->XLevel->Actors(1);
		check(Cast<ABrush>(ActiveBrush)!=NULL);
		check(Cast<ABrush>(ActiveBrush)->BrushComponent!=NULL);
		check(Cast<ABrush>(ActiveBrush)->Brush!=NULL);
	}

	if(BrushComp->Owner == ActiveBrush)
	{
		return true;
	}
	else
	{
		return false;
	}
}

//
//	UBrushComponent::GetLayerMask
//

DWORD UBrushComponent::GetLayerMask(const FSceneContext& Context) const
{
	if(Owner && ( OwnerIsActiveBrush(this) || ((Context.View->ShowFlags & SHOW_Volumes) && Owner->IsA(AVolume::StaticClass())) || GSelectionTools.IsSelected(Owner) ))
		return PLM_Foreground;
	else
		return Super::GetLayerMask(Context);
}

//
//	UBrushComponent::Visible
//

UBOOL UBrushComponent::Visible(struct FSceneView* View)
{
	if( (View->ShowFlags & SHOW_Collision) && CollideActors )
	{
		return true;
	}
	else
	{
		return Super::Visible(View);
	}
}


//
//	UBrushComponent::Render
//

void UBrushComponent::Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	if(WireObject && WireObject->WireVertexBuffer.Size && WireObject->WireIndexBuffer.Size)
	{
		// Determine the color of the brush.

		FColor	Color = GEngine->C_BrushWire;
		ABrush*	BrushOwner = Cast<ABrush>(Owner);
		if(BrushOwner)
		{
			if(BrushOwner->IsStaticBrush())
			{
				Color = BrushOwner->GetWireColor();
			}
			else if(BrushOwner->IsVolumeBrush())
			{
				Color =	BrushOwner->bColored ? BrushOwner->BrushColor : GEngine->C_Volume;
			}

			// If the editor is in a state where drawing the brush wireframe isn't desired, bail out.

			if( !GEngine->ShouldDrawBrushWireframe( BrushOwner ) )
			{
				return;
			}
		}

		// Render the brush wireframe.

		if( Context.View->ViewMode == SVM_BrushWireframe || ((Context.View->ShowFlags & SHOW_Collision) && CollideActors) || Owner->IsAShape() )
		{
			PRI->DrawWireframe(&WireObject->VertexFactory,&WireObject->WireIndexBuffer,WT_LineList,SelectedColor(Context,Color),0,WireObject->NumTriangles,0,WireObject->NumVertices - 1,0);
		}
	}
}

//
//	UBrushComponent::RenderForeground
//

void UBrushComponent::RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	if(WireObject && WireObject->WireVertexBuffer.Size && WireObject->WireIndexBuffer.Size)
	{
		// Determine the color of the brush.

		FColor	Color = GEngine->C_BrushWire;
		ABrush*	BrushOwner = Cast<ABrush>(Owner);
		if(BrushOwner)
		{
			if(BrushOwner->IsStaticBrush())
			{
				Color = BrushOwner->GetWireColor();
			}
			else if(BrushOwner->IsVolumeBrush())
			{
				Color =	BrushOwner->bColored ? BrushOwner->BrushColor : GEngine->C_Volume;
			}

			// If the editor is in a state where drawing the brush wireframe isn't desired, bail out.

			if( !GEngine->ShouldDrawBrushWireframe( BrushOwner ) )
			{
				return;
			}
		}

		// Render the brush wireframe.

		if(GIsEditor && Owner && Owner == Owner->XLevel->Brush())
		{
			PRI->DrawWireframe(
				&WireObject->VertexFactory,
				&WireObject->WireIndexBuffer,
				WT_LineList,
				SelectedColor(Context,GEngine->C_BrushWire),
				0,
				WireObject->NumTriangles,
				0,
				WireObject->NumVertices - 1,
				1);
		}
		// Draw volume if view flag is selected, or any brush when selected.
		else if(GIsEditor && Owner && ((Context.View->ShowFlags & SHOW_Volumes) && Owner->IsA(AVolume::StaticClass()) || GSelectionTools.IsSelected( Owner )) )
		{
			PRI->DrawWireframe(
				&WireObject->VertexFactory,
				&WireObject->WireIndexBuffer,
				WT_LineList,
				SelectedColor(Context,Color),
				0,
				WireObject->NumTriangles,
				0,
				WireObject->NumVertices - 1,
				0);
		}
	}
}

//
//	UBrushComponent::LineCheck
//

UBOOL UBrushComponent::LineCheck(FCheckResult &Result,
								 const FVector& End,
								 const FVector& Start,
								 const FVector& Extent,
								 DWORD TraceFlags)
{
	UBOOL res = Brush->LineCheck(Result, Owner, End, Start, Extent, TraceFlags);

	// If we hit something, fill in the component.
	if(!res)
		Result.Component = this;

	return res;

}

//
//	UBrushComponent::PointCheck
//

UBOOL UBrushComponent::PointCheck(FCheckResult&	Result,
								  const FVector& Location,
								  const FVector& Extent)
{
	UBOOL res = Brush->PointCheck(Result, Owner, Location, Extent);

	if(!res)
		Result.Component = this;

	return res;

}

//
//	UBrushComponent::UpdateBounds
//

void UBrushComponent::UpdateBounds()
{
	if(Brush && Brush->Polys && Brush->Polys->Element.Num())
	{
		TArray<FVector> Points;
		for( INT i=0; i<Brush->Polys->Element.Num(); i++ )
			for( INT j=0; j<Brush->Polys->Element(i).NumVertices; j++ )
				Points.AddItem(Brush->Polys->Element(i).Vertex[j]);
		Bounds = FBoxSphereBounds( &Points(0), Points.Num() ).TransformBy(LocalToWorld);
	}
	else
		Super::UpdateBounds();

}

void UBrushComponent::GetZoneMask(UModel* Model)
{
	if(Owner && ( OwnerIsActiveBrush(this) || GSelectionTools.IsSelected( Owner ) ))
		ZoneMask = FZoneSet::AllZones();
	else
		Super::GetZoneMask(Model);
}