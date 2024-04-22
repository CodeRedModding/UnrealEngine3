/*=============================================================================
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "GenerateUVsWindow.h"
#include "StaticMeshEditor.h"
#include "D3D9MeshUtils.h"

BEGIN_EVENT_TABLE(WxGenerateUVsWindow, wxPanel)
	EVT_BUTTON( ID_DialogGenerateUVOptions_Apply, WxGenerateUVsWindow::OnApply )
	EVT_COMBOBOX( ID_DialogGenerateUVOptions_LODCombo, WxGenerateUVsWindow::OnChangeLODCombo )
	EVT_RADIOBOX( ID_DialogGenerateUVOptions_GenerationMode, WxGenerateUVsWindow::OnChangeGenerationMode )
	EVT_RADIOBUTTON( ID_DialogGenerateUVOptions_MaxStretchRadio, WxGenerateUVsWindow::OnChangeChartMethodRadio )
	EVT_RADIOBUTTON( ID_DialogGenerateUVOptions_MaxChartsRadio, WxGenerateUVsWindow::OnChangeChartMethodRadio )
END_EVENT_TABLE()



WxGenerateUVsWindow::WxGenerateUVsWindow(	WxStaticMeshEditor* Parent )
	: wxScrolledWindow( Parent ),
	  ApplyButton( NULL ),
	  CancelButton( NULL ),
	  LODComboBox( NULL ),
	  GenerationMode( NULL ),
	  InputKeepExistingUVText( NULL ),
	  InputMinChartSpacingPercentText( NULL ),
	  InputMaxStretchSizer( NULL ),
	  TexIndexComboBox( NULL ),
	  MaxChartsRadio( NULL ),
	  MaxChartsEntry( NULL ),
	  MaxStretchRadio( NULL ),
	  MaxStretchEntry( NULL ),
	  MinChartSpacingPercentEntry( NULL ),
	  BorderSpacingPercentEntry( NULL ),
	  KeepExistingUVCheckBox( NULL ),
	  ResultText( NULL ),
	  StaticMeshEditor( Parent ),
	  StaticMesh( NULL )
{
	check( StaticMeshEditor != NULL );

	const FLOAT MinChartSpacingPercentDefault = 1.0f;	// 1%, about 3 pixels of 256x256 texture
	const FLOAT BorderSpacingPercentDefault = 0.0f;		// 0% because Lightmass will automatically pad textures
	const FLOAT MaxDesiredStretchDefault = 0.5f;
	const INT MaxChartsDefault = 100;

	ChosenLODIndex = 0;
	ChosenTexIndex = 0;
	MinChartSpacingPercent = MinChartSpacingPercentDefault;
	BorderSpacingPercent = BorderSpacingPercentDefault;
	MaxCharts = MaxChartsDefault;
	MaxStretch = MaxDesiredStretchDefault;
	bKeepExistingUVs = FALSE;
	bOnlyLayoutUVs = FALSE;
	bUseMaxStretch = TRUE;
	

	//the base sizer
	wxBoxSizer* MainVerticalSizer = new wxBoxSizer(wxVERTICAL);
	{
		// Choose between generation of from scrach or use exitsting charts from 0 channel and only layout them
		wxString groupMethodChoices[] = {*LocalizeUnrealEd("GenerateUniqueUVs_CreateNewText"), *LocalizeUnrealEd("GenerateUniqueUVs_LayoutText")};
		GenerationMode = new wxRadioBox(this, ID_DialogGenerateUVOptions_GenerationMode, *LocalizeUnrealEd("GenerateUniqueUVs_ModeText"), wxDefaultPosition, wxDefaultSize, 2, groupMethodChoices, 0, wxRA_HORIZONTAL);
		MainVerticalSizer->Add(GenerationMode, 0, wxEXPAND | wxALL, 5);

		//sizer for input LOD related controls
		wxBoxSizer* InputLODSizer = new wxBoxSizer(wxHORIZONTAL);
		{
			wxStaticText* InputLODText = new wxStaticText(this, wxID_ANY, *LocalizeUnrealEd("LODChooseText"));
			InputLODSizer->Add(InputLODText, 3, wxEXPAND | wxALL, 2);

			LODComboBox = new wxComboBox( this, ID_DialogGenerateUVOptions_LODCombo, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
			InputLODSizer->Add(LODComboBox, 1, wxEXPAND | wxALL, 2);
		}
		MainVerticalSizer->Add(InputLODSizer, 0, wxEXPAND | wxALL, 5);

		//sizer for input uv index related controls
		wxBoxSizer* InputTexIndexSizer = new wxBoxSizer(wxHORIZONTAL);
		{
			wxStaticText* InputTexIndexText = new wxStaticText(this, wxID_ANY, *LocalizeUnrealEd("UVChannelText"));
			InputTexIndexSizer->Add(InputTexIndexText, 3, wxEXPAND | wxALL, 2);

			TexIndexComboBox = new wxComboBox( this, ID_DialogGenerateUVOptions_TexIndexCombo, TEXT("0"), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
			InputTexIndexSizer->Add(TexIndexComboBox, 1, wxEXPAND | wxALL, 2);
		}
		MainVerticalSizer->Add(InputTexIndexSizer, 0, wxEXPAND | wxALL, 5);

		{
			wxStaticText* ChartingMethodText = new wxStaticText(this, wxID_ANY, *LocalizeUnrealEd("GenerateUniqueUVs_SelectChartingMethod"));
			MainVerticalSizer->Add(ChartingMethodText, 0, wxEXPAND | wxALL, 5);

			//sizer for input max stretch related controls
			InputMaxStretchSizer = new wxBoxSizer(wxHORIZONTAL);
			{
				MaxStretchRadio = new wxRadioButton( this, ID_DialogGenerateUVOptions_MaxStretchRadio, *LocalizeUnrealEd("MaxUVStretchingText"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP );
				InputMaxStretchSizer->Add(MaxStretchRadio, 3, wxEXPAND | wxALL, 2);

				FString MaxDesiredStretchString = FString::Printf(TEXT("%f"), MaxStretch);
				MaxStretchEntry = new wxTextCtrl( this, ID_DialogGenerateUVOptions_MaxStretchEdit, *MaxDesiredStretchString, wxDefaultPosition, wxSize(6, -1), 0 );

				InputMaxStretchSizer->Add(MaxStretchEntry, 1, wxEXPAND | wxALL, 2);
			}
			MainVerticalSizer->Add(InputMaxStretchSizer, 0, wxEXPAND | wxALL, 5);

			// Text entry for "max charts"
			wxBoxSizer* InputMaxChartsSizer = new wxBoxSizer(wxHORIZONTAL);
			{
				MaxChartsRadio = new wxRadioButton( this, ID_DialogGenerateUVOptions_MaxChartsRadio, *LocalizeUnrealEd("GenerateUniqueUVs_MaxChartsText"), wxDefaultPosition, wxDefaultSize, 0 );
				InputMaxChartsSizer->Add( MaxChartsRadio, 3, wxEXPAND | wxALL, 2);

				FString MaxChartsString = FString::Printf(TEXT("%i"), MaxCharts);
				MaxChartsEntry = new wxTextCtrl( this, ID_DialogGenerateUVOptions_MaxChartsEdit, *MaxChartsString, wxDefaultPosition, wxSize(6, -1), 0 );

				InputMaxChartsSizer->Add( MaxChartsEntry, 1, wxEXPAND | wxALL, 2);
			}
			MainVerticalSizer->Add(InputMaxChartsSizer, 0, wxEXPAND | wxALL, 5);
		}

		// Text entry for "min chart spacing"
		wxBoxSizer* InputMinChartSpacingPercentSizer = new wxBoxSizer(wxHORIZONTAL);
		{
			InputMinChartSpacingPercentText = new wxStaticText(this, wxID_ANY, *LocalizeUnrealEd("GenerateUniqueUVs_MinChartSpacingPercentText"));
			InputMinChartSpacingPercentSizer->Add(InputMinChartSpacingPercentText, 3, wxEXPAND | wxALL, 2);

			FString MinChartSpacingPercentString = FString::Printf(TEXT("%f"), MinChartSpacingPercent);
			MinChartSpacingPercentEntry = new wxTextCtrl( this, ID_DialogGenerateUVOptions_MinChartSpacingPercentEdit, *MinChartSpacingPercentString, wxDefaultPosition, wxSize(6, -1), 0 );

			InputMinChartSpacingPercentSizer->Add( MinChartSpacingPercentEntry, 1, wxEXPAND | wxALL, 2);
		}
		MainVerticalSizer->Add(InputMinChartSpacingPercentSizer, 0, wxEXPAND | wxALL, 5);


		// Don't display "border spacing" option as there's usually no reason to add border padding
		if( 0 )
		{
			// Text entry for "border spacing"
			wxBoxSizer* InputBorderSpacingPercentSizer = new wxBoxSizer(wxHORIZONTAL);
			{
				wxStaticText* InputBorderSpacingPercentText = new wxStaticText(this, wxID_ANY, *LocalizeUnrealEd("GenerateUniqueUVs_BorderSpacingPercentText"));
				InputBorderSpacingPercentSizer->Add(InputBorderSpacingPercentText, 3, wxEXPAND | wxALL, 2);

				FString BorderSpacingPercentString = FString::Printf(TEXT("%f"), BorderSpacingPercent);
				BorderSpacingPercentEntry = new wxTextCtrl( this, ID_DialogGenerateUVOptions_BorderSpacingPercentEdit, *BorderSpacingPercentString, wxDefaultPosition, wxSize(6, -1), 0 );

				InputBorderSpacingPercentSizer->Add( BorderSpacingPercentEntry, 1, wxEXPAND | wxALL, 2);
			}
			MainVerticalSizer->Add(InputBorderSpacingPercentSizer, 0, wxEXPAND | wxALL, 5);
		}

		// "keep existing charts"
		wxBoxSizer* InputKeepExistingUVSizer = new wxBoxSizer(wxHORIZONTAL);
		{
			InputKeepExistingUVText = new wxStaticText(this, wxID_ANY, *LocalizeUnrealEd("KeepExistingUVText"));
			InputKeepExistingUVSizer->Add(InputKeepExistingUVText, 3, wxEXPAND | wxALL, 2);

			KeepExistingUVCheckBox = new wxCheckBox( this, ID_DialogGenerateUVOptions_KeepExistingUVs, TEXT(""), wxDefaultPosition, wxSize(6, -1), 0 );
			KeepExistingUVCheckBox->SetValue(bKeepExistingUVs != FALSE);

			InputKeepExistingUVSizer->Add(KeepExistingUVCheckBox, 1, wxEXPAND | wxALL, 2);

		}
		MainVerticalSizer->Add(InputKeepExistingUVSizer, 0, wxEXPAND | wxALL, 5);


		// Result text
		ResultText = new wxStaticText( this, wxID_ANY, TEXT( "" ) );
		MainVerticalSizer->Add(ResultText, 0, wxEXPAND | wxALL, 5);

		// sizer for button controls
		wxBoxSizer* ButtonSizer = new wxBoxSizer(wxHORIZONTAL);
		{
			ApplyButton = new wxButton(this, ID_DialogGenerateUVOptions_Apply, *LocalizeUnrealEd("GenerateUniqueUVs_Apply"));
			ButtonSizer->Add(ApplyButton, 0, wxCENTER | wxALL, 2);
		}
		MainVerticalSizer->Add(ButtonSizer, 0, wxALIGN_CENTER | wxALL, 10);
	}
	SetSizer(MainVerticalSizer);


	SetStaticMesh( StaticMeshEditor->StaticMesh );

	// Set the scrolling rate of the window's scroll bars
	SetScrollRate( 0, 10 );

	// Update control states
	UpdateControlStates();



	//fit the window around the sizer
	Fit();
}



/** Called by the parent Static Mesh Editor when the currently-edited mesh or LOD has changed */
void WxGenerateUVsWindow::OnStaticMeshChanged()
{
	SetStaticMesh( StaticMeshEditor->StaticMesh );
}



