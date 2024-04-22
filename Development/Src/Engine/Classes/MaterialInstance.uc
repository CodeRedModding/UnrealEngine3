/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialInstance extends MaterialInterface
	abstract
	native(Material);


/** Physical material to use for this graphics material. Used for sounds, effects etc.*/
var() PhysicalMaterial PhysMaterial;

var() const MaterialInterface Parent;

/** A 1 bit monochrome texture that represents a mask for what physical material should be used if the collided texel is black or white. */
var(PhysicalMaterialMask)	Texture2D	PhysMaterialMask;				
/** The UV channel to use for the PhysMaterialMask. */
var(PhysicalMaterialMask)	INT	PhysMaterialMaskUVChannel;
/** The physical material to use when a black pixel in the PhysMaterialMask texture is hit. */
var(PhysicalMaterialMask)	PhysicalMaterial BlackPhysicalMaterial;
/** The physical material to use when a white pixel in the PhysMaterialMask texture is hit. */
var(PhysicalMaterialMask)	PhysicalMaterial WhitePhysicalMaterial;

/** indicates whether the instance has static permutation resources (which are required when static parameters are present) */
var bool bHasStaticPermutationResource;

/** indicates whether the static permutation resource needs to be updated on PostEditChange() */
var native transient bool bStaticPermutationDirty;

/**
* The set of static parameters that this instance will be compiled with.
* This is indexed by EMaterialShaderPlatform.
* Only the first entry is ever used now that SM2 is no longer supported, 
* But the member is kept as an array to make adding future material platforms easier.  
* The second entry is to work around the script compile error from having an array with one element.
*/
var const native duplicatetransient pointer StaticParameters[2]{FStaticParameterSet};

/**
* The material resources for this instance.
* This is indexed by EMaterialShaderPlatform.
* Only the first entry is ever used now that SM2 is no longer supported, 
* But the member is kept as an array to make adding future material platforms easier.  
* The second entry is to work around the script compile error from having an array with one element.
*/
var const native duplicatetransient pointer StaticPermutationResources[2]{FMaterialResource};

/** Second resource is used when selected. */
var const native duplicatetransient pointer Resources[3]{class FMaterialInstanceResource};

var private const native bool ReentrantFlag;

/** Legacy texture references, now handled by FMaterial. */
var deprecated private const array<texture> ReferencedTextures;

var private const editoronly array<guid> ReferencedTextureGuids;

/** Unique ID for this material, used for caching during distributed lighting */
var private const Guid ParentLightingGuid;

/** This instance is dirty and needs to be flattened upon save or some other good time (like closing the MIC editor) */
var private const transient bool bNeedsMaterialFlattening;

