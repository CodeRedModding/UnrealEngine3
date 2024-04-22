/*=============================================================================
	TerrainVertexFactory.cpp: Terrain vertex factory implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnTerrain.h"
#include "UnTerrainRender.h"

#if WIIU
// @todo wiiu: For some reason, when uploading these from bytes to uint4 in the shader, 
// it fails, but if we convert to float (using the PackedNormal type), it works...
#define PACKED_POSITION_TYPE VET_PackedNormal
#else
#define PACKED_POSITION_TYPE VET_UByte4
#endif

/** Vertex factory with vertex stream components for terrain vertices */
// FRenderResource interface.
void FTerrainVertexFactory::InitRHI()
{
	// list of declaration items
	FVertexDeclarationElementList Elements;

	// position decls
	Elements.AddItem(AccessStreamComponent(Data.PositionComponent, VEU_Position));
	// gradients
	Elements.AddItem(AccessStreamComponent(Data.GradientComponent, VEU_Tangent));

	// create the actual device decls
	//@todo.SAS. Include shadow map and light map
	InitDeclaration(Elements,FVertexFactory::DataType(),FALSE,FALSE);
}

/**
* Copy the data from another vertex factory
* @param Other - factory to copy from
*/
void FTerrainVertexFactory::Copy(const FTerrainVertexFactory& Other)
{
	SetTerrainObject(Other.GetTerrainObject());
	SetTessellationLevel(Other.GetTessellationLevel());
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FTerrainVertexFactoryCopyData,
		FTerrainVertexFactory*,VertexFactory,this,
		const DataType*,DataCopy,&Other.Data,
	{
		VertexFactory->Data = *DataCopy;
	});
	BeginUpdateResourceRHI(this);
}

/**
* Vertex factory interface for creating a corresponding decal vertex factory
* Copies the data from this existing vertex factory.
*
* @return new allocated decal vertex factory
*/
FDecalVertexFactoryBase* FTerrainVertexFactory::CreateDecalVertexFactory() const
{
	FTerrainDecalVertexFactory* DecalFactory = new FTerrainDecalVertexFactory();
	DecalFactory->Copy(*this);
	return DecalFactory;
}

/**
 *	Initialize the component streams.
 *	
 *	@param	Buffer	Pointer to the vertex buffer that will hold the data streams.
 *	@param	Stride	The stride of the provided vertex buffer.
 */
UBOOL FTerrainVertexFactory::InitComponentStreams(FTerrainVertexBuffer* Buffer)
{
	// update vertex factory components and sync it
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		InitTerrainVertexFactory,
		FTerrainVertexFactory*,VertexFactory,this,
		FTerrainVertexBuffer*,Buffer,Buffer,
		{
			// position
			VertexFactory->Data.PositionComponent = FVertexStreamComponent(
				Buffer, STRUCT_OFFSET(FTerrainVertex, PackedCoordinates), sizeof(FTerrainVertex), PACKED_POSITION_TYPE);
			// gradients
			VertexFactory->Data.GradientComponent = FVertexStreamComponent(
				Buffer, STRUCT_OFFSET(FTerrainVertex, GradientX), sizeof(FTerrainVertex), VET_Short2);
		});

	return TRUE;
}

