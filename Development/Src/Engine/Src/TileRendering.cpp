/*=============================================================================
	TranslucentRendering.cpp: Translucent rendering implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "TileRendering.h"

/** FTileRenderer globals */
UBOOL FTileRenderer::bInitialized = FALSE;
TGlobalResource<FLocalVertexFactory> FTileRenderer::VertexFactory;
TGlobalResource<FMaterialTileVertexBuffer> FTileRenderer::VertexBuffer;
FMeshBatch FTileRenderer::Mesh;

FTileRenderer::FTileRenderer()
{
	// if the static data was never initialized, do it now
	if (!bInitialized)
	{
		bInitialized = TRUE;

		FLocalVertexFactory::DataType Data;
		// position
		Data.PositionComponent = FVertexStreamComponent(
			&VertexBuffer,STRUCT_OFFSET(FMaterialTileVertex,Position),sizeof(FMaterialTileVertex),VET_Float3);
		// tangents
		Data.TangentBasisComponents[0] = FVertexStreamComponent(
			&VertexBuffer,STRUCT_OFFSET(FMaterialTileVertex,TangentX),sizeof(FMaterialTileVertex),VET_PackedNormal);
		Data.TangentBasisComponents[1] = FVertexStreamComponent(
			&VertexBuffer,STRUCT_OFFSET(FMaterialTileVertex,TangentZ),sizeof(FMaterialTileVertex),VET_PackedNormal);
		// color
		Data.ColorComponent = FVertexStreamComponent(
			&VertexBuffer,STRUCT_OFFSET(FMaterialTileVertex,Color),sizeof(FMaterialTileVertex),VET_Color);
		// UVs
		Data.TextureCoordinates.AddItem(FVertexStreamComponent(
			&VertexBuffer,STRUCT_OFFSET(FMaterialTileVertex,U),sizeof(FMaterialTileVertex),VET_Float2));

        // update the data
		VertexFactory.SetData(Data);
		FMeshBatchElement& BatchElement = Mesh.Elements(0);
		Mesh.VertexFactory = &VertexFactory;
		Mesh.DynamicVertexStride = sizeof(FMaterialTileVertex);
		BatchElement.WorldToLocal = FMatrix::Identity;
		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = 2;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = 3;
		Mesh.ReverseCulling = FALSE;
		Mesh.UseDynamicData = TRUE;
		Mesh.Type = PT_TriangleStrip;
		Mesh.DepthPriorityGroup = SDPG_Foreground;
		Mesh.bUsePreVertexShaderCulling = FALSE;
		Mesh.PlatformMeshData = NULL;
	}
}

void FTileRenderer::DrawTile(const class FViewInfo& View, const FMaterialRenderProxy* MaterialRenderProxy)
{
	// update the FMeshBatch
	Mesh.UseDynamicData = FALSE;
	Mesh.MaterialRenderProxy = MaterialRenderProxy;

	// full screen render, just use the identity matrix for LocalToWorld
	PrepareShaders( View, MaterialRenderProxy, FMatrix::Identity, TRUE);
}

void FTileRenderer::DrawTile(const class FViewInfo& View, const FMaterialRenderProxy* MaterialRenderProxy, FLOAT X, FLOAT Y, FLOAT SizeX, FLOAT SizeY, UBOOL bIsHitTesting, const FHitProxyId HitProxyId)
{
	// @GEMINI_TODO: Fix my matrices below :(
	DrawTile( View, MaterialRenderProxy, X, Y, SizeX, SizeY, 0, 0, 1, 1, bIsHitTesting, HitProxyId);
	return;
	// since we have 0..1 UVs, we can use the pre-made vertex factory data
#if GEMINI_TODO
	// force the fully dynamic path for hit testing (which has the hit testing code)
	if (bIsHitTesting)
	{
		DrawTile(View, MaterialRenderProxy, X, Y, SizeX, SizeY, 0, 0, 1, 1, bIsHitTesting, HitProxyId);
		return;
	}

	// update the FMeshBatch
	Mesh.UseDynamicData = FALSE;
	Mesh.MaterialRenderProxy = MaterialRenderProxy;

	// @GEMINI_TODO: Cache these inversions?
	FLOAT InvScreenX = 1.0f / View.SizeX;
 	FLOAT InvScreenY = 1.0f / View.SizeY;

	PrepareShaders(
		View, 
		MaterialRenderProxy,
		FMatrix(
			FPlane(InvScreenX * SizeX, 0.0f, 0.0f, 0.0f),
			FPlane(0.0f, InvScreenY * SizeY, 0.0f, 0.0f),
			FPlane(0.0f, 0.0f, 1.0f, 0.0f),
			FPlane(InvScreenX * (X * 0.5f), InvScreenY * (Y * 0.5f), 0.0f, 1.0f)),
		TRUE,
		bIsHitTesting,
		HitProxyId);

#endif
}

