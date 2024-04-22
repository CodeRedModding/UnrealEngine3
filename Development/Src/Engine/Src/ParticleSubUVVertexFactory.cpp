/*=============================================================================
	ParticleSubUVVertexFactory.cpp: Particle vertex factory implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"

/**
 * The particle system sub UV vertex declaration resource type.
 */
class FParticleSystemSubUVVertexDeclaration : public FRenderResource
{
public:

	FVertexDeclarationRHIRef VertexDeclarationRHI;

	// Destructor.
	virtual ~FParticleSystemSubUVVertexDeclaration() {}

	virtual void FillDeclElements(FVertexDeclarationElementList& Elements, INT&	Offset)
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
		/** The stream to read the texture coordinates from.	*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Float4,VEU_TextureCoordinate,0));
		Offset += sizeof(FLOAT) * 4;
		/** The stream to read the interpolation value from.	*/
		/** SHARED WITH the size scaling information.			*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Float4,VEU_TextureCoordinate,2));
		Offset += sizeof(FLOAT) * 4;
	}


	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		INT	Offset = 0;

		FillDeclElements(Elements, Offset);

		// Create the vertex declaration for rendering the factory normally.
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements, TEXT("SubUVSpriteParticle"));
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/** The simple element vertex declaration. */
static TGlobalResource<FParticleSystemSubUVVertexDeclaration> GParticleSystemSubUVVertexDeclaration;

/**
 * The particle system sub UV dynamic parameter vertex declaration resource type.
 */
class FParticleSystemSubUVDynamicParamVertexDeclaration : public FParticleSystemSubUVVertexDeclaration
{
public:
	// Destructor.
	virtual ~FParticleSystemSubUVDynamicParamVertexDeclaration() {}

	virtual void FillDeclElements(FVertexDeclarationElementList& Elements, INT&	Offset)
	{
		FParticleSystemSubUVVertexDeclaration::FillDeclElements(Elements, Offset);
		/** The stream to read the dynamic parameter value from. */
		Elements.AddItem(FVertexElement(0,Offset,VET_Float4,VEU_TextureCoordinate,3));
		Offset += sizeof(FLOAT) * 4;
	}
};

/** The simple element vertex declaration. */
static TGlobalResource<FParticleSystemSubUVDynamicParamVertexDeclaration> GParticleSystemSubUVDynamicParameterVertexDeclaration;

UBOOL FParticleSubUVVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return ((Material->IsUsedWithParticleSubUV() && !Material->GetUsesDynamicParameter()) || Material->IsSpecialEngineMaterial()) && !Material->IsUsedWithDecals();
}

/**
 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
 */
void FParticleSubUVVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
{
	FParticleVertexFactory::ModifyCompilationEnvironment(Platform, OutEnvironment);
	OutEnvironment.Definitions.Set(TEXT("SUBUV_PARTICLES"),TEXT("1"));
}

/**
 *	Initialize the Render Hardare Interface for this vertex factory
 */
void FParticleSubUVVertexFactory::InitRHI()
{
	SetDeclaration(GParticleSystemSubUVVertexDeclaration.VertexDeclarationRHI);
}

FVertexFactoryShaderParameters* FParticleSubUVVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FParticleVertexFactoryShaderParameters() : NULL;
}

/**
 *	
 */
/**
 * Should we cache the material's shadertype on this platform with this vertex factory? 
 */
UBOOL FParticleSubUVDynamicParameterVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return ((Material->IsUsedWithParticleSubUV() && Material->GetUsesDynamicParameter()) || Material->IsSpecialEngineMaterial()) && !Material->IsUsedWithDecals();
}

/**
 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
 */
void FParticleSubUVDynamicParameterVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
{
	FParticleSubUVVertexFactory::ModifyCompilationEnvironment(Platform, OutEnvironment);
	OutEnvironment.Definitions.Set(TEXT("USE_DYNAMIC_PARAMETERS"),TEXT("1"));
}

// FRenderResource interface.
void FParticleSubUVDynamicParameterVertexFactory::InitRHI()
{
	SetDeclaration(GParticleSystemSubUVDynamicParameterVertexDeclaration.VertexDeclarationRHI);
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FParticleSubUVVertexFactory,"ParticleSpriteVertexFactory",TRUE,FALSE,TRUE,FALSE,TRUE,VER_SPRITE_SUBUV_VFETCH_SUPPORT,0);
IMPLEMENT_VERTEX_FACTORY_TYPE(FParticleSubUVDynamicParameterVertexFactory,"ParticleSpriteVertexFactory",TRUE,FALSE,TRUE,FALSE,TRUE, VER_SPRITE_SUBUV_VFETCH_SUPPORT,0);
