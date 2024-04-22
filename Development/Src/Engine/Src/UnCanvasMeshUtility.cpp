/*=============================================================================
	UnCanvasMeshUtility.cpp: Utility functions/classes for rendering simple
	meshes through Canvas.

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineUserInterfaceClasses.h"
#include "UnCanvasMeshUtility.h"

/**
 * Allocates mesh data to be used in Canvas rendering.  The data must be filled
 * after the allocation.
 */
void InitMeshData(
	FCanvas* Canvas,
	const FTexture* Texture, UBOOL AlphaBlend,
	INT VertexCount, INT IndexCount,
	FCanvasMeshData* MeshData)
{
	const ESimpleElementBlendMode BlendMode = AlphaBlend ? SE_BLEND_Translucent : SE_BLEND_Opaque;
	const FTexture* FinalTexture = Texture ? Texture : GWhiteTexture;
	FBatchedElementParameters* BatchedElementParameters = NULL;
	FBatchedElements* BatchedElements = Canvas->GetBatchedElements(FCanvas::ET_Triangle, BatchedElementParameters, FinalTexture, BlendMode);
	FHitProxyId HitProxyId = Canvas->GetHitProxyId();

	WORD* IndexDataPointer;
	FSimpleElementVertex* VertexDataPointer;
	INT IndexOffset;

	BatchedElements->AllocateMeshData(VertexCount, IndexCount, FinalTexture, BlendMode, &VertexDataPointer, &IndexDataPointer, &IndexOffset);

	MeshData->VertexCount = VertexCount;
	MeshData->VertexDataBase = VertexDataPointer;
	MeshData->IndexStart = IndexOffset;
	MeshData->IndexCount = IndexCount;	
	MeshData->IndexDataBase = IndexDataPointer;
	MeshData->HitProxyIdColor = HitProxyId.GetColor();
}

/**
 * Initializes a helper structure used during mesh initialization.
 */
void InitMeshIt(const FCanvasMeshData& MeshData, FCanvasMeshIt* MeshIt)
{
	MeshIt->VertexDataCurrent = MeshData.VertexDataBase;
	MeshIt->VertexDataEnd = MeshData.VertexDataBase + MeshData.VertexCount;

	MeshIt->IndexDataCurrent = MeshData.IndexDataBase;
	MeshIt->IndexDataEnd = MeshData.IndexDataBase + MeshData.IndexCount;
}

/**
 * Constructor for the mesh builder helper class
 */
FCanvasTriangleMeshBatchDrawer::FCanvasTriangleMeshBatchDrawer()
{	
}

/**
 * Prepares a mesh builder to be ready for a triangle mesh where each triangle is explicitly specified.  That is
 * each triangle consists of three uniquely specified vertices.
 */
void FCanvasTriangleMeshBatchDrawer::Init(FCanvas* Canvas, const FTexture* Texture, UBOOL AlphaBlend, INT TriangleCount)
{
	InitMeshData(Canvas, Texture, AlphaBlend, TriangleCount * 3, TriangleCount * 3, &MeshData);
	InitMeshIt(MeshData, &MeshIt);
}

/**
 * Finishes creation of the canvas mesh.
 */
void FCanvasTriangleMeshBatchDrawer::Finalize()
{
	check(MeshIt.VertexDataCurrent == MeshIt.VertexDataEnd);
	check(MeshIt.IndexDataCurrent == MeshIt.IndexDataEnd);
}

/**
 * Constructor for the mesh builder helper class
 */
FCanvasIndexedTriangleMeshBatchDrawer::FCanvasIndexedTriangleMeshBatchDrawer()
{	
}

/**
 * Prepares a mesh builder to be ready for an indexed triangle mesh.
 */
void FCanvasIndexedTriangleMeshBatchDrawer::Init(FCanvas* Canvas,const FTexture* Texture, UBOOL AlphaBlend, INT VertexCount, INT IndexCount)
{
	InitMeshData(Canvas, Texture, AlphaBlend, VertexCount, IndexCount, &MeshData);
	InitMeshIt(MeshData, &MeshIt);
}

/**
 * Finishes creation of the canvas mesh.
 */
void FCanvasIndexedTriangleMeshBatchDrawer::Finalize()
{
	check(MeshIt.VertexDataCurrent == MeshIt.VertexDataEnd);
	check(MeshIt.IndexDataCurrent == MeshIt.IndexDataEnd);
}