/*=============================================================================
	GPUVertexFactory.cpp: GPU skin vertex factory implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "GPUSkinVertexFactory.h"
#include "UnSkeletalRenderGPUSkin.h"	// FPreviousPerBoneMotionBlur
#include "SceneRenderTargets.h"			// GSceneRenderTargets

/*-----------------------------------------------------------------------------
FBoneDataTexture
-----------------------------------------------------------------------------*/


/** constructor */
FBoneDataTexture::FBoneDataTexture() :SizeX(0)
{
}

/** 
* @param TotalTexelCount sum of all chunks bone count
*/
void FBoneDataTexture::SetTexelSize(UINT TotalTexelCount)
{
	checkSlow(TotalTexelCount);

	SizeX = TotalTexelCount;
}

/** 
* call UnlockData() after this one
* @return never 0
*/
FLOAT* FBoneDataTexture::LockData()
{
	checkSlow(IsInRenderingThread());
	checkSlow(GetSizeX());

	UINT TextureStride = 0;
	FLOAT* ret = (FLOAT*)RHILockTexture2D(Texture2DRHI, 0, TRUE, TextureStride, FALSE);

	checkSlow(ret);

	return ret;
}

/**
* Needs to be called after LockData()
*/
void FBoneDataTexture::UnlockData()
{
	RHIUnlockTexture2D(Texture2DRHI, 0, FALSE);
}

/** Called when the resource is initialized. This is only called by the rendering thread. */
void FBoneDataTexture::InitDynamicRHI()
{
	checkSlow(GetSizeX());

	WORD Flags = TexCreate_Dynamic | TexCreate_NoTiling | TexCreate_NoMipTail;
	UINT NumMips = 1;
	EPixelFormat Format = PF_A32B32G32R32F;
	Texture2DRHI = RHICreateTexture2D(GetSizeX(), GetSizeY(), Format, NumMips, Flags, NULL );
	TextureRHI = Texture2DRHI;

	INC_DWORD_STAT_BY( STAT_SkeletalMeshMotionBlurSkinningMemory, ComputeMemorySize());
}

/** Called when the resource is released. This is only called by the rendering thread. */
void FBoneDataTexture::ReleaseDynamicRHI()
{
	DEC_DWORD_STAT_BY( STAT_SkeletalMeshMotionBlurSkinningMemory, ComputeMemorySize());

	FTextureResource::ReleaseRHI();
	Texture2DRHI.SafeRelease();
}

/** Returns the width of the texture in pixels. */
UINT FBoneDataTexture::GetSizeX() const
{
	return SizeX;
}

/** Returns the height of the texture in pixels. */
UINT FBoneDataTexture::GetSizeY() const
{
	return 1;
}
/** Accessor */
FTexture2DRHIRef FBoneDataTexture::GetTexture2DRHI()
{
	return Texture2DRHI;
}

/** @return in bytes */
UINT FBoneDataTexture::ComputeMemorySize()
{
	checkSlow(IsInRenderingThread());
	return RHIGetTextureSize(GetTexture2DRHI());
}

/*-----------------------------------------------------------------------------
FGPUSkinVertexFactory
-----------------------------------------------------------------------------*/

/** ShouldCache function that is shared with the decal vertex factories. */
UBOOL FGPUSkinVertexFactory::SharedShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const FShaderType* ShaderType)
{
	return Material->IsUsedWithSkeletalMesh() || Material->IsUsedWithFracturedMeshes() || Material->IsSpecialEngineMaterial();
}

UBOOL FGPUSkinVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const FShaderType* ShaderType)
{
	return SharedShouldCache(Platform, Material, ShaderType) && !Material->IsUsedWithDecals();
}

void FGPUSkinVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
{
#if QUAT_SKINNING
	OutEnvironment.Definitions.Set(TEXT("QUAT_SKINNING"),TEXT("1"));
#else
	OutEnvironment.Definitions.Set(TEXT("QUAT_SKINNING"),TEXT("0"));
#endif
	OutEnvironment.Definitions.Set(TEXT("GPUSKIN_FACTORY"),TEXT("1"));
}

