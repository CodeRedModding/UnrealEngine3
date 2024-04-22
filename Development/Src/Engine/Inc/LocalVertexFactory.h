/*=============================================================================
	LocalVertexFactory.h: Local vertex factory definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * A vertex factory which simply transforms explicit vertex attributes from local to world space.
 */
class FLocalVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FLocalVertexFactory);
public:

	struct DataType : public FVertexFactory::DataType
	{
		/** The stream to read the vertex position from. */
		FVertexStreamComponent PositionComponent;

		/** The streams to read the tangent basis from. */
		FVertexStreamComponent TangentBasisComponents[2];

		/** The streams to read the texture coordinates from. */
		TPreallocatedArray<FVertexStreamComponent,MAX_TEXCOORDS> TextureCoordinates;

		/** The stream to read the shadow map texture coordinates from. */
		FVertexStreamComponent ShadowMapCoordinateComponent;

		/** The stream to read the vertex color from. */
		FVertexStreamComponent ColorComponent;
	};

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	 * An implementation of the interface used by TSynchronizedResource to update the resource with new data from the game thread.
	 */
	void SetData(const DataType& InData)
	{
		Data = InData;
		UpdateRHI();
	}

	/**
	* Copy the data from another vertex factory
	* @param Other - factory to copy from
	*/
	void Copy(const FLocalVertexFactory& Other);

	/**
	* Vertex factory interface for creating a corresponding decal vertex factory
	* Copies the data from this existing vertex factory.
	*
	* @return new allocated decal vertex factory
	*/
	virtual class FDecalVertexFactoryBase* CreateDecalVertexFactory() const;

	// FRenderResource interface.
	virtual void InitRHI();

	static UBOOL SupportsTessellationShaders() { return TRUE; }

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

protected:
	DataType Data;
};
