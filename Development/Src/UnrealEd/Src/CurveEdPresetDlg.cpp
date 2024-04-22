/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "CurveEd.h"
#include "CurveEdPresetDlg.h"
#include "PropertyWindow.h"

BEGIN_EVENT_TABLE(WxCurveEdPresetDlg, wxDialog)
	EVT_BUTTON(wxID_OK, WxCurveEdPresetDlg::OnOK)
	EVT_COMBOBOX(ID_CEP_PRESET_0 + (3 * 0), WxCurveEdPresetDlg::OnPresetCombo)
	EVT_COMBOBOX(ID_CEP_PRESET_0 + (3 * 1), WxCurveEdPresetDlg::OnPresetCombo)
	EVT_COMBOBOX(ID_CEP_PRESET_0 + (3 * 2), WxCurveEdPresetDlg::OnPresetCombo)
	EVT_COMBOBOX(ID_CEP_PRESET_0 + (3 * 3), WxCurveEdPresetDlg::OnPresetCombo)
	EVT_COMBOBOX(ID_CEP_PRESET_0 + (3 * 4), WxCurveEdPresetDlg::OnPresetCombo)
	EVT_COMBOBOX(ID_CEP_PRESET_0 + (3 * 5), WxCurveEdPresetDlg::OnPresetCombo)
END_EVENT_TABLE()

/**
 *	Constructor - create an instance of a CurveEdPreset dialog
 *	@param	parent			The parent window.
 */
WxCurveEdPresetDlg::WxCurveEdPresetDlg(WxCurveEditor* parent) :
	wxDialog(),
	CurveEditor(parent),
	bIsSaveDlg(FALSE)
{
	FString	Title	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("CurveEdPresetCaption"), TEXT("")));
    Create(parent, wxID_ANY, *Title);
	SetSize(1100,840);
	CreateControls();
    Centre();
}

/**
 *	Destructor
 */
WxCurveEdPresetDlg::~WxCurveEdPresetDlg()
{
}

/**
 *	Serialize
 *	Only required for garbage collection during usage
 *	@param	Ar	The archive serializing to
 */
void WxCurveEdPresetDlg::Serialize(FArchive& Ar)
{
	if (!Ar.IsSaving() && !Ar.IsLoading())
	{
		// Serialize the preset classes in the combo controls.
		for (INT CurveIndex = 0; CurveIndex < CEP_CURVE_MAX; CurveIndex++)
		{
			Ar << CurveControls[CurveIndex].CurrentPreset;
		}
	}
}

/**
 *	CreateControls
 *	Creates the controls required for this dialog
 */
