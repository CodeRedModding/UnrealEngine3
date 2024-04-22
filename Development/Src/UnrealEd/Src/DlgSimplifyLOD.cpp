/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DlgSimplifyLOD.h"

BEGIN_EVENT_TABLE(WxDlgSimplifyLOD, wxDialog)
END_EVENT_TABLE()

WxDlgSimplifyLOD::WxDlgSimplifyLOD( wxWindow* InParent, const TCHAR* DialogTitle )
	: wxDialog( InParent, wxID_ANY, DialogTitle, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE )
{
	const INT BorderSize = 5;

	// Setup controls
	wxBoxSizer* TopSizer = new wxBoxSizer( wxVERTICAL );
	SetSizer( TopSizer );
	{
		wxStaticBoxSizer* Sizer = new wxStaticBoxSizer( wxHORIZONTAL, this, *LocalizeUnrealEd( TEXT("MeshSimp_ViewDistance") ) );
		{
			Sizer->AddSpacer( BorderSize );
			// NOTE: Proper min/max/value settings will be filled in later in ShowModal()
			ViewDistanceSlider = new wxSlider( this, -1, 2000, 10, 10000, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_LABELS );
			wxSize SliderMinSize = ViewDistanceSlider->GetMinSize();
			ViewDistanceSlider->SetMinSize( wxSize( Max<INT>( SliderMinSize.GetWidth(), 250 ), SliderMinSize.GetHeight() ) );
			Sizer->Add( ViewDistanceSlider,	1, wxEXPAND | wxALL, BorderSize );
		}
		TopSizer->Add( Sizer, 1, wxEXPAND | wxALL, BorderSize );
	}
	TopSizer->AddSpacer( BorderSize );
	{
		wxStaticBoxSizer* Sizer = new wxStaticBoxSizer( wxHORIZONTAL, this, *LocalizeUnrealEd( TEXT("MeshSimp_PixelError") ) );
		{
			Sizer->AddSpacer( BorderSize );
			// NOTE: Proper min/max/value settings will be filled in later in ShowModal()
			PixelErrorSlider = new wxSlider( this, -1, 1, 1, 10, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_LABELS );
			wxSize SliderMinSize = PixelErrorSlider->GetMinSize();
			PixelErrorSlider->SetMinSize( wxSize( Max<INT>( SliderMinSize.GetWidth(), 250 ), SliderMinSize.GetHeight() ) );
			Sizer->Add( PixelErrorSlider,	1, wxEXPAND | wxALL, BorderSize );
		}
		TopSizer->Add( Sizer, 1, wxEXPAND | wxALL, BorderSize );
	}
	TopSizer->AddSpacer( BorderSize * 2 );
	{
		wxBoxSizer* ButtonSizer = new wxBoxSizer( wxHORIZONTAL );
		{
			wxButton* OkButton = new wxButton( this, wxID_OK, *LocalizeUnrealEd( TEXT("&OK") ) );
			ButtonSizer->Add( OkButton, 1, wxALIGN_RIGHT, BorderSize );
			wxButton* CancelButton = new wxButton( this, wxID_CANCEL, *LocalizeUnrealEd( TEXT("&Cancel") ) );
			ButtonSizer->Add( CancelButton, 1, wxALIGN_RIGHT, BorderSize );
		}
		TopSizer->Add( ButtonSizer, 0, wxALIGN_RIGHT | wxALL, BorderSize );
	}
}

/**
 * Shows the dialog box and waits for the user to respond.
 * @returns wxID_OK or wxID_CANCEL depending on which button the user dismissed the dialog with.
 */
int WxDlgSimplifyLOD::ShowModal()
{
	return wxDialog::ShowModal();
}

/** 
 * Computes the desired quality based on the user's input.
 * @returns the quality to give the desired at error at the specified view distance.
 */
FLOAT WxDlgSimplifyLOD::GetMaxDeviation() const
{
	// We want to solve for the distance in world space between two pixels at a given depth.
	//
	// Assumptions:
	//   1. There is no scaling in the view matrix.
	//   2. The horizontal FOV is 90 degrees.
	//   3. The backbuffer is 1280x720.
	//
	// If we project two points at (X,Y,Z) and (X',Y,Z) from view space, we get their screen
	// space positions: (X/Z, Y'/Z) and (X'/Z, Y'/Z) where Y' = Y * AspectRatio.
	//
	// The distance in screen space is then sqrt( (X'-X)^2/Z^2 + (Y'-Y')^2/Z^2 )
	// or (X'-X)/Z. This is in clip space, so PixelDist = 1280 * 0.5 * (X'-X)/Z.
	//
	// Solving for distance: X'-X = PixelDist * Z / 640 and Z is our view distance.
	const FLOAT ViewDistance = GetViewDistance();
	const FLOAT PixelError = (FLOAT)PixelErrorSlider->GetValue();
	const FLOAT MaxDeviation = PixelError * (ViewDistance / 640.0f);
	return MaxDeviation;
}

/**
 * Returns the view distance selected by the user.
 */
FLOAT WxDlgSimplifyLOD::GetViewDistance() const
{
	return (FLOAT)(Max<INT>( ViewDistanceSlider->GetValue(), 1 ));
}
