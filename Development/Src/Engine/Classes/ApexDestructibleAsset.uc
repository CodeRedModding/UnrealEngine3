/*=============================================================================
	ApexDestructibleAsset.h: PhysX APEX integration. Destructible Asset
	Copyright 2008-2009 NVIDIA Corporation.
=============================================================================*/

/** This class fully describes an APEX destructible asset and associated propreties */
class ApexDestructibleAsset extends ApexAsset
	hidecategories(Object)
	dependson(PrimitiveComponent)
	native(Mesh);
/**
	Chunks up to the depth DefaultImpactDamageDepth will take impact damage,
	unless IDO_On or IDO_Off is chosen.
*/
enum EImpactDamageOverride
{
	IDO_None,
	IDO_On,
	IDO_Off
};

/** MApexAsset is a pointer to the Apex asset interface for this destructible asset */
var   native pointer                                          MApexAsset{class FIApexAsset};
/** Materials contains an array of Materials which can be remapped relative to this asset */
var() const editfixedsize	array<MaterialInterface>		  Materials;
/** Fracture effects for each fracture level */
var() const editfixedsize	array<FractureMaterial>			  FractureMaterials;
/** If checked, only a single effect from FractureMaterials is played within the bounding box of all fractured chunks.  The effect chosen will be the one corresponding to the destructible's SupportDepth. */
var() const bool											  bPlaySingleFractureMaterialEffect;
/** Default physical material to use for this asset. If the actor has a physical material defined in its mesh component, that will be used instead.*/
var() PhysicalMaterial		DefaultPhysMaterial;

/** Make the Destructible Thumbnail component singular */
var native pointer				MDestructibleThumbnailComponent{class UApexStaticDestructibleComponent};

var bool bHasUniqueAssetMaterialNames;

/**
	Flags that may be set for all chunks at a particular depth in the fracture hierarchy
*/
struct native NxDestructibleDepthParameters
{
	/**
		Chunks at this hierarchy depth level may take impact damage if this flag is set.
		Note, NxDestructibleParameters::forceToDamage must also be positive for this
		to take effect.
	*/
	var deprecated	bool					TAKE_IMPACT_DAMAGE;

	/**
		Chunks at this depth should have pose updates ignored.
	*/
	var deprecated	bool					IGNORE_POSE_UPDATES;

	/**
		Chunks at this depth should be ignored in raycast callbacks.
	*/
	var deprecated	bool					IGNORE_RAYCAST_CALLBACKS;

	/**
		Chunks at this depth should be ignored in contact callbacks.
	*/
	var deprecated	bool					IGNORE_CONTACT_CALLBACKS;

	/**
		User defined flags.
	*/
	var deprecated	bool					USER_FLAG_0;
	var deprecated	bool					USER_FLAG_1;
	var deprecated	bool					USER_FLAG_2;
	var deprecated	bool					USER_FLAG_3;

	/**
		Chunks up to the depth DefaultImpactDamageDepth will take impact damage,
		unless one of the override options (see EImpactDamageOverride) is chosen.
	*/
	var() EImpactDamageOverride			ImpactDamageOverride;
};

/**
	Flags that apply to a destructible actor
*/
struct native NxDestructibleParametersFlag
{
	/**
		If set, chunks will "remember" damage applied to them, so that many applications of a damage amount
		below damageThreshold will eventually fracture the chunk.  If not set, a single application of
		damage must exceed damageThreshold in order to fracture the chunk.
	*/
	var()	bool					ACCUMULATE_DAMAGE;

	/**
		If set, then chunks which are tagged as "support" chunks (via NxDestructibleChunkDesc::isSupportChunk)
		will have environmental support in static destructibles.

		Note: if both ASSET_DEFINED_SUPPORT and WORLD_SUPPORT are set, then chunks must be tagged as
		"support" chunks AND overlap the NxScene's static geometry in order to be environmentally supported.
	*/
	var()	bool					ASSET_DEFINED_SUPPORT;

