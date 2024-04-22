/**
* MobilePlayerInput
*
* This is the base class for processing input for mobile devices while in the game
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

class MobilePlayerInput extends PlayerInput within GamePlayerController
	native
	DependsOn(MobileInputZone)
	config(Game);

/**
 * This structure contains data for individual touch events queued for a specific touch handle
 */

struct native TouchDataEvent
{
	/** Holds the type of event */
	var ETouchType EventType;

	/** What touchpad this came from */
	var byte TouchpadIndex;

	/** Holds the current location of the touch */
	var vector2D Location;

	/** Holds the device timestamp of when this event occurred */
	var double DeviceTime;
};



/**
 * the MPI keeps track of all touches coming from a device.  When the status of a touch changes, it tracks it and then passes it along to
 * the associated MobileInputZone.
 */

struct native TouchData
{
	/** Holds the ID of the current touch */
	var int Handle;

	/** What touchpad this came from */
	var byte TouchpadIndex;

	/** Holds the current location of the touch */
	var vector2D Location;

	/** Total distance that the finger moved since it initially touched down */
	var float TotalMoveDistance;

	/** Holds the device timestamp of when the original touch occurred */
	var double InitialDeviceTime;

	/** How long this touch has been active */
	var float TouchDuration;

	/** Device timestamp of most recent event */
	var double MoveEventDeviceTime;

	/** Time delta between the movement events the last time this touch moved */
	var float MoveDeltaTime;

	/** If true, this touch entry is in use, otherwise feel free to use it for touches */
	var bool bInUse;

	/** Holds the zone that is currently processing this touch */
	var MobileInputZone Zone;

	/** Holds the current state of the touch */
	var ETouchType State;

	/** Events queued up for this touch.  Because we may receive several touch movement events per tick,
	    we store a list of events and process them in order when we can. */
	var array<TouchDataEvent> Events;

	/** Holds the last active time (via TimeSeconds()) that will be used to timeout a zone */
	var float LastActiveTime;
};

/**
 * This enum contains the orientations of the interface (not the device)
 * The order of this currently matches the IPhone and changing it will break that device unless you fix those references
 */
enum EUIOrientation
{
	UI_Unknown,
	UI_Portait,
	UI_PortaitUpsideDown,
	UI_LandscapeRight,
	UI_LandscapeLeft,
};

/** Keeps track of all of the touches currently active on a device */
const NumTouchDataEntries		= 5;
var (Input) TouchData Touches[NumTouchDataEntries];

/**
 * The object that the user is currently interacting with.
 * e.g. When a user presses on the button, this button is the
 * interactive object until the user raises her finger and causes
 * an UnTouch event.
 */
var MobileMenuObject InteractiveObject;

/** Last time interactive object process input */
var double InteractiveObjectLastTime;

/** Defines a mobile input group */
struct native MobileInputGroup
{
	/** The name of this group */
	var string GroupName;

	/** The List of zones associated with this group */
	var editinline array<MobileInputZone> AssociatedZones;
};

/** Holds a list of available groups */
var (Input) EditInline array<MobileInputGroup> MobileInputGroups;

/** Holds the index of the current group */
var (Input) int CurrentMobileGroup;

/** Holds a list of mobile input zones. */
var (Input) editinline array<MobileInputZone> MobileInputZones;

/** Record of each MobileInputZone class (and subclasses) instances */
struct native MobileInputZoneClassMap
{
	/* Name of the instance */
	var string Name;

	/* Class type of the instance */
	var class<MobileInputZone> ClassType;
};

/** Classes that inherit from MobileInputZone - filled in by NativeInitializeInputSystem() */
var array<MobileInputZoneClassMap> MobileInputZoneClasses;

/** Holds the current Tilt value for mobile devices */
var (Input) float MobilePitch;

/** Holds the center value for the pitch. */
var (Input) float MobilePitchCenter;

