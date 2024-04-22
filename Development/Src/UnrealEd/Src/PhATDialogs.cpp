/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "UnrealEd.h"
#include "PhAT.h"
#include "EnginePhysicsClasses.h"

/*-----------------------------------------------------------------------------
	WxDlgNewPhysicsAsset.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgNewPhysicsAsset, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgNewPhysicsAsset::OnOK )
END_EVENT_TABLE()

WxDlgNewPhysicsAsset::WxDlgNewPhysicsAsset()
{
	const bool bSuccess = wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_NEWPHYSICSASSET") );
	check( bSuccess );

	MinSizeEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_MINBONESIZE" ) );
	check( MinSizeEdit != NULL );
	AlignBoneCheck = (wxCheckBox*)FindWindow( XRCID( "IDEC_ALONGBONE" ) );
	check( AlignBoneCheck != NULL );
	GeomTypeCombo = (wxComboBox*)FindWindow( XRCID( "IDEC_COLLISIONGEOM" ) );
	check( GeomTypeCombo != NULL );
	VertWeightCombo = (wxComboBox*)FindWindow( XRCID( "IDEC_VERTWEIGHT" ) );
	check( VertWeightCombo != NULL );
	MakeJointsCheck = (wxCheckBox*)FindWindow( XRCID( "IDEC_CREATEJOINTS" ) );
	check( MakeJointsCheck != NULL );
	WalkPastSmallCheck = (wxCheckBox*)FindWindow( XRCID( "IDEC_PASTSMALL" ) );
	check( WalkPastSmallCheck != NULL );
	BodyForAllCheck = (wxCheckBox*)FindWindow( XRCID( "IDEC_BODYFORALL" ) );
	check( BodyForAllCheck != NULL );
	OpenAssetNowCheck = (wxCheckBox*)FindWindow( XRCID( "IDEC_OPENNOW" ) );
	check( OpenAssetNowCheck != NULL );

	GeomTypeCombo->Append( *LocalizeUnrealEd("SphylSphere") );
	GeomTypeCombo->Append( *LocalizeUnrealEd("Box") );

	if(PHYSASSET_DEFAULT_GEOMTYPE == EFG_SphylSphere)
	{
		GeomTypeCombo->SetSelection(0);
	}
	else
	{
		GeomTypeCombo->SetSelection(1);
	}

	VertWeightCombo->Append( *LocalizeUnrealEd("DominantWeight") );
	VertWeightCombo->Append( *LocalizeUnrealEd("AnyWeight") );

	FLocalizeWindow( this );

	if(PHYSASSET_DEFAULT_VERTWEIGHT == EVW_DominantWeight)
	{
		VertWeightCombo->SetSelection(0);
	}
	else
	{
		VertWeightCombo->SetSelection(1);
	}
}

WxDlgNewPhysicsAsset::~WxDlgNewPhysicsAsset()
{
}

int WxDlgNewPhysicsAsset::ShowModal( USkeletalMesh* InMesh, UBOOL bReset )
{
	Mesh = InMesh;

	MinSizeEdit->SetValue( *FString::Printf(TEXT("%2.2f"), PHYSASSET_DEFAULT_MINBONESIZE) );
	AlignBoneCheck->SetValue( PHYSASSET_DEFAULT_ALIGNBONE );
	MakeJointsCheck->SetValue( PHYSASSET_DEFAULT_MAKEJOINTS );
	WalkPastSmallCheck->SetValue( PHYSASSET_DEFAULT_WALKPASTSMALL );
	BodyForAllCheck->SetValue( false );

	OpenAssetNowCheck->SetValue( true );

	if(bReset)
	{
		OpenAssetNowCheck->Disable();
	}

	return wxDialog::ShowModal();
}

void WxDlgNewPhysicsAsset::OnOK( wxCommandEvent& In )
{
	Params.bAlignDownBone = AlignBoneCheck->GetValue();
	Params.bCreateJoints = MakeJointsCheck->GetValue();
	Params.bWalkPastSmall = WalkPastSmallCheck->GetValue();
	Params.bBodyForAll = BodyForAllCheck->GetValue();

	double SizeNum;
	UBOOL bIsNumber = MinSizeEdit->GetValue().ToDouble(&SizeNum);
	if(!bIsNumber)
		SizeNum = PHYSASSET_DEFAULT_MINBONESIZE;
	Params.MinBoneSize = SizeNum;

	if(GeomTypeCombo->GetSelection() == 0)
		Params.GeomType = EFG_SphylSphere;
	else
		Params.GeomType = EFG_Box;

	if(VertWeightCombo->GetSelection() == 0)
		Params.VertWeight = EVW_DominantWeight;
	else
		Params.VertWeight = EVW_AnyWeight;

	bOpenAssetNow = OpenAssetNowCheck->GetValue();

	wxDialog::AcceptAndClose();
}

/*-----------------------------------------------------------------------------
	WxBodyContextMenu
-----------------------------------------------------------------------------*/
WxBodyContextMenu::WxBodyContextMenu(WxPhAT* AssetEditor)
{
	check(AssetEditor);
	check(AssetEditor->SelectedBodyIndex != INDEX_NONE);
	check(AssetEditor->PhysicsAsset);

	URB_BodySetup* BS = AssetEditor->PhysicsAsset->BodySetup(AssetEditor->SelectedBodyIndex);
	URB_BodyInstance* BI = AssetEditor->PhysicsAsset->DefaultInstance->Bodies(AssetEditor->SelectedBodyIndex);
		
	if(BS->bFixed)
	{
		Append( IDM_PHAT_TOGGLEFIXED, *LocalizeUnrealEd("UnfixBody"), TEXT("") );
	}
	else
	{
		Append( IDM_PHAT_TOGGLEFIXED, *LocalizeUnrealEd("FixBody"), TEXT("") );
	}

	Append( IDM_PHAT_FIXBELOW, *LocalizeUnrealEd("FixBodyBelow"), TEXT("") );
	Append( IDM_PHAT_UNFIXBELOW, *LocalizeUnrealEd("UnfixBodyBelow"), TEXT("") );

	Append( IDM_PHAT_DELETEBELOW, *LocalizeUnrealEd("DeleteBodyBelow"), TEXT("") );
}

