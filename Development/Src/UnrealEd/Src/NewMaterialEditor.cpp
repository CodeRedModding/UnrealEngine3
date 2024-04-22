/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EngineMaterialClasses.h"
#include "MaterialEditorBase.h"
#include "NewMaterialEditor.h"
#include "MaterialEditorContextMenus.h"
#include "MaterialEditorPreviewScene.h"
#include "MaterialEditorToolBar.h"
#include "UnLinkedObjDrawUtils.h"
#include "BusyCursor.h"
#include "ScopedTransaction.h"
#include "PropertyWindow.h"
#include "PropertyWindowManager.h"
#include "PropertyUtils.h"
#include "MaterialInstanceConstantEditor.h"

#if WITH_MANAGED_CODE
#include "GameAssetDatabaseShared.h"
#endif

IMPLEMENT_CLASS( UMaterialEditorOptions );

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Static helpers.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {

/** Location of the realtime preview icon, relative to the upper-hand corner of the material expression node. */
static const FIntPoint PreviewIconLoc( 2, 6 );

/** Width of the realtime preview icon. */
static const INT PreviewIconWidth = 10;

static const FColor ConnectionNormalColor( 0, 0, 0 );
static const FColor ConnectionOptionalColor( 100, 100, 100 );
static const FColor ConnectionSelectedColor( 255, 255, 0 );
static const FColor ConnectionMarkedColor( 40, 150, 40 );

static const FColor NormalBorderColor( 0, 0, 0 );
static const FColor NormalFontColor( 255, 255, 128 );
static const FColor SelectedBorderColor( 255, 255, 0 );
static const FColor SelectedFontColor( 128, 128, 255 );
static const FColor ErrorBorderColor( 255, 0, 0 );
static const FColor ErrorFontColor( NormalFontColor );
static const FColor SearchBorderColor( 100, 60, 60 );
static const FColor SearchFontColor( 255, 255, 255 );
static const FColor PreviewBorderColor( 70, 100, 200 );
static const FColor PreviewFontColor( NormalFontColor );
static const FColor DependancyBorderColor( 255, 0, 255 );
static const FColor DependancyFontColor( NormalFontColor );
static const FColor ParamBorderColor( 0, 128, 128 );
static const FColor ParamFontColor( NormalFontColor );
static const FColor ParamWithDupsBorderColor( 0, 255, 255 );
static const FColor ParamWithDupsFontColor( NormalFontColor );
static const FColor DynParamBorderColor( 0, 128, 128 );
static const FColor DynParamFontColor( NormalFontColor );
static const FColor DynParamWithDupsBorderColor( 0, 255, 255 );
static const FColor DynParamWithDupsFontColor( NormalFontColor );
static const FColor CurrentSearchedBorderColor( 128, 255, 128 );

static const FColor MaskRColor( 255, 0, 0 );
static const FColor MaskGColor( 0, 255, 0 );
static const FColor MaskBColor( 0, 0, 255 );
static const FColor MaskAColor( 255, 255, 255 );

static const FColor MaterialInputColor( 0, 0, 0 );
static const FColor LinkedObjColor( 112, 112, 112 );
static const FColor TitleBarColor( 70, 100, 200 );
static const FColor BorderShadowColor( 140, 140, 140 );
static const FColor PreviewColor( 70,100,200 );
static const FColor OutputColor( 0, 0, 0 );
static const FColor BackgroundColor( 0, 0, 0 );
static const FColor RealtimePreviewColor( 255, 215, 0 );
static const FColor AllPreviewsColor( 255, 0, 0 );
static const FColor CommentsColor( 64, 64, 192 );
static const FColor ShadowStringColor( 64, 64, 192 );
static const FColor CanvasColor( 161, 161, 161 );
} // namespace

/**
 * Class for rendering previews of material expressions in the material editor's linked object viewport.
 */
class FExpressionPreview : public FMaterial, public FMaterialRenderProxy
{
public:
	FExpressionPreview() :
		// Compile all the preview shaders together
		// They compile much faster together than one at a time since the shader compiler will use multiple cores
		bDeferCompilation(TRUE)
	{
		// Register this FMaterial derivative with AddEditorLoadedMaterialResource since it does not have a corresponding UMaterialInterface
		FMaterial::AddEditorLoadedMaterialResource(this);
	}

	FExpressionPreview(UMaterialExpression* InExpression) :	
		bDeferCompilation(TRUE),
		Expression(InExpression)
	{
		FMaterial::AddEditorLoadedMaterialResource(this);
	}

	/**
	 * Should the shader for this material with the given platform, shader type and vertex 
	 * factory type combination be compiled
	 *
	 * @param Platform		The platform currently being compiled for
	 * @param ShaderType	Which shader is being compiled
	 * @param VertexFactory	Which vertex factory is being compiled (can be NULL)
	 *
	 * @return TRUE if the shader should be compiled
	 */
	virtual UBOOL ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const
	{
		if(VertexFactoryType == FindVertexFactoryType(FName(TEXT("FLocalVertexFactory"), FNAME_Find)))
		{
			// we only need the non-light-mapped, base pass, local vertex factory shaders for drawing an opaque Material Tile
			// @todo: Added a FindShaderType by fname or something"

			if(appStristr(ShaderType->GetName(), TEXT("BasePassVertexShaderFNoLightMapPolicyFNoDensityPolicy")) ||
				appStristr(ShaderType->GetName(), TEXT("BasePassHullShaderFNoLightMapPolicyFNoDensityPolicy")) ||
				appStristr(ShaderType->GetName(), TEXT("BasePassDomainShaderFNoLightMapPolicyFNoDensityPolicy")))
			{
				return TRUE;
			}
			else if(appStristr(ShaderType->GetName(), TEXT("BasePassPixelShaderFNoLightMapPolicy")))
			{
				return TRUE;
			}
		}

		return FALSE;
	}

	/** Whether shaders should be compiled right away in CompileShaderMap or deferred until later. */
	virtual UBOOL DeferFinishCompiling() const { return bDeferCompilation; }

	////////////////
	// FMaterialRenderProxy interface.

	virtual const FMaterial* GetMaterial() const
	{
		if(GetShaderMap())
		{
			return this;
		}
		else
		{
			return GEngine->DefaultMaterial->GetRenderProxy(FALSE)->GetMaterial();
		}
	}

	virtual UBOOL GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
	{
		if (Expression->Material)
		{
			return Expression->Material->GetRenderProxy(0)->GetVectorValue(ParameterName, OutValue, Context);
		}
		return FALSE;
	}

