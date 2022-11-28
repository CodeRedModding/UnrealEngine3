/*=============================================================================
	UnScene.h: Scene manager definitions.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

//
//	FRawTriangleVertex
//

struct FRawTriangleVertex
{
	FVector		Position,
				TangentX,
				TangentY,
				TangentZ;
	FVector2D	TexCoord;

	// Constructors.

	FRawTriangleVertex() {}
	FRawTriangleVertex( const FVector& InPosition ):
		Position(InPosition),
		TangentX(FVector(1,0,0)),
		TangentY(FVector(0,1,0)),
		TangentZ(FVector(0,0,1)),
		TexCoord(FVector2D(0,0))
	{}
	FRawTriangleVertex(const FVector& InPosition,const FVector& InTangentX,const FVector& InTangentY,const FVector& InTangentZ,const FVector2D& InTexCoord):
		Position(InPosition),
		TangentX(InTangentX),
		TangentY(InTangentY),
		TangentZ(InTangentZ),
		TexCoord(InTexCoord)
	{}
};

//
//	FTriangleRenderInterface - An interface for rendering a batch of arbitrary, code generated triangles with the same material in a non-optimal but easy way.
//

struct FTriangleRenderInterface
{
	// DrawTriangle - Enqueues a triangle for rendering.

	virtual void DrawTriangle(const FRawTriangleVertex& V0,const FRawTriangleVertex& V1,const FRawTriangleVertex& V2) = 0;

	// DrawQuad - Enqueues a triangulated quad for rendering.

	void DrawQuad(const FRawTriangleVertex& V0,const FRawTriangleVertex& V1,const FRawTriangleVertex& V2,const FRawTriangleVertex& V3)
	{
		DrawTriangle(V0,V1,V2);
		DrawTriangle(V0,V2,V3);
	}

	// Finish - Cleans up the interface.  Interface pointer may not be used after calling Finish.

	virtual void Finish() = 0;
};

//
//	EWireframeType - How to interpret the index buffer passed to FPrimitiveRenderInterface::DrawWireframe
//

enum EWireframeType
{
	WT_LineList		= 0,
	WT_TriList		= 1,
	WT_TriStrip		= 2,
};

//
//	ECullMode
//

enum ECullMode
{
	CM_CW,
	CM_CCW
};

//
//	FPrimitiveRenderInterface
//

struct FPrimitiveRenderInterface
{
	// GetViewport - Returns the viewport that's being rendered to.

	virtual FChildViewport* GetViewport() = 0;

	// IsHitTesting - Returns whether this PRI is rendering hit proxies.

	virtual UBOOL IsHitTesting() = 0;

	// IsMaterialRelevant - Returns whether the given material is relevant to the current rendering pass.

	virtual UBOOL IsMaterialRelevant(FMaterial* Material) { return 1; }

	// SetHitProxy - Sets the active hit proxy.

	virtual void SetHitProxy(HHitProxy* HitProxy) {}

	// DrawMesh - Draws a lit triangle mesh.

	virtual void DrawMesh(FVertexFactory* VertexFactory,FIndexBuffer* IndexBuffer,FMaterial* Material,FMaterialInstance* MaterialInstance,UINT FirstIndex,UINT NumTriangles,UINT MinVertexIndex,UINT MaxVertexIndex,ECullMode CullMode = CM_CW) {}

	// DrawVertexLitMesh - Draws a triangle mesh with static per-vertex lighting.

	virtual void DrawVertexLitMesh(FVertexFactory* VertexFactory,FIndexBuffer* IndexBuffer,FMaterial* Material,FMaterialInstance* MaterialInstance,UINT FirstIndex,UINT NumTriangles,UINT MinVertexIndex,UINT MaxVertexIndex,ECullMode CullMode = CM_CW) {}

	// DrawWireframe - Draws unlit line segments.

	virtual void DrawWireframe(FVertexFactory* VertexFactory,FIndexBuffer* IndexBuffer,EWireframeType WireType,FColor Color,UINT FirstIndex,UINT NumPrimitives,UINT MinVertexIndex,UINT MaxVertexIndex,UBOOL DrawVertices = 1) {}

	// DrawSprite - Draws an unlit sprite.

	virtual void DrawSprite(const FVector& Position,FLOAT SizeX,FLOAT SizeY,FTexture2D* Sprite,FColor Color) {}

	// DrawLine - Draws an unlit line segment.

	virtual void DrawLine(const FVector& Start,const FVector& End,FColor Color) {}

	// DrawPoint - Draws an unlit point.

	virtual void DrawPoint(const FVector& Position,FColor Color,FLOAT PointSize) {}

	// Solid shape drawing utility functions. Not really designed for speed - more for debugging.
	// These utilities functions are implemented in UnScene.cpp using GetTRI.

	virtual FTriangleRenderInterface* GetTRI(const FMatrix& LocalToWorld,FMaterial* Material,FMaterialInstance* MaterialInstance);
	void DrawBox(const FMatrix& BoxToWorld, const FVector& Radii, FMaterial* Material,FMaterialInstance* MaterialInstance);
	void DrawSphere(const FVector& Center, const FVector& Radii,  INT NumSides, INT NumRings, FMaterial* Material,FMaterialInstance* MaterialInstance);
	void DrawFrustumBounds(const FMatrix& WorldToFrustum,FMaterial* Material,FMaterialInstance* MaterialInstance);

	// Line drawing utility functions.

	void DrawWireBox(const FBox& Box,FColor Color);
	void DrawCircle(const FVector& Base,const FVector& X,const FVector& Y,FColor Color,FLOAT Radius,INT NumSides);
	void DrawWireCylinder(const FVector& Base,const FVector& X,const FVector& Y,const FVector& Z,FColor Color,FLOAT Radius,FLOAT HalfHeight,INT NumSides);
	void DrawDirectionalArrow(const FMatrix& ArrowToWorld,FColor InColor,FLOAT Length,FLOAT ArrowSize = 1.f);
	void DrawWireStar(const FVector& Position, FLOAT Size, FColor Color);
	void DrawDashedLine(const FVector& Start, const FVector& End, FColor Color, FLOAT DashSize);

	void DrawFrustumWireframe(const FMatrix& WorldToFrustum,FColor Color);
};

//
//	FStaticLightPrimitiveRenderInterface
//

struct FStaticLightPrimitiveRenderInterface
{
	// DrawVisibilityTexture

	virtual void DrawVisibilityTexture(FVertexFactory* VertexFactory,FIndexBuffer* IndexBuffer,FMaterial* Material,FMaterialInstance* MaterialInstance,FTexture2D* VisibilityTexture,UINT FirstIndex,UINT NumTriangles,UINT MinVertexIndex,UINT MaxVertexIndex,ECullMode CullMode = CM_CW) {}

	// DrawVisibilityVertexMask

	virtual void DrawVisibilityVertexMask(FVertexFactory* VertexFactory,FIndexBuffer* IndexBuffer,FMaterial* Material,FMaterialInstance* MaterialInstance,UINT FirstIndex,UINT NumTriangles,UINT MinVertexIndex,UINT MaxVertexIndex,ECullMode CullMode = CM_CW) {}
};

//
//	EShadowStencilMode
//

enum EShadowStencilMode
{
	SSM_ZPass = 0,
	SSM_ZFail = 1
};

//
//	FShadowRenderInterface
//

struct FShadowRenderInterface
{
	// DrawShadowVolume - Draws a shadow volume.

	virtual void DrawShadowVolume(FVertexFactory* VertexFactory,FIndexBuffer* IndexBuffer,UINT FirstIndex,UINT NumTriangles,UINT MinVertexIndex,UINT MaxVertexIndex,EShadowStencilMode Mode) {}
};

//
//	FOutcode - Encapsulates the inside and/or outside state of an intersection test.
//

struct FOutcode
{
private:

	BITFIELD	Inside : 1,
				Outside : 1;

public:

	// Constructor.

	FOutcode():
		Inside(0), Outside(0)
	{}
	FOutcode(UBOOL InInside,UBOOL InOutside):
		Inside(InInside), Outside(InOutside)
	{}

	// Accessors.

	FORCEINLINE void SetInside(UBOOL NewInside) { Inside = NewInside; }
	FORCEINLINE void SetOutside(UBOOL NewOutside) { Outside = NewOutside; }
	FORCEINLINE UBOOL GetInside() const { return Inside; }
	FORCEINLINE UBOOL GetOutside() const { return Outside; }
};

//
//	FConvexVolume
//

struct FConvexVolume
{
public:

	TArray<FPlane>	Planes;

	/**
	 * Clips a polygon to the volume.
	 *
	 * @param	Polygon - The polygon to be clipped.  If the true is returned, contains the
	 *			clipped polygon.
	 *
	 * @return	Returns false if the polygon is entirely outside the volume and true otherwise.
	 */
	UBOOL ClipPolygon(FPoly& Polygon) const;

	// Intersection tests.

	FOutcode GetBoxIntersectionOutcode(const FVector& Origin,const FVector& Extent) const;

	UBOOL IntersectBox(const FVector& Origin,const FVector& Extent) const;
	UBOOL IntersectSphere(const FVector& Origin,const FLOAT Radius) const;

	/**
	 * Creates a convex volume bounding the view frustum for a view-projection matrix.
	 *
	 * @param	ViewProjectionMatrix - The view-projection matrix which defines the view frustum.
	 * @param	UseNearPlane - True if the convex volume should be bounded by the view frustum's near clipping plane.
	 * @param	OutFrustumBounds - The FConvexVolume which contains the view frustum bounds on return.
	 */
    static void GetViewFrustumBounds(const FMatrix& ViewProjectionMatrix,UBOOL UseNearPlane,FConvexVolume& OutFrustumBounds);	
};

