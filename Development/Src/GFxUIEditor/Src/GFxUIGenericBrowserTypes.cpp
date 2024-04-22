/**********************************************************************

Filename    :   GFxUIGenericBrowserTypes.cpp
Content     :   Browser entries and dialogs for Swf movies.

Copyright   :   (c) 2001-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Notes       :   

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING 
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/


#include "GFxUIEditor.h"

#if WITH_GFx

#include "GFxUIClasses.h"
#include "GFxUIEditorDialogs.h"
#include "PropertyWindow.h"

/** Copied from GenericBrowserTypes.cpp */
static inline void CreateNewPropertyWindowFrame()
{
	if(!GApp->ObjectPropertyWindow)
	{
		GApp->ObjectPropertyWindow = new WxPropertyWindowFrame;
		GApp->ObjectPropertyWindow->Create( GApp->EditorFrame, -1, GUnrealEd );
		GApp->ObjectPropertyWindow->SetSize( 64,64, 500,300 );
	}
}

/*-----------------------------------------------------------------------------
GFxUIWxDlgCreateInstance.
Like WxDlgPackageGroupName, but also choose class to instantiate.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(GFxUIWxDlgCreateInstance, wxDialog)
EVT_BUTTON( wxID_OK, GFxUIWxDlgCreateInstance::OnOK )
END_EVENT_TABLE()

GFxUIWxDlgCreateInstance::GFxUIWxDlgCreateInstance()
{
	wxDialog::Create(NULL, wxID_ANY, TEXT("PackageGroupNameClass"), wxDefaultPosition, wxDefaultSize );

	wxBoxSizer* HorizontalSizer = new wxBoxSizer(wxHORIZONTAL);
	{
		wxBoxSizer* InfoStaticBoxSizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Info"));
		{
			PGNCtrl = new WxPkgGrpNameCtrl( this, -1, NULL );
			PGNCtrl->SetSizer(PGNCtrl->FlexGridSizer);
			InfoStaticBoxSizer->Add(PGNCtrl, 3, wxEXPAND);

			wxFlexGridSizer* ClassSizer = new wxFlexGridSizer(1,2,0,0);
			{
				wxStaticText* ClassLabel = new wxStaticText( this, wxID_STATIC, *LocalizeUnrealEd("Class"));
				ClassSizer->Add( ClassLabel, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE|wxEXPAND, 5 );

				ClassCtrl = new wxComboBox( this, IDCB_GFX_CLASS, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY|wxCB_DROPDOWN|wxCB_SORT );
				ClassSizer->Add(ClassCtrl, 0, wxALIGN_LEFT|wxGROW|wxALL, 5 );
			}
			InfoStaticBoxSizer->Add(ClassSizer, 1, wxEXPAND);
		}
		HorizontalSizer->Add(InfoStaticBoxSizer, 1, wxALIGN_TOP|wxALL|wxEXPAND, 5);

		wxBoxSizer* ButtonSizer = new wxBoxSizer(wxVERTICAL);
		{
			wxButton* ButtonOK = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxDefaultSize, 0 );
			ButtonOK->SetDefault();
			ButtonSizer->Add(ButtonOK, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

			wxButton* ButtonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
			ButtonSizer->Add(ButtonCancel, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
		}
		HorizontalSizer->Add(ButtonSizer, 0, wxALIGN_TOP|wxALL, 5);

	}
	SetSizer(HorizontalSizer);

	FWindowUtil::LoadPosSize( TEXT("DlgPackageGroupNameClass"), this );
	GetSizer()->Fit(this);

	FLocalizeWindow( this );
}

GFxUIWxDlgCreateInstance::~GFxUIWxDlgCreateInstance()
{
	FWindowUtil::SavePosSize( TEXT("DlgPackageGroupNameClass"), this );
}

int GFxUIWxDlgCreateInstance::ShowModal(const FString& InPackage, const FString& InGroup, const FString& InName, UClass* Base)
{
	Package = InPackage;
	Group = InGroup;
	Name = InName;

	ClassCtrl->Clear();
	ClassCtrl->Select(ClassCtrl->Append(*Base->GetName(), Base));
	for( TObjectIterator<UClass> It ; It ; ++It )
	{
		if (*It != Base && It->IsChildOf(Base) && It->HasAnyClassFlags(CLASS_Abstract) == FALSE)
			ClassCtrl->Append(*It->GetName(), *It);
	}

	PGNCtrl->PkgCombo->SetValue( *InPackage );
	PGNCtrl->GrpEdit->SetValue( *InGroup );
	PGNCtrl->NameEdit->SetValue( *InName );

	return wxDialog::ShowModal();
}

bool GFxUIWxDlgCreateInstance::Validate()
{
	bool bResult = wxDialog::Validate();

	if ( bResult )
	{
		// validate that the object name is valid
		//@todo ronp - move this functionality to a WxTextValidator
		FString Reason;
		if( !FIsValidObjectName( *Name, Reason )
			||	!FIsValidGroupName( *Package, Reason )
			||	!FIsValidGroupName( *Group, Reason, TRUE ) )
		{
			appMsgf( AMT_OK, *Reason );
			bResult = false;
		}
	}

	return bResult;
}

void GFxUIWxDlgCreateInstance::OnOK( wxCommandEvent& In )
{
	Package = PGNCtrl->PkgCombo->GetValue();
	Group = PGNCtrl->GrpEdit->GetValue();
	Name = PGNCtrl->NameEdit->GetValue();
	Class = (UClass*)ClassCtrl->GetClientData(ClassCtrl->GetSelection());

	wxDialog::SetAffirmativeId( In.GetId() ); // not certain if need this
	wxDialog::AcceptAndClose();
}

/*------------------------------------------------------------------------------
UGFxGenericBrowserType_GFxMovie - Defines a resource handler for SWF movie data.
------------------------------------------------------------------------------*/