	/**
		If set, then chunks which overlap the NxScene's static geometry will have environmental support in
		static destructibles.

		Note: if both ASSET_DEFINED_SUPPORT and WORLD_SUPPORT are set, then chunks must be tagged as
		"support" chunks AND overlap the NxScene's static geometry in order to be environmentally supported.
	*/
	var()	bool					WORLD_SUPPORT;

	/**
		Whether or not chunks at or deeper than the "debris" depth (see NxDestructibleParameters::debrisDepth)
		will time out.  The lifetime is a value between NxDestructibleParameters::debrisLifetimeMin and
		NxDestructibleParameters::debrisLifetimeMax, based upon the destructible module's LOD setting.
	*/
	var()	bool					DEBRIS_TIMEOUT;

	/**
		Whether or not chunks at or deeper than the "debris" depth (see NxDestructibleParameters::debrisDepth)
		will be removed if they separate too far from their origins.  The maxSeparation is a value between
		NxDestructibleParameters::debrisMaxSeparationMin and NxDestructibleParameters::debrisMaxSeparationMax,
		based upon the destructible module's LOD setting.
	*/
	var()	bool					DEBRIS_MAX_SEPARATION;

	/**
		If set, the smallest chunks may be further broken down, either by fluid crumbles (if a crumble particle
		system is specified in the NxDestructibleActorDesc), or by simply removing the chunk if no crumble
		particle system is specified.  Note: the "smallest chunks" are normally defined to be the deepest level
		of the fracture hierarchy.  However, they may be taken from higher levels of the hierarchy if
		NxModuleDestructible::setMaxChunkDepthOffset is called with a non-zero value.
	*/
	var()	bool					CRUMBLE_SMALLEST_CHUNKS;

	/**
		If set, the NxDestructibleActor::rayCast function will search within the nearest visible chunk hit
		for collisions with child chunks.  This is used to get a better raycast position and normal, in
		case the parent collision volume does not tightly fit the graphics mesh.  The returned chunk index
		will always be that of the visible parent that is intersected, however.
	*/
	var()	bool					ACCURATE_RAYCASTS;

	/**
		If set, the ValidBounds field of NxDestructibleParameters will be used.  These bounds are translated
		(but not scaled or rotated) to the origin of the destructible actor.  If a chunk or chunk island moves
		outside of those bounds, it is destroyed.
	*/
	var()	bool					USE_VALID_BOUNDS;

	/**
		If initially static, the destructible will become part of an extended support structure if it is
		in contact with another static destructible that also has this flag set.
	*/
	var()	bool					FORM_EXTENDED_STRUCTURES;
};

/**
	Parameters that pertain to chunk damage
*/
struct native NxDestructibleDamageParameters
{
	/**
		The damage amount which will cause a chunk to fracture (break free) from the destructible.
		This is obtained from the damage value passed into the NxDestructibleActor::applyDamage,
		or NxDestructibleActor::applyRadiusDamage, or via impact (see 'forceToDamage', below).
	*/
	var()	float				DamageThreshold;

	/**
		Controls the distance into the destructible to propagate damage.  The damage applied to the chunk
		is multiplied by DamageSpread, to get the propagation distance.  All chunks within the radius
		will have damage applied to them.  The damage applied to each chunk varies with distance to the damage
		application position.  Full damage is taken at zero distance, and zero damage at the damage radius.
	*/
	var()	float				DamageSpread;

	/**
		If a chunk is at a depth which has NX_DESTRUCTIBLE_TAKE_IMPACT_DAMAGE set (see DepthParameters),
		then when a chunk has a collision in the NxScene, it will take damage equal to ImpactDamage mulitplied by
		the impact force.
		The default value is zero, which effectively disables impact damage.
	*/
	var()	float				ImpactDamage;

	/**
		When a chunk takes impact damage due to physical contact (see DepthParameters), this parameter
		is the maximum impulse the contact can generate.  Weak materials such as glass may have this set to a low value, so that
		heavier objects will pass through them during fracture.
		N.B.: Setting this parameter to 0 disables the impulse cap; that is, zero is interpreted as infinite.
		Default value = 0.0f.
	*/
	var()	float				ImpactResistance;

