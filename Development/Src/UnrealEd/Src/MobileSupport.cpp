/*=============================================================================
	MobileSupport.cpp: Editor support for mobile devices
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnConsoleSupportContainer.h"
#include <ddraw.h>

#if WITH_SUBSTANCE_AIR == 1
// forward declarations
namespace SubstanceAir
{
	struct FOutputInstance; 
	struct FGraphInstance;
}
#include "SubstanceAirTextureClasses.h"
#endif // WITH_SUBSTANCE_AIR

// we need to set this to 1 so ThumbnailHelpers defines the thumbnail scene that we use to flatten
#define _WANTS_MATERIAL_HELPER 1
#include "ThumbnailHelpers.h"

#define USE_LEGACY_CONVERTER 0

#if USE_LEGACY_CONVERTER
	#define PVRTEXTOOL TEXT("..\\Redist\\ImgTec\\PVRTexTool.exe")
	#define PVRTEXMAXSIZE 2048
#elif _WIN64
	#define PVRTEXTOOL TEXT("..\\Redist\\ImgTec\\PVRTexToolCL_64.exe")
	#define PVRTEXMAXSIZE 4096
#else
	#define PVRTEXTOOL TEXT("..\\Redist\\ImgTec\\PVRTexToolCL_32.exe")
	#define PVRTEXMAXSIZE 4096
#endif

#define LEGACY_PVRTEXTOOL TEXT("..\\Redist\\ImgTec\\PVRTexTool.exe")

FString UEditorEngine::GetMobileDeviceSystemSettingsSection() const
{
	FString DeviceName;

	switch (BuildPlayDevice)
	{
	case BPD_IPHONE_3GS:
		DeviceName=TEXT("IPhone3GS");
		break;
	case BPD_IPHONE_4:
		DeviceName=TEXT("IPhone4");
		break;
	case BPD_IPHONE_4S:
		DeviceName=TEXT("IPhone4S");
		break;
	case BPD_IPHONE_5:
		DeviceName=TEXT("IPhone5");
		break;
	case BPD_IPOD_TOUCH_4:
		DeviceName=TEXT("IPodTouch4");
		break;
	case BPD_IPOD_TOUCH_5:
		DeviceName=TEXT("IPodTouch5");
		break;
	case BPD_IPAD:
		DeviceName=TEXT("IPad");
		break;
	case BPD_IPAD2:
		DeviceName=TEXT("IPad2");
		break;
	case BPD_IPAD3:
		DeviceName=TEXT("IPad3");
		break;
	case BPD_IPAD4:
		DeviceName=TEXT("IPad4");
		break;
	case BPD_IPAD_MINI:
		DeviceName=TEXT("IPadMini");
		break;
#if !UDK
	case BPD_XBOX_360:
		DeviceName=TEXT("Xbox360");
		break;
	case BPD_PS3:
		DeviceName=TEXT("PS3");
		break;
	case BPD_FLASH:
		DeviceName=TEXT("Flash");
		break;
	case BPD_PSVITA:
		DeviceName=TEXT("NGP");
		break;
#endif
	default:
		break;
	}
	return DeviceName;
}

/** Initializes mobile global variables, must be called once before the rendering thread has been started. */
void InitializeMobileSettings()
{
	// Setup mobile emulation settings.  This needs to be set before the render thread is initialized.
	GEmulateMobileRendering = GEditor->GetUserSettings().bEmulateMobileFeatures;

	// Choose whether to use simple lightmaps or not based on the system setting of the target mobile device
	UBOOL bAllowDirectionalLightmaps = FALSE;
	if (GSystemSettings.bAllowDirectionalLightMaps)
	{
		GUseSimpleLightmapsForMobileEmulation = GEmulateMobileRendering && GWorld && (GWorld->GetWorldInfo()->PreferredLightmapType != EPLT_Directional);
	}
	else
	{
		GUseSimpleLightmapsForMobileEmulation = GEmulateMobileRendering;
	}

	GAlwaysOptimizeContentForMobile = GEditor->GetUserSettings().bAlwaysOptimizeContentForMobile || GEditor->GetUserSettings().bEmulateMobileFeatures;
	GEmulateMobileInput = GEditor->GetUserSettings().bEmulateMobileFeatures;
}

/**
 * Sets which mobile emulation features are enabled.  This is only intended to be used in Unreal Editor.
 * Note that this function can sometimes take awhile to complete as graphics resource may need updating.
 *
 * @param	bNewEmulateMobileRendering					Enables or disables mobile rendering emulation
 * @param	bNewUseGammaCorrectionForMobileEmulation	Enables or disables gamma correction when emulation mobile
 * @param	bReattachComponents							Reattaches components after setting the new emulate mobile rendering setting
 */
void SetMobileRenderingEmulation( const UBOOL bNewEmulateMobileRendering, const UBOOL bNewUseGammaCorrectionForMobileEmulation, const UBOOL bReattachComponents )
{
	static EBuildPlayDevice LastBuildPlayDevice = BPD_DEFAULT;
	const UBOOL bTogglingMobileRendering = GEmulateMobileRendering != bNewEmulateMobileRendering;
	const UBOOL bTargetHasChanged = GEditor->BuildPlayDevice != LastBuildPlayDevice;
	const UBOOL bTogglingGammaCorrection = GUseGammaCorrectionForMobileEmulation != bNewUseGammaCorrectionForMobileEmulation;

	if( bTogglingMobileRendering || bTargetHasChanged || bTogglingGammaCorrection )
	{
		LastBuildPlayDevice = GEditor->BuildPlayDevice;
		const UBOOL bHadSRGB = !GEmulateMobileRendering || GUseGammaCorrectionForMobileEmulation;
		const UBOOL bWantSRGB = !bNewEmulateMobileRendering || bNewUseGammaCorrectionForMobileEmulation;

		const UBOOL bNeedTexturesUpdated = bHadSRGB != bWantSRGB;

		// Changing the behavior of DirectionalLightMaps will require us to re-add light
		// mapped static meshes to the scene, so we'll issue a global reattach.
		TScopedPointer< FGlobalComponentReattachContext > ReattachContext;
		if( (bTogglingMobileRendering || bTargetHasChanged) && bReattachComponents )
		{
			ReattachContext.Reset( new FGlobalComponentReattachContext() );
		}

		// Flush the rendering thread while we toggle mobile rendering emulation, as it will be looking at
		// GEmulateMobileRendering while processing draw calls.  
		FlushRenderingCommands();

		// Update globals now that the render thread has been flushed
		GEmulateMobileRendering = bNewEmulateMobileRendering;

		// Choose whether to use simple lightmaps or not based on the system setting of the target mobile device
		// Choose whether to use simple lightmaps or not based on the system setting of the target mobile device
		UBOOL bAllowDirectionalLightmaps = FALSE;
		if (GSystemSettings.bAllowDirectionalLightMaps)
		{
			GUseSimpleLightmapsForMobileEmulation = GEmulateMobileRendering && GWorld && (GWorld->GetWorldInfo()->PreferredLightmapType != EPLT_Directional);
		}
		else
		{
			GUseSimpleLightmapsForMobileEmulation = GEmulateMobileRendering;
		}
		GUseGammaCorrectionForMobileEmulation = bNewUseGammaCorrectionForMobileEmulation;

#if WITH_EDITOR
		if (GEmulateMobileRendering == TRUE) 
		{
			FMobileEmulationMaterialManager::GetManager()->UpdateCachedMaterials(FALSE, TRUE);
		}
#endif

		// Toggle gamma correction in D3D11 requires all textures to be refreshed in place.
		// Only update textures if we're actually in mobile emulation mode.  Otherwise, they'll
		// be updated when the user toggles into that mode.
		if( bNeedTexturesUpdated )
		{
			// For D3D11, we need to refresh all textures in place.  This is because whether or not we're
			// using gamma correction is built into the texture format on that RHI
			if( GRHIShaderPlatform == SP_PCD3D_SM4 || GRHIShaderPlatform == SP_PCD3D_SM5 )
			{
				for( TObjectIterator<UTexture> TextureIt; TextureIt; ++TextureIt )
				{
					UTexture* Texture = *TextureIt;
					if( Texture != NULL )
					{
						Texture->UpdateResource();
					}
				}
			}
		}
	}
}

/**
* Class for rendering previews of material expressions in the material editor's linked object viewport.
*/
class FFlattenMaterialProxy : public FMaterial, public FMaterialRenderProxy
{
public:

