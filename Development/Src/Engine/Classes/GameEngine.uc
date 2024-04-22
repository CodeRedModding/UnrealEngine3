//=============================================================================
// GameEngine: The game subsystem.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class GameEngine extends Engine
	native(GameEngine)
	config(Engine)
	transient;

// URL structure.
struct transient native URL
{
	var		string			Protocol;	// Protocol, i.e. "unreal" or "http".
	var		string			Host;		// Optional hostname, i.e. "204.157.115.40" or "unreal.epicgames.com", blank if local.
	var		int				Port;		// Optional host port.
	var		string			Map;		// Map name, i.e. "SkyCity", default is "Index".
	var		array<string>	Op;			// Options.
	var		string			Portal;		// Portal to enter through, default is "".
	var		int 			Valid;
structcpptext
{

	// Statics.
	static FString DefaultProtocol;
	static FString DefaultName;
	static FString DefaultMap;
	static FString DefaultLocalMap;
	static FString DefaultLocalOptions;
	static FString DefaultTransitionMap; // map used as in-between for seamless travel
	static FString DefaultHost;
	static FString DefaultPortal;
	static FString DefaultMapExt;
	static FString DefaultSaveExt;
	/** Additional map extension to look for when parsing urls for map loading */
	static FString AdditionalMapExt;
	static INT DefaultPort;
	/** port to listen for new client peer connections */
	static INT DefaultPeerPort;
	static UBOOL bDefaultsInitialized;

	// Constructors.
	FURL( const TCHAR* Filename=NULL );
	FURL( FURL* Base, const TCHAR* TextURL, ETravelType Type );
	static void StaticInit();
	static void StaticExit();

	/**
	 * Static: Removes any special URL characters from the specified string
	 *
	 * @param Str String to be filtered
	 */
	static void FilterURLString( FString& Str );


	// Functions.
	UBOOL IsInternal() const;
	UBOOL IsLocalInternal() const;
	UBOOL HasOption( const TCHAR* Test ) const;
	const TCHAR* GetOption( const TCHAR* Match, const TCHAR* Default ) const;
	void LoadURLConfig( const TCHAR* Section, const TCHAR* Filename=NULL );
	void SaveURLConfig( const TCHAR* Section, const TCHAR* Item, const TCHAR* Filename=NULL ) const;
	void AddOption( const TCHAR* Str );
	void RemoveOption( const TCHAR* Key, const TCHAR* Section = NULL, const TCHAR* Filename = NULL);
	FString String( UBOOL FullyQualified=0 ) const;
	friend FArchive& operator<<( FArchive& Ar, FURL& U );

	// Operators.
	UBOOL operator==( const FURL& Other ) const;
}
};

var			PendingLevel	GPendingLevel;

/** The name of the class to spawn as the temporary pending level player controller */
var config string PendingLevelPlayerControllerClassName;

/** URL the last time we travelled */
var			URL				LastURL;

/** last server we connected to (for "reconnect" command) */
var URL LastRemoteURL;
var config	array<string>	ServerActors;

var			string			TravelURL;
var			byte			TravelType;

/** set for one tick after completely loading and initializing a new world
 * (regardless of whether it's LoadMap() or seamless travel)
 */
var const transient bool bWorldWasLoadedThisTick;

/** check to see if we need to start a movie capture
 * (used on the first tick when we want to record a matinee)
 */
var const transient bool bCheckForMovieCapture;

/**
 * Triggers a call to PostLoadMap() the next Tick, turns off loading movie if LoadMap() has been called.
 */
var const transient bool bTriggerPostLoadMap;

/**
 * TRUE if the loading movie was started during LoadMap().
 */
var const transient bool bStartedLoadMapMovie;

/** The singleton online interface for all game code to use */
var const transient OnlineSubsystem OnlineSubsystem;

/** The singleton interface that enumerates available DLC */
var const transient DownloadableContentEnumerator DLCEnumerator;

/** The name of the class to use for the DLC enumeration */
var config string DownloadableContentEnumeratorClassName;

/** The singleton interface that manages the installation and removal of DLC */
var const transient DownloadableContentManager DLCManager;

/** The name of the class to use for the DLC manager */
var config string DownloadableContentManagerClassName;

/**
 * Array of package/ level names that need to be loaded for the pending map change. First level in that array is
 * going to be made a fake persistent one by using ULevelStreamingPersistent.
 */
