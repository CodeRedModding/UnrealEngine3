/*=============================================================================
	D3DMeshShaders.cpp: Unreal Direct3D mesh shader implementation.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "D3DDrv.h"

//
//	GetD3DVertexFactoryType
//

ED3DVertexFactoryType GetD3DVertexFactoryType(FResourceType* Type)
{
	if(Type->IsA(FTerrainVertexFactory::StaticType))
		return VFT_Terrain;
	else if(Type->IsA(FFoliageVertexFactory::StaticType))
		return VFT_Foliage;
	else if (Type->IsA(FParticleSubUVVertexFactory::StaticType))
		return VFT_ParticleSubUV;
	else if(Type->IsA(FParticleVertexFactory::StaticType))
		return VFT_Particle;
	else if(Type->IsA(FLocalVertexFactory::StaticType))
		return VFT_Local;
	else if(Type)
	{
		appErrorf(TEXT("GetD3DVertexFactoryType: Unknown type %s"),Type->Name);
		return (ED3DVertexFactoryType)0;
	}
	else
		return VFT_Null;
}

//
//	FD3DLocalVertexFactoryParameters::Set
//

void FD3DLocalVertexFactoryParameters::Set(FVertexFactory* VertexFactory)
{
	check(VertexFactory->Type()->IsA(FLocalVertexFactory::StaticType));
	FLocalVertexFactory*	LocalVertexFactory = (FLocalVertexFactory*)VertexFactory;

	LocalToWorldParameter.SetMatrix4(LocalVertexFactory->LocalToWorld);
	WorldToLocalParameter.SetMatrix3(LocalVertexFactory->WorldToLocal);
}

//
//	FD3DTerrainVertexFactoryParameters::Set
//

void FD3DTerrainVertexFactoryParameters::Set(FVertexFactory* VertexFactory)
{
	check(VertexFactory->Type()->IsA(FTerrainVertexFactory::StaticType));

	FTerrainVertexFactory*	TerrainVertexFactory = (FTerrainVertexFactory*)VertexFactory;

	LocalToWorldParameter.SetMatrix4(TerrainVertexFactory->LocalToWorld);

	InvMaxTesselationLevelParameter.SetScalar(1.0f / TerrainVertexFactory->MaxTesselationLevel);
	InvTerrainSizeParameter.SetVector(1.0f / TerrainVertexFactory->TerrainSizeX,1.0f / TerrainVertexFactory->TerrainSizeY);
	InvSectionSizeParameter.SetVector(1.0f / TerrainVertexFactory->SectionSizeX,1.0f / TerrainVertexFactory->SectionSizeY);
	InvLightMapSizeParameter.SetVector(
		1.0f / (TerrainVertexFactory->SectionSizeX * TerrainVertexFactory->LightMapResolution + 1),
		1.0f / (TerrainVertexFactory->SectionSizeY * TerrainVertexFactory->LightMapResolution + 1)
		);
	ZScaleParameter.SetScalar(TerrainVertexFactory->TerrainHeightScale);
	SectionBaseParameter.SetVector(TerrainVertexFactory->SectionBaseX,TerrainVertexFactory->SectionBaseY);
}

//
//	FD3DFoliageVertexFactoryParameters::Set
//

void FD3DFoliageVertexFactoryParameters::Set(FVertexFactory* VertexFactory)
{
	check(VertexFactory->Type()->IsA(FFoliageVertexFactory::StaticType));

	FFoliageVertexFactory*	FoliageVertexFactory = (FFoliageVertexFactory*)VertexFactory;

	for(UINT SourceIndex = 0;SourceIndex < FoliageVertexFactory->WindPointSources.Num;SourceIndex++)
	{
		FWindPointSource&	Source = FoliageVertexFactory->WindPointSources(SourceIndex);
		WindPointSourcesParameter.SetVector(Source.SourceLocation,Source.Strength,SourceIndex * 2 + 0);
		WindPointSourcesParameter.SetVector(Source.Phase,Source.Frequency,Source.InvRadius,Source.InvDuration,SourceIndex * 2 + 1);
	}

	for(UINT SourceIndex = 0;SourceIndex < FoliageVertexFactory->WindDirectionalSources.Num;SourceIndex++)
	{
		FWindDirectionSource&	Source = FoliageVertexFactory->WindDirectionalSources(SourceIndex);
		FLOAT					Phase = Source.Phase * Source.Frequency;
		WindDirectionalSourcesParameter.SetVector(Source.Direction,Source.Strength,SourceIndex * 2 + 0);
		WindDirectionalSourcesParameter.SetVector((Phase - appFloor(Phase)) / Source.Frequency,Source.Frequency,0,0,SourceIndex * 2 + 1);
	}
	for(UINT SourceIndex = FoliageVertexFactory->WindDirectionalSources.Num;SourceIndex < 4;SourceIndex++)
	{
		WindDirectionalSourcesParameter.SetVector(0,0,0,0,SourceIndex * 2 + 0);
		WindDirectionalSourcesParameter.SetVector(0,0,0,0,SourceIndex * 2 + 1);
	}

	InvWindSpeedParameter.SetScalar(1.0f / 512.0f);
}

//
//	FD3DParticleVertexFactoryParameters::Set
//

void FD3DParticleVertexFactoryParameters::Set(FVertexFactory* VertexFactory)
{
	check(VertexFactory->Type()->IsA(FParticleVertexFactory::StaticType));
	check(GD3DSceneRenderer);
	
	FParticleVertexFactory* ParticleVertexFactory = (FParticleVertexFactory*) VertexFactory;

	FMatrix CameraToWorld	= GD3DSceneRenderer->View->ViewMatrix.Inverse();
	
	FPlane	CameraUp		= CameraToWorld.TransformNormal(FVector(0,1000,0)).SafeNormal();
	FPlane	CameraRight		= CameraToWorld.TransformNormal(FVector(1000,0,0)).SafeNormal();

	CameraUp.W				= 0.f;
	CameraRight.W			= 0.f;

	FPlane ScreenAlignment	= FPlane( ParticleVertexFactory->ScreenAlignment, ParticleVertexFactory->ScreenAlignment, ParticleVertexFactory->ScreenAlignment, ParticleVertexFactory->ScreenAlignment );

	LocalToWorldParameter.SetMatrix4(ParticleVertexFactory->LocalToWorld);
	CameraWorldPositionParameter.SetVector(CameraToWorld.GetOrigin(),1);
	CameraUpParameter.SetVector(CameraUp);
	CameraRightParameter.SetVector(CameraRight);
	ScreenAlignmentParameter.SetVector(ScreenAlignment);
}

//
//	FD3DParticleSubUVVertexFactoryParameters
// 
void FD3DParticleSubUVVertexFactoryParameters::Set(FVertexFactory* VertexFactory)
{
	FD3DParticleVertexFactoryParameters::Set(VertexFactory);
}

//
//	FD3DMeshShader::FD3DMeshShader
//

FD3DMeshShader::FD3DMeshShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction,INT InMaterialResourceIndex,FGuid InPersistentMaterialId,FResourceType* InVertexFactoryType):
	FD3DMaterialShader(InSet,InFunction,InMaterialResourceIndex,InPersistentMaterialId),
	VertexFactoryType(InVertexFactoryType),
	ViewProjectionMatrixParameter(this,TEXT("ViewProjectionMatrix")),
	CameraPositionParameter(this,TEXT("CameraPosition"))
{
	if(VertexFactoryType->IsA(FTerrainVertexFactory::StaticType))
		VertexFactoryParameters = new FD3DTerrainVertexFactoryParameters(this);
	else if(VertexFactoryType->IsA(FFoliageVertexFactory::StaticType))
		VertexFactoryParameters = new FD3DFoliageVertexFactoryParameters(this);
	else if (VertexFactoryType->IsA(FParticleSubUVVertexFactory::StaticType))
		VertexFactoryParameters = new FD3DParticleSubUVVertexFactoryParameters(this); 
	else if(VertexFactoryType->IsA(FParticleVertexFactory::StaticType))
		VertexFactoryParameters = new FD3DParticleVertexFactoryParameters(this);
	else if(VertexFactoryType->IsA(FLocalVertexFactory::StaticType))
		VertexFactoryParameters = new FD3DLocalVertexFactoryParameters(this);
	else if(VertexFactoryType)
		appErrorf(TEXT("Couldn't find parameter container type for vertex factory type %s"),VertexFactoryType->Name);
	else
		VertexFactoryParameters = NULL;
}

//
//	FD3DMeshShader::~FD3DMeshShader
//

FD3DMeshShader::~FD3DMeshShader()
{
	delete VertexFactoryParameters;
}

//
//	FD3DMeshShader::Cache
//

void FD3DMeshShader::Cache(const TCHAR* ShaderFilename,const TCHAR* VertexShaderFunction,const TCHAR* PixelShaderFunction,TArray<FD3DDefinition>& Definitions,FMaterial* Material)
{
	AddDefinition(Definitions,TEXT("USE_FP_BLENDING"),TEXT("%u"),(Function & SF_UseFPBlending) == SF_UseFPBlending);
	AddDefinition(Definitions,TEXT("OPAQUELAYER"),TEXT("%u"),(Function & SF_OpaqueLayer) == SF_OpaqueLayer);
	FD3DMaterialShader::Cache(ShaderFilename,VertexShaderFunction,PixelShaderFunction,Definitions,Material);
}

//
//	FD3DMeshShader::Set
//

void FD3DMeshShader::Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance)
{
	FD3DMaterialShader::Set(MaterialInstance,Primitive ? GD3DSceneRenderer->Scene->GetSceneTime() - Primitive->CreationTime() : GD3DSceneRenderer->Scene->GetSceneTime());

	if(VertexFactoryParameters)
		VertexFactoryParameters->Set(VertexFactory);

	ViewProjectionMatrixParameter.SetMatrix4(GD3DSceneRenderer->ViewProjectionMatrix);
	CameraPositionParameter.SetVector(GD3DSceneRenderer->CameraPosition,GD3DSceneRenderer->CameraPosition.W);
}

//
//	FD3DMeshShader::GetShaderSource
//

FString FD3DMeshShader::GetShaderSource(const TCHAR* Filename)
{
	if(!appStricmp(Filename,TEXT("VertexFactory.hlsl")))
	{
		if(VertexFactoryParameters)
			return FD3DShader::GetShaderSource(VertexFactoryParameters->GetShaderFilename());
		else
			return FD3DShader::GetShaderSource(TEXT("NullVertexFactory.hlsl"));
	}
	else
		return FD3DMaterialShader::GetShaderSource(Filename);
}

//
//	FD3DMeshShader::CompileShaders
//

TArray<FString> FD3DMeshShader::CompileShaders(const TCHAR* ShaderFilename,const TCHAR* VertexShaderFunction,const TCHAR* PixelShaderFunction,TArray<FD3DDefinition>& Definitions,INT& VertexShaderCode,INT& PixelShaderCode,TArray<FD3DUnmappedShaderParameter>& UnmappedParameters)
{
	TArray<FString>	Errors;

	if(PersistentMaterialId != FGuid(0,0,0,0))
	{
		FD3DPersistentMeshShaderId	PersistentCacheId(Function,PersistentMaterialId,VertexFactoryType);
		FD3DCompiledShader*			CachedCompiledShader = GD3DRenderDevice->CompiledMeshShaders.Find(PersistentCacheId);

		if(CachedCompiledShader)
		{
			VertexShaderCode = CachedCompiledShader->VertexShaderCode;
			PixelShaderCode = CachedCompiledShader->PixelShaderCode;
			UnmappedParameters = CachedCompiledShader->UnmappedParameters;
			return Errors;
		}
	}

	Errors = FD3DShader::CompileShaders(ShaderFilename,VertexShaderFunction,PixelShaderFunction,Definitions,VertexShaderCode,PixelShaderCode,UnmappedParameters);

	if(PersistentMaterialId != FGuid(0,0,0,0) && !Errors.Num())
	{
		FD3DPersistentMeshShaderId	PersistentCacheId(Function,PersistentMaterialId,VertexFactoryType);
		FD3DCompiledShader	CompiledShader;
		CompiledShader.VertexShaderCode = VertexShaderCode;
		CompiledShader.PixelShaderCode = PixelShaderCode;
		CompiledShader.UnmappedParameters = UnmappedParameters;
		GD3DRenderDevice->CompiledMeshShaders.Set(PersistentCacheId,CompiledShader);
	}

	return Errors;
}

//
//	FD3DMeshShader::Dump
//

void FD3DMeshShader::Dump(FOutputDevice& Ar)
{
	Ar.Logf(TEXT("%s: VertexFactoryType=%s, MaterialResourceIndex=%i, Function=%u, Cached=%u, NumRefs=%u"),*GetTypeDescription(),VertexFactoryType ? VertexFactoryType->Name : TEXT("None"),MaterialResourceIndex,(UINT)Function,Cached,NumRefs);
	Ar.Logf(TEXT("%s"),*UserShaderCode);
}

void FD3DDepthOnlyShader::Set()
{
	FD3DMaterialShader::Set(GEngine->DefaultMaterial->GetInstanceInterface(),0);
	ViewProjectionMatrixParameter.SetMatrix4(GD3DSceneRenderer->ViewProjectionMatrix);
	CameraPositionParameter.SetVector(GD3DSceneRenderer->CameraPosition,GD3DSceneRenderer->CameraPosition.W);
}

void FD3DDepthOnlyShader::SetInstance(UPrimitiveComponent* Primitive,FLocalVertexFactory* VertexFactory)
{
	if(VertexFactoryParameters)
		VertexFactoryParameters->Set(VertexFactory);
}

//
//	FD3DEmissiveShader::Set
//

void FD3DEmissiveShader::Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance)
{
	FD3DMeshShader::Set(Primitive,VertexFactory,MaterialInstance);
	check(GD3DSceneRenderer);
	SkyColorParameter.SetColor(GD3DSceneRenderer->SkyColor);
}

void FD3DUnlitTranslucencyShader::Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance)
{
	FD3DMeshShader::Set(Primitive,VertexFactory,MaterialInstance);
	check(GD3DSceneRenderer);
	for(INT LayerIndex = 0;LayerIndex < 4;LayerIndex++)
	{
		FogInScatteringParameter.SetColor(GD3DSceneRenderer->FogInScattering[LayerIndex],LayerIndex);
	}
	FogDistanceScaleParameter.SetVectors(GD3DSceneRenderer->FogDistanceScale,0,1);
	FogMinHeightParameter.SetVectors(GD3DSceneRenderer->FogMinHeight,0,1);
	FogMaxHeightParameter.SetVectors(GD3DSceneRenderer->FogMaxHeight,0,1);
}

//
//	FD3DTranslucentLayerShader::Set
//

void FD3DTranslucentLayerShader::Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,INT LayerIndex)
{
	FD3DMeshShader::Set(Primitive,VertexFactory,MaterialInstance);
	LayerIndexParameter.SetScalar(LayerIndex);
}

//
//	FD3DHitProxyShader::Set
//

void FD3DHitProxyShader::Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance)
{
	FD3DMeshShader::Set(Primitive,VertexFactory,MaterialInstance);
	FPlane IdColor = FColor(GD3DRenderInterface.CurrentHitProxyIndex).Plane();
	HitProxyIdParameter.SetVector(IdColor.X,IdColor.Y,IdColor.Z,IdColor.W);
}

//
//	FD3DWireframeShader::Set
//

void FD3DWireframeShader::Set(FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,const FLinearColor& WireColor)
{
	FD3DMeshShader::Set(NULL,VertexFactory,MaterialInstance);
	WireColorParameter.SetColor(WireColor);
}

//
//	FD3DWireframeHitProxyShader::Set
//

void FD3DWireframeHitProxyShader::Set(FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,const FLinearColor& WireColor)
{
	FD3DMeshShader::Set(NULL,VertexFactory,MaterialInstance);
	FPlane IdColor = FColor(GD3DRenderInterface.CurrentHitProxyIndex).Plane();
	HitProxyIdParameter.SetVector(IdColor.X,IdColor.Y,IdColor.Z,IdColor.W);
}

//
//	FD3DShadowDepthTestShader::GetSampleOffset
//

FVector2D FD3DShadowDepthTestShader::GetSampleOffset(UINT Index) const
{
	check(Index < 16);
	FVector2D	SampleOffsets[16] =
	{
		FVector2D(-1.5f,-1.5f), FVector2D(-0.5f,-1.5f), FVector2D(+0.5f,-1.5f), FVector2D(+1.5f,-1.5f),
		FVector2D(-1.5f,-0.5f), FVector2D(-0.5f,-0.5f), FVector2D(+0.5f,-0.5f), FVector2D(+1.5f,-0.5f),
		FVector2D(-1.5f,+0.5f), FVector2D(-0.5f,+0.5f), FVector2D(+0.5f,+0.5f), FVector2D(+1.5f,+0.5f),
		FVector2D(-1.5f,+1.5f), FVector2D(-0.5f,+1.5f), FVector2D(+0.5f,+1.5f), FVector2D(+1.5f,+1.5f)
	};
	return SampleOffsets[Index];
}

//
//	FD3DShadowDepthTestShader::Cache
//

void FD3DShadowDepthTestShader::Cache(FMaterial* Material,const TCHAR* PixelShaderFunction)
{
	if(GetNumSamples() & 3)
		appErrorf(TEXT("Number of depth test samples must be a multiple of 4! (%u)"),GetNumSamples());

	TArray<FD3DDefinition>	Definitions;
	AddDefinition(Definitions,TEXT("NUM_SAMPLE_CHUNKS"),TEXT("%u"),GetNumSamples() / 4);

	FD3DMeshShader::Cache(TEXT("ShadowDepthTest.hlsl"),TEXT("vertexShader"),PixelShaderFunction,Definitions,Material);
}

//
//	FD3DShadowDepthTestShader::Set
//

void FD3DShadowDepthTestShader::Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,const FMatrix& ShadowMatrix,FLOAT MaxShadowDepth,UBOOL DirectionalLight,UINT Resolution)
{
	FD3DMeshShader::Set(Primitive,VertexFactory,MaterialInstance);

	// Build a matrix which transforms from deprojected screen space to shadow space.

	FMatrix	ScreenToShadow = FMatrix(
								FPlane(1,0,0,0),
								FPlane(0,1,0,0),
								FPlane(0,0,(1.0f - Z_PRECISION),1),
								FPlane(0,0,-NEAR_CLIPPING_PLANE * (1.0f - Z_PRECISION),0)
								) *
							GD3DSceneRenderer->InvViewProjectionMatrix *
							ShadowMatrix;

	// For directional lights, imprecision in the above matrix math results in a matrix with extremely small values that a GPU with 24-bit FP chokes on.

	if(DirectionalLight)
	{
		ScreenToShadow.M[0][3] = 0.0f;
		ScreenToShadow.M[1][3] = 0.0f;
		ScreenToShadow.M[2][3] = 0.0f;
		ScreenToShadow.M[3][3] = 1.0f;
	}

	ShadowMatrixParameter.SetMatrix4(ScreenToShadow);

	ShadowDepthTextureParameter.SetTexture(GD3DRenderDevice->ShadowDepthRenderTarget->GetTexture(),D3DTEXF_POINT,1);

	ShadowTexCoordScaleParameter.SetVector(1.0f / (2.0f * GD3DRenderDevice->MaxShadowResolution / Resolution),1.0f / (-2.0f * GD3DRenderDevice->MaxShadowResolution / Resolution));
	InvMaxShadowDepthParameter.SetScalar(1.0f / MaxShadowDepth);

	UINT	NumChunks = GetNumSamples() / 4;
	FLOAT	BaseOffset = (1.0f /*Clamp border*/ + 0.5f /*Sample offset*/ + 0.5f * Resolution /*Origin*/) / GD3DRenderDevice->MaxShadowResolution,
			OffsetScale = 1.0f / GD3DRenderDevice->MaxShadowResolution,
			CosRotation = appCos(0.25f * (FLOAT)PI),
			SinRotation = appSin(0.25f * (FLOAT)PI);

	for(UINT ChunkIndex = 0;ChunkIndex < NumChunks;ChunkIndex++)
	{
		FVector2D	SampleOffsets[4] =
		{
			GetSampleOffset(ChunkIndex * 4 + 0),
			GetSampleOffset(ChunkIndex * 4 + 1),
			GetSampleOffset(ChunkIndex * 4 + 2),
			GetSampleOffset(ChunkIndex * 4 + 3),
		};

		SampleOffsetsParameter.SetVector(
			(SampleOffsets[0].X * +CosRotation + SampleOffsets[0].Y * +SinRotation) * OffsetScale + BaseOffset,
			(SampleOffsets[0].X * -SinRotation + SampleOffsets[0].Y * +CosRotation) * OffsetScale + BaseOffset,
			(SampleOffsets[1].X * -SinRotation + SampleOffsets[1].Y * +CosRotation) * OffsetScale + BaseOffset,
			(SampleOffsets[1].X * +CosRotation + SampleOffsets[1].Y * +SinRotation) * OffsetScale + BaseOffset,
			ChunkIndex * 2 + 0
			);
		SampleOffsetsParameter.SetVector(
			(SampleOffsets[2].X * +CosRotation + SampleOffsets[2].Y * +SinRotation) * OffsetScale + BaseOffset,
			(SampleOffsets[2].X * -SinRotation + SampleOffsets[2].Y * +CosRotation) * OffsetScale + BaseOffset,
			(SampleOffsets[3].X * -SinRotation + SampleOffsets[3].Y * +CosRotation) * OffsetScale + BaseOffset,
			(SampleOffsets[3].X * +CosRotation + SampleOffsets[3].Y * +SinRotation) * OffsetScale + BaseOffset,
			ChunkIndex * 2 + 1
			);
	}
}