	/**
	 * Constructor 
	 */
	FFlattenMaterialProxy(UMaterialInterface* InMaterialInterface, EFlattenType InFlattenType, FProxyMaterialCompiler* InOverrideMaterialCompiler=NULL)
		: MaterialInterface(InMaterialInterface)
		, Material(InMaterialInterface->GetMaterial())
		, OverrideMaterialCompiler(InOverrideMaterialCompiler)
		, FlattenType(InFlattenType)
	{
		check(Material);

		// always use high quality for flattening
		CacheShaders(GRHIShaderPlatform, MSQ_HIGH);
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
		if (VertexFactoryType == FindVertexFactoryType(FName(TEXT("FLocalVertexFactory"), FNAME_Find)))
		{
			// compile the shader for the local vertex factory
			return TRUE;
		}

		return FALSE;
	}

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
		return MaterialInterface->GetRenderProxy(0)->GetVectorValue(ParameterName, OutValue, Context);
	}

	virtual UBOOL GetScalarValue(const FName ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const
	{
		return MaterialInterface->GetRenderProxy(0)->GetScalarValue(ParameterName, OutValue, Context);
	}

	virtual UBOOL GetTextureValue(const FName ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const
	{
		return MaterialInterface->GetRenderProxy(0)->GetTextureValue(ParameterName,OutValue,Context);
	}

	// Material properties.
	virtual INT CompileProperty(EMaterialProperty Property,FMaterialCompiler* Compiler) const
	{
		UMaterial* Material = MaterialInterface->GetMaterial();
		check(Material);
		
		// If the property is not active, don't compile it
		if (!IsActiveMaterialProperty(Material, Property))
		{
			return INDEX_NONE;
		}
		
		if (Property == MP_SpecularColor)
		{
			// we need to tell the compiler which property this constant is for so that the code chunk is
			// put in the proper array
			Compiler->SetMaterialProperty(MP_SpecularColor);
			return Compiler->Constant4(0.0f, 0.0f, 0.0f, 0.0f);
		}
		else if (FlattenType == FLATTEN_NormalTexture && Property == MP_EmissiveColor)
		{
			// Output the material's normalized tangent space normal to emissive.
			Compiler->SetMaterialProperty(MP_EmissiveColor);
			UMaterial* BaseMaterial = MaterialInterface->GetMaterial();
			check(BaseMaterial);
			const INT TangentNormal = BaseMaterial->Normal.Compile(OverrideMaterialCompiler ? OverrideMaterialCompiler : Compiler, FVector(0.0f, 0.0f, 1.0f));
			const INT NormalizedTangentNormal = Compiler->Div(TangentNormal,Compiler->SquareRoot(Compiler->Dot(TangentNormal,TangentNormal)));
			const INT OneHalf = Compiler->Constant3( 0.5f, 0.5f, 0.5f );
			return Compiler->Add( Compiler->Mul( NormalizedTangentNormal, OneHalf ), OneHalf );
		}
		else if (FlattenType == FLATTEN_NormalTexture && Property == MP_DiffuseColor)
		{
			// Output a black diffuse color when rendering normal maps.
			Compiler->SetMaterialProperty(MP_DiffuseColor);
			return Compiler->Constant4(0.0f, 0.0f, 0.0f, 0.0f);
		}
		else if (FlattenType == FLATTEN_DiffuseTexture && Property == MP_DiffuseColor)
		{
			// Output a black diffuse color when rendering diffuse maps.
			Compiler->SetMaterialProperty(MP_DiffuseColor);
			return Compiler->Constant4(0.0f, 0.0f, 0.0f, 0.0f);
		}
		else if (FlattenType == FLATTEN_DiffuseTexture && Property == MP_EmissiveColor)
		{
			// Output the material's diffuse color to emissive.
			Compiler->SetMaterialProperty(MP_EmissiveColor);
			UMaterial* BaseMaterial = MaterialInterface->GetMaterial();
			check(BaseMaterial);
			const INT DiffuseColor = BaseMaterial->DiffuseColor.Compile(OverrideMaterialCompiler ? OverrideMaterialCompiler : Compiler, FColor(0,0,0));
			return DiffuseColor;
		}
		else
		{
			return MaterialInterface->GetMaterialResource()->CompileProperty(Property, OverrideMaterialCompiler ? OverrideMaterialCompiler : Compiler);
		}
	}

	virtual FString GetMaterialUsageDescription() const { return FString::Printf(TEXT("FFlattenMaterial %s"), MaterialInterface ? *MaterialInterface->GetName() : TEXT("NULL")); }
	virtual UBOOL IsTwoSided() const 
	{ 
		return Material->TwoSided == 1;
	}
	virtual UBOOL RenderTwoSidedSeparatePass() const
	{ 
		return Material->TwoSidedSeparatePass == 1;
	}
	virtual UBOOL RenderLitTranslucencyPrepass() const
	{ 
		return Material->bUseLitTranslucencyDepthPass == 1;
	}
	virtual UBOOL RenderLitTranslucencyDepthPostpass() const
	{ 
		return Material->bUseLitTranslucencyPostRenderDepthPass == 1;
	}
	virtual UBOOL CastLitTranslucencyShadowAsMasked() const
	{ 
		return Material->bCastLitTranslucencyShadowAsMasked == 1;
	}
	virtual UBOOL NeedsDepthTestDisabled() const
	{
		return Material->bDisableDepthTest == 1;
	}
	virtual UBOOL IsLightFunction() const
	{
		return Material->bUsedAsLightFunction == 1;
	}
	virtual UBOOL IsUsedWithFogVolumes() const
	{
		return FALSE;
	}
	virtual UBOOL IsSpecialEngineMaterial() const
	{
		return Material->bUsedAsSpecialEngineMaterial == 1;
	}
	virtual UBOOL IsUsedWithMobileLandscape() const
	{
		return FALSE;
	}
	virtual UBOOL IsTerrainMaterial() const
	{
		return FALSE;
	}
	virtual UBOOL IsDecalMaterial() const
	{
		return Material->bUsedWithDecals == 1;
	}
	virtual UBOOL IsWireframe() const
	{
		return FALSE;
	}
	virtual UBOOL IsDistorted() const
	{
		return Material->bUsesDistortion && !Material->bUseOneLayerDistortion; 
	}
	virtual UBOOL HasSubsurfaceScattering() const
	{
		return Material->EnableSubsurfaceScattering;
	}
	virtual UBOOL HasSeparateTranslucency() const
	{
		return Material->EnableSeparateTranslucency;
	}
	virtual UBOOL IsMasked() const
	{
		return Material->bIsMasked;
	}
	virtual enum EBlendMode GetBlendMode() const					
	{
		return (EBlendMode)Material->BlendMode; 
	}
	virtual enum EMaterialLightingModel GetLightingModel() const	
	{
		return (EMaterialLightingModel)Material->LightingModel; 
	}
	virtual FLOAT GetOpacityMaskClipValue() const
	{
		return Material->OpacityMaskClipValue; 
	}
	virtual FString GetFriendlyName() const
	{
		return FString::Printf(TEXT("FFlattenMaterialProxy %s"), MaterialInterface ? *MaterialInterface->GetName() : TEXT("NULL")); 
	}

	/**
	* Should shaders compiled for this material be saved to disk?
	*/
	virtual UBOOL IsPersistent() const { return FALSE; }

	const UMaterialInterface* GetMaterialInterface() const
	{
		return MaterialInterface;
	}

	friend FArchive& operator<< ( FArchive& Ar, FFlattenMaterialProxy& V )
	{
		return Ar << V.MaterialInterface;
	}

private:
	/** The material interface for this proxy */
	UMaterialInterface* MaterialInterface;

	/** The usable material for this proxy */
	UMaterial* Material;

	/** Compiler to use while compiling material expressions (can be NULL to use default compiler) */
	FProxyMaterialCompiler* OverrideMaterialCompiler;

	/** What flatten mode should be rendered. */
	EFlattenType FlattenType;
};


class UFlattenMaterial : public UMaterialInterface
{
	DECLARE_CLASS_INTRINSIC(UFlattenMaterial, UMaterialInterface, CLASS_Transient, UnrealEd);

public:

	/**
	 * Set up this material to use another material as its master
	 */
	void Initialize(UMaterialInterface* InSourceMaterial, EFlattenType InFlattenType)
	{
		SourceMaterial = InSourceMaterial;

		delete MaterialProxy;
		MaterialProxy = new FFlattenMaterialProxy(SourceMaterial,InFlattenType);
	}

	/**
	 * Cleanup unneeded references
	 */
	void Cleanup()
	{
		SourceMaterial = NULL;

		delete MaterialProxy;
		MaterialProxy = NULL;
	}

	/**
	 * Get the material which this is an instance of.
	 */
	virtual class UMaterial* GetMaterial()
	{
		return SourceMaterial->GetMaterial();
	}

	/**
	 * Tests this material instance for dependency on a given material instance.
	 * @param	TestDependency - The material instance to test this instance for dependency upon.
	 * @return	True if the material instance is dependent on TestDependency.
	 */
	virtual UBOOL IsDependent(UMaterialInterface* TestDependency)
	{
		return SourceMaterial->IsDependent(TestDependency);
	}

	/**
	 * Returns a pointer to the FMaterialRenderProxy used for rendering.
	 *
	 * @param	Selected	specify TRUE to return an alternate material used for rendering this material when part of a selection
	 *						@note: only valid in the editor!
	 *
	 * @return	The resource to use for rendering this material instance.
	 */
	virtual FMaterialRenderProxy* GetRenderProxy(UBOOL Selected, UBOOL bHovered=FALSE) const
	{
		return MaterialProxy;
	}

	/**
	 * Returns a pointer to the physical material used by this material instance.
	 * @return The physical material.
	 */
	virtual UPhysicalMaterial* GetPhysicalMaterial() const
	{
		return SourceMaterial->GetPhysicalMaterial();
	}

	/**
	 * Gathers the textures used to render the material instance.
	 * @param OutTextures	Upon return contains the textures used to render the material instance.
	 * @param Quality		The platform to get material textures for. If unspecified, it will get textures for current SystemSetting
	 * @param bAllQualities	Whether to iterate for all platforms. The Platform parameter is ignored if this is set to TRUE.
	 * @param bAllowOverride Whether you want to be given the original textures or allow override textures instead of the originals.
	 */
	virtual void GetUsedTextures(TArray<UTexture*> &OutTextures, const EMaterialShaderQuality Quality=MSQ_UNSPECIFIED, const UBOOL bAllQualities=FALSE, UBOOL bAllowOverride = FALSE)
	{
		SourceMaterial->GetUsedTextures(OutTextures, Quality, bAllQualities, bAllowOverride);
	}

	/**
	 * Overrides a specific texture (transient)
	 *
	 * @param InTextureToOverride The texture to override
	 * @param OverrideTexture The new texture to use
	 */
	virtual void OverrideTexture( const UTexture* InTextureToOverride, UTexture* OverrideTexture ) 
	{
		SourceMaterial->OverrideTexture(InTextureToOverride, OverrideTexture);
	}

	/**
	 * Checks if the material can be used with the given usage flag.  
	 * If the flag isn't set in the editor, it will be set and the material will be recompiled with it.
	 * @param Usage - The usage flag to check
	 * @param bSkipPrim - Bypass the primitive type checks
	 * @return UBOOL - TRUE if the material can be used for rendering with the given type.
	 */
	virtual UBOOL CheckMaterialUsage(const EMaterialUsage Usage, const UBOOL bSkipPrim)
	{
		return SourceMaterial->CheckMaterialUsage(Usage, bSkipPrim);
	}

	/**
	* Allocates a new material resource
	* @return	The allocated resource
	*/
	virtual FMaterialResource* AllocateResource()
	{
		return SourceMaterial->AllocateResource();
	}

	/**
	 * Gets the static permutation resource if the instance has one
	 * @return - the appropriate FMaterialResource if one exists, otherwise NULL
	 */
	virtual FMaterialResource* GetMaterialResource(EMaterialShaderQuality OverrideQuality=MSQ_UNSPECIFIED) 
	{
		return SourceMaterial->GetMaterialResource(OverrideQuality);
	}

private:
	/** The real material this flattener renders with */
	UMaterialInterface* SourceMaterial;

	/** The material proxy that can recompile the material without specular */
	FFlattenMaterialProxy* MaterialProxy;
};
IMPLEMENT_CLASS(UFlattenMaterial);


/**
 * Calculate the size that the auto-flatten texture for this material should be
 *
 * @param MaterialInterface The material to scan for texture size
 *
 * @return Size to make the auto-flattened texture
 */
INT CalcAutoFlattenTextureSize(UMaterialInterface* MaterialInterface)
{
	// static lod settings so that we only initialize them once
	static FTextureLODSettings GameTextureLODSettings;
	static UBOOL bAreLODSettingsInitialized=FALSE;
	if (!bAreLODSettingsInitialized)
	{
		// initialize LOD settings with game texture resolutions, since we don't want to use 
		// potentially bloated editor settings
		GameTextureLODSettings.Initialize(GSystemSettingsIni, TEXT("SystemSettings"));
	}

	TArray<UTexture*> MaterialTextures;
	
	MaterialInterface->GetUsedTextures(MaterialTextures);

	// find the largest texture in the list (applying it's LOD bias)
	INT MaxSize = 0;
	for (INT TexIndex = 0; TexIndex < MaterialTextures.Num(); TexIndex++)
	{
		UTexture* Texture = MaterialTextures(TexIndex);

		if (Texture == NULL)
		{
			continue;
		}

		// get the max size of the texture
		INT TexSize = 0;
		if (Texture->IsA(UTexture2D::StaticClass()))
		{
			UTexture2D* Tex2D = (UTexture2D*)Texture;
			TexSize = Max(Tex2D->SizeX, Tex2D->SizeY);
		}
		else if (Texture->IsA(UTextureCube::StaticClass()))
		{
			UTextureCube* TexCube = (UTextureCube*)Texture;
			TexSize = Max(TexCube->SizeX, TexCube->SizeY);
		}

		// bias the texture size based on LOD group
		INT BiasedSize = TexSize >> GameTextureLODSettings.CalculateLODBias(Texture);

		// finally, update the max size for the material
		MaxSize = Max<INT>(MaxSize, BiasedSize);
	}

	// if the material has no textures, then just default to 128
	if (MaxSize == 0)
	{
		MaxSize = 128;
	}

	// Make sure the texture size is a power of two
	MaxSize = appRoundUpToPowerOfTwo( MaxSize );

	// now, bias this by a global "mobile flatten bias"
	INT FlattenedTextureResolutionBias = 0;
	GConfig->GetInt(TEXT("MobileSupport"), TEXT("FlattenedTextureResolutionBias"), FlattenedTextureResolutionBias, GEngineIni);
	MaxSize >>= FlattenedTextureResolutionBias;

	// make the material at least 16x16, because PVRTC2 can't do 8x8
	MaxSize = Max(MaxSize, 16);

	// finally, return what we calculated
	return MaxSize;
}

/**
 * Flatten the given material to the desired size.
 *
 * @param MaterialInterface The material to flatten.
 * @param FlattenType How to flatten the material.
 * @param TextureName The name of the flattened texture.
 * @param TextureOuter The outer of the flattened texture.
 * @param TextureObjectFlags Object flags with which to create the flattened texture.
 * @param TextureSize The size of the texture to which the material will be flattened.
 * @returns The texture to which the material has been flattened.
 */
UTexture2D* FlattenMaterialToTexture(UMaterialInterface* MaterialInterface, EFlattenType FlattenType, const FString& TextureName, UObject* TextureOuter, EObjectFlags TextureObjectFlags, INT TextureSize)
{
	UBOOL bPrevEmulateMobileRendering = GEmulateMobileRendering;
	UBOOL bPrevUseGammaCorrectionInMobile = GUseGammaCorrectionForMobileEmulation;

	// Disable mobile rendering emulation when flattening materials so we dont bake in gamma settings
	SetMobileRenderingEmulation( FALSE, FALSE, FALSE );

	// Stream in all textures in the material  so we have the highest quality flattened texture.
	MaterialInterface->SetForceMipLevelsToBeResident( FALSE, FALSE, 10 );
	GStreamingManager->StreamAllResources( FALSE, 10 );

	// 2 copied from plane scale in thumbnail renderer
	FLOAT Scale = 2.f;

	// find a render target of the proper size
	FString RenderTargetName = FString::Printf(TEXT("FlattenRT_%d"), TextureSize);
	UTextureRenderTarget2D* RenderTarget = FindObject<UTextureRenderTarget2D>(UObject::GetTransientPackage(), *RenderTargetName);

	// if it's not created yet, create it
	if (!RenderTarget)
	{
		RenderTarget = CastChecked<UTextureRenderTarget2D>(UObject::StaticConstructObject(UTextureRenderTarget2D::StaticClass(), UObject::GetTransientPackage(), FName(*RenderTargetName), RF_Transient));
	}

	// Initialize the render target.
	const UBOOL bForceLinearGamma = (FlattenType == FLATTEN_NormalTexture);
	if ( RenderTarget->SizeX != TextureSize || RenderTarget->SizeY != TextureSize || RenderTarget->bForceLinearGamma != bForceLinearGamma )
	{
		RenderTarget->Init(TextureSize, TextureSize, PF_A8R8G8B8, bForceLinearGamma);
	}

	// find a flatten material
	FString FlattenMaterialName(TEXT("EditorFlattenMat"));
	UFlattenMaterial* FlattenMaterial = FindObject<UFlattenMaterial>(UObject::GetTransientPackage(), *FlattenMaterialName);

	// if it's not created yet, create it
	if (!FlattenMaterial)
	{
		FlattenMaterial = CastChecked<UFlattenMaterial>(UObject::StaticConstructObject(UFlattenMaterial::StaticClass(), UObject::GetTransientPackage(), FName(*FlattenMaterialName), RF_Transient));
	}

	// initialize it with the current material
	FlattenMaterial->Initialize(MaterialInterface,FlattenType);

	// Background colors.
	FColor BackgroundColor = (FlattenType == FLATTEN_NormalTexture) ? FColor(127,127,255,0) : MaterialInterface->FlattenBackgroundColor;
	FColor TranslucentBackgroundColor = (FlattenType == FLATTEN_NormalTexture) ? FColor(127,127,255,0) : MaterialInterface->FlattenBackgroundColor;

	// set up the scene to render the "thumbnail"
	FMaterialThumbnailScene	ThumbnailScene(FlattenMaterial, TPT_Plane, 0, 0, Scale, TBT_None, BackgroundColor, TranslucentBackgroundColor);

	// Setup the mobile lighting for this capture...
	ThumbnailScene.SetLightDirection(FRotator::MakeFromEuler(MaterialInterface->MobileDirectionalLightDirection));
	ThumbnailScene.SetLightBrightness(MaterialInterface->MobileDirectionalLightBrightness);
	ThumbnailScene.SetLightColor(MaterialInterface->MobileDirectionalLightColor);
	FRotator BounceDir = FRotator::MakeFromEuler(MaterialInterface->MobileBounceLightDirection);
	ThumbnailScene.EnableDirectionalBounceLight(
		MaterialInterface->bMobileEnableBounceLight, 
		MaterialInterface->MobileBounceLightBrightness, 
		&BounceDir);
	ThumbnailScene.SetBounceLightColor(MaterialInterface->MobileBounceLightColor);
	ThumbnailScene.SetSkyBrightness(MaterialInterface->MobileSkyLightBrightness);
	ThumbnailScene.SetSkyColor(MaterialInterface->MobileSkyLightColor);

	if (MaterialInterface->bGenerateSubUV == FALSE)
	{
		FSceneViewFamilyContext ViewFamily(
			RenderTarget->GameThread_GetRenderTargetResource(),
			ThumbnailScene.GetScene(),
			((SHOW_DefaultEditor & ~(SHOW_ViewMode_Mask)) | SHOW_ViewMode_Lit) & ~(SHOW_Fog | SHOW_PostProcess),
			0, //Flattened textures should be at 0 time.
			GDeltaTime,
			0, //Flattened textures should be at 0 time.
			FALSE, FALSE, FALSE, TRUE, TRUE, 1.0f, // pass defaults in
			(FlattenType != FLATTEN_NormalTexture) // write alpha
			);
		ThumbnailScene.GetView(&ViewFamily, 0, 0, TextureSize, TextureSize);

		FCanvas Canvas(RenderTarget->GameThread_GetRenderTargetResource(), NULL);
		BeginRenderingViewFamily(&Canvas,&ViewFamily);
		Canvas.Flush();
	}
	else
	{
		FLOAT TimeIncrement = 1.0f / MaterialInterface->SubUVFrameRate;

		FLOAT SubImageSize = (FLOAT)MaterialInterface->SubUVFrameSize;

		FLOAT CurrTargetX = 0.0f;
		FLOAT CurrTargetY = 0.0f;
		FLOAT CurrentTime = 0.0f;
		for (INT VertIdx = 0; VertIdx < MaterialInterface->SubUVFrameCountAlongAxes; VertIdx++)
		{
			for (INT HorzIdx = 0; HorzIdx < MaterialInterface->SubUVFrameCountAlongAxes; HorzIdx++)
			{
				FSceneViewFamilyContext ViewFamily(
					RenderTarget->GameThread_GetRenderTargetResource(),
					ThumbnailScene.GetScene(),
					((SHOW_DefaultEditor & ~(SHOW_ViewMode_Mask)) | SHOW_ViewMode_Lit) & ~(SHOW_Fog | SHOW_PostProcess),
					CurrentTime,
					GDeltaTime,
					CurrentTime,
					FALSE, FALSE, FALSE, TRUE, TRUE, 1.0f, // pass defaults in
					TRUE // write alpha
					);
				ThumbnailScene.GetView(&ViewFamily, CurrTargetX, CurrTargetY, SubImageSize, SubImageSize);

				FCanvas Canvas(RenderTarget->GameThread_GetRenderTargetResource(), NULL);
				BeginRenderingViewFamily(&Canvas,&ViewFamily);
				Canvas.Flush();

				CurrentTime += TimeIncrement;
				CurrTargetX += SubImageSize;
			}

			CurrTargetX = 0.0f;
			CurrTargetY += SubImageSize;
		}
	}


	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		FResolveCommand,
		FTextureRenderTargetResource*, RTResource ,RenderTarget->GameThread_GetRenderTargetResource(),
	{
		// copy the results of the scene rendering from the target surface to its texture
		RHICopyToResolveTarget(RTResource->GetRenderTargetSurface(), FALSE, FResolveParams());
	});

	// make sure we wait for the render to complete
	FlushRenderingCommands();

	// create the final rendered Texture2D, potentially fixing up alphas
	DWORD ConstructTextureFlags = CTF_Default | CTF_DeferCompression | CTF_AllowMips;

	// Turn off SRGB if rendering normal maps.
	if (FlattenType == FLATTEN_NormalTexture)
	{
		ConstructTextureFlags &= (~CTF_SRGB);
	}

	// if the material is masked, then we need to treat 255 one the alpha as not rendered, and anything else as rendered
	// because the alpha channel will have depth written to it, and a depth of 255 means not rendered
	if (MaterialInterface->GetMaterial()->BlendMode == BLEND_Masked && FlattenType == FLATTEN_BaseTexture)
	{
		ConstructTextureFlags |= CTF_RemapAlphaAsMasked;
	}
	// For opaque materials and normal textures we don't need an alpha channel -- force it to be opaque.
	else if (MaterialInterface->GetMaterial()->BlendMode == BLEND_Opaque || FlattenType == FLATTEN_NormalTexture)
	{
		ConstructTextureFlags |= CTF_ForceOpaque;
	}

	// Construct the flattened texture from the render target.
	UTexture2D* FlattenedTexture = RenderTarget->ConstructTexture2D(TextureOuter, TextureName, TextureObjectFlags, ConstructTextureFlags);

	// Ensure normal map settings are correct.
	if (FlattenType == FLATTEN_NormalTexture)
	{
		FlattenedTexture->SRGB = FALSE;
		FlattenedTexture->CompressionSettings = TC_Normalmap;
		FlattenedTexture->UnpackMin[0] = -1.0f;
		FlattenedTexture->UnpackMin[1] = -1.0f;
		FlattenedTexture->UnpackMin[2] = -1.0f;
		FlattenedTexture->UnpackMin[3] = +0.0f;
		FlattenedTexture->UnpackMax[0] = +1.0f;
		FlattenedTexture->UnpackMax[1] = +1.0f;
		FlattenedTexture->UnpackMax[2] = +1.0f;
		FlattenedTexture->UnpackMax[3] = +1.0f;
	}

	// cleanup references to original material interface
	FlattenMaterial->Cleanup();

	// Restore mobile emulation settings
	SetMobileRenderingEmulation( bPrevEmulateMobileRendering, bPrevUseGammaCorrectionInMobile, FALSE );

	// All the textures in the material can now be streamed out.
	MaterialInterface->SetForceMipLevelsToBeResident( FALSE, FALSE, -1 );

	// Done!
	return FlattenedTexture;
}

