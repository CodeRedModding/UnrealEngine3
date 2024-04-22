/*=============================================================================
	Engine.h: Unreal engine public header file.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_ENGINE_UTILS
#define _INC_ENGINE_UTILS

/*-----------------------------------------------------------------------------
	Hit proxies.
-----------------------------------------------------------------------------*/

// Hit an actor.
struct HActor : public HHitProxy
{
	DECLARE_HIT_PROXY(HActor,HHitProxy)
	AActor* Actor;
	HActor( AActor* InActor ) : Actor( InActor ) {}
	HActor( AActor* InActor, EHitProxyPriority InPriority) :
		HHitProxy(InPriority),
		Actor(InActor) {}

	virtual void Serialize(FArchive& Ar)
	{
		Ar << Actor;
	}

	virtual EMouseCursor GetMouseCursor()
	{
		return MC_Cross;
	}
};

/**
 * Hit an actor with additional information.
 */
struct HActorComplex : public HHitProxy
{
	DECLARE_HIT_PROXY(HActorComplex,HHitProxy)
	/** Owning actor */
	AActor *Actor;
	/** Any additional information to be stored */
	FString Desc;
	INT Index;

	HActorComplex(AActor *InActor, const TCHAR *InDesc, INT InIndex, EHitProxyPriority InPriority = HPP_World, EHitProxyPriority InOrthoPriority = HPP_World)
	: HHitProxy(InPriority, InOrthoPriority)
	, Actor(InActor)
	, Desc(InDesc)
	, Index(InIndex)
	{
	}

	virtual void Serialize(FArchive& Ar)
	{
		Ar << Actor;
		Ar << Desc;
		Ar << Index;
	}
	virtual EMouseCursor GetMouseCursor()
	{
		return MC_Cross;
	}
};

//
//	HBSPBrushVert
//

struct HBSPBrushVert : public HHitProxy
{
	DECLARE_HIT_PROXY(HBSPBrushVert,HHitProxy);
	ABrush*	Brush;
	FVector* Vertex;
	HBSPBrushVert(ABrush* InBrush,FVector* InVertex):
		HHitProxy(HPP_UI),
		Brush(InBrush),
		Vertex(InVertex)
	{}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Brush;
	}
};

//
//	HStaticMeshVert
//

struct HStaticMeshVert : public HHitProxy
{
	DECLARE_HIT_PROXY(HStaticMeshVert,HHitProxy);
	AActor*	Actor;
	FVector Vertex;
	HStaticMeshVert(AActor* InActor,FVector InVertex):
		HHitProxy(HPP_UI),
		Actor(InActor),
		Vertex(InVertex)
	{}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Actor << Vertex;
	}
};

//
//	HSplineProxy
//

struct HSplineProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HSplineProxy,HHitProxy);

	class USplineComponent* SplineComp;

	HSplineProxy(class USplineComponent* InSplineComp):
		HHitProxy(HPP_World),
		SplineComp(InSplineComp)
	{}
	virtual EMouseCursor GetMouseCursor()
	{
		return MC_Cross;
	}	
	virtual void Serialize(FArchive& Ar);
};

// Hit an actor even with translucency
struct HTranslucentActor : public HActor
{
	DECLARE_HIT_PROXY(HTranslucentActor,HActor)
	AActor* Actor;
	HTranslucentActor( AActor* InActor ) : HActor( InActor ) {}
	HTranslucentActor( AActor* InActor, EHitProxyPriority InPriority) : HActor(InActor, InPriority)
	{
	}

	virtual EMouseCursor GetMouseCursor()
	{
		return MC_Cross;
	}

	virtual UBOOL AlwaysAllowsTranslucentPrimitives() const
	{
		return TRUE;
	}
};


/* ==========================================================================================================
	Data stores
========================================================================================================== */

/**
 * This class creates and holds a reference to the global data store client.
 */
