/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"

#if WITH_SIMPLYGON

#include "EngineMeshClasses.h"
#include "SkeletalMeshSimplificationWindow.h"
#include "AnimSetViewer.h"
#include "SimplygonMeshUtilities.h"

BEGIN_EVENT_TABLE( WxSkeletalMeshSimplificationWindow, wxPanel )
	EVT_BUTTON( ID_MeshSimp_SimplifyButton, WxSkeletalMeshSimplificationWindow::OnSimplifyButton )
	EVT_CHECKBOX( ID_MeshSimp_RecalcNormalsCheckBox, WxSkeletalMeshSimplificationWindow::OnRecalcNormalsCheckBox )
END_EVENT_TABLE()

/** Constructor that takes a pointer to the owner object */
WxSkeletalMeshSimplificationWindow::WxSkeletalMeshSimplificationWindow( WxAnimSetViewer* InAnimSetViewer, wxWindow* Parent )
	: wxScrolledWindow( Parent )
	, AnimSetViewer( InAnimSetViewer )
{
	check( AnimSetViewer );

	// @todo: Should be a global constant for the editor
	const INT BorderSize = 5;

	// Setup controls
	wxBoxSizer* TopVSizer = new wxBoxSizer( wxVERTICAL );
	{
		// Vertical space
		TopVSizer->AddSpacer( BorderSize );

		// Original mesh stats group
		wxStaticBoxSizer* OrigMeshStatsGroupVSizer = new wxStaticBoxSizer( wxVERTICAL, this, *LocalizeUnrealEd( "MeshSimp_OrigMeshStatsGroup" ) );
		{
			// Vertical space
			OrigMeshStatsGroupVSizer->AddSpacer( BorderSize );

			// Triangle count label
			OriginalTriCountLabel = new wxStaticText( this,	-1, wxString() );
			OrigMeshStatsGroupVSizer->Add( OriginalTriCountLabel, 0, wxALL, BorderSize );

			// Vertex count label
			OriginalVertCountLabel = new wxStaticText( this, -1, wxString() );
			OrigMeshStatsGroupVSizer->Add( OriginalVertCountLabel, 0, wxALL, BorderSize );

			// Vertical space
			OrigMeshStatsGroupVSizer->AddSpacer( BorderSize );

			TopVSizer->Add( OrigMeshStatsGroupVSizer, 0, wxEXPAND | wxALL, BorderSize );
		}

		// Simplify by desired quality group
		wxStaticBoxSizer* QualityGroupVSizer = new wxStaticBoxSizer( wxVERTICAL, this, *LocalizeUnrealEd( "MeshSimp_DesiredQuality" ) );
		{
			// Vertical space
			QualityGroupVSizer->AddSpacer( BorderSize );

			// Quality reduction method.
			{
				wxGridSizer* GridSizer = new wxFlexGridSizer( 3, 2, 0, 0 );

				wxStaticText* Label = new wxStaticText( this, -1, *LocalizeUnrealEd( "MeshSimp_QualityReductionMethod" ) );
				GridSizer->Add( Label, 0, wxALL | wxALIGN_CENTER_VERTICAL, BorderSize );
				QualityReductionTypeComboBox = new WxComboBox( this, -1, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY );
				GridSizer->Add( QualityReductionTypeComboBox, 0, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxALL, BorderSize );
				QualityGroupVSizer->Add( GridSizer, 0, wxEXPAND | wxALL, 0 );
			}

			// Quality slider.
			{
				wxBoxSizer* HSizer = new wxBoxSizer( wxHORIZONTAL );

				// NOTE: Proper min/max/value settings will be filled in later in UpdateControls()
				QualitySlider = new wxSlider( this, ID_MeshSimp_QualitySlider, 1, 1, 1, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_LABELS );
				HSizer->Add( QualitySlider, 1, wxEXPAND | wxALL, BorderSize );
				QualityGroupVSizer->Add( HSizer, 0, wxEXPAND | wxALL, 0 );
			}

			// Importance.
			{
				wxGridSizer* GridSizer = new wxFlexGridSizer( 3, 2, 0, 0 );

				// Silhouette.
				{
					wxStaticText* Label = new wxStaticText( this, -1, *LocalizeUnrealEd( "MeshSimp_SilhouetteImportance" ) );
					GridSizer->Add( Label, 0, wxALL | wxALIGN_CENTER_VERTICAL, BorderSize );
					SilhouetteComboBox = new WxComboBox( this, -1, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY );
					GridSizer->Add( SilhouetteComboBox, 0, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxALL, BorderSize );
				}

				// Texture.
				{
					wxStaticText* Label = new wxStaticText( this, -1, *LocalizeUnrealEd( "MeshSimp_TextureImportance" ) );
					GridSizer->Add( Label, 0, wxALL | wxALIGN_CENTER_VERTICAL, BorderSize );
					TextureComboBox = new WxComboBox( this, -1, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY );
					GridSizer->Add( TextureComboBox, 0, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxALL, BorderSize );
				}

				// Shading.
				{
					wxStaticText* Label = new wxStaticText( this, -1, *LocalizeUnrealEd( "MeshSimp_ShadingImportance" ) );
					GridSizer->Add( Label, 0, wxALL | wxALIGN_CENTER_VERTICAL, BorderSize );
					ShadingComboBox = new WxComboBox( this, -1, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY );
					GridSizer->Add( ShadingComboBox, 0, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxALL, BorderSize );
				}

				// Skinning.
				{
					wxStaticText* Label = new wxStaticText( this, -1, *LocalizeUnrealEd( "MeshSimp_SkinningImportance" ) );
					GridSizer->Add( Label, 0, wxALL | wxALIGN_CENTER_VERTICAL, BorderSize );
					SkinningComboBox = new WxComboBox( this, -1, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY );
					GridSizer->Add( SkinningComboBox, 0, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxALL, BorderSize );
				}

				QualityGroupVSizer->Add( GridSizer, 0, wxEXPAND | wxALL, 0 );
			}

			TopVSizer->Add( QualityGroupVSizer, 0, wxEXPAND | wxALL, BorderSize );

			// Repair Options.
			wxStaticBoxSizer* RepairSettingsVSizer = new wxStaticBoxSizer( wxVERTICAL, this,  *LocalizeUnrealEd( "MeshSimp_RepairOptions" ));
			{
				wxGridSizer* GridSizer = new wxFlexGridSizer( 3, 2, 0, 0 );

				//Welding Threshold
				{
					wxStaticText* Label = new wxStaticText( this, -1, *LocalizeUnrealEd( "MeshSimp_WeldingThreshold" ) );
					GridSizer->Add( Label, 0, wxALL | wxALIGN_CENTER_VERTICAL, BorderSize );
					WeldingThresholdSpinner = new WxSpinCtrlReal(this,-1, 0.0f, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS );

					GridSizer->Add( WeldingThresholdSpinner, 0, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxALL, BorderSize );
				}

				//Recalculate Normals
				{
					wxStaticText* Label = new wxStaticText( this, -1, *LocalizeUnrealEd( "MeshSimp_RecomputeNormals" ) );
					GridSizer->Add( Label, 0, wxALL | wxALIGN_CENTER_VERTICAL, BorderSize );
					RecalcNormalsTickBox = new wxCheckBox( this, ID_MeshSimp_RecalcNormalsCheckBox, TEXT(""));
					GridSizer->Add( RecalcNormalsTickBox, 0, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxALL, BorderSize );
				}

				//Hard-edge angle threshold
				{
					NormalThresholdLabel = new wxStaticText( this, -1, *LocalizeUnrealEd( "MeshSimp_NormalsThreshold" ) );
					GridSizer->Add( NormalThresholdLabel, 0, wxALL | wxALIGN_CENTER_VERTICAL, BorderSize );
					NormalThresholdSpinner = new WxSpinCtrlReal(this,-1, 0.0f, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS );
					GridSizer->Add( NormalThresholdSpinner, 0, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxALL, BorderSize );
				}

				RepairSettingsVSizer->Add( GridSizer, 0, wxEXPAND | wxALL, 0 );
			}

			TopVSizer->Add( RepairSettingsVSizer, 0, wxEXPAND | wxALL, BorderSize );

			// Simplify bones by desired ratio
			wxStaticBoxSizer* BoneLodVSizer = new wxStaticBoxSizer( wxVERTICAL, this,  *LocalizeUnrealEd("MeshSimp_BoneLodSettings"));
			{
				// Vertical space
				BoneLodVSizer->AddSpacer( BorderSize );

				// Bone Ratio slider.
				{
					wxBoxSizer* BoxSizer = new wxBoxSizer( wxHORIZONTAL );

					wxStaticText* Label = new wxStaticText( this, -1, *LocalizeUnrealEd("MeshSimp_BoneTargetPercentage") );
					BoxSizer->Add( Label, 0, wxALL | wxALIGN_CENTER_VERTICAL, BorderSize );
					// NOTE: Proper min/max/value settings will be filled in later in UpdateControls()
					BonePercentageSlider = new wxSlider( this, -1, 1, 1, 1, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_LABELS );
					BoxSizer->Add( BonePercentageSlider, 1, wxALL | wxEXPAND , BorderSize );
					BoneLodVSizer->Add( BoxSizer, 0, wxEXPAND | wxALL, 0 );
				}

				// Max Bones Per Vertex.
				{
					wxGridSizer* GridSizer = new wxFlexGridSizer( 3, 2, 0, 0 );
					wxStaticText* Label = new wxStaticText( this, -1, *LocalizeUnrealEd("MeshSimp_MaxBonesPerVertex") );
					GridSizer->Add( Label, 0, wxALL | wxALIGN_CENTER_VERTICAL, BorderSize );
					// NOTE: Proper min/max/value settings will be filled in later in UpdateControls()
					MaxBonesSpinner = new wxSpinCtrl(this,-1, TEXT(""),  wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS );
					GridSizer->Add( MaxBonesSpinner, 0, wxALL | wxALIGN_CENTER_VERTICAL, BorderSize );
					BoneLodVSizer->Add( GridSizer, 0, wxEXPAND | wxALL, BorderSize );
				}

				// Vertical space
				BoneLodVSizer->AddSpacer( BorderSize );
			}

			TopVSizer->Add( BoneLodVSizer, 0, wxEXPAND | wxALL, BorderSize );

			// 'Simplify' button
			SimplifyButton = new wxButton( this, ID_MeshSimp_SimplifyButton, *LocalizeUnrealEd( "MeshSimp_Simplify" ) );
			TopVSizer->Add( SimplifyButton, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, BorderSize );						// Border size
		}
	}
	
	// Assign the master sizer to the window
	SetSizer( TopVSizer );

	// Set the scrolling rate of the window's scroll bars
	SetScrollRate( 0, BorderSize * 2 );

	// Update controls
	UpdateControls();
}