/**
* Add the decl elements for the streams
* @param InData - type with stream components
* @param OutElements - vertex decl list to modify
*/
void FGPUSkinVertexFactory::AddVertexElements(DataType& InData, FVertexDeclarationElementList& OutElements)
{
	// position decls
	OutElements.AddItem(AccessStreamComponent(InData.PositionComponent,VEU_Position));

	// tangent basis vector decls
	OutElements.AddItem(AccessStreamComponent(InData.TangentBasisComponents[0],VEU_Tangent));
	OutElements.AddItem(AccessStreamComponent(InData.TangentBasisComponents[1],VEU_Normal));

	// texture coordinate decls
	if(InData.TextureCoordinates.Num())
	{
		for(UINT CoordinateIndex = 0;CoordinateIndex < InData.TextureCoordinates.Num();CoordinateIndex++)
		{
			OutElements.AddItem(AccessStreamComponent(
				InData.TextureCoordinates(CoordinateIndex),
				VEU_TextureCoordinate,
				CoordinateIndex
				));
		}

		for(UINT CoordinateIndex = InData.TextureCoordinates.Num();CoordinateIndex < MAX_TEXCOORDS;CoordinateIndex++)
		{
			OutElements.AddItem(AccessStreamComponent(
				InData.TextureCoordinates(InData.TextureCoordinates.Num() - 1),
				VEU_TextureCoordinate,
				CoordinateIndex
				));
		}
	}

	// Account for the possibility that the mesh has no vertex colors
	if( InData.ColorComponent.VertexBuffer )
	{
		OutElements.AddItem(AccessStreamComponent(InData.ColorComponent, VEU_Color));
	}
	else
	{
		//If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
		//This wastes 4 bytes of bandwidth per vertex, but prevents having to compile out twice the number of vertex factories.
		FVertexStreamComponent NullColorComponent(&GNullColorVertexBuffer, 0, 0, VET_Color);
		OutElements.AddItem(AccessStreamComponent(NullColorComponent,VEU_Color));
	}

	// bone indices decls
	OutElements.AddItem(AccessStreamComponent(InData.BoneIndices,VEU_BlendIndices));

	// bone weights decls
	OutElements.AddItem(AccessStreamComponent(InData.BoneWeights,VEU_BlendWeight));
}

/**
* Creates declarations for each of the vertex stream components and
* initializes the device resource
*/
void FGPUSkinVertexFactory::InitRHI()
{
	// list of declaration items
	FVertexDeclarationElementList Elements;
	AddVertexElements(Data,Elements);	

	// create the actual device decls
	InitDeclaration(Elements,FVertexFactory::DataType(),FALSE,FALSE);
}

/*-----------------------------------------------------------------------------
FGPUSkinVertexFactoryShaderParameters
-----------------------------------------------------------------------------*/

/** Shader parameters for use with FGPUSkinVertexFactory */
class FGPUSkinVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
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
#if QUAT_SKINNING
		BoneMatricesParameter.Bind(ParameterMap,TEXT("BoneQuats"));
		BoneScalesParameter.Bind(ParameterMap,TEXT("BoneScales"), TRUE);
#else
		BoneMatricesParameter.Bind(ParameterMap,TEXT("BoneMatrices"), TRUE);