class FGlobalDataStoreClientManager
{
public:
	/** Destructor */
	virtual ~FGlobalDataStoreClientManager() {}

protected:
	/**
	 * Creates and initializes an instance of a UDataStoreClient.
	 *
	 * @param	InOuter		the object to use for Outer when creating the global data store client
	 *
	 * @return	a pointer to a fully initialized instance of the global data store client class.
	 */
	class UDataStoreClient* CreateGlobalDataStoreClient( UObject* InOuter ) const;

	/**
	 * Initializes the singleton data store client that will manage the global data stores.
	 */
	virtual void InitializeGlobalDataStore()=0;
};

/*-----------------------------------------------------------------------------
	Terrain editing brush types.
-----------------------------------------------------------------------------*/

enum ETerrainBrush
{
	TB_None				= -1,
	TB_VertexEdit		= 0,	// Select/Drag Vertices on heightmap
	TB_Paint			= 1,	// Paint on selected layer
	TB_Smooth			= 2,	// Does a filter on the selected vertices
	TB_Noise			= 3,	// Adds random noise into the selected vertices
	TB_Flatten			= 4,	// Flattens the selected vertices to the height of the vertex which was initially clicked
	TB_TexturePan		= 5,	// Pans the texture on selected layer
	TB_TextureRotate	= 6,	// Rotates the texture on selected layer
	TB_TextureScale		= 7,	// Scales the texture on selected layer
	TB_Select			= 8,	// Selects areas of the terrain for copying, generating new terrain, etc
	TB_Visibility		= 9,	// Toggles terrain sectors on/off
	TB_Color			= 10,	// Paints color into the RGB channels of layers
	TB_EdgeTurn			= 11,	// Turns edges of terrain triangulation
};

/*-----------------------------------------------------------------------------
	Iterator for the editor that loops through all selected actors.
-----------------------------------------------------------------------------*/

/**
 * Abstract base class for actor iteration. Implements all operators and relies on IsSuitable
 * to be overridden by derived classed. Also the code currently relies on the ++ operator being
 * called from within the constructor which is bad as it uses IsSuitable which is a virtual function.
 * This means that all derived classes are treated as "final" and they need to manually call
 * ++(*this) in their constructor so it ends up calling the right one. The long term plan is to
 * replace the use of virtual functions with template tricks.
 * Note that when Playing In Editor, this will find actors only in GWorld.
 */
class FActorIteratorBase
{
protected:
	/** Current index into actors array							*/
	INT		ActorIndex;
	/** Current index into levels array							*/
	INT		LevelIndex;
	/** Whether we already reached the end						*/
	UBOOL	ReachedEnd;
	/** Number of actors that have been considered thus far		*/
	INT		ConsideredCount;
	/** Current actor pointed to by actor iterator				*/
	AActor*	CurrentActor;

	/**
	 * Default ctor, inits everything
	 */
	FActorIteratorBase(void) :
		ActorIndex( -1 ),
		LevelIndex( 0 ),
		ReachedEnd( FALSE ),
		ConsideredCount( 0 ),
		CurrentActor( NULL )
	{
		check(IsInGameThread());
	}

public:
	/**
	 * Returns the actor count.
	 *
	 * @param total actor count
	 */
	static INT GetProgressDenominator(void);
	/**
	 * Returns the actor count.
	 *
	 * @param total actor count
	 */
	static INT GetActorCount(void);
	/**
	 * Returns the dynamic actor count.
	 *
	 * @param total dynamic actor count
	 */
	static INT GetDynamicActorCount(void);
	/**
	 * Returns the net relevant actor count.
	 *
	 * @param total net relevant actor count
	 */
	static INT GetNetRelevantActorCount(void);