/** Pitch sensitivity */
var (Input) float MobilePitchMultiplier;

/** Holds the current Yaw value for mobile devices */
var (Input) float MobileYaw;

/** Holds the center value for the Yaw. */
var (Input) float MobileYawCenter;

/** Pitch sensitivity */
var (Input) float MobileYawMultiplier;

/** How much of a dead zone should the pitch have */
var (input) config float MobilePitchDeadzoneSize;

/** How much of a dead zone should the yaw have */
var (input) config float MobileYawDeadzoneSize;

/** Used to determine if a touch is a double tap */
var (input) config float MobileDoubleTapTime;

/** You have to hold down a tap at least this long to register it as a tap */
var (input) config float MobileMinHoldForTap;

/** Used to determine how quickly to send repeat events for touch+held */
var (input) config float MobileTapRepeatTime;

/** If true, we want to allow input to occur during a cinematic */
var (input) bool bAllowTouchesInCinematic;
/** If set to true, then touches will be ignored */
var (input) bool bDisableTouchInput;
/** if set pass absolute touch locations instead of relative to the viewport location
 * (i.e. if this is true don't subtract MobileViewportOffset from touch locations)
 */
var (input) config bool bAbsoluteTouchLocations;
/** Holds the amount of time that a zone can go without input before being consider timed out */
var (input) config float ZoneTimeout;
/** If true this will collapse similar mobile inputs so that they won't get processed */
var (input) config bool bCollapseTouchInput;

// **********************************************************************************
// The MobilePlayerInput is also the hub for the mobile menu system.
// **********************************************************************************

/** This is the menu stack. */
var (menus) array<MobileMenuScene> MobileMenuStack;

/** Used for debugging native code */
var (debug) string NativeDebugString;

/** This will be set in NativeInitializeInputZones if -SimMobile is on the command line. */
var (debug) bool bFakeMobileTouches;

// Holds the amount of time the view port has been inactive
var (Current) float MobileInactiveTime;

// Ability to disable rendering of scenes.
var (debug) bool bDisableSceneRender;

// **********************************************************************************
// Motion support
// **********************************************************************************

/**
 * A General note about the DeviceMotion* variables.  The engine will attempt to use any motion sensing system
 * built in to the device to fill out these values.  For values not supported by the device, we will attempt to generate
 * values given the capibilites.  but so values may not be available / reliable
 */



/** Holds a list of handlers looking to listen in on Motion events */
var (Current) array<SeqEvent_MobileBase> MobileSeqEventHandlers;

/** Holds a list of handlers looking to listen in on raw touch events */
var (Current) array<SeqEvent_MobileRawInput> MobileRawInputSeqEventHandlers;


// @DEBUG - Remove if you wish to see all touch events
//var array<string> TouchDebug;

/** Store the current adjusted viewport size so we can adjust mobile input zones */
var vector2D MobileViewportOffset;
var vector2D MobileViewportSize;

/**
 * Process Mobile Input, handles mobile user input
 * Should be called regardless of game state.
 */
native function ProcessMobileInput( float DeltaTime );

/**
 * Function to cancel all mobile input before a Scaleform movie confuses the state
 */
native function CancelMobileInput();

/**
 * Overload the super PlayerInput event
 */
event PlayerInput( float DeltaTime )
{
	// Process the mobile input, this method is explicitly called regardless
	// of whether the game is paused so as to allow the mobile touch info
	// to be updated even when paused.
	ProcessMobileInput( DeltaTime );

	// update any other player input info.
	Super.PlayerInput( DeltaTime );
}

