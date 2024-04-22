/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class LandscapeComponent extends PrimitiveComponent
	native(Terrain)
	hidecategories(Display, Attachment, Physics, Debug, Lighting, Collision, Movement, Rendering, PrimitiveComponent, Object);

var() const editconst int			SectionBaseX,
									SectionBaseY;

var const int						ComponentSizeQuads,		// Total number of quads for this component
									SubsectionSizeQuads,	// Number of quads for a subsection of the component. SubsectionSizeQuads+1 must be a power of two.
									NumSubsections;			// Number of subsections in X or Y axis

var() MaterialInterface             OverrideMaterial;
var MaterialInstanceConstant		MaterialInstance;

/** Stores information about which weightmap texture and channel each layer is stored */
struct native WeightmapLayerAllocationInfo
{
	var Name LayerName;
	var byte WeightmapTextureIndex;
	var byte WeightmapTextureChannel;

	structcpptext
	{
		FWeightmapLayerAllocationInfo(FName InLayerName)
		:	LayerName(InLayerName)
		,	WeightmapTextureIndex(255)	// Indicates an invalid allocation
		,	WeightmapTextureChannel(255)
		{}
	}
};

/** List of layers, and the weightmap and channel they are stored */
var private const array<WeightmapLayerAllocationInfo> WeightmapLayerAllocations;

/** Weightmap texture reference */
var private const array<Texture2D> WeightmapTextures;

/** UV offset to component's weightmap data from component local coordinates*/
var Vector4 WeightmapScaleBias;

/** U or V offset into the weightmap for the first subsection, in texture UV space */
var float WeightmapSubsectionOffset;

/** UV offset to Heightmap data from component local coordinates */
var Vector4 HeightmapScaleBias;

/** Heightmap texture reference */
var private const Texture2D HeightmapTexture;

/** Cached bounds, created at heightmap update time */
var const BoxSphereBounds CachedBoxSphereBounds;

/** Cached local-space bounding box, created at heightmap update time */
var const Box CachedLocalBox;

/** Unique ID for this component, used for caching during distributed lighting */
var private const editoronly Guid LightingGuid;

/** Array of shadow maps for this component. */
var private const array<ShadowMap2D> ShadowMaps;
/**	INTERNAL: Array of lights that don't apply to the terrain component.		*/
var const array<Guid>		IrrelevantLights;

/** Reference to the texture lightmap resource. */
var native private const LightMapRef LightMap;

/** Pointer to data shared with the render therad, used by the editor tools */
var private native pointer EditToolRenderData{struct FLandscapeEditToolRenderData};

/** Heightfield mipmap used to generate collision */
var int CollisionMipLevel;

/** Platform-specific data */
var private native pointer PlatformData{void};

/** Platform-specific data size */
var const native int PlatformDataSize;

var editoronly transient bool bNeedPostUndo;

/** Forced LOD level to use when rendering */
var(LOD) int ForcedLOD;
/** Neighbor LOD data to use when rendering, 255 is unspecified */
var byte NeighborLOD[8];
/** LOD level Bias to use when rendering */
var(LOD) int LODBias;
/** Neighbor LOD bias to use when rendering, 128 is 0 bias, 0 is -128 bias, 255 is 127 bias */
var byte NeighborLODBias[8];

