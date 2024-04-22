/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"

static USpriteComponent* GetSpriteComponentFromActor( ALight* LightParent )
{
	USpriteComponent* Retval = NULL;
	for( INT CompIdx = 0; CompIdx < LightParent->Components.Num(); ++CompIdx )
	{
		UComponent* AComp = LightParent->Components(CompIdx);

		// now check to see if it actually is a SpriteComponent
		if( ( AComp != NULL ) && ( AComp->IsA(USpriteComponent::StaticClass()) == TRUE ) )
		{
			Retval = Cast<USpriteComponent>(AComp);
			break;
		}
	}
	return Retval;
}

static void SetIcon( USpriteComponent* SpriteComp, const FString& IconName )
{
	if( SpriteComp != NULL )
	{
		SpriteComp->Sprite = LoadObject<UTexture2D>(NULL, *IconName, NULL, LOAD_None, NULL);
	}
}

/** The quality level to use for half-resolution lightmaps (not exposed)		*/
ELightingBuildQuality FLightingBuildOptions::HalfResolutionLightmapQualityLevel = Quality_Medium;

/**
 * @return TRUE if the lighting should be built for the level, given the current set of lighting build options.
 */
UBOOL FLightingBuildOptions::ShouldBuildLightingForLevel(ULevel* Level) const
{
	if ( bOnlyBuildCurrentLevel )
	{
		// Reject non-current levels.
		if ( Level != GWorld->CurrentLevel )
		{
			return FALSE;
		}
	}
	else if ( bOnlyBuildSelectedLevels )
	{
		// Reject unselected levels.
		if ( !SelectedLevels.ContainsItem( Level ) )
		{
			return FALSE;
		}
	}

	// Reject NULL levels.
	return Level != NULL;
}

void ALight::DetermineAndSetEditorIcon()
{
}

void ALight::SetValuesForLight_DynamicAffecting()
{
	LightComponent->Modify();

	LightComponent->LightAffectsClassification = LAC_DYNAMIC_AFFECTING;

	LightComponent->CastShadows        = TRUE;
	LightComponent->CastStaticShadows  = FALSE;
	LightComponent->CastDynamicShadows = TRUE;
	LightComponent->bForceDynamicLight = FALSE;
	LightComponent->UseDirectLightMap  = FALSE;
		 
	LightComponent->LightingChannels.ClearAllChannels();
	LightComponent->LightingChannels.Dynamic	= TRUE;
}

void ALight::SetValuesForLight_StaticAffecting()
{
	LightComponent->Modify();

	LightComponent->LightAffectsClassification = LAC_STATIC_AFFECTING;

	LightComponent->CastShadows        = TRUE;
	LightComponent->CastStaticShadows  = TRUE;
	LightComponent->CastDynamicShadows = FALSE;
	LightComponent->bForceDynamicLight = FALSE;
	LightComponent->UseDirectLightMap  = TRUE;
	
	LightComponent->LightingChannels.ClearAllChannels();
	LightComponent->LightingChannels.BSP		= TRUE;
	LightComponent->LightingChannels.Static		= TRUE;
}

void ALight::SetValuesForLight_DynamicAndStaticAffecting()
{
	LightComponent->Modify();

	LightComponent->LightAffectsClassification = LAC_DYNAMIC_AND_STATIC_AFFECTING;

	LightComponent->CastShadows        = TRUE;
	LightComponent->CastStaticShadows  = TRUE;
	LightComponent->CastDynamicShadows = TRUE;
	LightComponent->bForceDynamicLight = FALSE;
	LightComponent->UseDirectLightMap  = FALSE;
	
	LightComponent->LightingChannels.ClearAllChannels();
	LightComponent->LightingChannels.BSP		= TRUE;
	LightComponent->LightingChannels.Static		= TRUE;
	LightComponent->LightingChannels.Dynamic	= TRUE;
}