//declared in generic browser types.
void AddSourceAssetCommands( TArray<FObjectSupportedCommandType>& OutCommands, UBOOL bEnabled );
void ExploreSourceAssetFolder( const FString& SourceFileName );
void OpenSourceAssetInExternalEditor( const FString& SourceFileName );

IMPLEMENT_CLASS(UGenericBrowserType_GFxMovie);

/**
* Does any initial set up that the type requires.
*/
void UGenericBrowserType_GFxMovie::Init()
{
	// TODO - add custom color (GFx, etc.)

    SupportInfo.AddItem(FGenericBrowserTypeInfo(USwfMovie::StaticClass(), FColor(200,128,128), 0, this));
}

/**
* Invokes the editor for an object.  
*
* @param	InObject	The object to invoke the editor for.
*/
UBOOL UGenericBrowserType_GFxMovie::ShowObjectEditor(UObject* InObject)
{
	CreateNewPropertyWindowFrame();
	GApp->ObjectPropertyWindow->SetObject( InObject,
		EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories );
	GApp->ObjectPropertyWindow->Show();
	return TRUE;
}

UBOOL UGenericBrowserType_GFxMovie::ShowObjectEditor()
{
	return 0;
}

UBOOL UGenericBrowserType_GFxMovie(UObject* InObject)
{
	return 0;
}

void UGenericBrowserType_GFxMovie::InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects )
{
	for( int i = 0; i < InObjects.Num(); i++)
	{
		UObject* InObject = InObjects(i);

		if(InCommand == IDMN_ObjectContext_Reimport)
		{
			if (!FReimportManager::Instance()->Reimport(InObject))
			{
				appMsgf(AMT_OK, TEXT("Error reimporting Flash movie \"%s\"."), *Cast<USwfMovie>(InObject)->SourceFile);
			}
		}
		else if(InCommand == IDMN_ObjectContext_FindSourceInExplorer)
		{
			USwfMovie* MovieInfo = Cast<USwfMovie>(InObject);
			if( MovieInfo && MovieInfo->SourceFile.Len() && (GFileManager->FileSize( *( MovieInfo->SourceFile ) ) != INDEX_NONE) )
			{
				ExploreSourceAssetFolder(MovieInfo->SourceFile);
			}
		}
		else if (InCommand == IDMN_ObjectContext_OpenSourceInExternalEditor)
		{
			USwfMovie* MovieInfo = Cast<USwfMovie>(InObject);
			if( MovieInfo && MovieInfo->SourceFile.Len() && (GFileManager->FileSize( *( MovieInfo->SourceFile ) ) != INDEX_NONE) )
			{
				FFilename TempFileName = MovieInfo->SourceFile;
				FString FlaFileName = TempFileName.GetBaseFilename(FALSE);
				FlaFileName += TEXT(".fla");
				OpenSourceAssetInExternalEditor(FlaFileName);
			}
		}
	}
}

void UGenericBrowserType_GFxMovie::QuerySupportedCommands( USelection* InObjects, TArray<FObjectSupportedCommandType>& OutCommands ) const
{
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Properties, *LocalizeUnrealEd( "ObjectContext_EditProperties" ) ) );
	
	OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Reimport, *LocalizeUnrealEd(TEXT("Reimport")) ) );

	// Determine if any of the selected skeletal meshes have a source asset that exists on disk; used to decide if the source asset
	// commands should be enabled or not
	UBOOL bHaveSourceAsset = FALSE;
	for( USelection::TObjectConstIterator It( InObjects->ObjectConstItor() ); It; ++It )
	{
		UObject* Object = *It;

		USwfMovie* MovieInfo = Cast<USwfMovie>(Object);
		if( MovieInfo && MovieInfo->SourceFile.Len() && (GFileManager->FileSize( *( MovieInfo->SourceFile ) ) != INDEX_NONE) )
		{
			bHaveSourceAsset = TRUE;
			break;
		}
	}

	AddSourceAssetCommands( OutCommands, bHaveSourceAsset );
}

void UGenericBrowserType_GFxMovie::DoubleClick()
{

}



#endif // WITH_GFx