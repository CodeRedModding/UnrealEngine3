/*=============================================================================
	D3DScene.h: Unreal Direct3D scene rendering definitions.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

//
//	ED3DScenePass
//

enum ED3DScenePass
{
	SP_Unlit,
	SP_UnlitHitTest,
	SP_DepthSortedHitTest,
	SP_OpaqueEmissive,
	SP_OpaqueLighting,
	SP_TranslucentEmissive,
	SP_TranslucentLighting,
	SP_UnlitTranslucency,
	SP_ShadowDepth,
	SP_LightFunction
};

//
//	FD3DPrimitiveRenderInterface
//

struct FD3DPrimitiveRenderInterface: FPrimitiveRenderInterface
{
	UPrimitiveComponent*	Primitive;
	ED3DScenePass			Pass;
	UBOOL					Dirty;

	// Constructor.

	FD3DPrimitiveRenderInterface(UPrimitiveComponent* InPrimitive,ED3DScenePass InPass);

	// FPrimitiveRenderInterface interface.

	virtual FChildViewport* GetViewport();
	virtual UBOOL IsHitTesting();

	virtual void DrawWireframe(FVertexFactory* VertexFactory,FIndexBuffer* IndexBuffer,EWireframeType WireType,FColor Color,UINT FirstIndex,UINT NumPrimitives,UINT MinVertexIndex,UINT MaxVertexIndex,UBOOL DrawVertices);
	virtual void DrawSprite(const FVector& Position,FLOAT SizeX,FLOAT SizeY,FTexture2D* Sprite,FColor Color);
	virtual void DrawLine(const FVector& Start,const FVector& End,FColor Color);
	virtual void DrawPoint(const FVector& Position,FColor Color,FLOAT PointSize);
};

//
//	FD3DMeshPRI
//

struct FD3DMeshPRI: FD3DPrimitiveRenderInterface
{
	// Constructor.

	FD3DMeshPRI(UPrimitiveComponent* InPrimitive,ED3DScenePass InPass):
		FD3DPrimitiveRenderInterface(InPrimitive,InPass)
	{}

	// FD3DMeshPRI interface.

	virtual void Set(FVertexFactory* VertexFactory,FMaterial* Material,FMaterialInstance* MaterialInstance) = 0;

	// FPrimitiveRenderInterface interface.

	virtual UBOOL IsMaterialRelevant(FMaterial* Material);
	virtual void DrawMesh(FVertexFactory* VertexFactory,FIndexBuffer* IndexBuffer,FMaterial* Material,FMaterialInstance* MaterialInstance,UINT FirstIndex,UINT NumTriangles,UINT MinVertexIndex,UINT MaxVertexIndex,ECullMode CullMode);
	virtual void DrawVertexLitMesh(FVertexFactory* VertexFactory,FIndexBuffer* IndexBuffer,FMaterial* Material,FMaterialInstance* MaterialInstance,UINT FirstIndex,UINT NumTriangles,UINT MinVertexIndex,UINT MaxVertexIndex) {}
};

//
//	FD3DMeshLightPRI
//

struct FD3DMeshLightPRI: FD3DMeshPRI
{
	ED3DStaticLightingType	StaticLightingType;
	ULightComponent*		Light;
	FD3DTexture2D*			VisibilityTexture;

	// Constructor.

	FD3DMeshLightPRI(UPrimitiveComponent* InPrimitive,ED3DScenePass InPass,ULightComponent* InLight,ED3DStaticLightingType InStaticLightingType,FD3DTexture2D* InVisibilityTexture = NULL):
		FD3DMeshPRI(InPrimitive,InPass),
		Light(InLight),
		StaticLightingType(InStaticLightingType),
		VisibilityTexture(InVisibilityTexture)
	{}

	// FD3DMeshPRI interface.

	virtual void Set(FVertexFactory* VertexFactory,FMaterial* Material,FMaterialInstance* MaterialInstance);
};

//
//	FD3DPrimitiveInfo - Stores information about a primitive specific to the frame being rendered.
//

struct FD3DPrimitiveInfo
{
	UPrimitiveComponent*		Primitive;
	FPrimitiveViewInterface*	PVI;
	BITFIELD					Visible : 1,
								Occluded : 1,
								NotOccluded : 1,
								Lit : 1;
	DWORD						LayerMask;
	TD3DRef<IDirect3DQuery9>	OcclusionQuery;

	// Constructor.

	FD3DPrimitiveInfo(UPrimitiveComponent* InPrimitive,const FSceneContext& Context);

	// IsOccluded - Returns whether this primitive is known to be occluded.

	UBOOL IsOccluded();
};

//
//	FD3DLayerVisibilitySet - The indices of all visible primitives in a layer.
//

struct FD3DLayerVisibilitySet: TArray<INT>
{
	DWORD	LayerMask;

	// Constructor.

	FD3DLayerVisibilitySet(DWORD InLayerMask): LayerMask(InLayerMask) {}
};

//
//	FD3DVisiblePrimitiveIterator - Iterates over visible primitives in the specified layers.
//

struct FD3DVisiblePrimitiveIterator
{
	// Constructor.

	FD3DVisiblePrimitiveIterator(DWORD InLayerMask);

	// Next - Advances to the next visible primitive.

	void Next();
	FD3DPrimitiveInfo* operator->() const;
	FD3DPrimitiveInfo* operator*() const;
	operator UBOOL() const;

private:
	DWORD					RemainingLayerMask,
							FinishedLayerMask;
	FD3DLayerVisibilitySet*	CurrentSet;
	INT						Index;

	FD3DPrimitiveInfo*		CurrentPrimitiveInfo;

	void NextSet();
};

/**
 * Used to store information about a projected shadow.
 */
