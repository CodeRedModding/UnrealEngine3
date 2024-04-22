/*=============================================================================
	BatchedElements.h: Batched element rendering.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_BATCHEDELEMENTS
#define _INC_BATCHEDELEMENTS

#include "StaticBoundShaderState.h"

/** Blend modes supported for simple element rendering */
enum ESimpleElementBlendMode
{
	SE_BLEND_Opaque=0,
	SE_BLEND_Masked,
	SE_BLEND_Translucent,
	SE_BLEND_Additive,
	SE_BLEND_Modulate,
	SE_BLEND_ModulateAndAdd,
	SE_BLEND_MaskedDistanceField,
	SE_BLEND_MaskedDistanceFieldShadowed,
	SE_BLEND_TranslucentDistanceField,
	SE_BLEND_TranslucentDistanceFieldShadowed,
	SE_BLEND_AlphaComposite,
	SE_BLEND_AlphaOnly,

	SE_BLEND_RGBA_MASK_START,
	SE_BLEND_RGBA_MASK_END = SE_BLEND_RGBA_MASK_START+31, //Using 5bit bit-field for red, green, blue, alpha and desaturation

	SE_BLEND_MAX
};

/** The type used to store batched line vertices. */
struct FSimpleElementVertex
{
	FVector4 Position;
	FVector2D TextureCoordinate;
	FLinearColor Color;
	FColor HitProxyIdColor;

	FSimpleElementVertex() {}

	FSimpleElementVertex(const FVector4& InPosition,const FVector2D& InTextureCoordinate,const FLinearColor& InColor,FHitProxyId InHitProxyId):
		Position(InPosition),
		TextureCoordinate(InTextureCoordinate),
		Color(InColor),
		HitProxyIdColor(InHitProxyId.GetColor())
	{}
};

/**
* The simple element vertex declaration resource type.
*/
class FSimpleElementVertexDeclaration : public FRenderResource
{
public:

	FVertexDeclarationRHIRef VertexDeclarationRHI;

	// Destructor.
	virtual ~FSimpleElementVertexDeclaration() {}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		Elements.AddItem(FVertexElement(0,STRUCT_OFFSET(FSimpleElementVertex,Position),VET_Float4,VEU_Position,0));
		Elements.AddItem(FVertexElement(0,STRUCT_OFFSET(FSimpleElementVertex,TextureCoordinate),VET_Float2,VEU_TextureCoordinate,0));
		Elements.AddItem(FVertexElement(0,STRUCT_OFFSET(FSimpleElementVertex,Color),VET_Float4,VEU_Color,0));
		Elements.AddItem(FVertexElement(0,STRUCT_OFFSET(FSimpleElementVertex,HitProxyIdColor),VET_Color,VEU_Color,1));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements, TEXT("Simple"));
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/** The simple element vertex declaration. */
extern TGlobalResource<FSimpleElementVertexDeclaration> GSimpleElementVertexDeclaration;



/** Custom parameters for batched element shaders.  Derive from this class to implement your shader bindings. */
class FBatchedElementParameters
	: public FRefCountedObject
{

public:

	/** Binds vertex and pixel shaders for this element */
	virtual void BindShaders_RenderThread( const FMatrix& InTransform, const FLOAT InGamma ) = 0;

};



/** Batched elements for later rendering. */
class FBatchedElements
{
public:

	/**
	* Constructor 
	*/
	FBatchedElements()
		:	bTranslucentLines(FALSE)
		,	MaxMeshIndicesAllowed(GDrawUPIndexCheckCount / sizeof(INT))
			// the index buffer is 2 bytes, so make sure we only address 0xFFFF vertices in the index buffer
		,	MaxMeshVerticesAllowed(Min<DWORD>(0xFFFF, GDrawUPVertexCheckCount / sizeof(FSimpleElementVertex)))
	{
	}

	/** Adds a line to the batch. */
	void AddLine(const FVector& Start, const FVector& End, const FLinearColor& Color, FHitProxyId HitProxyId, const FLOAT Thickness = 0.0f, const UBOOL bForceAlpha = TRUE);