void ALight::InvalidateLightingForRebuild(UBOOL bOnlyVisible)
{
	// Make a copy of AllComponents as calling InvalidateLightingCacheInner on some components will cause them to reattach, modifying AllComponents
	TArray<UActorComponent*> TempAllComponents = AllComponents;
	for(INT ComponentIndex = 0;ComponentIndex < TempAllComponents.Num();ComponentIndex++)
	{
		if(TempAllComponents(ComponentIndex))
		{
			ULightComponent* LightComponent = Cast<ULightComponent>(TempAllComponents(ComponentIndex));
			if (LightComponent)
			{
				// Don't regenerate light guids, since that would modify the light,
				// And cause hidden levels affected by this light to have uncached light interactions.
				LightComponent->InvalidateLightingCacheInner(FALSE, bOnlyVisible);
			}
		}
	}
}

void ADirectionalLight::DetermineAndSetEditorIcon()
{
	USpriteComponent* SpriteComp = GetSpriteComponentFromActor( this );

	switch( LightComponent->LightAffectsClassification )
	{
	case LAC_USER_SELECTED:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Directional_Stationary_UserSelected") );
		break;
	case LAC_DYNAMIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Directional_Stationary_Dynamics") );
		break;
	case LAC_STATIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Directional_Stationary_Statics") );
		break;
	case LAC_DYNAMIC_AND_STATIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Directional_Stationary_DynamicsAndStatics") );
		break;
	default:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Directional_Stationary_UserSelected") );
		break;
	}
}

/**
 * Called from within SpawnActor, setting up the default value for the Lightmass light source angle.
 */
void ADirectionalLight::Spawned()
{
	Super::Spawned();

	// Set the default value for the LightSourceAngle.
	// See LightmassPointLightSettings in EngineTypes.uc for an explanation.
	UDirectionalLightComponent* DirLightComp = Cast<UDirectionalLightComponent>(LightComponent);
	if (DirLightComp && !LightComponent->IsA(UDominantDirectionalLightComponent::StaticClass()))
	{
		DirLightComp->LightmassSettings.LightSourceAngle = 1.0f;
	}
}

void ADirectionalLightToggleable::DetermineAndSetEditorIcon()
{
	USpriteComponent* SpriteComp = GetSpriteComponentFromActor( this );

	switch( LightComponent->LightAffectsClassification )
	{
	case LAC_USER_SELECTED:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Directional_Toggleable_UserSelected") );
		break;
	case LAC_DYNAMIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Directional_Toggleable_Dynamics") );
		break;
	case LAC_STATIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Directional_Toggleable_Statics") );
		break;
	case LAC_DYNAMIC_AND_STATIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Directional_Toggleable_DynamicsAndStatics") );
		break;
	default:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Directional_Toggleable_UserSelected") );
		break;
	}
}

void ADirectionalLightToggleable::SetValuesForLight_StaticAffecting()
{
	Super::SetValuesForLight_StaticAffecting();
	LightComponent->UseDirectLightMap = FALSE;
}

void APointLight::DetermineAndSetEditorIcon()
{
	USpriteComponent* SpriteComp = GetSpriteComponentFromActor( this );

	switch( LightComponent->LightAffectsClassification )
	{
	case LAC_USER_SELECTED:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Point_Stationary_UserSelected") );
		break;
	case LAC_DYNAMIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Point_Toggleable_Dynamics") );
		break;
	case LAC_STATIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Point_Stationary_Statics") );
		break;
	case LAC_DYNAMIC_AND_STATIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Point_Stationary_DynamicsAndStatics") );
		break;
	default:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Point_Stationary_UserSelected") );
		break;
	}
}

void APointLightMovable::DetermineAndSetEditorIcon()
{
	USpriteComponent* SpriteComp = GetSpriteComponentFromActor( this );

	switch( LightComponent->LightAffectsClassification )
	{
	case LAC_USER_SELECTED:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Point_Moveable_UserSelected") );
		break;
	case LAC_DYNAMIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Point_Moveable_Dynamics") );
		break;
	case LAC_STATIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Point_Moveable_Statics") );
		break;
	case LAC_DYNAMIC_AND_STATIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Point_Moveable_DynamicsAndStatics") );
		break;
	default:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Point_Moveable_UserSelected") );
		break;
	}
}