/** Shader parameters for use with FTerrainVertexFactory */
class FTerrainVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:
	/**
	 * Bind shader constants by name
	 * @param	ParameterMap - mapping of named shader constants to indices
	 */
	virtual void Bind(const FShaderParameterMap& ParameterMap)
	{
		LocalToWorldParameter.Bind(ParameterMap,TEXT("LocalToWorld"));
		WorldToLocalParameter.Bind(ParameterMap,TEXT("WorldToLocal"),TRUE);
		LocalToViewParameter.Bind(ParameterMap,TEXT("LocalToView"),TRUE);
		LightmapCoordinateScaleBiasParameter.Bind(ParameterMap,TEXT("TerrainLightmapCoordinateScaleBias"),TRUE);
		TessellationInterpolation_Parameter.Bind(ParameterMap,TEXT("TessellationInterpolation"), TRUE);
		InvMaxTessLevel_ZScale_Parameter.Bind(ParameterMap,TEXT("InvMaxTesselationLevel_ZScale"), TRUE);
		InvTerrainSize_SectionBase_Parameter.Bind(ParameterMap,TEXT("InvTerrainSize_SectionBase"), TRUE);
		TessellationDistanceScaleParameter.Bind(ParameterMap,TEXT("TessellationDistanceScale"), TRUE);
		TessInterpDistanceValuesParameter.Bind(ParameterMap,TEXT("TessInterpDistanceValues"),TRUE);
		TerrainLayerCoordinateOffsetParameter.Bind(ParameterMap,TEXT("TerrainLayerCoordinateOffset"),TRUE);
	}

	/**
	 * Serialize shader params to an archive
	 * @param	Ar - archive to serialize to
	 */
	virtual void Serialize(FArchive& Ar)
	{
		Ar << LocalToWorldParameter;
		Ar << WorldToLocalParameter;
		Ar << LocalToViewParameter;
		Ar << LightmapCoordinateScaleBiasParameter;
		Ar << TessellationInterpolation_Parameter;
		Ar << InvMaxTessLevel_ZScale_Parameter;
		Ar << InvTerrainSize_SectionBase_Parameter;
		FShaderParameter Temp;
		Ar << Temp;
		Ar << TessellationDistanceScaleParameter;
		Ar << TessInterpDistanceValuesParameter;
		if( Ar.Ver() >= VER_ADDED_TERRAINLAYERCOORDINATEOFFSET_PARAM )
		{
			Ar << TerrainLayerCoordinateOffsetParameter;
		}
	}

	/**
	 * Set any shader data specific to this vertex factory
	 */
	virtual void Set(FShader* VertexShader,const FVertexFactory* VertexFactory,const FSceneView& View) const
	{
		FTerrainVertexFactory* TerrainVF = (FTerrainVertexFactory*)VertexFactory;
		FTerrainObject* TerrainObject = TerrainVF->GetTerrainObject();

		FVector4 LightMapValue;
		if (LightmapCoordinateScaleBiasParameter.IsBound())
		{
			INT LightMapRes = TerrainObject->GetLightMapResolution();

			// Assuming DXT_1 compression at the moment...
			INT PixelPaddingX = GPixelFormats[PF_DXT1].BlockSizeX;
			INT PixelPaddingY = GPixelFormats[PF_DXT1].BlockSizeY;
			if (GAllowLightmapCompression == FALSE)
			{
				PixelPaddingX = GPixelFormats[PF_A8R8G8B8].BlockSizeX;
				PixelPaddingY = GPixelFormats[PF_A8R8G8B8].BlockSizeY;
			}

			INT PatchExpandCountX = (TERRAIN_PATCH_EXPAND_SCALAR * PixelPaddingX) / LightMapRes;
			INT PatchExpandCountY = (TERRAIN_PATCH_EXPAND_SCALAR * PixelPaddingY) / LightMapRes;

			PatchExpandCountX = Max<INT>(1, PatchExpandCountX);
			PatchExpandCountY = Max<INT>(1, PatchExpandCountY);

			LightMapValue.X = (FLOAT)LightMapRes / ((FLOAT)(TerrainObject->GetComponentTrueSectionSizeX() + 2 * PatchExpandCountX) * (FLOAT)LightMapRes + 1.0f);
			LightMapValue.Y = (FLOAT)LightMapRes / ((FLOAT)(TerrainObject->GetComponentTrueSectionSizeY() + 2 * PatchExpandCountY) * (FLOAT)LightMapRes + 1.0f);
			// NOTE: In the usf file these are reversed
			//       Ie, the Y offset is in Z, the X in W.
			// Rather than change the shader (and recompile)
			// we are just fixing up here
			LightMapValue.Z = PatchExpandCountY * LightMapValue.Y;
			LightMapValue.W = PatchExpandCountX * LightMapValue.X;
		}

		if (LocalToViewParameter.IsBound())
		{
			FMatrix Value = TerrainObject->GetLocalToWorld() * View.ViewMatrix;
			SetVertexShaderValue( VertexShader->GetVertexShader(), LocalToViewParameter, Value);
		}

		if (TessellationInterpolation_Parameter.IsBound())
		{
			FLOAT Value = 1.0f;
			SetVertexShaderValue(VertexShader->GetVertexShader(),TessellationInterpolation_Parameter,Value);
		}

		if (InvMaxTessLevel_ZScale_Parameter.IsBound())
		{
			FVector4 Value;

			Value.X = 1.0f;
			Value.Y = TerrainObject->GetTerrainHeightScale();
			Value.Z = TerrainObject->GetScaleFactorX();
			Value.W = TerrainObject->GetScaleFactorY();
			SetVertexShaderValue(VertexShader->GetVertexShader(),InvMaxTessLevel_ZScale_Parameter,Value);
		}
		if (InvTerrainSize_SectionBase_Parameter.IsBound())
		{
			FVector4 Value;
			if (GPlatformNeedsPowerOfTwoTextures) // power of two
			{
				Value.X = 1.0f / appRoundUpToPowerOfTwo(TerrainObject->GetNumVerticesX());
				Value.Y = 1.0f / appRoundUpToPowerOfTwo(TerrainObject->GetNumVerticesY());
			}
			else
			{
				Value.X = 1.0f / TerrainObject->GetNumVerticesX();
				Value.Y = 1.0f / TerrainObject->GetNumVerticesY();
			}
			Value.Z = TerrainObject->GetComponentSectionBaseX();
			Value.W = TerrainObject->GetComponentSectionBaseY();
			SetVertexShaderValue(VertexShader->GetVertexShader(),InvTerrainSize_SectionBase_Parameter,Value);
		}
		if (LightmapCoordinateScaleBiasParameter.IsBound())
		{
			SetVertexShaderValue(VertexShader->GetVertexShader(),LightmapCoordinateScaleBiasParameter,LightMapValue);
		}
		if (TessellationDistanceScaleParameter.IsBound())
		{
			FVector4 Value(TerrainObject->GetTessellationDistanceScale(), 0.0f, 0.0f, 0.0f);
			SetVertexShaderValue( VertexShader->GetVertexShader(), TessellationDistanceScaleParameter, Value);
		}
		if (TessInterpDistanceValuesParameter.IsBound())
		{
			static const FLOAT TessInterpDistanceValues[][4] =
			{
				{     0.0f,    -1.0f, 0.0f, 0.0f },		// Highest tessellation level - NEVER goes away
				{ 16384.0f, 16384.0f, 0.0f, 0.0f },		// 1 level  up (16384 .. 32768)
				{  8192.0f,  8192.0f, 0.0f, 0.0f },		// 2 levels up ( 8192 .. 16384)
				{  4096.0f,  4096.0f, 0.0f, 0.0f },		// 3 levels up ( 4096 ..  8192)
				{     0.0f,  4096.0f, 0.0f, 0.0f }		// 4 levels up (    0 ..  4096)
			};
			SetVertexShaderValues( VertexShader->GetVertexShader(), TessInterpDistanceValuesParameter, TessInterpDistanceValues, 5);
		}
		if (TerrainLayerCoordinateOffsetParameter.IsBound())
		{
			SetVertexShaderValue( VertexShader->GetVertexShader(), TerrainLayerCoordinateOffsetParameter, TerrainObject->GetLayerCoordinateOffset());
		}
	}

	/**
	 * 
	 */
	virtual void SetMesh(FShader* VertexShader, const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View) const
	{
		const FMeshBatchElement& BatchElement = Mesh.Elements(BatchElementIndex);
		SetVertexShaderValue(
			VertexShader->GetVertexShader(),
			LocalToWorldParameter,
			BatchElement.LocalToWorld.ConcatTranslation(View.PreViewTranslation)
			);
		SetVertexShaderValue(VertexShader->GetVertexShader(),WorldToLocalParameter,BatchElement.WorldToLocal);
	}

