/**
 *	LensFlareVertexFactory.cpp: Lens flare vertex factory implementation.
 *	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "EnginePrivate.h"
#include "LensFlare.h"

/**
 * The lens flare vertex declaration resource type.
 */
class FLensFlareVertexDeclaration : public FRenderResource
{
public:

	FVertexDeclarationRHIRef VertexDeclarationRHI;

	// Destructor.
	virtual ~FLensFlareVertexDeclaration() {}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;

		INT Offset = 0;
		/** The stream to read the vertex position from.		*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Float4,VEU_Position,0));
		Offset += sizeof(FLOAT) * 4;
		/** The stream to read the vertex size from.			*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Float4,VEU_Tangent,0));
		Offset += sizeof(FLOAT) * 4;
		/** The stream to read the radial_dist/source_ratio/ray distance/intensity from*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Float4,VEU_TextureCoordinate,2));
		Offset += sizeof(FLOAT) * 4;
		/** The stream to read the rotation from.				*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Float2,VEU_BlendWeight,0));
		Offset += sizeof(FLOAT) * 2;
		/** The stream to read the texture coordinates from.	*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Float2,VEU_TextureCoordinate,0));
		Offset += sizeof(FLOAT) * 2;
		/** The stream to read the color from.					*/
		Elements.AddItem(FVertexElement(0,Offset,VET_Float4,VEU_TextureCoordinate,1));
		Offset += sizeof(FLOAT) * 4;

		// Create the vertex declaration for rendering the factory normally.
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements, TEXT("LensFlare"));
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/** The simple element vertex declaration. */
static TGlobalResource<FLensFlareVertexDeclaration> GLensFlareVertexDeclaration;

// FRenderResource interface.
void FLensFlareVertexFactory::InitRHI()
{
	SetDeclaration(GLensFlareVertexDeclaration.VertexDeclarationRHI);
}

void FLensFlareVertexFactory::ReleaseRHI()
{
	FRenderResource::ReleaseRHI();
}

/**
 * Should we cache the material's shadertype on this platform with this vertex factory? 
 */
UBOOL FLensFlareVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return (Material->IsUsedWithLensFlare() || Material->IsSpecialEngineMaterial()) && !Material->IsUsedWithDecals();
}

/**
 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
 */
void FLensFlareVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
{
	FVertexFactory::ModifyCompilationEnvironment(Platform, OutEnvironment);
	OutEnvironment.Definitions.Set(TEXT("USE_LENSFLARE"),TEXT("1"));
	OutEnvironment.Definitions.Set(TEXT("USE_OCCLUSION_PERCENTAGE"),TEXT("1"));
}

FVertexFactoryShaderParameters* FLensFlareVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FLensFlareVertexFactoryShaderParameters() : NULL;
}

/**
 *	
 */
void FLensFlareVertexFactoryShaderParameters::Set(FShader* VertexShader,const FVertexFactory* VertexFactory,const FSceneView& View) const
{
	FVector4 CameraRight, CameraUp;

	CameraUp	= -View.InvViewProjectionMatrix.TransformNormal(FVector(1.0f,0.0f,0.0f)).SafeNormal();
	CameraRight	= -View.InvViewProjectionMatrix.TransformNormal(FVector(0.0f,1.0f,0.0f)).SafeNormal();

	SetVertexShaderValue(VertexShader->GetVertexShader(),CameraRightParameter,CameraRight);
	SetVertexShaderValue(VertexShader->GetVertexShader(),CameraUpParameter,CameraUp);
}

void FLensFlareVertexFactoryShaderParameters::SetMesh(FShader* VertexShader, const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View) const
{
	SetVertexShaderValue(
		VertexShader->GetVertexShader(),
		LocalToWorldParameter,
		Mesh.Elements(BatchElementIndex).LocalToWorld.ConcatTranslation(View.PreViewTranslation)
		);
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FLensFlareVertexFactory,"LensFlareVertexFactory",TRUE,FALSE,FALSE,FALSE,TRUE, VER_ADDDED_OCCLUSION_PERCENTAGE_EXPRESSION,0);
