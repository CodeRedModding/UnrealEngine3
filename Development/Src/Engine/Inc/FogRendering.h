/*=============================================================================
	FogRendering.h: 
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/** Parameters needed to render exponential height fog. */
class FExponentialHeightFogShaderParameters
{
public:

	/** Binds the parameters. */
	void Bind(const FShaderParameterMap& ParameterMap);

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FExponentialHeightFogShaderParameters& P);

	FShaderParameter ExponentialFogParameters;
	FShaderParameter ExponentialFogColorParameter;
	FShaderParameter LightInscatteringColorParameter;
	FShaderParameter LightVectorParameter;
};

/** Encapsulates parameters needed to calculate height fog in a vertex shader. */
class FHeightFogShaderParameters
{
public:

	/** Binds the parameters. */
	void Bind(const FShaderParameterMap& ParameterMap);

	/** 
	* Sets the parameter values, this must be called before rendering the primitive with the shader applied. 
	* @param VertexShader - the vertex shader to set the parameters on
	*/
	void SetVertexShader(
		const FVertexFactory* VertexFactory, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FMaterial& Material,
		const FSceneView* View,
		const UBOOL bAllowGlobalFog,
		FShader* VertexShader) const;

#if WITH_D3D11_TESSELLATION
	/** 
	* Sets the parameter values, this must be called before rendering the primitive with the shader applied. 
	* @param DomainShader - the vertex shader to set the parameters on
	*/
	void SetDomainShader(
		const FVertexFactory* VertexFactory, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FMaterial& Material,
		const FSceneView* View,
		FShader* DomainShader) const;
#endif

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FHeightFogShaderParameters& P);

private:

	template<typename ShaderRHIParamRef>
	void Set(
		const FVertexFactory* VertexFactory, 
		const FMaterialRenderProxy* MaterialRenderProxy, 
		const FMaterial& Material,
		const FSceneView* View,
		const UBOOL bAllowGlobalFog,
		ShaderRHIParamRef Shader) const;

	FShaderParameter	bUseExponentialHeightFogParameter;
	FExponentialHeightFogShaderParameters ExponentialParameters;
	FShaderParameter	FogDistanceScaleParameter;
	FShaderParameter	FogExtinctionDistanceParameter;
	FShaderParameter	FogMinHeightParameter;
	FShaderParameter	FogMaxHeightParameter;
	FShaderParameter	FogInScatteringParameter;
	FShaderParameter	FogStartDistanceParameter;
};

extern UBOOL ShouldRenderFog(const EShowFlags& ShowFlags);
