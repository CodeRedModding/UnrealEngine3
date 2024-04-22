/**
 *	
 *	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"
#include "EngineMaterialClasses.h"
#include "MaterialInstance.h"

IMPLEMENT_CLASS(UMaterialInstance);

FMaterialInstanceResource::FMaterialInstanceResource(UMaterialInstance* InOwner,UBOOL bInSelected,UBOOL bInHovered)
:	Parent(NULL)
,	Owner(InOwner)
,	DistanceFieldPenumbraScale(1.0f)
,	bSelected(bInSelected)
,	bHovered(bInHovered)
,	GameThreadParent(NULL)
{
}


const FMaterial* FMaterialInstanceResource::GetMaterial() const
{
	checkSlow(IsInRenderingThread());

	// if there are static permutations expected, look for a good one
	if (Owner->bHasStaticPermutationResource)
	{
		// get the quality level we want - this function is safe to call on rendering thread (UNLIKE GetQualityLevel)
		EMaterialShaderQuality DesiredQuality = Owner->GetDesiredQualityLevel();
		const FMaterial * InstanceMaterial = Owner->StaticPermutationResources[DesiredQuality];

		// if it's missing, look for other quality levels
		// @todo qual: This could almost go into GetQualityLevel, which could skip going up parent if it's in the
		// rendering thread?
		if (!InstanceMaterial)
		{
			DesiredQuality = DesiredQuality == MSQ_HIGH ? MSQ_LOW : MSQ_HIGH;
			InstanceMaterial =  Owner->StaticPermutationResources[DesiredQuality]; 
		}

		//if the instance contains a static permutation resource, use that
		if (InstanceMaterial && InstanceMaterial->GetShaderMap())
		{
			// Verify that compilation has been finalized, the rendering thread shouldn't be touching it otherwise
			checkSlow(InstanceMaterial->GetShaderMap()->IsCompilationFinalized());
			// The shader map reference should have been NULL'ed if it did not compile successfully
			checkSlow(InstanceMaterial->GetShaderMap()->CompiledSuccessfully());
			return InstanceMaterial;
		}
		else 
		{
			//there was an error, use the default material's resource
			return GEngine->DefaultMaterial->GetRenderProxy(bSelected,bHovered)->GetMaterial();
		}
	}
	else
	{
		//use the parent's material resource
		return Parent->GetRenderProxy(bSelected,bHovered)->GetMaterial();
	}
}

/** Called from the game thread to update DistanceFieldPenumbraScale. */
void FMaterialInstanceResource::UpdateDistanceFieldPenumbraScale(FLOAT NewDistanceFieldPenumbraScale)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		UpdateDistanceFieldPenumbraScaleCommand,
		FLOAT*,DistanceFieldPenumbraScale,&DistanceFieldPenumbraScale,
		FLOAT,NewDistanceFieldPenumbraScale,NewDistanceFieldPenumbraScale,
	{
		*DistanceFieldPenumbraScale = NewDistanceFieldPenumbraScale;
	});
}

void FMaterialInstanceResource::GameThread_SetParent(UMaterialInterface* InParent)
{
	check(IsInGameThread());

	if( GameThreadParent != InParent )
	{
		// Set the game thread accessible parent.
		UMaterialInterface* OldParent = GameThreadParent;
		GameThreadParent = InParent;

		// Set the rendering thread's parent and instance pointers.
		check(InParent != NULL);
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			InitMaterialInstanceResource,
			FMaterialInstanceResource*,Resource,this,
			UMaterialInterface*,Parent,InParent,
		{
			Resource->Parent = Parent;
		});

		if( OldParent )
		{
			// make sure that the old parent sticks around until we've set the new parent on FMaterialInstanceResource
			OldParent->ParentRefFence.BeginFence();
		}
	}
}

#if WITH_MOBILE_RHI
/**
 * For UMaterials, this will return the flattened texture for platforms that don't 
 * have full material support
 *
 * @return the FTexture object that represents the flattened texture for this material (can be NULL)
 */
FTexture* FMaterialInstanceResource::GetMobileTexture(const INT MobileTextureUnit) const
{
	UTexture* MobileTexture = Owner->GetMobileTexture(MobileTextureUnit);
	return MobileTexture ? MobileTexture->Resource : NULL;
}
#endif

/**
 * Given a material instance, populate a material instance with textures based on priority of its'
 * generation. I.e. a MobileBaseTexture from a child is dominant over a MobileBaseTexture of its'
 * parent
 *
 * @param   InMaterial		The current material used to seek mobile textures
 * @param	BaseChild		The object which will maintain the textures we retrieve
 *
 * @return  void
 */
void PopulateTexturesFromTree( UMaterialInstance* InMaterial, UMaterialInstance* BaseChild )
{
checkf(0, TEXT("Do not call PopulateTexturesFromTree any more! (%s)"), InMaterial ? *(InMaterial->GetPathName()) : TEXT("NULL"));
}

#if WITH_MOBILE_RHI
/**
 * Fill the mobile material vertex params from a material instance. As material instances can be chained,
 * the root parent is used to determine all the mobile properties, while an additional material hosts the
 * textures which are to be used. 
 * 
 * @param   OutVertexParams		The struct we are filling with this materials vertex params
 *
 * @return  void
 */
void FMaterialInstanceResource::FillMobileMaterialVertexParams (FMobileMaterialVertexParams& OutVertexParams) const
{
	GetMaterial()->FillMobileMaterialVertexParams(Owner, OutVertexParams, NULL);
}

/**
 * Fill the mobile material pixel params from a material instance. As material instances can be chained,
 * the root parent is used to determine all the mobile properties, while an additional material hosts the
 * textures which are to be used. 
 * 
 * @param   OutPixelParams		The struct we are filling with this materials pixel params
 *
 * @return  void
 */
void FMaterialInstanceResource::FillMobileMaterialPixelParams (FMobileMaterialPixelParams& OutPixelParams) const
{
	GetMaterial()->FillMobileMaterialPixelParams(Owner, OutPixelParams, NULL);
}
#endif

UMaterialInstance::UMaterialInstance() :
	bStaticPermutationDirty(FALSE)
{
	if(HasAnyFlags(RF_ClassDefaultObject))
	{
		//don't allocate anything for the class default object
		//otherwise child classes will copy the reference to that memory and it will be deleted twice
		for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
		{
			StaticPermutationResources[QualityIndex] = NULL;
			StaticParameters[QualityIndex] = NULL;
		}
	}
	else
	{
		for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
		{
			//allocate static parameter sets
			StaticPermutationResources[QualityIndex] = NULL;
			StaticParameters[QualityIndex] = new FStaticParameterSet();
		}
	}
}

void UMaterialInstance::InitResources()
{
	// Find the instance's parent.
	UMaterialInterface* SafeParent = NULL;
	if(Parent)
	{
		SafeParent = Parent;
	}

	// Don't use the instance's parent if it has a circular dependency on the instance.
	if(SafeParent && SafeParent->IsDependent(this))
	{
		SafeParent = NULL;
	}

	// If the instance doesn't have a valid parent, use the default material as the parent.
	if(!SafeParent)
	{
		if(GEngine && GEngine->DefaultMaterial)
		{
			SafeParent = GEngine->DefaultMaterial;
		}
		else
		{
			// A material instance was loaded with an invalid GEngine.
			// This is probably because loading the default properties for the GEngine class lead to a material instance being loaded
			// before GEngine has been created.  In this case, we'll just pull the default material config value straight from the INI.
			SafeParent = LoadObject<UMaterialInterface>(NULL,TEXT("engine-ini:Engine.Engine.DefaultMaterialName"),NULL,LOAD_None,NULL);
		}
	}

	checkf(SafeParent, TEXT("Invalid parent on %s"), *GetFullName());

	// Set the material instance's parent on its resources.
	for( INT CurResourceIndex = 0; CurResourceIndex < ARRAY_COUNT( Resources ); ++CurResourceIndex )
	{	
		if( Resources[ CurResourceIndex ] != NULL )
		{
			Resources[ CurResourceIndex ]->GameThread_SetParent( SafeParent );
		}
	}

	if (!IsTemplate())
	{
		// pull down the value
		bHasQualitySwitch = GetMaterial() ? GetMaterial()->bHasQualitySwitch : FALSE;
	}
}

/**
 * NOTE: This is NOT SAFE to use on the rendering thread!
 *
 * @return the quality level this material should render with
 */
EMaterialShaderQuality UMaterialInstance::GetQualityLevel() const
{
	return Parent ? Parent->GetQualityLevel() : MSQ_HIGH;
}

/**
 * Get the material which this is an instance of.
 */
