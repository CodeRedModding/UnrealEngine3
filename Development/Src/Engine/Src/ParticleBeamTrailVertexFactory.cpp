/*=============================================================================
	ParticleBeamTrailVertexFactory.cpp: Particle vertex factory implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "EngineParticleClasses.h"

///////////////////////////////////////////////////////////////////////////////
/**
 * The particle system beam trail vertex declaration resource type.
 */
class FParticleBeamTrailVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	// Destructor.
	virtual ~FParticleBeamTrailVertexDeclaration() {}

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
		/** The stream to read the rotation from.				*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Float2,VEU_BlendWeight,0));
		Offset += sizeof(FLOAT) * 2;
		/** The stream to read the color from.					*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Float4,VEU_TextureCoordinate,1));
		Offset += sizeof(FLOAT) * 4;
		/** The stream to read the texture coordinates from.	*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Float4,VEU_TextureCoordinate,0));
		Offset += sizeof(FLOAT) * 4;
	}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		INT	Offset = 0;
		FillDeclElements(Elements, Offset);

		// Create the vertex declaration for rendering the factory normally.
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements, TEXT("BeamTrailParticle"));
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/** The simple element vertex declaration. */
static TGlobalResource<FParticleBeamTrailVertexDeclaration> GParticleBeamTrailVertexDeclaration;

///////////////////////////////////////////////////////////////////////////////

/**
 * The particle system dynamic parameter vertex declaration resource type.
 */
class FParticleBeamTrailDynamicParameterVertexDeclaration : public FParticleBeamTrailVertexDeclaration
{
public:
	// Destructor.
	virtual ~FParticleBeamTrailDynamicParameterVertexDeclaration() {}

	virtual void FillDeclElements(FVertexDeclarationElementList& Elements, INT& Offset)
	{
		FParticleBeamTrailVertexDeclaration::FillDeclElements(Elements, Offset);
		/** The stream to read the dynamic parameter from.	*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Float4,VEU_TextureCoordinate,2));
		Offset += sizeof(FLOAT) * 4;
	}
};

/** The simple element vertex declaration. */
static TGlobalResource<FParticleBeamTrailDynamicParameterVertexDeclaration> GParticleBeamTrailVertexDynamicParameterDeclaration;

///////////////////////////////////////////////////////////////////////////////

UBOOL FParticleBeamTrailVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return (Material->IsUsedWithBeamTrails() || Material->IsSpecialEngineMaterial()) && !Material->IsUsedWithDecals();
}

/**
 *	Initialize the Render Hardare Interface for this vertex factory
 */
void FParticleBeamTrailVertexFactory::InitRHI()
{
	SetDeclaration(GParticleBeamTrailVertexDeclaration.VertexDeclarationRHI);
}

FVertexFactoryShaderParameters* FParticleBeamTrailVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FParticleBeamTrailVertexFactoryShaderParameters() : NULL;
}

///////////////////////////////////////////////////////////////////////////////

/**
 * Should we cache the material's shadertype on this platform with this vertex factory? 
 */
UBOOL FParticleBeamTrailDynamicParameterVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return ((Material->IsUsedWithBeamTrails() && Material->GetUsesDynamicParameter()) || Material->IsSpecialEngineMaterial()) && !Material->IsUsedWithDecals();
}

/**
 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
 */
void FParticleBeamTrailDynamicParameterVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
{
	FParticleVertexFactory::ModifyCompilationEnvironment(Platform, OutEnvironment);
	OutEnvironment.Definitions.Set(TEXT("USE_DYNAMIC_PARAMETERS"),TEXT("1"));
}

// FRenderResource interface.
void FParticleBeamTrailDynamicParameterVertexFactory::InitRHI()
{
	SetDeclaration(GParticleBeamTrailVertexDynamicParameterDeclaration.VertexDeclarationRHI);
}

///////////////////////////////////////////////////////////////////////////////

void FParticleBeamTrailVertexFactoryShaderParameters::Set(FShader* VertexShader,const FVertexFactory* VertexFactory,const FSceneView& View) const
{
	FParticleBeamTrailVertexFactory* ParticleVF = (FParticleBeamTrailVertexFactory*)VertexFactory;

	FVector4	CameraRight, CameraUp;

	const FVector4 TranslatedViewOrigin = View.ViewOrigin + FVector4(View.PreViewTranslation,0);
	SetVertexShaderValue(VertexShader->GetVertexShader(),CameraWorldPositionParameter,FVector4(TranslatedViewOrigin, 0.0f));

	if (CameraRightParameter.IsBound() || CameraUpParameter.IsBound())
	{
		FVector RightUpScalar(0.0f,0.0f,0.0f);

		BYTE LockAxisFlag = ParticleVF->GetLockAxisFlag();
		if (LockAxisFlag == EPAL_NONE)
		{
			CameraUp	= -View.InvViewProjectionMatrix.TransformNormal(FVector(1.0f,0.0f,0.0f)).SafeNormal();
			CameraRight	= -View.InvViewProjectionMatrix.TransformNormal(FVector(0.0f,1.0f,0.0f)).SafeNormal();
		}
		else
		{
			CameraUp	= FVector4(ParticleVF->GetLockAxisUp(), 0.0f);
			CameraRight	= FVector4(ParticleVF->GetLockAxisRight(), 0.0f);
#if 0
			if ((LockAxisFlag >= EPAL_ROTATE_X) && (LockAxisFlag <= EPAL_ROTATE_Z))
			{
				warnf(TEXT("LockAxisFlag ROTATE_* not support for beams/trails"));
			}
#endif
		}
		SetVertexShaderValue(VertexShader->GetVertexShader(),CameraRightParameter,CameraRight);
		SetVertexShaderValue(VertexShader->GetVertexShader(),CameraUpParameter,CameraUp);
	}

	SetVertexShaderValue(VertexShader->GetVertexShader(),ScreenAlignmentParameter,FVector4((FLOAT)ParticleVF->GetScreenAlignment(),0.0f,0.0f,0.0f));
}

void FParticleBeamTrailVertexFactoryShaderParameters::SetMesh(FShader* VertexShader, const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View) const
{
	SetVertexShaderValue(VertexShader->GetVertexShader(),LocalToWorldParameter, Mesh.Elements(BatchElementIndex).LocalToWorld.ConcatTranslation(View.PreViewTranslation));
}

///////////////////////////////////////////////////////////////////////////////

IMPLEMENT_VERTEX_FACTORY_TYPE(FParticleBeamTrailVertexFactory,"ParticleBeamTrailVertexFactory",TRUE,FALSE,TRUE,FALSE,TRUE,VER_BEAM_TRAIL_DYNAMIC_PARAMETER,0);
IMPLEMENT_VERTEX_FACTORY_TYPE(FParticleBeamTrailDynamicParameterVertexFactory,"ParticleBeamTrailVertexFactory",TRUE,FALSE,TRUE,FALSE,TRUE, VER_BEAM_TRAIL_DYNAMIC_PARAMETER,0);
