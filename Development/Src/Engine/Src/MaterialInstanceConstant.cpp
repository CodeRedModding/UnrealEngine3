/*=============================================================================
	MaterialInstanceConstant.cpp: MaterialInstanceConstant implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineMaterialClasses.h"
#include "MaterialInstanceConstant.h"

IMPLEMENT_CLASS(UMaterialInstanceConstant);

/** Initializes a MIC's rendering thread mirror of the game thread parameter array. */
template<typename MappingType>
void InitMICParameters(const UMaterialInstanceConstant* Instance)
{
	if(!GIsUCCMake && !Instance->HasAnyFlags(RF_ClassDefaultObject))
	{
		const TArray<typename MappingType::ParameterType>& Parameters = MappingType::GetParameterArray(Instance);
		for(INT ParameterIndex = 0;ParameterIndex < Parameters.Num();ParameterIndex++)
		{
			MappingType::GameThread_UpdateParameter(
				Instance,
				Parameters(ParameterIndex)
				);
		}
	}
}

UBOOL FMaterialInstanceConstantResource::GetScalarValue(
	const FName ParameterName, 
	FLOAT* OutValue,
	const FMaterialRenderContext& Context
	) const
{
	checkSlow(IsInRenderingThread());
	const FLOAT* Value = MICScalarParameterMapping::FindResourceParameterByName(this, ParameterName);
	if(Value)
	{
		*OutValue = *Value;
		return TRUE;
	}
	else if(Parent)
	{
		return Parent->GetRenderProxy(bSelected,bHovered)->GetScalarValue(ParameterName, OutValue, Context);
	}
	else
	{
		return FALSE;
	}
}

UBOOL FMaterialInstanceConstantResource::GetVectorValue(
	const FName ParameterName, 
	FLinearColor* OutValue,
	const FMaterialRenderContext& Context
	) const
{
	checkSlow(IsInRenderingThread());
	const FLinearColor* Value = MICVectorParameterMapping::FindResourceParameterByName(this, ParameterName);
	if(Value)
	{
		*OutValue = *Value;
		return TRUE;
	}
	else if(Parent)
	{
		return Parent->GetRenderProxy(bSelected,bHovered)->GetVectorValue(ParameterName, OutValue, Context);
	}
	else
	{
		return FALSE;
	}
}

UBOOL FMaterialInstanceConstantResource::GetTextureValue(
	const FName ParameterName,
	const FTexture** OutValue,
	const FMaterialRenderContext& Context
	) const
{
	checkSlow(IsInRenderingThread());
	const UTexture* const * Value = MICTextureParameterMapping::FindResourceParameterByName(this, ParameterName);
	if(Value && *Value)
	{
		*OutValue = (*Value)->Resource;
		return TRUE;
	}
	else if(Parent)
	{
		return Parent->GetRenderProxy(bSelected,bHovered)->GetTextureValue(ParameterName,OutValue,Context);
	}
	else
	{
		return FALSE;
	}
}

UMaterialInstanceConstant::UMaterialInstanceConstant()
{
	// GIsUCCMake is not set when the class is initialized
	if(!GIsUCCMake && !HasAnyFlags(RF_ClassDefaultObject))
	{
		Resources[0] = new FMaterialInstanceConstantResource(this,FALSE,FALSE);
		if(GIsEditor)
		{
			Resources[1] = new FMaterialInstanceConstantResource(this,TRUE,FALSE);
			Resources[2] = new FMaterialInstanceConstantResource(this,FALSE,TRUE);
		}
		InitResources();
	}
}

void UMaterialInstanceConstant::InitResources()
{
	Super::InitResources();

	InitMICParameters<MICScalarParameterMapping>(this);
	InitMICParameters<MICVectorParameterMapping>(this);
	InitMICParameters<MICTextureParameterMapping>(this);
	InitMICParameters<MICFontParameterMapping>(this);
}

void UMaterialInstanceConstant::SetVectorParameterValue(FName ParameterName, const FLinearColor& Value)
{
#if !FINAL_RELEASE && !CONSOLE
	CheckSafeToModifyInGame(TEXT("MIC::SetVectorParameterValue"), *ParameterName.ToString());
#endif

	FVectorParameterValue* ParameterValue = MICVectorParameterMapping::FindParameterByName(this,ParameterName);
	if(!ParameterValue)
	{
		// If there's no element for the named parameter in array yet, add one.
		ParameterValue = new(VectorParameterValues) FVectorParameterValue;
		ParameterValue->ParameterName = ParameterName;
		ParameterValue->ExpressionGUID.Invalidate();
		// Force an update on first use
		ParameterValue->ParameterValue.B = Value.B - 1.f;
	}

	// Don't enqueue an update if it isn't needed
	if (ParameterValue->ParameterValue != Value)
	{
		ParameterValue->ParameterValue = Value;

		// Update the material instance data in the rendering thread.
		MICVectorParameterMapping::GameThread_UpdateParameter(this,*ParameterValue);
	}

#if WITH_MOBILE_RHI
	if (
		((GUsingMobileRHI == TRUE) && (GIsGame == TRUE)) 
//		|| ((GEmulateMobileRendering == TRUE) && (GIsEditor == TRUE))
		)
	{
		// We need to set the actual parameter...
		SetMobileVectorParameterValue(ParameterName, Value);
	}
#endif
}

