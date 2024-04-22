
/**
 * WorldInfo
 *
 * Actor containing all script accessible world properties.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class WorldInfo extends ZoneInfo
	native(GameEngine)
	config(game)
	hidecategories(Actor,Advanced,Display,Events,Object,Attachment)
	//hidecategories(PhysicsAdvanced)
	nativereplication
	notplaceable
	dependson(PostProcessEffect)
	dependson(MusicTrackDataStructures);

/** Maximum number of bookmarks																		*/
const MAX_BOOKMARK_NUMBER = 10;

/** Default post process settings used by post processing volumes.									*/
var(Rendering)	config				PostProcessSettings		DefaultPostProcessSettings;

/** The post process chain for the entire world*/
var(Rendering)						PostProcessChain		WorldPostProcessChain;

/** Whether or not post process effects should persist when this level is unloaded					*/
var(Rendering) config				bool					bPersistPostProcessToNextLevel;

/** Squint mode kernel size (same as DOF). */
var(Rendering) config				float					SquintModeKernelSize;

/** Linked list of post processing volumes, sorted in descending order of priority.					*/
var	const noimport transient		PostProcessVolume		HighestPriorityPostProcessVolume;

/** Default reverb settings used by reverb volumes.													*/
var(Audio)	config				ReverbSettings			DefaultReverbSettings;

/** Default interior settings used by reverb volumes.												*/
var(Audio)	config				InteriorSettings		DefaultAmbientZoneSettings;

/** Mobile only: Enables distance-based fog globally for this level */
var(Mobile) bool  bFogEnabled;

/** Mobile only: Sets the distance fog start distance.  Can be negative. */
var(Mobile) float FogStart;

/** Mobile only: Sets the distance fog end distance.  Must be larger than FogStart. */
var(Mobile) float FogEnd;

/** Mobile only: Sets the distance fog color */
var(Mobile) color FogColor;

/** Mobile only: Enables bump offset mapping for this level for mobile materials that use that */
var(Mobile) bool  bBumpOffsetEnabled;

/* Mobile only: Sets the bump offset threshold for mobile materials that use that */
var(Mobile) float BumpEnd;

/** Mobile: True to enable gamma correction for all shaders when rendering this level.  Enabling this will yield lighting that more closely resembles other platforms, but may reduce rendering performance slightly. */
var(Mobile) bool bUseGammaCorrection;

/** Linked list of reverb volumes, sorted in descending order of priority.							*/
var	const noimport transient		ReverbVolume			HighestPriorityReverbVolume;

/** Array of AMassiveLODOverrideVolume's in the world. */
var	const noimport transient		array<MassiveLODOverrideVolume>	MassiveLODOverrideVolumes;

/** A array of portal volumes */
var	const noimport transient		array<PortalVolume>		PortalVolumes;
/** An array of environment volumes */
var	const noimport transient		array<EnvironmentVolume>    EnvironmentVolumes;

/** Level collection. ULevels are referenced by FName (Package name) to avoid serialized references. Also contains offsets in world units */
var(WorldInfo) const editconst editinline array<LevelStreaming> StreamingLevels;

/**
 * This is a bool on the level which is set when a light that needs to have lighting rebuilt
 * is moved.  This is then checked in CheckMap for errors to let you know that this level should
 * have lighting rebuilt.
 **/
var 					bool 					bMapNeedsLightingFullyRebuilt;

/** Set to true when one or primitives are affected by multiple dominant lights. */
var                     bool                    bMapHasMultipleDominantLightsAffectingOnePrimitive;
/** Time in appSeconds unbuilt time was last encountered. 0 means not yet.							*/
var	transient			double					LastTimeUnbuiltLightingWasEncountered;

/**
 * This is a bool on the level which is set when the AI detects that paths are either not set up correctly
 * or need to be rebuilt. If set the HUD will display a warning message on the screen.
 */
var						bool					bMapHasPathingErrors;

/** Whether it was requested that the engine bring up a loading screen and block on async loading. */
var						bool					bRequestedBlockOnAsyncLoading;

var(Editor)	editoronly	BookMark				BookMarks[MAX_BOOKMARK_NUMBER];			// Level bookmarks
var(Editor)	editoronly	KismetBookMark			KismetBookMarks[MAX_BOOKMARK_NUMBER];		// Kismet bookmarks
var(Editor)	editoronly editinline	array<ClipPadEntry>		ClipPadEntries;			// Clip pad entries
var						float					TimeDilation;			// Normally 1 - scales real time passage.
var						float					DemoPlayTimeDilation;		// additional TimeDilation applied only during demo playback
var	transient			float					TimeSeconds;			// Time in seconds since level began play, but IS paused when the game is paused, and IS dilated/clamped.
var	transient			float					RealTimeSeconds;		// Time in seconds since level began play, but is NOT paused when the game is paused, and is NOT dilated/clamped.
var transient           float                   AudioTimeSeconds;		// Time in seconds since level began play, but IS paused when the game is paused, and is NOT dilated/clamped.
var	transient const		float					DeltaSeconds;			// Frame delta time in seconds adjusted by e.g. time dilation.
var transient			float					PauseDelay;				// time at which to start pause
var transient			float					RealTimeToUnPause;		// If non-zero, when RealTimeSeconds reaches this, unpause the game.

var						PlayerReplicationInfo	Pauser;					// If paused, name of person pausing the game.
var	editoronly deprecated string				VisibleGroups;			// List of the group names which were checked when the level was last saved
var	editoronly			string					VisibleLayers;			// List of the layer names which were checked when the level was last saved

var						bool					bBegunPlay;				// Whether gameplay has begun.
var						bool					bPlayersOnly;			// Only update players.
var						bool					bPlayersOnlyPending;	// Only update players.  Next frame will set bPlayersOnly
var                     bool                    bSuspendAI;             // Hook for game-specific suspension of AI processing
var transient			bool					bDropDetail;			// frame rate is below DesiredFrameRate, so drop high detail actors
var transient			bool					bAggressiveLOD;			// frame rate is well below DesiredFrameRate, so make LOD more aggressive
var						bool					bStartup;				// Starting gameplay.
var						bool					bPathsRebuilt;			// True if path network is valid
var						bool					bHasPathNodes;
/** That map is default map or not **/
var	transient const		bool					bIsMenuLevel;

// Kismet debugging flags
var const editoronly transient bool bDebugPauseExecution;
var const editoronly transient bool bDebugStepExecution;

/**
 * Bool that indicates that 'console' input is desired. This flag is mis named as it is used for a lot of gameplay related things
 * (e.g. increasing collision size, changing vehicle turning behavior, modifying put down/up weapon speed, bot behavior)
 *
 * currently set when you are running a console build (implicitly or explicitly via ?param on the commandline)
 */
var						transient bool			bUseConsoleInput;

var						Texture2D				DefaultTexture;
var						Texture2D				WireframeTexture;
var						Texture2D				WhiteSquareTexture;
var						Texture2D				LargeVertex;
var						Texture2D				BSPVertex;

/**
 * This is the array of string which will be called after a tick has occurred.  This allows
 * functions which GC and/or delete objects to be executed from .uc land!
 **/
var 					array<string>			DeferredExecs;


var transient			GameReplicationInfo		GRI;
var enum ENetMode
{
	NM_Standalone,        // Standalone game.
	NM_DedicatedServer,   // Dedicated server, no local client.
	NM_ListenServer,      // Listen server.
	NM_Client             // Client only, no local server.
} NetMode;

var						string					ComputerName;			// Machine's name according to the OS.
var						string					EngineVersion;			// Engine version.
var						string					MinNetVersion;			// Min engine version that is net compatible.

