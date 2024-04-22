/*=============================================================================
	GlobalShader.cpp: Global shader implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "GlobalShader.h"
#include "GlobalShaderNGP.h"
#include "StaticBoundShaderState.h"

/** The global shader map. */
TShaderMap<FGlobalShaderType>* GGlobalShaderMap[SP_NumPlatforms];

IMPLEMENT_SHADER_TYPE(,FNULLPixelShader,TEXT("NULLPixelShader"),TEXT("Main"),SF_Pixel,0,0);
IMPLEMENT_SHADER_TYPE(,FOneColorVertexShader,TEXT("OneColorShader"),TEXT("MainVertexShader"),SF_Vertex,0,0);
IMPLEMENT_SHADER_TYPE(,FOneColorPixelShader,TEXT("OneColorShader"),TEXT("MainPixelShader"),SF_Pixel,0,0);
IMPLEMENT_SHADER_TYPE(,FSplashVertexShader,TEXT("SplashScreenShader"),TEXT("MainVertexShader"),SF_Vertex,0,0);
IMPLEMENT_SHADER_TYPE(,FSplashPixelShader,TEXT("SplashScreenShader"),TEXT("MainPixelShader"),SF_Pixel,0,0);
IMPLEMENT_SHADER_TYPE(,FRestoreColorAndDepthVertexShader,TEXT("RestoreColorDepthShader"),TEXT("MainVertexShader"),SF_Vertex,0,0);
IMPLEMENT_SHADER_TYPE(,FRestoreDepthOnlyPixelShader,TEXT("RestoreColorDepthShader"),TEXT("MainPixelShaderDepthOnly"),SF_Pixel,0,0);
IMPLEMENT_SHADER_TYPE(,FRestoreDownsamplingDepthOnlyPixelShader,TEXT("RestoreColorDepthShader"),TEXT("MainPixelShaderDownsamplingDepthOnly"),SF_Pixel,0,0);
IMPLEMENT_SHADER_TYPE(,FRestoreColorAndDepthPixelShader,TEXT("RestoreColorDepthShader"),TEXT("MainPixelShaderColorAndDepth"),SF_Pixel,0,0);
IMPLEMENT_SHADER_TYPE(,FRestoreDownsamplingColorAndDepthPixelShader,TEXT("RestoreColorDepthShader"),TEXT("MainPixelShaderDownsamplingColorAndDepth"),SF_Pixel,0,0);
IMPLEMENT_SHADER_TYPE(,FMemCopyVertexShader,TEXT("MemCopyShader"),TEXT("MainVertexShader"),SF_Vertex,0,0);

void FGlobalShaderType::BeginCompileShader(EShaderPlatform Platform)
{
	// Construct the shader environment.
	FShaderCompilerEnvironment Environment;

	warnf(NAME_DevShadersDetailed, TEXT("	%s"), GetName());

	// Enqueue the shader to be compiled
	FShaderType::BeginCompileShader(0, NULL, Platform, Environment);
}

FShader* FGlobalShaderType::FinishCompileShader(const FShaderCompileJob& CompileJob)
{
	if (CompileJob.bSucceeded)
	{
		FShader* Shader = FindShaderByOutput(CompileJob.Output);

		if (!Shader)
		{
			// Create the shader.
			Shader = (*ConstructCompiledRef)(CompiledShaderInitializerType(this,CompileJob.Output));
			CompileJob.Output.ParameterMap.VerifyBindingsAreComplete(GetName(), (EShaderFrequency)CompileJob.Output.Target.Frequency, CompileJob.VFType);
		}
		return Shader;
	}
	else
	{
		return NULL;
	}
}

