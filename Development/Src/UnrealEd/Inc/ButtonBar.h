/*=============================================================================
	ButtonBar : The button bar on the left side of the editor

	Revision history:
		* Created by Warren Marshall

=============================================================================*/

enum EButtonType
{
	BUTTONTYPE_None			= 0,	// Nothing special
	BUTTONTYPE_BrushBuilder	= 1,	// Builds BSP brushes
	BUTTONTYPE_ClassMenu	= 2,	// Right clicking button pops up a menu containing sub classes
};

/*-----------------------------------------------------------------------------
	WxButtonGroupButton.

	A bitmap button with some extra info added.
-----------------------------------------------------------------------------*/

class WxButtonGroupButton : public wxEvtHandler, public WxSerializableWindow
{
public:
	WxButtonGroupButton();
	WxButtonGroupButton( wxWindow* parent, wxWindowID id, WxBitmap& InBitmap, EButtonType InButtonType, UClass* InClass, INT InStartMenuID );
	WxButtonGroupButton( wxWindow* parent, wxWindowID id, WxBitmap& InBitmapOff, WxBitmap& InBitmapOn, EButtonType InButtonType, UClass* InClass, INT InStartMenuID );
	~WxButtonGroupButton();

	// WxSerializableWindow interface
	
	/** 
	 * Serializes the BrushBuilder reference so it doesn't get garbage collected.
	 *
	 * @param Ar	FArchive to serialize with
	 */
	void Serialize(FArchive& Ar);

	class WxBitmapButton* Button;
	EButtonType ButtonType;
	UClass* Class;
	INT StartMenuID;
	UBrushBuilder* BrushBuilder;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxButtonGroup.

	A collection of buttons that are grouped together.
-----------------------------------------------------------------------------*/

class WxButtonGroup : public wxWindow
{
public:
	WxButtonGroup( wxWindow* InParent );
	~WxButtonGroup();

	enum
	{
		BUTTON_SZ=36,
		TITLEBAR_H=24,
	};
	TArray<WxButtonGroupButton*> Buttons;
	UBOOL bExpanded;
	TArray<UClass*> PopupMenuClasses;

	WxButtonGroupButton* AddButton( INT InID, FString InBitmapFilename, FString InToolTip, EButtonType InButtonType = BUTTONTYPE_None, UClass* InClass = NULL, INT InStartMenuID = -1 );
	WxButtonGroupButton* AddButtonChecked( INT InID, FString InBitmapFilename, FString InToolTip, EButtonType InButtonType = BUTTONTYPE_None, UClass* InClass = NULL, INT InStartMenuID = -1 );
	INT GetHeight();
	void UpdateUI();
	WxButtonGroupButton* FindButtonFromId( INT InID );

	void OnSize( wxSizeEvent& InEvent );
	void OnModeCamera( wxCommandEvent& In );
	void OnModeGeometry( wxCommandEvent& In );
	void OnModeTexture( wxCommandEvent& In );
	void OnModeTerrain( wxCommandEvent& In );
	void OnBrushAdd( wxCommandEvent& In );
	void OnBrushSubtract( wxCommandEvent& In );
	void OnBrushIntersect( wxCommandEvent& In );
	void OnBrushDeintersect( wxCommandEvent& In );
	void OnAddSpecial( wxCommandEvent& In );
	void OnAddAntiportal( wxCommandEvent& In );
	void OnAddMover( wxCommandEvent& In );
	void OnAddVolume( wxCommandEvent& In );
	void OnSelectShow( wxCommandEvent& In );
	void OnSelectHide( wxCommandEvent& In );
	void OnSelectInvert( wxCommandEvent& In );
	void OnShowAll( wxCommandEvent& In );
	void OnCameraSpeed( wxCommandEvent& In );
	void OnRightButtonDown( wxMouseEvent& In );
	void OnAddMoverClass( wxCommandEvent& In );
	void OnAddVolumeClass( wxCommandEvent& In );
	void OnPaint( wxPaintEvent& In );
	void OnLeftButtonDown( wxMouseEvent& In );
	void OnBrushBuilder( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxButtonBar.

	The button bar on the left side of the editor.
-----------------------------------------------------------------------------*/

class WxButtonBar : public wxPanel
{
public:
	WxButtonBar();
	~WxButtonBar();

	UBOOL Create( wxWindow* parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxTAB_TRAVERSAL, const FString& name = TEXT("panel") );
	void PositionChildControls();
	void UpdateUI();

	void OnSize( wxSizeEvent& InEvent );

	TArray<WxButtonGroup*> ButtonGroups;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
