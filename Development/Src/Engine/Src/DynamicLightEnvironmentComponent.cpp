/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"
#include "DynamicLightEnvironmentComponent.h"
#include "PrimitiveSceneInfo.h"
#include "EngineParticleClasses.h"

IMPLEMENT_CLASS(UDynamicLightEnvironmentComponent);
IMPLEMENT_CLASS(UParticleLightEnvironmentComponent);

DECLARE_STATS_GROUP(TEXT("DLE"),STATGROUP_DLE);
DECLARE_CYCLE_STAT(TEXT("Particle DLE Tick"),STAT_ParticleDLETickTime,STATGROUP_DLE);
DECLARE_CYCLE_STAT(TEXT("   CreateLights"),STAT_CreateLightsTime,STATGROUP_DLE);
DECLARE_CYCLE_STAT(TEXT("      Light visibility"),STAT_DynamicLightVisibilityTime,STATGROUP_DLE);
DECLARE_CYCLE_STAT(TEXT("   UpdateDynamicEnvironment"),STAT_UpdateDynamicEnvironmentTime,STATGROUP_DLE);
DECLARE_CYCLE_STAT(TEXT("   DominantShadowTransitions"),STAT_DominantShadowTransitions,STATGROUP_DLE);
DECLARE_CYCLE_STAT(TEXT("   UpdateEnvironmentInterpolation"),STAT_UpdateEnvironmentInterpolationTime,STATGROUP_DLE);
DECLARE_CYCLE_STAT(TEXT("      Interpolate indirect"),STAT_InterpolateIndirectTime,STATGROUP_DLE);
DECLARE_CYCLE_STAT(TEXT("      Light visibility"),STAT_StaticLightVisibilityTime,STATGROUP_DLE);
DECLARE_CYCLE_STAT(TEXT("   UpdateStaticEnvironment"),STAT_UpdateStaticEnvironmentTime,STATGROUP_DLE);
DECLARE_CYCLE_STAT(TEXT("   UpdateOwner"),STAT_UpdateOwnerTime,STATGROUP_DLE);
DECLARE_CYCLE_STAT(TEXT("DynamicLightEnvComp Tick"),STAT_DynamicLightEnvironmentComponentTickTime,STATGROUP_DLE);

DECLARE_DWORD_COUNTER_STAT(TEXT("Light Environments"),STAT_NumEnvironments,STATGROUP_DLE);
DECLARE_DWORD_COUNTER_STAT(TEXT("Static Updates"),STAT_StaticEnvironmentUpdates,STATGROUP_DLE);
DECLARE_DWORD_COUNTER_STAT(TEXT("Dynamic Updates"),STAT_DynamicEnvironmentUpdates,STATGROUP_DLE);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Lit particle components"),STAT_NumLitParticleComponents,STATGROUP_DLE);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num particle DLEs"),STAT_NumParticleDLEs,STATGROUP_DLE);

FLightEnvironmentDebugInfo GLightEnvironmentDebugInfo;

UBOOL ExecLightEnvironment(const TCHAR* Cmd, FOutputDevice& Ar)
{
	const FString FlagStr(ParseToken(Cmd, 0));
	UBOOL bRecognizedOption = FALSE;
	if( FlagStr.Len() > 0 )
	{
		if( appStricmp(*FlagStr, TEXT("List"))==0)
		{
			bRecognizedOption = TRUE;
			UBOOL bListDynamic = TRUE;
			UBOOL bListStatic = TRUE;
			UBOOL bListDetailed = FALSE;
			UBOOL bListPrecomputedVolumes = FALSE;
			UBOOL bListDominantShadowTransition = FALSE;
			const FString ListTypeStr(ParseToken(Cmd, 0));
			if( appStricmp(*ListTypeStr, TEXT("Dynamic"))==0)
			{
				bListStatic = FALSE;
			}
			else if( appStricmp(*ListTypeStr, TEXT("Static"))==0)
			{
				bListDynamic = FALSE;
			}
			else if( appStricmp(*ListTypeStr, TEXT("Volumes"))==0)
			{
				bListPrecomputedVolumes = TRUE;
			}
			else if( appStricmp(*ListTypeStr, TEXT("Transition"))==0)
			{
				bListDominantShadowTransition = TRUE;
			}
			else if( appStricmp(*ListTypeStr, TEXT("Detailed"))==0)
			{
				bListDetailed = TRUE;
			}

			if (bListPrecomputedVolumes)
			{
				INT TotalNumSamples = 0;
				SIZE_T TotalVolumeBytes = 0;
				for (INT LevelIndex = 0; LevelIndex < GWorld->Levels.Num(); LevelIndex++)
				{
					ULevel* CurrentLevel = GWorld->Levels(LevelIndex);
					if (CurrentLevel->PrecomputedLightVolume)
					{
						const INT NumVolumeSamples = CurrentLevel->PrecomputedLightVolume->GetNumSamples();
						TotalNumSamples += NumVolumeSamples;
						const SIZE_T NumVolumeBytes = CurrentLevel->PrecomputedLightVolume->GetAllocatedBytes();
						TotalVolumeBytes += NumVolumeBytes;
						Ar.Logf(TEXT("		%u samples, %.1fKb, %s"), NumVolumeSamples, NumVolumeBytes / 1024.0f, *CurrentLevel->GetPathName());
					}
				}
				Ar.Logf(TEXT("%u volume samples total, %.1fKb total"), TotalNumSamples, TotalVolumeBytes / 1024.0f);
			}
			else if (bListDominantShadowTransition)
			{
				if (GWorld->DominantDirectionalLight)
				{
					INT SizeX, SizeY;
					SIZE_T ShadowMapBytes;
					GWorld->DominantDirectionalLight->GetInfo(SizeX, SizeY, ShadowMapBytes);
					Ar.Logf(TEXT("%ux%u directional shadow map, %.1fKb total for %s"), SizeX, SizeY, ShadowMapBytes / 1024.0f, *GWorld->DominantDirectionalLight->GetPathName());
				}
				
				for (TSparseArray<UDominantSpotLightComponent*>::TIterator LightIt(GWorld->DominantSpotLights); LightIt; ++LightIt)
				{
					UDominantSpotLightComponent* CurrentLight = *LightIt;
					INT SizeX, SizeY;
					SIZE_T ShadowMapBytes;
					CurrentLight->GetInfo(SizeX, SizeY, ShadowMapBytes);
					Ar.Logf(TEXT("%ux%u spot shadow map, %.1fKb total for %s"), SizeX, SizeY, ShadowMapBytes / 1024.0f, *CurrentLight->GetPathName());
				}
			}
			else
			{
				INT NumDynamic = 0;
				INT NumStatic = 0;
				for (TObjectIterator<UDynamicLightEnvironmentComponent> It; It; ++It)
				{
					UDynamicLightEnvironmentComponent* Component = *It;
					if (Component->IsEnabled() && Component->GetOwner())
					{
						if (Component->bDynamic)
						{
							NumDynamic++;
						}
						else
						{
							NumStatic++;
						}
						if (bListDynamic && Component->bDynamic || bListStatic && !Component->bDynamic)
						{
							Ar.Logf( TEXT("%s	%s	%s"), 
								Component->bDynamic ? TEXT("Dynamic") : TEXT("Static"), 
								Component->IsA(UParticleLightEnvironmentComponent::StaticClass()) ? TEXT("Particle") : TEXT("Normal"), 
								*(Component->GetOwner()->GetPathName()));

							if (bListDetailed)
							{
								for (INT ComponentIndex = 0; ComponentIndex < Component->GetAffectedComponents().Num(); ComponentIndex++)
								{
									UPrimitiveComponent* PrimitiveComponent = Component->GetAffectedComponents()(ComponentIndex);
									UParticleSystemComponent* ParticleComponent = Cast<UParticleSystemComponent>(PrimitiveComponent);
									UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(PrimitiveComponent);
									USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(PrimitiveComponent);
									FString ResourceName = TEXT("");

									if (ParticleComponent)
									{
										ResourceName = ParticleComponent->Template->GetName();
									}
									else if (StaticMeshComponent)
									{
										ResourceName = StaticMeshComponent->StaticMesh->GetName();
									}
									else if (SkeletalMeshComponent)
									{
										ResourceName = SkeletalMeshComponent->SkeletalMesh->GetName();
									}

									Ar.Logf(TEXT("		%s	%s"), *PrimitiveComponent->GetName(), *ResourceName);
								}
								Ar.Logf(TEXT(""));
							}
						}
					}
				}
				Ar.Logf( TEXT("%u Enabled LightEnvironmentComponents, %u Dynamic and %u Static"), NumDynamic + NumStatic, NumDynamic, NumStatic);
			}
		}
		else if( appStricmp(*FlagStr, TEXT("Show"))==0)
		{
			const FString ShowStr(ParseToken(Cmd, 0));
			if( appStricmp(*ShowStr, TEXT("Bounds"))==0)
			{
				bRecognizedOption = TRUE;
				GLightEnvironmentDebugInfo.bShowBounds = !GLightEnvironmentDebugInfo.bShowBounds;
				Ar.Logf( TEXT("bShowBounds = %u"), GLightEnvironmentDebugInfo.bShowBounds);
			}
			else if( appStricmp(*ShowStr, TEXT("Visibility"))==0)
			{
				bRecognizedOption = TRUE;
				GLightEnvironmentDebugInfo.bShowVisibility = !GLightEnvironmentDebugInfo.bShowVisibility;
				Ar.Logf( TEXT("bShowVisibility = %u"), GLightEnvironmentDebugInfo.bShowVisibility);
			}
			else if( appStricmp(*ShowStr, TEXT("StaticUpdates"))==0)
			{
				bRecognizedOption = TRUE;
				GLightEnvironmentDebugInfo.bShowStaticUpdates = !GLightEnvironmentDebugInfo.bShowStaticUpdates;
				Ar.Logf( TEXT("bShowStaticUpdates = %u"), GLightEnvironmentDebugInfo.bShowStaticUpdates);
			}
			else if( appStricmp(*ShowStr, TEXT("CreateLights"))==0)
			{
				bRecognizedOption = TRUE;
				GLightEnvironmentDebugInfo.bShowCreateLights = !GLightEnvironmentDebugInfo.bShowCreateLights;
				Ar.Logf( TEXT("bShowCreateLights = %u"), GLightEnvironmentDebugInfo.bShowCreateLights);
			}
			else if( appStricmp(*ShowStr, TEXT("Interpolation"))==0)
			{
				bRecognizedOption = TRUE;
				GLightEnvironmentDebugInfo.bShowVolumeInterpolation = !GLightEnvironmentDebugInfo.bShowVolumeInterpolation;
				Ar.Logf( TEXT("bShowVolumeInterpolation = %u"), GLightEnvironmentDebugInfo.bShowVolumeInterpolation);
			}
			else if( appStricmp(*ShowStr, TEXT("Transition"))==0)
			{
				bRecognizedOption = TRUE;
				GLightEnvironmentDebugInfo.bShowDominantLightTransition = !GLightEnvironmentDebugInfo.bShowDominantLightTransition;
				Ar.Logf( TEXT("bShowDominantLightTransition = %u"), GLightEnvironmentDebugInfo.bShowDominantLightTransition);
			}
			else if( appStricmp(*ShowStr, TEXT("Primary"))==0)
			{
				bRecognizedOption = TRUE;
				GLightEnvironmentDebugInfo.bShowPrimaryLight = !GLightEnvironmentDebugInfo.bShowPrimaryLight;
				Ar.Logf( TEXT("bShowPrimaryLight = %u"), GLightEnvironmentDebugInfo.bShowPrimaryLight);
			}
			else if( appStricmp(*ShowStr, TEXT("Secondary"))==0)
			{
				bRecognizedOption = TRUE;
				GLightEnvironmentDebugInfo.bShowSecondaryLight = !GLightEnvironmentDebugInfo.bShowSecondaryLight;
				Ar.Logf( TEXT("bShowSecondaryLight = %u"), GLightEnvironmentDebugInfo.bShowSecondaryLight);
			}
			else if( appStricmp(*ShowStr, TEXT("DirectOnly"))==0)
			{
				bRecognizedOption = TRUE;
				GLightEnvironmentDebugInfo.bShowDirectLightingOnly = !GLightEnvironmentDebugInfo.bShowDirectLightingOnly;
				Ar.Logf( TEXT("bShowDirectLightingOnly = %u"), GLightEnvironmentDebugInfo.bShowDirectLightingOnly);
			}
			else if( appStricmp(*ShowStr, TEXT("IndirectOnly"))==0)
			{
				bRecognizedOption = TRUE;
				GLightEnvironmentDebugInfo.bShowIndirectLightingOnly = !GLightEnvironmentDebugInfo.bShowIndirectLightingOnly;
				Ar.Logf( TEXT("bShowIndirectLightingOnly = %u"), GLightEnvironmentDebugInfo.bShowIndirectLightingOnly);
			}
			else if( appStricmp(*ShowStr, TEXT("IndirectShadow"))==0)
			{
				bRecognizedOption = TRUE;
				GLightEnvironmentDebugInfo.bShowIndirectLightingShadowDirection = !GLightEnvironmentDebugInfo.bShowIndirectLightingShadowDirection;
				Ar.Logf( TEXT("bShowIndirectLightingShadowDirection = %u"), GLightEnvironmentDebugInfo.bShowIndirectLightingShadowDirection);
			}
			else if( appStricmp(*ShowStr, TEXT("Shadow"))==0)
			{
				bRecognizedOption = TRUE;
				GLightEnvironmentDebugInfo.bShowShadows = !GLightEnvironmentDebugInfo.bShowShadows;
				Ar.Logf( TEXT("bShowShadows = %u"), GLightEnvironmentDebugInfo.bShowShadows);
			}
			else if( appStricmp(*ShowStr, TEXT("Names"))==0)
			{
				bRecognizedOption = TRUE;
				GLightEnvironmentDebugInfo.bShowNames = !GLightEnvironmentDebugInfo.bShowNames;
				Ar.Logf( TEXT("bShowNames = %u"), GLightEnvironmentDebugInfo.bShowNames);
			}
		}
		else if( appStricmp(*FlagStr, TEXT("Debug"))==0)
		{
			bRecognizedOption = TRUE;
			const FString DebugStr(ParseToken(Cmd, 0));
			UBOOL bFoundComponent = FALSE;
			const UDynamicLightEnvironmentComponent* PreviousDebugComponent = GLightEnvironmentDebugInfo.Component;
			GLightEnvironmentDebugInfo.Component = NULL;
			for (TObjectIterator<UDynamicLightEnvironmentComponent> It; It; ++It)
			{
				UDynamicLightEnvironmentComponent* Component = *It;
				if (Component->IsEnabled() && Component->GetOwner() && Component->GetOwner()->GetPathName().InStr(*DebugStr, FALSE, TRUE) != INDEX_NONE)
				{
					if (Component == PreviousDebugComponent)
					{
						Ar.Logf( TEXT("Skipping previously debugged component %s class %s"), *Component->GetPathName(), *Component->GetClass()->GetName());
						bFoundComponent = TRUE;
					}
					else
					{
						GLightEnvironmentDebugInfo.Component = Component;
						Ar.Logf( TEXT("Debugging component %s class %s"), *Component->GetPathName(), *Component->GetClass()->GetName());
						bFoundComponent = TRUE;
						break;
					}
				}
			}
			if (!bFoundComponent)
			{
				Ar.Logf( TEXT("Couldn't find an enabled and attached DynamicLightEnvironmentComponent whose owner's name contains %s!"), *DebugStr);
			}
		}
		else if( appStricmp(*FlagStr, TEXT("Update"))==0)
		{
			for (TObjectIterator<UDynamicLightEnvironmentComponent> It; It; ++It)
			{
				UDynamicLightEnvironmentComponent* Component = *It;
				Component->ResetEnvironment();
			}
			bRecognizedOption = TRUE;
		}
	}

	if (bRecognizedOption)
	{
		// Reattach light environment components to propagate changes
		TComponentReattachContext<UDynamicLightEnvironmentComponent> LightEnvReattach;
	}
	else
	{
		Ar.Logf( TEXT("Failed to recognize LIGHTENV option!"));
	}
	return TRUE;
}