var						GameInfo				Game;
var(ZoneInfo)			float					StallZ;					// vehicles stall if they reach this
var	transient			float					WorldGravityZ;			// current gravity actually being used
var	const globalconfig	float					DefaultGravityZ;		// default gravity (game specific) - set in defaultgame.ini
var(ZoneInfo)			float					GlobalGravityZ;			// optional level specific gravity override set by level designer
var globalconfig		float					RBPhysicsGravityScaling;	// used to scale gravity for rigid body physics

var transient const private	NavigationPoint		NavigationPointList;
var const private			Controller			ControllerList;
var const					Pawn				PawnList;
var transient const			CoverLink			CoverList;
var transient const private Pylon               PylonList;

var						float					MoveRepSize;

/** stores information on a viewer that actors need to be checked against for relevancy */
struct native NetViewer
{
	var PlayerController InViewer;
	var Actor Viewer;
	var vector ViewLocation;
	var vector ViewDir;

	structcpptext
	{
		FNetViewer(UNetConnection* InConnection, FLOAT DeltaSeconds);
	}
};
/** valid only during replication - information about the player(s) being replicated to
 * (there could be more than one in the case of a splitscreen client)
 */
var const array<NetViewer> ReplicationViewers;

var						string					NextURL;
var						float					NextSwitchCountdown;
/** The type of travel to perform next when doing a server travel */
var ETravelType NextTravelType;

/** Maximum size of textures for packed light and shadow maps */
var(Rendering)			int						PackedLightAndShadowMapTextureSize;

/** 
 * Causes the BSP build to generate as few sections as possible.  
 * This is useful when you need to reduce draw calls but can reduce texture streaming efficiency and effective lightmap resolution.
 * Note - changes require a rebuild to propagate.  Also, be sure to select all surfaces and make sure they all have the same flags to minimize section count.
 */
var(Rendering)			bool					bMinimizeBSPSections;

/** Default color scale for the level */
var(Rendering)			vector					DefaultColorScale;

/** if true, do not grant player with default inventory (presumably, the LD's will be setting it manually) */
var()					bool					bNoDefaultInventoryForPlayer;

/** The default game type to use when starting this map in the game. If this value is NULL, the INI setting for default game type is used. */
var(GameType) class<GameInfo>	DefaultGameType;

/**
 * This is the list of GameTypes which this map can support.  This is used for SeekFree loading
 * code so the map has a reference to the game type it will be played with so it can cook
 * all of the specific assets
 **/
var(GameType) array< class<GameInfo> > GameTypesSupportedOnThisMap;

/** 
 * This is the gametype that should be used when starting the map in PIE. 
 * Note: if this value is not NULL it will override the DefaultGameType value when running PIE
 */
var(GameType) editoronly class<GameInfo> GameTypeForPIE;

/** list of objects referenced by Actors that will be destroyed by the client on load (because they have bStatic and bNoDelete == false)
 * this is so that they will persist in memory on the client in case the server needs to replicate them
 * (generated at editor time so that server and client memory usage is more consistent)
 */
var const editconst array<Object> ClientDestroyedActorContent;

/** array of levels that were loaded into this map via PrepareMapChange() / CommitMapChange() (to inform newly joining clients) */
var const transient array<name> PreparingLevelNames;
var const transient name CommittedPersistentLevelName;

var objectreferencer PersistentMapForcedObjects;


/** Audio component used for playing music tracks via SeqAct_PlayMusicTrack */
var transient AudioComponent MusicComp;
/** Param information for the currently playing MusicComp */
var transient MusicTrackStruct CurrentMusicTrack;
/** Version of a new music track request replicated to clients */
var transient repnotify MusicTrackStruct ReplicatedMusicTrack;

/** If true, don't add "no paths from" warnings to map error list in editor.  Useful for maps that don't need AI support, but still have a few NavigationPoint actors in them. */
var() bool bNoPathWarnings;

/** Flag for controlling whether Mobile warnings are shown in map checks */
var(WorldInfo) config bool bNoMobileMapWarnings;

/** title of the map displayed in the UI */
var() localized string Title;

/** Who created this map */
var() string Author;

/** when this flag is set, more time is allocated to background loading (replicated) */
var bool bHighPriorityLoading;
/** copy of bHighPriorityLoading that is not replicated, for clientside-only loading operations */
var bool bHighPriorityLoadingLocal;

/** game specific map information - access through GetMapInfo()/SetMapInfo() */
var() protected{protected} instanced MapInfo MyMapInfo;

/** particle emitter pool for gameplay effects that are spawned independent of their owning Actor */
var globalconfig string EmitterPoolClassPath;
var transient EmitterPool MyEmitterPool;

/** decal pool and lifetime manager */
var globalconfig string DecalManagerClassPath;
var transient DecalManager MyDecalManager;

/** fractured mesh manager */
var globalconfig string FractureManagerClassPath;
var transient FractureManager MyFractureManager;

/** Particle event manager **/
var globalconfig string ParticleEventManagerClassPath;
var transient ParticleEventManager MyParticleEventManager;

/**
 * Overriding is useful for doing perf testing / load testing / being able to overcome memory issues in other tools until they are fixed
 * withOUT compromising the RuleSet creation process and the application of the rulesets in the level 
 */
var(ProcBuildings) bool bUseProcBuildingRulesetOverride;
var(ProcBuildings) ProcBuildingRuleset ProcBuildingRulesetOverride;

/** Used by SkeletalMeshComponent Ticking optimization. */
var const transient int	SkelMeshCompTickTagCount;
/** TRUE if player is in Interactive mode. */
var const transient bool bInteractiveMode;

/** For specifying which compartments should run on a given frame */
struct native CompartmentRunList
{
	/** The rigid body compartment will run on this frame */
	var() bool RigidBody;

	/** The fluid compartment will run on this frame */
	var() bool Fluid;

	/** The cloth compartment will run on this frame */
	var() bool Cloth;

	/** The soft body compartment will run on this frame */
	var() bool SoftBody;

	structdefaultproperties
	{
		RigidBody=true
		Fluid=true
		Cloth=true
		SoftBody=true
	}
};

/** Parameters used for a PhysX primary scene or compartment */
struct native PhysXSimulationProperties
{
	/**
		Whether or not to put the scene (or compartment) in PhysX hardware, if available.
	*/
	var() bool	bUseHardware;

	/**
		If true, substep sizes are fixed equal to TimeStep.
		If false, substep sizes are varied to fit an integer number of times into the frame time.
	*/
	var() bool	bFixedTimeStep;

	/** The fixed or maximum substep size, depending on the value of bFixedTimeStep. */
	var() float	TimeStep;

	/** The maximum number of substeps allowed per frame. */
	var() int		MaxSubSteps;

	structdefaultproperties
	{
		bUseHardware=false
		bFixedTimeStep=false
		TimeStep=0.02
		MaxSubSteps=5
	}
};

/** Timings for primary and compartments. */
struct native PhysXSceneProperties
{
	/** Timing settings for the PhysX primary scene */
	var()	editinline	PhysXSimulationProperties			PrimaryScene;

	/** Timing settings for the PhysX rigid body compartment */
	var()	editinline	PhysXSimulationProperties			CompartmentRigidBody;

	/** Timing settings for the PhysX fluid compartment */
	var()	editinline	PhysXSimulationProperties			CompartmentFluid;

	/** Timing settings for the PhysX cloth compartment */
	var()	editinline	PhysXSimulationProperties			CompartmentCloth;

	/** Timing settings for the PhysX soft body compartment */
	var()	editinline	PhysXSimulationProperties			CompartmentSoftBody;

	structdefaultproperties
	{
		CompartmentRigidBody=(bUseHardware=false,bFixedTimeStep=false,TimeStep=0.02,MaxSubSteps=2)
		CompartmentFluid=(bUseHardware=true,bFixedTimeStep=false,TimeStep=0.02,MaxSubSteps=1)
		CompartmentCloth=(bUseHardware=true,bFixedTimeStep=true,TimeStep=0.02,MaxSubSteps=2)
		CompartmentSoftBody=(bUseHardware=true,bFixedTimeStep=true,TimeStep=0.02,MaxSubSteps=2)
	}
};