//
//	FScene
//

struct FScene
{
	TArray<USkyLightComponent*>					SkyLights;
	TArray<UHeightFogComponent*>				Fogs;
	TArray<UWindPointSourceComponent*>			WindPointSources;
	TArray<UWindDirectionalSourceComponent*>	WindDirectionalSources;

	// AddPrimitive
	
	virtual void AddPrimitive(UPrimitiveComponent* Primitive) = 0;

	// RemovePrimitive

	virtual void RemovePrimitive(UPrimitiveComponent* Primitive) = 0;

	// AddLight

	virtual void AddLight(ULightComponent* Light);

	// RemoveLight

	virtual void RemoveLight(ULightComponent* Light);

	// GetVisiblePrimitives

	virtual void GetVisiblePrimitives(const FSceneContext& Context,const FConvexVolume& Frustum,TArray<UPrimitiveComponent*>& OutPrimitives) = 0;

	// GetRelevantLights

	virtual void GetRelevantLights(UPrimitiveComponent* Primitive,TArray<ULightComponent*>& OutLights) = 0;

	// Serialize

	void Serialize(FArchive& Ar);

	// GetSceneTime

	virtual FLOAT GetSceneTime() { return 0.0f; }
};

//
//	FSceneViewState - A cachable resource for cross-frame scene view state: pupil dilation, etc.
//