WxBodyContextMenu::~WxBodyContextMenu()
{
}


/*-----------------------------------------------------------------------------
	WxConstraintContextMenu
-----------------------------------------------------------------------------*/
WxConstraintContextMenu::WxConstraintContextMenu(WxPhAT* AssetEditor)
{
	check(AssetEditor);
	check(AssetEditor->SelectedConstraintIndex != INDEX_NONE);
	check(AssetEditor->PhysicsAsset);

	URB_ConstraintSetup* CS = AssetEditor->PhysicsAsset->ConstraintSetup(AssetEditor->SelectedConstraintIndex);
	URB_ConstraintInstance* CI = AssetEditor->PhysicsAsset->DefaultInstance->Constraints(AssetEditor->SelectedConstraintIndex);

	if(CI->bSwingPositionDrive && CI->bTwistPositionDrive)
	{
		Append( IDM_PHAT_TOGGLEMOTORISE, *LocalizeUnrealEd("Unmotorise"), TEXT("") );
	}
	else
	{
		Append( IDM_PHAT_TOGGLEMOTORISE, *LocalizeUnrealEd("Motorise"), TEXT("") );
	}

	Append( IDM_PHAT_MOTORISEBELOW, *LocalizeUnrealEd("MotoriseBelow"), TEXT("") );
	Append( IDM_PHAT_UNMOTORISEBELOW, *LocalizeUnrealEd("UnmotoriseBelow"), TEXT("") );
}

WxConstraintContextMenu::~WxConstraintContextMenu()
{
}