/** Updates GLightEnvironmentDebugInfo.Component with the currently selected DLE. */
static void UpdateDebugComponent()
{
	UDynamicLightEnvironmentComponent* SelectedDLE = NULL;
	for( FSelectedActorIterator It; It && !SelectedDLE; ++It )
	{
		for (INT ComponentIndex = 0; ComponentIndex < It->Components.Num(); ComponentIndex++)
		{
			UDynamicLightEnvironmentComponent* FoundDLE = Cast<UDynamicLightEnvironmentComponent>(It->Components(ComponentIndex));
			if (FoundDLE)
			{
				SelectedDLE = FoundDLE;
				// Debug the selected DLE
				GLightEnvironmentDebugInfo.Component = FoundDLE;
				break;
			}
		}
	}
	if (!SelectedDLE)
	{
		GLightEnvironmentDebugInfo.Component = NULL;
	}
}

/** Draws debug info for DLE's in the editor. */
void DrawLightEnvironmentDebugInfo(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	if (GLightEnvironmentDebugInfo.bShowBounds 
		|| GLightEnvironmentDebugInfo.bShowVolumeInterpolation 
		|| GLightEnvironmentDebugInfo.bShowDominantLightTransition
		|| GLightEnvironmentDebugInfo.bShowVisibility
		|| GLightEnvironmentDebugInfo.bShowStaticUpdates
		|| GLightEnvironmentDebugInfo.bShowCreateLights
		|| GLightEnvironmentDebugInfo.bShowDirectLightingOnly
		|| GLightEnvironmentDebugInfo.bShowIndirectLightingOnly)
	{
		UpdateDebugComponent();

		for (TObjectIterator<UDynamicLightEnvironmentComponent> It; It; ++It)
		{
			UDynamicLightEnvironmentComponent* Component = *It;
			if (Component->IsEnabled() 
				&& Component->GetOwner()
				// Don't show debug info for DLE's that are hidden in the editor
				&& !Component->GetOwner()->bHiddenEdTemporary
				&& !Component->GetOwner()->bHiddenEdLevel
				&& Component->State)
			{
				if (GLightEnvironmentDebugInfo.bShowVolumeInterpolation)
				{
					// Make sure components created to visualize volume samples have an up to date hidden state,
					// Now that the debug component has been updated.
					const UBOOL bDesiredHiddenState = GLightEnvironmentDebugInfo.Component == NULL || GLightEnvironmentDebugInfo.Component != Component;
					for (INT MeshIndex = 0; MeshIndex < Component->State->DebugVolumeSampleMeshes.Num(); MeshIndex++)
					{
						UStaticMeshComponent* Mesh = Component->State->DebugVolumeSampleMeshes(MeshIndex);
						const UBOOL bCurrentHiddenState = Mesh->HiddenGame && Mesh->HiddenEditor;
						if (bCurrentHiddenState != bDesiredHiddenState)
						{
							Mesh->SetHiddenGame(bDesiredHiddenState);
							Mesh->SetHiddenEditor(bDesiredHiddenState);
						}
					}
				}
				// Draw debug info for the component being debugged or all components if there is not one being debugged
				if (!GLightEnvironmentDebugInfo.Component || GLightEnvironmentDebugInfo.Component == Component)
				{
					UBOOL bShowDLEDebugPrimitives = ((View->Family->ShowFlags & SHOW_Game) && !Component->GetOwner()->bHidden 
						|| !(View->Family->ShowFlags & SHOW_Game) && !Component->GetOwner()->bHiddenEd);
					for (INT ComponentIndex = 0; ComponentIndex < Component->AffectedComponents.Num(); ComponentIndex++)
					{
						UPrimitiveComponent* Primitive = Component->AffectedComponents(ComponentIndex);
						// Only render debug info for DLE's whose mesh components are supposed to be shown in the current mode
						bShowDLEDebugPrimitives = bShowDLEDebugPrimitives && 
							((View->Family->ShowFlags & SHOW_Game) && !Primitive->HiddenGame || !(View->Family->ShowFlags & SHOW_Game) && !Primitive->HiddenEditor);
					}
					if (bShowDLEDebugPrimitives)
					{
						if (GLightEnvironmentDebugInfo.bShowBounds)
						{
							DrawWireSphere(PDI, Component->State->OwnerBounds.Origin, FColor(200, 200, 255), Component->State->OwnerBounds.SphereRadius, 12, SDPG_World);
							for (INT SampleIndex = 0; SampleIndex < Component->State->LightVisibilitySamplePoints.Num(); SampleIndex++)
							{
								const FVector SamplePosition = Component->State->OwnerBounds.Origin + Component->State->LightVisibilitySamplePoints(SampleIndex) * Component->State->OwnerBounds.BoxExtent;
								PDI->DrawPoint(SamplePosition, FLinearColor::White, 8, SDPG_World);
							}
						}
						// Only draw the volume sample points if no component is being debugged, since lit spheres will be drawn if a component is being debugged
						if (GLightEnvironmentDebugInfo.bShowVolumeInterpolation && !GLightEnvironmentDebugInfo.Component)
						{
							for (INT i = 0; i < Component->State->DebugInterpolatedVolumeSamples.Num(); i++)
							{
								const FVolumeLightingSample& CurrentSample = Component->State->DebugInterpolatedVolumeSamples(i);
								FSHVectorRGB IncidentRadiance;
								CurrentSample.ToSHVector(IncidentRadiance, Component->bIsCharacterLightEnvironment);
								const FLinearColor AverageColor = IncidentRadiance.CalcIntegral() / FSHVector::ConstantBasisIntegral;
								PDI->DrawPoint(CurrentSample.Position, AverageColor, 8.0f, SDPG_World);
								// Sphere to visualize radius
								// Currently disabled as it adds quite a bit of clutter
								//DrawWireSphere(PDI, CurrentSample.Position, FColor(155, 155, 100), CurrentSample.Radius, 12, SDPG_World);
							}
						}
						if (GLightEnvironmentDebugInfo.bShowDominantLightTransition)
						{
							for (INT i = 0; i < Component->State->DebugClosestDominantLightRays.Num(); i++)
							{
								const FDebugShadowRay& CurrentRay =  Component->State->DebugClosestDominantLightRays(i);
								PDI->DrawLine(CurrentRay.Start, CurrentRay.End, CurrentRay.bHit ? FLinearColor(1.0f, 0.0f, 0.0f) : FLinearColor::White, SDPG_World);
							}
						}
						if (GLightEnvironmentDebugInfo.bShowVisibility)
						{
							for (INT i = 0; i < Component->State->DebugDynamicVisibilityTraces.Num(); i++)
							{
								const FDebugShadowRay& CurrentRay = Component->State->DebugDynamicVisibilityTraces(i);
								PDI->DrawLine(CurrentRay.Start, CurrentRay.End, CurrentRay.bHit ? FLinearColor(1.0f, 0.0f, 0.0f) : FLinearColor::White, SDPG_World);
							}
							for (INT i = 0; i < Component->State->DebugStaticVisibilityTraces.Num(); i++)
							{
								const FDebugShadowRay& CurrentRay = Component->State->DebugStaticVisibilityTraces(i);
								PDI->DrawLine(CurrentRay.Start, CurrentRay.End, CurrentRay.bHit ? FLinearColor(1.0f, 0.0f, 0.0f) : FLinearColor::White, SDPG_World);
							}
						}
						if (GLightEnvironmentDebugInfo.bShowStaticUpdates)
						{
							for (INT i = 0; i < Component->State->DebugStaticUpdates.Num(); i++)
							{
								const FVector& CurrentPoint = Component->State->DebugStaticUpdates(i);
								DrawWireSphere(PDI, CurrentPoint, FColor(155, 155, 100), 50.0f, 12, SDPG_World);
							}
						}
						if (GLightEnvironmentDebugInfo.bShowCreateLights)
						{
							for (INT i = 0; i < Component->State->DebugCreateLights.Num(); i++)
							{
								const FVector& CurrentPoint = Component->State->DebugCreateLights(i);
								PDI->DrawPoint(CurrentPoint, FColor(200, 155, 100), 8.0f, SDPG_World);
							}
						}
					}
				}
			}
		}
	}
}

/** Compute the direction which the spherical harmonic is highest at. */
static FVector SHGetMaximumDirection(const FSHVector& SH,UBOOL bLowerHemisphere,UBOOL bUpperHemisphere)
{
	// This is an approximation which only takes into account first and second order spherical harmonics.
	FLOAT Z = SH.V[2];
	if(!bLowerHemisphere)
	{
		Z = Max(Z,0.0f);
	}
	if(!bUpperHemisphere)
	{
		Z = Min(Z,0.0f);
	}
	return FVector(
		-SH.V[3],
		-SH.V[1],
		Z
		);
}

/** Compute the direction which the spherical harmonic is lowest at. */
static FVector SHGetMinimumDirection(const FSHVector& SH,UBOOL bLowerHemisphere,UBOOL bUpperHemisphere)
{
	// This is an approximation which only takes into account first and second order spherical harmonics.
	FLOAT Z = -SH.V[2];
	if(!bLowerHemisphere)
	{
		Z = Max(Z,0.0f);
	}
	if(!bUpperHemisphere)
	{
		Z = Min(Z,0.0f);
	}
	return FVector(
		+SH.V[3],
		+SH.V[1],
		Z
		);
}

/** Clamps each color component above 0. */
static FLinearColor GetPositiveColor(const FLinearColor& Color)
{
	return FLinearColor(
		Max(Color.R,0.0f),
		Max(Color.G,0.0f),
		Max(Color.B,0.0f),
		Color.A
		);
}

/**
 * Calculates the intensity to use for a light that minimizes Dot(RemainingLightEnvironment,RemainingLightEnvironment),
 * given RemainingLightEnvironment = LightEnvironment - UnitLightFunction * <resulting intensity>.
 * In other words, it tries to set a light intensity that accounts for as much of the light environment as possible given UnitLightFunction.
 * @param LightEnvironment - The light environment to subtract the light function from.
 * @param UnitLightFunction - The incident lighting that would result from a light intensity of 1.
 * @return The light intensity that minimizes the light remaining in the environment.
 */
static FLinearColor GetLightIntensity(const FSHVectorRGB& LightEnvironment,const FSHVector& UnitLightFunction)
{
	return GetPositiveColor(Dot(LightEnvironment,UnitLightFunction) / Dot(UnitLightFunction,UnitLightFunction));
}

/**
 * Extracts the dominant lighting from a composite light environment.
 * @param InOutLightEnvironment - The composite light environment.  The dominant light is subtracted from it.
 * @param OutDirection - On successful return, the direction to the dominant light.
 * @param OutIntensity - On successful return, the intensity of the dominant light.
 * @return TRUE if the light environment had light to extract.
 */
static UBOOL ExtractDominantLight(FSHVectorRGB& InOutLightEnvironment,FVector& OutDirection,FLinearColor& OutIntensity,FLOAT Weight)
{
	// Find the direction in the light environment with the highest luminance.
	const FSHVector EnvironmentLuminance = InOutLightEnvironment.GetLuminance();
	OutDirection = SHGetMaximumDirection(EnvironmentLuminance,TRUE,TRUE);
	if(OutDirection.SizeSquared() >= Square(DELTA))
	{
		OutDirection.Normalize();

		// Calculate the light intensity for this direction.
		const FSHVector UnitLightSH = SHBasisFunction(OutDirection);

		OutIntensity = GetLightIntensity(InOutLightEnvironment,UnitLightSH) * Weight;

		// Remove the dominant light from the environment.
		InOutLightEnvironment -= UnitLightSH * OutIntensity;

		return TRUE;
	}

	return FALSE;
}

/** Remove skylighting from a light environment. */
static void ExtractEnvironmentSkyLight(FSHVectorRGB& LightEnvironment,FLinearColor& OutSkyColor,UBOOL bLowerHemisphere,UBOOL bUpperHemisphere)
{
	// Set up SH coefficients representing incident lighting from a sky light of unit brightness.
	FSHVector SkyFunction;
	if(bLowerHemisphere)
	{
		SkyFunction += FSHVector::LowerSkyFunction();
	}
	if(bUpperHemisphere)
	{
		SkyFunction += FSHVector::UpperSkyFunction();
	}

	// Calculate the sky light intensity.
	const FLinearColor Intensity = GetLightIntensity(LightEnvironment,SkyFunction);
	if(Intensity.R > 0.0f || Intensity.G > 0.0f || Intensity.B > 0.0f)
	{
		OutSkyColor += Intensity;
		LightEnvironment -= SkyFunction * Intensity;
	}
}

/** Adds a skylight to a light environment. */
static FSHVectorRGB GetSkyLightEnvironment(const UDynamicLightEnvironmentComponent* LightEnvironment,const USkyLightComponent* SkyLight)
{
	const FLinearColor DirectUpperColor = FLinearColor(SkyLight->LightColor) * SkyLight->Brightness;
	const FLinearColor DirectLowerColor = FLinearColor(SkyLight->LowerColor) * SkyLight->LowerBrightness;

	return FSHVector::UpperSkyFunction() * DirectUpperColor + FSHVector::LowerSkyFunction() * DirectLowerColor;
}

/** Returns the integral of the square of the difference between two spherical harmonic projected functions. */
static FLOAT GetSquaredDifferenceIntegral(const FSHVectorRGB& A,const FSHVectorRGB& B)
{
	const FSHVectorRGB Difference = A - B;
	return	Dot(Difference.R,Difference.R) +
			Dot(Difference.G,Difference.G) +
			Dot(Difference.B,Difference.B);
}

FDynamicLightEnvironmentState::FDynamicLightEnvironmentState(UDynamicLightEnvironmentComponent* InComponent):
	Component(InComponent)
,	PredictedOwnerPosition(0,0,0)
,	OwnerPackage(NULL)
,	ShadowPlane(0, 0, 1, 0)
,	LastUpdateTime(0)
,	InvisibleUpdateTime(InComponent->InvisibleUpdateTime)
,	MinTimeBetweenFullUpdates(InComponent->MinTimeBetweenFullUpdates)
,	LastInterpolatePosition(0,0,0)
,	CurrentDominantShadowTransitionDistance(InComponent->DominantShadowTransitionStartDistance)
,	CurrentRepresentativeShadowLight(NULL)
,	bFirstFullUpdate(TRUE)
,	bNeedsStaticUpdate(FALSE)
{
}

