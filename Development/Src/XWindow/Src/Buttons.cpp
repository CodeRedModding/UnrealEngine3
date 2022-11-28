
#include "XWindowPrivate.h"

/*-----------------------------------------------------------------------------
	WxBitmapButton.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxBitmapButton, wxBitmapButton )
	EVT_RIGHT_DOWN( WxBitmapButton::OnRightButtonDown )
END_EVENT_TABLE()

WxBitmapButton::WxBitmapButton()
{
}

WxBitmapButton::WxBitmapButton( wxWindow* InParent, wxWindowID InID, WxBitmap InBitmap, const wxPoint& InPos, const wxSize& InSize )
	: wxBitmapButton( InParent, InID, InBitmap, InPos, InSize )
{
}

WxBitmapButton::~WxBitmapButton()
{
}

void WxBitmapButton::OnRightButtonDown( wxMouseEvent& In )
{
	GetParent()->ProcessEvent( In );
}

/*-----------------------------------------------------------------------------
	WxMenuButton.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxMenuButton, WxBitmapButton )
	EVT_COMMAND_RANGE( IDPB_DROPDOWN_START, IDPB_DROPDOWN_END, wxEVT_COMMAND_BUTTON_CLICKED, WxMenuButton::OnClick )
END_EVENT_TABLE()

WxMenuButton::WxMenuButton()
{
	Menu = NULL;
}

WxMenuButton::WxMenuButton( wxWindow* InParent, wxWindowID InID, WxBitmap* InBitmap, wxMenu* InMenu, const wxPoint& InPos, const wxSize& InSize )
	: WxBitmapButton( InParent, InID, *InBitmap, InPos, InSize )
{
	Menu = InMenu;
}

WxMenuButton::~WxMenuButton()
{
}

void WxMenuButton::Create( wxWindow* InParent, wxWindowID InID, WxBitmap* InBitmap, wxMenu* InMenu, const wxPoint& InPos, const wxSize& InSize )
{
	WxBitmapButton::Create( InParent, InID, *InBitmap, InPos, InSize );

	Menu = InMenu;
}

void WxMenuButton::OnClick( wxCommandEvent &In )
{
	// Display the menu directly below the button

	wxRect rc = GetRect();
	PopupMenu( Menu, 0, rc.GetHeight() );
}

/*-----------------------------------------------------------------------------
	FBitmapStateButtonState.
-----------------------------------------------------------------------------*/

FBitmapStateButtonState::FBitmapStateButtonState()
{
	check(0);	// Wrong ctor
}

FBitmapStateButtonState::FBitmapStateButtonState( INT InID, wxBitmap* InBitmap )
{
	ID = InID;
	Bitmap = InBitmap;
}

FBitmapStateButtonState::~FBitmapStateButtonState()
{
}

/*-----------------------------------------------------------------------------
	WxBitmapStateButton.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxBitmapStateButton, WxBitmapButton )
	EVT_LEFT_DOWN( WxBitmapStateButton::OnLeftButtonDown )
END_EVENT_TABLE()

WxBitmapStateButton::WxBitmapStateButton()
{
	check(0);	// Wrong ctor
}

WxBitmapStateButton::WxBitmapStateButton( wxWindow* InParent, wxWindow* InMsgTarget, wxWindowID InID )
	: WxBitmapButton( InParent, InID, WxBitmap(8,8) )
{
}

WxBitmapStateButton::~WxBitmapStateButton()
{
	States.Empty();
}

void WxBitmapStateButton::AddState( INT InID, wxBitmap* InBitmap )
{
	States.AddItem( new FBitmapStateButtonState( InID, InBitmap ) );
}

FBitmapStateButtonState* WxBitmapStateButton::GetCurrentState()
{
	return CurrentState;
}

void WxBitmapStateButton::SetCurrentState( INT InID )
{
	for( INT x = 0 ; x < States.Num() ; ++x )
		if( States(x)->ID == InID )
		{
			CurrentState = States(x);
			SetBitmapLabel( *CurrentState->Bitmap );
			Refresh();
			return;
		}

	check(0);		// Invalid state ID
}

void WxBitmapStateButton::OnLeftButtonDown( wxMouseEvent& In )
{
	CycleState();

	In.Skip();
}

/*-----------------------------------------------------------------------------
	WxBitmapCheckButton.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxBitmapCheckButton, WxBitmapStateButton )
END_EVENT_TABLE()

WxBitmapCheckButton::WxBitmapCheckButton()
{
	check(0);	// Wrong ctor
}

WxBitmapCheckButton::WxBitmapCheckButton( wxWindow* InParent, wxWindow* InMsgTarget, wxWindowID InID, wxBitmap* InBitmapOff, wxBitmap* InBitmapOn )
	: WxBitmapStateButton( InParent, InMsgTarget, InID )
{
	AddState( STATE_Off, InBitmapOff );
	AddState( STATE_On, InBitmapOn );

	SetCurrentState( STATE_Off );
}

WxBitmapCheckButton::~WxBitmapCheckButton()
{
}

void WxBitmapCheckButton::CycleState()
{
	if( GetCurrentState()->ID == STATE_On )
		SetCurrentState( STATE_Off );
	else
		SetCurrentState( STATE_On );
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
