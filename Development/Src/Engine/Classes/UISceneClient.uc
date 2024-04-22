/**
 * Serves as the interface between a UIScene and scene owners.  Provides scenes with all
 * data necessary for operation and routes rendering to the scenes.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UISceneClient extends UIRoot
	native(UserInterface)
	abstract
	inherits(FExec)
	transient;

/** the viewport to use for rendering scenes */
var const transient	native					pointer				RenderViewport{FViewport};

/**
 * the location of the mouse
 *
 * @fixme splitscreen
 */
var const transient							IntPoint			MousePosition;

/**
 * Manager all persistent global data stores.  Set by the object that creates the scene client.
 */
var	const transient							DataStoreClient		DataStoreManager;

/**
 * Stores the 3D projection matrix being used to render the UI.
 */
var	const transient							matrix				CanvasToScreen;
var	const transient							matrix				InvCanvasToScreen;


/** Post process chain to be applied when rendering UI Scenes */
var transient								PostProcessChain	UIScenePostProcess;
/** if TRUE then post processing is enabled using the UIScenePostProcess */
var transient								bool				bEnablePostProcess;

/**
 * Used to limit which scenes should be considered when determining whether the UI should be considered "active"
 * Represented in C++ as enums values.
 */
const	SCENEFILTER_None				=0x00000000;
/** Include the transient scene */
const	SCENEFILTER_IncludeTransient	=0x00000001;
/** Consider only scenes which can process input */
const	SCENEFILTER_InputProcessorOnly	=0x00000002;
/** Consider only scenes which require the game to be paused */
const	SCENEFILTER_PausersOnly			=0x00000004;
/** Consider only scenes which support 3D primitives rendering */
const	SCENEFILTER_PrimitiveUsersOnly	=0x00000008;
/** Only consider scenes which render full-screen */
const	SCENEFILTER_UsesPostProcessing	=0x00000010;
/** Include only those scenes which can receive focus (i.e. bNeverFocus=false) */
const	SCENEFILTER_ReceivesFocus		=0x00000020;
/** Any scene */
const	SCENEFILTER_Any					=0xFFFFFFFF;

`define		DECLARE_SCENEFILTER_TYPE(type) `{type}=UCONST_`{type},
cpptext
{

	/**
	 * Used to limit which scenes should be considered when determining whether the UI should be considered "active"
	 */
	enum ESceneFilterTypes
	{
		`DECLARE_SCENEFILTER_TYPE(SCENEFILTER_None)

		/** Include the transient scene */
		`DECLARE_SCENEFILTER_TYPE(SCENEFILTER_IncludeTransient)

		/** Consider only scenes which can process input */
		`DECLARE_SCENEFILTER_TYPE(SCENEFILTER_InputProcessorOnly)

		/** Consider only scenes which require the game to be paused */
		`DECLARE_SCENEFILTER_TYPE(SCENEFILTER_PausersOnly)

		/** Consider only scenes which support 3D primitives rendering */
		`DECLARE_SCENEFILTER_TYPE(SCENEFILTER_PrimitiveUsersOnly)

		/** Only consider scenes which render full-screen */
		`DECLARE_SCENEFILTER_TYPE(SCENEFILTER_UsesPostProcessing)

		/** Include only those scenes which can receive focus (i.e. bNeverFocus=false) */
		`DECLARE_SCENEFILTER_TYPE(SCENEFILTER_ReceivesFocus)

		/** Include ANY scene, regardless of feature set */
		`DECLARE_SCENEFILTER_TYPE(SCENEFILTER_Any)
	};

	/* =======================================
		FExec interface
	======================================= */
	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar);

	/* =======================================
		UUISceneClient interface
	======================================= */

	/**
	 * Performs any initialization for the UISceneClient.
	 */
	virtual void InitializeClient( );

	/**
	 * Assigns the viewport that scenes will use for rendering.
	 *
	 * @param	inViewport	the viewport to use for rendering scenes
	 */
	virtual void SetRenderViewport( FViewport* SceneViewport );

	/**
	 * Provides the scene client with a way to apply a platform input type other than the actual input type being used.
	 * Primarily for simulating platforms in the editor.
	 *
	 * @param	OwningPlayer		the player to use for determining the real platform input type, if necessary.
	 * @param	SimulatedPlatform	receives the value of the platform that should be used.
	 *
	 * @return	TRUE if the scene client wants to override the current platform input type.
	 */
	virtual UBOOL GetSimulatedPlatformInputType( BYTE& SimulatedPlatform ) const { return FALSE; }

	/**
	 * Returns true if there is an unhidden fullscreen UI active
	 *
	 * @param	Flags	modifies the logic which determines whether the UI is active
	 *
	 * @return TRUE if the UI is currently active
	 */
	virtual UBOOL IsUIActive( DWORD Flags=SCENEFILTER_Any ) const PURE_VIRTUAL(UUISceneClient::IsUIActive,return FALSE;);

	/**
	 * Returns true if the UI scenes should be rendered with post process
	 *
	 * @return TRUE if post process is enabled for any of the UI scenes
	 */
	virtual UBOOL UsesPostProcess() const;
}

/**
 * Returns true if there is an unhidden fullscreen UI active
 *
 * @param	Flags	a bitmask of values which alter the result of this method;  the bits are derived from the ESceneFilterTypes
 *					enum (which is native-only); script callers must pass these values literally
 *
 * @return TRUE if the UI is currently active
 */
native final noexportheader function bool IsUIActive( optional int Flags=SCENEFILTER_Any ) const;


/**
 * Loads the skin package containing the skin with the specified tag, and sets that skin as the currently active skin.
 * @todo
 */
//native final function SetActiveSkin( Name SkinTag );

/**
 * Returns the current canvas to screen projection matrix.
 *
 * @param	Widget	if specified, the returned matrix will include the widget's tranformation matrix as well.
 *
 * @return	a matrix which can be used to project 2D pixel coordines into 3D screen coordinates. ??
 */
native final function matrix GetCanvasToScreen() const;

/**
 * Returns the inverse of the local to world screen projection matrix.
 *
 * @param	Widget	if specified, the returned matrix will include the widget's tranformation matrix as well.
 *
 * @return	a matrix which can be used to transform normalized device coordinates (i.e. origin at center of screen) into
 *			into 0,0 based pixel coordinates. ??
 */
native final function matrix GetInverseCanvasToScreen() const;

/**
 * Called when the scene client is first initialized.
 */
event InitializeSceneClient();

DefaultProperties
{
	// enable post processing of UI by default
	bEnablePostProcess=True
}
