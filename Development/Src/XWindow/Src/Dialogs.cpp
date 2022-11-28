
#include "XWindowPrivate.h"

/*-----------------------------------------------------------------------------
	WxDlgColor.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgColor, wxColourDialog)
	EVT_BUTTON( wxID_OK, WxDlgColor::OnOK )
END_EVENT_TABLE()

WxDlgColor::WxDlgColor()
	: wxColourDialog()
{
}

WxDlgColor::~WxDlgColor()
{
	wxColourData cd = GetColourData();
	SaveColorData( &cd );
}

bool WxDlgColor::Create( wxWindow* InParent, wxColourData* InData )
{
	LoadColorData( InData );

	return wxColourDialog::Create( InParent, InData );
}

/**
 * Loads custom color data from the INI file.
 *
 * @param	InData	The color data structure to load into
 */

void WxDlgColor::LoadColorData( wxColourData* InData )
{
	for( INT x = 0 ; x < 16 ; ++x )
	{
		FString Key = FString::Printf( TEXT("Color%d"), x );
		INT Color = wxColour(255,255,255).GetPixel();
		GConfig->GetInt( TEXT("ColorDialog"), *Key, Color, GEditorIni );
		InData->SetCustomColour( x, wxColour( Color ) );
	}
}

/**
 * Saves custom color data to the INI file.
 *
 * @param	InData	The color data structure to save from
 */

void WxDlgColor::SaveColorData( wxColourData* InData )
{
	for( INT x = 0 ; x < 16 ; ++x )
	{
		FString Key = FString::Printf( TEXT("Color%d"), x );
		GConfig->SetInt( TEXT("ColorDialog"), *Key, InData->GetCustomColour(x).GetPixel(), GEditorIni );
	}
}

int WxDlgColor::ShowModal()
{
	return wxColourDialog::ShowModal();
}

void WxDlgColor::OnOK( wxCommandEvent& In )
{

	wxColourDialog::OnOK( In );
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