void APointLightToggleable::DetermineAndSetEditorIcon()
{
	USpriteComponent* SpriteComp = GetSpriteComponentFromActor( this );

	switch( LightComponent->LightAffectsClassification )
	{
	case LAC_USER_SELECTED:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Point_Toggleable_UserSelected") );
		break;
	case LAC_DYNAMIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Point_Toggleable_Dynamics") );
		break;
	case LAC_STATIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Point_Toggleable_Statics") );
		break;
	case LAC_DYNAMIC_AND_STATIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Point_Toggleable_DynamicsAndStatics") );
		break;
	default:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Point_Toggleable_UserSelected") );
		break;
	}
}

void APointLightToggleable::SetValuesForLight_StaticAffecting()
{
	Super::SetValuesForLight_StaticAffecting();
	LightComponent->UseDirectLightMap = FALSE;
}

void ASpotLight::DetermineAndSetEditorIcon()
{
	USpriteComponent* SpriteComp = GetSpriteComponentFromActor( this );

	switch( LightComponent->LightAffectsClassification )
	{
	case LAC_USER_SELECTED:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Spot_Stationary_UserSelected") );
		break;
	case LAC_DYNAMIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Spot_Stationary_Dynamics") );
		break;
	case LAC_STATIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Spot_Stationary_Statics") );
		break;
	case LAC_DYNAMIC_AND_STATIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Spot_Stationary_DynamicsAndStatics") );
		break;
	default:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Spot_Stationary_UserSelected") );
		break;
	}
}

/**
 * Called after this actor has been pasted into a level.  Attempts to catch cases where designers are pasting in really old
 * T3D data that was created when component instancing wasn't working correctly.
 */
