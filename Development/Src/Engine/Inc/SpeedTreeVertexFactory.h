/*=============================================================================
SpeedTreeVertexFactory.h: SpeedTree vertex factory definition.
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if WITH_SPEEDTREE

class FSpeedTreeVertexFactory : public FVertexFactory
{
public:

	struct DataType
	{
		FVertexStreamComponent								PositionComponent;
		FVertexStreamComponent								TangentBasisComponents[3];
		FVertexStreamComponent								WindInfo;
		TPreallocatedArray<FVertexStreamComponent,MAX_TEXCOORDS>	TextureCoordinates;
		FVertexStreamComponent								ShadowMapCoordinateComponent;
	};

	/** When rendering a mesh element using a FSpeedTreeVertexFactory, its user data must point to an instance of MeshUserDataType. */
	struct MeshUserDataType
	{
		FBoxSphereBounds Bounds;
		FMatrix		RotationOnlyMatrix;
		FVector2D	LODDistances;
		FVector2D	BillboardDistances;
	};

	/** Initialization constructor. */
	FSpeedTreeVertexFactory(const USpeedTree* InSpeedTree):
		SpeedTree(InSpeedTree)
	{}

	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);
	virtual void InitRHI(void);

	void SetData(const DataType& InData)			
	{ 
		Data = InData; 
		UpdateRHI(); 
	}

	const USpeedTree* GetSpeedTree() const
	{
		return SpeedTree;
	}

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

private:
	DataType Data;
	const USpeedTree* SpeedTree;
};


class FSpeedTreeBranchVertexFactory : public FSpeedTreeVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FSpeedTreeBranchVertexFactory);
public:

	/** Initialization constructor. */
	FSpeedTreeBranchVertexFactory(const USpeedTree* InSpeedTree):
		FSpeedTreeVertexFactory(InSpeedTree)
	{}
};

class FSpeedTreeFrondVertexFactory : public FSpeedTreeBranchVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FSpeedTreeFrondVertexFactory);
public:

	/** Initialization constructor. */
	FSpeedTreeFrondVertexFactory(const USpeedTree* InSpeedTree):
		FSpeedTreeBranchVertexFactory(InSpeedTree)
	{}
};

class FSpeedTreeLeafCardVertexFactory : public FSpeedTreeBranchVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FSpeedTreeLeafCardVertexFactory);
public:

	/** Initialization constructor. */
	FSpeedTreeLeafCardVertexFactory(const USpeedTree* InSpeedTree):
		FSpeedTreeBranchVertexFactory(InSpeedTree)
	{}
};


class FSpeedTreeLeafMeshVertexFactory : public FSpeedTreeLeafCardVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FSpeedTreeLeafMeshVertexFactory);
public:

	/** Initialization constructor. */
	FSpeedTreeLeafMeshVertexFactory(const USpeedTree* InSpeedTree):
		FSpeedTreeLeafCardVertexFactory(InSpeedTree)
	{}
};


class FSpeedTreeBillboardVertexFactory : public FSpeedTreeVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FSpeedTreeBillboardVertexFactory);
public:

	/** Initialization constructor. */
	FSpeedTreeBillboardVertexFactory(const USpeedTree* InSpeedTree):
		FSpeedTreeVertexFactory(InSpeedTree)
	{}

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);
};


#endif // WITH_SPEEDTREE