cpptext
{
	// UObject interface
	virtual void AddReferencedObjects( TArray<UObject*>& ObjectArray );
	virtual void Serialize(FArchive& Ar);
	virtual void BeginDestroy();
#if WITH_EDITOR
	virtual void PostLoad();
	virtual void PostEditImport();
	virtual void PostEditUndo();
	virtual void PostRename();

	// Register ourselves with the actor.
	ELandscapeSetupErrors SetupActor(UBOOL bForce = FALSE);

	virtual void Attach();
#endif

	// UPrimitiveComponent interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	virtual void GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const;
	virtual void UpdateBounds();
	void SetParentToWorld(const FMatrix& ParentToWorld);
	virtual INT GetNumElements() const { return 1; }
	virtual UMaterialInterface* GetElementMaterial(INT MaterialIndex) const { return MaterialInstance; }
	virtual void SetElementMaterial(INT ElementIndex, UMaterialInterface* InMaterial);

#if WITH_EDITOR
	class ULandscapeInfo* GetLandscapeInfo(UBOOL bSpawnNewActor = TRUE) const;
	virtual void GetStaticLightingInfo(FStaticLightingPrimitiveInfo& OutPrimitiveInfo,const TArray<ULightComponent*>& InRelevantLights,const FLightingBuildOptions& Options);
	virtual UBOOL GetLightMapResolution( INT& Width, INT& Height ) const;
	virtual void GetStaticTriangles(FPrimitiveTriangleDefinitionInterface* PTDI) const;
	void DeleteLayer(FName LayerName, struct FLandscapeEditDataInterface* LandscapeEdit);
	void GeneratePlatformData( UE3::EPlatformType Platform, void*& NewPlatformData, INT& NewPlatformDataSize, UTexture2D*& NewWeightTexture ) const;
#endif
	virtual void InvalidateLightingCache();
	/**
	 *	Requests whether the component will use texture, vertex or no lightmaps.
	 *
	 *	@return	ELightMapInteractionType		The type of lightmap interaction the component will use.
	 */
	virtual ELightMapInteractionType GetStaticLightingType() const	{ return LMIT_Texture;	}
	virtual void GetStreamingTextureInfo(TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const;

	// Decal
	void GenerateDecalRenderData(FDecalState* Decal, TArray< FDecalRenderData* >& OutDecalRenderDatas) const;

	// ULandscapeComponent Interface

	/** Return's the landscape actor associated with this component. */
	class ALandscape* GetLandscapeActor() const;
	class ALandscapeProxy* GetLandscapeProxy() const;
	TMap< UTexture2D*,struct FLandscapeWeightmapUsage >& GetWeightmapUsageMap();

	virtual const FGuid& GetLightingGuid() const
	{
#if WITH_EDITORONLY_DATA
		return LightingGuid;
#else
		static const FGuid NullGuid( 0, 0, 0, 0 );
		return NullGuid; 
#endif // WITH_EDITORONLY_DATA
	}

	virtual void SetLightingGuid()
	{
#if WITH_EDITORONLY_DATA
		LightingGuid = appCreateGuid();
#endif // WITH_EDITORONLY_DATA
	}

#if WITH_EDITOR
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	/** Initialize the landscape component */
	void Init(INT InBaseX,INT InBaseY,INT InComponentSizeQuads, INT InNumSubsections,INT InSubsectionSizeQuads);

	/**
	 * Recalculate cached bounds using height values.
	 */
	void UpdateCachedBounds();

	/**
	 * Update the MaterialInstance parameters to match the layer and weightmaps for this component
	 * Creates the MaterialInstance if it doesn't exist.
	 */
	void UpdateMaterialInstances();

	/**
	 * Generate mipmaps for height and tangent data.
	 * @param HeightmapTextureMipData - array of pointers to the locked mip data. 
	 *           This should only include the mips that are generated directly from this component's data
	 *           ie where each subsection has at least 2 vertices.
	 */
	void GenerateHeightmapMips(TArray<FColor*>& HeightmapTextureMipData, INT ComponentX1=0, INT ComponentY1=0, INT ComponentX2=MAXINT, INT ComponentY2=MAXINT,struct FLandscapeTextureDataInfo* TextureDataInfo=NULL);

	/**
	 * Generates empty mipmaps for weightmap
	 */
	static void CreateEmptyTextureMips(UTexture2D* Texture, UBOOL bClear = FALSE);

	/**
	 * Generate mipmaps for weightmap
	 * Assumes all weightmaps are unique to this component.
	 * @param WeightmapTextureBaseMipData: array of pointers to each of the weightmaps' base locked mip data.
	 */
	template<typename DataType>
	static void GenerateMipsTempl(INT InNumSubsections, INT InSubsectionSizeQuads, UTexture2D* WeightmapTexture, DataType* BaseMipData);
	static void GenerateWeightmapMips(INT InNumSubsections, INT InSubsectionSizeQuads, UTexture2D* WeightmapTexture, FColor* BaseMipData);
	static void GenerateDataMips(INT InNumSubsections, INT InSubsectionSizeQuads, UTexture2D* WeightmapTexture, BYTE* BaseMipData);

	/**
	 * Update mipmaps for existing weightmap texture
	 */
	template<typename DataType>
	static void UpdateMipsTempl(INT InNumSubsections, INT InSubsectionSizeQuads, UTexture2D* WeightmapTexture, TArray<DataType*>& WeightmapTextureMipData, INT ComponentX1=0, INT ComponentY1=0, INT ComponentX2=MAXINT, INT ComponentY2=MAXINT, struct FLandscapeTextureDataInfo* TextureDataInfo=NULL);
	static void UpdateWeightmapMips(INT InNumSubsections, INT InSubsectionSizeQuads, UTexture2D* WeightmapTexture, TArray<FColor*>& WeightmapTextureMipData, INT ComponentX1=0, INT ComponentY1=0, INT ComponentX2=MAXINT, INT ComponentY2=MAXINT, struct FLandscapeTextureDataInfo* TextureDataInfo=NULL);
	static void UpdateDataMips(INT InNumSubsections, INT InSubsectionSizeQuads, UTexture2D* Texture, TArray<BYTE*>& TextureMipData, INT ComponentX1=0, INT ComponentY1=0, INT ComponentX2=MAXINT, INT ComponentY2=MAXINT, struct FLandscapeTextureDataInfo* TextureDataInfo=NULL);

	/**
	 * Creates or updates collision component height data
	 * @param HeightmapTextureMipData: heightmap data
	 * @param ComponentX1, ComponentY1, ComponentX2, ComponentY2: region to update
	 * @param Whether to update bounds from render component.
	 */
	void UpdateCollisionHeightData(FColor* HeightmapTextureMipData, INT ComponentX1=0, INT ComponentY1=0, INT ComponentX2=MAXINT, INT ComponentY2=MAXINT, UBOOL bUpdateBounds=FALSE, UBOOL bRebuild=FALSE);

	/**
	 * Updates collision component dominant layer data
	 * @param WeightmapTextureMipData: weightmap data
	 * @param ComponentX1, ComponentY1, ComponentX2, ComponentY2: region to update
	 * @param Whether to update bounds from render component.
	 */
	void UpdateCollisionLayerData(TArray<FColor*>& WeightmapTextureMipData, INT ComponentX1=0, INT ComponentY1=0, INT ComponentX2=MAXINT, INT ComponentY2=MAXINT);

	/**
	 * Updates collision component dominant layer data for the whole component, locking and unlocking the weightmap textures.
	 */
	void UpdateCollisionLayerData();

	/**
	 * Creates weightmaps for this component for the layers specified in the WeightmapLayerAllocations array
	 */
	void ReallocateWeightmaps(struct FLandscapeEditDataInterface* DataInterface=NULL);

	/**
	 * Generate a key for this component's layer allocations to use with MaterialInstanceConstantMap.
	 */
	UMaterialInterface* GetLandscapeMaterial() const;
	FString GetLayerAllocationKey() const;
	void GetLayerDebugColorKey(INT& R, INT& G, INT& B) const;

	void RemoveInvalidWeightmaps();

	virtual void ExportCustomProperties(FOutputDevice& Out, UINT Indent);
	virtual void ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn);

	void InitHeightmapData(TArray<FColor>& Heights, UBOOL bUpdateCollision);
	void InitWeightmapData(TArray<FName>& LayerNames, TArray<TArray<BYTE> >& Weights);

	FLOAT GetLayerWeightAtLocation( const FVector& InLocation, FName InLayerName, TArray<BYTE>* LayerCache=NULL );

	/** Return the LandscapeHeightfieldCollisionComponent matching this component */
	ULandscapeHeightfieldCollisionComponent* GetCollisionComponent() const;
#endif

	friend class FLandscapeComponentSceneProxy;
	friend struct FLandscapeComponentDataInterface;

	INT GetLODBias(FLOAT HeightThreshold); // Calculate LOD Bias based on heightmap complexity
	void SetLOD(UBOOL bForced, INT InLODValue);
}

