/*=============================================================================
	TerrainVertexFactory.h: Terrain vertex factory definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
struct FTerrainObject;

// Forward declaration of the terrain vertex buffer...
struct FTerrainVertexBuffer;

#include "DecalVertexFactory.h"

// This defines the number of border blocks to surround terrain by when generating lightmaps
#define TERRAIN_PATCH_EXPAND_SCALAR	1

class FTerrainDecalVertexFactoryBase;

/** Vertex factory with vertex stream components for terrain vertices */
class FTerrainVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FTerrainVertexFactory);

public:
	struct DataType
	{
		/** The stream to read the vertex position from.		*/
		FVertexStreamComponent PositionComponent;
		/** The stream to read the vertex gradients from.		*/
		FVertexStreamComponent GradientComponent;
	};

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
	{
#if MOBILE
		return FALSE;
#endif
		// only compile terrain materials for terrain vertex factory
		// The special engine materials must be compiled for the terrain vertex factory because they are used with it for wireframe, etc.
		return (Material->IsTerrainMaterial() || Material->IsSpecialEngineMaterial()) && !Material->IsUsedWithDecals();
	}

	/**
	 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	/**
	 * An implementation of the interface used by TSynchronizedResource to 
	 * update the resource with new data from the game thread.
	 * @param	InData - new stream component data
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
	void Copy(const FTerrainVertexFactory& Other);

	/** accessor */
	void SetTerrainObject(FTerrainObject* InTerrainObject)
	{
		TerrainObject = InTerrainObject;
	}

	FTerrainObject* GetTerrainObject() const
	{
		return TerrainObject;
	}

	INT GetTessellationLevel() const
	{
		return TessellationLevel;
	}

	void SetTessellationLevel(INT InTessellationLevel)
	{
		TessellationLevel = InTessellationLevel;
	}

	/**
	* Vertex factory interface for creating a corresponding decal vertex factory
	* Copies the data from this existing vertex factory.
	*
	* @return new allocated decal vertex factory
	*/
	virtual class FDecalVertexFactoryBase* CreateDecalVertexFactory() const;

	// FRenderResource interface.
	virtual void InitRHI();

	/**
	 *	Initialize the component streams.
	 *	
	 *	@param	Buffer	Pointer to the vertex buffer that will hold the data streams.
	 *	@param	Stride	The stride of the provided vertex buffer.
	 */
	virtual UBOOL InitComponentStreams(FTerrainVertexBuffer* Buffer);

	virtual FTerrainDecalVertexFactoryBase* CastToFTerrainDecalVertexFactoryBase()
	{
		return NULL;
	}

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

private:
	/** stream component data bound to this vertex factory */
	DataType Data;  

	FTerrainObject* TerrainObject;
	INT TessellationLevel;
};

class FTerrainDecalVertexFactoryBase : public FDecalVertexFactoryBase
{
public:
	virtual FTerrainDecalVertexFactoryBase* CastToFTerrainDecalVertexFactoryBase()=0;
	virtual FTerrainVertexFactory* CastToFTerrainVertexFactory()=0;	
};

/** Decal vertex factory with vertex stream components for terrain vertices */
class FTerrainDecalVertexFactory : public FTerrainDecalVertexFactoryBase, public FTerrainVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FTerrainDecalVertexFactory);

public:
	typedef FTerrainVertexFactory Super;
	/**
	 * Should we cache the material's shader type on this platform with this vertex factory? 
	 */
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		Super::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.Definitions.Set(TEXT("DECAL_FACTORY"),TEXT("1"));
		// decals always need WORLD_COORD usage in order to pass 2x2 matrix for normal transform
		// using the color interpolators used by WORLD_COORDS
		OutEnvironment.Definitions.Set(TEXT("WORLD_COORDS"),TEXT("1"));
	}

	/** Must match the value of the DECAL_FACTORY define */
	virtual UBOOL IsDecalFactory() const { return TRUE; }

	virtual FVertexFactory* CastToFVertexFactory()
	{
		return static_cast<FVertexFactory*>( this );
	}

	virtual FTerrainVertexFactory* CastToFTerrainVertexFactory()
	{
		return static_cast<FTerrainVertexFactory*>( this );
	}

	virtual FTerrainDecalVertexFactoryBase* CastToFTerrainDecalVertexFactoryBase()
	{
		return static_cast<FTerrainDecalVertexFactoryBase*>( this );
	}

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);
};

/** Vertex factory with vertex stream components for terrain morphing vertices */
class FTerrainMorphVertexFactory : public FTerrainVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FTerrainMorphVertexFactory);