void SerializeGlobalShaders(EShaderPlatform Platform,FArchive& Ar)
{
	check(IsInGameThread());

	/** An archive wrapper that is used to serialize the global shader map into a raw binary file. */
	class FGlobalShaderArchive : public FArchiveProxy
	{
	public:

		/** Initialization constructor. */
		FGlobalShaderArchive(FArchive& InInnerArchive)
		:	FArchiveProxy(InInnerArchive)
		{
			ArIsFinalPackageSave = TRUE;
		}

		// FArchive interface.
		virtual FArchive& operator<<( class FName& N )
		{
			// Serialize the name as a FString.
			if(InnerArchive.IsSaving())
			{
				FString NameString = N.ToString();
				InnerArchive << NameString;
			}
			else
			{
				FString NameString;
				InnerArchive << NameString;
				N = FName(*NameString);
			}
			return *this;
		}
		virtual FArchive& operator<<( class UObject*& Res )
		{
			appErrorf(TEXT("Global shader cache doesn't support object references"));
			return *this;
		}
	};

	// Serialize the global shader map binary file tag.
	static const DWORD ReferenceTag = 0x47534D42;
	if (Ar.IsLoading())
	{
		// Initialize Tag to 0 as it won't be written to if the serialize fails (ie the global shader cache file is empty)
		DWORD Tag = 0;
		Ar << Tag;
		checkf(Tag == ReferenceTag, TEXT("Global shader map binary file is missing GSMB tag."));
	}
	else
	{
		DWORD Tag = ReferenceTag;
		Ar << Tag;
	}

	// Serialize the version data.
	INT Version = GPackageFileVersion;
	INT LicenseeVersion = GPackageFileLicenseeVersion;
	Ar << Version;
	Ar << LicenseeVersion;
	if(Ar.IsLoading())
	{
		Ar.SetVer(Version);
		Ar.SetLicenseeVer(LicenseeVersion);
	}

	// Wrap the provided archive with our global shader proxy archive.
	FGlobalShaderArchive GlobalShaderArchive(Ar);

	if ( Platform == SP_NGP )
	{
		SerializeGlobalShadersNGP(GlobalShaderArchive);
	}
	else
	{
		TShaderMap<FGlobalShaderType>& GlobalShaderMap = *GetGlobalShaderMap(Platform);
		FShaderCache* GlobalShaderCache = GetGlobalShaderCache(Platform);
		check(GlobalShaderCache);

		// Serialize the global shaders.
		if(Ar.IsSaving())
		{
			TMap<FGuid,FShader*> GlobalShaders;
			GlobalShaderMap.GetShaderList(GlobalShaders);
			GlobalShaderCache->Save(GlobalShaderArchive,GlobalShaders,TRUE);
		}
		else
		{
			GlobalShaderCache->Load(GlobalShaderArchive);
		}

		// Serialize the global shader map.
		GlobalShaderMap.Serialize(GlobalShaderArchive);
	}
}

/**
 * Makes sure all global shaders are loaded and/or compiled for the passed in platform.
 *
 * @param	Platform	Platform to verify global shaders for
 */
void VerifyGlobalShaders(EShaderPlatform Platform)
{
	check(IsInGameThread());
	check(!(appGetPlatformType() & UE3::PLATFORM_WindowsServer));

	warnf(NAME_DevShaders, TEXT("Verifying Global Shaders for %s"), ShaderPlatformToText(Platform));

	UBOOL bGlobalShaderCacheNeedToBeSaved = FALSE;
	if ( Platform == SP_NGP )
	{
		
	}
	else
	{
		// Ensure that the global shader map contains all global shader types.
		TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(Platform);
		const UBOOL bEmptyMap = GlobalShaderMap->IsEmpty();
		if (bEmptyMap)
		{
			warnf(NAME_DevShaders, TEXT("	Empty global shader map, recompiling all global shaders"));
		}
	
		// Flush compilation so that only global shaders will be in the results
		GShaderCompilingThreadManager->FinishDeferredCompilation();

		for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
		{
			FGlobalShaderType* GlobalShaderType = ShaderTypeIt->GetGlobalShaderType();
			if(GlobalShaderType && GlobalShaderType->ShouldCache(Platform))
			{
				if(!GlobalShaderMap->HasShader(GlobalShaderType))
				{
#if CONSOLE
					appErrorf(TEXT("Missing global shader %s, Please make sure cooking was successful."), GlobalShaderType->GetName());
#endif
					if (!bEmptyMap)
					{
						warnf(NAME_DevShaders, TEXT("	%s"), GlobalShaderType->GetName());
					}
	
					// Compile this global shader type.
					GlobalShaderType->BeginCompileShader(Platform);
	
					// Indicate that the global shader cache needs to be saved.
					bGlobalShaderCacheNeedToBeSaved = TRUE;
				}
			}
		}

		TArray<TRefCountPtr<FShaderCompileJob> > CompilationResults;
		STAT(DOUBLE GlobalShaderCompilingTime = 0);
		{
			SCOPE_SECONDS_COUNTER(GlobalShaderCompilingTime);
			// Flush compiling
			GShaderCompilingThreadManager->FinishCompiling(CompilationResults, TEXT("Global"), TRUE, FALSE);
		}
	#if STATS
		if (CompilationResults.Num() > 0)
		{
			warnf(NAME_DevShaders, TEXT("Compiled %u global shaders in %.1fs"), CompilationResults.Num(), GlobalShaderCompilingTime);
		}
	#endif
		INC_FLOAT_STAT_BY(STAT_ShaderCompiling_GlobalShaders,(FLOAT)GlobalShaderCompilingTime);

		for (INT ResultIndex = 0; ResultIndex < CompilationResults.Num(); ResultIndex++)
		{
			const FShaderCompileJob& CurrentJob = *CompilationResults(ResultIndex);
			FGlobalShaderType* GlobalShaderType = CurrentJob.ShaderType->GetGlobalShaderType();
			check(GlobalShaderType);
			FShader* Shader = GlobalShaderType->FinishCompileShader(CurrentJob);
			if(Shader)
			{
				// Add the new global shader instance to the global shader map.
				// This will cause FShader::AddRef to be called, which will cause BeginInitResource(Shader) to be called.
				GlobalShaderMap->AddShader(GlobalShaderType,Shader);
			}
			else
			{
				appErrorf(TEXT("Failed to compile global shader %s"), GlobalShaderType->GetName());
			}
		}

		GGlobalShaderMap[Platform]->BeginInit();
	}

#if !CONSOLE
	if(bGlobalShaderCacheNeedToBeSaved)
	{
		// Save the global shader cache corresponding to this platform.
		FArchive* GlobalShaderFile = GFileManager->CreateFileWriter(*GetGlobalShaderCacheFilename(Platform));
		SerializeGlobalShaders(Platform,*GlobalShaderFile);
		delete GlobalShaderFile;
	}
#endif
}