void FTileRenderer::DrawTile(const class FViewInfo& View, const FMaterialRenderProxy* MaterialRenderProxy, FLOAT X, FLOAT Y, FLOAT SizeX, FLOAT SizeY, FLOAT U, FLOAT V, FLOAT SizeU, FLOAT SizeV, UBOOL bIsHitTesting, const FHitProxyId HitProxyId)
{
	if (bIsHitTesting)
	{
		// @GEMINI_TODO: the hitproxy drawpolicy should force DefaultMaterial for opaque materials (will need to generate hitproxy shaders for non-opaque materials!)
		MaterialRenderProxy = GEngine->DefaultMaterial->GetRenderProxy(0);
	}

	// draw the mesh
#if GEMINI_TODO
	// let RHI allocate memory (needs DrawDymamicMesh to be able to use RHIEndDrawPrimitiveUP for hittesting)
	void* Vertices;
	//RHIBeginDrawPrimitiveUP( sizeof(FMaterialTileVertex) * 4, Vertices);
	FMaterialTileVertex* DestVertex = (FMaterialTileVertex*)Vertices;
#else
	FMaterialTileVertex DestVertex[4];
#endif

	// create verts
	DestVertex[0].Initialize(X + SizeX, Y, U + SizeU, V);
	DestVertex[1].Initialize(X, Y, U, V);
	DestVertex[2].Initialize(X + SizeX, Y + SizeY, U + SizeU, V + SizeV);
	DestVertex[3].Initialize(X, Y + SizeY, U, V + SizeV);

	// update the FMeshBatch
	Mesh.UseDynamicData = TRUE;
	Mesh.DynamicVertexData = DestVertex;
	Mesh.MaterialRenderProxy = MaterialRenderProxy;

	// set shaders and render the mesh
	PrepareShaders( View, MaterialRenderProxy, FMatrix::Identity, FALSE, bIsHitTesting, HitProxyId);
}




void FTileRenderer::PrepareShaders(const class FViewInfo& View, const FMaterialRenderProxy* MaterialRenderProxy, const FMatrix& LocalToWorld, UBOOL bUseIdentityViewProjection, UBOOL bIsHitTesting, const FHitProxyId HitProxyId)
{
	const FMaterial* Material = MaterialRenderProxy->GetMaterial();	

	//get the blend mode of the material
	const EBlendMode MaterialBlendMode = Material->GetBlendMode();

	static FMatrix SaveViewProjectionMatrix = FMatrix::Identity;
	static FVector4 SaveViewOrigin(0,0,0,0);
	static FVector SavePreViewTranslation(0,0,0);
	if (bUseIdentityViewProjection)
	{
		SaveViewProjectionMatrix = View.TranslatedViewProjectionMatrix;
		SaveViewOrigin = View.ViewOrigin;
		SavePreViewTranslation = View.PreViewTranslation;
		const_cast<FViewInfo&>(View).TranslatedViewProjectionMatrix = FMatrix::Identity;
		const_cast<FViewInfo&>(View).ViewOrigin.Set( 0, 0, 0, 0 );
		const_cast<FViewInfo&>(View).PreViewTranslation.Set( 0, 0, 0 );
	}

	// set view shader constants
	RHISetViewParameters(View);
	RHISetMobileHeightFogParams(View.HeightFogParams);

	FMeshBatchElement& BatchElement = Mesh.Elements(0);
	// set the LocalToWorld matrix
	BatchElement.LocalToWorld = LocalToWorld;

	// handle translucent material blend modes
	if (IsTranslucentBlendMode(MaterialBlendMode))
	{
		UBOOL bRenderingToLowResTranslucencyBuffer = FALSE;
		UBOOL bAllowDownsampling = FALSE;
		UBOOL bRenderingToDoFBlurBuffer = FALSE;
		const FProjectedShadowInfo* TranslucentPreShadowInfo = NULL;
		FTranslucencyDrawingPolicyFactory::DrawDynamicMesh( View, FTranslucencyDrawingPolicyFactory::ContextType(bRenderingToLowResTranslucencyBuffer,bAllowDownsampling,bRenderingToDoFBlurBuffer,TranslucentPreShadowInfo), Mesh, FALSE, FALSE, NULL, HitProxyId);
	}
	// handle opaque material
	else
	{
		// make sure we are doing opaque drawing
		// @GEMINI_TODO: Is this set in the below DrawDynamicMesh calls?
		RHISetBlendState( TStaticBlendState<>::GetRHI());

		// draw the mesh
		if (bIsHitTesting)
		{
			FHitProxyDrawingPolicyFactory::DrawDynamicMesh( View, FHitProxyDrawingPolicyFactory::ContextType(), Mesh, FALSE, FALSE, NULL, HitProxyId);
		}
		else
		{
			FBasePassOpaqueDrawingPolicyFactory::DrawDynamicMesh( View, FBasePassOpaqueDrawingPolicyFactory::ContextType(), Mesh, FALSE, FALSE, NULL, HitProxyId);
		}
	}		

	if (bUseIdentityViewProjection)
	{
		const_cast<FViewInfo&>(View).TranslatedViewProjectionMatrix = SaveViewProjectionMatrix;
		const_cast<FViewInfo&>(View).ViewOrigin = SaveViewOrigin;
		const_cast<FViewInfo&>(View).PreViewTranslation = SavePreViewTranslation;
	}
}