//
//	FD3DNVShadowDepthTestShader::Set
//

void FD3DNVShadowDepthTestShader::Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,const FMatrix& ShadowMatrix,FLOAT MaxShadowDepth,UBOOL DirectionalLight,UINT Resolution)
{
	FD3DShadowDepthTestShader::Set(Primitive,VertexFactory,MaterialInstance,ShadowMatrix,MaxShadowDepth,DirectionalLight,Resolution);

	check(GD3DRenderDevice->HWShadowDepthBufferTexture);
	HWShadowDepthTextureParameter.SetTexture(GD3DRenderDevice->HWShadowDepthBufferTexture,D3DTEXF_LINEAR,1);
}

//
//	FD3DShadowDepthShader::Set
//

void FD3DShadowDepthShader::Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,const FMatrix& ShadowMatrix,const FMatrix& ProjectionMatrix,FLOAT MaxShadowDepth)
{
	FD3DMeshShader::Set(Primitive,VertexFactory,MaterialInstance);

	ProjectionMatrixParameter.SetMatrix4(ProjectionMatrix);
	ShadowMatrixParameter.SetMatrix4(ShadowMatrix);

	MaxShadowDepthParameter.SetScalar(MaxShadowDepth);
}

//
//	FD3DLightFunctionShader::Set
//

