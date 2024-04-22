/*=============================================================================
	UnWorld.h: UWorld definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnPath.h"

class FObserverInterface;

template<typename,typename> class TOctree;

/** The type of crowd attractor octree. */
typedef TOctree<class ACrowdAttractor*, struct FCrowdAttractorOctreeSemantics> FCrowdAttractorOctreeType;

/**
 * UWorld is the global world abstraction containing several levels.
 */
class UWorld : public UObject, public FNetworkNotify
{
	DECLARE_CLASS_INTRINSIC(UWorld,UObject,0,Engine)
	NO_DEFAULT_CONSTRUCTOR(UWorld)

	/** The interface to the scene manager for this world. */
	FSceneInterface*							Scene;

	/** Array of levels currently in this world. Not serialized to disk to avoid hard references.								*/
	TArray<ULevel*>								Levels;
	/** Persistent level containing the world info, default brush and actors spawned during gameplay among other things			*/
	ULevel*										PersistentLevel;
	/** Persistent level FaceFXAnimSet, if present																				*/
	UFaceFXAnimSet*								PersistentFaceFXAnimSet;
	/** Pointer to the current level being edited. Level has to be in the Levels array and == PersistentLevel in the game.		*/
	ULevel*										CurrentLevel;
	/** Pointer to the current level in the queue to be made visible, NULL if none are pending.									*/
	ULevel*										CurrentLevelPendingVisibility;
	
	/** Pointer to the current level grid volume being edited, or NULL when not in use. */
	ALevelGridVolume*							CurrentLevelGridVolume;

	/** Saved editor viewport states - one for each view type. Indexed using ELevelViewportType above.							*/
	FLevelViewportInfo							EditorViews[4];

	/** Reference to last save game info used for serialization. The only time this is non NULL is during UEngine::SaveGame(..) */
	class UDEPRECATED_SaveGameSummary*			SaveGameSummary_DEPRECATED;

private:
	/** The unnamed game connection(s) for client/server communication */
	class UNetDriver*							NetDriver;

public:
	FURL										URL;

	/** Fake NetDriver for capturing network traffic to record demos															*/
	class UDemoRecDriver*						DemoRecDriver;
	/** Holds client connections to client peers. Used for peer-peer voice traffic and host migration */
	class UNetDriver*							PeerNetDriver;
#if WITH_STEAMWORKS_SOCKETS
	/** When the server is using steam sockets, this net driver redirects IP connections to the steam sockets URL */
	class UNetDriver*							RedirectNetDriver;
#endif

	/** Octree used for collision.																								*/
	FPrimitiveHashBase*							Hash;
	/** Used for backing up regular octree during lighting rebuilds as it is being replaced by subset.							*/
	FPrimitiveHashBase*							BackupHash;

	/** octree for navigation primitives (NavigationPoints, ReachSpecs, etc)													*/
	FNavigationOctree*							NavigationOctree;
	
	/** octree for crowd attractors.																							*/
	FCrowdAttractorOctreeType*					CrowdAttractorOctree;

	/** Navigation mesh global data.																							*/
	FNavMeshWorld*								NavMeshWorld;

	/** List of actors that were spawned during tick and need to be ticked														*/
	TArray<AActor*>								NewlySpawned;
	/** Whether we are in the middle of ticking actors/components or not														*/
	UBOOL										InTick;
	/** Toggle used to tell if an actor has been ticked by comparing their Ticked values										*/
	UBOOL										Ticked;
	/** The current ticking group																								*/
	ETickingGroup								TickGroup;

	/** 
	 * Indicates that during world ticking we are doing the final component update of dirty components 
     * (after PostAsyncWork and effect physics scene has run. 
	 */
	UBOOL										bPostTickComponentUpdate;

	INT											NetTag;
	/** Counter for allocating game- unique controller player numbers															*/
	INT											PlayerNum;

	ULineBatchComponent*						LineBatcher;
	ULineBatchComponent*						PersistentLineBatcher;

	UBOOL										bShowLineChecks;
	UBOOL										bShowExtentLineChecks;
	UBOOL										bShowPointChecks;

	/** Main physics scene, containing static world geometry. */
	FRBPhysScene*								RBPhysScene;

#if WITH_NOVODEX
	/** Renderer object used by Novodex to draw debug information in the world.													*/
	class FNxDebugRenderer*						DebugRenderer;
#endif // WITH_NOVODEX

	/** Pool of URB_BodyInstance objects to use at runtime. */
	TArray<URB_BodyInstance*>					BodyInstancePool;
	/** Pool of URB_ConstraintInstance objects to use at runtime. */
	TArray<URB_ConstraintInstance*>				ConstraintInstancePool;
	/** Pool of UAnimTree objects to use at runtime. */
	TArray<class UAnimTree*>					AnimTreePool;