defaultproperties
{
	LightingChannels=(Static=TRUE,bInitialized=TRUE)
	CollideActors=TRUE
	BlockActors=TRUE
	BlockZeroExtent=TRUE
	BlockNonZeroExtent=TRUE
	BlockRigidBody=TRUE
	CastShadow=TRUE
	bAcceptsLights=TRUE
	bAcceptsDecals=TRUE
	bAcceptsStaticDecals=TRUE
	bUsePrecomputedShadows=TRUE
	bForceDirectLightMap=TRUE
	bUseAsOccluder=TRUE
	bAllowCullDistanceVolume=FALSE
	CollisionMipLevel=0
	bNeedPostUndo=FALSE
	LODBias=0
	ForcedLOD=-1
	NeighborLOD[0]=255
	NeighborLOD[1]=255
	NeighborLOD[2]=255
	NeighborLOD[3]=255
	NeighborLOD[4]=255
	NeighborLOD[5]=255
	NeighborLOD[6]=255
	NeighborLOD[7]=255
	NeighborLODBias[0]=128
	NeighborLODBias[1]=128
	NeighborLODBias[2]=128
	NeighborLODBias[3]=128
	NeighborLODBias[4]=128
	NeighborLODBias[5]=128
	NeighborLODBias[6]=128
	NeighborLODBias[7]=128
}