/**
 * Flatten the given material to a texture.
 *
 * @param MaterialInterface The material to flatten.
 * @param FlattenType How to flatten the material.
 * @param TextureName The name of the flattened texture.
 * @param TextureOuter The outer of the flattened texture.
 * @param TextureObjectFlags Object flags with which to create the flattened texture.
 * @returns The texture to which the material has been flattened.
 */
UTexture2D* FlattenMaterialToTexture(UMaterialInterface* MaterialInterface, EFlattenType FlattenType, const FString& TextureName, UObject* TextureOuter, EObjectFlags TextureObjectFlags)
{
	INT TextureSize = CalcAutoFlattenTextureSize(MaterialInterface);
	return FlattenMaterialToTexture(MaterialInterface, FlattenType, TextureName, TextureOuter, TextureObjectFlags, TextureSize);
}

/** 
 * Flatten the given material to a texture if it should be flattened
 *
 * @param MaterialInterface The material to potentially flatten
 * @param bReflattenAutoFlatten If the dominant texture was already an auto-flattened texture, specifying TRUE will reflatten
 */
void ConditionalFlattenMaterial(UMaterialInterface* MaterialInterface, EFlattenType FlattenType, UBOOL bReflattenAutoFlattened, const UBOOL bInForceFlatten)
{
	UBOOL bAutoFlatten = FALSE;
	UTexture** DestFlattenedTexture = NULL;
	const TCHAR* FlattenedSuffix = TEXT("");

	switch (FlattenType)
	{
	case FLATTEN_BaseTexture:
		bAutoFlatten = MaterialInterface->bAutoFlattenMobile;
		DestFlattenedTexture = &MaterialInterface->MobileBaseTexture;
		FlattenedSuffix = TEXT("Flattened");
		break;

	case FLATTEN_NormalTexture:
		bAutoFlatten = MaterialInterface->bAutoFlattenMobileNormalTexture;
		DestFlattenedTexture = &MaterialInterface->MobileNormalTexture;
		FlattenedSuffix = TEXT("FlattenedNormals");
		break;

	default:
		warnf( NAME_Warning, TEXT("ConditionalFlattenMaterial: Invalid FlattenType: %d"), (INT)FlattenType );
		return;
	}
	check(DestFlattenedTexture);
	UTexture* ExistingFlattenedTexture = *DestFlattenedTexture;

	// figure out what the name for the auto-flattened texture should be
	FString FlattenedName = FString::Printf(TEXT("%s_%s"), *MaterialInterface->GetName(), FlattenedSuffix);

	// if we have a dominant texture, then check if it was an previously auto-flattened texture or not
	const UBOOL bIsAutoFlattenTexture = ExistingFlattenedTexture->GetName() == FlattenedName;

	// if the texture was auto flattened and we're supposed to reflatten it, force it to be reflattened.
	const UBOOL bForceFlatten = bInForceFlatten || bIsAutoFlattenTexture && bReflattenAutoFlattened;

	//if we're not force flattening, allow an early out
	if (!bForceFlatten)
	{
		// bail out if this material should never be flattened (eg Landscape MICs)
		if (!bAutoFlatten)
		{
			return;
		}

		// should we flatten materials at all?
		UBOOL bShouldFlattenMaterials = FALSE;
		GConfig->GetBool(TEXT("MobileSupport"), TEXT("bShouldFlattenMaterials"), bShouldFlattenMaterials, GEngineIni);

		bShouldFlattenMaterials = bShouldFlattenMaterials || GAlwaysOptimizeContentForMobile;

		// if not, return
		if (!bShouldFlattenMaterials)
		{
			return;
		}
	}

	if (MaterialInterface->bGenerateSubUV == TRUE)
	{
		if ((MaterialInterface->SubUVFrameCountAlongAxes <= 0) || 
			(MaterialInterface->SubUVFrameSize <= 0.0f))
		{
			appMsgf(AMT_OK, TEXT("Flatten material failed.\nInvalid SubUV generation parameters on\n%s"), *(MaterialInterface->GetFullName()));
			return;
		}

		if (appIsPowerOfTwo(MaterialInterface->SubUVFrameCountAlongAxes * MaterialInterface->SubUVFrameSize) == FALSE)
		{
			appMsgf(AMT_OK, TEXT("Flatten material failed.\n(SubUV frame size * count along axes) must be power-of-two on\n%s"), *(MaterialInterface->GetFullName()));
			return;
		}
	}

	// figure out if we even need to do anything
	UBOOL bShouldFlattenMaterial = bForceFlatten;

	// calculate what size the auto-flatten texture will be, if it gets created
	INT TargetSize = -1;

	// check to see if this is a MaterialInstance
	UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(MaterialInterface);

	// cannot auto-flatten fallbacks, and can't flatten materials with NULL material resource
	if (!MaterialInterface->IsFallbackMaterial() && MaterialInterface->GetMaterialResource())
	{
		// at this point if it has no dominant texture, we should auto-flatten
		// but only for the base texture unless auto flatten is enabled...
		if ( ExistingFlattenedTexture == NULL && (FlattenType == FLATTEN_BaseTexture || bAutoFlatten) )
		{
			bShouldFlattenMaterial = TRUE;
		}
		else
		{
			// if it's not an auto-flattened texture, then never flatten, as the user is specifying it manually
			if (bIsAutoFlattenTexture)
			{
				// if we want to force reflatten, then just do it
				if (bReflattenAutoFlattened)
				{
					bShouldFlattenMaterial = TRUE;
				}
				// if an MI was marked as needing flattening, do it now (we queue up the need to flatten because
				// otherwise editing MIC's would be very slow)
				else if (MaterialInstance && MaterialInstance->bNeedsMaterialFlattening)
				{
					bShouldFlattenMaterial = TRUE;
				}
				// otherwise, check if the pre-auto-flattened texture is "bad"
				else
				{
					// calculate what size if should be
					TargetSize = CalcAutoFlattenTextureSize(MaterialInterface);

					// get the size of the existing auto-flattened texture
					INT ExistingSize = -1;
					if (ExistingFlattenedTexture->IsA(UTexture2D::StaticClass()))
					{
						ExistingSize = ((UTexture2D*)ExistingFlattenedTexture)->SizeX;
					}
					else if (ExistingFlattenedTexture->IsA(UTextureCube::StaticClass()))
					{
						ExistingSize = ((UTextureCube*)ExistingFlattenedTexture)->SizeX;
					}
					check(ExistingSize != -1);

					// if it's the wrong size, then flatten
					if (TargetSize != ExistingSize)
					{
						bShouldFlattenMaterial = TRUE;
					}
				}
			}
		}
	}

	if (bShouldFlattenMaterial)
	{
		// calc the size if we haven't already
		if (TargetSize == -1)
		{
			TargetSize = CalcAutoFlattenTextureSize(MaterialInterface);
		}

		if (MaterialInterface->bGenerateSubUV == TRUE)
		{
			TargetSize = MaterialInterface->SubUVFrameCountAlongAxes * MaterialInterface->SubUVFrameSize;
		}

		// Flatten the texture.
		UTexture2D* FlattenedTexture = FlattenMaterialToTexture(MaterialInterface, FlattenType, FlattenedName, MaterialInterface->GetOuter(), RF_Public, TargetSize);

		// flattened textures go into new mobile group
		FlattenedTexture->LODGroup = TEXTUREGROUP_MobileFlattened;

		// assign this texture to be the dominant texture for this material
		*DestFlattenedTexture = FlattenedTexture;

		// If the flattened texture already existed, we don't need to do a repopulate in the CB to see it; just update the UI
		if ( ExistingFlattenedTexture != NULL )
		{
			GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_UpdateAssetListUI, FlattenedTexture ) );
		}
		// If the flattened texture didn't already exist, inform the CB of its creation
		else
		{
			GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_ObjectCreated|CBR_NoSync, FlattenedTexture ) );
		}
	}
}

/** 
 * Flatten the given material to a texture if it should be flattened
 *
 * @param MaterialInterface The material to potentially flatten
 * @param bReflattenAutoFlatten If the dominant texture was already an auto-flattened texture, specifying TRUE will reflatten
 */
void ConditionalFlattenMaterial(UMaterialInterface* MaterialInterface, UBOOL bReflattenAutoFlattened, const UBOOL bInForceFlatten)
{
	// Flatten the material for each texture type needed.
	for ( INT FlattenType = FLATTEN_BaseTexture; FlattenType < FLATTEN_MAX; ++FlattenType )
	{
		ConditionalFlattenMaterial(MaterialInterface, (EFlattenType)FlattenType, bReflattenAutoFlattened, bInForceFlatten);
	}

	// no matter what happened above, reset the MI's dirty flag
	if (MaterialInterface->IsA(UMaterialInstance::StaticClass()))
	{
		UMaterialInstance* Instance = (UMaterialInstance*)MaterialInterface;
		Instance->bNeedsMaterialFlattening = FALSE;
	}
}

/**
 * Flatten a material to a texture for mobile auto-fallback use
 */
void WxUnrealEdApp::CB_UpdateMobileFlattenedTexture(UMaterialInterface* MaterialInterface)
{
	if (MaterialInterface != NULL)
	{
		const UBOOL bReflattenAutoFlattened = TRUE;
		const UBOOL bForceFlatten = FALSE;
		// re-flatten if necessary
		ConditionalFlattenMaterial(MaterialInterface, bReflattenAutoFlattened, bForceFlatten);
	}
}

static FSystemSettings* GetFlashSystemSettings()
{
	check(GIsEditor || GIsCooking);
	static FSystemSettings s_FlashSettings;
	static UBOOL s_bFirstTime = TRUE;
	if (s_bFirstTime == TRUE)
	{
		// Initialize LOD settings from the platform's engine ini.
		s_FlashSettings.LoadFromIni(TEXT("Flash"));

		s_bFirstTime = FALSE;
	}

	return &s_FlashSettings;
}

void GetFlashTextureFirstMipSizes(UTexture2D* InTexture, UINT& OutFirstMipIndex, UINT& OutFirstMipSizeX, UINT& OutFirstMipSizeY)
{
	FSystemSettings* FlashSettings = GetFlashSystemSettings();
	check(FlashSettings);

	// Determine the LOD bias from the texture settings.
	OutFirstMipIndex = FlashSettings->TextureLODSettings.CalculateLODBias(InTexture);
	// max out at 2048 textures
	while (
		((InTexture->SizeX >> OutFirstMipIndex) > 2048) || 
		((InTexture->SizeY >> OutFirstMipIndex) > 2048)
		)
	{
		OutFirstMipIndex++;
	}

	OutFirstMipSizeX = Max<UINT>(InTexture->SizeX >> OutFirstMipIndex, 1);
	OutFirstMipSizeY = Max<UINT>(InTexture->SizeY >> OutFirstMipIndex, 1);
}

/**
 * Validate the Flash cached copy to be up to date and does not need converting
 */
static UBOOL ValidateFlash( UTexture2D* Texture )
{
	if( Texture->CachedFlashMips.GetBulkDataSize() <= 0 )
	{
		return FALSE;
	}

	return TRUE;
}

//Global cook state for flash textures
INT FlashPreferredLightmapType = EPLT_Directional;
/**
 * InLightmapType - the type of light map to keep around
 */
void SetPreferredLightmapType (EPreferredLightmapType InLightmapType)
{
	FlashPreferredLightmapType = InLightmapType;
}

/**
 * @return TRUE if the given texture should have its mips compressed to Flash format 
 */
static UBOOL IsFlashCompressable( UTexture2D* Texture )
{
	// we only convert DXT textures to PVRTC (or RGBA8 textures that will be DXT soon)
	UBOOL bIsDXT = Texture->Format >= PF_DXT1 && Texture->Format <= PF_DXT5;
	UBOOL bWillBeDXT = (Texture->Format == PF_A8R8G8B8 && Texture->DeferCompression);
	if (!(bIsDXT || bWillBeDXT))
	{
		return FALSE;
	}

	// check commandline one time for options
	static UBOOL bHasCheckedInvariants = FALSE;
	static UBOOL bForceFlash;
	static UBOOL bCompressLightmaps;
	if (!bHasCheckedInvariants)
	{
		bForceFlash = ParseParam(appCmdLine(), TEXT("forceflash"));
		bCompressLightmaps = !ParseParam(appCmdLine(), TEXT("noflashlightmaps"));

		bHasCheckedInvariants = TRUE;
	}

	UBOOL bIsLightmap = Texture->IsA(ULightMapTexture2D::StaticClass()) || Texture->IsA(UShadowMapTexture2D::StaticClass());

	// Compress lightmaps unless 'nopvrtclightmaps' was passed in the commandline.
	// Always compress lightmaps when cooking.
	if( bIsLightmap && !(bCompressLightmaps || GIsCooking ) )
	{
		return FALSE;
	}

	// don't cache lightmaps we don't want
	const UBOOL bSimpleLightmap = bIsLightmap && Texture->GetName().StartsWith("Simple");
	if (bIsLightmap)
	{
		if (((FlashPreferredLightmapType == EPLT_Directional) && bSimpleLightmap) || ((FlashPreferredLightmapType == EPLT_Simple) && !bSimpleLightmap))
		{
			// make sure previously cached non-simple lightmaps are cleaned up
			Texture->CachedFlashMips.RemoveBulkData();
			return FALSE;
		}
	}

	// skip thumbnails
	if (Texture->GetName() == TEXT("ThumbnailTexture"))
	{
		return FALSE;
	}

	UINT FirstMipIndex = 0;
	UINT FirstMipSizeX = 0;
	UINT FirstMipSizeY = 0;
	GetFlashTextureFirstMipSizes(Texture, FirstMipIndex, FirstMipSizeX, FirstMipSizeY);
	UINT MaxFirstMipSize = Max<UINT>(FirstMipSizeX, FirstMipSizeY);
	if (bForceFlash || MaxFirstMipSize != Texture->CachedFlashMipsMaxResolution)
	{
		if (Texture->CachedFlashMips.GetBulkDataSize() > 0)
		{
			warnf(NAME_Log, TEXT("Recompressing texture due to Mip size change: %s"), *(Texture->GetPathName()));
			Texture->CachedFlashMips.RemoveBulkData();
		}
	}

	// if nothing denied it, return TRUE!
	return TRUE;
}