void UMaterialInstanceConstant::SetScalarParameterValue(FName ParameterName, FLOAT Value)
{
#if !FINAL_RELEASE && !CONSOLE
	CheckSafeToModifyInGame(TEXT("MIC::SetScalarParameterValue"), *ParameterName.ToString());
#endif

	FScalarParameterValue* ParameterValue = MICScalarParameterMapping::FindParameterByName(this,ParameterName);
	if(!ParameterValue)
	{
		// If there's no element for the named parameter in array yet, add one.
		ParameterValue = new(ScalarParameterValues) FScalarParameterValue;
		ParameterValue->ParameterName = ParameterName;
		ParameterValue->ExpressionGUID.Invalidate();
		// Force an update on first use
		ParameterValue->ParameterValue = Value - 1.f;
	}

	// Don't enqueue an update if it isn't needed
	if (ParameterValue->ParameterValue != Value)
	{
		ParameterValue->ParameterValue = Value;

		// Update the material instance data in the rendering thread.
		MICScalarParameterMapping::GameThread_UpdateParameter(this,*ParameterValue);
	}

#if WITH_MOBILE_RHI
	if (
		((GUsingMobileRHI == TRUE) && (GIsGame == TRUE)) 
//		|| ((GEmulateMobileRendering == TRUE) && (GIsEditor == TRUE))
		)
	{
		// We need to set the actual parameter...
		SetMobileScalarParameterValue(ParameterName, Value);
	}
#endif
}

//
//  UMaterialInstanceConstant::SetTextureParameterValue
//
void UMaterialInstanceConstant::SetTextureParameterValue(FName ParameterName, UTexture* Value)
{
#if !FINAL_RELEASE && !CONSOLE
	CheckSafeToModifyInGame(TEXT("MIC::SetTextureParameterValue"), *ParameterName.ToString());
#endif

	FTextureParameterValue* ParameterValue = MICTextureParameterMapping::FindParameterByName(this,ParameterName);

	if(!ParameterValue)
	{
		// If there's no element for the named parameter in array yet, add one.
		ParameterValue = new(TextureParameterValues) FTextureParameterValue;
		ParameterValue->ParameterName = ParameterName;
		ParameterValue->ExpressionGUID.Invalidate();
		// Force an update on first use
		ParameterValue->ParameterValue = 
			GEngine ? ((Value == GEngine->DefaultTexture) ? NULL : GEngine->DefaultTexture) : NULL;
	}

	// Don't enqueue an update if it isn't needed
	if (ParameterValue->ParameterValue != Value)
	{
		ParameterValue->ParameterValue = Value;

		// Update the material instance data in the rendering thread.
		MICTextureParameterMapping::GameThread_UpdateParameter(this,*ParameterValue);
	}

#if WITH_MOBILE_RHI
	if (
		((GUsingMobileRHI == TRUE) && (GIsGame == TRUE)) 
//		|| ((GEmulateMobileRendering == TRUE) && (GIsEditor == TRUE))
		)
	{
		// We need to set the actual parameter...
		SetMobileTextureParameterValue(ParameterName, Value);
	}
#endif
}

/**
* Sets the value of the given font parameter.  
*
* @param	ParameterName	The name of the font parameter
* @param	OutFontValue	New font value to set for this MIC
* @param	OutFontPage		New font page value to set for this MIC
*/
void UMaterialInstanceConstant::SetFontParameterValue(FName ParameterName,class UFont* FontValue,INT FontPage)
{
#if !FINAL_RELEASE && !CONSOLE
	CheckSafeToModifyInGame(TEXT("MIC::SetFontParameterValue"), *ParameterName.ToString());
#endif

	FFontParameterValue* ParameterValue = MICFontParameterMapping::FindParameterByName(this,ParameterName);
	if( !ParameterValue )
	{
		// If there's no element for the named parameter in array yet, add one.
		ParameterValue = new(FontParameterValues) FFontParameterValue;
		ParameterValue->ParameterName = ParameterName;
		ParameterValue->ExpressionGUID.Invalidate();
		// Force an update on first use
		ParameterValue->FontValue = FontValue == GEngine->TinyFont ? NULL : GEngine->TinyFont;
		ParameterValue->FontPage = FontPage - 1;
	}

	// Don't enqueue an update if it isn't needed
	if (ParameterValue->FontValue != FontValue ||
		ParameterValue->FontPage != FontPage)
	{
		ParameterValue->FontValue = FontValue;
		ParameterValue->FontPage = FontPage;

		// Update the material instance data in the rendering thread.
		MICFontParameterMapping::GameThread_UpdateParameter(this,*ParameterValue);
	}
}

UBOOL UMaterialInstanceConstant::GetMobileScalarParameterValue(FName ParameterName,FLOAT& OutValue)
{
	if (ReentrantFlag)
	{
		return FALSE;
	}

	// Try to retrieve the parameter the normal way
	if (GetScalarParameterValue(ParameterName, OutValue) == TRUE)
	{
		return TRUE;
	}
	else if (Parent)
	{
#if MOBILE
		// avoid looking up the chain since it should all be baked in to Parent
		return Parent->UMaterialInterface::GetMobileScalarParameterValue(ParameterName, OutValue);
#else
		FMICReentranceGuard	Guard(this);
		return Parent->GetMobileScalarParameterValue(ParameterName, OutValue);
#endif
	}
	else
	{
	}

	return FALSE;
}

UBOOL UMaterialInstanceConstant::GetMobileTextureParameterValue(FName ParameterName,class UTexture*& OutValue)
{
	if (ReentrantFlag)
	{
		return FALSE;
	}

	// Try to retrieve the parameter the normal way
	if (GetTextureParameterValue(ParameterName, OutValue) == TRUE)
	{
		return TRUE;
	}
	else if (Parent)
	{
#if MOBILE
		// avoid looking up the chain since it should all be baked in to Parent
		return Parent->UMaterialInterface::GetMobileTextureParameterValue(ParameterName, OutValue);
#else
		FMICReentranceGuard	Guard(this);
		return Parent->GetMobileTextureParameterValue(ParameterName, OutValue);
#endif
	}
	else
	{
	}

	return FALSE;
}

UBOOL UMaterialInstanceConstant::GetMobileVectorParameterValue(FName ParameterName,FLinearColor& OutValue)
{
	if (ReentrantFlag)
	{
		return FALSE;
	}

	// Try to retrieve the parameter the normal way
	if (GetVectorParameterValue(ParameterName, OutValue) == TRUE)
	{
		return TRUE;
	}
	else if (Parent)
	{
#if MOBILE
		// avoid looking up the chain since it should all be baked in to Parent
		return Parent->UMaterialInterface::GetMobileVectorParameterValue(ParameterName, OutValue);
#else
		FMICReentranceGuard	Guard(this);
		return Parent->GetMobileVectorParameterValue(ParameterName, OutValue);
#endif
	}
	else
	{
	}

	return FALSE;
}