	virtual UBOOL GetScalarValue(const FName ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const
	{
		if (Expression->Material)
		{
			return Expression->Material->GetRenderProxy(0)->GetScalarValue(ParameterName, OutValue, Context);
		}
		return FALSE;
	}

	virtual UBOOL GetTextureValue(const FName ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const
	{
		if (Expression->Material)
		{
			return Expression->Material->GetRenderProxy(0)->GetTextureValue(ParameterName, OutValue, Context);
		}
		return FALSE;
	}
	
	// Material properties.
	/** Entry point for compiling a specific material property.  This must call SetMaterialProperty. */
	virtual INT CompileProperty(EMaterialProperty Property,FMaterialCompiler* Compiler) const
	{
		// If the property is not active, don't compile it
		if (!IsActiveMaterialProperty(Expression->Material, Property))
		{
			return INDEX_NONE;
		}
		
		Compiler->SetMaterialProperty(Property);
		if( Property == MP_EmissiveColor )
		{
			// Hardcoding output 0 as we don't have the UI to specify any other output
			const INT OutputIndex = 0;
			// Get back into gamma corrected space, as DrawTile does not do this adjustment.
			return Compiler->Power(Expression->CompilePreview(Compiler, OutputIndex), Compiler->Constant(1.f/2.2f));
		}
		else if ( Property == MP_WorldPositionOffset)
		{
			//set to 0 to prevent off by 1 pixel errors
			return Compiler->Constant(0.0f);
		}
		else
		{
			return Compiler->Constant(1.0f);
		}
	}

	virtual FString GetMaterialUsageDescription() const { return FString::Printf(TEXT("FExpressionPreview %s"), Expression ? *Expression->GetName() : TEXT("NULL")); }
	virtual UBOOL IsTwoSided() const { return FALSE; }
	virtual UBOOL RenderTwoSidedSeparatePass() const { return FALSE; }
	virtual UBOOL RenderLitTranslucencyPrepass() const { return FALSE; }
	virtual UBOOL RenderLitTranslucencyDepthPostpass() const { return FALSE; }
	virtual UBOOL NeedsDepthTestDisabled() const { return FALSE; }
	virtual UBOOL IsLightFunction() const { return FALSE; }
	virtual UBOOL IsUsedWithFogVolumes() const { return FALSE; }
	virtual UBOOL IsSpecialEngineMaterial() const { return FALSE; }
	virtual UBOOL IsTerrainMaterial() const { return FALSE; }
	virtual UBOOL IsDecalMaterial() const { return FALSE; }
	virtual UBOOL IsWireframe() const { return FALSE; }
	virtual UBOOL IsDistorted() const { return FALSE; }
	virtual UBOOL HasSubsurfaceScattering() const { return FALSE; }
	virtual UBOOL HasSeparateTranslucency() const { return FALSE; }
	virtual UBOOL IsMasked() const { return FALSE; }
	virtual UBOOL CastLitTranslucencyShadowAsMasked() const { return FALSE; }
	virtual enum EBlendMode GetBlendMode() const { return BLEND_Opaque; }
	virtual enum EMaterialLightingModel GetLightingModel() const { return MLM_Unlit; }
	virtual FLOAT GetOpacityMaskClipValue() const { return 0.5f; }
	virtual FString GetFriendlyName() const { return FString::Printf(TEXT("FExpressionPreview %s"), Expression ? *Expression->GetName() : TEXT("NULL")); }
	/**
	 * Should shaders compiled for this material be saved to disk?
	 */
	virtual UBOOL IsPersistent() const { return FALSE; }

	const UMaterialExpression* GetExpression() const
	{
		return Expression;
	}

	friend FArchive& operator<< ( FArchive& Ar, FExpressionPreview& V )
	{
		return Ar << V.Expression;
	}

	UBOOL bDeferCompilation;

private:
	UMaterialExpression* Expression;
};

namespace {

/**
 * Class for rendering the material on the preview mesh in the Material Editor
 */
class FPreviewMaterial : public FMaterialResource, public FMaterialRenderProxy
{
public:
	FPreviewMaterial(UMaterial* InMaterial)
	:	FMaterialResource(InMaterial)
	{
		CacheShaders();
	}

	/**
	 * Should the shader for this material with the given platform, shader type and vertex 
	 * factory type combination be compiled
	 *
	 * @param Platform		The platform currently being compiled for
	 * @param ShaderType	Which shader is being compiled
	 * @param VertexFactory	Which vertex factory is being compiled (can be NULL)
	 *
	 * @return TRUE if the shader should be compiled
	 */
	virtual UBOOL ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const
	{
		// only generate the needed shaders (which should be very restrictive)
		// @todo: Add a FindShaderType by fname or something

		// Always allow HitProxy shaders.
		if (appStristr(ShaderType->GetName(), TEXT("HitProxy")))
		{
			return TRUE;
		}

		// we only need local vertex factory for the preview static mesh
		if (VertexFactoryType != FindVertexFactoryType(FName(TEXT("FLocalVertexFactory"), FNAME_Find)))
		{
			//cache for gpu skinned vertex factory if the material allows it
			//this way we can have a preview skeletal mesh
			if (!IsUsedWithSkeletalMesh() || VertexFactoryType != FindVertexFactoryType(FName(TEXT("FGPUSkinVertexFactory"), FNAME_Find)))
			{
				return FALSE;
			}
		}

		// look for any of the needed type
		UBOOL bShaderTypeMatches = FALSE;
		if (appStristr(ShaderType->GetName(), TEXT("BasePassPixelShaderFNoLightMapPolicy")))
		{
			bShaderTypeMatches = TRUE;
		}
		//used for instruction count on lit materials
		else if (appStristr(ShaderType->GetName(), TEXT("TBasePassPixelShaderFDirectionalLightMapTexturePolicyNoSkyLight")))
		{
			bShaderTypeMatches = TRUE;
		}
		//used for instruction count on lit particle materials
		else if (appStristr(ShaderType->GetName(), TEXT("TBasePassPixelShaderFDirectionalLightLightMapPolicySkyLight")))
		{
			bShaderTypeMatches = TRUE;
		}
		//used for instruction count on lit materials
		else if (appStristr(ShaderType->GetName(), TEXT("TLightPixelShaderFPointLightPolicyFNoStaticShadowingPolicy")))
		{
			bShaderTypeMatches = TRUE;
		}
		// Pick tessellation shader based on material settings
		else if(appStristr(ShaderType->GetName(), TEXT("BasePassVertexShaderFNoLightMapPolicyFNoDensityPolicy")) ||
			appStristr(ShaderType->GetName(), TEXT("BasePassHullShaderFNoLightMapPolicyFNoDensityPolicy")) ||
			appStristr(ShaderType->GetName(), TEXT("BasePassDomainShaderFNoLightMapPolicyFNoDensityPolicy")))
		{
			bShaderTypeMatches = TRUE;
		}
		else if (appStristr(ShaderType->GetName(), TEXT("TDistortion")))
		{
			bShaderTypeMatches = TRUE;
		}
		else if (appStristr(ShaderType->GetName(), TEXT("TSubsurface")))
		{
			bShaderTypeMatches = TRUE;
		}
		else if (appStristr(ShaderType->GetName(), TEXT("TLight")))
		{
			if (appStristr(ShaderType->GetName(), TEXT("FDirectionalLightPolicyFShadowTexturePolicy")) ||
				appStristr(ShaderType->GetName(), TEXT("FDirectionalLightPolicyFNoStaticShadowingPolicy")))
			{
				bShaderTypeMatches = TRUE;
			}
		}
		else if (IsUsedWithFogVolumes() 
			&& (appStristr(ShaderType->GetName(), TEXT("FFogVolumeApply"))
			|| appStristr(ShaderType->GetName(), TEXT("TFogIntegral")) && appStristr(ShaderType->GetName(), TEXT("FConstantDensityPolicy"))))
		{
			bShaderTypeMatches = TRUE;
		}
		else if (appStristr(ShaderType->GetName(), TEXT("DepthOnly")))
		{
			bShaderTypeMatches = TRUE;
		}
		else if (appStristr(ShaderType->GetName(), TEXT("ShadowDepth")))
		{
			bShaderTypeMatches = TRUE;
		}
		else if ((RenderLitTranslucencyDepthPostpass() || GetBlendMode() == BLEND_SoftMasked)
			&& appStristr(ShaderType->GetName(), TEXT("FTranslucencyPostRenderDepth")))
		{
			bShaderTypeMatches = TRUE;
		}

		return bShaderTypeMatches;
	}

	/**
	 * Should shaders compiled for this material be saved to disk?
	 */
	virtual UBOOL IsPersistent() const { return FALSE; }

	// FMaterialRenderProxy interface
	virtual const FMaterial* GetMaterial() const
	{
		if(GetShaderMap())
		{
			return this;
		}
		else
		{
			return GEngine->DefaultMaterial->GetRenderProxy(FALSE)->GetMaterial();
		}
	}

	virtual UBOOL GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
	{
		return Material->GetRenderProxy(0)->GetVectorValue(ParameterName, OutValue, Context);
	}

	virtual UBOOL GetScalarValue(const FName ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const
	{
		return Material->GetRenderProxy(0)->GetScalarValue(ParameterName, OutValue, Context);
	}

	virtual UBOOL GetTextureValue(const FName ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const
	{
		return Material->GetRenderProxy(0)->GetTextureValue(ParameterName,OutValue,Context);
	}
};

} // namespace



namespace {

/**
 * Hitproxy for the realtime preview icon on material expression nodes.
 */
struct HRealtimePreviewProxy : public HHitProxy
{
	DECLARE_HIT_PROXY( HRealtimePreviewProxy, HHitProxy );

	UMaterialExpression*	MaterialExpression;

	HRealtimePreviewProxy(UMaterialExpression* InMaterialExpression)
		:	HHitProxy( HPP_UI )
		,	MaterialExpression( InMaterialExpression )
	{}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << MaterialExpression;
	}
};

/**
 * Returns a reference to the material to be used as the material expression copy-paste buffer.
 */
static UMaterial* GetCopyPasteBuffer()
{
	if ( !GUnrealEd->MaterialCopyPasteBuffer )
	{
		GUnrealEd->MaterialCopyPasteBuffer = ConstructObject<UMaterial>( UMaterial::StaticClass() );
	}
	return GUnrealEd->MaterialCopyPasteBuffer;
}

/**
 * Returns the UStruct corrsponding to UMaterialExpression::ExpressionInput.
 */
static const UStruct* GetExpressionInputStruct()
{
	static const UStruct* ExpressionInputStruct = FindField<UStruct>( UMaterialExpression::StaticClass(), TEXT("ExpressionInput") );
	check( ExpressionInputStruct );
	return ExpressionInputStruct;
}

/**
 * Allows access to a material's input by index.
 */
static FExpressionInput* GetMaterialInput(UMaterial* Material, INT Index)
{
	FExpressionInput* ExpressionInput = NULL;
	switch( Index )
	{
	case 0: ExpressionInput = &Material->DiffuseColor ; break;
	case 1: ExpressionInput = &Material->DiffusePower ; break;
	case 2: ExpressionInput = &Material->EmissiveColor ; break;
	case 3: ExpressionInput = &Material->SpecularColor ; break;
	case 4: ExpressionInput = &Material->SpecularPower ; break;
	case 5: ExpressionInput = &Material->Opacity ; break;
	case 6: ExpressionInput = &Material->OpacityMask ; break;
	case 7: ExpressionInput = &Material->Distortion ; break;
	case 8: ExpressionInput = &Material->TwoSidedLightingMask ; break;
	case 9: ExpressionInput = &Material->TwoSidedLightingColor ; break;
	case 10: ExpressionInput = &Material->Normal ; break;
	case 11: ExpressionInput = &Material->CustomLighting ; break;
	case 12: ExpressionInput = &Material->CustomSkylightDiffuse ; break;
	case 13: ExpressionInput = &Material->AnisotropicDirection ; break;
	case 14: ExpressionInput = &Material->WorldPositionOffset ; break;
	case 15: ExpressionInput = &Material->WorldDisplacement ; break;
	case 16: ExpressionInput = &Material->TessellationMultiplier ; break;
	case 17: ExpressionInput = &Material->SubsurfaceInscatteringColor; break;
	case 18: ExpressionInput = &Material->SubsurfaceAbsorptionColor; break;
	case 19: ExpressionInput = &Material->SubsurfaceScatteringRadius; break;
	default: appErrorf( TEXT("%i: Invalid material input index"), Index );
	}
	return ExpressionInput;
}

/**
 * Finds the material expression that maps to the given index when unused
 * connections are hidden. 
 *
 * When unused connections are hidden, the given input index does not correspond to the
 * visible input node presented to the user. This function translates it to the correct input.
 */
static FExpressionInput* GetMatchingVisibleMaterialInput(UMaterial* Material, INT Index)
{
	FExpressionInput* MaterialInput = NULL;
	INT VisibleNodesToFind = Index;
	INT InputIndex = 0;

	// When VisibleNodesToFind is zero, then we found the corresponding input.
	while (VisibleNodesToFind >= 0)
	{
		MaterialInput = GetMaterialInput( Material, InputIndex );

		if (MaterialInput->IsConnected())
		{
			VisibleNodesToFind--;
		}

		InputIndex++;
	}

	// If VisibleNodesToFind is less than zero, then the loop to find the matching material input 
	// terminated prematurely. The material input could be pointing to the incorrect input.
	check(VisibleNodesToFind < 0);

	return MaterialInput;
}

/**
 * Returns the expression input from the given material.
 *
 * This function is a wrapper for finding the correct material input in the event that unused connectors are hidden. 
 */
static FExpressionInput* GetMaterialInputConditional(UMaterial* Material, INT Index, UBOOL bAreUnusedConnectorsHidden)
{
	FExpressionInput* MaterialInput = NULL;

	if(bAreUnusedConnectorsHidden)
	{
		MaterialInput = GetMatchingVisibleMaterialInput(Material, Index);
	} 
	else
	{
		MaterialInput = GetMaterialInput(Material, Index);
	}

	return MaterialInput;
}

/**
 * Connects the specified input expression to the specified output expression.
 */
static void ConnectExpressions(FExpressionInput& Input, const FExpressionOutput& Output, INT OutputIndex, UMaterialExpression* Expression)
{
	Input.Expression = Expression;
	Input.OutputIndex = OutputIndex;
	Input.Mask = Output.Mask;
	Input.MaskR = Output.MaskR;
	Input.MaskG = Output.MaskG;
	Input.MaskB = Output.MaskB;
	Input.MaskA = Output.MaskA;
}

/**
 * Connects the MaterialInputIndex'th material input to the MaterialExpressionOutputIndex'th material expression output.
 */
static void ConnectMaterialToMaterialExpression(UMaterial* Material,
												INT MaterialInputIndex,
												UMaterialExpression* MaterialExpression,
												INT MaterialExpressionOutputIndex,
												UBOOL bUnusedConnectionsHidden)
{
	// Assemble a list of outputs this material expression has.
	TArray<FExpressionOutput>& Outputs = MaterialExpression->GetOutputs();

	if (Outputs.Num() > 0)
	{
		const FExpressionOutput& ExpressionOutput = Outputs( MaterialExpressionOutputIndex );
		FExpressionInput* MaterialInput = GetMaterialInputConditional( Material, MaterialInputIndex, bUnusedConnectionsHidden );

		ConnectExpressions( *MaterialInput, ExpressionOutput,MaterialExpressionOutputIndex,  MaterialExpression );
	}
}

/** 
 * Helper struct that contains a reference to an expression and a subset of its inputs.
 */
struct FMaterialExpressionReference
{
public:
	FMaterialExpressionReference(UMaterialExpression* InExpression) :
		Expression(InExpression)
	{}

	FMaterialExpressionReference(FExpressionInput* InInput) :
		Expression(NULL)
	{
		Inputs.AddItem(InInput);
	}

	UMaterialExpression* Expression;
	TArray<FExpressionInput*> Inputs;
};

/**
 * Assembles a list of UMaterialExpressions and their FExpressionInput objects that refer to the specified FExpressionOutput.
 */
static void GetListOfReferencingInputs(
	const UMaterialExpression* InMaterialExpression, 
	UMaterial* Material, 
	TArray<FMaterialExpressionReference>& OutReferencingInputs, 
	const FExpressionOutput* MaterialExpressionOutput, 
	INT OutputIndex)
{
	OutReferencingInputs.Empty();
	const UBOOL bOutputIndexIsValid = OutputIndex != INDEX_NONE;

	// Gather references from other expressions
	for ( INT ExpressionIndex = 0 ; ExpressionIndex < Material->Expressions.Num() ; ++ExpressionIndex )
	{
		UMaterialExpression* MaterialExpression = Material->Expressions( ExpressionIndex );

		const TArray<FExpressionInput*>& ExpressionInputs = MaterialExpression->GetInputs();
		FMaterialExpressionReference NewReference(MaterialExpression);
		for ( INT ExpressionInputIndex = 0 ; ExpressionInputIndex < ExpressionInputs.Num() ; ++ExpressionInputIndex )
		{
			FExpressionInput* Input = ExpressionInputs(ExpressionInputIndex);

			if ( Input->Expression == InMaterialExpression &&
				(!MaterialExpressionOutput ||
				bOutputIndexIsValid && Input->OutputIndex == OutputIndex ||
				!bOutputIndexIsValid &&
				Input->Mask == MaterialExpressionOutput->Mask &&
				Input->MaskR == MaterialExpressionOutput->MaskR &&
				Input->MaskG == MaterialExpressionOutput->MaskG &&
				Input->MaskB == MaterialExpressionOutput->MaskB &&
				Input->MaskA == MaterialExpressionOutput->MaskA) )
			{
				NewReference.Inputs.AddItem(Input);
			}
		}

		if (NewReference.Inputs.Num() > 0)
		{
			OutReferencingInputs.AddItem(NewReference);
		}
	}

	// Gather references from material inputs
#define __GATHER_REFERENCE_TO_EXPRESSION( Mat, Prop ) \
	if ( Mat->Prop.Expression == InMaterialExpression && \
		(!MaterialExpressionOutput || \
		bOutputIndexIsValid && Mat->Prop.OutputIndex == OutputIndex || \
		!bOutputIndexIsValid && \
		Mat->Prop.Mask == MaterialExpressionOutput->Mask && \
		Mat->Prop.MaskR == MaterialExpressionOutput->MaskR && \
		Mat->Prop.MaskG == MaterialExpressionOutput->MaskG && \
		Mat->Prop.MaskB == MaterialExpressionOutput->MaskB && \
		Mat->Prop.MaskA == MaterialExpressionOutput->MaskA )) \
	{ OutReferencingInputs.AddItem(FMaterialExpressionReference(&(Mat->Prop))); }

	__GATHER_REFERENCE_TO_EXPRESSION( Material, DiffuseColor );
	__GATHER_REFERENCE_TO_EXPRESSION( Material, DiffusePower );
	__GATHER_REFERENCE_TO_EXPRESSION( Material, EmissiveColor );
	__GATHER_REFERENCE_TO_EXPRESSION( Material, SpecularColor );
	__GATHER_REFERENCE_TO_EXPRESSION( Material, SpecularPower );
	__GATHER_REFERENCE_TO_EXPRESSION( Material, Opacity );
	__GATHER_REFERENCE_TO_EXPRESSION( Material, OpacityMask );
	__GATHER_REFERENCE_TO_EXPRESSION( Material, Distortion );
	__GATHER_REFERENCE_TO_EXPRESSION( Material, TwoSidedLightingMask );
	__GATHER_REFERENCE_TO_EXPRESSION( Material, TwoSidedLightingColor );
	__GATHER_REFERENCE_TO_EXPRESSION( Material, Normal );
	__GATHER_REFERENCE_TO_EXPRESSION( Material, CustomLighting );
	__GATHER_REFERENCE_TO_EXPRESSION( Material, CustomSkylightDiffuse );
	__GATHER_REFERENCE_TO_EXPRESSION( Material, AnisotropicDirection );
	__GATHER_REFERENCE_TO_EXPRESSION( Material, WorldPositionOffset );
	__GATHER_REFERENCE_TO_EXPRESSION( Material, WorldDisplacement );
	__GATHER_REFERENCE_TO_EXPRESSION( Material, TessellationMultiplier );
	__GATHER_REFERENCE_TO_EXPRESSION( Material, SubsurfaceInscatteringColor );
	__GATHER_REFERENCE_TO_EXPRESSION( Material, SubsurfaceAbsorptionColor );
	__GATHER_REFERENCE_TO_EXPRESSION( Material, SubsurfaceScatteringRadius );
#undef __GATHER_REFERENCE_TO_EXPRESSION
}

/**
 * Assembles a list of FExpressionInput objects that refer to the specified FExpressionOutput.
 */
static void GetListOfReferencingInputs(
	const UMaterialExpression* InMaterialExpression, 
	UMaterial* Material, 
	TArray<FExpressionInput*>& OutReferencingInputs, 
	const FExpressionOutput* MaterialExpressionOutput = NULL,
	INT OutputIndex = INDEX_NONE)
{
	TArray<FMaterialExpressionReference> References;
	GetListOfReferencingInputs(InMaterialExpression, Material, References, MaterialExpressionOutput, OutputIndex);
	OutReferencingInputs.Empty();
	for (INT ReferenceIndex = 0; ReferenceIndex < References.Num(); ReferenceIndex++)
	{
		for (INT InputIndex = 0; InputIndex < References(ReferenceIndex).Inputs.Num(); InputIndex++)
		{
			OutReferencingInputs.AddItem(References(ReferenceIndex).Inputs(InputIndex));
		}
	}
}

/**
 * Swaps all links to OldExpression with NewExpression.  NewExpression may be NULL.
 */
static void SwapLinksToExpression(UMaterialExpression* OldExpression, UMaterialExpression* NewExpression, UMaterial* Material)
{
	check(OldExpression);
	check(Material);

	Material->Modify();

	{
		// Move any of OldExpression's inputs over to NewExpression
		const TArray<FExpressionInput*>& OldExpressionInputs = OldExpression->GetInputs();

		TArray<FExpressionInput*> NewExpressionInputs;
		if (NewExpression)
		{
			NewExpression->Modify();
			NewExpressionInputs = NewExpression->GetInputs();
		}

		// Copy the inputs over, matching them up based on the order they are declared in each class
		for (INT InputIndex = 0; InputIndex < OldExpressionInputs.Num() && InputIndex < NewExpressionInputs.Num(); InputIndex++)
		{
			*NewExpressionInputs(InputIndex) = *OldExpressionInputs(InputIndex);
		}
	}	
	
	// Move any links from other expressions to OldExpression over to NewExpression
	TArray<FExpressionOutput>& Outputs = OldExpression->GetOutputs();
	TArray<FExpressionOutput> NewOutputs;
	if (NewExpression)
	{
		NewOutputs = NewExpression->GetOutputs();
	}
	else
	{
		NewOutputs.AddItem(FExpressionOutput(FALSE));
	}

	for (INT OutputIndex = 0; OutputIndex < Outputs.Num(); OutputIndex++)
	{
		const FExpressionOutput& CurrentOutput = Outputs(OutputIndex);
		
		FExpressionOutput NewOutput(FALSE);
		UBOOL bFoundMatchingOutput = FALSE;

		// Try to find an equivalent output in NewExpression
		for (INT NewOutputIndex = 0; NewOutputIndex < NewOutputs.Num(); NewOutputIndex++)
		{
			const FExpressionOutput& CurrentNewOutput = NewOutputs(NewOutputIndex);
			if(	CurrentOutput.Mask == CurrentNewOutput.Mask
				&& CurrentOutput.MaskR == CurrentNewOutput.MaskR
				&& CurrentOutput.MaskG == CurrentNewOutput.MaskG
				&& CurrentOutput.MaskB == CurrentNewOutput.MaskB
				&& CurrentOutput.MaskA == CurrentNewOutput.MaskA )
			{
				NewOutput = CurrentNewOutput;
				bFoundMatchingOutput = TRUE;
			}
		}
		// Couldn't find an equivalent output in NewExpression, just pick the first
		// The user will have to fix up any issues from the mismatch
		if (!bFoundMatchingOutput && NewOutputs.Num() > 0)
		{
			NewOutput = NewOutputs(0);
		}

		TArray<FMaterialExpressionReference> ReferencingInputs;
		GetListOfReferencingInputs(OldExpression, Material, ReferencingInputs, &CurrentOutput, OutputIndex);
		for (INT ReferenceIndex = 0; ReferenceIndex < ReferencingInputs.Num(); ReferenceIndex++)
		{
			FMaterialExpressionReference& CurrentReference = ReferencingInputs(ReferenceIndex);
			if (CurrentReference.Expression)
			{
				CurrentReference.Expression->Modify();
			}
			// Move the link to OldExpression over to NewExpression
			for (INT InputIndex = 0; InputIndex < CurrentReference.Inputs.Num(); InputIndex++)
			{
				ConnectExpressions(*CurrentReference.Inputs(InputIndex), NewOutput, OutputIndex, NewExpression);
			}
		}
	}
}

/**
 * Populates the specified material's Expressions array (eg if cooked out or old content).
 * Also ensures materials and expressions are RF_Transactional for undo/redo support.
 */
static void InitExpressions(UMaterial* Material)
{
	FString ParmName;

	if( Material->Expressions.Num() == 0 )
	{
		for( TObjectIterator<UMaterialExpression> It; It; ++It )
		{
			UMaterialExpression* MaterialExpression = *It;
			if( MaterialExpression->GetOuter() == Material && !MaterialExpression->IsPendingKill() )
			{
				// Comment expressions are stored in a separate list.
				if ( MaterialExpression->IsA( UMaterialExpressionComment::StaticClass() ) )
				{
					Material->EditorComments.AddItem( static_cast<UMaterialExpressionComment*>(MaterialExpression) );
				}
				else
				{
					Material->Expressions.AddItem( MaterialExpression );
				}

				Material->AddExpressionParameter(MaterialExpression);
			}
		}
	}
	else
	{
		Material->BuildEditorParameterList();
	}

	// Propagate RF_Transactional to all referenced material expressions.
	Material->SetFlags( RF_Transactional );
	for( INT MaterialExpressionIndex = 0 ; MaterialExpressionIndex < Material->Expressions.Num() ; ++MaterialExpressionIndex )
	{
		UMaterialExpression* MaterialExpression = Material->Expressions( MaterialExpressionIndex );
		MaterialExpression->SetFlags( RF_Transactional );
	}
	for( INT MaterialExpressionIndex = 0 ; MaterialExpressionIndex < Material->EditorComments.Num() ; ++MaterialExpressionIndex )
	{
		UMaterialExpressionComment* Comment = Material->EditorComments( MaterialExpressionIndex );
		Comment->SetFlags( RF_Transactional );
	}
}

} // namespace

/** Implementation of Preview Material functions*/
FMaterialResource* UPreviewMaterial::AllocateResource()
{
	return new FPreviewMaterial(this);
}
IMPLEMENT_CLASS(UPreviewMaterial);


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxMaterialEditor
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/** 
 * TRUE if the list of UMaterialExpression-derived classes shared between material
 * editors has already been created.
 */
UBOOL WxMaterialEditor::bMaterialExpressionClassesInitialized = FALSE;

UBOOL WxMaterialEditor::bMaterialFunctionLibraryInitialized = FALSE;

/** Static array of UMaterialExpression-derived classes, shared between all material editor instances. */
TArray<UClass*> WxMaterialEditor::MaterialExpressionClasses;

/** Static array of categorized material expression classes. */
TArray<FCategorizedMaterialExpressionNode> WxMaterialEditor::CategorizedMaterialExpressionClasses;
TArray<UClass*> WxMaterialEditor::FavoriteExpressionClasses;
TArray<UClass*> WxMaterialEditor::UnassignedExpressionClasses;
TArray<FCategorizedMaterialFunctions> WxMaterialEditor::FunctionsFromAssetDatabase;
TMap<FString, WxMaterialEditor*> WxMaterialEditor::ActiveMaterialEditors;

/**
 * Comparison function used to sort material expression classes by name.
 */
IMPLEMENT_COMPARE_POINTER( UClass, MaterialEditor, { return appStricmp( *A->GetName(), *B->GetName() ); } )

/**
 *	Grab the expression array for the given category.
 *	
 *	@param	InCategoryName	The category to retrieve
 *	@param	bCreate			If TRUE, create the entry if not found.
 *
 *	@return	The category node.
 */
FCategorizedMaterialExpressionNode* WxMaterialEditor::GetCategoryNode(const FString& InCategoryName, UBOOL bCreate)
{
	for (INT CheckIndex = 0; CheckIndex < CategorizedMaterialExpressionClasses.Num(); CheckIndex++)
	{
		FCategorizedMaterialExpressionNode* CheckNode = &(CategorizedMaterialExpressionClasses(CheckIndex));
		if (CheckNode)
		{
			if (CheckNode->CategoryName == InCategoryName)
			{
				return CheckNode;
			}
		}
	}

	if (bCreate == TRUE)
	{
		FCategorizedMaterialExpressionNode* NewNode = new(CategorizedMaterialExpressionClasses)FCategorizedMaterialExpressionNode;
		check(NewNode);

		NewNode->CategoryName = InCategoryName;
		return NewNode;
	}

	return NULL;
}

/** Sorting helper... */
IMPLEMENT_COMPARE_CONSTREF(FCategorizedMaterialExpressionNode,NewMaterialEditor,{ return A.CategoryName > B.CategoryName ? 1 : -1; });
IMPLEMENT_COMPARE_CONSTREF(FCategorizedMaterialFunctions,NewMaterialEditor,{ return A.CategoryName > B.CategoryName ? 1 : -1; });

/**
 * Initializes the list of UMaterialExpression-derived classes shared between all material editor instances.
 */
void WxMaterialEditor::InitMaterialExpressionClasses()
{
	if( !bMaterialExpressionClassesInitialized )
	{
		UMaterialEditorOptions* TempEditorOptions = ConstructObject<UMaterialEditorOptions>( UMaterialEditorOptions::StaticClass() );
		UClass* BaseType = UMaterialExpression::StaticClass();
		if( BaseType )
		{
			TArray<UStructProperty*>	ExpressionInputs;
			const UStruct*				ExpressionInputStruct = GetExpressionInputStruct();

			for( TObjectIterator<UClass> It ; It ; ++It )
			{
				UClass* Class = *It;
				if( !(Class->ClassFlags & CLASS_Abstract) && !(Class->ClassFlags & CLASS_Deprecated) )
				{
					if( Class->IsChildOf(UMaterialExpression::StaticClass()) )
					{
						ExpressionInputs.Empty();

						// Exclude comments from the expression list, as well as the base parameter expression, as it should not be used directly
						if ( Class != UMaterialExpressionComment::StaticClass() 
							&& Class != UMaterialExpressionParameter::StaticClass() )
						{
							MaterialExpressionClasses.AddItem( Class );

							// Initialize the expression class input map.							
							for( TFieldIterator<UStructProperty> InputIt(Class) ; InputIt ; ++InputIt )
							{
								UStructProperty* StructProp = *InputIt;
								if( StructProp->Struct == ExpressionInputStruct )
								{
									ExpressionInputs.AddItem( StructProp );
								}
							}

							// See if it is in the favorites array...
							for (INT FavoriteIndex = 0; FavoriteIndex < TempEditorOptions->FavoriteExpressions.Num(); FavoriteIndex++)
							{
								if (Class->GetName() == TempEditorOptions->FavoriteExpressions(FavoriteIndex))
								{
									FavoriteExpressionClasses.AddUniqueItem(Class);
								}
							}

							// Category fill...
							UMaterialExpression* TempObject = Cast<UMaterialExpression>(Class->GetDefaultObject());
							if (TempObject)
							{
								if (TempObject->MenuCategories.Num() == 0)
								{
									UnassignedExpressionClasses.AddItem(Class);
								}
								else
								{
									for (INT CategoryIndex = 0; CategoryIndex < TempObject->MenuCategories.Num(); CategoryIndex++)
									{
										const FString TempName = TempObject->MenuCategories(CategoryIndex).ToString();
										FCategorizedMaterialExpressionNode* CategoryNode = GetCategoryNode(
											TempName, TRUE);
										check(CategoryNode);

										CategoryNode->MaterialExpressionClasses.AddUniqueItem(Class);
									}
								}
							}
						}
					}
				}
			}
		}

		Sort<USE_COMPARE_POINTER(UClass,MaterialEditor)>( static_cast<UClass**>(MaterialExpressionClasses.GetTypedData()), MaterialExpressionClasses.Num() );
		Sort<USE_COMPARE_CONSTREF(FCategorizedMaterialExpressionNode,NewMaterialEditor)>(&(CategorizedMaterialExpressionClasses(0)),CategorizedMaterialExpressionClasses.Num());

		bMaterialExpressionClassesInitialized = TRUE;
	}
}

void WxMaterialEditor::InitializeMaterialFunctionLibrary()
{
	// Initialize FunctionsFromAssetDatabase if it hasn't been yet
	if (!bMaterialFunctionLibraryInitialized)
	{
#if WITH_MANAGED_CODE
		if (FGameAssetDatabase::Get().IsInitialized())
		{
			// Make a system tag for the material function object type
			const FString FunctionTypeTag = FGameAssetDatabase::MakeObjectTypeSystemTag(UMaterialFunction::StaticClass()->GetName());

			TArray<FString> AssetFullNames;
			// Get all material functions from the asset database
			FGameAssetDatabase::Get().QueryAssetsWithTag(FunctionTypeTag, AssetFullNames);

			for (INT AssetIndex = 0; AssetIndex < AssetFullNames.Num(); AssetIndex++)
			{
				const INT SpaceIndex = AssetFullNames(AssetIndex).InStr(TEXT(" "));
				if (SpaceIndex != INDEX_NONE)
				{
					FString FunctionPathName = AssetFullNames(AssetIndex).Right(AssetFullNames(AssetIndex).Len() - SpaceIndex - 1);

					// Load the function so we can check if it is actually valid and get the categories
					UMaterialFunction* Function = LoadObject<UMaterialFunction>(NULL, *FunctionPathName, NULL, 0, NULL);

					if (Function && Function->bExposeToLibrary)
					{
						// Update FunctionPathName, because if we loaded a redirector it will not be accurate anymore
						FunctionPathName = Function->GetPathName();

						for (INT CategoryIndex = 0; CategoryIndex < Function->LibraryCategories.Num(); CategoryIndex++)
						{
							const FString& CategoryName = Function->LibraryCategories(CategoryIndex);
							FCategorizedMaterialFunctions* ExistingCategory = NULL;

							for (INT SearchCategoryIndex = 0; SearchCategoryIndex < FunctionsFromAssetDatabase.Num(); SearchCategoryIndex++)
							{
								if (FunctionsFromAssetDatabase(SearchCategoryIndex).CategoryName == CategoryName)
								{
									ExistingCategory = &FunctionsFromAssetDatabase(SearchCategoryIndex);
									break;
								}
							}

							if (ExistingCategory)
							{
								// Add this function to an existing category
								FFunctionEntryInfo NewEntry;
								NewEntry.Path = FunctionPathName;
								NewEntry.ToolTip = Function->Description;
								ExistingCategory->FunctionInfos.AddUniqueItem(NewEntry);
							}
							else
							{
								// Add this function to a new category
								FCategorizedMaterialFunctions NewCategory;
								NewCategory.CategoryName = CategoryName;
								FFunctionEntryInfo NewEntry;
								NewEntry.Path = FunctionPathName;
								NewEntry.ToolTip = Function->Description;
								NewCategory.FunctionInfos.AddUniqueItem(NewEntry);
								FunctionsFromAssetDatabase.AddItem(NewCategory);
							}
						}
					}
				}
			}
		}
#endif
		bMaterialFunctionLibraryInitialized = TRUE;
	}

	// Start from the list from the asset database
	CategorizedFunctionLibrary = FunctionsFromAssetDatabase;

	// Trim any entries from FunctionsFromAssetDatabase that are no longer valid
	for (INT CategoryIndex = 0; CategoryIndex < CategorizedFunctionLibrary.Num(); CategoryIndex++)
	{
		FCategorizedMaterialFunctions& Category = CategorizedFunctionLibrary(CategoryIndex);
		for (INT EntryIndex = Category.FunctionInfos.Num() - 1; EntryIndex >= 0; EntryIndex--)
		{
			FFunctionEntryInfo& FunctionEntry = Category.FunctionInfos(EntryIndex);
			UMaterialFunction* Function = FindObject<UMaterialFunction>(NULL, *FunctionEntry.Path);

			if (!Function 
				|| Function->GetPathName() != FunctionEntry.Path
				// Handle the function having changed categories
				|| !Function->LibraryCategories.ContainsItem(Category.CategoryName))
			{
				Category.FunctionInfos.Remove(EntryIndex);
			}
		}
	}

	// Iterate over functions in memory and add them to the list
	for (TObjectIterator<UMaterialFunction> It; It; ++It)
	{
		UMaterialFunction* CurrentFunction = *It;
		if (CurrentFunction->bExposeToLibrary 
			&& !CurrentFunction->HasAnyFlags(RF_ClassDefaultObject)
			// Skip functions that are currently being edited
			&& CurrentFunction->GetOutermost() != UObject::GetTransientPackage())
		{
			for (INT CategoryIndex = 0; CategoryIndex < CurrentFunction->LibraryCategories.Num(); CategoryIndex++)
			{
				const FString& CategoryName = CurrentFunction->LibraryCategories(CategoryIndex);
				FCategorizedMaterialFunctions* ExistingCategory = NULL;

				for (INT SearchCategoryIndex = 0; SearchCategoryIndex < CategorizedFunctionLibrary.Num(); SearchCategoryIndex++)
				{
					if (CategorizedFunctionLibrary(SearchCategoryIndex).CategoryName == CategoryName)
					{
						ExistingCategory = &CategorizedFunctionLibrary(SearchCategoryIndex);
						break;
					}
				}

				if (ExistingCategory)
				{
					FFunctionEntryInfo NewEntry;
					NewEntry.Path = CurrentFunction->GetPathName();
					NewEntry.ToolTip = CurrentFunction->Description;
					ExistingCategory->FunctionInfos.AddUniqueItem(NewEntry);
				}
				else
				{
					FCategorizedMaterialFunctions NewCategory;
					NewCategory.CategoryName = CategoryName;
					FFunctionEntryInfo NewEntry;
					NewEntry.Path = CurrentFunction->GetPathName();
					NewEntry.ToolTip = CurrentFunction->Description;
					NewCategory.FunctionInfos.AddUniqueItem(NewEntry);
					CategorizedFunctionLibrary.AddItem(NewCategory);
				}
			}
		}
	}

	// Sort alphabetically
	Sort<USE_COMPARE_CONSTREF(FCategorizedMaterialFunctions,NewMaterialEditor)>(&(CategorizedFunctionLibrary(0)),CategorizedFunctionLibrary.Num());
}

/**
 * Remove the expression from the favorites menu list.
 */
void WxMaterialEditor::RemoveMaterialExpressionFromFavorites(UClass* InClass)
{
	FavoriteExpressionClasses.RemoveItem(InClass);
}

/**
 * Add the expression to the favorites menu list.
 */
void WxMaterialEditor::AddMaterialExpressionToFavorites(UClass* InClass)
{
	FavoriteExpressionClasses.AddUniqueItem(InClass);
}

BEGIN_EVENT_TABLE( WxMaterialEditor, WxMaterialEditorBase )
	EVT_CLOSE( WxMaterialEditor::OnClose )
	EVT_MENU_RANGE( IDM_NEW_SHADER_EXPRESSION_START, IDM_NEW_SHADER_EXPRESSION_END, WxMaterialEditor::OnNewMaterialExpression )
	EVT_MENU( ID_MATERIALEDITOR_NEW_COMMENT, WxMaterialEditor::OnNewComment )
	EVT_MENU( IDM_USE_CURRENT_TEXTURE, WxMaterialEditor::OnUseCurrentTexture )
	EVT_MENU( ID_MATERIALEDITOR_PASTE_HERE, WxMaterialEditor::OnPasteHere )

	EVT_TOOL( IDM_SHOW_BACKGROUND, WxMaterialEditor::OnShowBackground )
	EVT_TOOL( ID_MATERIALEDITOR_TOGGLEGRID, WxMaterialEditor::OnToggleGrid )
	EVT_TOOL( ID_MATERIALEDITOR_SHOWHIDE_CONNECTORS, WxMaterialEditor::OnShowHideConnectors )

	EVT_TOOL( ID_MATERIALEDITOR_REALTIME_EXPRESSIONS, WxMaterialEditor::OnRealTimeExpressions )
	EVT_TOOL( ID_MATERIALEDITOR_ALWAYS_REFRESH_ALL_PREVIEWS, WxMaterialEditor::OnAlwaysRefreshAllPreviews )

	EVT_UPDATE_UI( ID_MATERIALEDITOR_REALTIME_EXPRESSIONS, WxMaterialEditor::UI_RealTimeExpressions )
	EVT_UPDATE_UI( ID_MATERIALEDITOR_SHOWHIDE_CONNECTORS, WxMaterialEditor::UI_HideUnusedConnectors )

	EVT_BUTTON( ID_MATERIALEDITOR_APPLY, WxMaterialEditor::OnApply )
	EVT_UPDATE_UI( ID_MATERIALEDITOR_APPLY, WxMaterialEditor::UI_Apply )

	EVT_TOOL( ID_MATERIALEDITOR_FLATTEN, WxMaterialEditor::OnFlatten )

	EVT_TOOL( ID_MATERIALEDITOR_TOGGLESTATS, WxMaterialEditor::OnToggleStats )
	EVT_UPDATE_UI( ID_MATERIALEDITOR_TOGGLESTATS, WxMaterialEditor::UI_ToggleStats )

	EVT_TOOL( ID_MATERIALEDITOR_VIEWSOURCE, WxMaterialEditor::OnViewSource )
	EVT_UPDATE_UI( ID_MATERIALEDITOR_VIEWSOURCE, WxMaterialEditor::UI_ViewSource )

	EVT_TOOL( ID_GO_HOME, WxMaterialEditor::OnCameraHome )
	EVT_TOOL( ID_MATERIALEDITOR_CLEAN_UNUSED_EXPRESSIONS, WxMaterialEditor::OnCleanUnusedExpressions )

	EVT_TREE_BEGIN_DRAG( ID_MATERIALEDITOR_MATERIALEXPRESSIONTREE, WxMaterialEditor::OnMaterialExpressionTreeDrag )
	EVT_LIST_BEGIN_DRAG( ID_MATERIALEDITOR_MATERIALEXPRESSIONLIST, WxMaterialEditor::OnMaterialExpressionListDrag )

	EVT_TREE_BEGIN_DRAG( ID_MATERIALEDITOR_MATERIALFUNCTIONTREE, WxMaterialEditor::OnMaterialFunctionTreeDrag )
	EVT_LIST_BEGIN_DRAG( ID_MATERIALEDITOR_MATERIALFUNCTIONLIST, WxMaterialEditor::OnMaterialFunctionListDrag )

	EVT_MENU( ID_MATERIALEDITOR_DUPLICATE_NODES, WxMaterialEditor::OnDuplicateObjects )
	EVT_MENU( ID_MATERIALEDITOR_DELETE_NODE, WxMaterialEditor::OnDeleteObjects )
	EVT_MENU( ID_MATERIALEDITOR_CONVERT_TO_PARAMETER, WxMaterialEditor::OnConvertObjects )
	EVT_MENU( ID_MATERIALEDITOR_SELECT_DOWNSTREAM_NODES, WxMaterialEditor::OnSelectDownsteamNodes )
	EVT_MENU( ID_MATERIALEDITOR_SELECT_UPSTREAM_NODES, WxMaterialEditor::OnSelectUpsteamNodes )
	EVT_MENU( ID_MATERIALEDITOR_BREAK_LINK, WxMaterialEditor::OnBreakLink )
	EVT_MENU( ID_MATERIALEDITOR_BREAK_ALL_LINKS, WxMaterialEditor::OnBreakAllLinks )
	EVT_MENU( ID_MATERIALEDITOR_REMOVE_FROM_FAVORITES, WxMaterialEditor::OnRemoveFromFavorites )
	EVT_MENU( ID_MATERIALEDITOR_ADD_TO_FAVORITES, WxMaterialEditor::OnAddToFavorites )
	EVT_MENU( ID_MATERIALEDITOR_PREVIEW_NODE, WxMaterialEditor::OnPreviewNode )

	EVT_MENU( ID_MATERIALEDITOR_CONNECT_TO_DiffuseColor, WxMaterialEditor::OnConnectToMaterial_DiffuseColor )
	EVT_MENU( ID_MATERIALEDITOR_CONNECT_TO_DiffusePower, WxMaterialEditor::OnConnectToMaterial_DiffusePower )
	EVT_MENU( ID_MATERIALEDITOR_CONNECT_TO_EmissiveColor, WxMaterialEditor::OnConnectToMaterial_EmissiveColor )
	EVT_MENU( ID_MATERIALEDITOR_CONNECT_TO_SpecularColor, WxMaterialEditor::OnConnectToMaterial_SpecularColor )
	EVT_MENU( ID_MATERIALEDITOR_CONNECT_TO_SpecularPower, WxMaterialEditor::OnConnectToMaterial_SpecularPower )
	EVT_MENU( ID_MATERIALEDITOR_CONNECT_TO_Opacity, WxMaterialEditor::OnConnectToMaterial_Opacity )
	EVT_MENU( ID_MATERIALEDITOR_CONNECT_TO_OpacityMask, WxMaterialEditor::OnConnectToMaterial_OpacityMask )
	EVT_MENU( ID_MATERIALEDITOR_CONNECT_TO_Distortion, WxMaterialEditor::OnConnectToMaterial_Distortion)
	EVT_MENU( ID_MATERIALEDITOR_CONNECT_TO_TransmissionMask, WxMaterialEditor::OnConnectToMaterial_TransmissionMask )
	EVT_MENU( ID_MATERIALEDITOR_CONNECT_TO_TransmissionColor, WxMaterialEditor::OnConnectToMaterial_TransmissionColor )
	EVT_MENU( ID_MATERIALEDITOR_CONNECT_TO_Normal, WxMaterialEditor::OnConnectToMaterial_Normal )
	EVT_MENU( ID_MATERIALEDITOR_CONNECT_TO_CustomLighting, WxMaterialEditor::OnConnectToMaterial_CustomLighting )
	EVT_MENU( ID_MATERIALEDITOR_CONNECT_TO_CustomLightingDiffuse, WxMaterialEditor::OnConnectToMaterial_CustomLightingDiffuse )
	EVT_MENU( ID_MATERIALEDITOR_CONNECT_TO_AnisotropicDirection, WxMaterialEditor::OnConnectToMaterial_AnisotropicDirection )
	EVT_MENU( ID_MATERIALEDITOR_CONNECT_TO_WorldPositionOffset, WxMaterialEditor::OnConnectToMaterial_WorldPositionOffset )
	EVT_MENU( ID_MATERIALEDITOR_CONNECT_TO_WorldDisplacement, WxMaterialEditor::OnConnectToMaterial_WorldDisplacement )
	EVT_MENU( ID_MATERIALEDITOR_CONNECT_TO_TessellationMultiplier, WxMaterialEditor::OnConnectToMaterial_TessellationMultiplier )
	EVT_MENU( ID_MATERIALEDITOR_CONNECT_TO_SubsurfaceInscatteringColor, WxMaterialEditor::OnConnectToMaterial_SubsurfaceInscatteringColor )
	EVT_MENU( ID_MATERIALEDITOR_CONNECT_TO_SubsurfaceAbsorptionColor, WxMaterialEditor::OnConnectToMaterial_SubsurfaceAbsorptionColor )
	EVT_MENU( ID_MATERIALEDITOR_CONNECT_TO_SubsurfaceScatteringRadius, WxMaterialEditor::OnConnectToMaterial_SubsurfaceScatteringRadius )
	EVT_MENU_RANGE( ID_MATERIALEDITOR_CONNECT_TO_FunctionOutputBegin, ID_MATERIALEDITOR_CONNECT_TO_FunctionOutputEnd, WxMaterialEditor::OnConnectToFunctionOutput )
	
	EVT_TEXT(ID_MATERIALEDITOR_SEARCH, WxMaterialEditor::OnSearchChanged)
	EVT_BUTTON(ID_SEARCHTEXTCTRL_FINDNEXT_BUTTON, WxMaterialEditor::OnSearchNext)
	EVT_BUTTON(ID_SEARCHTEXTCTRL_FINDPREV_BUTTON, WxMaterialEditor::OnSearchPrev)
	
END_EVENT_TABLE()

/**
* A class to hold the data about an item in the tree
*/
class FExpressionTreeData : public wxTreeItemData
{
	INT Index;
public:
	FExpressionTreeData( INT InIndex )
	:	Index(InIndex)
	{}
	INT GetIndex()
	{
		return Index;
	}
};



/**
 * A panel containing a list of material expression node types.
 */
struct FExpressionListEntry
{
	FExpressionListEntry( const TCHAR* InExpressionName, INT InClassIndex, INT bInAllowedType )
	:	ExpressionName(InExpressionName), ClassIndex(InClassIndex), bAllowedType(bInAllowedType)
	{}

	FString ExpressionName;
	INT ClassIndex;
	UBOOL bAllowedType;
};

IMPLEMENT_COMPARE_CONSTREF( FExpressionListEntry, NewMaterialEditor, { return appStricmp(*A.ExpressionName,*B.ExpressionName); } );

class WxMaterialExpressionList : public wxPanel
{
public:
	WxMaterialExpressionList(wxWindow* InParent)
		: wxPanel( InParent )
	{
		// Search/Filter box
		FilterSearchCtrl = new WxSearchControl;
		FilterSearchCtrl->Create(this, ID_MATERIALEDITOR_MATERIALEXPRESSIONLIST_SEARCHBOX);

		// Tree
		MaterialExpressionTree = new wxTreeCtrl( this, ID_MATERIALEDITOR_MATERIALEXPRESSIONTREE, wxDefaultPosition, wxDefaultSize, wxTR_HIDE_ROOT|wxTR_SINGLE );
		MaterialExpressionFilterList = new wxListCtrl( this, ID_MATERIALEDITOR_MATERIALEXPRESSIONLIST, wxDefaultPosition, wxDefaultSize, wxLC_REPORT|wxLC_NO_HEADER|wxLC_SINGLE_SEL );

		// Hide the list unless we have a filter.
		MaterialExpressionFilterList->Show(FALSE);

		Sizer = new wxBoxSizer(wxVERTICAL);
		Sizer->Add( FilterSearchCtrl, 0, wxGROW|wxALL, 4 );
		Sizer->Add( MaterialExpressionTree, 1, wxGROW|wxALL, 4 );
		Sizer->Add( MaterialExpressionFilterList, 1, wxGROW|wxALL, 4 );
		SetSizer( Sizer );
		SetAutoLayout(true);
	}

	void PopulateExpressionsTree(UBOOL bPopulateForMaterialFunction)
	{
		MaterialExpressionTree->Freeze();
		MaterialExpressionTree->DeleteAllItems();

		TMap<FName,wxTreeItemId> ExpressionCategoryMap;

		check( WxMaterialEditor::bMaterialExpressionClassesInitialized );

		// not shown so doesn't need localization
		wxTreeItemId RootItem = MaterialExpressionTree->AddRoot(TEXT("Expressions"));

		for( INT Index = 0 ; Index < WxMaterialEditor::MaterialExpressionClasses.Num() ; ++Index )
		{
			UClass* MaterialExpressionClass = WxMaterialEditor::MaterialExpressionClasses(Index);

			// Trim the material expression name and add it to the list used for filtering.
			FString ExpressionName = FString(*MaterialExpressionClass->GetName()).Mid(appStrlen(TEXT("MaterialExpression"))); 
			
			UMaterialExpression* TempObject = Cast<UMaterialExpression>(MaterialExpressionClass->GetDefaultObject());

			const UBOOL bAllowedType = IsAllowedExpressionType(MaterialExpressionClass, bPopulateForMaterialFunction);
			new(ExpressionList) FExpressionListEntry(*ExpressionName, Index, bAllowedType);

			if (TempObject && bAllowedType)
			{
				// add at top level as there are no categories
				if (TempObject->MenuCategories.Num() == 0)
				{
					MaterialExpressionTree->AppendItem( RootItem, *ExpressionName, -1, -1, new FExpressionTreeData(Index) );
				}
				else
				{
					for( INT CategoryIdx = 0 ; CategoryIdx < TempObject->MenuCategories.Num() ; CategoryIdx++ )
					{
						// find or create category tree items as necessary
						FName CategoryName = TempObject->MenuCategories(CategoryIdx);
						wxTreeItemId* ExistingItem = ExpressionCategoryMap.Find(CategoryName);
						if ( ExistingItem == NULL )
						{
							ExistingItem = &ExpressionCategoryMap.Set( CategoryName, MaterialExpressionTree->AppendItem( RootItem, *CategoryName.ToString() ) );
							MaterialExpressionTree->SetItemBold( *ExistingItem );
						}

						// add the new item
						wxTreeItemId NewItem = MaterialExpressionTree->AppendItem( *ExistingItem, *ExpressionName, -1, -1, new FExpressionTreeData(Index) );
					}
				}
			}
		}

		// Sort
		for( TMap<FName,wxTreeItemId>::TIterator It(ExpressionCategoryMap); It; ++It )
		{
			MaterialExpressionTree->SortChildren(It.Value());
		}
		MaterialExpressionTree->SortChildren(RootItem);

		MaterialExpressionTree->ExpandAll();
		MaterialExpressionTree->Thaw();

		Sort<USE_COMPARE_CONSTREF(FExpressionListEntry,NewMaterialEditor)>( &ExpressionList(0), ExpressionList.Num() );
	}

	void OnFilterChanged( wxCommandEvent& In )
	{
		FString FilterString = In.GetString().c_str();
		if( FilterString.Len() > 0 )
		{
			MaterialExpressionTree->Show(FALSE);
			MaterialExpressionFilterList->Show(TRUE);

			MaterialExpressionFilterList->Freeze();
			MaterialExpressionFilterList->ClearAll();
			MaterialExpressionFilterList->InsertColumn( 0, TEXT(""), wxLIST_FORMAT_CENTRE, 250 );

			INT ItemIndex = 0;
			for (INT Index = 0; Index < ExpressionList.Num(); Index++)
			{
				if (ExpressionList(Index).ExpressionName.InStr(*FilterString, FALSE, TRUE) != -1
					&& ExpressionList(Index).bAllowedType)
				{
					MaterialExpressionFilterList->InsertItem( ItemIndex, *ExpressionList(Index).ExpressionName );
					MaterialExpressionFilterList->SetItemData( ItemIndex, ExpressionList(Index).ClassIndex );
					ItemIndex++;
				}
			}		

			MaterialExpressionFilterList->Thaw();
		}
		else
		{
			MaterialExpressionTree->Show(TRUE);
			MaterialExpressionFilterList->Show(FALSE);
		}

		Sizer->Layout();
	}

	FString GetSelectedTreeString(wxTreeEvent& DragEvent)
	{
		FString Result;
		wxTreeItemId SelectedItem = DragEvent.GetItem();
		wxTreeItemData* SelectedItemData = NULL;
		if ( SelectedItem.IsOk() && (SelectedItemData=MaterialExpressionTree->GetItemData(SelectedItem))!=NULL )
		{
			Result = FString::Printf( TEXT("%i"), static_cast<FExpressionTreeData*>(SelectedItemData)->GetIndex() );
		}
		return Result;
	}

	FString GetSelectedListString()
	{
		FString Result;
		const long ItemIndex = MaterialExpressionFilterList->GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
		if ( ItemIndex != -1 )
		{
			Result = FString::Printf( TEXT("%i"), MaterialExpressionFilterList->GetItemData(ItemIndex) );
		}
		return Result;
	}

	DECLARE_EVENT_TABLE()

private:
	wxTreeCtrl* MaterialExpressionTree;
	wxListCtrl* MaterialExpressionFilterList;
	WxSearchControl* FilterSearchCtrl;
	TArray<FExpressionListEntry> ExpressionList;
	wxBoxSizer* Sizer;
};

BEGIN_EVENT_TABLE( WxMaterialExpressionList, wxPanel )
	EVT_TEXT(ID_MATERIALEDITOR_MATERIALEXPRESSIONLIST_SEARCHBOX,WxMaterialExpressionList::OnFilterChanged)
END_EVENT_TABLE()


class FFunctionTreeData : public wxTreeItemData
{
	INT Index;
public:
	FFunctionTreeData( INT InIndex )
		:	Index(InIndex)
	{}
	INT GetIndex()
	{
		return Index;
	}
};

/** Used in the material function library to store function information for the library tree. */
struct FFunctionListEntry
{
	FFunctionListEntry( const FString& InFunctionPathName, const FString& InToolTip, INT InClassIndex )
	:	FunctionPathName(InFunctionPathName), ToolTip(InToolTip), ClassIndex(InClassIndex)
	{}

	FString FunctionPathName;
	FString ToolTip;
	INT ClassIndex;
};

IMPLEMENT_COMPARE_CONSTREF( FFunctionListEntry, NewMaterialEditor, { return appStricmp(*A.FunctionPathName,*B.FunctionPathName); } );

/** Window that displays the categorized function library. */
class WxMaterialFunctionLibraryList : public wxPanel
{
public:
	WxMaterialFunctionLibraryList(wxWindow* InParent)
		: wxPanel( InParent )
	{
		// Search/Filter box
		FilterSearchCtrl = new WxSearchControl;
		FilterSearchCtrl->Create(this, ID_MATERIALEDITOR_MATERIALFUNCTIONLIST_SEARCHBOX);

		// Tree
		MaterialFunctionTree = new wxTreeCtrl( this, ID_MATERIALEDITOR_MATERIALFUNCTIONTREE, wxDefaultPosition, wxDefaultSize, wxTR_HIDE_ROOT|wxTR_SINGLE );
		MaterialFunctionFilterList = new wxListCtrl( this, ID_MATERIALEDITOR_MATERIALFUNCTIONLIST, wxDefaultPosition, wxDefaultSize, wxLC_REPORT|wxLC_NO_HEADER|wxLC_SINGLE_SEL );

		// Hide the list unless we have a filter.
		MaterialFunctionFilterList->Show(FALSE);

		Sizer = new wxBoxSizer(wxVERTICAL);
		Sizer->Add( FilterSearchCtrl, 0, wxGROW|wxALL, 4 );
		Sizer->Add( MaterialFunctionTree, 1, wxGROW|wxALL, 4 );
		Sizer->Add( MaterialFunctionFilterList, 1, wxGROW|wxALL, 4 );
		SetSizer( Sizer );
		SetAutoLayout(true);
	}

	void PopulateFunctionTree(WxMaterialEditor* MaterialEditor)
	{
		MaterialFunctionTree->Freeze();
		MaterialFunctionTree->DeleteAllItems();
		FunctionList.Empty();

		TMap<FString,wxTreeItemId> FunctionCategoryMap;

		check( WxMaterialEditor::bMaterialFunctionLibraryInitialized );

		// not shown so doesn't need localization
		wxTreeItemId RootItem = MaterialFunctionTree->AddRoot(TEXT("Functions"));

		INT ListIndex = 0;
		for (INT CategoryIndex = 0; CategoryIndex < MaterialEditor->CategorizedFunctionLibrary.Num(); CategoryIndex++)
		{
			const FCategorizedMaterialFunctions& CurrentCategory = MaterialEditor->CategorizedFunctionLibrary(CategoryIndex);

			for (INT FunctionIndex = 0; FunctionIndex < CurrentCategory.FunctionInfos.Num(); FunctionIndex++)
			{
				const FFunctionEntryInfo& FunctionEntry = CurrentCategory.FunctionInfos(FunctionIndex);
				new(FunctionList) FFunctionListEntry(FunctionEntry.Path, FunctionEntry.ToolTip, ListIndex);

				wxTreeItemId* ExistingItem = FunctionCategoryMap.Find(CurrentCategory.CategoryName);
				if (!ExistingItem)
				{
					// Add an item for the category if there isn't already one
					ExistingItem = &FunctionCategoryMap.Set(CurrentCategory.CategoryName, MaterialFunctionTree->AppendItem(RootItem, *CurrentCategory.CategoryName));
					MaterialFunctionTree->SetItemBold(*ExistingItem);
				}

				FString FunctionName = FunctionEntry.Path;
				INT PeriodIndex = FunctionEntry.Path.InStr(TEXT("."), TRUE);

				// Extract the object name from the path
				if (PeriodIndex != INDEX_NONE)
				{
					FunctionName = FunctionEntry.Path.Right(FunctionEntry.Path.Len() - PeriodIndex - 1);
				}

				// Add an item for this function inside the category
				wxTreeItemId NewItem = MaterialFunctionTree->AppendItem(*ExistingItem, *FunctionName, -1, -1, new FFunctionTreeData(ListIndex));

				ListIndex++;
			}
		}

		// Sort
		for( TMap<FString,wxTreeItemId>::TIterator It(FunctionCategoryMap); It; ++It )
		{
			MaterialFunctionTree->SortChildren(It.Value());
		}
		MaterialFunctionTree->SortChildren(RootItem);

		MaterialFunctionTree->ExpandAll();
		MaterialFunctionTree->Thaw();

		Sort<USE_COMPARE_CONSTREF(FFunctionListEntry,NewMaterialEditor)>( &FunctionList(0), FunctionList.Num() );
	}

	/** Filters the list with the new filter string. */
	void OnFilterChanged( wxCommandEvent& In )
	{
		FString FilterString = In.GetString().c_str();
		if( FilterString.Len() > 0 )
		{
			MaterialFunctionTree->Show(FALSE);
			MaterialFunctionFilterList->Show(TRUE);

			MaterialFunctionFilterList->Freeze();
			MaterialFunctionFilterList->ClearAll();
			MaterialFunctionFilterList->InsertColumn( 0, TEXT(""), wxLIST_FORMAT_CENTRE, 250 );

			INT ItemIndex = 0;
			for( INT Index=0; Index < FunctionList.Num(); Index++ )
			{
				if( FunctionList(Index).FunctionPathName.InStr(*FilterString, FALSE, TRUE) != -1 )
				{
					FString FunctionName = FunctionList(Index).FunctionPathName;
					INT PeriodIndex = FunctionList(Index).FunctionPathName.InStr(TEXT("."), TRUE);

					if (PeriodIndex != INDEX_NONE)
					{
						FunctionName = FunctionList(Index).FunctionPathName.Right(FunctionList(Index).FunctionPathName.Len() - PeriodIndex - 1);
					}

					MaterialFunctionFilterList->InsertItem( ItemIndex, *FunctionName );
					MaterialFunctionFilterList->SetItemData( ItemIndex, FunctionList(Index).ClassIndex );
					ItemIndex++;
				}
			}		

			MaterialFunctionFilterList->Thaw();
		}
		else
		{
			MaterialFunctionTree->Show(TRUE);
			MaterialFunctionFilterList->Show(FALSE);
		}
		Sizer->Layout();
	}

	/** Generates a string for the tree item, needed for drag and drop. */
	FString GetSelectedTreeString(wxTreeEvent& DragEvent)
	{
		FString Result;
		wxTreeItemId SelectedItem = DragEvent.GetItem();
		wxTreeItemData* SelectedItemData = NULL;
		if ( SelectedItem.IsOk() && (SelectedItemData=MaterialFunctionTree->GetItemData(SelectedItem))!=NULL )
		{
			const INT TargetIndex = static_cast<FFunctionTreeData*>(SelectedItemData)->GetIndex();
			for (INT EntryIndex = 0; EntryIndex < FunctionList.Num(); EntryIndex++)
			{
				if (FunctionList(EntryIndex).ClassIndex == TargetIndex)
				{
					// Create the string in the format "Type,PathName", which is what WxMaterialEditorDropTarget::OnDropText expects
					Result = FString(TEXT("MaterialFunction,")) + FunctionList(EntryIndex).FunctionPathName;
					break;
				}
			}
		}
		return Result;
	}

	/** Generates a string for the list item, needed for drag and drop. */
	FString GetSelectedListString()
	{
		FString Result;
		const long ItemIndex = MaterialFunctionFilterList->GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
		if ( ItemIndex != -1 )
		{
			const INT TargetIndex = MaterialFunctionFilterList->GetItemData(ItemIndex);
			for (INT EntryIndex = 0; EntryIndex < FunctionList.Num(); EntryIndex++)
			{
				if (FunctionList(EntryIndex).ClassIndex == TargetIndex)
				{
					// Create the string in the format "Type,PathName", which is what WxMaterialEditorDropTarget::OnDropText expects
					Result = FString(TEXT("MaterialFunction,")) + FunctionList(EntryIndex).FunctionPathName;
					break;
				}
			}
		}
		return Result;
	}

	/** Called when a tooltip needs to be displayed for a tree item. */
	void OnTreeItemGetToolTip( wxTreeEvent& In )
	{
		wxTreeItemId SelectedItem = In.GetItem();
		wxTreeItemData* SelectedItemData = NULL;
		if ( SelectedItem.IsOk() && (SelectedItemData=MaterialFunctionTree->GetItemData(SelectedItem))!=NULL )
		{
			const INT TargetIndex = static_cast<FFunctionTreeData*>(SelectedItemData)->GetIndex();
			for (INT EntryIndex = 0; EntryIndex < FunctionList.Num(); EntryIndex++)
			{
				if (FunctionList(EntryIndex).ClassIndex == TargetIndex)
				{
					In.SetToolTip(*FunctionList(EntryIndex).ToolTip);
					break;
				}
			}
		}
	}

	/** Called when an item is double clicked */
	void OnTreeItemActivated( wxTreeEvent& Event )
	{
		wxTreeItemId SelectedItem = Event.GetItem();
		wxTreeItemData* SelectedItemData = NULL;
		if ( SelectedItem.IsOk() && (SelectedItemData=MaterialFunctionTree->GetItemData(SelectedItem))!=NULL )
		{
			const INT TargetIndex = static_cast<FFunctionTreeData*>(SelectedItemData)->GetIndex();
			for (INT EntryIndex = 0; EntryIndex < FunctionList.Num(); EntryIndex++)
			{
				if (FunctionList(EntryIndex).ClassIndex == TargetIndex)
				{
					// Double click syncs the content browser
					UMaterialFunction* Function = LoadObject<UMaterialFunction>(NULL, *FunctionList(EntryIndex).FunctionPathName, NULL, 0, NULL);
					TArray<UObject*> ObjectsToView;
					ObjectsToView.AddItem(Function);
					GApp->EditorFrame->SyncBrowserToObjects(ObjectsToView);
					break;
				}
			}
		}
	}

	DECLARE_EVENT_TABLE()

private:
	wxTreeCtrl* MaterialFunctionTree;
	wxListCtrl* MaterialFunctionFilterList;
	WxSearchControl* FilterSearchCtrl;
	TArray<FFunctionListEntry> FunctionList;
	wxBoxSizer* Sizer;
};

BEGIN_EVENT_TABLE( WxMaterialFunctionLibraryList, wxPanel )
	EVT_TEXT(ID_MATERIALEDITOR_MATERIALFUNCTIONLIST_SEARCHBOX,WxMaterialFunctionLibraryList::OnFilterChanged)
	EVT_TREE_ITEM_GETTOOLTIP( ID_MATERIALEDITOR_MATERIALFUNCTIONTREE, WxMaterialFunctionLibraryList::OnTreeItemGetToolTip )
	EVT_TREE_ITEM_ACTIVATED( ID_MATERIALEDITOR_MATERIALFUNCTIONTREE, WxMaterialFunctionLibraryList::OnTreeItemActivated )
END_EVENT_TABLE()


/**
* WxMaterialEditorSourceWindow - Source code display window
*/

struct FSourceHighlightRange
{
	FSourceHighlightRange(INT InBegin, INT InEnd)
	:	Begin(InBegin)
	,	End(InEnd)
	{}
	INT Begin;
	INT End;
};

class WxMaterialEditorSourceWindow : public wxPanel
{
	wxRichTextCtrl* RichTextCtrl;
	UMaterial* Material;
	UBOOL bNeedsUpdate;
	UBOOL bNeedsRedraw;

	FString Source;
	TMap<FMaterialExpressionKey,INT> ExpressionCodeMap[MP_MAX];
	TMap<INT, TArray<FSourceHighlightRange> > HighlightRangeMap;
	TArray<FSourceHighlightRange>* CurrentHighlightRangeArray;

public:

	UMaterialExpression* SelectedExpression;

	WxMaterialEditorSourceWindow(wxWindow* InParent, UMaterial* InMaterial)
	:	wxPanel( InParent )
	,	SelectedExpression(NULL)
	,	Material(InMaterial)
	,	bNeedsUpdate(FALSE)
	,	bNeedsRedraw(FALSE)
	,	CurrentHighlightRangeArray(NULL)
	{
		// Rich Text control
		RichTextCtrl = new wxRichTextCtrl( this, ID_MATERIALEDITOR_VIEWSOURCE_RICHTEXT, wxEmptyString, wxDefaultPosition,wxDefaultSize, wxRE_MULTILINE|wxRE_READONLY );

		wxBoxSizer* Sizer = new wxBoxSizer(wxVERTICAL);
		Sizer->Add( RichTextCtrl, 1, wxGROW|wxALL, 4 );
		SetSizer( Sizer );
		SetAutoLayout(true);
	}

#define MARKTAG TEXT("/*MARK_")
#define MARKTAGLEN 7

	void RefreshWindow(UBOOL bForce=FALSE)
	{
		// Don't do anything if the source window isn't visible.
		if( !bForce && !IsShownOnScreen() )
		{
			bNeedsUpdate = TRUE;
			return;
		}

		RegenerateSource(bForce);
	}

	void Redraw(UBOOL bForce=FALSE)
	{
		if( !bForce && !IsShownOnScreen() )
		{
			bNeedsRedraw = TRUE;
			return;
		}

		bNeedsRedraw = FALSE;

		RichTextCtrl->Freeze();
		RichTextCtrl->BeginSuppressUndo();

		// reset all to normal
		wxTextAttr NormalStyle;
		NormalStyle.SetTextColour(wxColour(0,0,0));

		// remove previous highlighting (this is much faster than setting the entire text)
		if( CurrentHighlightRangeArray )
		{
			for( INT i=0;i<CurrentHighlightRangeArray->Num();i++ )
			{
				FSourceHighlightRange& Range = (*CurrentHighlightRangeArray)(i);
				RichTextCtrl->SetStyle(Range.Begin, Range.End+1, NormalStyle);
			}
		}

		// Highlight the last-selected expression
		INT ShowPositionStart = MAXINT;
		INT ShowPositionEnd = -1;
		if( SelectedExpression )
		{
			wxTextAttr HighlightStyle;
			HighlightStyle.SetTextColour(wxColour(255,0,0));

			for (INT PropertyIndex = 0; PropertyIndex < MP_MAX; PropertyIndex++)
			{
				INT* SelectedCodeIndex = ExpressionCodeMap[PropertyIndex].Find(FMaterialExpressionKey(SelectedExpression, 0));
				if( SelectedCodeIndex )
				{
					CurrentHighlightRangeArray = HighlightRangeMap.Find(*SelectedCodeIndex);
					if( CurrentHighlightRangeArray )
					{
						for( INT i=0;i<CurrentHighlightRangeArray->Num();i++ )
						{
							FSourceHighlightRange& Range = (*CurrentHighlightRangeArray)(i);
							RichTextCtrl->SetStyle(Range.Begin, Range.End+1, HighlightStyle);

							ShowPositionStart = Min(Range.Begin, ShowPositionStart);
							ShowPositionEnd = Max(Range.End+1, ShowPositionEnd);
						}
					}
					break;
				}
			}
		}

		RichTextCtrl->EndSuppressUndo();
		RichTextCtrl->Thaw();

		// Scroll the control if necessary to show the highlight regions
		if( ShowPositionStart != MAXINT )
		{
			if( !RichTextCtrl->IsPositionVisible(ShowPositionEnd) )
			{
				RichTextCtrl->ShowPosition(ShowPositionEnd);
			}
			if( !RichTextCtrl->IsPositionVisible(ShowPositionStart) )
			{
				RichTextCtrl->ShowPosition(ShowPositionStart);
			}
		}
	}

private:
	void OnShow(wxShowEvent& Event)
	{
		if( Event.GetShow() )
		{
			if( bNeedsUpdate )
			{
				RegenerateSource();
			}
			if( bNeedsRedraw )
			{
				Redraw();
			}
		}
	}

	// EVT_SHOW isn't reliable for docking windows, so we will use Paint instead.
	void OnPaint(wxPaintEvent& Event)
	{
		wxPaintDC dc(this);

		if( bNeedsUpdate )
		{
			RegenerateSource();
		}
		if( bNeedsRedraw )
		{
			Redraw();
		}
	}

	void RegenerateSource(UBOOL bForce=FALSE)
	{
		bNeedsUpdate = FALSE;

		Source = TEXT("");
		for (INT PropertyIndex = 0; PropertyIndex < MP_MAX; PropertyIndex++)
		{
			ExpressionCodeMap[PropertyIndex].Empty();
		}
		HighlightRangeMap.Empty();
		CurrentHighlightRangeArray = NULL;

		FString MarkupSource;
		if( Material->GetMaterialResource()->GetMaterialExpressionSource( MarkupSource, ExpressionCodeMap, Material->GetQualityLevel()) )
		{
			// Remove line-feeds and leave just CRs so the character counts match the selection ranges.
			MarkupSource.ReplaceInline(TEXT("\n"), TEXT(""));

			// Extract highlight ranges from markup tags

			// Make a copy so we can insert null terminators.
			TCHAR* MarkupSourceCopy = new TCHAR[MarkupSource.Len()+1];
			appStrcpy(MarkupSourceCopy, MarkupSource.Len()+1, *MarkupSource);

			TCHAR* Ptr = MarkupSourceCopy;
			while( Ptr && *Ptr != '\0' )
			{
				TCHAR* NextTag = appStrstr( Ptr, MARKTAG );
				if( !NextTag )
				{
					// No more tags, so we're done!
					Source += Ptr;
					break;
				}

				// Copy the text up to the tag.
				*NextTag = '\0';
				Source += Ptr;

				// Advance past the markup tag to see what type it is (beginning or end)
				NextTag += MARKTAGLEN;
				INT TagNumber = appAtoi(NextTag+1);
				switch(*NextTag)
				{
				case 'B':
					{
						// begin tag
						TArray<FSourceHighlightRange>* RangeArray = HighlightRangeMap.Find(TagNumber);
						if( !RangeArray )
						{
							RangeArray = &HighlightRangeMap.Set(TagNumber,TArray<FSourceHighlightRange>() );
						}
						new(*RangeArray) FSourceHighlightRange(Source.Len(),-1);
					}
					break;
				case 'E':
					{
						// end tag
						TArray<FSourceHighlightRange>* RangeArray = HighlightRangeMap.Find(TagNumber);
						check(RangeArray);
						check(RangeArray->Num() > 0);
						(*RangeArray)(RangeArray->Num()-1).End = Source.Len();
					}
					break;
				default:
					appErrorf(TEXT("Unexpected character in material source markup tag"));
				}
				Ptr = appStrstr(NextTag, TEXT("*/")) + 2;
			}

			delete[] MarkupSourceCopy;
		}
		RichTextCtrl->Freeze();
		RichTextCtrl->BeginSuppressUndo();
		RichTextCtrl->Clear();
		RichTextCtrl->WriteText(*Source);
		RichTextCtrl->EndSuppressUndo();
		RichTextCtrl->Thaw();

		Redraw(bForce);
	}

	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE( WxMaterialEditorSourceWindow, wxPanel )
	EVT_SHOW(WxMaterialEditorSourceWindow::OnShow)
	EVT_PAINT(WxMaterialEditorSourceWindow::OnPaint)
END_EVENT_TABLE()

/** Constructor used when opening the material editor for a material. */
WxMaterialEditor::WxMaterialEditor(wxWindow* InParent, wxWindowID InID, UMaterial* InMaterial)
	:	WxMaterialEditorBase( InParent, InID, InMaterial )
	,	FDockingParent(this)
	,	ConnObj( NULL )
	,	ConnType( LOC_INPUT )
	,	ConnIndex( 0 )
	,	MarkedObject( NULL )
	,	MarkedConnType( LOC_INPUT )
	,	MarkedConnIndex( 0 )
	,	OriginalMaterial( InMaterial )
	,	EditMaterialFunction( NULL )
	,	ToolBar( NULL )
	,	MaterialExpressionList( NULL )
	,	MaterialFunctionLibraryList( NULL )
	,	SourceWindow(NULL)
	,	EditorOptions( NULL )
	,	bMaterialDirty( FALSE )
	,	bAlwaysRefreshAllPreviews( FALSE )
	,	bUseUnsortedMenus( FALSE )
	,	bHideUnusedConnectors( FALSE )
	,	bShowStats ( TRUE )
	, 	ScopedTransaction ( NULL )
	,	PreviewExpression( NULL )
	,	ExpressionPreviewMaterial( NULL )
	,	SelectedSearchResult(0)
	,	DblClickObject(NULL)
	,	DblClickConnType(INDEX_NONE)
	,	DblClickConnIndex(INDEX_NONE)
{
	check(!ActiveMaterialEditors.Find(InMaterial->GetPathName()));
	ActiveMaterialEditors.Set(InMaterial->GetPathName(), this);

	// Create a copy of the material for preview usage (duplicating to a different class than original!)
	// Propagate all object flags except for RF_Standalone, otherwise the preview material won't GC once
	// the material editor releases the reference.
	Material = (UMaterial*)UObject::StaticDuplicateObject(OriginalMaterial, OriginalMaterial, UObject::GetTransientPackage(), NULL, ~RF_Standalone, UPreviewMaterial::StaticClass()); 
	
	// Set the material editor window title to include the material being edited.
	SetTitle( *FString::Printf( LocalizeSecure(LocalizeUnrealEd("MaterialEditorCaption_F"), *OriginalMaterial->GetPathName()) ) );

#if WITH_EDITOR
	FMobileEmulationMaterialManager::GetManager()->UpdateMaterialInterface(InMaterial, FALSE, FALSE);
	FMobileEmulationMaterialManager::GetManager()->UpdateMaterialInterface(Material, FALSE, FALSE);
#endif

	Initialize(Material);
}

/** Constructor used when opening the material editor for a function. */
WxMaterialEditor::WxMaterialEditor(wxWindow* InParent, wxWindowID InID, UMaterialFunction* InMaterialFunction)
	:	WxMaterialEditorBase( InParent, InID, NULL )
	,	FDockingParent(this)
	,	ConnObj( NULL )
	,	ConnType( LOC_INPUT )
	,	ConnIndex( 0 )
	,	MarkedObject( NULL )
	,	MarkedConnType( LOC_INPUT )
	,	MarkedConnIndex( 0 )
	,	OriginalMaterial( NULL )
	,	EditMaterialFunction( NULL )
	,	ToolBar( NULL )
	,	MaterialExpressionList( NULL )
	,	MaterialFunctionLibraryList( NULL )
	,	SourceWindow(NULL)
	,	EditorOptions( NULL )
	,	bMaterialDirty( FALSE )
	,	bAlwaysRefreshAllPreviews( FALSE )
	,	bUseUnsortedMenus( FALSE )
	,	bHideUnusedConnectors( FALSE )
	,	bShowStats ( TRUE )
	, 	ScopedTransaction ( NULL )
	,	PreviewExpression( NULL )
	,	ExpressionPreviewMaterial( NULL )
	,	SelectedSearchResult(0)
	,	DblClickObject(NULL)
	,	DblClickConnType(INDEX_NONE)
	,	DblClickConnIndex(INDEX_NONE)
{
	check(!ActiveMaterialEditors.Find(InMaterialFunction->GetPathName()));
	ActiveMaterialEditors.Set(InMaterialFunction->GetPathName(), this);

	// Create a temporary material to preview the material function
	Material = (UMaterial*)UObject::StaticConstructObject(UMaterial::StaticClass()); 
	{
		FArchive DummyArchive;
		// Hack: serialize the new material with an archive that does nothing so that its material resources are created
		Material->Serialize(DummyArchive);
	}
	Material->LightingModel = MLM_Unlit;

	// Propagate all object flags except for RF_Standalone, otherwise the preview material function won't GC once
	// the material editor releases the reference.
	EditMaterialFunction = (UMaterialFunction*)UObject::StaticDuplicateObject(InMaterialFunction, InMaterialFunction, UObject::GetTransientPackage(), NULL, ~RF_Standalone, UMaterialFunction::StaticClass()); 
	EditMaterialFunction->ParentFunction = InMaterialFunction;

	OriginalMaterial = Material;

	// Set the material editor window title to include the material being edited.
	SetTitle( *FString::Printf( LocalizeSecure(LocalizeUnrealEd("MaterialEditorCaption_F"), *(FString(TEXT("Function ")) + EditMaterialFunction->ParentFunction->GetPathName())) ) );
	
	Initialize(Material);

	Material->Expressions = EditMaterialFunction->FunctionExpressions;
	Material->EditorComments = EditMaterialFunction->FunctionEditorComments;

	if (Material->Expressions.Num() == 0)
	{
		// If this is an empty functions, create an output by default and start previewing it
		UMaterialExpression* Expression = CreateNewMaterialExpression(UMaterialExpressionFunctionOutput::StaticClass(), FALSE, FALSE, TRUE, FIntPoint(200, 300));
		SetPreviewExpression(Expression);
	}
	else
	{
		UBOOL bSetPreviewExpression = FALSE;
		UMaterialExpressionFunctionOutput* FirstOutput = NULL;
		for (INT ExpressionIndex = Material->Expressions.Num() - 1; ExpressionIndex >= 0; ExpressionIndex--)
		{
			UMaterialExpression* Expression = Material->Expressions(ExpressionIndex);

			// Setup the expression to be used with the preview material instead of the function
			Expression->Function = NULL;
			Expression->Material = Material;

			UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(Expression);
			if (FunctionOutput)
			{
				FirstOutput = FunctionOutput;
				if (FunctionOutput->bLastPreviewed)
				{
					bSetPreviewExpression = TRUE;

					// Preview the last output previewed
					SetPreviewExpression(FunctionOutput);
				}
			}
		}

		if (!bSetPreviewExpression && FirstOutput)
		{
			SetPreviewExpression(FirstOutput);
		}
	}
}

// Create a material editor drop target.
/** Drop target for the material editor's linked object viewport. */
class WxMaterialEditorDropTarget : public wxTextDropTarget
{
public:
	WxMaterialEditorDropTarget(WxMaterialEditor* InMaterialEditor)
		: MaterialEditor( InMaterialEditor )
		, HasData( FALSE )
	{}
	virtual bool OnDropText(wxCoord x, wxCoord y, const wxString& text)
	{
		const INT LocX = (x - MaterialEditor->LinkedObjVC->Origin2D.X)/MaterialEditor->LinkedObjVC->Zoom2D;
		const INT LocY = (y - MaterialEditor->LinkedObjVC->Origin2D.Y)/MaterialEditor->LinkedObjVC->Zoom2D;
		long MaterialExpressionIndex;
		const UBOOL bIsExpression = text.ToLong(&MaterialExpressionIndex) && (MaterialExpressionIndex > -1) && (MaterialExpressionIndex < MaterialEditor->MaterialExpressionClasses.Num());
		
		FString DropText = text.c_str();
		FString TypeName;
		FString PathName;

		// Format is "Type,ObjectPath"
		const INT ColonIndex = DropText.InStr(TEXT(","));

		if (ColonIndex == INDEX_NONE)
		{
			PathName = DropText;
			TypeName = DropText;
		}
		else
		{
			TypeName = DropText.Left(ColonIndex);
			PathName = DropText.Right(DropText.Len() - ColonIndex - 1);
		}

		const UBOOL bIsTexture = TypeName.InStr(*UTexture::StaticClass()->GetName()) != INDEX_NONE;
		const UBOOL bIsFunction = TypeName.InStr(*UMaterialFunction::StaticClass()->GetName()) != INDEX_NONE;

		if ( bIsExpression )
		{
			UClass* NewExpressionClass = MaterialEditor->MaterialExpressionClasses(MaterialExpressionIndex);
			MaterialEditor->CreateNewMaterialExpression( NewExpressionClass, FALSE, TRUE, TRUE, FIntPoint(LocX, LocY) );
		}
		else if ( bIsTexture )
		{
			MaterialEditor->CreateNewMaterialExpression( UMaterialExpressionTextureSample::StaticClass(), FALSE, TRUE, TRUE, FIntPoint(LocX, LocY) );
		}
		else if (bIsFunction)
		{
			UMaterialExpressionMaterialFunctionCall* FunctionNode = CastChecked<UMaterialExpressionMaterialFunctionCall>(
				MaterialEditor->CreateNewMaterialExpression(UMaterialExpressionMaterialFunctionCall::StaticClass(), FALSE, TRUE, FALSE, FIntPoint(LocX, LocY)));

			if (!FunctionNode->MaterialFunction)
			{
				UMaterialFunction* FoundFunction = LoadObject<UMaterialFunction>(NULL, *PathName, NULL, 0, NULL);

				if (FoundFunction)
				{
					FunctionNode->SetMaterialFunction(MaterialEditor->EditMaterialFunction, NULL, FoundFunction);
				}
			}
		}

		HasData = FALSE;
		return true;
	}
	virtual bool IsAcceptedData(IDataObject *pIDataSource) const
	{
		if ( wxTextDropTarget::IsAcceptedData(pIDataSource) )
		{
			// hook to set our datasource prior to calling GetData() from OnEnter;
			// some evil stuff here - don't try this at home!
			const_cast<WxMaterialEditorDropTarget*>(this)->SetDataSource(pIDataSource);
			return true;
		}

		return false;
	}
	virtual wxDragResult OnEnter(wxCoord x, wxCoord y, wxDragResult def)
	{
		// populate the data so we can check its type in OnDragOver
		if (GetData())
		{
			HasData = TRUE;
		}

		return def;
	}
	virtual void OnLeave()
	{
		HasData = FALSE;
	}
	virtual wxDragResult OnDragOver(wxCoord x, wxCoord y, wxDragResult def)
	{
		// check if the data has been populated by OnEnter
		if (HasData)
		{
			wxTextDataObject* Data = (wxTextDataObject*)GetDataObject();
			if (Data)
			{
				const wxString& Text = Data->GetText();
				long MaterialExpressionIndex;
				UBOOL IsExpression = Text.ToLong(&MaterialExpressionIndex) && (MaterialExpressionIndex > -1) && (MaterialExpressionIndex < MaterialEditor->MaterialExpressionClasses.Num());
				UBOOL IsTexture2D = appStrstr(Text, *UTexture::StaticClass()->GetName()) != NULL;
				UBOOL bIsFunction = appStrstr(Text, *UMaterialFunction::StaticClass()->GetName()) != NULL;

				// only textures and new expressions can be dragged and dropped into the material editor
				if (IsExpression || IsTexture2D || bIsFunction)
				{
					return def;
				}
			}
		}

		// for object types that aren't allowed to dragged and dropped into the material editor we return wxDragNone, which,
		// changes the mouse icon to reflect that the currently selected object cannot be dropped into the editor
		return wxDragNone;
	}
	WxMaterialEditor* MaterialEditor;

private:
	UBOOL HasData;
};

/** Initializes material editor state, regardless of whether a material or material function is being edited. */
void WxMaterialEditor::Initialize(UMaterial* Material)
{
	// copy material usage
	for( INT Usage=0; Usage < MATUSAGE_MAX; Usage++ )
	{
		const EMaterialUsage UsageEnum = (EMaterialUsage)Usage;
		if( OriginalMaterial->GetUsageByFlag(UsageEnum) )
		{
			UBOOL bNeedsRecompile=FALSE;
			Material->SetMaterialUsage(bNeedsRecompile,UsageEnum);
		}
	}
	// Manually copy bUsedAsSpecialEngineMaterial as it is duplicate transient to prevent accidental creation of new special engine materials
	Material->bUsedAsSpecialEngineMaterial = OriginalMaterial->bUsedAsSpecialEngineMaterial;

	// copy the flattened texture manually because it's duplicatetransient so it's NULLed when duplicating normally
	// (but we don't want it NULLed in this case)
	Material->MobileBaseTexture = OriginalMaterial->MobileBaseTexture;

	SetPreviewMaterial( Material );

	// Mark material as being the preview material to avoid unnecessary work in UMaterial::PostEditChange. This property is duplicatetransient so we don't have
	// to worry about resetting it when propagating the preview to the original material after editing.
	// Note:  The material editor must reattach the preview meshes through RefreshPreviewViewport() after calling Material::PEC()!
	Material->bIsPreviewMaterial = TRUE;

	// Assume a quality switch to make sure both slots are open
	Material->bHasQualitySwitch = TRUE;

	// Initialize the shared list of material expression classes.
	InitMaterialExpressionClasses();

	InitializeMaterialFunctionLibrary();

	// Make sure the material is the list of material expressions it references.
	InitExpressions( Material );

	// Set default size
	SetSize(1152,864);

	// force preload because UCustomPropertyItemBindings::GetCustomInputProxy() is not working
	UClass* UPropertyInput_DynamicComboClass = UObject::StaticLoadClass( UPropertyInput_DynamicCombo::StaticClass(), NULL, TEXT("UnrealEd.PropertyInput_DynamicCombo"), NULL, LOAD_None, NULL );

	// Create property window.
	PropertyWindow = new WxPropertyWindowHost;
	PropertyWindow->Create( this, this );
	PropertyWindow->SetFlags(EPropertyWindowFlags::SupportsCustomControls, TRUE);
	
	WxMaterialEditorDropTarget* MaterialEditorDropTarget = new WxMaterialEditorDropTarget(this);

	// Create linked-object tree window.
	WxLinkedObjVCHolder* TreeWin = new WxLinkedObjVCHolder( this, -1, this );
	TreeWin->SetDropTarget( MaterialEditorDropTarget );
	LinkedObjVC = TreeWin->LinkedObjVC;
	LinkedObjVC->ToolTipDelayMS = 333;
	LinkedObjVC->SetRedrawInTick( FALSE );

	// Create material expression list.
	MaterialExpressionList = new WxMaterialExpressionList( this );
	MaterialExpressionList->PopulateExpressionsTree(EditMaterialFunction != NULL);

	MaterialFunctionLibraryList = new WxMaterialFunctionLibraryList( this );
	MaterialFunctionLibraryList->PopulateFunctionTree(this);

	// Create source window
	SourceWindow = new WxMaterialEditorSourceWindow( this, Material );

	// Add docking windows.
	{
		SetDockHostSize(FDockingParent::DH_Bottom, 150);
		SetDockHostSize(FDockingParent::DH_Right, 150);

		AddDockingWindow( TreeWin, FDockingParent::DH_None, NULL );

		AddDockingWindow(PropertyWindow, FDockingParent::DH_Bottom, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("PropertiesCaption_F"), *OriginalMaterial->GetPathName())), *LocalizeUnrealEd("Properties"));
		AddDockingWindow(SourceWindow, FDockingParent::DH_Bottom, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("SourceCaption_F"), *OriginalMaterial->GetPathName())), *LocalizeUnrealEd("Source"));

		// Source window is hidden by default.
		ShowDockingWindow(SourceWindow, FALSE);

		AddDockingWindow((wxWindow*)PreviewWin, FDockingParent::DH_Left, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("PreviewCaption_F"), *OriginalMaterial->GetPathName())), *LocalizeUnrealEd("Preview"));

		AddDockingWindow(MaterialExpressionList, FDockingParent::DH_Right, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("MaterialExpressionListCaption_F"), *OriginalMaterial->GetPathName())), *LocalizeUnrealEd("MaterialExpressions"), wxSize(285, 240));
		AddDockingWindow(MaterialFunctionLibraryList, FDockingParent::DH_Right, *LocalizeUnrealEd("MaterialFunctionListCaption"), *LocalizeUnrealEd("MaterialFunctions"), wxSize(285, 240));

		// Try to load a existing layout for the docking windows.
		LoadDockingLayout();
	}
	
	// If the Source window is visible, we need to update it now, otherwise defer til it's shown.
	FDockingParent::FDockWindowState SourceWindowState;
	if( GetDockingWindowState(SourceWindow, SourceWindowState) && SourceWindowState.bIsVisible )
	{
		SourceWindow->RefreshWindow(TRUE);
	}
	else
	{
		SourceWindow->RefreshWindow(FALSE);
	}

	wxMenuBar* MenuBar = new wxMenuBar();
	AppendWindowMenu(MenuBar);
	SetMenuBar(MenuBar);

	ToolBar = new WxMaterialEditorToolBar( this, -1 );
	SetToolBar( ToolBar );
	LinkedObjVC->Origin2D = FIntPoint(-Material->EditorX,-Material->EditorY);

	BackgroundTexture = LoadObject<UTexture2D>(NULL, TEXT("EditorMaterials.MaterialsBackground"), NULL, LOAD_None, NULL);

	// Load editor settings from disk.
	LoadEditorSettings();

	// Set the preview mesh for the material.  This call must occur after the toolbar is initialized.
	if ( !SetPreviewMesh( *Material->PreviewMesh ) )
	{
		// The material preview mesh couldn't be found or isn't loaded.  Default to the one of the primitive types.
		SetPrimitivePreview();
	}

	// Initialize the contents of the property window.
	UpdatePropertyWindow();

	// Initialize the material input list.
	MaterialInputs.AddItem( FMaterialInputInfo( TEXT("Diffuse"), &Material->DiffuseColor, MP_DiffuseColor ) );
	MaterialInputs.AddItem( FMaterialInputInfo( TEXT("DiffusePower"), &Material->DiffusePower, MP_DiffusePower ) );
	MaterialInputs.AddItem( FMaterialInputInfo( TEXT("Emissive"), &Material->EmissiveColor, MP_EmissiveColor ) );
	MaterialInputs.AddItem( FMaterialInputInfo( TEXT("Specular"), &Material->SpecularColor, MP_SpecularColor ) );
	MaterialInputs.AddItem( FMaterialInputInfo( TEXT("SpecularPower"), &Material->SpecularPower, MP_SpecularPower ) );
	MaterialInputs.AddItem( FMaterialInputInfo( TEXT("Opacity"), &Material->Opacity, MP_Opacity ) );
	MaterialInputs.AddItem( FMaterialInputInfo( TEXT("OpacityMask"), &Material->OpacityMask, MP_OpacityMask ) );
	MaterialInputs.AddItem( FMaterialInputInfo( TEXT("Distortion"), &Material->Distortion, MP_Distortion ) );
	MaterialInputs.AddItem( FMaterialInputInfo( TEXT("TransmissionMask"), &Material->TwoSidedLightingMask, MP_TwoSidedLightingMask ) );
	MaterialInputs.AddItem( FMaterialInputInfo( TEXT("TransmissionColor"), &Material->TwoSidedLightingColor, MP_TwoSidedLightingMask ) );
	MaterialInputs.AddItem( FMaterialInputInfo( TEXT("Normal"), &Material->Normal, MP_Normal ) );
	MaterialInputs.AddItem( FMaterialInputInfo( TEXT("CustomLighting"), &Material->CustomLighting, MP_CustomLighting ) );
	MaterialInputs.AddItem( FMaterialInputInfo( TEXT("CustomLightingDiffuse"), &Material->CustomSkylightDiffuse, MP_CustomLightingDiffuse ) );
	MaterialInputs.AddItem( FMaterialInputInfo( TEXT("AnisotropicDirection"), &Material->AnisotropicDirection, MP_AnisotropicDirection ) );
	MaterialInputs.AddItem( FMaterialInputInfo( TEXT("WorldPositionOffset"), &Material->WorldPositionOffset, MP_WorldPositionOffset ) );
	MaterialInputs.AddItem( FMaterialInputInfo( TEXT("WorldDisplacement"), &Material->WorldDisplacement, MP_WorldDisplacement ) );
	MaterialInputs.AddItem( FMaterialInputInfo( TEXT("TessellationMultiplier"), &Material->TessellationMultiplier, MP_TessellationMultiplier ) );
	MaterialInputs.AddItem( FMaterialInputInfo( TEXT("SubsurfaceInscatteringColor"), &Material->SubsurfaceInscatteringColor, MP_SubsurfaceInscatteringColor ) );
	MaterialInputs.AddItem( FMaterialInputInfo( TEXT("SubsurfaceAbsorptionColor"), &Material->SubsurfaceAbsorptionColor, MP_SubsurfaceAbsorptionColor ) );
	MaterialInputs.AddItem( FMaterialInputInfo( TEXT("SubsurfaceScatteringRadius"), &Material->SubsurfaceScatteringRadius, MP_SubsurfaceScatteringRadius ) );
	
	// Initialize expression previews.
	ForceRefreshExpressionPreviews();

	GCallbackEvent->Register(CALLBACK_PreEditorClose, this);
	GCallbackEvent->Register(CALLBACK_Undo,this);
}