/**
 * Validate the ATITC cached copy to be up to date and does not need converting
 */
static UBOOL ValidateATITC( UTexture2D* Texture )
{
	if( Texture->CachedATITCMips.Num() != Texture->Mips.Num() )
	{
		return FALSE;
	}

	return TRUE;
}

/**
 * @return TRUE if the given texture can have its mips compressed to ATITC
 */
static UBOOL IsATITCCompressable( UTexture2D* Texture )
{
	// we only convert DXT textures to ATITC (or RGBA8 textures that will be DXT soon)
	// also, leave uncompressed textures alone
	UBOOL bIsDXT = Texture->Format >= PF_DXT1 && Texture->Format <= PF_DXT5;
	UBOOL bWillBeDXT = (Texture->Format == PF_A8R8G8B8 && Texture->DeferCompression);
	if (!(bIsDXT || bWillBeDXT) || Texture->CompressionNone)
	{
		return FALSE;
	}

	// PVRTC2 textures must be at least 16x16 (after squaring)
	// 8x8 is always PVRTC4, since PVRTC2 can't handle it
	UBOOL bWillBePVRTC2 = (Texture->Format == PF_DXT1 && !Texture->bForcePVRTC4) &&
		!(Texture->SizeX == 8 && Texture->SizeY == 8);
	if (bWillBePVRTC2 && (Texture->SizeX < 16 && Texture->SizeY < 16))
	{
		return FALSE;
	}

	// PVRTC4 textures must be at least 8x8 (after squaring)
	if (Texture->SizeX < 8 && Texture->SizeY < 8)
	{
		return FALSE;
	}

	// check commandline one time for options
	static UBOOL bHasCheckedInvariants = FALSE;
	static UBOOL bForceATITC;
	static UBOOL bCompressLightmaps;
	static UBOOL bHasConverter;
	if (!bHasCheckedInvariants)
	{
		bForceATITC = ParseParam(appCmdLine(), TEXT("forceatitc"));
		bCompressLightmaps = !ParseParam(appCmdLine(), TEXT("noatitclightmaps"));
		bHasConverter = GFileManager->FileSize(TEXT("..\\NoRedist\\Compressonator\\TheCompressonator.exe")) > 0;

		bHasCheckedInvariants = TRUE;
	}

	// Don't need to convert if already converted
	if( bForceATITC )
	{
		Texture->CachedATITCMips.Empty();
	}

	UBOOL bIsLightmap = Texture->IsA(ULightMapTexture2D::StaticClass()) || Texture->IsA(UShadowMapTexture2D::StaticClass());
	
	// Compress lightmaps unless 'nopvrtclightmaps' was passed in the commandline.
	// Always compress lightmaps when cooking.
 	if( bIsLightmap && !(bCompressLightmaps || GIsCooking ) )
 	{
 		return FALSE;
 	}

	// don't cache non-simple lightmaps
	if (bIsLightmap && !Texture->GetName().StartsWith("Simple"))
	{
		// make sure previously cached non-simple lightmaps are cleaned up
		Texture->CachedATITCMips.Empty();
		return FALSE;
	}

	// we must have the converter
	if (!bHasConverter)
	{
		return FALSE;
	}

	// skip thumbnails
	if (Texture->GetName() == TEXT("ThumbnailTexture"))
	{
		return FALSE;
	}

	// if nothing denied it, return TRUE!
	return TRUE;
}

/**
 * Validate the ETC cached copy to be up to date and does not need converting
 */
static UBOOL ValidateETC( UTexture2D* Texture )
{
	if( Texture->CachedETCMips.Num() != Texture->Mips.Num() )
	{
		return FALSE;
	}

	return TRUE;
}

/**
 * @return TRUE if the given texture can have its mips compressed to ETC
 */
static UBOOL IsETCCompressable( UTexture2D* Texture )
{
	// we only convert DXT textures to ETC (or RGBA8 textures that will be DXT soon)
	// also, leave uncompressed textures alone
	UBOOL bIsDXT = Texture->Format >= PF_DXT1 && Texture->Format <= PF_DXT5;
	UBOOL bWillBeDXT = (Texture->Format == PF_A8R8G8B8 && Texture->DeferCompression);
	if (!(bIsDXT || bWillBeDXT) || Texture->CompressionNone)
	{
		return FALSE;
	}

	// PVRTC2 textures must be at least 16x16 (after squaring)
	// 8x8 is always PVRTC4, since PVRTC2 can't handle it
	UBOOL bWillBePVRTC2 = (Texture->Format == PF_DXT1 && !Texture->bForcePVRTC4) &&
		!(Texture->SizeX == 8 && Texture->SizeY == 8);
	if (bWillBePVRTC2 && (Texture->SizeX < 16 && Texture->SizeY < 16))
	{
		return FALSE;
	}

	// PVRTC4 textures must be at least 8x8 (after squaring)
	if (Texture->SizeX < 8 && Texture->SizeY < 8)
	{
		return FALSE;
	}

	// check commandline one time for options
	static UBOOL bHasCheckedInvariants = FALSE;
	static UBOOL bForceETC;
	static UBOOL bCompressLightmaps;
	static UBOOL bHasConverter;
	if (!bHasCheckedInvariants)
	{
		bForceETC = ParseParam(appCmdLine(), TEXT("forceetc"));
		bCompressLightmaps = !ParseParam(appCmdLine(), TEXT("noetclightmaps"));
		bHasConverter = GFileManager->FileSize(PVRTEXTOOL) > 0;

		bHasCheckedInvariants = TRUE;
	}

	// Don't need to convert if already converted
	if( bForceETC )
	{
		Texture->CachedETCMips.Empty();
	}

	UBOOL bIsLightmap = Texture->IsA(ULightMapTexture2D::StaticClass()) || Texture->IsA(UShadowMapTexture2D::StaticClass());
	
	// Compress lightmaps unless 'noetclightmaps' was passed in the commandline.
	// Always compress lightmaps when cooking.
 	if( bIsLightmap && !(bCompressLightmaps || GIsCooking ) )
 	{
 		return FALSE;
 	}

	// don't cache non-simple lightmaps
	if (bIsLightmap && !Texture->GetName().StartsWith("Simple"))
	{
		// make sure previously cached non-simple lightmaps are cleaned up
		Texture->CachedETCMips.Empty();
		return FALSE;
	}

	// we must have the converter
	if (!bHasConverter)
	{
		return FALSE;
	}

	// skip thumbnails
	if (Texture->GetName() == TEXT("ThumbnailTexture"))
	{
		return FALSE;
	}

	// if nothing denied it, return TRUE!
	return TRUE;
}

/**
 * Validate the PVRTC cached copy to be up to date and does not need converting
 */
static UBOOL ValidatePVRTC( UTexture2D* Texture )
{
	if( Texture->CachedPVRTCMips.Num() != Texture->Mips.Num() )
	{
		return FALSE;
	}

	return TRUE;
}

/**
 * @return TRUE if the given texture should have its mips compressed to PVRTC
 */
static UBOOL IsPVRTCCompressable( UTexture2D* Texture )
{
	// we only convert DXT textures to PVRTC (or RGBA8 textures that will be DXT soon)
	// also, leave uncompressed textures alone
	UBOOL bIsDXT = Texture->Format >= PF_DXT1 && Texture->Format <= PF_DXT5;
	UBOOL bWillBeDXT = (Texture->Format == PF_A8R8G8B8 && Texture->DeferCompression);
	if (!(bIsDXT || bWillBeDXT) || Texture->CompressionNone)
	{
		return FALSE;
	}

	// PVRTC2 textures must be at least 16x16 (after squaring)
	// 8x8 is always PVRTC4, since PVRTC2 can't handle it
	UBOOL bWillBePVRTC2 = (Texture->Format == PF_DXT1 && !Texture->bForcePVRTC4) &&
		!(Texture->SizeX == 8 && Texture->SizeY == 8);
	if (bWillBePVRTC2 && (Texture->SizeX < 16 && Texture->SizeY < 16))
	{
		return FALSE;
	}

	// PVRTC4 textures must be at least 8x8 (after squaring)
	if (Texture->SizeX < 8 && Texture->SizeY < 8)
	{
		return FALSE;
	}

	// check commandline one time for options
	static UBOOL bHasCheckedInvariants = FALSE;
	static UBOOL bForcePVRTC;
	static UBOOL bForcePVRTC2;
	static UBOOL bForcePVRTC4;
	static UBOOL bCompressLightmaps;
	static UBOOL bHasConverter;
	if (!bHasCheckedInvariants)
	{
		bForcePVRTC = ParseParam(appCmdLine(), TEXT("forcepvrtc"));
		bForcePVRTC2 = ParseParam(appCmdLine(), TEXT("forcepvrtc2"));
		bForcePVRTC4 = ParseParam(appCmdLine(), TEXT("forcepvrtc4"));
		bCompressLightmaps = !ParseParam(appCmdLine(), TEXT("nopvrtclightmaps"));
		bHasConverter = GFileManager->FileSize(PVRTEXTOOL) > 0;

		bHasCheckedInvariants = TRUE;
	}
	
	UBOOL bIsLightmap = Texture->IsA(ULightMapTexture2D::StaticClass()) || Texture->IsA(UShadowMapTexture2D::StaticClass());

	// If we're not forcing PVRTC compression, check a couple more conditions
	UBOOL bForceThisTexture = 
		bForcePVRTC ||
		(bForcePVRTC2 && bWillBePVRTC2) ||
		(bForcePVRTC4 && !bWillBePVRTC2);

	if( bForceThisTexture )
	{
		Texture->CachedPVRTCMips.Empty();
	}

	// Compress lightmaps unless 'nopvrtclightmaps' was passed in the commandline.
	// Always compress lightmaps when cooking.
 	if( bIsLightmap && !(bCompressLightmaps || GIsCooking ) )
 	{
 		return FALSE;
 	}

	// don't cache non-simple lightmaps
	if (bIsLightmap && !Texture->GetName().StartsWith("Simple"))
	{
		// make sure previously cached non-simple lightmaps are cleaned up
		Texture->CachedPVRTCMips.Empty();
		return FALSE;
	}

	// we must have the converter
	if (!bHasConverter)
	{
		return FALSE;
	}

	// skip thumbnails
	if (Texture->GetName() == TEXT("ThumbnailTexture"))
	{
		return FALSE;
	}

	// if nothing denied it, return TRUE!
	return TRUE;
}

// TGA file header format
struct FTGAFileHeader
{
	BYTE IdFieldLength;
	BYTE ColorMapType;
	BYTE ImageTypeCode;		// 2 for uncompressed RGB format
	BYTE ColorMapOrigin[2];
	BYTE ColorMapLength[2];
	BYTE ColorMapEntrySize;
	BYTE XOrigin[2];
	BYTE YOrigin[2];
	BYTE Width[2];
	BYTE Height[2];
	BYTE BitsPerPixel;
	BYTE ImageDescriptor;
};

// PVR file header format (V2)
struct FPVRHeader
{
	UINT HeaderSize;
	UINT Height;
	UINT Width;
	UINT NumMips;
	UINT Flags;
	UINT DataLength;
	UINT BitsPerPixel;
	UINT BitmaskRed;
	UINT BitmaskGreen;
	UINT BitmaskBlue;
	UINT BitmaskAlpha;
	UINT PVRTag;
	UINT NumSurfaces;
};

union PixelType
{
	QWORD PixelTypeID;

};

// PVR file header format (V3)
struct FPVRHeaderV3
{
	UINT      Version;
	UINT      Flags;
	BYTE      PixelTypeChar[8];
	UINT      ColourSpace;
	UINT      ChannelType;
	UINT      Height;
	UINT      Width;
	UINT      Depth;
	UINT      NumSurfaces;
	UINT      NumFaces;
	UINT      NumMips;
	UINT      MetaDataSize;
};

// We need to redeclare DDSURFACEDESC2, because it doesn't quite work as a header
// for the file since it has a pointer in it, so in 64-bit mode, it is the wrong size
struct FDDSHeader
{
	DWORD dwSize;                  // size of the DDSURFACEDESC structure
	DWORD dwFlags;                 // determines what fields are valid
	DWORD dwHeight;                // height of surface to be created
	DWORD dwWidth;                 // width of input surface
	DWORD dwLinearSize;            // Formless late-allocated optimized surface size
	DWORD dwDepth;                 // the depth if this is a volume texture 
	DWORD dwMipMapCount;           // number of mip-map levels requestde
	DWORD dwAlphaBitDepth;         // depth of alpha buffer requested
	DWORD dwReserved;              // reserved
	DWORD lpSurface;               // pointer to the associated surface memory
	QWORD ddckCKDestOverlay;       // Physical color for empty cubemap faces
	QWORD ddckCKDestBlt;           // color key for destination blt use
	QWORD ddckCKSrcOverlay;        // color key for source overlay use
	QWORD ddckCKSrcBlt;            // color key for source blt use
	DDPIXELFORMAT ddpfPixelFormat; // pixel format description of the surface
	DDSCAPS2 ddsCaps;              // direct draw surface capabilities
	DWORD dwTextureStage;          // stage in multitexture cascade
};

/**
 * Convert a texture's given DXT mip data to raw FColor data
 */