UMaterial* UMaterialInstance::GetMaterial()
{
	if(ReentrantFlag)
	{
		return GEngine->DefaultMaterial;
	}

	FMICReentranceGuard	Guard(this);
	if(Parent)
	{
		return Parent->GetMaterial();
	}
	else if (GEngine)
	{
		return GEngine->DefaultMaterial;
	}
	return NULL;
}

/** Returns the textures used to render this material instance for the given platform. */
void UMaterialInstance::GetUsedTextures(TArray<UTexture*> &OutTextures, EMaterialShaderQuality Quality, UBOOL bAllQualities, UBOOL bAllowOverride)
{
	OutTextures.Empty();

	//Do not care if we're running dedicated server
	if ((appGetPlatformType() & UE3::PLATFORM_WindowsServer) != 0)
	{
		return;
	}

	// default to desired quality if not specified
	if (Quality == MSQ_UNSPECIFIED && bAllQualities == FALSE)
	{
		Quality = GetQualityLevel();
	}

	for (INT QualityIndex = (bAllQualities ? 0 : Quality); (bAllQualities ? QualityIndex < MSQ_MAX : QualityIndex == Quality); QualityIndex++)
	{
		const UMaterialInstance* MaterialInstanceToUse = this;
		// Walk up the material instance chain to the first parent that has static parameters
		while (MaterialInstanceToUse && (!MaterialInstanceToUse->bHasStaticPermutationResource || 
			MaterialInstanceToUse->StaticPermutationResources[QualityIndex] == NULL ||
			!MaterialInstanceToUse->StaticPermutationResources[QualityIndex]->ShaderMap))
		{
			MaterialInstanceToUse = Cast<const UMaterialInstance>(MaterialInstanceToUse->Parent);
		}

		const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >* ExpressionsByType[2];
		const FMaterialResource* SourceMaterialResource = NULL;

		// Use the uniform expressions from the lowest material instance with static parameters in the chain, if one exists
		if ( MaterialInstanceToUse && MaterialInstanceToUse->bHasStaticPermutationResource && 
			MaterialInstanceToUse->StaticPermutationResources[QualityIndex] &&
			MaterialInstanceToUse->StaticPermutationResources[QualityIndex]->ShaderMap )
		{
			ExpressionsByType[0] = &MaterialInstanceToUse->StaticPermutationResources[QualityIndex]->GetUniform2DTextureExpressions();
			ExpressionsByType[1] = &MaterialInstanceToUse->StaticPermutationResources[QualityIndex]->GetUniformCubeTextureExpressions();
			SourceMaterialResource = MaterialInstanceToUse->StaticPermutationResources[QualityIndex];
		}
		else
		{
			// Use the uniform expressions from the base material
			UMaterial* Material = GetMaterial();
			if(!Material)
			{
				// If the material instance has no material, use the default material.
				GEngine->DefaultMaterial->GetUsedTextures(OutTextures, Quality, bAllQualities, bAllowOverride);
				return;
			}

			if (Material->MaterialResources[QualityIndex])
			{
				// Iterate over both the 2D textures and cube texture expressions.
				ExpressionsByType[0] = &Material->MaterialResources[QualityIndex]->GetUniform2DTextureExpressions();
				ExpressionsByType[1] = &Material->MaterialResources[QualityIndex]->GetUniformCubeTextureExpressions();
				SourceMaterialResource = Material->MaterialResources[QualityIndex];
			}
		}

		if (SourceMaterialResource != NULL)
		{
			for(INT TypeIndex = 0;TypeIndex < ARRAY_COUNT(ExpressionsByType);TypeIndex++)
			{
				const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >& Expressions = *ExpressionsByType[TypeIndex];

				// Iterate over each of the material's texture expressions.
				for(INT ExpressionIndex = 0;ExpressionIndex < Expressions.Num();ExpressionIndex++)
				{
					FMaterialUniformExpressionTexture* Expression = Expressions(ExpressionIndex);

					// Evaluate the expression in terms of this material instance.
					UTexture* Texture = NULL;
					Expression->GetGameThreadTextureValue(this,*SourceMaterialResource,Texture, bAllowOverride);
					OutTextures.AddUniqueItem(Texture);
				}
			}
		}
	}
}