	/** Time in seconds (game time so we respect time dilation) since the last time we purged references to pending kill objects */
	FLOAT										TimeSinceLastPendingKillPurge;

	/** Whether a full purge has been triggered, so that the next GarbageCollect will do a full purge no matter what.			*/
	UBOOL										FullPurgeTriggered;

	/** Whether we should delay GC for one frame to finish some pending operation												*/
	UBOOL										bShouldDelayGarbageCollect;

	/** Whether world object has been initialized via Init()																	*/
	UBOOL										bIsWorldInitialized;
	
	/** All static light component's attached to the world's scene. */
	TSparseArray<ULightComponent*>				StaticLightList;
	/** All dynamic light component's attached to the world's scene. */
	TSparseArray<ULightComponent*>				DynamicLightList;
	/** All light environment components that need to be notified when a static light changes. */
	TSparseArray<ULightEnvironmentComponent*>	LightEnvironmentList;
	/** The dominant directional light affecting the world, if any. */
	UDominantDirectionalLightComponent*			DominantDirectionalLight;
	/** All the dominant spot lights affecting the world. */
	TSparseArray<UDominantSpotLightComponent*>	DominantSpotLights;		
	/** All the dominant point lights affecting the world. */
	TSparseArray<UDominantPointLightComponent*>	DominantPointLights;	

	/** Override, forcing level load requests to be allowed. < 0 == not allowed, 0 == have code choose, > 1 == force allow.		 */
	INT											AllowLevelLoadOverride;

	/** If this is TRUE, then AddToWorld will NOT call Sequence->BeginPlay on the level's GameSequences							*/
	UBOOL										bDisallowRoutingSequenceBeginPlay;

	/** Number of frames to delay Streaming Volume updating, useful if you preload a bunch of levels but the camera hasn't caught up yet (INDEX_NONE for infinite) */
	INT											StreamingVolumeUpdateDelay;

	/** Array of any additional objects that need to be referenced by this world, to make sure they aren't GC'd */
	TArray<UObject*>							ExtraReferencedObjects;

	/** Is level streaming currently frozen?																					*/
	UBOOL										bIsLevelStreamingFrozen;

	/** Are crowds currently disabled.																							*/
	UBOOL										bDisableCrowds;

#if USE_MASSIVE_LOD
	/** True if any massive LOD has been used in the world																		*/
	UBOOL										bEditorHasMassiveLOD;
#endif

	/** Toggles allowing decal components to be attached to the world/levels */
	UBOOL bAllowDecalAttach;

	/** True we want to execute a call to UpdateCulledTriggerVolumes during Tick */
	UBOOL										bDoDelayedUpdateCullDistanceVolumes;

	/** Array of observers within this world */
	TArray<FObserverInterface*>					Observers;

	/**
	 * UWorld constructor called at game startup and when creating a new world in the Editor.
	 * Please note that this constructor does NOT get called when a world is loaded from disk.
	 *
	 * @param	InURL	URL associated with this world.
	 */
	UWorld( const FURL& InURL );
	
	/**
	 * Static constructor, called once during static initialization of global variables for native 
	 * classes. Used to e.g. register object references for native- only classes required for realtime
	 * garbage collection or to associate UProperties.
	 */
	void StaticConstructor();

	// Accessor functions.

	/**
	 * Returns the current netmode
	 *
	 * @return current netmode
	 */
	ENetMode GetNetMode() const;

	/**
	 * Returns the current game info object.
	 *
	 * @return current game info object
	 */
	AGameInfo* GetGameInfo()	const;	

	/**
	 * Returns the first navigation point. Note that ANavigationPoint contains
	 * a pointer to the next navigation point so this basically returns a linked
	 * list of navigation points in the world.
	 *
	 * @return first navigation point
	 */
	ANavigationPoint* GetFirstNavigationPoint() const;

	/**
	 * Returns the first controller. Note that AController contains a pointer to
	 * the next controller so this basically returns a linked list of controllers
	 * associated with the world.
	 *
	 * @return first controller
	 */
	AController* GetFirstController() const;

	/**
	 * Returns the first pawn. Note that APawn contains a pointer to
	 * the next pawn so this basically returns a linked list of pawns
	 * associated with the world.
	 *
	 * @return first pawn
	 */
	APawn* GetFirstPawn() const;

	/**
	 * Returns the default brush.
	 *
	 * @return default brush
	 */
	ABrush* GetBrush() const;

	/**
	 * Returns whether game has already begun play.
	 *
	 * @return TRUE if game has already started, FALSE otherwise
	 */
	UBOOL HasBegunPlay() const;

	/**
	 * Returns whether gameplay has already begun and we are not associating a level
	 * with the world.
	 *
	 * @return TRUE if game has already started and we're not associating a level, FALSE otherwise
	 */
	UBOOL HasBegunPlayAndNotAssociatingLevel() const;