WxMaterialEditor::~WxMaterialEditor()
{
	if (EditMaterialFunction)
	{
		ActiveMaterialEditors.Remove(EditMaterialFunction->ParentFunction->GetPathName());
	}
	else
	{
		ActiveMaterialEditors.Remove(OriginalMaterial->GetPathName());
	}

	// make sure rendering thread is done with the expressions, since they are about to be deleted
	FlushRenderingCommands();

	// Save editor settings to disk.
	SaveEditorSettings();

	check( !ScopedTransaction );

	// Null out the expression preview material so they can be GC'ed
	ExpressionPreviewMaterial = NULL;
	PreviewExpression = NULL;

#if WITH_MANAGED_CODE
	UnBindColorPickers(this);
#endif
}

/** Updates CategorizedFunctionLibrary from functions in memory and then rebuilds the function library window. */
void WxMaterialEditor::RebuildMaterialFunctionLibrary()
{
	InitializeMaterialFunctionLibrary();
	MaterialFunctionLibraryList->PopulateFunctionTree(this);
}

/**
 * Load editor settings from disk (docking state, window pos/size, option state, etc).
 */
void WxMaterialEditor::LoadEditorSettings()
{
	// Load the desired window position from .ini file.
	FWindowUtil::LoadPosSize(EditMaterialFunction ? TEXT("MaterialFunctionEditor") : TEXT("MaterialEditor"), this, 256, 256, 1152, 864);

	EditorOptions				= ConstructObject<UMaterialEditorOptions>( UMaterialEditorOptions::StaticClass() );
	bShowGrid					= EditorOptions->bShowGrid;
	bShowBackground				= EditorOptions->bShowBackground;
	bHideUnusedConnectors		= EditorOptions->bHideUnusedConnectors;
	bAlwaysRefreshAllPreviews	= EditorOptions->bAlwaysRefreshAllPreviews;
	bUseUnsortedMenus			= EditorOptions->bUseUnsortedMenus;

	if ( ToolBar )
	{
		ToolBar->SetShowGrid( bShowGrid );
		ToolBar->SetShowBackground( bShowBackground );
		ToolBar->SetHideConnectors( bHideUnusedConnectors );
		ToolBar->SetAlwaysRefreshAllPreviews( bAlwaysRefreshAllPreviews );
		ToolBar->SetRealtimeMaterialPreview( EditorOptions->bRealtimeMaterialViewport );
		ToolBar->SetRealtimeExpressionPreview( EditorOptions->bRealtimeExpressionViewport );
	}
	if ( PreviewVC )
	{
		PreviewVC->SetShowGrid( bShowGrid );
		PreviewVC->SetRealtime( EditorOptions->bRealtimeMaterialViewport || GEditor->AccessUserSettings().bStartInRealtimeMode );

		// Load the preview scene
		if ( PreviewVC->PreviewScene )
		{
			PreviewVC->PreviewScene->LoadSettings(TEXT("MaterialEditor"));
		}
	}
	if ( LinkedObjVC )
	{
		LinkedObjVC->SetRealtime( EditorOptions->bRealtimeExpressionViewport );
	}

	// Primitive type
	INT PrimType;
	if(GConfig->GetInt(TEXT("MaterialEditor"), TEXT("PrimType"), PrimType, GEditorUserSettingsIni))
	{
		PreviewPrimType = (EThumbnailPrimType)PrimType;
	}
	else
	{
		PreviewPrimType = TPT_Sphere;
	}
}

/**
 * Saves editor settings to disk (docking state, window pos/size, option state, etc).
 */
void WxMaterialEditor::SaveEditorSettings()
{
	// Save docking layout.
	SaveDockingLayout();

	// Save the preview scene
	check( PreviewVC );
	check( PreviewVC->PreviewScene );
	PreviewVC->PreviewScene->SaveSettings(TEXT("MaterialEditor"));

	// Save window position/size.
	FWindowUtil::SavePosSize(EditMaterialFunction ? TEXT("MaterialFunctionEditor") : TEXT("MaterialEditor"), this);

	if ( EditorOptions )
	{
		check( LinkedObjVC );
		EditorOptions->bShowGrid					= bShowGrid;
		EditorOptions->bShowBackground				= bShowBackground;
		EditorOptions->bHideUnusedConnectors		= bHideUnusedConnectors;
		EditorOptions->bAlwaysRefreshAllPreviews	= bAlwaysRefreshAllPreviews;
		EditorOptions->bRealtimeMaterialViewport	= PreviewVC->IsRealtime();
		EditorOptions->bRealtimeExpressionViewport	= LinkedObjVC->IsRealtime();
		EditorOptions->SaveConfig();
	}

	GConfig->SetInt(TEXT("MaterialEditor"), TEXT("PrimType"), PreviewPrimType, GEditorUserSettingsIni);

	//Material->PreviewCamPos = PreviewVC->ViewLocation;
	//Material->EditorPitch = PreviewVC->ViewRotation.Pitch;
	//Material->EditorYaw = PreviewVC->ViewRotation.Yaw;
}

