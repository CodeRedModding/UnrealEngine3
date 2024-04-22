/*=============================================================================
	UnPrecompileShaderCommandlet.cpp: Shader precompiler (both local/reference cache).
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EngineMaterialClasses.h"
#include "UnTerrain.h"
#include "GPUSkinVertexFactory.h"
#include "UnConsoleSupportContainer.h"

/*-----------------------------------------------------------------------------
	UPrecompileShadersCommandlet implementation.
-----------------------------------------------------------------------------*/

static FShaderType* FindShaderTypeByKeyword(const TCHAR* Keyword, UBOOL bVertexShader)
{
	for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		if (appStristr(ShaderTypeIt->GetName(), Keyword) && appStristr(ShaderTypeIt->GetName(), bVertexShader ? TEXT("Vertex") : TEXT("Pixel")))
		{
			return *ShaderTypeIt;
		}
	}
	return NULL;
}

/** 
 * Sanitise and display shader compiler errors
 */
static void DumpShaderCompileErrors( FMaterial* MaterialResource )
{
	const TArray<FString>& CompileErrors = MaterialResource->GetCompileErrors();
	for( INT ErrorIndex = 0; ErrorIndex < CompileErrors.Num(); ErrorIndex++ )
	{
		FString CompileError = CompileErrors( ErrorIndex );
		CompileError = CompileError.Replace( TEXT( ": error " ), TEXT( ": shader compiler error " ) );
		warnf( NAME_Warning, TEXT( "    %s" ), *CompileError );
	}
}

FLOAT MaterialProcessingTime = 0;

void UPrecompileShadersCommandlet::ProcessMaterial(UMaterial* Material, UBOOL bIsEditorOnlyPackage)
{
	DOUBLE MaterialProcessingStart = appSeconds();
	if(Material->IsTemplate())
	{
		// Material templates don't have shaders.
		return;
	}

	SET_WARN_COLOR(COLOR_WHITE);
	warnf(TEXT("Processing %s..."), *Material->GetPathName());
	CLEAR_WARN_COLOR();

	UBOOL bForceCompile = FALSE;
	// if the name contains our force string, then flush the shader from the cache
	if (ForceName.Len() && Material->GetPathName().ToUpper().InStr(ForceName.ToUpper()) != -1)
	{
		SET_WARN_COLOR(COLOR_DARK_YELLOW);
		warnf(TEXT("Flushing shaders for %s"), *Material->GetPathName());
		CLEAR_WARN_COLOR();

		bForceCompile = TRUE;
	}

	// compile for the platform!
	for(INT PlatformIndex = 0;PlatformIndex < ShaderPlatforms.Num();PlatformIndex++)
	{
		const EShaderPlatform ShaderPlatform = ShaderPlatforms(PlatformIndex);

		if (bIsEditorOnlyPackage && (ShaderPlatform==SP_XBOXD3D || ShaderPlatform==SP_PS3 || ShaderPlatform==SP_NGP))
		{
			// Skip this editor-only packages for this platform
			continue;
		}

	    TRefCountPtr<FMaterialShaderMap> MapRef;
		for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
		{
			FMaterialResource* MaterialResource = Material->GetMaterialResource((EMaterialShaderQuality)QualityIndex);
			if (MaterialResource)
			{
				FStaticParameterSet EmptySet(MaterialResource->GetId());
				if (!MaterialResource->Compile(&EmptySet, ShaderPlatform, (EMaterialShaderQuality)QualityIndex, MapRef, bForceCompile))
				{
					// handle errors
					warnf(NAME_Warning, TEXT("Failed to compile Material %s for platform %s."), 
						*Material->GetPathName(), 
						ShaderPlatformToText(ShaderPlatform));

					DumpShaderCompileErrors( MaterialResource );
				}
				else
				{
					MapRef->BeginRelease();
				}
			}
		}
	}
	MaterialProcessingTime += appSeconds() - MaterialProcessingStart;
}

FLOAT MaterialInstanceProcessingTime = 0;

void UPrecompileShadersCommandlet::ProcessMaterialInstance(UMaterialInstance* MaterialInstance, UBOOL bIsEditorOnlyPackage)
{
	DOUBLE MIProcessingStart = appSeconds();

	// only compile this material instance if it has a static permutation resource
	if(MaterialInstance->bHasStaticPermutationResource)
	{
		SET_WARN_COLOR(COLOR_WHITE);
		warnf(TEXT("Processing %s..."), *MaterialInstance->GetPathName());
		CLEAR_WARN_COLOR();

		UBOOL bForceCompile = FALSE;
		// if the name contains our force string, then flush the shader from the cache
		if (ForceName.Len() && MaterialInstance->GetPathName().ToUpper().InStr(ForceName.ToUpper()) != -1)
		{
			SET_WARN_COLOR(COLOR_DARK_YELLOW);
			warnf(TEXT("Flushing shaders for %s"), *MaterialInstance->GetPathName());
			CLEAR_WARN_COLOR();

			bForceCompile = TRUE;
		}

		for(INT PlatformIndex = 0;PlatformIndex < ShaderPlatforms.Num();PlatformIndex++)
		{
			const EShaderPlatform ShaderPlatform = ShaderPlatforms(PlatformIndex);
    
			if (bIsEditorOnlyPackage && (ShaderPlatform==SP_XBOXD3D || ShaderPlatform==SP_PS3 || ShaderPlatform==SP_NGP))
			{
				// Skip this editor-only packages for this platform
				continue;
			}

		    // compile the material instance's shaders for the platform
		    MaterialInstance->CacheResourceShaders(ShaderPlatform, bForceCompile, FALSE);

			for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
			{
				// don't need to set the parameters if we don't have a quality switch
				if (QualityIndex != MSQ_HIGH && !MaterialInstance->bHasQualitySwitch)
				{
					continue;
				}

				FMaterialResource* MaterialResource = MaterialInstance->GetMaterialResource((EMaterialShaderQuality)QualityIndex);
				TRefCountPtr<FMaterialShaderMap> MaterialShaderMapRef = MaterialResource ? MaterialResource->GetShaderMap() : NULL;
				if (!MaterialShaderMapRef)
				{
					// handle errors
					const UMaterial* BaseMaterial = MaterialInstance->GetMaterial();
					warnf(NAME_Warning, TEXT("Failed to compile Material Instance %s with Base %s for platform %s with quality level %d."), 
						*GetPathName(), 
						BaseMaterial ? *BaseMaterial->GetName() : TEXT("Null"), 
						ShaderPlatformToText(ShaderPlatform),
						QualityIndex);

					DumpShaderCompileErrors( MaterialResource );
				}
				else
				{
					MaterialResource->SetShaderMap(NULL);
				}
			}
		}
	}
	MaterialInstanceProcessingTime += appSeconds() - MIProcessingStart;
}