cpptext
{
	// Constructor.
	UMaterialInstance();

	/**
	* Passes the allocation request up the MIC chain
	* @return	The allocated resource
	*/
	FMaterialResource* AllocateResource();

	/** Initializes the material instance's resources. */
	virtual void InitResources();

	/**
	 * Gets the static permutation resource if the instance has one
	 * @return - the appropriate FMaterialResource if one exists, otherwise NULL
	 */
	virtual FMaterialResource* GetMaterialResource(EMaterialShaderQuality OverrideQuality=MSQ_UNSPECIFIED);

	/**
	 * @return the flattened texture for the material instance, or the parent's if we don't have one
	 */
	virtual UTexture* GetMobileTexture(const INT MobileTextureUnit);

	// UMaterialInterface interface.

	/**
	 * NOTE: This is NOT SAFE to use on the rendering thread!
	 * 
	 * @return the quality level this material should render with
	 */
	virtual EMaterialShaderQuality GetQualityLevel() const;

	/**
	 * Get the material which this is an instance of.
	 */
	virtual UMaterial* GetMaterial();

	/** Returns the textures used to render this material for the given quality */
	virtual void GetUsedTextures(TArray<UTexture*> &OutTextures, const EMaterialShaderQuality Quality=MSQ_UNSPECIFIED, const UBOOL bAllQualities = FALSE, UBOOL bAllowOverride=TRUE);

	/**
	* Checks whether the specified texture is needed to render the material instance.
	* @param Texture	The texture to check.
	* @param bAllowOverride Whether you want to be given the original textures or allow override textures instead of the originals.
	* @return UBOOL - TRUE if the material uses the specified texture.
	*/
	virtual UBOOL UsesTexture(const UTexture* Texture, const UBOOL bAllowOverride=TRUE);

	/**
	 * Overrides a specific texture (transient)
	 *
	 * @param InTextureToOverride The texture to override
	 * @param OverrideTexture The new texture to use
	 */
	virtual void OverrideTexture( const UTexture* InTextureToOverride, UTexture* OverrideTexture );


	/**
	 * Checks if the material can be used with the given usage flag.
	 * If the flag isn't set in the editor, it will be set and the material will be recompiled with it.
	 * @param Usage - The usage flag to check
	 * @param bSkipPrim - Bypass the primitive type checks
	 * @return UBOOL - TRUE if the material can be used for rendering with the given type.
	 */
	virtual UBOOL CheckMaterialUsage(const EMaterialUsage Usage, const UBOOL bSkipPrim = FALSE);

	/**
	* Gets the value of the given static switch parameter.  If it is not found in this instance then
	*		the request is forwarded up the MIC chain.
	*
	* @param	ParameterName	The name of the static switch parameter
	* @param	OutValue		Will contain the value of the parameter if successful
	* @return					True if successful
	*/
	virtual UBOOL GetStaticSwitchParameterValue(FName ParameterName,UBOOL &OutValue,FGuid &OutExpressionGuid);

	/**
	* Gets the value of the given static component mask parameter. If it is not found in this instance then
	*		the request is forwarded up the MIC chain.
	*
	* @param	ParameterName	The name of the parameter
	* @param	R, G, B, A		Will contain the values of the parameter if successful
	* @return					True if successful
	*/
	virtual UBOOL GetStaticComponentMaskParameterValue(FName ParameterName, UBOOL &R, UBOOL &G, UBOOL &B, UBOOL &A,FGuid &OutExpressionGuid);

	/**
	* Gets the compression format of the given normal parameter
	*
	* @param	ParameterName	The name of the parameter
	* @param	CompressionSettings	Will contain the values of the parameter if successful
	* @return					True if successful
	*/
	virtual UBOOL GetNormalParameterValue(FName ParameterName, BYTE& OutCompressionSettings, FGuid &OutExpressionGuid);

	/**
	* Gets the weightmap index of the given terrain layer weight parameter
	*
	* @param	ParameterName	The name of the parameter
	* @param	OutWeightmapIndex	Will contain the values of the parameter if successful
	* @return					True if successful
	*/
	virtual UBOOL GetTerrainLayerWeightParameterValue(FName ParameterName, INT& OutWeightmapIndex, FGuid &OutExpressionGuid);

	virtual UBOOL IsDependent(UMaterialInterface* TestDependency);
	virtual FMaterialRenderProxy* GetRenderProxy(UBOOL Selected, UBOOL bHovered=FALSE) const;
	virtual UPhysicalMaterial* GetPhysicalMaterial() const;

	/**
	* Makes a copy of all the instance's inherited and overridden static parameters
	*
	* @param StaticParameters - The set of static parameters to fill, must be empty
	*/
	void GetStaticParameterValues(FStaticParameterSet* StaticParameters);

	/**
	* Sets the instance's static parameters and marks it dirty if appropriate.
	*
	* @param	EditorParameters	The new static parameters.  If the set does not contain any static parameters,
	*								the static permutation resource will be released.
	* @return		TRUE if the static permutation resource has been marked dirty
	*/
	UBOOL SetStaticParameterValues(const FStaticParameterSet* EditorParameters);

	/**
	* Checks if any of the static parameter values are outdated based on what they reference (eg a normalmap has changed format)
	*
	* @param	EditorParameters	The new static parameters. 
	*/
	virtual void CheckStaticParameterValues(FStaticParameterSet* EditorParameters);

	/**
	* Compiles the static permutation resource if the base material has changed and updates dirty states
	*/
	void UpdateStaticPermutation();

	/**
	* Updates static parameters and recompiles the static permutation resource if necessary
	*/
	void InitStaticPermutation();

	/**
	* Compiles material resources for the given platform if the shader map for that resource didn't already exist.
	*
	* @param ShaderPlatform - the platform to compile for.
	* @param bFlushExistingShaderMaps - forces a compile, removes existing shader maps from shader cache.
	*/
	void CacheResourceShaders(EShaderPlatform ShaderPlatform, UBOOL bFlushExistingShaderMaps=FALSE, UBOOL bDebugDump=FALSE);

	/**
	 * Passes the compile request up the MIC chain
	 *
	 * @param StaticParameters - The set of static parameters to compile for
	 * @param StaticPermutation - The resource to compile
	 * @param Platform - The platform to compile for
	 * @param Quality - The material quality to compile for
	 * @param bFlushExistingShaderMaps - Indicates that existing shader maps should be discarded
	 * @return TRUE if compilation was successful or not necessary
	 */
	UBOOL CompileStaticPermutation(
		FStaticParameterSet* Permutation,
		FMaterialResource* StaticPermutation,
		EShaderPlatform Platform,
		EMaterialShaderQuality Quality,
		UBOOL bFlushExistingShaderMaps,
		UBOOL bDebugDump);

	/**
	* Allocates the static permutation resources for all platforms if they haven't been already.
	* Also updates the material resource's Material member as it may have changed.
	*/
	void AllocateStaticPermutations();

	/**
	* Releases the static permutation resource if it exists, in a thread safe way
	*/
	void ReleaseStaticPermutations();

	/** === USurface interface === */
	/**
	 * Method for retrieving the width of this surface.
	 *
	 * This implementation returns the maximum width of all textures applied to this material - not exactly accurate, but best approximation.
	 *
	 * @return	the width of this surface, in pixels.
	 */
	virtual FLOAT GetSurfaceWidth() const;
	/**
	 * Method for retrieving the height of this surface.
	 *
	 * This implementation returns the maximum height of all textures applied to this material - not exactly accurate, but best approximation.
	 *
	 * @return	the height of this surface, in pixels.
	 */
	virtual FLOAT GetSurfaceHeight() const;

	// UObject interface.
	virtual void AddReferencedObjects(TArray<UObject*>& ObjectArray);
	void PreSave();
	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void BeginDestroy();
	virtual UBOOL IsReadyForFinishDestroy();
	virtual void FinishDestroy();

	/**
	* Refreshes parameter names using the stored reference to the expression object for the parameter.
	*/
	virtual void UpdateParameterNames();

#if !FINAL_RELEASE
	/** Displays warning if this material instance is not safe to modify in the game (is in content package) */
	void CheckSafeToModifyInGame(const TCHAR* FuncName, const TCHAR* ParamName) const;
#endif

	/**
	 *	Check if the textures have changed since the last time the material was
	 *	serialized for Lightmass... Update the lists while in here.
	 *	NOTE: This will mark the package dirty if they have changed.
	 *
	 *	@return	UBOOL	TRUE if the textures have changed.
	 *					FALSE if they have not.
	 */
	virtual UBOOL UpdateLightmassTextureTracking();

	/** @return	The bCastShadowAsMasked value for this material. */
	virtual UBOOL GetCastShadowAsMasked() const;
	/** @return	The Emissive boost value for this material. */
	virtual FLOAT GetEmissiveBoost() const;
	/** @return	The Diffuse boost value for this material. */
	virtual FLOAT GetDiffuseBoost() const;
	/** @return	The Specular boost value for this material. */
	virtual FLOAT GetSpecularBoost() const;
	/** @return	The ExportResolutionScale value for this material. */
	virtual FLOAT GetExportResolutionScale() const;
	virtual FLOAT GetDistanceFieldPenumbraScale() const;

	/**
	 *	Get all of the textures in the expression chain for the given property (ie fill in the given array with all textures in the chain).
	 *
	 *	@param	InProperty				The material property chain to inspect, such as MP_DiffuseColor.
	 *	@param	OutTextures				The array to fill in all of the textures.
	 *	@param	InStaticParameterSet	Optional static parameter set - if supplied only walk the StaticSwitch branches according to it.
	 *	@param	OutTextureParamNames	Optional array to fill in with texture parameter names.
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if not.
	 */
	virtual UBOOL GetTexturesInPropertyChain(EMaterialProperty InProperty, TArray<UTexture*>& OutTextures, 
		TArray<FName>* OutTextureParamNames, class FStaticParameterSet* InStaticParameterSet);

	/**
	 * Returns the lookup texture to be used in the physical material mask.  Tries to get the parents lookup texture if not overridden here. 
	 */
	virtual UTexture2D* GetPhysicalMaterialMaskTexture() const;

	/**
	 * Returns the black physical material to be used in the physical material mask.  Tries to get the parents black phys mat if not overridden here. 
	 */
	virtual UPhysicalMaterial* GetBlackPhysicalMaterial() const;
	
	/**
	 * Returns the white physical material to be used in the physical material mask.  Tries to get the parents white phys mat if not overridden here. 
	 */
	virtual UPhysicalMaterial* GetWhitePhysicalMaterial() const;

	/** 
	 * Returns the UV channel that should be used to look up physical material mask information. Tries to get the parents UV channel if not present here. 
	 */
	virtual INT GetPhysMaterialMaskUVChannel() const;

	/**
	 *	Setup the mobile properties for this instance
	 */
	virtual void SetupMobileProperties();
};

