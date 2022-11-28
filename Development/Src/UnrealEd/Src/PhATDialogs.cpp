#include "UnrealEd.h"
#include "EnginePhysicsClasses.h"

/*-----------------------------------------------------------------------------
	WxDlgNewPhysicsAsset.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgNewPhysicsAsset, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgNewPhysicsAsset::OnOK )
END_EVENT_TABLE()

WxDlgNewPhysicsAsset::WxDlgNewPhysicsAsset()
	: wxDialog( )
{
	wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_NEWPHYSICSASSET") );

	PackageEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_PACKAGE" ) );
	NameEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_NAME" ) );
	MinSizeEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_MINBONESIZE" ) );
	AlignBoneCheck = (wxCheckBox*)FindWindow( XRCID( "IDEC_ALONGBONE" ) );
	GeomTypeCombo = (wxComboBox*)FindWindow( XRCID( "IDEC_COLLISIONGEOM" ) );
	VertWeightCombo = (wxComboBox*)FindWindow( XRCID( "IDEC_VERTWEIGHT" ) );
	MakeJointsCheck = (wxCheckBox*)FindWindow( XRCID( "IDEC_CREATEJOINTS" ) );
	WalkPastSmallCheck = (wxCheckBox*)FindWindow( XRCID( "IDEC_PASTSMALL" ) );
	OpenAssetNowCheck = (wxCheckBox*)FindWindow( XRCID( "IDEC_OPENNOW" ) );

	GeomTypeCombo->Append( TEXT("Sphyl/Sphere") );
	GeomTypeCombo->Append( TEXT("Box") );

	if(PHYSASSET_DEFAULT_GEOMTYPE == EFG_SphylSphere)
		GeomTypeCombo->SetSelection(0);
	else
		GeomTypeCombo->SetSelection(1);

	VertWeightCombo->Append( TEXT("Dominant Weight") );
	VertWeightCombo->Append( TEXT("Any Weight") );

	if(PHYSASSET_DEFAULT_VERTWEIGHT == EVW_DominantWeight)
		VertWeightCombo->SetSelection(0);
	else
		VertWeightCombo->SetSelection(1);
}

WxDlgNewPhysicsAsset::~WxDlgNewPhysicsAsset()
{
}

int WxDlgNewPhysicsAsset::ShowModal( FString InPackage, FString InName, USkeletalMesh* InMesh, UBOOL bReset )
{
	Package = InPackage;
	Mesh = InMesh;
	Name = InName;

	PackageEdit->SetValue( *Package );
	NameEdit->SetValue( *Name );

	MinSizeEdit->SetValue( *FString::Printf(TEXT("%2.2f"), PHYSASSET_DEFAULT_MINBONESIZE) );
	AlignBoneCheck->SetValue( PHYSASSET_DEFAULT_ALIGNBONE );
	MakeJointsCheck->SetValue( PHYSASSET_DEFAULT_MAKEJOINTS );
	WalkPastSmallCheck->SetValue( PHYSASSET_DEFAULT_WALKPASTSMALL );

	OpenAssetNowCheck->SetValue( true );

	if(bReset)
	{
		PackageEdit->Disable();
		NameEdit->Disable();
		OpenAssetNowCheck->Disable();
	}

	return wxDialog::ShowModal();
}

void WxDlgNewPhysicsAsset::OnOK( wxCommandEvent& In )
{
	Package = PackageEdit->GetValue();
	Name = NameEdit->GetValue();

	Params.bAlignDownBone = AlignBoneCheck->GetValue();
	Params.bCreateJoints = MakeJointsCheck->GetValue();
	Params.bWalkPastSmall = WalkPastSmallCheck->GetValue();

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

	wxDialog::OnOK( In );
}


/*-----------------------------------------------------------------------------
	WxDlgSetAssetPhysMaterial.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgSetAssetPhysMaterial, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgSetAssetPhysMaterial::OnOK )
END_EVENT_TABLE()

WxDlgSetAssetPhysMaterial::WxDlgSetAssetPhysMaterial()
	: wxDialog( )
{
	wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_SETPHYSMATERIAL_DIALOG") );

	PhysMaterialCombo = (wxComboBox*)FindWindow( XRCID( "IDEC_PHYSMATERIAL" ) );

	PhysMaterialClass = NULL;
}

WxDlgSetAssetPhysMaterial::~WxDlgSetAssetPhysMaterial()
{
}

int WxDlgSetAssetPhysMaterial::ShowModal()
{
	for( TObjectIterator<UClass> It; It; ++It )
	{
		if( It->IsChildOf(UPhysicalMaterial::StaticClass()) )
		{
			UClass* MatClass = *It;
			PhysMaterialCombo->Append( MatClass->GetName() );
		}
	}

	PhysMaterialCombo->SetSelection(0);

	return wxDialog::ShowModal();
}

void WxDlgSetAssetPhysMaterial::OnOK( wxCommandEvent& In )
{
	FString ChosenClassName = PhysMaterialCombo->GetValue();

	for( TObjectIterator<UClass> It; It && !PhysMaterialClass; ++It )
	{
		if( It->IsChildOf(UPhysicalMaterial::StaticClass()) )
		{
			UClass* MatClass = *It;
			if( ChosenClassName == FString(MatClass->GetName()) )
				PhysMaterialClass = MatClass;
		}
	}

	wxDialog::OnOK( In );
}

/*-----------------------------------------------------------------------------
	WxDlgSetAssetDisableParams.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgSetAssetDisableParams, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgSetAssetDisableParams::OnOK )
END_EVENT_TABLE()

WxDlgSetAssetDisableParams::WxDlgSetAssetDisableParams()
	: wxDialog( )
{
	wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_SETASSETDISABLING") );

	LinVelEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_LINVEL" ) );
	LinAccEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_LINACC" ) );
	AngVelEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_ANGVEL" ) );
	AngAccEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_ANGACC" ) );
}

WxDlgSetAssetDisableParams::~WxDlgSetAssetDisableParams()
{
}

int WxDlgSetAssetDisableParams::ShowModal(FLOAT InLinVel, FLOAT InLinAcc, FLOAT InAngVel, FLOAT InAngAcc)
{
	LinVelEdit->SetValue( *FString::Printf(TEXT("%2.3f"), InLinVel) );
	LinVel = InLinVel;

	LinAccEdit->SetValue( *FString::Printf(TEXT("%2.3f"), InLinAcc) );
	LinAcc = InLinAcc;

	AngVelEdit->SetValue( *FString::Printf(TEXT("%2.3f"), InAngVel) );
	AngVel = InAngVel;

	AngAccEdit->SetValue( *FString::Printf(TEXT("%2.3f"), InAngAcc) );
	AngAcc = InAngAcc;

	return wxDialog::ShowModal();
}

void WxDlgSetAssetDisableParams::OnOK( wxCommandEvent& In )
{
	double Num;

	if( LinVelEdit->GetValue().ToDouble(&Num) )
		LinVel = Num;

	if( LinAccEdit->GetValue().ToDouble(&Num) )
		LinAcc = Num;

	if( AngVelEdit->GetValue().ToDouble(&Num) )
		AngVel = Num;

	if( AngAccEdit->GetValue().ToDouble(&Num) )
		AngAcc = Num;

	wxDialog::OnOK( In );
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