static TArray<FColor> ConvertDXTToRaw(UTexture2D* Texture, INT MipIndex)
{
	INT TexSizeX = Max(Texture->SizeX >> MipIndex, 1);
	INT TexSizeY = Max(Texture->SizeY >> MipIndex, 1);

	// size the raw data properly
	TArray<FColor> RawData;
	RawData.Add(TexSizeX * TexSizeY);

	// fill out the file header
	FDDSHeader DDSHeader;
	appMemzero(&DDSHeader, sizeof(DDSHeader));
	DDSHeader.dwSize = sizeof(DDSHeader);
	DDSHeader.dwFlags = (DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_LINEARSIZE | DDSD_MIPMAPCOUNT);
	DDSHeader.dwWidth = TexSizeX;
	DDSHeader.dwHeight = TexSizeY;
	DDSHeader.dwLinearSize = Texture->Mips(MipIndex).Data.GetBulkDataSize();
	DDSHeader.dwMipMapCount = 1;
	DDSHeader.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	DDSHeader.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
	DDSHeader.ddpfPixelFormat.dwFourCC = 
		Texture->Format == PF_DXT1 ? '1TXD' :
		Texture->Format == PF_DXT3 ? '3TXD' :
		'5TXD';
	DDSHeader.ddsCaps.dwCaps = DDSCAPS_TEXTURE;

	// Get the current process ID and add to the filenames.
	// In the event that there are two simultaneous builds (which should probably not be happening) compressing textures of the same name this will prevent race conditions.
	DWORD ProcessID = 0;
#if _WINDOWS
	ProcessID = GetCurrentProcessId();
#endif
	FString InputFilePath = GSys->CachePath * FString::Printf( TEXT("%s_%d_DDSToRGB.dds"),*Texture->GetName(), ProcessID);
	FString OutputFilePath = GSys->CachePath * FString::Printf( TEXT("%s_%d_DDSToRGB.pvr"),*Texture->GetName(), ProcessID);

	FArchive* DDSFile = NULL;
	while(!DDSFile)
	{
		DDSFile = GFileManager->CreateFileWriter(*InputFilePath);	// Occasionally returns NULL due to error code ERROR_SHARING_VIOLATION
		appSleep(0.01f);											// ... no choice but to wait for the file to become free to access
	}

	// write out the magic tag for a DDS file
	DWORD MagicNumber = 0x20534444;
	DDSFile->Serialize(&MagicNumber, sizeof(MagicNumber));

	// write out header
	DDSFile->Serialize(&DDSHeader, sizeof(DDSHeader));

	// write out the dxt data
	void* Src = Texture->Mips(MipIndex).Data.Lock(LOCK_READ_ONLY);
	DDSFile->Serialize(Src, DDSHeader.dwLinearSize);
	Texture->Mips(MipIndex).Data.Unlock();

	// that's it, close the file
	delete DDSFile;

	// convert it with the old texture converter (this is because the new PVR texture converter does not properly
	// handle DXT textures)

#if TRUE || USE_LEGACY_CONVERTER
	FString Params = FString::Printf(TEXT("-yflip0 -fOGL8888 -i%s -o%s"), *InputFilePath, *OutputFilePath);
	void* Proc = appCreateProc(LEGACY_PVRTEXTOOL, *Params, TRUE, FALSE, FALSE, NULL, -1);
#else
	FString Params = FString::Printf(TEXT("-legacypvr -f r8g8b8a8 -i %s -o %s"), *InputFilePath, *OutputFilePath);
	void* Proc = appCreateProc(PVRTEXTOOL, *Params, TRUE, FALSE, FALSE, NULL, -1);
#endif

	// wait for the process to complete
	INT ReturnCode;
	while (!appGetProcReturnCode(Proc, &ReturnCode))
	{
		appSleep(0.01f);
	}

	// make sure it worked
	if (GFileManager->FileSize(*OutputFilePath) <= 0)
	{
		debugf(NAME_Warning, TEXT("Failed to decompress %s, mip %d [%dx%d]"), *Texture->GetName(), MipIndex, TexSizeX, TexSizeY);
		return RawData;
	}

	TArray<BYTE> Data;
	appLoadFileToArray(Data, *OutputFilePath);

	// process it
	FPVRHeader* Header = (FPVRHeader*)Data.GetData();
	// only copy as many colors as went into the dds file, since we may have dropped a mip
	appMemcpy(RawData.GetData(), Data.GetTypedData() + Header->HeaderSize, DDSHeader.dwWidth * DDSHeader.dwHeight * 4);

	static UBOOL bHasCheckedInvariants = FALSE;
	static UBOOL bSaveFlashWorkingTextures;
	if (!bHasCheckedInvariants)
	{
		bSaveFlashWorkingTextures = ParseParam(appCmdLine(), TEXT("saveflashtemps"));
		bHasCheckedInvariants = TRUE;
	}

	if (bSaveFlashWorkingTextures == FALSE)
	{
		// Delete the temp files
		GFileManager->Delete( *InputFilePath );
		GFileManager->Delete( *OutputFilePath ); 
	}

	return RawData;
}

/**
 * For platforms that needs square compressed data, duplicate rows/columns to make them square
 */
TArray<FColor> SquarifyRawData(const TArray<FColor>& RawData, UINT SizeX, UINT SizeY)
{
	// figure out the squarified size
	UINT SquareSize = Max(SizeX, SizeY);

	// calculate how many times to dup each row or column
	UINT MultX = SquareSize / SizeX;
	UINT MultY = SquareSize / SizeY;

	// allocate room to fill out into
	TArray<FColor> SquareRawData;
	SquareRawData.Add(SquareSize * SquareSize);

	DWORD* RectData = (DWORD*)RawData.GetData();
	DWORD* SquareData = (DWORD*)SquareRawData.GetData();

	// raw data is now at DataOffset, so we can now duplicate rows/columns into the square data
	for (UINT Y = 0; Y < SizeY; Y++)
	{
		for (UINT X = 0; X < SizeX; X++)
		{
			// get where the non-duplicated source color is
			DWORD SourceColor = *(RectData + Y * SizeX + X);

			for (UINT YDup = 0; YDup < MultY; YDup++)
			{
				for (UINT XDup = 0; XDup < MultX; XDup++)
				{
					// get where to write the duplicated color to
					DWORD* DestColor = SquareData + ((Y * MultY + YDup) * SquareSize + (X * MultX + XDup));
					*DestColor = SourceColor;
				}
			}
		}
	}

	return SquareRawData;
}

/**
 * Downsamples a square input image to the desired size. Sizes must be a power of two.
 * @param RawData - The input image. Number of colors must be equal to InSize squared.
 * @param bSRGB - Whether or not colors are in the SRGB colorspace.
 * @param InSize - Size of the input image: InSize x InSize.
 * @param DesiredSize - Size of the output image: DesiredSize x DesiredSize.
 * @param OutData - The output image.
 */
static void DownsampleImage( const TArray<FColor>& RawData, UBOOL bSRGB, UINT InSize, UINT DesiredSize, TArray<FColor>& OutData )
{
	// Incoming raw data should be the proper size.
	check( RawData.Num() == InSize * InSize );

	// This function can only downsample.
	check( DesiredSize <= InSize );

	// The input and output sizes must be powers of two.
	check( (InSize & (InSize-1)) == 0 );
	check( (DesiredSize & (DesiredSize-1)) == 0 );

	if ( DesiredSize == InSize )
	{
		OutData = RawData;
	}
	else
	{
		const UINT BlockSize = InSize / DesiredSize;
		const UINT SrcPitch = InSize;
		const UINT DestPitch = DesiredSize;
		const FLOAT Weight = 1.0f / ( (FLOAT)BlockSize * (FLOAT)BlockSize );
		OutData.Empty( DesiredSize * DesiredSize );
		OutData.Add( DesiredSize * DesiredSize );
		const FColor* RESTRICT SrcData = RawData.GetTypedData();
		FColor* RESTRICT DestData = OutData.GetTypedData();

		for ( UINT Y = 0; Y < DesiredSize; ++Y )
		{
			for ( UINT X = 0; X < DesiredSize; ++X )
			{
				FLinearColor FilteredColor( 0.0f, 0.0f, 0.0f, 0.0f );
				for ( UINT BlockY = 0; BlockY < BlockSize; ++BlockY )
				{
					const FColor* RESTRICT SrcRow = SrcData + SrcPitch * Y * BlockSize + X * BlockSize + SrcPitch * BlockY;
					for ( UINT BlockX = 0; BlockX < BlockSize; ++BlockX )
					{
						FLinearColor SrcColor = bSRGB ? FLinearColor(SrcRow[BlockX]) : SrcRow[BlockX].ReinterpretAsLinear();
						FilteredColor += Weight * SrcColor;
					}
				}
				DestData[Y * DestPitch + X] = FilteredColor.ToFColor( bSRGB );
			}
		}
	}
}

/**
 * Downsamples a square input image to the desired size. Sizes must be a power of two.
 * @param RawData - The input image. Number of colors must be equal to InSizeX * InSizeY. Upon return contains the downsampled image.
 * @param bSRGB - Whether or not colors are in the SRGB colorspace.
 * @param InSizeX - Width of the input image.
 * @param InSizeY - Height of the input image.
 * @param DesiredSize - Size of the output image: DesiredSize x DesiredSize.
 */
static void SquarifyAndDownsample( TArray<FColor>& InOutRawData, UBOOL bSRGB, UINT InSizeX, UINT InSizeY, UINT DesiredSize )
{
	check( InOutRawData.Num() == InSizeX * InSizeY );
	check( DesiredSize <= InSizeX || DesiredSize <= InSizeY );

	const UINT InSizeSquare = Max<UINT>( InSizeX, InSizeY );
	TArray<FColor> SquareData = SquarifyRawData( InOutRawData, InSizeX, InSizeY );
	DownsampleImage( SquareData, bSRGB, InSizeSquare, DesiredSize, InOutRawData );
}

/**
 * Retrieve source art appropriate for mobile use.
 * @param Texture - The texture for which to retrieve source art.
 * @param DesiredSize - The desired size of the source art.
 * @param SourceArtOverride - Override source art. Must be Texture.SizeX x Texture.SizeY.
 * @param OutSourceArt - Upon return contains the DesiredSize x DesiredSize source art.
 */
static void GetMobileSourceArt( UTexture2D& Texture, UINT DesiredSize, const TArray<FColor>* SourceArtOverride, TArray<FColor>& OutSourceArt )
{
	if (SourceArtOverride)
	{
		checkf(SourceArtOverride->Num() == Texture.SizeX * Texture.SizeY, TEXT("When supplying source art, it must match the texture size: %dx%d"), Texture.SizeX, Texture.SizeY);
		OutSourceArt = SquarifyRawData(*SourceArtOverride, Texture.SizeX, Texture.SizeY);

		// do a red blue swap, our PNG seems to red/blue swap (when viewed in Windows)
		for (INT Color = 0; Color < OutSourceArt.Num(); Color++)
		{
			const FColor& Orig = OutSourceArt(Color);
			OutSourceArt(Color) = FColor(Orig.B, Orig.G, Orig.R, Orig.A);
		}

	}
	// use the PNG data if we don't have to drop mips to get to 2048 (source art would be too big)
	else if (Texture.HasSourceArt())
	{
		// get the source art in FColor form
		TArray<BYTE> RawSrcArt;
		Texture.GetUncompressedSourceArt(RawSrcArt);
		OutSourceArt.Add(RawSrcArt.Num() / sizeof(FColor));
		appMemcpy(OutSourceArt.GetData(), RawSrcArt.GetData(), RawSrcArt.Num());

		// do a red blue swap, our PNG seems to red/blue swap (when viewed in Windows)
		for (INT Color = 0; Color < OutSourceArt.Num(); Color++)
		{
			const FColor& Orig = OutSourceArt(Color);
			OutSourceArt(Color) = FColor(Orig.B, Orig.G, Orig.R, Orig.A);
		}

		SquarifyAndDownsample( OutSourceArt, Texture.SRGB, Texture.OriginalSizeX, Texture.OriginalSizeY, DesiredSize );
	}
	else
	{
		UINT MipIndex = 0;
		UINT MipSize = Max<UINT>( Texture.SizeX, Texture.SizeY );
		UINT MipSizeX = Texture.SizeX;
		UINT MipSizeY = Texture.SizeY;
		while ( MipSize != DesiredSize )
		{
			MipIndex++;
			MipSize >>= 1;
			MipSizeX >>= 1;
			MipSizeY >>= 1;
		}

		// finally, if all else fails, use DXT data as the source
		TArray<FColor> RawData;
		RawData = ConvertDXTToRaw(&Texture, MipIndex);
		check( RawData.Num() == MipSizeX * MipSizeY );
		OutSourceArt = SquarifyRawData( RawData, MipSizeX, MipSizeY );
		check( OutSourceArt.Num() == DesiredSize * DesiredSize );
	}
}

/**
 * Utility function to open a file even if there are sharing violations.
 */
static FArchive* OpenFile(FString const& FilePath)
{
	FArchive* File = NULL;
	while(!File)
	{
		File = GFileManager->CreateFileWriter(*FilePath);	// Occasionally returns NULL due to error code ERROR_SHARING_VIOLATION
		appSleep(0.01f);									// ... no choice but to wait for the file to become free to access
	}

	return File;
}


/**
 * Utility function to write a PNG 32-bit RGBA file.
 */
static UBOOL WritePNG(FString const& FilePath, UINT Width, UINT Height, TArray<FColor>& RawData)
{
	FPNGHelper PNGHelper;
	PNGHelper.InitRaw(RawData.GetData(), Width * Height * 4, Width, Height);
	TArray<BYTE> const& CompressedData = PNGHelper.GetCompressedData();

	FArchive* PNGFile = OpenFile(FilePath);

	// write out the compressed PNG data
	PNGFile->Serialize((void*)CompressedData.GetData(), CompressedData.Num());

	// that's it, close the file
	delete PNGFile;

	return TRUE;
}

/**
 * Utility function to write a DDS 32-bit RGBA file.
 */
static UBOOL WriteDDS(FString const& FilePath, UINT Width, UINT Height, TArray<FColor>& RawData)
{
	check(Width * Height <= (UINT)RawData.Num());

	FDDSHeader DDSHeader;
	appMemzero(&DDSHeader, sizeof(DDSHeader));

	DDSHeader.dwSize = sizeof(DDSHeader);
	DDSHeader.dwFlags = (DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_LINEARSIZE | DDSD_MIPMAPCOUNT);
	DDSHeader.dwWidth = Width;
	DDSHeader.dwHeight = Height;
	DDSHeader.dwLinearSize = Width * Height * 4;
	DDSHeader.dwMipMapCount = 1;
	DDSHeader.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	DDSHeader.ddpfPixelFormat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
	DDSHeader.ddpfPixelFormat.dwFourCC = 0;
	DDSHeader.ddpfPixelFormat.dwRGBBitCount = 32;
	DDSHeader.ddpfPixelFormat.dwRBitMask = 0x000000ff;
	DDSHeader.ddpfPixelFormat.dwGBitMask = 0x0000ff00;
	DDSHeader.ddpfPixelFormat.dwBBitMask = 0x00ff0000;
	DDSHeader.ddpfPixelFormat.dwRGBAlphaBitMask = 0xff000000;

	FArchive* DDSFile = OpenFile(FilePath);

	// write out the magic tag for a DDS file
	DWORD MagicNumber = 0x20534444;
	DDSFile->Serialize(&MagicNumber, sizeof(MagicNumber));

	// write out header
	DDSFile->Serialize(&DDSHeader, sizeof(DDSHeader));

	// write out the dxt data
	DDSFile->Serialize((void*)RawData.GetData(), DDSHeader.dwLinearSize);

	// that's it, close the file
	delete DDSFile;
}
/**
 * Utility function to write a TGA 32-bit RGBA file.  NOTE: modifies the input data.
 */
static UBOOL WriteTGA(FString const& FilePath, UINT Width, UINT Height, TArray<FColor>& RawData)
{
	FTGAFileHeader TGA;
	appMemzero(&TGA, sizeof(TGA));

	TGA.ImageTypeCode = 2;
	TGA.BitsPerPixel = 32;
	TGA.Height[0] = Height & 0xFF;
	TGA.Height[1] = Height >> 8;
	TGA.Width[0] = Width & 0xFF;
	TGA.Width[1] = Width >> 8;

	FArchive* TGAFile = OpenFile(FilePath);

	// write out header
	TGAFile->Serialize(&TGA, sizeof(TGA));

	// blue red swap, for whatever reason
	for (INT Index = 0; Index < RawData.Num(); Index++)
	{
		RawData(Index) = FColor(RawData(Index).B, RawData(Index).G, RawData(Index).R, RawData(Index).A);
	}

	// write out the tga data
	for (UINT Y=0; Y < Height;Y++)
	{
		// If we aren't skipping alpha channels we can serialize each line
		TGAFile->Serialize(&RawData((Height - Y - 1) * Width), Width * 4);
	}

	// that's it, close the file
	delete TGAFile;

	return TRUE;
}

