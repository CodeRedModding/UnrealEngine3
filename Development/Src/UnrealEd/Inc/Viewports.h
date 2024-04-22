/*=============================================================================
	Viewports.h: The viewport windows used by the editor
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __VIEWPORTS_H__
#define __VIEWPORTS_H__

class WxViewportsContainer;
class WxFloatingViewportFrame;


enum EViewportConfig
{ 
	VC_None			= -1,
	VC_2_2_Split,
	VC_1_2_Split,
	VC_1_1_SplitH,
	VC_1_1_SplitV,
	VC_Max
};

namespace EditorViewportDefs
{
	/** Default camera position for level editor perspective viewports */
	const FVector DefaultPerspectiveViewLocation( -1024.0f, 0.0f, 512.0f );

	/** Default camera orientation for level editor perspective viewports -- appRound below ICEs in VS2008 */
	const FRotator DefaultPerspectiveViewRotation( (INT)( -15.0f * (32768.f / 180.f) ), 0, 0 );
}

/**
 * The default values for a viewport within an FViewportConfig_Template.
 */
class FViewportConfig_Viewport
{
public:
	FViewportConfig_Viewport();
	virtual ~FViewportConfig_Viewport();

	ELevelViewportType ViewportType;
	EShowFlags ShowFlags;

	/** If FALSE, this viewport template is not being used within its owner config template. */
	UBOOL bEnabled;

	/** If TRUE, sets the listener position */
	UBOOL bSetListenerPosition;
};

/**
 * A template for a viewport configuration.  Holds the baseline layouts and flags for a config.
 */
class FViewportConfig_Template
{
public:
	FViewportConfig_Template();
	virtual ~FViewportConfig_Template();
	
	void Set( EViewportConfig InViewportConfig );

	/** The description for this configuration. */
	FString Desc;

	/** The viewport config this template represents. */
	EViewportConfig ViewportConfig;

	/** The viewport templates within this config template. */
	FViewportConfig_Viewport ViewportTemplates[4];
};



/**
 * An instance of a FViewportConfig_Template.  There is only one of these
 * in use at any given time and it represents the current editor viewport
 * layout.  This contains more information than the template (i.e. splitter 
 * and camera locations).
 */
class FVCD_Viewport : public FViewportConfig_Viewport
{
public:
	FVCD_Viewport();
	virtual ~FVCD_Viewport();

	/** Stores the sash position from the INI until we can use it in FViewportConfig_Data::Apply. */
	FLOAT SashPos;

	/** Container window for floating viewport windows.  Not used for regular non-floating viewports.  */
	class WxFloatingViewportFrame* FloatingViewportFrame;

	/** The window that holds the viewport itself. */
	class WxLevelViewportWindow* ViewportWindow;

	/** Play-In-Editor container window for this viewport, if it has one.  This will be NULL unless
	    the user has an active PIE session in this viewport. */
	class WxPIEContainerWindow* PIEContainerWindow;

	inline FVCD_Viewport operator=( const FViewportConfig_Viewport &Other )
	{
		ViewportType = Other.ViewportType;
		ShowFlags = Other.ShowFlags;
		bEnabled = Other.bEnabled;
		bSetListenerPosition = Other.bSetListenerPosition;

		return *this;
	}
};

/**Composite structure that describes how new floating viewports should be spawned*/
struct FFloatingViewportParams
{
	//ParentWxWindow The parent window
	wxWindow* ParentWxWindow;
	//Type of viewport window
	ELevelViewportType ViewportType;
	//Show flags for the new window
	EShowFlags ShowFlags;
	//Width of the window
	INT Width;
	//Height of the window
	INT Height;
	//Preferred Window Title
	FString Title;
};

class WxSplitterWindow : public wxSplitterWindow
{

public:

	// Default constructor
	WxSplitterWindow()
	: wxSplitterWindow()
	{
		OldWindows[0] = OldWindows[1] = NULL;
	}

	// Normal constructor
	WxSplitterWindow(wxWindow *parent, wxWindowID id = wxID_ANY,
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = wxSP_3D,
		const wxString& name = wxT("splitter"))
	: wxSplitterWindow(parent, id, pos, size, style, name)
	{
		OldWindows[0] = OldWindows[1] = NULL;
	}