void ASpotLight::PostEditImport()
{
	Super::PostEditImport();

	if ( GIsEditor == TRUE && !IsTemplate(RF_ClassDefaultObject) )
	{
		// somehow SpotLightComponents are ending up with invalid SpotLightConeComponents (the PreviewInnerCone/PreviewOuterCone should be the same object that is in this
		// light's components array, but for some SLC's it's not... I suspect that this is caused by pasting in T3D text that is very old (created when 
		// component instancing was broken), so try to detect when this occurs
		USpotLightComponent* SLC = Cast<USpotLightComponent>(LightComponent);
		if ( SLC != NULL )
		{
			ASpotLight* LightArchetype = GetArchetype<ASpotLight>();
			check(LightArchetype);

			USpotLightComponent* SLConeArchetype = Cast<USpotLightComponent>(LightArchetype->LightComponent);
			check(SLConeArchetype);
			check(SLConeArchetype->PreviewInnerCone);
			check(SLConeArchetype->PreviewOuterCone);
			if ( SLC->PreviewInnerCone != NULL && (SLC->PreviewInnerCone->GetOuter() != this || !Components.ContainsItem(SLC->PreviewInnerCone)) )
			{
				debugf(NAME_Error, TEXT("The PreviewInnerCone for %s is NOT contained in the components array for the light (this is bad).  Please let Ron know what you did to get this error message!"), *SLC->PreviewInnerCone->GetFullName());
				appMsgf(AMT_OK, TEXT("The PreviewInnerCone for %s is NOT contained in the components array for the light (this is bad).  Please let Ron know what you did to get this error message! (this message has also been written to the log)."), *SLC->PreviewInnerCone->GetFullName());

				// attempt to correct this problem
				for ( INT ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++ )
				{
					UDrawLightConeComponent* ConeComp = Cast<UDrawLightConeComponent>(Components(ComponentIndex));
					if ( ConeComp != NULL && ConeComp->GetArchetype() == SLConeArchetype->PreviewInnerCone)
					{
						// it's possible that the DrawLightRadiusComponent is the one that was serialized as the value for this PLC's PreviewLightRadius;
						// if so, then we need to rename it into the correct Outer
						if ( SLC->PreviewInnerCone == ConeComp )
						{
							Modify(TRUE);
							debugf(TEXT("Correcting Outer for PreviewLightRadius component '%s'."), *SLC->PreviewInnerCone->GetFullName());
							ConeComp->Rename(NULL, this, REN_ForceNoResetLoaders);
							break;
						}
						else
						{
							debugf(TEXT("Resetting invalid PreviewLightRadius reference for '%s': %s.  Package should be resaved to reduce load times."), *GetFullName(), *ConeComp->GetFullName());
							Modify(TRUE);
							SLC->PreviewInnerCone = ConeComp;
							break;
						}
					}
				}
			}

			if ( SLC->PreviewOuterCone != NULL && (SLC->PreviewOuterCone->GetOuter() != this || !Components.ContainsItem(SLC->PreviewOuterCone)) )
			{
				debugf(NAME_Error, TEXT("The PreviewOuterCone for %s is NOT contained in the components array for the light (this is bad).  Please let Ron know what you did to get this error message!"), *SLC->PreviewOuterCone->GetFullName());
				appMsgf(AMT_OK, TEXT("The PreviewOuterCone for %s is NOT contained in the components array for the light (this is bad).  Please let Ron know what you did to get this error message! (this message has also been written to the log)."), *SLC->PreviewOuterCone->GetFullName());

				// attempt to correct this problem
				for ( INT ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++ )
				{
					UDrawLightConeComponent* ConeComp = Cast<UDrawLightConeComponent>(Components(ComponentIndex));
					if ( ConeComp != NULL && ConeComp->GetArchetype() == SLConeArchetype->PreviewOuterCone )
					{
						// it's possible that the DrawLightRadiusComponent is the one that was serialized as the value for this PLC's PreviewLightRadius;
						// if so, then we need to rename it into the correct Outer
						if ( SLC->PreviewOuterCone == ConeComp )
						{
							Modify(TRUE);
							debugf(TEXT("Correcting Outer for PreviewLightRadius component '%s'."), *SLC->PreviewOuterCone->GetFullName());
							ConeComp->Rename(NULL, this, REN_ForceNoResetLoaders);
							break;
						}
						else
						{
							debugf(TEXT("Resetting invalid PreviewLightRadius reference for '%s': %s.  Package should be resaved to reduce load times."), *GetFullName(), *ConeComp->GetFullName());
							Modify(TRUE);
							SLC->PreviewOuterCone = ConeComp;
							break;
						}
					}
				}
			}
		}
	}
}

void ASpotLightMovable::DetermineAndSetEditorIcon()
{
	USpriteComponent* SpriteComp = GetSpriteComponentFromActor( this );

	switch( LightComponent->LightAffectsClassification )
	{
	case LAC_USER_SELECTED:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Spot_Moveable_UserSelected") );
		break;
	case LAC_DYNAMIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Spot_Moveable_Dynamics") );
		break;
	case LAC_STATIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Spot_Moveable_Statics") );
		break;
	case LAC_DYNAMIC_AND_STATIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Spot_Moveable_DynamicsAndStatics") );
		break;
	default:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Spot_Moveable_UserSelected") );
		break;
	}
}

void ASpotLightToggleable::DetermineAndSetEditorIcon()
{
	USpriteComponent* SpriteComp = GetSpriteComponentFromActor( this );

	switch( LightComponent->LightAffectsClassification )
	{
	case LAC_USER_SELECTED:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Spot_Toggleable_UserSelected") );
		break;
	case LAC_DYNAMIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Spot_Toggleable_Dynamics") );
		break;
	case LAC_STATIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Spot_Toggleable_Statics") );
		break;
	case LAC_DYNAMIC_AND_STATIC_AFFECTING:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Spot_Toggleable_DynamicsAndStatics") );
		break;
	default:
		SetIcon( SpriteComp, TEXT("EditorResources.LightIcons.Light_Spot_Toggleable_UserSelected") );
		break;
	}
}

