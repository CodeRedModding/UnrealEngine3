/*=============================================================================
	DlgDensityRenderingOptions.cpp: UnrealEd dialog for displaying density 
		render mode options.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "UnrealEd.h"
#include "DlgDensityRenderingOptions.h"
#include "BusyCursor.h"
#include "ScopedTransaction.h"

BEGIN_EVENT_TABLE(WxDlgDensityRenderingOptions, WxTrackableDialog)
END_EVENT_TABLE()

/**
 * Dialog that displays the options for rendering density view modes.
 */
WxDlgDensityRenderingOptions::WxDlgDensityRenderingOptions(wxWindow* InParent) : 
	WxTrackableDialog(InParent, wxID_ANY, (wxString)*LocalizeUnrealEd("DensityRenderingOptions"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	check(GEngine);
	// Create and layout the controls
	wxBoxSizer* MainSizer = new wxBoxSizer(wxVERTICAL);
	{
		for (INT ControlGroup = DRO_IdealDensity; ControlGroup <= DRO_GrayscaleScale; ControlGroup++)
		{
			CreateControlGroup((EControlGroup)ControlGroup, MainSizer);
		}

		wxBoxSizer* BooleanSizer = new wxBoxSizer(wxHORIZONTAL);
		{
			RenderGrayscaleCheckBox = new wxCheckBox(this, wxID_ANY, *LocalizeUnrealEd("RenderGrayscale"));
			BooleanSizer->Add(RenderGrayscaleCheckBox, 0, wxALIGN_TOP|wxALL, 5);
			RenderGrayscaleCheckBox->SetValue(GEngine->bRenderLightMapDensityGrayscale);
			MainSizer->Add(BooleanSizer, 0, wxALIGN_TOP|wxALL, 5);
		}
	}
	SetSizer(MainSizer);

	// Setup the events
	// Use the event handler and the Id of the button of interest
	wxEvtHandler* EvtHandler = GetEventHandler();
	EvtHandler->Connect(RenderGrayscaleCheckBox->GetId(), wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(WxDlgDensityRenderingOptions::OnRenderGrayscaleChecked));

	EvtHandler->Connect(IdealDensitySlider.Slider->GetId(), wxEVT_SCROLL_CHANGED, wxScrollEventHandler(WxDlgDensityRenderingOptions::OnSliderChanged));
	EvtHandler->Connect(IdealDensitySlider.TextCtrl->GetId(), wxEVT_COMMAND_TEXT_UPDATED, wxTextEventHandler(WxDlgDensityRenderingOptions::OnTextCtrlChanged));
	EvtHandler->Connect(MaximumDensitySlider.Slider->GetId(), wxEVT_SCROLL_CHANGED, wxScrollEventHandler(WxDlgDensityRenderingOptions::OnSliderChanged));
	EvtHandler->Connect(MaximumDensitySlider.TextCtrl->GetId(), wxEVT_COMMAND_TEXT_UPDATED, wxTextEventHandler(WxDlgDensityRenderingOptions::OnTextCtrlChanged));
	EvtHandler->Connect(ColorScaleSlider.Slider->GetId(), wxEVT_SCROLL_CHANGED, wxScrollEventHandler(WxDlgDensityRenderingOptions::OnSliderChanged));
	EvtHandler->Connect(ColorScaleSlider.TextCtrl->GetId(), wxEVT_COMMAND_TEXT_UPDATED, wxTextEventHandler(WxDlgDensityRenderingOptions::OnTextCtrlChanged));
	EvtHandler->Connect(GrayscaleScaleSlider.Slider->GetId(), wxEVT_SCROLL_CHANGED, wxScrollEventHandler(WxDlgDensityRenderingOptions::OnSliderChanged));
	EvtHandler->Connect(GrayscaleScaleSlider.TextCtrl->GetId(), wxEVT_COMMAND_TEXT_UPDATED, wxTextEventHandler(WxDlgDensityRenderingOptions::OnTextCtrlChanged));

	// Load window position.
	FWindowUtil::LoadPosSize(TEXT("DlgDensityRenderingOptions"), this, -1, -1, 255,345);
}

WxDlgDensityRenderingOptions::~WxDlgDensityRenderingOptions()
{
	// Save window position.
	FWindowUtil::SavePosSize(TEXT("DlgDensityRenderingOptions"), this);
}

/**
 *	Show or hide the dialog
 *
 *	@param	show	If TRUE, show the dialog; if FALSE, hide it
 *
 *	@return	bool	
 */
bool WxDlgDensityRenderingOptions::Show( bool show )
{
	return wxDialog::Show(show);
}

/** Event handler for render grayscale checkbox */
void WxDlgDensityRenderingOptions::OnRenderGrayscaleChecked(wxCommandEvent& In)
{
	GEngine->bRenderLightMapDensityGrayscale = (In.IsChecked() == true) ? TRUE : FALSE;
	GEngine->SaveConfig();
	GCallbackEvent->Send(CALLBACK_RedrawAllViewports);
}

/** Event handler for slider changes */
void WxDlgDensityRenderingOptions::OnSliderChanged(wxScrollEvent& In)
{
	SliderGroup* Group = GetControlGroupFromSliderId(In.GetId());
	if (Group)
	{
		FLOAT NewValue = (FLOAT)(In.GetPosition())/100.0f;
		UpdateControlGroupValue(Group, NewValue, FALSE, TRUE);
	}
}

/** Event handler for text ctrl changes */
void WxDlgDensityRenderingOptions::OnTextCtrlChanged(wxCommandEvent& In)
{
	SliderGroup* Group = GetControlGroupFromTextCtrlId(In.GetId());
	if (Group)
	{
		FLOAT NewValue = appAtof((const TCHAR*)(In.GetString()));
		UpdateControlGroupValue(Group, NewValue, TRUE, FALSE);
	}
}

/** Set up the controls for the given type */
void WxDlgDensityRenderingOptions::CreateControlGroup(EControlGroup ControlGroup, wxSizer* OwningSizer)
{
	FString ControlName;
	SliderGroup* Group = NULL;
	INT InitValue = 0;
	INT MaxValue = 10000;	// 100.0f (10000 / 100.0f)
	FString TextCtrlString;

	check(GEngine);

	switch (ControlGroup)
	{
	case DRO_IdealDensity:
		ControlName = LocalizeUnrealEd("IdealDensity");
		Group = &IdealDensitySlider;
		InitValue = appTrunc(GEngine->IdealLightMapDensity * 100);
		Group->DataPtr = &GEngine->IdealLightMapDensity;
		break;
	case DRO_MaximumDensity:
		ControlName = LocalizeUnrealEd("MaximumDensity");
		Group = &MaximumDensitySlider;
		InitValue = appTrunc(GEngine->MaxLightMapDensity * 100);
		Group->DataPtr = &GEngine->MaxLightMapDensity;
		break;
	case DRO_ColorScale:
		ControlName = LocalizeUnrealEd("ColorScale");
		Group = &ColorScaleSlider;
		InitValue = appTrunc(GEngine->RenderLightMapDensityColorScale * 100);
		MaxValue = 1000;	// 10.0f
		Group->DataPtr = &GEngine->RenderLightMapDensityColorScale;
		break;
	case DRO_GrayscaleScale:
		ControlName = LocalizeUnrealEd("GrayscaleScale");
		Group = &GrayscaleScaleSlider;
		InitValue = appTrunc(GEngine->RenderLightMapDensityGrayscaleScale * 100);
		MaxValue = 1000;	// 10.0f
		Group->DataPtr = &GEngine->RenderLightMapDensityGrayscaleScale;
		break;
	}

	if (Group != NULL)
	{
		FLOAT Value = (FLOAT)InitValue / 100.0f;
		TextCtrlString = FString::Printf(TEXT("%4.2f"), Value);
		Group->BoxSizer = new wxStaticBoxSizer(wxHORIZONTAL, this, *ControlName);
		Group->Slider = new wxSlider(this, wxID_ANY, InitValue, 0, MaxValue);
		Group->BoxSizer->Add(Group->Slider, 0, wxEXPAND|wxALL, 5);
		Group->TextCtrl = new wxTextCtrl(this, wxID_ANY, *TextCtrlString);
		Group->BoxSizer->Add(Group->TextCtrl, 0, wxEXPAND|wxALL, 5);

		Group->ControlGroup = ControlGroup;

		OwningSizer->Add(Group->BoxSizer, 0, wxEXPAND|wxALL, 5);

		ControlGroups[ControlGroup] = Group;
	}
}

/** Get the slider group for the give ControlGroup */
WxDlgDensityRenderingOptions::SliderGroup* WxDlgDensityRenderingOptions::GetControlGroup(EControlGroup ControlGroup)
{
	SliderGroup* Group = NULL;
	switch (ControlGroup)
	{
	case DRO_IdealDensity:
		Group = &IdealDensitySlider;
		break;
	case DRO_MaximumDensity:
		Group = &MaximumDensitySlider;
		break;
	case DRO_ColorScale:
		Group = &ColorScaleSlider;
		break;
	case DRO_GrayscaleScale:
		Group = &GrayscaleScaleSlider;
		break;
	}

	return Group;
}

/** Get the slider group for the given control type and its Id */
WxDlgDensityRenderingOptions::SliderGroup* WxDlgDensityRenderingOptions::GetControlGroupFromSliderId(int InId)
{
	if (IdealDensitySlider.Slider->GetId() == InId)
	{
		return &IdealDensitySlider;
	}
	if (MaximumDensitySlider.Slider->GetId() == InId)
	{
		return &MaximumDensitySlider;
	}
	if (ColorScaleSlider.Slider->GetId() == InId)
	{
		return &ColorScaleSlider;
	}
	if (GrayscaleScaleSlider.Slider->GetId() == InId)
	{
		return &GrayscaleScaleSlider;
	}
	return NULL;
}

WxDlgDensityRenderingOptions::SliderGroup* WxDlgDensityRenderingOptions::GetControlGroupFromTextCtrlId(int InId)
{
	if (IdealDensitySlider.TextCtrl->GetId() == InId)
	{
		return &IdealDensitySlider;
	}
	if (MaximumDensitySlider.TextCtrl->GetId() == InId)
	{
		return &MaximumDensitySlider;
	}
	if (ColorScaleSlider.TextCtrl->GetId() == InId)
	{
		return &ColorScaleSlider;
	}
	if (GrayscaleScaleSlider.TextCtrl->GetId() == InId)
	{
		return &GrayscaleScaleSlider;
	}
	return NULL;
}

/** 
 *	Update the value for the given group.
 *
 *	@param	Group			The group to update the value for.
 *	@param	NewValue		The value to set.
 *	@param	bSetSlider		If TRUE, set the slider control.
 *	@param	bSetText		If TRUE, set the text control.
 */
void WxDlgDensityRenderingOptions::UpdateControlGroupValue(
	WxDlgDensityRenderingOptions::SliderGroup* Group, FLOAT NewValue,
	UBOOL bSetSlider, UBOOL bSetText)
{
	// Don't let the Max go below the ideal...
	if (Group == &MaximumDensitySlider)
	{
		// We need to make sure that Maximum is always slightly larger than ideal...
		INT IdealSettingInt = IdealDensitySlider.Slider->GetValue();
		FLOAT IdealSetting = (FLOAT)IdealSettingInt / 100.0f;
		if (NewValue <= IdealSetting + 0.01f)
		{
			NewValue = IdealSetting + 0.01f;
		}
	}

	// The DataPtr values are all pointers to values in GEngine - so make sure it is valid!
	check(GEngine);
	*(Group->DataPtr) = NewValue;

	INT NewValueInt = (INT)(NewValue * 100.0f);
	if (bSetSlider)
	{
		NewValueInt = Clamp<INT>(NewValueInt, Group->Slider->GetMin(), Group->Slider->GetMax());
		Group->Slider->SetValue(NewValueInt);
		NewValue = (FLOAT)NewValueInt / 100.0f;
	}

	if (bSetText)
	{
		Group->TextCtrl->SetValue(*FString::Printf(TEXT("%4.2f"), NewValue));
	}

	if (Group == &IdealDensitySlider)
	{
		// We need to make sure that Maximum is always slightly larger than ideal...
		INT MaxSettingInt = MaximumDensitySlider.Slider->GetValue();
		FLOAT MaxSetting = (FLOAT)MaxSettingInt / 100.0f;
		if (NewValue >= MaxSetting - 0.01f)
		{
			MaxSetting = NewValue + 0.01f;
			UpdateControlGroupValue(&MaximumDensitySlider, MaxSetting, TRUE, TRUE);
		}
	}

	GCallbackEvent->Send(CALLBACK_RedrawAllViewports);
}
