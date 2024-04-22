/*=============================================================================
	GameStatsVisualizer.h: Window for working with game stats data
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if _MSC_VER
#pragma once
#endif

#ifndef __GAMESTATSBROWSER_H__
#define __GAMESTATSBROWSER_H__

#include "GameFrameworkGameStatsClasses.h"
#include "UnrealEdGameStatsClasses.h"



/** 
 * Base class for all visualizer option dialogs
 * Opens the visualizer's specified .xrc file entry nd appends an OK button to the bottom.
 * Child classes are responsible for overriding create and attaching custom event handling
 */
class WxVisualizerOptionsDialog : public wxDialog
{
public:
	WxVisualizerOptionsDialog() {}
	virtual ~WxVisualizerOptionsDialog();

	/**
	* Create the dialog with the given visualizer
	* @param InVisualizer - Visualizer to wrap (must match the appropriate dialog type)
	*/
	virtual bool Create(class UGameStatsVisualizer* InVisualizer);

	/**
	* Callback when the OK button is clicked, shuts down the options panel
	* @param In - Properties of the event triggered
	*/
	void OnOK( wxCommandEvent& In );

	/**
	* Callback when the window is closed, shuts down the options panel
	* @param In - Properties of the event triggered
	*/
	void OnClose(wxCloseEvent& In);

	/**
	 * Get the visualizer associated with this options dialog
	 * @return The visualizer associated with the dialog
	 */
	class UGameStatsVisualizer* GetVisualizer() { return Visualizer; }

protected:

	UGameStatsVisualizer* Visualizer;

	DECLARE_EVENT_TABLE()
};

/** 
* Heatmap visualizer option dialogs
* adds the ability to adjust the min/max scales for which
* the heatmap draws
*/
class WxHeatmapOptionsDialog : public WxVisualizerOptionsDialog
{
public:
	WxHeatmapOptionsDialog() {}
	virtual ~WxHeatmapOptionsDialog() {}

	/**
	* Create the dialog with the given visualizer
	* @param InVisualizer - Visualizer to wrap (type UHeatmapVisualizer)
	*/
	virtual bool Create(class UGameStatsVisualizer* InVisualizer);

	/**
	* Callback when the min density slider is manipulated, regenerates the heatmap texture with new scaling
	* @param In - Properties of the event triggered
	*/
	void OnMinDensityFilterUpdated( wxScrollEvent& In );

	/**
	* Callback when the max density slider is manipulated, regenerates the heatmap texture with new scaling
	* @param In - Properties of the event triggered
	*/
	void OnMaxDensityFilterUpdated( wxScrollEvent& In );

	/**
	* Callback when the heat radius slider is manipulated, regenerates the heatmap texture with new scaling
	* @param In - Properties of the event triggered
	*/
	void OnHeatRadiusUpdated( wxScrollEvent& In );

	/**
	* Callback when the pixel density slider is manipulated, regenerates the heatmap texture with new scaling
	* @param In - Properties of the event triggered
	*/
	void OnPixelDensityUpdated( wxScrollEvent& In );

	/**
	* Callback when user pushes the take screenshot button
	* @param In - Properties of the event triggered
	*/
	void OnScreenshotClicked( wxCommandEvent& In );

protected:

	DECLARE_EVENT_TABLE()
};

/**
 *  Hit proxy for game stats visualization
 *  Routes the rendered item selected to the proper visualizer
 */
struct HGameStatsHitProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HGameStatsHitProxy, HHitProxy);

	/** Reference to the visualizer that generated the hit proxy */
	class UGameStatsVisualizer* Visualizer;

	/** Index relevant to the visualizer that generated the hit proxy */
	INT	StatIndex;

	HGameStatsHitProxy(class UGameStatsVisualizer* InVisualizer, INT InStatIndex):
		HHitProxy(HPP_UI),
		Visualizer(InVisualizer),
		StatIndex(InStatIndex) {}
};

/*-----------------------------------------------------------------------------
	WxGameStatsPopUpMenu
-----------------------------------------------------------------------------*/
class WxGameStatsPopUpMenu : public wxMenu
{
public:
	WxGameStatsPopUpMenu(WxGameStatsVisualizer* GameStatsBrowser);
	~WxGameStatsPopUpMenu();
};

/*
*  Container for an individual visualizer and its filters
*/
class WxVisualizerTab : public wxNotebookPage
{
public:

	WxVisualizerTab();
	virtual ~WxVisualizerTab();

	/** Unique ID for this tab in the notebook containing it */
	INT TabID;

