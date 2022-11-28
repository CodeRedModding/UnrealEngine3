/*=============================================================================
	Dialogs.h: Customized Dialog boxes 

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

/*-----------------------------------------------------------------------------
	WxDlgColor.
-----------------------------------------------------------------------------*/

class WxDlgColor : public wxColourDialog
{
public:
	WxDlgColor();
	~WxDlgColor();

	void LoadColorData( wxColourData* InData );
	void SaveColorData( wxColourData* InData );

	bool Create( wxWindow* InParent, wxColourData* InData );
	int ShowModal();
	void OnOK( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