private:
	INT	TessellationLevel;
	FShaderParameter LocalToWorldParameter;
	FShaderParameter WorldToLocalParameter;
	FShaderParameter LocalToViewParameter;
	FShaderParameter LightmapCoordinateScaleBiasParameter;
	FShaderParameter TessellationInterpolation_Parameter;
	FShaderParameter InvMaxTessLevel_ZScale_Parameter;
	FShaderParameter InvTerrainSize_SectionBase_Parameter;
	FShaderParameter TessellationDistanceScaleParameter;
	FShaderParameter TessInterpDistanceValuesParameter;
	FShaderParameter TerrainLayerCoordinateOffsetParameter;
};

FVertexFactoryShaderParameters* FTerrainVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FTerrainVertexFactoryShaderParameters() : NULL;
}


/** bind terrain vertex factory to its shader file and its shader parameters */
IMPLEMENT_VERTEX_FACTORY_TYPE(FTerrainVertexFactory, "TerrainVertexFactory", TRUE, TRUE, TRUE, FALSE, TRUE, VER_TERRAIN_REMOVED_DISPLACEMENTS,0);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FTerrainDecalVertexFactory
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FTerrainDecalVertexFactoryShaderParameters : public FTerrainVertexFactoryShaderParameters
{
public:
	typedef FTerrainVertexFactoryShaderParameters Super;

	/**
	 * Bind shader constants by name
	 * @param	ParameterMap - mapping of named shader constants to indices
	 */
	virtual void Bind(const FShaderParameterMap& ParameterMap)
	{
		Super::Bind( ParameterMap );
		DecalMatrixParameter.Bind( ParameterMap, TEXT("DecalMatrix"), TRUE );
		DecalLocationParameter.Bind( ParameterMap, TEXT("DecalLocation"), TRUE );
		DecalOffsetParameter.Bind( ParameterMap, TEXT("DecalOffset"), TRUE );
		DecalLocalBinormal.Bind( ParameterMap, TEXT("DecalLocalBinormal"), TRUE );
		DecalLocalTangent.Bind( ParameterMap, TEXT("DecalLocalTangent"), TRUE );
		DecalLocalNormal.Bind( ParameterMap, TEXT("DecalLocalNormal"), TRUE );
		DecalBlendIntervalParameter.Bind( ParameterMap, TEXT("DecalBlendInterval"), TRUE );
	}

	/**
	* Serialize shader params to an archive
	* @param	Ar - archive to serialize to
	*/
	virtual void Serialize(FArchive& Ar)
	{
		Super::Serialize( Ar );
		Ar << DecalMatrixParameter;
		Ar << DecalLocationParameter;
		Ar << DecalOffsetParameter;
		Ar << DecalLocalBinormal;
		Ar << DecalLocalTangent;
		Ar << DecalLocalNormal;
		Ar << DecalBlendIntervalParameter;
	}

	/**
	 * Set any shader data specific to this vertex factory
	 */
	virtual void Set(FShader* VertexShader,const FVertexFactory* VertexFactory,const FSceneView& View) const
	{
		Super::Set( VertexShader, VertexFactory, View );

		FTerrainVertexFactory* TerrainVF = (FTerrainVertexFactory*)VertexFactory;
		FTerrainDecalVertexFactoryBase* DecalBase = TerrainVF->CastToFTerrainDecalVertexFactoryBase();
		if (DecalBase)
		{
			SetVertexShaderValue(  VertexShader->GetVertexShader(), DecalMatrixParameter, DecalBase->GetDecalMatrix() );
			SetVertexShaderValue(  VertexShader->GetVertexShader(), DecalLocationParameter, DecalBase->GetDecalLocation() + View.PreViewTranslation  );
			SetVertexShaderValue(  VertexShader->GetVertexShader(), DecalOffsetParameter, DecalBase->GetDecalOffset() );
			SetVertexShaderValue(  VertexShader->GetVertexShader(), DecalLocalBinormal, DecalBase->GetDecalLocalBinormal() );
			SetVertexShaderValue(  VertexShader->GetVertexShader(), DecalLocalTangent, DecalBase->GetDecalLocalTangent() );
			SetVertexShaderValue(  VertexShader->GetVertexShader(), DecalLocalNormal, DecalBase->GetDecalLocalNormal() );
			SetVertexShaderValue(  VertexShader->GetVertexShader(), DecalBlendIntervalParameter, DecalBase->GetDecalMinMaxBlend() );
		}
	}

private:
	FShaderParameter DecalMatrixParameter;	
	FShaderParameter DecalLocationParameter;
	FShaderParameter DecalOffsetParameter;
	FShaderParameter DecalLocalBinormal;
	FShaderParameter DecalLocalTangent;
	FShaderParameter DecalLocalNormal;
	FShaderParameter DecalBlendIntervalParameter;
};