UBOOL FDynamicLightEnvironmentState::UpdateOwner()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateOwnerTime);

	AActor* Owner = Component->GetOwner();
	// Light environments without an Owner must override information gathered from the Owner.
	// Currently this is only light environments used in preview scenes.
	check(Owner || Component->BoundsMethod == DLEB_ManualOverride && Component->bOverrideOwnerLightingChannels);
	if (Owner 
		&& (Component->BoundsMethod == DLEB_OwnerComponents 
		|| Component->BoundsMethod == DLEB_ManualOverride && !Component->bOverrideOwnerLightingChannels))
	{
		check(Owner->AllComponents.ContainsItem(Component));

		// Ensure that the owner's other components have been attached before we gather their attributes.
		for(INT ComponentIndex = 0;ComponentIndex < Owner->Components.Num();ComponentIndex++)
		{
			UActorComponent* Component = Owner->Components(ComponentIndex);
			if(Component && !Component->IsAttached())
			{
				Component->ConditionalAttach(GWorld->Scene,Owner,Owner->LocalToWorld());
			}
		}

		// Find the owner's bounds and lighting channels.
		OwnerBounds = FBoxSphereBounds(FVector(0,0,0),FVector(0,0,0),0);
		OwnerLightingChannels.Bitfield = 0;
		OwnerLightingChannels.bInitialized = TRUE;
		UBOOL bFirstComponentFound = FALSE;
		for(INT ComponentIndex = 0;ComponentIndex < Owner->AllComponents.Num();ComponentIndex++)
		{
			UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Owner->AllComponents(ComponentIndex));

			// Only look at primitives which use this light environment.
			if(Primitive && Primitive->LightEnvironment == Component)
			{
				if (bFirstComponentFound)
				{
					// Add the primitive's bounds to the composite owner bounds.
					OwnerBounds = OwnerBounds + Primitive->Bounds;

					// Add the primitive's lighting channels to the composite owner lighting channels.
					OwnerLightingChannels.Bitfield |= Primitive->LightingChannels.Bitfield;
				}
				else
				{
					bFirstComponentFound = TRUE;
					OwnerBounds = Primitive->Bounds;
					OwnerLightingChannels.Bitfield = Primitive->LightingChannels.Bitfield;
				}
			}
		}
		// Find the owner's package.
		OwnerPackage = Owner->GetOutermost();
	}

	if (Component->BoundsMethod == DLEB_ManualOverride)
	{
		OwnerBounds = Component->OverriddenBounds;
	}

	if (Component->BoundsMethod == DLEB_ActiveComponents)
	{
		if (Component->AffectedComponents.Num() > 0)
		{
			UBOOL bFirstComponentFound = FALSE;
			for (INT ComponentIndex = 0; ComponentIndex < Component->AffectedComponents.Num(); ComponentIndex++)
			{
				UPrimitiveComponent* Primitive = Component->AffectedComponents(ComponentIndex);
				// Only valid components are added, but GC may have NULL'ed the reference.
				if (Primitive)
				{
					checkSlow(Primitive->IsAttached());
					if (bFirstComponentFound)
					{
						OwnerBounds = OwnerBounds + Primitive->Bounds;
						OwnerLightingChannels.Bitfield |= Primitive->LightingChannels.Bitfield;
					}
					else
					{
						bFirstComponentFound = TRUE;
						OwnerBounds = Primitive->Bounds;
						OwnerLightingChannels.Bitfield = Primitive->LightingChannels.Bitfield;
					}
				}
			}
		}
		else
		{
			OwnerBounds = FBoxSphereBounds(FVector(0,0,0), FVector(1,1,1), 1.0f);
			OwnerLightingChannels.SetAllChannels();
		}
	}

	OwnerBounds.BoxExtent *= Component->LightingBoundsScale;
	OwnerBounds.SphereRadius *= Component->LightingBoundsScale;

	if (Component->bOverrideOwnerLightingChannels)
	{
		OwnerLightingChannels = Component->OverriddenLightingChannels;
	}

	PredictedOwnerPosition = OwnerBounds.Origin;

	// Update the cached light visibility sample points if we don't have the right number.
	if(LightVisibilitySamplePoints.Num() != Component->NumVolumeVisibilitySamples)
	{
		FRandomStream RandomStream(0);
		LightVisibilitySamplePoints.Empty();

		// Initialize the random light visibility sample points.
		const INT NumLightVisibilitySamplePoints = Component->NumVolumeVisibilitySamples;
		LightVisibilitySamplePoints.Empty(NumLightVisibilitySamplePoints);
		for(INT PointIndex = 1;PointIndex < NumLightVisibilitySamplePoints;PointIndex++)
		{
			LightVisibilitySamplePoints.AddItem(
				FVector(
					-1.0f + 2.0f * RandomStream.GetFraction(),
					-1.0f + 2.0f * RandomStream.GetFraction(),
					-1.0f + 2.0f * RandomStream.GetFraction()
					)
				);
		}

		// Always place one sample at the center of the owner's bounds.
		LightVisibilitySamplePoints.AddItem(FVector(0,0,0));
	}

	UBOOL bNeedsUpdate = FALSE;
	for (INT ComponentIndex = 0; ComponentIndex < Component->AffectedComponents.Num(); ComponentIndex++)
	{
		UPrimitiveComponent* Primitive = Component->AffectedComponents(ComponentIndex);
		if (Primitive && !bNeedsUpdate)
		{
			bNeedsUpdate = Component->NeedsUpdateBasedOnComponent(Primitive);
		}
	}

	return bNeedsUpdate;
}

void FDynamicLightEnvironmentState::UpdateStaticEnvironment(ULightComponent* NewAffectingDominantLight)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateStaticEnvironmentTime);
	INC_DWORD_STAT(STAT_StaticEnvironmentUpdates);

	if (GLightEnvironmentDebugInfo.bShowStaticUpdates 
		&& (!GLightEnvironmentDebugInfo.Component || GLightEnvironmentDebugInfo.Component == Component))
	{
		DebugStaticUpdates.AddItem(OwnerBounds.Origin);
	}

	ClearPreviewComponents();

	// Reset as the below code is going to accumulate from scratch.
	NewStaticLightEnvironment = FSHVectorRGB();
	NewStaticNonShadowedLightEnvironment = FSHVectorRGB();
	NewStaticShadowInfo.DominantShadowFactor = 1.0f;
	FSHVectorRGB NewStaticShadowEnvironment;

	if ((!GLightEnvironmentDebugInfo.bShowIndirectLightingOnly || GLightEnvironmentDebugInfo.Component && GLightEnvironmentDebugInfo.Component != Component) && 
		Component->OverriddenLightComponents.Num() == 0)
	{
		if (GLightEnvironmentDebugInfo.bShowVisibility)
		{
			DebugStaticVisibilityTraces.Empty();
		}

		if (!GLightEnvironmentDebugInfo.bShowStaticUpdates)
		{
			DebugStaticUpdates.Empty();
		}

		if (!GLightEnvironmentDebugInfo.bShowCreateLights)
		{
			DebugCreateLights.Empty();
		}

		// Iterate over static lights and update the static light environment.
		for(TSparseArray<ULightComponent*>::TConstIterator LightIt(GWorld->StaticLightList);LightIt;++LightIt)
		{
			const ULightComponent* Light = *LightIt;

			// Add static light to static light environment
			AddLightToEnvironment(Light,NewStaticLightEnvironment,NewStaticNonShadowedLightEnvironment,NewStaticShadowEnvironment,OwnerBounds,FALSE);
		}

		// Dominant lights are being composited, trace rays for shadowing
		if (Component->bForceCompositeAllLights)
		{
			if (GWorld->DominantDirectionalLight)
			{
				AddLightToEnvironment(GWorld->DominantDirectionalLight,NewStaticLightEnvironment,NewStaticNonShadowedLightEnvironment,NewStaticShadowEnvironment,OwnerBounds,FALSE);
			}
			
			for (TSparseArray<UDominantPointLightComponent*>::TConstIterator LightIt(GWorld->DominantPointLights); LightIt; ++LightIt)
			{
				const UDominantPointLightComponent* CurrentLight = *LightIt;
				AddLightToEnvironment(CurrentLight,NewStaticLightEnvironment,NewStaticNonShadowedLightEnvironment,NewStaticShadowEnvironment,OwnerBounds,FALSE);
			}

			for (TSparseArray<UDominantSpotLightComponent*>::TConstIterator LightIt(GWorld->DominantSpotLights); LightIt; ++LightIt)
			{
				const UDominantSpotLightComponent* CurrentLight = *LightIt;
				AddLightToEnvironment(CurrentLight,NewStaticLightEnvironment,NewStaticNonShadowedLightEnvironment,NewStaticShadowEnvironment,OwnerBounds,FALSE);
			}
		}
		else if (Component->bUseBooleanEnvironmentShadowing)
		{
			FLOAT VisibilityFactor;
			if (!NewAffectingDominantLight
				// Skip tracing a ray if we know that the light is shadowed based on the dominant transition distance
				|| CurrentDominantShadowTransitionDistance > Component->DominantShadowTransitionStartDistance 
				// Trace a ray to determine boolean environment visibility
				|| !IsLightVisible(NewAffectingDominantLight, PredictedOwnerPosition, FALSE, VisibilityFactor))
			{
				//@todo - take VisibilityFactor into account?  
				// The current behavior is to be completely unshadowed if any of the sample points are unshadowed
				NewStaticShadowInfo.DominantShadowFactor = 0.0f;
			}
		}
	}

#if WITH_MOBILE_RHI
	if (GUsingMobileRHI 
		&& GSystemSettings.bAllowDynamicShadows 
		// Planar shadows are only used when bMobileModShadows is disabled
		&& !GSystemSettings.bMobileModShadows)
	{
		FCheckResult CheckResult;
		// Trace against simplified collision to find the plane under the DLE
		const UBOOL bHit = !GWorld->SingleLineCheck(CheckResult, NULL, OwnerBounds.Origin - FVector(0.0f, 0.0f, HALF_WORLD_MAX), OwnerBounds.Origin, TRACE_LevelGeometry | TRACE_Level | TRACE_Terrain);
		if (bHit)
		{
			// Use the simplified collision triangle under the DLE as the shadow plane for planar shadows on Mobile
			// Move the plane up a bit to avoid z-fighting with the underlying geometry
			ShadowPlane = FPlane(CheckResult.Location + FVector(0, 0, 1.0f), CheckResult.Normal);
		}
		else
		{
			ShadowPlane = FPlane(0, 1, 0, 0);
		}
	}
#endif

	if ((!GLightEnvironmentDebugInfo.bShowDirectLightingOnly || GLightEnvironmentDebugInfo.Component && GLightEnvironmentDebugInfo.Component != Component)
		&& Component->OverriddenLightComponents.Num() == 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_InterpolateIndirectTime);
		DebugInterpolatedVolumeSamples.Empty();

		FSHVectorRGB AccumulatedIncidentRadiance;
		FLOAT AccumulatedWeight = 0.0f;
		for (INT LevelIndex = 0; LevelIndex < GWorld->Levels.Num(); LevelIndex++)
		{
			FPrecomputedLightVolume* PrecomputedLightVolume = GWorld->Levels(LevelIndex)->PrecomputedLightVolume;
			if (PrecomputedLightVolume && PrecomputedLightVolume->IsInitialized())
			{
				FSHVectorRGB CurrentIncidentRadiance;
				FLOAT CurrentWeight = 0.0f;
				PrecomputedLightVolume->InterpolateIncidentRadiance(
					OwnerBounds.Origin, 
					Component->bIsCharacterLightEnvironment,
					GLightEnvironmentDebugInfo.bShowVolumeInterpolation, 
					DebugInterpolatedVolumeSamples,
					CurrentWeight,
					CurrentIncidentRadiance);
				AccumulatedIncidentRadiance += CurrentIncidentRadiance;
				AccumulatedWeight += CurrentWeight;
			}
		}

		if (AccumulatedWeight > 0.0f)
		{
			const FSHVectorRGB CombinedIncidentRadiance = AccumulatedIncidentRadiance * (1.0f / AccumulatedWeight);

			NewStaticLightEnvironment += CombinedIncidentRadiance;
			if (GLightEnvironmentDebugInfo.bShowIndirectLightingShadowDirection)
			{
				// Apply the indirect lighting to the shadow environment so that it will affect the shadow direction and color
				NewStaticShadowEnvironment += CombinedIncidentRadiance;
			}
			else
			{
				// Apply an ambient function with the correct intensity to the shadow environment so the indirect lighting will affect only the shadow color, not direction
				// This is the default because having the indirect lighting affect shadow direction results in a lot of direction popping
				NewStaticShadowEnvironment += FSHVector::AmbientFunction() * CombinedIncidentRadiance.CalcIntegral();
			}
		}
		else
		{
			const FLinearColor EnvironmentColor = GWorld->GetWorldInfo()->GetEnvironmentColor();
			// Add some of the environment color if no indirect lighting samples were found
			// This prevents DLE's outside any importance volume from having black indirect lighting
			const FSHVectorRGB EnvironmentSH = FSHVector::UpperSkyFunction() * EnvironmentColor * .9f + FSHVector::LowerSkyFunction() * EnvironmentColor * .1f;
			NewStaticLightEnvironment += EnvironmentSH;
			NewStaticShadowEnvironment += EnvironmentSH;
		}

		// Update debug info if we're visualizing volume samples
		if (GLightEnvironmentDebugInfo.bShowVolumeInterpolation)
		{
			// Mesh that will be used to visualize each volume sample
			UStaticMesh* SphereMesh = (UStaticMesh*)UObject::StaticLoadObject(UStaticMesh::StaticClass(),NULL,TEXT("EngineMeshes.Sphere"),NULL,LOAD_None,NULL);
			for (INT SampleIndex = 0; SampleIndex < DebugInterpolatedVolumeSamples.Num(); SampleIndex++)
			{
				const FVolumeLightingSample& VolumeSample = DebugInterpolatedVolumeSamples(SampleIndex);
				// Create a static mesh component for the sample
				UStaticMeshComponent* NewMesh = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass(), Component->GetOuter());
				
				NewMesh->AbsoluteTranslation = TRUE;
				NewMesh->Translation = VolumeSample.Position;
				NewMesh->AbsoluteRotation = TRUE;
				NewMesh->AbsoluteScale = TRUE;
				NewMesh->Scale = .05f;

				// Use a lit material with .5 diffuse
				NewMesh->SetMaterial(0, GEngine->LevelColorationLitMaterial);
				NewMesh->SetStaticMesh(SphereMesh);
				NewMesh->LightingChannels.ClearAllChannels();
				NewMesh->LightingChannels.bInitialized = TRUE;
				NewMesh->LightingChannels.Unnamed_6 = TRUE;
				// Don't want the selection highlight to be applied
				NewMesh->bSelectable = FALSE;

				// Initialize the hidden state of the component
				// Only show the sphere mesh visualization if this component is selected, because the meshes are fairly slow to render
				const UBOOL bDesiredHiddenState = GLightEnvironmentDebugInfo.Component == NULL || GLightEnvironmentDebugInfo.Component != Component;
				NewMesh->HiddenGame = bDesiredHiddenState;
				NewMesh->HiddenEditor = bDesiredHiddenState;

				USphericalHarmonicLightComponent* NewLight = ConstructObject<USphericalHarmonicLightComponent>(USphericalHarmonicLightComponent::StaticClass(), Component->GetOuter());
				NewLight->LightingChannels.ClearAllChannels();
				NewLight->LightingChannels.bInitialized = TRUE;
				// Use the same lighting channel as the mesh
				NewLight->LightingChannels.Unnamed_6 = TRUE;
				// Mark the light as being explicitly assigned so that it won't affect any other meshes in the same lighting channel
				NewLight->bExplicitlyAssignedLight = TRUE;
				// Set the light's SH environment to match the volume sample
				VolumeSample.ToSHVector(NewLight->WorldSpaceIncidentLighting, Component->bIsCharacterLightEnvironment);
				Component->GetOwner()->AttachComponent(NewLight);

				// Set the light as the override light on the mesh, so that only this light can affect the mesh
				NewMesh->OverrideLightComponent = NewLight;
				Component->GetOwner()->AttachComponent(NewMesh);

				DebugVolumeSampleLights.AddItem(NewLight);
				DebugVolumeSampleMeshes.AddItem(NewMesh);
			}
		}
	}

	if (Component->OverriddenLightComponents.Num() > 0)
	{
		for (INT i = 0; i < Component->OverriddenLightComponents.Num(); i++)
		{
			AddLightToEnvironment(Component->OverriddenLightComponents(i),NewStaticLightEnvironment,NewStaticNonShadowedLightEnvironment,NewStaticShadowEnvironment,OwnerBounds,FALSE);
		}
	}

	// Add the ambient glow.
	NewStaticLightEnvironment += FSHVector::AmbientFunction() * (Component->AmbientGlow * 4.0f);

	// Add the ambient shadow source.
	const FSHVector AmbientShadowSH = SHBasisFunction(Component->AmbientShadowSourceDirection.SafeNormal());
	NewStaticShadowEnvironment += AmbientShadowSH * Component->AmbientShadowColor;

	if (ExtractDominantLight(NewStaticShadowEnvironment, NewStaticShadowInfo.ShadowDirection, NewStaticShadowInfo.DominantShadowIntensity, 1.0f))
	{
		const FLinearColor RemainingShadowIntensity = GetLightIntensity(NewStaticShadowEnvironment, FSHVector::AmbientFunction());
		NewStaticShadowInfo.TotalShadowIntensity = RemainingShadowIntensity + NewStaticShadowInfo.DominantShadowIntensity;
	}
	else
	{
		NewStaticShadowInfo.ShadowDirection = FVector(0,0,0);
		NewStaticShadowInfo.DominantShadowIntensity = FLinearColor::Black;
		NewStaticShadowInfo.TotalShadowIntensity = FLinearColor::Black;
	}
}

