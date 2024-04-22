/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DlgBuildProgress.h"
#include "DlgLightingResults.h"

BEGIN_EVENT_TABLE(WxDlgBuildProgress, wxDialog)
	EVT_BUTTON( ID_DialogBuildProgress_StopBuild, WxDlgBuildProgress::OnStopBuild )
END_EVENT_TABLE()

WxDlgBuildProgress::WxDlgBuildProgress(wxWindow* InParent) : 
wxDialog(InParent, wxID_ANY, TEXT("DlgBuildProgressTitle"), wxDefaultPosition, wxDefaultSize, wxCAPTION )
{
	static const INT MinDialogWidth = 400;

	VerticalSizer = new wxBoxSizer(wxVERTICAL);
	{
		VerticalSizer->SetMinSize(wxSize(MinDialogWidth, -1));

		wxStaticBoxSizer* BuildStatusSizer = new wxStaticBoxSizer(wxHORIZONTAL, this, TEXT("BuildStatus"));
		{
			BuildStatusTime = new wxStaticText(this, wxID_ANY, TEXT(""));
			BuildStatusSizer->Add(BuildStatusTime, 0, wxEXPAND | wxALL, 1);

			BuildStatusText = new wxStaticText(this, wxID_ANY, TEXT("Default Status Text"));
			BuildStatusSizer->Add(BuildStatusText, 1, wxEXPAND | wxALL, 1);
		}
		VerticalSizer->Add(BuildStatusSizer, 0, wxEXPAND | wxALL, 5);

		wxStaticBoxSizer* BuildProgressSizer = new wxStaticBoxSizer(wxVERTICAL, this, TEXT("BuildProgress"));
		{
			BuildProgress = new wxGauge(this, wxID_ANY, 100);
			BuildProgressSizer->Add(BuildProgress, 1, wxEXPAND);
		}
		VerticalSizer->Add(BuildProgressSizer, 0, wxEXPAND | wxALL, 5);

		DetailsSizer = new wxStaticBoxSizer(wxVERTICAL, this, TEXT(""));
		{
			StopBuildButton = new wxButton(this, ID_DialogBuildProgress_StopBuild, TEXT("StopBuild"));
			DetailsSizer->Add(StopBuildButton, 0, wxCENTER | wxALL, 5);
		}
		VerticalSizer->Add(DetailsSizer, 0, wxEXPAND | wxALL, 5);

	}
	SetSizer(VerticalSizer);

	FWindowUtil::LoadPosSize(TEXT("DlgBuildProgress2"), this, 760, 475, 406, 200);
	
	FLocalizeWindow( this );
}

WxDlgBuildProgress::~WxDlgBuildProgress()
{
	FWindowUtil::SavePosSize( TEXT("DlgBuildProgress"), this );
}

/**
 * Displays the build progress dialog.
 */
void WxDlgBuildProgress::ShowDialog(EBuildType InBuildType)
{
	// Reset progress indicators
	BuildStartTime = -1;
	StopBuildButton->Enable( TRUE );
	SetBuildType(InBuildType);
	SetBuildStatusText(TEXT(""));
	SetBuildProgressPercent( 0, 100 );
	Show();
}

/**
 *	Sets the current build type.
 *	@param	InBuildType		The build that is occurring.
 */
void WxDlgBuildProgress::SetBuildType(EBuildType InBuildType)
{
	BuildType = InBuildType;
}

/**
 * Assembles a string containing the elapsed build time.
 */
FString WxDlgBuildProgress::BuildElapsedTimeString() const
{
	// Display elapsed build time.
	const DOUBLE ElapsedBuildTimeSeconds = appSeconds() - BuildStartTime;
	return appPrettyTime( ElapsedBuildTimeSeconds );
}

/**
 * Updates the label displaying the current time.
 */
void WxDlgBuildProgress::UpdateTime()
{
	BuildStatusTime->SetLabel(*BuildElapsedTimeString());
}

/**
 * Sets the text that describes what part of the build we are currently on.
 *
 * @param StatusText	Text to set the status label to.
 */
void WxDlgBuildProgress::SetBuildStatusText( const TCHAR* StatusText )
{
	UpdateTime();
	const UBOOL bStoppingBuild = !StopBuildButton->IsEnabled();
	
	// Only update the text if we haven't cancelled the build.
	if( !bStoppingBuild )
	{
		const wxString CurrentValue = BuildStatusText->GetLabel();
		const UBOOL bLabelChanged = CurrentValue.Len() != static_cast<size_t>(appStrlen(StatusText))
								||	CurrentValue != StatusText;
		if( bLabelChanged )
		{
			BuildStatusText->SetLabel( StatusText );
		}
	}
}


/**
 * Sets the build progress bar percentage. Specifying 0 means INFINITE.
 *
 *	@param ProgressNumerator		Numerator for the progress meter (its current value).
 *	@param ProgressDenominitator	Denominiator for the progress meter (its range). Specifying 0 means INFINITE.
 */
void WxDlgBuildProgress::SetBuildProgressPercent( INT ProgressNumerator, INT ProgressDenominator )
{
	UpdateTime();
	const UBOOL bStoppingBuild = !StopBuildButton->IsEnabled();

	// Only update the progress bar if we haven't cancelled the build.
	if( !bStoppingBuild )
	{
		if( ProgressDenominator == 0 )
		{
			// Marquee-style progress bars seem to require a specific range to be set
			if( BuildProgress->GetRange() != 100 )
			{
				BuildProgress->SetRange( 100 );
			}
			if( BuildProgress->GetValue() != 0 )
			{
				BuildProgress->SetValue( 0 );
			}

			// No progress denominator, so use 'indeterminate mode'
			BuildProgress->SetIndeterminateMode();

			BuildProgress->Pulse();

			//@TODO: Figure out why sending PBM_SETMARQUEE to BuildProgress doesn't work.
			//       Use that instead of BuildProgress->Pulse().
		}
		else
		{
			BuildProgress->SetDeterminateMode();

  			if( BuildProgress->GetRange() != ProgressDenominator )
			{
				BuildProgress->SetRange( ProgressDenominator );
			}
			
			BuildProgress->SetValue( ProgressNumerator );

			// Force a repaint so the user sees a smoother progress bar
			BuildProgress->Update();
		}
	}
}

/**
 * Records the application time in seconds; used in display of elapsed build time.
 */
void WxDlgBuildProgress::MarkBuildStartTime()
{
	BuildStartTime = appSeconds();
}


/**
 * Callback for the Stop Build button, stops the current build.
 */
void WxDlgBuildProgress::OnStopBuild(wxCommandEvent& In )
{
	GApp->SetMapBuildCancelled( TRUE );
	
	SetBuildStatusText( *LocalizeUnrealEd("StoppingMapBuild") );

	StopBuildButton->Enable( FALSE );
}