FLOAT TerrainProcessingTime = 0;

void UPrecompileShadersCommandlet::ProcessTerrain(ATerrain* Terrain)
{
	DOUBLE TerrainProcessingStart = appSeconds();

	// Make sure materials are compiled for the target platform and add them to the shader cache embedded into seekfree packages.
	for(INT PlatformIndex = 0;PlatformIndex < ShaderPlatforms.Num();PlatformIndex++)
	{
		const EShaderPlatform ShaderPlatform = ShaderPlatforms(PlatformIndex);

		TArrayNoInit<FTerrainMaterialResource*>& CachedMaterials = Terrain->GetCachedTerrainMaterials();
	    for (INT CachedMatIndex = 0; CachedMatIndex < CachedMaterials.Num(); CachedMatIndex++)
	    {
		    FTerrainMaterialResource* TMatRes = CachedMaterials(CachedMatIndex);
		    if (TMatRes)
		    {
			    SET_WARN_COLOR(COLOR_WHITE);
			    warnf(TEXT("Processing %s[%d]..."), *Terrain->GetPathName(), CachedMatIndex);
			    CLEAR_WARN_COLOR();
    
			    // Compile the material...
			    FStaticParameterSet EmptySet(TMatRes->GetId());
			    TRefCountPtr<FMaterialShaderMap> MaterialShaderMapRef;
			    if (!TMatRes->Compile(&EmptySet, ShaderPlatform, MSQ_TERRAIN, MaterialShaderMapRef, FALSE))
			    {
				    // handle errors
				    warnf(NAME_Warning, TEXT("Terrain material failed to be compiled:"));

					DumpShaderCompileErrors( TMatRes );
			    }
				else
				{
					MaterialShaderMapRef->BeginRelease();
				}
		    }
	    }
	}
	TerrainProcessingTime += appSeconds() - TerrainProcessingStart;
}