void FDynamicLightEnvironmentState::UpdateDynamicEnvironment()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateDynamicEnvironmentTime);
	INC_DWORD_STAT(STAT_DynamicEnvironmentUpdates);
	
	DynamicLightEnvironment = FSHVectorRGB();
	DynamicNonShadowedLightEnvironment = FSHVectorRGB();
	FSHVectorRGB DynamicShadowEnvironment;

	if ((!GLightEnvironmentDebugInfo.bShowIndirectLightingOnly || GLightEnvironmentDebugInfo.Component && GLightEnvironmentDebugInfo.Component != Component)
		&& Component->OverriddenLightComponents.Num() == 0)
	{
		if (GLightEnvironmentDebugInfo.bShowVisibility 
			&& (!GLightEnvironmentDebugInfo.Component || GLightEnvironmentDebugInfo.Component == Component))
		{
			DebugDynamicVisibilityTraces.Empty();
		}

		// Iterate over dynamic lights and update the dynamic light environment.
		for(TSparseArray<ULightComponent*>::TConstIterator LightIt(GWorld->DynamicLightList);LightIt;++LightIt)
		{
			ULightComponent* Light = *LightIt;

			// Add the dynamic light to the light environment.
			AddLightToEnvironment(Light,DynamicLightEnvironment,DynamicNonShadowedLightEnvironment,DynamicShadowEnvironment,OwnerBounds,TRUE);
		}
	}

	if (ExtractDominantLight(DynamicShadowEnvironment, DynamicShadowInfo.ShadowDirection, DynamicShadowInfo.DominantShadowIntensity, 1.0f))
	{
		const FLinearColor RemainingShadowIntensity = GetLightIntensity(DynamicShadowEnvironment, FSHVector::AmbientFunction());
		DynamicShadowInfo.TotalShadowIntensity = RemainingShadowIntensity + DynamicShadowInfo.DominantShadowIntensity;
	}
	else
	{
		DynamicShadowInfo.ShadowDirection = FVector(0,0,0);
		DynamicShadowInfo.DominantShadowIntensity = FLinearColor::Black;
		DynamicShadowInfo.TotalShadowIntensity = FLinearColor::Black;
	}
}

void FDynamicLightEnvironmentState::UpdateEnvironmentInterpolation(FLOAT DeltaTime,FLOAT TimeBetweenUpdates)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateEnvironmentInterpolationTime);

	// Smoothly interpolate the static light set.
	const FLOAT RemainingTransitionTime = Max(DELTA,LastUpdateTime + TimeBetweenUpdates - GWorld->GetTimeSeconds());
	const FLOAT TransitionAlpha = Clamp(DeltaTime / RemainingTransitionTime,0.0f,1.0f);
	StaticLightEnvironment						*= (1.0f - TransitionAlpha);
	StaticNonShadowedLightEnvironment			*= (1.0f - TransitionAlpha);
	StaticLightEnvironment						+= TransitionAlpha * NewStaticLightEnvironment;
	StaticNonShadowedLightEnvironment			+= TransitionAlpha * NewStaticNonShadowedLightEnvironment;
	StaticShadowInfo.DominantShadowFactor		= (1.0f - TransitionAlpha) * StaticShadowInfo.DominantShadowFactor + TransitionAlpha * NewStaticShadowInfo.DominantShadowFactor;

	// Interpolate the shadow based on distance traveled instead of time.
	// This has the advantage that the shadow interpolation will stop when the DLE stops moving.
	const FLOAT DistanceTransitionAlpha = Min((LastInterpolatePosition - PredictedOwnerPosition).Size() * Component->ShadowInterpolationSpeed, 1.0f);
	LastInterpolatePosition = PredictedOwnerPosition;
	StaticShadowInfo.ShadowDirection			*= (1.0f - DistanceTransitionAlpha);
	StaticShadowInfo.DominantShadowIntensity	*= (1.0f - DistanceTransitionAlpha);
	StaticShadowInfo.TotalShadowIntensity		*= (1.0f - DistanceTransitionAlpha);
	StaticShadowInfo.ShadowDirection			+= DistanceTransitionAlpha * NewStaticShadowInfo.ShadowDirection;
	StaticShadowInfo.DominantShadowIntensity	+= DistanceTransitionAlpha * NewStaticShadowInfo.DominantShadowIntensity;
	StaticShadowInfo.TotalShadowIntensity		+= DistanceTransitionAlpha * NewStaticShadowInfo.TotalShadowIntensity;
}

void FDynamicLightEnvironmentState::Update()
{
	if (GIsEditor && !GIsGame)
	{
		UpdateDebugComponent();
	}

	UpdateOwner();

	ULightComponent* NewAffectingDominantLight;
	FLOAT NewDominantShadowTransitionDistance;
	CalculateDominantShadowTransitionDistance(NewAffectingDominantLight, NewDominantShadowTransitionDistance);

	UpdateStaticEnvironment(NewAffectingDominantLight);
	UpdateDynamicEnvironment();

	// Immediately transition to the newly computed light environment.
	StaticLightEnvironment = NewStaticLightEnvironment;
	StaticNonShadowedLightEnvironment = NewStaticNonShadowedLightEnvironment;
	StaticShadowInfo.ShadowDirection = NewStaticShadowInfo.ShadowDirection;
	StaticShadowInfo.DominantShadowFactor = NewStaticShadowInfo.DominantShadowFactor;
	StaticShadowInfo.DominantShadowIntensity = NewStaticShadowInfo.DominantShadowIntensity;
	StaticShadowInfo.TotalShadowIntensity = NewStaticShadowInfo.TotalShadowIntensity;

	// Update the lights from the environment.
	CreateEnvironmentLightList(NewAffectingDominantLight, NewDominantShadowTransitionDistance);
}

void FDynamicLightEnvironmentState::Tick(FLOAT DeltaTime)
{
	INC_DWORD_STAT(STAT_NumEnvironments);

	const FVector PreviousPredictedOwnerPosition = PredictedOwnerPosition;
	const FBoxSphereBounds PreviousOwnerBounds = OwnerBounds;

	// Stagger the forced static updates to avoid a hitch when many DLE's need a static update
	const UBOOL bNeedsStaticUpdateThisFrame = bNeedsStaticUpdate && (GFrameCounter % 10 == RandHelper(9));

	if (bFirstFullUpdate || bNeedsStaticUpdateThisFrame || Component->bRequiresNonLatentUpdates)
	{
		// The first time a light environment is ticked, perform a full update.
		Update();
		bFirstFullUpdate = FALSE;
		bNeedsStaticUpdate = FALSE;
	}
	else if (!bFirstFullUpdate)
	{
		FLOAT LastRenderTime = -FLT_MAX;
		for (INT ComponentIndex = 0; ComponentIndex < Component->AffectedComponents.Num(); ComponentIndex++)
		{
			UPrimitiveComponent* CurrentComponent = Component->AffectedComponents(ComponentIndex);
			if (CurrentComponent && CurrentComponent->LastRenderTime > LastRenderTime)
			{
				LastRenderTime = CurrentComponent->LastRenderTime;
			}
		}
		const UBOOL bVisible = (GWorld->GetTimeSeconds() - LastRenderTime) < 1.0f;
		UBOOL bForceDynamicLightUpdate = FALSE;

		ULightComponent* NewAffectingDominantLight = Component->AffectingDominantLight;
		FLOAT NewDominantShadowTransitionDistance = CurrentDominantShadowTransitionDistance;

		UBOOL bNeedsUpdateBasedOnComponents = TRUE;

		if (Component->bDynamic)
		{
			const FVector LastOwnerPosition = OwnerBounds.Origin;
			// Update the owner bounds and other info.
			bNeedsUpdateBasedOnComponents = UpdateOwner();

			if (bNeedsUpdateBasedOnComponents)
			{
				if (PredictedOwnerPosition != PreviousPredictedOwnerPosition 
					|| OwnerBounds.SphereRadius != PreviousOwnerBounds.SphereRadius)
				{
					// Only update the dominant shadow transition if the DLE has moved
					CalculateDominantShadowTransitionDistance(NewAffectingDominantLight, NewDominantShadowTransitionDistance);
				}

				// Determine the distance of the light environment's owner from the closest local player's view.
				FLOAT MinPlayerDistanceSquared = 0.0f;
				if (GIsGame)
				{
					MinPlayerDistanceSquared = Square(WORLD_MAX);
					for (INT PlayerIndex = 0; PlayerIndex < GEngine->GamePlayers.Num(); PlayerIndex++)
					{
						if (GEngine->GamePlayers(PlayerIndex))
						{
							const FVector& PlayerViewLocation = GEngine->GamePlayers(PlayerIndex)->LastViewLocation;
							MinPlayerDistanceSquared = Min(MinPlayerDistanceSquared,(PlayerViewLocation - OwnerBounds.Origin).SizeSquared());
						}
					}
				}
				const FLOAT FullSpeedUpdateMaxDistance = Max(DELTA,OwnerBounds.SphereRadius * 4.0f);
				const FLOAT TimeBetweenUpdatesDistanceFactor = Clamp(appSqrt(MinPlayerDistanceSquared) / FullSpeedUpdateMaxDistance, 1.0f, 10.0f);

				// Determine if the light environment's primitives have been rendered in the last second.
				const FLOAT TimeSinceLastUpdate	= GWorld->GetTimeSeconds() - LastUpdateTime;
				const FLOAT TimeBetweenUpdatesVisibilityFactor = bVisible ?
					MinTimeBetweenFullUpdates :
					InvisibleUpdateTime;

				const FLOAT DistanceTravelled = (LastOwnerPosition - OwnerBounds.Origin).Size();
				const FLOAT TimeBetweenUpdatesVelocityFactor = Clamp(DistanceTravelled * Component->VelocityUpdateTimeScale / Max(DeltaTime, DELTA), 1.0f, 10.0f);

				// Only update the light environment if it's visible, or it hasn't been updated for the last InvisibleUpdateTime seconds.
				const FLOAT TimeBetweenUpdates = TimeBetweenUpdatesVisibilityFactor * TimeBetweenUpdatesDistanceFactor / TimeBetweenUpdatesVelocityFactor;
				const UBOOL bDynamicUpdateNeeded = TimeSinceLastUpdate > TimeBetweenUpdates;
				UBOOL bPerformFullUpdate = FALSE;
				if (bDynamicUpdateNeeded)
				{
					LastUpdateTime = GWorld->GetTimeSeconds();
					LastInterpolatePosition = PredictedOwnerPosition;

					// Only perform an update of the static light environment after the first update if the primitive has moved.
					if (PredictedOwnerPosition != PreviousPredictedOwnerPosition
						|| OwnerBounds.SphereRadius != PreviousOwnerBounds.SphereRadius)
					{
						UpdateStaticEnvironment(NewAffectingDominantLight);
					}

					// Spread out updating light environments over several frames to avoid frame time spikes for level
					// placed light environments that start being ticked on the same frame.
					InvisibleUpdateTime = Component->InvisibleUpdateTime * (0.8 + 0.4 * appSRand());
					MinTimeBetweenFullUpdates = Component->MinTimeBetweenFullUpdates * (0.8 + 0.4 * appSRand());
				}

				// Interpolate toward the previously calculated light environment state.
				UpdateEnvironmentInterpolation(DeltaTime,TimeBetweenUpdates);

				// Also update the environment if it's within the full-speed update radius.
				if(MinPlayerDistanceSquared < Square(FullSpeedUpdateMaxDistance))
				{
					bForceDynamicLightUpdate = TRUE;
				}
			}
		}

		// Skip updating the dynamic environment or synthesized lights for light environments on primitives that aren't visible.
		if (bNeedsUpdateBasedOnComponents && (bVisible || bForceDynamicLightUpdate))
		{
			// Update the effect of dynamic lights on the light environment.
			UpdateDynamicEnvironment();

			// Create lights to represent the environment.
			CreateEnvironmentLightList(NewAffectingDominantLight, NewDominantShadowTransitionDistance);
		}
	}

#if !FINAL_RELEASE
	// Draw debug info if we are in game
	// DrawLightEnvironmentDebugInfo is used to draw debug info in the editor
	if (!GLightEnvironmentDebugInfo.Component || GLightEnvironmentDebugInfo.Component == Component)
	{
		if (GLightEnvironmentDebugInfo.bShowNames)
		{
			Component->Owner->DrawDebugString(OwnerBounds.Origin, Component->Owner->GetPathName(), NULL, FColor( 255, 0, 0, 255 ), .05f);
		}

		if (GLightEnvironmentDebugInfo.bShowBounds)
		{
			for (INT SampleIndex = 0; SampleIndex < LightVisibilitySamplePoints.Num(); SampleIndex++)
			{
				const FVector SamplePosition = OwnerBounds.Origin + LightVisibilitySamplePoints(SampleIndex) * OwnerBounds.BoxExtent;
				Component->Owner->DrawDebugPoint(SamplePosition, 12.0f, FLinearColor::White, FALSE);
			}
			Component->Owner->DrawDebugSphere(OwnerBounds.Origin, OwnerBounds.SphereRadius, 6, 200, 200, 255, FALSE);
		}

		if (GLightEnvironmentDebugInfo.bShowVolumeInterpolation)
		{
			const UBOOL bDesiredHiddenState = GLightEnvironmentDebugInfo.Component == NULL || GLightEnvironmentDebugInfo.Component != Component;
			for (INT MeshIndex = 0; MeshIndex < Component->State->DebugVolumeSampleMeshes.Num(); MeshIndex++)
			{
				UStaticMeshComponent* Mesh = Component->State->DebugVolumeSampleMeshes(MeshIndex);
				const UBOOL bCurrentHiddenState = Mesh->HiddenGame && Mesh->HiddenEditor;
				if (bCurrentHiddenState != bDesiredHiddenState)
				{
					Mesh->SetHiddenGame(bDesiredHiddenState);
					Mesh->SetHiddenEditor(bDesiredHiddenState);
				}
			}
			if (!GLightEnvironmentDebugInfo.Component)
			{
				for (INT i = 0; i < DebugInterpolatedVolumeSamples.Num(); i++)
				{
					const FVolumeLightingSample& CurrentSample = DebugInterpolatedVolumeSamples(i);
					FSHVectorRGB IncidentRadiance;
					CurrentSample.ToSHVector(IncidentRadiance, Component->bIsCharacterLightEnvironment);
					const FLinearColor AverageColor = IncidentRadiance.CalcIntegral() / FSHVector::ConstantBasisIntegral;
					Component->Owner->DrawDebugPoint(CurrentSample.Position, 8.0f, AverageColor, FALSE);
					//Component->Owner->DrawDebugSphere(CurrentSample.Position, CurrentSample.Radius, 12, 155, 155, 100, FALSE);
				}
			}
		}
		if (GLightEnvironmentDebugInfo.bShowDominantLightTransition)
		{
			for (INT i = 0; i < Component->State->DebugClosestDominantLightRays.Num(); i++)
			{
				const FDebugShadowRay& CurrentRay = Component->State->DebugClosestDominantLightRays(i);
				const FColor LineColor = CurrentRay.bHit ? FColor(255, 0, 0):  FColor(255, 255, 255);
				Component->Owner->DrawDebugLine(CurrentRay.Start, CurrentRay.End, LineColor.R, LineColor.G, LineColor.B, FALSE);
			}
		}
		if (GLightEnvironmentDebugInfo.bShowVisibility)
		{
			for (INT i = 0; i < Component->State->DebugDynamicVisibilityTraces.Num(); i++)
			{
				const FDebugShadowRay& CurrentRay = Component->State->DebugDynamicVisibilityTraces(i);
				const FColor LineColor = CurrentRay.bHit ? FColor(255, 0, 0):  FColor(255, 255, 255);
				Component->Owner->DrawDebugLine(CurrentRay.Start, CurrentRay.End, LineColor.R, LineColor.G, LineColor.B, FALSE);
			}
			for (INT i = 0; i < Component->State->DebugStaticVisibilityTraces.Num(); i++)
			{
				const FDebugShadowRay& CurrentRay = Component->State->DebugStaticVisibilityTraces(i);
				const FColor LineColor = CurrentRay.bHit ? FColor(255, 0, 0):  FColor(255, 255, 255);
				Component->Owner->DrawDebugLine(CurrentRay.Start, CurrentRay.End, LineColor.R, LineColor.G, LineColor.B, FALSE);
			}
		}
		if (GLightEnvironmentDebugInfo.bShowStaticUpdates)
		{
			for (INT i = 0; i < Component->State->DebugStaticUpdates.Num(); i++)
			{
				const FVector& CurrentPosition = Component->State->DebugStaticUpdates(i);
				Component->Owner->DrawDebugSphere(CurrentPosition, 50.0f, 6, 155, 155, 100, FALSE);
			}
		}
		if (GLightEnvironmentDebugInfo.bShowCreateLights)
		{
			for (INT i = 0; i < Component->State->DebugCreateLights.Num(); i++)
			{
				const FVector& CurrentPosition = Component->State->DebugCreateLights(i);
				Component->Owner->DrawDebugPoint(CurrentPosition, 8.0f, FColor(200, 155, 100), FALSE);
			}
		}
	}
#endif
}