/** Timings for primary and compartments. */

/** Double buffered physics compartments enabled */
var(PhysicsAdvanced)	bool								bSupportDoubleBufferedPhysics;

/** The maximum frame time allowed for physics calculations */
var(PhysicsAdvanced)	float								MaxPhysicsDeltaTime;

/** The maximum number of substeps allowed in any physics scene/partition. */
var				config int						MaxPhysicsSubsteps;

/** If TRUE, physics simulation will ignore time elapsed between frames, and use (0.033 * TimeDilation) */
var(Physics)    bool                            bPhysicsIgnoreDeltaTime;

/** Timing parameters for the scene, primary and compartments. */
var(PhysicsAdvanced)	editinline PhysXSceneProperties	PhysicsProperties;

/** Which compartments run on which frames (list is cyclic).  An empty list means all compartments run on all frames. */
var(PhysicsAdvanced)	array< CompartmentRunList >		CompartmentRunFrames;

/** Default skin width */
var(PhysicsAdvanced) 	float							DefaultSkinWidth;

/** Global APEX resource budget override. If negative the INI setting will be used instead. */
var(PhysicsAdvanced)	float								ApexLODResourceBudget;
/** A higher value would increase the amount of resources allocated to Apex Destruction */
var(PhysicsAdvanced)    float							ApexDestructionLODResourceValue;

/** A higher value would increase the amount of resources allocated to Apex Clothing */
var(PhysicsAdvanced)    float							ApexClothingLODResourceValue;
/** APEX **/
struct native ApexModuleDestructibleSettings
{
	/** The maximum number of active PhysX actors which represent dynamic groups of chunks (islands).  If a 
		fracturing event would cause more islands to be created, then oldest islands are released and the chunks
		they represent destroyed. If negative the INI setting will be used instead. */
	var()	int		MaxChunkIslandCount;
	/** The maximum number of PhysX shapes which represent destructible chunks.  
		If a fracturing event would cause more shapes to be created, then oldest islands are released 
		and the chunks they represent destroyed.*/
	var()	int		MaxShapeCount;
	/** Reserved for future use. */
	var		int		MaxRrbActorCount;
	/** Every destructible asset defines a min and max lifetime, and maximum separation distance for its chunks.
		Chunk islands are destroyed after this time or separation from their origins. This parameter sets the
		lifetimes and max separations within their min-max ranges. The valid range is [0,1]. */
	var()	float	MaxChunkSeparationLOD<ClampMin=0.0|ClampMax=1.0>;
	/** If set, override the INI setting with Max Chunk Separation LOD in the WorldInfo properties. */
	var()	bool	bOverrideMaxChunkSeparationLOD;

	structdefaultproperties
	{
		MaxShapeCount=-1
		MaxChunkIslandCount=-1
		MaxRrbActorCount=-1
		MaxChunkSeparationLOD=1.0
		bOverrideMaxChunkSeparationLOD=false
	}
};

var(PhysicsAdvanced) ApexModuleDestructibleSettings		DestructibleSettings;

/** Verticals */
var				PhysicsLODVerticalEmitter		EmitterVertical;

/** Parameters for emitter vertical */
struct native PhysXEmitterVerticalProperties
{
	var() bool	bDisableLod;

	/**
		Min value for particle LOD range.
	*/
	var() int	ParticlesLodMin;

	/**
		Max value for particle LOD range.
	*/
	var() int	ParticlesLodMax;

	/**
		Limit for packets per PhysXParticleSystem. Caped to 900.
	*/
	var() int PacketsPerPhysXParticleSystemMax;


	/** Selects either cylindrical or spherical packet range culling. */
	var() bool	bApplyCylindricalPacketCulling;

	/**
		Parameter for scaling spawn lod impact. 1.0: As much as possible lod through
		emitter spawn rate/lifetime control. 0.0: Lod constraint handled only through
		fifo control.
	*/
	var() float SpawnLodVsFifoBias;

	structdefaultproperties
	{
		bDisableLod=true
		ParticlesLodMin=0
		ParticlesLodMax=15000
		PacketsPerPhysXParticleSystemMax=500
		bApplyCylindricalPacketCulling=true
		SpawnLodVsFifoBias=1.0
	}
};

struct native PhysXVerticalProperties
{
	/** Parameters for Emitter Vertical */
	var()	editinline	PhysXEmitterVerticalProperties		Emitters;

	structdefaultproperties
	{
	}
};

/** Vertical parameters. */
var(PhysicsAdvanced)	editinline PhysXVerticalProperties VerticalProperties;

var native private array<pointer> WorldAttractors{class AWorldAttractor};

enum EConsoleType
{
	CONSOLE_Any,
	CONSOLE_Xbox360,
	CONSOLE_PS3,
	CONSOLE_Mobile,
	CONSOLE_IPhone,
	CONSOLE_Android,
	CONSOLE_WiiU,
	CONSOLE_Flash,
};

/** Struct used for passing back results from GetWorldFractureSettings */
struct native WorldFractureSettings
{
	var	float	ChanceOfPhysicsChunkOverride;
	var	bool	bEnableChanceOfPhysicsChunkOverride;
	var bool	bLimitExplosionChunkSize;
	var float	MaxExplosionChunkSize;
	var bool	bLimitDamageChunkSize;
	var float	MaxDamageChunkSize;
	var int		MaxNumFacturedChunksToSpawnInAFrame;
	var float	FractureExplosionVelScale;
};

// DO NOT READ THESE FRACTURE VALUES DIRECTLY - USE GetWorldFractureSettings FUNCTION TO HANDLE STREAMING CORRECTLY

/** Allows global override of the ChanceOfPhysicsChunk setting. */
var(Fracture)	private{private} config float	ChanceOfPhysicsChunkOverride;

/** If TRUE, uses ChanceOfPhysicsChunkOverride instead of that set in the FracturedStaticMesh. */
var(Fracture)	private{private} config bool	bEnableChanceOfPhysicsChunkOverride;

/**	If TRUE, limit the max dimension of the bounding box of a fracture chunk due to explosion to MaxExplosionChunkSize */
var(Fracture)	private{private} config bool		bLimitExplosionChunkSize;
/** Max dimension of the bounding box of a fracture chunk due to explosion. */
var(Fracture)	private{private} config float	MaxExplosionChunkSize;
/**	If TRUE, limit the max dimension of the bounding box of a fracture chunk due to weapon damage to bLimitDamageChunkSize */
var(Fracture)	private{private} config bool		bLimitDamageChunkSize;
/** Max dimension of the bounding box of a fracture chunk due to weapon damage. */
var(Fracture)	private{private} config float	MaxDamageChunkSize;
/** Scaling for chunk thrown out during explosion on a fractured mesh */
var(Fracture)	private{private} config float	FractureExplosionVelScale;

/** Max number of Fractured Chunks to Spawn in a frame. **/
var(Fracture) private{private} int MaxNumFacturedChunksToSpawnInAFrame;
/** Number of chunks already spawned this frame **/
var transient int NumFacturedChunksSpawnedThisFrame;

/** How much damage weapons do to fractured static meshes. */
var				config float	FracturedMeshWeaponDamage;

/** 
 * Whether to place visibility cells inside Precomputed Visibility Volumes and along camera tracks in this level. 
 * Precomputing visibility reduces rendering thread time at the cost of some runtime memory and somewhat increased lighting build times.
 */
var	(PrecomputedVisibility)		bool	bPrecomputeVisibility;

/** 
 * Whether to place visibility cells on shadow casting surfaces only, or everywhere inside Precomputed Visibility Volumes. 
 * Placing cells everywhere in the volumes is useful for games where the camera is not restrained to an area around the ground,
 * But generates a lot more cells than just placing on surfaces, so build times and memory usage will increase.
 */