/**
 * Called by SetPreviewMesh, allows derived types to veto the setting of a preview mesh.
 *
 * @return	TRUE if the specified mesh can be set as the preview mesh, FALSE otherwise.
 */
UBOOL WxMaterialEditor::ApproveSetPreviewMesh(UStaticMesh* InStaticMesh, USkeletalMesh* InSkeletalMesh)
{
	UBOOL bApproved = TRUE;
	// Only permit the use of a skeletal mesh if the material has bUsedWithSkeltalMesh.
	if ( InSkeletalMesh && !Material->bUsedWithSkeletalMesh )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_MaterialEditor_CantPreviewOnSkelMesh") );
		bApproved = FALSE;
	}
	return bApproved;
}

/** Refreshes the viewport containing the material expression graph. */
void WxMaterialEditor::RefreshExpressionViewport()
{
	LinkedObjVC->Viewport->Invalidate();
}

/**
 * Refreshes the preview for the specified material expression.  Does nothing if the specified expression
 * has a bRealtimePreview of FALSE.
 *
 * @param	MaterialExpression		The material expression to update.
 */
void WxMaterialEditor::RefreshExpressionPreview(UMaterialExpression* MaterialExpression, UBOOL bRecompile)
{
	if ( MaterialExpression->bRealtimePreview || MaterialExpression->bNeedToUpdatePreview )
	{
		for( INT PreviewIndex = 0 ; PreviewIndex < ExpressionPreviews.Num() ; ++PreviewIndex )
		{
			FExpressionPreview& ExpressionPreview = ExpressionPreviews(PreviewIndex);
			if( ExpressionPreview.GetExpression() == MaterialExpression )
			{
				// we need to make sure the rendering thread isn't drawing this tile
				FlushRenderingCommands();
				ExpressionPreviews.Remove( PreviewIndex );
				MaterialExpression->bNeedToUpdatePreview = FALSE;
				if (bRecompile)
				{
					UBOOL bNewlyCreated;
					GetExpressionPreview(MaterialExpression, bNewlyCreated, FALSE);
				}
				break;
			}
		}
	}
}

/**
 * Refreshes material expression previews.  Refreshes all previews if bAlwaysRefreshAllPreviews is TRUE.
 * Otherwise, refreshes only those previews that have a bRealtimePreview of TRUE.
 */
void WxMaterialEditor::RefreshExpressionPreviews()
{
	const FScopedBusyCursor BusyCursor;

	if ( bAlwaysRefreshAllPreviews )
	{
		// we need to make sure the rendering thread isn't drawing these tiles
		FlushRenderingCommands();

		// Refresh all expression previews.
		ExpressionPreviews.Empty();
	}
	else
	{
		// Only refresh expressions that are marked for realtime update.
		for ( INT ExpressionIndex = 0 ; ExpressionIndex < Material->Expressions.Num() ; ++ExpressionIndex )
		{
			UMaterialExpression* MaterialExpression = Material->Expressions( ExpressionIndex );
			RefreshExpressionPreview( MaterialExpression, FALSE );
		}
	}

	TArray<FExpressionPreview*> ExpressionPreviewsBeingCompiled;
	ExpressionPreviewsBeingCompiled.Empty(50);
	// Go through all expression previews and create new ones as needed, and maintain a list of previews that are being compiled
	for( INT ExpressionIndex = 0; ExpressionIndex < Material->Expressions.Num(); ++ExpressionIndex )
	{
		UMaterialExpression* MaterialExpression = Material->Expressions( ExpressionIndex );
		if ( !MaterialExpression->IsA(UMaterialExpressionComment::StaticClass()) )
		{
			UBOOL bNewlyCreated;
			FExpressionPreview* Preview = GetExpressionPreview( MaterialExpression, bNewlyCreated, TRUE );
			if (bNewlyCreated && Preview)
			{
				ExpressionPreviewsBeingCompiled.AddItem(Preview);
			}
		}
	}

	GShaderCompilingThreadManager->FinishDeferredCompilation();
}

/**
 * Refreshes all material expression previews, regardless of whether or not realtime previews are enabled.
 */
void WxMaterialEditor::ForceRefreshExpressionPreviews()
{
	// Initialize expression previews.
	const UBOOL bOldAlwaysRefreshAllPreviews = bAlwaysRefreshAllPreviews;
	bAlwaysRefreshAllPreviews = TRUE;
	RefreshExpressionPreviews();
	bAlwaysRefreshAllPreviews = bOldAlwaysRefreshAllPreviews;
}

/**
 * @param		InMaterialExpression	The material expression to query.
 * @param		ConnType				Type of the connection (LOC_INPUT or LOC_OUTPUT).
 * @param		ConnIndex				Index of the connection to query
 * @return								A viewport location for the connection.
 */
FIntPoint WxMaterialEditor::GetExpressionConnectionLocation(UMaterialExpression* InMaterialExpression, INT ConnType, INT ConnIndex)
{
	const FMaterialNodeDrawInfo& ExpressionDrawInfo = GetDrawInfo( InMaterialExpression );

	FIntPoint Result(0,0);
	if ( ConnType == LOC_OUTPUT ) // connectors on the right side of the material
	{
		Result.X = InMaterialExpression->MaterialExpressionEditorX + ExpressionDrawInfo.DrawWidth + LO_CONNECTOR_LENGTH;
		Result.Y = ExpressionDrawInfo.RightYs(ConnIndex);
	}
	else if ( ConnType == LOC_INPUT ) // connectors on the left side of the material
	{
		Result.X = InMaterialExpression->MaterialExpressionEditorX - LO_CONNECTOR_LENGTH,
		Result.Y = ExpressionDrawInfo.LeftYs(ConnIndex);
	}
	return Result;
}

/**
 * @param		InMaterial	The material to query.
 * @param		ConnType	Type of the connection (LOC_OUTPUT).
 * @param		ConnIndex	Index of the connection to query
 * @return					A viewport location for the connection.
 */
FIntPoint WxMaterialEditor::GetMaterialConnectionLocation(UMaterial* InMaterial, INT ConnType, INT ConnIndex)
{
	FIntPoint Result(0,0);
	if ( ConnType == LOC_OUTPUT ) // connectors on the right side of the material
	{
		Result.X = InMaterial->EditorX + MaterialDrawInfo.DrawWidth + LO_CONNECTOR_LENGTH;
		Result.Y = MaterialDrawInfo.RightYs( ConnIndex );
	}
	return Result;
}

/**
 * Returns the expression preview for the specified material expression.
 */
FExpressionPreview* WxMaterialEditor::GetExpressionPreview(UMaterialExpression* MaterialExpression, UBOOL& bNewlyCreated, UBOOL bDeferCompilation)
{
	bNewlyCreated = FALSE;
	if (MaterialExpression->bHidePreviewWindow == FALSE)
	{
		FExpressionPreview*	Preview = NULL;
		for( INT PreviewIndex = 0 ; PreviewIndex < ExpressionPreviews.Num() ; ++PreviewIndex )
		{
			FExpressionPreview& ExpressionPreview = ExpressionPreviews(PreviewIndex);
			if( ExpressionPreview.GetExpression() == MaterialExpression )
			{
				Preview = &ExpressionPreviews(PreviewIndex);
				break;
			}
		}

		if( !Preview )
		{
			bNewlyCreated = TRUE;
			Preview = new(ExpressionPreviews) FExpressionPreview(MaterialExpression);
			if (!bDeferCompilation)
			{
				Preview->bDeferCompilation = FALSE;
			}
			Preview->CacheShaders();
			if (!bDeferCompilation)
			{
				Preview->bDeferCompilation = TRUE;
			}
		}
		return Preview;
	}

	return NULL;
}

/**
* Returns the drawinfo object for the specified expression, creating it if one does not currently exist.
*/
WxMaterialEditor::FMaterialNodeDrawInfo& WxMaterialEditor::GetDrawInfo(UMaterialExpression* MaterialExpression)
{
	FMaterialNodeDrawInfo* ExpressionDrawInfo = MaterialNodeDrawInfo.Find( MaterialExpression );
	return ExpressionDrawInfo ? *ExpressionDrawInfo : MaterialNodeDrawInfo.Set( MaterialExpression, FMaterialNodeDrawInfo(MaterialExpression->MaterialExpressionEditorY) );
}

/**
 * Disconnects and removes the given expressions and comments.
 */
void WxMaterialEditor::DeleteObjects( const TArray<UMaterialExpression*>& Expressions, const TArray<UMaterialExpressionComment*>& Comments)
{
	const UBOOL bHaveExpressionsToDelete	= Expressions.Num() > 0;
	const UBOOL bHaveCommentsToDelete		= Comments.Num() > 0;
	if ( bHaveExpressionsToDelete || bHaveCommentsToDelete )
	{
		{
			FString FunctionWarningString;
			UBOOL bFirstExpression = TRUE;
			for (INT MaterialExpressionIndex = 0; MaterialExpressionIndex < Expressions.Num(); ++MaterialExpressionIndex)
			{
				UMaterialExpressionFunctionInput* FunctionInput = Cast<UMaterialExpressionFunctionInput>(Expressions(MaterialExpressionIndex));
				UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(Expressions(MaterialExpressionIndex));

				if (FunctionInput)
				{
					if (!bFirstExpression)
					{
						FunctionWarningString += TEXT(", ");
					}
					bFirstExpression = FALSE;
					FunctionWarningString += FunctionInput->InputName;
				}

				if (FunctionOutput)
				{
					if (!bFirstExpression)
					{
						FunctionWarningString += TEXT(", ");
					}
					bFirstExpression = FALSE;
					FunctionWarningString += FunctionOutput->OutputName;
				}
			}

			if (FunctionWarningString.Len() > 0)
			{
				if (appMsgf(AMT_YesNo, LocalizeSecure(LocalizeUnrealEd("Prompt_MaterialEditorDeleteFunctionInputs"), *FunctionWarningString)) == 0)
				{
					// User said don't delete
					return;
				}
			}
		}
		
		// If we are previewing an expression and the expression being previewed was deleted
		UBOOL bPreviewExpressionDeleted			= FALSE;

		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("MaterialEditorDelete")) );
			Material->Modify();

			// Whack selected expressions.
			for( INT MaterialExpressionIndex = 0 ; MaterialExpressionIndex < Expressions.Num() ; ++MaterialExpressionIndex )
			{
				UMaterialExpression* MaterialExpression = Expressions( MaterialExpressionIndex );

#if WITH_MANAGED_CODE
				UnBindColorPickers(MaterialExpression);
#endif

				if( PreviewExpression == MaterialExpression )
				{
					// The expression being previewed is also being deleted
					bPreviewExpressionDeleted = TRUE;

					//Clear out any displayed error message related to the preview, to avoid cases where 
					//	now incorrect error messages  may stick around.  They will repopulate properly the 
					//	next time the expressions are compiled.
					if (ExpressionPreviewMaterial && ExpressionPreviewMaterial->GetMaterialResource())
					{
						TArray<FString> CompileErrors;
						ExpressionPreviewMaterial->GetMaterialResource()->SetCompileErrors(CompileErrors);
					}
				}

				MaterialExpression->Modify();
				SwapLinksToExpression(MaterialExpression, NULL, Material);
				Material->Expressions.RemoveItem( MaterialExpression );
				Material->RemoveExpressionParameter(MaterialExpression);
				// Make sure the deleted expression is caught by gc
				MaterialExpression->MarkPendingKill();
			}	

			// Whack selected comments.
			for( INT CommentIndex = 0 ; CommentIndex < Comments.Num() ; ++CommentIndex )
			{
				UMaterialExpressionComment* Comment = Comments( CommentIndex );
				Comment->Modify();
				Material->EditorComments.RemoveItem( Comment );
			}
		} // ScopedTransaction

		// Deselect all expressions and comments.
		EmptySelection();

		if ( bHaveExpressionsToDelete )
		{
			if( bPreviewExpressionDeleted )
			{
				// The preview expression was deleted.  Null out our reference to it and reset to the normal preview mateiral
				PreviewExpression = NULL;
				SetPreviewMaterial( Material );
			}
			RefreshSourceWindowMaterial();
			UpdateSearch(FALSE);
		}
		UpdatePreviewMaterial();
		Material->MarkPackageDirty();
		bMaterialDirty = TRUE;

		UpdatePropertyWindow();

		if ( bHaveExpressionsToDelete )
		{
			RefreshExpressionPreviews();
		}
		RefreshExpressionViewport();
	}
}

/**
 * Disconnects and removes the selected material expressions.
 */
void WxMaterialEditor::DeleteSelectedObjects()
{
	DeleteObjects(SelectedExpressions, SelectedComments);
}

/**
 * Pastes into this material from the editor's material copy buffer.
 *
 * @param	PasteLocation	If non-NULL locate the upper-left corner of the new nodes' bound at this location.
 */
void WxMaterialEditor::PasteExpressionsIntoMaterial(const FIntPoint* PasteLocation)
{
	if ( GetCopyPasteBuffer()->Expressions.Num() > 0 || GetCopyPasteBuffer()->EditorComments.Num() > 0 )
	{
		// Empty the selection set, as we'll be selecting the newly created expressions.
		EmptySelection();

		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("MaterialEditorPaste")) );
			Material->Modify();

			// Copy the expressions in the material copy buffer into this material.
			TArray<UMaterialExpression*> NewExpressions;
			TArray<UMaterialExpression*> NewComments;

			UMaterialExpression::CopyMaterialExpressions( GetCopyPasteBuffer()->Expressions, GetCopyPasteBuffer()->EditorComments, Material, EditMaterialFunction, NewExpressions, NewComments );

			// Append the comments list to the expressions list so we can position all of the items at once.
			NewExpressions.Append(NewComments);
				
			// Locate and select the newly created nodes.
			const FIntRect NewExpressionBounds( GetBoundingBoxOfExpressions( NewExpressions ) );
			for ( INT ExpressionIndex = 0 ; ExpressionIndex < NewExpressions.Num() ; ++ExpressionIndex )
			{
				UMaterialExpression* NewExpression = NewExpressions( ExpressionIndex );
				if ( PasteLocation )
				{
					// We're doing a paste here.
					NewExpression->MaterialExpressionEditorX += -NewExpressionBounds.Min.X + PasteLocation->X;
					NewExpression->MaterialExpressionEditorY += -NewExpressionBounds.Min.Y + PasteLocation->Y;
				}
				else
				{
					// We're doing a duplicate or straight-up paste; offset the nodes by a fixed amount.
					const INT DuplicateOffset = 30;
					NewExpression->MaterialExpressionEditorX += DuplicateOffset;
					NewExpression->MaterialExpressionEditorY += DuplicateOffset;
				}
				AddToSelection( NewExpression );
				Material->AddExpressionParameter(NewExpression);
			}
		}

		// Update the current preview material
		UpdatePreviewMaterial();
		RefreshSourceWindowMaterial();
		UpdateSearch(FALSE);
		Material->MarkPackageDirty();

		UpdatePropertyWindow();

		RefreshExpressionPreviews();
		RefreshExpressionViewport();
		bMaterialDirty = TRUE;
	}
}

/**
 * Duplicates the selected material expressions.  Preserves internal references.
 */
void WxMaterialEditor::DuplicateSelectedObjects()
{
	// Clear the material copy buffer and copy the selected expressions into it.
	TArray<UMaterialExpression*> NewExpressions;
	TArray<UMaterialExpression*> NewComments;

	GetCopyPasteBuffer()->Expressions.Empty();
	GetCopyPasteBuffer()->EditorComments.Empty();
	UMaterialExpression::CopyMaterialExpressions( SelectedExpressions, SelectedComments, GetCopyPasteBuffer(), EditMaterialFunction, NewExpressions, NewComments );

	// Paste the material copy buffer into this material.
	PasteExpressionsIntoMaterial( NULL );
}

/**
 * Deletes any disconnected material expressions.
 */
void WxMaterialEditor::CleanUnusedExpressions()
{
	EmptySelection();

	// The set of material expressions to visit.
	TArray<UMaterialExpression*> Stack;

	// Populate the stack with inputs to the material.
	for ( INT MaterialInputIndex = 0 ; MaterialInputIndex < MaterialInputs.Num() ; ++MaterialInputIndex )
	{
		const FMaterialInputInfo& MaterialInput = MaterialInputs(MaterialInputIndex);
		UMaterialExpression* Expression = MaterialInput.Input->Expression;
		if ( Expression )
		{
			Stack.Push( Expression );
		}
	}

	if (EditMaterialFunction)
	{
		for (INT ExpressionIndex = 0; ExpressionIndex < Material->Expressions.Num(); ExpressionIndex++)
		{
			UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(Material->Expressions(ExpressionIndex));
			if (FunctionOutput)
			{
				Stack.Push(FunctionOutput);
			}
		}
	}

	// Depth-first traverse the material expression graph.
	TArray<UMaterialExpression*>	NewExpressions;
	TMap<UMaterialExpression*, INT> ReachableExpressions;
	while ( Stack.Num() > 0 )
	{
		UMaterialExpression* MaterialExpression = Stack.Pop();
		INT* AlreadyVisited = ReachableExpressions.Find( MaterialExpression );
		if ( !AlreadyVisited )
		{
			// Mark the expression as reachable.
			ReachableExpressions.Set( MaterialExpression, 0 );
			NewExpressions.AddItem( MaterialExpression );

			// Iterate over the expression's inputs and add them to the pending stack.
			const TArray<FExpressionInput*>& ExpressionInputs = MaterialExpression->GetInputs();
			for( INT ExpressionInputIndex = 0 ; ExpressionInputIndex < ExpressionInputs.Num() ; ++ ExpressionInputIndex )
			{
				FExpressionInput* Input = ExpressionInputs(ExpressionInputIndex);
				UMaterialExpression* InputExpression = Input->Expression;
				if ( InputExpression )
				{
					Stack.Push( InputExpression );
				}
			}
		}
	}

	// Kill off expressions referenced by the material that aren't reachable.
	{
		const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("MaterialEditorCleanUnusedExpressions")) );
		Material->Modify();
		Material->Expressions = NewExpressions;
	}

	RefreshExpressionViewport();
	bMaterialDirty = TRUE;
}

/** Draws the root node -- that is, the node corresponding to the final material. */
void WxMaterialEditor::DrawMaterialRoot(UBOOL bIsSelected, FCanvas* Canvas)
{
	// Construct the FLinkedObjDrawInfo for use by the linked-obj drawing utils.
	FLinkedObjDrawInfo ObjInfo;
	ObjInfo.ObjObject = Material;

	// Check if we want to pan, and our mouse is over a material input.
	UBOOL bPanMouseOverInput = DblClickObject==Material && DblClickConnType==LOC_OUTPUT;

	for ( INT MaterialInputIndex = 0 ; MaterialInputIndex < MaterialInputs.Num() ; ++MaterialInputIndex )
	{
		const FMaterialInputInfo& MaterialInput = MaterialInputs(MaterialInputIndex);
		const UBOOL bShouldAddInputConnector = !bHideUnusedConnectors || MaterialInput.Input->Expression;
		if ( bShouldAddInputConnector )
		{
			if( bPanMouseOverInput && MaterialInput.Input->Expression && DblClickConnIndex==ObjInfo.Outputs.Num() )
			{
				PanLocationOnscreen( MaterialInput.Input->Expression->MaterialExpressionEditorX+50, MaterialInput.Input->Expression->MaterialExpressionEditorY+50, 100 );
			}
			ObjInfo.Outputs.AddItem( FLinkedObjConnInfo(MaterialInput.Name, MaterialInputColor, FALSE, TRUE, 0, 0, IsActiveMaterialInput(MaterialInput)) );
		}
	}

	// Generate border color
	const FColor BorderColor( bIsSelected ? SelectedBorderColor : NormalBorderColor );

	// Highlight the currently moused over connector
	HighlightActiveConnectors( ObjInfo );

	// Use util to draw box with connectors, etc.
	const FIntPoint MaterialPos( Material->EditorX, Material->EditorY );
	FLinkedObjDrawUtils::DrawLinkedObj( Canvas, ObjInfo, *Material->GetName(), NULL, NormalFontColor, BorderColor, LinkedObjColor, MaterialPos );

	// Copy off connector values for use
	MaterialDrawInfo.DrawWidth	= ObjInfo.DrawWidth;
	MaterialDrawInfo.RightYs	= ObjInfo.OutputY;
}

/**
 * Render connections between the material's inputs and the material expression outputs connected to them.
 */
void WxMaterialEditor::DrawMaterialRootConnections(FCanvas* Canvas)
{
	// Compensates for the difference between the number of rendered inputs
	// (based on bHideUnusedConnectors) and the true number of inputs.
	INT ActiveInputCounter = -1;

	TArray<FExpressionInput*> ReferencingInputs;
	for ( INT MaterialInputIndex = 0 ; MaterialInputIndex < MaterialInputs.Num() ; ++MaterialInputIndex )
	{
		const FMaterialInputInfo& MaterialInput = MaterialInputs(MaterialInputIndex);
		UMaterialExpression* MaterialExpression = MaterialInput.Input->Expression;
		const UBOOL bWasAddedInputConnector = !bHideUnusedConnectors || MaterialExpression;

		if (bWasAddedInputConnector)
		{
			++ActiveInputCounter;

			if (MaterialExpression)
			{
				TArray<FExpressionOutput>& Outputs = MaterialExpression->GetOutputs();

				if (Outputs.Num() > 0)
				{
					INT ActiveOutputCounter = -1;
					INT OutputIndex = 0;
					const UBOOL bOutputIndexIsValid = Outputs.IsValidIndex(MaterialInput.Input->OutputIndex)
						// Attempt to handle legacy connections before OutputIndex was used that had a mask
						&& (MaterialInput.Input->OutputIndex != 0 || MaterialInput.Input->Mask == 0);

					for( ; OutputIndex < Outputs.Num() ; ++OutputIndex )
					{
						const FExpressionOutput& Output = Outputs(OutputIndex);

						// If unused connectors are hidden, the material expression output index needs to be transformed
						// to visible index rather than absolute.
						if ( bHideUnusedConnectors )
						{
							// Get a list of inputs that refer to the selected output.
							GetListOfReferencingInputs( MaterialExpression, Material, ReferencingInputs, &Output, OutputIndex );
							if ( ReferencingInputs.Num() > 0 )
							{
								++ActiveOutputCounter;
							}
						}

						if(	bOutputIndexIsValid && OutputIndex == MaterialInput.Input->OutputIndex
							|| !bOutputIndexIsValid
							&& Output.Mask == MaterialInput.Input->Mask
							&& Output.MaskR == MaterialInput.Input->MaskR
							&& Output.MaskG == MaterialInput.Input->MaskG
							&& Output.MaskB == MaterialInput.Input->MaskB
							&& Output.MaskA == MaterialInput.Input->MaskA )
						{
							break;
						}
					}
					
					if (OutputIndex >= Outputs.Num())
					{
						// Work around for non-reproducible crash where OutputIndex would be out of bounds
						OutputIndex = Outputs.Num() - 1;
					}

					const FIntPoint Start( GetMaterialConnectionLocation(Material,LOC_OUTPUT,ActiveInputCounter) );

					const INT ExpressionOutputLookupIndex		= bHideUnusedConnectors ? ActiveOutputCounter : OutputIndex;
					const FIntPoint End( GetExpressionConnectionLocation(MaterialExpression,LOC_INPUT,ExpressionOutputLookupIndex) );


					// If either of the connection ends are highlighted then highlight the line.
					FColor Color;

					if (!IsActiveMaterialInput(MaterialInput))
					{
						// always color connections to inactive inputs gray
						Color = ConnectionOptionalColor;
					}
					else if(IsConnectorHighlighted(Material, LOC_OUTPUT, ActiveInputCounter) || IsConnectorHighlighted(MaterialExpression, LOC_INPUT, ExpressionOutputLookupIndex))
					{
						Color = ConnectionSelectedColor;
					}
					else if(IsConnectorMarked(Material, LOC_OUTPUT, ActiveInputCounter) || IsConnectorMarked(MaterialExpression, LOC_INPUT, ExpressionOutputLookupIndex))
					{
						Color = ConnectionMarkedColor;
					}
					else
					{
						Color = ConnectionNormalColor;
					}

					// DrawCurves
					{
						const FLOAT Tension = Abs<INT>( Start.X - End.X );
						FLinkedObjDrawUtils::DrawSpline( Canvas, End, -Tension*FVector2D(1,0), Start, -Tension*FVector2D(1,0), Color, TRUE );
					}
				}
			}
		}
	}
}

/**
* Draws specified material expression node:
* 
* @param	Canvas				The canvas the expression is being rendered too.
* @param	ObjInfo				The linking information for this expression.
* @param	Name				Title Text shown at the top of the expression.
* @param	Comment				Comment Text associated with this expression.
* @param	BorderColor			The boarder color of the expression.
* @param	TitleBkgColor		The background color of the title bar.
* @param	MaterialExpression	Material expression to be drawn.
* @param	bRenderPreview		Allows for disabling the render preview pane.
* @param	bFreezeTime			Allows time to be paused for this expression.
*/
void WxMaterialEditor::DrawMaterialExpressionLinkedObj(FCanvas* Canvas, FLinkedObjDrawInfo& ObjInfo, const TCHAR* Name, const TCHAR* Comment, const FColor& FontColor, const FColor& BorderColor, const FColor& TitleBkgColor, UMaterialExpression* MaterialExpression, UBOOL bRenderPreview, UBOOL bFreezeTime)
{
	static const INT ExpressionPreviewSize = 96;
	static const INT ExpressionPreviewBorder = 1;
	//static const INT ExpressionPreviewSize = ExpressionPreviewBaseSize + ExpressionPreviewBorder;
	
	// If an expression is currently being previewed
	const UBOOL bPreviewing = (MaterialExpression == PreviewExpression);

	const FIntPoint Pos( MaterialExpression->MaterialExpressionEditorX, MaterialExpression->MaterialExpressionEditorY );
#if 0
	FLinkedObjDrawUtils::DrawLinkedObj( Canvas, ObjInfo, Name, Comment, FontColor, BorderColor, TitleBkgColor, Pos );
#else
	const UBOOL bHitTesting = Canvas->IsHitTesting();

	const FIntPoint TitleSize	= FLinkedObjDrawUtils::GetTitleBarSize(Canvas, Name);
	const FIntPoint LogicSize	= FLinkedObjDrawUtils::GetLogicConnectorsSize(ObjInfo);

	// Includes one-pixel border on left and right and a one-pixel border between the preview icon and the title text.
	ObjInfo.DrawWidth	= 2 + 1 + PreviewIconWidth + Max(Max(TitleSize.X, LogicSize.X), ExpressionPreviewSize+2*ExpressionPreviewBorder);
	const INT BodyHeight = 2 + Max(LogicSize.Y, ExpressionPreviewSize+2*ExpressionPreviewBorder);

	// Includes one-pixel spacer between title and body.
	ObjInfo.DrawHeight	= TitleSize.Y + 1 + BodyHeight;

	if(bHitTesting) Canvas->SetHitProxy( new HLinkedObjProxy(ObjInfo.ObjObject) );

	// Added array of comments
	TArray<FString> Comments;

	if( bPreviewing )
	{
		// Draw a box on top of the normal title bar that indicates we are currently previewing this node
		FLinkedObjDrawUtils::DrawTitleBar( Canvas, FIntPoint(Pos.X, Pos.Y - TitleSize.Y + 1), FIntPoint(ObjInfo.DrawWidth, TitleSize.Y), FontColor, BorderColor, TitleBarColor, TEXT("Previewing"), Comments );
	}

	Comments.AddItem(FString(Comment));
	FLinkedObjDrawUtils::DrawTitleBar(Canvas, Pos, FIntPoint(ObjInfo.DrawWidth, TitleSize.Y), FontColor, BorderColor, TitleBkgColor, Name, Comments);

	FLinkedObjDrawUtils::DrawTile( Canvas, Pos.X,		Pos.Y + TitleSize.Y + 1,	ObjInfo.DrawWidth,		BodyHeight,		0.0f,0.0f,0.0f,0.0f, BorderColor );
	FLinkedObjDrawUtils::DrawTile( Canvas, Pos.X + 1,	Pos.Y + TitleSize.Y + 2,	ObjInfo.DrawWidth - 2,	BodyHeight-2,	0.0f,0.0f,0.0f,0.0f, BorderShadowColor );

	if ( bRenderPreview )
	{
		UBOOL bNewlyCreated;
		FExpressionPreview* ExpressionPreview = GetExpressionPreview( MaterialExpression, bNewlyCreated, FALSE);
		FLinkedObjDrawUtils::DrawTile( Canvas, Pos.X + 1 + ExpressionPreviewBorder,	Pos.Y + TitleSize.Y + 2 + ExpressionPreviewBorder,	ExpressionPreviewSize,	ExpressionPreviewSize,	0.0f,0.0f,1.0f,1.0f, ExpressionPreview, bFreezeTime );
	}

	if(bHitTesting) Canvas->SetHitProxy( NULL );

	// Highlight the currently moused over connector
	HighlightActiveConnectors( ObjInfo );

	//const FLinearColor ConnectorTileBackgroundColor( 0.f, 0.f, 0.f, 0.5f );
	FLinkedObjDrawUtils::DrawLogicConnectors(Canvas, ObjInfo, Pos + FIntPoint(0, TitleSize.Y + 1), FIntPoint(ObjInfo.DrawWidth, LogicSize.Y), NULL);//&ConnectorTileBackgroundColor);
#endif
}

/**
 * Draws messages on the specified viewport and canvas.
 */
void WxMaterialEditor::DrawMessages( FViewport* Viewport, FCanvas* Canvas )
{
	if( PreviewExpression != NULL )
	{
		Canvas->PushAbsoluteTransform( FMatrix::Identity );

		// The message to display in the viewport.
		FString Name = FString::Printf( TEXT("Previewing: %s"), *PreviewExpression->GetName() );

		// Size of the tile we are about to draw.  Should extend the length of the view in X.
		const FIntPoint TileSize( Viewport->GetSizeX(), 25);
		
		UFont* FontToUse = GEditor->EditorFont;
		
		DrawTile( Canvas, 0.0f, 0.0f, TileSize.X, TileSize.Y, 0.0f, 0.0f, 0.0f, 0.0f, PreviewColor );

		INT XL, YL;
		StringSize( FontToUse, XL, YL, *Name );
		if( XL > TileSize.X )
		{
			// There isn't enough room to show the preview expression name
			Name = TEXT("Previewing");
			StringSize( FontToUse, XL, YL, *Name );
		}
		
		// Center the string in the middle of the tile.
		const FIntPoint StringPos( (TileSize.X-XL)/2, ((TileSize.Y-YL)/2)+1 );
		// Draw the preview message
		DrawShadowedString( Canvas, StringPos.X, StringPos.Y, *Name, FontToUse, NormalFontColor );

		Canvas->PopTransform();
	}
}

/**
 * Called when the user double-clicks an object in the viewport
 *
 * @param	Obj		the object that was double-clicked on
 */
void WxMaterialEditor::DoubleClickedObject(UObject* Obj)
{
	check(Obj);

	UMaterialExpressionConstant3Vector* Constant3Expression = Cast<UMaterialExpressionConstant3Vector>(Obj);
	UMaterialExpressionConstant4Vector* Constant4Expression = Cast<UMaterialExpressionConstant4Vector>(Obj);
	UMaterialExpressionFunctionInput* InputExpression = Cast<UMaterialExpressionFunctionInput>(Obj);
	UMaterialExpressionVectorParameter* VectorExpression = Cast<UMaterialExpressionVectorParameter>(Obj);
	
	FColorChannelStruct ChannelEditStruct;
	FPropertyNode* NotifyNode = NULL;
	if (Constant3Expression)
	{
		ChannelEditStruct.Red = &Constant3Expression->R;
		ChannelEditStruct.Green = &Constant3Expression->G;
		ChannelEditStruct.Blue = &Constant3Expression->B;
		NotifyNode = PropertyWindow->FindPropertyNode(TEXT("R"));
	}
	else if (Constant4Expression)
	{
		ChannelEditStruct.Red = &Constant4Expression->R;
		ChannelEditStruct.Green = &Constant4Expression->G;
		ChannelEditStruct.Blue = &Constant4Expression->B;
		ChannelEditStruct.Alpha = &Constant4Expression->A;
		NotifyNode = PropertyWindow->FindPropertyNode(TEXT("R"));
	}
	else if (InputExpression)
	{
		ChannelEditStruct.Red = &InputExpression->PreviewValue.X;
		ChannelEditStruct.Green = &InputExpression->PreviewValue.Y;
		ChannelEditStruct.Blue = &InputExpression->PreviewValue.Z;
		ChannelEditStruct.Alpha = &InputExpression->PreviewValue.W;
		NotifyNode = PropertyWindow->FindPropertyNode(TEXT("X"));
	}
	else if (VectorExpression)
	{
		ChannelEditStruct.Red = &VectorExpression->DefaultValue.R;
		ChannelEditStruct.Green = &VectorExpression->DefaultValue.G;
		ChannelEditStruct.Blue = &VectorExpression->DefaultValue.B;
		ChannelEditStruct.Alpha = &VectorExpression->DefaultValue.A;
		NotifyNode = PropertyWindow->FindPropertyNode(TEXT("R"));
	}

	if (ChannelEditStruct.Red || ChannelEditStruct.Green || ChannelEditStruct.Blue || ChannelEditStruct.Alpha)
	{
		FPickColorStruct PickColorStruct;
		PickColorStruct.RefreshWindows.AddItem(this);
		PickColorStruct.PropertyWindow = PropertyWindow->GetPropertyWindowForCallbacks();
		//pass in one of the child property nodes, to force the previews to update
		PickColorStruct.PropertyNode = NotifyNode;
		PickColorStruct.PartialFLOATColorArray.AddItem(ChannelEditStruct);
		PickColorStruct.ParentObjects.AddItem(Obj);
		PickColorStruct.bSendEventsOnlyOnMouseUp = TRUE;

		PickColor(PickColorStruct);
	}

	UMaterialExpressionTextureSample* TextureExpression = Cast<UMaterialExpressionTextureSample>(Obj);
	UMaterialExpressionTextureSampleParameter* TextureParameterExpression = Cast<UMaterialExpressionTextureSampleParameter>(Obj);
	UMaterialExpressionMaterialFunctionCall* FunctionExpression = Cast<UMaterialExpressionMaterialFunctionCall>(Obj);

	TArray<UObject*> ObjectsToView;
	UObject* ObjectToEdit = NULL;

	if (TextureExpression && TextureExpression->Texture)
	{
		ObjectsToView.AddItem(TextureExpression->Texture);
	}
	else if (TextureParameterExpression && TextureParameterExpression->Texture)
	{
		ObjectsToView.AddItem(TextureParameterExpression->Texture);
	}
	else if (FunctionExpression && FunctionExpression->MaterialFunction)
	{
		ObjectToEdit = FunctionExpression->MaterialFunction;
	}

	if (ObjectsToView.Num() > 0)
	{
		GApp->EditorFrame->SyncBrowserToObjects(ObjectsToView);
	}

	if (ObjectToEdit)
	{
#if WITH_MANAGED_CODE
		GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_ActivateObject, ObjectToEdit ) );
#endif
	}
}

/**
* Called when double-clicking a connector.
* Used to pan the connection's link into view
*
* @param	Connector	The connector that was double-clicked
*/
void WxMaterialEditor::DoubleClickedConnector(FLinkedObjectConnector& Connector)
{
	DblClickObject = Connector.ConnObj;
	DblClickConnType = Connector.ConnType;
	DblClickConnIndex = Connector.ConnIndex;
}

