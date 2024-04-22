/*=============================================================================
	ApexClothingAsset.h: PhysX APEX integration. Clothing Asset
	Copyright 2008-2009 NVIDIA Corporation.
=============================================================================*/

class ApexClothingAsset extends ApexAsset
	hidecategories(Object)
	native(Mesh);

struct native ClothingLodInfo
{
	/** Mapping of clothing submesh to material array */
	var() const init editfixedsize array<INT>	LODMaterialMap;
};
/** Clothing Material Mapping for each graphical lod level */
var() const editfixedsize array<ClothingLodInfo>	LodMaterialInfo;
var   native pointer                                          MApexAsset{class FIApexAsset};
/** Clothing material override. Used only when UseClothingAssetMaterial is checked in the skeletal mesh component. */
var() const editfixedsize array<MaterialInterface>	Materials;

/** ApexClothingLibrary is only for legacy APEX 0.9 assets. */
var const deprecated ApexGenericAsset ApexClothingLibrary;

var() const bool bUseHardwareCloth;						// if true use hardware clothing for simulation
var() const bool bFallbackSkinning;						// if true, falls back to skinning clothing in software instead of using GPU skinning
var() const bool bSlowStart;							// Designates the 'slowStart' flag; see APEX clothing documentation
var() const bool bRecomputeNormals;						// Designates the 'recomputeNormals' flag; see APEX clothing documentation
var() const bool bAllowAdaptiveTargetFrequency;			// Slightly modifies gravity to avoid high frequency jittering due to variable time steps.
var() const int UVChannelForTangentUpdate;				// Which UV channel is used for updating tangent space.
var() const float MaxDistanceBlendTime<ClampMin=0.0>;	// The maximimum distance blend time (see APEX clothing documentation)
var() const float ContinuousRotationThreshold;			// The angle in degrees to consider the clothing simulation continuous.
var() const float ContinuousDistanceThreshold;			// The distance to consider the clothing simulation continuous.
var() const bool  bResetAfterTeleport;					// If true, it resets the simulation after a teleport.
var() const float LodWeightsMaxDistance;				// LodWeightMaxDistance (see APEX clothing documentation)
var() const float LodWeightsDistanceWeight;				// LodWeightDistanceWeight (see APEX clothing documentation)
var() const float LodWeightsBias;						// LodWeightBias (see APEX clothing documentation)
var() const float LodWeightsBenefitsBias;				// LodWeightMaxBenefitsBias (see APEX clothing documentation)

/**
**  If true, cloth is simulated in local space.
**	Inertia effects are added based on inertiaScale (from the imported clothing asset) if 3.x simulation is used.
**	This needs to be turned off for 2.8.x cloth (unless collisions are filtered correctly).
**/
var() const bool bUseLocalSpaceSimulation;				

var bool bHasUniqueAssetMaterialNames;
var() const float LODDecayTime<ClampMin=0.0>;		// How fast LodWeightDistanceWeight is reduced to 0 when the clothing is not visible.

/** Sound cue to play when the clothing starts moving */
var(Sound) SoundCue					SoundOnMove;
/** Sound cue to play when clothing is moving*/
var(Sound) SoundCue					SoundOnRest;
/** Sound cue to play when the clothing is settling down */
var(Sound) SoundCue					SoundWhileMoving;

/** Speed above which the clothing is considered moving */
var(Sound) float						SpeedThresholdOnMove;
/** Speed below which the clothing is considered at rest */
var(Sound) float						SpeedThresholdOnRest;
/** Whether to ignore the triggers when the cloth first settles into place */
var(Sound) bool						    IgnoreInitialTrigger;