INT UPrecompileShadersCommandlet::Main(const FString& Params)
{
#if !SHIPPING_PC_GAME
	// in a shipping game, don't bother checking for another instance, won't find it

	if( ( !GIsFirstInstance ) && ( ParseParam(appCmdLine(),TEXT("ALLOW_PARALLEL_PRECOMPILESHADERS")) == FALSE ) )
	{
		SET_WARN_COLOR_AND_BACKGROUND(COLOR_RED, COLOR_WHITE);
		warnf(TEXT(""));
		warnf(TEXT("ANOTHER INSTANCE IS RUNNING, THIS WILL NOT BE ABLE TO SAVE SHADER CACHE... QUITTING!"));
		CLEAR_WARN_COLOR();
		return 1;
	}
#endif

	// get some parameter
	FString Platform;
	if (!Parse(*Params, TEXT("PLATFORM="), Platform))
	{
		SET_WARN_COLOR(COLOR_YELLOW);
		warnf(TEXT("Usage: PrecompileShaders platform=<platform> [package=<package> [-depends]] [-refcache] [-skipmaps]\n   Platform is ps3, xbox360, allpc, pc_sm3, allpc, or all\n   Specifying no package will look in all packages\n   -depends will also look in dependencies of the package"));
		CLEAR_WARN_COLOR();
		return 1;
	}

	UBOOL bProcessDependencies = FALSE;

	// are we making the ref shader cache?
	UBOOL bSaveReferenceShaderCache = ParseParam(*Params, TEXT("refcache"));

	// Whether we should skip maps or not.
	UBOOL bShouldSkipMaps = ParseParam(*Params, TEXT("skipmaps"));

	// Determine the platform DLL and shader platform specified by the command-line platform ID.
	const UBOOL bCompileForAllPlatforms = (Platform == TEXT("ALL"));
	if(bCompileForAllPlatforms || Platform == TEXT("PS3"))
	{
		ShaderPlatforms.AddItem(SP_PS3);
	}
	if(bCompileForAllPlatforms || Platform == TEXT("xenon") || Platform == TEXT("xbox360"))
	{	
		ShaderPlatforms.AddItem(SP_XBOXD3D);
	}
	if(bCompileForAllPlatforms || Platform == TEXT("allpc") || Platform == TEXT("pc") || Platform == TEXT("pc_sm3"))
	{
		ShaderPlatforms.AddItem(SP_PCD3D_SM3);
	}
	if( !ShaderUtils::ShouldForceSM3ShadersOnPC() )
	{
		if (bCompileForAllPlatforms || Platform == TEXT("allpc") || Platform == TEXT("pc_sm5"))
		{
			ShaderPlatforms.AddItem(SP_PCD3D_SM5);
		}
		if (bCompileForAllPlatforms || Platform == TEXT("allpc") || Platform == TEXT("pc_ogl"))
		{
			ShaderPlatforms.AddItem(SP_PCOGL);
		}
	}
	if(bCompileForAllPlatforms || Platform == TEXT("wiiu"))
	{	
		ShaderPlatforms.AddItem(SP_WIIU);
	}

	// Find the target platform PC-side support implementations.
	static const TCHAR* ShaderPlatformToPlatformNameMap[] =
	{
		TEXT("PC"),			// SP_PCD3D_SM3
		TEXT("PS3"),		// SP_PS3
		TEXT("Xbox360"),	// SP_XBOXD3D
		TEXT("PC"),			// SP_PCD3D_SM4
		TEXT("PC"),			// SP_PCD3D_SM5
		TEXT("PC"),			// SP_NGP
		TEXT("PC"),			// SP_PCOGL
		TEXT("WiiU"),		// SP_WIIU
	};
	checkAtCompileTime(ARRAY_COUNT(ShaderPlatformToPlatformNameMap) == SP_NumPlatforms, ERROR_UpdateThis);
	for(INT PlatformIndex = 0;PlatformIndex < ShaderPlatforms.Num();PlatformIndex++)
	{
		const EShaderPlatform ShaderPlatform = ShaderPlatforms(PlatformIndex);
		const TCHAR* ConsolePlatform = ShaderPlatformToPlatformNameMap[ShaderPlatform];
		FConsoleSupport* ConsoleSupport = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport(ConsolePlatform);
		if(!ConsoleSupport)
		{
			SET_WARN_COLOR(COLOR_RED);
			warnf(NAME_Error, TEXT("Can't load the platform DLL for %s - shaders won't be compiled for that platform!"), ConsolePlatform);
			CLEAR_WARN_COLOR();

			ShaderPlatforms.Remove(PlatformIndex--);
		}
		else
		{
			warnf(TEXT("Compiling shaders for %s"),ShaderPlatformToText(ShaderPlatform));

			// Create the shader precompiler for the target platform.
			check(!GConsoleShaderPrecompilers[ShaderPlatform]);
			if( ShaderPlatform != SP_PCD3D_SM3 &&
				ShaderPlatform != SP_PCD3D_SM5 &&
				ShaderPlatform != SP_PCOGL )
			{
				GConsoleShaderPrecompilers[ShaderPlatform] = ConsoleSupport->GetGlobalShaderPrecompiler();
			}
			// force a load now
			GetLocalShaderCache( ShaderPlatform );
		}
	}

	if(!ShaderPlatforms.Num())
	{
		SET_WARN_COLOR(COLOR_RED);
		warnf(NAME_Error, TEXT("No valid platforms specified. Run with no parameters for list of known platforms."));
		CLEAR_WARN_COLOR();
		return 1;
	}

//	UBOOL bPrecook = ParseParam(*Params, TEXT("precook"));
	UBOOL bPrecook = !ParseParam(*Params, TEXT("LegacyDoSM3"));

	if (!bCompileForAllPlatforms && bPrecook && ShaderPlatforms.Num() == 1)
	{
		// Code for an optimized PCS
		// the idea here is to NOT compile any SM3 shaders
		if(ShaderPlatforms(0) == SP_PS3)
		{
			GIsCooking				= TRUE;
			GCookingTarget 			= UE3::PLATFORM_PS3;
			GCookingShaderPlatform	= SP_PS3;
			warnf(NAME_Log, TEXT("Only compiling PS3 shaders; no PC."));
		}
		else if(ShaderPlatforms(0) == SP_XBOXD3D)
		{	
			GIsCooking				= TRUE;
			GCookingTarget 			= UE3::PLATFORM_Xbox360;
			GCookingShaderPlatform	= SP_XBOXD3D;
			warnf(NAME_Log, TEXT("Only compiling 360 shaders; no PC."));
		}
		else if(ShaderPlatforms(0) == SP_WIIU)
		{	
			GIsCooking				= TRUE;
			GCookingTarget 			= UE3::PLATFORM_WiiU;
			GCookingShaderPlatform	= SP_WIIU;
			warnf(NAME_Log, TEXT("Only compiling WiiU shaders; no PC."));
		}
	}

	DOUBLE StartProcessingTime = appSeconds();
	DOUBLE PackageLoopSetupStartTime = appSeconds();

	// figure out what packages to iterate over
	TArray<FString> PackageList;
	FString SinglePackage;
	if (Parse(*Params, TEXT("PACKAGE="), SinglePackage))
	{
		FString PackagePath;
		if (!GPackageFileCache->FindPackageFile(*SinglePackage, NULL, PackagePath))
		{
			SET_WARN_COLOR(COLOR_RED);
			warnf(NAME_Error, TEXT("Failed to find file %s"), *SinglePackage);
			CLEAR_WARN_COLOR();
			return 1;
		}
		else
		{
			PackageList.AddItem(PackagePath);

			bProcessDependencies = ParseParam(*Params, TEXT("depends"));
		}
	}
	else
	{
		PackageList = GPackageFileCache->GetPackageFileList();
	}

	// Parse the ForceName if it is present.
	Parse(*Params, TEXT("FORCE="), ForceName);

	// make sure we have all global shaders for the platform
	for(INT PlatformIndex = 0;PlatformIndex < ShaderPlatforms.Num();PlatformIndex++)
	{
		const EShaderPlatform ShaderPlatform = ShaderPlatforms(PlatformIndex);
		VerifyGlobalShaders(ShaderPlatform);
	}

	FLOAT PackageLoopSetupTime = appSeconds() - PackageLoopSetupStartTime;

	FLOAT PackageLoadingTime = 0;
	FLOAT ObjectLoadingTime = 0;
	FLOAT TotalProcessingTime = 0;
	FLOAT GCTime = 0;
	FLOAT PackagePrologueTime = 0;
	FLOAT DependencyProcessingTime = 0;
	FLOAT DistributedShaderCompilingTime = 0;
	FLOAT ShaderCacheSavingTime = 0;
	UINT NumFastPackages = 0;
	UINT NumFullyLoadedPackages = 0;

	TMap<INT, DOUBLE> PackageTimes;
	DOUBLE PackageStartTime;
	DOUBLE PackageTotalTime = 0;
	
	TArray<FString> EditorOnlyPackagesArray;
	GConfig->GetArray(TEXT("UnrealEd.EditorEngine"), TEXT("EditorOnlyContentPackages"), EditorOnlyPackagesArray, GEngineIni);	
	for( INT Idx=0;Idx<EditorOnlyPackagesArray.Num();Idx++ )
	{
		EditorOnlyPackagesArray(Idx) = EditorOnlyPackagesArray(Idx).ToLower();
	}

	SaveLocalShaderCaches();

	for (INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++)
	{
		SET_WARN_COLOR(COLOR_DARK_GREEN);
		warnf(TEXT("Starting package %d of %d..."), PackageIndex + 1, PackageList.Num());
		CLEAR_WARN_COLOR();

		PackageStartTime = appSeconds();

		DOUBLE PackagePrologueTimeStart = appSeconds();

		// Skip editor-only packages when compiling for consoles
		UBOOL bIsEditorOnlyPackage = EditorOnlyPackagesArray.FindItemIndex( *FFilename(PackageList(PackageIndex)).GetBaseFilename().ToLower()) != INDEX_NONE;

		if (bIsEditorOnlyPackage)
		{
			// See if we need to compile the editor-only package for any platform.
			UBOOL bFoundEditorPlatform = FALSE;
			for(INT PlatformIndex = 0;PlatformIndex < ShaderPlatforms.Num();PlatformIndex++)
			{
				const EShaderPlatform ShaderPlatform = ShaderPlatforms(PlatformIndex);
				if ((ShaderPlatform==SP_XBOXD3D || ShaderPlatform==SP_PS3 || ShaderPlatform==SP_NGP))
				{
					SET_WARN_COLOR(COLOR_DARK_GREEN);
					warnf(TEXT("Skipping editor-only package %s for platform %s"), *PackageList(PackageIndex), ShaderPlatformToText(ShaderPlatform));
					CLEAR_WARN_COLOR();
				}
				else
				{
					bFoundEditorPlatform = TRUE;
				}
			}
			if( !bFoundEditorPlatform )
			{
				continue;
			}
		}

		// Skip maps with default map extension if -skipmaps command line option is used.
		if( bShouldSkipMaps && FFilename(PackageList(PackageIndex)).GetExtension() == FURL::DefaultMapExt )
		{
			SET_WARN_COLOR(COLOR_DARK_GREEN);
			warnf(TEXT("Skipping map %s..."), *PackageList(PackageIndex));
			CLEAR_WARN_COLOR();
			continue;
		}

		if (PackageList(PackageIndex). InStr(TEXT("ShaderCache")) != -1)
		{
			SET_WARN_COLOR(COLOR_DARK_GREEN);
			warnf(TEXT("Skipping shader cache %s..."), *PackageList(PackageIndex));
			CLEAR_WARN_COLOR();
			continue;
		}

		SET_WARN_COLOR(COLOR_GREEN);
		warnf(TEXT("Loading %s..."), *PackageList(PackageIndex));
		CLEAR_WARN_COLOR();

		ULinkerLoad* PackageLinker = NULL;
		UBOOL bFullyLoadPackage = FALSE;

		// Check if there are any terrain actors in the package.  
		// If there are, we need to load the whole package to workaround LoadObject<ATerrain>() failing.
		if (!bProcessDependencies)
		{
			// Load the package's linker.
			UObject::BeginLoad();
			PackageLinker = UObject::GetPackageLinker(NULL,*PackageList(PackageIndex),LOAD_None,NULL,NULL);
			UObject::EndLoad();

			if(PackageLinker)
			{
				for(INT ExportIndex = 0;ExportIndex < PackageLinker->ExportMap.Num();ExportIndex++)
				{
					if(PackageLinker->GetExportClassName(ExportIndex) == ATerrain::StaticClass()->GetFName())
					{
						// There is at least one terrain actor in the package, the whole package needs to be loaded.
						bFullyLoadPackage = TRUE;
						break;
					}
				}
			}
		}

		PackagePrologueTime += appSeconds() - PackagePrologueTimeStart;

		if (bProcessDependencies || bFullyLoadPackage)
		{
			NumFullyLoadedPackages++;
			// If we're processing dependencies, we need to load the package to determine its dependencies.

			DOUBLE StartLoadPackageTime = appSeconds();
			// Load the package.
			UPackage* Package = Cast<UPackage>(UObject::LoadPackage( NULL, *PackageList(PackageIndex), LOAD_None ));
			PackageLoadingTime += appSeconds() - StartLoadPackageTime;

			// go over all the materials
			for (TObjectIterator<UMaterial> It; It; ++It)
			{
				UMaterial* Material = *It;

				// if we are loading a single package, and we want to process its dependencies, then don't restrict by package
				if (bProcessDependencies || Material->IsIn(Package))
				{
					ProcessMaterial(Material,bIsEditorOnlyPackage);
				}
			}

			// go over all the material instances with static parameters
			for (TObjectIterator<UMaterialInstance> It; It; ++It)
			{
				UMaterialInstance* MaterialInstance = *It;
				// if we are loading a single package, and we want to process its dependencies, then don't restrict by package
				if (bProcessDependencies || MaterialInstance->IsIn(Package))
				{
					ProcessMaterialInstance(MaterialInstance,bIsEditorOnlyPackage);
				}
			}

			for (TObjectIterator<ATerrain> It; It; ++It)
			{
				ATerrain* Terrain = *It;
				if (Terrain && Terrain->IsIn(Package))
				{
					ProcessTerrain(Terrain);
				}
			}
		}
		else if (PackageLinker)
		{
			NumFastPackages++;
			// If we don't need to process dependencies, we can skip loading the entire package and only load the relevant objects.
			for(INT ExportIndex = 0;ExportIndex < PackageLinker->ExportMap.Num();ExportIndex++)
			{
				FName ClassName = PackageLinker->GetExportClassName(ExportIndex);
				FName ClassPackageName = PackageLinker->GetExportClassPackage(ExportIndex);

				DOUBLE StartLoadObjectTime = appSeconds();
				// Load the object's class so we can do a robust comparison, handling inheritance.
				UClass* ObjectClass = LoadObject<UClass>(NULL,*(ClassPackageName.ToString() + "." + ClassName.ToString()),NULL,LOAD_None,NULL);
				ObjectLoadingTime += appSeconds() - StartLoadObjectTime;

				if (ObjectClass)
				{
					if (ObjectClass->IsChildOf(UMaterial::StaticClass()))
					{
						StartLoadObjectTime = appSeconds();
						UMaterial* Material = LoadObject<UMaterial>(NULL,*PackageLinker->GetExportPathName(ExportIndex),*PackageList(PackageIndex),LOAD_None,NULL);
						ObjectLoadingTime += appSeconds() - StartLoadObjectTime;

						if(Material)
						{
							ProcessMaterial(Material,bIsEditorOnlyPackage);
						}
					}
					else if (ObjectClass->IsChildOf(UMaterialInstance::StaticClass()))
					{
						StartLoadObjectTime = appSeconds();
						UMaterialInstance* MaterialInstance = LoadObject<UMaterialInstance>(NULL,*PackageLinker->GetExportPathName(ExportIndex),*PackageList(PackageIndex),LOAD_None,NULL);
						ObjectLoadingTime += appSeconds() - StartLoadObjectTime;

						if(MaterialInstance)
						{
							ProcessMaterialInstance(MaterialInstance,bIsEditorOnlyPackage);
						}
					}
				}

				if(ClassName == ATerrain::StaticClass()->GetFName())
				{
					appErrorf(TEXT("Terrain actor found when trying to use the fast path!"));
				}
			}
		}

		// Only garbage collect every 10 packages to balance a reduction in
		// time spent doing it and memory cost of not doing it.
		if (((PackageIndex + 1) % 10) == 0)
		{
			DOUBLE StartGCTime = appSeconds();
			// close the package
			UObject::CollectGarbage(RF_Native);
			GCTime += appSeconds() - StartGCTime;
		}

		DOUBLE CurrentPackageTime = appSeconds() - PackageStartTime;
		PackageTimes.Set(PackageIndex, CurrentPackageTime);
		PackageTotalTime += CurrentPackageTime;

		const DOUBLE StartDistributedTime = appSeconds();

		// Allow distributed shader compiling to process if needed
		GShaderCompilingThreadManager->ConditionallyCompileForPCS();

		DistributedShaderCompilingTime += appSeconds() - StartDistributedTime;
	}

	// Finish GC'ing for any packages not GC'd during the loop above
	DOUBLE StartGCTime = appSeconds();
	// close the package
	UObject::CollectGarbage(RF_Native);
	GCTime += appSeconds() - StartGCTime;

	DOUBLE StartCacheSavingTime = appSeconds();
	// save out the local shader caches
	SaveLocalShaderCaches();
	ShaderCacheSavingTime += appSeconds() - StartCacheSavingTime;

	TotalProcessingTime += appSeconds() - StartProcessingTime;
	extern DOUBLE GRHIShaderCompileTime_Total;
	extern DOUBLE GRHIShaderCompileTime_PS3;
	extern DOUBLE GRHIShaderCompileTime_XBOXD3D;
	extern DOUBLE GRHIShaderCompileTime_NGP;
	extern DOUBLE GRHIShaderCompileTime_WIIU;
	extern DOUBLE GRHIShaderCompileTime_PCD3D_SM3;
	extern DOUBLE GRHIShaderCompileTime_PCD3D_SM5;
	extern DOUBLE GRHIShaderCompileTime_PCOGL;
	const FLOAT BreakdownTimeTotal = MaterialProcessingTime
		                           + MaterialInstanceProcessingTime
								   + TerrainProcessingTime
								   + PackageLoadingTime
								   + ObjectLoadingTime
								   + GCTime
								   + ShaderCacheSavingTime
								   + PackageLoopSetupTime
								   + PackagePrologueTime
								   + DistributedShaderCompilingTime;

	warnf(TEXT("Package processing complete."));
	warnf(TEXT("	TotalProcessingTime      = %.2f min"), TotalProcessingTime / 60.0f);
	warnf(TEXT("	Material                 = %.2f min"), MaterialProcessingTime / 60.0f);
	warnf(TEXT("	Material Instance        = %.2f min"), MaterialInstanceProcessingTime / 60.0f);
	warnf(TEXT("	Terrain                  = %.2f min"), TerrainProcessingTime / 60.0f);
	warnf(TEXT("	PackageLoopSetupTime     = %.2f min"), PackageLoopSetupTime / 60.0f);
	warnf(TEXT("	PackagePrologueTime      = %.2f min"), PackagePrologueTime / 60.0f);
	warnf(TEXT("	PackageLoadingTime       = %.2f min"), PackageLoadingTime / 60.0f);
	warnf(TEXT("	  NumFullyLoadedPackages = %u"), NumFullyLoadedPackages);
	warnf(TEXT("	  NumFastPackages        = %u"), NumFastPackages);
	warnf(TEXT("	ObjectLoadingTime        = %.2f min"), ObjectLoadingTime / 60.0f);
	warnf(TEXT("	GCTime                   = %.2f min"), GCTime / 60.0f);
	warnf(TEXT("	DistributedCompiling     = %.2f min"), DistributedShaderCompilingTime / 60.0f);
	warnf(TEXT("	ShaderCacheSavingTime    = %.2f min"), ShaderCacheSavingTime / 60.0f);
	warnf(TEXT("	RHIShaderCompileTime     = %.2f min"), GRHIShaderCompileTime_Total / 60.0f);
	warnf(TEXT("	  PS3                    = %.2f min"), GRHIShaderCompileTime_PS3 / 60.0f);
	warnf(TEXT("	  XBOXD3D                = %.2f min"), GRHIShaderCompileTime_XBOXD3D / 60.0f);
	warnf(TEXT("	  NGP                    = %.2f min"), GRHIShaderCompileTime_NGP / 60.0f);
	warnf(TEXT("	  WIIU                   = %.2f min"), GRHIShaderCompileTime_WIIU / 60.0f);
	warnf(TEXT("	  PCD3D_SM3              = %.2f min"), GRHIShaderCompileTime_PCD3D_SM3 / 60.0f);
	warnf(TEXT("	  PCD3D_SM5              = %.2f min"), GRHIShaderCompileTime_PCD3D_SM5 / 60.0f);
	warnf(TEXT("	  PCOGL                  = %.2f min"), GRHIShaderCompileTime_PCOGL / 60.0f);
	warnf(TEXT("	UnaccountedTime          = %.2f min"), (TotalProcessingTime - BreakdownTimeTotal) / 60.0f);

	// Compute the standard deviation for the package times
	DOUBLE AllPackagesMean = PackageTotalTime / PackageTimes.Num();
	DOUBLE AllPackagesDifferenceSquaredSum = 0;
	DOUBLE PackageMeanDifference;
	for (TMap<INT, DOUBLE>::TConstIterator PackageTimesIt(PackageTimes); PackageTimesIt; ++PackageTimesIt)
	{
		PackageMeanDifference = PackageTimesIt.Value() - AllPackagesMean;
		AllPackagesDifferenceSquaredSum += (PackageMeanDifference * PackageMeanDifference);
	}
	DOUBLE AllPackagesStandardDeviation = sqrt(AllPackagesDifferenceSquaredSum / PackageTimes.Num());

	// Warn about any packages with times greater than a couple standard deviations out
	SET_WARN_COLOR(COLOR_YELLOW);
	DOUBLE WarningTimeLimit = AllPackagesMean + (3 * AllPackagesStandardDeviation);
	warnf(TEXT("\n"));
	for (TMap<INT, DOUBLE>::TConstIterator PackageTimesIt(PackageTimes); PackageTimesIt; ++PackageTimesIt)
	{
		if (PackageTimesIt.Value() > WarningTimeLimit)
		{
			warnf(TEXT("Warning: Package time greater than 3 standard deviations!"));
			warnf(TEXT("    Name = %s"), *PackageList(PackageTimesIt.Key()));
			warnf(TEXT("    Time = %.2f min"), PackageTimesIt.Value() / 60.0f);
			warnf(TEXT("\n"));
		}
	}
	CLEAR_WARN_COLOR();

	// Optionally dump all of the individual package times
	debugf(NAME_DevShadersDetailed, TEXT("\nPackage Times:"));
	for (TMap<INT, DOUBLE>::TConstIterator PackageTimesIt(PackageTimes); PackageTimesIt; ++PackageTimesIt)
	{
		debugf(NAME_DevShadersDetailed, TEXT("	%s"), *PackageList(PackageTimesIt.Key()));
		debugf(NAME_DevShadersDetailed, TEXT("	                         = %.2f min"), PackageTimesIt.Value() / 60.0f);
	}

	// Reset the shader precompiler reference to NULL;
	for(INT PlatformIndex = 0;PlatformIndex < ShaderPlatforms.Num();PlatformIndex++)
	{
		const EShaderPlatform ShaderPlatform = ShaderPlatforms(PlatformIndex);

		// destroy the precompiler objects
		GConsoleShaderPrecompilers[ShaderPlatform] = NULL;
	}

	// save out the local shader caches
	SaveLocalShaderCaches();

	// if we're saving the ref shader cache, save it now
	if (bSaveReferenceShaderCache)
	{
		for(INT PlatformIndex = 0;PlatformIndex < ShaderPlatforms.Num();PlatformIndex++)
		{
			const EShaderPlatform ShaderPlatform = ShaderPlatforms(PlatformIndex);

			// mark it as dirty so it will save
			GetLocalShaderCache(ShaderPlatform)->MarkDirty();

			// save it as the reference package
			SaveLocalShaderCache(ShaderPlatform,*GetReferenceShaderCacheFilename(ShaderPlatform));
		}
	}

	return 0;
}
IMPLEMENT_CLASS(UPrecompileShadersCommandlet)