/**
 * Utility function to write a PVRT 32-bit RGBA file using the V2 format.
 */
static UBOOL WritePVRv2(FString const& FilePath, UINT Width, UINT Height, TArray<FColor>& RawData)
{
	check(Width * Height <= (UINT)RawData.Num());

	FPVRHeader PVRHeader;
	PVRHeader.HeaderSize = sizeof(PVRHeader);
	PVRHeader.Height = Height;
	PVRHeader.Width = Width;
	PVRHeader.NumMips = 0;
	PVRHeader.Flags = 0x12 | 0x8000; // OGL8888 | AlphaInTexture
	PVRHeader.DataLength = Width * Height * 4;
	PVRHeader.BitsPerPixel = 32;
	PVRHeader.BitmaskRed = 0xFF;
	PVRHeader.BitmaskGreen = 0xFF00;
	PVRHeader.BitmaskBlue = 0xFF00000;
	PVRHeader.BitmaskAlpha = 0xFF000000;
	PVRHeader.PVRTag = '!RVP';
	PVRHeader.NumSurfaces = 1;

	FArchive* PVRFile = OpenFile(FilePath);

	// write out header
	PVRFile->Serialize(&PVRHeader, PVRHeader.HeaderSize);

	// write out the RGBA data
	PVRFile->Serialize(RawData.GetData(), PVRHeader.DataLength);

	// that's it, close the file
	delete PVRFile;

	return TRUE;
}

/**
 * Utility function to write a PVRT 32-bit RGBA file using the V3 format.
 */
static UBOOL WritePVRv3(FString const& FilePath, UINT Width, UINT Height, TArray<FColor>& RawData)
{
	check(Width * Height <= (UINT)RawData.Num());

	FPVRHeaderV3 PVRHeader;
	PVRHeader.Version = 0x03525650;	// 'P''V''R'3
	PVRHeader.Flags = 0;
	PVRHeader.PixelTypeChar[0] = 'r';
	PVRHeader.PixelTypeChar[1] = 'g';
	PVRHeader.PixelTypeChar[2] = 'b';
	PVRHeader.PixelTypeChar[3] = 'a';
	PVRHeader.PixelTypeChar[4] = 8;
	PVRHeader.PixelTypeChar[5] = 8;
	PVRHeader.PixelTypeChar[6] = 8;
	PVRHeader.PixelTypeChar[7] = 8;
	PVRHeader.ColourSpace = 0; // ePVRTCSpacelRGB
	PVRHeader.ChannelType = 0; // ePVRTVarTypeUnsignedByteNorm
	PVRHeader.Height = Height;
	PVRHeader.Width = Width;
	PVRHeader.Depth = 1;
	PVRHeader.NumSurfaces = 1;
	PVRHeader.NumFaces = 1;
	PVRHeader.NumMips = 1;
	PVRHeader.MetaDataSize = 0;

	FArchive* PVRFile = OpenFile(FilePath);

	// write out header
	PVRFile->Serialize(&PVRHeader, sizeof(PVRHeader));

	// write out the RGBA data
	PVRFile->Serialize(RawData.GetData(), Width * Height * 4);

	// that's it, close the file
	delete PVRFile;

	return TRUE;
}

/**
 * Converts textures to PVRTC, using the textures source art or DXT data(if source art is not available) and caches the converted data in the texture
 *
 * @param Texture Texture to convert
 * @param bUseFastCompression If TRUE, the code will compress as fast as possible, if FALSE, it will be slow but better quality
 * @bForceCompression Forces compression of the texture
 * @param SourceArtOverride	Source art to use instead of the textures source art or to use if the texture has no source art.  This can be null 
 * @return TRUE if any work was completed (no early out was taken)
 */
UBOOL ConditionalCachePVRTCTextures(UTexture2D* Texture, UBOOL bUseFastCompression, UBOOL bForceCompression, TArray<FColor>* SourceArtOverride)
{
	// should this be compressed?
	if( !IsPVRTCCompressable( Texture ) )
	{
		warnf( NAME_Log, TEXT( "Unable to PVRTC texture: %s" ), *Texture->GetName() );
		return FALSE;
	}

	if( !bForceCompression && ValidatePVRTC( Texture ) )
	{
		//warnf( NAME_Log, TEXT( "PVRTC texture is up to date: %s" ), *Texture->GetName() );
		return FALSE;
	}

	warnf( NAME_Log, TEXT( "Compressing PVRTC texture: %s" ), *Texture->GetName() );

	// clear any existing data
	Texture->CachedPVRTCMips.Empty();

	// cache some values
	UINT TexSizeX = Texture->SizeX;
	UINT TexSizeY = Texture->SizeY;
	UINT TexSizeSquare = Max(TexSizeX, TexSizeY);
	UINT NumColors = TexSizeX * TexSizeY;

	// the PVRTC compressor can't handle 8192 or bigger textures, so we have to go down in mips and use those
	INT FirstMipIndex = 0;
	while ((TexSizeSquare >> FirstMipIndex) > PVRTEXMAXSIZE)
	{
		FirstMipIndex++;
	}

	UINT FirstMipSizeX = TexSizeX >> FirstMipIndex;
	UINT FirstMipSizeY = TexSizeY >> FirstMipIndex;
	UINT FirstMipSizeSquare = TexSizeSquare >> FirstMipIndex;
	UINT FirstMipNumColors = TexSizeSquare;

	// if we need a smaller mip, but it doesn't exist, we can't proceed
	if (Texture->Mips.Num() <= FirstMipIndex)
	{
		return FALSE;
	}

	// room for raw data that will be converted
	TArray<FColor> RawData;

	UBOOL bSourceArtIsValid = FALSE;

	if( SourceArtOverride )
	{
		RawData = *SourceArtOverride;
		bSourceArtIsValid = TRUE;
	}
	// use the source art if it exists and we support the size of (ie, don't have to drop down mip levels)
	FByteBulkData& SourceArt = Texture->SourceArt;
	UBOOL bSourceArtExists = (SourceArt.IsBulkDataLoaded() && (SourceArt.GetBulkDataSize() > 0)) || (SourceArt.GetBulkDataSizeOnDisk() > 0);
	if (!bSourceArtIsValid && 
		bSourceArtExists &&
		FirstMipIndex == 0)
	{
		// cache some values
		UINT OriginalTexSizeX = Texture->OriginalSizeX;
		UINT OriginalTexSizeY = Texture->OriginalSizeY;
		UINT OriginalTexSizeSquare = Max(OriginalTexSizeX, OriginalTexSizeY);

		// size the raw data properly for the original content
		RawData.Add(OriginalTexSizeSquare * OriginalTexSizeSquare);

		// assume the decompress will work (only will fail via exception)
		bSourceArtIsValid = TRUE;
		try
		{
			// in this case, we have source art, so we need to decompress the PNG data
			FPNGHelper PNG;
			PNG.InitCompressed(Texture->SourceArt.Lock(LOCK_READ_ONLY), Texture->SourceArt.GetBulkDataSize(), OriginalTexSizeX, OriginalTexSizeY);
			appMemcpy(RawData.GetData(), PNG.GetRawData().GetData(), OriginalTexSizeX * OriginalTexSizeY * 4);
		}
		catch (...)
		{
			bSourceArtIsValid = FALSE;

			warnf(TEXT("%s had bad Source Art!"), *Texture->GetFullName());
		}
		Texture->SourceArt.Unlock();

		if (bSourceArtIsValid)
		{
			// swap red/blue for raw data (don't need this when loading a PVR file in the previous block)
			for (UINT Color = 0; Color < OriginalTexSizeX * OriginalTexSizeY; Color++)
			{
				FColor Orig = RawData(Color);
				RawData(Color) = FColor(Orig.B, Orig.G, Orig.R, Orig.A);
			}
		}
	}

	// Get the current process ID and add to the filenames.
	// In the event that there are two simultaneous builds (which should probably not be happening) compressing textures of the same name this will prevent race conditions.
	DWORD ProcessID = 0;
#if _WINDOWS
	ProcessID = GetCurrentProcessId();
#endif

	// if after the above, we didn't have valid source art, then use the DXT mip data as the source
	if (!bSourceArtIsValid)
	{
		// explain what's happening, (and possibly tell the cooker that we are performing an operation that reduces the quality of the image)
		if (GIsUCC)
		{
			warnf(TEXT("Warning: \"%s\" is already compressed as DXT...%s"),
				*Texture->GetFullName(),
				GIsCooking ? TEXT("\n\tAttempting to compress an already compressed image will lead to a low quality PVRT texture.\n\tTo fix this, resave the package with \"Always optimize content for mobile\" enabled in the editor.") : TEXT(""));
		}

		// convert DXT mip to raw FColor data
		RawData = ConvertDXTToRaw(Texture, FirstMipIndex);
	}

	// at this point, RawData has RGBA8 data, possibly non square, which needs to be fixed
	if (FirstMipSizeX != FirstMipSizeY)
	{
		RawData = SquarifyRawData(RawData, FirstMipSizeX, FirstMipSizeY);
	}

	FString InputFilePath = GSys->CachePath * FString::Printf( TEXT("%s_%d_RGBToPVRIn.pvr"),*Texture->GetName(), ProcessID );
	WritePVRv2(InputFilePath, FirstMipSizeSquare, FirstMipSizeSquare, RawData);

	FString OutputFilePath = GSys->CachePath * FString::Printf( TEXT("%s_%d_RGBToPVROut.pvr"),*Texture->GetName(), ProcessID );

	// figure out whether or not to use 2 bits per pixel (as opposed to 4)
	// 8x8 is always PVRTC4, since PVRTC2 can't handle it
	UBOOL bIsPVRTC2 = Texture->Format == PF_DXT1 && !Texture->bForcePVRTC4 &&
		!(Texture->SizeX == 8 && Texture->SizeY == 8);

#if USE_LEGACY_CONVERTER
	//TEMPORARY - NormalMap Alpha textures are crashing the pvrtextool.  use pvrtcfast does not crash
	if (Texture->CompressionSettings == TC_NormalmapAlpha)
	{
		bUseFastCompression = TRUE;
	}

	// we now have a file on disk that can be converted
	FString Params = FString::Printf(TEXT("-m %s -fOGLPVRTC%d -yflip0 -i%s -o%s"), 
		bUseFastCompression ? TEXT("-pvrtciterations1 -pvrtcfast") : TEXT("-pvrtciterations8"),
		bIsPVRTC2 ? 2 : 4, *InputFilePath, *OutputFilePath);
#else
	FString Params = FString::Printf(TEXT("-legacypvr %s -m -f PVRTC1_%d -i %s -o %s"), 
		bUseFastCompression ? TEXT("-q pvrtcfast") : TEXT("-q pvrtcbest"),
		bIsPVRTC2 ? 2 : 4, *InputFilePath, *OutputFilePath);
#endif

	// explain what's happening, (and possibly tell the cooker that we are performing a repeating slow operation and how to fix it)
	if (GIsUCC)
	{
		warnf(TEXT("%s compressing %s to PVRTC %d for mobile devices... %s"), 
			bUseFastCompression ? TEXT("FAST") : TEXT("SLOW"), 
			*Texture->GetFullName(), 
			bIsPVRTC2 ? 2 : 4,
			GIsCooking ? TEXT("To fix this, resave the package with \"Always optimize content for mobile\" enabled in the editor.") : TEXT(""));
	}
	
	void* Proc = appCreateProc(PVRTEXTOOL, *Params, TRUE, FALSE, FALSE, NULL, -1);
	
	// wait for the process to complete
	INT ReturnCode;
	while (!appGetProcReturnCode(Proc, &ReturnCode))
	{
		appSleep(0.01f);
	}

	// did it work?
	UBOOL bConversionWasSuccessful = ReturnCode == 0;

	// if the conversion worked, open up the output file, and get the mip data from within
	if (bConversionWasSuccessful)
	{
		// read compressed data
		TArray<BYTE> PVRData;
		appLoadFileToArray(PVRData, *OutputFilePath);

		// process it
		FPVRHeader* Header = (FPVRHeader*)PVRData.GetData();
		INT FileOffset = Header->HeaderSize;

		for (INT MipLevel = 0; MipLevel < Texture->Mips.Num(); MipLevel++)
		{
			// calculate the size of the mip in bytes for PVR data
			UINT MipSizeX = Max<UINT>(TexSizeSquare >> MipLevel, 1);
			UINT MipSizeY = MipSizeX; // always square
			
			// PVRTC2 uses 8x4 blocks
			// PVRTC4 uses 4x4 blocks
			// min 2x2 blocks per mip
			UINT BlocksX = Max<UINT>(MipSizeX / (bIsPVRTC2 ? 8 : 4), 2); 
			UINT BlocksY = Max<UINT>(MipSizeY / 4, 2);
			
			// both are 8 bytes per block
			UINT MipSize = BlocksX * BlocksY * 8;

			FTexture2DMipMap* NewMipMap = new(Texture->CachedPVRTCMips) FTexture2DMipMap;

			// fill out the mip using data from the converted file
			NewMipMap->SizeX = MipSizeX;
			NewMipMap->SizeY = MipSizeY;
			NewMipMap->Data.Lock(LOCK_READ_WRITE);
			void* MipData = NewMipMap->Data.Realloc(MipSize);

			// if we skipped over any mips, don't attempt to get them out of the converted file, just
			// use black instead (and let's hope the final target won't render that large a mip)
			if (MipLevel < FirstMipIndex)
			{
				appMemzero(MipData, MipSize);
			}
			else
			{
				// copy mip data from the file data
				appMemcpy(MipData, PVRData.GetTypedData() + FileOffset, MipSize);

				// move to next mip
				FileOffset += MipSize;
			}

			NewMipMap->Data.Unlock();
		}
	}
	else
	{
		warnf(NAME_Error, TEXT("%s failed to compress to PVRTC! SizeX=%d, SizeY=%d"), *Texture->GetFullName(), Texture->SizeX, Texture->SizeY);
	}

	// Delete the temp files
	GFileManager->Delete( *InputFilePath );
	GFileManager->Delete( *OutputFilePath ); 

	return TRUE;
}

/**
 * Converts textures to ATITC, using the textures source art or DXT data(if source art is not available) and caches the converted data in the texture
 *
 * @param Texture Texture to convert
 * @param bUseFastCompression If TRUE, the code will compress as fast as possible, if FALSE, it will be slow but better quality
 * @bForceCompression Forces compression of the texture
 * @param SourceArtOverride	Source art to use instead of the textures source art or to use if the texture has no source art.  This can be null 
 * @return TRUE if any work was completed (no early out was taken)
 */
