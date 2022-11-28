//=============================================================================
// LevelInfo contains information about the current level. There should 
// be one per level and it should be actor 0. UnrealEd creates each level's 
// LevelInfo automatically so you should never have to place one
// manually.
//
// The ZoneInfo properties in the LevelInfo are used to define
// the properties of all zones which don't themselves have ZoneInfo.
//=============================================================================
class LevelInfo extends ZoneInfo
	native
	config(game)
	nativereplication
notplaceable;

//-----------------------------------------------------------------------------
// Editor support.

var(Editor) array<EdLayer>	Layers;			// The master list of layers defined by the level designers
var(Editor) BookMark		BookMarks[10];	// Level bookmarks

//-----------------------------------------------------------------------------
// Level time.

// Time passage.
var float TimeDilation;          // Normally 1 - scales real time passage.

// Current time.
var           float	TimeSeconds;   // Time in seconds since level began play.
var transient int   Year;          // Year.
var transient int   Month;         // Month.
var transient int   Day;           // Day of month.
var transient int   DayOfWeek;     // Day of week.
var transient int   Hour;          // Hour.
var transient int   Minute;        // Minute.
var transient int   Second;        // Second.
var transient int   Millisecond;   // Millisecond.
var			  float	PauseDelay;		// time at which to start pause

var()		bool	bRBPhysNoInit;		// Start _NO_ Rigid Body physics for this level. Only really for the Entry level.

//-----------------------------------------------------------------------------
// Level Summary Info

var(LevelSummary) localized String Title;
var(LevelSummary) String Author;
var(LevelSummary) localized String Description;
var(LevelSummary) Texture2D Screenshot;
var(LevelSummary) String DecoTextName;
var             PlayerReplicationInfo Pauser;          // If paused, name of person pausing the game.
var		LevelSummary Summary;
var           string VisibleGroups;			// List of the group names which were checked when the level was last saved
var transient string SelectedGroups;		// A list of selected groups in the group browser (only used in editor)
//-----------------------------------------------------------------------------
// Flags affecting the level.

var(LevelSummary) bool HideFromMenus;
var() bool           bLonePlayer;     // No multiplayer coordination, i.e. for entranceways.
var bool             bBegunPlay;      // Whether gameplay has begun.
var bool             bPlayersOnly;    // Only update players.
var bool			 bDropDetail;	  // frame rate is below DesiredFrameRate, so drop high detail actors
var bool			 bAggressiveLOD;  // frame rate is well below DesiredFrameRate, so make LOD more aggressive
var bool             bStartup;        // Starting gameplay.
var	bool			 bPathsRebuilt;	  // True if path network is valid
var bool			 bHasPathNodes;
var globalconfig bool bKickLiveIdlers;	// if true, even playercontrollers with pawns can be kicked for idling

//-----------------------------------------------------------------------------
// Miscellaneous information.

var Texture2D DefaultTexture;
var Texture2D WireframeTexture;
var Texture2D WhiteSquareTexture;
var Texture2D LargeVertex;
var Texture2D BSPVertex;
var transient enum ELevelAction
{
	LEVACT_None,
	LEVACT_Loading,
	LEVACT_Saving,
	LEVACT_Connecting,
	LEVACT_Precaching
} LevelAction;

var transient GameReplicationInfo GRI;

//-----------------------------------------------------------------------------
// Networking.

var enum ENetMode
{
	NM_Standalone,        // Standalone game.
	NM_DedicatedServer,   // Dedicated server, no local client.
	NM_ListenServer,      // Listen server.
	NM_Client             // Client only, no local server.
} NetMode;
var string ComputerName;  // Machine's name according to the OS.
var string EngineVersion; // Engine version.
var string MinNetVersion; // Min engine version that is net compatible.

//-----------------------------------------------------------------------------
// Gameplay rules

var() string DefaultGameType;
var GameInfo Game;
var float DefaultGravity;
var() float StallZ;	//vehicles stall if they reach this

//-----------------------------------------------------------------------------
// Navigation point and Pawn lists (chained using nextNavigationPoint and nextPawn).

var const	NavigationPoint		NavigationPointList;
var const	Controller			ControllerList;
var			Objective			ObjectiveList; //FIXMESTEVE REMOVE