void FD3DLightFunctionShader::Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,const FMatrix& WorldToLight)
{
	FD3DMeshShader::Set(Primitive,VertexFactory,MaterialInstance);
	ScreenToLightParameter.SetMatrix4(
		FMatrix(
		FPlane(1,0,0,0),
		FPlane(0,1,0,0),
		FPlane(0,0,(1.0f - Z_PRECISION),1),
		FPlane(0,0,-NEAR_CLIPPING_PLANE * (1.0f - Z_PRECISION),0)
		) *
		GD3DSceneRenderer->InvViewProjectionMatrix *
		WorldToLight
		);
}

//
//	FD3DMeshShaderCache::GetCachedShader
//

FD3DMeshShader* FD3DMeshShaderCache::GetCachedShader(ED3DShaderFunction Function,FMaterial* Material,FResourceType* VertexFactoryType)
{
	FD3DMeshShaderId	ShaderId(Function,Material->ResourceIndex,VertexFactoryType);
	FD3DMeshShader*		CachedShader = GetCachedResource(ShaderId);
	if(!CachedShader)
	{
		if((Function & SF_FunctionMask) == SF_DepthOnly)
			CachedShader = new FD3DDepthOnlyShader(this,Function,Material,VertexFactoryType);
		else if((Function & SF_FunctionMask) == SF_Emissive)
			CachedShader = new FD3DEmissiveShader(this,Function,Material,VertexFactoryType);
		else if((Function & SF_FunctionMask) == SF_UnlitTranslucency)
			CachedShader = new FD3DUnlitTranslucencyShader(this,Function,Material,VertexFactoryType);
		else if((Function & SF_FunctionMask) == SF_Unlit)
			CachedShader = new FD3DUnlitShader(this,Function,Material,VertexFactoryType);
		else if((Function & SF_FunctionMask) == SF_VertexLighting)
			CachedShader = new FD3DVertexLightingShader(this,Function,Material,VertexFactoryType);
		else if((Function & SF_FunctionMask) == SF_TranslucentLayer)
			CachedShader = new FD3DTranslucentLayerShader(this,Material,VertexFactoryType);
		else if((Function & SF_FunctionMask) == SF_HitProxy)
			CachedShader = new FD3DHitProxyShader(this,Material,VertexFactoryType);
		else if((Function & SF_FunctionMask) == SF_Wireframe)
			CachedShader = new FD3DWireframeShader(this,Material,VertexFactoryType);
		else if((Function & SF_FunctionMask) == SF_WireframeHitProxy)
			CachedShader = new FD3DWireframeHitProxyShader(this,Material,VertexFactoryType);
		else if((Function & SF_FunctionMask) == SF_ShadowDepth)
			CachedShader = new FD3DShadowDepthShader(this,Material,VertexFactoryType);
		else if((Function & SF_FunctionMask) == SF_ShadowDepthTest)
			CachedShader = new FD3DShadowDepthTestShader(this,Material,VertexFactoryType);
		else if((Function & SF_FunctionMask) == SF_NVShadowDepthTest)
			CachedShader = new FD3DNVShadowDepthTestShader(this,Material,VertexFactoryType);
		else if((Function & SF_FunctionMask) == SF_LightFunction)
			CachedShader = new FD3DLightFunctionShader(this,Material,VertexFactoryType);
		else
			appErrorf(TEXT("Cannot cache mesh shader function %u"),(UINT)Function);
		CachedShader->Description = Material->DescribeResource();
		AddCachedResource(ShaderId,CachedShader);
	}
	return CachedShader;
}