	/** Access to the main window frame */
	class WxGameStatsVisualizer* Parent;

	/** Panel loaded from .xrc containing the child widgets */
	class wxPanel* VisualizerPanel;

	/** Visualizer to control with this tab's filters/options */
	class UGameStatsVisualizer* Visualizer;

	/** Control for displaying all visualizers available for use */
	class wxChoice* AvailableVisualizers;

	/** List of all gameplay events recorded in the data stream */
	class wxListCtrl* GameplayEventsListCtrl;

	/** List of players in the data stream */
	class wxListCtrl* PlayerListCtrl;

	/** List of teams in the data stream */
	class wxListCtrl* TeamListCtrl;

	/** Mapping of the UI widget for event data with event ID in the DB */
	TArray<INT> GameplayEventsListUserData;
	/** Mapping of the UI widget for team data with the session that it belongs to in the DB */
	TArray<struct FSessionIndexPair> TeamListUserData;
	/** Mapping of the UI widget for player data with the session that it belongs to in the DB */
	TArray<struct FSessionIndexPair> PlayerListUserData;

	/** Current query used to generate data for the visualizer */
	FGameStatsSearchQuery Query;

	/** Flags indicating if any element of the query has changed, requiring an update */
	INT QueryFlags;

	/** Number of results from the last query run */
	INT ResultCount;		

	/** Is the visualizer drawing to the editor viewports (all other updates still occur) */
	UBOOL bIsEnabled;

	/** Second step of creation, required to initialize the window */
	UBOOL Create(class wxWindow* InParent, class UGameStatsVisualizer* InVisualizer);

	/** Set all the visualizer tab widgets back to initial state */
	void Reset();

	/** 
	 *  Get the string to display in the tab label area
	 */
	const FString& GetTabCaption() const;

	/** 
	 *  Poll all controls related to queries to fill in the required search query struct
	 *	@param  SearchQuery - empty search query object to fill in
	 */
	void GenerateSearchQuery(FGameStatsSearchQuery& SearchQuery);

	/** Initialize the event lists control */
	void InitEventsListControl();
	
	/*
	 *   Update the events list control with all known events in the database
	 *   @param StatsDB - the database to query for all recorded events
	 */
	void UpdateEventsListControl(const FString& SessionID, TArray<FGameplayEventMetaData>& GameplayEvents);

	/** Initialize the team list controls */
	void InitTeamListControl();
	
	/**
	 *	Update the team list control given team metadata, removes previous data
	 *  @param TeamList - array of team metadata to include in the control
	 */
	void UpdateTeamListControl(const FString& SessionID, TArray<FTeamInformation>& TeamList);

	/** Initialize the controls related to the player list */
	void InitPlayerListControl();
	/**
	 * Update the player list controls, removing previous data
	 * @param SessionID - session this player list is relevant to
	 * @param PlayerList - list of player metadata to display in the control
	 */
	void UpdatePlayerListControl(const FString& SessionID, TArray<FPlayerInformation>& PlayerList);

	/**
	 *   Called when any parameters of the query have changed, refreshes the visualizer
	 */
	void QueryUpdate(DOUBLE& OutQueryTime, DOUBLE& OutVisitTime);

	/************************************************************************/
	/* EVENTS                                                               */
	/************************************************************************/

	/** 
	* Callback on whole window size adjustments
	* @param In - Properties of the event triggered
	*/
	void OnSize( wxSizeEvent& In );

	/**
	* Callback when the window is closed, shuts down the visualizer panel
	* @param In - Properties of the event triggered
	*/
	void OnClose( wxCloseEvent& In );

	/**
	* Callback when the tab is enabled/disabled by the user
	* @param In - Properties of the event triggered
	*/
	void OnEnabled( wxCommandEvent& In );

	/**
	* Callback when options button is clicked
	* @param In - Properties of the event triggered
	*/
	void OnOptionsClicked( wxCommandEvent& In );

	/**
	* Called when an item in the stats event list is selected
	* @param In - Properties of the event triggered
	*/
	void OnEventsListItemSelected( wxListEvent& In );

	/**
	* Called when an item in the team list is selected 
	* @param In - Properties of the event triggered
	*/
	void OnTeamListItemSelected( wxListEvent& In );

	/** 
	* Called when an item in the player list is double-clicked
	* @param In - Properties of the event triggered
	*/
	void OnPlayerListItemActivated( wxListEvent& In );

	/** 
	* Called when an item in the player list is selected
	* @param In - Properties of the event triggered
	*/
	void OnPlayerListItemSelected( wxListEvent& In );