/** Removes all parameter values */
void UMaterialInstanceConstant::ClearParameterValues()
{
#if !FINAL_RELEASE && !CONSOLE
	CheckSafeToModifyInGame(TEXT("MIC::ClearParameterValues"), TEXT(""));
#endif

	VectorParameterValues.Empty();
	ScalarParameterValues.Empty();
	TextureParameterValues.Empty();
	FontParameterValues.Empty();

	if (GUsingMobileRHI)
	{
		SetupMobileProperties();
	}

	MICVectorParameterMapping::GameThread_ClearParameters(this);
	MICScalarParameterMapping::GameThread_ClearParameters(this);
	MICTextureParameterMapping::GameThread_ClearParameters(this);
	MICFontParameterMapping::GameThread_ClearParameters(this);

	// Update the material instance data in the rendering thread.
	InitResources();
}

UBOOL UMaterialInstanceConstant::GetVectorParameterValue(FName ParameterName, FLinearColor& OutValue)
{
	UBOOL bFoundAValue = FALSE;

	if(ReentrantFlag)
	{
		return FALSE;
	}

	FVectorParameterValue* ParameterValue = MICVectorParameterMapping::FindParameterByName(this,ParameterName);
	if(ParameterValue)
	{
		OutValue = ParameterValue->ParameterValue;
		return TRUE;
	}
	else if(Parent)
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetVectorParameterValue(ParameterName,OutValue);
	}
	else
	{
		return FALSE;
	}
}

UBOOL UMaterialInstanceConstant::GetScalarParameterValue(FName ParameterName, FLOAT& OutValue)
{
	UBOOL bFoundAValue = FALSE;

	if(ReentrantFlag)
	{
		return FALSE;
	}

	FScalarParameterValue* ParameterValue = MICScalarParameterMapping::FindParameterByName(this,ParameterName);
	if(ParameterValue)
	{
		OutValue = ParameterValue->ParameterValue;
		return TRUE;
	}
	else if(Parent)
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetScalarParameterValue(ParameterName,OutValue);
	}
	else
	{
		return FALSE;
	}
}

UBOOL UMaterialInstanceConstant::GetTextureParameterValue(FName ParameterName, UTexture*& OutValue)
{
	if(ReentrantFlag)
	{
		return FALSE;
	}

	FTextureParameterValue* ParameterValue = MICTextureParameterMapping::FindParameterByName(this,ParameterName);
	if(ParameterValue && ParameterValue->ParameterValue)
	{
		OutValue = ParameterValue->ParameterValue;
		return TRUE;
	}
	else if(Parent)
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetTextureParameterValue(ParameterName,OutValue);
	}
	else
	{
		return FALSE;
	}
}

UBOOL UMaterialInstanceConstant::GetFontParameterValue(FName ParameterName,class UFont*& OutFontValue, INT& OutFontPage)
{
	if( ReentrantFlag )
	{
		return FALSE;
	}

	FFontParameterValue* ParameterValue = MICFontParameterMapping::FindParameterByName(this,ParameterName);
	if(ParameterValue && ParameterValue->FontValue)
	{
		OutFontValue = ParameterValue->FontValue;
		OutFontPage = ParameterValue->FontPage;
		return TRUE;
	}
		//@todo sz
		// 		// try the parent if values were invalid
		// 		else if( Parent )
		// 		{
		// 			FMICReentranceGuard	Guard(this);
		// 			Result = Parent->GetFontParameterValue(ParameterName,OutFontValue,OutFontPage);
		// 		}
	else
	{
		return FALSE;
	}
}


void UMaterialInstanceConstant::SetParent(UMaterialInterface* NewParent)
{
#if !FINAL_RELEASE && !CONSOLE
	CheckSafeToModifyInGame(TEXT("MIC::SetParent"), TEXT(""));
#endif

	if (Parent != NewParent)
	{
		Super::SetParent(NewParent);

		if (GUsingMobileRHI == TRUE)
		{
			SetupMobileProperties();
		}

		InitResources();
	}
}

void UMaterialInstanceConstant::PostLoad()
{
	// Ensure that the instance's parent is PostLoaded before the instance.
	if(Parent)
	{
		Parent->ConditionalPostLoad();
	}

	// Add references to the expression object if we do not have one already, and fix up any names that were changed.
	UpdateParameterNames();

	// We have to make sure the resources are created for all used textures.
	for( INT ValueIndex=0; ValueIndex<TextureParameterValues.Num(); ValueIndex++ )
	{
		// Make sure the texture is postloaded so the resource isn't null.
		UTexture* Texture = TextureParameterValues(ValueIndex).ParameterValue;
		if( Texture )
		{
			Texture->ConditionalPostLoad();
		}
	}
	// do the same for font textures
	for( INT ValueIndex=0; ValueIndex < FontParameterValues.Num(); ValueIndex++ )
	{
		// Make sure the font is postloaded so the resource isn't null.
		UFont* Font = FontParameterValues(ValueIndex).FontValue;
		if( Font )
		{
			Font->ConditionalPostLoad();
		}
	}

	// Have to do this BEFORE Super::PostLoad as the PostLoad will compile the material
#if WITH_MOBILE_RHI
	// If we are a non-cooked mobile platform, setup the properties from the parameters
	if (GIsEditor || 
		(GUsingMobileRHI && !GUseSeekFreeLoading) ||
		((GCookingTarget & UE3::PLATFORM_Mobile) != 0))
	{
		SetupMobileProperties();
	}
#endif

	Super::PostLoad();

	InitResources();
}

