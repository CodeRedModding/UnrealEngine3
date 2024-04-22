/*=============================================================================
	LocalDecalVertexFactory.h: Local vertex factory bound to a shader that computes decal texture coordinates.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __LOCALDECALVERTEXFACTORY_H__
#define __LOCALDECALVERTEXFACTORY_H__

/**
 * A vertex factory which simply transforms explicit vertex attributes from local to world space.
 */
class FLocalDecalVertexFactory : public FLocalVertexFactory, public FDecalVertexFactoryBase
{
	DECLARE_VERTEX_FACTORY_TYPE(FLocalDecalVertexFactory);
public:

	virtual FVertexFactory* CastToFVertexFactory()
	{
		return static_cast<FVertexFactory*>( this );
	}

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
	{
		return (Material->IsUsedWithDecals() || Material->IsDecalMaterial() || (AllowDebugViewmodes(Platform) && Material->IsSpecialEngineMaterial())) 
			&& FLocalVertexFactory::ShouldCache( Platform, Material, ShaderType );
	}

	/**
	* Modify compile environment to enable the decal codepath
	* @param OutEnvironment - shader compile environment to modify
	*/
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		// decals always need WORLD_COORD usage in order to pass 2x2 matrix for normal transform
		// using the color interpolators used by WORLD_COORDS
		OutEnvironment.Definitions.Set(TEXT("WORLD_COORDS"),TEXT("1"));
		OutEnvironment.Definitions.Set(TEXT("DECAL_FACTORY"),TEXT("1"));
	}

	/** Must match the value of the DECAL_FACTORY define */
	virtual UBOOL IsDecalFactory() const { return TRUE; }

	static UBOOL SupportsTessellationShaders() { return TRUE; }

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);
};

#endif // __LOCALDECALVERTEXFACTORY_H__
