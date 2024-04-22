/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"

#if WITH_SIMPLYGON

#include "EngineMeshClasses.h"
#include "MeshSimplificationWindow.h"
#include "StaticMeshEditor.h"
#include "SimplygonMeshUtilities.h"

/**
 * WxMeshSimplificationWindow
 *
 * The Mesh Simplification tool allows users to optimize meshes.
 *
 * If you change functionality here, be sure to update the UDN help page:
 *      https://udn.epicgames.com/Three/MeshSimplificationTool
 */

BEGIN_EVENT_TABLE( WxMeshSimplificationWindow, wxPanel )
	EVT_BUTTON( ID_MeshSimp_SimplifyButton, WxMeshSimplificationWindow::OnSimplifyButton )
	EVT_CHECKBOX( ID_MeshSimp_RecalcNormalsCheckBox, WxMeshSimplificationWindow::OnRecalcNormalsCheckBox )
END_EVENT_TABLE()


/** Constructor that takes a pointer to the owner object */
WxMeshSimplificationWindow::WxMeshSimplificationWindow( WxStaticMeshEditor* InStaticMeshEditor )
	: wxScrolledWindow( InStaticMeshEditor )	// Parent window
{
	// Store owner object
	StaticMeshEditor = InStaticMeshEditor;
	
	// @todo: Should be a global constant for the editor
	const INT BorderSize = 5;

	// Setup controls
	wxBoxSizer* TopVSizer = new wxBoxSizer( wxVERTICAL );
	{
		// Vertical space
		TopVSizer->AddSpacer( BorderSize );

		// Original mesh stats group
		wxStaticBoxSizer* OrigMeshStatsGroupVSizer =
			new wxStaticBoxSizer( wxVERTICAL, this, *LocalizeUnrealEd( "MeshSimp_OrigMeshStatsGroup" ) );
		{
			// Vertical space
			OrigMeshStatsGroupVSizer->AddSpacer( BorderSize );

			// Triangle count label
			{
				OriginalTriCountLabel =
					new wxStaticText(
						this,						// Parent
						-1,							// ID
						wxString() );				// Text

				const INT SizerItemFlags =
					wxALL;							// Use border spacing on ALL sides

				OrigMeshStatsGroupVSizer->Add(
					OriginalTriCountLabel,			// Sizer or control
					0,								// Sizing proportion
					SizerItemFlags,   				// Flags
					BorderSize );					// Border size
			}

			// Vertex count label
			{
				OriginalVertCountLabel =
					new wxStaticText(
						this,						// Parent
						-1,							// ID
						wxString() );				// Text

				const INT SizerItemFlags =
					wxALL;							// Use border spacing on ALL sides

				OrigMeshStatsGroupVSizer->Add(
					OriginalVertCountLabel,			// Sizer or control
					0,								// Sizing proportion
					SizerItemFlags,   				// Flags
					BorderSize );					// Border size
			}

			// Vertical space
			OrigMeshStatsGroupVSizer->AddSpacer( BorderSize );

			const INT SizerItemFlags =
				wxEXPAND |						// Expand to fill space
				wxALL;							// Use border spacing on ALL sides

			TopVSizer->Add(
				OrigMeshStatsGroupVSizer,		// Sizer or control
				0,								// Sizing proportion
				SizerItemFlags,   				// Flags
				BorderSize );
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
				{
					{
						const INT SliderFlags =
							wxSL_HORIZONTAL	|		// Horizontal slider
							wxSL_LABELS;			// Displays min/max/current text labels

						// NOTE: Proper min/max/value settings will be filled in later in UpdateControls()
						QualitySlider =
							new wxSlider(
								this,						// Parent window
								ID_MeshSimp_QualitySlider,	// Event ID
								1,							// Current value
								1,							// Min value
								1,							// Max value
								wxDefaultPosition,			// Position
								wxDefaultSize,				// Size
								SliderFlags );				// Flags

						const INT SizerItemFlags =
							wxEXPAND |						// Expand to fill space
							wxALL;							// Use border spacing on ALL sides

						HSizer->Add(
							QualitySlider,					// Control window
							1,								// Sizing proportion
							SizerItemFlags,					// Flags
							BorderSize );					// Border spacing amount
					}

					const INT SizerItemFlags =
						wxEXPAND |							// Expand to fill space
						wxALL;								// Use border spacing on ALL sides

					QualityGroupVSizer->Add(
						HSizer,								// Sizer or control
						0,									// Sizing proportion
						SizerItemFlags,   					// Flags
						0 );								// Border size
				}
			}

			// Importance.
			{
				wxGridSizer* GridSizer = new wxFlexGridSizer( 3, 2, 0, 0 );
				{
					wxStaticText* Label = new wxStaticText(
						this,						// Parent
						-1,							// ID
						*LocalizeUnrealEd( "MeshSimp_SilhouetteImportance" ) );		// Text

					GridSizer->Add(
						Label,
						0,
						wxALL | wxALIGN_CENTER_VERTICAL,
						BorderSize );

					SilhouetteComboBox =
						new WxComboBox(
							this,
							-1,
							TEXT(""),
							wxDefaultPosition,
							wxDefaultSize,
							0,
							NULL,
							wxCB_READONLY );

					const INT SizerItemFlags =
						wxEXPAND |						// Expand to fill space
						wxALIGN_CENTER_VERTICAL |		// Center align vertically
						wxALL;							// Use border spacing on ALL sides

					GridSizer->Add(
						SilhouetteComboBox,				// Control window
						0,								// Sizing proportion
						SizerItemFlags,					// Flags
						BorderSize );					// Border spacing amount
				}

				{
					wxStaticText* Label = new wxStaticText(
						this,						// Parent
						-1,							// ID
						*LocalizeUnrealEd( "MeshSimp_TextureImportance" ) );		// Text

					GridSizer->Add(
						Label,
						0,
						wxALL | wxALIGN_CENTER_VERTICAL,
						BorderSize );

					TextureComboBox =
						new WxComboBox(
						this,
						-1,
						TEXT(""),
						wxDefaultPosition,
						wxDefaultSize,
						0,
						NULL,
						wxCB_READONLY );

					const INT SizerItemFlags =
						wxEXPAND |						// Expand to fill space
						wxALIGN_CENTER_VERTICAL |		// Center align vertically
						wxALL;							// Use border spacing on ALL sides

					GridSizer->Add(
						TextureComboBox,				// Control window
						0,								// Sizing proportion
						SizerItemFlags,					// Flags
						BorderSize );					// Border spacing amount
				}

				{
					wxStaticText* Label = new wxStaticText(
						this,						// Parent
						-1,							// ID
						*LocalizeUnrealEd( "MeshSimp_ShadingImportance" ) );		// Text

					GridSizer->Add(
						Label,
						0,
						wxALL | wxALIGN_CENTER_VERTICAL,
						BorderSize );

					ShadingComboBox =
						new WxComboBox(
						this,
						-1,
						TEXT(""),
						wxDefaultPosition,
						wxDefaultSize,
						0,
						NULL,
						wxCB_READONLY );

					const INT SizerItemFlags =
						wxEXPAND |						// Expand to fill space
						wxALIGN_CENTER_VERTICAL |		// Center align vertically
						wxALL;							// Use border spacing on ALL sides

					GridSizer->Add(
						ShadingComboBox,					// Control window
						0,								// Sizing proportion
						SizerItemFlags,					// Flags
						BorderSize );					// Border spacing amount
				}

				const INT SizerItemFlags =
					wxEXPAND |							// Expand to fill space
					wxALL;								// Use border spacing on ALL sides

				QualityGroupVSizer->Add(
					GridSizer,							// Sizer or control
					0,									// Sizing proportion
					SizerItemFlags,   					// Flags
					0 );								// Border size
			}

			const INT SizerItemFlags =
				wxEXPAND |						// Expand to fill space
				wxALL;							// Use border spacing on ALL sides

			TopVSizer->Add(
				QualityGroupVSizer,				// Sizer or control
				0,								// Sizing proportion
				SizerItemFlags,   				// Flags
				BorderSize );					// Border size

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

			TopVSizer->Add(  RepairSettingsVSizer, 0, wxEXPAND | wxALL, BorderSize );

			// 'Simplify' button
			{
				SimplifyButton =
					new wxButton(
						this,										// Parent
						ID_MeshSimp_SimplifyButton,					// Event ID
						*LocalizeUnrealEd( "MeshSimp_Simplify" ) );	// Caption

				const INT SizerItemFlags =
					wxALIGN_CENTER_HORIZONTAL |			// Center horizontally
					wxALL;								// Use border spacing on ALL sides

				TopVSizer->Add(
					SimplifyButton,						// Sizer or control
					0,									// Sizing proportion
					SizerItemFlags,   					// Flags
					BorderSize );						// Border size
			}
		}
	}
	
	// Assign the master sizer to the window
	SetSizer( TopVSizer );

	// Set the scrolling rate of the window's scroll bars
	SetScrollRate( 0, BorderSize * 2 );

	// Update controls
	UpdateControls();
}

