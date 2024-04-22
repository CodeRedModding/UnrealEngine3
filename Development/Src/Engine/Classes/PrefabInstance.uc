/**
 * An Actor representing an instance of a Prefab in a level.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class PrefabInstance extends Actor
	native(Prefab);

/** The prefab that this is an instance of. */
var		const		Prefab					TemplatePrefab;

/**
 *	The version of the Prefab that this is an instance of.
 *	This allows us to detect if the prefab has changed, and the instance needs to be updated.
 */
var		const		int						TemplateVersion;

/** Mapping from archetypes in the source prefab (TemplatePrefab) to instances of those archetypes in this PrefabInstance. */
var		const native Map{UObject*,UObject*}	ArchetypeToInstanceMap;

/** Kismet sequence that was created for this PrefabInstance. */
var		const		PrefabSequence			SequenceInstance;


/** Contains the epic+licensee version that this PrefabInstance's package was saved with. */
var	const			int						PI_PackageVersion;
var	const			int						PI_LicenseePackageVersion;

var	const			array<byte>				PI_Bytes;
var	const			array<object>			PI_CompleteObjects;
var	const			array<object>			PI_ReferencedObjects;
var	const			array<string>			PI_SavedNames;
var	const native	Map{UObject*,INT}		PI_ObjectMap;

cpptext
{

	// UObject interface
	virtual void			Serialize(FArchive& Ar);
	virtual void			PreSave();
	virtual void			PostLoad();

#if REQUIRES_SAMECLASS_ARCHETYPE
	/**
	 * Provides PrefabInstance objects with a way to override incorrect behavior in ConditionalPostLoad()
	 * until different-class archetypes are supported.
	 *
	 * @fixme - temporary hack; correct fix would be to support archetypes of a different class
	 *
	 * @return	pointer to an object instancing graph to use for logic in ConditionalPostLoad().
	 */
	virtual struct FObjectInstancingGraph* GetCustomPostLoadInstanceGraph();
#endif

	/**
	 * Callback used to allow object register its direct object references that are not already covered by
	 * the token stream.
	 *
	 * @param ObjectArray	array to add referenced objects to via AddReferencedObject
	 */
	void AddReferencedObjects( TArray<UObject*>& ObjectArray );

	/**
	 * Create an instance of the supplied Prefab, including creating all objects and Kismet sequence.
	 */
	void InstancePrefab(UPrefab* InPrefab);

	/**
	 * Do any teardown to destroy anything instanced for this PrefabInstance.
	 * Sets TemplatePrefab back to NULL.
	 */
	void DestroyPrefab(class USelection* Selection);

	/**
	 * Update this instance of a prefab to match the template prefab.
	 * This will destroy/create objects as necessary.
	 * It also recreates the Kismet sequence.
	 */
	void UpdatePrefabInstance(class USelection* Selection);

	/**
	 * Convert this prefab instance to look like the Prefab archetype version of it (by changing object refs to archetype refs and
	 * converting positions to local space). Then serialise it, so we only get things that are unique to this instance. We store this
	 * archive in the PrefabInstance.
	 */
	void SavePrefabDifferences();

	/**
	 * Iterates through the ArchetypeToInstanceMap and verifies that the archetypes for each of this PrefabInstance's actors exist.
	 * For any actors contained by this PrefabInstance that do not have a corresponding archetype, removes the actor from the
	 * ArchetypeToInstanceMap.  This is normally caused by adding a new actor to a PrefabInstance, updating the source Prefab, then loading
	 * a new map without first saving the package containing the updated Prefab.  When the original map is reloaded, though it contains
	 * an entry for the newly added actor, the source Prefab's linker does not contain an entry for the corresponding archetype.
	 *
	 * @return	TRUE if each pair in the ArchetypeToInstanceMap had a valid archetype.  FALSE if one or more archetypes were NULL.
	 */
	UBOOL VerifyMemberArchetypes();

	/**
	 * Utility for getting all Actors that are part of this PrefabInstance.
	 */
	void GetActorsInPrefabInstance( TArray<AActor*>& OutActors ) const;

	/**
	 * Examines the selection status of each actor in this prefab instance, and
	 * returns TRUE if the selection state of all actors matches the input state.
	 */
	UBOOL GetActorSelectionStatus(UBOOL bInSelected) const;

	/** Instance the Kismet sequence if we have one into the 'Prefabs' subsequence. */
	void InstanceKismetSequence(USequence* SrcSequence, const FString& InSeqName);
	/** Destroy the Kismet sequence associated with this Prefab instance. */
	void DestroyKismetSequence();

	/** Copy information to a FPrefabUpdateArchive from this PrefabInstance for updating a PrefabInstance with. */
	void CopyToArchive(FPrefabUpdateArc* InArc);
	/** Copy information from a FPrefabUpdateArchive into this PrefabInstance for saving etc. */
	void CopyFromArchive(FPrefabUpdateArc* InArc);

	/** Applies a transform to the object if its an actor. */
	static void ApplyTransformIfActor(UObject* Obj, const FMatrix& Transform);

	/** Utility for taking a map and inverting it. */
	static void CreateInverseMap(TMap<UObject*, UObject*>& OutMap, TMap<UObject*, UObject*>& InMap);

	/**
	 * Utility	for copying UModel from one ABrush to another.
	 * Sees if DestActor is an ABrush. If so, assumes SrcActor is one. Then uses StaticDuplicateObject to copy UModel from
	 * SrcActor to DestActor.
	 */
	static void CopyModelIfBrush(UObject* DestObj, UObject* SrcObj);
}

defaultproperties
{
	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.PrefabSprite'
		HiddenGame=True
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		bIsScreenSizeScaled=True
		ScreenSize=0.0025
		SpriteCategoryName="Prefabs"
	End Object
	Components.Add(Sprite)

	// the defaults for the PrefabInstance file versions MUST be -1 (@see APrefabInstance::Copy*Archive)
	PI_PackageVersion=-1
	PI_LicenseePackageVersion=-1
}