/** Updates the controls in this panel. */
void WxSkeletalMeshSimplificationWindow::UpdateControls()
{
	USkeletalMesh* SkeletalMesh = AnimSetViewer->SelectedSkelMesh;

	// Look up the original triangle and vertex counts for this mesh.
	INT OrigTriCount = 0;
	INT OrigVertCount = 0;
	if ( SkeletalMesh && SkeletalMesh->LODModels.Num() )
	{
		const FStaticLODModel& SourceModel = SkeletalMesh->GetSourceModel();
		OrigVertCount = SourceModel.NumVertices;
		const INT SectionCount = SourceModel.Sections.Num();
		for ( INT SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex )
		{
			OrigTriCount += SourceModel.Sections( SectionIndex ).NumTriangles;
		}
	}
	OriginalTriCountLabel->SetLabel( *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT("MeshSimp_TriangleCount_F") ), OrigTriCount ) ) );
	OriginalVertCountLabel->SetLabel( *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT("MeshSimp_VertexCount_F") ), OrigVertCount ) ) );

	// Determine LOD for which to display current quality.
	INT LODIndex = AnimSetViewer->GetCurrentLODIndex();

	// Look up stored optimization settings for this LOD.
	FSkeletalMeshOptimizationSettings DefaultSettings;
	const FSkeletalMeshOptimizationSettings& Settings = ( SkeletalMesh && SkeletalMesh->HasOptimizationSettings( LODIndex ) ) ? SkeletalMesh->GetOptimizationSettings( LODIndex ) : DefaultSettings;

	//Setup quality reduction options
	const INT MaxValidReductionType = SMOT_MAX - 1;
	QualityReductionTypeComboBox->Clear();
	QualityReductionTypeComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_NumberofTriangles") ) );
	QualityReductionTypeComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_QualityDeviation") ) );
	QualityReductionTypeComboBox->SetSelection( Clamp<INT>( Settings.ReductionMethod, 0, MaxValidReductionType ) );

	// Setup quality slider.
	QualitySlider->SetRange( 0, 100 );
	if( Settings.ReductionMethod == SMOT_MaxDeviation)
	{
		const INT CurrentQuality = 100 - Clamp<INT>( appTrunc( Settings.MaxDeviationPercentage * 2000.0f + SMALL_NUMBER ), 0, 100 );
		QualitySlider->SetValue( CurrentQuality );
	}
	else
	{
		const INT CurrentQuality = Clamp<INT>( appTrunc( Settings.NumOfTrianglesPercentage * 100.0f + SMALL_NUMBER ), 0, 100 );
		QualitySlider->SetValue( CurrentQuality );
	}

	// Setup the importance settings.
	const INT MaxImportance = SMOI_Max - 1;
	SilhouetteComboBox->Clear();
	SilhouetteComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_ImportanceOff") ) );
	SilhouetteComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_LowestImportance") ) );
	SilhouetteComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_LowImportance") ) );
	SilhouetteComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_NormalImportance") ) );
	SilhouetteComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_HighImportance") ) );
	SilhouetteComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_HighestImportance") ) );
	SilhouetteComboBox->SetSelection( Clamp<INT>( Settings.SilhouetteImportance, 0, MaxImportance ) );

	TextureComboBox->Clear();
	TextureComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_ImportanceOff") ) );
	TextureComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_LowestImportance") ) );
	TextureComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_LowImportance") ) );
	TextureComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_NormalImportance") ) );
	TextureComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_HighImportance") ) );
	TextureComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_HighestImportance") ) );
	TextureComboBox->SetSelection( Clamp<INT>( Settings.TextureImportance, 0, MaxImportance ) );

	ShadingComboBox->Clear();
	ShadingComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_ImportanceOff") ) );
	ShadingComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_LowestImportance") ) );
	ShadingComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_LowImportance") ) );
	ShadingComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_NormalImportance") ) );
	ShadingComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_HighImportance") ) );
	ShadingComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_HighestImportance") ) );
	ShadingComboBox->SetSelection( Clamp<INT>( Settings.ShadingImportance, 0, MaxImportance ) );

	SkinningComboBox->Clear();
	SkinningComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_ImportanceOff") ) );
	SkinningComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_LowestImportance") ) );
	SkinningComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_LowImportance") ) );
	SkinningComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_NormalImportance") ) );
	SkinningComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_HighImportance") ) );
	SkinningComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_HighestImportance") ) );
	SkinningComboBox->SetSelection( Clamp<INT>( Settings.SkinningImportance, 0, MaxImportance ) );

	//Setup repair options
	WeldingThresholdSpinner->SetMinValue(0);
	WeldingThresholdSpinner->SetMaxValue(10);
	WeldingThresholdSpinner->SetValue(Settings.WeldingThreshold);

	RecalcNormalsTickBox->SetValue(Settings.bRecalcNormals ? true : false);

	NormalThresholdSpinner->SetMinValue(0);
	NormalThresholdSpinner->SetMaxValue(90);
	NormalThresholdSpinner->SetValue(Settings.NormalsThreshold);

	const bool bShouldEnable = RecalcNormalsTickBox->IsChecked();
	NormalThresholdLabel->Enable(bShouldEnable);
	NormalThresholdSpinner->Enable(bShouldEnable);

	// Setup bone spinner.
	BonePercentageSlider->SetRange( 0, 100 );
	const INT CurrentBonePercentage = Clamp<INT>(appTrunc( Settings.BoneReductionRatio * 100.0f ), 0, 100 );
	BonePercentageSlider->SetValue( CurrentBonePercentage );

	MaxBonesSpinner->SetRange( 1, 4 );
	MaxBonesSpinner->SetValue( Clamp<INT>( Settings.MaxBonesPerVertex, 1, 4 ) );

	// Update label on simplify button.
	SimplifyButton->SetLabel( *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT("MeshSimp_SimplifyLOD_F") ), LODIndex ) ) );

	// Control sizes may have changed, so update the window layout
	Layout();
}