void UMaterialInstanceConstant::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	InitResources();

	// mark that the instance is dirty, and that it may need to be flattened
	bNeedsMaterialFlattening = TRUE;

	UpdateStaticPermutation();
}

/**
 * Refreshes parameter names using the stored reference to the expression object for the parameter.
 */
void UMaterialInstanceConstant::UpdateParameterNames()
{
	if(IsTemplate(RF_ClassDefaultObject)==FALSE)
	{
		// Get a pointer to the parent material.
		UMaterial* ParentMaterial = NULL;
		UMaterialInstance* ParentInst = this;
		while(ParentInst && ParentInst->Parent)
		{
			if(ParentInst->Parent->IsA(UMaterial::StaticClass()))
			{
				ParentMaterial = Cast<UMaterial>(ParentInst->Parent);
				break;
			}
			else
			{
				ParentInst = Cast<UMaterialInstance>(ParentInst->Parent);
			}
		}

		if(ParentMaterial)
		{
			UBOOL bDirty = FALSE;

			// Scalar parameters
			bDirty = UpdateParameterSet<FScalarParameterValue, UMaterialExpressionScalarParameter>(ScalarParameterValues, ParentMaterial) || bDirty;

			// Vector parameters	
			bDirty = UpdateParameterSet<FVectorParameterValue, UMaterialExpressionVectorParameter>(VectorParameterValues, ParentMaterial) || bDirty;

			// Texture parameters
			bDirty = UpdateParameterSet<FTextureParameterValue, UMaterialExpressionTextureSampleParameter>(TextureParameterValues, ParentMaterial) || bDirty;

			// Font parameters
			bDirty = UpdateParameterSet<FFontParameterValue, UMaterialExpressionFontSampleParameter>(FontParameterValues, ParentMaterial) || bDirty;

			for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
			{
				// Static switch parameters
				bDirty = UpdateParameterSet<FStaticSwitchParameter, UMaterialExpressionStaticBoolParameter>(StaticParameters[QualityIndex]->StaticSwitchParameters, ParentMaterial) || bDirty;

				// Static component mask parameters
				bDirty = UpdateParameterSet<FStaticComponentMaskParameter, UMaterialExpressionStaticComponentMaskParameter>(StaticParameters[QualityIndex]->StaticComponentMaskParameters, ParentMaterial) || bDirty;

				// Static component mask parameters
				bDirty = UpdateParameterSet<FNormalParameter, UMaterialExpressionTextureSampleParameterNormal>(StaticParameters[QualityIndex]->NormalParameters, ParentMaterial) || bDirty;
			}

			// Atleast 1 parameter changed, initialize parameters
			if (bDirty)
			{
				InitResources();
			}
		}
	}
}

/**
 *	Cleanup the TextureParameter lists in the instance
 *
 *	@param	InRefdTextureParamsMap		Map of actual TextureParams used by the parent.
 *
 *	NOTE: This is intended to be called only when cooking for stripped platforms!
 */
void UMaterialInstanceConstant::CleanupTextureParameterReferences(const TMap<FName,UTexture*>& InRefdTextureParamsMap)
{
	check(GIsCooking);
	if ((GCookingTarget & UE3::PLATFORM_Stripped) != 0)
	{
		// Remove any texture parameter values that were not found
		for (INT CheckIdx = TextureParameterValues.Num() - 1; CheckIdx >= 0; CheckIdx--)
		{
			UTexture*const* ParentTexture = InRefdTextureParamsMap.Find(TextureParameterValues(CheckIdx).ParameterName);
			if (ParentTexture == NULL)
			{
				// Parameter wasn't found... remove it
				//@todo. Remove the entire entry?
				//TextureParameterValues.Remove(CheckIdx);
				TextureParameterValues(CheckIdx).ParameterValue = NULL;
			}
			else
			{
				// There was a default texture found. 
			}
		}
	}
}

/**
 *	Setup the mobile properties for this instance
 */