/**
 * Should we cache the material's shader type on this platform with this vertex factory? 
 */
UBOOL FTerrainDecalVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
#if MOBILE
	return FALSE;
#endif

	// Only compile decal materials and special engine materials for a terrain decal vertex factory.
	// The special engine materials must be compiled for the terrain decal vertex factory because they are used with it for wireframe, etc.
	if ( Material->IsUsedWithDecals() || AllowDebugViewmodes(Platform) && Material->IsSpecialEngineMaterial() || Material->IsDecalMaterial() )
	{
		if ( !appStrstr(ShaderType->GetName(),TEXT("VertexLightMapPolicy")) )
		{
			return TRUE;
		}
	}
	return FALSE;
}

FVertexFactoryShaderParameters* FTerrainDecalVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FTerrainDecalVertexFactoryShaderParameters() : NULL;
}

/** bind terrain decal vertex factory to its shader file and its shader parameters */
IMPLEMENT_VERTEX_FACTORY_TYPE(FTerrainDecalVertexFactory, "TerrainVertexFactory", TRUE, TRUE, TRUE, FALSE, TRUE, VER_TERRAIN_REMOVED_DISPLACEMENTS,0);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FTerrainMorphVertexFactory
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FRenderResource interface.
void FTerrainMorphVertexFactory::InitRHI()
{
	// list of declaration items
	FVertexDeclarationElementList Elements;

	// position decls
	Elements.AddItem(AccessStreamComponent(Data.PositionComponent, VEU_Position));
	// gradients
	Elements.AddItem(AccessStreamComponent(Data.GradientComponent, VEU_Tangent));
	// height transitions
	Elements.AddItem(AccessStreamComponent(Data.HeightTransitionComponent, VEU_Normal));

	// create the actual device decls
	//@todo.SAS. Include shadow map and light map
	InitDeclaration(Elements,FVertexFactory::DataType(),FALSE,FALSE);
}