	/**
	 * Returns time in seconds since world was brought up for play, IS stopped when game pauses, IS dilated/clamped
	 *
	 * @return time in seconds since world was brought up for play
	 */
	FLOAT GetTimeSeconds() const;

	/**
	* Returns time in seconds since world was brought up for play, does NOT stop when game pauses, NOT dilated/clamped
	*
	* @return time in seconds since world was brought up for play
	*/
	FLOAT GetRealTimeSeconds() const;

	/**
	* Returns time in seconds since world was brought up for play, IS stopped when game pauses, NOT dilated/clamped
	*
	* @return time in seconds since world was brought up for play
	*/
	FLOAT GetAudioTimeSeconds() const;

	/**
	 * Returns the frame delta time in seconds adjusted by e.g. time dilation.
	 *
	 * @return frame delta time in seconds adjusted by e.g. time dilation
	 */
	FLOAT GetDeltaSeconds() const;

	/**
	 * Returns the default physics volume and creates it if necessary.
	 * 
	 * @return default physics volume
	 */
	APhysicsVolume* GetDefaultPhysicsVolume() const;

	/**
	 * Returns the physics volume a given actor is in.
	 *
	 * @param	Location
	 * @param	Actor
	 * @param	bUseTouch
	 *
	 * @return physics volume given actor is in.
	 */
	APhysicsVolume* GetPhysicsVolume(FVector Location, AActor* Actor, UBOOL bUseTouch) const;

	/**
	 * Returns the current (or specified) level's Kismet sequence
	 *
	 * @param	OwnerLevel		the level to get the sequence from - must correspond to one of the levels in GWorld's Levels array;
	 *							thus, only applicable when editing a multi-level map.  Defaults to the level currently being edited.
	 *
	 * @return	a pointer to the root sequence of the specified level, or NULL if that level doesn't exist or doesn't have any sequences
	 */
	USequence* GetGameSequence( ULevel* OwnerLevel=NULL ) const;

	/**
	 * Sets the current (or specified) level's kismet sequence to the sequence specified.
	 *
	 * @param	GameSequence	the sequence to add to the level
	 * @param	OwnerLevel		the level to add the sequence to - must correspond to one of the levels in GWorld's Levels array;
	 *							thus, only applicable when editing a multi-level map.  Defaults to the level currently being edited.
	 */
	void SetGameSequence( USequence* GameSequence, ULevel* OwnerLevel=NULL );

	/**
	 * Returns the AWorldInfo actor associated with this world.
	 *
	 * @return AWorldInfo actor associated with this world
	 */
	AWorldInfo* GetWorldInfo( UBOOL bCheckStreamingPesistent = FALSE ) const;

	/**
	 * Returns the current levels BSP model.
	 *
	 * @return BSP UModel
	 */
	UModel* GetModel() const;

	/**
	 * Returns the Z component of the current world gravity.
	 *
	 * @return Z component of current world gravity.
	 */
	FLOAT GetGravityZ() const;

	/**
	 * Returns the Z component of the default world gravity.
	 *
	 * @return Z component of the default world gravity.
	 */
	FLOAT GetDefaultGravityZ() const;

	/**
	 * Returns the Z component of the current world gravity scaled for rigid body physics.
	 *
	 * @return Z component of the current world gravity scaled for rigid body physics.
	 */
	FLOAT GetRBGravityZ() const;

	/**
	 * Returns the name of the current map, taking into account using a dummy persistent world
	 * and loading levels into it via PrepareMapChange.
	 *
	 * @return	name of the current map
	 */
	const FString GetMapName() const;

	/**
	 * Adds a level's navigation list to the world's.
	 */
	void AddLevelNavList( class ULevel *Level, UBOOL bDebugNavList = FALSE );

	/**
	 * Removes a level's navigation list from the world's.
	 */
	void RemoveLevelNavList( class ULevel *Level, UBOOL bDebugNavList = FALSE );

	/**
	 * Resets the world navigation point list (by nuking the head).
	 */
	void ResetNavList();

	/**
	 * Searches for a navigation point by guid.
	 */
	AActor* FindActorByGuid(FGuid &Guid, UClass* InClass = NULL);

	/**
	 * Inserts the passed in controller at the front of the linked list of controllers.
	 *
	 * @param	Controller	Controller to insert, use NULL to clear list
	 */
	void AddController( AController* Controller );
	
	/**
	 * Removes the passed in controller from the linked list of controllers.
	 *
	 * @param	Controller	Controller to remove
	 */
	void RemoveController( AController* Controller );

	/**
	 * Inserts the passed in pawn at the front of the linked list of pawns.
	 *
	 * @param	Pawn	Pawn to insert, use NULL to clear list
	 */
	void AddPawn( APawn* Pawn );
	
	/**
	 * Removes the passed in pawn from the linked list of pawns.
	 *
	 * @param	Pawn	Pawn to remove
	 */
	void RemovePawn( APawn* Pawn );

