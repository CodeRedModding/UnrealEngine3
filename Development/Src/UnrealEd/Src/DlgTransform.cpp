/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DlgTransform.h"
#include "ScopedTransaction.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	WxDlgTransform
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE( WxDlgTransform, wxDialog )
	EVT_BUTTON( ID_DlgTransform_OnApply, WxDlgTransform::OnApplyTransform )
	EVT_CHECKBOX( ID_DlgTransform_DeltaCheck, WxDlgTransform::OnDeltaCheck )
END_EVENT_TABLE()

WxDlgTransform::WxDlgTransform(wxWindow* InParent)
	:	wxDialog( InParent, -1, *LocalizeUnrealEd("Transform"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE )
	,	TransformMode( TM_Translate )
	,	bIsDelta( FALSE )
{
	wxSizer* UberSizer = new wxBoxSizer(wxHORIZONTAL);
	{
		// X/Y/Z fields.
		wxSizer* FieldSizer = new wxBoxSizer(wxVERTICAL);
		{
			wxSizer* XSizer = new wxBoxSizer(wxHORIZONTAL);
			wxSizer* YSizer = new wxBoxSizer(wxHORIZONTAL);
			wxSizer* ZSizer = new wxBoxSizer(wxHORIZONTAL);
			{
				XLabel = new wxStaticText( this, wxID_STATIC, *LocalizeUnrealEd("X") );
				XSizer->Add( XLabel, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE|wxEXPAND, 5 );
				XEdit = new wxTextCtrl( this, -1, TEXT(""), wxDefaultPosition, wxSize(-1, -1) );
				XSizer->Add( XEdit, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

				YLabel = new wxStaticText( this, wxID_STATIC, *LocalizeUnrealEd("Y") );
				YSizer->Add( YLabel, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE|wxEXPAND, 5 );
				YEdit = new wxTextCtrl( this, -1, TEXT(""), wxDefaultPosition, wxSize(-1, -1) );
				YSizer->Add( YEdit, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

				ZLabel = new wxStaticText( this, wxID_STATIC, *LocalizeUnrealEd("Z") );
				ZSizer->Add( ZLabel, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE|wxEXPAND, 5 );
				ZEdit = new wxTextCtrl( this, -1, TEXT(""), wxDefaultPosition, wxSize(-1, -1) );
				ZSizer->Add( ZEdit, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );
			}
			FieldSizer->Add( XSizer, 0, wxALL, 5 );
			FieldSizer->Add( YSizer, 0, wxALL, 5 );
			FieldSizer->Add( ZSizer, 0, wxALL, 5 );
		}
		UberSizer->Add( FieldSizer, 0, wxALL, 5 );

		// OK button and Delta checkbox.
		wxSizer *ButtonSizer = new wxBoxSizer(wxVERTICAL);
		{
			ApplyButton = new wxButton(this, ID_DlgTransform_OnApply, *LocalizeUnrealEd("Apply"));
			ApplyButton->SetDefault();
			ButtonSizer->Add(ApplyButton, 0, wxALL, 5);
			DeltaCheck = new wxCheckBox( this, ID_DlgTransform_DeltaCheck, *LocalizeUnrealEd("Relative") );
			ButtonSizer->Add( DeltaCheck, 0, wxALL, 5 );
		}
		UberSizer->Add( ButtonSizer, 0, wxALL, 5);
	}

	SetSizer( UberSizer );
	SetAutoLayout( true );

	FWindowUtil::LoadPosSize( TEXT("DlgTransform"), this, -1, -1, 300, 165 );
}

WxDlgTransform::~WxDlgTransform()
{
	FWindowUtil::SavePosSize( TEXT("DlgTransform"), this );
}

/** Sets whether the dialog applies translation, rotation or scaling. */
void WxDlgTransform::SetTransformMode(ETransformMode InTransformMode)
{
	TransformMode = InTransformMode;
	UpdateLabels();
}

/** Updates text labels based on the current transform mode. */
void WxDlgTransform::UpdateLabels()
{
	FString XLabelString;
	FString YLabelString;
	FString ZLabelString;
	if ( TransformMode == TM_Rotate )
	{
		const FString TransformString = *LocalizeUnrealEd( "Rotate" );
		SetTitle( *TransformString );
		XLabelString = FString::Printf( TEXT("%s (%c)"), *LocalizeUnrealEd( "Pitch" ), 176 );
		YLabelString = FString::Printf( TEXT("%s (%c)"), *LocalizeUnrealEd( "Yaw" ), 176 );
		ZLabelString = FString::Printf( TEXT("%s (%c)"), *LocalizeUnrealEd( "Roll" ), 176 );
	}
	else
	{
		const FString TransformString = ( TransformMode == TM_Scale )
				? *LocalizeUnrealEd( "Scale" )
				: *LocalizeUnrealEd( "Translate" );
		SetTitle( *TransformString );
		XLabelString = FString::Printf( TEXT("%s X"), *TransformString );
		YLabelString = FString::Printf( TEXT("%s Y"), *TransformString );
		ZLabelString = FString::Printf( TEXT("%s Z"), *TransformString );
	}
	XLabel->SetLabel( *XLabelString );
	YLabel->SetLabel( *YLabelString );
	ZLabel->SetLabel( *ZLabelString );
	GetSizer()->SetSizeHints( this );
}

/** Applies the transform to selected actors. */
void WxDlgTransform::ApplyTransform()
{
	TArray<AActor*> ActorsToTransform;
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );
		if ( !Actor->bLockLocation )
		{
			ActorsToTransform.AddItem( Actor );
		}
	}

	if ( !ActorsToTransform.Num() )
	{
		return;
	}

	const FScopedTransaction Transaction( TEXT("Transform Actors") );

	// Translation
	if ( TransformMode == TM_Translate )
	{
		const FLOAT XPos = appAtof( XEdit->GetValue() );
		const FLOAT YPos = appAtof( YEdit->GetValue() );
		const FLOAT ZPos = appAtof( ZEdit->GetValue() );
		const FVector NewTranslation( XPos, YPos, ZPos );
		for ( INT ActorIndex = 0 ; ActorIndex < ActorsToTransform.Num() ; ++ActorIndex )
		{
			AActor* Actor = ActorsToTransform(ActorIndex);
			GEditor->ApplyDeltaToActor(
					Actor,
					bIsDelta,
					&NewTranslation,
					NULL,
					NULL );
		}
	}
	// Rotation
	else if ( TransformMode == TM_Rotate )
	{
		const FLOAT Pitch = appAtof( XEdit->GetValue() );
		const FLOAT Yaw = appAtof( YEdit->GetValue() );
		const FLOAT Roll = appAtof( ZEdit->GetValue() );
		const INT PitchR = appTrunc( 65536.f * (Pitch / 360.f) );
		const INT YawR = appTrunc( 65536.f * (Yaw / 360.f) );
		const INT RollR = appTrunc( 65536.f * (Roll / 360.f) );
		const FRotator NewRotation( PitchR, YawR, RollR );
		for ( INT ActorIndex = 0 ; ActorIndex < ActorsToTransform.Num() ; ++ActorIndex )
		{
			AActor* Actor = ActorsToTransform(ActorIndex);
			GEditor->ApplyDeltaToActor(
					Actor,
					bIsDelta,
					NULL,
					&NewRotation,
					NULL );
		}
	}
	// Scaling
	else if ( TransformMode == TM_Scale )
	{
		const FLOAT XScale = appAtof( XEdit->GetValue() );
		const FLOAT YScale = appAtof( YEdit->GetValue() );
		const FLOAT ZScale = appAtof( ZEdit->GetValue() );
		const FVector NewScale( XScale, YScale, ZScale );
		for ( INT ActorIndex = 0 ; ActorIndex < ActorsToTransform.Num() ; ++ActorIndex )
		{
			AActor* Actor = ActorsToTransform(ActorIndex);
			GEditor->ApplyDeltaToActor(
				Actor,
				bIsDelta,
				NULL,
				NULL,
				&NewScale );
		}
	}
	else
	{
		warnf( TEXT("WxDlgTransform::ApplyTransform() -- Unknown transform mode") );
	}

	// Route PostEditMove with bFinished=TRUE
	for ( INT ActorIndex = 0 ; ActorIndex < ActorsToTransform.Num() ; ++ActorIndex )
	{
		AActor* Actor = ActorsToTransform(ActorIndex);
		Actor->PostEditMove( TRUE );
	}

	// Fires CALLBACK_LevelDirtied when falling out of scope.
	FScopedLevelDirtied LevelDirtyCallback;
	LevelDirtyCallback.Request();

	// Redraw viewports.
	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}


/** Called when the 'Apply' button is clicked; applies transform to selected actors. */
void WxDlgTransform::OnApplyTransform(wxCommandEvent& In)
{
	ApplyTransform();
}

/** Called when the 'Delta' checkbox is clicked; toggles between absolute and relative transformation. */
void WxDlgTransform::OnDeltaCheck(wxCommandEvent& In)
{
	const bool bIsChecked = DeltaCheck->IsChecked();
	bIsDelta = bIsChecked ? TRUE : FALSE;
}