	/**
		By default, impact damage will only be taken to this depth.  For a particular depth, this
		default may be overridden in the DepthParameters.  If negative, impact damage
		is disabled.
	*/
	var()	int					DefaultImpactDamageDepth;

	structdefaultproperties
	{
		DefaultImpactDamageDepth=-1
	}
};

/**
	Parameters that pertain to chunk debris-level settings
*/
struct native NxDestructibleDebrisParameters
{
	/**
		"Debris chunks" (see debrisDepth, above) will be destroyed after a time (in seconds)
		separated from non-debris chunks.  The actual lifetime is interpolated between these
		two values, based upon the module's LOD setting.  To disable lifetime, clear the
		NX_DESTRUCTIBLE_DEBRIS_TIMEOUT flag in the flags field.
		If debrisLifetimeMax < debrisLifetimeMin, the mean of the two is used for both.
		Default debrisLifetimeMin = 1.0, debrisLifetimeMax = 10.0f.
	*/
	var()	float				DebrisLifetimeMin;
	var()	float				DebrisLifetimeMax;

	/**
		"Debris chunks" (see debrisDepth, above) will be destroyed if they are separated from
		their origin by a distance greater than maxSeparation.  The actual maxSeparation is
		interpolated between these two values, based upon the module's LOD setting.  To disable
		maxSeparation, clear the NX_DESTRUCTIBLE_DEBRIS_MAX_SEPARATION flag in the flags field.
		If debrisMaxSeparationMax < debrisMaxSeparationMin, the mean of the two is used for both.
		Default debrisMaxSeparationMin = 1.0, debrisMaxSeparationMax = 10.0f.
	*/
	var()	float				DebrisMaxSeparationMin;
	var()	float				DebrisMaxSeparationMax;

	/**
		"Debris chunks" (see debrisDepth, above) will be destroyed if they are separated from
		their origin by a distance greater than maxSeparation multiplied by the original
		destructible asset size.  The actual maxSeparation is interpolated between these
		two values, based upon the module's LOD setting.  To disable maxSeparation, clear the
		NX_DESTRUCTIBLE_DEBRIS_MAX_SEPARATION flag in the flags field.
		If debrisMaxSeparationMax < debrisMaxSeparationMin, the mean of the two is used for both.
		Default debrisMaxSeparationMin = 1.0, debrisMaxSeparationMax = 10.0f.
	*/
	var()	Box					ValidBounds;

	structdefaultproperties
	{
		ValidBounds=(Min=(X=-500000.f,Y=-500000.f,Z=-500000.f),Max=(X=500000.f,Y=500000.f,Z=500000.f))
	}
};

/**
	Parameters that are less-often used
*/
struct native NxDestructibleAdvancedParameters
{
	/**
		Limits the amount of damage applied to a chunk.  This is useful for preventing the entire destructible
		from getting pulverized by a very large application of damage.  This can easily happen when impact damage is
		used, and the damage amount is proportional to the impact force (see forceToDamage).
	*/
	var()	float			DamageCap;

	/** 
		Large impact force may be reported if rigid bodies are spawned inside one another.  In this case the realative velocity of the two
		objects will be low.  This variable allows the user to set a minimum velocity threshold for impacts to ensure that the objects are 
		moving at a min velocity in order for the impact force to be considered.  
	*/
	var()	float			ImpactVelocityThreshold;

	/**
		If greater than 0, the chunks' speeds will not be allowed to exceed this value.  Use 0
		to disable this feature (this is the default).
	*/
	var()	float			MaxChunkSpeed;

	/**
		See MassScale.  Values less than 1 have the
		effect of reducing the ratio of different masses.  The closer MassScaleExponent is to zero, the
		more the ratio will be "flattened."  This helps PhysX converge when there is a very large number
		of interacting rigid bodies (such as a pile of destructible chunks).
		Valid range: [0,1].  Default = 0.5.
	*/
	var()	float			MassScaleExponent;

	/**
		Dynamic chunk islands will have their masses divided by MassScale, raised to the power MassScaleExponent,
		then multiplied by MassScale.  See MassScaleExponent.
		Valid range: (0,infinity).  Default = 1.0.
	*/
	var()	float			MassScale;