/** Updates the controls in this panel */
void WxMeshSimplificationWindow::UpdateControls()
{
	UStaticMesh* StaticMesh = StaticMeshEditor->StaticMesh;
	check( StaticMesh );

	// Look up the original triangle and vertex counts for this mesh.
	INT OrigTriCount = 0;
	INT OrigVertCount = 0;
	if ( StaticMesh->LODModels.Num() )
	{
		const FStaticMeshRenderData& SourceData = StaticMesh->GetSourceData();
		OrigTriCount = SourceData.IndexBuffer.Indices.Num() / 3;
		OrigVertCount = SourceData.NumVertices;
	}
	OriginalTriCountLabel->SetLabel( *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT("MeshSimp_TriangleCount_F") ), OrigTriCount ) ) );
	OriginalVertCountLabel->SetLabel( *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT("MeshSimp_VertexCount_F") ), OrigVertCount ) ) );

	// Determine LOD for which to display current quality.
	INT LODIndex = StaticMeshEditor->GetCurrentLODIndex();

	// Look up stored optimization settings for this LOD.
	FStaticMeshOptimizationSettings DefaultSettings;
	const FStaticMeshOptimizationSettings& Settings = StaticMesh->HasOptimizationSettings( LODIndex ) ? StaticMesh->GetOptimizationSettings( LODIndex ) : DefaultSettings;

	//Setup quality reduction options
	const INT MaxValidReductionType = FStaticMeshOptimizationSettings::OT_MAX - 1;
	QualityReductionTypeComboBox->Clear();
	QualityReductionTypeComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_NumberofTriangles") ) );
	QualityReductionTypeComboBox->Append( *LocalizeUnrealEd( TEXT("MeshSimp_QualityDeviation") ) );
	QualityReductionTypeComboBox->SetSelection( Clamp<INT>( Settings.ReductionMethod, 0, MaxValidReductionType ) );

	// Setup quality slider.
	QualitySlider->SetRange( 0, 100 );
	if( Settings.ReductionMethod == FStaticMeshOptimizationSettings::OT_MaxDeviation)
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
	const INT MaxImportance = FStaticMeshOptimizationSettings::IL_Max - 1;
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

	// Update label on simplify button.
	SimplifyButton->SetLabel( *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT("MeshSimp_SimplifyLOD_F") ), LODIndex ) ) );

	// Control sizes may have changed, so update the window layout
	Layout();
}

