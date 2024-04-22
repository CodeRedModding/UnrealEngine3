/*=============================================================================
	LightComponent.cpp: LightComponent implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "EngineLightClasses.h"

IMPLEMENT_CLASS(ULightComponent);
IMPLEMENT_CLASS(ULightFunction);

/**
 * Updates/ resets light GUIDs.
 */
void ULightComponent::UpdateLightGUIDs()
{
	LightGuid		= appCreateGuid();
	LightmapGuid	= appCreateGuid();
	MarkLightingRequiringRebuild();
}

/**
 * Validates light GUIDs and resets as appropriate.
 */
void ULightComponent::ValidateLightGUIDs()
{
	// Validate light guids.
	if( !LightGuid.IsValid() )
	{
		LightGuid = appCreateGuid();
	}
	if( !LightmapGuid.IsValid() )
	{
		LightmapGuid = appCreateGuid();
	}
}

UBOOL ULightComponent::AffectsPrimitive(const UPrimitiveComponent* Primitive, UBOOL bCompareLightingChannels) const
{
	ULightEnvironmentComponent* PrimitiveLightEnvironment = Primitive->LightEnvironment;
	if(PrimitiveLightEnvironment && !PrimitiveLightEnvironment->IsEnabled())
	{
		PrimitiveLightEnvironment = NULL;
	}
	if(PrimitiveLightEnvironment != LightEnvironment)
	{
		return FALSE;
	}

	if( bCompareLightingChannels && !LightingChannels.OverlapsWith( Primitive->LightingChannels ) )
	{
		return FALSE;
	}

	if(!Primitive->bAcceptsLights)
	{
		return FALSE;
	}

	if( !Primitive->bAcceptsDynamicLights && !HasStaticShadowing() )
	{
		return FALSE;
	}

	// If the primitive has an OverrideLightComponent specified, only that light can affect it
	if (Primitive->OverrideLightComponent && Primitive->OverrideLightComponent != this
		|| !Primitive->OverrideLightComponent && bExplicitlyAssignedLight)
	{
		return FALSE;
	}

	// Check whether the light affects the primitive's bounding volume.
	return AffectsBounds(Primitive->Bounds);
}

UBOOL ULightComponent::AffectsBounds(const FBoxSphereBounds& Bounds) const
{
	return TRUE;
}

UBOOL ULightComponent::IsShadowCast(UPrimitiveComponent* Primitive) const
{
	if(Primitive->HasStaticShadowing())
	{
		return CastShadows && CastStaticShadows;
	}
	else
	{
		return CastShadows && CastDynamicShadows;
	}
}

UBOOL ULightComponent::HasStaticShadowing() const
{
	// Skylights don't support non-light-mapped static shadowing.
	const UBOOL bSupportsStaticShadowing =
		IsA(USkyLightComponent::StaticClass()) ?
			HasStaticLighting() :
			TRUE;

	return (!Owner || Owner->HasStaticShadowing()) && !bForceDynamicLight && !LightEnvironment && bSupportsStaticShadowing;
}

UBOOL ULightComponent::HasProjectedShadowing() const
{
	return (!Owner || Owner->HasStaticShadowing()) && !bForceDynamicLight;
}

UBOOL ULightComponent::HasStaticLighting() const
{
	return (!Owner || Owner->IsStatic()) && !Function && !bForceDynamicLight && !LightEnvironment;
}

/**
 * Returns whether static lighting, aka lightmaps, is being used for primitive/ light
 * interaction.
 *
 * @param bForceDirectLightMap	Whether primitive is set to force lightmaps
 * @return TRUE if lightmaps/ static lighting is being used, FALSE otherwise
 */
UBOOL ULightComponent::UseStaticLighting( UBOOL bForceDirectLightMap ) const
{
	return HasStaticLighting() && (UseDirectLightMap || bForceDirectLightMap || !HasStaticShadowing());
}

//
//	ULightComponent::SetParentToWorld
//

void ULightComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	FMatrix OrthonormalParentToWorld = ParentToWorld;
	OrthonormalParentToWorld.RemoveScaling();

	const FVector LightOrigin = OrthonormalParentToWorld.InverseTransformFVectorNoScale(FVector4(0,0,0));

	static const FMatrix LightToParent(
		FPlane(+0,+0,+1,+0),
		FPlane(+0,+1,+0,+0),
		FPlane(+1,+0,+0,+0),
		FPlane(+0,+0,+0,+1)
		);
	static const FMatrix ParentToLight = LightToParent.Inverse();

	LightToWorld = LightToParent * OrthonormalParentToWorld;
	WorldToLight = FTranslationMatrix(LightOrigin) * OrthonormalParentToWorld.RemoveTranslation().Inverse() * ParentToLight;
}


UBOOL ULightComponent::IsLACDynamicAffecting()
{
	UBOOL Retval = FALSE;

	if(	   ( CastShadows				== TRUE )
		&& ( CastStaticShadows			== FALSE )
		&& ( CastDynamicShadows			== TRUE )
		&& ( bForceDynamicLight			== FALSE )
		&& ( UseDirectLightMap			== FALSE )

		&& ( LightingChannels.BSP		== FALSE )
		&& ( LightingChannels.Dynamic	== TRUE )
		&& ( LightingChannels.Static	== FALSE )
		)
	{
		Retval = TRUE;
	}

	return Retval;
}


UBOOL ULightComponent::IsLACStaticAffecting()
{
	UBOOL Retval = FALSE;

	// here we need to check our outer and see if it is a light 
	// and if it is a toggleable light
	UBOOL bIsToggleableLight = FALSE;
	ALight* LightActor = Cast<ALight>(GetOuter());

	if( LightActor != NULL )
	{
		// now check to see if it is a toggleable light.  We can can just check the flags of the
		// the LightActor to see if it has the "toggleable" set.    Could also cast each to the 
		// three types of lights we have and check those

		if( ( LightActor->bMovable == FALSE )
			&& (LightActor->IsStatic() == FALSE )
			&& ( LightActor->bHardAttach == TRUE )
			)
		{
			bIsToggleableLight = TRUE;
		}
	}

	if( ( CastShadows == TRUE )
		&& ( CastStaticShadows == TRUE )
		&& ( CastDynamicShadows == FALSE )
		&& ( bForceDynamicLight == FALSE )

		// not certain we need to have the UseDirectLightMap check here?   should be an optional 
		// as objects can be forced lightmapped (tho that is the "Wrong" way to do things
		&& ( ( ( bIsToggleableLight == FALSE ) && ( UseDirectLightMap == TRUE ) )
		   || ( ( bIsToggleableLight == TRUE ) && ( UseDirectLightMap == FALSE ) )// toggleable lights can't have direct light maps
		   )

		   // Lots of wonderful Lighting Channels exist so S lights are basically any light
		   // that meets the above values  And doesn't have the dynamic channel set
		&& ( LightingChannels.BSP == TRUE )
		&& ( LightingChannels.Dynamic == FALSE )
		&& ( LightingChannels.Static == TRUE )
		)
	{
		Retval = TRUE;
	}

	return Retval;
}


UBOOL ULightComponent::IsLACDynamicAndStaticAffecting()
{
	UBOOL Retval = FALSE;

	if( ( CastShadows == TRUE )
		&& ( CastStaticShadows == TRUE )
		&& ( CastDynamicShadows == TRUE )
		&& ( bForceDynamicLight == FALSE )
		&& ( UseDirectLightMap == FALSE )

		&& ( LightingChannels.BSP == TRUE )
		&& ( LightingChannels.Dynamic == TRUE )
		&& ( LightingChannels.Static == TRUE )
		)
	{
		Retval = TRUE;
	}

	return Retval;
}


/**
 * Called after this UObject has been serialized
 */