var	(PrecomputedVisibility)		bool	bPlaceCellsOnSurfaces;

/** 
 * World space size of precomputed visibility cells in x and y.
 * Smaller sizes produce more effective occlusion culling at the cost of increased runtime memory usage and lighting build times.
 */
var	(PrecomputedVisibility)		int		VisibilityCellSize;

enum EVisibilityAggressiveness
{
	VIS_LeastAggressive,
	VIS_ModeratelyAggressive,
	VIS_MostAggressive,
	VIS_Max
};

/** 
 * Determines how aggressive precomputed visibility should be.  
 * More aggressive settings cull more objects but also cause more visibility errors like popping.
 */
var (PrecomputedVisibility)		EVisibilityAggressiveness VisibilityAggressiveness;

/** Brightness applied to the indirect lighting of character light environments that are lit by any dominant light. */
var	(LightEnvironment)			float	CharacterLitIndirectBrightness <UIMin=0.1 | UIMax=1.0 | ClampMin=0.0 | ClampMax=5.0>;

/** Contrast factor applied to the indirect lighting of character light environments that are lit by any dominant light. */
var	(LightEnvironment)			float	CharacterLitIndirectContrastFactor <UIMin=1.0 | UIMax=2.0 | ClampMin=0.5 | ClampMax=5.0>;

/** Brightness applied to the indirect lighting of character light environments that are shadowed by all dominant lights. */
var	(LightEnvironment)			float	CharacterShadowedIndirectBrightness <UIMin=0.1 | UIMax=1.0 | ClampMin=0.0 | ClampMax=5.0>;

/** Contrast factor applied to the indirect lighting of character light environments that are shadowed by all dominant lights. */
var	(LightEnvironment)			float	CharacterShadowedIndirectContrastFactor <UIMin=1.0 | UIMax=2.0 | ClampMin=0.5 | ClampMax=5.0>;

/** 
 * Increases the contrast of light environment lighting on characters by scaling up the brightest directions and scaling down the rest by this factor. 
 * Note that this setting only affects light environments completely in shadow from all dominant lights.
 */
var	(LightEnvironment)			float	CharacterLightingContrastFactor <UIMin=1.0 | UIMax=2.0 | ClampMin=0.5 | ClampMax=5.0>;

/** Whether to allow temporal AA. */
var (Rendering) globalconfig	bool	bAllowTemporalAA;

/** 
 * Panoramic environment texture for image reflections. 
 * The texture should be laid out so that the horizon is along the bottom (v = 0) and straight up in world space is along the top (v = 1).
 * The u direction of the texture then corresponds to rotation around the Z world axis.
 */
var	(Rendering)				Texture2D	ImageReflectionEnvironmentTexture <bShowOnlyWhenTrue=bShowD3D11Properties>;

/** Color to be multiplied against ImageReflectionEnvironmentTexture.  Alpha controls brightness. */
var	(Rendering)				LinearColor	ImageReflectionEnvironmentColor <bShowOnlyWhenTrue=bShowD3D11Properties>;

/** Angle to rotate the environment texture around the world Z axis, in degrees. */
var	(Rendering)				float		ImageReflectionEnvironmentRotation <bShowOnlyWhenTrue=bShowD3D11Properties>;

/** The type of lightmaps to use on this map for mobile platforms */
enum EPreferredLightmapType
{
	EPLT_Default,
	EPLT_Directional,
	EPLT_Simple
};

/**
 *	Which lightmap type to use on this level.
 *	EPLT_Default		Use the platform default.
 *	EPLT_Directional	Force directional lightmaps (provided the platform supports them)
 *	EPLT_Simple			Force simple lightmaps
 */
var			EPreferredLightmapType		PreferredLightmapType;

/** On-screen debug message handling */
/** Helper struct for tracking on screen messages. */
struct transient native ScreenMessageString
{
	/** The 'key' for this message. */
	var transient qword Key;
	/** The message to display. */
	var transient string ScreenMessage;
	/** The color to display the message in. */
	var transient color DisplayColor;
	/** The number of frames to display it. */
	var transient float TimeToDisplay;
	/** The number of frames it has been displayed so far. */
	var transient float CurrentTimeDisplayed;
};

/** A collection of messages to display on-screen. */
var transient native Map_Mirror ScreenMessages{TMap<INT, FScreenMessageString>};

/** A collection of messages to display on-screen. */
var transient native array<ScreenMessageString>	PriorityScreenMessages;

/** Whether this level should be using fully-featured global illumination. If not, it will use old-style direct lightmaps. */
var(Lightmass) editoronly bool bUseGlobalIllumination;

/** 
 * Whether to force lightmaps and other precomputed lighting to not be created even when the engine thinks they are needed.
 * This is useful for improving iteration in levels with fully dynamic lighting and shadowing.
 * Note that any lighting and shadowing interactions that are usually precomputed will be lost if this is enabled.
 */
var(Lightmass) bool bForceNoPrecomputedLighting;

/** The number of triangles per leaf of the kdop tree in lightmass*/
var editoronly int MaxTrianglesPerLeaf <FixedIncrement=4|ClampMin=4|Multiple=4>;

/** The Lightmass-related settings for this level */
var deprecated editoronly instanced LightmassLevelSettings LMLevelSettings;

var editoronly transient native map{FGuid, class ULandscapeInfo*} LandscapeInfoMap;

struct native LightmassWorldInfoSettings
{
    /** 
	 * Scale of the level relative to Gears of War 2 levels. 
	 * All scale-dependent Lightmass setting defaults have been tweaked to work well in Gears 2 levels, 
	 * Any levels with a different scale should use this scale to compensate.   
	 */
    var(General)    float       StaticLightingLevelScale;
	/** 
	 * Number of times light is allowed to bounce off of surfaces, starting from the light source. 
	 * 0 is direct lighting only, 1 is one bounce, etc. 
	 * Bounce 1 takes the most time to calculate and contributes the most to visual quality, followed by bounce 2.  
	 * Successive bounces are nearly free, but have a much lower impact.
	 */
	var(General)	int			NumIndirectLightingBounces;
	/** 
	 * Color that rays which miss the scene will pick up.  
	 * This is effectively a light surrounding the entire level that is shadowed, but doesn't emit indirect lighting.
	 */
	var(General)	color		EnvironmentColor;
	/** Scales EnvironmentColor to allow independent color and brightness controls. */
	var(General)	float		EnvironmentIntensity;

	/**
	 *	In advanced mode the color that rays which miss the scen will pick up depends on 'sun'.
	 *  The closer ray's direction is to the direction of sun the more color of sun is blended/added into picked color.
	 */
	var(AdvancedEnvironmentColor)	bool		bEnableAdvancedEnvironmentColor;
	/** Color of the sun */
	var(AdvancedEnvironmentColor)	color		EnvironmentSunColor;
	/** Scales EnvironmentSunColor to allow independent color and brightness controls. */
	var(AdvancedEnvironmentColor)	float		EnvironmentSunIntensity;
	var(AdvancedEnvironmentColor)	float	    EnvironmentLightTerminatorAngle<ToolTip=In degrees. Range of sun color.>;
	var(AdvancedEnvironmentColor)	Vector	    EnvironmentLightDirection<ToolTip=If this value is zeroed, the direction of dominant directional light on current scene will be used>;

	/** Scales the emissive contribution of all materials in the scene. */
	var(General)	float		EmissiveBoost;
	/** Scales the diffuse contribution of all materials in the scene. */
	var(General)	float		DiffuseBoost;
	/** Scales the specular contribution of all materials in the scene. */
	var				float		SpecularBoost;
	/** 
	 * Lerp factor that controls the influence of normal maps with directional lightmaps on indirect lighting.
	 * A value of 0 gives a physically correct distribution of light, which may result in little normal influence in areas only lit by indirect lighting, but less lightmap compression artifacts.
	 * A value of .8 results in 80% of the lighting being redistributed in the dominant incident lighting direction, which effectively increases the per-pixel normal's influence,
	 * But causes more severe lightmap compression artifacts.
	 */
	var(General)	float		IndirectNormalInfluenceBoost;