#endif
		BoneIndexOffsetAndScaleParameter.Bind(ParameterMap,TEXT("BoneIndexOffsetAndScale"),TRUE);
		MeshOriginParameter.Bind(ParameterMap,TEXT("MeshOrigin"),TRUE);
		MeshExtensionParameter.Bind(ParameterMap,TEXT("MeshExtension"),TRUE);
		PreviousBoneMatricesParameter.Bind(ParameterMap,TEXT("PreviousBoneMatrices"),TRUE);
		UsePerBoneMotionBlurParameter.Bind(ParameterMap,TEXT("bUsePerBoneMotionBlur"),TRUE);
	}
	/**
	* Serialize shader params to an archive
	* @param	Ar - archive to serialize to
	*/
	virtual void Serialize(FArchive& Ar)
	{
		Ar << LocalToWorldParameter;
		Ar << WorldToLocalParameter;
		Ar << BoneMatricesParameter;
#if QUAT_SKINNING
		Ar << BoneScalesParameter;
#endif
		Ar << BoneIndexOffsetAndScaleParameter;
		Ar << MeshOriginParameter;
		Ar << MeshExtensionParameter;
		Ar << PreviousBoneMatricesParameter;
		Ar << UsePerBoneMotionBlurParameter;
		
		// set parameter names for platforms that need them
		LocalToWorldParameter.SetShaderParamName(TEXT("LocalToWorld"));
		BoneMatricesParameter.SetShaderParamName(TEXT("BoneMatrices"));
	}
	/**
	* Set any shader data specific to this vertex factory
	*/
	virtual void Set(FShader* VertexShader,const FVertexFactory* VertexFactory,const FSceneView& View) const
	{
		const FGPUSkinVertexFactory::ShaderDataType& ShaderData = ((const FGPUSkinVertexFactory*)VertexFactory)->GetShaderData();
		SetVertexShaderValues<FBoneSkinning>(
			VertexShader->GetVertexShader(),
			BoneMatricesParameter,
			ShaderData.BoneMatrices.GetTypedData(),
			ShaderData.BoneMatrices.Num()
			);
#if QUAT_SKINNING
		SetVertexShaderFloats(
			VertexShader->GetVertexShader(),
			BoneScalesParameter,
			ShaderData.BoneScales.GetTypedData(),
			ShaderData.BoneScales.Num());
#endif
		SetVertexShaderValue<FVector>(
			VertexShader->GetVertexShader(), 
			MeshOriginParameter, 
			ShaderData.MeshOrigin
			);
		SetVertexShaderValue<FVector>(
			VertexShader->GetVertexShader(), 
			MeshExtensionParameter, 
			ShaderData.MeshExtension
			);


		UBOOL bLocalPerBoneMotionBlur = FALSE;

		if(GSceneRenderTargets.PrevPerBoneMotionBlur.IsLocked())
		{
			// we are in the velocity rendering pass

			// 0xffffffff or valid index
			UINT OldBoneDataIndex = ShaderData.GetOldBoneData(View.FrameNumber);

			// Read old data if it was written last frame (normal data) or this frame (e.g. split screen)
			bLocalPerBoneMotionBlur = (OldBoneDataIndex != 0xffffffff) && View.RenderingOverrides.bAllowMotionBlurSkinning;

			// we tell the shader where to pickup the data (always, even if we don't have bone data, to avoid false binding)
			SetVertexShaderTextureParameter(
				VertexShader->GetVertexShader(),
				PreviousBoneMatricesParameter,
				GSceneRenderTargets.PrevPerBoneMotionBlur.GetReadData()->GetTexture2DRHI());

			if(bLocalPerBoneMotionBlur)
			{
				FVector4 BoneIndexOffsetAndScale;

				// we have old bone data for this draw call available
				FLOAT InvTextureWidth = GSceneRenderTargets.PrevPerBoneMotionBlur.GetInvSizeX();	
				// 0.5 for the half texel offset to do a stable lookup at the texel center
				BoneIndexOffsetAndScale.X = (OldBoneDataIndex + 0.5f) * InvTextureWidth;
				BoneIndexOffsetAndScale.Y = (OldBoneDataIndex + 1.5f) * InvTextureWidth;
				BoneIndexOffsetAndScale.Z = (OldBoneDataIndex + 2.5f) * InvTextureWidth;
				// 3 because we have three texels per bone (one texel: float4, one bone matrix: 4x3 matrix)
				BoneIndexOffsetAndScale.W = 3.0f * InvTextureWidth;

				SetVertexShaderValue<FVector4>(
					VertexShader->GetVertexShader(), 
					BoneIndexOffsetAndScaleParameter, 
					BoneIndexOffsetAndScale
					);
			}

#if XBOX
			// on Xbox360 we use static branching to get more efficient shader execution
			// Should be a bool on all platforms, but bool params don't work in vertex shaders yet (TTP 125134)
			SetVertexShaderBool(VertexShader->GetVertexShader(), UsePerBoneMotionBlurParameter, bLocalPerBoneMotionBlur);	
#else // XBOX
/*			if(!bLocalPerBoneMotionBlur)
			{
				// disable texture lookup (needed for PC)
				FVector4 BoneIndexOffsetAndScale(0, 0, 0, 0);

				SetVertexShaderValue<FVector4>(
					VertexShader->GetVertexShader(), 
					BoneIndexOffsetAndScaleParameter, 
					BoneIndexOffsetAndScale
					);
			}
*/
#endif // XBOX

			// if we haven't copied the data yet we skip the update (e.g. splitscreen)
			if(ShaderData.IsOldBoneDataUpdateNeeded(View.FrameNumber))
			{
				const FGPUSkinVertexFactory* GPUVertexFactory = (const FGPUSkinVertexFactory*)VertexFactory;

				// copy the bone data and tell the instance where it can pick it up next frame

				// append data to a buffer we bind next frame to read old matrix data for motionblur
				UINT OldBoneDataStartIndex = GSceneRenderTargets.PrevPerBoneMotionBlur.AppendData(ShaderData.BoneMatrices.GetTypedData(), ShaderData.BoneMatrices.Num());
				GPUVertexFactory->SetOldBoneDataStartIndex(View.FrameNumber, OldBoneDataStartIndex);
			}
		}
	}
	/**
	* Set the l2w transform shader
	*/
	virtual void SetMesh(FShader* VertexShader, const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View) const
	{
		const FMeshBatchElement& BatchElement = Mesh.Elements(BatchElementIndex);
		const FGPUSkinVertexFactory* VertexFactory = (const FGPUSkinVertexFactory*)Mesh.VertexFactory;
		const FGPUSkinVertexFactory::ShaderDataType& ShaderData = VertexFactory->GetShaderData();

		SetVertexShaderValue(
			VertexShader->GetVertexShader(),
			LocalToWorldParameter,
			BatchElement.LocalToWorld.ConcatTranslation(View.PreViewTranslation)
			);

		// Used to flip the normal direction if LocalToWorldRotDeterminant is negative.  
		// This prevents non-uniform negative scaling from making vectors transformed with CalcTangentToWorld pointing in the wrong quadrant.
		const FLOAT LocalToWorldRotDeterminant = BatchElement.LocalToWorld.RotDeterminant();

		// NOTE: This parameter is passed to the shader compiler as a 3x4 matrix.  We'll only use the
		//		 3x3 part for the WorldToLocal matrix, and the other 3 floats are general-purpose.
		FMatrix WorldToLocalWithFriends = BatchElement.WorldToLocal;

		UBOOL bLocalPerBoneMotionBlur = (ShaderData.GetOldBoneData(View.FrameNumber) != 0xffffffff) && View.RenderingOverrides.bAllowMotionBlurSkinning;

		// NOTE: We pack the data into the WorldToLocal 4x4 matrix in
		//		 order to free up vertex shader constants.
		//       Bone matrices use up a lot of constants so this is crucial!
		WorldToLocalWithFriends.M[0][3] = appFloatSelect(LocalToWorldRotDeterminant, 1, -1);
		WorldToLocalWithFriends.M[1][3] = bLocalPerBoneMotionBlur ? 1.0f : 0.0f;

		// This matrix should always be treated as a 3x3 in the shader code so we'll zero out the other
		// unused elements.
		WorldToLocalWithFriends.M[2][3] = 0.0f;
		WorldToLocalWithFriends.M[3][3] = 0.0f;

		// This does not set the full 4x4 matrix as the shader side only has the size of a 4x3 matrix.
		SetVertexShaderValue(VertexShader->GetVertexShader(),WorldToLocalParameter,WorldToLocalWithFriends);
	}
