/*=============================================================================
	UnStaticMesh.h: Static mesh class definition.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#define STATICMESH_VERSION 15

//
//	FStaticMeshTriangle
//

struct FStaticMeshTriangle
{
public:

	FVector			Vertices[3];
	FVector2D		UVs[3][8];
	FColor			Colors[3];
	INT				MaterialIndex;
	DWORD			SmoothingMask;
	INT				NumUVs;

	UMaterialInstance*	LegacyMaterial;
	DWORD				LegacyPolyFlags;

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FStaticMeshTriangle& T);
};

//
//	FStaticMeshVertex
//

struct FStaticMeshVertex
{
	FVector			Position;
	FPackedNormal	TangentX,
					TangentY,
					TangentZ;

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FStaticMeshVertex& V)
	{
		Ar << V.Position;
		Ar << V.TangentX << V.TangentY << V.TangentZ;
		return Ar;
	}
};

//
//	FStaticMeshMaterial
//

class FStaticMeshMaterial
{
public:

	UMaterialInstance*		Material;

	UBOOL					EnableCollision,
							OldEnableCollision;

	UINT					FirstIndex,
							NumTriangles,
							MinVertexIndex,
							MaxVertexIndex;

	// Constructor.

	FStaticMeshMaterial(): Material(NULL), EnableCollision(0), OldEnableCollision(0) {}
	FStaticMeshMaterial(UMaterialInstance* InMaterial):
		Material(InMaterial),
		EnableCollision(1),
		OldEnableCollision(1)
	{
		EnableCollision = OldEnableCollision = 1;
	}

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FStaticMeshMaterial& M)
	{
		return Ar	<< M.Material
					<< M.EnableCollision
					<< M.OldEnableCollision
					<< M.FirstIndex
					<< M.NumTriangles
					<< M.MinVertexIndex
					<< M.MaxVertexIndex;
	}
};

//
//	FStaticMeshPositionVertexBuffer
//

struct FStaticMeshPositionVertexBuffer: FVertexBuffer
{
	UStaticMesh*	StaticMesh;

	// Constructor.

	FStaticMeshPositionVertexBuffer(UStaticMesh* InStaticMesh): StaticMesh(InStaticMesh) {}

	// Update - Updates the size/stride of the vertex buffer from the static mesh's vertices array.

	void Update();

	// FVertexBuffer interface.

	virtual void GetData(void* Buffer);
};

//
//	FStaticMeshTangentVertexBuffer
//

struct FStaticMeshTangentVertexBuffer: FVertexBuffer
{
	UStaticMesh*	StaticMesh;

	// Constructor.

	FStaticMeshTangentVertexBuffer(UStaticMesh* InStaticMesh): StaticMesh(InStaticMesh) {}

	// Update - Updates the size/stride of the vertex buffer from the static mesh's vertices array.

	void Update();

	// FVertexBuffer interface.

	virtual void GetData(void* Buffer);
};

//
//	FStaticMeshUVBuffer
//

struct FStaticMeshUVBuffer: FVertexBuffer
{
	TArray<FVector2D>	UVs;

	// Constructor.

	FStaticMeshUVBuffer()
	{
		Stride = sizeof(FVector2D);
		Size = 0;
	}

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FStaticMeshUVBuffer& B)
	{
		Ar << B.UVs << B.Size;
		if(Ar.IsLoading())
			GResourceManager->UpdateResource(&B);
		return Ar;
	}

	// FVertexBuffer interface.

	virtual void GetData(void* Buffer) { appMemcpy(Buffer,&UVs(0),Size); }
};

#include "UnkDOP.h"

//
//	UStaticMesh
//

class UStaticMesh : public UObject
{
	DECLARE_CLASS(UStaticMesh,UObject,CLASS_SafeReplace|CLASS_CollapseCategories,Engine);
public:

	// Rendering data.

	TArray<FStaticMeshVertex>				Vertices;
	FStaticMeshPositionVertexBuffer			PositionVertexBuffer;
	FStaticMeshTangentVertexBuffer			TangentVertexBuffer;
	TArray<FStaticMeshUVBuffer>				UVBuffers;
	FRawIndexBuffer							IndexBuffer;
	FRawIndexBuffer							WireframeIndexBuffer;
	TArray<FStaticMeshMaterial>				Materials;
	TArray<FMeshEdge>						Edges;
	TArray<BYTE>							ShadowTriangleDoubleSided;
	FRotator								ThumbnailAngle;

	INT										LightMapResolution,
											LightMapCoordinateIndex;

	// Collision data.

	UModel*									CollisionModel;
	FkDOPTree								kDOPTree;
	URB_BodySetup*							BodySetup;
	FBoxSphereBounds						Bounds;

	// Artist-accessible options.

	UBOOL									UseSimpleLineCollision,
											UseSimpleBoxCollision,
											UseVertexColor,
											UseSimpleKarmaCollision,
											DoubleSidedShadowVolumes;

	// Source data.

	TLazyArray<FStaticMeshTriangle>			RawTriangles;
	INT										InternalVersion;


	// Constructor.

	UStaticMesh();

	// UObject interface.

	void StaticConstructor();
	virtual void PostEditChange(UProperty* PropertyThatChanged);
	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();
	virtual void Destroy();
	virtual void Rename( const TCHAR* NewName=NULL, UObject* NewOuter=NULL );

	// UStaticMesh interface.

	void Build();

	/**
	 * Returns the scale dependent texture factor used by the texture streaming code.	
	 *
	 * @param RequestedUVIndex UVIndex to look at
	 * @return scale dependent texture factor
	 */
	FLOAT GetStreamingTextureFactor( INT UVIndex );

	virtual FString GetDesc();
	virtual void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz );
	virtual FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz );
	virtual INT GetThumbnailLabels( TArray<FString>* InLabels );
};