void ULightComponent::PostLoad()
{
	Super::PostLoad();

	if (IsDominantLightType(GetLightType()))
	{
		// Not supported for dominant lights as the shaders required to handle a lightmapped object and a dynamic light in the base pass are not compiled
		bForceDynamicLight = FALSE;
		// Dominant lights must cast normal shadows
		LightShadowMode = LightShadow_Normal;
	}

	if (LightShadowMode == LightShadow_ModulateBetter)
	{
		LightShadowMode = LightShadow_Modulate;
	}

	if ( Function != NULL && Function->GetOuter() != this && !IsTemplate() )
	{
		// this is the culprit behind PointLightComponents that have a NULL PreviewLightRadius;  basically, this PLC's Function is pointing to the Function object owned by a
		// PLC that has been removed from the level....fix up these guys now
		ULightFunction* NewFunction = Cast<ULightFunction>(StaticDuplicateObject(Function, Function, this, *Function->GetName()));
		if ( NewFunction != NULL )
		{
			debugf(NAME_Warning, TEXT("Invalid LightFunction detected for %s: %s.  Replacing existing value with fresh copy %s"), *GetFullName(), *Function->GetFullName(), *NewFunction->GetFullName());
			Function = NewFunction;
		}
	}

	if (UseDirectLightMap)
	{
		// Light functions are only allowed on lights which render their direct lighting dynamically
		Function = FALSE;
	}

	// so we have loaded up a map we want to make certain all of the lights have the most
    // recent light classification icon
	SetLightAffectsClassificationBasedOnSettings();
}

void ULightComponent::PreEditUndo()
{
	// Directly call UActorComponent::PreEditChange to avoid ULightComponent::PreEditChange being called for transactions.
	UActorComponent::PreEditChange(NULL);
}

void ULightComponent::PostEditUndo()
{
	// Directly call UActorComponent::PostEditChange to avoid ULightComponent::PostEditChange being called for transactions.
	UActorComponent::PostEditChange();
}

void ULightComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	// depending on what has changed we will possibly change the icon on the light
	const FName PropertyName = PropertyThatChanged ? PropertyThatChanged->GetFName() : NAME_None;
#if CONSOLE
	const FName PropertyCategory = NAME_None;
#else
	const FName PropertyCategory = PropertyThatChanged ? PropertyThatChanged->Category : NAME_None;
