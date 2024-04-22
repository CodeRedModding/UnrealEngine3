/*=============================================================================
	DrawingPolicy.h: Drawing policy definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DRAWINGPOLICY_H__
#define __DRAWINGPOLICY_H__

/**
 * A macro to compare members of two drawing policies(A and B), and return based on the result.
 * If the members are the same, the macro continues execution rather than returning to the caller.
 */
#define COMPAREDRAWINGPOLICYMEMBERS(MemberName) \
	if(A.MemberName < B.MemberName) { return -1; } \
	else if(A.MemberName > B.MemberName) { return +1; }

/**
 * The base mesh drawing policy.  Subclasses are used to draw meshes with type-specific context variables.
 * May be used either simply as a helper to render a dynamic mesh, or as a static instance shared between
 * similar meshs.
 */
class FMeshDrawingPolicy
{
public:

	struct ElementDataType {};

	FMeshDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		UBOOL bOverrideWithShaderComplexity = FALSE,
		UBOOL bInTwoSidedOverride = FALSE,
		FLOAT InDepthBias = 0.0f,
		UBOOL bInTwoSidedSeparatePassOverride = FALSE
		);

	FMeshDrawingPolicy& operator = (const FMeshDrawingPolicy& Other)
	{ 
		VertexFactory = Other.VertexFactory;
		MaterialRenderProxy = Other.MaterialRenderProxy;
		MaterialResource = Other.MaterialResource;
		bIsTwoSidedMaterial = Other.bIsTwoSidedMaterial;
		bIsWireframeMaterial = Other.bIsWireframeMaterial;
		bNeedsBackfacePass = Other.bNeedsBackfacePass;
		bOverrideWithShaderComplexity = Other.bOverrideWithShaderComplexity;
		DepthBias = Other.DepthBias;
		return *this; 
	}

	DWORD GetTypeHash() const
	{
		return PointerHash(VertexFactory,PointerHash(MaterialRenderProxy));
	}

	UBOOL Matches(const FMeshDrawingPolicy& OtherDrawer) const
	{
		return
			VertexFactory == OtherDrawer.VertexFactory &&
			MaterialRenderProxy == OtherDrawer.MaterialRenderProxy &&
			bIsTwoSidedMaterial == OtherDrawer.bIsTwoSidedMaterial && 
			bIsWireframeMaterial == OtherDrawer.bIsWireframeMaterial;
	}

	/**
	 * Sets the render states for drawing a mesh.
	 * @param PrimitiveSceneInfo - The primitive drawing the dynamic mesh.  If this is a view element, this will be NULL.
	 */
	void SetMeshRenderState(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		UBOOL bBackFace,
		const ElementDataType& ElementData
		) const;

	/**
	 * Executes the draw commands for a mesh.
	 */
	virtual void DrawMesh(const FMeshBatch& Mesh, INT BatchElementIndex) const;

	/**
	 * Executes the draw commands which can be shared between any meshes using this drawer.
	 * @param CI - The command interface to execute the draw commands on.
	 * @param View - The view of the scene being drawn.
	 */
	void DrawShared(const FSceneView* View) const;

	/**
	* Get the decl and stream strides for this mesh policy type and vertexfactory
	* @param VertexDeclaration - output decl 
	* @param StreamStrides - output array of vertex stream strides 
	*/
	void GetVertexDeclarationInfo(FVertexDeclarationRHIRef& VertexDeclaration, DWORD *StreamStrides) const;

	friend INT Compare(const FMeshDrawingPolicy& A,const FMeshDrawingPolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
		COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
		return 0;
	}

	// Accessors.
	UBOOL IsTwoSided() const
	{
		return bIsTwoSidedMaterial;
	}
	UBOOL IsWireframe() const
	{
		return bIsWireframeMaterial;
	}
	UBOOL NeedsBackfacePass() const
	{
		return bNeedsBackfacePass;
	}

	const FVertexFactory* GetVertexFactory() const { return VertexFactory; }
	const FMaterialRenderProxy* GetMaterialRenderProxy() const { return MaterialRenderProxy; }

protected:
	const FVertexFactory* VertexFactory;
	const FMaterialRenderProxy* MaterialRenderProxy;
	const FMaterial* MaterialResource;
	BITFIELD bIsTwoSidedMaterial : 1;
	BITFIELD bIsWireframeMaterial : 1;
	BITFIELD bNeedsBackfacePass : 1;
	BITFIELD bOverrideWithShaderComplexity : 1;
	FLOAT DepthBias;
};


DWORD GetTypeHash(const FBoundShaderStateRHIRef &Key);

#endif
