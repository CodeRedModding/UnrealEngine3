//! @file SubstanceAirEdCreateImageInputDlg.cpp
//! @brief Substance Air new graph instance dialog box
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include "UnrealEd.h"
#include "BusyCursor.h"
#include "SubstanceAirEdCreateImageInputDlg.h"


BEGIN_EVENT_TABLE(WxDlgCreateImageInput, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgCreateImageInput::OnOK )
END_EVENT_TABLE()


WxDlgCreateImageInput::WxDlgCreateImageInput():
	
	InstanceOuter(NULL),
	PGNSizer(NULL),
	PGNPanel(NULL),
	PGNCtrl(NULL)
{
	const bool bSuccess =
		wxXmlResource::Get()->LoadDialog(
			this,
			GApp->EditorFrame,
			TEXT("ID_DLG_SBS_AIR_NEW_IMAGE_INPUT"));
	check(bSuccess);

	PGNPanel = (wxPanel*)FindWindow( XRCID( "ID_PKGGRPNAME" ) );
	check( PGNPanel != NULL );

	PGNSizer = new wxBoxSizer(wxHORIZONTAL);
	{
		PGNCtrl = new WxPkgGrpNameCtrl( PGNPanel, -1, NULL, TRUE );
		PGNCtrl->SetSizer(PGNCtrl->FlexGridSizer);
		PGNSizer->Add(PGNCtrl, 1, wxEXPAND);
	}
	PGNPanel->SetSizer(PGNSizer);
	PGNPanel->SetAutoLayout(true);
	PGNPanel->Layout();

	//Has to be done before property window is created
	FLocalizeWindow( this );

	wxRect ThisRect = GetRect();
	ThisRect.width = 500;
	ThisRect.height = 200;
	SetSize( ThisRect );
}


int WxDlgCreateImageInput::ShowModal(const FString& InName, const FString& InPackage, const FString& InGroup)
{
	InstanceName = InName;
	Package = InPackage;
	Group = InGroup;

	PGNCtrl->PkgCombo->SetValue( *Package );
	PGNCtrl->GrpEdit->SetValue( *Group );
	PGNCtrl->NameEdit->SetValue( *InName );

	if (GIsUnitTesting)
	{
		EvaluatePackageAndGroup();
		return wxID_OK;
	}

	Refresh();

	return wxDialog::ShowModal();
}


void WxDlgCreateImageInput::EvaluatePackageAndGroup()
{
	Package = PGNCtrl->PkgCombo->GetValue();
	Group = PGNCtrl->GrpEdit->GetValue();
	InstanceName = PGNCtrl->NameEdit->GetValue();
	
	FString	QualifiedName;
	if (Group.Len())
	{
		QualifiedName = Package + TEXT(".") + Group + TEXT(".") + InstanceName;
	}
	else if (Package.Len())
	{
		QualifiedName = Package + TEXT(".") + InstanceName;
	}

	FString Reason;
	if (!FIsValidObjectName( *InstanceName, Reason ))
	{
		if (QualifiedName.Len())
		{
			appMsgf( AMT_OK, *FString::Printf(LocalizeSecure(LocalizeUnrealEd(TEXT("Error_ImportFailed_f")), *(InstanceName + TEXT(": ") + Reason))) );
		}
		else
		{
			appMsgf( AMT_OK, *FString::Printf(LocalizeSecure(LocalizeUnrealEd(TEXT("Error_ImportFailed_f")), *(QualifiedName + TEXT(": ") + Reason))) );
		}
		
		return;
	}

	if (QualifiedName.Len())
	{
		if (!FIsValidGroupName( *Package, Reason ) ||
			!FIsValidGroupName( *Group, Reason, TRUE))
		{
			appMsgf( AMT_OK, *FString::Printf(
				LocalizeSecure(
					LocalizeUnrealEd(
						TEXT("Error_ImportFailed_f")),
						*(QualifiedName + TEXT(": ") + Reason))) );
			return;
		}
	}

	UPackage* Pkg = NULL;

	if (Package.Len())
	{
		Pkg = GEngine->CreatePackage(NULL,*Package);
		if( Group.Len() )
		{
			Pkg = GEngine->CreatePackage(Pkg,*Group);
		}
	}

	if( Pkg && !Pkg->IsFullyLoaded() )
	{	
		// Ask user to fully load
		if(appMsgf( AMT_YesNo, *FString::Printf( 
				LocalizeSecure(
					LocalizeUnrealEd(
						TEXT("NeedsToFullyLoadPackageF")),
						*Pkg->GetName(), *LocalizeUnrealEd("Import")) ) ) )
		{
			// Fully load package.
			const FScopedBusyCursor BusyCursor;
			GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("FullyLoadingPackages")), TRUE );
			Pkg->FullyLoad();
			GWarn->EndSlowTask();
		}
		// User declined abort operation.
		else
		{
			appDebugMessagef(TEXT("Aborting operation as %s was not fully loaded."),*Pkg->GetName());
			return;
		}
	}

	InstanceOuter = Pkg;

	if( IsModal() )
	{
		EndModal( wxID_OK );
	}
}