void ASpotLightToggleable::SetValuesForLight_StaticAffecting()
{
	Super::SetValuesForLight_StaticAffecting();
	LightComponent->UseDirectLightMap = FALSE;
}

/**
 * Called after this actor has been pasted into a level.  Attempts to catch cases where designers are pasting in really old
 * T3D data that was created when component instancing wasn't working correctly.
 */
void APointLight::PostEditImport()
{
	Super::PostEditImport();

	if ( GIsEditor == TRUE && !IsTemplate(RF_ClassDefaultObject) )
	{
		// somehow PointLightComponents are ending up with invalid PreviewLightRadius (the PreviewLightRadius should be the same object that is in this
		// light's components array, but for some PLC's it's not... I suspect that this is caused by pasting in T3D text that is very old (created when 
		// component instancing was broken), so try to detect when this occurs
		UPointLightComponent* PLC = Cast<UPointLightComponent>(LightComponent);
		if ( PLC != NULL && PLC->PreviewLightRadius != NULL && PLC->PreviewLightRadius->GetOuter() != this )
		{
			debugf(NAME_Error, TEXT("The PreviewLightRadius for %s is NOT contained in the components array for the light (this is bad).  Please let Ron know what you did to get this error message!"), *PLC->PreviewLightRadius->GetFullName());
			appMsgf(AMT_OK, TEXT("The PreviewLightRadius for %s is NOT contained in the components array for the light (this is bad).  Please let Ron know what you did to get this error message! (this message has also been written to the log)."), *PLC->PreviewLightRadius->GetFullName());

			// attempt to correct this problem
			for ( INT ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++ )
			{
				UDrawLightRadiusComponent* LightRadiusComp = Cast<UDrawLightRadiusComponent>(Components(ComponentIndex));
				if ( LightRadiusComp != NULL )
				{
					// it's possible that the DrawLightRadiusComponent is the one that was serialized as the value for this PLC's PreviewLightRadius;
					// if so, then we need to rename it into the correct Outer
					if ( PLC->PreviewLightRadius == LightRadiusComp )
					{
						Modify(TRUE);
						debugf(TEXT("Correcting Outer for PreviewLightRadius component '%s'."), *PLC->PreviewLightRadius->GetFullName());
						LightRadiusComp->Rename(NULL, this, REN_ForceNoResetLoaders);
						break;
					}
					else
					{
						debugf(TEXT("Resetting invalid PreviewLightRadius reference for '%s': %s.  Package should be resaved to reduce load times."), *GetFullName(), *LightRadiusComp->GetFullName());
						Modify(TRUE);
						PLC->PreviewLightRadius = LightRadiusComp;
						break;
					}
				}
			}
		}
	}
}

/**
 * Called from within SpawnActor, setting up the default value for the Lightmass light source radius.
 */
void APointLight::Spawned()
{
	Super::Spawned();

	// Set the default value for the LightSourceRadius.
	// See LightmassPointLightSettings in EngineTypes.uc for an explanation.
	UPointLightComponent* PtLightComp = Cast<UPointLightComponent>(LightComponent);
	if (PtLightComp)
	{
		PtLightComp->LightmassSettings.LightSourceRadius = 32.0f;
	}
}

/**
 * Called from within SpawnActor, setting up the default value for the Lightmass light source radius.
 */
void ASpotLight::Spawned()
{
	Super::Spawned();

	// Set the default value for the LightSourceRadius.
	// See LightmassPointLightSettings in EngineTypes.uc for an explanation.
	USpotLightComponent* SpotLightComp = Cast<USpotLightComponent>(LightComponent);
	if (SpotLightComp)
	{
		SpotLightComp->LightmassSettings.LightSourceRadius = 32.0f;
	}
}