	/**
	 * Returns whether the passed in actor is part of any of the loaded levels actors array.
	 *
	 * @param	Actor	Actor to check whether it is contained by any level
	 *	
	 * @return	TRUE if actor is contained by any of the loaded levels, FALSE otherwise
	 */
	UBOOL ContainsActor( AActor* Actor );

	/**
	 * Returns whether audio playback is allowed for this scene.
	 *
	 * @return TRUE if current world is GWorld, FALSE otherwise
	 */
	virtual UBOOL AllowAudioPlayback();

	/**
	 * Serialize function.
	 *
	 * @param Ar	Archive to use for serialization
	 */
	void Serialize( FArchive& Ar );

	/**
	 * Destroy function, cleaning up world components, delete octree, physics scene, ....
	 */
	void FinishDestroy();
	
	/**
	 * Clears all level components and world components like e.g. line batcher.
	 */
	void ClearComponents();

	/**
	 * Updates world components like e.g. line batcher and all level components.
	 *
	 * @param	bCurrentLevelOnly		If TRUE, affect only the current level.
	 */
	void UpdateComponents(UBOOL bCurrentLevelOnly);

	/**
	 * Updates all cull distance volumes.
	 */
	void UpdateCullDistanceVolumes();

	/**
	 * Cleans up components, streaming data and assorted other intermediate data.
	 * @param bSessionEnded whether to notify the viewport that the game session has ended
	 */
	void CleanupWorld(UBOOL bSessionEnded = TRUE);
	
	/**
	 * Invalidates the cached data used to render the levels' UModel.
	 *
	 * @param	bCurrentLevelOnly		If TRUE, affect only the current level.
	 */
	void InvalidateModelGeometry(UBOOL bCurrentLevelOnly);

	/**
	 * Discards the cached data used to render the levels' UModel.  Assumes that the
	 * faces and vertex positions haven't changed, only the applied materials.
	 *
	 * @param	bCurrentLevelOnly		If TRUE, affect only the current level.
	 */
	void InvalidateModelSurface(UBOOL bCurrentLevelOnly);

	/**
	 * Commits changes made to the surfaces of the UModels of all levels.
	 */
	void CommitModelSurfaces();

	/**
	 * Fixes up any cross level references. Called from e.g. UpdateLevelStreaming when associating a new level with the world.
	 *
	 * @param	bIsRemovingLevel	Whether we are adding or removing a level
	 * @param	Level				Level to add or remove
	 */
	void FixupCrossLevelRefs( UBOOL bIsRemovingLevel, ULevel* Level );

	/**
	 * Associates the passed in level with the world. The work to make the level visible is spread across several frames and this
	 * function has to be called till it returns TRUE for the level to be visible/ associated with the world and no longer be in
	 * a limbo state.
	 *
	 * @param StreamingLevel	Level streaming object whose level we should add
 	 * @param RelativeOffset	Relative offset to move actors
	 */
	void AddToWorld( ULevelStreaming* StreamingLevel );

	/** 
	 * Dissociates the passed in level from the world. The removal is blocking.
	 *
	 * @param LevelStreaming	Level streaming object whose level we should remove
	 */
	void RemoveFromWorld( ULevelStreaming* StreamingLevel );

	/**
	 * Updates the world based on the current view location of the player and sets level LODs accordingly.	 
	 *
	 * @param ViewFamily	Optional collection of views to take into account
	 */
	void UpdateLevelStreaming( FSceneViewFamily* ViewFamily = NULL );

	/**
	 * Flushs level streaming in blocking fashion and returns when all levels are loaded/ visible/ hidden
	 * so further calls to UpdateLevelStreaming won't do any work unless state changes. Basically blocks
	 * on all async operation like updating components.
	 *
	 * @param ViewFamily				Optional collection of views to take into account
	 * @param bOnlyFlushVisibility		Whether to only flush level visibility operations (optional)
	 * @param ExcludeType				Exclude packages of this type from flushing
	 */
	void FlushLevelStreaming( FSceneViewFamily* ViewFamily = NULL, UBOOL bOnlyFlushVisibility = FALSE, FName ExcludeType = NAME_None);

	/** @return whether there is at least one level with a pending visibility request */
	UBOOL IsVisibilityRequestPending();

	/**
	 * Returns whether the level streaming code is allowed to issue load requests.
	 *
	 * @return TRUE if level load requests are allowed, FALSE otherwise.
	 */
	UBOOL AllowLevelLoadRequests();
	
	/**
	 * Initializes the world, associates the persistent level and sets the proper zones.
	 */
	void Init();

	/**
	 * Static function that creates a new UWorld and replaces GWorld with it.
	 */
	static void CreateNew();