	virtual ~WxSplitterWindow() { }

	// This will save and unsplit the windows to allow for
	// re-splitting later.
	void SaveAndUnsplit(wxWindow *toRemove);

	// Can be called after an Unsplit to re-split the window as
	// it was before the Unsplit.
	void ReSplit();

protected:

	wxWindow* OldWindows[2];

};

/**
 * Used for storing off camera data for a given viewport type.
 */
struct FViewportCameraData
{
	/** Viewport type **/
	ELevelViewportType Type;

	/** Viewport camera position **/
	FVector Location;

	/** Viewport camera rotaion **/
	FRotator Orientation;

	FViewportCameraData() {}

	FViewportCameraData(ELevelViewportType Type, FVector Location, FRotator Orientation)
	{
		this->Type = Type;
		this->Location = Location;
		this->Orientation = Orientation;
	}
};

/**
 * IFloatingViewportCallback
 *
 * Callback mechanism for floating viewport objects
 */
class IFloatingViewportCallback
{

public:

	/** Called when a floating viewport window is closed */
	virtual void OnFloatingViewportClosed( WxFloatingViewportFrame* InViewportFrame ) = 0;

};


class FViewportConfig_Data :
	public IFloatingViewportCallback
{
public:
	FViewportConfig_Data();
	virtual ~FViewportConfig_Data();

	void	SetTemplate( EViewportConfig InTemplate );
	void	Apply( WxViewportsContainer* InParent, UBOOL bFoundMaximizedViewport=FALSE );
	void	ResizeProportionally( FLOAT ScaleX, FLOAT ScaleY, UBOOL bRedraw=TRUE );
	void	ToggleMaximize( FViewport* Viewport );
	void	Layout();
	void	Save();
	void	Load( FViewportConfig_Data* InData, UBOOL bTransferFloatingViewports );
	void	SaveToINI();
	UBOOL	LoadFromINI();
	UBOOL	IsViewportMaximized();

	/**
	 * Helper function to allow setting of a viewports size explicitely
	 * @param Viewport - Viewport to resize
	 * @param InWidth - New width requested for this viewport
	 * @param InHeight - New height requested for this viewport
	 */
	void ResizeViewportToSize(FViewport* Viewport, const INT InWidth, const INT InHeight);

	/** Helper functions */
	INT		FindSplitter( wxWindow* ContainedWindow, INT *WhichWindow=NULL );

	/** IFloatingViewportCallback: Called when a floating viewport window is closed */
	virtual void OnFloatingViewportClosed( WxFloatingViewportFrame* InViewportFrame );

	/** Returns the total number of viewports */
	const INT GetViewportCount() const
	{
		return 4 + FloatingViewports.Num();
	}

	/** Returns a const reference to the viewport of the specified index */
	const FVCD_Viewport& GetViewport( const INT ViewportIndex ) const
	{
		if( ViewportIndex < 4 )
		{
			return Viewports[ ViewportIndex ];
		}

		const FVCD_Viewport* FloatingViewport = FloatingViewports( ViewportIndex - 4 );
		check( FloatingViewport != NULL );
		return *FloatingViewport;
	}

	/** Returns a reference to the viewport of the specified index */
	FVCD_Viewport& AccessViewport( const INT ViewportIndex )
	{
		if( ViewportIndex < 4 )
		{
			return Viewports[ ViewportIndex ];
		}

		FVCD_Viewport* FloatingViewport = FloatingViewports( ViewportIndex - 4 );
		check( FloatingViewport != NULL );
		return *FloatingViewport;
	}

	inline FViewportConfig_Data operator=( const FViewportConfig_Template &Other )
	{
		// NOTE: Floating viewports are not affected by this operation
		for( INT x = 0 ; x < 4 ; ++x )
		{
			Viewports[x] = Other.ViewportTemplates[x];
		}

		return *this;
	}

	/**
	 * Opens a new floating viewport window
	 *
	 * @param InNewViewportParams - The settings to use for the newly created viewport
	 * @param OutViewportIndex [out] Index of newly created viewport
	 * @param bDisablePlayInViewport Choose whether or not to disable the Play in viewport option (Button and HotKey)
	 *
	 * @return Returns TRUE if everything went OK
	 */
	UBOOL OpenNewFloatingViewport(const FFloatingViewportParams& InViewportParams, INT& OutViewportIndex, UBOOL bDisablePlayInViewport = FALSE );

	/**
	 * Helper functions to get and set the custom fov
	 */
	INT GetCustomFOV( void ) const { return CustomFOV; }
	void SetCustomFOV( const INT iCustomFOV ) { CustomFOV = iCustomFOV; }

public:

	/** The splitters windows that make this config possible. */
	TArray<WxSplitterWindow*> SplitterWindows;

	/** Maximized viewport (-1 if none) */
	INT				MaximizedViewport;

	/** Custom FOV for viewports with slider */
	INT				CustomFOV;

	/** The top level sizer for the viewports. */
	wxBoxSizer*		Sizer;

	/** The template this instance is based on. */
	EViewportConfig	Template;

	/** Four standard viewports */
	FVCD_Viewport Viewports[ 4 ];

	/** Additional 'floating' viewports */
	TArray< FVCD_Viewport* > FloatingViewports;

	/** Templte type prior to changing the viewport layout **/
	EViewportConfig OldTemplate;

	/** Positions/rotations of cameras prior to changing the viewport layout **/
	TArray<FViewportCameraData> OldViewportData;
};