/** Draws the specified material expression node. */
void WxMaterialEditor::DrawMaterialExpression(UMaterialExpression* MaterialExpression, UBOOL bExpressionSelected, FCanvas* Canvas, FLinkedObjDrawInfo& ObjInfo)
{
	// Construct the FLinkedObjDrawInfo for use by the linked-obj drawing utils.
	ObjInfo.ObjObject = MaterialExpression;

	if (LinkedObjVC->MouseOverObject == MaterialExpression 
		&& LinkedObjVC->MouseOverConnIndex == INDEX_NONE
		&& (appSeconds() - LinkedObjVC->MouseOverTime) > LinkedObjVC->ToolTipDelayMS * .001f)
	{
		MaterialExpression->GetExpressionToolTip(ObjInfo.ToolTips);
	}

	// Check if we want to pan, and our mouse is over an input for this expression.
	UBOOL bPanMouseOverInput = DblClickObject==MaterialExpression && DblClickConnType==LOC_OUTPUT;

	// Material expression inputs, drawn on the right side of the node.
	const TArray<FExpressionInput*>& ExpressionInputs = MaterialExpression->GetInputs();
	for( INT ExpressionInputIndex = 0 ; ExpressionInputIndex < ExpressionInputs.Num() ; ++ ExpressionInputIndex )
	{
		FExpressionInput* Input = ExpressionInputs(ExpressionInputIndex);
		UMaterialExpression* InputExpression = Input->Expression;
		const UBOOL bShouldAddInputConnector = (!bHideUnusedConnectors || InputExpression) && MaterialExpression->bShowInputs;
		if ( bShouldAddInputConnector )
		{
			if( bPanMouseOverInput && Input->Expression && DblClickConnIndex==ObjInfo.Outputs.Num() )
			{
				PanLocationOnscreen( Input->Expression->MaterialExpressionEditorX+50, Input->Expression->MaterialExpressionEditorY+50, 100 );
			}

			FString InputName = MaterialExpression->GetInputName(ExpressionInputIndex);
			// Shorten long expression input names.
			if ( !appStricmp( *InputName, TEXT("Coordinates") ) )
			{
				InputName = TEXT("UVs");
			}
			else if ( !appStricmp( *InputName, TEXT("TextureObject") ) )
			{
				InputName = TEXT("Tex");
			}
			else if ( !appStricmp( *InputName, TEXT("Input") ) )
			{
				InputName = TEXT("");
			}
			else if ( !appStricmp( *InputName, TEXT("Exponent") ) )
			{
				InputName = TEXT("Exp");
			}
			else if ( !appStricmp( *InputName, TEXT("AGreaterThanB") ) )
			{
				InputName = TEXT("A>B");
			}
			else if ( !appStricmp( *InputName, TEXT("AEqualsB") ) )
			{
				InputName = TEXT("A=B");
			}
			else if ( !appStricmp( *InputName, TEXT("ALessThanB") ) )
			{
				InputName = TEXT("A<B");
			}

			const FColor InputConnectorColor = MaterialExpression->IsInputConnectionRequired(ExpressionInputIndex) ? ConnectionNormalColor : ConnectionOptionalColor;
			ObjInfo.Outputs.AddItem( FLinkedObjConnInfo(*InputName, InputConnectorColor) );

			if (IsConnectorHighlighted(MaterialExpression, LOC_OUTPUT, ObjInfo.Outputs.Num() - 1)
				&& (appSeconds() - LinkedObjVC->MouseOverTime) > LinkedObjVC->ToolTipDelayMS * .001f)
			{
				MaterialExpression->GetConnectorToolTip(ExpressionInputIndex, INDEX_NONE, ObjInfo.Outputs.Last().ToolTips);
			}
		}
	}

	// Material expression outputs, drawn on the left side of the node
	TArray<FExpressionOutput>& Outputs = MaterialExpression->GetOutputs();

	// Check if we want to pan, and our mouse is over an output for this expression.
	UBOOL bPanMouseOverOutput = DblClickObject==MaterialExpression && DblClickConnType==LOC_INPUT;

	TArray<FExpressionInput*> ReferencingInputs;
	for( INT OutputIndex = 0 ; OutputIndex < Outputs.Num() ; ++OutputIndex )
	{
		const FExpressionOutput& ExpressionOutput = Outputs(OutputIndex);
		UBOOL bShouldAddOutputConnector = TRUE;
		if ( bHideUnusedConnectors )
		{
			// Get a list of inputs that refer to the selected output.
			GetListOfReferencingInputs( MaterialExpression, Material, ReferencingInputs, &ExpressionOutput, OutputIndex );
			bShouldAddOutputConnector = ReferencingInputs.Num() > 0;
		}

		if ( bShouldAddOutputConnector && MaterialExpression->bShowOutputs )
		{
			const TCHAR* OutputName = TEXT("");
			FColor OutputColor( 0, 0, 0 );

			if( ExpressionOutput.Mask )
			{
				if		( ExpressionOutput.MaskR && !ExpressionOutput.MaskG && !ExpressionOutput.MaskB && !ExpressionOutput.MaskA)
				{
					// R
					OutputColor = MaskRColor;
					//OutputName = TEXT("R");
				}
				else if	(!ExpressionOutput.MaskR &&  ExpressionOutput.MaskG && !ExpressionOutput.MaskB && !ExpressionOutput.MaskA)
				{
					// G
					OutputColor = MaskGColor;
					//OutputName = TEXT("G");
				}
				else if	(!ExpressionOutput.MaskR && !ExpressionOutput.MaskG &&  ExpressionOutput.MaskB && !ExpressionOutput.MaskA)
				{
					// B
					OutputColor = MaskBColor;
					//OutputName = TEXT("B");
				}
				else if	(!ExpressionOutput.MaskR && !ExpressionOutput.MaskG && !ExpressionOutput.MaskB &&  ExpressionOutput.MaskA)
				{
					// A
					OutputColor = MaskAColor;
					//OutputName = TEXT("A");
				}
				else
				{
					// RGBA
					//OutputName = TEXT("RGBA");
				}
			}

			if (MaterialExpression->bShowOutputNameOnPin)
			{
				OutputName = *(ExpressionOutput.OutputName);
			}

			// If this is the output we're hovering over, pan its first connection into view.
			if( bPanMouseOverOutput && DblClickConnIndex==ObjInfo.Inputs.Num() )
			{
				// Find what this output links to.
				TArray<FMaterialExpressionReference> References;
				GetListOfReferencingInputs(MaterialExpression, Material, References, &ExpressionOutput, OutputIndex);
				if( References.Num() > 0 )
				{
					if( References(0).Expression == NULL )
					{
						// connects to the root node
						PanLocationOnscreen( Material->EditorX+50, Material->EditorY+50, 100 );
					}
					else
					{
						PanLocationOnscreen( References(0).Expression->MaterialExpressionEditorX+50, References(0).Expression->MaterialExpressionEditorY+50, 100 );
					}
				}
			}

			// We use the "Inputs" array so that the connectors are drawn on the left side of the node.
			ObjInfo.Inputs.AddItem( FLinkedObjConnInfo(OutputName, OutputColor) );

			if (IsConnectorHighlighted(MaterialExpression, LOC_INPUT, ObjInfo.Inputs.Num() - 1)
				&& (appSeconds() - LinkedObjVC->MouseOverTime) > LinkedObjVC->ToolTipDelayMS * .001f)
			{
				MaterialExpression->GetConnectorToolTip(INDEX_NONE, OutputIndex, ObjInfo.Inputs.Last().ToolTips);
			}
		}
	}

	// Determine the texture dependency length for the material and the expression.
	const FMaterialResource* MaterialResource = Material->GetMaterialResource();
	INT MaxTextureDependencyLength = MaterialResource->GetMaxTextureDependencyLength();
	const INT* TextureDependencyLength = MaterialResource->GetTextureDependencyLengthMap().Find(MaterialExpression);
	const UBOOL bCurrentSearchResult = SelectedSearchResult >= 0 && SelectedSearchResult < SearchResults.Num() && SearchResults(SelectedSearchResult) == MaterialExpression;

	const FMaterialResource* ErrorMaterialResource = PreviewExpression ? ExpressionPreviewMaterial->GetMaterialResource() : MaterialResource;
	const UBOOL bIsErrorExpression = ErrorMaterialResource->GetErrorExpressions().FindItemIndex(MaterialExpression) != INDEX_NONE;

	// Generate border color
	FColor BorderColor = MaterialExpression->BorderColor;
	FColor FontColor = NormalFontColor;	// default to yellow
	BorderColor.A = 255;
	
	if (bExpressionSelected)
	{
		BorderColor = SelectedBorderColor;
		FontColor = SelectedFontColor;
	}
	else if (bIsErrorExpression)
	{
		// Outline expressions that caused errors in red
		BorderColor = ErrorBorderColor;
		FontColor = ErrorFontColor;
	}
	else if (bCurrentSearchResult)
	{
		BorderColor = SearchBorderColor;
		FontColor = SearchFontColor;
	}
	else if (PreviewExpression == MaterialExpression)
	{
		// If we are currently previewing a node, its border should be the preview color.
		BorderColor = PreviewBorderColor;
		FontColor = PreviewFontColor;
	}
	else if (TextureDependencyLength && *TextureDependencyLength == MaxTextureDependencyLength && MaxTextureDependencyLength > 1)
	{
		BorderColor = DependancyBorderColor;
		FontColor = DependancyFontColor;
	}
	else if (UMaterial::IsParameter(MaterialExpression))
	{
		if (Material->HasDuplicateParameters(MaterialExpression))
		{
			BorderColor = ParamWithDupsBorderColor;
			FontColor = ParamWithDupsFontColor;
		}
		else
		{
			BorderColor = ParamBorderColor;
			FontColor = ParamFontColor;
		}
	}
	else if (UMaterial::IsDynamicParameter(MaterialExpression))
	{
		if (Material->HasDuplicateDynamicParameters(MaterialExpression))
		{
			BorderColor = DynParamWithDupsBorderColor;
			FontColor = DynParamWithDupsFontColor;
		}
		else
		{
			BorderColor = DynParamBorderColor;
			FontColor = DynParamFontColor;
		}
	}

	// Time is only advanced on when viewport is in real time mode, or that node is previewing.
	UBOOL bRealTimePreview = (MaterialExpression->bRealtimePreview || bAlwaysRefreshAllPreviews || LinkedObjVC->IsRealtime());

	// Use util to draw box with connectors, etc.
	DrawMaterialExpressionLinkedObj( Canvas, ObjInfo, *MaterialExpression->GetCaption(), NULL, FontColor, BorderColor, bCurrentSearchResult ? BorderColor : LinkedObjColor, MaterialExpression, !(MaterialExpression->bHidePreviewWindow), !bRealTimePreview);

	// Read back the height of the first connector on the left side of the node,
	// for use later when drawing connections to this node.
	FMaterialNodeDrawInfo& ExpressionDrawInfo	= GetDrawInfo( MaterialExpression );
	ExpressionDrawInfo.LeftYs					= ObjInfo.InputY;
	ExpressionDrawInfo.RightYs					= ObjInfo.OutputY;
	ExpressionDrawInfo.DrawWidth				= ObjInfo.DrawWidth;

	check( bHideUnusedConnectors || ExpressionDrawInfo.RightYs.Num() == ExpressionInputs.Num() || !MaterialExpression->bShowInputs );

	// Draw realtime preview indicator above the node.
	if (MaterialExpression->bHidePreviewWindow == FALSE)
	{
		if ( FLinkedObjDrawUtils::AABBLiesWithinViewport( Canvas, MaterialExpression->MaterialExpressionEditorX+PreviewIconLoc.X, MaterialExpression->MaterialExpressionEditorY+PreviewIconLoc.Y, PreviewIconWidth, PreviewIconWidth ) )
		{
			const UBOOL bHitTesting = Canvas->IsHitTesting();
			if( bHitTesting )  Canvas->SetHitProxy( new HRealtimePreviewProxy( MaterialExpression ) );

			// Draw black background icon.
			FLinkedObjDrawUtils::DrawTile( Canvas, MaterialExpression->MaterialExpressionEditorX+PreviewIconLoc.X,		MaterialExpression->MaterialExpressionEditorY+PreviewIconLoc.Y, PreviewIconWidth,	PreviewIconWidth, 0.f, 0.f, 1.f, 1.f, BackgroundColor );

			// Draw yellow fill if realtime preview is enabled for this node.
			if( MaterialExpression->bRealtimePreview )
			{
				FLinkedObjDrawUtils::DrawTile( Canvas, MaterialExpression->MaterialExpressionEditorX+PreviewIconLoc.X+1,	MaterialExpression->MaterialExpressionEditorY+PreviewIconLoc.Y+1, PreviewIconWidth-2,	PreviewIconWidth-2,	0.f, 0.f, 1.f, 1.f, RealtimePreviewColor );
			}

			// Draw a small red icon above the node if realtime preview is enabled for all nodes.
			if ( bAlwaysRefreshAllPreviews )
			{
				FLinkedObjDrawUtils::DrawTile( Canvas, MaterialExpression->MaterialExpressionEditorX+PreviewIconLoc.X+2,	MaterialExpression->MaterialExpressionEditorY+PreviewIconLoc.Y+2, PreviewIconWidth-4,	PreviewIconWidth-4,	0.f, 0.f, 1.f, 1.f, AllPreviewsColor );
			}
			if( bHitTesting )  Canvas->SetHitProxy( NULL );
		}
	}
}

/**
 * Render connectors between this material expression's inputs and the material expression outputs connected to them.
 */
void WxMaterialEditor::DrawMaterialExpressionConnections(UMaterialExpression* MaterialExpression, FCanvas* Canvas)
{
	// Compensates for the difference between the number of rendered inputs
	// (based on bHideUnusedConnectors) and the true number of inputs.
	INT ActiveInputCounter = -1;

	TArray<FExpressionInput*> ReferencingInputs;

	const TArray<FExpressionInput*>& ExpressionInputs = MaterialExpression->GetInputs();
	for( INT ExpressionInputIndex = 0 ; ExpressionInputIndex < ExpressionInputs.Num() ; ++ ExpressionInputIndex )
	{
		FExpressionInput* Input = ExpressionInputs(ExpressionInputIndex);
		UMaterialExpression* InputExpression = Input->Expression;
		if ( InputExpression )
		{
			++ActiveInputCounter;

			// Find the matching output.
			TArray<FExpressionOutput>& Outputs = InputExpression->GetOutputs();

			if (Outputs.Num() > 0)
			{
				INT ActiveOutputCounter = -1;
				INT OutputIndex = 0;
				const UBOOL bOutputIndexIsValid = Outputs.IsValidIndex(Input->OutputIndex)
					// Attempt to handle legacy connections before OutputIndex was used that had a mask
					&& (Input->OutputIndex != 0 || Input->Mask == 0);

				for( ; OutputIndex < Outputs.Num() ; ++OutputIndex )
				{
					const FExpressionOutput& Output = Outputs(OutputIndex);

					// If unused connectors are hidden, the material expression output index needs to be transformed
					// to visible index rather than absolute.
					if ( bHideUnusedConnectors )
					{
						// Get a list of inputs that refer to the selected output.
						GetListOfReferencingInputs( InputExpression, Material, ReferencingInputs, &Output, OutputIndex );
						if ( ReferencingInputs.Num() > 0 )
						{
							++ActiveOutputCounter;
						}
					}

					if (bOutputIndexIsValid && Input->OutputIndex == OutputIndex
						|| !bOutputIndexIsValid 
						&& Output.Mask == Input->Mask
						&& Output.MaskR == Input->MaskR
						&& Output.MaskG == Input->MaskG
						&& Output.MaskB == Input->MaskB
						&& Output.MaskA == Input->MaskA )
					{
						break;
					}
				}

				if (OutputIndex >= Outputs.Num())
				{
					// Work around for non-reproducible crash where OutputIndex would be out of bounds
					OutputIndex = Outputs.Num() - 1;
				}

				const INT ExpressionInputLookupIndex		= bHideUnusedConnectors ? ActiveInputCounter : ExpressionInputIndex;
				const FIntPoint Start( GetExpressionConnectionLocation(MaterialExpression,LOC_OUTPUT,ExpressionInputLookupIndex) );
				const INT ExpressionOutputLookupIndex		= bHideUnusedConnectors ? ActiveOutputCounter : OutputIndex;
				const FIntPoint End( GetExpressionConnectionLocation(InputExpression,LOC_INPUT,ExpressionOutputLookupIndex) );


				// If either of the connection ends are highlighted then highlight the line.
				FColor Color;

				if(IsConnectorHighlighted(MaterialExpression, LOC_OUTPUT, ExpressionInputLookupIndex) || IsConnectorHighlighted(InputExpression, LOC_INPUT, ExpressionOutputLookupIndex))
				{
					Color = ConnectionSelectedColor;
				}
				else if(IsConnectorMarked(MaterialExpression, LOC_OUTPUT, ExpressionInputLookupIndex) || IsConnectorMarked(InputExpression, LOC_INPUT, ExpressionOutputLookupIndex))
				{
					Color = ConnectionMarkedColor;
				}
				else
				{
					Color = ConnectionNormalColor;
				}

				// DrawCurves
				{
					const FLOAT Tension = Abs<INT>( Start.X - End.X );
					FLinkedObjDrawUtils::DrawSpline( Canvas, End, -Tension*FVector2D(1,0), Start, -Tension*FVector2D(1,0), Color, TRUE );
				}
			}
		}
	}
}

/** Draws comments for the specified material expression node. */
void WxMaterialEditor::DrawMaterialExpressionComments(UMaterialExpression* MaterialExpression, FCanvas* Canvas)
{
	// Draw the material expression comment string unzoomed.
	if( MaterialExpression->Desc.Len() > 0 )
	{
		const FLOAT OldZoom2D = FLinkedObjDrawUtils::GetUniformScaleFromMatrix(Canvas->GetFullTransform());

		INT XL, YL;
		StringSize( GEditor->EditorFont, XL, YL, *MaterialExpression->Desc );

		// We always draw comment-box text at normal size (don't scale it as we zoom in and out.)
		const INT x = appTrunc( MaterialExpression->MaterialExpressionEditorX*OldZoom2D );
		const INT y = appTrunc( MaterialExpression->MaterialExpressionEditorY*OldZoom2D - YL );

		// Viewport cull at a zoom of 1.0, because that's what we'll be drawing with.
		if ( FLinkedObjDrawUtils::AABBLiesWithinViewport( Canvas, MaterialExpression->MaterialExpressionEditorX, MaterialExpression->MaterialExpressionEditorY - YL, XL, YL ) )
		{
			Canvas->PushRelativeTransform(FScaleMatrix(1.0f / OldZoom2D));
			{
				FLinkedObjDrawUtils::DrawString( Canvas, x, y, *MaterialExpression->Desc, GEditor->EditorFont, CommentsColor );
			}
			Canvas->PopTransform();
		}
	}
}

/** Draws tooltips for material expressions. */
void WxMaterialEditor::DrawMaterialExpressionToolTips(const TArray<FLinkedObjDrawInfo>& LinkedObjDrawInfos, FCanvas* Canvas)
{
	for (INT DrawInfoIndex = 0; DrawInfoIndex < LinkedObjDrawInfos.Num(); DrawInfoIndex++)
	{
		const FLinkedObjDrawInfo& ObjInfo = LinkedObjDrawInfos(DrawInfoIndex);
		UMaterialExpression* MaterialExpression = Cast<UMaterialExpression>(ObjInfo.ObjObject);
		if (MaterialExpression)
		{
			const FIntPoint Pos( MaterialExpression->MaterialExpressionEditorX, MaterialExpression->MaterialExpressionEditorY );
			const FIntPoint TitleSize	= FLinkedObjDrawUtils::GetTitleBarSize(Canvas, *MaterialExpression->GetCaption());

			FLinkedObjDrawUtils::DrawToolTips(Canvas, ObjInfo, Pos + FIntPoint(0, TitleSize.Y + 1), FIntPoint(ObjInfo.DrawWidth, ObjInfo.DrawHeight));
		}
	}
}

/** Draws UMaterialExpressionComment nodes. */
void WxMaterialEditor::DrawCommentExpressions(FCanvas* Canvas)
{
	const UBOOL bHitTesting = Canvas->IsHitTesting();
	for ( INT CommentIndex = 0 ; CommentIndex < Material->EditorComments.Num() ; ++CommentIndex )
	{
		UMaterialExpressionComment* Comment = Material->EditorComments(CommentIndex);

		const UBOOL bSelected = SelectedComments.ContainsItem( Comment );
		const UBOOL bCurrentSearchResult = SelectedSearchResult >= 0 && SelectedSearchResult < SearchResults.Num() && SearchResults(SelectedSearchResult) == Comment;

		const FColor FrameColor = bCurrentSearchResult ? CurrentSearchedBorderColor : bSelected ? SelectedBorderColor : NormalBorderColor;

		for(INT i=0; i<1; i++)
		{
			DrawLine2D(Canvas, FVector2D(Comment->MaterialExpressionEditorX,						Comment->MaterialExpressionEditorY + i),					FVector2D(Comment->MaterialExpressionEditorX + Comment->SizeX,		Comment->MaterialExpressionEditorY + i),					FrameColor );
			DrawLine2D(Canvas, FVector2D(Comment->MaterialExpressionEditorX + Comment->SizeX - i,	Comment->MaterialExpressionEditorY),						FVector2D(Comment->MaterialExpressionEditorX + Comment->SizeX - i,	Comment->MaterialExpressionEditorY + Comment->SizeY),		FrameColor );
			DrawLine2D(Canvas, FVector2D(Comment->MaterialExpressionEditorX + Comment->SizeX,		Comment->MaterialExpressionEditorY + Comment->SizeY - i),	FVector2D(Comment->MaterialExpressionEditorX,						Comment->MaterialExpressionEditorY + Comment->SizeY - i),	FrameColor );
			DrawLine2D(Canvas, FVector2D(Comment->MaterialExpressionEditorX + i,					Comment->MaterialExpressionEditorY + Comment->SizeY),		FVector2D(Comment->MaterialExpressionEditorX + i,					Comment->MaterialExpressionEditorY - 1),					FrameColor );
		}

		// Draw little sizing triangle in bottom left.
		const INT HandleSize = 16;
		const FIntPoint A(Comment->MaterialExpressionEditorX + Comment->SizeX,				Comment->MaterialExpressionEditorY + Comment->SizeY);
		const FIntPoint B(Comment->MaterialExpressionEditorX + Comment->SizeX,				Comment->MaterialExpressionEditorY + Comment->SizeY - HandleSize);
		const FIntPoint C(Comment->MaterialExpressionEditorX + Comment->SizeX - HandleSize,	Comment->MaterialExpressionEditorY + Comment->SizeY);
		const BYTE TriangleAlpha = (bSelected) ? 255 : 32; // Make it more transparent if comment is not selected.

		if(bHitTesting)  Canvas->SetHitProxy( new HLinkedObjProxySpecial(Comment, 1) );
		DrawTriangle2D( Canvas, A, FVector2D(0,0), B, FVector2D(0,0), C, FVector2D(0,0), FColor(0,0,0,TriangleAlpha) );
		if(bHitTesting)  Canvas->SetHitProxy( NULL );

		// Check there are some valid chars in string. If not - we can't select it! So we force it back to default.
		UBOOL bHasProperChars = FALSE;
		for( INT i = 0 ; i < Comment->Text.Len() ; ++i )
		{
			if ( Comment->Text[i] != ' ' )
			{
				bHasProperChars = TRUE;
				break;
			}
		}
		if ( !bHasProperChars )
		{
			Comment->Text = TEXT("Comment");
		}

		const FLOAT OldZoom2D = FLinkedObjDrawUtils::GetUniformScaleFromMatrix(Canvas->GetFullTransform());

		INT XL, YL;
		StringSize( GEditor->EditorFont, XL, YL, *Comment->Text );

		// We always draw comment-box text at normal size (don't scale it as we zoom in and out.)
		const INT x = appTrunc(Comment->MaterialExpressionEditorX*OldZoom2D + 2);
		const INT y = appTrunc(Comment->MaterialExpressionEditorY*OldZoom2D - YL - 2);

		// Viewport cull at a zoom of 1.0, because that's what we'll be drawing with.
		if ( FLinkedObjDrawUtils::AABBLiesWithinViewport( Canvas, Comment->MaterialExpressionEditorX+2, Comment->MaterialExpressionEditorY-YL-2, XL, YL ) )
		{
			Canvas->PushRelativeTransform(FScaleMatrix(1.0f / OldZoom2D));
			{
				// We only set the hit proxy for the comment text.
				if ( bHitTesting ) Canvas->SetHitProxy( new HLinkedObjProxy(Comment) );
				DrawShadowedString(Canvas, x, y, *Comment->Text, GEditor->EditorFont, ShadowStringColor );
				if ( bHitTesting ) Canvas->SetHitProxy( NULL );
			}
			Canvas->PopTransform();
		}
	}
}

/**
 * Highlights any active logic connectors in the specified linked object.
 */
void WxMaterialEditor::HighlightActiveConnectors(FLinkedObjDrawInfo &ObjInfo)
{
	for(INT InputIdx=0; InputIdx<ObjInfo.Inputs.Num(); InputIdx++)
	{
		if((LinkedObjVC->MouseOverConnType==LOC_INPUT || DblClickConnType==LOC_INPUT)
			&& IsConnectorHighlighted(ObjInfo.ObjObject, LOC_INPUT, InputIdx))
		{
			ObjInfo.Inputs(InputIdx).Color = ConnectionSelectedColor;
		}
		else if(IsConnectorMarked(ObjInfo.ObjObject, LOC_INPUT, InputIdx))
		{
			ObjInfo.Inputs(InputIdx).Color = ConnectionMarkedColor;
		}
	}

	for(INT OutputIdx=0; OutputIdx<ObjInfo.Outputs.Num(); OutputIdx++)
	{
		if((LinkedObjVC->MouseOverConnType==LOC_OUTPUT || DblClickConnType==LOC_OUTPUT)
			&& IsConnectorHighlighted(ObjInfo.ObjObject, LOC_OUTPUT, OutputIdx))
		{
			ObjInfo.Outputs(OutputIdx).Color = ConnectionSelectedColor;
		}
		else if(IsConnectorMarked(ObjInfo.ObjObject, LOC_OUTPUT, OutputIdx))
		{
			ObjInfo.Outputs(OutputIdx).Color = ConnectionMarkedColor;
		}
	}
}

/** @return Returns whether the specified connector is currently highlighted or not. */
UBOOL WxMaterialEditor::IsConnectorHighlighted(UObject* Object, BYTE ConnType, INT ConnIndex)
{
	UBOOL bResult = FALSE;

	if((Object==LinkedObjVC->MouseOverObject && ConnIndex==LinkedObjVC->MouseOverConnIndex && ConnType==LinkedObjVC->MouseOverConnType)
	||(Object==DblClickObject && ConnIndex==DblClickConnIndex && ConnType==DblClickConnType))
	{
		bResult = TRUE;
	}

	return bResult;
}

/** @return Returns whether the specified connector is currently marked or not. */
UBOOL WxMaterialEditor::IsConnectorMarked(UObject* Object, BYTE ConnType, INT ConnIndex)
{
	UBOOL bResult = FALSE;

	if (Object == MarkedObject && ConnIndex == MarkedConnIndex && ConnType == MarkedConnType)
	{
		bResult = TRUE;
	}

	return bResult;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FLinkedObjViewportClient interface
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WxMaterialEditor::DrawObjects(FViewport* Viewport, FCanvas* Canvas)
{
	if (BackgroundTexture != NULL)
	{
		Clear( Canvas, CanvasColor );

		Canvas->PushAbsoluteTransform(FMatrix::Identity);

		const INT ViewWidth = LinkedObjVC->Viewport->GetSizeX();
		const INT ViewHeight = LinkedObjVC->Viewport->GetSizeY();

		// draw the texture to the side, stretched vertically
		FLinkedObjDrawUtils::DrawTile( Canvas, ViewWidth - BackgroundTexture->SizeX, 0,
			BackgroundTexture->SizeX, ViewHeight,
			0.f, 0.f,
			1.f, 1.f,
			FLinearColor::White,
			BackgroundTexture->Resource );

		// stretch the left part of the texture to fill the remaining gap
		if (ViewWidth > BackgroundTexture->SizeX)
		{
			FLinkedObjDrawUtils::DrawTile( Canvas, 0, 0,
				ViewWidth - BackgroundTexture->SizeX, ViewHeight,
				0.f, 0.f,
				0.1f, 0.1f,
				FLinearColor::White,
				BackgroundTexture->Resource );
		}

		Canvas->PopTransform();
	}

	// Draw comments.
	DrawCommentExpressions( Canvas );

	if (!EditMaterialFunction)
	{
		// Draw the root node -- that is, the node corresponding to the final material.
		DrawMaterialRoot( FALSE, Canvas );
	}
	
	TArray<FLinkedObjDrawInfo> LinkedObjDrawInfos;
	LinkedObjDrawInfos.Empty(Material->Expressions.Num());
	LinkedObjDrawInfos.AddZeroed(Material->Expressions.Num());

	// Draw the material expression nodes.
	for( INT ExpressionIndex = 0 ; ExpressionIndex < Material->Expressions.Num() ; ++ExpressionIndex )
	{
		UMaterialExpression* MaterialExpression = Material->Expressions( ExpressionIndex );
		// @hack DB: For some reason, materials that were resaved in gemini have comment expressions in the material's
		// @hack DB: expressions list.  The check below is required to prevent them from rendering as normal expression nodes.
		if ( !MaterialExpression->IsA(UMaterialExpressionComment::StaticClass()) )
		{
			const UBOOL bExpressionSelected = SelectedExpressions.ContainsItem( MaterialExpression );
			DrawMaterialExpression( MaterialExpression, bExpressionSelected, Canvas, LinkedObjDrawInfos(ExpressionIndex) );
		}
	}

	if (!EditMaterialFunction)
	{
		// Render connections between the material's inputs and the material expression outputs connected to them.
		DrawMaterialRootConnections( Canvas );
	}

	// Render connectors between material expressions' inputs and the material expression outputs connected to them.
	for( INT ExpressionIndex = 0 ; ExpressionIndex < Material->Expressions.Num() ; ++ExpressionIndex )
	{
		UMaterialExpression* MaterialExpression = Material->Expressions( ExpressionIndex );
		const UBOOL bExpressionSelected = SelectedExpressions.ContainsItem( MaterialExpression );
		DrawMaterialExpressionConnections( MaterialExpression, Canvas );
	}

	// Draw the material expression comments.
	for( INT ExpressionIndex = 0 ; ExpressionIndex < Material->Expressions.Num() ; ++ExpressionIndex )
	{
		UMaterialExpression* MaterialExpression = Material->Expressions( ExpressionIndex );
		DrawMaterialExpressionComments( MaterialExpression, Canvas );
	}

	if (LinkedObjVC->MouseOverObject)
	{
		DrawMaterialExpressionToolTips(LinkedObjDrawInfos, Canvas);
	}

	const FMaterialResource* MaterialResource = Material->GetMaterialResource();
	
	if( bShowStats)
	{
		INT DrawPositionY = 5;
		
		Canvas->PushAbsoluteTransform(FMatrix::Identity);

		TArray<FString> CompileErrors;
		
		if (EditMaterialFunction && ExpressionPreviewMaterial)
		{
			// Add a compile error message for functions missing an output
			CompileErrors = ExpressionPreviewMaterial->GetMaterialResource()->GetCompileErrors();

			UBOOL bFoundFunctionOutput = FALSE;
			for (INT ExpressionIndex = 0; ExpressionIndex < Material->Expressions.Num(); ExpressionIndex++)
			{
				if (Material->Expressions(ExpressionIndex)->IsA(UMaterialExpressionFunctionOutput::StaticClass()))
				{
					bFoundFunctionOutput = TRUE;
					break;
				}
			}

			if (!bFoundFunctionOutput)
			{
				CompileErrors.AddItem(TEXT("Missing a function output"));
			}
		}
		else
		{
			CompileErrors = MaterialResource->GetCompileErrors();
		}

		// Only draw material stats if enabled.
		DrawMaterialInfoStrings(Canvas, Material, MaterialResource, CompileErrors, DrawPositionY, EditMaterialFunction == NULL);
	
		Canvas->PopTransform();
	}
}

/**
 * Called when the user right-clicks on an empty region of the expression viewport.
 */
void WxMaterialEditor::OpenNewObjectMenu()
{
	// The user has clicked on the background, so clear the last connector object reference so that
	// any newly created material expression node will not be automatically connected to the
	// connector last clicked upon.
	ConnObj = NULL;

	WxMaterialEditorContextMenu_NewNode Menu( this );
	FTrackPopupMenu tpm( this, &Menu );
	tpm.Show();
}


/**
 * Called when the user right-clicks on an object in the viewport.
 */
void WxMaterialEditor::OpenObjectOptionsMenu()
{
	WxMaterialEditorContextMenu_NodeOptions Menu( this );
	FTrackPopupMenu tpm( this, &Menu );
	tpm.Show();
}

/**
 * Called when the user right-clicks on an object connector in the viewport.
 */
void WxMaterialEditor::OpenConnectorOptionsMenu()
{
	WxMaterialEditorContextMenu_ConnectorOptions Menu( this );
	FTrackPopupMenu tpm( this, &Menu );
	tpm.Show();
}



/**
 * Updates the editor's property window to contain material expression properties if any are selected.
 * Otherwise, properties for the material are displayed.
 */
void WxMaterialEditor::UpdatePropertyWindow()
{
	if( SelectedExpressions.Num() == 0 && SelectedComments.Num() == 0 )
	{
		UObject* EditObject = Material;
		if (EditMaterialFunction)
		{
			EditObject = EditMaterialFunction;
		}

		PropertyWindow->SetObject( EditObject, EPropertyWindowFlags::ShouldShowCategories | EPropertyWindowFlags::Sorted);
	}
	else
	{
		TArray<UMaterialExpression*> SelectedObjects;

		// Expressions
		for ( INT Idx = 0 ; Idx < SelectedExpressions.Num() ; ++Idx )
		{
			SelectedObjects.AddItem( SelectedExpressions(Idx) );
		}

		// Comments
		for ( INT Idx = 0 ; Idx < SelectedComments.Num() ; ++Idx )
		{
			SelectedObjects.AddItem( (UMaterialExpression*)SelectedComments(Idx) );
		}

		PropertyWindow->SetObjectArray( SelectedObjects, EPropertyWindowFlags::ShouldShowCategories | EPropertyWindowFlags::Sorted);
	}
}

/**
 * Deselects all selected material expressions and comments.
 */
void WxMaterialEditor::EmptySelection()
{
	SelectedExpressions.Empty();
	SelectedComments.Empty();

	SourceWindow->SelectedExpression = NULL;
	SourceWindow->Redraw();
}

/**
 * Add the specified object to the list of selected objects
 */
void WxMaterialEditor::AddToSelection(UObject* Obj)
{
	if( Obj->IsA(UMaterialExpressionComment::StaticClass()) )
	{
		SelectedComments.AddUniqueItem( static_cast<UMaterialExpressionComment*>(Obj) );
	}
	else if( Obj->IsA(UMaterialExpression::StaticClass()) )
	{
		SelectedExpressions.AddUniqueItem( static_cast<UMaterialExpression*>(Obj) );

		SourceWindow->SelectedExpression = static_cast<UMaterialExpression*>(Obj);
		SourceWindow->Redraw();
	}
}

/**
 * Remove the specified object from the list of selected objects.
 */
void WxMaterialEditor::RemoveFromSelection(UObject* Obj)
{
	if( Obj->IsA(UMaterialExpressionComment::StaticClass()) )
	{
		SelectedComments.RemoveItem( static_cast<UMaterialExpressionComment*>(Obj) );
	}
	else if( Obj->IsA(UMaterialExpression::StaticClass()) )
	{
		SelectedExpressions.RemoveItem( static_cast<UMaterialExpression*>(Obj) );
	}
}

/**
 * Checks whether the specified object is currently selected.
 *
 * @return	TRUE if the specified object is currently selected
 */
UBOOL WxMaterialEditor::IsInSelection(UObject* Obj) const
{
	UBOOL bIsInSelection = FALSE;
	if( Obj->IsA(UMaterialExpressionComment::StaticClass()) )
	{
		bIsInSelection = SelectedComments.ContainsItem( static_cast<UMaterialExpressionComment*>(Obj) );
	}
	else if( Obj->IsA(UMaterialExpression::StaticClass()) )
	{
		bIsInSelection = SelectedExpressions.ContainsItem( static_cast<UMaterialExpression*>(Obj) );
	}
	return bIsInSelection;
}

/**
 * Returns the number of selected objects
 */
INT WxMaterialEditor::GetNumSelected() const
{
	return SelectedExpressions.Num() + SelectedComments.Num();
}

/**
 * Called when the user clicks on an unselected link connector.
 *
 * @param	Connector	the link connector that was clicked on
 */
void WxMaterialEditor::SetSelectedConnector(FLinkedObjectConnector& Connector)
{
	ConnObj = Connector.ConnObj;
	ConnType = Connector.ConnType;
	ConnIndex = Connector.ConnIndex;
}

/**
 * Gets the position of the selected connector.
 *
 * @return	an FIntPoint that represents the position of the selected connector, or (0,0)
 *			if no connectors are currently selected
 */
FIntPoint WxMaterialEditor::GetSelectedConnLocation(FCanvas* Canvas)
{
	if( ConnObj )
	{
		UMaterialExpression* ExpressionNode = Cast<UMaterialExpression>( ConnObj );
		if( ExpressionNode )
		{
			return GetExpressionConnectionLocation( ExpressionNode, ConnType, ConnIndex );
		}

		UMaterial* MaterialNode = Cast<UMaterial>( ConnObj );
		if( MaterialNode )
		{
			return GetMaterialConnectionLocation( MaterialNode, ConnType, ConnIndex );
		}
	}

	return FIntPoint(0,0);
}

/**
 * Gets the EConnectorHitProxyType for the currently selected connector
 *
 * @return	the type for the currently selected connector, or 0 if no connector is selected
 */
INT WxMaterialEditor::GetSelectedConnectorType()
{
	return ConnType;
}

/**
 * Makes a connection between connectors.
 */
void WxMaterialEditor::MakeConnectionToConnector(FLinkedObjectConnector& Connector)
{
	MakeConnectionToConnector(Connector.ConnObj, Connector.ConnType, Connector.ConnIndex);
}

/** Makes a connection between the currently selected connector and the passed in end point. */
void WxMaterialEditor::MakeConnectionToConnector(UObject* EndObject, INT EndConnType, INT EndConnIndex)
{
	// Nothing to do if object pointers are NULL.
	if ( !EndObject && !ConnObj )
	{	
		return;
	}

	if ( !EndObject || !ConnObj )
	{
		return;
	}

	// Avoid connections to yourself.
	if( EndObject == ConnObj )
	{
		if (ConnType == EndConnType)
		{
			//allow for moving a connection
		}
		else
		{
			return;
		}
	}

	UBOOL bConnectionWasMade = FALSE;

	UMaterial* EndConnMaterial						= Cast<UMaterial>( EndObject );
	UMaterialExpression* EndConnMaterialExpression	= Cast<UMaterialExpression>( EndObject );
	check( EndConnMaterial || EndConnMaterialExpression );

	UMaterial* ConnMaterial							= Cast<UMaterial>( ConnObj );
	UMaterialExpression* ConnMaterialExpression		= Cast<UMaterialExpression>( ConnObj );
	check( ConnMaterial || ConnMaterialExpression );

	// Material to . . .
	if ( ConnMaterial )
	{
		// Materials are only connected along their right side.
		check( ConnType == LOC_OUTPUT ); 

		// Material to expression.
		if ( EndConnMaterialExpression )
		{
			// Material expressions can only connect to their materials on their left side.
			if ( EndConnType == LOC_INPUT ) 
			{
				const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("MaterialEditorMakeConnection")) );
				Material->Modify();
				ConnectMaterialToMaterialExpression( ConnMaterial, ConnIndex, EndConnMaterialExpression, EndConnIndex, bHideUnusedConnectors );
				bConnectionWasMade = TRUE;
			}
		}
	}
	// Expression to . . .
	else if ( ConnMaterialExpression )
	{
		// Expression to expression.
		if ( EndConnMaterialExpression )
		{
			if ( ConnType != EndConnType )
			{
				UMaterialExpression* InputExpression;
				UMaterialExpression* OutputExpression;
				INT InputIndex;
				INT OutputIndex;

				if( ConnType == LOC_OUTPUT && EndConnType == LOC_INPUT )
				{
					InputExpression = ConnMaterialExpression;
					InputIndex = ConnIndex;
					OutputExpression = EndConnMaterialExpression;
					OutputIndex = EndConnIndex;
				}
				else
				{
					InputExpression = EndConnMaterialExpression;
					InputIndex = EndConnIndex;
					OutputExpression = ConnMaterialExpression;
					OutputIndex = ConnIndex;
				}

				// Input expression.				
				FExpressionInput* ExpressionInput = InputExpression->GetInput(InputIndex);

				// Output expression.
				TArray<FExpressionOutput>& Outputs = OutputExpression->GetOutputs();
				if (Outputs.Num() > 0)
				{
					const FExpressionOutput& ExpressionOutput = Outputs( OutputIndex );

					const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("MaterialEditorMakeConnection")) );
					InputExpression->Modify();
					ConnectExpressions( *ExpressionInput, ExpressionOutput, OutputIndex, OutputExpression );

					bConnectionWasMade = TRUE;
				}
			}
			else if (ConnType == LOC_INPUT && EndConnType == LOC_INPUT)
			{
				//if we are attempting to move outbound (leftside) connections from one connection to another
				UMaterialExpression* SourceMaterialExpression = ConnMaterialExpression;
				UMaterialExpression* DestMaterialExpression = EndConnMaterialExpression;

				//get the output from our SOURCE
				TArray<FExpressionOutput>& SourceOutputs = SourceMaterialExpression->GetOutputs();
				INT SourceIndex = ConnIndex;
				FExpressionOutput& SourceExpressionOutput = SourceOutputs( SourceIndex );

				//get a list of inputs that reference the SOURCE output
				TArray<FMaterialExpressionReference> ReferencingInputs;
				GetListOfReferencingInputs( SourceMaterialExpression, Material, ReferencingInputs, &SourceExpressionOutput,SourceIndex );
				if (ReferencingInputs.Num())
				{
					const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("MaterialEditorMoveConnections")) );
					for ( INT RefIndex = 0; RefIndex < ReferencingInputs.Num(); RefIndex++ )
					{
						if (ReferencingInputs(RefIndex).Expression)
						{
							ReferencingInputs(RefIndex).Expression->Modify();
						}
						else
						{
							Material->Modify();
						}

						//Get the inputs associated with this reference...
						FMaterialExpressionReference &InputExpressionReference = ReferencingInputs(RefIndex);
						for ( INT InputIndex = 0; InputIndex < InputExpressionReference.Inputs.Num(); InputIndex++ )
						{
							FExpressionInput* Input = InputExpressionReference.Inputs(InputIndex);
							check( Input->Expression == SourceMaterialExpression );	
							
							if (InputExpressionReference.Expression != DestMaterialExpression) //ensure we're not connecting to ourselves
							{
								//break the existing link				
								Input->Expression = NULL;

								//connect to our DEST
								TArray<FExpressionOutput>& DestOutputs = DestMaterialExpression->GetOutputs();
								INT DestIndex = EndConnIndex;
								FExpressionOutput& DestExpressionOutput = DestOutputs( DestIndex );

								//const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("MaterialEditorMakeConnection")) );
								DestMaterialExpression->Modify();
								ConnectExpressions( *Input, DestExpressionOutput, DestIndex, DestMaterialExpression );

								bConnectionWasMade = TRUE;
							}
						}
					}
				}
			}
			else if (ConnType == LOC_OUTPUT && EndConnType == LOC_OUTPUT)
			{
				//if we are attempting to move inbound (rightside) connections from one connection to another 
				UMaterialExpression* SourceMaterialExpression = ConnMaterialExpression;
				UMaterialExpression* DestMaterialExpression = EndConnMaterialExpression;
				INT SourceIndex = ConnIndex;
				INT DestIndex = EndConnIndex;

				//get the input from our SOURCE
				FExpressionInput* SourceExpressionInput = SourceMaterialExpression->GetInput(SourceIndex);

				if (SourceExpressionInput->Expression && SourceExpressionInput->Expression != DestMaterialExpression)
				{
					const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("MaterialMoveConnections")) );
					SourceMaterialExpression->Modify();
					DestMaterialExpression->Modify();
					
					//get the connected output
					INT OutputIndex = SourceExpressionInput->OutputIndex;
					TArray<FExpressionOutput>& Outputs = SourceExpressionInput->Expression->GetOutputs();
					check(Outputs.Num() > 0);
					FExpressionOutput& ExpressionOutput = Outputs( OutputIndex );
						
					//get the DEST input
					FExpressionInput* DestExpressionInput = DestMaterialExpression->GetInput(DestIndex);
					
					//make the new connection
					ConnectExpressions(*DestExpressionInput, ExpressionOutput, OutputIndex, SourceExpressionInput->Expression);
					
					//break the old connections
					SourceExpressionInput->Expression = NULL;

					bConnectionWasMade = TRUE;
				}
											
			}
			
		}
		// Expression to Material.
		else if ( EndConnMaterial )
		{
			// Materials are only connected along their right side.
			check( EndConnType == LOC_OUTPUT ); 

			// Material expressions can only connect to their materials on their left side.
			if ( ConnType == LOC_INPUT ) 
			{
				const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("MaterialEditorMakeConnection")) );
				Material->Modify();
				ConnectMaterialToMaterialExpression( EndConnMaterial, EndConnIndex, ConnMaterialExpression, ConnIndex, bHideUnusedConnectors );
				bConnectionWasMade = TRUE;
			}
		}
	}
	
	if ( bConnectionWasMade )
	{
		// Update the current preview material.
		UpdatePreviewMaterial();

		Material->MarkPackageDirty();
		RefreshSourceWindowMaterial();
		RefreshExpressionPreviews();
		RefreshExpressionViewport();
		bMaterialDirty = TRUE;
	}
}

