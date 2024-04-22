/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DlgRename.h"

WxDlgRename::WxDlgRename()
{
	const bool bSuccess = wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_RENAME") );
	check( bSuccess );

	SetTitle( *LocalizeUnrealEd("Rename") );

	PGNPanel = (wxPanel*)FindWindow( XRCID( "ID_PKGGRPNAME" ) );
	check( PGNPanel != NULL );
	//wxSizer* szr = PGNPanel->GetSizer();

	PGNSizer = new wxBoxSizer(wxHORIZONTAL);
	PGNCtrl = new WxPkgGrpNameCtrl( PGNPanel, -1, PGNSizer, TRUE );
	const wxRect rc = PGNPanel->GetClientRect();
	PGNCtrl->SetSizer(PGNSizer);
	PGNCtrl->Show();
	PGNCtrl->SetSize( rc );
	PGNCtrl->SetAutoLayout(true);

	PGNPanel->SetAutoLayout(true);

	FLocalizeWindow( this );
}

WxDlgRename::~WxDlgRename()
{
	FWindowUtil::SavePosSize( TEXT("DlgRename"), this );
}

int WxDlgRename::ShowModal(const FString& InPackage, const FString& InGroup, const FString& InName)
{
	OldPackage = NewPackage = InPackage;
	OldGroup = NewGroup = InGroup;
	OldName = NewName = InName;

	PGNCtrl->PkgCombo->SetValue( *InPackage );
	PGNCtrl->GrpEdit->SetValue( *InGroup );
	PGNCtrl->NameEdit->SetValue( *InName );

	return wxDialog::ShowModal();
}

bool WxDlgRename::Validate()
{
	NewPackage = PGNCtrl->PkgCombo->GetValue();
	NewGroup = PGNCtrl->GrpEdit->GetValue();
	NewName = PGNCtrl->NameEdit->GetValue();

	FString	QualifiedName;
	if( NewGroup.Len() )
	{
		QualifiedName = NewPackage + TEXT(".") + NewGroup + TEXT(".") + NewName;
	}
	else
	{
		QualifiedName = NewPackage + TEXT(".") + NewName;
	}

	FString Reason;
	if (!FIsValidObjectName( *NewName, Reason )
	||	!FIsValidGroupName( *NewPackage, Reason )
	||	!FIsValidGroupName( *NewGroup, Reason, TRUE )
	||	!FIsUniqueObjectName(*QualifiedName, ANY_PACKAGE, Reason) )
	{
		appMsgf( AMT_OK, *Reason );
		return false;
	}

	return true;
}