	/** If TRUE, AmbientOcclusion will be enabled. */
	var(Occlusion)	bool		bUseAmbientOcclusion;
	/** If TRUE, Lightmass will generate static shadowing information for image reflections. */
	var(Occlusion)	bool		bEnableImageReflectionShadowing;
	/** How much of the AO to apply to direct lighting. */
	var(Occlusion)	float		DirectIlluminationOcclusionFraction;
	/** How much of the AO to apply to indirect lighting. */
	var(Occlusion)	float		IndirectIlluminationOcclusionFraction;
	/** Higher exponents increase contrast. */
	var(Occlusion)	float		OcclusionExponent;
	/** Fraction of samples taken that must be occluded in order to reach full occlusion. */
	var(Occlusion)	float		FullyOccludedSamplesFraction;
	/** Maximum distance for an object to cause occlusion on another object. */
	var(Occlusion)	float		MaxOcclusionDistance;
	/** If TRUE, override normal direct and indirect lighting with just the exported diffuse term. */
	var(Debug)		bool		bVisualizeMaterialDiffuse;
	/** If TRUE, override normal direct and indirect lighting with just the AO term. */
	var(Debug)		bool		bVisualizeAmbientOcclusion;
	/** If TRUE, compress shadowmap with DXT1. */
	var(General)	bool		bCompressShadowmap;

	// Default diffuse boost to a high value since diffuse textures are often authored centering around a value of .1 or so
	// Change this to default closer to 1 if your game's diffuse textures average to closer to .5
	structdefaultproperties
	{
        StaticLightingLevelScale=1
		NumIndirectLightingBounces=3
		EnvironmentColor=(R=0,G=0,B=0)
		EnvironmentIntensity=1.0

		bEnableAdvancedEnvironmentColor=false
		EnvironmentSunColor=(R=0,G=0,B=0)
		EnvironmentSunIntensity=1.0
		EnvironmentLightTerminatorAngle=90.0

		EmissiveBoost=1.0
		DiffuseBoost=5.0
		SpecularBoost=1.0
		IndirectNormalInfluenceBoost=0.3
		bUseAmbientOcclusion=false
		DirectIlluminationOcclusionFraction=0.5
		IndirectIlluminationOcclusionFraction=1.0
		OcclusionExponent=1.0
		FullyOccludedSamplesFraction=1.0
		MaxOcclusionDistance=200.0
		bVisualizeMaterialDiffuse=false
		bVisualizeAmbientOcclusion=false
		bCompressShadowmap=false
	}
};

var(Lightmass) LightmassWorldInfoSettings LightmassSettings <ScriptOrder=true>;

/**
 *	Path constraint path goal evaluator pools
 *  - we don't want to new/delete these every time we path.. so we pool them here
 */
/** Path constraint */
const MAX_INSTANCES_PER_CLASS = 5;
struct native NavMeshPathConstraintCacheDatum
{
	var int ListIdx;
	var NavMeshPathConstraint List[MAX_INSTANCES_PER_CLASS];
};
var native map{UClass*, FNavMeshPathConstraintCacheDatum} NavMeshPathConstraintCache;


/** Path Goal evaluator pool */
struct native NavMeshPathGoalEvaluatorCacheDatum
{
	// the current free index
	var int ListIdx;
	// the list of constraints
	var NavMeshPathGoalEvaluator List[MAX_INSTANCES_PER_CLASS];
};
var native map{UClass*, FNavMeshPathGoalEvaluatorCacheDatum} NavMeshPathGoalEvaluatorCache;

/** Set if any CrowdAgents are currently spawned in this world - needs to be set when agent is spawned */
var bool bHaveActiveCrowd;

/** Population manager being used by this level */
var CrowdPopulationManagerBase PopulationManager;

/** The lighting quality the level was last built with */
var(Lightmass) editconst ELightingBuildQuality LevelLightingQuality;

/** Steps in host migration progression */
enum EHostMigrationProgress
{
	/** Not migrating */
	HostMigration_None,
	/** Client peer is waiting to find the new host */
	HostMigration_FindingNewHost,
	/** Client peer is starting migration as new host */
	HostMigration_MigratingAsHost,
	/** Client peer is starting migration as a client */
	HostMigration_MigratingAsClient,
	/** Client peer is traveling as a client */
	HostMigration_ClientTravel,
	/** New host has been selected and is waiting to travel. HostMigrationTravelCountdown begins */
	HostMigration_HostReadyToTravel,
	/** Migration process started but failed due to timeout */
	HostMigration_Failed
};
/** State of host migration */
struct native HostMigrationState
{
	/** Current progress of host migration process for client peers */
	var EHostMigrationProgress HostMigrationProgress;
	/** Elapsed time since host migration has started. Ie. transitioned from HostMigration_None */
	var float HostMigrationElapsedTime;
	/** Countdown begins ticking once new host has been selected. When it hits 0 then the new host will travel */
	var float HostMigrationTravelCountdown;
	/** URL to be used by the newly selected host to travel */
	var string HostMigrationTravelURL;
	/** if TRUE then host migration is currently enabled */
	var bool bHostMigrationEnabled;
};
/** Info relevant to migrating a client peer to new host */
var const transient HostMigrationState PeerHostMigration;
/** Config if TRUE then host migration is allowed to occur */
var config bool bAllowHostMigration;
/** Started as soon as a client peer disconnects.  If migration does succeed within this time then fall back to server disconnect failure */
var config float HostMigrationTimeout;

var transient PhysicsVolume FirstPhysicsVolume;

/** Allow gameplay frame pauses that IB does on hits (allow pause to not cut out the audio) */
var bool bGameplayFramePause;

