/*=============================================================================
	MeshMaterialShader.cpp: Mesh material shader implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

/**
* Finds a FMeshMaterialShaderType by name.
*/
FMeshMaterialShaderType* FMeshMaterialShaderType::GetTypeByName(const FString& TypeName)
{
	for(TLinkedList<FShaderType*>::TIterator It(FShaderType::GetTypeList()); It; It.Next())
	{
		FString CurrentTypeName = FString(It->GetName());
		FMeshMaterialShaderType* CurrentType = It->GetMeshMaterialShaderType();
		if (CurrentType && CurrentTypeName == TypeName)
		{
			return CurrentType;
		}
	}
	return NULL;
}

/**
 * Enqueues a compilation for a new shader of this type.
 * @param Platform - The platform to compile for.
 * @param Material - The material to link the shader with.
 * @param MaterialShaderCode - The shader code for the material.
 * @param VertexFactoryType - The vertex factory to compile with.
 */
void FMeshMaterialShaderType::BeginCompileShader(
	UINT ShaderMapId,
	EShaderPlatform Platform,
	const FMaterial* Material,
	const ANSICHAR* MaterialShaderCode,
	FVertexFactoryType* VertexFactoryType
	)
{
	// Construct the shader environment.
	FShaderCompilerEnvironment Environment;
	
	Material->SetupMaterialEnvironment(Platform, VertexFactoryType, Environment);
	Environment.MaterialShaderCode = MaterialShaderCode;

	//update material shader stats
	UpdateMaterialShaderCompilingStats(Material);

	warnf(NAME_DevShadersDetailed, TEXT("			%s"), GetName());

	// Enqueue the compile
	FShaderType::BeginCompileShader(ShaderMapId, VertexFactoryType, Platform, Environment);
}

/**
 * Either creates a new instance of this type or returns an equivalent existing shader.
 * @param Material - The material to link the shader with.
 * @param CurrentJob - Compile job that was enqueued by BeginCompileShader.
 */
FShader* FMeshMaterialShaderType::FinishCompileShader(
	const FUniformExpressionSet& UniformExpressionSet,
	const FShaderCompileJob& CurrentJob)
{
	check(CurrentJob.bSucceeded);
	check(CurrentJob.VFType);

	// Check for shaders with identical compiled code.
	//@todo - don't share shaders with different vertex factory parameter classes
	FShader* Shader = FindShaderByOutput(CurrentJob.Output);

	if (!Shader)
	{
		// Create the shader.
		CurrentJob.Output.ParameterMap.UniformExpressionSet = &UniformExpressionSet;
		Shader = (*ConstructCompiledRef)(CompiledShaderInitializerType(this,CurrentJob.Output,CurrentJob.VFType));
		CurrentJob.Output.ParameterMap.VerifyBindingsAreComplete(GetName(), (EShaderFrequency)CurrentJob.Output.Target.Frequency, CurrentJob.VFType);
	}
	return Shader;
}

/**
 * Enqueues compilation for all shaders for a material and vertex factory type.
 * @param Material - The material to compile shaders for.
 * @param MaterialShaderCode - The shader code for Material.
 * @param VertexFactoryType - The vertex factory type to compile shaders for.
 * @param Platform - The platform to compile for.
 */
UINT FMeshMaterialShaderMap::BeginCompile(
	UINT ShaderMapId,
	const FMaterial* Material,
	const ANSICHAR* MaterialShaderCode,
	FVertexFactoryType* InVertexFactoryType,
	EShaderPlatform Platform
	)
{
	UINT NumShadersPerVF = 0;
	VertexFactoryType = InVertexFactoryType;

	// Iterate over all mesh material shader types.
	for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		FMeshMaterialShaderType* ShaderType = ShaderTypeIt->GetMeshMaterialShaderType();
		if (ShaderType && 
			VertexFactoryType && 
			ShaderType->ShouldCache(Platform, Material, VertexFactoryType) && 
			Material->ShouldCache(Platform, ShaderType, VertexFactoryType) &&
			VertexFactoryType->ShouldCache(Platform, Material, ShaderType)
			)
		{
			NumShadersPerVF++;
			// only compile the shader if we don't already have it
			if (!HasShader(ShaderType))
			{
			    // Compile this mesh material shader for this material and vertex factory type.
				ShaderType->BeginCompileShader(
					ShaderMapId,
					Platform,
					Material,
					MaterialShaderCode,
					VertexFactoryType				
					);
			}
		}
	}

	if (NumShadersPerVF > 0)
	{
		warnf(NAME_DevShadersDetailed, TEXT("			%s - %u shaders"), VertexFactoryType->GetName(), NumShadersPerVF);
	}

	return NumShadersPerVF;
}