var const	array<name>		LevelsToLoadForPendingMapChange;
/** Array of already loaded levels. The ordering is arbitrary and depends on what is already loaded and such.	*/
var	const	array<level>	LoadedLevelsForPendingMapChange;
/** Human readable error string for any failure during a map change request. Empty if there were no failures.	*/
var const	string			PendingMapChangeFailureDescription;
/** If TRUE, commit map change the next frame.																	*/
var const	bool			bShouldCommitPendingMapChange;
/** Maximium delta time the engine uses to populate GDeltaTime. If 0, unbound.									*/
var config	float			MaxDeltaTime;
/**
 *	If true - clear all AnimSet LinkupCaches during map load.
 *	You need to do this is the set of skeletal meshes that you are playing anims on is not bounded.
 */
var config	bool			bClearAnimSetLinkupCachesOnLoadMap;

/** If true, and the platform allows it, the game engine will utilize a secondary screen */
var config bool		bEnableSecondaryDisplay;
/** 
 *	If true, the game engine will create a secondary viewport on init 
 *	Dependent on bEnabledSecondaryDisplay being enabled.
 *	If false, and bEnableSecondaryDisplay is true, the secondary screen will just copy the first.
 */
var config bool		bEnableSecondaryViewport;
/** String name for any secondary viewport clients created for secondary screens */
var config string	SecondaryViewportClientClassName;

/** Secondary viewport clients inside of secondary windows (not for split screen) */
var init array<ScriptViewportClient>	SecondaryViewportClients;

/** Array parallel to SecondaryViewportClients - these are the frames that render the SecondaryViewport clients */
var init array<pointer>		SecondaryViewportFrames{FViewportFrame};


/** level streaming updates that should be applied immediately after committing the map change */
struct native LevelStreamingStatus
{
	var name PackageName;
	var bool bShouldBeLoaded, bShouldBeVisible;

	structcpptext
	{
		/** Constructors */
		FLevelStreamingStatus(FName InPackageName, UBOOL bInShouldBeLoaded, UBOOL bInShouldBeVisible)
		: PackageName(InPackageName), bShouldBeLoaded(bInShouldBeLoaded), bShouldBeVisible(bInShouldBeVisible)
		{}
		FLevelStreamingStatus()
		{}
    		FLevelStreamingStatus(EEventParm)
		{
			appMemzero(this, sizeof(FLevelStreamingStatus));
		}
	}
};
var const array<LevelStreamingStatus> PendingLevelStreamingStatusUpdates;

/** Handles to object references; used by the engine to e.g. the prevent objects from being garbage collected.	*/
var const	array<ObjectReferencer>	ObjectReferencers;

enum EFullyLoadPackageType
{
	/** Load the packages when the map in Tag is loaded */
	FULLYLOAD_Map,
	/** Load the packages before the game class in Tag is loaded. The Game name MUST be specified in the URL (game=Package.GameName). Useful for loading packages needed to load the game type (a DLC game type, for instance) */
	FULLYLOAD_Game_PreLoadClass,
	/** Load the packages after the game class in Tag is loaded. Will work no matter how game is specified in UWorld::SetGameInfo. Useful for modifying shipping gametypes by loading more packages (mutators, for instance) */
	FULLYLOAD_Game_PostLoadClass,
	/** Fully load the package as long as the DLC is loaded */
	FULLYLOAD_Always,
	/** Load the package for a mutator that is active */
	FULLYLOAD_Mutator,
};

/** Struct to help hold information about packages needing to be fully-loaded for DLC, etc */
struct native FullyLoadedPackagesInfo
{
	/** When to load these packages */
	var EFullyLoadPackageType FullyLoadType;

	/** When this map or gametype is loaded, the packages in the following array will be loaded and added to root, then removed from root when map is unloaded */
	var string Tag;

	/** The list of packages that will be fully loaded when the above Map is loaded */
	var array<name> PackagesToLoad;

	/** List of objects that were loaded, for faster cleanup */
	var array<object> LoadedObjects;
};

/** A list of tag/array pairs that is used at LoadMap time to fully load packages that may be needed for the map/game with DLC, but we can't use DynamicLoadObject to load from the packages */
var array<FullyLoadedPackagesInfo> PackagesToFullyLoad;

/** Struct to hold a UNetDriver and an assoicated name */
struct native NamedNetDriver
{
	/** The name associated with the driver */
	var name NetDriverName;