//
//	FD3DMeshLightShader::FD3DMeshLightShader
//

FD3DMeshLightShader::FD3DMeshLightShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction,INT InMaterialResourceIndex,FGuid InPersistentMaterialId,FResourceType* InVertexFactoryType,ED3DStaticLightingType InStaticLightingType):
	FD3DMeshShader(InSet,InFunction,InMaterialResourceIndex,InPersistentMaterialId,InVertexFactoryType),
	StaticLightingType(InStaticLightingType),
	VisibilityTextureParameter(this,TEXT("VisibilityTexture"))
{}

//
//	FD3DMeshLightShader::Set
//

void FD3DMeshLightShader::Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,ULightComponent* Light,FD3DTexture2D* VisibilityTexture)
{
	FD3DMeshShader::Set(Primitive,VertexFactory,MaterialInstance);

	if(VisibilityTexture)
	{
		switch(StaticLightingType)
		{
			case SLT_TextureMask:
				VisibilityTextureParameter.SetTexture(VisibilityTexture->Texture,D3DTEXF_LINEAR,1);
				break;
		};
	}
}

//
//	FD3DMeshLightShader::Cache
//

void FD3DMeshLightShader::Cache(const TCHAR* ShaderFilename,const TCHAR* VertexShaderFunction,const TCHAR* PixelShaderFunction,TArray<FD3DDefinition>& Definitions,FMaterial* Material)
{
	AddDefinition(Definitions,TEXT("STATICLIGHTING_VERTEXMASK"),TEXT("%u"),StaticLightingType == SLT_VertexMask);
	AddDefinition(Definitions,TEXT("STATICLIGHTING_TEXTUREMASK"),TEXT("%u"),StaticLightingType == SLT_TextureMask);
	FD3DMeshShader::Cache(ShaderFilename,VertexShaderFunction,PixelShaderFunction,Definitions,Material);
}