cpptext
{
	/**
	 * Takes a touch and looks up the InputZone that would handle it.
	 *
	 * @param TouchLocation		 Where the touch occurred
	 * @param TouchpadIndex		The index of the touchpad this touch came from
	 * @returns the zone that will be managing this touch
	 */
	UMobileInputZone* HitTest(FVector2D TouchLocation, UINT TouchpadIndex);

	/**
	 * Process an input key event received from the viewport.
	 *
	 * @param	Viewport		the viewport the input event was received from
	 * @param	ControllerId	gamepad/controller that generated this input event
	 * @param	Key				the name of the key which an event occured for (KEY_Up, KEY_Down, etc.)
	 * @param	EventType		the type of event which occured (pressed, released, etc.)
	 * @param	AmountDepressed	(analog keys only) the depression percent.
	 * @param	bGamepad - input came from gamepad (ie xbox controller)
	 *
	 * @return	TRUE to consume the key event, FALSE to pass it on.
	 */
	virtual UBOOL InputKey(INT ControllerId,FName Key,EInputEvent Event,FLOAT AmountDepressed=1.f,UBOOL bGamepad=FALSE);

	/**
		* Process an input axis (joystick, thumbstick, or mouse) event received from the viewport.
		*
		* @param	Viewport		the viewport the input event was received from
		* @param	ControllerId	the controller that generated this input axis event
		* @param	Key				the name of the axis that moved  (KEY_MouseX, KEY_XboxTypeS_LeftX, etc.)
		* @param	Delta			the movement delta for the axis
		* @param	DeltaTime		the time (in seconds) since the last axis update.
		*
		* @return	TRUE to consume the axis event, FALSE to pass it on.
		*/
	virtual UBOOL InputAxis(INT ControllerId,FName Key,FLOAT Delta,FLOAT DeltaTime, UBOOL bGamepad=FALSE);

	/**
	 * Handle a touch event coming from the device.
	 *
	 * NOTE: no processing of the touch happens here.  This just tracks the touch in the Touches stack.  Processing
	 * happens each tick
	 *
	 * @param Handle			the id of the touch
	 * @param Type				What type of event is this
	 * @param TouchLocation		Where the touch occurred
	 * @param DeviceTimestamp	Input event timestamp from the device
	 * @param TouchpadIndex		The index of the touchpad this touch came from
	 */
	virtual UBOOL InputTouch(INT ControllerId, UINT Handle, ETouchType Type, FVector2D TouchLocation, DOUBLE DeviceTimestamp, UINT TouchpadIndex=0);

	/**
	 * Update active touches, etc
	 *
	 * @param DeltaTime		Much time has elapsed since the last processing
	 */
	virtual void Tick(FLOAT DeltaTime);

	/**
	 * When input comes in to the player input, the first thing we need to do is process it for
	 * the menus.
	 *
	 * @param TouchHandle       A unique id for the touch
	 * @param TouchpadIndex		The index of the touchpad this touch came from
	 * @param EventType         What type of event is this
	 * @param TouchLocation     Where the touch occurred
	 * @param DeviceTimestamp	Time this event happened.
	 *
	 * @returns true if the menu system swallowed the input
	 */
	UBOOL ProcessMenuInput(UINT TouchHandle, UINT TouchpadIndex, ETouchType EventType, FVector2D TouchLocation, DOUBLE DeviceTimestamp);


	/**
	 * This function will iterate over the MobileSeqEventHandles array and cause them to be updated.
	 * It get's called once per frame.
	 */
	void UpdateListeners();

	/**
	 * Returns the Global Scaling values
	 *
	 * @return a FVector2 containing the global scale values
	 */
	FVector2D GetGlobalScale();


	/**
	 * Swap around components of a vector (with rotational values) based on an orientation.
	 * This will swap pitch and roll when a mobile device is rotated 90 degrees, etc
	 *
	 * @param Vec [in/out] Vector to modify
	 * @param Orientation Orientation for which the values were generated in
	 * @param bIsRotation TRUE if the vector represents rotation, not a directional vector
	 */
	static void ModifyVectorByOrientation(FVector& Vec, EUIOrientation Orientation, UBOOL bIsRotation)
	{
		switch (Orientation)
		{
			case UI_Portait:
				// this is the base orientation, so nothing to do
				break;

			case UI_PortaitUpsideDown:
				if (bIsRotation)
				{
					// negate roll and pitch
					Vec.X = -Vec.X;
					Vec.Z = -Vec.Z;
				}
				else
				{
					// negate x/y
					Vec.X = -Vec.X;
					Vec.Y = -Vec.Y;
				}
				break;

			case UI_LandscapeRight:
				if (bIsRotation)
				{
					// swap and negate (as needed) roll and pitch
					FLOAT Temp = Vec.X;
					Vec.X = -Vec.Z;
					Vec.Z = Temp;
				}
				else
				{
					// swap and negate (as needed) x and y
					FLOAT Temp = Vec.X;
					Vec.X = -Vec.Y;
					Vec.Y = Temp;
				}
				break;

			case UI_LandscapeLeft:
				if (bIsRotation)
				{
					// swap and negate (as needed) roll and pitch
					FLOAT Temp = Vec.X;
					Vec.X = Vec.Z;
					Vec.Z = -Temp;
				}
				else
				{
					// swap and negate (as needed) x and y
					FLOAT Temp = Vec.X;
					Vec.X = Vec.Y;
					Vec.Y = -Temp;
				}
				break;
		}
	}

public:
	/**
	 * Determine the size of the current interactive canvas
	 *
	 * @param ViewportSize Size of the canvas
	 */
	void GetInteractiveViewportSize(FVector2D& ViewportSize);

	/**
	 * Initialize, or reinitialize a zone
	 *
	 * @param Zone The Zone to initialize
	 * @param ViewportSize Size of the canvas
	 * @param bIsFirstInitialize TRUE if this is the first time the zone is initialized (use FALSE when canvas was resized)
	 */
	void NativeInitializeZone(UMobileInputZone* Zone, const FVector2D& ViewportSize, UBOOL bIsFirstInitialize);
};