	/** A pointer to a UNetDriver */
	var const native pointer NetDriver{class UNetDriver};
};

/** A list of named UNetDrivers */
var const transient array<NamedNetDriver> NamedNetDrivers;

/** Temporary Animation Tagging Information: until we integrate Content Tagging
  * This is configurable information in Engine
  * Tag: Name of Tag
  * Contains: Contains text
  * Priority is index of array
  */
struct native AnimTag
{
	var string 	Tag; // This is Tag name
	var array<string> Contains; // This is contains strings, i.e. _cvr_ or _cover_ for Tag name Cover
};

/**
 * Animation tag for stat system: This is temporary until we can add content tag to animation
 * Currently it auto tags based on "contains" - Check DefaultEngine.ini for modification
 */
var config array<AnimTag> AnimTags;	

/**
 * Creates a UNetDriver and associates a name with it.
 *
 * @param NetDriverName The name to associate with the driver.
 *
 * @return True if the driver was created successfully, false if there was an error.
 */
native final function bool CreateNamedNetDriver(name NetDriverName);

/**
 * Destroys a UNetDriver based on its name.
 *
 * @param NetDriverName The name associated with the driver to destroy.
 */
native final function DestroyNamedNetDriver(name NetDriverName);

/** Returns the global online subsytem pointer. This will be null for PIE */
native static final noexport function OnlineSubsystem GetOnlineSubsystem();

/** Returns the DLC enumerator object pointer. This will be null for PIE */
native static final noexport function DownloadableContentEnumerator GetDLCEnumerator();

/** Returns the DLC manager object pointer. This will be null for PIE */
native static final noexport function DownloadableContentManager GetDLCManager();

/** Returns whether this game engine has any secondary screens attached */
native static final noexport function bool HasSecondaryScreenActive();