/** Sets the static mesh that we're generating UVs for */
void WxGenerateUVsWindow::SetStaticMesh( UStaticMesh* InStaticMesh )
{
	StaticMesh = InStaticMesh;

	// Update controls
	LODComboBox->Clear();
	TexIndexComboBox->Clear();

	if( StaticMesh != NULL )
	{
		// an entry for each LOD
		TArray<FString> LODComboOptions;

		// the number of texture coordinates each LOD has used for updating the tex coord combo box when a different LOD is chosen
		LODComboOptions.AddZeroed(StaticMesh->LODModels.Num());
		LODNumTexcoords.AddZeroed(StaticMesh->LODModels.Num());
		for(INT i = 0; i < StaticMesh->LODModels.Num(); i++)
		{
			// populate the arrays
			LODComboOptions(i) = FString::Printf( TEXT("%d"), i );
			LODNumTexcoords(i) = StaticMesh->LODModels(i).VertexBuffer.GetNumTexCoords();
		}

		// populate the combo box with available LOD's
		for( INT OptionIndex = 0 ; OptionIndex < LODComboOptions.Num(); OptionIndex++)
		{
			const FString& Option = LODComboOptions( OptionIndex );
			LODComboBox->Append( *Option );
		}
		LODComboBox->SetSelection(0);

		// populate combo with available UV indices, clamp to the maximum number of tex coord channels allowed
		for( INT OptionIndex = 0 ; OptionIndex < LODNumTexcoords(0) + 1 && OptionIndex < MAX_TEXCOORDS; OptionIndex++)
		{
			TexIndexComboBox->Append(*FString::Printf( TEXT("%d"), OptionIndex ));
		}
		TexIndexComboBox->SetSelection( InStaticMesh->LightMapCoordinateIndex > 0 ? InStaticMesh->LightMapCoordinateIndex : 1 );
	}
}