/**
 * Creates shaders for all of the compile jobs and caches them in this shader map.
 * @param Material - The material to compile shaders for.
 * @param CompilationResults - The compile results that were enqueued by BeginCompile.
 */
void FMeshMaterialShaderMap::FinishCompile(UINT ShaderMapId, const FUniformExpressionSet& UniformExpressionSet, const TArray<TRefCountPtr<FShaderCompileJob> >& CompilationResults)
{
	// Find the matching FMeshMaterialShaderType for each compile job
	for (INT JobIndex = 0; JobIndex < CompilationResults.Num(); JobIndex++)
	{
		const FShaderCompileJob& CurrentJob = *CompilationResults(JobIndex);
		if (CurrentJob.Id == ShaderMapId && CurrentJob.VFType == VertexFactoryType)
		{
			for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
			{
				FMeshMaterialShaderType* MeshMaterialShaderType = ShaderTypeIt->GetMeshMaterialShaderType();
				if (*ShaderTypeIt == CurrentJob.ShaderType && MeshMaterialShaderType != NULL)
				{
					FShader* Shader = MeshMaterialShaderType->FinishCompileShader(UniformExpressionSet, CurrentJob);
					check(Shader);
					AddShader(MeshMaterialShaderType,Shader);
				}
			}
		}
	}
}

UBOOL FMeshMaterialShaderMap::IsComplete(
	const FMeshMaterialShaderMap* MeshShaderMap,
	EShaderPlatform Platform,
	const FMaterial* Material,
	FVertexFactoryType* InVertexFactoryType,
	UBOOL bSilent
	)
{
	UBOOL bIsComplete = TRUE;

	// Iterate over all mesh material shader types.
	for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		FMeshMaterialShaderType* ShaderType = ShaderTypeIt->GetMeshMaterialShaderType();
		if (ShaderType && 
			ShaderType->ShouldCache(Platform, Material, InVertexFactoryType) && 
			Material->ShouldCache(Platform, ShaderType, InVertexFactoryType) &&
			InVertexFactoryType->ShouldCache(Platform, Material, ShaderType) &&
			(!MeshShaderMap || !MeshShaderMap->HasShader(ShaderType))
			)
		{
			if (!bSilent)
			{
				warnf(NAME_DevShaders, TEXT("Incomplete material %s, missing %s from %s."), *Material->GetFriendlyName(), ShaderType->GetName(), InVertexFactoryType->GetName());
			}
			bIsComplete = FALSE;
			break;
		}
	}

	return bIsComplete;
}

/**
 * Removes all entries in the cache with exceptions based on a shader type
 * @param ShaderType - The shader type to flush
 */
void FMeshMaterialShaderMap::FlushShadersByShaderType(FShaderType* ShaderType)
{
	if (ShaderType->GetMeshMaterialShaderType())
	{
		RemoveShaderType(ShaderType->GetMeshMaterialShaderType());
	}
}

FArchive& operator<<(FArchive& Ar,FMeshMaterialShaderMap& S)
{
	S.Serialize(Ar);
	Ar << S.VertexFactoryType;
	if (Ar.IsLoading())
	{
		// Check the required version for the vertex factory type
		// If the package version is less, toss the shaders.
		FVertexFactoryType* VFType = S.GetVertexFactoryType();
		if (VFType && (Ar.Ver() < VFType->GetMinPackageVersion() || Ar.LicenseeVer() < VFType->GetMinLicenseePackageVersion()))
		{
			S.Empty();
		}
	}
	return Ar;
}