void UMaterialInstanceConstant::SetupMobileProperties()
{
#if !FLASH
	// We want to copy the various 'static' settings from the absolute parent material
	// While walking the chain for all parameter based settings.
	UMaterial* AbsoluteParent = GetMaterial();
	check(AbsoluteParent);
	GetMobileTextureParameterValue(NAME_MobileBaseTexture, MobileBaseTexture);
	MobileBaseTextureTexCoordsSource = AbsoluteParent->MobileBaseTextureTexCoordsSource;
	GetMobileTextureParameterValue(NAME_MobileNormalTexture, MobileNormalTexture);
	MobileAmbientOcclusionSource = AbsoluteParent->MobileAmbientOcclusionSource;
	bMobileAllowFog = AbsoluteParent->bMobileAllowFog;

	bUseMobileSpecular = AbsoluteParent->bUseMobileSpecular;
	bUseMobilePixelSpecular = AbsoluteParent->bUseMobilePixelSpecular;
	GetMobileVectorParameterValue(NAME_MobileSpecularColor, MobileSpecularColor);
	GetMobileScalarParameterValue(NAME_MobileSpecularPower, MobileSpecularPower);
	MobileSpecularMask = AbsoluteParent->MobileSpecularMask;

	GetMobileTextureParameterValue(NAME_MobileEmissiveTexture, MobileEmissiveTexture);
	MobileEmissiveColorSource = AbsoluteParent->MobileEmissiveColorSource;
	GetMobileVectorParameterValue(NAME_MobileEmissiveColor, MobileEmissiveColor);
	MobileEmissiveMaskSource = AbsoluteParent->MobileEmissiveMaskSource;

	GetMobileTextureParameterValue(NAME_MobileEnvironmentTexture, MobileEnvironmentTexture);
	MobileEnvironmentMaskSource = AbsoluteParent->MobileEnvironmentMaskSource;
	GetMobileScalarParameterValue(NAME_MobileEnvironmentAmount, MobileEnvironmentAmount);
	MobileEnvironmentBlendMode = AbsoluteParent->MobileEnvironmentBlendMode;
	GetMobileVectorParameterValue(NAME_MobileEnvironmentColor, MobileEnvironmentColor);
	GetMobileScalarParameterValue(NAME_MobileEnvironmentFresnelAmount, MobileEnvironmentFresnelAmount);
	GetMobileScalarParameterValue(NAME_MobileEnvironmentFresnelExponent, MobileEnvironmentFresnelExponent);

	GetMobileScalarParameterValue(NAME_MobileRimLightingStrength, MobileRimLightingStrength);
	GetMobileScalarParameterValue(NAME_MobileRimLightingExponent, MobileRimLightingExponent);
	MobileRimLightingMaskSource = AbsoluteParent->MobileRimLightingMaskSource;
	GetMobileVectorParameterValue(NAME_MobileRimLightingColor, MobileRimLightingColor);

	bUseMobileBumpOffset = AbsoluteParent->bUseMobileBumpOffset;
	GetMobileScalarParameterValue(NAME_MobileBumpOffsetReferencePlane, MobileBumpOffsetReferencePlane);
	GetMobileScalarParameterValue(NAME_MobileBumpOffsetHeightRatio, MobileBumpOffsetHeightRatio);

	GetMobileTextureParameterValue(NAME_MobileMaskTexture, MobileMaskTexture);
	MobileMaskTextureTexCoordsSource = AbsoluteParent->MobileMaskTextureTexCoordsSource;
	MobileAlphaValueSource = AbsoluteParent->MobileAlphaValueSource;

	GetMobileTextureParameterValue(NAME_MobileDetailTexture, MobileDetailTexture);
	GetMobileTextureParameterValue(NAME_MobileDetailTexture2, MobileDetailTexture2);
	GetMobileTextureParameterValue(NAME_MobileDetailTexture3, MobileDetailTexture3);
	MobileDetailTextureTexCoordsSource = AbsoluteParent->MobileDetailTextureTexCoordsSource;
	MobileTextureBlendFactorSource = AbsoluteParent->MobileTextureBlendFactorSource;
	bLockColorBlending = AbsoluteParent->bLockColorBlending;

	bUseMobileUniformColorMultiply = AbsoluteParent->bUseMobileUniformColorMultiply;
	GetMobileVectorParameterValue(NAME_MobileDefaultUniformColor, MobileDefaultUniformColor);
	bUseMobileVertexColorMultiply = AbsoluteParent->bUseMobileVertexColorMultiply;
	MobileColorMultiplySource = AbsoluteParent->MobileColorMultiplySource;
	bUseMobileDetailNormal = AbsoluteParent->bUseMobileDetailNormal;
	bUseMobileLandscapeMonochromeLayerBlending = AbsoluteParent->bUseMobileLandscapeMonochromeLayerBlending;

	bBaseTextureTransformed = AbsoluteParent->bBaseTextureTransformed;
	bEmissiveTextureTransformed = AbsoluteParent->bEmissiveTextureTransformed;
	bNormalTextureTransformed = AbsoluteParent->bNormalTextureTransformed;
	bMaskTextureTransformed = AbsoluteParent->bMaskTextureTransformed;
	bDetailTextureTransformed = AbsoluteParent->bDetailTextureTransformed;
	GetMobileScalarParameterValue(NAME_MobileTransformCenterX, MobileTransformCenterX);
	GetMobileScalarParameterValue(NAME_MobileTransformCenterY, MobileTransformCenterY);
	GetMobileScalarParameterValue(NAME_MobilePannerSpeedX, MobilePannerSpeedX);
	GetMobileScalarParameterValue(NAME_MobilePannerSpeedY, MobilePannerSpeedY);
	GetMobileScalarParameterValue(NAME_MobileRotateSpeed, MobileRotateSpeed);
	GetMobileScalarParameterValue(NAME_MobileFixedScaleX, MobileFixedScaleX);
	GetMobileScalarParameterValue(NAME_MobileFixedScaleY, MobileFixedScaleY);
	GetMobileScalarParameterValue(NAME_MobileSineScaleX, MobileSineScaleX);
	GetMobileScalarParameterValue(NAME_MobileSineScaleY, MobileSineScaleY);
	GetMobileScalarParameterValue(NAME_MobileSineScaleFrequencyMultipler, MobileSineScaleFrequencyMultipler);
	GetMobileScalarParameterValue(NAME_MobileFixedOffsetX, MobileFixedOffsetX);
	GetMobileScalarParameterValue(NAME_MobileFixedOffsetY, MobileFixedOffsetY);

	bUseMobileWaveVertexMovement = AbsoluteParent->bUseMobileWaveVertexMovement;
	GetMobileScalarParameterValue(NAME_MobileTangentVertexFrequencyMultiplier, MobileTangentVertexFrequencyMultiplier);
	GetMobileScalarParameterValue(NAME_MobileVerticalFrequencyMultiplier, MobileVerticalFrequencyMultiplier);
	GetMobileScalarParameterValue(NAME_MobileMaxVertexMovementAmplitude, MobileMaxVertexMovementAmplitude);
	GetMobileScalarParameterValue(NAME_MobileSwayFrequencyMultiplier, MobileSwayFrequencyMultiplier);
	GetMobileScalarParameterValue(NAME_MobileSwayMaxAngle, MobileSwayMaxAngle);

	GetMobileScalarParameterValue(NAME_MobileOpacityMultiplier, MobileOpacityMultiplier);
#else
	// Assuming this is only done at run-time, just copy the parent values
	// It should only be done this way on platforms that cook their content as well...
	//@todo. Post-gdc, fix this issue up cleanly!
	MobileBaseTexture = Parent->MobileBaseTexture;
	MobileBaseTextureTexCoordsSource = Parent->MobileBaseTextureTexCoordsSource;
	MobileNormalTexture = Parent->MobileNormalTexture;
	MobileAmbientOcclusionSource = Parent->MobileAmbientOcclusionSource;
	bMobileAllowFog = Parent->bMobileAllowFog;

	bUseMobileSpecular = Parent->bUseMobileSpecular;
	bUseMobilePixelSpecular = Parent->bUseMobilePixelSpecular;
	MobileSpecularColor = Parent->MobileSpecularColor;
	MobileSpecularPower = Parent->MobileSpecularPower;
	MobileSpecularMask = Parent->MobileSpecularMask;

	MobileEmissiveTexture = Parent->MobileEmissiveTexture;
	MobileEmissiveColorSource = Parent->MobileEmissiveColorSource;
	MobileEmissiveColor = Parent->MobileEmissiveColor;
	MobileEmissiveMaskSource = Parent->MobileEmissiveMaskSource;

	MobileEnvironmentTexture = Parent->MobileEnvironmentTexture;
	MobileEnvironmentMaskSource = Parent->MobileEnvironmentMaskSource;
	MobileEnvironmentAmount = Parent->MobileEnvironmentAmount;
	MobileEnvironmentBlendMode = Parent->MobileEnvironmentBlendMode;
	MobileEnvironmentColor = Parent->MobileEnvironmentColor;
	MobileEnvironmentFresnelAmount = Parent->MobileEnvironmentFresnelAmount;
	MobileEnvironmentFresnelExponent = Parent->MobileEnvironmentFresnelExponent;

	MobileRimLightingStrength = Parent->MobileRimLightingStrength;
	MobileRimLightingExponent = Parent->MobileRimLightingExponent;
	MobileRimLightingMaskSource = Parent->MobileRimLightingMaskSource;
	MobileRimLightingColor = Parent->MobileRimLightingColor;

	bUseMobileBumpOffset = Parent->bUseMobileBumpOffset;
	MobileBumpOffsetReferencePlane = Parent->MobileBumpOffsetReferencePlane;
	MobileBumpOffsetHeightRatio = Parent->MobileBumpOffsetHeightRatio;

	MobileMaskTexture = Parent->MobileMaskTexture;
	MobileMaskTextureTexCoordsSource = Parent->MobileMaskTextureTexCoordsSource;
	MobileAlphaValueSource = Parent->MobileAlphaValueSource;

	MobileDetailTexture = Parent->MobileDetailTexture;
	MobileDetailTexture2 = Parent->MobileDetailTexture2;
	MobileDetailTexture3 = Parent->MobileDetailTexture3;
	MobileDetailTextureTexCoordsSource = Parent->MobileDetailTextureTexCoordsSource;
	MobileTextureBlendFactorSource = Parent->MobileTextureBlendFactorSource;
	bLockColorBlending = Parent->bLockColorBlending;

	bUseMobileUniformColorMultiply = Parent->bUseMobileUniformColorMultiply;
	MobileDefaultUniformColor = Parent->MobileDefaultUniformColor;
	bUseMobileVertexColorMultiply = Parent->bUseMobileVertexColorMultiply;
	MobileColorMultiplySource = Parent->MobileColorMultiplySource;
	bUseMobileDetailNormal = Parent->bUseMobileDetailNormal;

	bBaseTextureTransformed = Parent->bBaseTextureTransformed;
	bEmissiveTextureTransformed = Parent->bEmissiveTextureTransformed;
	bNormalTextureTransformed = Parent->bNormalTextureTransformed;
	bMaskTextureTransformed = Parent->bMaskTextureTransformed;
	bDetailTextureTransformed = Parent->bDetailTextureTransformed;
	MobileTransformCenterX = Parent->MobileTransformCenterX;
	MobileTransformCenterY = Parent->MobileTransformCenterY;
	MobilePannerSpeedX = Parent->MobilePannerSpeedX;
	MobilePannerSpeedY = Parent->MobilePannerSpeedY;
	MobileRotateSpeed = Parent->MobileRotateSpeed;
	MobileFixedScaleX = Parent->MobileFixedScaleX;
	MobileFixedScaleY = Parent->MobileFixedScaleY;
	MobileSineScaleX = Parent->MobileSineScaleX;
	MobileSineScaleY = Parent->MobileSineScaleY;
	MobileSineScaleFrequencyMultipler = Parent->MobileSineScaleFrequencyMultipler;
	MobileFixedOffsetX = Parent->MobileFixedOffsetX;
	MobileFixedOffsetY = Parent->MobileFixedOffsetY;

	bUseMobileWaveVertexMovement = Parent->bUseMobileWaveVertexMovement;
	MobileTangentVertexFrequencyMultiplier = Parent->MobileTangentVertexFrequencyMultiplier;
	MobileVerticalFrequencyMultiplier = Parent->MobileVerticalFrequencyMultiplier;
	MobileMaxVertexMovementAmplitude = Parent->MobileMaxVertexMovementAmplitude;
	MobileSwayFrequencyMultiplier = Parent->MobileSwayFrequencyMultiplier;
	MobileSwayMaxAngle = Parent->MobileSwayMaxAngle;

	MobileOpacityMultiplier = Parent->MobileOpacityMultiplier;
#endif
}