/**
 * Invoked when the mobile menus did not process an Touch_Began.
 */
delegate OnTouchNotHandledInMenu();

/**
 * PreviewTouch X,Y - screenspace coordinates; true/false for handled/not handled.
 */
delegate bool OnPreviewTouch(float X, float Y, int TouchpadIndex);

/**
 * OnInputTouch is a low level handler for getting the actual touch data
 */
delegate OnInputTouch(int Handle, ETouchType Type, Vector2D TouchLocation, float DeviceTimestamp, int TouchpadIndex);

/**
 * Perform any native initialization of the subsystem
 */
native function NativeInitializeInputSystem();

/**
 * Iterates over the zones and pre-calculates the actual bounds based on the current device resolution
 */
native function NativeInitializeInputZones(bool bIsFirstInitialize);

/**
 * Update the mobile touch zones if the viewport aspect ratio changes
 */
native function ConditionalUpdateInputZones(int NewViewportX, int NewViewportY, int NewViewportSizeX, int NewViewportSizeY);

/**
 * Allows the game to send a InputKey event though the viewport.
 *
 * @param Key				the new of the key we are sending
 * @param Event				the Type of event
 * @param AmountDepressed	the strength of the event
 */
native function SendInputKey(name Key, EInputEvent Event, float AmountDepressed);

/**
 * Allows the game to send an InputAxis event through the viewport
 *
 * @param Key				the key we are sending
 * @param	Delta			the movement delta for the axis
 * @param	DeltaTime		the time (in seconds) since the last axis update.
 */
native function SendInputAxis(name Key, FLOAT Delta, FLOAT DeltaTime);

/**
 * Handle touch events in the 3D world. To use this assign the OnTapDelegate in a MobileInputZone to this function.
 *
 * @param Zone		        The mobile Input zone that triggered the delegate
 * @param EventType	        The type of input event that occurred
 * @param TouchLocation	    The screen location of the touch event
 *
 * @return true if the world actor swallows the input
 */
