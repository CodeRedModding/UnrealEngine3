/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DlgActorFactory.h"
#include "PropertyWindow.h"

BEGIN_EVENT_TABLE(WxDlgActorFactory, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgActorFactory::OnOK )
	EVT_CLOSE( WxDlgActorFactory::OnClose )
END_EVENT_TABLE()

WxDlgActorFactory::WxDlgActorFactory()
{
	const bool bSuccess = wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_ACTOR_FACTORY") );
	check( bSuccess );

	NameText = (wxStaticText*)FindWindow( XRCID( "IDEC_NAME" ) );
	check( NameText != NULL );

	Factory = NULL;
	bIsReplaceOp = FALSE;

	FWindowUtil::LoadPosSize( TEXT("DlgActorFactory"), this );
	FLocalizeWindow( this );

	// Replace the placeholder window with a property window
	PropertyWindow = new WxPropertyWindowHost;
	PropertyWindow->Create( this, GUnrealEd );
	wxWindow* win = (wxWindow*)FindWindow( XRCID( "ID_PROPERTY_WINDOW" ) );
	check( win != NULL );
	const wxRect rc = win->GetRect();
	PropertyWindow->SetSize( rc );
	win->Show(0);
}

WxDlgActorFactory::~WxDlgActorFactory()
{
	FWindowUtil::SavePosSize( TEXT("DlgActorFactory"), this );
}

void WxDlgActorFactory::ShowDialog(UActorFactory* InFactory, UBOOL bInReplace)
{
	Factory = InFactory;
	bIsReplaceOp = bInReplace;
	PropertyWindow->SetObject( Factory, EPropertyWindowFlags::Sorted );
	NameText->SetLabel( *(Factory->MenuName) );

	wxDialog::Show( TRUE );
}

void WxDlgActorFactory::OnOK( wxCommandEvent& In )
{
	// Provide a default error value, in case the actor factor doesn't specify one.
	FString ActorErrorMsg( TEXT("Error_CouldNotCreateActor") );

	if( Factory->CanCreateActor( ActorErrorMsg ) )
	{
		if (bIsReplaceOp)
		{
			GEditor->ReplaceSelectedActors(Factory, NULL);
		}
		else
		{
			const UBOOL bIgnoreCanCreate = FALSE;
			const UBOOL bUseSurfaceOrientation = FALSE;

			// NOTE: We ignore the current selection as we presume that the factory dialog has been populated
			//		 with the desired object properties already.  Also, the current selection may not match
			//		 the desired object and we don't want it to overwrite the user's choice
			const UBOOL bUseCurrentSelection = FALSE;
			GEditor->UseActorFactory( Factory, bIgnoreCanCreate, bUseSurfaceOrientation, bUseCurrentSelection );
		}
	}
	else
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd( *ActorErrorMsg ) );
	}

	wxDialog::AcceptAndClose();
}

void WxDlgActorFactory::OnClose( wxCloseEvent& CloseEvent )
{
	if ( Factory != NULL )
	{
		// clear any factory references to the selected objects
		Factory->ClearFields();
	}

	this->Destroy();
}