/**
 *	Set the mobile scalar parameter value to the given value.
 *
 *	@param	InParameterName		Name of the parameter to set
 *	@param	InValue				The scalar value to set it to
 */
void UMaterialInstanceConstant::SetMobileScalarParameterValue(FName& ParameterName, FLOAT InValue)
{
	if (ParameterName == NAME_MobileSpecularPower)
	{
		MobileSpecularPower = InValue;
	}
	else if (ParameterName == NAME_MobileEnvironmentAmount)
	{
		MobileEnvironmentAmount = InValue;
	}
	else if (ParameterName == NAME_MobileEnvironmentFresnelAmount)
	{
		MobileEnvironmentFresnelAmount = InValue;
	}
	else if (ParameterName == NAME_MobileEnvironmentFresnelExponent)
	{
		MobileEnvironmentFresnelExponent = InValue;
	}
	else if (ParameterName == NAME_MobileRimLightingStrength)
	{
		MobileRimLightingStrength = InValue;
	}
	else if (ParameterName == NAME_MobileRimLightingExponent)
	{
		MobileRimLightingExponent = InValue;
	}
	else if (ParameterName == NAME_MobileBumpOffsetReferencePlane)
	{
		MobileBumpOffsetReferencePlane = InValue;
	}
	else if (ParameterName == NAME_MobileBumpOffsetHeightRatio)
	{
		MobileBumpOffsetHeightRatio = InValue;
	}
	else if (ParameterName == NAME_MobileTransformCenterX)
	{
		MobileTransformCenterX = InValue;
	}
	else if (ParameterName == NAME_MobileTransformCenterY)
	{
		MobileTransformCenterY = InValue;
	}
	else if (ParameterName == NAME_MobilePannerSpeedX)
	{
		MobilePannerSpeedX = InValue;
	}
	else if (ParameterName == NAME_MobilePannerSpeedY)
	{
		MobilePannerSpeedY = InValue;
	}
	else if (ParameterName == NAME_MobileRotateSpeed)
	{
		MobileRotateSpeed = InValue;
	}
	else if (ParameterName == NAME_MobileFixedScaleX)
	{
		MobileFixedScaleX = InValue;
	}
	else if (ParameterName == NAME_MobileFixedScaleY)
	{
		MobileFixedScaleY = InValue;
	}
	else if (ParameterName == NAME_MobileSineScaleX)
	{
		MobileSineScaleX = InValue;
	}
	else if (ParameterName == NAME_MobileSineScaleY)
	{
		MobileSineScaleY = InValue;
	}
	else if (ParameterName == NAME_MobileSineScaleFrequencyMultipler)
	{
		MobileSineScaleFrequencyMultipler = InValue;
	}
	else if (ParameterName == NAME_MobileFixedOffsetX)
	{
		MobileFixedOffsetX = InValue;
	}
	else if (ParameterName == NAME_MobileFixedOffsetY)
	{
		MobileFixedOffsetY = InValue;
	}
	else if (ParameterName == NAME_MobileTangentVertexFrequencyMultiplier)
	{
		MobileTangentVertexFrequencyMultiplier = InValue;
	}
	else if (ParameterName == NAME_MobileVerticalFrequencyMultiplier)
	{
		MobileVerticalFrequencyMultiplier = InValue;
	}
	else if (ParameterName == NAME_MobileMaxVertexMovementAmplitude)
	{
		MobileMaxVertexMovementAmplitude = InValue;
	}
	else if (ParameterName == NAME_MobileSwayFrequencyMultiplier)
	{
		MobileSwayFrequencyMultiplier = InValue;
	}
	else if (ParameterName == NAME_MobileSwayMaxAngle)
	{
		MobileSwayMaxAngle = InValue;
	}
	else if (ParameterName == NAME_MobileOpacityMultiplier)
	{
		MobileOpacityMultiplier = InValue;
	}
}