ULightComponent* FDynamicLightEnvironmentState::CreateRepresentativeLight(const FVector& Direction,const FLinearColor& Intensity)
{
	// Construct a point light to represent the brightest direction of the remaining light environment.
	UDirectionalLightComponent* Light = AllocateLight<UDirectionalLightComponent>();
	const FVector LightDirection = Direction.SafeNormal();
	Light->LightingChannels = OwnerLightingChannels;
	Light->LightEnvironment = Component;
	Light->bCastCompositeShadow = TRUE;

	ComputeAndFixedColorAndIntensity(Intensity,Light->LightColor,Light->Brightness);

	Light->CastShadows = FALSE;

	return Light;
}

UPointLightComponent* FDynamicLightEnvironmentState::CreateRepresentativeShadowLight()
{
	// Construct a point light to represent the brightest direction of the remaining light environment.
	UPointLightComponent* Light = AllocateLight<UPointLightComponent>();
	Light->LightingChannels = OwnerLightingChannels;
	Light->LightEnvironment = Component;

	Light->CastShadows = TRUE;
	Light->ModShadowFadeoutTime = Component->ModShadowFadeoutTime;
	Light->ModShadowFadeoutExponent = Component->ModShadowFadeoutExponent;
	Light->ShadowPlane = ShadowPlane;

	Light->Brightness = 0;

	return Light;
}

void FDynamicLightEnvironmentState::DetachRepresentativeLights(UBOOL bAllLights)
{
	// Detach the environment's representative lights.
	for(INT LightIndex = 0;LightIndex < RepresentativeLightPool.Num();LightIndex++)
	{
		if (bAllLights || RepresentativeLightPool(LightIndex) != CurrentRepresentativeShadowLight)
		{
			RepresentativeLightPool(LightIndex)->ConditionalDetach();
		}
	}
}