TShaderMap<FGlobalShaderType>* GetGlobalShaderMap(EShaderPlatform Platform)
{
	// If the global shader map hasn't been created yet, create it.
	if(!GGlobalShaderMap[Platform])
	{
		// GetGlobalShaderMap is called the first time during startup in the main thread.
		check(IsInGameThread());

		GGlobalShaderMap[Platform] = new TShaderMap<FGlobalShaderType>();

		// Try to load the global shader map for this platform.
		FArchive* GlobalShaderFile = GFileManager->CreateFileReader(*GetGlobalShaderCacheFilename(Platform));
		if(GlobalShaderFile)
		{
			SerializeGlobalShaders(Platform,*GlobalShaderFile);
			delete GlobalShaderFile;
		}
		else if (CONSOLE)
		{
			appErrorf(TEXT("Couldn't find Global Shader Cache '%s', please recook."), *GetGlobalShaderCacheFilename(Platform));
		}

		// If any shaders weren't loaded, compile them now.
		VerifyGlobalShaders(Platform);
	}
	return GGlobalShaderMap[Platform];
}

/**
 * Forces a recompile of the global shaders.
 */
void RecompileGlobalShaders()
{
	if( !GUseSeekFreeLoading )
	{
		// Flush pending accesses to the existing global shaders.
		FlushRenderingCommands();

		GetGlobalShaderMap(GRHIShaderPlatform)->Empty();

		VerifyGlobalShaders(GRHIShaderPlatform);

		//invalidate global bound shader states so they will be created with the new shaders the next time they are set (in SetGlobalBoundShaderState)
		for(TLinkedList<FGlobalBoundShaderStateResource*>::TIterator It(FGlobalBoundShaderStateResource::GetGlobalBoundShaderStateList());It;It.Next())
		{
			BeginUpdateResourceRHI(*It);
		}
	}
}

/**
 * Recompiles the specified global shader types, and flushes their bound shader states.
 */
void RecompileGlobalShaders(const TArray<FShaderType*>& OutdatedShaderTypes)
{
	if( !GUseSeekFreeLoading )
	{
		// Flush pending accesses to the existing global shaders.
		FlushRenderingCommands();

		TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(GRHIShaderPlatform);

		for (INT TypeIndex = 0; TypeIndex < OutdatedShaderTypes.Num(); TypeIndex++)
		{
			FGlobalShaderType* CurrentGlobalShaderType = OutdatedShaderTypes(TypeIndex)->GetGlobalShaderType();
			if (CurrentGlobalShaderType)
			{
				debugf(TEXT("Flushing Global Shader %s"), CurrentGlobalShaderType->GetName());
				GlobalShaderMap->RemoveShaderType(CurrentGlobalShaderType);
				
				//invalidate global bound shader states so they will be created with the new shaders the next time they are set (in SetGlobalBoundShaderState)
				for(TLinkedList<FGlobalBoundShaderStateResource*>::TIterator It(FGlobalBoundShaderStateResource::GetGlobalBoundShaderStateList());It;It.Next())
				{
					BeginUpdateResourceRHI(*It);
				}
			}
		}

		VerifyGlobalShaders(GRHIShaderPlatform);
	}
}

