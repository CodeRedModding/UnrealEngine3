/*=============================================================================
	Docking.h: Classes to support window docking

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

enum EDockingEdge
{
	DE_None					= 0,	// Uninitialized
	DE_Floating				= 1,	// Undocked
	DE_Dockable_Start		= 2,
		DE_Left = DE_Dockable_Start,
		DE_Right			= 3,
		DE_Top				= 4,
		DE_Bottom			= 5,
	DE_Dockable_End			= 6,
	DE_Container			= 7,
};

enum EDockingMouseOver
{
	DMO_None				= 0,
	DMO_FrameTop			= 1,
	DMO_FrameRight			= 2,
	DMO_FrameBottom			= 3,
	DMO_FrameLeft			= 4,
	DMO_TabbedContainer		= 5,
};

/*-----------------------------------------------------------------------------
	WxSash.
-----------------------------------------------------------------------------*/

enum ESashOrientation
{
	SO_None,
	SO_Vertical,
	SO_Horizontal,
};

class WxSash : public wxWindow
{
public:
	WxSash( wxWindow* InParent, wxWindowID InID, ESashOrientation InOrientation );
	~WxSash();

	ESashOrientation Orientation;
	wxPoint MouseStart;

	void SetOrientation( ESashOrientation InOrientation );

	void OnPaint( wxPaintEvent& In );
	void OnLeftButtonDown( wxMouseEvent& In );
	void OnLeftButtonUp( wxMouseEvent& In );
	void OnMouseMove( wxMouseEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDockableWindow.

	This is a container that holds a single window that the user interacts
	with (i.e. the generic browser).

	Any window that you want to be dockable needs to derive from this class.
-----------------------------------------------------------------------------*/

class WxDockableWindow : public wxPanel
{
public:
	WxDockableWindow( wxWindow* InParent, wxWindowID InID, EDockableWindowType InType, FString InDockingName );
	~WxDockableWindow();

	FString DockingName;				// The name of this child window.
	EDockableWindowType Type;			// The type of window.
	wxMenuBar* MenuBar;
	wxToolBar* ToolBar;
	wxPanel* Panel;						// Windows that use "wxXmlResource::Get()->LoadPanel" store the resulting Panel* here

	virtual void Update();
	virtual void Activated();

	inline FString GetDockingName() { return DockingName; }

	void OnActivate( wxActivateEvent& In );
	void OnSize( wxSizeEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDockingNotebook.

	A notebook control that assumes it has a parent
	of WxDockingTabbedContainer.  It notifies this parent of drag events.
-----------------------------------------------------------------------------*/

class WxDockingNotebook : public wxNotebook
{
public:
	WxDockingNotebook( wxWindow* InParent, wxWindowID InID );
	~WxDockingNotebook();

	UBOOL bStartingDrag;
	FDragChecker DragChecker;

	void OnLeftButtonDown( wxMouseEvent& In );
	void OnLeftButtonUp( wxMouseEvent& In );
	void OnMouseMove( wxMouseEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDockingTabbedContainer.

	A docking parent can hold one or more XDockableWindows.

	These are the windows that draw controls related to the
	children (like the tab control and the drag bar at the top).
-----------------------------------------------------------------------------*/

class WxDockingTabbedContainer : public wxFrame
{
public:
	WxDockingTabbedContainer( wxWindow* InParent, wxWindowID InID );
	~WxDockingTabbedContainer();

	EDockingEdge Edge;
	INT DockingDepth;					// How far away from the docking edge this window extends
	wxRect FloatingRect;					// The position/size of this window when it was last floating
	WxDockingNotebook* NotebookCtrl;
	UBOOL bStartingDrag, bIsDragging;
	wxPoint MouseLast;
	FDragChecker DragChecker;
	WxSash* SizingSash;					// Shows up when the window is docked and allows resizing in one direction
	TArray<WxDockableWindow*> DockableWindows;

	inline UBOOL IsDocked() { return (Edge != DE_Floating && Edge != DE_None); };

	void AddDockableWindow( EDockableWindowType InType );
	void RemoveDockableWindow( EDockableWindowType InType );
	void OnSize( wxSizeEvent& In );
	void OnLeftButtonDown( wxMouseEvent& In );
	void OnLeftButtonUp( wxMouseEvent& In );
	void OnMouseMove( wxMouseEvent& In );
	void OnRightMouseButtonDown( wxMouseEvent& In );
	void OnWindowMove( wxMoveEvent& In );
	void OnCommand( wxCommandEvent& In );
	void OnPageChanged( wxNotebookEvent& In );
	void OnClose( wxCloseEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDockingRoot.

	Sits inside the editor frame and allows docking on any of it's 4 borders.
	This window houses one or more WxDockingTabbedContainer windows.

	If any edge has an WxDockingTabbedContainer docked to it, it 
	displays a sizing sash.
-----------------------------------------------------------------------------*/

class WxDockingRoot : public wxWindow
{
public:
	WxDockingRoot( wxWindow* InParent, wxWindowID InID );
	~WxDockingRoot();

	void UpdateMouseOver();
	void UpdateMouseCursor();
	WxDockingTabbedContainer* AddTabbedContainer( wxWindow* InParent, INT InX, INT InY, INT InWidth, INT InHeight );
	void DeleteTabbedContainer( WxDockingTabbedContainer* InTabbedContainer );
	void MergeTabbedContainers( WxDockingTabbedContainer* InA, WxDockingTabbedContainer* InB );
	void CleanUpDeadContainers();
	void Dock( WxDockingTabbedContainer* InDockingTabbedContainer, EDockingEdge InEdge );
	void Float( WxDockingTabbedContainer* InDockingTabbedContainer );
	void LayoutDockedWindows();
	INT GetDockingDepth( EDockingEdge InEdge, INT InDefValue );
	INT CountWindowsOnEdge( EDockingEdge InEdge );
	void RegisterDockableWindow( WxDockableWindow* InDockableWindow );
	EDockingEdge GetEdgeFromMouseOver( EDockingMouseOver InMouseOver );
	void OnSize( wxSizeEvent& In );
	wxRect GetEdgeRect( EDockingEdge InEdge );
	WxDockableWindow* CreateDockableWindow( wxWindow* InParent, EDockableWindowType InType );
	void SaveConfig();
	void LoadConfig();
	void ShowDockableWindow( EDockableWindowType InType );
	WxDockableWindow* FindDockableWindow( EDockableWindowType InType );
	void RefreshEditor( DWORD InFlags );

	TArray<WxDockingTabbedContainer*> TabbedContainers;
	EDockingMouseOver MouseOver;							// When dragging a window, this indicates what letting up on the mouse button will do
	WxDockingTabbedContainer *DraggedContainer,				// The container window that is currently being dragged around
		*DestContainer;										// The container window that the dragged container is hovering over

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