/**
 * Breaks the link currently indicated by ConnObj/ConnType/ConnIndex.
 */
void WxMaterialEditor::BreakConnLink(UBOOL bOnlyIfMouseOver)
{
	if ( !ConnObj )
	{
		return;
	}

	UBOOL bConnectionWasBroken = FALSE;

	UMaterialExpression* MaterialExpression	= Cast<UMaterialExpression>( ConnObj );
	UMaterial* MaterialNode					= Cast<UMaterial>( ConnObj );

	if ( ConnType == LOC_OUTPUT ) // right side
	{
		if (MaterialNode && (!bOnlyIfMouseOver || IsConnectorHighlighted(MaterialNode, ConnType, ConnIndex)))
		{
			// Clear the material input.
			FExpressionInput* MaterialInput = GetMaterialInputConditional( MaterialNode, ConnIndex, bHideUnusedConnectors );
			if ( MaterialInput->Expression != NULL )
			{
				const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("MaterialEditorBreakConnection")) );
				Material->Modify();
				MaterialInput->Expression = NULL;
				MaterialInput->OutputIndex = INDEX_NONE;

				bConnectionWasBroken = TRUE;
			}
		}
		else if (MaterialExpression && (!bOnlyIfMouseOver || IsConnectorHighlighted(MaterialExpression, ConnType, ConnIndex)))
		{
			FExpressionInput* Input = MaterialExpression->GetInput(ConnIndex);

			const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("MaterialEditorBreakConnection")) );
			MaterialExpression->Modify();
			Input->Expression = NULL;
			Input->OutputIndex = INDEX_NONE;

			bConnectionWasBroken = TRUE;
		}
	}
	else if ( ConnType == LOC_INPUT ) // left side
	{
		// Only expressions can have connectors on the left side.
		check( MaterialExpression );
		if (!bOnlyIfMouseOver || IsConnectorHighlighted(MaterialExpression, ConnType, ConnIndex))
		{
			TArray<FExpressionOutput>& Outputs = MaterialExpression->GetOutputs();
			const FExpressionOutput& ExpressionOutput = Outputs( ConnIndex );

			// Get a list of inputs that refer to the selected output.
			TArray<FMaterialExpressionReference> ReferencingInputs;
			GetListOfReferencingInputs( MaterialExpression, Material, ReferencingInputs, &ExpressionOutput, ConnIndex );

			bConnectionWasBroken = ReferencingInputs.Num() > 0;
			if ( bConnectionWasBroken )
			{
				// Clear all referencing inputs.
				const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("MaterialEditorBreakConnection")) );
				for ( INT RefIndex = 0 ; RefIndex < ReferencingInputs.Num() ; ++RefIndex )
				{
					if (ReferencingInputs(RefIndex).Expression)
					{
						ReferencingInputs(RefIndex).Expression->Modify();
					}
					else
					{
						Material->Modify();
					}
					for ( INT InputIndex = 0 ; InputIndex < ReferencingInputs(RefIndex).Inputs.Num() ; ++InputIndex )
					{
						FExpressionInput* Input = ReferencingInputs(RefIndex).Inputs(InputIndex);
						check( Input->Expression == MaterialExpression );
						Input->Expression = NULL;
						Input->OutputIndex = INDEX_NONE;
					}
				}
			}
		}
	}

	if ( bConnectionWasBroken )
	{
		// Update the current preview material.
		UpdatePreviewMaterial();

		Material->MarkPackageDirty();
		RefreshSourceWindowMaterial();
		RefreshExpressionPreviews();
		RefreshExpressionViewport();
		bMaterialDirty = TRUE;
	}
}

/** Updates the marked connector based on what's currently under the mouse, and potentially makes a new connection. */
void WxMaterialEditor::UpdateMarkedConnector()
{
	if (LinkedObjVC->MouseOverObject == MarkedObject && LinkedObjVC->MouseOverConnType == MarkedConnType && LinkedObjVC->MouseOverConnIndex == MarkedConnIndex)
	{
		MarkedObject = NULL;
		MarkedConnType = INDEX_NONE;
		MarkedConnIndex = INDEX_NONE;
	}
	else
	{
		// Only make a connection if both ends are valid
		if (MarkedConnType >= 0 && MarkedConnIndex >= 0)
		{
			MakeConnectionToConnector(MarkedObject, MarkedConnType, MarkedConnIndex);
		}
		
		MarkedObject = LinkedObjVC->MouseOverObject;
		MarkedConnType = LinkedObjVC->MouseOverConnType;
		MarkedConnIndex = LinkedObjVC->MouseOverConnIndex;
	}
}

/**
 * Connects an expression output to the specified material input slot.
 *
 * @param	MaterialInputIndex		Index to the material input (Diffuse=0, Emissive=1, etc).
 */
void WxMaterialEditor::OnConnectToMaterial(INT MaterialInputIndex)
{
	// Checks if over expression connection.
	UMaterialExpression* MaterialExpression = NULL;
	if ( ConnObj )
	{
		MaterialExpression = Cast<UMaterialExpression>( ConnObj );
	}

	UBOOL bConnectionWasMade = FALSE;
	if ( MaterialExpression && ConnType == LOC_INPUT )
	{
		UMaterial* MaterialNode = Cast<UMaterial>( ConnObj );

		const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("MaterialEditorMakeConnection")) );
		Material->Modify();
		ConnectMaterialToMaterialExpression(Material, MaterialInputIndex, MaterialExpression, ConnIndex, bHideUnusedConnectors);
		bConnectionWasMade = TRUE;
	}

	if ( bConnectionWasMade && MaterialExpression )
	{
		// Update the current preview material.
		UpdatePreviewMaterial();
		Material->MarkPackageDirty();
		RefreshSourceWindowMaterial();
		RefreshExpressionPreviews();
		RefreshExpressionViewport();
		bMaterialDirty = TRUE;
	}
}

/**
 * Breaks all connections to/from selected material expressions.
 */
void WxMaterialEditor::BreakAllConnectionsToSelectedExpressions()
{
	if ( SelectedExpressions.Num() > 0 )
	{
		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("MaterialEditorBreakAllConnections")) );
			for( INT ExpressionIndex = 0 ; ExpressionIndex < SelectedExpressions.Num() ; ++ExpressionIndex )
			{
				UMaterialExpression* MaterialExpression = SelectedExpressions( ExpressionIndex );
				MaterialExpression->Modify();

				// Break links to expression.
				SwapLinksToExpression(MaterialExpression, NULL, Material);
			}
		}
	
		// Update the current preview material. 
		UpdatePreviewMaterial();
		Material->MarkPackageDirty();
		RefreshSourceWindowMaterial();
		RefreshExpressionPreviews();
		RefreshExpressionViewport();
		bMaterialDirty = TRUE;
	}
}

/** Removes the selected expression from the favorites list. */
void WxMaterialEditor::RemoveSelectedExpressionFromFavorites()
{
	if ( SelectedExpressions.Num() == 1 )
	{
		UMaterialExpression* MaterialExpression = SelectedExpressions(0);
		if (MaterialExpression)
		{
			RemoveMaterialExpressionFromFavorites(MaterialExpression->GetClass());
			EditorOptions->FavoriteExpressions.RemoveItem(MaterialExpression->GetClass()->GetName());
			EditorOptions->SaveConfig();
		}
	}
}

/** Adds the selected expression to the favorites list. */
void WxMaterialEditor::AddSelectedExpressionToFavorites()
{
	if ( SelectedExpressions.Num() == 1 )
	{
		UMaterialExpression* MaterialExpression = SelectedExpressions(0);
		if (MaterialExpression)
		{
			AddMaterialExpressionToFavorites(MaterialExpression->GetClass());
			EditorOptions->FavoriteExpressions.AddUniqueItem(MaterialExpression->GetClass()->GetName());
			EditorOptions->SaveConfig();
		}
	}
}

/**
 * Called when the user releases the mouse over a link connector and is holding the ALT key.
 * Commonly used as a shortcut to breaking connections.
 *
 * @param	Connector	The connector that was ALT+clicked upon.
 */
void WxMaterialEditor::AltClickConnector(FLinkedObjectConnector& Connector)
{
	BreakConnLink(FALSE);
}

/**
 * Called when the user performs a draw operation while objects are selected.
 *
 * @param	DeltaX	the X value to move the objects
 * @param	DeltaY	the Y value to move the objects
 */
void WxMaterialEditor::MoveSelectedObjects(INT DeltaX, INT DeltaY)
{
	const UBOOL bFirstMove = LinkedObjVC->DistanceDragged < 4;
	if ( bFirstMove )
	{
		ExpressionsLinkedToComments.Empty();
	}

	TArray<UMaterialExpression*> ExpressionsToMove;

	// Add selected expressions.
	for( INT ExpressionIndex = 0 ; ExpressionIndex < SelectedExpressions.Num() ; ++ExpressionIndex )
	{
		UMaterialExpression* Expression = SelectedExpressions(ExpressionIndex);
		ExpressionsToMove.AddItem( Expression );
	}

	if ( SelectedComments.Num() > 0 )
	{
		TArray<FIntRect> CommentRects;

		// Add selected comments.  At the same time, create rects for the comments.
		for( INT CommentIndex = 0 ; CommentIndex < SelectedComments.Num() ; ++CommentIndex )
		{
			UMaterialExpressionComment* Comment = SelectedComments(CommentIndex);
			ExpressionsToMove.AddItem( Comment );
			CommentRects.AddItem( FIntRect( FIntPoint(Comment->MaterialExpressionEditorX, Comment->MaterialExpressionEditorY),
											FIntPoint(Comment->MaterialExpressionEditorX+Comment->SizeX, Comment->MaterialExpressionEditorY+Comment->SizeY) ) );
		}

		// If this is the first move, generate a list of expressions that are contained by comments.
		if ( bFirstMove )
		{
			for( INT ExpressionIndex = 0 ; ExpressionIndex < Material->Expressions.Num() ; ++ExpressionIndex )
			{
				UMaterialExpression* Expression = Material->Expressions(ExpressionIndex);
				const FIntPoint ExpressionPos( Expression->MaterialExpressionEditorX, Expression->MaterialExpressionEditorY );
				for( INT CommentIndex = 0 ; CommentIndex < CommentRects.Num() ; ++CommentIndex )
				{
					const FIntRect& CommentRect = CommentRects(CommentIndex);
					if ( CommentRect.Contains(ExpressionPos) )
					{
						ExpressionsLinkedToComments.AddUniqueItem( Expression );
						break;
					}
				}
			}

			// Also check comments to see if they are contained by other comments which are selected
			for ( INT CommentIndex = 0; CommentIndex < Material->EditorComments.Num(); ++CommentIndex )
			{
				UMaterialExpressionComment* CurComment = Material->EditorComments( CommentIndex );
				const FIntPoint ExpressionPos( CurComment->MaterialExpressionEditorX, CurComment->MaterialExpressionEditorY );
				for ( INT CommentRectIndex = 0; CommentRectIndex < CommentRects.Num(); ++CommentRectIndex )
				{
					const FIntRect& CommentRect = CommentRects(CommentRectIndex);
					if ( CommentRect.Contains( ExpressionPos ) )
					{
						ExpressionsLinkedToComments.AddUniqueItem( CurComment );
						break;
					}
				}

			}
		}

		// Merge the expression lists.
		for( INT ExpressionIndex = 0 ; ExpressionIndex < ExpressionsLinkedToComments.Num() ; ++ExpressionIndex )
		{
			UMaterialExpression* Expression = ExpressionsLinkedToComments(ExpressionIndex);
			ExpressionsToMove.AddUniqueItem( Expression );
		}
	}

	// Perform the move.
	if ( ExpressionsToMove.Num() > 0 )
	{
		for( INT ExpressionIndex = 0 ; ExpressionIndex < ExpressionsToMove.Num() ; ++ExpressionIndex )
		{
			UMaterialExpression* Expression = ExpressionsToMove(ExpressionIndex);
			Expression->MaterialExpressionEditorX += DeltaX;
			Expression->MaterialExpressionEditorY += DeltaY;
		}
		Material->MarkPackageDirty();
		bMaterialDirty = TRUE;
	}
}

void WxMaterialEditor::EdHandleKeyInput(FViewport* Viewport, FName Key, EInputEvent Event)
{
	const UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	if( Event == IE_Pressed )
	{
		if ( bCtrlDown )
		{
			if( Key == KEY_C )
			{
				// Clear the material copy buffer and copy the selected expressions into it.
				TArray<UMaterialExpression*> NewExpressions;
				TArray<UMaterialExpression*> NewComments;
				GetCopyPasteBuffer()->Expressions.Empty();
				GetCopyPasteBuffer()->EditorComments.Empty();

				UMaterialExpression::CopyMaterialExpressions( SelectedExpressions, SelectedComments, GetCopyPasteBuffer(), EditMaterialFunction, NewExpressions, NewComments );
			}
			else if ( Key == KEY_V )
			{
				// Paste the material copy buffer into this material.
				PasteExpressionsIntoMaterial( NULL );
			}
			else if( Key == KEY_W )
			{
				DuplicateSelectedObjects();
			}
			else if( Key == KEY_Y )
			{
				Redo();
			}
			else if( Key == KEY_Z )
			{
				Undo();
			}
		}
		else
		{
			if( Key == KEY_Delete )
			{
				DeleteSelectedObjects();
			}
			else if ( Key == KEY_SpaceBar )
			{
				// Spacebar force-refreshes previews.
				ForceRefreshExpressionPreviews();
				RefreshExpressionViewport();
				RefreshPreviewViewport();
			}
			else if ( Key == KEY_LeftMouseButton )
			{
				// Check if the "Toggle realtime preview" icon was clicked on.
				Viewport->Invalidate();
				const INT HitX = Viewport->GetMouseX();
				const INT HitY = Viewport->GetMouseY();
				HHitProxy*	HitResult = Viewport->GetHitProxy( HitX, HitY );
				if ( HitResult && HitResult->IsA( HRealtimePreviewProxy::StaticGetType() ) )
				{
					// Toggle the material expression's realtime preview state.
					UMaterialExpression* MaterialExpression = static_cast<HRealtimePreviewProxy*>( HitResult )->MaterialExpression;
					check( MaterialExpression );
					{
						const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("MaterialEditorTogglePreview")) );
						MaterialExpression->Modify();
						MaterialExpression->bRealtimePreview = !MaterialExpression->bRealtimePreview;
					}
					MaterialExpression->PreEditChange( NULL );
					MaterialExpression->PostEditChange();
					MaterialExpression->MarkPackageDirty();
					bMaterialDirty = TRUE;

					// Update the preview.
					RefreshExpressionPreview( MaterialExpression, TRUE );
					RefreshPreviewViewport();
				}

				// Clear the double click selection next time the mouse is clicked
				DblClickObject = NULL;
				DblClickConnType = INDEX_NONE;
				DblClickConnIndex = INDEX_NONE;
			}
			else if ( Key == KEY_RightMouseButton )
			{
				// Clear the double click selection next time the mouse is clicked
				DblClickObject = NULL;
				DblClickConnType = INDEX_NONE;
				DblClickConnIndex = INDEX_NONE;
			}
		}
		if ( Key == KEY_Enter )
		{
			if (bMaterialDirty)
			{
				UpdateOriginalMaterial();
			}
		}
	}
	else if ( Event == IE_Released )
	{
		// LMBRelease + hotkey places certain material expression types.
		if( Key == KEY_LeftMouseButton )
		{
			const UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);
			if( bShiftDown && Viewport->KeyState(KEY_C))
			{
				CreateNewCommentExpression();
			}
			else if ( bCtrlDown )
			{
				BreakConnLink(TRUE);
			}
			else if ( bShiftDown )
			{
				UpdateMarkedConnector();
			}
			else
			{
				struct FMaterialExpressionHotkey
				{
					FName		Key;
					UClass*		MaterialExpressionClass;
				};
				static FMaterialExpressionHotkey MaterialExpressionHotkeys[] =
				{
					{ KEY_A, UMaterialExpressionAdd::StaticClass() },
					{ KEY_B, UMaterialExpressionBumpOffset::StaticClass() },
					{ KEY_C, UMaterialExpressionComponentMask::StaticClass() },
					{ KEY_D, UMaterialExpressionDivide::StaticClass() },
					{ KEY_E, UMaterialExpressionPower::StaticClass() },
					{ KEY_F, UMaterialExpressionMaterialFunctionCall::StaticClass() },
					{ KEY_I, UMaterialExpressionIf::StaticClass() },
					{ KEY_L, UMaterialExpressionLinearInterpolate::StaticClass() },
					{ KEY_M, UMaterialExpressionMultiply::StaticClass() },
					{ KEY_N, UMaterialExpressionNormalize::StaticClass() },
					{ KEY_O, UMaterialExpressionOneMinus::StaticClass() },
					{ KEY_P, UMaterialExpressionPanner::StaticClass() },
					{ KEY_R, UMaterialExpressionReflectionVector::StaticClass() },
					{ KEY_S, UMaterialExpressionScalarParameter::StaticClass() },
					{ KEY_T, UMaterialExpressionTextureSample::StaticClass() },
					{ KEY_U, UMaterialExpressionTextureCoordinate::StaticClass() },
					{ KEY_V, UMaterialExpressionVectorParameter::StaticClass() },
					{ KEY_One, UMaterialExpressionConstant::StaticClass() },
					{ KEY_Two, UMaterialExpressionConstant2Vector::StaticClass() },
					{ KEY_Three, UMaterialExpressionConstant3Vector::StaticClass() },
					{ KEY_Four, UMaterialExpressionConstant4Vector::StaticClass() },
					{ NAME_None, NULL },
				};

				// Iterate over all expression hotkeys, reporting the first expression that has a key down.
				UClass* NewMaterialExpressionClass = NULL;
				for ( INT Index = 0 ; ; ++Index )
				{
					const FMaterialExpressionHotkey& ExpressionHotKey = MaterialExpressionHotkeys[Index];
					if ( ExpressionHotKey.MaterialExpressionClass )
					{
						if ( Viewport->KeyState(ExpressionHotKey.Key) )
						{
							NewMaterialExpressionClass = ExpressionHotKey.MaterialExpressionClass;
							break;
						}
					}
					else
					{
						// A NULL MaterialExpressionClass indicates end of list.
						break;
					}
				}

				// Create a new material expression if the associated hotkey was found to be down.
				if ( NewMaterialExpressionClass )
				{
					const INT LocX = (LinkedObjVC->NewX - LinkedObjVC->Origin2D.X)/LinkedObjVC->Zoom2D;
					const INT LocY = (LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y)/LinkedObjVC->Zoom2D;
					CreateNewMaterialExpression( NewMaterialExpressionClass, FALSE, TRUE, TRUE, FIntPoint(LocX, LocY) );
				}
			}
		}
	}
}

/**
 * Called once the user begins a drag operation.  Transactions expression and comment position.
 */
void WxMaterialEditor::BeginTransactionOnSelected()
{
	check( !ScopedTransaction );
	ScopedTransaction = new FScopedTransaction( *LocalizeUnrealEd(TEXT("LinkedObjectModify")) );

	Material->Modify();
	for( INT MaterialExpressionIndex = 0 ; MaterialExpressionIndex < Material->Expressions.Num() ; ++MaterialExpressionIndex )
	{
		UMaterialExpression* MaterialExpression = Material->Expressions( MaterialExpressionIndex );
		MaterialExpression->Modify();
	}
	for( INT MaterialExpressionIndex = 0 ; MaterialExpressionIndex < Material->EditorComments.Num() ; ++MaterialExpressionIndex )
	{
		UMaterialExpressionComment* Comment = Material->EditorComments( MaterialExpressionIndex );
		Comment->Modify();
	}
}

/**
 * Called when the user releases the mouse button while a drag operation is active.
 */
void WxMaterialEditor::EndTransactionOnSelected()
{
	check( ScopedTransaction );
	delete ScopedTransaction;
	ScopedTransaction = NULL;
}


/**
 *	Handling for dragging on 'special' hit proxies. Should only have 1 object selected when this is being called. 
 *	In this case used for handling resizing handles on Comment objects. 
 *
 *	@param	DeltaX			Horizontal drag distance this frame (in pixels)
 *	@param	DeltaY			Vertical drag distance this frame (in pixels)
 *	@param	SpecialIndex	Used to indicate different classes of action. @see HLinkedObjProxySpecial.
 */
void WxMaterialEditor::SpecialDrag( INT DeltaX, INT DeltaY, INT NewX, INT NewY, INT SpecialIndex )
{
	// Can only 'special drag' one object at a time.
	if( SelectedComments.Num() == 1 )
	{
		if ( SpecialIndex == 1 )
		{
			// Apply dragging to the comment size.
			UMaterialExpressionComment* Comment = SelectedComments(0);
			Comment->SizeX += DeltaX;
			Comment->SizeX = ::Max<INT>(Comment->SizeX, 64);

			Comment->SizeY += DeltaY;
			Comment->SizeY = ::Max<INT>(Comment->SizeY, 64);
			Comment->MarkPackageDirty();
			bMaterialDirty = TRUE;
		}
	}
}
/**
* Updates Material Instance Editors
* @param		MatInst	Material Instance to search dependent editors and force refresh
*/
void WxMaterialEditor::RebuildMaterialInstanceEditors(UMaterialInstance * MatInst)
{

	TArray<FTrackableEntry> TrackableWindows;
	WxTrackableWindow::GetTrackableWindows( TrackableWindows );
	for(INT WinIdx=0; WinIdx<TrackableWindows.Num(); WinIdx++)
	{
		wxWindow* Window = TrackableWindows(WinIdx).Window;
		if (Window )
		{
			WxMaterialInstanceConstantEditor* MaterialInstanceConstantEditor = wxDynamicCast(Window, WxMaterialInstanceConstantEditor);
			if(MaterialInstanceConstantEditor)
			{					
				if ( MaterialInstanceConstantEditor->MaterialEditorInstance )
				{
					UMaterialInstanceConstant* SourceInstance = MaterialInstanceConstantEditor->MaterialEditorInstance->SourceInstance;
					if (SourceInstance )
					{
						UMaterial * MIC_OrginalMaterial = SourceInstance->GetMaterial();
						if (MIC_OrginalMaterial == OriginalMaterial)
						{
							MaterialInstanceConstantEditor->MaterialEditorInstance->RegenerateArrays();
							MaterialInstanceConstantEditor->PropertyWindow->RequestReconnectToData();
						}
					}
				}
			}
		}
	}			
}
/**
 * Updates the original material with the changes made in the editor
 */