//-----------------------------------------------------------------------------
// Replication/Networking - FIXMESTEVE move these properties
var float MoveRepSize;
var globalconfig float MaxClientFrameRate;

// speed hack detection
var globalconfig float MaxTimeMargin; 
var globalconfig float TimeMarginSlack;
var globalconfig float MinTimeMargin;


// these two properties are valid only during replication
var const PlayerController ReplicationViewer;	// during replication, set to the playercontroller to
												// which actors are currently being replicated
var const Actor  ReplicationViewTarget;				// during replication, set to the viewtarget to
												// which actors are currently being replicated

//-----------------------------------------------------------------------------
// Server related.

var string NextURL;
var bool bNextItems;
var float NextSwitchCountdown;

cpptext
{
	// Constructors.
	ALevelInfo() {}

	// AActor interface.
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
	void CheckForErrors();
	void PreNetReceive();
	void PostNetReceive();

	// Level functions
	void SetZone( UBOOL bTest, UBOOL bForceRefresh );
	void SetVolumes();
	virtual void SetVolumes(const TArray<class AVolume*>& Volumes);
	APhysicsVolume* GetDefaultPhysicsVolume();
	APhysicsVolume* GetPhysicsVolume(FVector Loc, AActor *A, UBOOL bUseTouch);
}

//-----------------------------------------------------------------------------
// Functions.

native simulated function bool IsEntry();

simulated function class<GameInfo> GetGameClass()
{
	local class<GameInfo> G;

	if(Level.Game != None)
		return Level.Game.Class;

	if (GRI != None && GRI.GameClass != "")
		G = class<GameInfo>(DynamicLoadObject(GRI.GameClass,class'Class'));
	if(G != None)
		return G;

	if ( DefaultGameType != "" )
		G = class<GameInfo>(DynamicLoadObject(DefaultGameType,class'Class'));

	return G;
}

//
// Return the URL of this level on the local machine.
//
native simulated function string GetLocalURL();

//
// Demo build flag
//
native simulated static final function bool IsDemoBuild();  // True if this is a demo build.

/** 
 * Returns whether we are running on a console platform or on the PC.
 *
 * @return TRUE if we're on a console, FALSE if we're running on a PC
 */
native simulated static final function bool IsConsoleBuild();

//
// Return the URL of this level, which may possibly
// exist on a remote machine.
//
native simulated function string GetAddressURL();

//
// Jump the server to a new level.
//
event ServerTravel( string URL, bool bItems )
{
	if( NextURL=="" )
	{
		bNextItems          = bItems;
		NextURL             = URL;
		if( Game!=None )
			Game.ProcessServerTravel( URL, bItems );
		else
			NextSwitchCountdown = 0;
	}
}

//
// ensure the DefaultPhysicsVolume class is loaded.
//
function ThisIsNeverExecuted()
{
	local DefaultPhysicsVolume P;
	P = None;
}

/* Reset() 
reset actor to initial state - used when restarting level without reloading.
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
	reliable if( bNetDirty && Role==ROLE_Authority )
		Pauser, TimeDilation, DefaultGravity;
}

/**
 * Grabs the default level sequence and returns it.
 * 
 * @return		the default level sequence object
 */
native final function Sequence GetLevelSequence();

native final function SetLevelRBGravity(vector NewGrav);

defaultproperties
{
	RemoteRole=ROLE_SimulatedProxy
	bAlwaysRelevant=true
	TimeDilation=1.0
	Title="Untitled"
	bHiddenEd=True
	WhiteSquareTexture=WhiteSquareTexture
	LargeVertex=Texture2D'EngineResources.LargeVertex'
	BSPVertex=Texture2D'EngineResources.BSPVertex'
	bWorldGeometry=true
	VisibleGroups="None"
	DefaultGravity=-750.0
    MoveRepSize=+42.0
    StallZ=10000.f
    MaxTimeMargin=+1.0
    TimeMarginSlack=+1.35
    MinTimeMargin=-1.0
    MaxClientFrameRate=+90.0

	bBlockActors=true

	Components.Remove(Sprite)
}
