/*=============================================================================
	Viewports.h: The viewport windows used by the editor

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

class WxLevelViewportWindow;

enum EViewportConfig
{ 
	VC_None			= -1,
	VC_2_2_Split,
	VC_1_2_Split,
	VC_1_1_Split,
	VC_Max
};

/*-----------------------------------------------------------------------------
	FViewportConfig_Viewport.

	The default values for a viewport within an FViewportConfig_Template.
-----------------------------------------------------------------------------*/

class FViewportConfig_Viewport
{
public:
	FViewportConfig_Viewport();
	virtual ~FViewportConfig_Viewport();

	ELevelViewportType ViewportType;
	ESceneViewMode ViewMode;
	DWORD ShowFlags;
	UBOOL bEnabled;						// If 0, this viewport template is not being used within its owner config template
};

/*-----------------------------------------------------------------------------
	FViewportConfig_Template.

	A template for a viewport configuration.  Holds the baseline layouts
	and flags for a config.
-----------------------------------------------------------------------------*/

class FViewportConfig_Template
{
public:
	FViewportConfig_Template();
	virtual ~FViewportConfig_Template();
	
	void Set( EViewportConfig InViewportConfig );

	FString Desc;										// The description for this configuration
	EViewportConfig ViewportConfig;						// The viewport config this template represents
	FViewportConfig_Viewport ViewportTemplates[4];		// The viewport templates within this config template
};

/*-----------------------------------------------------------------------------
	FViewportConfig_Data.

	An instance of a FViewportConfig_Template.  There is only one of these
	in use at any given time and it represents the current editor viewport
	layout.  This contains more information than the template (i.e. splitter 
	and camera locations).
-----------------------------------------------------------------------------*/

class FVCD_Viewport : public FViewportConfig_Viewport
{
public:
	FVCD_Viewport();
	virtual ~FVCD_Viewport();

	DWORD ShowFlags;
	ESceneViewMode ViewMode;
	FLOAT SashPos;					// Stores the sash position from the INI until we can use it in FViewportConfig_Data::Apply

	WxLevelViewportWindow* ViewportWindow;				// The window that holds the viewport itself

	inline FVCD_Viewport operator=( FViewportConfig_Viewport Other )
	{
		ViewportType = Other.ViewportType;
		ViewMode = Other.ViewMode;
		ShowFlags = Other.ShowFlags;
		bEnabled = Other.bEnabled;

		return *this;
	}
};

class FViewportConfig_Data
{
public:
	FViewportConfig_Data();
	virtual ~FViewportConfig_Data();

	void SetTemplate( EViewportConfig InTemplate );
	void Apply( wxWindow* InParent );
	void Save();
	void Load( FViewportConfig_Data* InData );
	void SaveToINI();
	UBOOL LoadFromINI();

	TArray<wxSplitterWindow*> SplitterWindows;		// The splitters windows that make this config possible
	wxBoxSizer* Sizer;								// The top level sizer for the viewports
	EViewportConfig Template;						// The template this instance is based on.
	FVCD_Viewport Viewports[4];						// The overriding viewport data for this instance

	inline FViewportConfig_Data operator=( FViewportConfig_Template Other )
	{
		for( INT x = 0 ; x < 4 ; ++x )
		{
			Viewports[x] = Other.ViewportTemplates[x];
		}

		return *this;
	}
};

/*-----------------------------------------------------------------------------
	WxLevelViewportWindow.

	Contains a level editing viewport.
-----------------------------------------------------------------------------*/

class WxLevelViewportWindow : public wxWindow, public FEditorLevelViewportClient
{
public:

	WxLevelViewportToolBar* ToolBar;

	WxLevelViewportWindow();
	~WxLevelViewportWindow();

	void SetUp( INT InViewportNum, ELevelViewportType InViewportType, ESceneViewMode InViewMode, DWORD InShowFlags );

	void OnSize( wxSizeEvent& InEvent );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	EViewportHolder.

	A panel window that holds a viewport inside of it.  This class is used
	to ease the use of splitters and other things that want wxWindows, not
	UViewports.
-----------------------------------------------------------------------------*/

class EViewportHolder : public wxPanel
{
public:
	EViewportHolder( wxWindow* InParent, wxWindowID InID, bool InWantScrollBar );

	FChildViewport* Viewport;	// The viewport living inside of this holder
	wxScrollBar* ScrollBar;		// An optional scroll bar
	INT SBPos, SBRange;			// Vars for controlling the scrollbar

	void SetViewport( FChildViewport* InViewport );
	void UpdateScrollBar( INT InPos, INT InRange );
	INT GetScrollThumbPos();
	void SetScrollThumbPos( INT InPos );

	void OnSize( wxSizeEvent& InEvent );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