void WxMaterialEditor::UpdateOriginalMaterial()
{
	// If the Material has compilation errors, warn the user
	if( Material->GetMaterialResource()->GetCompileErrors().Num() > 0 )
	{
		WxSuppressableWarningDialog CompileErrorsWarning( LocalizeUnrealEd( "Warning_CompileErrorsInMaterial" ), LocalizeUnrealEd( "Warning_CompileErrorsInMaterial_Title" ), "Warning_CompileErrorsInMaterial", TRUE );
		if( CompileErrorsWarning.ShowModal() == wxID_CANCEL )
		{
			return;
		}
	}

	// Cannot save material to a cooked package
	if( Material->GetOutermost()->PackageFlags & PKG_Cooked )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_OperationDisallowedOnCookedContent") );
		return;
	}

	//remove any memory copies of shader files, so they will be reloaded from disk
	//this way the material editor can be used for quick shader iteration
	FlushShaderFileCache();

	//recompile and refresh the preview material so it will be updated if there was a shader change
	UpdatePreviewMaterial();

	const FScopedBusyCursor BusyCursor;

	const FString LocalizedMaterialEditorApply( LocalizeUnrealEd("ToolTip_MaterialEditorApply") );
	GWarn->BeginSlowTask( *LocalizedMaterialEditorApply, TRUE );
	GWarn->StatusUpdatef( 1, 1, *LocalizedMaterialEditorApply );

	// Handle propagation of the material function being edited
	if (EditMaterialFunction)
	{
		// Copy the expressions back from the preview material
		EditMaterialFunction->FunctionExpressions = Material->Expressions;
		EditMaterialFunction->FunctionEditorComments = Material->EditorComments;

		// overwrite the original material function in place by constructing a new one with the same name
		EditMaterialFunction->ParentFunction = (UMaterialFunction*)UObject::StaticDuplicateObject(
			EditMaterialFunction, 
			EditMaterialFunction, 
			EditMaterialFunction->ParentFunction->GetOuter(), 
			*EditMaterialFunction->ParentFunction->GetName(), 
			~0, 
			EditMaterialFunction->ParentFunction->GetClass());

		// Restore RF_Standalone on the original material function, as it had been removed from the preview material so that it could be GC'd.
		EditMaterialFunction->ParentFunction->SetFlags( RF_Standalone );

		for (INT ExpressionIndex = 0; ExpressionIndex < EditMaterialFunction->ParentFunction->FunctionExpressions.Num(); ExpressionIndex++)
		{
			UMaterialExpression* CurrentExpression = EditMaterialFunction->ParentFunction->FunctionExpressions(ExpressionIndex);

			// Link the expressions back to their function
			CurrentExpression->Material = NULL;
			CurrentExpression->Function = EditMaterialFunction->ParentFunction;
		}

		for (INT ExpressionIndex = 0; ExpressionIndex < EditMaterialFunction->ParentFunction->FunctionEditorComments.Num(); ExpressionIndex++)
		{
			UMaterialExpressionComment* CurrentExpression = EditMaterialFunction->ParentFunction->FunctionEditorComments(ExpressionIndex);

			// Link the expressions back to their function
			CurrentExpression->Material = NULL;
			CurrentExpression->Function = EditMaterialFunction->ParentFunction;
		}

		// mark the parent function as changed
		EditMaterialFunction->ParentFunction->PreEditChange(NULL);
		EditMaterialFunction->ParentFunction->PostEditChange();
		EditMaterialFunction->ParentFunction->MarkPackageDirty();

		// clear the dirty flag
		bMaterialDirty = FALSE;

		// Update any open material editor function libraries (including the one used by this material editor), in case this function's categories have changed
		for (TMap<FString, WxMaterialEditor*>::TIterator It(ActiveMaterialEditors); It; ++It)
		{
			It.Value()->RebuildMaterialFunctionLibrary();
		}

		{
			// Detach components while we are propagating changes to dependent materials to make sure that we don't reattach them multiple times
			// This is also necessary to propagate changes to preview materials since those are not reattached in UMaterial::PostEditChangeProperty
			FGlobalComponentReattachContext ReattachContext;

			TArray<UMaterial*> RecompiledMaterials;

			// Go through all materials in memory and recompile them if they use this material function
			for (TObjectIterator<UMaterial> It; It; ++It)
			{
				UMaterial* CurrentMaterial = *It;
				if (CurrentMaterial != Material)
				{
					UBOOL bRecompile = FALSE;

					// Preview materials often use expressions for rendering that are not in their Expressions array, 
					// And therefore their MaterialFunctionInfos are not up to date.
					if (CurrentMaterial->bIsPreviewMaterial)
					{
						bRecompile = TRUE;
					}
					else
					{
						for (INT FunctionIndex = 0; FunctionIndex < CurrentMaterial->MaterialFunctionInfos.Num(); FunctionIndex++)
						{
							if (CurrentMaterial->MaterialFunctionInfos(FunctionIndex).Function == EditMaterialFunction->ParentFunction)
							{
								bRecompile = TRUE;
								break;
							}
						}
					}

					if (bRecompile)
					{
						RecompiledMaterials.AddItem(CurrentMaterial);

						// Propagate the function change to this material
						CurrentMaterial->PreEditChange(NULL);
						CurrentMaterial->PostEditChange();
						CurrentMaterial->MarkPackageDirty();
					}
				}
			}

			// Go through all material instances with static parameters and terrain materials and recompile them if necessary
			for (FObjectIterator It; It; ++It)
			{
				UMaterialInstance* MatInst = Cast<UMaterialInstance>(*It);
				ATerrain* Terrain = Cast<ATerrain>(*It);
				AEmitter* Emitter = Cast<AEmitter>(*It);

				for (INT MaterialIndex = 0; MaterialIndex < RecompiledMaterials.Num(); MaterialIndex++)
				{
					if (MatInst)
					{
						// Go through all loaded material instances and recompile their static permutation resources if needed
						UMaterial* BaseMaterial = MatInst->GetMaterial();

						// Only recompile if the instance is a child of the material that got updated
						if (RecompiledMaterials(MaterialIndex) == BaseMaterial)
						{
							MatInst->InitStaticPermutation();
							MatInst->MarkPackageDirty();
						}
					}
					else if (Terrain)
					{
						Terrain->UpdateCachedMaterial(RecompiledMaterials(MaterialIndex));
					}
				}
			}
		}

		// update the world's viewports
		GCallbackEvent->Send(CALLBACK_RefreshEditor);
		GCallbackEvent->Send(CALLBACK_RedrawAllViewports);
	}
	// Handle propagation of the material being edited
	else
	{
		// make sure that any staticmeshes, etc using this material will stop using the FMaterialResource of the original 
		// material, and will use the new FMaterialResource created when we make a new UMaterial inplace
		FGlobalComponentReattachContext RecreateComponents;

		// overwrite the original material in place by constructing a new one with the same name
		OriginalMaterial = (UMaterial*)UObject::StaticDuplicateObject(Material, Material, OriginalMaterial->GetOuter(), *OriginalMaterial->GetName(), ~0, OriginalMaterial->GetClass());
		// Restore RF_Standalone on the original material, as it had been removed from the preview material so that it could be GC'd.
		OriginalMaterial->SetFlags( RF_Standalone );

		// copy the flattened texture manually because it's duplicatetransient so it's NULLed when duplicating normally
		// (but we don't want it NULLed in this case)
		OriginalMaterial->MobileBaseTexture = Material->MobileBaseTexture;

		// Manually copy bUsedAsSpecialEngineMaterial as it is duplicate transient to prevent accidental creation of new special engine materials
		OriginalMaterial->bUsedAsSpecialEngineMaterial = Material->bUsedAsSpecialEngineMaterial;

		// let the material update itself if necessary
		OriginalMaterial->PreEditChange(NULL);
		if( !OriginalMaterial->bUsedAsSpecialEngineMaterial )
		{
			FPropertyChangedEvent ChangedEvent(NULL);
			ChangedEvent.ChangeType = EPropertyChangeType::Duplicate;
			OriginalMaterial->PostEditChangeProperty(ChangedEvent);
		}
		else
		{
			OriginalMaterial->PostEditChange();
		}
		OriginalMaterial->MarkPackageDirty();

		//copy the compile errors from the original material to the preview material
		//this is necessary since some errors are not encountered while compiling the preview material, but only when compiling the full material
		//without this copy the errors will only be reported after the material editor is closed and reopened
		const FMaterialResource* OriginalMaterialResource = OriginalMaterial->GetMaterialResource();
		FMaterialResource* PreviewMaterialResource = Material->GetMaterialResource();
		if (OriginalMaterialResource && PreviewMaterialResource)
		{
			PreviewMaterialResource->SetCompileErrors(OriginalMaterialResource->GetCompileErrors());
		}

		// clear the dirty flag
		bMaterialDirty = FALSE;

		// update the world's viewports
		GCallbackEvent->Send(CALLBACK_RefreshEditor);
		GCallbackEvent->Send(CALLBACK_RedrawAllViewports);

		TArray<UMaterialInstance*> MatInstances;
		for (FObjectIterator It; It; ++It)
		{
			UMaterialInstance* MatInst = Cast<UMaterialInstance>(*It);
			ATerrain* Terrain = Cast<ATerrain>(*It);
			AEmitter* Emitter = Cast<AEmitter>(*It);
			if (MatInst)
			{
				UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(MatInst);
				if (MIC)
				{
					// Loop through all loaded material instance constants and update their parameter names since they may have changed.
					MIC->UpdateParameterNames();
				}

				// Go through all loaded material instances and recompile their static permutation resources if needed
				// This is necessary since the parent UMaterial stores information about how it should be rendered, (eg bUsesDistortion)
				// but the child can have its own shader map which may not contain all the shaders that the parent's settings indicate that it should.
				// this code is duplicated in Material.cpp UMaterial::SetMaterialUsage
				UMaterial* BaseMaterial = MatInst->GetMaterial();
				//only recompile if the instance is a child of the material that got updated
				if (OriginalMaterial == BaseMaterial)
				{
					MatInstances.AddItem(MatInst);
				}
			}
			else if (Terrain)
			{
				Terrain->UpdateCachedMaterial(OriginalMaterial);
			}
			else if (Emitter && Emitter->ParticleSystemComponent)
			{
				Emitter->ParticleSystemComponent->bIsViewRelevanceDirty = TRUE;
			}
		}

		// Handle instances separately so we can provide status bar feedback
		if( MatInstances.Num() > 0 )
		{
			GWarn->EndSlowTask();
			const FString LocalizedMaterialEditorApplyInstances( LocalizeUnrealEd("ToolTip_MaterialEditorApplyInstances") );
			GWarn->BeginSlowTask( *LocalizedMaterialEditorApplyInstances, TRUE );

			for( INT InstIdx=0;InstIdx<MatInstances.Num();InstIdx++ )
			{
				GWarn->StatusUpdatef( InstIdx, MatInstances.Num(), *LocalizedMaterialEditorApplyInstances );
				warnf(NAME_DevShaders, TEXT("	InitStaticPermutaton for instance %s"), *MatInstances(InstIdx)->GetPathName());

				// update instance's quality switch flag
				MatInstances(InstIdx)->bHasQualitySwitch = OriginalMaterial->bHasQualitySwitch;

				MatInstances(InstIdx)->InitStaticPermutation();

				MatInstances(InstIdx)->SetupMobileProperties();

				// If we are in emulation mode, ensure these changes get through to the emulated material.
				if (GEmulateMobileRendering && !GForceDisableEmulateMobileRendering)
				{
					FMobileEmulationMaterialManager::GetManager()->UpdateMaterialInterface( MatInstances(InstIdx), FALSE, TRUE );
				}

				// Mark the package dirty so the user who edited the parent material has an idea of which packages he should resave to avoid repeated recompiling
				MatInstances(InstIdx)->MarkPackageDirty();
			}
		}
		RebuildMaterialInstanceEditors(NULL);

		// flatten this material if needed
		GCallbackEvent->Send(CALLBACK_MobileFlattenedTextureUpdate, OriginalMaterial);

		// copy the dominant texture into the preview material so user can see it
		Material->MobileBaseTexture = OriginalMaterial->MobileBaseTexture;
	}


	GWarn->EndSlowTask();
}

/**
 * Uses the global Undo transactor to reverse changes, update viewports etc.
 */
INT WxMaterialEditor::PreEditUndo()
{
	EmptySelection();

	return Material->Expressions.Num();
}
void WxMaterialEditor::Undo()
{
	INT iNumExpressions = PreEditUndo();
	GEditor->UndoTransaction();
	PostEditUndo( iNumExpressions );
}
void WxMaterialEditor::PostEditUndo( const INT iPreNumExpressions )
{
	if( iPreNumExpressions != Material->Expressions.Num() )
	{
		Material->BuildEditorParameterList();
	}

	UpdateSearch(FALSE);

	// Update the current preview material.
	UpdatePreviewMaterial();

	UpdatePropertyWindow();

	RefreshExpressionPreviews();
	RefreshExpressionViewport();
	bMaterialDirty = TRUE;
}

/**
 * Uses the global Undo transactor to redo changes, update viewports etc.
 */
INT WxMaterialEditor::PreEditRedo()
{
	EmptySelection();

	return Material->Expressions.Num();
}
void WxMaterialEditor::Redo()
{
	INT iNumExpressions = PreEditRedo();
	GEditor->RedoTransaction();
	PostEditRedo( iNumExpressions );
}
void WxMaterialEditor::PostEditRedo( const INT iPreNumExpressions )
{
	if( iPreNumExpressions != Material->Expressions.Num() )
	{
		Material->BuildEditorParameterList();
	}

	// Update the current preview material.
	UpdatePreviewMaterial();

	UpdatePropertyWindow();

	RefreshExpressionPreviews();
	RefreshExpressionViewport();
	bMaterialDirty = TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FSerializableObject interface
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WxMaterialEditor::Serialize(FArchive& Ar)
{
	WxMaterialEditorBase::Serialize(Ar);

	Ar << Material;
	Ar << SelectedExpressions;
	Ar << SelectedComments;
	Ar << ExpressionPreviews;
	Ar << ExpressionsLinkedToComments;
	Ar << EditorOptions;
	Ar << OriginalMaterial;
	Ar << EditMaterialFunction;
	Ar << ExpressionPreviewMaterial;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FNotifyHook interface
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WxMaterialEditor::NotifyPreChange(void* Src, UProperty* PropertyAboutToChange)
{
	check( !ScopedTransaction );
	ScopedTransaction = new FScopedTransaction( *LocalizeUnrealEd(TEXT("MaterialEditorEditProperties")) );
	FlushRenderingCommands();
}

void WxMaterialEditor::NotifyPostChange(void* Src, UProperty* PropertyThatChanged)
{
	check( ScopedTransaction );

	if ( PropertyThatChanged )
	{
		const FName NameOfPropertyThatChanged( *PropertyThatChanged->GetName() );
		if ( NameOfPropertyThatChanged == FName(TEXT("PreviewMesh")) ||
			 NameOfPropertyThatChanged == FName(TEXT("bUsedWithSkeletalMesh")) )
		{
			// SetPreviewMesh will return FALSE if the material has bUsedWithSkeletalMesh and
			// a skeleton was requested, in which case revert to a sphere static mesh.
			if ( !SetPreviewMesh( *Material->PreviewMesh ) )
			{
				SetPreviewMesh( GUnrealEd->GetThumbnailManager()->TexPropSphere, NULL );
			}
		}

		WxPropertyControl* PropertyWindowItem = static_cast<WxPropertyControl*>( Src );
		WxPropertyWindow* PropertyWindow = PropertyWindowItem->GetPropertyWindow();
		for ( WxPropertyWindow::TObjectIterator Itor( PropertyWindow->ObjectIterator() ) ; Itor ; ++Itor )
		{
			UMaterialExpression* Expression = Cast< UMaterialExpression >( *Itor );
			if(Expression)
			{
				if(NameOfPropertyThatChanged == FName(TEXT("ParameterName")))
				{
					Material->UpdateExpressionParameterName(Expression);
				}
				else if (NameOfPropertyThatChanged == FName(TEXT("ParamNames")))
				{
					Material->UpdateExpressionDynamicParameterNames(Expression);
				}
				else
				{
					Material->PropagateExpressionParameterChanges(Expression);
				}
			}
		}
	}

	// Update the current preview material.
	UpdatePreviewMaterial();

	delete ScopedTransaction;
	ScopedTransaction = NULL;

	UpdateSearch(FALSE);
	Material->MarkPackageDirty();
	RefreshExpressionPreviews();
	RefreshSourceWindowMaterial();
	bMaterialDirty = TRUE;
}

void WxMaterialEditor::NotifyExec(void* Src, const TCHAR* Cmd)
{
	GUnrealEd->NotifyExec( Src, Cmd );
}

/** 
 * Computes a bounding box for the selected material expressions.  Output is not sensible if no expressions are selected.
 */
FIntRect WxMaterialEditor::GetBoundingBoxOfSelectedExpressions()
{
	return GetBoundingBoxOfExpressions( SelectedExpressions );
}

/** 
 * Computes a bounding box for the specified set of material expressions.  Output is not sensible if the set is empty.
 */
FIntRect WxMaterialEditor::GetBoundingBoxOfExpressions(const TArray<UMaterialExpression*>& Expressions)
{
	FIntRect Result(0, 0, 0, 0);
	UBOOL bResultValid = FALSE;

	for( INT ExpressionIndex = 0 ; ExpressionIndex < Expressions.Num() ; ++ExpressionIndex )
	{
		UMaterialExpression* Expression = Expressions(ExpressionIndex);
		// @todo DB: Remove these hardcoded extents.
		const FIntRect ObjBox( Expression->MaterialExpressionEditorX-30, Expression->MaterialExpressionEditorY-20, Expression->MaterialExpressionEditorX+150, Expression->MaterialExpressionEditorY+150 );

		if( bResultValid )
		{
			// Expand result box to encompass
			Result.Min.X = ::Min(Result.Min.X, ObjBox.Min.X);
			Result.Min.Y = ::Min(Result.Min.Y, ObjBox.Min.Y);

			Result.Max.X = ::Max(Result.Max.X, ObjBox.Max.X);
			Result.Max.Y = ::Max(Result.Max.Y, ObjBox.Max.Y);
		}
		else
		{
			// Use this objects bounding box to initialise result.
			Result = ObjBox;
			bResultValid = TRUE;
		}
	}

	return Result;
}

/**
 * Creates a new material expression comment frame.
 */
void WxMaterialEditor::CreateNewCommentExpression()
{
	WxDlgGenericStringEntry dlg;
	const INT Result = dlg.ShowModal( TEXT("NewComment"), TEXT("CommentText"), TEXT("Comment") );
	if ( Result == wxID_OK )
	{
		UMaterialExpressionComment* NewComment = NULL;
		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("MaterialEditorNewComment")) );
			Material->Modify();

			UObject* ExpressionOuter = Material;
			if (EditMaterialFunction)
			{
				ExpressionOuter = EditMaterialFunction;
			}

			NewComment = ConstructObject<UMaterialExpressionComment>( UMaterialExpressionComment::StaticClass(), ExpressionOuter, NAME_None, RF_Transactional );

			// Add to the list of comments associated with this material.
			Material->EditorComments.AddItem( NewComment );

			if ( SelectedExpressions.Num() > 0 )
			{
				const FIntRect SelectedBounds = GetBoundingBoxOfSelectedExpressions();
				NewComment->MaterialExpressionEditorX = SelectedBounds.Min.X;
				NewComment->MaterialExpressionEditorY = SelectedBounds.Min.Y;
				NewComment->SizeX = SelectedBounds.Max.X - SelectedBounds.Min.X;
				NewComment->SizeY = SelectedBounds.Max.Y - SelectedBounds.Min.Y;
			}
			else
			{

				NewComment->MaterialExpressionEditorX = (LinkedObjVC->NewX - LinkedObjVC->Origin2D.X)/LinkedObjVC->Zoom2D;
				NewComment->MaterialExpressionEditorY = (LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y)/LinkedObjVC->Zoom2D;
				NewComment->SizeX = 128;
				NewComment->SizeY = 128;
			}

			NewComment->Text = dlg.GetEnteredString();
		}

		// Select the new comment.
		EmptySelection();
		AddToSelection( NewComment );

		Material->MarkPackageDirty();
		RefreshExpressionViewport();
		bMaterialDirty = TRUE;
	}
}

/**
 * Creates a new material expression of the specified class.  Will automatically connect output 0
 * of the new expression to an input tab, if one was clicked on.
 *
 * @param	NewExpressionClass		The type of material expression to add.  Must be a child of UMaterialExpression.
 * @param	bAutoConnect			If TRUE, connect the new material expression to the most recently selected connector, if possible.
 * @param	bAutoSelect				If TRUE, deselect all expressions and select the newly created one.
 * @param	NodePos					Position of the new node.
 */
UMaterialExpression* WxMaterialEditor::CreateNewMaterialExpression(UClass* NewExpressionClass, UBOOL bAutoConnect, UBOOL bAutoSelect, UBOOL bAutoAssignResource, const FIntPoint& NodePos)
{
	check( NewExpressionClass->IsChildOf(UMaterialExpression::StaticClass()) );

	if (!IsAllowedExpressionType(NewExpressionClass, EditMaterialFunction != NULL))
	{
		// Disallowed types should not be visible to the ui to be placed, so we don't need a warning here
		return NULL;
	}

	// Create the new expression.
	UMaterialExpression* NewExpression = NULL;
	{
		const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("MaterialEditorNewExpression")) );
		Material->Modify();

		UObject* ExpressionOuter = Material;
		if (EditMaterialFunction)
		{
			ExpressionOuter = EditMaterialFunction;
		}

		NewExpression = ConstructObject<UMaterialExpression>( NewExpressionClass, ExpressionOuter, NAME_None, RF_Transactional );
		Material->Expressions.AddItem( NewExpression );
		NewExpression->Material = Material;

		// If the new expression is created connected to an input tab, offset it by this amount.
		INT NewConnectionOffset = 0;

		if (bAutoConnect)
		{
			// If an input tab was clicked on, bind the new expression to that tab.
			if ( ConnType == LOC_OUTPUT && ConnObj )
			{
				UMaterial* ConnMaterial							= Cast<UMaterial>( ConnObj );
				UMaterialExpression* ConnMaterialExpression		= Cast<UMaterialExpression>( ConnObj );
				check( ConnMaterial  || ConnMaterialExpression );

				if ( ConnMaterial )
				{
					ConnectMaterialToMaterialExpression( ConnMaterial, ConnIndex, NewExpression, 0, bHideUnusedConnectors );
				}
				else if ( ConnMaterialExpression )
				{
					UMaterialExpression* InputExpression = ConnMaterialExpression;
					UMaterialExpression* OutputExpression = NewExpression;

					INT InputIndex = ConnIndex;
					INT OutputIndex = 0;

					// Input expression.
					FExpressionInput* ExpressionInput = InputExpression->GetInput(InputIndex);

					// Output expression.
					TArray<FExpressionOutput>& Outputs = OutputExpression->GetOutputs();
					const FExpressionOutput& ExpressionOutput = Outputs( OutputIndex );

					InputExpression->Modify();
					ConnectExpressions( *ExpressionInput, ExpressionOutput, OutputIndex, OutputExpression );
				}

				// Offset the new expression it by this amount.
				NewConnectionOffset = 30;
			}
		}

		// Set the expression location.
		NewExpression->MaterialExpressionEditorX = NodePos.X + NewConnectionOffset;
		NewExpression->MaterialExpressionEditorY = NodePos.Y + NewConnectionOffset;

		if (bAutoAssignResource)
		{
			// If the user is adding a texture sample, automatically assign the currently selected texture to it.
			UMaterialExpressionTextureSample* METextureSample = Cast<UMaterialExpressionTextureSample>( NewExpression );
			if( METextureSample )
			{
				GCallbackEvent->Send( CALLBACK_LoadSelectedAssetsIfNeeded );
				METextureSample->Texture = GEditor->GetSelectedObjects()->GetTop<UTexture>();
			}

			UMaterialExpressionMaterialFunctionCall* MEMaterialFunction = Cast<UMaterialExpressionMaterialFunctionCall>( NewExpression );
			if( MEMaterialFunction )
			{
				GCallbackEvent->Send( CALLBACK_LoadSelectedAssetsIfNeeded );
				MEMaterialFunction->SetMaterialFunction(EditMaterialFunction, NULL, GEditor->GetSelectedObjects()->GetTop<UMaterialFunction>());
			}
		}

		UMaterialExpressionFunctionInput* FunctionInput = Cast<UMaterialExpressionFunctionInput>( NewExpression );
		if( FunctionInput )
		{
			FunctionInput->ConditionallyGenerateId(TRUE);
			FunctionInput->ValidateName();
		}

		UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>( NewExpression );
		if( FunctionOutput )
		{
			FunctionOutput->ConditionallyGenerateId(TRUE);
			FunctionOutput->ValidateName();
		}

		// If the user is creating any parameter objects, then generate a GUID for it.
		UMaterialExpressionParameter* ParameterExpression = Cast<UMaterialExpressionParameter>( NewExpression );

		if( ParameterExpression )
		{
			ParameterExpression->ConditionallyGenerateGUID(TRUE);
		}

		UMaterialExpressionTextureSampleParameter* TextureParameterExpression = Cast<UMaterialExpressionTextureSampleParameter>( NewExpression );
		if( TextureParameterExpression )
		{
			// Change the parameter's name on creation to mirror the object's name; this avoids issues of having colliding parameter
			// names and having the name left as "None"
			TextureParameterExpression->ParameterName = TextureParameterExpression->GetFName();
			TextureParameterExpression->ConditionallyGenerateGUID(TRUE);
		}

		UMaterialExpressionComponentMask* ComponentMaskExpression = Cast<UMaterialExpressionComponentMask>( NewExpression );
		// Setup defaults for the most likely use case
		// For Gears2 content a mask of .rg is 47% of all mask combinations
		// Can't change default properties as that will affect existing content
		if( ComponentMaskExpression )
		{
			ComponentMaskExpression->R = TRUE;
			ComponentMaskExpression->G = TRUE;
		}

		UMaterialExpressionStaticComponentMaskParameter* StaticComponentMaskExpression = Cast<UMaterialExpressionStaticComponentMaskParameter>( NewExpression );
		// Setup defaults for the most likely use case
		// For Gears2 content a static mask of .r is 67% of all mask combinations
		// Can't change default properties as that will affect existing content
		if( StaticComponentMaskExpression )
		{
			StaticComponentMaskExpression->DefaultR = TRUE;
		}

		Material->AddExpressionParameter(NewExpression);
	}

	// Select the new node.
	if ( bAutoSelect )
	{
		EmptySelection();
		AddToSelection( NewExpression );
	}

	RefreshSourceWindowMaterial();
	UpdateSearch(FALSE);

	// Update the current preview material.
	UpdatePreviewMaterial();
	Material->MarkPackageDirty();

	UpdatePropertyWindow();


	RefreshExpressionPreviews();
	RefreshExpressionViewport();
	bMaterialDirty = TRUE;
	return NewExpression;
}

/**
 *	This function returns the name of the docking parent.  This name is used for saving and loading the layout files.
 *  @return A string representing a name to use for this docking parent.
 */
const TCHAR* WxMaterialEditor::GetDockingParentName() const
{
	return EditMaterialFunction ? TEXT("MaterialFunctionEditor") : TEXT("MaterialEditor");
}

/**
 * @return The current version of the docking parent, this value needs to be increased every time new docking windows are added or removed.
 */
const INT WxMaterialEditor::GetDockingParentVersion() const
{
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Wx event handlers.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WxMaterialEditor::OnClose(wxCloseEvent& In)
{
	// @todo DB: Store off the viewport camera position/orientation to the material.
	//AnimTree->PreviewCamPos = PreviewVC->ViewLocation;
	//AnimTree->PreviewCamRot = PreviewVC->ViewRotation;

	if (bMaterialDirty)
	{
		// find out the user wants to do with this dirty material
		INT YesNoCancelReply = appMsgf(AMT_YesNoCancel, *LocalizeUnrealEd("Prompt_MaterialEditorClose"));
		// act on it
		switch (YesNoCancelReply)
		{
			case 0: // Yes
				// update material
				UpdateOriginalMaterial();
				break;

			case 1: // No
				// do nothing
				break;

			case 2: // Cancel
				// veto closing the window
				if (In.CanVeto())
				{
					In.Veto();
				}
				// don't call destroy!
				return;
		}
	}

	// Reset the transaction buffer so that references to the preview material are cleared.
	GEditor->ResetTransaction( TEXT("CloseMaterialEditor") );
	Destroy();
}

void WxMaterialEditor::OnNewMaterialExpression(wxCommandEvent& In)
{
	const INT NewNodeClassIndex = In.GetId() - IDM_NEW_SHADER_EXPRESSION_START;

	UClass* NewExpressionClass = NULL;
	if (bUseUnsortedMenus == TRUE)
	{
	check( NewNodeClassIndex >= 0 && NewNodeClassIndex < MaterialExpressionClasses.Num() );
		NewExpressionClass = MaterialExpressionClasses(NewNodeClassIndex);
	}
	else
	{
		INT NodeClassIndex = NewNodeClassIndex;
		INT FavoriteCount = FavoriteExpressionClasses.Num();
		if (NodeClassIndex < FavoriteCount)
		{
			// It's from the favorite expressions menu...
			NewExpressionClass = FavoriteExpressionClasses(NodeClassIndex);
		}
		else
		{
			INT Count = FavoriteCount;
			for (INT CheckIndex = 0; (CheckIndex < CategorizedMaterialExpressionClasses.Num()) && (NewExpressionClass == NULL); CheckIndex++)
			{
				FCategorizedMaterialExpressionNode& CategoryNode = CategorizedMaterialExpressionClasses(CheckIndex);
				if (NodeClassIndex < (Count + CategoryNode.MaterialExpressionClasses.Num()))
				{
					NodeClassIndex -= Count;
					NewExpressionClass = CategoryNode.MaterialExpressionClasses(NodeClassIndex);
				}
				Count += CategoryNode.MaterialExpressionClasses.Num();
			}

			if (NewExpressionClass == NULL)
			{
				// Assume it is in the unassigned list...
				NodeClassIndex -= Count;
				if (NodeClassIndex < UnassignedExpressionClasses.Num())
				{
					NewExpressionClass = UnassignedExpressionClasses(NodeClassIndex);
				}
			}
		}
	}

	if (NewExpressionClass)
	{
		const INT LocX = (LinkedObjVC->NewX - LinkedObjVC->Origin2D.X)/LinkedObjVC->Zoom2D;
		const INT LocY = (LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y)/LinkedObjVC->Zoom2D;
		CreateNewMaterialExpression( NewExpressionClass, TRUE, TRUE, TRUE, FIntPoint(LocX, LocY) );
	}
}

void WxMaterialEditor::OnNewComment(wxCommandEvent& In)
{
	CreateNewCommentExpression();
}

void WxMaterialEditor::OnUseCurrentTexture(wxCommandEvent& In)
{
	// Set the currently selected texture in the generic browser
	// as the texture to use in all selected texture sample expressions.
	GCallbackEvent->Send( CALLBACK_LoadSelectedAssetsIfNeeded );
	UTexture* SelectedTexture = GEditor->GetSelectedObjects()->GetTop<UTexture>();
	if ( SelectedTexture )
	{
		const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("UseCurrentTexture")) );
		for( INT MaterialExpressionIndex = 0 ; MaterialExpressionIndex < SelectedExpressions.Num() ; ++MaterialExpressionIndex )
		{
			UMaterialExpression* MaterialExpression = SelectedExpressions( MaterialExpressionIndex );
			if ( MaterialExpression->IsA(UMaterialExpressionTextureSample::StaticClass()) )
			{
				UMaterialExpressionTextureSample* TextureSample = static_cast<UMaterialExpressionTextureSample*>(MaterialExpression);
				TextureSample->Modify();
				TextureSample->Texture = SelectedTexture;
			}
		}

		// Update the current preview material. 
		UpdatePreviewMaterial();
		Material->MarkPackageDirty();
		RefreshSourceWindowMaterial();
		UpdatePropertyWindow();
		RefreshExpressionPreviews();
		RefreshExpressionViewport();
		bMaterialDirty = TRUE;
	}
}

void WxMaterialEditor::OnDuplicateObjects(wxCommandEvent& In)
{
	DuplicateSelectedObjects();
}

void WxMaterialEditor::OnPasteHere(wxCommandEvent& In)
{
	const FIntPoint ClickLocation( (LinkedObjVC->NewX - LinkedObjVC->Origin2D.X)/LinkedObjVC->Zoom2D, (LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y)/LinkedObjVC->Zoom2D );
	PasteExpressionsIntoMaterial( &ClickLocation );
}

void WxMaterialEditor::OnConvertObjects(wxCommandEvent& In)
{
	if (SelectedExpressions.Num() > 0)
	{
		const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("MaterialEditorConvert")) );
		Material->Modify();
		TArray<UMaterialExpression*> ExpressionsToDelete;
		TArray<UMaterialExpression*> ExpressionsToSelect;
		for (INT ExpressionIndex = 0; ExpressionIndex < SelectedExpressions.Num(); ++ExpressionIndex)
		{
			// Look for the supported classes to convert from
			UMaterialExpression* CurrentSelectedExpression = SelectedExpressions(ExpressionIndex);
			UMaterialExpressionConstant* Constant1Expression = Cast<UMaterialExpressionConstant>(CurrentSelectedExpression);
			UMaterialExpressionConstant2Vector* Constant2Expression = Cast<UMaterialExpressionConstant2Vector>(CurrentSelectedExpression);
			UMaterialExpressionConstant3Vector* Constant3Expression = Cast<UMaterialExpressionConstant3Vector>(CurrentSelectedExpression);
			UMaterialExpressionConstant4Vector* Constant4Expression = Cast<UMaterialExpressionConstant4Vector>(CurrentSelectedExpression);
			UMaterialExpressionTextureSample* TextureSampleExpression = Cast<UMaterialExpressionTextureSample>(CurrentSelectedExpression);
			UMaterialExpressionComponentMask* ComponentMaskExpression = Cast<UMaterialExpressionComponentMask>(CurrentSelectedExpression);
			UMaterialExpressionParticleSubUV* ParticleSubUVExpression = Cast<UMaterialExpressionParticleSubUV>(CurrentSelectedExpression);
			UMaterialExpressionMeshSubUV* MeshSubUVExpression = Cast<UMaterialExpressionMeshSubUV>(CurrentSelectedExpression);
			UMaterialExpressionMeshSubUVBlend* MeshSubUVBlendExpression = Cast<UMaterialExpressionMeshSubUVBlend>(CurrentSelectedExpression);
			UMaterialExpressionFlipBookSample* FlipbookSampleExpression = Cast<UMaterialExpressionFlipBookSample>(CurrentSelectedExpression);

			// Setup the class to convert to
			UClass* ClassToCreate = NULL;
			if (Constant1Expression)
			{
				ClassToCreate = UMaterialExpressionScalarParameter::StaticClass();
			}
			else if (Constant2Expression || Constant3Expression || Constant4Expression)
			{
				ClassToCreate = UMaterialExpressionVectorParameter::StaticClass();
			}
			else if (MeshSubUVBlendExpression) // Has to come before the MeshSubUV comparison...
			{
				ClassToCreate = UMaterialExpressionTextureSampleParameterMeshSubUVBlend::StaticClass();
			}
			else if (MeshSubUVExpression) // Has to come before the TextureSample comparison...
			{
				ClassToCreate = UMaterialExpressionTextureSampleParameterMeshSubUV::StaticClass();
			}
			else if (ParticleSubUVExpression) // Has to come before the TextureSample comparison...
			{
				ClassToCreate = UMaterialExpressionTextureSampleParameterSubUV::StaticClass();
			}
			else if (TextureSampleExpression && TextureSampleExpression->Texture && TextureSampleExpression->Texture->IsA(UTextureCube::StaticClass()))
			{
				ClassToCreate = UMaterialExpressionTextureSampleParameterCube::StaticClass();
			}	
			else if (FlipbookSampleExpression)
			{
				ClassToCreate = UMaterialExpressionTextureSampleParameterFlipbook::StaticClass();
			}
			else if (TextureSampleExpression)
			{
				ClassToCreate = UMaterialExpressionTextureSampleParameter2D::StaticClass();
			}	
			else if (ComponentMaskExpression)
			{
				ClassToCreate = UMaterialExpressionStaticComponentMaskParameter::StaticClass();
			}

			if (ClassToCreate)
			{
				UMaterialExpression* NewExpression = CreateNewMaterialExpression(ClassToCreate, FALSE, FALSE, TRUE, FIntPoint(CurrentSelectedExpression->MaterialExpressionEditorX, CurrentSelectedExpression->MaterialExpressionEditorY));
				if (NewExpression)
				{
					SwapLinksToExpression(CurrentSelectedExpression, NewExpression, Material);
					UBOOL bNeedsRefresh = FALSE;

					// Copy over expression-specific values
					if (Constant1Expression)
					{
						bNeedsRefresh = TRUE;
						CastChecked<UMaterialExpressionScalarParameter>(NewExpression)->DefaultValue = Constant1Expression->R;
					}
					else if (Constant2Expression)
					{
						bNeedsRefresh = TRUE;
						CastChecked<UMaterialExpressionVectorParameter>(NewExpression)->DefaultValue = FLinearColor(Constant2Expression->R, Constant2Expression->G, 0);
					}
					else if (Constant3Expression)
					{
						bNeedsRefresh = TRUE;
						CastChecked<UMaterialExpressionVectorParameter>(NewExpression)->DefaultValue = FLinearColor(Constant3Expression->R, Constant3Expression->G, Constant3Expression->B);
					}
					else if (Constant4Expression)
					{
						bNeedsRefresh = TRUE;
						CastChecked<UMaterialExpressionVectorParameter>(NewExpression)->DefaultValue = FLinearColor(Constant4Expression->R, Constant4Expression->G, Constant4Expression->B, Constant4Expression->A);
					}
					else if (TextureSampleExpression)
					{
						bNeedsRefresh = TRUE;
						CastChecked<UMaterialExpressionTextureSample>(NewExpression)->Texture = TextureSampleExpression->Texture;
					}
					else if (ComponentMaskExpression)
					{
						bNeedsRefresh = TRUE;
						UMaterialExpressionStaticComponentMaskParameter* ComponentMask = CastChecked<UMaterialExpressionStaticComponentMaskParameter>(NewExpression);
						ComponentMask->DefaultR = ComponentMaskExpression->R;
						ComponentMask->DefaultG = ComponentMaskExpression->G;
						ComponentMask->DefaultB = ComponentMaskExpression->B;
						ComponentMask->DefaultA = ComponentMaskExpression->A;
					}
					else if (MeshSubUVBlendExpression) // Has to come before the MeshSubUV comparison...
					{
						bNeedsRefresh = TRUE;
						CastChecked<UMaterialExpressionTextureSampleParameterMeshSubUVBlend>(NewExpression)->Texture = MeshSubUVBlendExpression->Texture;
					}
					else if (MeshSubUVExpression) // Has to come before the TextureSample comparison...
					{
						bNeedsRefresh = TRUE;
						CastChecked<UMaterialExpressionTextureSampleParameterMeshSubUV>(NewExpression)->Texture = MeshSubUVExpression->Texture;
					}
					else if (ParticleSubUVExpression)
					{
						bNeedsRefresh = TRUE;
						CastChecked<UMaterialExpressionTextureSampleParameterSubUV>(NewExpression)->Texture = ParticleSubUVExpression->Texture;
					}

					if (bNeedsRefresh)
					{
						// Refresh the expression preview if we changed its properties after it was created
						NewExpression->bNeedToUpdatePreview = TRUE;
						RefreshExpressionPreview( NewExpression, TRUE );
					}

					ExpressionsToDelete.AddUniqueItem(CurrentSelectedExpression);
					ExpressionsToSelect.AddItem(NewExpression);
				}
			}
		}
		// Delete the replaced expressions
		TArray< UMaterialExpressionComment *> Comments;

		DeleteObjects(ExpressionsToDelete, Comments );

		// Select each of the newly converted expressions
		for ( TArray<UMaterialExpression*>::TConstIterator ExpressionIter(ExpressionsToSelect); ExpressionIter; ++ExpressionIter )
		{
			AddToSelection(*ExpressionIter);
		}

		// If only one expression was converted, select it's "Parameter Name" property in the property window
		if (ExpressionsToSelect.Num() == 1)
		{
			// Update the property window so that it displays the properties for the new expression
			UpdatePropertyWindow();

			FPropertyNode* ParamNameProp = PropertyWindow->FindPropertyNode(TEXT("ParameterName"));

			if (ParamNameProp)
			{
				WxPropertyControl* PropertyNodeWindow = ParamNameProp->GetNodeWindow();
				
				check( PropertyNodeWindow );
				check( PropertyNodeWindow->InputProxy );

				// Set the focus to the "Parameter Name" property
				PropertyWindow->SetActiveFocus(PropertyNodeWindow, TRUE);
				PropertyNodeWindow->InputProxy->OnSetFocus(PropertyNodeWindow);
			}
		}
	}
}