/**
 *	Set the mobile texture parameter value to the given value.
 *
 *	@param	InParameterName		Name of the parameter to set
 *	@param	InValue				The texture value to set it to
 */
void UMaterialInstanceConstant::SetMobileTextureParameterValue(FName& ParameterName, UTexture* InValue)
{
	if (ParameterName == NAME_MobileBaseTexture)
	{
		MobileBaseTexture = InValue;
	}
	else if (ParameterName == NAME_MobileNormalTexture)
	{
		MobileNormalTexture = InValue;
	}
	else if (ParameterName == NAME_MobileEmissiveTexture)
	{
		MobileEmissiveTexture = InValue;
	}
	else if (ParameterName == NAME_MobileEnvironmentTexture)
	{
		MobileEnvironmentTexture = InValue;
	}
	else if (ParameterName == NAME_MobileMaskTexture)
	{
		MobileMaskTexture = InValue;
	}
	else if (ParameterName == NAME_MobileDetailTexture)
	{
		MobileDetailTexture = InValue;
	}
	else if (ParameterName == NAME_MobileDetailTexture2)
	{
		MobileDetailTexture2 = InValue;
	}
	else if (ParameterName == NAME_MobileDetailTexture3)
	{
		MobileDetailTexture3 = InValue;
	}
}

/**
 *	Set the mobile vector parameter value to the given value.
 *
 *	@param	InParameterName		Name of the parameter to set
 *	@param	InValue				The vector value to set it to
 */
void UMaterialInstanceConstant::SetMobileVectorParameterValue(FName& ParameterName, const FLinearColor& InValue)
{
	if (ParameterName == NAME_MobileSpecularColor)
	{
		MobileSpecularColor = InValue;
	}
	else if (ParameterName == NAME_MobileEmissiveColor)
	{
		MobileEmissiveColor = InValue;
	}
	else if (ParameterName == NAME_MobileEnvironmentColor)
	{
		MobileEnvironmentColor = InValue;
	}
	else if (ParameterName == NAME_MobileRimLightingColor)
	{
		MobileRimLightingColor = InValue;
	}
	else if (ParameterName == NAME_MobileDefaultUniformColor)
	{
		MobileDefaultUniformColor = InValue;
	}
}

/**
* Checks if any of the static parameter values are outdated based on what they reference (eg a normalmap has changed format)
*
* @param	EditorParameters	The new static parameters. 
*/
void UMaterialInstanceConstant::CheckStaticParameterValues(FStaticParameterSet* EditorParameters)
{
	if(IsTemplate(RF_ClassDefaultObject)==FALSE && Parent)
	{
		// Check the CompressionSettings of all NormalParameters to make sure they still match those of any Texture
		for (INT NormalParameterIdx = 0;NormalParameterIdx < EditorParameters->NormalParameters.Num();NormalParameterIdx++)
		{
			FNormalParameter& NormalParameter = EditorParameters->NormalParameters(NormalParameterIdx);
			if( NormalParameter.bOverride == TRUE )
			{
				for(INT TexParameterIdx = 0; TexParameterIdx < TextureParameterValues.Num(); TexParameterIdx++)
				{
					FTextureParameterValue& TextureParameter = TextureParameterValues(TexParameterIdx);
					if( TextureParameter.ParameterName == NormalParameter.ParameterName &&
						TextureParameter.ParameterValue && TextureParameter.ParameterValue->CompressionSettings != NormalParameter.CompressionSettings )
					{
						NormalParameter.CompressionSettings = TextureParameter.ParameterValue->CompressionSettings;
						break;
					}
				}
			}
		}
	}
}