	/**
	 * Called from within SavePackage on the passed in base/ root. The return value of this function will be passed to
	 * PostSaveRoot. This is used to allow objects used as base to perform required actions before saving and cleanup
	 * afterwards.
	 * @param Filename: Name of the file being saved to (includes path)
	 * @param AdditionalPackagesToCook [out] Array of other packages the Root wants to make sure are cooked when this is cooked
	 *
	 * @return	Whether PostSaveRoot needs to perform internal cleanup
	 */
	virtual UBOOL PreSaveRoot(const TCHAR* Filename, TArray<FString>& AdditionalPackagesToCook);
	/**
	 * Called from within SavePackage on the passed in base/ root. This function is being called after the package
	 * has been saved and can perform cleanup.
	 *
	 * @param	bCleanupIsRequired	Whether PreSaveRoot dirtied state that needs to be cleaned up
	 */
	virtual void PostSaveRoot( UBOOL bCleanupIsRequired );

	/**
	 * Saves this world.  Safe to call on non-GWorld worlds.
	 *
	 * @param	Filename					The filename to use.
	 * @param	bForceGarbageCollection		Whether to force a garbage collection before saving.
	 * @param	bAutosaving					If TRUE, don't perform optional caching tasks.
	 * @param	bPIESaving					If TRUE, don't perform tasks that will conflict with editor state.
	 */
	UBOOL SaveWorld( const FString& Filename, UBOOL bForceGarbageCollection, UBOOL bAutosaving, UBOOL bPIESaving );

	/**
	 *  Interface to allow WorldInfo to request immediate garbage collection
	 */
	void PerformGarbageCollection();

	/**
	 *  Requests a one frame delay of Garbage Collection
	 */
	void DelayGarbageCollection();

	/**
	 * Called after the object has been serialized. Currently ensures that CurrentLevel gets initialized as
	 * it is required for saving secondary levels in the multi- level editing case.
	 */
	virtual void PostLoad();

	/**
	 * Update the level after a variable amount of time, DeltaSeconds, has passed.
	 * All child actors are ticked after their owners have been ticked.
	 */
	void Tick( ELevelTick TickType, FLOAT DeltaSeconds );

	void TickNetClient( FLOAT DeltaSeconds );
	void TickNetServer( FLOAT DeltaSeconds );	
	INT ServerTickClients( FLOAT DeltaSeconds );
	
	/**
	 * Tick host migration process.
	 *
	 * @param DeltaSeconds - The elapsed time between frames
	 */
	void TickHostMigration(FLOAT DeltaSeconds);

	/** Tick demo recording */
	INT TickDemoRecord( FLOAT DeltaSeconds );

	/** Tick demo playback */
	INT TickDemoPlayback( FLOAT DeltaSeconds );

	/**
	 * Ticks any of our async worker threads (notifies them of their work to do)
	 *
	 * @param DeltaSeconds - The elapsed time between frames
	 */
	void TickAsyncWork(FLOAT DeltaSeconds);

	/**
	 * Waits for any async work that needs to be done before continuing
	 */
	void WaitForAsyncWork(void);

	/**
	 * Issues level streaming load/unload requests based on whether
	 * local players are inside/outside level streaming volumes.
	 *
	 * @param OverrideViewLocation Optional position used to override the location used to calculate current streaming volumes
	 */
	void ProcessLevelStreamingVolumes(FVector* OverrideViewLocation=NULL);

	/**
	 * Transacts the specified level -- the correct way to modify a level
	 * as opposed to calling Level->Modify.
	 */
	void ModifyLevel(ULevel* Level);

	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	void ShrinkLevel();
	UBOOL Listen( FURL InURL, FString& Error );
	/**
	 * Builds the master package map
	 */
	void BuildServerMasterMap(void);
	UBOOL IsClient();
	UBOOL IsServer();
	UBOOL IsPaused();
	UBOOL MoveActor( AActor *Actor, const FVector& Delta, const FRotator& NewRotation, DWORD MoveFlags, FCheckResult &Hit );
	UBOOL FarMoveActor( AActor* Actor, const FVector& DestLocation, UBOOL Test=0, UBOOL bNoCheck=0, UBOOL bAttachedMove=0 );

	/** 
	 * Completely removes the level from the world.
	 *
	 * NOTE: This function doesn't remove the associated streaming level.
	 *
	 * @param	ToDestroy		A non-NULL, non-Persistent Level that will be destroyed.
	 * @return					TRUE if the level was removed.
	 */
	UBOOL EditorDestroyLevel( ULevel* ToDestroy );

	/**
	 * Wrapper for DestroyActor() that should be called in the editor.
	 *
	 * @param	bShouldModifyLevel		If TRUE, Modify() the level before removing the actor.
	 */
	UBOOL EditorDestroyActor( AActor* Actor, UBOOL bShouldModifyLevel );

