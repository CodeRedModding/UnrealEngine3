/*=============================================================================
	DlgDensityRenderingOptions.h: UnrealEd dialog for displaying density render
		mode options.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef __DLGDENSITYRENDERINGOPTIONS_H__
#define __DLGDENSITYRENDERINGOPTIONS_H__

#include "TrackableWindow.h"

/**
 * Dialog that displays the options for rendering density view modes.
 */
class WxDlgDensityRenderingOptions : public WxTrackableDialog
{
public:
	WxDlgDensityRenderingOptions(wxWindow* InParent);
	virtual ~WxDlgDensityRenderingOptions();

	enum EControlGroup
	{
		DRO_IdealDensity,
		DRO_MaximumDensity,
		DRO_ColorScale,
		DRO_GrayscaleScale,
		DRO_MAX
	};

	/**
	 *	Show or hide the dialog
	 *
	 *	@param	show	If TRUE, show the dialog; if FALSE, hide it
	 *
	 *	@return	bool	
	 */
	virtual bool Show( bool show = true );

	/** Event handler for render grayscale checkbox */
	virtual void OnRenderGrayscaleChecked(wxCommandEvent& In);
	/** Event handler for slider changes */
	virtual void OnSliderChanged(wxScrollEvent& In);
	/** Event handler for text ctrl changes */
	virtual void OnTextCtrlChanged(wxCommandEvent& In);

protected:
	struct SliderGroup
	{
		/** The control group ID */
		EControlGroup ControlGroup;
		/** Slider name. */
		wxStaticBoxSizer* BoxSizer;
		/** Slider for controlling value. */
		wxSlider* Slider;
		/** Text-box for displaying value. */
		wxTextCtrl* TextCtrl;

		/** The data value that goes w/ this control */
		FLOAT* DataPtr;
	};
	/** Ideal Density value. */
	SliderGroup IdealDensitySlider;
	/** Maximum Density value. */
	SliderGroup MaximumDensitySlider;
	/** Color Density Scale value. */
	SliderGroup ColorScaleSlider;
	/** Grayscale Density Scale value. */
	SliderGroup GrayscaleScaleSlider;

	SliderGroup* ControlGroups[DRO_MAX];

	wxCheckBox* RenderGrayscaleCheckBox;

	/** The values tied to the above controls. */

	/** Set up the controls for the given type */
	void CreateControlGroup(EControlGroup ControlGroup, wxSizer* OwningSizer);

	/** Get the slider group for the given ControlGroup */
	SliderGroup* GetControlGroup(EControlGroup ControlGroup);

	/** Get the slider group for the given control type and its Id */
	SliderGroup* GetControlGroupFromSliderId(int InId);
	SliderGroup* GetControlGroupFromTextCtrlId(int InId);

	/** 
	 *	Update the value for the given group.
	 *
	 *	@param	Group			The group to update the value for.
	 *	@param	NewValue		The value to set.
	 *	@param	bSetSlider		If TRUE, set the slider control.
	 *	@param	bSetText		If TRUE, set the text control.
	 */
	void UpdateControlGroupValue(SliderGroup* Group, FLOAT NewValue, UBOOL bSetSlider, UBOOL bSetText);

protected:
	DECLARE_EVENT_TABLE()
};

#endif	//__DLGDENSITYRENDERINGOPTIONS_H__