native function bool ProcessWorldTouch(MobileInputZone Zone, ETouchType EventType, Vector2D TouchLocation);

/**
 * The player controller will call this function directly after creating the input system
 */
function InitInputSystem()
{
	super.InitInputSystem();
	InitTouchSystem();
}

/**
 * When the client inits the input system, initialize it's touch system
 */
function ClientInitInputSystem()
{
	super.ClientInitInputSystem();
	InitTouchSystem();
}

function InitTouchSystem()
{
	NativeInitializeInputSystem();

	// We only want to initialize the
	if (bFakeMobileTouches || WorldInfo.IsConsoleBuild(CONSOLE_Mobile))
	{
		InitializeInputZones();
	}
}

/**
 * Initializes the input zones
 */
function InitializeInputZones()
{
	local int i,j;
	local MobileInputZone Zone;
	local class<FrameworkGame> FrameworkGameClass;

	// In a MP game, this function gets called twice - and that caused the zone
	// to be initialized twice making its pos/size wrong.
	if (MobileInputGroups.Length > 0)
	{
		return;
	}

	FrameworkGameClass = Class<FrameworkGame>(WorldInfo.GRI.GameClass);

	//`log("Initializing Input zones");

	if (FrameworkGameClass != none)
	{
		//`log("No of Config Groups:"@FrameworkGameClass.Default.RequiredMobileInputConfigs.Length);

		// Allocate Space
		MobileInputGroups.Length = FrameworkGameClass.Default.RequiredMobileInputConfigs.Length;

		for(i=0;i<FrameworkGameClass.Default.RequiredMobileInputConfigs.Length;i++)
		{
			// Add a Group for this config.

			//`log("Building Group"@FrameworkGameClass.Default.RequiredMobileInputConfigs[i].GroupName);

			// Attempt to add a game-defined, optional "DebugZone" if not final release script
			// `if(`notdefined(FINAL_RELEASE))
			// 		Zone = FindOrAddZone("DebugZone");
			// 		if (Zone != none)
			// 		{
			// 			//`log("    Adding special DebugZone");
			// 			MobileInputGroups[i].AssociatedZones.AddItem(Zone);
			// 		}
			// `endif

			MobileInputGroups[i].GroupName = FrameworkGameClass.Default.RequiredMobileInputConfigs[i].GroupName;
			for (j=0;j < FrameworkGameClass.Default.RequiredMobileInputConfigs[i].RequireZoneNames.Length;j++)
			{
				Zone = FindOrAddZone(FrameworkGameClass.Default.RequiredMobileInputConfigs[i].RequireZoneNames[j]);
				MobileInputGroups[i].AssociatedZones.AddItem(Zone);
			}
		}

		// Initialize the stored viewport size
		LocalPlayer(Outer.Player).ViewportClient.GetViewportSize(MobileViewportSize);

		// Perform the native initialization for them
		NativeInitializeInputZones(true);
		RefreshKismetLinks();
	}
}

/**
 * Search for zone in the list and return it if found
 *
 */
function SwapZoneOwners()
{
	local int i;
	for (i=0;i<MobileInputZones.Length;i++)
	{
		MobileInputZones[i].InputOwner = self;
	}
}

/**
 * The SeqEvent's from the level's kismet will have their RegisterEvent function called before the inputzones are
 * configured.  So just this once, have all of them try again.
 */