cpptext
{
	// UObject interface.

	/**
	 * Called when a property on this object has been modified externally
	 *
	 * @param PropertyThatChanged the property that was modified
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();

	/**
	 * Called after GWorld has been set. Used to load, but not associate, all
	 * levels in the world in the Editor and at least create linkers in the game.
	 * Should only be called against GWorld::PersistentLevel's WorldInfo.
	 */
	void LoadSecondaryLevels();

	/** Utility for returning the ULevelStreaming object for a particular sub-level, specified by package name */
	ULevelStreaming* GetLevelStreamingForPackageName(FName PackageName);

	// AActor interface.
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif

	// Level functions
	void SetZone( UBOOL bTest, UBOOL bForceRefresh );
	void SetVolumes();
	virtual void SetVolumes(const TArray<class AVolume*>& Volumes);
	APhysicsVolume* GetDefaultPhysicsVolume();
	APhysicsVolume* GetPhysicsVolume(const FVector& Loc, AActor *A, UBOOL bUseTouch);
	FLOAT GetRBGravityZ();

	/**
	 * Finds the post process settings to use for a given view location, taking into account the world's default
	 * settings and the post process volumes in the world.
	 * @param	ViewLocation			Current view location.
	 * @param	bUseVolumes				Whether to use the world's post process volumes
	 * @param	OutPostProcessSettings	Upon return, the post process settings for a camera at ViewLocation.
	 * @return	If the settings came from a post process volume, the post process volume is returned.
	 */
	APostProcessVolume* GetPostProcessSettings(const FVector& ViewLocation,UBOOL bUseVolumes,FPostProcessSettings& OutPostProcessSettings);

	/** Checks whether temporal AA is allowed. */
	UBOOL GetAllowTemporalAA() const;

	FLinearColor GetEnvironmentColor() const;

	/**
	 * Finds the reverb settings to use for a given view location, taking into account the world's default
	 * settings and the reverb volumes in the world.
	 *
	 * @param	ViewLocation			Current view location.
	 * @param	OutReverbSettings		[out] Upon return, the reverb settings for a camera at ViewLocation.
	 * @param	OutInteriorSettings		[out] Upon return, the interior settings for a camera at ViewLocation.
	 * @return							If the settings came from a reverb volume, the reverb volume's object index is returned.
	 */
	INT GetAudioSettings( const FVector& ViewLocation, struct FReverbSettings* OutReverbSettings, struct FInteriorSettings* OutInteriorSettings );

	/**
	 * Finds the portal volume actor at a given location
	 */
	APortalVolume* GetPortalVolume( const FVector& Location );

	/** Returns TRUE if the given position is inside any AMassiveLODOverrideVolume in the world. */
	UBOOL IsInsideMassiveLODVolume(const FVector& Location) const;

	/**
	 * Remap sound locations through portals
	 */
	FVector RemapLocationThroughPortals( const FVector& SourceLocation, const FVector& ListenerLocation );

	/**
	 * Sets bMapNeedsLightingFullyRebuild to the specified value.  Marks the worldinfo package dirty if the value changed.
	 *
	 * @param	bInMapNeedsLightingFullyRebuild			The new value.
	 */
	void SetMapNeedsLightingFullyRebuilt(UBOOL bInMapNeedsLightingFullyRebuild);

	/**
     * @return Whether or not we can spawn more fractured chunks this frame
	 **/
	UBOOL CanSpawnMoreFracturedChunksThisFrame() const;

	/**
	* Determines whether a map is the default local map.
	*
	* @param	MapName	if specified, checks whether MapName is the default local map; otherwise, checks the currently loaded map.
	*
	* @return	TRUE if the map is the default local (or front-end) map.
	*/
	UBOOL IsMenuLevel( FString MapName=TEXT("") );

	/** Add a string to the On-screen debug message system */
    void AddOnScreenDebugMessage(QWORD Key,FLOAT TimeToDisplay,FColor DisplayColor,const FString& DebugMessage);

	/** Retrieve the message for the given key */
	UBOOL OnScreenDebugMessageExists(QWORD Key);

	/**
	 * overidden to track references of pooled path constraints
	 */
	virtual void AddReferencedObjects( TArray<UObject*>& ObjectArray );

	/** 
     * Update the current host migration progress
	 * 
	 * @param NewState current host migration state to set 
	 */
	void UpdateHostMigrationState(EHostMigrationProgress NewState);

	/**
	 *  Registers an attractor and returns TRUE if it was added successfully.
	 */
	UBOOL RegisterAttractor(AWorldAttractor* Attractor);

	/**
	 *  Unregisters an attractor and returns TRUE if it was already registered and removed.
	 */
	UBOOL UnregisterAttractor(AWorldAttractor* Attractor);

	typedef TIndexedContainerIterator< TArray<AWorldAttractor*> > AWorldAttractorIter;

	/**
	 *  Gets a non-const iterator for the list of attractors.
	 */
	AWorldAttractorIter GetAttractorIter();
}

//-----------------------------------------------------------------------------
// Functions.

/** ==== NAV MESH PATH CONSTRAINT/GOAL EVALUATOR POOLING ==== */
/**
 * will go through all the pools for each constraint/evaluator class that has been added so far and reset
 * the instance index to 0 (call this after the search is complete to indicate you're done
 * with the constraints from the pool)
 */
native function ReleaseCachedConstraintsAndEvaluators();

/**
 * Will search for an existing free constraint from the cache, and return it if one is found
 * otherwise it will add a new instance to the cache and return that
 * @param ConstraintClass - the class of the constraint to grab from the cache
 * @param Requestor - the handle that needs this constraint
 */
native function NavMeshPathConstraint GetNavMeshPathConstraintFromCache(class<NavMeshPathConstraint> ConstraintClass, NavigationHandle Requestor);

/**
 * Will search for an existing free Evaluator from the cache, and return it if one is found
 * otherwise it will add a new instance to the cache and return that
 * @param GoalEvalClass - the class of the evaluator to grab from the cache
 * @param Requestor - the handle that needs this constraint
 */
native function NavMeshPathGoalEvaluator GetNavMeshPathGoalEvaluatorFromCache(class<NavMeshPathGoalEvaluator> GoalEvalClass, NavigationHandle Requestor);


simulated event ReplicatedEvent(Name VarName)
{
	if (VarName == 'ReplicatedMusicTrack')
	{
		UpdateMusicTrack(ReplicatedMusicTrack);
	}
	Super.ReplicatedEvent(VarName);
}

/** Add a string to the On-screen debug message system */
native final function AddOnScreenDebugMessage(INT Key, FLOAT TimeToDisplay, Color DisplayColor, String DebugMessage);

/**
* Determines whether a map is the default local map.
*
* @param	MapName	if specified, checks whether MapName is the default local map; otherwise, checks the currently loaded map.
*
* @return	TRUE if the map is the default local (or front-end) map.
*/

static native noexport final function bool IsMenuLevel( optional string MapName );

/**
 * Sets the volume scale for music only (excludes sound effects). For Mobile, these are
 * mp3 files, for PC/Console these are any AudioComponents where "bIsMusic" is true.
 * 
 * @param VolumeMultiplier - Value between 0.0 and 1.0 to scale the volume
 */
native final function SetMusicVolume(float VolumeMultiplier);

native final function UpdateMusicTrack(MusicTrackStruct NewMusicTrack);

/**
 * Returns the Z component of the current world gravity and initializes it to the default
 * gravity if called for the first time.
 *
 * @return Z component of current world gravity.
 */
native function float GetGravityZ();

/**
 * Grabs the default game sequence and returns it.
 *
 * @return		the default game sequence object
 */
native simulated final function Sequence GetGameSequence() const;

/** Go over all loaded levels and get each root sequence */
native simulated final function array<Sequence> GetAllRootSequences() const;

native final function SetLevelRBGravity(vector NewGrav);

//
// Return the URL of this level on the local machine.
//
native simulated function string GetLocalURL() const;

//
// Demo build flag
//
native simulated static final function bool IsDemoBuild() const;  // True if this is a demo build.

/**
 * Returns whether we are running on a console platform or on the PC.
 * @param ConsoleType - if specified, only returns true if we're running on the specified platform
 *
 * @return TRUE if we're on a console, FALSE if we're running on a PC
 */
native simulated static final function bool IsConsoleBuild(optional EConsoleType ConsoleType=CONSOLE_Any) const;

/** Returns whether Scaleform is compiled in. */
native simulated static final function bool IsWithGFx() const;

/** Returns whether script is executing within the editor. */
native simulated static final function bool IsPlayInEditor() const;

/** Returns whether script is executing within a preview window */
native simulated static final function bool IsPlayInPreview() const;

/** Returns whether script is executing within a mobile preview window */
native simulated static final function bool IsPlayInMobilePreview() const;


native simulated final function ForceGarbageCollection( optional bool bFullPurge );

native simulated final function VerifyNavList();

//
// Return the URL of this level, which may possibly
// exist on a remote machine.
//
native simulated function string GetAddressURL() const;

simulated function class<GameInfo> GetGameClass()
{
	if(WorldInfo.Game != None)
		return WorldInfo.Game.Class;

	if (GRI != None && GRI.GameClass != None)
		return GRI.GameClass;

	return None;
}

/**
 * Jumps the server to new level.  If bAbsolute is true and we are using seemless traveling, we
 * will do an absolute travel (URL will be flushed).
 *
 * @param URL the URL that we are traveling to
 * @param bAbsolute whether we are using relative or absolute travel
 * @param bShouldSkipGameNotify whether to notify the clients/game or not
 */
