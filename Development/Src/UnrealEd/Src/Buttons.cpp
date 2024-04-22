/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"

/*-----------------------------------------------------------------------------
	WxBitmapButton.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxBitmapButton, wxBitmapButton )
	EVT_RIGHT_DOWN(	WxBitmapButton::OnRightButtonDown)
	EVT_RIGHT_UP(	WxBitmapButton::OnRightButtonUp)
	EVT_ERASE_BACKGROUND( WxBitmapButton::OnEraseBackground )
END_EVENT_TABLE()

WxBitmapButton::WxBitmapButton()
{
	bOnRightForward = TRUE;			// Default to TRUE, so forwards right click/button input to the parent
}

WxBitmapButton::WxBitmapButton( wxWindow* InParent, wxWindowID InID, WxBitmap InBitmap, const wxPoint& InPos, const wxSize& InSize, const UBOOL InOnRightForward )
	: wxBitmapButton( InParent, InID, InBitmap, InPos, InSize )
{
	bOnRightForward = InOnRightForward;
}

WxBitmapButton::~WxBitmapButton()
{
}

void WxBitmapButton::Create( wxWindow* InParent, wxWindowID InID, WxBitmap* InBitmap, const wxPoint& InPos, const wxSize& InSize, const UBOOL InOnRightForward )
{
	wxBitmapButton::Create( InParent, InID, *InBitmap, InPos, InSize );

	bOnRightForward = InOnRightForward;
}

void WxBitmapButton::OnRightButtonDown( wxMouseEvent& In )
{
	// Only forward right click/button input to the parent if specified
	if (bOnRightForward && GetParent())
	{
		GetParent()->ProcessEvent(In);
	}
}

void WxBitmapButton::OnRightButtonUp(wxMouseEvent& In)
{
	// Only forward right click/button input to the parent if specified
	if (bOnRightForward && GetParent())
	{
		GetParent()->ProcessEvent(In);
	}
}

void WxBitmapButton::OnRightClick(wxCommandEvent& In)
{
	// Only forward right click/button input to the parent if specified
	if (bOnRightForward && GetParent())
	{
		GetParent()->ProcessEvent(In);
	}
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

WxMenuButton::WxMenuButton( wxWindow* InParent, wxWindowID InID, WxBitmap* InBitmap, wxMenu* InMenu, const wxPoint& InPos, const wxSize& InSize, const UBOOL InOnRightForward )
	: WxBitmapButton( InParent, InID, *InBitmap, InPos, InSize, InOnRightForward )
{
	Menu = InMenu;
}

WxMenuButton::~WxMenuButton()
{
}

void WxMenuButton::Create( wxWindow* InParent, wxWindowID InID, WxBitmap* InBitmap, wxMenu* InMenu, const wxPoint& InPos, const wxSize& InSize, const UBOOL InOnRightForward )
{
	WxBitmapButton::Create( InParent, InID, InBitmap, InPos, InSize, InOnRightForward );

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
END_EVENT_TABLE()

/** Construct a WxBitmapStateButton */
WxBitmapStateButton::WxBitmapStateButton()
{
	check(0);	// Wrong ctor
}

/** 
 * Construct a WxBitmapStateButton
 *
 * @param	InParent				Parent window of this button
 * @param	InMsgTarget				Unused?
 * @param	InID					ID of this button
 * @param	InPos					Position of this button
 * @param	InSize					Size of this button
 * @param	InOnRightForward	If TRUE, forwards right click/button input to the parent
 * @param	InCycleOnLeftMouseDown	If TRUE, left mouse down on the button will cycle to the next bitmap state
 */
WxBitmapStateButton::WxBitmapStateButton( wxWindow* InParent, wxWindow* InMsgTarget, wxWindowID InID, const wxPoint& InPos, const wxSize& InSize, const UBOOL InCycleOnLeftMouseDown, const UBOOL InOnRightForward )
	: WxBitmapButton( InParent, InID, WxBitmap(8,8), InPos, InSize, InOnRightForward )
{
	if ( InCycleOnLeftMouseDown )
	{
		// Connect the left mouse button handler dynamically
		GetEventHandler()->Connect( InID, wxEVT_LEFT_DOWN, wxMouseEventHandler( WxBitmapStateButton::OnLeftButtonDown ) );
	}
}

/** Destruct a WxBitmapStateButton */
WxBitmapStateButton::~WxBitmapStateButton()
{
	States.Empty();
}

/**
 * Add a new state to the button
 *
 * @param	InID		ID of the new state
 * @param	InBitmap	Bitmap to use for the new state
 */
void WxBitmapStateButton::AddState( INT InID, wxBitmap* InBitmap )
{
	States.AddItem( new FBitmapStateButtonState( InID, InBitmap ) );
}

/**
 * Return the current state, if any
 *
 * @return	The current state, if it has been set; NULL otherwise
 */
FBitmapStateButtonState* WxBitmapStateButton::GetCurrentState()
{
	return CurrentState;
}

/**
 * Set the current state to the state represented by the provided ID
 *
 * @param	InID	ID of the state to set as the current state
 */
void WxBitmapStateButton::SetCurrentState( INT InID )
{
	for( INT StateIndex = 0; StateIndex < States.Num(); ++StateIndex )
	{
		if( States(StateIndex)->ID == InID )
		{
			CurrentState = States(StateIndex);
			SetBitmapLabel( *CurrentState->Bitmap );
			Refresh();
			return;
		}
	}
	check(0);		// Invalid state ID
}

/** If a current state is specified, cycle to the next bitmap state, wrapping once the end state has been hit */
void WxBitmapStateButton::CycleState()
{
	const INT NumStates = States.Num();

	// Only cycle if a current state has been set, and there is more than one state
	if ( CurrentState && NumStates > 1 )
	{
		const INT CurStateIndex = States.FindItemIndex( CurrentState );
		
		// Handle case of another state right after this one
		if ( CurStateIndex + 1 < NumStates )
		{
			CurrentState = States( CurStateIndex + 1 );
		}

		// If at end of the states array, wrap around back to the beginning
		else
		{
			CurrentState = States( 0 );
		}

		SetBitmapLabel( *CurrentState->Bitmap );
		Refresh();
	}
}

/** 
 * Called in response to a left mouse down action on the button, provided InCycleOnLeftMouseDown was set to TRUE during construction
 *
 * @param	In	Event automatically generated by wxWidgets upon left mouse down on the button
 */
void WxBitmapStateButton::OnLeftButtonDown( wxMouseEvent& In )
{
	CycleState();

	In.Skip();
}

/*-----------------------------------------------------------------------------
	WxBitmapCheckButton.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxBitmapCheckButton, WxBitmapStateButton )
	EVT_ERASE_BACKGROUND( WxBitmapCheckButton::OnEraseBackground )
END_EVENT_TABLE()

WxBitmapCheckButton::WxBitmapCheckButton()
{
	check(0);	// Wrong ctor
}

WxBitmapCheckButton::WxBitmapCheckButton( wxWindow* InParent, wxWindow* InMsgTarget, wxWindowID InID, wxBitmap* InBitmapOff, wxBitmap* InBitmapOn, const wxPoint& InPos, const wxSize& InSize, const UBOOL InOnRightForward )
	: WxBitmapStateButton( InParent, InMsgTarget, InID, InPos, InSize, InOnRightForward )
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

IMPLEMENT_DYNAMIC_CLASS(WxBitmapButton, wxBitmapButton);
IMPLEMENT_DYNAMIC_CLASS(WxBitmapCheckButton, WxBitmapButton);