	/**
	 * Removes the actor from its level's actor list and generally cleans up the engine's internal state.
	 * What this function does not do, but is handled via garbage collection instead, is remove references
	 * to this actor from all other actors, and kill the actor's resources.  This function is set up so that
	 * no problems occur even if the actor is being destroyed inside its recursion stack.
	 *
	 * @param	ThisActor				Actor to remove.
	 * @param	bNetForce				[opt] Ignored unless called during play.  Default is FALSE.
	 * @param	bShouldModifyLevel		[opt] If TRUE, Modify() the level before removing the actor.  Default is TRUE.
	 * @return							TRUE if destroy, FALSE if actor couldn't be destroyed.
	 */
	UBOOL DestroyActor( AActor* Actor, UBOOL bNetForce=FALSE, UBOOL bShouldModifyLevel=TRUE );

	/**
	 * Removes the passed in actor from the actor lists. Please note that the code actually doesn't physically remove the
	 * index but rather clears it so other indices are still valid and the actors array size doesn't change.
	 *
	 * @param	Actor					Actor to remove.
	 * @param	bShouldModifyLevel		If TRUE, Modify() the level before removing the actor if in the editor.
	 */
	void RemoveActor( AActor* Actor, UBOOL bShouldModifyLevel );

	AActor* SpawnActor( UClass* Class, FName InName=NAME_None, const FVector& Location=FVector(0,0,0), const FRotator& Rotation=FRotator(0,0,0), AActor* Template=NULL, UBOOL bNoCollisionFail=0, UBOOL bRemoteOwned=0, AActor* Owner=NULL, APawn* Instigator=NULL, UBOOL bNoFail=0, class ULevel* OverrideLevel = NULL );
	ABrush*	SpawnBrush();
	/** spawns a PlayerController and binds it to the passed in Player with the specified RemoteRole and options
	 * @param Player - the Player to set on the PlayerController
	 * @param RemoteRole - the RemoteRole to set on the PlayerController
	 * @param URL - URL containing player options (name, etc)
	 * @param UniqueId - unique net ID of the player (may be zeroed if no online subsystem or not logged in, e.g. a local game or LAN match)
	 * @param Error (out) - if set, indicates that there was an error - usually is set to a property from which the calling code can look up the actual message
	 * @param InNetPlayerIndex (optional) - the NetPlayerIndex to set on the PlayerController
	 * @return the PlayerController that was spawned (may fail and return NULL)
	 */
	APlayerController* SpawnPlayActor(UPlayer* Player, ENetRole RemoteRole, const FURL& URL, const struct FUniqueNetId& UniqueId, FString& Error, BYTE InNetPlayerIndex = 0);

	/**
	 * Destroys actors marked as bKillDuringLevelTransition. 
	 */
	void CleanUpBeforeLevelTransition();

	UBOOL FindSpot( const FVector& Extent, FVector& Location, UBOOL bUseComplexCollision, AActor* TestActor = NULL );
	UBOOL CheckSlice( FVector& Location, const FVector& Extent, INT& bKeepTrying, AActor* TestActor );
	UBOOL CheckEncroachment( AActor* Actor, FVector TestLocation, FRotator TestRotation, UBOOL bTouchNotify );
	UBOOL SinglePointCheck( FCheckResult& Hit, const FVector& Location, const FVector& Extent, DWORD TraceFlags );
    UBOOL EncroachingWorldGeometry( FCheckResult& Hit, const FVector& Location, const FVector& Extent, UBOOL bUseComplexCollision=FALSE, AActor* TestActor = NULL );
	UBOOL SingleLineCheck( FCheckResult& Hit, AActor* SourceActor, const FVector& End, const FVector& Start, DWORD TraceFlags, const FVector& Extent=FVector(0,0,0), ULightComponent* SourceLight = NULL );
	FCheckResult* MultiPointCheck( FMemStack& Mem, const FVector& Location, const FVector& Extent, DWORD TraceFlags );
	FCheckResult* MultiLineCheck( FMemStack& Mem, const FVector& End, const FVector& Start, const FVector& Size, DWORD TraceFlags, AActor* SourceActor, ULightComponent* SourceLight = NULL );
	UBOOL BSPLineCheck(	FCheckResult& Hit, AActor* Owner, const FVector& End, const FVector& Start, const FVector& Extent, DWORD TraceFlags );
	UBOOL BSPFastLineCheck( const FVector& End, const FVector& Start );
	UBOOL BSPPointCheck( FCheckResult &Result, AActor *Owner, const FVector& Location, const FVector& Extent );

	void InitWorldRBPhys();
	void TermWorldRBPhys();
	void TickWorldRBPhys(FLOAT DeltaSeconds);
	/**
	 * Waits for the physics scene to be done processing
	 */
	void WaitWorldRBPhys();