/** Called when 'Simplify' button is pressed in the quality group. */
void WxMeshSimplificationWindow::OnSimplifyButton( wxCommandEvent& In )
{
	UStaticMesh* StaticMesh = (StaticMeshEditor && StaticMeshEditor->StaticMesh) ? StaticMeshEditor->StaticMesh : NULL;	
	if( StaticMesh == NULL || StaticMesh->LODModels.Num() == 0 )
	{
		// No static mesh in parent editor. Should never happen?
		return;
	}

	// Disallow simplification of meshes that are COOKED
	if( StaticMesh->GetOutermost()->PackageFlags & PKG_Cooked )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd( "Error_OperationDisallowedOnCookedContent" ) );
		return;
	}

	// Grab settings from the UI.
	FStaticMeshOptimizationSettings Settings;
	const INT LODIndex = StaticMeshEditor->GetCurrentLODIndex();
	const INT DesiredQuality = QualitySlider->GetValue();
	Settings.ReductionMethod = (FStaticMeshOptimizationSettings::EOptimizationType)QualityReductionTypeComboBox->GetSelection();

	//Given the current reduction method set the max deviation or reduction ratio.
	if(Settings.ReductionMethod == FStaticMeshOptimizationSettings::OT_MaxDeviation)
	{
		Settings.MaxDeviationPercentage = ( 100 - DesiredQuality ) / 2000.0f;
	}
	else
	{
		Settings.NumOfTrianglesPercentage = DesiredQuality / 100.0f;
	}

	Settings.SilhouetteImportance = (FStaticMeshOptimizationSettings::EImportanceLevel)SilhouetteComboBox->GetSelection();
	Settings.TextureImportance	= (FStaticMeshOptimizationSettings::EImportanceLevel)TextureComboBox->GetSelection();
	Settings.ShadingImportance = (FStaticMeshOptimizationSettings::EImportanceLevel)ShadingComboBox->GetSelection();
	Settings.WeldingThreshold = WeldingThresholdSpinner->GetValue();
	Settings.bRecalcNormals = RecalcNormalsTickBox->GetValue();
	Settings.NormalsThreshold = NormalThresholdSpinner->GetValue();

	// Detach all instances of the static mesh while we're working
	FStaticMeshComponentReattachContext	ComponentReattachContext( StaticMesh );
	StaticMesh->ReleaseResources();
	StaticMesh->ReleaseResourcesFence.Wait();

	// Simplify it!
	GWarn->BeginSlowTask( *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MeshSimp_SimplifyingMesh_F" ), LODIndex, *StaticMesh->GetName() ) ), TRUE );
	if( !SimplygonMeshUtilities::OptimizeStaticMesh( StaticMesh, StaticMesh, LODIndex, Settings ) )
	{
		// Simplification failed! Warn the user.
		appMsgf( AMT_OK, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MeshSimp_GenerateLODFailed_F" ), *StaticMesh->GetName() ) ) );
	}

	// Restore static mesh state
	StaticMesh->InitResources();

	GWarn->EndSlowTask();

	// We modified an object, update the content browser 
	GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_UpdateAssetListUI | CBR_UpdatePackageListUI ) );

	// The triangle/vertex counts for the currently-viewed mesh may have changed; UpdateToolbars re-caches this
	StaticMeshEditor->UpdateToolbars();

	// We modified the mesh we're looking at, so invalidate the viewport!
	StaticMeshEditor->InvalidateViewport();

	// We changed an asset that was probably referenced by in-level assets, so make sure the level editor viewports get refreshed
	GEditor->RedrawLevelEditingViewports();

	// Update mesh simplification window controls since actor instance count, etc may have changed
	UpdateControls();
}

void WxMeshSimplificationWindow::OnRecalcNormalsCheckBox( wxCommandEvent& In )
{
	const bool bShouldEnable = In.IsChecked();
	NormalThresholdLabel->Enable(bShouldEnable);
	NormalThresholdSpinner->Enable(bShouldEnable);
}

#endif // #if WITH_SIMPLYGON