/**
* Copy the data from another vertex factory
* @param Other - factory to copy from
*/
void FTerrainMorphVertexFactory::Copy(const FTerrainMorphVertexFactory& Other)
{
	SetTerrainObject(Other.GetTerrainObject());
	SetTessellationLevel(Other.GetTessellationLevel());
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FTerrainMorphVertexFactoryCopyData,
		FTerrainMorphVertexFactory*,VertexFactory,this,
		const DataType*,DataCopy,&Other.Data,
	{
		VertexFactory->Data = *DataCopy;
	});
	BeginUpdateResourceRHI(this);
}

/**
* Vertex factory interface for creating a corresponding decal vertex factory
* Copies the data from this existing vertex factory.
*
* @return new allocated decal vertex factory
*/
FDecalVertexFactoryBase* FTerrainMorphVertexFactory::CreateDecalVertexFactory() const
{
	FTerrainMorphDecalVertexFactory* DecalFactory = new FTerrainMorphDecalVertexFactory();
	DecalFactory->Copy(*this);
	return DecalFactory;
}

/**
 *	Initialize the component streams.
 *	
 *	@param	Buffer	Pointer to the vertex buffer that will hold the data streams.
 *	@param	Stride	The stride of the provided vertex buffer.
 */
UBOOL FTerrainMorphVertexFactory::InitComponentStreams(FTerrainVertexBuffer* Buffer)
{
	// update vertex factory components and sync it
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		InitTerrainMorphVertexFactory,
		FTerrainMorphVertexFactory*,VertexFactory,this,
		FTerrainVertexBuffer*,Buffer,Buffer,
		{
			// position
			VertexFactory->Data.PositionComponent = FVertexStreamComponent(
				Buffer, STRUCT_OFFSET(FTerrainVertex, PackedCoordinates), sizeof(FTerrainMorphingVertex), PACKED_POSITION_TYPE);
			// gradients
			VertexFactory->Data.GradientComponent = FVertexStreamComponent(
				Buffer, STRUCT_OFFSET(FTerrainVertex, GradientX), sizeof(FTerrainMorphingVertex), VET_Short2);
			// Transitions
			VertexFactory->Data.HeightTransitionComponent = FVertexStreamComponent(
				Buffer, STRUCT_OFFSET(FTerrainMorphingVertex, PackedData), sizeof(FTerrainMorphingVertex), PACKED_POSITION_TYPE);
		});

	return TRUE;
}