	/** 
	 *	Get a new BodyInstance from the pool, copying the values from the supplied BodyInstance. 
	 *	TemplateBody can by NULL, in which case the resulting BodyInstance will have the class default properties.
	 */
	URB_BodyInstance* InstanceRBBody(URB_BodyInstance const * TemplateBody);
	/** Return an RB_BodyInstance to the pool - must not keep a reference to it after this! */
	void ReturnRBBody(URB_BodyInstance* ReturnBody);

	/** 
	 *	Get a new ConstraintInstance from the pool, copying the values from the supplied ConstraintInstance. 
	 *	TemplateConstraint can by NULL, in which case the resulting ConstraintInstance will have the class default properties.
	 */
	URB_ConstraintInstance* InstanceRBConstraint(URB_ConstraintInstance const * TemplateConstraint);
	/** Return an RB_ConstraintInstance to the pool - must not keep a reference to it after this! */
	void ReturnRBConstraint(URB_ConstraintInstance* ReturnConstraint);

	// SetGameInfo - Spawns gameinfo for the level.
	void SetGameInfo(const FURL& InURL);

	/** BeginPlay - Begins gameplay in the level.
	 * @param InURL commandline URL
	 * @param bResetTime (optional) whether the WorldInfo's TimeSeconds should be reset to zero
	 */
	void BeginPlay(const FURL& InURL, UBOOL bResetTime = TRUE);

	/** verifies that the client has loaded or can load the package with the specified information
	 * if found, sets the Info's Parent to the package and notifies the server of our generation of the package
	 * if not, handles downloading the package, skipping it, or disconnecting, depending on the requirements of the package
	 * @param Info the info on the package the client should have
	 * @return TRUE if we're done verifying this package, FALSE if we're not done yet (because i.e. async loading is in progress)
	 */
	UBOOL VerifyPackageInfo(FPackageInfo& Info);

	/** looks for a PlayerController that was being swapped by the given NetConnection and, if found, destroys it
	 * (because the swap is complete or the connection was closed)
	 * @param Connection - the connection that performed the swap
	 * @return whether a PC waiting for a swap was found
	 */
	UBOOL DestroySwappedPC(UNetConnection* Connection);

// FNetworkNotify interface
	EAcceptConnection NotifyAcceptingConnection();
	void NotifyAcceptedConnection( class UNetConnection* Connection );
	UBOOL NotifyAcceptingChannel( class UChannel* Channel );
	UWorld* NotifyGetWorld() {return this;}
	virtual void NotifyControlMessage(UNetConnection* Connection, BYTE MessageType, class FInBunch& Bunch);
	void NotifyReceivedFile( UNetConnection* Connection, INT PackageIndex, const TCHAR* Error, UBOOL Skipped );
	UBOOL NotifySendingFile( UNetConnection* Connection, FGuid GUID );
	void NotifyProgress( EProgressMessageType MessageType, const FString& Title, const FString& Message );
	/**
	 * Determine if peer connections are currently being accepted
	 *
	 * @return EAcceptConnection type based on if ready to accept a new peer connection
	 */
	virtual EAcceptConnection NotifyAcceptingPeerConnection();
	/**
	 * Notify that a new peer connection was created from the listening socket
	 *
	 * @param Connection net connection that was just created
	 */
	virtual void NotifyAcceptedPeerConnection( class UNetConnection* Connection );
	/**
	 * Handler for control channel messages sent on a peer connection
	 *
	 * @param Connection net connection that received the message bunch
	 * @param MessageType type of the message bunch
	 * @param Bunch bunch containing the data for the message type read from the connection
	 */
	virtual void NotifyPeerControlMessage(UNetConnection* Connection, BYTE MessageType, class FInBunch& Bunch);

	/**
	 * Welcomes a player to the server, after passing PreLogin
	 *
	 * @param Connection	The connection to welcome
	 */
	void WelcomePlayer(UNetConnection* Connection);

	/**
	 * Welcomes a splitscreen player to the server, after passing PreLogin
	 *
	 * @param Connection	The child connection to welcome
	 */
	void WelcomeSplitPlayer(UChildConnection* Connection);
// End FNetworkNotify interface

	/**
	 * Used to get a net driver object by name. Default name is the game net driver
	 *
	 * @param NetDriverName the name of the net driver being asked for
	 *
	 * @return a pointer to the net driver or NULL if the named driver is not found
	 */
	inline UNetDriver* GetNetDriver(FName NetDriverName = NAME_None)
	{
		if (NetDriverName == NAME_None)
		{
			return NetDriver;
		}
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		if(GameEngine)
		{
			return GameEngine->FindNamedNetDriver(NetDriverName);
		}
		return NULL;
	}

	/**
	 * Sets the net driver to use for this world
	 *
	 * @param NewDriver the new net driver to use
	 */
	void SetNetDriver(UNetDriver* NewDriver,FName NetDriverName = NAME_None)
	{
		if (NetDriverName == NAME_None)
		{
			NetDriver = NewDriver;
		}
	}