event RefreshKismetLinks()
{
	local array<SequenceObject> MobileInputSeqEvents;
	local Sequence GameSeq;
	local int i;

	GameSeq = WorldInfo.GetGameSequence();
	if (GameSeq != None)
	{
		// Find all SeqEvent_MobileInput objects anywhere and call RegisterEvent on them
		GameSeq.FindSeqObjectsByClass(class'SeqEvent_MobileBase', TRUE, MobileInputSeqEvents);

		for (i=0;i< MobileInputSeqEvents.Length; i++)
		{
			SeqEvent_MobileBase(MobileInputSeqEvents[i]).RegisterEvent();
		}

		// Find all SeqEvent_MobileRawInput objects anywhere and call RegisterEvent on them
		MobileInputSeqEvents.length = 0;
		GameSeq.FindSeqObjectsByClass(class'SeqEvent_MobileRawInput', TRUE, MobileInputSeqEvents);

		for (i=0;i< MobileInputSeqEvents.Length; i++)
		{
			SeqEvent_MobileRawInput(MobileInputSeqEvents[i]).RegisterEvent();
		}


	}
}

/**
 * Adds a listen to the mobile handler list.
 *
 * @param Handler the MobileMotion sequence event to add to the handler list
 */
function AddKismetEventHandler(SeqEvent_MobileBase NewHandler)
{
	local int i;
	//`log("Adding Mobile Kismet handler " @ NewHandler.Name);

	// More sure this event handler isn't already in the array

	for (i=0;i<MobileSeqEventHandlers.Length;i++)
	{
		if (MobileSeqEventHandlers[i] == NewHandler)
		{
			return;	// Already Registered
		}
	}

	// Look though the array and see if there is an empty sport.  These empty sports
	// can occur when a kismet sequence is streamed out.

	for	(i=0;i<MobileSeqEventHandlers.Length;i++)
	{
		if (MobileSeqEventHandlers[i] == none)
		{
			MobileSeqEventHandlers[i] = NewHandler;
			return;
		}
	}

	MobileSeqEventHandlers.AddItem(NewHandler);
}

/**
 * Adds a raw listen to the mobile handler list.
 *
 * @param Handler the MobileMotion sequence event to add to the handler list
 */
function AddKismetRawInputEventHandler(SeqEvent_MobileRawInput NewHandler)
{
	local int i;
	//`log("ZONE: Adding Mobile Kismet Raw input handler " @ NewHandler.Name);

	// More sure this event handler isn't already in the array

	for (i=0;i<MobileRawInputSeqEventHandlers.Length;i++)
	{
		if (MobileRawInputSeqEventHandlers[i] == NewHandler)
		{
			return;	// Already Registered
		}
	}

	// Look though the array and see if there is an empty sport.  These empty sports
	// can occur when a kismet sequence is streamed out.

	for	(i=0;i<MobileRawInputSeqEventHandlers.Length;i++)
	{
		if (MobileRawInputSeqEventHandlers[i] == none)
		{
			MobileRawInputSeqEventHandlers[i] = NewHandler;
			return;
		}
	}

	MobileRawInputSeqEventHandlers.AddItem(NewHandler);
}

/**
 * Search for zone in the list and return it if found
 *
 * @param ZoneName	- The name of the Mobile Input Zone we are looking for
 * @returns a zone.
 */
function MobileInputzone FindZone(string ZoneName)
{
	local int i;
	for (i=0;i<MobileInputZones.Length;i++)
	{
		if (MobileInputZones[i].Name == Name(ZoneName))
		{
			return MobileInputZones[i];
		}
	}

	return none;
}

/**
 * Searchings the zone array for a zone and returns it if found.  Otherwise add it and return the new zone
 *
 * @param ZoneName	- The name of the Mobile Input Zone we are looking for
 * @returns a zone.
 */
function MobileInputZone FindorAddZone(string ZoneName)
{
	local MobileInputZone Zone;
	local class<MobileInputZone> ClassType;
	local int ClassIndex;

	Zone = FindZone(ZoneName);
	if (Zone == None)
	{
		ClassType = class'MobileInputZone';

		// Search for the class type that is associated with ZoneName in the ini file.
		for (ClassIndex = 0; ClassIndex < MobileInputZoneClasses.length; ClassIndex++)
		{
			if (ZoneName == MobileInputZoneClasses[ClassIndex].Name)
			{
				ClassType = MobileInputZoneClasses[ClassIndex].ClassType;
				break;
			}
		}
		Zone = new(none,ZoneName) ClassType;
		Zone.InputOwner = self;

		MobileInputZones.AddItem(Zone);
	}
	return Zone;
}

