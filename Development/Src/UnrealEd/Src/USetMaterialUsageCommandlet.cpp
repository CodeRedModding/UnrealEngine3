/*=============================================================================
	USetMaterialUsageCommandlet.cpp - Commandlet which finds what types of geometry a material is used on.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EngineMaterialClasses.h"
#include "EngineParticleClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineAnimClasses.h"
#include "UnTerrain.h"
#include "EngineFoliageClasses.h"
#include "SpeedTree.h"
#include "EngineDecalClasses.h"
#include "UnFracturedStaticMesh.h"
#include "EngineMeshClasses.h"

#include "SourceControl.h"

struct FPackageMaterialInfo
{
	TArray<FString> SkeletalMeshMaterials;
	TArray<FString> FracturedMeshMaterials;
	TArray<FString> ParticleSpriteMaterials;
	TArray<FString> BeamTrailMaterials;
	TArray<FString> ParticleSubUVMaterials;
	TArray<FString> DecalMaterials;
	TArray<FString> StaticLightingMaterials;
	TArray<FString> FluidSurfacesMaterials;
	TArray<FString> MaterialEffectMaterials;
	TArray<FString> InstancedMeshMaterials;
	TArray<FString> SplineMeshMaterials;
	TArray<FString> ScreenDoorFadeMaterials;
};

static void SetMaterialUsage(
	TMap<FName,FPackageMaterialInfo>& PackageInfoMap,
	UMaterialInterface* MaterialInterface,
	UBOOL bUsedWithSkeletalMesh,
	UBOOL bUsedWithFracturedStaticMeshes,
	UBOOL bUsedWithParticleSprites,
	UBOOL bUsedWithBeamTrails,
	UBOOL bUsedWithParticleSubUV,
	UBOOL bUsedWithDecals,
	UBOOL bUsedWithStaticLighting,
	UBOOL bUsedWithFluidSurfaces,
	UBOOL bUsedWithMaterialEffect,
	UBOOL bUsedWithInstancedMeshes,
	UBOOL bUsedWithSplineMeshes,
	UBOOL bUsedWithScreenDoorFade
	)
{
	UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
	if(Material)
	{
		FPackageMaterialInfo* PackageInfo = PackageInfoMap.Find(Material->GetOutermost()->GetFName());
		if(!PackageInfo)
		{
			PackageInfo = &PackageInfoMap.Set(Material->GetOutermost()->GetFName(),FPackageMaterialInfo());
		}
		if(bUsedWithSkeletalMesh && !Material->bUsedWithSkeletalMesh)
		{
			PackageInfo->SkeletalMeshMaterials.AddUniqueItem(Material->GetPathName());
		}
		if(bUsedWithFracturedStaticMeshes && !Material->bUsedWithFracturedMeshes)
		{
			PackageInfo->FracturedMeshMaterials.AddUniqueItem(Material->GetPathName());
		}
		if(bUsedWithParticleSprites)
		{
			PackageInfo->ParticleSpriteMaterials.AddUniqueItem(Material->GetPathName());
		}
		if(bUsedWithBeamTrails)
		{
			PackageInfo->BeamTrailMaterials.AddUniqueItem(Material->GetPathName());
		}
		if(bUsedWithParticleSubUV)
		{
			PackageInfo->ParticleSubUVMaterials.AddUniqueItem(Material->GetPathName());
		}
		if(bUsedWithDecals)
		{
			PackageInfo->DecalMaterials.AddUniqueItem(Material->GetPathName());
		}
		if(bUsedWithStaticLighting && !Material->bUsedWithStaticLighting)
		{
			PackageInfo->StaticLightingMaterials.AddUniqueItem(Material->GetPathName());
		}
		if(bUsedWithFluidSurfaces && !Material->bUsedWithFluidSurfaces)
		{
			PackageInfo->FluidSurfacesMaterials.AddUniqueItem(Material->GetPathName());
		}
		if(bUsedWithMaterialEffect && !Material->bUsedWithMaterialEffect)
		{
			PackageInfo->MaterialEffectMaterials.AddUniqueItem(Material->GetPathName());
		}
		if(bUsedWithInstancedMeshes && !Material->bUsedWithInstancedMeshes)
		{
			PackageInfo->InstancedMeshMaterials.AddUniqueItem(Material->GetPathName());
		}
		if(bUsedWithSplineMeshes && !Material->bUsedWithSplineMeshes)
		{
			PackageInfo->SplineMeshMaterials.AddUniqueItem(Material->GetPathName());
		}		
		if(bUsedWithScreenDoorFade && !Material->bUsedWithScreenDoorFade)
		{
			PackageInfo->ScreenDoorFadeMaterials.AddUniqueItem(Material->GetPathName());
		}		
	}
}

INT USetMaterialUsageCommandlet::Main( const FString& Params )
{
	const TCHAR* Parms = *Params;

	// Retrieve list of all packages in .ini paths.
	TArray<FString> PackageList;

	PackageList = GPackageFileCache->GetPackageFileList();
	
	if( !PackageList.Num() )
	{
		warnf( TEXT( "Found no packages to fun SetMaterialUsageCommandlet on!" ) );
		return 0;
	}

	const UBOOL bAutoCheckOut = ParseParam(appCmdLine(),TEXT("AutoCheckOutPackages"));
#if HAVE_SCC
	// Ensure source control is initialized and shut down properly
	FScopedSourceControl SourceControl;
#endif
	
	GEngine->Exec( TEXT("DumpShaderStats") );

	// Iterate over all packages.
	TMap<FName,FPackageMaterialInfo> PackageInfoMap;
	for( INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++ )
	{
		FFilename Filename = PackageList(PackageIndex);
#if HAVE_SCC
		if ( bAutoCheckOut && FSourceControl::ForceGetStatus( Filename ) == SCC_NotCurrent )
		{
			warnf(NAME_Log, TEXT("Skipping %s (Not at head source control revision)"), *Filename);
			continue;
		}
#endif

		warnf(NAME_Log, TEXT("Loading %s"), *Filename);

		UPackage* Package = UObject::LoadPackage( NULL, *Filename, 0 );
		if (Package != NULL)
		{
			// Iterate over all objects in the package.
			for(FObjectIterator ObjectIt;ObjectIt;++ObjectIt)
			{
				if(ObjectIt->IsIn(Package))
				{
					USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(*ObjectIt);
					USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(*ObjectIt);
					UFracturedStaticMeshComponent* FracturedComponent = Cast<UFracturedStaticMeshComponent>(*ObjectIt);
					UFracturedStaticMesh* FracturedMesh = Cast<UFracturedStaticMesh>(*ObjectIt);
					UParticleSpriteEmitter* SpriteEmitter = Cast<UParticleSpriteEmitter>(*ObjectIt);
					UModelComponent* ModelComponent = Cast<UModelComponent>(*ObjectIt);
					UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(*ObjectIt);
					UTerrainMaterial* TerrainMaterial = Cast<UTerrainMaterial>(*ObjectIt);
					UDecalMaterial* DecalMaterial = Cast<UDecalMaterial>(*ObjectIt);
					USpeedTreeComponent* SpeedTreeComponent = Cast<USpeedTreeComponent>(*ObjectIt);
					UDecalComponent* DecalComponent = Cast<UDecalComponent>(*ObjectIt);
					UMaterialEffect* MaterialEffect = Cast<UMaterialEffect>(*ObjectIt);
					UInstancedStaticMeshComponent* InstancedStaticMeshComponent = Cast<UInstancedStaticMeshComponent>(*ObjectIt);
					USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>(*ObjectIt);

					if(SkeletalMeshComponent)
					{
						// Mark all the materials referenced by the skeletal mesh component as being used with a skeletal mesh.
						for(INT MaterialIndex = 0;MaterialIndex < SkeletalMeshComponent->Materials.Num();MaterialIndex++)
						{
							UMaterialInterface* Material = SkeletalMeshComponent->Materials(MaterialIndex);
							SetMaterialUsage(PackageInfoMap,Material,TRUE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE);
						}
					}
					else if(SkeletalMesh)
					{
						// Mark all the materials referenced by the skeletal mesh as being used with a skeletal mesh.
						for(INT MaterialIndex = 0;MaterialIndex < SkeletalMesh->Materials.Num();MaterialIndex++)
						{
							UMaterialInterface* Material = SkeletalMesh->Materials(MaterialIndex);
							SetMaterialUsage(PackageInfoMap,Material,TRUE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE);
						}
					}
					else if (FracturedComponent)
					{
						// Mark all the materials referenced by the fractured mesh component as being used with a fractured mesh.
						for(INT MaterialIndex = 0;MaterialIndex < FracturedComponent->Materials.Num();MaterialIndex++)
						{
							UMaterialInterface* Material = FracturedComponent->Materials(MaterialIndex);
							SetMaterialUsage(PackageInfoMap,Material,FALSE,TRUE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE);
						}
					}
					else if(FracturedMesh)
					{
						for(INT LODIndex = 0; LODIndex < FracturedMesh->LODInfo.Num(); LODIndex++)
						{
							for(INT ElementIndex = 0; ElementIndex < FracturedMesh->LODInfo(LODIndex).Elements.Num(); ElementIndex++)
							{
								UMaterialInterface* Material = FracturedMesh->LODInfo(LODIndex).Elements(ElementIndex).Material;
								SetMaterialUsage(PackageInfoMap,Material,FALSE,TRUE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE);
							}
						}
					}
					else if(SpriteEmitter)
					{
						UBOOL bSpriteEmitter = FALSE;
						UBOOL bBeamTrailEmitter = FALSE;
						UBOOL bSubUVEmitter = FALSE;
						
						//check each LOD
						for (INT LODIndex = 0; LODIndex < SpriteEmitter->LODLevels.Num(); LODIndex++)
						{
							UParticleLODLevel* LODLevel = SpriteEmitter->LODLevels(LODIndex);
							if (LODLevel && LODLevel->RequiredModule && LODLevel->RequiredModule->Material)
							{
								if (LODLevel->TypeDataModule
									&& (LODLevel->TypeDataModule->IsA(UParticleModuleTypeDataBeam2::StaticClass())
									|| LODLevel->TypeDataModule->IsA(UParticleModuleTypeDataTrail2::StaticClass())))
								{
									//if the LOD has a data type module and it is a beam or trail module, then material is used with beams or trails
									//don't set anything for mesh emitters, the material is used by the mesh and not the emitter
									bBeamTrailEmitter = TRUE;
								}
								else if (LODLevel->RequiredModule->InterpolationMethod != PSUVIM_None)
								{
									//LODLevel->RequiredModule->InterpolationMethod being anything other than PSUVIM_None indicates that the material is used with the Sub UV vertex factory
									bSubUVEmitter = TRUE;
								}
								else 
								{
									//otherwise it must be a sprite emitter
									bSpriteEmitter = TRUE;
								}
								
								SetMaterialUsage(PackageInfoMap,LODLevel->RequiredModule->Material,FALSE,FALSE,bSpriteEmitter,bBeamTrailEmitter,bSubUVEmitter,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE);
							}
						}
						//don't handle SpriteEmitter->Material, since it is deprecated
					}
					else if(ModelComponent)
					{
						// Check each of the model's elements for static lighting, and if present mark the element's material as being used with static lighting.
						for(INT ElementIndex = 0;ElementIndex < ModelComponent->GetElements().Num();ElementIndex++)
						{
							const FModelElement& Element = ModelComponent->GetElements()(ElementIndex);
							const UBOOL bHasStaticLighting = Element.LightMap != NULL || Element.ShadowMaps.Num();
							if(bHasStaticLighting)
							{
								SetMaterialUsage(PackageInfoMap,Element.Material,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,TRUE,FALSE,FALSE,FALSE,FALSE,FALSE);
							}
						}
					}
					else if(StaticMeshComponent && StaticMeshComponent->IsValidComponent())
					{
						if(StaticMeshComponent->HasStaticShadowing())
						{
							// Mark the element's material as being used with static lighting.
							for(INT ElementIndex = 0;ElementIndex < StaticMeshComponent->GetNumElements();ElementIndex++)
							{
								UMaterialInterface* Material = StaticMeshComponent->GetMaterial(ElementIndex);
								SetMaterialUsage(PackageInfoMap,Material,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,TRUE,FALSE,FALSE,FALSE,FALSE,FALSE);
							}
						}
					}
					else if(TerrainMaterial)
					{
						// handle decal material usage with terrain materials
						DecalMaterial = Cast<UDecalMaterial>(TerrainMaterial->Material);
					}
					else if(SpeedTreeComponent && SpeedTreeComponent->IsValidComponent())
					{
						if(SpeedTreeComponent->HasStaticShadowing())
						{
							// Mark the SpeedTreeComponent's materials as being used with static lighting.
							for (BYTE MeshTypeIdx = STMT_MinMinusOne + 1; MeshTypeIdx < STMT_Max; MeshTypeIdx++)
							{
								SetMaterialUsage(PackageInfoMap,SpeedTreeComponent->GetMaterial(MeshTypeIdx),FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,TRUE,FALSE,FALSE,FALSE,FALSE,FALSE);
							}
						}
					}
					else if( DecalComponent )
					{
						// Make sure that all materials used with decals have decal usage 
						SetMaterialUsage(PackageInfoMap,DecalComponent->GetDecalMaterial(),FALSE,FALSE,FALSE,FALSE,FALSE,TRUE,TRUE,FALSE,FALSE,FALSE,FALSE,FALSE);
					}
					else if(MaterialEffect)
					{
						SetMaterialUsage(PackageInfoMap,MaterialEffect->Material,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,TRUE,FALSE,FALSE,FALSE);
					}
					else if(InstancedStaticMeshComponent)
					{
						// Mark the element's material as being used with static lighting.
						for(INT ElementIndex = 0;ElementIndex < InstancedStaticMeshComponent->GetNumElements();ElementIndex++)
						{
							UMaterialInterface* Material = InstancedStaticMeshComponent->GetMaterial(ElementIndex);
							SetMaterialUsage(PackageInfoMap,Material,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,TRUE,FALSE,FALSE,TRUE,FALSE,FALSE);
						}
					}
					else if(SplineMeshComponent)
					{
						// Mark the element's material as being used with spline mesh.
						for(INT ElementIndex = 0;ElementIndex < SplineMeshComponent->GetNumElements(); ElementIndex++)
						{
							UMaterialInterface* Material = SplineMeshComponent->GetMaterial(ElementIndex);
							SetMaterialUsage(PackageInfoMap,Material,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,TRUE,FALSE,FALSE,FALSE,TRUE,FALSE);
						}
					}
					else if(!appStricmp(*ObjectIt->GetClass()->GetName(),TEXT("SeqAct_SetMaterial")))
					{
						// Extract the value of the script NewMaterial property.
						UProperty* MaterialProperty = CastChecked<UProperty>(ObjectIt->FindObjectField(FName(TEXT("NewMaterial"))));
						UMaterialInterface* Material = *(UMaterialInterface**)((BYTE*)*ObjectIt + MaterialProperty->Offset);
						USequenceAction* SequenceAction = CastChecked<USequenceAction>(*ObjectIt);
						// If the SetMaterial targets include a skeletal mesh, mark the material as being used with a skeletal mesh.
						for(INT TargetIndex = 0;TargetIndex < SequenceAction->Targets.Num();TargetIndex++)
						{
							ASkeletalMeshActor* SkeletalMeshActor = Cast<ASkeletalMeshActor>(SequenceAction->Targets(TargetIndex));
							if(SkeletalMeshActor)
							{
								SetMaterialUsage(PackageInfoMap,Material,TRUE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE);
								break;
							}
						}
					}

					if( DecalMaterial )
					{
						// Make sure that all materials used with decals have decal usage 
						SetMaterialUsage(PackageInfoMap,DecalMaterial,TRUE,TRUE,FALSE,FALSE,FALSE,TRUE,TRUE,TRUE,FALSE,FALSE,FALSE,FALSE);
					}

	
					// here we give the specific game a chance to set flags based on info it maybe has (e.g. gears has physical materials that have semi complicated organization OF data and we want to find materials in there that are decals)
					{
						TArray<UMaterialInterface*> DecalMaterials = GEditor->GetDecalMaterialsFromGame( *ObjectIt );

						// now set the DecalMaterial settings
						for( INT Idx = 0; Idx < DecalMaterials.Num(); ++Idx )
						{
							UMaterialInterface* DecalMaterial = DecalMaterials(Idx);
							SetMaterialUsage(PackageInfoMap,DecalMaterial,TRUE,TRUE,FALSE,FALSE,FALSE,TRUE,TRUE,TRUE,FALSE,FALSE,FALSE,FALSE);
						}
					}
				}
			}
		}

		UObject::CollectGarbage(RF_Native);
		SaveLocalShaderCaches();
	}

	UINT NumStaticLightingMaterials = 0;
	UINT NumDecalMaterials = 0;
	UINT NumTotalMaterials = 0;

	for(TMap<FName,FPackageMaterialInfo>::TConstIterator PackageIt(PackageInfoMap);PackageIt;++PackageIt)
	{
		const FPackageMaterialInfo& PackageInfo = PackageIt.Value();
		// Only save dirty packages.
		if(PackageInfo.SkeletalMeshMaterials.Num() 
			|| PackageInfo.FracturedMeshMaterials.Num() 
			|| PackageInfo.ParticleSpriteMaterials.Num() 
			|| PackageInfo.BeamTrailMaterials.Num() 
			|| PackageInfo.ParticleSubUVMaterials.Num() 
			|| PackageInfo.StaticLightingMaterials.Num()
			|| PackageInfo.DecalMaterials.Num()
			|| PackageInfo.FluidSurfacesMaterials.Num()
			|| PackageInfo.MaterialEffectMaterials.Num()
			|| PackageInfo.InstancedMeshMaterials.Num()
			|| PackageInfo.SplineMeshMaterials.Num()
			|| PackageInfo.ScreenDoorFadeMaterials.Num()
			)
		{
			warnf(
				TEXT("Package %s is dirty(%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u)"),
				*PackageIt.Key().ToString(),
				PackageInfo.SkeletalMeshMaterials.Num(),
				PackageInfo.FracturedMeshMaterials.Num(),
				PackageInfo.ParticleSpriteMaterials.Num(),
				PackageInfo.BeamTrailMaterials.Num(),
				PackageInfo.ParticleSubUVMaterials.Num(),
				PackageInfo.StaticLightingMaterials.Num(),
				PackageInfo.DecalMaterials.Num(),
				PackageInfo.FluidSurfacesMaterials.Num(),
				PackageInfo.MaterialEffectMaterials.Num(),
				PackageInfo.InstancedMeshMaterials.Num(),
				PackageInfo.SplineMeshMaterials.Num(),
				PackageInfo.ScreenDoorFadeMaterials.Num()
				);

			FString Filename;
			if(GPackageFileCache->FindPackageFile(*PackageIt.Key().ToString(),NULL,Filename))
			{
				UPackage* Package = UObject::LoadPackage( NULL, *Filename, 0 );
				if(Package)
				{
					UObject::ResetLoaders(Package);

					// Count the materials in the package.
					for(TObjectIterator<UMaterial> MaterialIt;MaterialIt;++MaterialIt)
					{
						if (MaterialIt->IsIn(Package))
						{
							NumTotalMaterials++;

							UMaterial* CurrentMaterial = *MaterialIt;
							FString CurrentMaterialName = CurrentMaterial->GetPathName();
							if (CurrentMaterialName != TEXT("EngineMaterials.DefaultParticle"))
							{
								UBOOL bResetUsageFlags = FALSE;

								if (CurrentMaterial->bUsedWithParticleSprites
									&& !PackageInfo.ParticleSpriteMaterials.ContainsItem(CurrentMaterialName))
								{
									bResetUsageFlags = TRUE;
								}
								else if (CurrentMaterial->bUsedWithBeamTrails
									&& !PackageInfo.BeamTrailMaterials.ContainsItem(CurrentMaterialName))
								{
									bResetUsageFlags = TRUE;
								}
								else if (CurrentMaterial->bUsedWithParticleSubUV
									&& !PackageInfo.ParticleSubUVMaterials.ContainsItem(CurrentMaterialName))
								{
									bResetUsageFlags = TRUE;
								}

								if (bResetUsageFlags)
								{
									//set particle usage flags on existing materials to FALSE
									CurrentMaterial->bUsedWithParticleSprites = FALSE;
									CurrentMaterial->bUsedWithBeamTrails = FALSE;
									CurrentMaterial->bUsedWithParticleSubUV = FALSE;
									CurrentMaterial->MarkPackageDirty();
								}

								// Reset incorrectly set bUsedWithMaterialEffect
								if (CurrentMaterial->bUsedWithMaterialEffect
									&& !PackageInfo.MaterialEffectMaterials.ContainsItem(CurrentMaterialName))
								{
									CurrentMaterial->bUsedWithMaterialEffect = FALSE;
									CurrentMaterial->MarkPackageDirty();
								}
							}
							else
							{
								warnf(TEXT("Skipped clearing flags on EngineMaterials.DefaultParticle"));
							}
						}
					}

					for(INT MaterialIndex = 0;MaterialIndex < PackageInfo.SkeletalMeshMaterials.Num();MaterialIndex++)
					{
						UMaterial* Material = FindObjectChecked<UMaterial>(NULL,*PackageInfo.SkeletalMeshMaterials(MaterialIndex));
						Material->CheckMaterialUsage(MATUSAGE_SkeletalMesh);
					}

					for(INT MaterialIndex = 0;MaterialIndex < PackageInfo.FracturedMeshMaterials.Num();MaterialIndex++)
					{
						UMaterial* Material = FindObjectChecked<UMaterial>(NULL,*PackageInfo.FracturedMeshMaterials(MaterialIndex));
						Material->CheckMaterialUsage(MATUSAGE_FracturedMeshes);
					}

					for(INT MaterialIndex = 0;MaterialIndex < PackageInfo.ParticleSpriteMaterials.Num();MaterialIndex++)
					{
						UMaterial* Material = FindObjectChecked<UMaterial>(NULL,*PackageInfo.ParticleSpriteMaterials(MaterialIndex));
						Material->CheckMaterialUsage(MATUSAGE_ParticleSprites);
					}

					for(INT MaterialIndex = 0;MaterialIndex < PackageInfo.BeamTrailMaterials.Num();MaterialIndex++)
					{
						UMaterial* Material = FindObjectChecked<UMaterial>(NULL,*PackageInfo.BeamTrailMaterials(MaterialIndex));
						Material->CheckMaterialUsage(MATUSAGE_BeamTrails);
					}

					for(INT MaterialIndex = 0;MaterialIndex < PackageInfo.ParticleSubUVMaterials.Num();MaterialIndex++)
					{
						UMaterial* Material = FindObjectChecked<UMaterial>(NULL,*PackageInfo.ParticleSubUVMaterials(MaterialIndex));
						Material->CheckMaterialUsage(MATUSAGE_ParticleSubUV);
					}

					for(INT MaterialIndex = 0;MaterialIndex < PackageInfo.StaticLightingMaterials.Num();MaterialIndex++)
					{
						UMaterial* Material = FindObjectChecked<UMaterial>(NULL,*PackageInfo.StaticLightingMaterials(MaterialIndex));
						Material->CheckMaterialUsage(MATUSAGE_StaticLighting);
						NumStaticLightingMaterials++;
					}
					for(INT MaterialIndex = 0;MaterialIndex < PackageInfo.DecalMaterials.Num();MaterialIndex++)
					{
						UMaterial* Material = FindObjectChecked<UMaterial>(NULL,*PackageInfo.DecalMaterials(MaterialIndex));
						Material->CheckMaterialUsage(MATUSAGE_Decals);
						NumDecalMaterials++;
					}
					for(INT MaterialIndex = 0;MaterialIndex < PackageInfo.FluidSurfacesMaterials.Num();MaterialIndex++)
					{
						UMaterial* Material = FindObjectChecked<UMaterial>(NULL,*PackageInfo.FluidSurfacesMaterials(MaterialIndex));
						Material->CheckMaterialUsage(MATUSAGE_FluidSurface);
					}
					for(INT MaterialIndex = 0;MaterialIndex < PackageInfo.MaterialEffectMaterials.Num();MaterialIndex++)
					{
						UMaterial* Material = FindObjectChecked<UMaterial>(NULL,*PackageInfo.MaterialEffectMaterials(MaterialIndex));
						Material->CheckMaterialUsage(MATUSAGE_MaterialEffect);
					}
					for(INT MaterialIndex = 0;MaterialIndex < PackageInfo.InstancedMeshMaterials.Num();MaterialIndex++)
					{
						UMaterial* Material = FindObjectChecked<UMaterial>(NULL,*PackageInfo.InstancedMeshMaterials(MaterialIndex));
						Material->CheckMaterialUsage(MATUSAGE_InstancedMeshes);
					}
					for(INT MaterialIndex = 0;MaterialIndex < PackageInfo.SplineMeshMaterials.Num();MaterialIndex++)
					{
						UMaterial* Material = FindObjectChecked<UMaterial>(NULL,*PackageInfo.SplineMeshMaterials(MaterialIndex));
						Material->CheckMaterialUsage(MATUSAGE_SplineMesh);
					}
					for(INT MaterialIndex = 0;MaterialIndex < PackageInfo.ScreenDoorFadeMaterials.Num();MaterialIndex++)
					{
						UMaterial* Material = FindObjectChecked<UMaterial>(NULL,*PackageInfo.ScreenDoorFadeMaterials(MaterialIndex));
						Material->CheckMaterialUsage(MATUSAGE_ScreenDoorFade);
					}
					
					if(Package->IsDirty())
					{
#if HAVE_SCC
						if( (GFileManager->IsReadOnly(*Filename)) && ( bAutoCheckOut == TRUE ) )
						{
							FSourceControl::CheckOut(Package);
						}
#endif
						if (!GFileManager->IsReadOnly(*Filename))
						{
							// resave the package
							UWorld* World = FindObject<UWorld>( Package, TEXT("TheWorld") );
							if( World )
							{	
								UObject::SavePackage( Package, World, 0, *Filename, GWarn );
							}
							else
							{
								UObject::SavePackage( Package, NULL, RF_Standalone, *Filename, GWarn );
							}
						}
					}
				}
			}

			UObject::CollectGarbage(RF_Native);
			SaveLocalShaderCaches();
		}
	}

	warnf(TEXT("%u decal materials out of %u total materials."),NumDecalMaterials,NumTotalMaterials); 
	warnf(TEXT("%u static lighting materials out of %u total materials."),NumStaticLightingMaterials,NumTotalMaterials); 

	GEngine->Exec( TEXT("DumpShaderStats") );

	return 0;
}

IMPLEMENT_CLASS(USetMaterialUsageCommandlet);