/*-----------------------------------------------------------------------------
	UDumpShadersCommandlet implementation.
-----------------------------------------------------------------------------*/

INT UDumpShadersCommandlet::Main(const FString& Params)
{
	// get some parameter
	FString Platform;
	if (!Parse(*Params, TEXT("PLATFORM="), Platform))
	{
		warnf(NAME_Warning, TEXT("Usage: DumpShaders platform=<platform> [globalshader=<shadertype>] [material=<materialname>]\n Platform is ps3, xenon, allpc, pc_sm3"));
		return 1;
	}

	// Determine the platform DLL and shader platform specified by the command-line platform ID.
	const UBOOL bCompileForAllPlatforms = (Platform == TEXT("ALL"));
	if(bCompileForAllPlatforms || Platform == TEXT("PS3"))
	{
		ShaderPlatforms.AddItem(SP_PS3);
	}
	if(bCompileForAllPlatforms || Platform == TEXT("xenon") || Platform == TEXT("xbox360"))
	{	
		ShaderPlatforms.AddItem(SP_XBOXD3D);
	}
	if(bCompileForAllPlatforms || Platform == TEXT("allpc") || Platform == TEXT("pc") || Platform == TEXT("pc_sm3"))
	{
		ShaderPlatforms.AddItem(SP_PCD3D_SM3);
	}
	if(bCompileForAllPlatforms || Platform == TEXT("wiiu"))
	{	
		ShaderPlatforms.AddItem(SP_WIIU);
	}
	if( !ShaderUtils::ShouldForceSM3ShadersOnPC() )
	{
		if (bCompileForAllPlatforms || Platform == TEXT("allpc") || Platform == TEXT("pc_sm5"))
		{
			ShaderPlatforms.AddItem(SP_PCD3D_SM5);
		}
		if (bCompileForAllPlatforms || Platform == TEXT("allpc") || Platform == TEXT("pc_ogl"))
		{
			ShaderPlatforms.AddItem(SP_PCOGL);
		}
	}

	// Find the target platform PC-side support implementations.
	static const TCHAR* ShaderPlatformToPlatformNameMap[] =
	{
		TEXT("PC"),			// SP_PCD3D_SM3
		TEXT("PS3"),		// SP_PS3
		TEXT("Xbox360"),	// SP_XBOXD3D
		TEXT("PC"),			// SP_PCD3D_SM4
		TEXT("PC"),			// SP_PCD3D_SM5
		TEXT("PC"),			// SP_NGP
		TEXT("PC"),			// SP_PCOGL
		TEXT("WiiU"),		// SP_WIIU
	};
	checkAtCompileTime(ARRAY_COUNT(ShaderPlatformToPlatformNameMap) == SP_NumPlatforms, ERROR_UpdateThis);
	for(INT PlatformIndex = 0;PlatformIndex < ShaderPlatforms.Num();PlatformIndex++)
	{
		const EShaderPlatform ShaderPlatform = ShaderPlatforms(PlatformIndex);
		const TCHAR* ConsolePlatform = ShaderPlatformToPlatformNameMap[ShaderPlatform];
		FConsoleSupport* ConsoleSupport = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport(ConsolePlatform);
		if(!ConsoleSupport)
		{
			warnf(NAME_Error, TEXT("Can't load the platform DLL for %s - shaders won't be compiled for that platform!"), ConsolePlatform);
			ShaderPlatforms.Remove(PlatformIndex--);
		}
		else
		{
			GetLocalShaderCache(ShaderPlatform);
			warnf(TEXT("Compiling shaders for %s"),ShaderPlatformToText(ShaderPlatform));

			// Create the shader precompiler for the target platform.
			check(!GConsoleShaderPrecompilers[ShaderPlatform]);
			if( ShaderPlatform != SP_PCD3D_SM3 &&
				ShaderPlatform != SP_PCD3D_SM5 &&
				ShaderPlatform != SP_PCOGL)
			{
				GConsoleShaderPrecompilers[ShaderPlatform] = ConsoleSupport->GetGlobalShaderPrecompiler();
			}
		}
	}

	if(!ShaderPlatforms.Num())
	{
		warnf(NAME_Error, TEXT("No valid platforms specified. Run with no parameters for list of known platforms."));
		return 1;
	}

	FString GlobalShaderTypeName;
	UBOOL bOperatingOnGlobalShader = FALSE;
	UBOOL bFoundGlobalShader = FALSE;
	if (Parse(*Params, TEXT("GLOBALSHADER="), GlobalShaderTypeName))
	{
		bOperatingOnGlobalShader = TRUE;
	}

	FString MaterialName;
	Parse(*Params, TEXT("MATERIAL="), MaterialName);

	if (!bOperatingOnGlobalShader && MaterialName.Len() == 0)
	{
		warnf(NAME_Error, TEXT("Missing global shader or material to operate on.  Run without any parameters to see usage."));
		return 1;
	}

	if (bOperatingOnGlobalShader)
	{
		// Flush compilation so that only global shaders will be in the next results
		GShaderCompilingThreadManager->FinishDeferredCompilation();

		for (INT PlatformIndex = 0;PlatformIndex < ShaderPlatforms.Num();PlatformIndex++)
		{
			const EShaderPlatform ShaderPlatform = ShaderPlatforms(PlatformIndex);

			// search for the specified global shader
			TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(ShaderPlatform);
			for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
			{
				FGlobalShaderType* GlobalShaderType = ShaderTypeIt->GetGlobalShaderType();
				if (GlobalShaderType != NULL && GlobalShaderType->ShouldCache(ShaderPlatform))
				{
					FString TypeName = FString(GlobalShaderType->GetName());
					if (TypeName.ToUpper().InStr(GlobalShaderTypeName.ToUpper()) != -1)
					{
						// preprocess this global shader type
						GlobalShaderType->BeginCompileShader(ShaderPlatform);
						bFoundGlobalShader = TRUE;
					}
				}
			}

			TArray<TRefCountPtr<FShaderCompileJob> > CompilationResults;
			GShaderCompilingThreadManager->FinishCompiling(CompilationResults, TEXT("Global"), TRUE, TRUE);
		}
	}

	if (bOperatingOnGlobalShader)
	{
		if (bFoundGlobalShader)
		{
			SET_WARN_COLOR(COLOR_GREEN);
			warnf(TEXT("Preprocessed all global shaders matching: %s.\nDone."), *GlobalShaderTypeName);
			CLEAR_WARN_COLOR();
			return 0;
		}
		else
		{
			warnf(NAME_Error, TEXT("Could not find a global shader matching: %s.\nDone."), *GlobalShaderTypeName);
			return 1;
		}
	}

	UBOOL bMaterialDone = FALSE;

	// figure out what packages to iterate over
	FString SinglePackage;
	TArray<FString> PackageList;
	INT PackageEndIndex = MaterialName.InStr(TEXT("."));
	if (PackageEndIndex != INDEX_NONE)
	{
		SinglePackage = MaterialName.Left(PackageEndIndex);
		FString PackagePath;
		if (!GPackageFileCache->FindPackageFile(*SinglePackage, NULL, PackagePath))
		{
			warnf(NAME_Error, TEXT("Failed to find specified package %s"), *SinglePackage);
			return 1;
		}
		else
		{
			check(MaterialName.Len() > PackageEndIndex);
			MaterialName = MaterialName.Right(MaterialName.Len() - (PackageEndIndex + 1));
			PackageList.AddItem(PackagePath);
		}
	}
	else
	{
		warnf(NAME_Warning, TEXT("No package specified, searching through all packages which may be slow."), *SinglePackage);
		PackageList = GPackageFileCache->GetPackageFileList();
	}

	for (INT PackageIndex = 0; PackageIndex < PackageList.Num() && !bMaterialDone; PackageIndex++)
	{
		if (PackageList(PackageIndex). InStr(TEXT("ShaderCache")) != -1)
		{
			SET_WARN_COLOR(COLOR_DARK_GREEN);
			warnf(TEXT("Skipping shader cache %s..."), *PackageList(PackageIndex));
			CLEAR_WARN_COLOR();
			continue;
		}

		SET_WARN_COLOR(COLOR_GREEN);
		warnf(TEXT("Loading %s..."), *PackageList(PackageIndex));
		CLEAR_WARN_COLOR();

		// Load the package's linker.
		UObject::BeginLoad();
		ULinkerLoad* PackageLinker = UObject::GetPackageLinker(NULL,*PackageList(PackageIndex),LOAD_None,NULL,NULL);
		UObject::EndLoad();

		if (PackageLinker)
		{
			// If we don't need to process dependencies, we can skip loading the entire package and only load the relevant objects.
			for(INT ExportIndex = 0;ExportIndex < PackageLinker->ExportMap.Num();ExportIndex++)
			{
				FName ClassName = PackageLinker->GetExportClassName(ExportIndex);
				FName ClassPackageName = PackageLinker->GetExportClassPackage(ExportIndex);

				DOUBLE StartLoadObjectTime = appSeconds();
				// Load the object's class so we can do a robust comparison, handling inheritance.
				UClass* ObjectClass = LoadObject<UClass>(NULL,*(ClassPackageName.ToString() + "." + ClassName.ToString()),NULL,LOAD_None,NULL);

				if (ObjectClass)
				{
					if (ObjectClass->IsChildOf(UMaterialInterface::StaticClass()))
					{
						const FString ExportPathName = PackageLinker->GetExportPathName(ExportIndex);
						// Check if this is the material we are looking for
						if (ExportPathName.ToUpper().InStr(MaterialName.ToUpper()) != INDEX_NONE)
						{
							UMaterialInterface* MaterialInterface = LoadObject<UMaterialInterface>(NULL,*ExportPathName,*PackageList(PackageIndex),LOAD_None,NULL);
							if (MaterialInterface)
							{
								if(MaterialInterface->IsTemplate())
								{
									// Material templates don't have shaders.
									continue;
								}

								// Flush deferred compilation so that only the shaders from the material being dumped will be in the queue
								GShaderCompilingThreadManager->FinishDeferredCompilation();

								SET_WARN_COLOR(COLOR_WHITE);
								warnf(TEXT("Processing %s..."), *MaterialInterface->GetPathName());
								CLEAR_WARN_COLOR();

								UMaterial* BaseMaterial = Cast<UMaterial>(MaterialInterface);
								UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(MaterialInterface);
								if (BaseMaterial)
								{
									// compile for the platform!
									for(INT PlatformIndex = 0;PlatformIndex < ShaderPlatforms.Num();PlatformIndex++)
									{
										const EShaderPlatform ShaderPlatform = ShaderPlatforms(PlatformIndex);
										TRefCountPtr<FMaterialShaderMap> MapRef;

										for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
										{
											FMaterialResource* MaterialResource = BaseMaterial->GetMaterialResource((EMaterialShaderQuality)QualityIndex);
											if (MaterialResource)
											{
												FStaticParameterSet EmptySet(MaterialResource->GetId());
												if (!MaterialResource->Compile(&EmptySet, ShaderPlatform, (EMaterialShaderQuality)QualityIndex, MapRef, TRUE, TRUE))
												{
													// handle errors
													warnf(NAME_Warning, TEXT("Material failed to be compiled:"));

													DumpShaderCompileErrors( MaterialResource );
												}
												else
												{
													MapRef->BeginRelease();
												}
											}
										}
									}
									bMaterialDone = TRUE;
									break;
								}
								else if (MaterialInstance)
								{
									for(INT PlatformIndex = 0;PlatformIndex < ShaderPlatforms.Num();PlatformIndex++)
									{
										const EShaderPlatform ShaderPlatform = ShaderPlatforms(PlatformIndex);

										// compile the material instance's shaders for the platform
										MaterialInstance->CacheResourceShaders(ShaderPlatform, TRUE, TRUE);

										for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
										{
											FMaterialResource* MaterialResource = MaterialInstance->GetMaterialResource((EMaterialShaderQuality)QualityIndex);
											TRefCountPtr<FMaterialShaderMap> MaterialShaderMapRef = MaterialResource ? MaterialResource->GetShaderMap() : NULL;
											if (!MaterialShaderMapRef)
											{
												// handle errors
												warnf(NAME_Warning, TEXT("Failed to compile Material Instance [quality level %d]:"), QualityIndex);
												DumpShaderCompileErrors( MaterialResource );
											}
											else
											{
												MaterialResource->SetShaderMap(NULL);
											}
										}
									}
									bMaterialDone = TRUE;
									break;
								}
							}
						}
					}
				}
			}
		}

		// close the package
		UObject::CollectGarbage(RF_Native);
	}

	if (MaterialName.Len() && !bMaterialDone)
	{
		warnf(TEXT("Material named %s not found"), *MaterialName );
	}

	// Reset the shader precompiler reference to NULL;
	for(INT PlatformIndex = 0;PlatformIndex < ShaderPlatforms.Num();PlatformIndex++)
	{
		const EShaderPlatform ShaderPlatform = ShaderPlatforms(PlatformIndex);

		// destroy the precompiler objects
		GConsoleShaderPrecompilers[ShaderPlatform] = NULL;
	}

	return 0;
}
IMPLEMENT_CLASS(UDumpShadersCommandlet)