function bool HasZones()
{
	return (MobileInputGroups.Length>0 && CurrentMobileGroup < MobileInputGroups.Length);
}

function array<MobileInputZone> GetCurrentZones()
{
	return MobileInputGroups[CurrentMobileGroup].AssociatedZones;
}

exec function ActivateInputGroup(string GroupName)
{
	local int i;
	for (i=0;i<MobileInputGroups.Length;i++)
	{
		if (MobileInputGroups[i].GroupName == GroupName)
		{
			CurrentMobileGroup = i;
			return;
		}
	}

	`log("Attempted to activate a mobile input group" @ GroupName @ "that did not exist.");
}

/**
 * Switch to the input config with the specified group name
 */
exec function SetMobileInputConfig(string GroupName)
{
	local int NewConfig;
	for( NewConfig=0; NewConfig<MobileInputGroups.Length; NewConfig++ )
	{
		if( MobileInputGroups[NewConfig].GroupName ~= GroupName )
			break;
	}
	if( NewConfig < MobileInputGroups.Length )
	{
		CurrentMobileGroup = NewConfig;
		//`Log("MobileInputConfig="$CurrentMobileGroup);
	}
	else
	{
		`Warn("Could not find a MobileInputGroup called" @ GroupName);
	}
}

// **********************************************************************************
// Menu System
// **********************************************************************************

/**
 *  Call this function to open a menu scene.
 *
 * @param SceneClass - The class of the menu scene to open.
 * @param Mode - Optional string that lets the opener pass extra information to the scene
 */
event MobileMenuScene OpenMenuScene(class<MobileMenuScene> SceneClass, optional string Mode)
{

	local MobileMenuScene Scene;
	local Vector2D ViewportSize;

	if (SceneClass != none)
	{
		// We have the menu scene, create it.

		Scene = new(outer) SceneClass;
		if (Scene != none)
		{
			//`log("### OpenMenuScene "@SceneClass);
			LocalPlayer(Outer.Player).ViewportClient.GetViewportSize(ViewportSize);
			Scene.InitMenuScene(self, ViewportSize.X, ViewportSize.Y,true);
			MobileMenuStack.InsertItem(0,Scene);
			Scene.Opened(Mode);
			Scene.MadeTopMenu();
			return Scene;
		}
		else
		{
			`log("Could not create menu scene " $ SceneClass);
		}
	}

	return none;

}

/**
 * Call this function to close a menu scene.  Remove it from the stack and notify the scene/etc.
 *
 * @param SceneToClose - The actual scene to close.
 */
event CloseMenuScene(MobileMenuScene SceneToClose)
{
	local int i,idx;
	local bool bClosedTopMenu;

	// Check to make sure the Scene wants to let itself close
	if (SceneToClose.Closing())
	{
		idx = -1;
		// Find the scene in the stack
		for (i=0;i<MobileMenuStack.Length;i++)
		{
			if (MobileMenuStack[i] == SceneToClose)
			{
				idx = i;
				break;
			}
		}

		if (idx>=0)
		{
			// did we just remove the top menu?
			bClosedTopMenu = (idx == 0);
			MobileMenuStack.Remove(idx,1);
			SceneToClose.Closed();

			// if we closed the top menu, send a MadeTopMenu to the new top menu
			if (bClosedTopMenu)
			{
				if (MobileMenuStack.length > 0)
				{
					MobileMenuStack[0].MadeTopMenu();
				}
			}
		}
	}
}

/**
 * Call this function to close all menus, used to "restart" the stack
 */
event CloseAllMenus()
{
	while (MobileMenuStack.length > 0)
	{
		CloseMenuScene(MobileMenuStack[MobileMenuStack.length -1]);
	}
}

/**
 * Start the rendering chain for the UI Scenes
 *
 * @param Canvas - The canvas for drawing
 */
event RenderMenus(Canvas Canvas,float RenderDelta)
{
	local int i;
	Canvas.Reset();

	if (bDisableSceneRender)
	{
		return;
	}

	for (i = MobileMenuStack.Length-1; i >= 0; i--)
	{
		MobileMenuStack[i].RenderScene(Canvas,RenderDelta);
	}
}

/**
 * We need a PreClientTravel to clean up the menu system.
 */
function PreClientTravel( string PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{
	local int i;
	Super.PreClientTravel(PendingURL, TravelType, bIsSeamlessTravel);
	for (i = MobileMenuStack.Length-1; i >= 0; i--)
	{
		MobileMenuStack[i].Closed();
	}
}

exec function SceneRenderToggle()
{
	bDisableSceneRender = !bDisableSceneRender;
}

exec function MobileMenuCommand(string MenuCommand)
{
	local int i;
	for (i = 0; i < MobileMenuStack.Length; i++)
	{
		if (MobileMenuStack[i].MobileMenuCommand(MenuCommand))
		{
			return;
		}
	}
}

/**
 * Opens a menu by class
 *
 * @Param MenuClassName - the name of the class to open
 */
exec function MobileMenuScene OpenMobileMenu(string MenuClassName)
{
	local class<MobileMenuScene> MenuClass;

	MenuClass = class<MobileMenuScene>(DynamicLoadObject(MenuClassName,class'class'));
	if (MenuClass != none)
	{
		return OpenMenuScene(MenuClass);
	}
	return none;
}

/**
 * Opens a menu by class and passes extra info to the scene
 *
 * @Param MenuClassName - the name of the class to open - REQUIRES QUOTES!
 * @Param Mode - the extra mode information to pass to the scene (two strings in OpenMobileMenu above breaks a.b for class names!)
 */
exec function MobileMenuScene OpenMobileMenuMode(string MenuClassName, string Mode)
{
	local class<MobileMenuScene> MenuClass;

	MenuClass = class<MobileMenuScene>(DynamicLoadObject(MenuClassName,class'class'));
	if (MenuClass != none)
	{
		return OpenMenuScene(MenuClass, Mode);
	}
	return none;
}

// @DEBUG - Remove if you wish to see all touch events
/*
event AddTouchDebug(int Handle, TouchDataEvent Event)
{
	local string s;

	if (Event.EventType == Touch_Ended && Event.Location.Y > 400)
	{
		TouchDebug.Remove(0,TouchDebug.Length);
		return;
	}

	if (Event.EventType == Touch_Moved)
	{
		return;

	}
	s = "TOUCH: Hander="@Handle @ "Event=" @ Event.EventType @ Event.Location.X @ Event.Location.Y;
	AddTouchDebugMsg(s);
}

event AddTouchDebugMsg(string s)
{
	local int i;
	i = TouchDebug.Length;
	TouchDebug.Length = i+1;
	TouchDebug[i] = s;

	if (TouchDebug.Length>20)
	{
		TouchDebug.Remove(0,1);
	}
}

function DrawTouchDebug(canvas Canvas)
{
	local int i;
	local int y;

	y = 80;
	for (i=0;i<TouchDebug.Length;i++)
	{
		DrawDebugString(Canvas, TouchDebug[i],y);
		y+=18;
	}
}

function DrawDebugString(Canvas Canvas, string Msg, float y)
{
	Canvas.SetPos(0,Y);
	Canvas.SetDrawColor(0,0,0,255);
	Canvas.DrawText(Msg);
	Canvas.SetPos(1,Y+1);
	Canvas.SetDrawColor(255,255,255,255);
	Canvas.DrawText(Msg);
}
*/