struct FMICKey
{
	UMaterialInstanceConstant* MIC;
	UMaterialInterface* Parent;
	ULevel* Level;
	UBOOL operator==(const FMICKey& Other) const
	{
		return 
			Parent == Other.Parent && 
			Level == Other.Level &&
			MIC->FontParameterValues == Other.MIC->FontParameterValues &&
			MIC->ScalarParameterValues == Other.MIC->ScalarParameterValues &&
			MIC->TextureParameterValues == Other.MIC->TextureParameterValues &&
			MIC->VectorParameterValues == Other.MIC->VectorParameterValues;
	}
};

/**
 * This will iterate over MICs in the world and find identical MICs and replace uses of the 
 * duplicate ones to a single unique MIC. It's based on Parent, overriden parameters, and
 * the level they are in. This only operates on transient or MICs in a level, it won't
 * try to mess with content packages.
 *
 * @param NumFailuresToPrint If you are looking for a reason why some MICs don't get GC'd, specify a number greater than 0, and the function will tell you why that number aren't getting GC'd (via OBJ REFS)
 */
void UMaterialInstanceConstant::CollapseMICs(UINT NumFailuresToPrint)
{
	DOUBLE StartTime = appSeconds();

	// toss the junky MICs that can happen
	UObject::CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, TRUE);

	// collections of MICs
	TArray<FMICKey> MICKeys;
	TArray<UMaterialInstanceConstant*> LevelMICs;
	TMap<UMaterialInstanceConstant*, UMaterialInstanceConstant*> MICRemap;

	INT TotalNumberOfMICs = 0;
	for (TObjectIterator<UMaterialInstanceConstant> It; It; ++It)
	{
		// track total number
		TotalNumberOfMICs++;
		
		UMaterialInstanceConstant* MIC = *It;

		// make a key object
		FMICKey Key;
		Key.MIC = MIC;
		Key.Parent = MIC->Parent;
		Key.Level = MIC->GetTypedOuter<ULevel>();

		// we only care about MICs in a level or transient packages
		if (Key.Level == NULL && !MIC->IsIn(UObject::GetTransientPackage()))
		{
			continue;
		}
		LevelMICs.AddItem(MIC);
	

		// does a matching MIC already exist?
		INT ExistingIndex = MICKeys.FindItemIndex(Key);
		// if so, set up a remap
		if (ExistingIndex != INDEX_NONE)
		{
			MICRemap.Set(MIC, MICKeys(ExistingIndex).MIC);
		}
		// otherwise, add it to the list
		else
		{
			MICKeys.AddItem(Key);
		}
	}

	// report some stats
	debugf(TEXT("There exist %d MICs. %d of these are level MICs. %d of these are unique."), TotalNumberOfMICs, LevelMICs.Num(), MICKeys.Num());

	{
		FGlobalComponentReattachContext Reattach;

		// replace MICs for mesh materials
		for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
		{
			INT NumElements = It->GetNumElements();
			for (INT ElementIndex = 0; ElementIndex < NumElements; ElementIndex++)
			{
				UMaterialInterface* Mat = It->GetElementMaterial(ElementIndex);
				UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Mat);
				if (MIC)
				{
					UMaterialInstanceConstant* ReplacementMIC = MICRemap.FindRef(MIC);
					if (ReplacementMIC)
					{
						It->SetElementMaterial(ElementIndex, ReplacementMIC);
					}
				}
			}
		}

		// replace any non-unique parents of unique MICs
		for (INT Index = 0; Index < MICKeys.Num(); Index++)
		{
			// is the parent an MIC?
			UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(MICKeys(Index).Parent);
			if (MIC)
			{
				// if so, should we replace it?
				UMaterialInstanceConstant* ReplacementMIC = MICRemap.FindRef(MIC);
				if (ReplacementMIC)
				{
					MICKeys(Index).MIC->SetParent(ReplacementMIC);
				}
			}
		}
	}

	// toss the newly-unreferenced MICs
	UObject::CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, TRUE);

	DOUBLE EndTime = appSeconds();

	// count again for info dump
	UINT NewNumberOfMICs = 0;
	UINT NewNumberOfLevelMICs = 0;
	// look for any that we thought could go, but didn't, and print out one reason why not (obj refs)
	for (TObjectIterator<UMaterialInstanceConstant> It; It; ++It)
	{
		// track count
		NewNumberOfMICs++;

		ULevel* Level = It->GetTypedOuter<ULevel>();

		if (Level || It->IsIn(UObject::GetTransientPackage()))
		{
			NewNumberOfLevelMICs++;

			// let's do one OBJ REFs for info on a "should have been tossed" MIC
			if (NumFailuresToPrint && MICRemap.Find(*It))
			{
				debugf(TEXT("%s wasn't tossed because:"), *It->GetFullName());
				FString ObjRefs = FString::Printf(TEXT("obj refs class=materialinstanceconstant name=%s"), *It->GetPathName());
				UObject::StaticExec(*ObjRefs);

				NumFailuresToPrint--;
			}

			// If this is a level MIC make sure the outer is the actual level, to avoid issues when actors are deleted
			if (Level && It->GetOuter() != Level)
			{
				It->Rename(NULL, Level, REN_ForceNoResetLoaders);
			}
		}
	}

	debugf(TEXT("\nAfter collapsing there are now %d MICs, with %d of them level MICs."), NewNumberOfMICs, NewNumberOfLevelMICs);
	debugf(TEXT("Collapsing took %.2f seconds, post-collapse data collection took %.2f seconds."), EndTime - StartTime, appSeconds() - EndTime);
}