cpptext
{

	// Constructors.
	UGameEngine();

	/**
	 * Redraws all viewports.
	 *
	 * @param	bShouldPresent	Whether we want this frame to be presented
	 */
	void RedrawViewports( UBOOL bShouldPresent = TRUE );
	
	/**
	 * Called to allow overloading by child engines
	 */
	virtual void LoadMapRedrawViewports(void)
	{
		RedrawViewports();
	}

	// UObject interface.
	void FinishDestroy();

	// UEngine interface.
	void Init();

	/**
	 * Called at shutdown, just before the exit purge.
	 */
	virtual void PreExit();

	virtual void Tick( FLOAT DeltaSeconds );
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	void SetClientTravel( const TCHAR* NextURL, ETravelType TravelType );
	virtual FLOAT GetMaxTickRate( FLOAT DeltaTime, UBOOL bAllowFrameRateSmoothing = TRUE );
	virtual void SetProgress( EProgressMessageType MessageType, const FString& Title, const FString& Message );

	/**
	 * Handles freezing/unfreezing of rendering
	 */
	virtual void ProcessToggleFreezeCommand();

	/**
	 * Handles frezing/unfreezing of streaming
	 */
	 virtual void ProcessToggleFreezeStreamingCommand();

	// UGameEngine interface.
	virtual UBOOL Browse( FURL URL, FString& Error );
	virtual UBOOL LoadMap( const FURL& URL, class UPendingLevel* Pending, FString& Error );
	virtual void PostLoadMap();
	virtual void CancelPending();

	/**
	 * Spawns all of the registered server actors
	 */
	virtual void SpawnServerActors(void);

	/**
	 * Construct a UNetDriver object based on an .ini setting
	 *
	 * @return The created NetDriver object, or NULL if it fails
	 */
	virtual class UNetDriver* ConstructNetDriver();

	/**
	 * Returns the online subsystem object. Returns null if GEngine isn't a
	 * game engine
	 */
	static UOnlineSubsystem* GetOnlineSubsystem(void);

	/**
	 * Creates the online subsystem that was specified in UEngine's
	 * OnlineSubsystemClass. This function is virtual so that licensees
	 * can provide their own version without modifying Epic code.
	 */
	virtual void InitOnlineSubsystem(void);

	/**
	 * Creates the specified objects for dealing with DLC.
	 */
	void InitGameSingletonObjects(void);

	/**
	 * @return the DLC enumerator, or null if GEngine isn't a game engine
	 */
	static UDownloadableContentEnumerator* GetDLCEnumerator(void)
	{
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		if (GameEngine)
		{
			return GameEngine->DLCEnumerator;
		}
		return NULL;
	}

	/**
	 * @return the DLC manager, or null if GEngine isn't a game engine
	 */
	static UDownloadableContentManager* GetDLCManager(void)
	{
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		if (GameEngine)
		{
			return GameEngine->DLCManager;
		}
		return NULL;
	}

	/**
	 * @return TRUE, if the GEngine is a game engine and has any secondary screens active
	 */
	static UBOOL HasSecondaryScreenActive(void)
	{
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		if (GameEngine)
		{
			check(GameEngine->SecondaryViewportClients.Num() == GameEngine->SecondaryViewportFrames.Num());
			return (GameEngine->SecondaryViewportFrames.Num() > 0 ? TRUE : FALSE);
		}
		return FALSE;
	}

	// Async map change/ persistent level transition code.

	/**
	 * Prepares the engine for a map change by pre-loading level packages in the background.
	 *
	 * @param	LevelNames	Array of levels to load in the background; the first level in this
	 *						list is assumed to be the new "persistent" one.
	 *
	 * @return	TRUE if all packages were in the package file cache and the operation succeeded,
	 *			FALSE otherwise. FALSE as a return value also indicates that the code has given
	 *			up.
	 */
	UBOOL PrepareMapChange(const TArray<FName>& LevelNames);

	/**
	 * Returns the failure description in case of a failed map change request.
	 *
	 * @return	Human readable failure description in case of failure, empty string otherwise
	 */
	FString GetMapChangeFailureDescription();

	/**
	 * Returns whether we are currently preparing for a map change or not.
	 *
	 * @return TRUE if we are preparing for a map change, FALSE otherwise
	 */
	UBOOL IsPreparingMapChange();

	/**
	 * Returns whether the prepared map change is ready for commit having called.
	 *
	 * @return TRUE if we're ready to commit the map change, FALSE otherwise
	 */
	UBOOL IsReadyForMapChange();

	/**
	 * Finalizes the pending map change that was being kicked off by PrepareMapChange.
	 *
	 * @return	TRUE if successful, FALSE if there were errors (use GetMapChangeFailureDescription
	 *			for error description)
	 */
	UBOOL CommitMapChange();

	/**
	 * Commit map change if requested and map change is pending. Called every frame.
	 */
	void ConditionalCommitMapChange();

	/**
	 * Cancels pending map change.
	 */
	void CancelPendingMapChange();

	/**
	 * Adds a map/package array pair for pacakges to load at LoadMap
	 *
	 * @param FullyLoadType When to load the packages (based on map, gametype, etc)
	 * @param Tag Map/game for which the packages need to be loaded
	 * @param Packages List of package names to fully load when the map is loaded
	 * @param bLoadPackagesForCurrentMap If TRUE, the packages for the currently loaded map will be loaded now
	 */
	void AddPackagesToFullyLoad(EFullyLoadPackageType FullyLoadType, const FString& Tag, const TArray<FName>& Packages, UBOOL bLoadPackagesForCurrentMap);

	/**
	 * Empties the PerMapPackages array, and removes any currently loaded packages from the Root
	 */
	void CleanupAllPackagesToFullyLoad();

	/**
	 * Loads the PerMapPackages for the given map, and adds them to the RootSet
	 *
	 * @param FullyLoadType When to load the packages (based on map, gametype, etc)
	 * @param Tag Name of the map/game to load packages for
	 */
	void LoadPackagesFully(EFullyLoadPackageType FullyLoadType, const FString& Tag);

	/**
	 * Removes the PerMapPackages from the RootSet
	 *
	 * @param FullyLoadType When to load the packages (based on map, gametype, etc)
	 * @param Tag Name of the map/game to cleanup packages for
	 */
	void CleanupPackagesToFullyLoad(EFullyLoadPackageType FullyLoadType, const FString& Tag);

	/**
	 * Finds a UNetDriver based on its name.
	 *
	 * @param NetDriverName The name associated with the driver to find.
	 *
	 * @return A pointer to the UNetDriver that was found, or NULL if it wasn't found.
	 */
	UNetDriver* FindNamedNetDriver(FName NetDriverName);

	/**
	 * Creates a new FViewportFrame with a viewport client of class SecondaryViewportClientClassName
	 */
	void CreateSecondaryViewport(UINT SizeX, UINT SizeY);

	/**
	 * Closes all secondary viewports opened with CreateSecondaryViewport
	 */
	void CloseSecondaryViewports();
}
