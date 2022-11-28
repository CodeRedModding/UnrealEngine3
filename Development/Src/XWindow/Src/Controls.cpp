
#include "XWindowPrivate.h"

/*-----------------------------------------------------------------------------
	WxStatusBar.
-----------------------------------------------------------------------------*/

WxStatusBar::WxStatusBar()
	: wxStatusBar()
{
}

WxStatusBar::WxStatusBar( wxWindow* parent, wxWindowID id, long style, const wxString& name )
	: wxStatusBar( parent, id, style, name )
{
}

WxStatusBar::~WxStatusBar()
{
}

/*-----------------------------------------------------------------------------
	WxTextCtrl.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxTextCtrl, wxTextCtrl )
	EVT_CHAR( WxTextCtrl::OnChar )
END_EVENT_TABLE()

WxTextCtrl::WxTextCtrl()
	: wxTextCtrl()
{
}

WxTextCtrl::WxTextCtrl( wxWindow* parent, wxWindowID id, const wxString& value, const wxPoint& pos, const wxSize& size, long style, const wxValidator& validator, const wxString& name )
	: wxTextCtrl( parent, id, value, pos, size, style, validator, name )
{
}

WxTextCtrl::~WxTextCtrl()
{
}

void WxTextCtrl::OnChar( wxKeyEvent& In )
{
	switch( In.GetKeyCode() )
	{
		case WXK_DOWN:
		case WXK_UP:
		case WXK_TAB:
			GetParent()->AddPendingEvent( In );
			break;

		default:
			In.Skip();
			break;
	}
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