public:
	struct DataType : public FTerrainVertexFactory::DataType
	{
		/** The stream to read the height and gradient transitions from.	*/
		FVertexStreamComponent HeightTransitionComponent;		// TessDataIndexLo, TessDataIndexHi, ZLo, ZHi
	};

	/**
	 * An implementation of the interface used by TSynchronizedResource to 
	 * update the resource with new data from the game thread.
	 * @param	InData - new stream component data
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
	void Copy(const FTerrainMorphVertexFactory& Other);

	/**
	 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("TERRAIN_MORPHING_ENABLED"),TEXT("1"));
	}

	// FRenderResource interface.
	virtual void InitRHI();

	/**
	 *	Initialize the component streams.
	 *	
	 *	@param	Buffer	Pointer to the vertex buffer that will hold the data streams.
	 *	@param	Stride	The stride of the provided vertex buffer.
	 */
	virtual UBOOL InitComponentStreams(FTerrainVertexBuffer* Buffer);

	/**
	* Vertex factory interface for creating a corresponding decal vertex factory
	* Copies the data from this existing vertex factory.
	*
	* @return new allocated decal vertex factory
	*/
	virtual class FDecalVertexFactoryBase* CreateDecalVertexFactory() const;

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

private:
	/** stream component data bound to this vertex factory */
	DataType Data;  
};

/** Decal vertex factory with vertex stream components for terrain morphing vertices */
class FTerrainMorphDecalVertexFactory : public FTerrainDecalVertexFactoryBase, public FTerrainMorphVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FTerrainMorphDecalVertexFactory);

public:
	typedef FTerrainMorphVertexFactory Super;

	/**
	 * Should we cache the material's shader type on this platform with this vertex factory? 
	 */
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		Super::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.Definitions.Set(TEXT("DECAL_FACTORY"),TEXT("1"));
		// decals always need WORLD_COORD usage in order to pass 2x2 matrix for normal transform
		// using the color interpolators used by WORLD_COORDS
		OutEnvironment.Definitions.Set(TEXT("WORLD_COORDS"),TEXT("1"));
	}

	/** Must match the value of the DECAL_FACTORY define */
	virtual UBOOL IsDecalFactory() const { return TRUE; }

	virtual FVertexFactory* CastToFVertexFactory()
	{
		return static_cast<FVertexFactory*>( this );
	}

	virtual FTerrainVertexFactory* CastToFTerrainVertexFactory()
	{
		return static_cast<FTerrainVertexFactory*>( this );
	}

	virtual FTerrainDecalVertexFactoryBase* CastToFTerrainDecalVertexFactoryBase()
	{
		return static_cast<FTerrainDecalVertexFactoryBase*>( this );
	}

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);
};

/** Vertex factory with vertex stream components for terrain morphing vertices */
class FTerrainFullMorphVertexFactory : public FTerrainMorphVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FTerrainFullMorphVertexFactory);

public:
	struct DataType : public FTerrainMorphVertexFactory::DataType
	{
		/** The stream to read the height and gradient transitions from.	*/
		FVertexStreamComponent GradientTransitionComponent;		// XGrad, YGrad
	};

	/**
	 * An implementation of the interface used by TSynchronizedResource to 
	 * update the resource with new data from the game thread.
	 * @param	InData - new stream component data
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
	void Copy(const FTerrainFullMorphVertexFactory& Other);

	/**
	 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("TERRAIN_MORPHING_ENABLED"),TEXT("1"));
		OutEnvironment.Definitions.Set(TEXT("TERRAIN_MORPHING_GRADIENTS"),TEXT("1"));
	}

	/**
	* Vertex factory interface for creating a corresponding decal vertex factory
	* Copies the data from this existing vertex factory.
	*
	* @return new allocated decal vertex factory
	*/
	virtual class FDecalVertexFactoryBase* CreateDecalVertexFactory() const;

	// FRenderResource interface.
	virtual void InitRHI();

	/**
	 *	Initialize the component streams.
	 *	
	 *	@param	Buffer	Pointer to the vertex buffer that will hold the data streams.
	 *	@param	Stride	The stride of the provided vertex buffer.
	 */
	virtual UBOOL InitComponentStreams(FTerrainVertexBuffer* Buffer);

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

private:
	/** stream component data bound to this vertex factory */
	DataType Data;  
};

/** Decal vertex factory with vertex stream components for terrain full morphing vertices */
class FTerrainFullMorphDecalVertexFactory : public FTerrainDecalVertexFactoryBase, public FTerrainFullMorphVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FTerrainFullMorphDecalVertexFactory);

public:
	typedef FTerrainFullMorphVertexFactory Super;

	/**
	 * Should we cache the material's shader type on this platform with this vertex factory? 
	 */
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		Super::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.Definitions.Set(TEXT("DECAL_FACTORY"),TEXT("1"));
		// decals always need WORLD_COORD usage in order to pass 2x2 matrix for normal transform
		// using the color interpolators used by WORLD_COORDS
		OutEnvironment.Definitions.Set(TEXT("WORLD_COORDS"),TEXT("1"));
	}

	/** Must match the value of the DECAL_FACTORY define */
	virtual UBOOL IsDecalFactory() const { return TRUE; }

	virtual FVertexFactory* CastToFVertexFactory()
	{
		return static_cast<FVertexFactory*>( this );
	}

	virtual FTerrainVertexFactory* CastToFTerrainVertexFactory()
	{
		return static_cast<FTerrainVertexFactory*>( this );
	}

	virtual FTerrainDecalVertexFactoryBase* CastToFTerrainDecalVertexFactoryBase()
	{
		return static_cast<FTerrainDecalVertexFactoryBase*>( this );
	}

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);
};
