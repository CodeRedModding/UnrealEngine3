/*=============================================================================
	DlgRotateAnimSequence.h: Dialog for rotating UAnimSequence objects.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DlgRotateAnimSequence.h"

BEGIN_EVENT_TABLE(WxDlgRotateAnimSeq, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgRotateAnimSeq::OnOK )
END_EVENT_TABLE()

WxDlgRotateAnimSeq::WxDlgRotateAnimSeq()
{
	const bool bSuccess = wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_ROTATEANIMSEQ") );
	check( bSuccess );

	DegreesEntry = (wxTextCtrl*)FindWindow( XRCID( "IDEC_ANGLEENTRY" ) );
	check( DegreesEntry != NULL );

	DegreesEntry->SetValue( TEXT("0.0") );

	AxisCombo = (wxComboBox*)FindWindow( XRCID( "IDEC_AXISCOMBO" ) );
	check( AxisCombo != NULL );

	AxisCombo->Append( *LocalizeUnrealEd("XAxis") );
	AxisCombo->Append( *LocalizeUnrealEd("YAxis") );
	AxisCombo->Append( *LocalizeUnrealEd("ZAxis") );
	AxisCombo->SetSelection(2);

	Degrees = 0.f;
	Axis = AXIS_X;
}

int WxDlgRotateAnimSeq::ShowModal()
{
	return wxDialog::ShowModal();
}

void WxDlgRotateAnimSeq::OnOK( wxCommandEvent& In )
{
	double DegreesNum;
	const UBOOL bIsNumber = DegreesEntry->GetValue().ToDouble( &DegreesNum );
	if( bIsNumber )
	{
		Degrees = DegreesNum;
	}

	switch( AxisCombo->GetSelection() )
	{
		case 0:
			Axis = AXIS_X;
			break;
		case 1:
			Axis = AXIS_Y;
			break;
		case 2:
			Axis = AXIS_Z;
			break;
		default:
			break;
	}

	wxDialog::AcceptAndClose();
}
