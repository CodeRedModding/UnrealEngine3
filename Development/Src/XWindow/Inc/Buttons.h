/*=============================================================================
	Buttons.h: Special controls based on standard buttons

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

/*-----------------------------------------------------------------------------
	WxBitmapButton.

	A wrapper for wxBitmapButton.
-----------------------------------------------------------------------------*/

class WxBitmapButton : public wxBitmapButton
{
public:
	WxBitmapButton();
	WxBitmapButton( wxWindow* InParent, wxWindowID InID, WxBitmap InBitmap, const wxPoint& InPos = wxDefaultPosition, const wxSize& InSize = wxDefaultSize );
	~WxBitmapButton();

	void OnRightButtonDown( wxMouseEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxMenuButton.

	Displays a menu below the button when clicked.
-----------------------------------------------------------------------------*/

class WxMenuButton : public WxBitmapButton
{
public:
	WxMenuButton();
	WxMenuButton( wxWindow* InParent, wxWindowID InID, WxBitmap* InBitmap, wxMenu* InMenu, const wxPoint& InPos = wxDefaultPosition, const wxSize& InSize = wxDefaultSize );
	~WxMenuButton();

	void Create( wxWindow* InParent, wxWindowID InID, WxBitmap* InBitmap, wxMenu* InMenu, const wxPoint& InPos, const wxSize& InSize );

	wxMenu* Menu;			// The menu to display when clicked

	void OnClick( wxCommandEvent &In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxBitmapStateButton.
-----------------------------------------------------------------------------*/

class FBitmapStateButtonState
{
public:
	FBitmapStateButtonState();
	FBitmapStateButtonState( INT InID, wxBitmap* InBitmap );
	~FBitmapStateButtonState();

	INT ID;
	wxBitmap* Bitmap;
};

class WxBitmapStateButton : public WxBitmapButton
{
public:
	WxBitmapStateButton();
	WxBitmapStateButton( wxWindow* InParent, wxWindow* InMsgTarget, wxWindowID InID );
	~WxBitmapStateButton();

	TArray<FBitmapStateButtonState*> States;
	FBitmapStateButtonState* CurrentState;

	void AddState( INT InID, wxBitmap* InBitmap );
	FBitmapStateButtonState* GetCurrentState();
	void SetCurrentState( INT InID );

	virtual void CycleState() = 0;

	void OnLeftButtonDown( wxMouseEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxBitmapCheckButton.

	A button that toggles between two states.
-----------------------------------------------------------------------------*/

class WxBitmapCheckButton : public WxBitmapStateButton
{
public:
	WxBitmapCheckButton();
	WxBitmapCheckButton( wxWindow* InParent, wxWindow* InMsgTarget, wxWindowID InID, wxBitmap* InBitmapOff, wxBitmap* InBitmapOn );
	~WxBitmapCheckButton();

	virtual void CycleState();

	enum { STATE_Off, STATE_On };

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
