/*=============================================================================
	UnCanvasMeshUtility.cpp: Utility functions/classes for rendering simple
	meshes through Canvas.

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * Helper structure that contains information about allocated mesh data.
 */
struct FCanvasMeshData
{
	FSimpleElementVertex* VertexDataBase;
	WORD* IndexDataBase;

	INT VertexCount;
	INT IndexStart;
	INT IndexCount;

	FColor HitProxyIdColor;
};

/**
 * Allocates mesh data to be used in Canvas rendering.  The data must be filled
 * after the allocation.
 */
extern void InitMeshData(FCanvas* Canvas, const FTexture* Texture, UBOOL AlphaBlend, INT VertexCount, INT IndexCount, FCanvasMeshData* MeshData);

/**
 * Helper structure that contains information about allocated mesh data.
 */
struct FCanvasMeshIt
{
	FSimpleElementVertex* VertexDataCurrent;
	FSimpleElementVertex* VertexDataEnd;

	WORD* IndexDataCurrent;
	WORD* IndexDataEnd;
};

/**
 * Initializes a helper structure used during mesh initialization.
 */
extern void InitMeshIt(const FCanvasMeshData& MeshData, FCanvasMeshIt* MeshIt);

/**
 * Utility class used to render a triangle mesh using canvas.  This is faster than
 * rendering multiple individual triangles through Canvas.  Use this when rendering
 * five or more triangles through DrawTriangle2D.
 */
class FCanvasTriangleMeshBatchDrawer
{
public:
	FCanvasTriangleMeshBatchDrawer();

	/**
	 * Initialize the triangle builder for a triangle mesh where each triangle is specified
	 * explicitly.
	 */
	void Init(FCanvas* Canvas, const FTexture* Texture, UBOOL AlphaBlend, INT TriangleCount);

	/**
	 * Add a triangle to the mesh.
	 */
	inline void AddTriangle2D(
		const FVector2D& PosA, const FVector2D& TexA, const FLinearColor& ColorA,
		const FVector2D& PosB, const FVector2D& TexB, const FLinearColor& ColorB,
		const FVector2D& PosC, const FVector2D& TexC, const FLinearColor& ColorC);

	/**
	 * Finish drawing the mesh.  Note that the number of triangles requested during
	 * initialization must be added before finalizing the rendering.
	 */
	void Finalize();

private:
	FCanvasMeshData MeshData;
	FCanvasMeshIt   MeshIt;
};

/**
 * Utility class used to render an indexed triangle mesh using canvas.  This is faster
 * than rendering multiple individual triangles through Canvas.  Use this when rendering
 * five or more triangles through DrawTriangle2D.
 */
class FCanvasIndexedTriangleMeshBatchDrawer
{
public:
	FCanvasIndexedTriangleMeshBatchDrawer();

	/**
	 * Initialize the triangle builder for an indexed triangle mesh
	 */
	void Init(FCanvas* Canvas, const FTexture* Texture, UBOOL AlphaBlend, INT VertexCount, INT IndexCount);

	/**
	 * Add a vertex to the triangle mesh.
	 */
	inline void AddVertex2D(
		const FVector2D& PosA, const FVector2D& TexA, const FLinearColor& ColorA);

	/**
	 * Add a triangle to the mesh indexing into the provided vertices.  Indices are 0
	 * based.
	 */
	inline void AddTriangleIndices(
		INT IndexA, INT IndexB, INT IndexC);

	/**
	 * Finish drawing the mesh.  Note that the number of vertices and indices requested during
	 * initialization must be added before finalizing the rendering.
	 */
	void Finalize();

private:
	FCanvasMeshData MeshData;
	FCanvasMeshIt   MeshIt;
};


inline void FCanvasTriangleMeshBatchDrawer::AddTriangle2D(
	const FVector2D& PosA, const FVector2D& TexA, const FLinearColor& ColorA,
	const FVector2D& PosB, const FVector2D& TexB, const FLinearColor& ColorB,
	const FVector2D& PosC, const FVector2D& TexC, const FLinearColor& ColorC)
{
	FSimpleElementVertex* VertexData = MeshIt.VertexDataCurrent;

	INT VertexIndex = MeshData.IndexStart + (VertexData - MeshData.VertexDataBase);

	VertexData[0].Position = FVector4(PosA.X, PosA.Y, 0.f, 1.f);
	VertexData[0].TextureCoordinate = TexA;
	VertexData[0].Color = ColorA;
	VertexData[0].HitProxyIdColor = MeshData.HitProxyIdColor;

	VertexData[1].Position = FVector4(PosB.X, PosB.Y, 0.f, 1.f);
	VertexData[1].TextureCoordinate = TexB;
	VertexData[1].Color = ColorB;
	VertexData[1].HitProxyIdColor = MeshData.HitProxyIdColor;

	VertexData[2].Position = FVector4(PosC.X, PosC.Y, 0.f, 1.f);
	VertexData[2].TextureCoordinate = TexC;
	VertexData[2].Color = ColorC;
	VertexData[2].HitProxyIdColor = MeshData.HitProxyIdColor;

	MeshIt.VertexDataCurrent = VertexData + 3;

	check(MeshIt.VertexDataCurrent <= MeshIt.VertexDataEnd);	

	WORD* IndexData = MeshIt.IndexDataCurrent;

	IndexData[0] = VertexIndex + 0;
	IndexData[1] = VertexIndex + 1;
	IndexData[2] = VertexIndex + 2;

	MeshIt.IndexDataCurrent = IndexData + 3;

	check(MeshIt.IndexDataCurrent <= MeshIt.IndexDataEnd);
}

inline void FCanvasIndexedTriangleMeshBatchDrawer::AddVertex2D(const FVector2D& Pos, const FVector2D& Tex, const FLinearColor& Color)
{
	FSimpleElementVertex* VertexData = MeshIt.VertexDataCurrent;

	VertexData->Position = FVector4(Pos.X, Pos.Y, 0.f, 1.f);
	VertexData->TextureCoordinate = Tex;
	VertexData->Color = Color;
	VertexData->HitProxyIdColor = MeshData.HitProxyIdColor;

	++MeshIt.VertexDataCurrent;

	check(MeshIt.VertexDataCurrent <= MeshIt.VertexDataEnd);	
}

inline void FCanvasIndexedTriangleMeshBatchDrawer::AddTriangleIndices(INT IndexA, INT IndexB, INT IndexC)
{
	INT IndexOffset = MeshData.IndexStart;

	WORD* IndexData = MeshIt.IndexDataCurrent;

	IndexData[0] = IndexOffset + IndexA;
	IndexData[1] = IndexOffset + IndexB;
	IndexData[2] = IndexOffset + IndexC;

	MeshIt.IndexDataCurrent += 3;

	check(MeshIt.IndexDataCurrent <= MeshIt.IndexDataEnd);
}
