/*=============================================================================
	ParticleVertexFactory.cpp: Particle vertex factory implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "EngineParticleClasses.h"

/**
 * The particle system vertex declaration resource type.
 */
class FParticleSystemVertexDeclaration : public FRenderResource
{
public:

	FVertexDeclarationRHIRef VertexDeclarationRHI;

	// Destructor.
	virtual ~FParticleSystemVertexDeclaration() {}

	virtual void FillDeclElements(FVertexDeclarationElementList& Elements, INT& Offset)
	{
		/** The stream to read the vertex position from.		*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Float3,VEU_Position,0));
		Offset += sizeof(FLOAT) * 3;
		/** The stream to read the vertex old position from.	*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Float3,VEU_Normal,0));
		Offset += sizeof(FLOAT) * 3;
		/** The stream to read the vertex size from.			*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Float3,VEU_Tangent,0));
		Offset += sizeof(FLOAT) * 3;
		/** The stream to read the rotation & sizer index from.	*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Float2,VEU_BlendWeight,0));
		Offset += sizeof(FLOAT) * 2;
		/** The stream to read the color from.					*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Float4,VEU_TextureCoordinate,1));
		Offset += sizeof(FLOAT) * 4;
#if !PARTICLES_USE_INDEXED_SPRITES
		/** The stream to read the texture coordinates from.	*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Float2,VEU_TextureCoordinate,0));
		Offset += sizeof(FLOAT) * 2;
#endif
	}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		INT	Offset = 0;

		FillDeclElements(Elements, Offset);

		// Create the vertex declaration for rendering the factory normally.
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements, TEXT("SpriteParticle"));
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/** The simple element vertex declaration. */
static TGlobalResource<FParticleSystemVertexDeclaration> GParticleSystemVertexDeclaration;

/**
 * The particle system dynamic parameter vertex declaration resource type.
 */
class FParticleSystemDynamicParameterVertexDeclaration : public FParticleSystemVertexDeclaration
{
public:
	// Destructor.
	virtual ~FParticleSystemDynamicParameterVertexDeclaration() {}

	virtual void FillDeclElements(FVertexDeclarationElementList& Elements, INT& Offset)
	{
		FParticleSystemVertexDeclaration::FillDeclElements(Elements, Offset);
		/** The stream to read the dynamic parameter from.	*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Float4,VEU_TextureCoordinate,3));
		Offset += sizeof(FLOAT) * 4;
	}
};

/** The simple element vertex declaration. */
static TGlobalResource<FParticleSystemDynamicParameterVertexDeclaration> GParticleSystemDynamicParameterVertexDeclaration;

/**
 * The particle system point sprite vertex declaration resource type.
 */
class FParticleSystemPointSpriteVertexDeclaration : public FParticleSystemVertexDeclaration
{
public:
	// Destructor.
	virtual ~FParticleSystemPointSpriteVertexDeclaration() {}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		INT	Offset = 0;

		FillDeclElements(Elements, Offset);

		// Create the vertex declaration for rendering the factory normally.
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements, TEXT("PointSpriteParticle"));
	}

	virtual void FillDeclElements(FVertexDeclarationElementList& Elements, INT& Offset)
	{
		/** The stream to read the vertex position from.		*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Float3,VEU_Position,0));
		Offset += sizeof(FLOAT) * 3;

		/** The stream to read the size from.				*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Float1,VEU_Tangent,0));
		Offset += sizeof(FLOAT) * 1;

		/** The stream to read the color.				*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Color,VEU_Color,0));
		Offset += sizeof(DWORD) * 1;
	}
};

/** The simple element vertex declaration. */
static TGlobalResource<FParticleSystemPointSpriteVertexDeclaration> GParticleSystemPointSpriteVertexDeclaration;


UBOOL FParticleVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return ((Material->IsUsedWithParticleSprites() && !Material->GetUsesDynamicParameter()) || Material->IsSpecialEngineMaterial()) && !Material->IsUsedWithDecals();
}

/**
 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
 */
void FParticleVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
{
	FVertexFactory::ModifyCompilationEnvironment(Platform, OutEnvironment);
	OutEnvironment.Definitions.Set(TEXT("PARTICLES_ALLOW_AXIS_ROTATION"),TEXT("1"));
	// There are only 2 slots required for the axis rotation vectors.
	OutEnvironment.Definitions.Set(TEXT("NUM_AXIS_ROTATION_VECTORS"),TEXT("2"));
	// Occlusion percentage - currently always enabled for particle systems.
	OutEnvironment.Definitions.Set(TEXT("USE_OCCLUSION_PERCENTAGE"),TEXT("1"));
#if ALLOW_INDEXED_PARTICLE_SPRITES
	// On Xbox, use indexed particle vertices (vfetch)
	if (Platform == SP_XBOXD3D)
	{
		OutEnvironment.Definitions.Set(TEXT("USE_PARTICLE_VERTEX_INDEX"),TEXT("1"));
	}
	else
#endif
	{
		OutEnvironment.Definitions.Set(TEXT("USE_PARTICLE_VERTEX_INDEX"),TEXT("0"));
	}
}

/**
 *	Initialize the Render Hardware Interface for this vertex factory
 */
void FParticleVertexFactory::InitRHI()
{
	SetDeclaration(GParticleSystemVertexDeclaration.VertexDeclarationRHI);
}

ESpriteScreenAlignment FParticleVertexFactory::StaticGetSpriteScreenAlignment(BYTE InLockAxisFlag, BYTE InScreenAlignment)
{
	if (InLockAxisFlag != EPAL_NONE)
	{
		// LockedAxis flag is needed only for rotation cases.
		// All other locked axis cases work by faking the camera up/right 
		// vectors passed to the shader.
		if ((InLockAxisFlag >= EPAL_ROTATE_X) && (InLockAxisFlag <= EPAL_ROTATE_Z))
		{
			return SSA_LockedAxis;
		}
	}
	else if (InScreenAlignment == PSA_Velocity)
	{
		return SSA_Velocity;
	}

	return SSA_CameraFacing;
}


/** Return the sprite screen alignment */
ESpriteScreenAlignment FParticleVertexFactory::GetSpriteScreenAlignment() const
{
	return StaticGetSpriteScreenAlignment(LockAxisFlag, ScreenAlignment);
}

FVertexFactoryShaderParameters* FParticleVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FParticleVertexFactoryShaderParameters() : NULL;
}

/**
 * Should we cache the material's shadertype on this platform with this vertex factory? 
 */
UBOOL FParticleDynamicParameterVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return ((Material->IsUsedWithParticleSprites() && Material->GetUsesDynamicParameter()) || Material->IsSpecialEngineMaterial()) && !Material->IsUsedWithDecals();
}

/**
 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
 */
void FParticleDynamicParameterVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
{
	FParticleVertexFactory::ModifyCompilationEnvironment(Platform, OutEnvironment);
	OutEnvironment.Definitions.Set(TEXT("USE_DYNAMIC_PARAMETERS"),TEXT("1"));
}

/**
 *	
 */
void FParticleDynamicParameterVertexFactory::InitRHI()
{
	SetDeclaration(GParticleSystemDynamicParameterVertexDeclaration.VertexDeclarationRHI);
}

/**
 *	Initialize the Render Hardware Interface for this vertex factory
 */
void FParticlePointSpriteVertexFactory::InitRHI()
{
	SetDeclaration(GParticleSystemPointSpriteVertexDeclaration.VertexDeclarationRHI);
}

/**
 * Should we cache the material's shadertype on this platform with this vertex factory? 
 */
UBOOL FParticlePointSpriteVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return (
		(Platform != SP_XBOXD3D) && 
		FParticleVertexFactory::ShouldCache(Platform, Material, ShaderType)
		);
}