//
//	FStaticMeshLight
//

struct FStaticMeshLight
{
	ULightComponent*		Light;
	TLazyArray<FLOAT>		Visibility;

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FStaticMeshLight& L);
};

//
//	UStaticMeshComponent
//

class UStaticMeshComponent : public UMeshComponent
{
	DECLARE_CLASS(UStaticMeshComponent,UMeshComponent,0,Engine);
public:

	UStaticMesh*						StaticMesh;
	struct FStaticMeshObject*			MeshObject;
	FColor								WireframeColor;
	TIndirectArray<FStaticMeshLight>	StaticLights;
	TArray<UShadowMap*>					StaticLightMaps;
	TArray<ULightComponent*>			IgnoreLights;	// Statically irrelevant lights.

	// UMeshComponent interface.

	virtual UMaterialInstance* GetMaterial(INT MaterialIndex) const;

	// UPrimitiveComponent interface.

	virtual void CacheLighting();
	virtual void InvalidateLightingCache(ULightComponent* Light);
	virtual EStaticLightingAvailability GetStaticLightPrimitive(ULightComponent* Light,FStaticLightPrimitive*& OutStaticLightPrimitive);

	virtual UBOOL PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent);
	virtual UBOOL LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags);

	virtual UBOOL Visible(FSceneView* View);
	virtual void UpdateBounds();

	virtual void UpdateWindForces();

	virtual UBOOL IsValidComponent() const;

	// FPrimitiveViewInterface interface.

	virtual DWORD GetLayerMask(const FSceneContext& Context) const;
	virtual void Render(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI);
	virtual void RenderForeground(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI);
	virtual void RenderShadowVolume(const struct FSceneContext& Context,struct FShadowRenderInterface* PRI,ULightComponent* Light);

	// UActorComponent interface.

	virtual void Created();
	virtual void Update();
	virtual void Destroyed();

	virtual void Precache(FPrecacheInterface* P);

	virtual void InvalidateLightingCache();

	// UObject interface.

	virtual void Serialize(FArchive& Ar);
};

//
//	FStaticMeshComponentRecreateContext - Destroys StaticMeshComponents using a given StaticMesh and recreates them when destructed.
//	Used to ensure stale rendering data isn't kept around in the components when importing over or rebuilding an existing static mesh.
//

struct FStaticMeshComponentRecreateContext
{
	UStaticMesh*					StaticMesh;
	TArray<UStaticMeshComponent*>	Components;

	// Constructor/destructor.

	FStaticMeshComponentRecreateContext(UStaticMesh* InStaticMesh):
		StaticMesh(InStaticMesh)
	{
		for(TObjectIterator<UStaticMeshComponent> It;It;++It)
		{
			if(It->StaticMesh == StaticMesh && It->Initialized)
			{
				((UActorComponent*)*It)->InvalidateLightingCache();
				It->Destroyed();
				Components.AddItem(*It);
			}
		}
	}

	~FStaticMeshComponentRecreateContext()
	{
		for(UINT ComponentIndex = 0;ComponentIndex < (UINT)Components.Num();ComponentIndex++)
			Components(ComponentIndex)->Created();
	}
};