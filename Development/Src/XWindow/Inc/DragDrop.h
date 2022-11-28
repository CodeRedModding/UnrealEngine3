/*=============================================================================
	DragDrop.h: Classes for handling drag&drop operations

	Revision history:
		* Created by Warren Marshall

=============================================================================*/

/*-----------------------------------------------------------------------------
	WxDropTarget_PropWindowObjectItem

	Drop target for UObject names.
-----------------------------------------------------------------------------*/

class WxDropTarget_PropWindowObjectItem : public wxTextDropTarget
{
public:
	WxDropTarget_PropWindowObjectItem( UProperty* InProperty );

	virtual bool OnDropText( wxCoord x, wxCoord y, const wxString& text );

private:

	UProperty* Property;
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