void FDynamicLightEnvironmentState::CreateEnvironmentLightList(ULightComponent* NewAffectingDominantLight, FLOAT NewDominantShadowTransitionDistance, UBOOL bForceUpdate)
{
	SCOPE_CYCLE_COUNTER(STAT_CreateLightsTime);

	// If we're using composite dynamic lights, include their lighting contribution in the composite light environment.
	FSHVectorRGB CompositeLightEnvironment = StaticLightEnvironment + DynamicLightEnvironment;
	FSHVectorRGB CompositeNonShadowedLightEnvironment = StaticNonShadowedLightEnvironment + DynamicNonShadowedLightEnvironment;

	FLightEnvShadowInfo CompositeShadowInfo = StaticShadowInfo;
	// If the light environment is set to composite shadows from dynamic lights, include their lighting in the composite shadow environment.
	if(Component->bCompositeShadowsFromDynamicLights)
	{
		// Use the direction of whichever is brighter
		if (DynamicShadowInfo.TotalShadowIntensity.GetLuminance() > StaticShadowInfo.TotalShadowIntensity.GetLuminance())
		{
			CompositeShadowInfo.ShadowDirection = DynamicShadowInfo.ShadowDirection;
		}
		
		CompositeShadowInfo.DominantShadowIntensity += DynamicShadowInfo.DominantShadowIntensity;
		CompositeShadowInfo.TotalShadowIntensity += DynamicShadowInfo.TotalShadowIntensity;
	}
	CompositeShadowInfo.ShadowDirection = CompositeShadowInfo.ShadowDirection.SafeNormal();

	NewAffectingDominantLight = NewDominantShadowTransitionDistance < Component->DominantShadowTransitionStartDistance ? NewAffectingDominantLight : NULL;

	if (GLightEnvironmentDebugInfo.bShowIndirectLightingOnly && (!GLightEnvironmentDebugInfo.Component || Component == GLightEnvironmentDebugInfo.Component))
	{
		// Disable the dominant light if we are visualizing indirect lighting only
		NewAffectingDominantLight = NULL;
	}

	const FLOAT InvTransitionRange = 1.0f / (Max(Component->DominantShadowTransitionStartDistance - Component->DominantShadowTransitionEndDistance, 1.0f));
	const FLOAT TransitionFraction = Clamp((NewDominantShadowTransitionDistance - Component->DominantShadowTransitionEndDistance) * InvTransitionRange, 0.0f, 1.0f);

	// Apply a power to the linear fade factors to get a smoother looking fade
	// Use whichever is more faded out, the DominantShadowFactor which is interpolated over time or TransitionFraction, which is based on distance to the dominant shadow transition
	// We have to take TransitionFraction into account here to avoid a pop when NewAffectingDominantLight is determined no longer visible based on the dominant shadow transition distance.
	const FLOAT EffectiveDominantShadowFactor = Min(CompositeShadowInfo.DominantShadowFactor * CompositeShadowInfo.DominantShadowFactor * CompositeShadowInfo.DominantShadowFactor, 1.0f - TransitionFraction * TransitionFraction * TransitionFraction);
	if (Component->bUseBooleanEnvironmentShadowing
		&& (Abs(EffectiveDominantShadowFactor - CurrentShadowInfo.DominantShadowFactor) > .01f
		|| Abs(Component->DominantShadowFactor - CurrentShadowInfo.DominantShadowFactor) > .01f)
		|| !Component->bUseBooleanEnvironmentShadowing && Component->DominantShadowFactor < 1.0f)
	{
		CurrentShadowInfo.DominantShadowFactor = EffectiveDominantShadowFactor;
		Component->DominantShadowFactor = EffectiveDominantShadowFactor;
		for (INT ComponentIndex = 0; ComponentIndex < Component->AffectedComponents.Num(); ComponentIndex++)
		{
			UPrimitiveComponent* CurrentComponent = Component->AffectedComponents(ComponentIndex);
			// Push the updated shadow factor to the rendering thread
			if (CurrentComponent && CurrentComponent->IsAttached() && CurrentComponent->SceneInfo)
			{
				ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
					UpdateDominantShadowFactor,
					FPrimitiveSceneInfo*,PrimitiveSceneInfo,CurrentComponent->SceneInfo,
					FLOAT,DominantShadowFactor,EffectiveDominantShadowFactor,
				{
					PrimitiveSceneInfo->DominantShadowFactor = DominantShadowFactor;
				});
			}
		}
	}

	// Only update the representative lights if the environment has changed substantially from the last representative light update.
	static const FLOAT ErrorThreshold = Square(1.0f / 256.0f);
	const FLOAT LightError = GetSquaredDifferenceIntegral(CompositeLightEnvironment,CurrentRepresentativeLightEnvironment);
	const FLOAT NonShadowedLightError = GetSquaredDifferenceIntegral(CompositeNonShadowedLightEnvironment,CurrentRepresentativeNonShadowedLightEnvironment);
	if(	LightError > ErrorThreshold ||
		NonShadowedLightError > ErrorThreshold || 
		// Update if the transition fade has changed by 1%
		Abs(NewDominantShadowTransitionDistance - CurrentDominantShadowTransitionDistance) > Component->DominantShadowTransitionStartDistance * .01f ||
		NewAffectingDominantLight != Component->AffectingDominantLight ||
		bForceUpdate)
	{
		if (GLightEnvironmentDebugInfo.bShowCreateLights 
			&& (!GLightEnvironmentDebugInfo.Component || GLightEnvironmentDebugInfo.Component == Component))
		{
			DebugCreateLights.AddItem(OwnerBounds.Origin);
		}

		CurrentRepresentativeLightEnvironment = CompositeLightEnvironment;
		CurrentRepresentativeNonShadowedLightEnvironment = CompositeNonShadowedLightEnvironment;
		CurrentDominantShadowTransitionDistance = NewDominantShadowTransitionDistance;

		// Detach the old representative lights.
		DetachRepresentativeLights(FALSE);

		if (Component->bForceCompositeAllLights && Component->AffectingDominantLight)
		{
			// Set AffectingDominantLight to NULL if all lights are being composited.  
			// AffectingDominantLight can be non-null if bForceCompositeAllLights was changed since the last update.
			Component->AffectingDominantLight = NULL;
			// Copy the affected components array since reattaching the components will modify the array
			TArray<UPrimitiveComponent*,TInlineAllocator<5> > TempAffectedComponents = Component->AffectedComponents;
			for (INT ComponentIndex = 0; ComponentIndex < TempAffectedComponents.Num(); ComponentIndex++)
			{
				UPrimitiveComponent* CurrentComponent = TempAffectedComponents(ComponentIndex);
				if (CurrentComponent)
				{
					CurrentComponent->BeginDeferredReattach();
				}
			}
		}
		else if (!Component->bForceCompositeAllLights)
		{
			// Determine a single shadow value for translucency that is not allowed to receive dynamic shadows based on the transition distance
			// This will produce really coarse shadowing but is much cheaper for translucency than receiving dynamic shadows
			const UBOOL bNewTranslucencyShadowed = CurrentDominantShadowTransitionDistance > Component->DominantShadowTransitionEndDistance;
			// If using boolean shadowing from the environment, no preshadows are allowed to handle the environment shadowing this DLE.
			const UBOOL bNewAllowPreShadow = !Component->bUseBooleanEnvironmentShadowing;

			if (Component->AffectingDominantLight != NewAffectingDominantLight)
			{
				Component->AffectingDominantLight = NewAffectingDominantLight;

				for (INT ComponentIndex = 0; ComponentIndex < Component->AffectedComponents.Num(); ComponentIndex++)
				{
					UPrimitiveComponent* CurrentComponent = Component->AffectedComponents(ComponentIndex);
					if (CurrentComponent && CurrentComponent->GetScene())
					{
						// Use the fast update command to propagate the change to AffectingDominantLight
						CurrentComponent->GetScene()->UpdatePrimitiveAffectingDominantLight(CurrentComponent, NewAffectingDominantLight);
					}
				}
			}

			if (!Component->bAllowDynamicShadowsOnTranslucency && Component->bTranslucencyShadowed != bNewTranslucencyShadowed
				|| Component->bAllowPreShadow != bNewAllowPreShadow
				)
			{
				Component->bTranslucencyShadowed = bNewTranslucencyShadowed;
				Component->bAllowPreShadow = bNewAllowPreShadow;
				
				// Reattach all primitive components using this light environment so that they recreate their light interactions 
				// Copy the affected components array since reattaching the components will modify the array
				TArray<UPrimitiveComponent*,TInlineAllocator<5> > TempAffectedComponents = Component->AffectedComponents;
				for (INT ComponentIndex = 0; ComponentIndex < TempAffectedComponents.Num(); ComponentIndex++)
				{
					UPrimitiveComponent* CurrentComponent = TempAffectedComponents(ComponentIndex);
					if (CurrentComponent)
					{
						CurrentComponent->BeginDeferredReattach();
					}
				}
			}
		}

		// Fade the primary light out as the DLE gets closer to the dominant shadow transition
		// This ensures that there is only one directional light affecting the DLE most of the time
		const FLOAT PrimaryLightWeight = TransitionFraction;
		const FLOAT ContrastFactor = Component->bIsCharacterLightEnvironment ? GWorld->GetWorldInfo(TRUE)->CharacterLightingContrastFactor : 1.0f;

		if(Component->bSynthesizeDirectionalLight 
			&& GLightEnvironmentDebugInfo.bShowPrimaryLight
			&& CurrentDominantShadowTransitionDistance > Component->DominantShadowTransitionEndDistance)
		{
			FVector DominantLightDirection;
			FLinearColor DominantLightIntensity;
			if (ExtractDominantLight(CompositeLightEnvironment,DominantLightDirection,DominantLightIntensity, PrimaryLightWeight))
			{
				if (DominantLightIntensity.R > 0.0f || DominantLightIntensity.G > 0.0f || DominantLightIntensity.B > 0.0f)
				{
					// Create a directional light that is representative of the light environment.
					ULightComponent* Light = CreateRepresentativeLight(
						DominantLightDirection,
						// Scale the contribution of the primary light up to increase contrast
						DominantLightIntensity * ContrastFactor
						);

					// Attach the light after it is associated with the light environment to ensure it is only attached once.
					Light->ConditionalAttach(Component->GetScene(),NULL,FRotationMatrix((-DominantLightDirection).Rotation()));
				}
			}
		}

		// Include the remaining shadowed light that couldn't be represented by the directional light in the secondary lighting,
		// in addition to all the lights that aren't modulated by the composite shadow.
		FSHVectorRGB SecondaryLightEnvironment = CompositeLightEnvironment + CompositeNonShadowedLightEnvironment;

		if (GLightEnvironmentDebugInfo.bShowSecondaryLight)
		{
			// Scale the contribution of the secondary light down to increase contrast
			const FLOAT SecondaryLightWeight = 1.0f / Lerp(1.0f, ContrastFactor, PrimaryLightWeight);
			if (Component->bSynthesizeSHLight && 
				GSystemSettings.bAllowSHSecondaryLighting )
			{
				// Create a SH light for the lights not represented by the directional lights.
				USphericalHarmonicLightComponent* SHLight = AllocateLight<USphericalHarmonicLightComponent>();
				SHLight->LightingChannels = OwnerLightingChannels;
				SHLight->LightEnvironment = Component;
				SHLight->WorldSpaceIncidentLighting = SecondaryLightEnvironment * SecondaryLightWeight;
				SHLight->bCastCompositeShadow = FALSE;

				// Combine the SH light into the base pass if the light environment is not casting shadows
				// Otherwise mod shadows would darken the SH light
				SHLight->bRenderBeforeModShadows = !(Component->bCastShadows && GSystemSettings.bAllowLightEnvironmentShadows);
		    
				// Attach the SH light after it is associated with the light environment to ensure it is only attached once.
				SHLight->ConditionalAttach(Component->GetScene(),NULL,FMatrix::Identity);
			}
			else
			{
				// Move as much light as possible into the sky light.
				FLinearColor LowerSkyLightColor(FLinearColor::Black);
				FLinearColor UpperSkyLightColor(FLinearColor::Black);
				ExtractEnvironmentSkyLight(SecondaryLightEnvironment,UpperSkyLightColor,FALSE,TRUE);
				ExtractEnvironmentSkyLight(SecondaryLightEnvironment,LowerSkyLightColor,TRUE,FALSE);

				// Create a sky light for the lights not represented by the directional lights.
				USkyLightComponent* SkyLight = AllocateLight<USkyLightComponent>();
				SkyLight->LightingChannels = OwnerLightingChannels;
				SkyLight->LightEnvironment = Component;
				SkyLight->bCastCompositeShadow = FALSE;

				// Desaturate sky light color and add ambient glow afterwards.
				UpperSkyLightColor = UpperSkyLightColor * SecondaryLightWeight;
				LowerSkyLightColor = LowerSkyLightColor * SecondaryLightWeight;

				// Convert linear color to color and brightness pair.
				ComputeAndFixedColorAndIntensity(UpperSkyLightColor,SkyLight->LightColor,SkyLight->Brightness);
				ComputeAndFixedColorAndIntensity(LowerSkyLightColor,SkyLight->LowerColor,SkyLight->LowerBrightness);

				// Attach the skylight after it is associated with the light environment to ensure it is only attached once.
				SkyLight->ConditionalAttach(Component->GetScene(),NULL,FMatrix::Identity);
			}
		}
	}

	// Update if the shadow direction changes by .5 degrees
	// Anything less has noticeable hitching when the shadow direction is changing quickly
	if ((CompositeShadowInfo.ShadowDirection | CurrentShadowInfo.ShadowDirection) < .99996f ||
		Abs(CompositeShadowInfo.TotalShadowIntensity.GetLuminance() - CurrentShadowInfo.TotalShadowIntensity.GetLuminance()) > .01f ||
		bForceUpdate)
	{
		CurrentShadowInfo = CompositeShadowInfo;

		// Create a shadow-only point light that is representative of the shadow-casting light environment.
		if( Component->bCastShadows 
			&& GSystemSettings.bAllowLightEnvironmentShadows 
			&& GLightEnvironmentDebugInfo.bShowShadows
			// Disable the modulated shadow if the DLE is visible to the dominant light
			&& NewDominantShadowTransitionDistance > Component->DominantShadowTransitionEndDistance
			&& CompositeShadowInfo.ShadowDirection.SizeSquared() > DELTA)
		{
			// Use a shadow color that lets through light proportional to the shadowing not represented by the dominant shadow direction.
			FLinearColor DominantShadowIntensityRatio(
				Min(1.0f,(CompositeShadowInfo.TotalShadowIntensity.R - CompositeShadowInfo.DominantShadowIntensity.R) / Max(CompositeShadowInfo.TotalShadowIntensity.R,DELTA)),
				Min(1.0f,(CompositeShadowInfo.TotalShadowIntensity.G - CompositeShadowInfo.DominantShadowIntensity.G) / Max(CompositeShadowInfo.TotalShadowIntensity.G,DELTA)),
				Min(1.0f,(CompositeShadowInfo.TotalShadowIntensity.B - CompositeShadowInfo.DominantShadowIntensity.B) / Max(CompositeShadowInfo.TotalShadowIntensity.B,DELTA))
				);

			// Clamp the modulated shadow intensity
			DominantShadowIntensityRatio.R = Min(DominantShadowIntensityRatio.R, Component->MaxModulatedShadowColor.R);
			DominantShadowIntensityRatio.G = Min(DominantShadowIntensityRatio.G, Component->MaxModulatedShadowColor.G);
			DominantShadowIntensityRatio.B = Min(DominantShadowIntensityRatio.B, Component->MaxModulatedShadowColor.B);

			// Fade the modulated shadow out as the DLE gets closer to the dominant shadow transition
			// This ensures that there is only one dynamic shadow affecting the DLE most of the time
			DominantShadowIntensityRatio = Lerp(FLinearColor::White, DominantShadowIntensityRatio, TransitionFraction);

			if (!CurrentRepresentativeShadowLight)
			{
				// Create a new shadow light if needed
				CurrentRepresentativeShadowLight = CreateRepresentativeShadowLight();
			}

			CurrentRepresentativeShadowLight->ModShadowColor = DominantShadowIntensityRatio;

			// If the shadow light is already attached, do a lightweight update of ModShadowColor
			if (CurrentRepresentativeShadowLight->IsAttached())
			{
				Component->GetScene()->UpdateLightColorAndBrightness(CurrentRepresentativeShadowLight);
			}
		}
		else
		{
			// Detach the shadow light if it should be disabled
			if (CurrentRepresentativeShadowLight)
			{
				CurrentRepresentativeShadowLight->ConditionalDetach();
			}
			CurrentRepresentativeShadowLight = NULL;
		}
	}

	// Attach the shadow light at the appropriate position.
	if (CurrentRepresentativeShadowLight)
	{		
		// Don't allow the shadow to come from below the light environment's minimum shadow angle.
		const FLOAT AngleFromZAxis = Clamp(90.0f - Component->MinShadowAngle, 0.0f, 180.0f) * PI / 180.0f;
		FVector EffectiveShadowDirection = CurrentShadowInfo.ShadowDirection;
		EffectiveShadowDirection.Z = Max(CurrentShadowInfo.ShadowDirection.Z, appCos(AngleFromZAxis));
		EffectiveShadowDirection = EffectiveShadowDirection.UnsafeNormal();

		// Compute the light's position and transform.
		const FLOAT LightDistance = OwnerBounds.SphereRadius * Component->LightDistance;
		const FVector LightPosition = OwnerBounds.Origin + EffectiveShadowDirection * LightDistance;
		const FMatrix LightToWorld = FTranslationMatrix(LightPosition);

		// Update the light's radius for the owner's current bounding radius.
		CurrentRepresentativeShadowLight->Radius = OwnerBounds.SphereRadius * (Component->LightDistance + Component->ShadowDistance + 2);
		CurrentRepresentativeShadowLight->MinShadowFalloffRadius = OwnerBounds.SphereRadius * (Component->LightDistance + 1);
		const FLightingChannelContainer OriginalLightingChannels = CurrentRepresentativeShadowLight->LightingChannels;
		CurrentRepresentativeShadowLight->LightingChannels = OwnerLightingChannels;

		// If the shadow light is already attached, and the lighting channels have not changed, use a lightweight update
		if (CurrentRepresentativeShadowLight->IsAttached() && OriginalLightingChannels == OwnerLightingChannels)
		{
			// Move the light to the new position.
			CurrentRepresentativeShadowLight->ConditionalUpdateTransform(LightToWorld);

			// Update the light's color and brightness.
			Component->GetScene()->UpdateLightColorAndBrightness(CurrentRepresentativeShadowLight);
		}
		else
		{
			// Force a full reattach if the lighting channels have changed
			CurrentRepresentativeShadowLight->ConditionalDetach(TRUE);

			// Attach the light after it is associated with the light environment to ensure it is only attached once.
			CurrentRepresentativeShadowLight->ConditionalAttach(Component->GetScene(),NULL,LightToWorld);
		}

#if WITH_MOBILE_RHI
		if (GUsingMobileRHI 
			&& GSystemSettings.bAllowDynamicShadows 
			&& GSystemSettings.bMobileModShadows
			// Forward shadows are only used when depth textures are not supported
			&& !GSupportsDepthTextures)
		{
			FCheckResult CheckResult;
			// Find a mesh under the DLE, this will be the only mesh allowed to receive forward shadows from the DLE modulated shadow, in order to reduce draw calls
			// This can cause artifacts when the shadow is falling across multiple components.
			const UBOOL bHit = !GWorld->SingleLineCheck(CheckResult, NULL, OwnerBounds.Origin - FVector(0.0f, 0.0f, OwnerBounds.SphereRadius * 5), OwnerBounds.Origin, TRACE_LevelGeometry | TRACE_Level | TRACE_Terrain | TRACE_ComplexCollision);
			TArray<UPrimitiveComponent*> ReceiverComponents;
			
			if (bHit)
			{
				if (CheckResult.Component)
				{
					ReceiverComponents.AddItem(CheckResult.Component);
				}
				else if (CheckResult.Level)
				{
					for (INT Index = 0; Index < CheckResult.Level->ModelComponents.Num(); Index++)
					{
						ReceiverComponents.AddItem(CheckResult.Level->ModelComponents(Index));
					}
				}
			}

			CurrentRepresentativeShadowLight->UpdateForwardShadowReceivers(ReceiverComponents);
		}
#endif
	}
}

void FDynamicLightEnvironmentState::AddReferencedObjects(TArray<UObject*>& ObjectArray)
{
	// Add the light environment's representative lights.
	for(INT LightIndex = 0;LightIndex < RepresentativeLightPool.Num();LightIndex++)
	{
		UObject::AddReferencedObject(ObjectArray,RepresentativeLightPool(LightIndex));
	}

	for(INT LightIndex = 0;LightIndex < DebugVolumeSampleLights.Num();LightIndex++)
	{
		UObject::AddReferencedObject(ObjectArray,DebugVolumeSampleLights(LightIndex));
	}
	for(INT MeshIndex = 0;MeshIndex < DebugVolumeSampleMeshes.Num();MeshIndex++)
	{
		UObject::AddReferencedObject(ObjectArray,DebugVolumeSampleMeshes(MeshIndex));
	}
}

/** Forces a full update on the next Tick. */
void FDynamicLightEnvironmentState::ResetEnvironment()
{
	bFirstFullUpdate = TRUE;
}

UBOOL FDynamicLightEnvironmentState::IsLightVisible(const ULightComponent* Light, const FVector& OwnerPosition, UBOOL bIsDynamic, FLOAT& OutVisibilityFactor) const
{
	SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_DynamicLightVisibilityTime, bIsDynamic);
	SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_StaticLightVisibilityTime, !bIsDynamic);

	// Sky lights are always visible.
	if(Light->IsA(USkyLightComponent::StaticClass()))
	{
		OutVisibilityFactor = 1.0f;
		return TRUE;
	}

	// Lights which don't cast static shadows are always visible.
	if(!Light->CastShadows 
		|| !Light->CastStaticShadows 
		|| !Component->bShadowFromEnvironment
		// Normal shadowing (as opposed to modulated) is when a light's shadowing masks only that light's influence
		// DLE's do normal shadowing implicitly, so only do DLE shadowing when the light is setup to use normal shadowing.
		// Lightmapped lights also do normal shadowing implicitly, but dynamic lights only do normal shadowing if LightShadowMode is LightShadow_Normal.
		|| bIsDynamic && Light->LightShadowMode != LightShadow_Normal)
	{
		OutVisibilityFactor = 1.0f;
		return TRUE;
	}

	// Get the owning actor
	AActor* OwnerActor = Component ? Component->GetOwner() : NULL;

	// Compute light visibility for one or more points within the owner's bounds.
	INT NumVisibleSamples = 0;
	for(INT SampleIndex = 0;SampleIndex < LightVisibilitySamplePoints.Num();SampleIndex++)
	{
		FVector StartPosition = PredictedOwnerPosition;
		FVector4 LightPosition = Light->GetPosition();
		if (Component->bTraceFromClosestBoundsPoint || Component->bUseBooleanEnvironmentShadowing)
		{
			FVector LightVector = (FVector)LightPosition - PredictedOwnerPosition * LightPosition.W;
			LightVector.Normalize();
			StartPosition = PredictedOwnerPosition + LightVector * OwnerBounds.SphereRadius;
		}
		// Determine a random point to test visibility for in the owner's bounds.
		const FVector VisibilityTestPoint = StartPosition + LightVisibilitySamplePoints(SampleIndex) * OwnerBounds.BoxExtent;

		// Determine the direction from the primitive to the light.
		FVector LightVector = (FVector)LightPosition - VisibilityTestPoint * LightPosition.W;

		// Check the line between the light and the primitive's origin for occlusion.
		FCheckResult Hit(1.0f);
		const UBOOL bPointIsLit = GWorld->SingleLineCheck(
			Hit,
			OwnerActor,
			VisibilityTestPoint,
			VisibilityTestPoint + LightVector,
			TRACE_Level|TRACE_Actors|TRACE_ShadowCast|TRACE_StopAtAnyHit,
			FVector(0,0,0),
			const_cast<ULightComponent*>(Light)
			);
		if(bPointIsLit)
		{
			NumVisibleSamples++;
		}

		if (GLightEnvironmentDebugInfo.bShowVisibility 
			&& (!GLightEnvironmentDebugInfo.Component || GLightEnvironmentDebugInfo.Component == Component))
		{
			TArray<FDebugShadowRay>& DebugRays = bIsDynamic ? DebugDynamicVisibilityTraces : DebugStaticVisibilityTraces;
			DebugRays.AddItem((FDebugShadowRay(VisibilityTestPoint, bPointIsLit ? VisibilityTestPoint + LightVector : Hit.Location, !bPointIsLit)));
		}
	}

	OutVisibilityFactor = (FLOAT)NumVisibleSamples / (FLOAT)LightVisibilitySamplePoints.Num();

	return OutVisibilityFactor > 0.0f;
}