struct FSceneViewState : FResource
{
	DECLARE_RESOURCE_TYPE(FSceneViewState,FResource);

	// Constructor.

	FSceneViewState()
	{}
};

//
//	EShowFlags
//

enum EShowFlags
{
	SHOW_Editor			= 0x00000001,
	SHOW_Game			= 0x00000002,
	SHOW_Collision		= 0x00000004,
	SHOW_Grid			= 0x00000008,
	SHOW_Selection		= 0x00000010,
	SHOW_Bounds			= 0x00000020,
	SHOW_StaticMeshes	= 0x00000040,
	SHOW_Terrain		= 0x00000080,
	SHOW_BSP			= 0x00000100,
	SHOW_SkeletalMeshes = 0x00000200,
	SHOW_Constraints	= 0x00000400,
	SHOW_Fog			= 0x00000800,
	SHOW_Foliage		= 0x00001000,
	SHOW_Orthographic	= 0x00002000,
	SHOW_Paths			= 0x00004000,
	SHOW_MeshEdges		= 0x00008000,	// In the filled view modes, render mesh edges as well as the filled surfaces.
	SHOW_LargeVertices	= 0x00010000,	// Displays large clickable icons on static mesh vertices
	SHOW_ZoneColors		= 0x00020000,
	SHOW_Portals		= 0x00040000,
	SHOW_HitProxies		= 0x00080000,	// Draws each hit proxy in the scene with a different color.
	SHOW_ShadowFrustums	= 0x00100000,	// Draws un-occluded shadow frustums as 
	SHOW_ModeWidgets	= 0x00200000,	// Draws mode specific widgets and controls in the viewports (should only be set on viewport clients that are editing the level itself)
	SHOW_KismetRefs		= 0x00400000,	// Draws green boxes around actors in level which are referenced by Kismet. Only works in editor.
	SHOW_Volumes		= 0x00800000,	// Draws Volumes
	SHOW_CamFrustums	= 0x01000000,	// Draws camera frustums

	SHOW_DefaultGame	= SHOW_Game | SHOW_StaticMeshes | SHOW_SkeletalMeshes | SHOW_Terrain | SHOW_Foliage | SHOW_BSP | SHOW_Fog | SHOW_Portals,
	SHOW_DefaultEditor	= SHOW_Editor | SHOW_StaticMeshes | SHOW_SkeletalMeshes | SHOW_Terrain | SHOW_Foliage | SHOW_BSP | SHOW_Fog | SHOW_Grid | SHOW_Selection | SHOW_Volumes | SHOW_CamFrustums
};

//
//	ESceneViewMode
//

enum ESceneViewMode
{
	SVM_Wireframe		= 0x01,
	SVM_BrushWireframe	= 0x02,	// Wireframe which shows CSG brushes and not BSP.
	SVM_Unlit			= 0x04,
	SVM_Lit				= 0x08,
	SVM_LightingOnly	= 0x10,
	SVM_LightComplexity	= 0x20,