FVertexFactoryShaderParameters* FTerrainMorphVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FTerrainVertexFactoryShaderParameters() : NULL;
}

/** bind terrain vertex factory to its shader file and its shader parameters */
IMPLEMENT_VERTEX_FACTORY_TYPE(FTerrainMorphVertexFactory, "TerrainVertexFactory", TRUE, TRUE, TRUE, FALSE, TRUE, VER_TERRAIN_REMOVED_DISPLACEMENTS,0);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FTerrainMorphDecalVertexFactory
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Should we cache the material's shader type on this platform with this vertex factory? 
 */
UBOOL FTerrainMorphDecalVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
#if MOBILE
	return FALSE;
#endif

	// Only compile decal materials and special engine materials for a terrain decal vertex factory.
	// The special engine materials must be compiled for the terrain decal vertex factory because they are used with it for wireframe, etc.
	if ( Material->IsUsedWithDecals() || AllowDebugViewmodes(Platform) && Material->IsSpecialEngineMaterial() || Material->IsDecalMaterial() )
	{
		if ( !appStrstr(ShaderType->GetName(),TEXT("VertexLightMapPolicy")) )
		{
			return TRUE;
		}
	}
	return FALSE;
}

FVertexFactoryShaderParameters* FTerrainMorphDecalVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FTerrainDecalVertexFactoryShaderParameters() : NULL;
}

/** bind terrain decal vertex factory to its shader file and its shader parameters */
IMPLEMENT_VERTEX_FACTORY_TYPE(FTerrainMorphDecalVertexFactory, "TerrainVertexFactory", TRUE, TRUE, TRUE, FALSE, TRUE, VER_TERRAIN_REMOVED_DISPLACEMENTS,0);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FTerrainFullMorphVertexFactory
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FRenderResource interface.
void FTerrainFullMorphVertexFactory::InitRHI()
{
	// list of declaration items
	FVertexDeclarationElementList Elements;

	// position decls
	Elements.AddItem(AccessStreamComponent(Data.PositionComponent, VEU_Position));
	// gradients
	Elements.AddItem(AccessStreamComponent(Data.GradientComponent, VEU_Tangent));
	// height transitions
	Elements.AddItem(AccessStreamComponent(Data.HeightTransitionComponent, VEU_Normal));
	// gradient transitions
	Elements.AddItem(AccessStreamComponent(Data.GradientTransitionComponent, VEU_Binormal));

	// create the actual device decls
	//@todo.SAS. Include shadow map and light map
	InitDeclaration(Elements,FVertexFactory::DataType(),FALSE,FALSE);
}

/**
* Copy the data from another vertex factory
* @param Other - factory to copy from
*/
void FTerrainFullMorphVertexFactory::Copy(const FTerrainFullMorphVertexFactory& Other)
{
	SetTerrainObject(Other.GetTerrainObject());
	SetTessellationLevel(Other.GetTessellationLevel());
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FTerrainFullMorphVertexFactoryCopyData,
		FTerrainFullMorphVertexFactory*,VertexFactory,this,
		const DataType*,DataCopy,&Other.Data,
	{
		VertexFactory->Data = *DataCopy;
	});	
	BeginUpdateResourceRHI(this);
}