#endif

	MinShadowResolution = Max(MinShadowResolution,0);
	MaxShadowResolution = Max(MaxShadowResolution,0);

	OcclusionDepthRange = Max(OcclusionDepthRange, 1.0f);
	BloomScale = Clamp(BloomScale, 0.0f, 100.0f);
	RadialBlurPercent = Clamp(RadialBlurPercent, 15.0f, 100.0f);
	OcclusionMaskDarkness = Clamp(OcclusionMaskDarkness, 0.0f, 1.0f);

	if (IsDominantLightType(GetLightType()))
	{
		// Not supported for dominant lights as the shaders required to handle a lightmapped object and a dynamic light in the base pass are not compiled
		bForceDynamicLight = FALSE;
		// Dominant lights must cast normal shadows
		LightShadowMode = LightShadow_Normal;
	}

	// Don't allow deprecated settings
	if (LightShadowMode == LightShadow_ModulateBetter)
	{
		LightShadowMode = LightShadow_Modulate;
	}

	// Light functions are only allowed on lights that render their direct lighting dynamically
	if (UseDirectLightMap)
	{
		Function = NULL;
	}

	// check for the things we care about for light classification
	if(		( PropertyName == NAME_None )
		||	( PropertyName == TEXT("CastShadows") )
		||	( PropertyName == TEXT("CastStaticShadows") )
		||	( PropertyName == TEXT("CastDynamicShadows") )
		||	( PropertyName == TEXT("BSP") ) 
		||	( PropertyName == TEXT("Dynamic") ) 
		||	( PropertyName == TEXT("Static") ) 
		)
	{
		SetLightAffectsClassificationBasedOnSettings();
	}

	// Check for properties that don't require any static lighting to be rebuilt.
	if( PropertyName != TEXT("CastDynamicShadows") &&
		PropertyName != TEXT("bCastCompositeShadow") &&
		PropertyName != TEXT("bAffectCompositeShadowDirection") &&
		PropertyName != TEXT("LightShadowMode") &&
		PropertyName != TEXT("ModShadowColor") &&
		PropertyName != TEXT("ModShadowFadeoutTime") &&
		PropertyName != TEXT("ModShadowFadeoutExponent") &&
		PropertyName != TEXT("ShadowProjectionTechnique") &&
		PropertyName != TEXT("ShadowFilterQuality") &&
		PropertyName != TEXT("MinShadowResolution") &&
		PropertyName != TEXT("MaxShadowResolution") &&
		PropertyName != TEXT("ShadowFadeResolution") &&
		PropertyName != TEXT("LightSourceAngle") &&
		PropertyName != TEXT("LightSourceRadius") &&
		PropertyName != TEXT("ShadowExponent") &&
		PropertyName != TEXT("ShadowRadiusMultiplier") &&
		PropertyName != TEXT("Function") &&
		PropertyName != TEXT("WholeSceneDynamicShadowRadius") &&
		PropertyName != TEXT("NumWholeSceneDynamicShadowCascades") &&
		PropertyName != TEXT("CascadeDistributionExponent") && 
		PropertyName != TEXT("OcclusionDepthRange") && 
		PropertyName != TEXT("BloomScale") && 
		PropertyName != TEXT("BloomThreshold") && 
		PropertyName != TEXT("BloomScreenBlendThreshold") && 
		PropertyName != TEXT("BloomTint") && 
		PropertyName != TEXT("RadialBlurPercent") && 
		PropertyName != TEXT("OcclusionMaskDarkness") && 
		PropertyName != TEXT("bRenderLightShafts") &&
		PropertyName != TEXT("bUseImageReflectionSpecular")
		)
	{
		// Check for properties that only require light-maps to be rebuilt, and not shadow-maps.
		if(	PropertyName == TEXT("LightColor") ||
			PropertyName == TEXT("Brightness") ||
			PropertyName == TEXT("bEnabled") ||
			// Attempt to detect changes to individual components of LightColor
			// This isn't robust, it will detect changes to individual components of any FColor property
			PropertyCategory == TEXT("Color") && (PropertyName == TEXT("R") || PropertyName == TEXT("G") || PropertyName == TEXT("B") || PropertyName == TEXT("A")))
		{
			// Create a new GUID for lightmap usage.
			LightmapGuid = appCreateGuid();
			// Invalidate just lightmap data.
			InvalidateLightmapData();
			// Mark level as requiring lighting to be rebuilt.
			MarkLightingRequiringRebuild();		
		}
		// If the property isn't explicitly defined as not needing a static lighting rebuild, then discard the light's current
		// static lighting data.
		else
		{
			// Fully invalidate lighting cache.
			InvalidateLightingCache();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/**
 * Serialize function.
 */
void ULightComponent::Serialize(FArchive& Ar)
{
	Super::Serialize( Ar );

	if (Ar.Ver() < VER_REMOVE_UNUSED_LIGHTING_PROPERTIES)
	{
		TArray<FConvexVolume> Dummy;
		Ar << Dummy;
		TArray<FConvexVolume> Dummy2;
		Ar << Dummy2;
	}
}

/**
 * Called after duplication & serialization and before PostLoad. Used to e.g. make sure GUIDs remains globally unique.
 */
void ULightComponent::PostDuplicate()
{
	Super::PostDuplicate();
	// Create new guids for light.
	UpdateLightGUIDs();
	// This is basically a new object so we don't have to worry about invalidating lightmap data.
	bHasLightEverBeenBuiltIntoLightMap	= FALSE;
}

/**
 * Called after importing property values for this object (paste, duplicate or .t3d import)
 * Allow the object to perform any cleanup for properties which shouldn't be duplicated or
 * are unsupported by the script serialization
 */
void ULightComponent::PostEditImport()
{
	Super::PostEditImport();
	// Create new guids for light.
	UpdateLightGUIDs();
	// This is basically a new object so we don't have to worry about invalidating lightmap data.
	bHasLightEverBeenBuiltIntoLightMap	= FALSE;
}

void ULightComponent::SetLightAffectsClassificationBasedOnSettings()
{
	ALight* LightActor = Cast<ALight>(GetOuter());

    // check to make certain out parent is a light otherwise we probably don't want
    // to override the current icon
	// @todo wiiu: Find out why the !IsTemplate() check is needed to not crash
	if( ( LightActor != NULL ) && ( LightActor->LightComponent == this ) && !IsTemplate())
	{
		if( IsLACDynamicAffecting() == TRUE )
		{
			LightAffectsClassification = LAC_DYNAMIC_AFFECTING;
		}
		else if( IsLACStaticAffecting() == TRUE )
		{
			LightAffectsClassification = LAC_STATIC_AFFECTING;
		}
		else if( IsLACDynamicAndStaticAffecting() == TRUE )
		{
			LightAffectsClassification = LAC_DYNAMIC_AND_STATIC_AFFECTING;
		}
		else
		{
			LightAffectsClassification = LAC_USER_SELECTED;
		}

		// DetermineAndSetEditorIcon depends on the LAC being set
		LightActor->DetermineAndSetEditorIcon();
	}
}

/**
 * Update of all light environments in the given light's world.
 */
static void UpdateLightEnvironments(const ULightComponent* Light)
{
	UWorld* World = Light->GetScene()->GetWorld();
	if(World)
	{
		// Make a local copy of the light environment list, since the reattachments will modify the world's list.
		TSparseArray<ULightEnvironmentComponent*> LocalLightEnvironmentList = World->LightEnvironmentList;
		for(TSparseArray<ULightEnvironmentComponent*>::TConstIterator EnvironmentIt(LocalLightEnvironmentList);EnvironmentIt;++EnvironmentIt)
		{
			ULightEnvironmentComponent* LightEnvironmentComponent = *EnvironmentIt;
			if(!LightEnvironmentComponent->HasAnyFlags(RF_Unreachable))
			{
				LightEnvironmentComponent->UpdateLight(Light);
			}
		}
	}
}

/**
 * Adds this light to the world's dynamic or static light list.
 */
void ULightComponent::AddToLightList()
{
	UWorld* World = Scene->GetWorld();

	// Add the light to the world's light set.
	if( World )
	{
		UBOOL bMarkLightEnvironmentsDirty = TRUE;
		if (GetLightType() == LightType_DominantDirectional)
		{
			World->DominantDirectionalLight = CastChecked<UDominantDirectionalLightComponent>(this);
		}
		else if (GetLightType() == LightType_DominantPoint)
		{
			SetStaticLightListIndex(World->DominantPointLights.AddItem(CastChecked<UDominantPointLightComponent>(this)));
		}
		else if (GetLightType() == LightType_DominantSpot)
		{
			SetStaticLightListIndex(World->DominantSpotLights.AddItem(CastChecked<UDominantSpotLightComponent>(this)));
		}
		// Insert the light into the correct list
		else if( HasStaticLighting() )
		{
			SetStaticLightListIndex( World->StaticLightList.AddItem(this) );
		}
		else
		{
			bMarkLightEnvironmentsDirty = FALSE;
			SetDynamicLightListIndex( World->DynamicLightList.AddItem(this) );
		}

		if (bMarkLightEnvironmentsDirty)
		{
			// If we have just added a static light type, mark all light environments as needing a static update, 
			// So they will take the newly added light into account.
			for(TSparseArray<ULightEnvironmentComponent*>::TConstIterator EnvironmentIt(World->LightEnvironmentList);EnvironmentIt;++EnvironmentIt)
			{
				ULightEnvironmentComponent* LightEnvironmentComponent = *EnvironmentIt;
				if (!LightEnvironmentComponent->HasAnyFlags(RF_Unreachable))
				{
					LightEnvironmentComponent->SetNeedsStaticUpdate();
				}
			}
		}
	}
}

void ULightComponent::Attach()
{
	// Update GUIDs on attachment if they are not valid.
	ValidateLightGUIDs();

	Super::Attach();

	if (bEnabled && (!GetOwner() || !GetOwner()->bHiddenEdLevel))
	{
		// Add the light to the scene.
		Scene->AddLight(this);

		if(!LightEnvironment)
		{
			// Add the light to the world's light set.
			AddToLightList();
			
			// In the editor, update light environments to include the changed static lighting.
			if(!GIsGame && GetLightType() != LightType_SphericalHarmonic)
			{
				UpdateLightEnvironments(this);
			}
		}

		if (bUseImageReflectionSpecular)
		{
			Scene->AddImageReflection(this, NULL, 1, FLinearColor(LightColor) * Brightness * ReflectionSpecularBrightness, FALSE, TRUE);
		}
	}
}

void ULightComponent::UpdateTransform()
{
	Super::UpdateTransform();

	// Update the scene info's transform for this light.
	Scene->UpdateLightTransform(this);

	if( bEnabled && (!GetOwner() || !GetOwner()->bHiddenEdLevel) )
	{
		if(!LightEnvironment)
		{
			// Add the light to the world's light set, if it hasn't already been added.
			if( !IsInLightList() )
			{
				AddToLightList();
			}
			
			// In the editor, update light environments to include the changed static lighting.
			if(!GIsGame && GetLightType() != LightType_SphericalHarmonic)
			{
				UpdateLightEnvironments(this);
			}
		}

		if (bUseImageReflectionSpecular)
		{
			Scene->UpdateImageReflection(this, NULL, 1, FLinearColor(LightColor) * Brightness * ReflectionSpecularBrightness, FALSE, TRUE);
		}
	}
}

/**
 * Sets bEnabled and handles updating the component.
 */
void ULightComponent::execSetEnabled(FFrame &Stack,RESULT_DECL)
{
	P_GET_UBOOL(bSetEnabled);
	P_FINISH;
	SetEnabled( bSetEnabled );
}
IMPLEMENT_FUNCTION(ULightComponent,INDEX_NONE,execSetEnabled);



/**
 * Toggles the light on or off
 *
 * @param bSetEnabled TRUE to enable the light or FALSE to disable it
 */
void ULightComponent::SetEnabled( UBOOL bSetEnabled )
{
	if (bEnabled != bSetEnabled)
	{
		bEnabled = bSetEnabled;

		// Request a deferred component update.
		BeginDeferredReattach();
	}
}

void ULightComponent::SetLightProperties(FLOAT NewBrightness, const FColor & NewLightColor, ULightFunction* NewLightFunction)
{
	// If we're not changing anything, then bail out.
	if( Brightness == NewBrightness &&
		LightColor == NewLightColor &&
		Function == NewLightFunction )
	{
		return;
	}

	Brightness	= NewBrightness;
	LightColor	= NewLightColor;

	if (Function == NewLightFunction && !IsA(USkyLightComponent::StaticClass()))
	{
		// Use the lightweight color and brightness update if they are the only properties that have changed
		if( Scene )
		{
			//@todo - remove from scene if brightness or color becomes 0
			Scene->UpdateLightColorAndBrightness( this );
		}
	}
	else
	{		
		Function	= NewLightFunction;

		// Request a deferred component update.
		BeginDeferredReattach();
	}
}

/** Updates the selection state of this light. */
void ULightComponent::UpdateSelection(UBOOL bInSelected)
{
	if (SceneInfo)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			SetNewSelection,
			FLightSceneInfo*,SceneInfo,SceneInfo,
			UBOOL,bNewSelection,bInSelected,
		{
			SceneInfo->bOwnerSelected = bNewSelection;
		});
	}
}