WxGenerateUVsWindow::~WxGenerateUVsWindow()
{
}

void WxGenerateUVsWindow::OnApply(wxCommandEvent& In )
{
	//parse user input, use defaults if input is invalid
	if (LODComboBox->GetSelection() >= 0)
	{
		ChosenLODIndex = LODComboBox->GetSelection();
	}

	if (TexIndexComboBox->GetSelection() >= 0)
	{
		ChosenTexIndex = TexIndexComboBox->GetSelection();
	}
	
	MinChartSpacingPercent = Clamp( appAtof( MinChartSpacingPercentEntry->GetValue() ), 0.01f, 100.0f );

	if( BorderSpacingPercentEntry != NULL )
	{
		BorderSpacingPercent = Clamp( appAtof( BorderSpacingPercentEntry->GetValue() ), 0.0f, 100.0f );
	}

	MaxCharts = Clamp( appAtoi( MaxChartsEntry->GetValue() ), 1, 100000 );

	MaxStretch = Clamp( appAtof( MaxStretchEntry->GetValue() ), 0.01f, 1.0f );

	bKeepExistingUVs = KeepExistingUVCheckBox->GetValue();

	bOnlyLayoutUVs = GenerationMode->GetSelection() ==0  ? FALSE  : TRUE;

	{
		GWarn->BeginSlowTask( *LocalizeUnrealEd("GenerateUVsProgressText"), TRUE );

		// Detach all instances of the static mesh while generating the UVs, then reattach them.
		FStaticMeshComponentReattachContext	ComponentReattachContext(StaticMesh);


		// If the user has selected any edges in the static mesh editor, we'll create an array of chart UV
		// seam edge indices to pass along to the GenerateUVs function.
		TArray< INT >* FalseEdgeIndicesPtr = NULL;
		TArray< INT > FalseEdgeIndices;
		if( StaticMeshEditor->SelectedEdgeIndices.Num() > 0 &&
			ChosenLODIndex == 0 )		// @todo: Support other LODs than LOD 0 (edge selection in SME needed)
		{
			for( WxStaticMeshEditor::FSelectedEdgeSet::TIterator SelectionIt( StaticMeshEditor->SelectedEdgeIndices );
				 SelectionIt != NULL;
				 ++SelectionIt )
			{
				const UINT EdgeIndex = *SelectionIt;

				FalseEdgeIndices.AddItem( EdgeIndex );
			}

			FalseEdgeIndicesPtr = &FalseEdgeIndices;
		}

		UBOOL bStatus =FALSE;
		//call the utility helper with the user supplied parameters
		if (bOnlyLayoutUVs)
		{
			if (D3D9MeshUtilities::LayoutUVs(
				StaticMesh,
				ChosenLODIndex, 
				ChosenTexIndex	
				))
			{
				bStatus=TRUE;
			}
		}		
		else if (D3D9MeshUtilities::GenerateUVs(StaticMesh, ChosenLODIndex, ChosenTexIndex, bKeepExistingUVs, MinChartSpacingPercent, BorderSpacingPercent, bUseMaxStretch, FalseEdgeIndicesPtr, MaxCharts, MaxStretch))
		{
			bStatus =TRUE;
		}

		if (bStatus == TRUE)
		{
			ResultText->SetLabel( *FString::Printf(LocalizeSecure(LocalizeUnrealEd("GenerateUniqueUVs_UVGenerationSuccessful"), MaxCharts, MaxStretch)));
		}
		else
		{
			ResultText->SetLabel( *LocalizeUnrealEd("GenerateUniqueUVs_UVGenerationFailed" ) );
		}

		GWarn->EndSlowTask();


		// Refresh the static mesh editor
		WxStaticMeshEditor* ParentSME = StaticMeshEditor; //The parent window is a wxAuiFloatingFrame //static_cast<WxStaticMeshEditor*>( GetParent() );
		ParentSME->UpdateToolbars();
		ParentSME->InvalidateViewport();
	}
	StaticMeshEditor->UpdateViewportPreview();
}