void WxCurveEdPresetDlg::CreateControls()
{
	FString	LocalizedSizerLabels[6];

	LocalizedSizerLabels[0] = LocalizeUnrealEd("CurveEdPreset_X_Red_X-Max");
	LocalizedSizerLabels[1] = LocalizeUnrealEd("CurveEdPreset_Y_Green_Y-Max");
	LocalizedSizerLabels[2] = LocalizeUnrealEd("CurveEdPreset_Z_Blue_Z-Max");
	LocalizedSizerLabels[3] = LocalizeUnrealEd("CurveEdPreset_X2_Red2_X-Min");
	LocalizedSizerLabels[4] = LocalizeUnrealEd("CurveEdPreset_Y2_Green2_Y-Min");
	LocalizedSizerLabels[5] = LocalizeUnrealEd("CurveEdPreset_Z2_Blue2_Z-Min");

    WxCurveEdPresetDlg* itemDialog1 = this;

	wxFlexGridSizer* itemFlexGridSizer2 = new wxFlexGridSizer(5, 1, 0, 0);
    itemDialog1->SetSizer(itemFlexGridSizer2);

    wxBoxSizer* itemBoxSizer3 = new wxBoxSizer(wxVERTICAL);
    itemFlexGridSizer2->Add(itemBoxSizer3, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

	//
    wxStaticBox* itemStaticBoxSizer4Static = new wxStaticBox(itemDialog1, wxID_ANY, *LocalizeUnrealEd("CurveEdPreset_CurveName"));
    wxStaticBoxSizer* itemStaticBoxSizer4 = new wxStaticBoxSizer(itemStaticBoxSizer4Static, wxVERTICAL);
    itemBoxSizer3->Add(itemStaticBoxSizer4, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    CurveNameCtrl = new wxTextCtrl( itemDialog1, ID_CEP_CURVENAME, TEXT(""), wxDefaultPosition, wxSize(256, -1), wxTE_READONLY );
    itemStaticBoxSizer4->Add(CurveNameCtrl, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

	INT	ControlCount	= 0;

	// Top Boxes
    wxBoxSizer* itemBoxSizer6 = new wxBoxSizer(wxVERTICAL);
    itemFlexGridSizer2->Add(itemBoxSizer6, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxFlexGridSizer* itemFlexGridSizer7 = new wxFlexGridSizer(2, 3, 0, 0);
    itemBoxSizer6->Add(itemFlexGridSizer7, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

	// Create set of controls for each possible curve
	// Includes an enabled check-box, a preset selection combo, and a panel and property window pair.
	for (INT CurveIndex = 0; CurveIndex < CEP_CURVE_MAX; CurveIndex++)
	{
		INT	ControlIDOffset	= CurveIndex * 3;

		wxStaticBox* itemStaticBoxSizerStatic = new wxStaticBox(itemDialog1, wxID_ANY, *LocalizedSizerLabels[CurveIndex]);
		wxStaticBoxSizer* itemStaticBoxSizer = new wxStaticBoxSizer(itemStaticBoxSizerStatic, wxHORIZONTAL);
		itemFlexGridSizer7->Add(itemStaticBoxSizer, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

		wxFlexGridSizer* itemFlexGridSizer = new wxFlexGridSizer(3, 1, 0, 0);
		itemStaticBoxSizer->Add(itemFlexGridSizer, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

		CurveControls[CurveIndex].CurrentPreset	= NULL;

		CurveControls[CurveIndex].EnabledCheckBox = new wxCheckBox(itemDialog1, ID_CEP_ENABLE_0 + ControlIDOffset, TEXT(""), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
		CurveControls[CurveIndex].EnabledCheckBox->SetValue(FALSE);
		CurveControls[CurveIndex].EnabledCheckBox->Enable(FALSE);
		itemFlexGridSizer->Add(CurveControls[CurveIndex].EnabledCheckBox, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

		wxString* itemComboBoxStrings = NULL;
		CurveControls[CurveIndex].PresetCombo = new wxComboBox( itemDialog1, ID_CEP_PRESET_0 + ControlIDOffset, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, itemComboBoxStrings, wxCB_DROPDOWN );
		itemFlexGridSizer->Add(CurveControls[CurveIndex].PresetCombo, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

		CurveControls[CurveIndex].PropertyPanel = new wxPanel( itemDialog1, ID_CEP_PANEL_0 + ControlIDOffset, wxDefaultPosition, wxSize(320, 200), wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
		itemFlexGridSizer->Add(CurveControls[CurveIndex].PropertyPanel, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

		const wxRect rc = CurveControls[CurveIndex].PropertyPanel->GetRect();

		CurveControls[CurveIndex].PropertyWindow = new WxPropertyWindowHost;
		CurveControls[CurveIndex].PropertyWindow->Create(CurveControls[CurveIndex].PropertyPanel, NULL);
		CurveControls[CurveIndex].PropertyWindow->SetSize(rc);
	}

	// OK/Cancel box
	wxBoxSizer* itemBoxSizer106 = new wxBoxSizer(wxVERTICAL);
    itemFlexGridSizer2->Add(itemBoxSizer106, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxButton* itemButton107 = new wxButton( itemDialog1, wxID_OK, _("OK"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer106->Add(itemButton107, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    wxBoxSizer* itemBoxSizer108 = new wxBoxSizer(wxVERTICAL);
    itemFlexGridSizer2->Add(itemBoxSizer108, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxButton* itemButton109 = new wxButton( itemDialog1, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer108->Add(itemButton109, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
}

/**
 *	Show
 *	Display the dialog
 *
 *	@param	bShow			TRUE to show, FALSE to hide
 *	@param	CurveName		The name of the curve (distribution) that is being preset
 *	@param	ValidCurves		Flags indicating the valid sub-curves for the preset (combination of VCL_* flags)
 *	@param	CurveEdPresets	An array of UClass* to all available preset curve classes (derived from CurveEdPresetBase)
 *	@param	bInIsSaveDlg	Boolean indicating whether the dialog is being used for saving preset curves or creating them
 *
 *	@return	boolean indicating whether it was successful or not
 */
bool WxCurveEdPresetDlg::ShowDialog(UBOOL bShow, const TCHAR* CurveName, INT ValidCurves, TArray<UClass*>& CurveEdPresets, UBOOL bInIsSaveDlg)
{
	bIsSaveDlg	= bInIsSaveDlg;
	
	FString	Title	= FString::Printf(LocalizeSecure(LocalizeUnrealEd("CurveEdPresetCaption"), bIsSaveDlg ? *LocalizeUnrealEd("Saving") : TEXT("")));
	this->SetTitle(*Title);

	LocalCurveEdPresets	= &CurveEdPresets;

	ValidityFlags	= ValidCurves;

	// Set the curve name
	CurveNameCtrl->SetValue(CurveName);

	CurveControls[CEP_CURVE_XMAX].EnabledCheckBox->SetValue((ValidCurves & VCL_XMax) ? TRUE : FALSE);
	CurveControls[CEP_CURVE_YMAX].EnabledCheckBox->SetValue((ValidCurves & VCL_YMax) ? TRUE : FALSE);
	CurveControls[CEP_CURVE_ZMAX].EnabledCheckBox->SetValue((ValidCurves & VCL_ZMax) ? TRUE : FALSE);
	CurveControls[CEP_CURVE_XMIN].EnabledCheckBox->SetValue((ValidCurves & VCL_XMin) ? TRUE : FALSE);
	CurveControls[CEP_CURVE_YMIN].EnabledCheckBox->SetValue((ValidCurves & VCL_YMin) ? TRUE : FALSE);
	CurveControls[CEP_CURVE_ZMIN].EnabledCheckBox->SetValue((ValidCurves & VCL_ZMin) ? TRUE : FALSE);

	// Fill the combos
	FillPresetCombos(CurveEdPresets);

	// Set default values
	SetDefaultValues();

	// Show it
	const bool bShouldShow = (bShow == TRUE);
	return wxDialog::Show( bShouldShow );
}

/**
 *	OnOK
 *	Called when the OK button is pressed
 *
 *	@param	In		The wxCommandEvent generated by the caller
 */
void WxCurveEdPresetDlg::OnOK(wxCommandEvent& In)
{
#if defined(_PRESET_SHOW_NUMSAMPLES_STARTENDTIMES_)
	FString	Value;

	INT		OutNumSamples;
	FLOAT	OutStartValue;
	FLOAT	OutEndValue;

	Value			= FString(NumSamples->GetValue());
	OutNumSamples	= appAtoi(*Value);

	Value			= FString(StartTimeValue->GetValue());
	OutStartValue	= appAtof(*Value);

	Value			= FString(EndTimeValue->GetValue());
	OutEndValue		= appAtof(*Value);
#endif	//#if defined(_PRESET_SHOW_NUMSAMPLES_STARTENDTIMES_)

	// Verify that the settings are correct...
	UBOOL	bSettingsAreOk = TRUE;
	INT		CurveIndex;

	for (CurveIndex = 0; CurveIndex < CEP_CURVE_MAX; CurveIndex++)
	{
		if (ValidityFlags & (1 << CurveIndex))
		{
			if (CurveControls[CurveIndex].CurrentPreset)
			{
				if (CurveControls[CurveIndex].CurrentPreset->eventCheckAreSettingsValid(bIsSaveDlg) == FALSE)
				{
					FString	CurveLabel;
					
					// Warn the user.
					switch (CurveIndex)
					{
					case 0:		CurveLabel	= LocalizeUnrealEd("CurveEdPreset_X_Red_X-Max");		break;
					case 1:		CurveLabel	= LocalizeUnrealEd("CurveEdPreset_Y_Green_Y-Max");		break;
					case 2:		CurveLabel	= LocalizeUnrealEd("CurveEdPreset_Z_Blue_Z-Max");		break;
					case 3:		CurveLabel	= LocalizeUnrealEd("CurveEdPreset_X2_Red2_X-Min");		break;
					case 4:		CurveLabel	= LocalizeUnrealEd("CurveEdPreset_Y2_Green2_Y-Min");	break;
					case 5:		CurveLabel	= LocalizeUnrealEd("CurveEdPreset_Z2_Blue2_Z-Min");		break;
					default:	CurveLabel	= TEXT("*** ERROR ***");								break;
					}

					appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("CurveEdPreset_InvalidSetting"), *CurveLabel));

					bSettingsAreOk = FALSE;
					break;
				}
			}
			else
			{
				// Warn the user.
				bSettingsAreOk = FALSE;
				break;
			}
		}
	}

	if (bSettingsAreOk)
	{
		CurveEditor->PresetDialog_OnOK();
		for (CurveIndex = 0; CurveIndex < CEP_CURVE_MAX; CurveIndex++)
		{
			if (CurveControls[CurveIndex].CurrentPreset)
			{
				CurveControls[CurveIndex].CurrentPreset	= NULL;
			}

			if (CurveControls[CurveIndex].PropertyWindow)
			{
				CurveControls[CurveIndex].PropertyWindow->SetObject(NULL, EPropertyWindowFlags::NoFlags);
			}
		}

		wxDialog::AcceptAndClose();
	}
}

/**
 *	OnPresetCombo
 *	Called when the preset combo box selection changed.
 *
 *	@param	In	The wxCommandEvent giving the details of the change
 */
void WxCurveEdPresetDlg::OnPresetCombo(wxCommandEvent& In)
{
	PresetCurveControls*	Controls	= NULL;
	UClass*					Class		= NULL;
	INT						CurveIndex	= -1;
	INT						ComboIndex	= -1;

#if defined(_PRESETS_DEBUG_ENABLED_)
	switch (In.GetId())
	{
	case ID_CEP_PRESET_0 + (3 * 0):		debugf(TEXT("X  Combo"));		break;
	case ID_CEP_PRESET_0 + (3 * 1):		debugf(TEXT("Y  Combo"));		break;
	case ID_CEP_PRESET_0 + (3 * 2):		debugf(TEXT("Z  Combo"));		break;
	case ID_CEP_PRESET_0 + (3 * 3):		debugf(TEXT("X2 Combo"));		break;
	case ID_CEP_PRESET_0 + (3 * 4):		debugf(TEXT("Y2 Combo"));		break;
	case ID_CEP_PRESET_0 + (3 * 5):		debugf(TEXT("Z2 Combo"));		break;
	}
#endif	//#if defined(_PRESETS_DEBUG_ENABLED_)

	switch (In.GetId())
	{
	case ID_CEP_PRESET_0 + (3 * 0):
		CurveIndex	= CEP_CURVE_XMAX;
		break;
	case ID_CEP_PRESET_0 + (3 * 1):
		CurveIndex	= CEP_CURVE_YMAX;
		break;
	case ID_CEP_PRESET_0 + (3 * 2):
		CurveIndex	= CEP_CURVE_ZMAX;
		break;
	case ID_CEP_PRESET_0 + (3 * 3):
		CurveIndex	= CEP_CURVE_XMIN;
		break;
	case ID_CEP_PRESET_0 + (3 * 4):
		CurveIndex	= CEP_CURVE_YMIN;
		break;
	case ID_CEP_PRESET_0 + (3 * 5):
		CurveIndex	= CEP_CURVE_ZMIN;
		break;
	}

	if (CurveIndex != -1)
	{
		Controls	= &CurveControls[CurveIndex];
		ComboIndex	= Controls->PresetCombo->GetSelection();
		Class		= (*LocalCurveEdPresets)(ComboIndex);

		UCurveEdPresetBase* Base = ConstructObject<UCurveEdPresetBase>(Class);
		check(Base);

		if (Controls->CurrentPreset)
		{
			if (Controls->CurrentPreset->IsA(Class->StaticClass()) == FALSE)
			{
				Controls->CurrentPreset	= NULL;
			}
		}

		if (Controls->CurrentPreset == NULL)
		{
			Controls->CurrentPreset	= Base;
			Controls->PropertyWindow->SetObject(Base, EPropertyWindowFlags::ShouldShowCategories);
		}
	}
}

/**
 *	FillPresetCombos
 *	Fill in the combo boxes with available preset classes.
 *
 *	@param	CurveEdPresets	An array of available preset classes
 */
void WxCurveEdPresetDlg::FillPresetCombos(TArray<UClass*>& CurveEdPresets)
{
	for (INT ComboIndex = 0; ComboIndex < CEP_CURVE_MAX; ComboIndex++)
	{
		CurveControls[ComboIndex].PresetCombo->Clear();

		for (INT CurveIndex = 0; CurveIndex < CurveEdPresets.Num(); CurveIndex++)
		{
			UClass*				Class		= CurveEdPresets(CurveIndex);
			UCurveEdPresetBase*	BasePreset	= ConstructObject<UCurveEdPresetBase>(Class);
			
			if (BasePreset)
			{
				FString	DisplayName;
				BasePreset->eventFetchDisplayName(DisplayName);

				CurveControls[ComboIndex].PresetCombo->Append(*DisplayName, CurveEdPresets(CurveIndex));

				if (CurveIndex == 0)
				{
					CurveControls[ComboIndex].CurrentPreset	= BasePreset;
					if (CurveControls[ComboIndex].PropertyWindow)
					{
						CurveControls[ComboIndex].PropertyWindow->SetObject(BasePreset, EPropertyWindowFlags::ShouldShowCategories);
					}
				}
			}
		}

		CurveControls[ComboIndex].PresetCombo->SetSelection(0);
/***
		wxCommandEvent Touch;
		
		Touch.SetId(ID_CEP_PRESET_0 + (3 * ComboIndex));
		OnPresetCombo(Touch);
***/
	}
}

/**
 *	SetDefaultValues
 *	Enables/disables sub-curve blocks based on valid flags.
 */
void WxCurveEdPresetDlg::SetDefaultValues()
{
	for (INT BlockIndex = 0; BlockIndex < CEP_CURVE_MAX; BlockIndex++)
	{
		if (CurveControls[BlockIndex].EnabledCheckBox->GetValue() == FALSE)
		{
			CurveControls[BlockIndex].EnabledCheckBox->Enable(FALSE);
			CurveControls[BlockIndex].PresetCombo->Enable(FALSE);
		}
		else
		{
			CurveControls[BlockIndex].EnabledCheckBox->Enable(FALSE);
			CurveControls[BlockIndex].PresetCombo->Enable(TRUE);
		}
	}
}

/**
 *	IsCurveValid
 *	
 *	@param	CurveIndex	The sub-curve being checked
 *
 *	@return	TRUE if the sub-curve is valid, FALSE if not
 */
UBOOL WxCurveEdPresetDlg::IsCurveValid(INT CurveIndex)
{
	if ((CurveIndex < 0) || (CurveIndex >= NUMDATACONTROLS))
	{
		return FALSE;
	}

	if (ValidityFlags & (1 << CurveIndex))
	{
		return TRUE;
	}

	return FALSE;
}

/**
 *	GetPreset
 *	Returned the preset class for the given index.
 *
 *	@param	CurveIndex	Index of the sub-curve of interest.
 *
 *	@return				NULL if invalid index, pointer to the preset class selected otherwise.
 */
UCurveEdPresetBase*	WxCurveEdPresetDlg::GetPreset(INT CurveIndex)
{
	if ((CurveIndex < 0) || (CurveIndex >= CEP_CURVE_MAX))
	{
		return NULL;
	}

	return CurveControls[CurveIndex].CurrentPreset;
}

WxCurveEdPresetDlg::PresetCurveControls::~PresetCurveControls()
{
	delete PropertyWindow;
}