	/**
		Scale factor used to apply an impulse force along the normal of chunk when fractured.  This is used
		in order to "push" the pieces out as they fracture.
	*/
	var()	float			FractureImpulseScale;
};


/**
	Parameters that apply to a destructible actor
*/
struct native NxDestructibleParameters
{
	/**
		Parameters that pertain to chunk damage.  See NxDestructibleDamageParameters.
	*/
	var(Damage)		NxDestructibleDamageParameters		DamageParameters;

	/**
		Parameters that pertain to chunk debris-level settings.  See NxDestructibleDebrisParameters.
	*/
	var(Debris)		NxDestructibleDebrisParameters		DebrisParameters;

	/**
		Parameters that are less-often used.  See NxDestructibleAdvancedParameters.
	*/
	var(Advanced)	NxDestructibleAdvancedParameters	AdvancedParameters;

	/**
		The damage amount which will cause a chunk to fracture (break free) from the destructible.
		This is obtained from the damage value passed into the NxDestructibleActor::applyDamage,
		or NxDestructibleActor::applyRadiusDamage, or via impact (see 'forceToDamage', below).
	*/
	var deprecated	float			DamageThreshold;

	/**
		Controls the distance into the destructible to propagate damage.  The damage applied to the chunk
		is multiplied by DamageSpread, to get the propagation distance.  All chunks within the radius
		will have damage applied to them.  The damage applied to each chunk varies with distance to the damage
		application position.  Full damage is taken at zero distance, and zero damage at the damage radius.
	*/
	var deprecated	float			DamageToRadius<DisplayName=Damage Spread>;

	/**
		Limits the amount of damage applied to a chunk.  This is useful for preventing the entire destructible
		from getting pulverized by a very large application of damage.  This can easily happen when impact damage is
		used, and the damage amount is proportional to the impact force (see forceToDamage).
	*/
	var deprecated	float			DamageCap;

	/**
		If a chunk is at a depth which has NX_DESTRUCTIBLE_TAKE_IMPACT_DAMAGE set (see DepthParameters),
		then when a chunk has a collision in the NxScene, it will take damage equal to ImpactDamage mulitplied by
		the impact force.
		The default value is zero, which effectively disables impact damage.
	*/
	var deprecated	float			ForceToDamage<DisplayName=Impact Damage>;

	/** 
		Large impact force may be reported if rigid bodies are spawned inside one another.  In this case the realative velocity of the two
		objects will be low.  This variable allows the user to set a minimum velocity threshold for impacts to ensure that the objects are 
		moving at a min velocity in order for the impact force to be considered.  
	*/
	var deprecated	float			ImpactVelocityThreshold;

	/**
		When a chunk takes impact damage due to physical contact (see DepthParameters), this parameter
		is the maximum impulse the contact can generate.  Weak materials such as glass may have this set to a low value, so that
		heavier objects will pass through them during fracture.
		N.B.: Setting this parameter to 0 disables the impulse cap; that is, zero is interpreted as infinite.
		Default value = 0.0f.
	*/
	var deprecated	float			MaterialStrength<DisplayName=Impact Resistance>;

	/**
		Damage applied to chunks may deform (move) a chunk without fracturing it, if damageToPercentDeformation is
		positive.  The damage applied to the chunk is multiplied by damageToPercentDeformation, and the resulting
		"percent deformation" is used to translate and rotate the chunk.  The translation is the "percent deformation"
		times the size of the chunk, in the direction given by the 'direction' paramater in applyDamage
		(see NxDestructibleActor).  For radius damage, the direction is always radial from the impact position.
		The rotation appplied is the "percent deformation" times one radian.
		The default value is zero, which disables deformation.
	*/
	var deprecated	float			DamageToPercentDeformation;

	/**
		If a chunk's percent deformation (see damageToPercentDeformation) exceeds deformationPercentLimit in
		either translation or rotation, then the chunk will fracture.
	*/
	var deprecated	float			DeformationPercentLimit;

	/**
		If initially static, the destructible will become part of an extended support structure if it is
		in contact with another static destructible that also has this flag set.
	*/
	var deprecated	bool			bFormExtendedStructures;