struct FD3DShadowInfo
{
	UPrimitiveComponent* SubjectPrimitive;

	FMatrix	ShadowMatrix,			// Z range from MinLightZ to MaxShadowZ.
			SubjectShadowMatrix,	// Z range from MinShadowZ to MaxShadowZ.
			ReceiverShadowMatrix;	// Z range from MinShadowZ to MaxLightZ.
	FLOAT	MinShadowW,
			MaxShadowZ;
	UINT	Resolution;

	FConvexVolume ShadowFrustum;

	TD3DRef<IDirect3DQuery9> OcclusionQuery;

	BITFIELD Occluded : 1;
	BITFIELD NotOccluded : 1;

	FD3DShadowInfo(UPrimitiveComponent* InSubjectPrimitive,const FMatrix& WorldToLight,const FVector& FaceDirection,const FSphere& SubjectBoundingSphere,const FPlane& WPlane,FLOAT MinLightZ,FLOAT MaxLightZ,UINT InResolution);

	/**
	 * Checks if the shadow is known to be occluded.
	 *
	 * @return True if the shadow is occluded.
	 */

	UBOOL IsOccluded();
};

/**
 * Used to store the projected shadows for a specific light.
 */
struct FD3DLightInfo
{
	TIndirectArray<FD3DShadowInfo> Shadows;
};

//
//	FD3DSceneRenderer
//

struct FD3DSceneRenderer
{
	UINT	LastLightingBuffer,
			NewLightingBuffer;
	UBOOL	LastLightingLayerFinished;

	UINT	LastBlurBuffer;

	FSceneContext	Context;
	FSceneView*		View;
	FScene*			Scene;
	FMatrix			ViewProjectionMatrix,
					InvViewProjectionMatrix;
	FPlane			CameraPosition;
	FLOAT			TwoSidedSign;
	FLinearColor	SkyColor;

	TArray<FD3DPrimitiveInfo>		PrimitiveInfoArray;
	TMap<UPrimitiveComponent*,INT>	PrimitiveIndexMap;		// A map from primitive to index in PrimitiveInfoArray.
	TArray<FD3DLayerVisibilitySet>	LayerVisibilitySets;

	TMap<ULightComponent*,FD3DLightInfo> VisibleLights;

	TArray<TD3DRef<IDirect3DQuery9> >	TranslucencyLayerQueries;	// Each row is a translucency layer, each column is an occlusion query for a primitive in that layer.

	FConvexVolume	ViewFrustum;

	FLOAT FogMinHeight[4];
	FLOAT FogMaxHeight[4];
	FLOAT FogDistanceScale[4];
	FLinearColor FogInScattering[4];

	// Constructor/destructor.

	FD3DSceneRenderer(const FSceneContext& InContext);
	~FD3DSceneRenderer();

	// CreatePrimitiveInfo - Adds an entry to the PrimitiveInfoArray for a primitive.

	FORCEINLINE INT CreatePrimitiveInfo(UPrimitiveComponent* Primitive);

	// GetPrimitiveInfoIndex - Finds a cached FD3DPrimitiveInfo for a primitive, or creates a new one if necessary.  Returns an index into PrimitiveInfoArray.

	INT GetPrimitiveInfoIndex(UPrimitiveComponent* Primitive)
	{
		INT*	PrimitiveIndex = PrimitiveIndexMap.Find(Primitive);
		if(PrimitiveIndex)
			return *PrimitiveIndex;
		else
			return CreatePrimitiveInfo(Primitive);
	}

	// GetPrimitiveInfo - Finds a cached FD3DPrimitiveInfo for a primitive, or creates a new one if necessary.  Reference may change if PrimitiveInfoArray is reallocated.

	FD3DPrimitiveInfo& GetPrimitiveInfo(UPrimitiveComponent* Primitive)
	{
		INT*	PrimitiveIndex = PrimitiveIndexMap.Find(Primitive);
		if(PrimitiveIndex)
			return PrimitiveInfoArray(*PrimitiveIndex);
		else
			return PrimitiveInfoArray(CreatePrimitiveInfo(Primitive));
	}

	// GetVisiblePrimitiveInfo - Finds a cached FD3DPrimitiveInfo for a primitive and returns it if it's visible.

	FD3DPrimitiveInfo* GetVisiblePrimitiveInfo(UPrimitiveComponent* Primitive)
	{
		INT*	PrimitiveIndex = PrimitiveIndexMap.Find(Primitive);
		if(PrimitiveIndex && PrimitiveInfoArray(*PrimitiveIndex).Visible)
			return &PrimitiveInfoArray(*PrimitiveIndex);
		else
			return NULL;
	}

	// GetLastLightingBuffer

	const TD3DRef<IDirect3DTexture9>& GetLastLightingBuffer();

	// GetNewLightingBuffer

	const TD3DRef<IDirect3DSurface9>& GetNewLightingBuffer(UBOOL Replace = 0,UBOOL Additive = 0,UBOOL Fullscreen = 0);

	// ClearLightingBufferDepth

	void ClearLightingBufferDepth(FLOAT DepthValue);

	// FinishLightingLayer

	void FinishLightingLayer();

	// RenderShadowDepthBuffer

	void RenderShadowDepthBuffer(ULightComponent* Light,FD3DShadowInfo* ShadowInfo,DWORD LayerMask);

	// RenderLight

	void RenderLight(ULightComponent* Light,FD3DLightInfo* LightInfo,UBOOL Translucent);

	// RenderLayer - Renders a depth-sorted scene layer.  Returns zero if no primitives were rendered, non-zero otherwise.

	UBOOL RenderLayer(UBOOL Translucent);

	// Render

	void Render();

	// RenderHitProxies - Renders the hit proxies of the scene primitives.

	void RenderHitProxies();
};