//
//	FD3DMeshLightShader::CompileShaders
//

TArray<FString> FD3DMeshLightShader::CompileShaders(const TCHAR* ShaderFilename,const TCHAR* VertexShaderFunction,const TCHAR* PixelShaderFunction,TArray<FD3DDefinition>& Definitions,INT& VertexShaderCode,INT& PixelShaderCode,TArray<FD3DUnmappedShaderParameter>& UnmappedParameters)
{
	TArray<FString>	Errors;

	if(PersistentMaterialId != FGuid(0,0,0,0))
	{
		FD3DPersistentMeshLightShaderId	PersistentCacheId(PersistentMaterialId,Function,VertexFactoryType,StaticLightingType);
		FD3DCompiledShader*				CachedCompiledShader = GD3DRenderDevice->CompiledMeshLightShaders.Find(PersistentCacheId);
		TArray<FString>					Errors;

		if(CachedCompiledShader)
		{
			VertexShaderCode = CachedCompiledShader->VertexShaderCode;
			PixelShaderCode = CachedCompiledShader->PixelShaderCode;
			UnmappedParameters = CachedCompiledShader->UnmappedParameters;
			return Errors;
		}
	}

	Errors = FD3DShader::CompileShaders(ShaderFilename,VertexShaderFunction,PixelShaderFunction,Definitions,VertexShaderCode,PixelShaderCode,UnmappedParameters);

	if(PersistentMaterialId != FGuid(0,0,0,0) && !Errors.Num())
	{
		FD3DPersistentMeshLightShaderId	PersistentCacheId(PersistentMaterialId,Function,VertexFactoryType,StaticLightingType);
		FD3DCompiledShader				CompiledShader;
		CompiledShader.VertexShaderCode = VertexShaderCode;
		CompiledShader.PixelShaderCode = PixelShaderCode;
		CompiledShader.UnmappedParameters = UnmappedParameters;
		GD3DRenderDevice->CompiledMeshLightShaders.Set(PersistentCacheId,CompiledShader);
	}

	return Errors;
}