UBOOL ConditionalCacheATITCTextures(UTexture2D* Texture, UBOOL bUseFastCompression, UBOOL bForceCompression, TArray<FColor>* SourceArtOverride)
{
	// should this be compressed?
	if( !IsATITCCompressable( Texture ) )
	{
		warnf( NAME_Log, TEXT( "Unable to ATITC texture: %s" ), *Texture->GetName() );
		return FALSE;
	}

	if( !bForceCompression && ValidateATITC( Texture ) )
	{
		//warnf( NAME_Log, TEXT( "ATITC texture is up to date: %s" ), *Texture->GetName() );
		return FALSE;
	}

	warnf( NAME_Log, TEXT( "Compressing ATITC texture: %s" ), *Texture->GetName() );

	// clear any existing data
	Texture->CachedATITCMips.Empty();

	// cache some values
	UINT TexSizeX = Texture->SizeX;
	UINT TexSizeY = Texture->SizeY;
	UINT TexSizeSquare = Max(TexSizeX, TexSizeY);
	UINT NumColors = TexSizeX * TexSizeY;

	// the PVRTC compressor can't handle 8192 or bigger textures, so we have to go down in mips and use those
	INT FirstMipIndex = 0;
	while ((TexSizeSquare >> FirstMipIndex) > PVRTEXMAXSIZE)
	{
		FirstMipIndex++;
	}

	UINT FirstMipSizeX = TexSizeX >> FirstMipIndex;
	UINT FirstMipSizeY = TexSizeY >> FirstMipIndex;
	UINT FirstMipSizeSquare = TexSizeSquare >> FirstMipIndex;
	UINT FirstMipNumColors = TexSizeSquare;

	// if we need a smaller mip, but it doesn't exist, we can't proceed
	if (Texture->Mips.Num() <= FirstMipIndex)
	{
		return FALSE;
	}

	// room for raw data that will be converted
	TArray<FColor> RawData;

	UBOOL bSourceArtIsValid = FALSE;

	if( SourceArtOverride )
	{
		RawData = *SourceArtOverride;
		bSourceArtIsValid = TRUE;
	}
	// use the source art if it exists and we support the size of (ie, don't have to drop down mip levels)
	if (!bSourceArtIsValid && Texture->SourceArt.GetBulkDataSizeOnDisk() > 0 &&	FirstMipIndex == 0)
	{
		// cache some values
		UINT OriginalTexSizeX = Texture->OriginalSizeX;
		UINT OriginalTexSizeY = Texture->OriginalSizeY;
		UINT OriginalTexSizeSquare = Max(OriginalTexSizeX, OriginalTexSizeY);

		// size the raw data properly for the original content
		RawData.Add(OriginalTexSizeSquare * OriginalTexSizeSquare);

		// assume the decompress will work (only will fail via exception)
		bSourceArtIsValid = TRUE;
		try
		{
			// in this case, we have source art, so we need to decompress the PNG data
			FPNGHelper PNG;
			PNG.InitCompressed(Texture->SourceArt.Lock(LOCK_READ_ONLY), Texture->SourceArt.GetBulkDataSize(), OriginalTexSizeX, OriginalTexSizeY);
			appMemcpy(RawData.GetData(), PNG.GetRawData().GetData(), OriginalTexSizeX * OriginalTexSizeY * 4);
		}
		catch (...)
		{
			bSourceArtIsValid = FALSE;

			warnf(TEXT(">>>>>>>>>>>>> %s had bad Source Art!"), *Texture->GetFullName());
		}
		Texture->SourceArt.Unlock();

		if (bSourceArtIsValid)
		{
			// swap red/blue for raw data (don't need this when loading a PVR file in the previous block)
			for (UINT Color = 0; Color < OriginalTexSizeX * OriginalTexSizeY; Color++)
			{
				FColor Orig = RawData(Color);
				RawData(Color) = FColor(Orig.B, Orig.G, Orig.R, Orig.A);
			}
		}
	}

	// Get the current process ID and add to the filenames.
	// In the event that there are two simultaneous builds (which should probably not be happening) compressing textures of the same name this will prevent race conditions.
	DWORD ProcessID = 0;
#if _WINDOWS
	ProcessID = GetCurrentProcessId();
#endif

	// if after the above, we didn't have valid source art, then use the DXT mip data as the source
	if (!bSourceArtIsValid)
	{
		// convert DXT mip to raw FColor data
		RawData = ConvertDXTToRaw(Texture, FirstMipIndex);
	}

	// at this point, RawData has RGBA8 data, possibly non square, which needs to be fixed
	if (FirstMipSizeX != FirstMipSizeY)
	{
		// now convert the unsquare data to square
		RawData = SquarifyRawData(RawData, FirstMipSizeX, FirstMipSizeY);
	}

	FString InputFilePath = GSys->CachePath * FString::Printf( TEXT("%s_%d_RGBToATCIn.tga"),*Texture->GetName(), ProcessID );
	FString OutputFilePath = GSys->CachePath * FString::Printf( TEXT("%s_%d_RGBToATCOut.dds"),*Texture->GetName(), ProcessID );

	WriteTGA(InputFilePath, FirstMipSizeSquare, FirstMipSizeSquare, RawData);

	// figure out whether or not to use alpha
	UBOOL bIsATCA = Texture->Format == PF_DXT3 || Texture->Format == PF_DXT5;

	// we now have a file on disk that can be converted
	FString Params = FString::Printf(TEXT("-convert -overwrite -mipmaps %s %s -codec aticompressor.dll +fourCC ATC%c"), 
		*InputFilePath, *OutputFilePath, bIsATCA ? 'A' : ' ');

	// explain what's happening, (and possibly tell the cooker that we are performing a repeating slow operation and how to fix it)
	if (GIsUCC)
	{
		warnf(TEXT("%s compressing %s to ATC%c...%s"), 
			bUseFastCompression ? TEXT("FAST") : TEXT("SLOW"), 
			*Texture->GetFullName(), 
			bIsATCA ? 'A' : ' ',
			GIsCooking ? TEXT(" resave the package to fix this.") : TEXT(""));
	}
	
	void* Proc = appCreateProc(TEXT("..\\NoRedist\\Compressonator\\TheCompressonator.exe"), *Params, FALSE, TRUE, TRUE, NULL, -1);
	
	// wait for the process to complete
	INT ReturnCode;
	while (!appGetProcReturnCode(Proc, &ReturnCode))
	{
		appSleep(0.01f);
	}

	// did it work?
	UBOOL bConversionWasSuccessful = ReturnCode == 0;

	// if the conversion worked, open up the output file, and get the mip data from within
	if (bConversionWasSuccessful)
	{
		// read compressed data
		TArray<BYTE> DDSData;
		appLoadFileToArray(DDSData, *OutputFilePath);

		// get header (skipping 4 bytes of magic)
		FDDSHeader* Header = (FDDSHeader*)(DDSData.GetTypedData() + 4);
		INT FileOffset = Header->dwSize + 4;

		for (INT MipLevel = 0; MipLevel < Texture->Mips.Num(); MipLevel++)
		{
			// calculate the size of the mip in bytes for PVR data
			UINT MipSizeX = Max<UINT>(TexSizeSquare >> MipLevel, 1);
			UINT MipSizeY = MipSizeX; // always square
			
			// ATC/ATCA uses 4x4 blocks
			// min 1x1 blocks per mip
			UINT BlocksX = Max<UINT>(MipSizeX / GPixelFormats[Texture->Format].BlockSizeX, 1); 
			UINT BlocksY = Max<UINT>(MipSizeY / GPixelFormats[Texture->Format].BlockSizeY, 1);
			
			// both are 8 bytes per block
			UINT MipSize = BlocksX * BlocksY * GPixelFormats[Texture->Format].BlockBytes;

			FTexture2DMipMap* NewMipMap = new(Texture->CachedATITCMips) FTexture2DMipMap;

			// fill out the mip using data from the converted file
			NewMipMap->SizeX = MipSizeX;
			NewMipMap->SizeY = MipSizeY;
			NewMipMap->Data.Lock(LOCK_READ_WRITE);
			void* MipData = NewMipMap->Data.Realloc(MipSize);

			// if we skipped over any mips, don't attempt to get them out of the converted file, just
			// use black instead (and let's hope the final target won't render that large a mip)
			if (MipLevel < FirstMipIndex)
			{
				appMemzero(MipData, MipSize);
			}
			else
			{
				// copy mip data from the file data
				appMemcpy(MipData, DDSData.GetTypedData() + FileOffset, MipSize);

				// move to next mip
				FileOffset += MipSize;
			}

			NewMipMap->Data.Unlock();
		}
	}

	// Delete the temp files
	GFileManager->Delete( *InputFilePath );
	GFileManager->Delete( *OutputFilePath ); 

	return TRUE;
}

/**
* Converts textures to ETC, using the textures source art or DXT data(if source art is not available) and caches the converted data in the texture
*
* @param Texture Texture to convert
* @bForceCompression Forces compression of the texture
*/
UBOOL ConditionalCacheETCTextures(UTexture2D* Texture, UBOOL bForceCompression, TArray<FColor>* SourceArtOverride)
{
	// should this be compressed?
	if( !IsETCCompressable( Texture ) )
	{
		warnf( NAME_Log, TEXT( "Unable to ETC texture: %s" ), *Texture->GetName() );
		return FALSE;
	}

	if( !bForceCompression && ValidateETC( Texture ) )
	{
		//warnf( NAME_Log, TEXT( "ATITC texture is up to date: %s" ), *Texture->GetName() );
		return FALSE;
	}

	warnf( NAME_Log, TEXT( "Compressing ETC texture: %s" ), *Texture->GetName() );

	// clear any existing data
	Texture->CachedETCMips.Empty();

	// cache some values
	UINT TexSizeX = Texture->SizeX;
	UINT TexSizeY = Texture->SizeY;
	UINT TexSizeSquare = Max(TexSizeX, TexSizeY);
	UINT NumColors = TexSizeX * TexSizeY;

	// the PVRTC compressor can't handle 8192 or bigger textures, so we have to go down in mips and use those
	INT FirstMipIndex = 0;
	while ((TexSizeSquare >> FirstMipIndex) > PVRTEXMAXSIZE)
	{
		FirstMipIndex++;
	}

	UINT FirstMipSizeX = TexSizeX >> FirstMipIndex;
	UINT FirstMipSizeY = TexSizeY >> FirstMipIndex;
	UINT FirstMipSizeSquare = TexSizeSquare >> FirstMipIndex;
	UINT FirstMipNumColors = TexSizeSquare;

	// if we need a smaller mip, but it doesn't exist, we can't proceed
	if (Texture->Mips.Num() <= FirstMipIndex)
	{
		return FALSE;
	}

	// room for raw data that will be converted
	TArray<FColor> RawData;

	UBOOL bSourceArtIsValid = FALSE;

	if( SourceArtOverride )
	{
		RawData = *SourceArtOverride;
		bSourceArtIsValid = TRUE;
	}
	// use the source art if it exists and we support the size of (ie, don't have to drop down mip levels)
	if (!bSourceArtIsValid && Texture->SourceArt.GetBulkDataSizeOnDisk() > 0 && FirstMipIndex == 0)
	{
		// cache some values
		UINT OriginalTexSizeX = Texture->OriginalSizeX;
		UINT OriginalTexSizeY = Texture->OriginalSizeY;
		UINT OriginalTexSizeSquare = Max(OriginalTexSizeX, OriginalTexSizeY);

		// size the raw data properly for the original content
		RawData.Add(OriginalTexSizeSquare * OriginalTexSizeSquare);

		// assume the decompress will work (only will fail via exception)
		bSourceArtIsValid = TRUE;
		try
		{
			// in this case, we have source art, so we need to decompress the PNG data
			FPNGHelper PNG;
			PNG.InitCompressed(Texture->SourceArt.Lock(LOCK_READ_ONLY), Texture->SourceArt.GetBulkDataSize(), OriginalTexSizeX, OriginalTexSizeY);
			appMemcpy(RawData.GetData(), PNG.GetRawData().GetData(), OriginalTexSizeX * OriginalTexSizeY * 4);
		}
		catch (...)
		{
			bSourceArtIsValid = FALSE;

			warnf(TEXT(">>>>>>>>>>>>> %s had bad Source Art!"), *Texture->GetFullName());
		}
		Texture->SourceArt.Unlock();

		if (bSourceArtIsValid)
		{
			// swap red/blue for raw data (don't need this when loading a PVR file in the previous block)
			for (UINT Color = 0; Color < OriginalTexSizeX * OriginalTexSizeY; Color++)
			{
				FColor Orig = RawData(Color);
				RawData(Color) = FColor(Orig.B, Orig.G, Orig.R, Orig.A);
			}
		}
	}

	// Get the current process ID and add to the filenames.
	// In the event that there are two simultaneous builds (which should probably not be happening) compressing textures of the same name this will prevent race conditions.
	DWORD ProcessID = 0;
#if _WINDOWS
	ProcessID = GetCurrentProcessId();
#endif  

	// if after the above, we didn't have valid source art, then use the DXT mip data as the source
	if (!bSourceArtIsValid)
	{
		// convert DXT mip to raw FColor data
		RawData = ConvertDXTToRaw(Texture, FirstMipIndex);
	}

	// at this point, RawData has RGBA8 data, possibly non square, which needs to be fixed
	if (FirstMipSizeX != FirstMipSizeY)
	{
		// now convert the unsquare data to square
		RawData = SquarifyRawData(RawData, FirstMipSizeX, FirstMipSizeY);
	}

	// if its DXT1 we can make it an ETC!
	if( Texture->Format == PF_DXT1 )
	{

		FString OutputFilePath = GSys->CachePath * FString::Printf( TEXT("%s_%d_RGBToETCOut.pvr"),*Texture->GetName(), ProcessID );

		// we now have square RGBA8 data, ready for PVRTC compression

		FString InputFilePath = GSys->CachePath * FString::Printf( TEXT("%s_%d_RGBToETCIn.pvr"),*Texture->GetName(), ProcessID );
		WritePVRv2(InputFilePath, FirstMipSizeSquare, FirstMipSizeSquare, RawData);

		// we now have a file on disk that can be converted
#if USE_LEGACY_CONVERTER
		FString Params = FString::Printf(TEXT("-m -i%s -fETC -yflip0 -o%s"), *InputFilePath, *OutputFilePath);
#else
		FString Params = FString::Printf(TEXT("-legacypvr -m -f ETC1 -i %s -o %s"), *InputFilePath, *OutputFilePath);
#endif

		// explain what's happening, (and possibly tell the cooker that we are performing a repeating slow operation and how to fix it)
		if (GIsUCC) 
		{
			warnf(TEXT("compressing %s to ETC1...%s"), 
				*Texture->GetFullName(), 
				GIsCooking ? TEXT(" resave the package to fix this.") : TEXT(""));
		}

		void* Proc = appCreateProc(PVRTEXTOOL, *Params, TRUE, FALSE, FALSE, NULL, -1);

		// wait for the process to complete
		INT ReturnCode;
		while (!appGetProcReturnCode(Proc, &ReturnCode))
		{
			appSleep(0.01f);
		}

		// did it work?
		UBOOL bConversionWasSuccessful = ReturnCode == 0;

		// if the conversion worked, open up the output file, and get the mip data from within
		if (bConversionWasSuccessful)
		{
			// read compressed data
			TArray<BYTE> PVRData;
			appLoadFileToArray(PVRData, *OutputFilePath);

			// process it
			FPVRHeader* Header = (FPVRHeader*)PVRData.GetData();
			INT FileOffset = Header->HeaderSize;

			for (INT MipLevel = 0; MipLevel < Texture->Mips.Num(); MipLevel++)
			{
				// calculate the size of the mip in bytes for PVR data
				UINT MipSizeX = Max<UINT>(TexSizeSquare >> MipLevel, 1);
				UINT MipSizeY = MipSizeX; // always square

				// min 1x1 blocks per mip
				UINT BlocksX = Max<UINT>(MipSizeX / 4, 1); 
				UINT BlocksY = Max<UINT>(MipSizeY / 4, 1);

				// both are 8 bytes per block
				UINT MipSize = BlocksX * BlocksY * 8;

				FTexture2DMipMap* NewMipMap = new(Texture->CachedETCMips) FTexture2DMipMap;

				// fill out the mip using data from the converted file
				NewMipMap->SizeX = MipSizeX;
				NewMipMap->SizeY = MipSizeY;
				NewMipMap->Data.Lock(LOCK_READ_WRITE);
				void* MipData = NewMipMap->Data.Realloc(MipSize);

				// if we skipped over any mips, don't attempt to get them out of the converted file, just
				// use black instead (and let's hope the final target won't render that large a mip)
				if (MipLevel < FirstMipIndex)
				{
					appMemzero(MipData, MipSize);
				}
				else
				{
					// copy mip data from the file data
					appMemcpy(MipData, PVRData.GetTypedData() + FileOffset, MipSize);

					// move to next mip
					FileOffset += MipSize;
				}

				NewMipMap->Data.Unlock();
			}
		}

		// Delete the temp files
		GFileManager->Delete( *InputFilePath );
		GFileManager->Delete( *OutputFilePath );  
	}
	// if its not a DXT 1 or RGB change her to full A8R8G8B8 )
	else
	{		
		if (GIsUCC)
		{
			warnf(TEXT("converting %s to A8R8G8B8...%s"), 
				*Texture->GetFullName(), 
				GIsCooking ? TEXT(" resave the package to fix this.") : TEXT(""));
		}

		INT TextureSquareSize = FirstMipSizeSquare;
		UTexture2D* TransientTexture = CastChecked<UTexture2D>( UObject::StaticConstructObject(UTexture2D::StaticClass(), UObject::GetTransientPackage(), FName(TEXT("DummyTexture")) ) );
		TransientTexture->Init(TextureSquareSize, TextureSquareSize, PF_A8R8G8B8);

		// Create base mip for the texture we created.
		BYTE* MipData = (BYTE*) TransientTexture->Mips(0).Data.Lock(LOCK_READ_WRITE);
		for( INT y = 0; y< TextureSquareSize; y++ )
		{
			BYTE* DestPtr = &MipData[(TextureSquareSize - 1 - y) * TextureSquareSize * sizeof(FColor)];
			FColor* SrcPtr = const_cast<FColor*>(&RawData((TextureSquareSize - 1 - y) * TextureSquareSize));
			for( INT x=0; x < TextureSquareSize; x++ )
			{
				*DestPtr++ = SrcPtr->B;
				*DestPtr++ = SrcPtr->G;
				*DestPtr++ = SrcPtr->R;
				*DestPtr++ = SrcPtr->A;				
				SrcPtr++;
			}
		}
		TransientTexture->Mips(0).Data.Unlock();

		TransientTexture->MipGenSettings = Texture->MipGenSettings;
		TransientTexture->CompressionNone = TRUE;
		TransientTexture->Compress();

		// put into cache direction
		for (INT MipLevel = 0; MipLevel < Texture->Mips.Num(); MipLevel++)
		{
			FTexture2DMipMap* NewMipMap = new(Texture->CachedETCMips) FTexture2DMipMap;
			NewMipMap->SizeX	= TransientTexture->Mips(MipLevel).SizeX;
			NewMipMap->SizeY	= TransientTexture->Mips(MipLevel).SizeY;
			NewMipMap->Data		= TransientTexture->Mips(MipLevel).Data;
		}
	}

	return TRUE;
}