/** Declare a custom event type to use to update the camera speed UI */
BEGIN_DECLARE_EVENT_TYPES()
	DECLARE_LOCAL_EVENT_TYPE(EVT_CAMSPEED_UPDATE, -1)
END_DECLARE_EVENT_TYPES()

/**
 * Contains a level editing viewport.
 */
class WxLevelViewportWindow : public wxWindow, public FEditorLevelViewportClient
{
public:
	class WxLevelViewportToolBar* ToolBar;
	class WxLevelViewportMaximizeToolBar* MaximizeToolBar;

	WxLevelViewportWindow();
	~WxLevelViewportWindow();

	virtual void GetViewportDimensions( FIntPoint& out_Origin, FIntPoint& out_Size );
	void SetUp( ELevelViewportType InViewportType, UBOOL bInSetListenerPosition, EShowFlags InShowFlags );
	void ResizeToolBar(void);
	virtual UBOOL InputAxis(FViewport* Viewport,INT ControllerId,FName Key,FLOAT Delta,FLOAT DeltaTime, UBOOL bGamePad=FALSE);

	/**
	 * Set the viewport type of the client
	 *
	 * @param InViewportType	The viewport type to set the client to
	 */
	virtual void SetViewportType( ELevelViewportType InViewportType );

protected:
	
	/**
	 * Checks to see if the current input event modified any show flags.
	 * @param Key				Key that was pressed.
	 * @param bControlDown		Flag for whether or not the control key is held down.
	 * @param bAltDown			Flag for whether or not the alt key is held down.
	 * @param bShiftDown		Flag for whether or not the shift key is held down.
	 * @return					Flag for whether or not we handled the input.
	 */
	virtual UBOOL CheckForShowFlagInput(FName Key, UBOOL bControlDown, UBOOL bAltDown, UBOOL bShiftDown);


private:

	/**
	 * Called in response to the camera speed being altered via toolbar button
	 *
	 * @param	Event	Event containing the Id specifying which speed was selected
	 */
	void OnCameraSpeedSelection( wxCommandEvent& Event );

	/**
	 * Called during idle time by wxWidgets to update the UI in relation to the camera
	 * speed button
	 *
	 * @param	Event	Event generated by wxWidgets in relation to the camera speed button
	 */
	void OnCameraUIUpdate( wxUpdateUIEvent& Event );

	void OnSize( wxSizeEvent& InEvent );
	void OnSetFocus( wxFocusEvent& In );
	
	DECLARE_EVENT_TABLE()
};


/**
 * Container frame window for floating editor viewport windows
 */
class WxFloatingViewportFrame : public WxTrackableFrame
{

public:

	/** Constructor */
	WxFloatingViewportFrame( wxWindow* InParent, wxWindowID InID, const FString& InTitle, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = 0 );

	/** Destructor */
	virtual ~WxFloatingViewportFrame();

	/** Sets the callback interface for this viewport */
	void SetCallbackInterface( IFloatingViewportCallback* NewCallbackInterface );


private:

	/** Called when the window is closed */
	virtual void OnClose( wxCloseEvent& InEvent );