	/**
		The chunk hierarchy depth at which to create a support graph.  Higher depth levels give more detailed support,
		but will give a higher computational load.  Chunks below the support depth will never be supported.
	*/
	var()	int						SupportDepth;
	
	/**
		The chunks will not be broken free below this depth.
	*/
	var()	int						MinimumFractureDepth;

	/**
		The chunk hierarchy depth at which chunks are considered to be "debris."  Chunks at this depth or
		below will be considered for various debris settings, such as debrisLifetime.
		Negative values indicate that no chunk depth is considered debris.
		Default value is -1.
	*/
	var()	int						DebrisDepth;

	/**
		The chunk hierarchy depth up to which chunks will always be processed.  These chunks are considered
		to be essential either for gameplay or visually.
		The minimum value is 0, meaning the level 0 chunk is always considered essential.
		Default value is 0.
	*/
	var()	int						EssentialDepth<DisplayName=Essential LOD Depth>;

	/**
		"Debris chunks" (see debrisDepth, above) will be destroyed after a time (in seconds)
		separated from non-debris chunks.  The actual lifetime is interpolated between these
		two values, based upon the module's LOD setting.  To disable lifetime, clear the
		NX_DESTRUCTIBLE_DEBRIS_TIMEOUT flag in the flags field.
		If debrisLifetimeMax < debrisLifetimeMin, the mean of the two is used for both.
		Default debrisLifetimeMin = 1.0, debrisLifetimeMax = 10.0f.
	*/
	var deprecated	float				DebrisLifetimeMin;
	var deprecated	float				DebrisLifetimeMax;

	/**
		"Debris chunks" (see debrisDepth, above) will be destroyed if they are separated from
		their origin by a distance greater than maxSeparation.  The actual maxSeparation is
		interpolated between these two values, based upon the module's LOD setting.  To disable
		maxSeparation, clear the NX_DESTRUCTIBLE_DEBRIS_MAX_SEPARATION flag in the flags field.
		If debrisMaxSeparationMax < debrisMaxSeparationMin, the mean of the two is used for both.
		Default debrisMaxSeparationMin = 1.0, debrisMaxSeparationMax = 10.0f.
	*/
	var deprecated	float				DebrisMaxSeparationMin;
	var deprecated	float				DebrisMaxSeparationMax;

	/**
		"Debris chunks" (see debrisDepth, above) will be destroyed if they are separated from
		their origin by a distance greater than maxSeparation multiplied by the original
		destructible asset size.  The actual maxSeparation is interpolated between these
		two values, based upon the module's LOD setting.  To disable maxSeparation, clear the
		NX_DESTRUCTIBLE_DEBRIS_MAX_SEPARATION flag in the flags field.
		If debrisMaxSeparationMax < debrisMaxSeparationMin, the mean of the two is used for both.
		Default debrisMaxSeparationMin = 1.0, debrisMaxSeparationMax = 10.0f.
	*/
	var deprecated	Box					ValidBounds;

	/**
		If greater than 0, the chunks' speeds will not be allowed to exceed this value.  Use 0
		to disable this feature (this is the default).
	*/
	var deprecated	float			MaxChunkSpeed;

	/**
		See MassScale.  Values less than 1 have the
		effect of reducing the ratio of different masses.  The closer MassScaleExponent is to zero, the
		more the ratio will be "flattened."  This helps PhysX converge when there is a very large number
		of interacting rigid bodies (such as a pile of destructible chunks).
		Valid range: [0,1].  Default = 0.5.
	*/
	var deprecated	float			MassScaleExponent;

	/**
		A collection of flags defined in NxDestructibleParametersFlag.
	*/
	var()	NxDestructibleParametersFlag Flags;

	/**
		The relative volume (chunk volume / whole destructible volume) below which GRBs are used
		instead of RBs to represent chunks in the physics scene.
	*/
	var deprecated	float			GrbVolumeLimit;

	/**
		Spacing of particle grid used to represent rigid bodies in GRB
	*/
	var deprecated	float			GrbParticleSpacing;