void ULightComponent::UpdateForwardShadowReceivers(const TArray<UPrimitiveComponent*>& Receivers)
{
	if (SceneInfo)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			UpdateReceivers,
			FLightSceneInfo*,SceneInfo,SceneInfo,
			TArray<UPrimitiveComponent*>,NewReceivers,Receivers,
		{
			SceneInfo->ForwardShadowReceivers = NewReceivers;
		});
	}
}

void ULightComponent::execSetLightProperties(FFrame& Stack, RESULT_DECL)
{
	P_GET_FLOAT_OPTX(NewBrightness, Brightness);
	P_GET_STRUCT_OPTX(FColor, NewLightColor, LightColor);
	P_GET_OBJECT_OPTX(ULightFunction, NewLightFunction, Function);
	P_FINISH;

	SetLightProperties(NewBrightness, NewLightColor, NewLightFunction);
}

IMPLEMENT_FUNCTION(ULightComponent,INDEX_NONE,execSetLightProperties);

void ULightComponent::execGetOrigin(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;
	*(FVector*)Result = GetOrigin();
}
IMPLEMENT_FUNCTION(ULightComponent,INDEX_NONE,execGetOrigin);

void ULightComponent::execGetDirection(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;
	*(FVector*)Result = GetDirection();
}
IMPLEMENT_FUNCTION(ULightComponent,INDEX_NONE,execGetDirection);