private:
	FShaderParameter LocalToWorldParameter;
	FShaderParameter WorldToLocalParameter;
	FShaderParameter BoneMatricesParameter;
#if QUAT_SKINNING
	FShaderParameter BoneScalesParameter;
#endif
	FShaderParameter BoneIndexOffsetAndScaleParameter;
	FShaderParameter MeshOriginParameter;
	FShaderParameter MeshExtensionParameter;
	FShaderResourceParameter PreviousBoneMatricesParameter;
	FShaderParameter UsePerBoneMotionBlurParameter;
};

FVertexFactoryShaderParameters* FGPUSkinVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FGPUSkinVertexFactoryShaderParameters() : NULL;
}

/** bind gpu skin vertex factory to its shader file and its shader parameters */
IMPLEMENT_VERTEX_FACTORY_TYPE(FGPUSkinVertexFactory, "GpuSkinVertexFactory", TRUE, FALSE, TRUE, FALSE, TRUE, VER_PERBONEMOTIONBLUR, 0);

/*-----------------------------------------------------------------------------
FGPUSkinMorphVertexFactory
-----------------------------------------------------------------------------*/

/**
* Modify compile environment to enable the morph blend codepath
* @param OutEnvironment - shader compile environment to modify
*/
void FGPUSkinMorphVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
{
	Super::ModifyCompilationEnvironment(Platform, OutEnvironment);
	OutEnvironment.Definitions.Set(TEXT("GPUSKIN_MORPH_BLEND"),TEXT("1"));
}