	/** 
	* Called when a column button is clicked on the task list 
	* @param In - Properties of the event triggered
	*/
	void OnPlayerListColumnButtonClicked( wxListEvent& In );

	/** 
	* Called when a new filter is selected in the choice box
	* @param In - Properties of the event triggered
	*/
	void OnAvailableVisualizerChoiceSelected( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/** Main gameplay statistics viewport window */
class WxGameStatsVisualizer : 
	public WxBrowser, 
	public FSerializableObject,		// Garbage collection for UObjects
	public FTickableObject			// Tickable, so we can "play"
{
	DECLARE_DYNAMIC_CLASS( WxGameStatsVisualizer );

public:

	WxGameStatsVisualizer( );
	virtual ~WxGameStatsVisualizer();

    //Convenience for the tab's access
	friend class WxVisualizerTab;

	/**
	* Forwards the call to our base class to create the window relationship.
	* Creates any internally used windows after that
	*
	* @param	DockID			The unique id to associate with this dockable window
	* @param	FriendlyName	The friendly name to assign to this window
	* @param	Parent			The parent of this window (should be a Notebook)
	*/
	virtual void Create( INT DockID, const TCHAR* FriendlyName, wxWindow* Parent );

	/** Tells the browser manager whether or not this browser can be cloned */
	virtual UBOOL IsClonable()
	{
		// Only allow one instance of a game stats window.
		return FALSE;
	}

	/** Called when the browser is getting activated (becoming the visible window in it's dockable frame). */
	virtual void Activated();

	/** Tells the browser to update itself */
	virtual void Update();

	/** Returns the key to use when looking up values */
	virtual const TCHAR* GetLocalizationKey() const
	{
		return TEXT( "GameStatsVisualizer_Caption" );
	}

	/** Called when a new map is loaded */
	void OnLoadMap();
	/** Called when a map is closed */
	void OnCloseMap();
	/** Called by editor to render stats graphics in 2D viewports. */
	void RenderStats(FEditorLevelViewportClient* ViewportClient, const FSceneView* View, FCanvas* Canvas, ELevelViewportType ViewportType);
	/** Called by editor to render stats graphics in perspective viewports. */
	void RenderStats3D(const FSceneView* View,class FPrimitiveDrawInterface* PDI, ELevelViewportType ViewportType);
	/** Called by editor when mouse moves */
	void MouseMoved(FEditorLevelViewportClient* ViewportClient, INT X, INT Y);
	/** Called by editor when key pressed. */
	void InputKey(FEditorLevelViewportClient* ViewportClient, FName Key,EInputEvent Event);
	/** 
	 * Adds a string entry into the output window of the visualizer
	 * @param Output - string to display in the window
	 */
	void AddVisualizerOutput(const FString& Output);

	/**
	* Used to determine if an object should be ticked when the game is paused.
	* Defaults to false, as that mimics old behavior.
	*
	* @return TRUE if it should be ticked when paused, FALSE otherwise
	*/
	virtual UBOOL IsTickableWhenPaused() const
	{
		return TRUE;
	}

	/**
	* Used to determine whether the object should be ticked in the editor.  Defaults to FALSE since
	* that is the previous behavior.
	*
	* @return	TRUE if this tickable object can be ticked in the editor
	*/
	virtual UBOOL IsTickableInEditor() const
	{
		return TRUE;
	}

	/**
	* Used to determine whether an object is ready to be ticked. This is 
	* required for example for all UObject derived classes as they might be
	* loaded async and therefore won't be ready immediately.
	*
	* @return	TRUE if class is ready to be ticked, FALSE otherwise.
	*/
	virtual UBOOL IsTickable() const
	{
		return TRUE;
	}

	/**
	* Called from within UnLevTic.cpp after ticking all actors or from
	* the rendering thread (depending on bIsRenderingThreadObject)
	*
	* @param DeltaTime	Game time passed since the last call.
	*/
	virtual void Tick( FLOAT DeltaTime );

	/**
	 * Listens for CALLBACK_MapChange events.  Clears all references to actors in the current level.
	 */
	virtual void Send(ECallbackEventType InType, DWORD Flag);

	/** FSerializableObject: Serialize object references for garbage collector */
	virtual void Serialize( FArchive& Ar );

private:

	/** The panel root read out of the xRC file */
	class wxPanel* MainBrowserPanel;

	/** Array of all stats sessions currently in the database */
	TArray<FString> GameSessionsInDB;

	/** The source of all game stats data*/
	class UGameStatsDatabase* GameStatsDB;

	/** Force an update of all visualizer tab queries */
	UBOOL bForceQueryUpdate;

	/** Control for displaying all date filters */
	class wxChoice* DateFilterList;

	/** Control for displaying all game type filters */
	class wxChoice* GameTypeFilterList;

	/** List of game info in the data stream */
	class wxListCtrl* GameSessionInfoCtrl;

	/** All the available visualizers */
	class wxNotebook* VisualizerTabRoot;

	/** Time shuttle slider for manipulating the data view */
	class wxSlider* TimeScrubSlider;

	/** Spin control for setting the start time in the query */
	class wxSpinCtrl* StartSpinner;

	/** Spin control for setting the end time in the query */
	class wxSpinCtrl* EndSpinner;

	/** Array of all game stat visualizer class required to construct the visualizer itself (shared by all tabs) */
	TArray<UClass*> VisualizerClasses;

	/** Text output window for visualizer */
	class wxTextCtrl* VisualizerOutputCtrl; 

	/** Have all the controls been initialized (TRUE after CreateControls()) */
	UBOOL bAreControlsInitialized;

	/** Is the database setup for this map */
	UBOOL bIsDBInitialized;

	/** Are we currently stepping time */
	UBOOL bSteppingTime;
	/** Current time in the playback */
	FLOAT CurrentTime;

	/** Visualizer under cursor */
	class UGameStatsVisualizer* TooltipVisualizerHit;
	/** Tooltip information to render */
	INT	ToolTipStatIndex;
	/** Position to draw the current tooltip */
	INT ToolTipX;
	/** Position to draw the current tooltip */
	INT ToolTipY;
	/** Active viewport for tooltip drawing */
	FEditorLevelViewportClient* ToolTipViewport;


	// Bitmap resources
	WxBitmap StopB;
	WxBitmap PlayB;

    /** 
 	 * Given a session and heatmap query description, generate heatmap image files
 	 * @param SessionID - session of interest
   	 * @param HeatmapQueries - definition of individual heatmap queries to run
 	 */
	void CreateHeatmaps(const FString& SessionID, const TArray<FHeatmapQuery>& HeatmapQueries);

	/************************************************************************/
	/*  UI window/control functionality                                     */
	/************************************************************************/

	/** 
	 * Callback on whole window size adjustments
	 * @param In - Properties of the event triggered
	 */
	void OnSize( wxSizeEvent& In );

	/**
	 * Callback when the window is closed, shuts down the visualizer panel
	 * @param In - Properties of the event triggered
	 */
	void OnClose( wxCloseEvent& In );

	/** Creates all controls related to the browser window */
	void CreateControls();

	/** Resets all widget controls to their default state */
	void InitAllControls();

	/** Initialize the controls related to top level filters */
	void InitGameStatsFilterControl();

	/** @return the Date filter in current use */
	GameStatsDateFilters GetDateFilter();

	/** @return the GameType filter in current use */
	FString GetGameTypeFilter();

	/**
	 *   Update the game types filter
	 * @param GameTypes - the gametypes to populate the filter
	 */
	void UpdateGameTypesFilterControl(const TArray<FString>& GameTypes);

	/** Initialize the controls related to the game session info list */
	void InitGameSessionInfoControl();

	/** 
	 * Update the game session control with information for the given game session
	 * @param GameSessionInfo - info to add to the control
	 */
	void UpdateGameSessionInfo(TArray<struct FGameSessionInformation>& GameSessionInfoList);

	/**
	 *   Get the list of session IDs current selected in the UI
	 *   @param OutArray - array of sessions IDs selected
	 */
	void GetSelectedSessions(TArray<FString>& OutArray);

	/*
	 * Called when the user selects a date filter from the list
	 * @param In - Properties of the event triggered
	 */
	void OnDateListSelected( wxCommandEvent& In );

	/*
	 * Called when the user selects a game type filter from the list
	 * @param In - Properties of the event triggered
	 */
	void OnGameTypeListSelected( wxCommandEvent& In );

	/*
	 * Called when the user selects a game session from the list
	 * @param In - Properties of the event triggered
	 */
	void OnGameSessionListItemSelected( wxListEvent& In );

	/*
	 * Called when the user double clicks a game session from the list
	 * @param In - Properties of the event triggered
	 */
	void OnGameSessionListItemActivated( wxListEvent& In );

	/*
	 * Called when the user right clicks a game session from the list
	 * pops up a menu of options
	 * @param In - Properties of the event triggered
	 */
	void OnGameSessionListRightClick( wxListEvent& In );

	/*
	 * Called when the user selects a right click menu option
	 * @param In - Properties of the event triggered
	 */
	void OnGameSessionListHandleCommand( wxCommandEvent &In );

	/** Initialize controls related to the available visualizers */
	void InitVisualizerControls();

	/**
	 * Set the label on a given visualizer tab
	 * @param TabID - the unique ID for the tab to set the label on
	 * @param Label - the string to display
	 */
	void SetVisualizerTabLabel(INT TabID, const FString& Label);
	
	/** 
	 * Called when the visualizer tab changes
	 * @param In - Properties of the event triggered
	 */
	void OnNotebookPageChanged( wxNotebookEvent& In );

	/** Initialize all the controls related to time manipulation */
	void InitTimeScrubControls();

	/** 
	 * Update the time manipulation controls one a database is associated with the window 
	 * @param SessionStart - Time to set the start time spinner to (and implictly defines the slider range)
	 * @param SessionEnd - Time to set the end time spinner to (and implictly defines the slider range)
	 */
	void UpdateTimeScrubControls(INT SessionStart, INT SessionEnd);

	/** 
	 * Set the enable state of the time controls (spinners/sliders/play/stop)
	 *   @param bEnabled - TRUE to enable the controls, FALSE to disable
	 */
	void EnableTimeControls(UBOOL bEnabled);

	/** 
	 * Event handler for clicking on the up/down buttons of the Start Time search criteria
	 * @param In - Properties of the event triggered
	 */
	void OnDurationStartFilterUpdated( wxSpinEvent& In );
	/** 
	 * Event handler for clicking on the up/down buttons of the End Time search criteria
	 * @param In - Properties of the event triggered
	 */
	void OnDurationEndFilterUpdated( wxSpinEvent& In );

	/** 
	* Called at the end of an action against the time control slider
	* @param In - event callback data
	*/
	void OnTimeScrubUpdate( wxScrollEvent& In );
	/** 
	* Called during mouse held movement of the thumb icon
	* @param In - event callback data
	*/
	void OnTimeScrubTrack( wxScrollEvent& In );

	/** 
	 *  Adjust the start/end spinners and slider position to represent a given time
	 * @param TimePosition - the middle time position the spinners/slider should represent
	 */
	void SetTimeControls( INT TimePosition );

	/** 
	* Called when the play button is toggled
	* @param In - event callback data
	*/
	void OnPlayToggle( wxCommandEvent& In );
	/** 
	* Called when the stop button is toggled
	* @param In - event callback data
	*/
	void OnStopToggle( wxCommandEvent& In );

	/** 
	 * Return the time adjusted value for the current database times requested 
	 * @param StartTime - out value for start time in database unit
	 * @param EndTime - out value for end time in database units
	 */
	void GetCurrentStartEndWindow(INT& StartTime, INT& EndTime);

	/** Initialize the controls related to the output window */
	void InitGameStatsOutputControl();

	/************************************************************************/
	/*  Database query functionality                                        */
	/************************************************************************/

	/** 
	 * Collate the data from files on disk as well as the remote database 
	 * @param LoadedMap - map name to filter the database on
	 */
	void InitDB(const FString& LoadedMap);

	/** 
	 *	Fill out the game session control using filters specified by the user
	 * @param DateFilter - date range to filter
	 * @param GameTypeFilter - string representing GameClass
	 */
	void PopulateGameSessions(GameStatsDateFilters DateFilter, const FString& GameTypeFilter);

	/** Update all currently active queries with the current time settings */
	void UpdateAllQueries();
	
	/** 
	 * Query the connected database, passing relevant data to the visualizer expected to handle the data
	 * @param SearchQuery - the query to run on the database
	 * @param Visualizer - the visualizer to receive/process the data
	 * @param TotalResults - out value of total search results found
	 * @param TotalQueryTime - out value of time taken to search the database
	 * @param TotalVisitTiem - out value of time taken to process the data by the visualizer
	 * @return TRUE is the query was successful (Visualizer has something to work with) and FALSE otherwise
	 */
	UBOOL QueryDatabase(const struct FGameStatsSearchQuery& SearchQuery, class UGameStatsVisualizer* Visualizer, INT& TotalResults, DOUBLE& TotalQueryTime, DOUBLE& TotalVisitTime);

	DECLARE_EVENT_TABLE()
};

#endif // __GAMESTATSBROWSER_H__