void ULightComponent::execUpdateColorAndBrightness(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;
	if( Scene )
	{
		Scene->UpdateLightColorAndBrightness( this );
	}
}
IMPLEMENT_FUNCTION(ULightComponent,INDEX_NONE,execUpdateColorAndBrightness);

void ULightComponent::execUpdateLightShaftParameters(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;

	struct FLightShaftParameters
	{
		FLightShaftParameters(const ULightComponent* InComponent) :
			OcclusionDepthRange(InComponent->OcclusionDepthRange),
			BloomScale(InComponent->BloomScale),
			BloomThreshold(InComponent->BloomThreshold),
			BloomScreenBlendThreshold(InComponent->BloomScreenBlendThreshold),
			BloomTint(InComponent->BloomTint),
			RadialBlurPercent(InComponent->RadialBlurPercent),
			OcclusionMaskDarkness(InComponent->OcclusionMaskDarkness)
		{}

		FLOAT OcclusionDepthRange;
		FLOAT BloomScale;
		FLOAT BloomThreshold;
		FLOAT BloomScreenBlendThreshold;
		FColor BloomTint;
		FLOAT RadialBlurPercent;
		FLOAT OcclusionMaskDarkness;
	};
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		UpdateLightShaftParameters,
		FLightSceneInfo*,LightSceneInfo,SceneInfo,
		FLightShaftParameters,Parameters,FLightShaftParameters(this),
	{
		if( LightSceneInfo )
		{
			LightSceneInfo->OcclusionDepthRange = Parameters.OcclusionDepthRange;
			LightSceneInfo->BloomScale = Parameters.BloomScale;
			LightSceneInfo->BloomThreshold = Parameters.BloomThreshold;
			LightSceneInfo->BloomScreenBlendThreshold = Parameters.BloomScreenBlendThreshold;
			LightSceneInfo->BloomTint = Parameters.BloomTint;
			LightSceneInfo->RadialBlurPercent = Parameters.RadialBlurPercent;
			LightSceneInfo->OcclusionMaskDarkness = Parameters.OcclusionMaskDarkness;
		}
	});
}
IMPLEMENT_FUNCTION(ULightComponent,INDEX_NONE,execUpdateLightShaftParameters);