/**
 * Converts textures to Flash, using the textures source art or DXT data(if source art is not available) and caches the converted data in the texture
 *
 * @param Texture Texture to convert
 * @param bUseFastCompression If TRUE, the code will compress as fast as possible, if FALSE, it will be slow but better quality
 * @bForceCompression Forces compression of the texture
 * @param SourceArtOverride	Source art to use instead of the textures source art or to use if the texture has no source art.  This can be null 
 * @return TRUE if any work was completed (no early out was taken)
 */
UBOOL ConditionalCacheFlashTextures(UTexture2D* Texture, UBOOL bUseFastCompression, UBOOL bForceCompression, TArray<FColor>* SourceArtOverride)
{
	// should this be compressed?
	if( !IsFlashCompressable( Texture ) )
	{
		warnf( NAME_Log, TEXT( "Unable to Flash texture: %s" ), *Texture->GetName() );
		return FALSE;
	}

	if( !bForceCompression && ValidateFlash( Texture ) )
	{
		warnf( NAME_Log, TEXT( "Flash texture is up to date: %s" ), *Texture->GetName() );
		return FALSE;
	}

	// warnf( NAME_Log, TEXT( "Compressing Flash texture: %s" ), *Texture->GetName() );

	// if this is true, the PNG data has already been written to disk, and does not need to be squarified
	UBOOL bSourceIsPerfect = FALSE;
	
	UINT FirstMipIndex = 0;
	UINT FirstMipSizeX = 0;
	UINT FirstMipSizeY = 0;
	GetFlashTextureFirstMipSizes(Texture, FirstMipIndex, FirstMipSizeX, FirstMipSizeY);
	if ((FirstMipSizeX == 0) || (FirstMipSizeY == 0))
	{
		warnf(NAME_Warning, TEXT("Texture w/ 0 mip size (%4d x %4d): %s"),
			FirstMipSizeX, FirstMipSizeY, *(Texture->GetPathName()));
	}

	// Clear the max mip resolution on the texture
	Texture->CachedFlashMipsMaxResolution = 0;

	// start making an ATF format (based on Adobe's ATF spec PDF file)
	TArray<BYTE> ATFData;
	INT SignatureIndex = ATFData.Add(3);
	ATFData(SignatureIndex + 0) = 'A';
	ATFData(SignatureIndex + 1) = 'T';
	ATFData(SignatureIndex + 2) = 'F';

	// we will come back and fill this in at the end
	INT TotalATFSizeIndex = ATFData.Add(3);

	INT FormatIndex = ATFData.Add(1);

	// this is the format of the data:
	//   - high bit is cubemap (we always have texture2d's at this point)
	//   - rest is format (3 is compressed no alpha, 5 is compressed with alpha)
	BYTE CubemapAndFormat = Texture->Format == PF_DXT1 ? 3 : 5;

	ATFData(FormatIndex) = CubemapAndFormat;

	// write width/height as their log 2 of the size (1..12)
	INT WidthIndex = ATFData.Add(1);
	ATFData(WidthIndex) = appCeilLogTwo(FirstMipSizeX);
	INT HeightIndex = ATFData.Add(1);
	ATFData(HeightIndex) = appCeilLogTwo(FirstMipSizeY);
	
	// how many mips are we going to write out
	INT NumMipsIndex = ATFData.Add(1);
	ATFData(NumMipsIndex) = Texture->Mips.Num() - FirstMipIndex;
	
	// write each mip
	for (INT MipIndex = FirstMipIndex; MipIndex < Texture->Mips.Num(); MipIndex++)
	{
		// write out the size of the mip (24bits)
		INT MipSize = Texture->Mips(MipIndex).Data.GetBulkDataSize();

		INT MipSizeIndex = ATFData.Add(3);
		ATFData(MipSizeIndex + 0) = (MipSize & 0xFF0000) >> 16;
		ATFData(MipSizeIndex + 1) = (MipSize & 0x00FF00) >> 8;
		ATFData(MipSizeIndex + 2) = (MipSize & 0x0000FF) >> 0;

		// get a pointer to the mip data
		void* MipData = Texture->Mips(MipIndex).Data.Lock(LOCK_READ_ONLY);

		// now copy the mip data right into it
		INT MipDataIndex = ATFData.Add(MipSize);
		appMemcpy(&ATFData(MipDataIndex), MipData, MipSize);

		// done with the mip data
		Texture->Mips(MipIndex).Data.Unlock();

		// for compressed raw formats, we need to tell it to skip ETC and PVR

		// the standard seems to be expecting 1x1 textures for placeholders
		INT DummyPVRTCDataSize = 1 * 1 * sizeof(UINT) * 2;
		// no PVRTC data (3 bytes for the length of the data blocks)
		ATFData.AddItem(0);
		ATFData.AddItem(0);
		ATFData.AddItem(DummyPVRTCDataSize);
		ATFData.AddZeroed(DummyPVRTCDataSize);

		INT DummyATIDataSize = 1 * 1 * sizeof(UINT) * 2;
		// no ETC data (3 bytes for the length of the data blocks)
		ATFData.AddItem(0);
		ATFData.AddItem(0);
		ATFData.AddItem(DummyATIDataSize);
		ATFData.AddZeroed(DummyATIDataSize);
	}

	// now we can go back and set the overall size
	INT ATFSize = ATFData.Num() - 6;
	ATFData(TotalATFSizeIndex + 0) = (ATFSize & 0xFF0000) >> 16;
	ATFData(TotalATFSizeIndex + 1) = (ATFSize & 0x00FF00) >> 8;
	ATFData(TotalATFSizeIndex + 2) = (ATFSize & 0x0000FF) >> 0;

	// Store the max mip resolution on the texture
	Texture->CachedFlashMipsMaxResolution = Max<UINT>(FirstMipSizeX, FirstMipSizeY);

	Texture->CachedFlashMips.RemoveBulkData();

	// make space in the bulk data
	Texture->CachedFlashMips.Lock(LOCK_READ_WRITE);
	void* BulkData = Texture->CachedFlashMips.Realloc(ATFData.Num());

	// @todo: Slight optimization would be to use the CachedFlashMips locked buffer directly, but that
	// would take Reallocing bulk data, or precalculating the ATF size
	appMemcpy(BulkData, ATFData.GetData(), ATFData.Num());

	// and done!
	Texture->CachedFlashMips.Unlock();
	return TRUE;
}

/**
 * Cook any compressed textures that don't have cached cooked data
 *
 * @param Package Package to cache textures for
 * @param bIsSilent If TRUE, don't show the slow task dialog
 * @param WorldBeingSaved	The world that is currently being saved.  NULL if no world is being saved.
 */
void PreparePackageForMobile(UPackage* Package, UBOOL bIsSilent, UWorld* WorldBeingSaved )
{
	// Flatten textures if we are in cooking for a mobile platform or if we've been asked to always optimize assets for mobile
	UE3::EPlatformType Platform = ParsePlatformType( appCmdLine() );
	UBOOL bShouldFlattenMaterials = ( Platform & UE3::PLATFORM_Mobile ) || GAlwaysOptimizeContentForMobile;

	// If in other mode, check the config file for this setting
	if( !bShouldFlattenMaterials )
	{
		GConfig->GetBool(TEXT("MobileSupport"), TEXT("bShouldFlattenMaterials"), bShouldFlattenMaterials, GEngineIni);
	}

	if (bShouldFlattenMaterials)
	{
		checkf(GEditor != NULL, TEXT("Only the editor can flatten materials!"));

		// disable screen savers while flattening so that we don't get black textures
		GEngine->EnableScreenSaver( FALSE );

		// flatten any materials that need it
		for (TObjectIterator<UMaterialInterface> It; It; ++It)
		{
			if (It->IsIn(Package))
			{
				const UBOOL bReflattenAutoFlattened = FALSE;
				const UBOOL bForceFlatten = FALSE;
				// re-flatten if necessary, passing FALSE so that if the texture is already auto-flattened to NOT reflatten
				ConditionalFlattenMaterial(*It, bReflattenAutoFlattened, bForceFlatten);
			}
		}

		// reenable screensaver
		GEditor->EnableScreenSaver( TRUE );
	}

	// Read whether to cache mobile textures from the ini file
	UBOOL bShouldCachePVRTCTextures = FALSE;
	UBOOL bShouldCacheATITCTextures = FALSE;
	UBOOL bShouldCacheETCTextures = FALSE;
	UBOOL bShouldCacheFlashTextures = FALSE;
	GConfig->GetBool(TEXT("MobileSupport"), TEXT("bShouldCachePVRTCTextures"), bShouldCachePVRTCTextures, GEngineIni);
	GConfig->GetBool(TEXT("MobileSupport"), TEXT("bShouldCacheATITCTextures"), bShouldCacheATITCTextures, GEngineIni);
	GConfig->GetBool(TEXT("MobileSupport"), TEXT("bShouldCacheETCTextures"), bShouldCacheETCTextures, GEngineIni);
	GConfig->GetBool(TEXT("MobileSupport"), TEXT("bShouldCacheFlashTextures"), bShouldCacheFlashTextures, GEngineIni);

	if (bShouldCachePVRTCTextures || bShouldCacheATITCTextures || bShouldCacheETCTextures || bShouldCacheFlashTextures)
	{
		TArray<UTexture2D*> TexturesToConvert;
		for( TObjectIterator<UTexture2D> It; It; ++It )
		{
			UTexture2D* Texture = *It;

			// any textures without cached IPhone data need cooking
			if (Texture->IsIn(Package))
			{
#if WITH_SUBSTANCE_AIR == 1
				// disable PVRTC for substance textures for the moment
				// as it is not thread safe.
				if (Texture->IsA(USubstanceAirTexture2D::StaticClass()))
				{
					continue;
				}
#endif // WITH_SUBSTANCE_AIR 

				TexturesToConvert.AddItem(Texture);
			}
		}

		if (TexturesToConvert.Num())
		{
			if (!bIsSilent)
			{
				GWarn->BeginSlowTask(TEXT(""), TRUE);
			}

			// Create a pvr compressor to help with multithreaded compression.
			FAsyncPVRTCCompressor PVRCompressor;

			for (INT TextureIndex = 0; TextureIndex < TexturesToConvert.Num(); TextureIndex++)
			{
				UTexture2D* Texture = TexturesToConvert(TextureIndex);
				// make sure the texture is compressed if needed
				Texture->PreSave();

				if( bShouldCachePVRTCTextures && !ValidatePVRTC( Texture ) )
				{
					// By default do not use fast PVRTC
					UBOOL bUseFastPVR = FALSE;
					if( ( Texture->IsA( ULightMapTexture2D::StaticClass() ) || Texture->IsA( UShadowMapTexture2D::StaticClass() ) ) && WorldBeingSaved )
					{
						//Use fast PVR compression if we are on a low quality lighting build and are compressing lightmaps
						bUseFastPVR = WorldBeingSaved->GetWorldInfo()->LevelLightingQuality != Quality_Production;
					}
					// Add this texture to the list of textures needing compression
					PVRCompressor.AddTexture( Texture, bUseFastPVR );
				}

				if( bShouldCacheATITCTextures && !ValidateATITC( Texture ) )
				{
					ConditionalCacheATITCTextures(Texture, FALSE, FALSE, NULL);
				}

				if( bShouldCacheETCTextures && !ValidateETC( Texture ) )
				{
					ConditionalCacheETCTextures(Texture, FALSE, NULL);
				}

				if( bShouldCacheFlashTextures && !ValidateFlash( Texture ) )
				{
					ConditionalCacheFlashTextures(Texture, FALSE, FALSE, NULL);
				}
			}

			// Wait for all textures to be compressed.
			PVRCompressor.CompressTextures();

			if (!bIsSilent)
			{
				GWarn->EndSlowTask();
			}

			// mark package dirty
			Package->MarkPackageDirty(TRUE);
		}
	}
}

