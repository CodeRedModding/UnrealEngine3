/*=============================================================================
	Controls.h: Misc custom/overridden controls.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

/*-----------------------------------------------------------------------------
	WxStatusBar.
-----------------------------------------------------------------------------*/

class WxStatusBar : public wxStatusBar
{
public:
	WxStatusBar();
	WxStatusBar( wxWindow* parent, wxWindowID id, long style = wxST_SIZEGRIP, const wxString& name = TEXT("statusBar") );
	~WxStatusBar();

	virtual void SetUp()=0;
	virtual void UpdateUI()=0;
};

/*-----------------------------------------------------------------------------
	WxTextCtrl.
-----------------------------------------------------------------------------*/

class WxTextCtrl : public wxTextCtrl, public wxDropTarget
{
public:
	WxTextCtrl();
	WxTextCtrl( wxWindow* parent, wxWindowID id, const wxString& value = TEXT(""), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = 0, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxTextCtrlNameStr );
	~WxTextCtrl();

	void OnChar( wxKeyEvent& In );

	virtual wxDragResult OnEnter(wxCoord x, wxCoord y, wxDragResult def)
	{
		//m_frame->SetStatusText(_T("Mouse entered the frame"));
		
		return OnDragOver(x, y, def);
	}
	virtual void OnLeave()
	{
		//m_frame->SetStatusText(_T("Mouse left the frame"));
	}
	virtual wxDragResult OnData(wxCoord x, wxCoord y, wxDragResult def)
	{
		//if( !GetData() )
		//{
		//	wxLogError(wxT("Failed to get drag and drop data"));

		//	return wxDragNone;
		//}

		//m_frame->OnDrop(x, y, ((DnDShapeDataObject *)GetDataObject())->GetShape());

		return def;
	}

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
