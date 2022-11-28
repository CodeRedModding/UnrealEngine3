
#include "XWindowPrivate.h"

/*-----------------------------------------------------------------------------
	WxDropTarget_PropWindowObjectItem
-----------------------------------------------------------------------------*/

WxDropTarget_PropWindowObjectItem::WxDropTarget_PropWindowObjectItem( UProperty* InProperty )
	: wxTextDropTarget()
{
	Property = InProperty;
}

bool WxDropTarget_PropWindowObjectItem::OnDropText( wxCoord x, wxCoord y, const wxString& text )
{

	return 1;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