// SetParent - Updates the parent.

native function SetParent(MaterialInterface NewParent);

// Set*ParameterValue - Updates the entry in ParameterValues for the named parameter, or adds a new entry.

native function SetVectorParameterValue(name ParameterName, const out LinearColor Value);
native function SetScalarParameterValue(name ParameterName, float Value);
native function SetScalarCurveParameterValue(name ParameterName, const out InterpCurveFloat Value);
native function SetTextureParameterValue(name ParameterName, Texture Value);

native function bool GetTextureParameterValue(name ParameterName, out Texture Value);

/**
* Sets the value of the given font parameter.
*
* @param	ParameterName	The name of the font parameter
* @param	OutFontValue	New font value to set for this MIC
* @param	OutFontPage		New font page value to set for this MIC
*/
native function SetFontParameterValue(name ParameterName, Font FontValue, int FontPage);

/** Removes all parameter values */
native function ClearParameterValues();

/**
 *	Returns if this MI is either in a map package or the transient package
 *	During gameplay, Set..Parameter should only be called on MIs where this is TRUE -
 *	otherwise you are modifying an MI within a content package, that will persist across level reload etc.
 */
native function bool IsInMapOrTransientPackage() const;

defaultproperties
{
	bHasStaticPermutationResource=False
	PhysMaterialMaskUVChannel=-1
}