/** ShouldCache function that is shared with the decal vertex factories. */
UBOOL FGPUSkinMorphVertexFactory::SharedShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return (Material->IsUsedWithMorphTargets() || Material->IsSpecialEngineMaterial()) 
		&& Super::SharedShouldCache(Platform, Material, ShaderType);
}

/**
* Should we cache the material's shader type on this platform with this vertex factory? 
*/
UBOOL FGPUSkinMorphVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return SharedShouldCache(Platform, Material, ShaderType) && !Material->IsUsedWithDecals();
}

/**
* Add the decl elements for the streams
* @param InData - type with stream components
* @param OutElements - vertex decl list to modify
*/
void FGPUSkinMorphVertexFactory::AddVertexElements(DataType& InData, FVertexDeclarationElementList& OutElements)
{
	// add the base gpu skin elements
	FGPUSkinVertexFactory::AddVertexElements(InData,OutElements);
	// add the morph delta elements
	// NOTE: TEXCOORD6,TEXCOORD7 used instead of POSITION1,NORMAL1 since those semantics are not supported by Cg 
	OutElements.AddItem(AccessStreamComponent(InData.DeltaPositionComponent,VEU_TextureCoordinate,6));
	OutElements.AddItem(AccessStreamComponent(InData.DeltaTangentZComponent,VEU_TextureCoordinate,7));
}

/**
* Creates declarations for each of the vertex stream components and
* initializes the device resource
*/
void FGPUSkinMorphVertexFactory::InitRHI()
{
	// list of declaration items
	FVertexDeclarationElementList Elements;	
	AddVertexElements(MorphData,Elements);

	// create the actual device decls
	InitDeclaration(Elements,FVertexFactory::DataType(),FALSE,FALSE);
}

FVertexFactoryShaderParameters* FGPUSkinMorphVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FGPUSkinVertexFactoryShaderParameters() : NULL;
}

/** bind morph target gpu skin vertex factory to its shader file and its shader parameters */
IMPLEMENT_VERTEX_FACTORY_TYPE(FGPUSkinMorphVertexFactory, "GpuSkinVertexFactory", TRUE, FALSE, TRUE, FALSE, TRUE, VER_PERBONEMOTIONBLUR, 0);

/*-----------------------------------------------------------------------------
FGPUSkinDecalVertexFactory
-----------------------------------------------------------------------------*/

/**
 * Shader parameters for use with FGPUSkinDecalVertexFactory.
 */
class FGPUSkinDecalVertexFactoryShaderParameters : public FGPUSkinVertexFactoryShaderParameters
{
public:
	typedef FGPUSkinVertexFactoryShaderParameters Super;

	/**
	 * Bind shader constants by name
	 * @param	ParameterMap - mapping of named shader constants to indices
	 */
	virtual void Bind(const FShaderParameterMap& ParameterMap)
	{
		Super::Bind( ParameterMap );
		BoneToDecalRow0Parameter.Bind( ParameterMap, TEXT("BoneToDecalRow0"), TRUE );
		BoneToDecalRow1Parameter.Bind( ParameterMap, TEXT("BoneToDecalRow1"), TRUE );
		DecalLocationParameter.Bind( ParameterMap, TEXT("DecalLocation"), TRUE );
	}

	/**
	 * Serialize shader params to an archive
	 * @param	Ar - archive to serialize to
	 */
	virtual void Serialize(FArchive& Ar)
	{
		Super::Serialize( Ar );
		Ar << BoneToDecalRow0Parameter;
		Ar << BoneToDecalRow1Parameter;
		Ar << DecalLocationParameter;
	}