/**
* Checks whether the specified texture is needed to render the material instance.
* @param CheckTexture	The texture to check.
* @param bAllowOverride Whether you want to be given the original textures or allow override textures instead of the originals.
* @return UBOOL - TRUE if the material uses the specified texture.
*/
UBOOL UMaterialInstance::UsesTexture(const UTexture* CheckTexture, const UBOOL bAllowOverride)
{
	//Do not care if we're running dedicated server
	if ((appGetPlatformType() & UE3::PLATFORM_WindowsServer) != 0)
	{
		return FALSE;
	}

	TArray<UTexture*> Textures;
	
	GetUsedTextures(Textures, MSQ_UNSPECIFIED, TRUE, bAllowOverride);
	for (INT i = 0; i < Textures.Num(); i++)
	{
		if (Textures(i) == CheckTexture)
		{
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Overrides a specific texture (transient)
 *
 * @param InTextureToOverride The texture to override
 * @param OverrideTexture The new texture to use
 */
void UMaterialInstance::OverrideTexture( const UTexture* InTextureToOverride, UTexture* OverrideTexture )
{
	// Gather texture references from every material resource
	for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
	{
		const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >* ExpressionsByType[2];

		const FMaterialResource* SourceMaterialResource = NULL;
		if (bHasStaticPermutationResource)
		{
			check(StaticPermutationResources[QualityIndex]);
			// Iterate over both the 2D textures and cube texture expressions.
			ExpressionsByType[0] = &StaticPermutationResources[QualityIndex]->GetUniform2DTextureExpressions();
			ExpressionsByType[1] = &StaticPermutationResources[QualityIndex]->GetUniformCubeTextureExpressions();
			SourceMaterialResource = StaticPermutationResources[QualityIndex];
		}
		else
		{
			UMaterial* Material = GetMaterial();
			if(!Material || !Material->MaterialResources[QualityIndex])
			{
				continue;
			}

			// Iterate over both the 2D textures and cube texture expressions.
			ExpressionsByType[0] = &Material->MaterialResources[QualityIndex]->GetUniform2DTextureExpressions();
			ExpressionsByType[1] = &Material->MaterialResources[QualityIndex]->GetUniformCubeTextureExpressions();
			SourceMaterialResource = Material->MaterialResources[QualityIndex];
		}
		
		for(INT TypeIndex = 0;TypeIndex < ARRAY_COUNT(ExpressionsByType);TypeIndex++)
		{
			const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >& Expressions = *ExpressionsByType[TypeIndex];

			// Iterate over each of the material's texture expressions.
			for(INT ExpressionIndex = 0;ExpressionIndex < Expressions.Num();ExpressionIndex++)
			{
				FMaterialUniformExpressionTexture* Expression = Expressions(ExpressionIndex);

				// Evaluate the expression in terms of this material instance.
				const UBOOL bAllowOverride = FALSE;
				UTexture* Texture = NULL;
				Expression->GetGameThreadTextureValue(this,*SourceMaterialResource,Texture,bAllowOverride);

				if( Texture != NULL && Texture == InTextureToOverride )
				{
					// Override this texture!
					Expression->SetTransientOverrideTextureValue( OverrideTexture );
				}
			}
		}
	}
}

/**
 * Checks if the material can be used with the given usage flag.
 * If the flag isn't set in the editor, it will be set and the material will be recompiled with it.
 * @param Usage - The usage flag to check
 * @param bSkipPrim - Bypass the primitive type checks
 * @return UBOOL - TRUE if the material can be used for rendering with the given type.
 */
UBOOL UMaterialInstance::CheckMaterialUsage(const EMaterialUsage Usage, const UBOOL bSkipPrim)
{
	checkSlow(IsInGameThread());
	UMaterial* Material = GetMaterial();
	if(Material)
	{
#if WITH_EDITOR
		if (GEmulateMobileRendering && !GForceDisableEmulateMobileRendering)
		{
			// If we're in mobile emulation mode and there's a mobile emulation material for this material then we will be using that for rendering, so we need to check its usage flags not ours
			UMaterialInstance* MobileMaterial = FMobileEmulationMaterialManager::GetManager()->GetMobileMaterialInstance(this);
			if (MobileMaterial != NULL)
			{
				UMaterial* MobileMaterialRoot = MobileMaterial->GetMaterial();
				checkSlow(MobileMaterialRoot);

				// We call GetUsageByFlag() not CheckMaterialUsage() so that we get back a FALSE for anything the mobile material intentially doesn't support
				UBOOL bUsage = MobileMaterialRoot->GetUsageByFlag(Usage);

				if (!bUsage)
				{
					warnf( NAME_Warning, TEXT("Material usage %s (Material: %s) is not supported in mobile emulation mode."), *Material->GetUsageName(Usage), *GetName() );
				}

				return bUsage;
			}
		}
#endif

		UBOOL bNeedsRecompile = FALSE;
		UBOOL bUsageSetSuccessfully = Material->SetMaterialUsage(bNeedsRecompile, Usage, bSkipPrim);
		if (bNeedsRecompile)
		{
			CacheResourceShaders(GRHIShaderPlatform, FALSE);
			MarkPackageDirty();
		}
		return bUsageSetSuccessfully;
	}
	else
	{
		return FALSE;
	}
}

UBOOL UMaterialInstance::IsDependent(UMaterialInterface* TestDependency)
{
	if(TestDependency == this)
	{
		return TRUE;
	}
	else if(Parent)
	{
		if(ReentrantFlag)
		{
			return TRUE;
		}

		FMICReentranceGuard	Guard(this);
		return Parent->IsDependent(TestDependency);
	}
	else
	{
		return FALSE;
	}
}

/**
* Passes the allocation request up the MIC chain
* @return	The allocated resource
*/
FMaterialResource* UMaterialInstance::AllocateResource()
{
	//pass the allocation request on if the Parent exists
	if (Parent)
	{
		FMaterialResource* NewResource = Parent->AllocateResource();
		if (NewResource)
		{
			return NewResource;
		}
	}

	//otherwise allocate a material resource without specifying a material.
	//the material will be set by AllocateStaticPermutations() before trying to compile this material resource.
	return new FMaterialResource(NULL);
}

/**
 * Gets the static permutation resource if the instance has one
 * @return - the appropriate FMaterialResource if one exists, otherwise NULL
 */
FMaterialResource* UMaterialInstance::GetMaterialResource(EMaterialShaderQuality OverrideQuality)
{
	//if there is a static permutation resource, use that
	if(bHasStaticPermutationResource)
	{
		EMaterialShaderQuality Quality = (OverrideQuality == MSQ_UNSPECIFIED) ? GetQualityLevel() : OverrideQuality;

		check(StaticPermutationResources[Quality]);
		return StaticPermutationResources[Quality];
	}

	//there was no static permutation resource
	return Parent ? Parent->GetMaterialResource(OverrideQuality) : NULL;
}

/*
 * get the Mobile Texture of the Material instance. If none is assigned to the MI, then check its' parent for a texture
 * @return Material Instance Texture || Parents Texture || NULL
 */
UTexture* UMaterialInstance::GetMobileTexture(const INT MobileTextureUnit)
{
	UTexture* MyTexture = UMaterialInterface::GetMobileTexture( MobileTextureUnit );

	return ( MyTexture && MyTexture != GEngine->DefaultTexture ) ? 
				MyTexture : Parent ? Parent->GetMobileTexture(MobileTextureUnit) : NULL;
}

FMaterialRenderProxy* UMaterialInstance::GetRenderProxy(UBOOL Selected, UBOOL bHovered) const
{
	check(!( Selected || bHovered ) || GIsEditor);
#if WITH_EDITOR
	if ((GEmulateMobileRendering == FALSE)||(GForceDisableEmulateMobileRendering))
#endif
	{
		return Resources[Selected ? 1 : ( bHovered ? 2 : 0 )];
	}
#if WITH_EDITOR
	else
	{
		//@todo. Check for mobile base texture?
		FMaterialInstanceResource* MIResource = FMobileEmulationMaterialManager::GetManager()->GetInstanceResource(this, Selected, bHovered);
		if (MIResource != NULL)
		{
			return (FMaterialRenderProxy*)MIResource;
		}
		return Resources[Selected ? 1 : ( bHovered ? 2 : 0 )];
	}
#endif
}

UPhysicalMaterial* UMaterialInstance::GetPhysicalMaterial() const
{
	if(ReentrantFlag)
	{
		return GEngine->DefaultMaterial->GetPhysicalMaterial();
	}

	FMICReentranceGuard	Guard(const_cast<UMaterialInstance*>(this));  // should not need this to determine loop
	if(PhysMaterial)
	{
		return PhysMaterial;
	}
	else if(Parent)
	{
		// If no physical material has been associated with this instance, simply use the parent's physical material.
		return Parent->GetPhysicalMaterial();
	}
	else
	{
		return NULL;
	}
}

/**
 * Returns the lookup texture to be used in the physical material mask.  Tries to get the parents lookup texture if not overridden here. 
 */
UTexture2D* UMaterialInstance::GetPhysicalMaterialMaskTexture() const
{
	if(ReentrantFlag)
	{
		return NULL;
	}

	FMICReentranceGuard	Guard(const_cast<UMaterialInstance*>(this)); 

	if( PhysMaterialMask )
	{
		return PhysMaterialMask;
	}
	else if( Parent )
	{
		return Parent->GetPhysicalMaterialMaskTexture();
	}
	else
	{
		return NULL;
	}
}
	
/** 
 * Returns the UV channel that should be used to look up physical material mask information. Tries to get the parents UV channel if not present here.
 */
INT UMaterialInstance::GetPhysMaterialMaskUVChannel() const
{
	if(ReentrantFlag)
	{
		return -1;
	}

	FMICReentranceGuard	Guard(const_cast<UMaterialInstance*>(this)); 

	if( PhysMaterialMaskUVChannel != -1 )
	{
		return PhysMaterialMaskUVChannel;
	}
	else if( Parent )
	{
		return Parent->GetPhysMaterialMaskUVChannel();
	}
	else
	{
		return -1;
	}
}

/**
 *	Setup the mobile properties for this instance
 */
void UMaterialInstance::SetupMobileProperties()
{
	// 
}

/**
 * Returns the black physical material to be used in the physical material mask.  Tries to get the parents black phys mat if not overridden here. 
 */
UPhysicalMaterial* UMaterialInstance::GetBlackPhysicalMaterial() const
{
	if(ReentrantFlag)
	{
		return NULL;
	}

	FMICReentranceGuard	Guard(const_cast<UMaterialInstance*>(this)); 

	if( BlackPhysicalMaterial )
	{
		return BlackPhysicalMaterial;
	}
	else if( Parent )
	{
		return Parent->GetBlackPhysicalMaterial();
	}
	else
	{
		return NULL;
	}
}

/**
 * Returns the white physical material to be used in the physical material mask.  Tries to get the parents white phys mat if not overridden here. 
 */
UPhysicalMaterial* UMaterialInstance::GetWhitePhysicalMaterial() const
{
	if(ReentrantFlag)
	{
		return NULL;
	}

	FMICReentranceGuard	Guard(const_cast<UMaterialInstance*>(this)); 

	if( WhitePhysicalMaterial )
	{
		return WhitePhysicalMaterial;
	}
	else if( Parent )
	{
		return Parent->GetWhitePhysicalMaterial();
	}
	else
	{
		return NULL;
	}
}

/**
* Makes a copy of all the instance's inherited and overridden static parameters
*
* @param StaticParameters - The set of static parameters to fill, must be empty
*/
void UMaterialInstance::GetStaticParameterValues(FStaticParameterSet* InStaticParameters)
{
	check(IsInGameThread());
	check(InStaticParameters && InStaticParameters->IsEmpty());
	if (Parent)
	{
		UMaterial * ParentMaterial = Parent->GetMaterial();
		TArray<FName> ParameterNames;
		TArray<FGuid> Guids;

		// Static Switch Parameters
		ParentMaterial->GetAllStaticSwitchParameterNames(ParameterNames, Guids);
		if (ParentMaterial->bUsedWithLandscape )
		{
			ULandscapeMaterialInstanceConstant* LandscapeMIC = Cast<ULandscapeMaterialInstanceConstant>(this);
			if (LandscapeMIC && LandscapeMIC->DataWeightmapIndex != INDEX_NONE && LandscapeMIC->DataWeightmapSize )
			{
				ParameterNames.AddUniqueItem(FName(*FString::Printf(TEXT("%s_%d"), *ULandscapeMaterialInstanceConstant::LandscapeVisibilitySwitchName, LandscapeMIC->DataWeightmapIndex)));
				Guids.AddItem(FGuid());
			}
		}
		InStaticParameters->StaticSwitchParameters.AddZeroed(ParameterNames.Num());
		for(INT ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{
			FStaticSwitchParameter& ParentParameter = InStaticParameters->StaticSwitchParameters(ParameterIdx);
			FName ParameterName = ParameterNames(ParameterIdx);
			UBOOL Value = FALSE;
			FGuid ExpressionId = Guids(ParameterIdx);

			ParentParameter.bOverride = FALSE;
			ParentParameter.ParameterName = ParameterName;

			//get the settings from the parent in the MIC chain
			if(Parent->GetStaticSwitchParameterValue(ParameterName, Value, ExpressionId))
			{
				ParentParameter.Value = Value;
			}
			ParentParameter.ExpressionGUID = ExpressionId;

			//if the SourceInstance is overriding this parameter, use its settings
			for(INT ParameterIdx = 0; ParameterIdx < StaticParameters[GetQualityLevel()]->StaticSwitchParameters.Num(); ParameterIdx++)
			{
				const FStaticSwitchParameter &StaticSwitchParam = StaticParameters[GetQualityLevel()]->StaticSwitchParameters(ParameterIdx);

				if(ParameterName == StaticSwitchParam.ParameterName)
				{
					ParentParameter.bOverride = StaticSwitchParam.bOverride;
					if (StaticSwitchParam.bOverride)
					{
						ParentParameter.Value = StaticSwitchParam.Value;
					}
				}
			}
		}

		// Static Component Mask Parameters
		ParentMaterial->GetAllStaticComponentMaskParameterNames(ParameterNames, Guids);
		InStaticParameters->StaticComponentMaskParameters.AddZeroed(ParameterNames.Num());
		for(INT ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{
			FStaticComponentMaskParameter& ParentParameter = InStaticParameters->StaticComponentMaskParameters(ParameterIdx);
			FName ParameterName = ParameterNames(ParameterIdx);
			UBOOL R = FALSE;
			UBOOL G = FALSE;
			UBOOL B = FALSE;
			UBOOL A = FALSE;
			FGuid ExpressionId = Guids(ParameterIdx);

			ParentParameter.bOverride = FALSE;
			ParentParameter.ParameterName = ParameterName;

			//get the settings from the parent in the MIC chain
			if(Parent->GetStaticComponentMaskParameterValue(ParameterName, R, G, B, A, ExpressionId))
			{
				ParentParameter.R = R;
				ParentParameter.G = G;
				ParentParameter.B = B;
				ParentParameter.A = A;
			}
			ParentParameter.ExpressionGUID = ExpressionId;

			//if the SourceInstance is overriding this parameter, use its settings
			for(INT ParameterIdx = 0; ParameterIdx < StaticParameters[GetQualityLevel()]->StaticComponentMaskParameters.Num(); ParameterIdx++)
			{
				const FStaticComponentMaskParameter &StaticComponentMaskParam = StaticParameters[GetQualityLevel()]->StaticComponentMaskParameters(ParameterIdx);

				if(ParameterName == StaticComponentMaskParam.ParameterName)
				{
					ParentParameter.bOverride = StaticComponentMaskParam.bOverride;
					if (StaticComponentMaskParam.bOverride)
					{
						ParentParameter.R = StaticComponentMaskParam.R;
						ParentParameter.G = StaticComponentMaskParam.G;
						ParentParameter.B = StaticComponentMaskParam.B;
						ParentParameter.A = StaticComponentMaskParam.A;
					}
				}
			}
		}

		// Normal Parameters
		ParentMaterial->GetAllNormalParameterNames(ParameterNames, Guids);
		InStaticParameters->NormalParameters.AddZeroed(ParameterNames.Num());
		for(INT ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{
			FNormalParameter& ParentParameter = InStaticParameters->NormalParameters(ParameterIdx);
			FName ParameterName = ParameterNames(ParameterIdx);
			BYTE CompressionSettings = TC_Normalmap;
			FGuid ExpressionId = Guids(ParameterIdx);

			ParentParameter.bOverride = FALSE;
			ParentParameter.ParameterName = ParameterName;

			//get the settings from the parent in the MIC chain
			if(Parent->GetNormalParameterValue(ParameterName, CompressionSettings, ExpressionId))
			{
				ParentParameter.CompressionSettings = CompressionSettings;
			}
			ParentParameter.ExpressionGUID = ExpressionId;

			//if the SourceInstance is overriding this parameter, use its settings
			for(INT ParameterIdx = 0; ParameterIdx < StaticParameters[GetQualityLevel()]->NormalParameters.Num(); ParameterIdx++)
			{
				const FNormalParameter &NormalParam = StaticParameters[GetQualityLevel()]->NormalParameters(ParameterIdx);

				if(ParameterName == NormalParam.ParameterName)
				{
					ParentParameter.bOverride = NormalParam.bOverride;
					if (NormalParam.bOverride)
					{
						ParentParameter.CompressionSettings = NormalParam.CompressionSettings;
					}
				}
			}
		}

		// TerrainLayerWeight Parameters
		ParentMaterial->GetAllTerrainLayerWeightParameterNames(ParameterNames, Guids);
		InStaticParameters->TerrainLayerWeightParameters.AddZeroed(ParameterNames.Num());
		for(INT ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{
			FStaticTerrainLayerWeightParameter& ParentParameter = InStaticParameters->TerrainLayerWeightParameters(ParameterIdx);
			FName ParameterName = ParameterNames(ParameterIdx);
			FGuid ExpressionId = Guids(ParameterIdx);
			INT WeightmapIndex = INDEX_NONE;

			ParentParameter.bOverride = FALSE;
			ParentParameter.ParameterName = ParameterName;
			//get the settings from the parent in the MIC chain
			if(Parent->GetTerrainLayerWeightParameterValue(ParameterName, WeightmapIndex, ExpressionId))
			{
				ParentParameter.WeightmapIndex = WeightmapIndex;
			}
			//ParentParameter.ExpressionGUID = ExpressionId;

			// if the SourceInstance is overriding this parameter, use its settings
			for(INT ParameterIdx = 0; ParameterIdx < StaticParameters[GetQualityLevel()]->TerrainLayerWeightParameters.Num(); ParameterIdx++)
			{
				const FStaticTerrainLayerWeightParameter &TerrainLayerWeightParam = StaticParameters[GetQualityLevel()]->TerrainLayerWeightParameters(ParameterIdx);

				if(ParameterName == TerrainLayerWeightParam.ParameterName)
				{
					ParentParameter.bOverride = TerrainLayerWeightParam.bOverride;
					if (TerrainLayerWeightParam.bOverride)
					{
						ParentParameter.WeightmapIndex = TerrainLayerWeightParam.WeightmapIndex;
					}
				}
			}
		}
	}
}

/**
* Sets the instance's static parameters and marks it dirty if appropriate. 
*
* @param	EditorParameters	The new static parameters.  If the set does not contain any static parameters,
*								the static permutation resource will be released.
* @return		TRUE if the static permutation resource has been marked dirty
*/
UBOOL UMaterialInstance::SetStaticParameterValues(const FStaticParameterSet* EditorParameters)
{
	check(IsInGameThread());

	for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
	{
		// don't need to set the parameters if we don't have a quality switch
		if (QualityIndex != MSQ_HIGH && !bHasQualitySwitch)
		{
			continue;
		}

		if (StaticParameters[QualityIndex])
		{
			//mark the static permutation resource dirty if necessary, so it will be recompiled on PostEditChange()
			bStaticPermutationDirty = bStaticPermutationDirty || StaticParameters[QualityIndex]->ShouldMarkDirty(EditorParameters);
		}

		if (Parent)
		{
			UMaterial * ParentMaterial = Parent->GetMaterial();
			const FMaterial * ParentMaterialResource = ParentMaterial->GetMaterialResource((EMaterialShaderQuality)QualityIndex);

			//check if the BaseMaterialId of the appropriate static parameter set still matches the Id 
			//of the material resource that the base UMaterial owns.  If they are different, the base UMaterial
			//has been edited since the static permutation was compiled.
			if (ParentMaterialResource &&
				ParentMaterialResource->GetId() != StaticParameters[QualityIndex]->BaseMaterialId &&
				!StaticParameters[QualityIndex]->IsEmpty())
			{
				bStaticPermutationDirty = TRUE;
			}
		}
	}

	if (bStaticPermutationDirty)
	{
		//copy the new static parameter set
		//no need to preserve StaticParameters.BaseMaterialId since this will be regenerated
		//now that the static resource has been marked dirty
		for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
		{
			*StaticParameters[QualityIndex] = *EditorParameters;
		}
	}

	return bStaticPermutationDirty;
}

/**
* Checks if any of the static parameter values are outdated based on what they reference (eg a normalmap has changed format)
*
* @param	EditorParameters	The new static parameters. 
*/
void UMaterialInstance::CheckStaticParameterValues(FStaticParameterSet* EditorParameters)
{
}

/**
* Compiles the static permutation resource if the base material has changed and updates dirty states
*/
void UMaterialInstance::UpdateStaticPermutation()
{
	if (bStaticPermutationDirty && Parent)
	{
		//a static permutation resource must exist if there are static parameters
		for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
		{
			if (StaticParameters[QualityIndex] && !StaticParameters[QualityIndex]->IsEmpty())
			{
				bHasStaticPermutationResource = TRUE;
				break;
			}
		}

		CacheResourceShaders(GRHIShaderPlatform, FALSE);
		if (bHasStaticPermutationResource)
		{
			FGlobalComponentReattachContext RecreateComponents;	
		}
		bStaticPermutationDirty = FALSE;
	}
}

/**
* Updates static parameters and recompiles the static permutation resource if necessary
*/
void UMaterialInstance::InitStaticPermutation()
{
	if (appGetPlatformType() & UE3::PLATFORM_WindowsServer)
	{
		// Do nothing
		return;
	}

	if (Parent && bHasStaticPermutationResource && (!GUseSeekFreeLoading || GIsEditor))
	{
		// When seekfreeloading it is assumed the parent material has not changed!
		// Unless the editor is being run with seekfreeloading...
		//update the static parameters, since they may have changed in the parent material
		FStaticParameterSet UpdatedStaticParameters;
		GetStaticParameterValues(&UpdatedStaticParameters);
		//update any normal parameters that have that have CompressionSettings that no longer match their referenced texture.
		CheckStaticParameterValues(&UpdatedStaticParameters);
		if(SetStaticParameterValues(&UpdatedStaticParameters))
		{
#if WITH_EDITOR
			if(GIsEditor)
			{
				// A material must be recompiled because a function it uses is out of date
				MaterialPackagesWithDependentChanges.Add( GetOutermost() );
			}
#endif // WITH_EDITOR
		}
	}

	if (GForceMinimalShaderCompilation)
	{
		// do nothing
	}
	else if (GCookingTarget & (UE3::PLATFORM_Windows|UE3::PLATFORM_WindowsConsole))
	{
		//If we are cooking for PC, cache shaders for all PC platforms.
		CacheResourceShaders(SP_PCD3D_SM3, FALSE);
		if( !ShaderUtils::ShouldForceSM3ShadersOnPC() )
		{
			CacheResourceShaders(SP_PCD3D_SM5, FALSE);
			CacheResourceShaders(SP_PCOGL, FALSE);
		}
	}
	else if (GCookingTarget & UE3::PLATFORM_WindowsServer)
	{
		// do nothing
	}
	else if (GIsCooking)
	{
		//Make sure the material resource shaders are cached.
		CacheResourceShaders(GCookingShaderPlatform, FALSE);
	}
	else
	{
		//Make sure the material resource shaders are cached.
		CacheResourceShaders(GRHIShaderPlatform, FALSE);
	}
}

/**
* Compiles material resources for the given platform if the shader map for that resource didn't already exist.
*
* @param ShaderPlatform - the platform to compile for.
* @param bFlushExistingShaderMaps - forces a compile, removes existing shader maps from shader cache.
* @param bForceAllPlatforms - compile for all platforms, not just the current.
*/
void UMaterialInstance::CacheResourceShaders(EShaderPlatform ShaderPlatform, UBOOL bFlushExistingShaderMaps, UBOOL bDebugDump)
{
#if WITH_EDITOR
	UBOOL bForceMobileEmulationUpdate = FALSE;
#endif
	// Fix-up the parent lighting guid if it has changed...
	if (Parent && (Parent->GetLightingGuid() != ParentLightingGuid))
	{
		SetLightingGuid();
		ParentLightingGuid = Parent ? Parent->GetLightingGuid() : FGuid(0,0,0,0);
	}

	if (bHasStaticPermutationResource)
	{
		//make sure all material resources are allocated and have their Material member updated
		AllocateStaticPermutations();

		if (appGetPlatformType() & UE3::PLATFORM_WindowsServer)
		{	
			//Only allocate shader resources if not running dedicated server
			return;
		}

		//go through each material resource
		for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
		{
			UBOOL bKeepAllMaterialQualityLevelsLoaded = TRUE;
			if (!GIsEditor)
			{
				verify(GConfig->GetBool(TEXT("Engine.Engine"), TEXT("bKeepAllMaterialQualityLevelsLoaded"), bKeepAllMaterialQualityLevelsLoaded, GEngineIni));
			}
			// don't need to compile a low quality material if there is no ability to make a low quality material (ie has a switch)
			// and only compile all versions if we want all to be loaded
			bKeepAllMaterialQualityLevelsLoaded = bKeepAllMaterialQualityLevelsLoaded && bHasQualitySwitch;

			UBOOL bShouldCacheThisLevel = FALSE;
			if (bKeepAllMaterialQualityLevelsLoaded || QualityIndex == GetDesiredQualityLevel())
			{
				bShouldCacheThisLevel = TRUE;
			}

			// if we don't need to make sure this is compiled, skip it
			if (!bShouldCacheThisLevel)
			{
				continue;
			}

			// we can't compile a resource if the parent already tossed it for that quality level
			if (Parent && GetMaterial()->GetMaterialResource((EMaterialShaderQuality)QualityIndex) == NULL)
			{
				continue;
			}

			//mark the package dirty if the material resource has never been compiled
			if (GIsEditor && !StaticPermutationResources[QualityIndex]->GetId().IsValid()
				|| bFlushExistingShaderMaps)
			{
				MarkPackageDirty();
			}

			const UBOOL bSuccess = Parent->CompileStaticPermutation(
				StaticParameters[QualityIndex], 
				StaticPermutationResources[QualityIndex], 
				ShaderPlatform, 
				(EMaterialShaderQuality)QualityIndex,
				bFlushExistingShaderMaps,
				bDebugDump);

			if (bSuccess)
			{
				// After the compile, the material resource has references to all textures used by uniform expressions
				// Add references to textures used for rendering that might be different from what the uniform expressions reference (FMaterialUniformExpressionTextureParameter)

				TArray<UTexture*> Textures;
					
				GetUsedTextures(Textures, (EMaterialShaderQuality)QualityIndex, FALSE);
				StaticPermutationResources[QualityIndex]->AddReferencedTextures(Textures);
			}

			if (!bSuccess)
			{
				const UMaterial* BaseMaterial = GetMaterial();
				warnf(NAME_Warning, TEXT("Failed to compile Material Instance %s with Base %s for platform %s, Default Material will be used in game."), 
					*GetPathName(), 
					BaseMaterial ? *BaseMaterial->GetName() : TEXT("Null"), 
					ShaderPlatformToText(ShaderPlatform));

				const TArray<FString>& CompileErrors = StaticPermutationResources[QualityIndex]->GetCompileErrors();
				for (INT ErrorIndex = 0; ErrorIndex < CompileErrors.Num(); ErrorIndex++)
				{
					warnf(NAME_DevShaders, TEXT("	%s"), *CompileErrors(ErrorIndex));
				}
			}
#if PLATFORM_DESKTOP && !USE_NULL_RHI
			else if (ShaderPlatform == SP_PCOGL)
			{
				extern void AddMaterialToOpenGLProgramCache(const FString &MaterialName, const FMaterialResource *MaterialResource);
				AddMaterialToOpenGLProgramCache(GetPathName(), StaticPermutationResources[QualityIndex]);
			}
#endif

#if WITH_EDITOR
			bForceMobileEmulationUpdate = TRUE;
#endif
			bStaticPermutationDirty = FALSE;
		}
	}
	else
	{
		ReleaseStaticPermutations();
	}

#if WITH_EDITOR
	FMobileEmulationMaterialManager::GetManager()->UpdateMaterialInterface(this, bForceMobileEmulationUpdate, FALSE);
#endif
}

/**
* Passes the compile request up the MIC chain
*
* @param StaticParameters - The set of static parameters to compile for
* @param StaticPermutation - The resource to compile
* @param Platform - The platform to compile for
* @param MaterialPlatform - The material platform to compile for
* @param bFlushExistingShaderMaps - Indicates that existing shader maps should be discarded
* @return TRUE if compilation was successful or not necessary
*/
UBOOL UMaterialInstance::CompileStaticPermutation(
	FStaticParameterSet* Permutation, 
	FMaterialResource* StaticPermutation, 
	EShaderPlatform Platform, 
	EMaterialShaderQuality Quality,
	UBOOL bFlushExistingShaderMaps,
	UBOOL bDebugDump)
{
	UBOOL bCompileSucceeded = FALSE;
	if (Parent)
	{
		bCompileSucceeded = Parent->CompileStaticPermutation(Permutation, StaticPermutation, Platform, Quality, bFlushExistingShaderMaps, bDebugDump);
	}
	return bCompileSucceeded;
}

/**
* Allocates the static permutation resources for all platforms if they haven't been already.
* Also updates the material resource's Material member as it may have changed.
*/
void UMaterialInstance::AllocateStaticPermutations()
{
	for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
	{
		//allocate the material resource if it hasn't already been
		if (!StaticPermutationResources[QualityIndex])
		{
			StaticPermutationResources[QualityIndex] = AllocateResource();
		}

		if (Parent)
		{
			UMaterial* ParentMaterial = Parent->GetMaterial();
			StaticPermutationResources[QualityIndex]->SetMaterial(ParentMaterial);
		}
	}
}

/**
* Releases the static permutation resource if it exists, in a thread safe way
*/
void UMaterialInstance::ReleaseStaticPermutations()
{
	for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
	{
		if (StaticPermutationResources[QualityIndex])
		{
			StaticPermutationResources[QualityIndex]->ReleaseFence.BeginFence();
			while (StaticPermutationResources[QualityIndex]->ReleaseFence.GetNumPendingFences())
			{
				appSleep(0);
			}
			delete StaticPermutationResources[QualityIndex];
			StaticPermutationResources[QualityIndex] = NULL;
		}
	}
}



/**
* Gets the value of the given static switch parameter.  If it is not found in this instance then
*		the request is forwarded up the MIC chain.
*
* @param	ParameterName	The name of the static switch parameter
* @param	OutValue		Will contain the value of the parameter if successful
* @return					True if successful
*/
UBOOL UMaterialInstance::GetStaticSwitchParameterValue(FName ParameterName, UBOOL &OutValue,FGuid &OutExpressionGuid)
{
	if(ReentrantFlag)
	{
		return FALSE;
	}

	UBOOL* Value = NULL;
	FGuid* Guid = NULL;
	for (INT ValueIndex = 0;ValueIndex < StaticParameters[GetQualityLevel()]->StaticSwitchParameters.Num();ValueIndex++)
	{
		if (StaticParameters[GetQualityLevel()]->StaticSwitchParameters(ValueIndex).ParameterName == ParameterName)
		{
			Value = &StaticParameters[GetQualityLevel()]->StaticSwitchParameters(ValueIndex).Value;
			Guid = &StaticParameters[GetQualityLevel()]->StaticSwitchParameters(ValueIndex).ExpressionGUID;
			break;
		}
	}
	if(Value)
	{
		OutValue = *Value;
		OutExpressionGuid = *Guid;
		return TRUE;
	}
	else if(Parent)
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetStaticSwitchParameterValue(ParameterName,OutValue,OutExpressionGuid);
	}
	else
	{
		return FALSE;
	}
}

/**
* Gets the value of the given static component mask parameter. If it is not found in this instance then
*		the request is forwarded up the MIC chain.
*
* @param	ParameterName	The name of the parameter
* @param	R, G, B, A		Will contain the values of the parameter if successful
* @return					True if successful
*/
UBOOL UMaterialInstance::GetStaticComponentMaskParameterValue(FName ParameterName, UBOOL &OutR, UBOOL &OutG, UBOOL &OutB, UBOOL &OutA, FGuid &OutExpressionGuid)
{
	if(ReentrantFlag)
	{
		return FALSE;
	}

	UBOOL* R = NULL;
	UBOOL* G = NULL;
	UBOOL* B = NULL;
	UBOOL* A = NULL;
	FGuid* ExpressionId = NULL;
	
	EMaterialShaderQuality Quality = GetQualityLevel();
	for (INT ValueIndex = 0;ValueIndex < StaticParameters[Quality]->StaticComponentMaskParameters.Num();ValueIndex++)
	{
		if (StaticParameters[Quality]->StaticComponentMaskParameters(ValueIndex).ParameterName == ParameterName)
		{
			R = &StaticParameters[Quality]->StaticComponentMaskParameters(ValueIndex).R;
			G = &StaticParameters[Quality]->StaticComponentMaskParameters(ValueIndex).G;
			B = &StaticParameters[Quality]->StaticComponentMaskParameters(ValueIndex).B;
			A = &StaticParameters[Quality]->StaticComponentMaskParameters(ValueIndex).A;
			ExpressionId = &StaticParameters[Quality]->StaticComponentMaskParameters(ValueIndex).ExpressionGUID;
			break;
		}
	}
	if(R && G && B && A)
	{
		OutR = *R;
		OutG = *G;
		OutB = *B;
		OutA = *A;
		OutExpressionGuid = *ExpressionId;
		return TRUE;
	}
	else if(Parent)
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetStaticComponentMaskParameterValue(ParameterName, OutR, OutG, OutB, OutA, OutExpressionGuid);
	}
	else
	{
		return FALSE;
	}
}


/**
* Gets the compression format of the given normal parameter
*
* @param	ParameterName	The name of the parameter
* @param	CompressionSettings	Will contain the values of the parameter if successful
* @return					True if successful
*/
UBOOL UMaterialInstance::GetNormalParameterValue(FName ParameterName, BYTE& OutCompressionSettings, FGuid &OutExpressionGuid)
{
	if(ReentrantFlag)
	{
		return FALSE;
	}

	BYTE* CompressionSettings = NULL;

	FGuid* ExpressionId = NULL;
	EMaterialShaderQuality Quality = GetQualityLevel();
	for (INT ValueIndex = 0;ValueIndex < StaticParameters[Quality]->NormalParameters.Num();ValueIndex++)
	{
		if (StaticParameters[Quality]->NormalParameters(ValueIndex).ParameterName == ParameterName)
		{
			CompressionSettings = &StaticParameters[Quality]->NormalParameters(ValueIndex).CompressionSettings;
			ExpressionId = &StaticParameters[Quality]->NormalParameters(ValueIndex).ExpressionGUID;
			break;
		}
	}
	if(CompressionSettings)
	{
		OutCompressionSettings = *CompressionSettings;
		OutExpressionGuid = *ExpressionId;
		return TRUE;
	}
	else if(Parent)
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetNormalParameterValue(ParameterName, OutCompressionSettings, OutExpressionGuid);
	}
	else
	{
		return FALSE;
	}
}

/**
* Gets the weightmap index of the given terrain layer weight parameter
*
* @param	ParameterName	The name of the parameter
* @param	OutWeightmapIndex	Will contain the values of the parameter if successful
* @return					True if successful
*/
UBOOL UMaterialInstance::GetTerrainLayerWeightParameterValue(FName ParameterName, INT& OutWeightmapIndex, FGuid &OutExpressionGuid)
{
	if(ReentrantFlag)
	{
		return FALSE;
	}

	INT WeightmapIndex = INDEX_NONE;

	FGuid* ExpressionId = NULL;
	EMaterialShaderQuality Quality = GetQualityLevel();
	for (INT ValueIndex = 0;ValueIndex < StaticParameters[Quality]->TerrainLayerWeightParameters.Num();ValueIndex++)
	{
		if (StaticParameters[Quality]->TerrainLayerWeightParameters(ValueIndex).ParameterName == ParameterName)
		{
			WeightmapIndex = StaticParameters[Quality]->TerrainLayerWeightParameters(ValueIndex).WeightmapIndex;
			ExpressionId = &StaticParameters[Quality]->TerrainLayerWeightParameters(ValueIndex).ExpressionGUID;
			break;
		}
	}
	if (WeightmapIndex >= 0)
	{
		OutWeightmapIndex = WeightmapIndex;
		OutExpressionGuid = *ExpressionId;
		return TRUE;
	}
	else if(Parent)
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetTerrainLayerWeightParameterValue(ParameterName, OutWeightmapIndex, OutExpressionGuid);
	}
	else
	{
		return FALSE;
	}
}

/** === USurface interface === */
/** 
 * Method for retrieving the width of this surface.
 *
 * This implementation returns the maximum width of all textures applied to this material - not exactly accurate, but best approximation.
 *
 * @return	the width of this surface, in pixels.
 */
FLOAT UMaterialInstance::GetSurfaceWidth() const
{
	FLOAT MaxTextureWidth = 0.f;
	TArray<UTexture*> Textures;
	
	const_cast<UMaterialInstance*>(this)->GetUsedTextures(Textures);
	for ( INT TextureIndex = 0; TextureIndex < Textures.Num(); TextureIndex++ )
	{
		UTexture* AppliedTexture = Textures(TextureIndex);
		if ( AppliedTexture != NULL )
		{
			MaxTextureWidth = Max(MaxTextureWidth, AppliedTexture->GetSurfaceWidth());
		}
	}

	if ( Abs(MaxTextureWidth) < DELTA && Parent != NULL )
	{
 		MaxTextureWidth = Parent->GetSurfaceWidth();
	}

	return MaxTextureWidth;
}
/** 
 * Method for retrieving the height of this surface.
 *
 * This implementation returns the maximum height of all textures applied to this material - not exactly accurate, but best approximation.
 *
 * @return	the height of this surface, in pixels.
 */
FLOAT UMaterialInstance::GetSurfaceHeight() const
{
	FLOAT MaxTextureHeight = 0.f;
	TArray<UTexture*> Textures;
	
	const_cast<UMaterialInstance*>(this)->GetUsedTextures(Textures);
	for ( INT TextureIndex = 0; TextureIndex < Textures.Num(); TextureIndex++ )
	{
		UTexture* AppliedTexture = Textures(TextureIndex);
		if ( AppliedTexture != NULL )
		{
			MaxTextureHeight = Max(MaxTextureHeight, AppliedTexture->GetSurfaceHeight());
		}
	}

	if ( Abs(MaxTextureHeight) < DELTA && Parent != NULL )
	{
		MaxTextureHeight = Parent->GetSurfaceHeight();
	}

	return MaxTextureHeight;
}

void UMaterialInstance::AddReferencedObjects(TArray<UObject*>& ObjectArray)
{
	Super::AddReferencedObjects(ObjectArray);

	for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
	{
		if(StaticPermutationResources[QualityIndex])
		{
			StaticPermutationResources[QualityIndex]->AddReferencedObjects(ObjectArray);
		}
	}
}

void UMaterialInstance::PreSave()
{
	Super::PreSave();

#if WITH_EDITORONLY_DATA
	//make sure all material resources have been initialized (and compiled if needed)
	//so that the material resources will be complete for saving.  Don't do this during script compile,
	//since only dummy materials are saved, or when cooking, since compiling material shaders is handled explicitly by the commandlet.
	if (!GIsUCCMake && !GIsCooking && !GIsUCC)
	{
		if (Parent && Parent->GetMaterial()->bUsedWithLandscape)
		{
			InitStaticPermutation(); // Landscape Material Instance need special handling for Visibility mask
			CacheResourceShaders(GRHIShaderPlatform, FALSE);
		}
		else
		{
			CacheResourceShaders(GRHIShaderPlatform, FALSE);
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void UMaterialInstance::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	//only serialize the static permutation resource if one exists
	if (bHasStaticPermutationResource)
	{
		// explicitly record which qualities were saved out, so we know what to load in
		// (default to just HIGH detail for old packages)
		UINT QualityMask = 0x1;
		if (Ar.Ver() >= VER_ADDED_MATERIAL_QUALITY_LEVEL)
		{
			// figure out what we are going to save
			if (Ar.IsSaving())
			{
				for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
				{
					if (StaticPermutationResources[QualityIndex])
					{
						QualityMask |= (1 << QualityIndex);
					}
				}
			}

			// serialize the mask
			Ar << QualityMask;
		}

		for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
		{
			if (Ar.IsSaving() && StaticPermutationResources[QualityIndex])
			{
				//remove external private dependencies on the base material
				//@todo dw - come up with a better solution
				StaticPermutationResources[QualityIndex]->RemoveExpressions();
			}

			// don't serialize unwanted expressions
			if ((QualityMask & (1<<QualityIndex)) == 0)
			{
				continue;
			}


			if (Ar.IsLoading())
			{
				StaticPermutationResources[QualityIndex] = AllocateResource();
			}

			StaticPermutationResources[QualityIndex]->Serialize(Ar);
			if (Ar.Ver() < VER_UNIFORM_EXPRESSIONS_IN_SHADER_CACHE)
			{
				// If we are loading a material resource saved before texture references were managed by the material resource,
				// Pass the legacy texture references to the material resource.
				StaticPermutationResources[QualityIndex]->AddLegacyTextures(ReferencedTextures_DEPRECATED);
			}

			StaticParameters[QualityIndex]->Serialize(Ar);
		}
	}

	if (bHasStaticPermutationResource && Ar.Ver() < VER_REMOVED_SHADER_MODEL_2)
	{
		FMaterialResource* LegacySM2Resource = NULL;
		if (Ar.IsLoading())
		{
			LegacySM2Resource = AllocateResource();
		}

		LegacySM2Resource->Serialize(Ar);
		FStaticParameterSet LegacySM2Paramters;
		LegacySM2Paramters.Serialize(Ar);
	}

	if (Ar.Ver() < VER_UNIFORM_EXPRESSIONS_IN_SHADER_CACHE)
	{
		// Empty legacy texture references on load
		ReferencedTextures_DEPRECATED.Empty();
	}

	if (Ar.Ver() < VER_INTEGRATED_LIGHTMASS)
	{
		ParentLightingGuid = Parent ? Parent->GetLightingGuid() : FGuid(0,0,0,0);
	}

	if (Ar.IsLoading())
	{
		if ((GIsEditor || GIsCooking || GUsingMobileRHI) && (Ar.Ver() < VER_MOBILE_MATERIAL_PARAMETER_RENAME))
		{
			// We need to copy the overridden mobile texture stuff into the parameters...
			if (MobileBaseTexture != NULL)
			{
				SetTextureParameterValue(NAME_MobileBaseTexture, MobileBaseTexture);
			}
			if (MobileEmissiveTexture != NULL)
			{
				SetTextureParameterValue(NAME_MobileEmissiveTexture, MobileEmissiveTexture);
			}
			if (MobileDetailTexture != NULL)
			{
				SetTextureParameterValue(NAME_MobileDetailTexture, MobileDetailTexture);
			}
			if (MobileEnvironmentTexture != NULL)
			{
				SetTextureParameterValue(NAME_MobileEnvironmentTexture, MobileEnvironmentTexture);
			}
			if (MobileNormalTexture != NULL)
			{
				SetTextureParameterValue(NAME_MobileNormalTexture, MobileNormalTexture);
			}
			if (MobileMaskTexture != NULL)
			{
				SetTextureParameterValue(NAME_MobileMaskTexture, MobileMaskTexture);
			}
		}
	}
}

void UMaterialInstance::PostLoad()
{
	Super::PostLoad();

	//fixup the instance if the parent doesn't exist anymore
	if (bHasStaticPermutationResource && !Parent)
	{
		bHasStaticPermutationResource = FALSE;
	}

	if (!IsTemplate())
	{
		// cache the quality switch before initializing anything
		bHasQualitySwitch = GetMaterial() ? GetMaterial()->bHasQualitySwitch : FALSE;
	}

	// Make sure static parameters are up to date and shaders are cached for the current platform
	InitStaticPermutation();

	if (GIsEditor && GEngine != NULL && !IsTemplate())
	{
		// Ensure that the ReferencedTextureGuids array is up to date.
		UpdateLightmassTextureTracking();
	}

	for (INT i = 0; i < ARRAY_COUNT(Resources); i++)
	{
		if (Resources[i])
		{
			Resources[i]->UpdateDistanceFieldPenumbraScale(GetDistanceFieldPenumbraScale());
		}
	}

	UBOOL bTossUnusedLevels = FALSE;
	if (GIsCooking)
	{
		// when cooking, always throw away levels we don't need
		bTossUnusedLevels = TRUE;
	}
	else if (!GIsEditor)
	{
		// if we are not in the editor (where it makes sense to have all levels loaded), toss non-active
		// quality levels if desired
		UBOOL bKeepAllMaterialQualityLevelsLoaded;
		verify(GConfig->GetBool(TEXT("Engine.Engine"), TEXT("bKeepAllMaterialQualityLevelsLoaded"), bKeepAllMaterialQualityLevelsLoaded, GEngineIni));
		bTossUnusedLevels = !bKeepAllMaterialQualityLevelsLoaded;
	}

	if (bTossUnusedLevels)
	{
		// this is the quality we want to use
		EMaterialShaderQuality DesiredQuality = GetQualityLevel();

		// toss other levels
		for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
		{
			if (StaticPermutationResources[QualityIndex] && QualityIndex != DesiredQuality)
			{
				// just toss the whole thing
				delete StaticPermutationResources[QualityIndex];
				StaticPermutationResources[QualityIndex] = NULL;
			}
		}
	}
}

void UMaterialInstance::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if( PropertyThatChanged )
	{
		if(PropertyThatChanged->GetName()==TEXT("Parent"))
		{
			if (Parent == NULL)
			{
				bHasStaticPermutationResource = FALSE;
			}

			ParentLightingGuid = Parent ? Parent->GetLightingGuid() : FGuid(0,0,0,0);
		}
	}

	// Ensure that the ReferencedTextureGuids array is up to date.
	if (GIsEditor)
	{
		UpdateLightmassTextureTracking();
	}

	for (INT i = 0; i < ARRAY_COUNT(Resources); i++)
	{
		if (Resources[i])
		{
			Resources[i]->UpdateDistanceFieldPenumbraScale(GetDistanceFieldPenumbraScale());
		}
	}
}

void UMaterialInstance::BeginDestroy()
{
	Super::BeginDestroy();

	for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
	{
		if(StaticPermutationResources[QualityIndex])
		{
			StaticPermutationResources[QualityIndex]->ReleaseFence.BeginFence();
		}
	}
}

UBOOL UMaterialInstance::IsReadyForFinishDestroy()
{
	UBOOL bIsReady = Super::IsReadyForFinishDestroy();

	for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
	{
		bIsReady = bIsReady && (!StaticPermutationResources[QualityIndex] || !StaticPermutationResources[QualityIndex]->ReleaseFence.GetNumPendingFences());
	}

	return bIsReady;
}

void UMaterialInstance::FinishDestroy()
{
	if(!GIsUCCMake&&!HasAnyFlags(RF_ClassDefaultObject))
	{
		BeginCleanup(Resources[0]);
		if(GIsEditor)
		{
			BeginCleanup(Resources[1]);
			BeginCleanup(Resources[2]);
		}
	}

	for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
	{
		if (StaticPermutationResources[QualityIndex])
		{
			check(!StaticPermutationResources[QualityIndex]->ReleaseFence.GetNumPendingFences());
			delete StaticPermutationResources[QualityIndex];
			StaticPermutationResources[QualityIndex] = NULL;
		}

		if (StaticParameters[QualityIndex])
		{
			delete StaticParameters[QualityIndex];
			StaticParameters[QualityIndex] = NULL;
		}
	}

	Super::FinishDestroy();
}

/**
* Refreshes parameter names using the stored reference to the expression object for the parameter.
*/
void UMaterialInstance::UpdateParameterNames()
{
}

void UMaterialInstance::SetParent(UMaterialInterface* NewParent)
{
	Parent = NewParent;
}

void UMaterialInstance::SetVectorParameterValue(FName ParameterName,const FLinearColor& Value)
{
}

void UMaterialInstance::SetScalarParameterValue(FName ParameterName, FLOAT Value)
{
}

void UMaterialInstance::SetScalarCurveParameterValue(FName ParameterName, const FInterpCurveFloat& Value)
{
}

void UMaterialInstance::SetTextureParameterValue(FName ParameterName, UTexture* Value)
{
}

UBOOL UMaterialInstance::GetTextureParameterValue(FName ParameterName, UTexture*& OutValue)
{
	return FALSE;
}


void UMaterialInstance::SetFontParameterValue(FName ParameterName,class UFont* FontValue,INT FontPage)
{
}

void UMaterialInstance::ClearParameterValues()
{
}

UBOOL UMaterialInstance::IsInMapOrTransientPackage() const
{
	const UBOOL bInMap = GetOutermost()->ContainsMap();
	const UBOOL bInTransient = GetOutermost() == UObject::GetTransientPackage();

	return (bInMap || bInTransient);
}

#if !FINAL_RELEASE
/** Displays warning if this material instance is not safe to modify in the game (is in content package) */
void UMaterialInstance::CheckSafeToModifyInGame(const TCHAR* FuncName, const TCHAR* ParamName) const
{
	if(GIsGame && !IsInMapOrTransientPackage() && GAreScreenMessagesEnabled)
	{
		const FString Message = FString::Printf(TEXT("%s : Modifying '%s' during gameplay. ParamName: %s"), FuncName, *GetName(), ParamName);
		if (GWorld != NULL)
		{
			AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
			if (WorldInfo != NULL)
			{
				GWorld->GetWorldInfo()->AddOnScreenDebugMessage((QWORD)((PTRINT)this), 5.f, FColor(0,255,255), *Message);
			}
		}
		debugf(*Message);
	}
}
#endif

/**
 *	Check if the textures have changed since the last time the material was
 *	serialized for Lightmass... Update the lists while in here.
 *	It will update the LightingGuid if the textures have changed.
 *	NOTE: This will NOT mark the package dirty if they have changed.
 *
 *	@return	UBOOL	TRUE if the textures have changed.
 *					FALSE if they have not.
 */
UBOOL UMaterialInstance::UpdateLightmassTextureTracking()
{
	UBOOL bTexturesHaveChanged = FALSE;
#if WITH_EDITORONLY_DATA
	TArray<UTexture*> UsedTextures;
	
	GetUsedTextures(UsedTextures, MSQ_UNSPECIFIED, TRUE);
	if (UsedTextures.Num() != ReferencedTextureGuids.Num())
	{
		bTexturesHaveChanged = TRUE;
		// Just clear out all the guids and the code below will
		// fill them back in...
		ReferencedTextureGuids.Empty(UsedTextures.Num());
		ReferencedTextureGuids.AddZeroed(UsedTextures.Num());
	}
	
	for (INT CheckIdx = 0; CheckIdx < UsedTextures.Num(); CheckIdx++)
	{
		UTexture* Texture = UsedTextures(CheckIdx);
		if (Texture)
		{
			if (ReferencedTextureGuids(CheckIdx) != Texture->GetLightingGuid())
			{
				ReferencedTextureGuids(CheckIdx) = Texture->GetLightingGuid();
				bTexturesHaveChanged = TRUE;
			}
		}
		else
		{
			if (ReferencedTextureGuids(CheckIdx) != FGuid(0,0,0,0))
			{
				ReferencedTextureGuids(CheckIdx) = FGuid(0,0,0,0);
				bTexturesHaveChanged = TRUE;
			}
		}
	}
#endif // WITH_EDITORONLY_DATA

	if ( bTexturesHaveChanged )
	{
		// This will invalidate any cached Lightmass material exports
		SetLightingGuid();
	}

	return bTexturesHaveChanged;
}

/** @return	The bCastShadowAsMasked value for this material. */
UBOOL UMaterialInstance::GetCastShadowAsMasked() const
{
	if (LightmassSettings.bOverrideCastShadowAsMasked)
	{
		return LightmassSettings.bCastShadowAsMasked;
	}

	if (Parent)
	{
		return Parent->GetCastShadowAsMasked();
	}

	return FALSE;
}

/** @return	The Emissive boost value for this material. */
FLOAT UMaterialInstance::GetEmissiveBoost() const
{
	if (LightmassSettings.bOverrideEmissiveBoost)
	{
		return LightmassSettings.EmissiveBoost;
	}

	if (Parent)
	{
		return Parent->GetEmissiveBoost();
	}

	return 1.0f;
}

/** @return	The Diffuse boost value for this material. */
FLOAT UMaterialInstance::GetDiffuseBoost() const
{
	if (LightmassSettings.bOverrideDiffuseBoost)
	{
		return LightmassSettings.DiffuseBoost;
	}

	if (Parent)
	{
		return Parent->GetDiffuseBoost();
	}

	return 1.0f;
}

/** @return	The Specular boost value for this material. */
FLOAT UMaterialInstance::GetSpecularBoost() const
{
	if (LightmassSettings.bOverrideSpecularBoost)
	{
		return LightmassSettings.SpecularBoost;
	}

	if (Parent)
	{
		return Parent->GetSpecularBoost();
	}

	return 1.0f;
}

/** @return	The ExportResolutionScale value for this material. */
FLOAT UMaterialInstance::GetExportResolutionScale() const
{
	if (LightmassSettings.bOverrideExportResolutionScale)
	{
		return LightmassSettings.ExportResolutionScale;
	}

	if (Parent)
	{
		return Parent->GetExportResolutionScale();
	}

	return 1.0f;
}

FLOAT UMaterialInstance::GetDistanceFieldPenumbraScale() const
{
	if (LightmassSettings.bOverrideDistanceFieldPenumbraScale)
	{
		return LightmassSettings.DistanceFieldPenumbraScale;
	}

	if (Parent)
	{
		return Parent->GetDistanceFieldPenumbraScale();
	}

	return 1.0f;
}

/**
 *	Get all of the textures in the expression chain for the given property (ie fill in the given array with all textures in the chain).
 *
 *	@param	InProperty				The material property chain to inspect, such as MP_DiffuseColor.
 *	@param	OutTextures				The array to fill in all of the textures.
 *	@param	OutTextureParamNames	Optional array to fill in with texture parameter names.
 *	@param	InStaticParameterSet	Optional static parameter set - if supplied only walk the StaticSwitch branches according to it.
 *
 *	@return	UBOOL			TRUE if successful, FALSE if not.
 */
UBOOL UMaterialInstance::GetTexturesInPropertyChain(EMaterialProperty InProperty, TArray<UTexture*>& OutTextures,  
	TArray<FName>* OutTextureParamNames, class FStaticParameterSet* InStaticParameterSet)
{
	if (Parent != NULL)
	{
		TArray<FName> LocalTextureParamNames;
		UBOOL bResult = Parent->GetTexturesInPropertyChain(InProperty, OutTextures, &LocalTextureParamNames, InStaticParameterSet);
		if (LocalTextureParamNames.Num() > 0)
		{
			// Check textures set in parameters as well...
			for (INT TPIdx = 0; TPIdx < LocalTextureParamNames.Num(); TPIdx++)
			{
				UTexture* ParamTexture = NULL;
				if (GetTextureParameterValue(LocalTextureParamNames(TPIdx), ParamTexture) == TRUE)
				{
					if (ParamTexture != NULL)
					{
						OutTextures.AddUniqueItem(ParamTexture);
					}
				}

				if (OutTextureParamNames != NULL)
				{
					OutTextureParamNames->AddUniqueItem(LocalTextureParamNames(TPIdx));
				}
			}
		}
		return bResult;
	}
	return FALSE;
}