//
//	FD3DMeshLightShader::Dump
//

void FD3DMeshLightShader::Dump(FOutputDevice& Ar)
{
	Ar.Logf(TEXT("%s: StaticLightingType=%u, VertexFactoryType=%s, MaterialResourceIndex=%i, Function=%u, Cached=%u, NumRefs=%u"),*GetTypeDescription(),(UINT)StaticLightingType,VertexFactoryType ? VertexFactoryType->Name : TEXT("None"),MaterialResourceIndex,(UINT)Function,Cached,NumRefs);
	//Ar.Logf(TEXT("%s"),*UserShaderCode);
}

//
//	FD3DPointLightShader::Set
//

void FD3DPointLightShader::Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,ULightComponent* Light,FD3DTexture2D* VisibilityTexture)
{
	UPointLightComponent*	PointLight = Cast<UPointLightComponent>(Light);

	FD3DMeshLightShader::Set(Primitive,VertexFactory,MaterialInstance,Light,VisibilityTexture);

	LightPositionParameter.SetVector(Light->GetOrigin(),1.0f / PointLight->Radius);
	LightColorParameter.SetColor((FLinearColor)Light->Color * Light->Brightness);
}

//
//	FD3DDirectionalLightShader::Set
//

void FD3DDirectionalLightShader::Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,ULightComponent* Light,FD3DTexture2D* VisibilityTexture)
{
	FD3DMeshLightShader::Set(Primitive,VertexFactory,MaterialInstance,Light,VisibilityTexture);

	LightDirectionParameter.SetVector(-Light->GetDirection(),0);
	LightColorParameter.SetColor((FLinearColor)Light->Color * Light->Brightness);
}