	/**
	 * Set any shader data specific to this vertex factory
	 */
	virtual void Set(FShader* VertexShader, const FVertexFactory* VertexFactory, const FSceneView& View) const
	{
		Super::Set( VertexShader, VertexFactory, View );

		FGPUSkinDecalVertexFactory * DecalVertexFactory = (FGPUSkinDecalVertexFactory *)VertexFactory;
		const FMatrix& DecalMtx = DecalVertexFactory->GetDecalMatrix();
		if ( BoneToDecalRow0Parameter.IsBound() )
		{
			const FVector4 Row0( DecalMtx.M[0][0], DecalMtx.M[1][0], DecalMtx.M[2][0], DecalMtx.M[3][0]);
			SetVertexShaderValue( VertexShader->GetVertexShader(), BoneToDecalRow0Parameter, Row0 );
		}
		if ( BoneToDecalRow1Parameter.IsBound() )
		{
			const FVector4 Row1( DecalMtx.M[0][1], DecalMtx.M[1][1], DecalMtx.M[2][1], DecalMtx.M[3][1]);
			SetVertexShaderValue( VertexShader->GetVertexShader(), BoneToDecalRow1Parameter, Row1 );
		}
		if ( DecalLocationParameter.IsBound() )
		{
			SetVertexShaderValue( VertexShader->GetVertexShader(), DecalLocationParameter, DecalVertexFactory->GetDecalLocation() );
		}
	}

private:
	FShaderParameter BoneToDecalRow0Parameter;
	FShaderParameter BoneToDecalRow1Parameter;
	FShaderParameter DecalLocationParameter;
};

/**
 * Should we cache the material's shader type on this platform with this vertex factory? 
 */
UBOOL FGPUSkinDecalVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return (Material->IsUsedWithDecals() || Material->IsDecalMaterial() || AllowDebugViewmodes(Platform) && Material->IsSpecialEngineMaterial()) &&
		Super::SharedShouldCache(Platform,Material,ShaderType);
}

/**
* Modify compile environment to enable the decal codepath
* @param OutEnvironment - shader compile environment to modify
*/
void FGPUSkinDecalVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
{
	Super::ModifyCompilationEnvironment(Platform, OutEnvironment);
	OutEnvironment.Definitions.Set(TEXT("GPUSKIN_DECAL"),TEXT("1"));
	OutEnvironment.Definitions.Set(TEXT("DECAL_FACTORY"),TEXT("1"));
	// decals always need WORLD_COORD usage in order to pass 2x2 matrix for normal transform
	// using the color interpolators used by WORLD_COORDS
	OutEnvironment.Definitions.Set(TEXT("WORLD_COORDS"),TEXT("1"));
}

FVertexFactoryShaderParameters* FGPUSkinDecalVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FGPUSkinDecalVertexFactoryShaderParameters() : NULL;
}

/** bind gpu skin decal vertex factory to its shader file and its shader parameters */
IMPLEMENT_VERTEX_FACTORY_TYPE( FGPUSkinDecalVertexFactory, "GpuSkinVertexFactory", TRUE, FALSE, TRUE, FALSE, TRUE, VER_PERBONEMOTIONBLUR, 0 );

/*-----------------------------------------------------------------------------
FGPUSkinMorphDecalVertexFactory
-----------------------------------------------------------------------------*/

/**
* Should we cache the material's shader type on this platform with this vertex factory? 
*/
UBOOL FGPUSkinMorphDecalVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return (Material->IsUsedWithDecals() || Material->IsDecalMaterial() || AllowDebugViewmodes(Platform) && Material->IsSpecialEngineMaterial()) &&
		Super::SharedShouldCache(Platform,Material,ShaderType);
}

/**
* Modify compile environment to enable the decal codepath
* @param OutEnvironment - shader compile environment to modify
*/
void FGPUSkinMorphDecalVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
{
	Super::ModifyCompilationEnvironment(Platform, OutEnvironment);
	OutEnvironment.Definitions.Set(TEXT("GPUSKIN_DECAL"),TEXT("1"));
	OutEnvironment.Definitions.Set(TEXT("DECAL_FACTORY"),TEXT("1"));
	// decals always need WORLD_COORD usage in order to pass 2x2 matrix for normal transform
	// using the color interpolators used by WORLD_COORDS
	OutEnvironment.Definitions.Set(TEXT("WORLD_COORDS"),TEXT("1"));
}

FVertexFactoryShaderParameters* FGPUSkinMorphDecalVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FGPUSkinDecalVertexFactoryShaderParameters() : NULL;
}

/** bind gpu skin decal vertex factory to its shader file and its shader parameters */
IMPLEMENT_VERTEX_FACTORY_TYPE( FGPUSkinMorphDecalVertexFactory, "GpuSkinVertexFactory", TRUE, FALSE, TRUE, FALSE, TRUE, VER_PERBONEMOTIONBLUR, 0 );