//
void FParticleVertexFactoryShaderParameters::Set(FShader* VertexShader,const FVertexFactory* VertexFactory,const FSceneView& View) const
{
	FParticleVertexFactory* ParticleVF = (FParticleVertexFactory*)VertexFactory;

	FVector4 CameraRight, CameraUp;
	FVector UpRightScalarParam(0.0f);
	FLOAT SourceIndex = 0.0f;

	const FVector4 TranslatedViewOrigin = View.ViewOrigin + FVector4(View.PreViewTranslation,0);
	SetVertexShaderValue(VertexShader->GetVertexShader(),CameraWorldPositionParameter,FVector4(TranslatedViewOrigin, 0.0f));

	BYTE LockAxisFlag = ParticleVF->GetLockAxisFlag();
	if (LockAxisFlag == EPAL_NONE)
	{
		CameraUp	= -View.InvViewProjectionMatrix.TransformNormal(FVector(1.0f,0.0f,0.0f)).SafeNormal();
		CameraRight	= -View.InvViewProjectionMatrix.TransformNormal(FVector(0.0f,1.0f,0.0f)).SafeNormal();

		if (ParticleVF->GetScreenAlignment() == PSA_Velocity)
		{
			UpRightScalarParam.Y = 1.0f;
		}
		else
		{
			UpRightScalarParam.X = 1.0f;
		}
	}
	else
	if ((LockAxisFlag >= EPAL_ROTATE_X) && (LockAxisFlag <= EPAL_ROTATE_Z))
	{
		FVector4 RotationVectors[2];
		switch (LockAxisFlag)
		{
		case EPAL_ROTATE_X:
			RotationVectors[0] = FVector4(ParticleVF->GetLockAxisUp(), 1.0f);
			RotationVectors[1] = FVector4(ParticleVF->GetLockAxisRight(), 0.0f);
			break;
		case EPAL_ROTATE_Y:
			RotationVectors[0] = FVector4(ParticleVF->GetLockAxisUp(), 1.0f);
			RotationVectors[1] = FVector4(ParticleVF->GetLockAxisRight(), 0.0f);
			break;
		case EPAL_ROTATE_Z:
			RotationVectors[0] = FVector4(ParticleVF->GetLockAxisUp(), 0.0f);
			RotationVectors[1] = FVector4(ParticleVF->GetLockAxisRight(),-1.0f);
			SourceIndex = 1.0f;
			break;
		}
		SetVertexShaderValue(VertexShader->GetVertexShader(), AxisRotationVectorsArrayParameter, RotationVectors, 0);
		// The mobile renderers will send the shader constants for both rotation vectors down with a single call to
		// SetVertexShaderValue, so we don't want to call the function again on ES2 as it will just stomp over
		// the constant that we just set.
#if WITH_MOBILE_RHI
		if( !GUsingMobileRHI )
#endif
		{
			SetVertexShaderValue(VertexShader->GetVertexShader(), AxisRotationVectorsArrayParameter, RotationVectors, 1);
		}
		UpRightScalarParam.Z = 1.0f;
	}
	else
	{
		CameraUp	= FVector4(ParticleVF->GetLockAxisUp(), 0.0f);
		CameraRight	= FVector4(ParticleVF->GetLockAxisRight(), 0.0f);
		UpRightScalarParam.X = 1.0f;
	}

	SetVertexShaderValue(VertexShader->GetVertexShader(), AxisRotationVectorSourceIndexParameter, SourceIndex);
	SetVertexShaderValue(VertexShader->GetVertexShader(),CameraRightParameter,CameraRight);
	SetVertexShaderValue(VertexShader->GetVertexShader(),CameraUpParameter,CameraUp);
	SetVertexShaderValue(VertexShader->GetVertexShader(),ScreenAlignmentParameter,FVector4((FLOAT)ParticleVF->GetScreenAlignment(),0.0f,0.0f,0.0f));
	SetVertexShaderValue(VertexShader->GetVertexShader(), ParticleUpRightResultScalarsParameter, UpRightScalarParam);

	const EEmitterNormalsMode NormalsMode = (EEmitterNormalsMode)ParticleVF->EmitterNormalsMode;
	//@todo - use static branching
	SetVertexShaderValue(VertexShader->GetVertexShader(), NormalsTypeParameter, (FLOAT)ParticleVF->EmitterNormalsMode);
	if (NormalsMode == ENM_Spherical || NormalsMode == ENM_Cylindrical)
	{
		SetVertexShaderValue(VertexShader->GetVertexShader(), NormalsSphereCenterParameter, FVector4(ParticleVF->NormalsSphereCenter + View.PreViewTranslation, 0.0f));
		if (NormalsMode == ENM_Cylindrical)
		{
			SetVertexShaderValue(VertexShader->GetVertexShader(), NormalsCylinderUnitDirectionParameter, FVector4(ParticleVF->NormalsCylinderDirection.SafeNormal(), 0.0f));
		}
	}

	if (GRHIShaderPlatform == SP_PS3 || GRHIShaderPlatform == SP_PCOGL)
	{
		FVector4 CornerUVs[4] = 
			{ 
				FVector4(0.0f, 0.0f, 0.0f, 0.0f), 
				FVector4(0.0f, 1.0f, 0.0f, 0.0f), 
				FVector4(1.0f, 1.0f, 0.0f, 0.0f), 
				FVector4(1.0f, 0.0f, 0.0f, 0.0f)
			};
		SetVertexShaderValue(VertexShader->GetVertexShader(), CornerUVsParameter, CornerUVs);
	}
}

void FParticleVertexFactoryShaderParameters::SetMesh(FShader* VertexShader, const FMeshBatch& Mesh, INT BatchElementIndex,const FSceneView& View) const
{
	SetVertexShaderValue(
		VertexShader->GetVertexShader(),
		LocalToWorldParameter,
		Mesh.Elements(BatchElementIndex).LocalToWorld.ConcatTranslation(View.PreViewTranslation)
		);
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FParticleVertexFactory,"ParticleSpriteVertexFactory",TRUE,FALSE,TRUE,FALSE,TRUE, VER_SPRITE_SUBUV_VFETCH_SUPPORT,0);
IMPLEMENT_VERTEX_FACTORY_TYPE(FParticleDynamicParameterVertexFactory,"ParticleSpriteVertexFactory",TRUE,FALSE,TRUE,FALSE,TRUE, VER_SPRITE_SUBUV_VFETCH_SUPPORT,0);
IMPLEMENT_VERTEX_FACTORY_TYPE(FParticlePointSpriteVertexFactory,"ParticleSpriteVertexFactory",TRUE,FALSE,TRUE,FALSE,TRUE, VER_SPRITE_SUBUV_VFETCH_SUPPORT,0);