//
//	FD3DSpotLightShader::Set
//

void FD3DSpotLightShader::Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,ULightComponent* Light,FD3DTexture2D* VisibilityTexture)
{
	USpotLightComponent*	SpotLight = Cast<USpotLightComponent>(Light);

	FD3DMeshLightShader::Set(Primitive,VertexFactory,MaterialInstance,Light,VisibilityTexture);

	LightPositionParameter.SetVector(Light->GetOrigin(),1.0f / SpotLight->Radius);
	LightColorParameter.SetColor((FLinearColor)Light->Color * Light->Brightness);

	FLOAT	InnerConeAngle = Clamp(SpotLight->InnerConeAngle,0.0f,89.0f) * (FLOAT)PI / 180.0f,
			OuterConeAngle = Clamp(SpotLight->OuterConeAngle * (FLOAT)PI / 180.0f,InnerConeAngle + 0.001f,89.0f * (FLOAT)PI / 180.0f + 0.001f),
			OuterCone = appCos(OuterConeAngle),
			InnerCone = appCos(InnerConeAngle);

	SpotAnglesParameter.SetVector(OuterCone,1.0f / (InnerCone - OuterCone));
	SpotDirectionParameter.SetVector(Light->GetDirection(),0);
}

//
//	FD3DMeshLightShaderCache::GetCachedShader
//

FD3DMeshLightShader* FD3DMeshLightShaderCache::GetCachedShader(ED3DShaderFunction Function,FMaterial* Material,FResourceType* VertexFactoryType,ED3DStaticLightingType StaticLightingType)
{
	FD3DMeshLightShaderId	ShaderId(Function,Material->ResourceIndex,VertexFactoryType,StaticLightingType);
	FD3DMeshLightShader*	CachedShader = GetCachedResource(ShaderId);
	if(!CachedShader)
	{
		if((Function & SF_FunctionMask) == SF_PointLight)
			CachedShader = new FD3DPointLightShader(this,Function,Material,VertexFactoryType,StaticLightingType);
		else if((Function & SF_FunctionMask) == SF_SpotLight)
			CachedShader = new FD3DSpotLightShader(this,Function,Material,VertexFactoryType,StaticLightingType);
		else if((Function & SF_FunctionMask) == SF_DirectionalLight)
			CachedShader = new FD3DDirectionalLightShader(this,Function,Material,VertexFactoryType,StaticLightingType);
		else
			appErrorf(TEXT("Cannot cache mesh light shader function %u"),(UINT)Function);
		AddCachedResource(ShaderId,CachedShader);
	}
	return CachedShader;
}