/**
* Vertex factory interface for creating a corresponding decal vertex factory
* Copies the data from this existing vertex factory.
*
* @return new allocated decal vertex factory
*/
FDecalVertexFactoryBase* FTerrainFullMorphVertexFactory::CreateDecalVertexFactory() const
{
	FTerrainFullMorphDecalVertexFactory* DecalFactory = new FTerrainFullMorphDecalVertexFactory();
	DecalFactory->Copy(*this);
	return DecalFactory;
}

/**
 *	Initialize the component streams.
 *	
 *	@param	Buffer	Pointer to the vertex buffer that will hold the data streams.
 *	@param	Stride	The stride of the provided vertex buffer.
 */
UBOOL FTerrainFullMorphVertexFactory::InitComponentStreams(FTerrainVertexBuffer* Buffer)
{
	// update vertex factory components and sync it
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		InitTerrainFullMorphVertexFactory,
		FTerrainFullMorphVertexFactory*,VertexFactory,this,
		FTerrainVertexBuffer*,Buffer,Buffer,
		{
			// position
			VertexFactory->Data.PositionComponent = FVertexStreamComponent(
				Buffer, STRUCT_OFFSET(FTerrainVertex, PackedCoordinates), sizeof(FTerrainFullMorphingVertex), PACKED_POSITION_TYPE);
			// gradients
			VertexFactory->Data.GradientComponent = FVertexStreamComponent(
				Buffer, STRUCT_OFFSET(FTerrainVertex, GradientX), sizeof(FTerrainFullMorphingVertex), VET_Short2);
			// Transitions
			VertexFactory->Data.HeightTransitionComponent = FVertexStreamComponent(
				Buffer, STRUCT_OFFSET(FTerrainFullMorphingVertex, PackedData), sizeof(FTerrainFullMorphingVertex), PACKED_POSITION_TYPE);
			VertexFactory->Data.GradientTransitionComponent = FVertexStreamComponent(
				Buffer, STRUCT_OFFSET(FTerrainFullMorphingVertex, TransGradientX), sizeof(FTerrainFullMorphingVertex), VET_Short2);
		});

	return TRUE;
}

FVertexFactoryShaderParameters* FTerrainFullMorphVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FTerrainVertexFactoryShaderParameters() : NULL;
}

/** bind terrain vertex factory to its shader file and its shader parameters */
IMPLEMENT_VERTEX_FACTORY_TYPE(FTerrainFullMorphVertexFactory, "TerrainVertexFactory", TRUE, TRUE, TRUE, FALSE, TRUE, VER_TERRAIN_REMOVED_DISPLACEMENTS,0);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FTerrainFullMorphDecalVertexFactory
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Should we cache the material's shader type on this platform with this vertex factory? 
 */
UBOOL FTerrainFullMorphDecalVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
#if MOBILE
	return FALSE;
#endif

	// Only compile decal materials and special engine materials for a terrain decal vertex factory.
	// The special engine materials must be compiled for the terrain decal vertex factory because they are used with it for wireframe, etc.
	if ( Material->IsUsedWithDecals() || AllowDebugViewmodes(Platform) && Material->IsSpecialEngineMaterial() || Material->IsDecalMaterial() )
	{
		if ( !appStrstr(ShaderType->GetName(),TEXT("VertexLightMapPolicy")) )
		{
			return TRUE;
		}
	}
	return FALSE;
}

FVertexFactoryShaderParameters* FTerrainFullMorphDecalVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FTerrainDecalVertexFactoryShaderParameters() : NULL;
}

/** bind terrain decal vertex factory to its shader file and its shader parameters */
IMPLEMENT_VERTEX_FACTORY_TYPE(FTerrainFullMorphDecalVertexFactory, "TerrainVertexFactory", TRUE, TRUE, TRUE, FALSE, TRUE, VER_TERRAIN_REMOVED_DISPLACEMENTS,0);