void WxGenerateUVsWindow::OnChangeLODCombo(wxCommandEvent& In )
{
	//re-populate the texture coordinate index based on the number of tex coords in the newly selected LOD
	if (LODComboBox->GetSelection() >= 0)
	{
		ChosenLODIndex = LODComboBox->GetSelection();
	}

	const INT OldTexIndexSelection = TexIndexComboBox->GetSelection();
	TexIndexComboBox->Clear();
	for( INT OptionIndex = 0 ; OptionIndex < LODNumTexcoords(ChosenLODIndex) + 1 && OptionIndex < MAX_TEXCOORDS; OptionIndex++)
	{
		TexIndexComboBox->Append(*FString::Printf( TEXT("%d"), OptionIndex));
	}
	TexIndexComboBox->SetSelection( OldTexIndexSelection );
	if( TexIndexComboBox->GetSelection() < 0 )
	{
		TexIndexComboBox->SetSelection(0);
	}
}

void WxGenerateUVsWindow::OnChangeGenerationMode(wxCommandEvent& In )
{
		if (GenerationMode->GetSelection() == 1)
		{

			if (BorderSpacingPercentEntry)
			{
				BorderSpacingPercentEntry->Disable();
			}
			KeepExistingUVCheckBox->Disable();
			MaxStretchRadio->Disable();
			MaxChartsRadio->Disable();
			MaxStretchEntry->Disable();
			MaxChartsEntry->Disable();
			MinChartSpacingPercentEntry->Disable();
			InputKeepExistingUVText->Disable();
			InputMinChartSpacingPercentText->Disable();

		}
		else
		{
			if (BorderSpacingPercentEntry)
			{
				BorderSpacingPercentEntry->Enable();
			}
			KeepExistingUVCheckBox->Enable();
			MaxStretchRadio->Enable();
			MaxStretchEntry->Enable();
			MaxChartsEntry->Enable();
			MinChartSpacingPercentEntry->Enable();
			MaxChartsRadio->Enable();
			InputKeepExistingUVText->Enable();
			InputMinChartSpacingPercentText->Enable();
		}
}

/**
 * Updates the state of controls within this dialog box
 */
void WxGenerateUVsWindow::UpdateControlStates()
{
	MaxStretchEntry->Enable( !MaxChartsRadio->GetValue() );
	MaxChartsEntry->Enable( !MaxStretchRadio->GetValue() );

	bUseMaxStretch = MaxStretchRadio->GetValue();
}


void WxGenerateUVsWindow::OnChangeChartMethodRadio( wxCommandEvent& In )
{
	UpdateControlStates();
}