cpptext
{
	public:
		/** Notification of the post load event. */
		virtual void                 PostLoad();

		/**** Serializes the asset
		* @param : Ar is a reference to the FArchive to either serialize from or to.
		*/
		virtual void                 Serialize(FArchive& Ar);

		/*** Returns the array of strings to display in the browser window */
		virtual TArray<FString>      GetGenericBrowserInfo();

		/*** This method is called when a generic asset is imported from an external file on disk.
		**
		** @param Buffer : A pointer to the raw data.
		** @param BufferSize : The length of the raw input data.
		** @param Name : The name of the asset which is being imported.
		**
		** @return : Returns true if the import was successful.
		**/
		UBOOL                        Import( const BYTE* Buffer, INT BufferSize, const FString& Name,UBOOL convertToUE3Coordinates );

		/*** Export asset to a file, in xml/bin format.
		**
		** @param Name: The name of file name for exported asset
		** @param isKeepUE3Coords: Export type, in original coords (true) or keep UE3 coords (false)
		** 
		**/
		virtual UBOOL Export(const FName& Name, UBOOL isKeepUE3Coords);


		/*** This method is called after a property has changed. */
		virtual void                 PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

		/** Fix up unique material names in the APEX asset after rename or duplication */
		virtual void PostRename();
		virtual void PostDuplicate();

		/** This method is called prior to the object being destroyed */
		virtual void                 BeginDestroy(void);

		/*** This method is called when the asset is renamed
		**
		** @param : InName : The new name of the object
		** @param : NewOuter : The new outer object (package) for this object.
		** @param : Flags : The ERenameFlags to honor.
		**
		** @return : Returns TRUE if the rename was successful
		**/
		virtual UBOOL Rename( const TCHAR* NewName=NULL, UObject* NewOuter=NULL, ERenameFlags Flags=REN_None );

	   	/** virtual method to return the number of materials used by this asset */
		virtual UINT                GetNumMaterials(void) const 
		{ 
#if WITH_EDITORONLY_DATA
			return Materials.Num();  
#else
			return 0;
#endif // WITH_EDITORONLY_DATA
		}
		/** Returns the default ::NxParameterized::Interface for this asset. */
		virtual UMaterialInterface* GetMaterial(UINT Index) const 
		{ 
#if WITH_EDITORONLY_DATA
			return Materials(Index); 
#else
			return NULL;
#endif // WITH_EDITORONLY_DATA
		}

		/** Returns the default ::NxParameterized::Interface for this object */
		virtual void * GetNxParameterized(void);

		/** Returns a *copy* of the :NxParameterized::Interface for this asset. Caller must manually 'destroy' it.*/
		virtual void * GetAssetNxParameterized(void);

		/** Interface to ApexGenericAsset */
		class FIApexAsset * GetApexGenericAsset() const { return MApexAsset; }

		/** Re-assigns the APEX material resources by name with the current array of UE3 materials */
		void UpdateMaterials(void);
		virtual void NotifyApexEditMode(class ApexEditInterface *iface);

		/** Whether the APEX asset's materials can be overridden */
		virtual UBOOL SupportsMaterialOverride() const { return bHasUniqueAssetMaterialNames; }
		
	private:
}

defaultproperties
{
  bUseHardwareCloth=true                 // if true use hardware clothing for simulation
  bFallbackSkinning=false                // if true, falls back to skinning clothing in software instead of using GPU skinning
  bSlowStart=true                        // Designates the 'slowStart' flag; see APEX clothing documentation
  bRecomputeNormals=false;
  bAllowAdaptiveTargetFrequency=false;    // Slightly modifies gravity to avoid high frequency jittering due to variable time steps.
  UVChannelForTangentUpdate=0            // Which UV channel is used for updating tangent space.
  MaxDistanceBlendTime=1                 // The maximimum distance blend time (see APEX clothing documentation)
  ContinuousRotationThreshold=84         // The angle in degrees to consider the clothing simulation continuous.
  ContinuousDistanceThreshold=50.0f      // The distance to consider the clothing simulation continuous.
  bResetAfterTeleport = true		 	 // If true, it resets the simulation after a teleport
  LodWeightsMaxDistance=10000            // LodWeightMaxDistance (see APEX clothing documentation)
  LodWeightsDistanceWeight=1             // LodWeightDistanceWeight (see APEX clothing documentation)
  LodWeightsBias=0                       // LodWeightBias (see APEX clothing documentation)
  LodWeightsBenefitsBias=0               // LodWeightMaxBenefitsBias (see APEX clothing documentation)
  LODDecayTime=10.0f					 // LODDecayTime
  bUseLocalSpaceSimulation=false		 // Enable Local Space Simulation
}