	/** Adds a point to the batch. */
	void AddPoint(const FVector& Position,FLOAT Size,const FLinearColor& Color,FHitProxyId HitProxyId);

	/** Adds a mesh vertex to the batch. */
	INT AddVertex(const FVector4& InPosition,const FVector2D& InTextureCoordinate,const FLinearColor& InColor,FHitProxyId HitProxyId);

	/** Adds a quad mesh vertex to the batch. */
	void AddQuadVertex(const FVector4& InPosition,const FVector2D& InTextureCoordinate,const FLinearColor& InColor,FHitProxyId HitProxyId, const FTexture* Texture,ESimpleElementBlendMode BlendMode);

	/** Adds a triangle to the batch. */
	void AddTriangle(INT V0,INT V1,INT V2,const FTexture* Texture,EBlendMode BlendMode);

	/** Adds a triangle to the batch. */
	void AddTriangle(INT V0, INT V1, INT V2, const FTexture* Texture, ESimpleElementBlendMode BlendMode, const FDepthFieldGlowInfo& GlowInfo = FDepthFieldGlowInfo(EC_EventParm));

	/** Adds a triangle to the batch. */
	void AddTriangle(INT V0,INT V1,INT V2,FBatchedElementParameters* BatchedElementParameters,ESimpleElementBlendMode BlendMode);

	/** 
	* Reserves space in index array for a mesh element
	* 
	* @param NumMeshTriangles - number of triangles to reserve space for
	* @param Texture - used to find the mesh element entry
	* @param BlendMode - used to find the mesh element entry
	*/
	void AddReserveTriangles(INT NumMeshTriangles,const FTexture* Texture,ESimpleElementBlendMode BlendMode);
	/** 
	* Reserves space in mesh vertex array
	* 
	* @param NumMeshVerts - number of verts to reserve space for
	* @param Texture - used to find the mesh element entry
	* @param BlendMode - used to find the mesh element entry
	*/
	void AddReserveVertices(INT NumMeshVerts);

	/** 
	 * Reserves space in line vertex array
	 * 
	 * @param NumLines - number of lines to reserve space for
	 */
	void AddReserveLines( INT NumLines );

	/** Adds a sprite to the batch. */
	void AddSprite(
		const FVector& Position,
		FLOAT SizeX,
		FLOAT SizeY,
		const FTexture* Texture,
		const FLinearColor& Color,
		FHitProxyId HitProxyId,
		FLOAT U,
		FLOAT UL,
		FLOAT V,
		FLOAT VL,
		BYTE BlendMode = SE_BLEND_Masked
		);

	/**
	 * Allocates space for the mesh vertices and indices from the batch and returns raw pointers to be filled out by the caller.
	 *
	 * @param NumMeshVertices - number of vertices to reserve space for
	 * @param NumMeshIndices - number of indices to allocate space for	 
	 * @param Texture - used to find the mesh element entry
	 * @param BlendMode - used to find the mesh element entry
	 * @param Vertices - will hold the resulting vertex allocation pointer
	 * @param Indices - will hold the resulting index allocation pointer
	 * @param IndexOffset - will hold the index of the first vertex in the pointer with each additional vertex following consecutive indices
	 */
	void AllocateMeshData(INT NumMeshVertices, INT NumMeshIndices, const FTexture* Texture, ESimpleElementBlendMode BlendMode,  FSimpleElementVertex** Vertices, WORD** Indices, INT* IndexOffset);

	/** Draws the batch. */
	UBOOL Draw(const FMatrix& Transform,UINT ViewportSizeX,UINT ViewportSizeY,UBOOL bHitTesting,FLOAT Gamma = 1.0f) const;

	FORCEINLINE UBOOL HasPrimsToDraw() const
	{
		return( LineVertices.Num() || Points.Num() || Sprites.Num() || MeshElements.Num() || QuadMeshElements.Num() || ThickLines.Num() );
	}

private:

	/** Adds a triangle to the batch. */
	void AddTriangle(INT V0,INT V1,INT V2,FBatchedElementParameters* BatchedElementParameters,const FTexture* Texture,ESimpleElementBlendMode BlendMode, const FDepthFieldGlowInfo& GlowInfo = FDepthFieldGlowInfo(EC_EventParm));

	TArray<FSimpleElementVertex> LineVertices;

	struct FBatchedPoint
	{
		FVector Position;
		FLOAT Size;
		FColor Color;
		FHitProxyId HitProxyId;
	};
	TArray<FBatchedPoint> Points;

	struct FBatchedThickLines
	{
		FVector Start;
		FVector End;
		FLOAT Thickness;
		FColor Color;
		FHitProxyId HitProxyId;
	};
	TArray<FBatchedThickLines> ThickLines;

	UBOOL bTranslucentLines;

	struct FBatchedSprite
	{
		FVector Position;
		FLOAT SizeX;
		FLOAT SizeY;
		const FTexture* Texture;
		FColor Color;
		FHitProxyId HitProxyId;
		FLOAT U;
		FLOAT UL;
		FLOAT V;
		FLOAT VL;
		BYTE BlendMode;
	};
	TArray<FBatchedSprite> Sprites;

	struct FBatchedMeshElement
	{
		/** starting index in vertex buffer for this batch */
		UINT MinVertex;
		/** largest vertex index used by this batch */
		UINT MaxVertex;
		/** index buffer for triangles */
		TArray<WORD,TInlineAllocator<6> > Indices;
		/** all triangles in this batch draw with the same texture */
		const FTexture* Texture;
		/** Parameters for this batched element */
		TRefCountPtr<FBatchedElementParameters> BatchedElementParameters;
		/** all triangles in this batch draw with the same blend mode */
		ESimpleElementBlendMode BlendMode;
		/** all triangles in this batch draw with the same depth field glow (depth field blend modes only) */
		FDepthFieldGlowInfo GlowInfo;
	};

	struct FBatchedQuadMeshElement
	{
		TArray<FSimpleElementVertex> Vertices;
		const FTexture* Texture;
		ESimpleElementBlendMode BlendMode;
	};

	/** Max number of mesh index entries that will fit in a DrawPriUP call */
	INT MaxMeshIndicesAllowed;
	/** Max number of mesh vertices that will fit in a DrawPriUP call */
	INT MaxMeshVerticesAllowed;

	TArray<FBatchedMeshElement,TInlineAllocator<1> > MeshElements;
	TArray<FSimpleElementVertex,TInlineAllocator<4> > MeshVertices;

	TArray<FBatchedQuadMeshElement> QuadMeshElements;

	/** bound shader state for the fast path */
	static FGlobalBoundShaderState SimpleBoundShaderState;
	/** bound shader state for the regular mesh elements */
	static FGlobalBoundShaderState RegularBoundShaderState;
	/** bound shader state for masked mesh elements */
	static FGlobalBoundShaderState MaskedBoundShaderState;
	/** bound shader state for masked mesh elements */
	static FGlobalBoundShaderState DistanceFieldBoundShaderState;
	/** bound shader state for the hit testing mesh elements */
	static FGlobalBoundShaderState HitTestingBoundShaderState;
	/** bound shader state for color masked elements */
	static FGlobalBoundShaderState ColorChannelMaskShaderState;

	/*
	 * Sets the appropriate vertex and pixel shader.
	 */
	void PrepareShaders(
		ESimpleElementBlendMode BlendMode,
		const FMatrix& Transform,
		FBatchedElementParameters* BatchedElementParameters,
		const FTexture* Texture,
		UBOOL bHitTesting,
		FLOAT Gamma,
		const FDepthFieldGlowInfo* GlowInfo = NULL
		) const;

	/** Compare two texture pointers, return 0 if equal */
	static QSORT_RETURN TextureCompare(const FBatchedElements::FBatchedSprite* SpriteA, const FBatchedElements::FBatchedSprite* SpriteB)
	{
		return (SpriteA->Texture == SpriteB->Texture && SpriteA->BlendMode == SpriteB->BlendMode) ? 0 : 1; 
	}
};

#endif