/* ==========================================================================================================
	AStaticLightCollectionActor
========================================================================================================== */
/* === AActor interface === */
/**
 * Updates the LightToWorld transform for all attached components.
 */
void AStaticLightCollectionActor::UpdateComponentsInternal( UBOOL bCollisionUpdate/*=FALSE*/ )
{
	checkf(!HasAnyFlags(RF_Unreachable), TEXT("%s"), *GetFullName());
	checkf(!HasAnyFlags(RF_ArchetypeObject|RF_ClassDefaultObject), TEXT("%s"), *GetFullName());
	checkf(!ActorIsPendingKill(), TEXT("%s"), *GetFullName());

	// the only components that can be attached to this actor are LightComponents, so update them all regardless
	// of whether this is a collision update or not.
	for(INT ComponentIndex = 0;ComponentIndex < Components.Num();ComponentIndex++)
	{
		if( Components(ComponentIndex) != NULL )
		{
			ULightComponent* LightComp = CastChecked<ULightComponent>(Components(ComponentIndex));

			// never reapply the CachedParentToWorld transform for our LightComponents, since it will never change.
			UPointLightComponent* PLC = Cast<UPointLightComponent>(LightComp);
			if ( PLC != NULL )
			{
				PLC->UpdateComponent(GWorld->Scene, this, PLC->CachedParentToWorld);
			}
			else
			{
				// ULightComponent::SetParentToWorld multiplies the ParentToWorld by a matrix which flips the X and Z
				// axis values, so in order for the component's final LightToWorld to remain the same, we'll need to
				// flip the current value here so that when it's flipped in SetParentToWorld it ends up the correct value.
				static FMatrix ReverseZAxisMat = 
				FMatrix(
					FPlane(+0,+0,+1,+0),
					FPlane(+0,+1,+0,+0),
					FPlane(+1,+0,+0,+0),
					FPlane(+0,+0,+0,+1)
					);

 				LightComp->UpdateComponent(GWorld->Scene, this, ReverseZAxisMat * LightComp->LightToWorld );
			}
		}
	}
}


/* === UObject interface === */
/**
 * Serializes the LocalToWorld transforms for the StaticMeshComponents contained in this actor.
 */
void AStaticLightCollectionActor::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);

	if (!HasAnyFlags(RF_ClassDefaultObject) && Ar.GetLinker() != NULL )
	{
		if ( Ar.IsLoading() )
		{
			FMatrix LightToWorldMatrix;
			for ( INT CompIndex = 0; CompIndex < LightComponents.Num(); CompIndex++ )
			{
				// even if we had a NULL component for whatever reason, we still need to read the matrix data
				// from the stream so that we de-serialize the same amount of data that was serialized.
				Ar << LightToWorldMatrix;
				if ( LightComponents(CompIndex) != NULL )
				{
					LightComponents(CompIndex)->ConditionalUpdateTransform(LightToWorldMatrix);
				}
			}

			Components = (TArrayNoInit<UActorComponent*>&)LightComponents;
			LightComponents.Empty();
		}
		else if ( Ar.IsSaving() )
		{
			check(GIsCooking);

			// serialize the default matrix for any components which are NULL so that we are always guaranteed to
			// de-serialize the correct amount of data
			FMatrix IdentityMatrix(FMatrix::Identity);
			for ( INT CompIndex = 0; CompIndex < LightComponents.Num(); CompIndex++ )
			{
				if ( LightComponents(CompIndex) != NULL )
				{
					// the cooker sets the LightComponent's LightToWorld to the actor's LocalToWorld() directly, so we don't have to worry
					// about extracting the addition matrix applied to LightComponent transforms (@see ULightComponent::SetParentToWorld)
					Ar << LightComponents(CompIndex)->LightToWorld;
				}
				else
				{
					Ar << IdentityMatrix;
				}
			}
		}
	}
}
IMPLEMENT_CLASS(AStaticLightCollectionActor);


// EOF



