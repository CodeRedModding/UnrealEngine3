/*=============================================================================
	UnStaticMeshRender.cpp: Static mesh rendering code.
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"

//
//	FStaticMeshLightPrimitive
//

struct FStaticMeshLightPrimitive: FStaticLightPrimitive, FVertexBuffer
{
	UStaticMeshComponent*	Component;
	FStaticMeshLight*		Light;
	FLocalVertexFactory		VertexFactory;

	// Constructor.

	FStaticMeshLightPrimitive() {}
	FStaticMeshLightPrimitive(UStaticMeshComponent* InComponent,FStaticMeshLight* InLight):
		Component(InComponent),
		Light(InLight)
	{
		Stride = sizeof(FLOAT);
		Size = Component->StaticMesh->Vertices.Num() * Stride;

		VertexFactory.LocalToWorld = Component->LocalToWorld;
		VertexFactory.WorldToLocal = Component->LocalToWorld.Inverse();
		VertexFactory.PositionComponent = FVertexStreamComponent(&Component->StaticMesh->PositionVertexBuffer,STRUCT_OFFSET(FStaticMeshVertex,Position),VCT_Float3);
		VertexFactory.TangentBasisComponents[0] = FVertexStreamComponent(&Component->StaticMesh->TangentVertexBuffer,sizeof(FPackedNormal) * 0,VCT_PackedNormal);
		VertexFactory.TangentBasisComponents[1] = FVertexStreamComponent(&Component->StaticMesh->TangentVertexBuffer,sizeof(FPackedNormal) * 1,VCT_PackedNormal);
		VertexFactory.TangentBasisComponents[2] = FVertexStreamComponent(&Component->StaticMesh->TangentVertexBuffer,sizeof(FPackedNormal) * 2,VCT_PackedNormal);
		VertexFactory.TextureCoordinates.Num = 0;
		for(UINT TexCoordIndex = 0;TexCoordIndex < (UINT)Component->StaticMesh->UVBuffers.Num();TexCoordIndex++)
			VertexFactory.TextureCoordinates.AddItem(FVertexStreamComponent(&Component->StaticMesh->UVBuffers(TexCoordIndex),0,VCT_Float2));

		VertexFactory.LightMaskComponent = FVertexStreamComponent(this,0,VCT_Float1);

		GResourceManager->UpdateResource(&VertexFactory);
	}

	// FVertexBuffer interface.

	virtual void GetData(void* Buffer)
	{
		Light->Visibility.Load();
		appMemcpy(Buffer,&Light->Visibility(0),sizeof(FLOAT) * Light->Visibility.Num());
		Light->Visibility.Unload();
	}

	// FStaticLightPrimitive interface.

	virtual void Render(const FSceneContext& Context,FPrimitiveViewInterface* PVI,FStaticLightPrimitiveRenderInterface* PRI)
	{
		if(Context.View->ShowFlags & SHOW_StaticMeshes)
		{
			for(UINT MaterialIndex = 0;MaterialIndex < (UINT)Component->StaticMesh->Materials.Num();MaterialIndex++)
			{
				FStaticMeshMaterial&	Material = Component->StaticMesh->Materials(MaterialIndex);

				GEngineStats.StaticMeshTriangles.Value += Material.NumTriangles;
				if(Material.NumTriangles)
					PRI->DrawVisibilityVertexMask(
						&VertexFactory,
						&Component->StaticMesh->IndexBuffer,
						Component->SelectedMaterial(Context,Component->GetMaterial(MaterialIndex)),
						Component->GetMaterial(MaterialIndex)->GetInstanceInterface(),
						Material.FirstIndex,
						Material.NumTriangles,
						Material.MinVertexIndex,
						Material.MaxVertexIndex,
						Component->LocalToWorldDeterminant > 0.0f ? CM_CW : CM_CCW
						);
			}
		}
	}

	virtual UPrimitiveComponent* GetSource() { return Component; }
};

//
//	FStaticMeshLightMapPrimitive
//

struct FStaticMeshLightMapPrimitive : FStaticLightPrimitive
{
	UStaticMeshComponent*	Component;
	UShadowMap*				LightMap;

	// Constructor.

	FStaticMeshLightMapPrimitive() {}
	FStaticMeshLightMapPrimitive(UStaticMeshComponent* InComponent,UShadowMap* InLightMap):
		Component(InComponent),
		LightMap(InLightMap)
	{}

	// FStaticLightPrimitive interface.

	virtual void Render(const FSceneContext& Context,FPrimitiveViewInterface* PVI,FStaticLightPrimitiveRenderInterface* PRI);
	virtual UPrimitiveComponent* GetSource() { return Component; }
};

//
//	FStaticMeshObject
//

struct FStaticMeshObject
{
	FLocalVertexFactory			VertexFactory;
	FLocalShadowVertexFactory	ShadowVertexFactory;
	UStaticMesh*				StaticMesh;
	UStaticMeshComponent*		Component;

	TIndirectArray<FStaticMeshLightPrimitive>		StaticLightPrimitives;
	TIndirectArray<FStaticMeshLightMapPrimitive>	StaticLightMapPrimitives;

	// Constructor.

	FStaticMeshObject(UStaticMesh* InStaticMesh,UStaticMeshComponent* InComponent):
		StaticMesh(InStaticMesh),
		Component(InComponent)
	{
		VertexFactory.PositionComponent = FVertexStreamComponent(&StaticMesh->PositionVertexBuffer,STRUCT_OFFSET(FStaticMeshVertex,Position),VCT_Float3);
		VertexFactory.TangentBasisComponents[0] = FVertexStreamComponent(&StaticMesh->TangentVertexBuffer,sizeof(FPackedNormal) * 0,VCT_PackedNormal);
		VertexFactory.TangentBasisComponents[1] = FVertexStreamComponent(&StaticMesh->TangentVertexBuffer,sizeof(FPackedNormal) * 1,VCT_PackedNormal);
		VertexFactory.TangentBasisComponents[2] = FVertexStreamComponent(&StaticMesh->TangentVertexBuffer,sizeof(FPackedNormal) * 2,VCT_PackedNormal);

		if(StaticMesh->LightMapCoordinateIndex >= 0 && StaticMesh->LightMapCoordinateIndex < StaticMesh->UVBuffers.Num())
			VertexFactory.LightMapCoordinateComponent = FVertexStreamComponent(&StaticMesh->UVBuffers(StaticMesh->LightMapCoordinateIndex),0,VCT_Float2);

		for(UINT TexCoordIndex = 0;TexCoordIndex < (UINT)StaticMesh->UVBuffers.Num();TexCoordIndex++)
			VertexFactory.TextureCoordinates.AddItem(FVertexStreamComponent(&StaticMesh->UVBuffers(TexCoordIndex),0,VCT_Float2));

		ShadowVertexFactory.PositionComponent = FVertexStreamComponent(&StaticMesh->PositionVertexBuffer,STRUCT_OFFSET(FShadowVertex,Position),VCT_Float3);
		ShadowVertexFactory.ExtrusionComponent = FVertexStreamComponent(&StaticMesh->PositionVertexBuffer,STRUCT_OFFSET(FShadowVertex,Extrusion),VCT_Float1);

		// Create the static light primitives.

		for(INT LightIndex = 0;LightIndex < Component->StaticLights.Num();LightIndex++)
		{
			new(StaticLightPrimitives) FStaticMeshLightPrimitive(Component,&Component->StaticLights(LightIndex));
		}
		
		for(INT LightIndex = 0;LightIndex < Component->StaticLightMaps.Num();LightIndex++)
		{
			new(StaticLightMapPrimitives) FStaticMeshLightMapPrimitive(Component,Component->StaticLightMaps(LightIndex));
		}
	}
};

//
//	UStaticMeshComponent::Created
//

void UStaticMeshComponent::Created()
{
	MeshObject = new FStaticMeshObject(StaticMesh,this);
	MeshObject->VertexFactory.LocalToWorld = LocalToWorld;
	MeshObject->VertexFactory.WorldToLocal = LocalToWorld.Inverse();
	MeshObject->ShadowVertexFactory.LocalToWorld = LocalToWorld;
	GResourceManager->UpdateResource(&MeshObject->VertexFactory);
	GResourceManager->UpdateResource(&MeshObject->ShadowVertexFactory);

	Super::Created();
}

//
//	UStaticMeshComponent::Update
//

void UStaticMeshComponent::Update()
{
	Super::Update();

	MeshObject->VertexFactory.LocalToWorld = LocalToWorld;
	MeshObject->VertexFactory.WorldToLocal = LocalToWorld.Inverse();
	MeshObject->ShadowVertexFactory.LocalToWorld = LocalToWorld;
	GResourceManager->UpdateResource(&MeshObject->VertexFactory);
	GResourceManager->UpdateResource(&MeshObject->ShadowVertexFactory);

	for(UINT LightIndex = 0;LightIndex < (UINT)MeshObject->StaticLightPrimitives.Num();LightIndex++)
	{
		MeshObject->StaticLightPrimitives(LightIndex).VertexFactory.LocalToWorld = LocalToWorld;
		GResourceManager->UpdateResource(&MeshObject->StaticLightPrimitives(LightIndex).VertexFactory);
	}

}

//
//	UStaticMeshComponent::Destroyed
//

void UStaticMeshComponent::Destroyed()
{
	Super::Destroyed();

	delete MeshObject;
	MeshObject = NULL;

}

//
//	UStaticMeshComponent::Visible
//

UBOOL UStaticMeshComponent::Visible(FSceneView* View)
{
	// If 'show static mesh' flag is not set, never render.
	if( !(View->ShowFlags & SHOW_StaticMeshes) )
		return false;

	return Super::Visible(View);
}

//
//	UStaticMeshComponent::GetLayerMask
//

DWORD UStaticMeshComponent::GetLayerMask(const FSceneContext& Context) const
{
	check(IsValidComponent());
	DWORD	LayerMask = 0;
	for(INT MaterialIndex = 0;MaterialIndex < StaticMesh->Materials.Num();MaterialIndex++)
		LayerMask |= GetMaterial(MaterialIndex)->GetLayerMask();
	if((Context.View->ShowFlags & SHOW_Bounds) && (!GIsEditor || !Owner || GSelectionTools.IsSelected( Owner ) ))
		LayerMask |= PLM_Foreground;
	return LayerMask;
}

//
//	UStaticMeshComponent::Render
//

void UStaticMeshComponent::Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	if((Context.View->ViewMode & SVM_WireframeMask) && StaticMesh->WireframeIndexBuffer.Indices.Num())
	{
		PRI->DrawWireframe(
			&MeshObject->VertexFactory,
			&StaticMesh->WireframeIndexBuffer,
			WT_LineList,
			SelectedColor(Context,WireframeColor),
			0,
			StaticMesh->WireframeIndexBuffer.Indices.Num() / 2,
			0,
			StaticMesh->Vertices.Num() - 1
			);
	}
	else
	{
		for(UINT MaterialIndex = 0;MaterialIndex < (UINT)StaticMesh->Materials.Num();MaterialIndex++)
		{
			FStaticMeshMaterial&	Material = StaticMesh->Materials(MaterialIndex);

			check(Material.MinVertexIndex <= Material.MaxVertexIndex);
			check(Material.FirstIndex + Material.NumTriangles * 3 <= StaticMesh->IndexBuffer.Num());

			GEngineStats.StaticMeshTriangles.Value += Material.NumTriangles;
			if(Material.NumTriangles)
				PRI->DrawMesh(
					&MeshObject->VertexFactory,
					&StaticMesh->IndexBuffer,
					SelectedMaterial(Context,GetMaterial(MaterialIndex)),
					GetMaterial(MaterialIndex)->GetInstanceInterface(),
					Material.FirstIndex,
					Material.NumTriangles,
					Material.MinVertexIndex,
					Material.MaxVertexIndex,
					LocalToWorldDeterminant > 0.0f ? CM_CW : CM_CCW
					);
		}
	}

	if(Context.View->ShowFlags & SHOW_Collision)
	{
		if(StaticMesh->CollisionModel)
		{
			for(INT NodeIndex = 0;NodeIndex < StaticMesh->CollisionModel->Nodes.Num(); NodeIndex++)
			{
				FBspNode&	Node = StaticMesh->CollisionModel->Nodes(NodeIndex);

				for(INT VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
					PRI->DrawLine(
						LocalToWorld.TransformFVector(StaticMesh->CollisionModel->Points(StaticMesh->CollisionModel->Verts(Node.iVertPool + VertexIndex).pVertex)),
						LocalToWorld.TransformFVector(StaticMesh->CollisionModel->Points(StaticMesh->CollisionModel->Verts(Node.iVertPool + ((VertexIndex + 1) % Node.NumVertices)).pVertex)),
						GEngine->C_ScaleBoxHi
						);
			}
		}

		if(StaticMesh->BodySetup)
		{
			FMatrix GeomMatrix = LocalToWorld;
			GeomMatrix.RemoveScaling();

			FVector TotalScale(1.f);
			if(Owner)
				TotalScale = Owner->DrawScale * Owner->DrawScale3D;

			// We don't draw convex prims - let the collision model drawing code do that.
			StaticMesh->BodySetup->AggGeom.DrawAggGeom(PRI, GeomMatrix, TotalScale, GEngine->C_ScaleBoxHi, true);
		}
	}
}

//
//	UStaticMeshComponent::RenderForeground
//

void UStaticMeshComponent::RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	if((Context.View->ShowFlags & SHOW_Bounds) && (!GIsEditor || !Owner || GSelectionTools.IsSelected( Owner ) ))
	{
		PRI->DrawWireBox(Bounds.GetBox(), FColor(72,72,255));

		PRI->DrawCircle(Bounds.Origin,FVector(1,0,0),FVector(0,1,0),FColor(255,255,0),Bounds.SphereRadius,32);
		PRI->DrawCircle(Bounds.Origin,FVector(1,0,0),FVector(0,0,1),FColor(255,255,0),Bounds.SphereRadius,32);
		PRI->DrawCircle(Bounds.Origin,FVector(0,1,0),FVector(0,0,1),FColor(255,255,0),Bounds.SphereRadius,32);
	}
}

//
//	UStaticMeshComponent::RenderShadowVolume
//

void UStaticMeshComponent::RenderShadowVolume(const FSceneContext& Context,FShadowRenderInterface* SRI,ULightComponent* Light)
{
	if(Context.View->ShowFlags & SHOW_StaticMeshes)
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
			UINT	NumTriangles = (UINT)StaticMesh->IndexBuffer.Indices.Num() / 3;
			FLOAT*	PlaneDots = new FLOAT[NumTriangles];
			_WORD*	Indices = &StaticMesh->IndexBuffer.Indices(0);
			_WORD	FirstExtrudedVertex = (_WORD)StaticMesh->Vertices.Num();

			for(UINT TriangleIndex = 0;TriangleIndex < NumTriangles;TriangleIndex++)
			{
				const FVector&	V1 = StaticMesh->Vertices(Indices[TriangleIndex * 3 + 0]).Position,
								V2 = StaticMesh->Vertices(Indices[TriangleIndex * 3 + 1]).Position,
								V3 = StaticMesh->Vertices(Indices[TriangleIndex * 3 + 2]).Position;
				PlaneDots[TriangleIndex] = ((V2-V3) ^ (V1-V3)) | (FVector(LightPosition) - V1 * LightPosition.W);
			}

			CachedShadowVolume->IndexBuffer.Indices.Empty(NumTriangles * 3);

			for(UINT TriangleIndex = 0;TriangleIndex < NumTriangles;TriangleIndex++)
			{
				_WORD	Offset = IsNegativeFloat(PlaneDots[TriangleIndex]) ? FirstExtrudedVertex : 0;
				if(Mode == SSM_ZFail || IsNegativeFloat(PlaneDots[TriangleIndex]))
				{
					CachedShadowVolume->IndexBuffer.AddFace(
						Offset + Indices[TriangleIndex * 3 + 0],
						Offset + Indices[TriangleIndex * 3 + 1],
						Offset + Indices[TriangleIndex * 3 + 2]
						);
				}
				if((StaticMesh->DoubleSidedShadowVolumes || StaticMesh->ShadowTriangleDoubleSided(TriangleIndex)) && (Mode == SSM_ZFail || !IsNegativeFloat(PlaneDots[TriangleIndex])))
					CachedShadowVolume->IndexBuffer.AddFace(
						(FirstExtrudedVertex - Offset) + Indices[TriangleIndex * 3 + 2],
						(FirstExtrudedVertex - Offset) + Indices[TriangleIndex * 3 + 1],
						(FirstExtrudedVertex - Offset) + Indices[TriangleIndex * 3 + 0]
						);
			}

			for(UINT EdgeIndex = 0;EdgeIndex < (UINT)StaticMesh->Edges.Num();EdgeIndex++)
			{
				FMeshEdge&	Edge = StaticMesh->Edges(EdgeIndex);
				if(Edge.Faces[1] == INDEX_NONE || IsNegativeFloat(PlaneDots[Edge.Faces[0]]) != IsNegativeFloat(PlaneDots[Edge.Faces[1]]))
				{
					CachedShadowVolume->IndexBuffer.AddEdge(
						Edge.Vertices[IsNegativeFloat(PlaneDots[Edge.Faces[0]]) ? 1 : 0],
						Edge.Vertices[IsNegativeFloat(PlaneDots[Edge.Faces[0]]) ? 0 : 1],
						FirstExtrudedVertex
						);
					if((StaticMesh->DoubleSidedShadowVolumes || StaticMesh->ShadowTriangleDoubleSided(Edge.Faces[0])) && Edge.Faces[1] != INDEX_NONE)
						CachedShadowVolume->IndexBuffer.AddEdge(
							Edge.Vertices[IsNegativeFloat(PlaneDots[Edge.Faces[0]]) ? 1 : 0],
							Edge.Vertices[IsNegativeFloat(PlaneDots[Edge.Faces[0]]) ? 0 : 1],
							FirstExtrudedVertex
							);
				}
			}

			delete [] PlaneDots;

			CachedShadowVolume->IndexBuffer.CalcSize();
		}

		GEngineStats.ShadowTriangles.Value += CachedShadowVolume->IndexBuffer.Indices.Num() / 3;
		SRI->DrawShadowVolume(&MeshObject->ShadowVertexFactory,&CachedShadowVolume->IndexBuffer,0,CachedShadowVolume->IndexBuffer.Indices.Num() / 3,0,StaticMesh->Vertices.Num() * 2 - 1,Mode);
	}

}

EStaticLightingAvailability UStaticMeshComponent::GetStaticLightPrimitive(ULightComponent* Light,FStaticLightPrimitive*& OutStaticLightPrimitive)
{
	check(Initialized);

	if(Light->HasStaticShadowing())
	{
		for(INT LightIndex = 0;LightIndex < StaticLights.Num();LightIndex++)
		{
			FStaticMeshLight* StaticLight = &StaticLights(LightIndex);
			if(StaticLight->Light == Light)
			{
				StaticLight->Visibility.Load();
				if(StaticLight->Visibility.Num() == StaticMesh->Vertices.Num())
				{
					OutStaticLightPrimitive = &MeshObject->StaticLightPrimitives(LightIndex);
					return SLA_Available;
				}
			}
		}

		for(INT LightIndex = 0;LightIndex < StaticLightMaps.Num();LightIndex++)
		{
			UShadowMap* StaticLightMap = StaticLightMaps(LightIndex);
			if(StaticLightMap->Light == Light)
			{
				OutStaticLightPrimitive = &MeshObject->StaticLightMapPrimitives(LightIndex);
				return SLA_Available;
			}
		}

		if(IgnoreLights.FindItemIndex(Light) != INDEX_NONE)
			return SLA_Shadowed;
	}

	return SLA_Unavailable;
}

//
//	FStaticMeshLightMapPrimitive::Render
//

void FStaticMeshLightMapPrimitive::Render(const FSceneContext& Context,FPrimitiveViewInterface* PVI,FStaticLightPrimitiveRenderInterface* PRI)
{
	if(Context.View->ShowFlags & SHOW_StaticMeshes)
	{
		for(UINT MaterialIndex = 0;MaterialIndex < (UINT)Component->StaticMesh->Materials.Num();MaterialIndex++)
		{
			FStaticMeshMaterial&	Material = Component->StaticMesh->Materials(MaterialIndex);

			GEngineStats.StaticMeshTriangles.Value += Material.NumTriangles;
			if(Material.NumTriangles)
				PRI->DrawVisibilityTexture(
					&Component->MeshObject->VertexFactory,
					&Component->StaticMesh->IndexBuffer,
					Component->SelectedMaterial(Context,Component->GetMaterial(MaterialIndex)),
					Component->GetMaterial(MaterialIndex)->GetInstanceInterface(),
					LightMap,
					Material.FirstIndex,
					Material.NumTriangles,
					Material.MinVertexIndex,
					Material.MaxVertexIndex,
					Component->LocalToWorldDeterminant > 0.0f ? CM_CW : CM_CCW
					);
		}
	}
}

//
//	FStaticMeshThumbnailScene
//

struct FStaticMeshThumbnailScene: FPreviewScene
{
	FSceneView	View;

	// Constructor/destructor.

	FStaticMeshThumbnailScene(UStaticMesh* StaticMesh)
	{
		UStaticMeshComponent*	StaticMeshComponent = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass());
		StaticMeshComponent->StaticMesh = StaticMesh;
		StaticMeshComponent->Scene = this;

		FMatrix LockMatrix = FRotationMatrix( FRotator(0,StaticMesh->ThumbnailAngle.Yaw,0) ) * FRotationMatrix( FRotator(0,0,StaticMesh->ThumbnailAngle.Pitch) );

		StaticMeshComponent->SetParentToWorld( FTranslationMatrix(-StaticMesh->Bounds.Origin) * LockMatrix );

		if(StaticMeshComponent->IsValidComponent())
			StaticMeshComponent->Created();
		Components.AddItem(StaticMeshComponent);

		View.ViewMatrix = FTranslationMatrix(FVector(0,StaticMesh->Bounds.SphereRadius / (75.0f * (FLOAT)PI / 360.0f),0));
		View.ViewMatrix = View.ViewMatrix * FInverseRotationMatrix(FRotator(0,16384,0));
		View.ViewMatrix = View.ViewMatrix * FMatrix(
			FPlane(0,	0,	1,	0),
			FPlane(1,	0,	0,	0),
			FPlane(0,	1,	0,	0),
			FPlane(0,	0,	0,	1));
		View.ProjectionMatrix = FPerspectiveMatrix(
			75.0f * (FLOAT)PI / 360.0f,
			1.0f,
			1.0f,
			NEAR_CLIPPING_PLANE
			);
		View.ShowFlags = SHOW_DefaultEditor;
		View.BackgroundColor = FColor(0,0,0);
	}
};

//
//	UStaticMesh::DrawThumbnail
//

void UStaticMesh::DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz )
{
	FStaticMeshThumbnailScene	ThumbnailScene(this);

	InRI->DrawScene(
		FSceneContext(
			&ThumbnailScene.View,
			&ThumbnailScene,
			InX,
			InY,
			InFixedSz ? InFixedSz : 1024 * InZoom,
			InFixedSz ? InFixedSz : 1024 * InZoom
			)
		);
}

//
//	UStaticMesh::GetThumbnailDesc
//

FThumbnailDesc UStaticMesh::GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz )
{
	FThumbnailDesc td;

	td.Width = InFixedSz ? InFixedSz : 1024*InZoom;
	td.Height = InFixedSz ? InFixedSz : 1024*InZoom;
	return td;
}

//
//	UStaticMesh::GetThumbnailLabels
//

INT UStaticMesh::GetThumbnailLabels( TArray<FString>* InLabels )
{
	InLabels->Empty();

	new( *InLabels )FString( GetName() );
	new( *InLabels )FString( FString::Printf( TEXT("%d Triangles"), RawTriangles.Num() ) );

	return InLabels->Num();
}

//
//	UStaticMeshComponent::Precache
//

void UStaticMeshComponent::Precache(FPrecacheInterface* P)
{
	check(Initialized);

	for(UINT LightIndex = 0;LightIndex < (UINT)MeshObject->StaticLightPrimitives.Num();LightIndex++)
		P->CacheResource(&MeshObject->StaticLightPrimitives(LightIndex).VertexFactory);

	for(UINT LightMapIndex = 0;LightMapIndex < (UINT)MeshObject->StaticLightMapPrimitives.Num();LightMapIndex++)
		P->CacheResource(MeshObject->StaticLightMapPrimitives(LightMapIndex).LightMap);
	
	for(UINT MaterialIndex = 0;MaterialIndex < (UINT)StaticMesh->Materials.Num();MaterialIndex++)
		if( GetMaterial(MaterialIndex) )
			P->CacheResource(GetMaterial(MaterialIndex)->GetMaterialInterface(0));

	P->CacheResource(&MeshObject->ShadowVertexFactory);
	P->CacheResource(&MeshObject->VertexFactory);
	P->CacheResource(&StaticMesh->IndexBuffer);
}