void WxMaterialEditor::OnSelectDownsteamNodes(wxCommandEvent& In)
{
	TArray<UMaterialExpression*> ExpressionsToEvaluate;	// Graph nodes that need to be traced downstream
	TArray<UMaterialExpression*> ExpressionsEvalated;	// Keep track of evaluated graph nodes so we don't re-evaluate them
	TArray<UMaterialExpression*> ExpressionsToSelect;	// Downstream graph nodes that will end up being selected

	// Add currently selected graph nodes to the "need to be traced downstream" list
	for ( TArray<UMaterialExpression*>::TConstIterator ExpressionIter(SelectedExpressions); ExpressionIter; ++ExpressionIter )
	{
		ExpressionsToEvaluate.AddItem(*ExpressionIter);
	}

	// Generate a list of downstream nodes
	while (ExpressionsToEvaluate.Num() > 0)
	{
		UMaterialExpression* CurrentExpression = ExpressionsToEvaluate.Last();
		if (CurrentExpression)
		{
			TArray<FExpressionOutput>& Outputs = CurrentExpression->GetOutputs();
			
			for (INT OutputIndex = 0; OutputIndex < Outputs.Num(); OutputIndex++)
			{
				const FExpressionOutput& CurrentOutput = Outputs(OutputIndex);
				TArray<FMaterialExpressionReference> ReferencingInputs;
				GetListOfReferencingInputs(CurrentExpression, Material, ReferencingInputs, &CurrentOutput, OutputIndex);

				for (INT ReferenceIndex = 0; ReferenceIndex < ReferencingInputs.Num(); ReferenceIndex++)
				{
					FMaterialExpressionReference& CurrentReference = ReferencingInputs(ReferenceIndex);
					if (CurrentReference.Expression)
					{
						INT index = -1;
						ExpressionsEvalated.FindItem(CurrentReference.Expression, index);

						if (index < 0)
						{
							// This node is a downstream node (so, we'll need to select it) and it's children need to be traced as well
							ExpressionsToSelect.AddItem(CurrentReference.Expression);
							ExpressionsToEvaluate.AddItem(CurrentReference.Expression);
						}
					}
				}
			}
		}

		// This graph node has now been examined
		ExpressionsEvalated.AddItem(CurrentExpression);
		ExpressionsToEvaluate.RemoveItem(CurrentExpression);
	}

	// Select all downstream nodes
	if (ExpressionsToSelect.Num() > 0)
	{
		for ( TArray<UMaterialExpression*>::TConstIterator ExpressionIter(ExpressionsToSelect); ExpressionIter; ++ExpressionIter )
		{
			AddToSelection(*ExpressionIter);
		}

		UpdatePropertyWindow();
	}
}

void WxMaterialEditor::OnSelectUpsteamNodes(wxCommandEvent& In)
{
	TArray<UMaterialExpression*> ExpressionsToEvaluate;	// Graph nodes that need to be traced upstream
	TArray<UMaterialExpression*> ExpressionsEvalated;	// Keep track of evaluated graph nodes so we don't re-evaluate them
	TArray<UMaterialExpression*> ExpressionsToSelect;	// Upstream graph nodes that will end up being selected

	// Add currently selected graph nodes to the "need to be traced upstream" list
	for ( TArray<UMaterialExpression*>::TConstIterator ExpressionIter(SelectedExpressions); ExpressionIter; ++ExpressionIter )
	{
		ExpressionsToEvaluate.AddItem(*ExpressionIter);
	}

	// Generate a list of upstream nodes
	while (ExpressionsToEvaluate.Num() > 0)
	{
		UMaterialExpression* CurrentExpression = ExpressionsToEvaluate.Last();
		if (CurrentExpression)
		{
			const TArray<FExpressionInput*>& Intputs = CurrentExpression->GetInputs();

			for (INT InputIndex = 0; InputIndex < Intputs.Num(); InputIndex++)
			{
				const FExpressionInput* CurrentInput = Intputs(InputIndex);
				if (CurrentInput->Expression)
				{
					INT index = -1;
					ExpressionsEvalated.FindItem(CurrentInput->Expression, index);

					if (index < 0)
					{
						// This node is an upstream node (so, we'll need to select it) and it's children need to be traced as well
						ExpressionsToSelect.AddItem(CurrentInput->Expression);
						ExpressionsToEvaluate.AddItem(CurrentInput->Expression);
					}
				}
			}
		}

		// This graph node has now been examined
		ExpressionsEvalated.AddItem(CurrentExpression);
		ExpressionsToEvaluate.RemoveItem(CurrentExpression);
	}

	// Select all upstream nodes
	if (ExpressionsToSelect.Num() > 0)
	{
		for ( TArray<UMaterialExpression*>::TConstIterator ExpressionIter(ExpressionsToSelect); ExpressionIter; ++ExpressionIter )
		{
			AddToSelection(*ExpressionIter);
		}

		UpdatePropertyWindow();
	}
}

void WxMaterialEditor::OnDeleteObjects(wxCommandEvent& In)
{
	DeleteSelectedObjects();
}

void WxMaterialEditor::OnBreakLink(wxCommandEvent& In)
{
	BreakConnLink(FALSE);
}

void WxMaterialEditor::OnBreakAllLinks(wxCommandEvent& In)
{
	BreakAllConnectionsToSelectedExpressions();
}

void WxMaterialEditor::OnRemoveFromFavorites(wxCommandEvent& In)
{
	RemoveSelectedExpressionFromFavorites();
}

void WxMaterialEditor::OnAddToFavorites(wxCommandEvent& In)
{
	AddSelectedExpressionToFavorites();
}

/**
 * Called when clicking the "Preview Node on Mesh" option in a material expression context menu.
 */
void WxMaterialEditor::OnPreviewNode(wxCommandEvent &In)
{
	if (SelectedExpressions.Num() == 1)
	{
		SetPreviewExpression(SelectedExpressions(0));
	}
}

void WxMaterialEditor::OnConnectToMaterial_DiffuseColor(wxCommandEvent& In) { OnConnectToMaterial(0); }
void WxMaterialEditor::OnConnectToMaterial_DiffusePower(wxCommandEvent& In) { OnConnectToMaterial(1); }
void WxMaterialEditor::OnConnectToMaterial_EmissiveColor(wxCommandEvent& In) { OnConnectToMaterial(2); }
void WxMaterialEditor::OnConnectToMaterial_SpecularColor(wxCommandEvent& In) { OnConnectToMaterial(3); }
void WxMaterialEditor::OnConnectToMaterial_SpecularPower(wxCommandEvent& In) { OnConnectToMaterial(4); }
void WxMaterialEditor::OnConnectToMaterial_Opacity(wxCommandEvent& In) { OnConnectToMaterial(5); }
void WxMaterialEditor::OnConnectToMaterial_OpacityMask(wxCommandEvent& In) { OnConnectToMaterial(6); }
void WxMaterialEditor::OnConnectToMaterial_Distortion(wxCommandEvent& In) { OnConnectToMaterial(7);	}
void WxMaterialEditor::OnConnectToMaterial_TransmissionMask(wxCommandEvent& In) { OnConnectToMaterial(8);	}
void WxMaterialEditor::OnConnectToMaterial_TransmissionColor(wxCommandEvent& In) { OnConnectToMaterial(9);	}
void WxMaterialEditor::OnConnectToMaterial_Normal(wxCommandEvent& In) { OnConnectToMaterial(10);	}
void WxMaterialEditor::OnConnectToMaterial_CustomLighting(wxCommandEvent& In) { OnConnectToMaterial(11);	}
void WxMaterialEditor::OnConnectToMaterial_CustomLightingDiffuse(wxCommandEvent& In) { OnConnectToMaterial(12);	}
void WxMaterialEditor::OnConnectToMaterial_AnisotropicDirection(wxCommandEvent& In) { OnConnectToMaterial(13);	}
void WxMaterialEditor::OnConnectToMaterial_WorldPositionOffset(wxCommandEvent& In) { OnConnectToMaterial(14);	}
void WxMaterialEditor::OnConnectToMaterial_WorldDisplacement(wxCommandEvent& In) { OnConnectToMaterial(15);	}
void WxMaterialEditor::OnConnectToMaterial_TessellationMultiplier(wxCommandEvent& In) { OnConnectToMaterial(16);	}
void WxMaterialEditor::OnConnectToMaterial_SubsurfaceInscatteringColor(wxCommandEvent& In) { OnConnectToMaterial(17); }
void WxMaterialEditor::OnConnectToMaterial_SubsurfaceAbsorptionColor(wxCommandEvent& In) { OnConnectToMaterial(18); }
void WxMaterialEditor::OnConnectToMaterial_SubsurfaceScatteringRadius(wxCommandEvent& In) { OnConnectToMaterial(19); }

void WxMaterialEditor::OnConnectToFunctionOutput(wxCommandEvent& In) 
{  
	if (ConnObj)
	{
		const INT SelectedFunctionOutputIndex = In.GetId() - ID_MATERIALEDITOR_CONNECT_TO_FunctionOutputBegin;

		INT FunctionOutputIndex = 0;
		const INT NumFunctionOutputsSupported = ID_MATERIALEDITOR_CONNECT_TO_FunctionOutputEnd - ID_MATERIALEDITOR_CONNECT_TO_FunctionOutputBegin;
		UMaterialExpressionFunctionOutput* FunctionOutput = NULL;

		for (INT ExpressionIndex = 0; ExpressionIndex < Material->Expressions.Num() && FunctionOutputIndex < NumFunctionOutputsSupported; ExpressionIndex++)
		{
			FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(Material->Expressions(ExpressionIndex));
			if (FunctionOutput)
			{
				if (FunctionOutputIndex == SelectedFunctionOutputIndex)
				{
					break;
				}
				FunctionOutputIndex++;
			}
		}

		if (FunctionOutput)
		{
			UMaterialExpression* SelectedMaterialExpression = Cast<UMaterialExpression>(ConnObj);

			// Input expression.
			FExpressionInput* ExpressionInput = FunctionOutput->GetInput(0);

			const INT OutputIndex = ConnIndex;

			// Output expression.
			TArray<FExpressionOutput>& Outputs = SelectedMaterialExpression->GetOutputs();
			const FExpressionOutput& ExpressionOutput = Outputs(OutputIndex);

			{
				const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("MaterialEditorMakeConnection")) );

				FunctionOutput->Modify();
				ConnectExpressions(*ExpressionInput, ExpressionOutput, OutputIndex, SelectedMaterialExpression);
			}
			
			// Update the current preview material.
			UpdatePreviewMaterial();
			Material->MarkPackageDirty();
			RefreshSourceWindowMaterial();
			RefreshExpressionPreviews();
			RefreshExpressionViewport();
			bMaterialDirty = TRUE;
		}
	}
}

void WxMaterialEditor::OnShowHideConnectors(wxCommandEvent& In)
{
	bHideUnusedConnectors = !bHideUnusedConnectors;
	RefreshExpressionViewport();
}


void WxMaterialEditor::OnRealTimeExpressions(wxCommandEvent& In)
{
	LinkedObjVC->ToggleRealtime();
}

void WxMaterialEditor::OnAlwaysRefreshAllPreviews(wxCommandEvent& In)
{
	bAlwaysRefreshAllPreviews = In.IsChecked() ? TRUE : FALSE;
	if ( bAlwaysRefreshAllPreviews )
	{
		RefreshExpressionPreviews();
	}
	RefreshExpressionViewport();
}

void WxMaterialEditor::OnApply(wxCommandEvent& In)
{
	UpdateOriginalMaterial();
}

//forward declaration for texture flattening
void ConditionalFlattenMaterial(UMaterialInterface* MaterialInterface, UBOOL bReflattenAutoFlattened, const UBOOL bInForceFlatten);

void WxMaterialEditor::OnFlatten(wxCommandEvent& In)
{
	//make sure the changes have been committed
	UpdateOriginalMaterial();

	//bake out the flattened texture using the newly committed original material (for naming)
	const UBOOL bReflattenAutoFlattened = TRUE;
	const UBOOL bForceFlatten = TRUE;
	ConditionalFlattenMaterial(OriginalMaterial, bReflattenAutoFlattened, bForceFlatten);
}


void WxMaterialEditor::OnToggleStats(wxCommandEvent& In)
{
	// Toggle the showing of material stats each time the user presses the show stats button
	bShowStats = !bShowStats;
	RefreshExpressionViewport();
}

void WxMaterialEditor::UI_ToggleStats(wxUpdateUIEvent& In)
{
	In.Check( bShowStats == TRUE );
}

void WxMaterialEditor::OnViewSource(wxCommandEvent& In)
{
	FDockingParent::FDockWindowState SourceWindowState;
	if( GetDockingWindowState(SourceWindow, SourceWindowState) )
	{
		ShowDockingWindow( SourceWindow, !SourceWindowState.bIsVisible );
	}
}

void WxMaterialEditor::UI_ViewSource(wxUpdateUIEvent& In)
{
	FDockingParent::FDockWindowState SourceWindowState;
	if( GetDockingWindowState(SourceWindow, SourceWindowState) )
	{
		In.Check( SourceWindowState.bIsVisible ? TRUE : FALSE );
	}
}

void WxMaterialEditor::OnSearchChanged(wxCommandEvent& In)
{
	SearchQuery = In.GetString().c_str();
	UpdateSearch(TRUE);
}

void WxMaterialEditor::ShowSearchResult()
{
	if( SelectedSearchResult >= 0 && SelectedSearchResult < SearchResults.Num() )
	{
		UMaterialExpression* Expression = SearchResults(SelectedSearchResult);

		// Select the selected search item
		EmptySelection();
		AddToSelection(Expression);
		UpdatePropertyWindow();

		PanLocationOnscreen( Expression->MaterialExpressionEditorX+50, Expression->MaterialExpressionEditorY+50, 100 );
	}
}

/**
* PanLocationOnscreen: pan the viewport if necessary to make the particular location visible
*
* @param	X, Y		Coordinates of the location we want onscreen
* @param	Border		Minimum distance we want the location from the edge of the screen.
*/
void WxMaterialEditor::PanLocationOnscreen( INT LocationX, INT LocationY, INT Border )
{
	// Find the bound of the currently visible area of the expression viewport
	INT X1 = -LinkedObjVC->Origin2D.X + appRound((FLOAT)Border*LinkedObjVC->Zoom2D);
	INT Y1 = -LinkedObjVC->Origin2D.Y + appRound((FLOAT)Border*LinkedObjVC->Zoom2D);
	INT X2 = -LinkedObjVC->Origin2D.X + LinkedObjVC->Viewport->GetSizeX() - appRound((FLOAT)Border*LinkedObjVC->Zoom2D);
	INT Y2 = -LinkedObjVC->Origin2D.Y + LinkedObjVC->Viewport->GetSizeY() - appRound((FLOAT)Border*LinkedObjVC->Zoom2D);

	INT X = appRound(((FLOAT)LocationX) * LinkedObjVC->Zoom2D);
	INT Y = appRound(((FLOAT)LocationY) * LinkedObjVC->Zoom2D);

	// See if we need to pan the viewport to show the selected search result.
	LinkedObjVC->DesiredOrigin2D = LinkedObjVC->Origin2D;
	UBOOL bChanged = FALSE;
	if( X < X1 )
	{
		LinkedObjVC->DesiredOrigin2D.X += (X1 - X);
		bChanged = TRUE;
	}
	if( Y < Y1 )
	{
		LinkedObjVC->DesiredOrigin2D.Y += (Y1 - Y);
		bChanged = TRUE;
	}
	if( X > X2 )
	{
		LinkedObjVC->DesiredOrigin2D.X -= (X - X2);
		bChanged = TRUE;
	}
	if( Y > Y2 )
	{
		LinkedObjVC->DesiredOrigin2D.Y -= (Y - Y2);
		bChanged = TRUE;
	}
	if( bChanged )
	{
		// Pan to the result in 0.1 seconds
		LinkedObjVC->DesiredPanTime = 0.1f;
	}
	RefreshExpressionViewport();
}

/**
	* IsActiveMaterialInput: get whether a particular input should be considered "active"
	* based on whether it is relevant to the current material.
* e.g. SubsurfaceXXX inputs are only active when EnableSubsurfaceScattering is TRUE
*
* @param	InputInfo		The input to check.
* @return	TRUE if the input is active and should be shown, FALSE if the input should be hidden.
*/
UBOOL WxMaterialEditor::IsActiveMaterialInput(const FMaterialInputInfo& InputInfo)
{
	return IsActiveMaterialProperty(Material, InputInfo.Property);
}

/**
* Comparison function used to sort search results
*/
IMPLEMENT_COMPARE_POINTER(UMaterialExpression,MaterialEditorSearch,
{ 
	// Divide into grid cells and step horizontally and then vertically.
	INT AGridX = A->MaterialExpressionEditorX / 100;
	INT AGridY = A->MaterialExpressionEditorY / 100;
	INT BGridX = B->MaterialExpressionEditorX / 100;
	INT BGridY = B->MaterialExpressionEditorY / 100;

	if( AGridY < BGridY )
	{
		return -1;
	}
	else
	if( AGridY > BGridY )
	{
		return 1;
	}
	else
	{
		return AGridX - BGridX;
	}
} )

/**
* Updates the SearchResults array based on the search query
*
* @param	bQueryChanged		Indicates whether the update is because of a query change or a potential material content change.
*/
void WxMaterialEditor::UpdateSearch( UBOOL bQueryChanged )
{
	SearchResults.Empty();

	if( SearchQuery.Len() == 0 )
	{
		if( bQueryChanged )
		{
			// We just cleared the search
			SelectedSearchResult = 0;
			ToolBar->SearchControl.EnableNextPrev(FALSE,FALSE);
			RefreshExpressionViewport();
		}
	}
	else
	{
		// Search expressions
		for( INT Index=0;Index<Material->Expressions.Num();Index++ )
		{
			if(Material->Expressions(Index)->MatchesSearchQuery(*SearchQuery) )
			{
				SearchResults.AddItem(Material->Expressions(Index));
			}
		}

		// Search comments
		for( INT Index=0;Index<Material->EditorComments.Num();Index++ )
		{
			if(Material->EditorComments(Index)->MatchesSearchQuery(*SearchQuery) )
			{
				SearchResults.AddItem(Material->EditorComments(Index));
			}
		}

		Sort<USE_COMPARE_POINTER(UMaterialExpression,MaterialEditorSearch)>( &SearchResults(0), SearchResults.Num() );

		if( bQueryChanged )
		{
			// This is a new query rather than a material change, so navigate to first search result.
			SelectedSearchResult = 0;
			ShowSearchResult();
		}
		else
		{
			if( SelectedSearchResult < 0 || SelectedSearchResult >= SearchResults.Num() )
			{
				SelectedSearchResult = 0;
			}
		}

		ToolBar->SearchControl.EnableNextPrev(SearchResults.Num()>0, SearchResults.Num()>0);
	}
}


void WxMaterialEditor::OnSearchNext(wxCommandEvent& In)
{
	SelectedSearchResult++;
	if( SelectedSearchResult >= SearchResults.Num() )
	{
		SelectedSearchResult = 0;
	}
	ShowSearchResult();
}

void WxMaterialEditor::OnSearchPrev(wxCommandEvent& In)
{
	SelectedSearchResult--;
	if( SelectedSearchResult < 0 )
	{
		SelectedSearchResult = Max<INT>(SearchResults.Num()-1,0);
	}
	ShowSearchResult();
}


/**
 * Routes the event to the appropriate handlers
 *
 * @param InType the event that was fired
 */
void WxMaterialEditor::Send(ECallbackEventType InType, UObject* InObject)
{
	switch( InType )
	{
		case CALLBACK_PreEditorClose:
			if ( bMaterialDirty )
			{
				// find out the user wants to do with this dirty material
				switch( appMsgf(AMT_YesNo, *LocalizeUnrealEd("Prompt_MaterialEditorClose")) )
				{
				case ART_Yes:
					UpdateOriginalMaterial();
					break;
				}
			}
			break;
		case CALLBACK_Undo:
			// Only Undo if this material editor DOESN'T have focus, then it's picked up by ::Undo
			wxWindow* FocusWindow = wxWindow::FindFocus();
			if ( !FocusWindow || !FindWindow( FocusWindow->GetId() ) )
			{
				// Make sure it's just an expression we're dealing with and that it belongs to this window
				UMaterialExpression* MaterialExpression = Cast<UMaterialExpression>( InObject );
				if ( MaterialExpression && Material && Material->Expressions.ContainsItem( MaterialExpression ) )
				{
					PreEditUndo();
					PostEditUndo(-1);	// Force it to rebuild the number of expression incase this has been undeleted
				}
			}
			break;
	}
}

void WxMaterialEditor::UI_RealTimeExpressions(wxUpdateUIEvent& In)
{
	In.Check( LinkedObjVC->IsRealtime() == TRUE );
}

void WxMaterialEditor::UI_AlwaysRefreshAllPreviews(wxUpdateUIEvent& In)
{
	In.Check( bAlwaysRefreshAllPreviews == TRUE );
}

void WxMaterialEditor::UI_HideUnusedConnectors(wxUpdateUIEvent& In)
{
	In.Check( bHideUnusedConnectors == TRUE );
}


void WxMaterialEditor::UI_Apply(wxUpdateUIEvent& In)
{
	In.Enable(bMaterialDirty == TRUE);
}

void WxMaterialEditor::OnCameraHome(wxCommandEvent& In)
{
	LinkedObjVC->Origin2D = FIntPoint(-Material->EditorX,-Material->EditorY);
	RefreshExpressionViewport();
}

void WxMaterialEditor::OnCleanUnusedExpressions(wxCommandEvent& In)
{
	CleanUnusedExpressions();
}

void WxMaterialEditor::OnSetPreviewMeshFromSelection(wxCommandEvent& In)
{
	UBOOL bFoundPreviewMesh = FALSE;
	GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);

	// Look for a selected static mesh.
	UStaticMesh* SelectedStaticMesh = GEditor->GetSelectedObjects()->GetTop<UStaticMesh>();
	if ( SelectedStaticMesh )
	{
		SetPreviewMesh( SelectedStaticMesh, NULL );
		Material->PreviewMesh = SelectedStaticMesh->GetPathName();
		bFoundPreviewMesh = TRUE;
	}
	else
	{
		// No static meshes were selected; look for a selected skeletal mesh.
		USkeletalMesh* SelectedSkeletalMesh = GEditor->GetSelectedObjects()->GetTop<USkeletalMesh>();
		if ( SelectedSkeletalMesh && SetPreviewMesh( NULL, SelectedSkeletalMesh ) )
		{
			Material->PreviewMesh = SelectedSkeletalMesh->GetPathName();
			bFoundPreviewMesh = TRUE;
		}
	}

	if ( bFoundPreviewMesh )
	{
		Material->MarkPackageDirty();
		RefreshPreviewViewport();
		bMaterialDirty = TRUE;
	}
}

void WxMaterialEditor::OnMaterialExpressionTreeDrag(wxTreeEvent& DragEvent)
{
	const FString SelectedString( MaterialExpressionList->GetSelectedTreeString(DragEvent) );
	if ( SelectedString.Len() > 0 )
	{
		wxTextDataObject DataObject( *SelectedString );
		wxDropSource DragSource( this );
		DragSource.SetData( DataObject );
		DragSource.DoDragDrop( TRUE );
	}
}

void WxMaterialEditor::OnMaterialExpressionListDrag(wxListEvent& In)
{
	const FString SelectedString( MaterialExpressionList->GetSelectedListString() );
	if ( SelectedString.Len() > 0 )
	{
		wxTextDataObject DataObject( *SelectedString );
		wxDropSource DragSource( this );
		DragSource.SetData( DataObject );
		DragSource.DoDragDrop( TRUE );
	}
}

void WxMaterialEditor::OnMaterialFunctionTreeDrag(wxTreeEvent& DragEvent)
{
	const FString SelectedString( MaterialFunctionLibraryList->GetSelectedTreeString(DragEvent) );
	if ( SelectedString.Len() > 0 )
	{
		wxTextDataObject DataObject( *SelectedString );
		wxDropSource DragSource( this );
		DragSource.SetData( DataObject );
		DragSource.DoDragDrop( TRUE );
	}
}

void WxMaterialEditor::OnMaterialFunctionListDrag(wxListEvent& In)
{
	const FString SelectedString( MaterialFunctionLibraryList->GetSelectedListString() );
	if ( SelectedString.Len() > 0 )
	{
		wxTextDataObject DataObject( *SelectedString );
		wxDropSource DragSource( this );
		DragSource.SetData( DataObject );
		DragSource.DoDragDrop( TRUE );
	}
}

/**
 * Retrieves all visible parameters within the material.
 *
 * @param	Material			The material to retrieve the parameters from.
 * @param	MaterialInstance	The material instance that contains all parameter overrides.
 * @param	VisisbleExpressions	The array that will contain the id's of the visible parameter expressions.
 */
void WxMaterialEditor::GetVisibleMaterialParameters(const UMaterial *Material, UMaterialInstance *MaterialInstance, TArray<FGuid> &VisibleExpressions)
{
	VisibleExpressions.Empty();

	InitMaterialExpressionClasses();

	TArray<UMaterialExpression*> ProcessedExpressions;
	GetVisibleMaterialParametersFromExpression(Material->DiffuseColor.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
	GetVisibleMaterialParametersFromExpression(Material->DiffusePower.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
	GetVisibleMaterialParametersFromExpression(Material->SpecularColor.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
	GetVisibleMaterialParametersFromExpression(Material->SpecularPower.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
	GetVisibleMaterialParametersFromExpression(Material->Normal.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
	GetVisibleMaterialParametersFromExpression(Material->EmissiveColor.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
	GetVisibleMaterialParametersFromExpression(Material->Opacity.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
	GetVisibleMaterialParametersFromExpression(Material->OpacityMask.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
	GetVisibleMaterialParametersFromExpression(Material->Distortion.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
	GetVisibleMaterialParametersFromExpression(Material->CustomLighting.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
	GetVisibleMaterialParametersFromExpression(Material->CustomSkylightDiffuse.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
	GetVisibleMaterialParametersFromExpression(Material->AnisotropicDirection.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
	GetVisibleMaterialParametersFromExpression(Material->TwoSidedLightingMask.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
	GetVisibleMaterialParametersFromExpression(Material->TwoSidedLightingColor.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
	GetVisibleMaterialParametersFromExpression(Material->WorldPositionOffset.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
	GetVisibleMaterialParametersFromExpression(Material->WorldDisplacement.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
	GetVisibleMaterialParametersFromExpression(Material->TessellationMultiplier.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
	GetVisibleMaterialParametersFromExpression(Material->SubsurfaceInscatteringColor.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
	GetVisibleMaterialParametersFromExpression(Material->SubsurfaceAbsorptionColor.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
	GetVisibleMaterialParametersFromExpression(Material->SubsurfaceScatteringRadius.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);

}

/**
 *	Checks if the given expression is in the favorites list...
 *
 *	@param	InExpression	The expression to check for.
 *
 *	@return	UBOOL			TRUE if it's in the list, FALSE if not.
 */
UBOOL WxMaterialEditor::IsMaterialExpressionInFavorites(UMaterialExpression* InExpression)
{
	for (INT CheckIndex = 0; CheckIndex < FavoriteExpressionClasses.Num(); CheckIndex++)
	{
		if (FavoriteExpressionClasses(CheckIndex) == InExpression->GetClass())
		{
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * Recursively walks the expression tree and parses the visible expression branches.
 *
 * @param	MaterialExpression	The expression to parse.
 * @param	MaterialInstance	The material instance that contains all parameter overrides.
 * @param	VisisbleExpressions	The array that will contain the id's of the visible parameter expressions.
 */
void WxMaterialEditor::GetVisibleMaterialParametersFromExpression(UMaterialExpression *MaterialExpression, UMaterialInstance *MaterialInstance, TArray<FGuid> &VisibleExpressions, TArray<UMaterialExpression*> &ProcessedExpressions)
{
	if(!MaterialExpression)
	{
		return;
	}

	check(MaterialInstance);

	//don't allow re-entrant expressions to continue
	if (ProcessedExpressions.ContainsItem(MaterialExpression))
	{
		return;
	}
	ProcessedExpressions.Push(MaterialExpression);

	// if it's a material parameter it must be visible so add it to the map
	UMaterialExpressionParameter *Param = Cast<UMaterialExpressionParameter>( MaterialExpression );
	UMaterialExpressionTextureSampleParameter *TexParam = Cast<UMaterialExpressionTextureSampleParameter>( MaterialExpression );
	UMaterialExpressionFontSampleParameter *FontParam = Cast<UMaterialExpressionFontSampleParameter>( MaterialExpression );
	if( Param )
	{
		VisibleExpressions.AddUniqueItem(Param->ExpressionGUID);

		UMaterialExpressionScalarParameter *ScalarParam = Cast<UMaterialExpressionScalarParameter>( MaterialExpression );
		UMaterialExpressionVectorParameter *VectorParam = Cast<UMaterialExpressionVectorParameter>( MaterialExpression );
		TArray<FName> Names;
		TArray<FGuid> Ids;
		if( ScalarParam )
		{
			MaterialInstance->GetMaterial()->GetAllScalarParameterNames( Names, Ids );
			for( int i = 0; i < Names.Num(); i++ )
			{
				if( Names(i) == ScalarParam->ParameterName )
				{
					VisibleExpressions.AddUniqueItem( Ids( i ) );
				}
			}
		}
		else if ( VectorParam )
		{
			MaterialInstance->GetMaterial()->GetAllVectorParameterNames( Names, Ids );
			for( int i = 0; i < Names.Num(); i++ )
			{
				if( Names(i) == VectorParam->ParameterName )
				{
					VisibleExpressions.AddUniqueItem( Ids( i ) );
				}
			}
		}
	}
	else if(TexParam)
	{
		VisibleExpressions.AddUniqueItem( TexParam->ExpressionGUID );
		TArray<FName> Names;
		TArray<FGuid> Ids;
		MaterialInstance->GetMaterial()->GetAllTextureParameterNames( Names, Ids );
		for( int i = 0; i < Names.Num(); i++ )
		{
			if( Names(i) == TexParam->ParameterName )
			{
				VisibleExpressions.AddUniqueItem( Ids( i ) );
			}
		}
	}
	else if(FontParam)
	{
		VisibleExpressions.AddUniqueItem( FontParam->ExpressionGUID );
		TArray<FName> Names;
		TArray<FGuid> Ids;
		MaterialInstance->GetMaterial()->GetAllFontParameterNames( Names, Ids );
		for( int i = 0; i < Names.Num(); i++ )
		{
			if( Names(i) == FontParam->ParameterName )
			{
				VisibleExpressions.AddUniqueItem( Ids( i ) );
			}
		}
	}

	// check if it's a switch expression and branch according to its value
	UMaterialExpressionStaticSwitchParameter *Switch = Cast<UMaterialExpressionStaticSwitchParameter>(MaterialExpression);
	if(Switch)
	{
		UBOOL Value = FALSE;
		FGuid ExpressionID;
		MaterialInstance->GetStaticSwitchParameterValue(Switch->ParameterName, Value, ExpressionID);
		VisibleExpressions.AddUniqueItem(ExpressionID);

		if(Value)
		{
			GetVisibleMaterialParametersFromExpression(Switch->A.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
		}
		else
		{
			GetVisibleMaterialParametersFromExpression(Switch->B.Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
		}
	}
	else
	{
		const TArray<FExpressionInput*>& ExpressionInputs = MaterialExpression->GetInputs();
		for(INT ExpressionInputIndex = 0; ExpressionInputIndex < ExpressionInputs.Num(); ExpressionInputIndex++)
		{
			//retrieve the expression input and then start parsing its children
			FExpressionInput* Input = ExpressionInputs(ExpressionInputIndex);
			GetVisibleMaterialParametersFromExpression(Input->Expression, MaterialInstance, VisibleExpressions, ProcessedExpressions);
		}
	}

	UMaterialExpression* TopExpression = ProcessedExpressions.Pop();
	//ensure that the top of the stack matches what we expect (the same as MaterialExpression)
	check(MaterialExpression == TopExpression);
}

/**
* Run the HLSL material translator and refreshes the View Source window.
*/
void WxMaterialEditor::RefreshSourceWindowMaterial()
{
	SourceWindow->RefreshWindow();
}

/**
 * Recompiles the material used the preview window.
 */
void WxMaterialEditor::UpdatePreviewMaterial( )
{
	// do we have a quality switch? set it before compiling
	UBOOL bHasQualitySwitch = FALSE;
	for (INT ExprIndex = 0; ExprIndex < Material->Expressions.Num(); ExprIndex++)
	{
		if (Material->Expressions(ExprIndex)->IsA(UMaterialExpressionQualitySwitch::StaticClass()))
		{
			bHasQualitySwitch = TRUE;
			break;
		}
	}

	if( PreviewExpression )
	{
		// The preview material's expressions array must stay up to date before recompiling 
		// So that RebuildMaterialFunctionInfo will see all the nested material functions that may need to be updated
		ExpressionPreviewMaterial->Expressions = Material->Expressions;

		ExpressionPreviewMaterial->bHasQualitySwitch = bHasQualitySwitch;

		// If we are previewing an expression, update the expression preview material
		ExpressionPreviewMaterial->PreEditChange( NULL );
		ExpressionPreviewMaterial->PostEditChange();
	}
	else 
	{
		Material->bHasQualitySwitch = bHasQualitySwitch;

		// Update the regular preview material when not previewing an expression.
		Material->PreEditChange( NULL );
		Material->PostEditChange();
	}

	// Reattach all components that use the preview material, since UMaterial::PEC does not reattach components using a bIsPreviewMaterial=true material
	RefreshPreviewViewport();
}

/** Sets the expression to be previewed. */
void WxMaterialEditor::SetPreviewExpression(UMaterialExpression* NewPreviewExpression)
{
	UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(NewPreviewExpression);

	if( PreviewExpression == NewPreviewExpression )
	{
		if (FunctionOutput)
		{
			FunctionOutput->bLastPreviewed = FALSE;
		}
		// If we are already previewing the selected expression toggle previewing off
		PreviewExpression = NULL;
		ExpressionPreviewMaterial->Expressions.Empty();
		SetPreviewMaterial( Material );
		// Recompile the preview material to get changes that might have been made during previewing
		UpdatePreviewMaterial();
	}
	else if (NewPreviewExpression)
	{
		if( ExpressionPreviewMaterial == NULL )
		{
			// Create the expression preview material if it hasnt already been created
			ExpressionPreviewMaterial = (UMaterial*)UObject::StaticConstructObject( UMaterial::StaticClass(), UObject::GetTransientPackage(), NAME_None, RF_Public);
			ExpressionPreviewMaterial->bIsPreviewMaterial = TRUE;
		}

		if (FunctionOutput)
		{
			FunctionOutput->bLastPreviewed = TRUE;
		}

		// The expression preview material's expressions array must stay up to date before recompiling 
		// So that RebuildMaterialFunctionInfo will see all the nested material functions that may need to be updated
		ExpressionPreviewMaterial->Expressions = Material->Expressions;

		// The preview window should now show the expression preview material
		SetPreviewMaterial( ExpressionPreviewMaterial );

		// Connect the selected expression to the emissive node of the expression preview material.  The emissive material is not affected by light which is why its a good choice.
		ConnectMaterialToMaterialExpression( ExpressionPreviewMaterial, 2, NewPreviewExpression, 0, FALSE );
		// Set the preview expression
		PreviewExpression = NewPreviewExpression;

		// Recompile the preview material
		UpdatePreviewMaterial();
	}
}