template<typename LightType>
LightType* FDynamicLightEnvironmentState::AllocateLight() const
{
	// Try to find an unattached light of matching type in the representative light pool.
	for(INT LightIndex = 0;LightIndex < RepresentativeLightPool.Num();LightIndex++)
	{
		ULightComponent* Light = RepresentativeLightPool(LightIndex);
		if(Light && !Light->IsAttached() && Light->IsA(LightType::StaticClass()))
		{
			return CastChecked<LightType>(Light);
		}
	}

	// Create a new light.
	LightType* NewLight = ConstructObject<LightType>(LightType::StaticClass(),Component);
	RepresentativeLightPool.AddItem(NewLight);
	return NewLight;
}

UBOOL FDynamicLightEnvironmentState::DoesLightAffectOwner(const ULightComponent* Light,const FVector& OwnerPosition) const
{
	// Skip disabled lights.
	if(!Light->bEnabled)
	{
		return FALSE;
	}

	// Use the CompositeDynamic lighting channel as the Dynamic lighting channel. 
	FLightingChannelContainer ConvertedLightingChannels = Light->LightingChannels;
	ConvertedLightingChannels.Dynamic = FALSE;
	if(ConvertedLightingChannels.CompositeDynamic)
	{
		ConvertedLightingChannels.CompositeDynamic = FALSE;
		ConvertedLightingChannels.Dynamic = TRUE;
	}

	// Skip lights which don't affect the owner's lighting channels.
	if(!ConvertedLightingChannels.OverlapsWith(OwnerLightingChannels))
	{
		return FALSE;
	}

	// Skip lights which don't affect the owner's predicted bounds.
	if(!Light->AffectsBounds(FBoxSphereBounds(OwnerPosition,OwnerBounds.BoxExtent,OwnerBounds.SphereRadius)))
	{
		return FALSE;
	}

	return TRUE;
}

void FDynamicLightEnvironmentState::AddLightToEnvironment(
	const ULightComponent* Light, 
	FSHVectorRGB& ShadowedLightEnvironment,
	FSHVectorRGB& NonShadowedLightEnvironment,
	FSHVectorRGB& ShadowEnvironment,
	const FBoxSphereBounds& OwnerBounds,
	UBOOL bIsDynamic
	)
{
	// Determine whether the light affects the owner, and its visibility factor.
	FLOAT VisibilityFactor;
	//Make sure we're allowing standard compositing into the DLE
	if (Light->bAllowCompositingIntoDLE
		&& DoesLightAffectOwner(Light,OwnerBounds.Origin) 
		&& IsLightVisible(Light,OwnerBounds.Origin,bIsDynamic,VisibilityFactor)
		// Don't allow lights smaller than half the DLE bounds to affect the DLE if bAffectedBySmallDynamicLights is FALSE
		&& (Component->bAffectedBySmallDynamicLights || Square(OwnerBounds.SphereRadius) < Light->GetBoundingBox().GetExtent().SizeSquared()))
	{
		// If the light doesn't cast composite shadows, add its lighting to the light environment SH that isn't modulated by the composite shadow.
		FSHVectorRGB& LightEnvironment =
			Light->bCastCompositeShadow ? 
				ShadowedLightEnvironment :
				NonShadowedLightEnvironment;

		if(Light->IsA(USkyLightComponent::StaticClass()))
		{
			const USkyLightComponent* SkyLight = ConstCast<USkyLightComponent>(Light);

			// Compute the sky light's effect on the environment SH.
			const FSHVectorRGB IndividualSkyLightEnvironment = GetSkyLightEnvironment(Component,SkyLight);

			// Add the sky light to the light environment SH.
			LightEnvironment += IndividualSkyLightEnvironment;

			if(Light->bCastCompositeShadow)
			{
				FSHVectorRGB IndividualShadowEnvironment;
				if(!Light->bAffectCompositeShadowDirection)
				{
					// If the sky light is set to not affect the shadow direction, add it to the shadow environment as an ambient light.
					IndividualShadowEnvironment = FSHVector::AmbientFunction() * IndividualSkyLightEnvironment.CalcIntegral();
				}
				else
				{
					// Add the sky light to the shadow environment SH.
					IndividualShadowEnvironment = IndividualSkyLightEnvironment;
				}

				// Use the ModShadowColor to blend between adding the light directly to the light environment or as an ambient shadow source that
				// lightens the composite shadow.
				ShadowEnvironment += FSHVector::AmbientFunction() * (IndividualShadowEnvironment.CalcIntegral() * Light->ModShadowColor);
				ShadowEnvironment += IndividualShadowEnvironment * (FLinearColor::White - Light->ModShadowColor);
			}
		}
		else
		{
			// Determine the direction from the primitive to the light.
			const FVector4 LightPosition = Light->GetPosition();
			const FVector LightVector = ((FVector)LightPosition - OwnerBounds.Origin * LightPosition.W).SafeNormal();

			// Compute the light's intensity at the actor's origin.
			const FLinearColor Intensity = Light->GetDirectIntensity(OwnerBounds.Origin) * VisibilityFactor;

			const UBOOL bUseCompositeDynamicLights = (GSystemSettings.bUseCompositeDynamicLights && !Component->bForceNonCompositeDynamicLights)
				// Don't composite lights that have a light function
				&& (Light->Function == NULL);
			if(!bIsDynamic || bUseCompositeDynamicLights)
			{
				// Add the light to the light environment SH.
				const FSHVectorRGB IndividualLightEnvironment = SHBasisFunction(LightVector) * Intensity;
				LightEnvironment += IndividualLightEnvironment;
			}

			if(Light->bCastCompositeShadow)
			{
				FSHVectorRGB IndividualShadowEnvironment;
				if(Light->bAffectCompositeShadowDirection)
				{
					// Add the light to the shadow casting environment SH.
					IndividualShadowEnvironment = SHBasisFunction(LightVector) * Intensity;
				}
				else
				{
					// Add the light to the shadow casting environment SH as an ambient light.
					IndividualShadowEnvironment = FSHVector::AmbientFunction() * (Intensity * (1.0f / (2.0f * appSqrt(PI))));
				}

				// Use the ModShadowColor to blend between adding the light directly to the light environment or as an ambient shadow source that
				// lightens the composite shadow.
				ShadowEnvironment += FSHVector::AmbientFunction() * (IndividualShadowEnvironment.CalcIntegral() * Light->ModShadowColor);
				ShadowEnvironment += IndividualShadowEnvironment * (FLinearColor::White - Light->ModShadowColor);
			}
		}
	}
}

/** 
 * Calculates the minimum distance to a dominant shadow transition, or 0 if not shadowed by a dominant light.
 * NewAffectingDominantLight will be the dominant light whose shadow transition is closest.
 */
void FDynamicLightEnvironmentState::CalculateDominantShadowTransitionDistance(ULightComponent*& NewAffectingDominantLight, FLOAT& NewDominantShadowTransitionDistance)
{
	SCOPE_CYCLE_COUNTER(STAT_DominantShadowTransitions);

	NewAffectingDominantLight = NULL;
	NewDominantShadowTransitionDistance = Component->DominantShadowTransitionStartDistance;
	if (!Component->bForceCompositeAllLights)
	{
		DebugClosestDominantLightRays.Empty();
		UBOOL bLightingIsBuilt;
		FLOAT ClosestDistanceSquared = FLT_MAX;
		if (GWorld->DominantDirectionalLight)
		{
			// Search for the distance to the dominant shadow transition
			FLOAT DominantDirectionalShadowTransitionDistance = 0.0f;
			if( Component->bAlwaysInfluencedByDominantDirectionalLight )
			{
				bLightingIsBuilt =
					GWorld->DominantDirectionalLight->IsDominantLightShadowMapValid() ||
					GWorld->DominantDirectionalLight->GetOwner()->bMovable;
			}
			else
			{
				DominantDirectionalShadowTransitionDistance = GWorld->DominantDirectionalLight->GetDominantShadowTransitionDistance(
					OwnerBounds, 
					Component->DominantShadowTransitionStartDistance, 
					GLightEnvironmentDebugInfo.bShowDominantLightTransition, 
					DebugClosestDominantLightRays,
					bLightingIsBuilt);
			}

			FLOAT VisibilityFactor;
			if (DominantDirectionalShadowTransitionDistance < NewDominantShadowTransitionDistance
				&& (bLightingIsBuilt 
				// Trace a ray to determine shadowing if static lighting data is not built for the light
				|| DoesLightAffectOwner(GWorld->DominantDirectionalLight, PredictedOwnerPosition) 
				&& IsLightVisible(GWorld->DominantDirectionalLight, PredictedOwnerPosition, FALSE, VisibilityFactor)))
			{
				NewAffectingDominantLight = GWorld->DominantDirectionalLight;
				NewDominantShadowTransitionDistance = DominantDirectionalShadowTransitionDistance;
			}
		}
		
		for (TSparseArray<UDominantPointLightComponent*>::TIterator LightIt(GWorld->DominantPointLights); LightIt; ++LightIt)
		{
			UDominantPointLightComponent* CurrentLight = *LightIt;
			const FLOAT CurrentDistanceSquared = (CurrentLight->GetOrigin() - PredictedOwnerPosition).SizeSquared();
			// Only continue checking lights if we're shadowed or we are unshadowed but this light is closer than NewAffectingDominantLight
			if (NewDominantShadowTransitionDistance > KINDA_SMALL_NUMBER || CurrentDistanceSquared < ClosestDistanceSquared)
			{
				const FLOAT DominantPointShadowTransitionDistance = 
					CurrentLight->GetDominantShadowTransitionDistance(
					OwnerBounds, 
					Component->DominantShadowTransitionStartDistance, 
					GLightEnvironmentDebugInfo.bShowDominantLightTransition, 
					DebugClosestDominantLightRays,
					bLightingIsBuilt);

				FLOAT VisibilityFactor;
				if ((bLightingIsBuilt 
					// Trace a ray to determine shadowing if static lighting data is not built for the light
					|| DoesLightAffectOwner(CurrentLight, PredictedOwnerPosition) 
					&& IsLightVisible(CurrentLight, PredictedOwnerPosition, FALSE, VisibilityFactor))
					// Use this light if it is closer to being visible than NewAffectingDominantLight,
					&& (DominantPointShadowTransitionDistance < NewDominantShadowTransitionDistance
					// Or if both this light and NewAffectingDominantLight are unshadowed but this light is closer to the DLE
					|| NewDominantShadowTransitionDistance < KINDA_SMALL_NUMBER
					&& Abs(DominantPointShadowTransitionDistance - NewDominantShadowTransitionDistance) < KINDA_SMALL_NUMBER
					&& CurrentDistanceSquared < ClosestDistanceSquared))
				{
					NewAffectingDominantLight = CurrentLight;
					NewDominantShadowTransitionDistance = DominantPointShadowTransitionDistance;
					ClosestDistanceSquared = CurrentDistanceSquared;
				}
			}
		}

		for (TSparseArray<UDominantSpotLightComponent*>::TIterator LightIt(GWorld->DominantSpotLights); LightIt; ++LightIt)
		{
			UDominantSpotLightComponent* CurrentLight = *LightIt;
			const FLOAT CurrentDistanceSquared = (CurrentLight->GetOrigin() - PredictedOwnerPosition).SizeSquared();
			if (NewDominantShadowTransitionDistance > KINDA_SMALL_NUMBER || CurrentDistanceSquared < ClosestDistanceSquared)
			{
				const FLOAT DominantSpotShadowTransitionDistance = 
					CurrentLight->GetDominantShadowTransitionDistance(
					OwnerBounds, 
					Component->DominantShadowTransitionStartDistance, 
					GLightEnvironmentDebugInfo.bShowDominantLightTransition, 
					DebugClosestDominantLightRays,
					bLightingIsBuilt);

				FLOAT VisibilityFactor;
				if ((bLightingIsBuilt 
					|| DoesLightAffectOwner(CurrentLight, PredictedOwnerPosition) 
					&& IsLightVisible(CurrentLight, PredictedOwnerPosition, FALSE, VisibilityFactor))
					&& (DominantSpotShadowTransitionDistance < NewDominantShadowTransitionDistance
					|| NewDominantShadowTransitionDistance < KINDA_SMALL_NUMBER
					&& Abs(DominantSpotShadowTransitionDistance - NewDominantShadowTransitionDistance) < KINDA_SMALL_NUMBER
					&& CurrentDistanceSquared < ClosestDistanceSquared))
				{
					NewAffectingDominantLight = CurrentLight;
					NewDominantShadowTransitionDistance = DominantSpotShadowTransitionDistance;
					ClosestDistanceSquared = CurrentDistanceSquared;
				}
			}
		}
	}
}

/** Cleans up preview components. */
void FDynamicLightEnvironmentState::ClearPreviewComponents()
{
	if (Component && Component->GetOwner())
	{
		for (INT LightIndex = 0; LightIndex < DebugVolumeSampleLights.Num(); LightIndex++)
		{
			ULightComponent* Light = DebugVolumeSampleLights(LightIndex);
			Component->GetOwner()->DetachComponent(Light);
		}
	}
	
	DebugVolumeSampleLights.Empty();

	if (Component && Component->GetOwner())
	{
		for (INT MeshIndex = 0; MeshIndex < DebugVolumeSampleMeshes.Num(); MeshIndex++)
		{
			UStaticMeshComponent* Mesh = DebugVolumeSampleMeshes(MeshIndex);
			Component->GetOwner()->DetachComponent(Mesh);
		}
	}
	DebugVolumeSampleMeshes.Empty();
}