	/**
	 * Returns the current suitable actor pointed at by the Iterator
	 *
	 * @return	Current suitable actor
	 */
	FORCEINLINE AActor* operator*()
	{
		check(CurrentActor);
		checkf(!CurrentActor->HasAnyFlags(RF_Unreachable), TEXT("%s"), *CurrentActor->GetFullName());
		return CurrentActor;
	}
	/**
	 * Returns the current suitable actor pointed at by the Iterator
	 *
	 * @return	Current suitable actor
	 */
	FORCEINLINE AActor* operator->()
	{
		check(CurrentActor);
		checkf(!CurrentActor->HasAnyFlags(RF_Unreachable), TEXT("%s"), *CurrentActor->GetFullName());
		return CurrentActor;
	}
	/**
	 * Returns whether the iterator has reached the end and no longer points
	 * to a suitable actor.
	 *
	 * @return TRUE if iterator points to a suitable actor, FALSE if it has reached the end
	 */
	FORCEINLINE operator UBOOL()
	{
		return !ReachedEnd;
	}

	/**
	 * Returns whether the currently pointed to actor is a level's world info (actor index 0).
	 *
	 * return TRUE if the currently pointed to actor is a level's world info, FALSE otherwise
	 */
	UBOOL IsWorldInfo() const
	{
		check(!ReachedEnd);
		check(ActorIndex>=0);
		if( ActorIndex == 0 )
		{
			check(CurrentActor);
			check(CurrentActor->IsA(AWorldInfo::StaticClass()));
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}

	/**
	 * Returns whether the currently pointed to actor is a level's default brush (actor index 1).
	 *
	 * return TRUE if the currently pointed to actor is a level default brush, FALSE otherwise
	 */
	UBOOL IsDefaultBrush() const
	{
		check(!ReachedEnd);
		check(ActorIndex>=0);
		if( ActorIndex == 1 )
		{
			check(CurrentActor);
			check(CurrentActor->IsA(ABrush::StaticClass()));
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}

	/**
	 * Clears the current Actor in the array (setting it to NULL).
	 */
	void ClearCurrent()
	{
		check(!ReachedEnd);
		GWorld->RemoveActor(GWorld->Levels(LevelIndex)->Actors(ActorIndex), TRUE);
	}

	/**
	 * Returns the number of actors considered thus far. Can be used in combination
	 * with GetProgressDenominator to gauge progress iterating over all actors.
	 *
	 * @return number of actors considered thus far.
	 */
	INT GetProgressNumerator()
	{
		return ConsideredCount;
	}
};

/**
 * Default level iteration filter. Doesn't cull levels out.
 */
struct FDefaultLevelFilter
{
	/**
	 * Used to examine whether this level is valid for iteration or not
	 *
	 * @param Level the level to check for iteration
	 *
	 * @return TRUE if the level can be iterated, FALSE otherwise
	 */
	UBOOL CanIterateLevel(ULevel* Level) const
	{
		return TRUE;
	}
};

/**
 * Filter class that prevents ticking of levels that are still loading
 */
struct FTickableLevelFilter
{
	/**
	 * Used to examine whether this level is valid for iteration or not
	 *
	 * @param Level the level to check for iteration
	 *
	 * @return TRUE if the level can be iterated, FALSE otherwise
	 */
	UBOOL CanIterateLevel(ULevel* Level) const
	{
		return !Level->bHasVisibilityRequestPending || GIsAssociatingLevel;
	}
};

/**
 * Template class used to avoid the virtual function call cost for filtering
 * actors by certain characteristics
 */
template<typename FILTER_CLASS,typename LEVEL_FILTER_CLASS = FDefaultLevelFilter>
class TActorIteratorBase :
	public FActorIteratorBase
{
protected:
	/**
	 * Ref to the class that is going to filter actors
	 */
	const FILTER_CLASS& FilterClass;
	/**
	 * Ref to the object that is going to filter levels
	 */
	const LEVEL_FILTER_CLASS& LevelFilterClass;

	/**
	 * Hide the constructor as construction on this class should only be done by subclasses
	 */
	TActorIteratorBase(const FILTER_CLASS& InFilterClass = FILTER_CLASS(),
		const LEVEL_FILTER_CLASS& InLevelClass = LEVEL_FILTER_CLASS())
	:	FilterClass(InFilterClass),
		LevelFilterClass(InLevelClass)
	{
	}

public:
	/**
	 * Iterates to next suitable actor.
	 */
	void operator++()
	{
		// Use local version to avoid LHSs as compiler is not required to write out member variables to memory.
		AActor* LocalCurrentActor	= NULL;
		INT		LocalActorIndex		= ActorIndex;
		ULevel*	Level				= GWorld->Levels(LevelIndex);
		INT		LevelActorNum		= Level->Actors.Num();

		// Iterate over actors till a suitable is found or we reach the end.
		while( !ReachedEnd && !LocalCurrentActor )
		{
			// If this level can't be iterating or is at the end of its list
			if( LevelFilterClass.CanIterateLevel(Level) == FALSE ||
				++LocalActorIndex >= LevelActorNum )
			{
				if( ++LevelIndex >= GWorld->Levels.Num() )
				{
					LocalActorIndex = 0;
					LevelIndex = 0;
					ReachedEnd = TRUE;
					break;
				}
				else
				{
					// Get the next level for iterating
					Level = GWorld->Levels(LevelIndex);
					LevelActorNum = Level->Actors.Num();
					// Skip processing levels that can't be ticked
					if (LevelFilterClass.CanIterateLevel(Level) == FALSE)
					{
						continue;
					}

					// Now get the actor index to start iterating at
					LocalActorIndex	= FilterClass.GetFirstSuitableActorIndex( Level );
					
					// Gracefully handle levels with insufficient number of actors.
					if( LocalActorIndex >= LevelActorNum )
					{
						continue;
					}
				}
			}
			ConsideredCount++;

			if (LocalActorIndex+2 < LevelActorNum)
			{
				CONSOLE_PREFETCH( Level->Actors(LocalActorIndex+2) );
				CONSOLE_PREFETCH_NEXT_CACHE_LINE( Level->Actors(LocalActorIndex+2) );
			}
			// See whether current actor is suitable for actor iterator and reset it if not.
			LocalCurrentActor = Level->Actors(LocalActorIndex);
			if( !FilterClass.IsSuitable( LocalActorIndex, LocalCurrentActor ) )
			{
				LocalCurrentActor = NULL;
			}
		}

		// Propagate persistent data from local version.
		CurrentActor	= LocalCurrentActor;
		ActorIndex		= LocalActorIndex;
	}
};

/** iterator that only iterates over Actors that may be ticked (i.e, the Levels' TickableActors array) */
class FTickableActorIterator : public FActorIteratorBase
{
public:
	FTickableActorIterator()
	{
		++(*this);
	}

	UBOOL IsWorldInfo() const
	{
		// Brush is static so it can't possibly be encountered by this iterator
		return FALSE;
	}

	/**
	 * Returns whether the currently pointed to actor is a level's default brush (actor index 1).
	 *
	 * return TRUE if the currently pointed to actor is a level default brush, FALSE otherwise
	 */
	UBOOL IsDefaultBrush() const
	{
		// Brush is static so it can't possibly be encountered by this iterator
		return FALSE;
	}

	/**
	 * Clears the current Actor in the array (setting it to NULL).
	 */
	void ClearCurrent()
	{
		check(!ReachedEnd);
		GWorld->RemoveActor(GWorld->Levels(LevelIndex)->TickableActors(ActorIndex), TRUE);
	}

	/**
	 * Iterates to next suitable actor.
	 */
	void operator++()
	{
		// Use local version to avoid LHSs as compiler is not required to write out member variables to memory.
		AActor* LocalCurrentActor = NULL;
		INT LocalActorIndex = ActorIndex;
		ULevel* Level = GWorld->Levels(LevelIndex);
		INT LevelActorNum = Level->TickableActors.Num();

		FTickableLevelFilter LevelFilterClass;

		// Iterate over actors till a suitable is found or we reach the end.
		while (!ReachedEnd && LocalCurrentActor == NULL)
		{
			// If this level can't be iterating or is at the end of its list
			if (!LevelFilterClass.CanIterateLevel(Level) || ++LocalActorIndex >= LevelActorNum)
			{
				if (++LevelIndex >= GWorld->Levels.Num())
				{
					LocalActorIndex = 0;
					LevelIndex = 0;
					ReachedEnd = TRUE;
					break;
				}
				else
				{
					// Get the next level for iterating
					Level = GWorld->Levels(LevelIndex);
					LevelActorNum = Level->TickableActors.Num();
					// Skip processing levels that can't be ticked
					if (!LevelFilterClass.CanIterateLevel(Level))
					{
						continue;
					}

					// Now get the actor index to start iterating at
					LocalActorIndex	= 0;
					
					// Gracefully handle levels with insufficient number of actors.
					if (LocalActorIndex >= LevelActorNum)
					{
						continue;
					}
				}
			}
			ConsideredCount++;

			if (LocalActorIndex + 2 < LevelActorNum)
			{
				CONSOLE_PREFETCH( Level->TickableActors(LocalActorIndex + 2) );
				CONSOLE_PREFETCH_NEXT_CACHE_LINE( Level->TickableActors(LocalActorIndex + 2) );
			}
			// See whether current actor is suitable for actor iterator and reset it if not.
			LocalCurrentActor = Level->TickableActors(LocalActorIndex);
		}

		// Propagate persistent data from local version.
		CurrentActor	= LocalCurrentActor;
		ActorIndex		= LocalActorIndex;
	}
};

/**
 * Base filter class for actor filtering in iterators.
 */
class FActorFilter
{
public:
	/**
	 * Determines whether this is a valid actor or not
	 *
	 * @param	ActorIndex ignored
	 * @param	Actor	Actor to check
	 * @return	TRUE if actor is != NULL, FALSE otherwise
	 */
	FORCEINLINE UBOOL IsSuitable( INT /*ActorIndex*/, AActor* Actor ) const
	{
		return Actor != NULL;
	}

	/**
	 * Returns the index of the first suitable actor in the passed in level. Used as an 
	 * optimization for e.g. the dynamic and net relevant actor iterators.
	 *
	 * @param	Level	level to use
	 * @return	first suitable actor index in level
	 */
	FORCEINLINE INT GetFirstSuitableActorIndex( ULevel* Level ) const
	{
		// Skip the AWorldInfo actor (index == 0) for levels that aren't the persistent one.
		if( Level == GWorld->PersistentLevel )
		{
			return 0;
		}
		else
		{
			return 1;
		}
	}
};

/**
 * Actor iterator
 * Note that when Playing In Editor, this will find actors only in GWorld
 */
class FActorIterator :
	public TActorIteratorBase<FActorFilter, FTickableLevelFilter>
{
public:
	/**
	 * Constructor, inits the starting position for iteration
	 */
	FActorIterator()
	{
		++(*this);
	}
};

/**
 * Filters actors by whether or not they are selected
 */
class FSelectedActorFilter :
	public FActorFilter
{
public:
	/**
	 * Determines if the actor should be returned during iteration or not
	 *
	 * @param	ActorIndex ignored
	 * @param	Actor	Actor to check
	 * @return	TRUE if actor is selected, FALSE otherwise
	 */
	FORCEINLINE UBOOL IsSuitable( INT /*ActorIndex*/, AActor* Actor ) const
	{
		return Actor ? Actor->IsSelected() : FALSE;
	}
};

/**
 * Selected actor iterator
 */
class FSelectedActorIterator :
	public TActorIteratorBase<FSelectedActorFilter>
{
public:
	/**
	 * Constructor, inits the starting position for iteration
	 */
	FSelectedActorIterator()
	{
		++(*this);
	}
};

/**
 * Filters actors by whether or not they are dynamic
 */
class FDynamicActorFilter
{
public:
	/**
	 * Determines if the actor should be returned during iteration or not
	 *
	 * @param	ActorIndex ignored
	 * @param	Actor	Actor to check
	 * @return	TRUE if actor is selected, FALSE otherwise
	 */
	FORCEINLINE UBOOL IsSuitable( INT /*ActorIndex*/, AActor* Actor ) const
	{
		return (Actor != NULL && !Actor->IsStatic());
	}

	/**
	 * Returns the index of the first suitable actor in the passed in level. Used as an 
	 * optimization for e.g. the dynamic and net relevant actor iterators.
	 *
	 * @param	Level	level to use
	 * @return	first suitable actor index in level
	 */
	FORCEINLINE INT GetFirstSuitableActorIndex( ULevel* Level ) const
	{
		return Level->iFirstDynamicActor;
	}
};

/**
 * Dynamic actor iterator
 */
class FDynamicActorIterator :
	public TActorIteratorBase<FDynamicActorFilter, FTickableLevelFilter>
{
public:
	/**
	 * Positions the iterator at the first dynamic actor
	 */
	FDynamicActorIterator()
	{
		// Override initialization of ActorIndex.
		ULevel* Level	= GWorld->Levels(LevelIndex);
		ActorIndex		= FilterClass.GetFirstSuitableActorIndex( Level ) - 1; // -1 as ++ will increase Actor index first
		++(*this);
	}
};

/**
 * Filters actors by whether or not they are network relevant
 */
class FNetRelevantActorFilter :
	public FActorFilter
{
public:
	/**
	 * Returns the index of the first suitable actor in the passed in level. Used as an 
	 * optimization for e.g. the dynamic and net relevant actor iterators.
	 *
	 * @param	Level	level to use
	 * @return	first suitable actor index in level
	 */
	FORCEINLINE INT GetFirstSuitableActorIndex( ULevel* Level ) const
	{
		return Level->iFirstNetRelevantActor;
	}
};

/**
 * Net relevant actor iterator
 */
class FNetRelevantActorIterator :
	public TActorIteratorBase<FNetRelevantActorFilter, FTickableLevelFilter>
{
public:
	/**
	 * Positions the iterator at the first network relevant actor
	 */
	FNetRelevantActorIterator()
	{
		// Override initialization of ActorIndex.
		ULevel* Level	= GWorld->Levels(LevelIndex);
		ActorIndex		= FilterClass.GetFirstSuitableActorIndex( Level ) - 1; // -1 as ++ will increase Actor index first
		++(*this);
	}
};

/**
 * An output device that forwards output to both the log and the console.
 */
class FConsoleOutputDevice : public FStringOutputDevice
{
public:

	/**
	 * Minimal initialization constructor.
	 */
	FConsoleOutputDevice(class UConsole* InConsole):
		FStringOutputDevice(TEXT("")),
		Console(InConsole)
	{}

	/** @name FOutputDevice interface. */
	//@{
	virtual void Serialize(const TCHAR* Text,EName Event);
	//@}

private:

	/** The console which output is written to. */
	UConsole* Console;
};


/** TimeSince only works once there is a valid GWorld.  Otherwise it is just going to be crazy values more than likely. **/
FORCEINLINE FLOAT TimeSince( FLOAT Time )
{
	return GWorld ? GWorld->GetTimeSeconds() - Time : -1.0f;
}


/**
 *	Renders stats
 *
 *	@param Viewport	The viewport to render to
 *	@param Canvas	Canvas object to use for rendering
 *	@param CanvasObject		Optional canvas object for visualizing properties
 *	@param DebugProperties	List of properties to visualize (in/out)
 *	@param ViewLocation	Location of camera
 *	@param ViewRotation	Rotation of camera
 */
void DrawStatsHUD( FViewport* Viewport, FCanvas* Canvas, UCanvas* CanvasObject, TArray<FDebugDisplayProperty>& DebugProperties, const FVector& ViewLocation, const FRotator& ViewRotation );


/**
 *	Renders the FPS counter
 *
 *	@param Viewport	The viewport to render to
 *	@param Canvas	Canvas object to use for rendering
 *	@param X		Suggested X coordinate for where to start drawing
 *	@param Y		Suggested Y coordinate for where to start drawing
 *	@return			Y coordinate of the next line after this output
 */
INT DrawFPSCounter( FViewport* Viewport, FCanvas* Canvas, INT X, INT Y );

/**
 *	Renders AI stats
 *
 *	@param Viewport	The viewport to render to
 *	@param Canvas	Canvas object to use for rendering
 *	@param X		Suggested X coordinate for where to start drawing
 *	@param Y		Suggested Y coordinate for where to start drawing
 *	@return			Y coordinate of the next line after this output
 */
INT DrawAIStats( FViewport* Viewport, FCanvas* Canvas, INT X, INT Y );


/** This will set the StreamingLevels TMap with the current Streaming Level Status and also set which level the player is in **/
void GetLevelStremingStatus( TMap<FName,INT>& StreamingLevels, FString& LevelPlayerIsInName );

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE

// Helper class containing information about UObject assets referenced.
struct FContentComparisonAssetInfo
{
	/** Name of the asset */
	FString AssetName;
	/** The resource size of the asset */
	INT ResourceSize;

	/** Constructor */
	FContentComparisonAssetInfo()
	{
		appMemzero(this, sizeof(FContentComparisonAssetInfo));
	}

	/** operator == */
	UBOOL operator==(const FContentComparisonAssetInfo& Other)
	{
		return (
			(AssetName == Other.AssetName) &&
			(ResourceSize == Other.ResourceSize)
			);
	}

	/** operator = */
	FContentComparisonAssetInfo& operator=(const FContentComparisonAssetInfo& Other)
	{
		AssetName = Other.AssetName;
		ResourceSize = Other.ResourceSize;
		return *this;
	}
};

/** Helper class for performing the content comparison console command */
class FContentComparisonHelper
{
public:
	FContentComparisonHelper();
	virtual ~FContentComparisonHelper();

	/**
	 *	Compare the classes derived from the given base class.
	 *
	 *	@param	InBaseClassName			The base class to perform the comparison on.
	 *	@param	InRecursionDepth		How deep to recurse when walking the object reference chain. (Max = 4)
	 *
	 *	@return	UBOOL					TRUE if successful, FALSE if not
	 */
	virtual UBOOL CompareClasses(const FString& InBaseClassName, INT InRecursionDepth);

	/**
	 *	Compare the classes derived from the given base class, ignoring specified base classes.
	 *
	 *	@param	InBaseClassName			The base class to perform the comparison on.
	 *	@param	InBaseClassesToIgnore	The base classes to ignore when processing objects.
	 *	@param	InRecursionDepth		How deep to recurse when walking the object reference chain. (Max = 4)
	 *
	 *	@return	UBOOL					TRUE if successful, FALSE if not
	 */
	virtual UBOOL CompareClasses(const FString& InBaseClassName, const TArray<FString>& InBaseClassesToIgnore, INT InRecursionDepth);

	/**
	 *	Recursive function for collecting objects referenced by the given object.
	 *
	 *	@param	InStartObject			The object to collect the referencees for
	 *	@param	InCurrDepth				The current depth being processed
	 *	@param	InMaxDepth				The maximum depth to traverse the reference chain
	 *	@param	OutCollectReferences	The resulting referenced object list
	 */
	void RecursiveObjectCollection(UObject* InStartObject, INT InCurrDepth, INT InMaxDepth, TMap<UObject*,UBOOL>& OutCollectedReferences);

protected:
	TMap<FString,UBOOL> ReferenceClassesOfInterest;
};
#endif

#endif // _INC_ENGINE_UTILS