simulated event ServerTravel(string URL, optional bool bAbsolute, optional bool bShouldSkipGameNotify)
{
	if ( InStr(url,"%")>=0 )
	{
		`log("URL Contains illegal character '%'.");
		return;
	}

	if (InStr(url,":")>=0 || InStr(url,"/")>=0 || InStr(url,"\\")>=0)
	{
		`log("URL blocked");
		return;
	}

	// Check for an error in the server's connection
	if (Game != None && Game.bHasNetworkError)
	{
		`Log("Not traveling because of network error");
		return;
	}

	// Set the next travel type to use
	NextTravelType = bAbsolute ? TRAVEL_Absolute : TRAVEL_Relative;

	// if we're not already in a level change, start one now
	// If the bShouldSkipGameNotify is there, then don't worry about seamless travel recursion
	// and accept that we really want to travel
	if (NextURL == "" && (!IsInSeamlessTravel() || bShouldSkipGameNotify))
	{
		NextURL = URL;
		if (Game != None)
		{
			// Skip notifying clients if requested
			if (!bShouldSkipGameNotify)
			{
				Game.ProcessServerTravel(URL, bAbsolute);
			}
		}
		else
		{
			NextSwitchCountdown = 0;
		}
	}
}

//
// ensure the DefaultPhysicsVolume class is loaded.
//
function ThisIsNeverExecuted(DefaultPhysicsVolume P)
{
	P = None;
}

simulated function PreBeginPlay()
{
	local class<EmitterPool> PoolClass;
	local class<DecalManager> DecalManagerClass;
	local class<FractureManager> FractureManagerClass;
	local class<ParticleEventManager> ParticleEventManagerClass;

	Super.PreBeginPlay();

	// create the emitter pool, decal manager, and fracture manager
	// we only need to do this for the persistent level's WorldInfo as sublevel actors will have their WorldInfo set to it on association
	if (WorldInfo.NetMode != NM_DedicatedServer && IsInPersistentLevel())
	{
		if (EmitterPoolClassPath != "")
		{
			PoolClass = class<EmitterPool>(DynamicLoadObject(EmitterPoolClassPath, class'Class'));
			if (PoolClass != None)
			{
				MyEmitterPool = Spawn(PoolClass, self,, vect(0,0,0), rot(0,0,0));
			}
		}
		if (DecalManagerClassPath != "")
		{
			DecalManagerClass = class<DecalManager>(DynamicLoadObject(DecalManagerClassPath, class'Class'));
			if (DecalManagerClass != None)
			{
				MyDecalManager = Spawn(DecalManagerClass, self,, vect(0,0,0), rot(0,0,0));
			}
		}
		if (FractureManagerClassPath != "")
		{
			FractureManagerClass = class<FractureManager>(DynamicLoadObject(FractureManagerClassPath, class'Class'));
			if (FractureManagerClass != None)
			{
				MyFractureManager = Spawn(FractureManagerClass, self,, vect(0,0,0), rot(0,0,0));
			}
		}
		if (ParticleEventManagerClassPath != "")
		{
			ParticleEventManagerClass = class<ParticleEventManager>(DynamicLoadObject(ParticleEventManagerClassPath, class'Class'));
			if (ParticleEventManagerClass != None)
			{
				MyParticleEventManager = Spawn(ParticleEventManagerClass, self,, vect(0,0,0), rot(0,0,0));
			}
		}
	}
}

simulated function PostBeginPlay()
{
	Super.PostBeginPlay();

	if (IsConsoleBuild())
	{
		bUseConsoleInput = true;
	}
}

/**
 * Reset actor to initial state - used when restarting level without reloading.
 */
function Reset()
{
	// perform garbage collection of objects (not done during gameplay)
	//ConsoleCommand("OBJ GARBAGE");
	Super.Reset();
}

//-----------------------------------------------------------------------------
// Network replication.

replication
{
	if( bNetDirty && Role==ROLE_Authority )
		Pauser, TimeDilation, WorldGravityZ, bHighPriorityLoading, ReplicatedMusicTrack;
}

/** returns all NavigationPoints in the NavigationPointList that are of the specified class or a subclass
 * @note this function cannot be used during level startup because the NavigationPointList is created in C++ ANavigationPoint::PreBeginPlay()
 * @param BaseClass the base class of NavigationPoint to return
 * @param (out) N the returned NavigationPoint for each iteration
 */
native final iterator function AllNavigationPoints(class<NavigationPoint> BaseClass, out NavigationPoint N);

/** returns all NavigationPoints whose cylinder intersects with a sphere of the specified radius at the specified point
 * this function uses the navigation octree and is therefore fast for reasonably small radii
 * @note this function cannot be used during level startup because the navigation octree is populated in C++ ANavigationPoint::PreBeginPlay()
 * @param BaseClass the base class of NavigationPoint to return
 * @param (out) N the returned NavigationPoint for each iteration
 * @param Radius the radius to search in
 */
native final iterator function RadiusNavigationPoints(class<NavigationPoint> BaseClass, out NavigationPoint N, vector Point, float Radius);

/** returns a list of NavigationPoints and ReachSpecs that intersect with the given point and extent
 * @param Point point to check
 * @param Extent box extent to check
 * @param (optional, out) Navs list of NavigationPoints that intersect
 * @param (optional, out) Specs list of ReachSpecs that intersect
 */
native final noexport function NavigationPointCheck(vector Point, vector Extent, optional out array<NavigationPoint> Navs, optional out array<ReachSpec> Specs);

/** returns all Controllers in the ControllerList that are of the specified class or a subclass
 * @note this function is only useful on the server; if you need the local player on a client, use Actor::LocalPlayerControllers()
 * @param BaseClass the base class of Controller to return
 * @param (out) C the returned Controller for each iteration
 */
native final iterator function AllControllers(class<Controller> BaseClass, out Controller C);

/** returns all Pawns in the PawnList that are of the specified class or a subclass
 * @note: useful on both client and server; pawns are added/removed from pawnlist in PostBeginPlay (on clients, only relevant pawns will be available)
 * @param BaseClass the base class of Pawn to return
 * @param (out) P the returned Pawn for each iteration
 * @param (optional) TestLocation is the world location used if
 *        TestRadius > 0
 * @param (optional) TestRadius is the radius checked against
 *        using TestLocation, skipping any pawns not within that
 *        value
 */
native final iterator function AllPawns(class<Pawn> BaseClass, out Pawn P, optional vector TestLocation, optional float TestRadius);

/**
 * Returns all NetConnections in the NetDriver ClientConnections list, along with their IP and Port
 * NOTE: Serverside only
 *
 * @param ClientConnection	The returned NetConnection
 * @param ClientIP		The IP of the NetConnection
 * @param ClientPort		The port the net connection is on
 */
native final iterator function AllClientConnections(out Player ClientConnection, out int ClientIP, out int ClientPort);


/**
 * Called by GameInfo.StartMatch, used to notify native classes of match startup (such as Kismet).
 */
native final function NotifyMatchStarted(optional bool bShouldActivateLevelStartupEvents=true, optional bool bShouldActivateLevelBeginningEvents=true, optional bool bShouldActivateLevelLoadedEvents=false);


/** asynchronously loads the given levels in preparation for a streaming map transition.
 * This codepath is designed for worlds that heavily use level streaming and gametypes where the game state should
 * be preserved through a transition.
 * @param LevelNames the names of the level packages to load. LevelNames[0] will be the new persistent (primary) level
 */
native final function PrepareMapChange(const out array<name> LevelNames);
/** returns whether there's a map change currently in progress */
native final function bool IsPreparingMapChange();
/** if there is a map change being prepared, returns whether that change is ready to be committed
 * (if no change is pending, always returns false)
 */
native final function bool IsMapChangeReady();
/** cancels pending map change (@note: we can't cancel pending async loads, so this won't immediately free the memory) */
native final function CancelPendingMapChange();
/** actually performs the map transition prepared by PrepareMapChange()
 * it happens in the next tick to avoid GC issues
 * if a map change is being prepared but isn't ready yet, the transition code will block until it is
 * wait until IsMapChangeReady() returns true if this is undesired behavior
 */
native final function CommitMapChange();


/** seamlessly travels to the given URL by first loading the entry level in the background,
 * switching to it, and then loading the specified level. Does not disrupt network communication or disconnet clients.
 * You may need to implement GameInfo::GetSeamlessTravelActorList(), PlayerController::GetSeamlessTravelActorList(),
 * GameInfo::PostSeamlessTravel(), and/or GameInfo::HandleSeamlessTravelPlayer() to handle preserving any information
 * that should be maintained (player teams, etc)
 * This codepath is designed for worlds that use little or no level streaming and gametypes where the game state
 * is reset/reloaded when transitioning. (like UT)
 * @param URL - the URL to travel to; must be on the same server as the current URL
 * @param bAbsolute (opt) - if true, URL is absolute, otherwise relative
 * @param MapPackageGuid (opt) - the GUID of the map package to travel to - this is used to find the file when it has been autodownloaded,
 * 				so it is only needed for clients
 */
native final function SeamlessTravel(string URL, optional bool bAbsolute, optional init Guid MapPackageGuid);

/** @return whether we're currently in a seamless transition */
native final function bool IsInSeamlessTravel();

/** this function allows pausing the seamless travel in the middle,
 * right before it starts loading the destination (i.e. while in the transition level)
 * this gives the opportunity to perform any other loading tasks before the final transition
 * this function has no effect if we have already started loading the destination (you will get a log warning if this is the case)
 * @param bNowPaused - whether the transition should now be paused
 */
native final function SetSeamlessTravelMidpointPause(bool bNowPaused);

/** @return the current MapInfo that should be used. May return one of the inner StreamingLevels' MapInfo if a streaming level
 *	transition has occurred via PrepareMapChange()
 */
native final function MapInfo GetMapInfo();

/** sets the current MapInfo to the passed in one */
native final function SetMapInfo(MapInfo NewMapInfo);

/**
 * @Returns the name of the current map
 */

native final function string GetMapName(optional bool bIncludePrefix);

/** @return the current detail mode */
native final function EDetailMode GetDetailMode();

/** @return whether a demo is being recorded */
native final function bool IsRecordingDemo();
/** @return whether a demo is being played back */
native final function bool IsPlayingDemo();
/** if playing back a demo, returns the frame information for that demo
 * @param CurrentFrame (optional, out) - current frame the demo playback is on
 * @param TotalFrames (optional, out) - total number of frames in the demo
 */
native final function GetDemoFrameInfo(optional out int CurrentFrame, optional out int TotalFrames);
/** @return if a demo is being played back and rewinding is enabled, returns the list of rewind points that are available
 * the values correspond to demo frames, relative to the return value of GetNumDemoFrames()
 */
native final function bool GetDemoRewindPoints(out array<int> OutRewindPoints);

/**
 * This function will do what ever memory tracking we have enabled.  Basically there are a myriad of memory tracking/leak detection
 * methods and this function abstracts all of that.
 **/
native final function DoMemoryTracking();

/** Get the current fracture settings for the loaded world - handles streaming correctly. */
native final function WorldFractureSettings GetWorldFractureSettings() const;

/** @return Allows non-actor derived classes to get access to the world info easily */
native final static function WorldInfo GetWorldInfo();

/** Returns an Environment Volume if one is found at given location. If many are overlapping, only the first one found will be returned. */
native final function EnvironmentVolume FindEnvironmentVolume(Vector TestLocation);

/**
 * Verifies that all players can be part of host migration
 *
 * @return TRUE if the host migration process can be started
 */
simulated event bool CanBeginHostMigration()
{
	local PlayerController PC;

	foreach LocalPlayerControllers(class'PlayerController', PC)
	{
		if (PC.IsPrimaryPlayer())
		{
			// make sure all players allowed to play online
			if (!PC.CanAllPlayersPlayOnline() ||
			// make sure there is a potential host to migrate to
				PC.BestNextHostPeers.Length == 0)
			{
				return false;
			}
		}
	}
	return true;
}

/**
 * Try to start the process of host migration. This is called when server disconnect is detected.
 * If host migration can't be started then fall back to the normal disconnect process.
 * Also notify peers through peer net driver RPC that server connection has been lost.
 *
 * @return TRUE if the host migration process was initiated successfully
 */
native function bool BeginHostMigration();

/**
 * Notification when host migration state has changed
 *
 * @param NewState current host migration state
 * @param OldState previous host migration state
 */
simulated event NotifyHostMigrationStateChanged(EHostMigrationProgress NewState,EHostMigrationProgress OldState)
{
	local PlayerController PC;

	// Detect initial start of host migration process 
	if (OldState == HostMigration_None &&
		NewState != HostMigration_None)
	{
		foreach LocalPlayerControllers(class'PlayerController', PC)
		{
			if (PC.IsPrimaryPlayer())
			{
				PC.NotifyHostMigrationStarted();
				break;
			}
		}
	}
}

/**
 * Enable or disable host migration.
 *
 * @param bEnabled TRUE if host migration should be enabled.
 */
native function ToggleHostMigration(bool bEnabled);

/**
 * Tells the world to compact the current Physics instance pools to return memory to general availability
 */
native final function ClearObjectPools();

defaultproperties
{
	Components.Remove(Sprite)

	RemoteRole=ROLE_SimulatedProxy
	bAlwaysRelevant=true
	TimeDilation=1.0
	DemoPlayTimeDilation=1.0
	bHiddenEd=True
	DefaultTexture=Texture2D'EngineResources.DefaultTexture'
	WhiteSquareTexture=WhiteSquareTexture
	LargeVertex=Texture2D'EditorResources.LargeVertex'
	BSPVertex=Texture2D'EditorResources.BSPVertex'
	bWorldGeometry=true
	VisibleGroups="None"
	VisibleLayers="None"
	MoveRepSize=+42.0
	bBlockActors=true
	StallZ=+1000000.0
	PackedLightAndShadowMapTextureSize=1024

	bUseGlobalIllumination=true
	MaxTrianglesPerLeaf=4

	DefaultColorScale=(X=1.f,Y=1.f,Z=1.f)

	bPlaceCellsOnSurfaces=True
	VisibilityCellSize=200
	VisibilityAggressiveness=VIS_LeastAggressive
	ImageReflectionEnvironmentColor=(R=1.f,G=1.f,B=1.f,A=1.f)

	CharacterLitIndirectBrightness=1
	CharacterLitIndirectContrastFactor=1
	CharacterShadowedIndirectBrightness=1
	CharacterShadowedIndirectContrastFactor=1
	CharacterLightingContrastFactor=1.5

	MaxPhysicsDeltaTime=0.33333333
	DefaultSkinWidth=0.025
	ApexLODResourceBudget=-1
	ApexDestructionLODResourceValue = 1000
	ApexClothingLODResourceValue = 1000

	MaxNumFacturedChunksToSpawnInAFrame=12

	Begin Object Class=PhysicsLODVerticalEmitter Name=PhysicsLODVerticalEmitter0
	End Object
	EmitterVertical=PhysicsLODVerticalEmitter0

	// Server travel is relative by default
	NextTravelType=TRAVEL_Relative

	bMovable=FALSE

	// mobile fog settings
	bFogEnabled=false
	FogStart=400.0
	FogEnd=4000.0
	FogColor=(R=128,G=128,B=255,A=192)

	// mobile bump offset settings
	bBumpOffsetEnabled=true
	BumpEnd=1000.0

	// Gamma correction on mobile defaults to off
	bUseGammaCorrection=false

	// Set level lighting quality to invalid for new maps with no built lighting or old maps where
	// quaility cant be determined.  The cooker will not warn when the quality is invalid
	LevelLightingQuality = Quality_MAX;
}