/** Adds lights that affect this DLE to RelevantLightList. */
void FDynamicLightEnvironmentState::AddRelevantLights(TArray<ALight*>& RelevantLightList, UBOOL bDominantOnly) const
{
	UBOOL bLightingIsBuilt;
	TArray<FDebugShadowRay> DummyClosestDominantLightRays;
	if (GWorld->DominantDirectionalLight)
	{
		const FLOAT DominantDirectionalShadowTransitionDistance = GWorld->DominantDirectionalLight->GetDominantShadowTransitionDistance(
			OwnerBounds, 
			Component->DominantShadowTransitionStartDistance, 
			FALSE, 
			DummyClosestDominantLightRays,
			bLightingIsBuilt);

		ALight* LightOwner = Cast<ALight>(GWorld->DominantDirectionalLight->GetOwner());
		if (LightOwner && DominantDirectionalShadowTransitionDistance < Component->DominantShadowTransitionStartDistance)
		{
			RelevantLightList.AddUniqueItem(LightOwner);
		}
	}

	for (TSparseArray<UDominantPointLightComponent*>::TIterator LightIt(GWorld->DominantPointLights); LightIt; ++LightIt)
	{
		UDominantPointLightComponent* CurrentLight = *LightIt;

		const FLOAT DominantPointShadowTransitionDistance = 
			CurrentLight->GetDominantShadowTransitionDistance(
			OwnerBounds, 
			Component->DominantShadowTransitionStartDistance, 
			FALSE, 
			DummyClosestDominantLightRays,
			bLightingIsBuilt);

		ALight* LightOwner = Cast<ALight>(CurrentLight->GetOwner());
		if (LightOwner && DominantPointShadowTransitionDistance < Component->DominantShadowTransitionStartDistance)
		{
			RelevantLightList.AddUniqueItem(LightOwner);
		}
	}

	for (TSparseArray<UDominantSpotLightComponent*>::TIterator LightIt(GWorld->DominantSpotLights); LightIt; ++LightIt)
	{
		UDominantSpotLightComponent* CurrentLight = *LightIt;

		const FLOAT DominantSpotShadowTransitionDistance = 
			CurrentLight->GetDominantShadowTransitionDistance(
			OwnerBounds, 
			Component->DominantShadowTransitionStartDistance, 
			FALSE, 
			DummyClosestDominantLightRays,
			bLightingIsBuilt);

		ALight* LightOwner = Cast<ALight>(CurrentLight->GetOwner());
		if (LightOwner && DominantSpotShadowTransitionDistance < Component->DominantShadowTransitionStartDistance)
		{
			RelevantLightList.AddUniqueItem(LightOwner);
		}
	}

	if (!bDominantOnly)
	{
		for (TSparseArray<ULightComponent*>::TConstIterator LightIt(GWorld->DynamicLightList); LightIt; ++LightIt)
		{
			ULightComponent* Light = *LightIt;
			ALight* LightOwner = Cast<ALight>(Light->GetOwner());
			FLOAT VisibilityFactor;

			if (LightOwner
				&& DoesLightAffectOwner(Light, OwnerBounds.Origin) 
				&& IsLightVisible(Light, OwnerBounds.Origin, TRUE, VisibilityFactor))
			{
				RelevantLightList.AddUniqueItem(LightOwner);
			}
		}

		for (TSparseArray<ULightComponent*>::TConstIterator LightIt(GWorld->StaticLightList); LightIt; ++LightIt)
		{
			ULightComponent* Light = *LightIt;
			ALight* LightOwner = Cast<ALight>(Light->GetOwner());
			FLOAT VisibilityFactor;

			if (LightOwner
				&& DoesLightAffectOwner(Light, OwnerBounds.Origin) 
				&& IsLightVisible(Light, OwnerBounds.Origin, FALSE, VisibilityFactor))
			{
				RelevantLightList.AddUniqueItem(LightOwner);
			}
		}
	}
}

void UDynamicLightEnvironmentComponent::FinishDestroy()
{
	Super::FinishDestroy();

	if (State)
	{
		State->ClearPreviewComponents();
		// Clean up the light environment's state.
		delete State;
		State = NULL;
	}
}

void UDynamicLightEnvironmentComponent::AddReferencedObjects(TArray<UObject*>& ObjectArray)
{
	Super::AddReferencedObjects(ObjectArray);

	if(State)
	{
		State->AddReferencedObjects(ObjectArray);
	}
}

void UDynamicLightEnvironmentComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(!Ar.IsSaving() && !Ar.IsLoading())
	{
		// If serialization is being used to find references for garbage collection, use AddReferencedObjects to gather a list to serialize.
		TArray<UObject*> ReferencedObjects;
		AddReferencedObjects(ReferencedObjects);
		Ar << ReferencedObjects;
	}
}

void UDynamicLightEnvironmentComponent::PostLoad()
{
	Super::PostLoad();
	// Clamp NumVolumeVisibilitySamples to a sane max for performance
	NumVolumeVisibilitySamples = Min(NumVolumeVisibilitySamples, 4);
}

void UDynamicLightEnvironmentComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Clamp NumVolumeVisibilitySamples to a sane max for performance
	NumVolumeVisibilitySamples = Min(NumVolumeVisibilitySamples, 4);
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#if USE_GAMEPLAY_PROFILER
/** 
 * This function actually does the work for the GetProfilerAssetObject and is virtual.  
 * It should only be called from GetProfilerAssetObject as GetProfilerAssetObject is safe to call on NULL object pointers
 */
UObject* UDynamicLightEnvironmentComponent::GetProfilerAssetObjectInternal() const
{
	if (GetOuter() != NULL)
	{
		return GetOuter()->GetProfilerAssetObject();
	}
	return NULL;
}
#endif

/**
 * This will return detail info about this specific object. (e.g. AudioComponent will return the name of the cue,
 * ParticleSystemComponent will return the name of the ParticleSystem)  The idea here is that in many places
 * you have a component of interest but what you really want is some characteristic that you can use to track
 * down where it came from.  
 *
 */
FString UDynamicLightEnvironmentComponent::GetDetailedInfoInternal() const
{
	FString Result;  

	if( GetOuter() != NULL )
	{
		Result = GetOuter()->GetDetailedInfo();
	}
	else
	{
		Result = TEXT("No_Outer!?");
	}

	return Result;  
}


void UDynamicLightEnvironmentComponent::Tick(FLOAT DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_DynamicLightEnvironmentComponentTickTime);

	Super::Tick(DeltaTime);

#if !DEDICATED_SERVER
	// A DLE can have bEnabled == TRUE and State == NULL if ULightEnvironmentComponent::SetEnabled was recently called,
	// Because SetEnabled uses BeginDeferredReattach, so the DLE may get ticked again before getting reattached.
	if(bEnabled && !NeedsReattach())
	{
#if !FINAL_RELEASE
		extern UBOOL GShouldLogOutAFrameOfLightEnvTick;
		if(GShouldLogOutAFrameOfLightEnvTick)
		{
			debugf(TEXT("LE: %s %s %s %u"), *GetPathName(), *GetDetailedInfo(), bDynamic ? TEXT("DYNAMIC") : TEXT("STATIC"), TickGroup );
		}
#endif

		// If the light environment requires non-latent updates, ensure that the light environment is
		// ticked after everything is in its final location for the frame.
		if(bRequiresNonLatentUpdates && TickGroup != TG_PostUpdateWork)
		{
			SetTickGroup(TG_PostUpdateWork);
		}

		// Update the light environment's state.
		check(State);
		State->Tick(DeltaTime);
	}
#endif // !DEDICATED_SERVER
}

void UDynamicLightEnvironmentComponent::Attach()
{
	Super::Attach();

#if !DEDICATED_SERVER
	if(bEnabled)
	{
		// Initialize the light environment's state the first time it's attached.
		if(!State)
		{
			State = new FDynamicLightEnvironmentState(this);
		}

		// if we know we're not going to be ticked, update the light environment now
		if (!GIsGame || (Scene->GetWorld() != NULL && Scene->GetWorld()->IsPaused()))
		{
			State->Update();
		}

		// Add the light environment to the world's list, so it can be updated when static lights change.
		if (Scene->GetWorld())
		{
			Scene->GetWorld()->LightEnvironmentList.AddItem(this);
		}

		// Recreate the lights.
		State->CreateEnvironmentLightList(AffectingDominantLight, State->GetDominantShadowTransitionDistance(), TRUE);
	}
#endif // !DEDICATED_SERVER
}

void UDynamicLightEnvironmentComponent::UpdateTransform()
{
	Super::UpdateTransform();

	if(bEnabled && State)
	{
		// if we know we're not going to be ticked, update the light environment now
		if (!GIsGame || (Scene->GetWorld() != NULL && Scene->GetWorld()->IsPaused()))
		{
			State->Update();
		}
	}
}

void UDynamicLightEnvironmentComponent::Detach( UBOOL bWillReattach )
{
	Super::Detach( bWillReattach );

	// Remove the light environment from the world's list.
	if (Scene->GetWorld())
	{
		for(TSparseArray<ULightEnvironmentComponent*>::TIterator It(Scene->GetWorld()->LightEnvironmentList);It;++It)
		{
			if(*It == this)
			{
				Scene->GetWorld()->LightEnvironmentList.Remove(It.GetIndex());
				break;
			}
		}
	}

	if(State)
	{
		// Detach the light environment's representative lights.
		State->DetachRepresentativeLights(TRUE);
	}
}

void UDynamicLightEnvironmentComponent::BeginPlay()
{
	Super::BeginPlay();
	// By default DLE updates are overlayed with async work which can cause latency.
	if( bRequiresNonLatentUpdates )
	{
		SetTickGroup( TG_PostUpdateWork );
	}
}

void UDynamicLightEnvironmentComponent::SetNeedsStaticUpdate()
{
	if (State)
	{
		State->SetNeedsStaticUpdate();
	}
}

void UDynamicLightEnvironmentComponent::UpdateLight(const ULightComponent* Light)
{
	Super::UpdateLight(Light);
	if(bEnabled && IsAttached())
	{
		if(!GIsGame)
		{
			// Outside the game, update the environment if any light changes.
			BeginDeferredUpdateTransform();
		}
	}
}

/** Forces a full update on the next Tick. */
void UDynamicLightEnvironmentComponent::ResetEnvironment()
{
	if (State)
	{
		State->ResetEnvironment();
	}
}

#if WITH_EDITOR
void UDynamicLightEnvironmentComponent::CheckForErrors()
{
	Super::CheckForErrors();

	// Set a warning if we detect any DLE outside of the Lightmass Importance Volume
	if (Owner && IsValidComponent() && IsEnabled() && State)
	{
		// Make sure the world is set up as we need it to be
		if (GWorld && GWorld->PersistentLevel)
		{
			//@todo - warn when no affected components?
			if (AffectedComponents.Num() > 0)
			{
				UBOOL bAnyComponentAcceptsLights = FALSE;
				for (INT ComponentIndex = 0; ComponentIndex < AffectedComponents.Num(); ComponentIndex++)
				{
					UPrimitiveComponent* Primitive = AffectedComponents(ComponentIndex);
					if (Primitive)
					{
						bAnyComponentAcceptsLights = bAnyComponentAcceptsLights || Primitive->bAcceptsLights;
					}
				}
				if (!bAnyComponentAcceptsLights)
				{
					GWarn->MapCheck_Add( MCTYPE_PERFORMANCEWARNING, Owner, *FString( LocalizeUnrealEd( "MapCheck_Message_DynamicLightEnvironmentEnabled_ComponentsNotAcceptingLights" ) ), TEXT( "DynamicLightEnvironmentEnabled_ComponentsNotAcceptingLights" ) );
				}
			}
		}
	}
}
#endif

/** Adds lights that affect this DLE to RelevantLightList. */
void UDynamicLightEnvironmentComponent::AddRelevantLights(TArray<ALight*>& RelevantLightList, UBOOL bDominantOnly) const
{
	if (State)
	{
		State->AddRelevantLights(RelevantLightList, bDominantOnly);
	}
}

UBOOL UDynamicLightEnvironmentComponent::NeedsUpdateBasedOnComponent(UPrimitiveComponent* Component) const
{
	return Component->ShouldComponentAddToScene();
}

void UParticleLightEnvironmentComponent::Tick(FLOAT DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleDLETickTime);
	Super::Tick(DeltaTime);
	if (GetRefCount() == 0 && Owner)
	{
		DEC_DWORD_STAT(STAT_NumParticleDLEs);
		// Detach if no longer in use, to remove the Owner's reference so this DLE can be GC'ed
		Owner->DetachComponent(this);
	}
}

void UParticleLightEnvironmentComponent::UpdateLight(const ULightComponent* Light)
{
	Super::UpdateLight(Light);
	if (!GIsGame && GetRefCount() == 0 && Owner)
	{
		DEC_DWORD_STAT(STAT_NumParticleDLEs);
		// Detach if no longer in use, to remove the Owner's reference so this DLE can be GC'ed
		Owner->DetachComponent(this);
	}
}

void UParticleLightEnvironmentComponent::BeginDestroy()
{
	Super::BeginDestroy();
	if (GetRefCount() > 0)
	{
		DEC_DWORD_STAT(STAT_NumParticleDLEs);
	}
}

UBOOL UParticleLightEnvironmentComponent::NeedsUpdateBasedOnComponent(UPrimitiveComponent* Component) const
{
	UParticleSystemComponent* ParticleComponent = Cast<UParticleSystemComponent>(Component);
	if (ParticleComponent && !ParticleComponent->bIsActive)
	{
		// The DLE doesn't need to be updated for this particle component, since the component has not been activated yet
		return FALSE;
	}
	return Super::NeedsUpdateBasedOnComponent(Component);
}

#if USE_GAMEPLAY_PROFILER
/** 
 * This function actually does the work for the GetProfilerAssetObject and is virtual.  
 * It should only be called from GetProfilerAssetObject as GetProfilerAssetObject is safe to call on NULL object pointers
 */
UObject* UParticleLightEnvironmentComponent::GetProfilerAssetObjectInternal() const
{
	if (SharedParticleSystem != NULL)
	{
		return SharedParticleSystem->GetProfilerAssetObject();
	}

	if( SharedInstigator != NULL )
	{
		return SharedInstigator->GetProfilerAssetObject();
	}
	return NULL;
}
#endif

/**
 * This will return detail info about this specific object. (e.g. AudioComponent will return the name of the cue,
 * ParticleSystemComponent will return the name of the ParticleSystem)  The idea here is that in many places
 * you have a component of interest but what you really want is some characteristic that you can use to track
 * down where it came from.  
 *
 */
FString UParticleLightEnvironmentComponent::GetDetailedInfoInternal() const
{
	FString Result;  

	if( SharedParticleSystem != NULL )
	{
		Result = SharedParticleSystem->GetPathName( NULL );	
	}
	else
	{
		Result = TEXT("No_SharedParticleSystem_set!");
	}

	if( SharedInstigator != NULL )
	{
		Result += FString( TEXT("__") );
		Result += SharedInstigator->GetDetailedInfo();
	}
	else
	{
		Result += FString( TEXT("__") );
		Result += TEXT("No_SharedInstigator_set!");
	}

	//now look at at all of the affected comps that are PSCs
	for( INT i = 0; i < AffectedComponents.Num(); ++i )
	{
		UParticleSystemComponent* PSC = Cast<UParticleSystemComponent>(AffectedComponents(i));
		if( PSC != NULL )
		{
			Result += FString( TEXT("_") );
			Result += PSC->GetDetailedInfo();
		}
	}


	return Result;  
}