	SVM_WireframeMask = SVM_Wireframe | SVM_BrushWireframe
};

//
//	FSceneView
//

struct FSceneView
{
	FMatrix				ViewMatrix,
						ProjectionMatrix;

	DWORD				ViewMode,
						ShowFlags;
	FColor				BackgroundColor,
						OverlayColor;

	FSceneViewState*	State;
	UBOOL				AutomaticBrightness;
	FLOAT				BrightnessAdjustSpeed,
						ManualBrightnessScale;

	/** The actor which is being viewed from. */
	AActor*	ViewActor;

	// Constructor.

	FSceneView():
		ViewMatrix(FMatrix::Identity),
		ProjectionMatrix(FMatrix::Identity),
		ViewMode(SVM_Lit),
		ShowFlags(0),
		BackgroundColor(0,0,0,0),
		OverlayColor(0,0,0,0),
		State(NULL),
		AutomaticBrightness(0),
		BrightnessAdjustSpeed(0.25f),
		ManualBrightnessScale(1.0f),
		ViewActor(NULL)
	{}

	/**
	 * Returns whether lighting is visible in this view.
	 */
	UBOOL IsLit() const
	{
		switch(ViewMode)
		{
		case SVM_Lit:
		case SVM_LightingOnly:
			return 1;
		default:
			return 0;
		};
	}

	// Project/Deproject - Transforms points from world space to post-perspective screen space.

	FPlane Project(const FVector& V) const;
	FVector Deproject(const FPlane& P) const;
};

//
//	FSceneContext - Information about a scene currently being rendered, passed to each primitive in the scene.
//

struct FSceneContext
{
	FSceneView*	View;
	FScene*		Scene;
	INT			X,
				Y;
	UINT		SizeX,
				SizeY;

	// Constructor.

	FSceneContext(FSceneView* InView,FScene* InScene,INT InX,INT InY,UINT InSizeX,UINT InSizeY):
		View(InView),
		Scene(InScene),
		X(InX),
		Y(InY),
		SizeX(InSizeX),
		SizeY(InSizeY)
	{}

	void FixAspectRatio(FLOAT InAspectRatio);

	// Project - Transforms a point into pixel coordinates.  Returns false if the resulting W is negative, meaning the point is behind the view.

	UBOOL Project(const FVector& V,INT& InX, INT& InY) const;
};

//
//	FPreviewScene - A scene containing common thumbnail code.
//

struct FPreviewScene: FScene
{
	TArray<UActorComponent*>		Components;
	TArray<UPrimitiveComponent*>	Primitives;
	TArray<ULightComponent*>		Lights;

	// Constructor/destructor.

	FPreviewScene(FRotator LightRotation = FRotator(-8192,8192,0),FLOAT SkyBrightness = 0.25f,FLOAT LightBrightness = 1.0f)
	{
		USkyLightComponent*			SkyLightComponent = ConstructObject<USkyLightComponent>(USkyLightComponent::StaticClass());
		SkyLightComponent->Brightness = SkyBrightness;
		SkyLightComponent->Color = FColor(255,255,255);
		SkyLightComponent->Scene = this;
		SkyLightComponent->SetParentToWorld(FMatrix::Identity);
		SkyLightComponent->Created();
		Components.AddItem(SkyLightComponent);

		UDirectionalLightComponent*	DirectionalLightComponent = ConstructObject<UDirectionalLightComponent>(UDirectionalLightComponent::StaticClass());
		DirectionalLightComponent->Brightness = LightBrightness;
		DirectionalLightComponent->Color = FColor(255,255,255);
		DirectionalLightComponent->Scene = this;
		DirectionalLightComponent->SetParentToWorld(FRotationMatrix(LightRotation));
		DirectionalLightComponent->Created();
		Components.AddItem(DirectionalLightComponent);
	}

	virtual ~FPreviewScene()
	{
		for(UINT ComponentIndex = 0;ComponentIndex < (UINT)Components.Num();ComponentIndex++)
		{
			if(Components(ComponentIndex)->Initialized)
				Components(ComponentIndex)->Destroyed();
			delete Components(ComponentIndex);
		}
	}

	// FScene interface.

	virtual void AddPrimitive(UPrimitiveComponent* Primitive);
	virtual void RemovePrimitive(UPrimitiveComponent* Primitive);
	virtual void AddLight(ULightComponent* Light);
	virtual void RemoveLight(ULightComponent* Light);
	virtual void GetVisiblePrimitives(const FSceneContext& Context,const FConvexVolume& Frustum,TArray<UPrimitiveComponent*>& OutPrimitives);
	virtual void GetRelevantLights(UPrimitiveComponent* Primitive,TArray<ULightComponent*>& OutLights);

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FPreviewScene& P)
	{
		return Ar << P.Components;
	}
};