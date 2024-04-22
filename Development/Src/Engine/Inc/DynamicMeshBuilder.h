/*=============================================================================
	DynamicMeshBuilder.h: Dynamic mesh builder definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/** The vertex type used for dynamic meshes. */
struct FDynamicMeshVertex
{
	FDynamicMeshVertex() {}
	FDynamicMeshVertex( const FVector& InPosition ):
		Position(InPosition),
		TextureCoordinate(FVector2D(0,0)),
		TangentX(FVector(1,0,0)),
		TangentZ(FVector(0,0,1)),
		Color(FColor(255,255,255)) 
	{
		// basis determinant default to +1.0
		TangentZ.Vector.W = 255;
	}

	FDynamicMeshVertex(const FVector& InPosition,const FVector& InTangentX,const FVector& InTangentZ,const FVector2D& InTexCoord, const FColor& InColor):
		Position(InPosition),
		TextureCoordinate(InTexCoord),
		TangentX(InTangentX),
		TangentZ(InTangentZ),
		Color(InColor)
	{
		// basis determinant default to +1.0
		TangentZ.Vector.W = 255;
	}

	void SetTangents( const FVector& InTangentX, const FVector& InTangentY, const FVector& InTangentZ )
	{
		TangentX = InTangentX;
		TangentZ = InTangentZ;
		// store determinant of basis in w component of normal vector
		TangentZ.Vector.W = GetBasisDeterminantSign(InTangentX,InTangentY,InTangentZ) < 0.0f ? 0 : 255;
	}

	FVector GetTangentY()
	{
		return (FVector(TangentZ) ^ FVector(TangentX)) * ((FLOAT)TangentZ.Vector.W / 127.5f - 1.0f);
	};

	FVector Position;
	FVector2D TextureCoordinate;
	FPackedNormal TangentX;
	FPackedNormal TangentZ;
	FColor Color;
};

/**
 * A utility used to construct dynamically generated meshes, and render them to a FPrimitiveDrawInterface.
 * Note: This is meant to be easy to use, not fast.  It moves the data around more than necessary, and requires dynamically allocating RHI
 * resources.  Exercise caution.
 */
class FDynamicMeshBuilder
{
public:

	/** Initialization constructor. */
	FDynamicMeshBuilder();

	/** Destructor. */
	~FDynamicMeshBuilder();

	/** Adds a vertex to the mesh. */
	INT AddVertex(
		const FVector& InPosition,
		const FVector2D& InTextureCoordinate,
		const FVector& InTangentX,
		const FVector& InTangentY,
		const FVector& InTangentZ,
		const FColor& InColor
		);

	/** Adds a vertex to the mesh. */
	INT AddVertex(const FDynamicMeshVertex &InVertex);

	/** Adds a triangle to the mesh. */
	void AddTriangle(INT V0,INT V1,INT V2);

	/** Adds many vertices to the mesh. */
	INT AddVertices(const TArray<FDynamicMeshVertex> &InVertices);

	/** Add many indices to the mesh. */
	void AddTriangles(const TArray<INT> &InIndices);

	/**
	 * Draws the mesh to the given primitive draw interface.
	 * @param PDI - The primitive draw interface to draw the mesh on.
	 * @param LocalToWorld - The local to world transform to apply to the vertices of the mesh.
	 * @param FMaterialRenderProxy - The material instance to render on the mesh.
	 * @param DepthPriorityGroup - The depth priority group to render the mesh in.
	 * @param DepthBias - The depth bias to render the mesh with.
	 */
	void Draw(FPrimitiveDrawInterface* PDI,const FMatrix& LocalToWorld,const FMaterialRenderProxy* MaterialRenderProxy,BYTE DepthPriorityGroup,FLOAT DepthBias,UBOOL bDisableBackfaceCulling=FALSE);

private:
	class FDynamicMeshIndexBuffer* IndexBuffer;
	class FDynamicMeshVertexBuffer* VertexBuffer;
};