void ULightComponent::Detach( UBOOL bWillReattach )
{
	Super::Detach( bWillReattach );
	Scene->RemoveLight(this);

	UWorld* World = Scene->GetWorld();
	if( World )
	{
		if(!LightEnvironment)
		{
			if (this == World->DominantDirectionalLight)
			{
				World->DominantDirectionalLight = NULL;
			}
			else if (IsInStaticLightList() && GetLightType() == LightType_DominantPoint)
			{
				World->DominantPointLights.Remove( GetLightListIndex() );
			}
			else if (IsInStaticLightList() && GetLightType() == LightType_DominantSpot)
			{
				World->DominantSpotLights.Remove( GetLightListIndex() );
			}
			// Remove light from the correct list.
			else if( IsInDynamicLightList() )
			{
				World->DynamicLightList.Remove( GetLightListIndex() );
			}
			else if( IsInStaticLightList() )
			{
				World->StaticLightList.Remove( GetLightListIndex() );
			}
			// Update light environments to include the updated lighting.
			if(!GIsGame && !GIsPlayInEditorWorld && GetLightType() != LightType_SphericalHarmonic)
			{
				UpdateLightEnvironments(this);
			}
			// Invalidate index.
			InvalidateLightListIndex();
		}
	}

	Scene->RemoveImageReflection(this);
}