	/** Called when the window is resized */
	virtual void OnSize( wxSizeEvent& InEvent );

	/** This function is called when the WxTrackableDialog has been selected from within the ctrl + tab dialog. */
	virtual void OnSelected();

	DECLARE_EVENT_TABLE()


private:

	/** Callback object for this floating viewport */
	IFloatingViewportCallback* CallbackInterface;

};


/**
 * A panel window that holds a viewport inside of it.  This class is used
 * to ease the use of splitters and other things that want wxWindows, not
 * UViewports.
 */
class WxViewportHolder : public wxPanel
{
public:
	WxViewportHolder(wxWindow* InParent, wxWindowID InID, bool InWantScrollBar, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxTAB_TRAVERSAL);
	virtual ~WxViewportHolder();

	/** The viewport living inside of this holder. */
	FViewport* Viewport;

	/** An optional scroll bar. */
	wxScrollBar* ScrollBar;

	/** Vars for controlling the scrollbar. */
	INT SBPos, SBRange;

	/** if TRUE, will call CloseViewport() when this window is destroyed */
	UBOOL bAutoDestroy;

	void SetViewport( FViewport* InViewport );
	void UpdateScrollBar( INT InPos, INT InRange );
	INT GetScrollThumbPos();
	void SetScrollThumbPos( INT InPos );

	/** 
	 * Scrolls the viewport up or down to a specified location. Also supports
	 * relative position scrolling
	 *
	 * @param Pos the position to scroll to or the amount to scroll by
	 * @param bIsDelta whether to apply a relative scroll or not
	 */
	void ScrollViewport(INT Pos,UBOOL bIsDelta = TRUE)
	{
		if (ScrollBar != NULL)
		{
			// If this is true, scroll relative to the current scroll location
			if (bIsDelta == TRUE)
			{
				Pos += GetScrollThumbPos();
			}
			// Set the new scroll position and invalidate
			SetScrollThumbPos(Pos);
			Viewport->Invalidate();
			// Force it to draw so the view change is seen
			Viewport->Draw();
		}
	}

protected:
	void OnSize( wxSizeEvent& InEvent );

	DECLARE_EVENT_TABLE()
};



/**
 * Container frame window for embedded Play-In-Editor windows
 */
class WxPIEContainerWindow
	: public wxPanel,
	  public FCallbackEventDevice
{

public:

	/**
	 * Creates a Play In Editor viewport window and embeds it into a level editor viewport (if possible)
	 *
	 * NOTE: This is a static method
	 *
	 * @param ViewportClient The viewport client the new viewport will be associated with
	 * @param TargetViewport The viewport window to possess
	 *
	 * @return Newly created WxPIEContainerWindow if successful, otherwise NULL
	 */
	static WxPIEContainerWindow* CreatePIEWindowAndPossessViewport( UGameViewportClient* ViewportClient,
																	FVCD_Viewport* TargetViewport );


protected:

	/** Constructor.  Should only be called from within CreatePIEContainerWindow() */
	WxPIEContainerWindow( wxWindow* InParent, wxWindowID InID, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = 0, const FString& InTitle = "" );


public:

	/** Destructor */
	virtual ~WxPIEContainerWindow();


	/**
	 * Setup the PIE viewport window
	 *
	 * @param ViewportClient The viewport client this window should be associated with
	 *
	 * @return TRUE if everything went OK.  If FALSE is returned, you should call Destroy on the window.
	 */
	UBOOL Initialize( UGameViewportClient* ViewportClient );


protected:

	/** Called when the window is resized */
	void OnSize( wxSizeEvent& InEvent );

	/** Called when the window receives focus */
	void OnSetFocus( wxFocusEvent& In );

	/** Closes the the Play-In-Editor window and restores any previous viewport */
	void ClosePIEWindowAndRestoreViewport();

	/** FCallbackEventDevice: Called from the global event handler when a registered event is fired */
	virtual void Send( ECallbackEventType InType );


private:

	/** Viewport contained by this PIE container */
	FViewport* Viewport;

	/** True if we're embedded in a floating viewport window */
	UBOOL bIsEmbeddedInFloatingWindow;

	DECLARE_EVENT_TABLE()

};




#endif // __VIEWPORTS_H__