	/** verifies that the navlist pointers have not been corrupted.
	 *	helps track down errors w/ dynamic pathnodes being added/removed from the navigation list
	 */
	static void VerifyNavList( const TCHAR* DebugTxt, ANavigationPoint* IgnoreNav = NULL );

	/**
	 * dump cover stats (memory stats)
	 */
	void DumpCoverStats();

	/**
	 * Sets the number of frames to delay Streaming Volume updating, 
	 * useful if you preload a bunch of levels but the camera hasn't caught up yet 
	 */
	void DelayStreamingVolumeUpdates(INT InFrameDelay)
	{
		StreamingVolumeUpdateDelay = InFrameDelay;
	}

	/**
	 *	Sets the persistent FaceFXAnimSet to the given one...
	 *
	 *	@param	InPersistentFaceFXAnimSet	The anim set to set as the persistent one
	 */
	void SetPersistentFaceFXAnimSet(UFaceFXAnimSet* InPersistentFaceFXAnimSet);

	/**
	 *	Finds the persistent FaceFXAnimSet and sets it if found...
	 */
	void FindAndSetPersistentFaceFXAnimSet();

	/**
	 *	Iterates over the existing Pawns and SkeletalMeshActors and mounts the current PersistentFaceFXAnimSet
	 */
	void MountPersistentFaceFXAnimSet();

	/**
	 *	Iterates over the existing Pawns and SkeletalMeshActors and unmounts the current PersistentFaceFXAnimSet
	 */
	void UnmountPersistentFaceFXAnimSet();

	/**
	 *	Mounts the current PersistentFaceFXAnimSet on the given Actor (pawn or SkeletalMeshActor)
	 *
	 *	@param	InActor		The actor to mount it on...
	 */
	void MountPersistentFaceFXAnimSetOnActor(AActor* InActor);
};

/** Global UWorld pointer */
extern UWorld* GWorld;

/** class that encapsulates seamless world traveling */
class FSeamlessTravelHandler
{
private:
	/** set when a transition is in progress */
	UBOOL bTransitionInProgress;
	/** URL we're traveling to */
	FURL PendingTravelURL;
	/** Guid of the destination map (for finding it in the package cache if autodownloaded) */
	FGuid PendingTravelGuid;
	/** whether or not we've transitioned to the entry level and are now moving on to the specified map */
	UBOOL bSwitchedToDefaultMap;
	/** set to the loaded package once loading is complete. Transition to it is performed in the next tick where it's safe to perform the required operations */
	UObject* LoadedPackage;
	/** set to the loaded world object inside that package. This is added to the root set (so that if a GC gets in between it won't break loading) */
	UWorld* LoadedWorld;
	/** while set, pause at midpoint (after loading transition level, before loading final destination) */
	UBOOL bPauseAtMidpoint;
	/** set when we started a new travel in the middle of a previous one and still need to clean up that previous attempt */
	UBOOL bNeedCancelCleanUp;

	/** callback sent to async loading code to inform us when the level package is complete */
	static void SeamlessTravelLoadCallback(UObject* LevelPackage, void* Handler);

	/** called to kick off async loading of the destination map and any other packages it requires */
	void StartLoadingDestination();

public:
	FSeamlessTravelHandler()
	: bTransitionInProgress(FALSE), PendingTravelGuid(0, 0, 0, 0), bSwitchedToDefaultMap(FALSE), LoadedPackage(NULL), LoadedWorld(NULL), bPauseAtMidpoint(FALSE), bNeedCancelCleanUp(FALSE)
	{}

	/** starts traveling to the given URL. The required packages will be loaded async and Tick() will perform the transition once we are ready
	 * @param InURL the URL to travel to
	 * @param InGuid the GUID of the destination map package
	 * @return whether or not we succeeded in starting the travel
	 */
	UBOOL StartTravel(const FURL& InURL, const FGuid& InGuid);

	/** @return whether a transition is already in progress */
	FORCEINLINE UBOOL IsInTransition()
	{
		return bTransitionInProgress;
	}
	/** @return if current transition has switched to the default map; returns FALSE if no transition is in progress */
	FORCEINLINE UBOOL HasSwitchedToDefaultMap()
	{
		return IsInTransition() && bSwitchedToDefaultMap;
	}

	inline FString GetDestinationMapName()
	{
		return (IsInTransition() ? FFilename(PendingTravelURL.Map).GetBaseFilename() : TEXT(""));
	}

	/** cancels transition in progress */
	void CancelTravel();

	/** turns on/off pausing after loading the transition map
	 * only valid during travel, before we've started loading the final destination
	 * @param bNowPaused - whether the transition should now be paused
	 */
	void SetPauseAtMidpoint(UBOOL bNowPaused);

	/** ticks the transition; handles performing the world switch once the required packages have been loaded */
	void Tick();
};
/** global travel handler */
extern FSeamlessTravelHandler GSeamlessTravelHandler;