	/**
		Scale factor used to apply an impulse force along the normal of chunk when fractured.  This is used
		in order to "push" the pieces out as they fracture.
	*/
	var deprecated	float			FractureImpulseScale;

	/**
		Parameters that apply to every chunk at a given level.
		the element [0] of the array applies to the level 0 (unfractured) chunk, element [1] applies
		to the level 1 chunks, etc.
	*/
	var()   editfixedsize			array<NxDestructibleDepthParameters> DepthParameters;

	/**
	 *	Optional dominance group for dynamic chunks created when fractured. (ignored if > 31)
	 */
	var()		int		DynamicChunksDominanceGroup;	
	
	/** 
		Whether or not to usedynamicChunksGroupsMask.  If false, NULL will be passed into the DestructibleActor upon
		instantiation, through the NxDestructibleActorDesc.
	 */
	var()		bool	UseDynamicChunksGroupsMask;
	
	/** Enum indicating what type of object this should be considered for rigid body collision. */
	var()	const ERBCollisionChannel	DynamicChunksChannel;

	/** Types of objects that this physics objects will collide with. */
	var()	const RBCollisionChannelContainer	DynamicChunksCollideWithChannels;

   structdefaultproperties
   {
      DynamicChunksDominanceGroup=-1;
   }
};

/**
	The name of the NxMeshParticleSystem to use for crumbling.  This overrides the crumble system defined
	in the NxDestructibleAsset if specified.
*/
var()   string       				CrumbleEmitterName;

/**
	The name of the NxMeshParticleSystem to use for fracture-line dust.  This overrides the dust system defined
	in the NxDestructibleAsset if specified.
*/
var()	string						DustEmitterName;

/**
	Whether or not the destructible starts life as a dynamic actor
*/
var deprecated	bool                        bDynamic;

/**
	Parameters controlling the destruction properties.
*/
var()   NxDestructibleParameters 	DestructibleParameters;

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

		/** Releases a destructible actor.
		** @param ApexDestructibleActor : The destructibe actor to release.
		** @param Component : The associated component to release.
		**/
		void                         ReleaseDestructibleActor( class       physx::apex::NxDestructibleActor     &ApexDestructibleActor,     class UApexComponentBase &Component);

		/** create an APEX destructible preview object.
		**
		** @param : ApexDestructiblePreviewDesc : A descriptor for the preview object.
		** @param : Component : A reference to the component being creaated.
		**
		** @return : Returns the destructible preview object if successful or a NULL pointer if it failed.
		**/
		physx::apex::NxDestructiblePreview *CreateDestructiblePreview(class UApexComponentBase &Component);

		/** Releases the previously created APEX destructible preview object.
		**
		** @param :	ApexDestructiblePreview : A reference to the previously created destructible preview object.
		** @param : Component : A reference to the component object.
		**/
		void                         ReleaseDestructiblePreview(physx::apex::NxDestructiblePreview     &ApexDestructiblePreview,     class UApexComponentBase &Component);

		/*** Returns the number of materials used by this asset */
		virtual UINT                GetNumMaterials(void) const { return Materials.Num();  }

		/*** Returns a particular material by index **/
		virtual UMaterialInterface *GetMaterial(UINT Index) const { return Materials(Index); }

		/** Returns the default ::NxParameterized::Interface for this asset. **/
		virtual void * 				GetNxParameterized(void);

		/** Returns a *copy* of the :NxParameterized::Interface for this asset. Caller must manually 'destroy' it.*/
		virtual void * GetAssetNxParameterized(void);

		/** Returns the APEX asset interface pointer for this object */
		class FIApexAsset * GetApexGenericAsset() const { return MApexAsset; }

		/** A method to update the APEX named resource pointers with individual materials assigned to this asset */
		void UpdateMaterials(void);

		virtual void NotifyApexEditMode(class ApexEditInterface *iface);		

		/** Whether the APEX asset's materials can be overridden in the actor's ApexComponent */
		virtual UBOOL SupportsMaterialOverride() const { return bHasUniqueAssetMaterialNames; }

		/** Update old assets. Only has an effect when called from the editor. */
		void FixupAsset();

	private:
}

defaultproperties
{
}