//
//	ULightComponent::InvalidateLightingCache
//

void ULightComponent::InvalidateLightingCache()
{
	InvalidateLightingCacheInner(TRUE, FALSE);
}

/** Invalidates the light's cached lighting with the option to recreate the light Guids. */
void ULightComponent::InvalidateLightingCacheInner(UBOOL bRecreateLightGuids, UBOOL bOnlyVisible /* = FALSE */)
{
	// Save the light state for transactions.
	Modify();

	bPrecomputedLightingIsValid = FALSE;

	if (bRecreateLightGuids)
	{
		// Create new guids for light.
		UpdateLightGUIDs();
	}
	else
	{
		ValidateLightGUIDs();
		MarkLightingRequiringRebuild();
	}
	
	// Invalidate lightmap data.
	InvalidateLightmapData(bOnlyVisible);
	// reattach the invalidated component
	// this has to be done after the above calls so that the Owner remains valid
	FComponentReattachContext ReattachContext( this );
}

UBOOL GLightBuildOnlyVisibleLevels = FALSE;

/**
 * Invalidates lightmap data of affected primitives if this light has ever been built 
 * into a lightmap.
 */
void ULightComponent::InvalidateLightmapData(UBOOL bOnlyVisible/*= FALSE*/)
{
	// If the light has been built into any lightmaps, try to invalidate the lightmaps by invalidating the cached lighting for the
	// primitives the light is attached to.  This isn't perfect, but will catch most cases.
	if(bHasLightEverBeenBuiltIntoLightMap)
	{
		bHasLightEverBeenBuiltIntoLightMap = FALSE;

		// Iterate over primitives which the light affects.
		for( TObjectIterator<UPrimitiveComponent> It; It; ++It )
		{
			UPrimitiveComponent* Primitive = *It;

			AActor* Actor = Cast<AActor>(Primitive->GetOuter());
			// If the primitive is attached, affected and the interaction is lightmapped, invalidate the lighting cache
			// of the primitive, which in turn will invalidate its lightmaps.
			if( Primitive->IsAttached() 
			&&	AffectsPrimitive(Primitive, TRUE) 
			&&	UseStaticLighting(Primitive->bForceDirectLightMap) )
			{
				if (!bOnlyVisible || !Actor || !Actor->bHiddenEdLevel)
				{
					// Invalidate the primitive's lighting cache.
					Primitive->InvalidateLightingCache();
				}
			}
		}
	}
}

/**
 * Computes the intensity of the direct lighting from this light on a specific point.
 */
FLinearColor ULightComponent::GetDirectIntensity(const FVector& Point) const 
{ 
	if(bEnabled)
	{
		return FLinearColor(LightColor) * Brightness;
	}
	else
	{
		return FLinearColor::Black;
	}
}

INT ULightComponent::GetNumElements() const
{
	return (Function != NULL) ? 1 : 0;
}

UMaterialInterface* ULightComponent::GetElementMaterial(INT ElementIndex) const
{
	if (ElementIndex == 0)
	{
		return (Function != NULL) ? Function->SourceMaterial : NULL;
	}
	else
	{
		return NULL;
	}
}

void ULightComponent::SetElementMaterial(INT ElementIndex, UMaterialInterface* InMaterial)
{
	if (ElementIndex == 0 && Function != NULL)
	{
		FComponentReattachContext ReattachContext(this);
		Function->SourceMaterial = InMaterial;
	}
}