/** Called when 'Simplify' button is pressed in the quality group. */
void WxSkeletalMeshSimplificationWindow::OnSimplifyButton( wxCommandEvent& In )
{
	USkeletalMesh* SkeletalMesh = AnimSetViewer->SelectedSkelMesh;
	FSkeletalMeshOptimizationSettings Settings;

	if( SkeletalMesh == NULL || SkeletalMesh->LODModels.Num() == 0 )
	{
		return;
	}

	// Disallow simplification of meshes that are COOKED
	if( SkeletalMesh->GetOutermost()->PackageFlags & PKG_Cooked )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd( "Error_OperationDisallowedOnCookedContent" ) );
		return;
	}

	// Grab settings from the UI.
	const INT LODIndex = AnimSetViewer->GetCurrentLODIndex();
	const INT DesiredQuality = QualitySlider->GetValue();
	Settings.ReductionMethod = (SkeletalMeshOptimizationType)QualityReductionTypeComboBox->GetSelection();

	//Given the current reduction method set the max deviation or reduction ratio.
	if(Settings.ReductionMethod == SMOT_MaxDeviation)
	{
		Settings.MaxDeviationPercentage = ( 100 - DesiredQuality ) / 2000.0f;
	}
	else
	{
		Settings.NumOfTrianglesPercentage = DesiredQuality / 100.0f;
	}

	Settings.SilhouetteImportance = SilhouetteComboBox->GetSelection();
	Settings.TextureImportance = TextureComboBox->GetSelection();
	Settings.ShadingImportance = ShadingComboBox->GetSelection();
	Settings.SkinningImportance = SkinningComboBox->GetSelection();
	Settings.WeldingThreshold = WeldingThresholdSpinner->GetValue();
	Settings.bRecalcNormals = RecalcNormalsTickBox->GetValue();
	Settings.NormalsThreshold = NormalThresholdSpinner->GetValue();
	Settings.BoneReductionRatio = BonePercentageSlider->GetValue() / 100.0f;
	Settings.MaxBonesPerVertex = MaxBonesSpinner->GetValue();

	// Simplify it!
	GWarn->BeginSlowTask( *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MeshSimp_SimplifyingMesh_F" ), LODIndex, *SkeletalMesh->GetName() ) ), TRUE );
	if( SimplygonMeshUtilities::OptimizeSkeletalMesh( SkeletalMesh, LODIndex, Settings ) )
	{
		SkeletalMesh->MarkPackageDirty();
	}
	else
	{
		// Simplification failed! Warn the user.
		appMsgf( AMT_OK, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MeshSimp_GenerateLODFailed_F" ), *SkeletalMesh->GetName() ) ) );
	}

	GWarn->EndSlowTask();

	// We modified an object, update the content browser
	GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_UpdateAssetListUI | CBR_UpdatePackageListUI ) );

	// The triangle/vertex counts for the currently-viewed mesh may have changed; UpdateToolbars re-caches this
	AnimSetViewer->UpdateForceLODMenu();
	AnimSetViewer->UpdateForceLODButtons();
	AnimSetViewer->UpdateStatusBar();

	// We modified the mesh we're looking at, so invalidate the viewport!
	if ( AnimSetViewer->PreviewWindow && AnimSetViewer->PreviewWindow->ASVPreviewVC )
	{
		AnimSetViewer->PreviewWindow->ASVPreviewVC->Invalidate();
	}

	// We changed an asset that was probably referenced by in-level assets, so make sure the level editor viewports get refreshed
	GEditor->RedrawLevelEditingViewports();

	// Update mesh simplification window controls since actor instance count, etc may have changed
	UpdateControls();
}

void WxSkeletalMeshSimplificationWindow::OnRecalcNormalsCheckBox( wxCommandEvent& In )
{
	const bool bShouldEnable = In.IsChecked();
	NormalThresholdLabel->Enable(bShouldEnable);
	NormalThresholdSpinner->Enable(bShouldEnable);
}

#endif // #if WITH_SIMPLYGON
