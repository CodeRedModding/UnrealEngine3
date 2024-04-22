/*=============================================================================
	DlgGeometryTools.h: UnrealEd dialog for displaying map build progress and cancelling builds.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DLGGEOMETRYTOOLS_H
#define __DLGGEOMETRYTOOLS_H

/**
  * UnrealEd dialog with various geometry mode related tools on it.
  */
class WxDlgGeometryTools : public wxDialog
{
public:
	WxDlgGeometryTools( wxWindow* InParent );
	virtual ~WxDlgGeometryTools();

	void OnModifierSelected( wxCommandEvent& In );
	void OnModifierClicked( wxCommandEvent& In );

	/**
	* Enable/Disable modifier buttons based on the modifier supporting the current selection type.
	*/
	void OnUpdateUI( wxUpdateUIEvent& In );

	/**
	* Takes any input the user has made to the Keyboard section
	* of the current modifier and causes the modifier activate.
	*/
	void OnActionClicked( wxCommandEvent& In );

	void OnClose( wxCloseEvent& In );

	class WxPropertyWindowHost* PropertyWindow;

private:

	UBOOL IsShowingProperties;
	class wxBoxSizer* MainVerticalSizer;
	class wxStaticBoxSizer* PropertyGroupSizer;

	TArray<wxRadioButton*> RadioButtons;
	TArray<wxButton*> PushButtons;

	DECLARE_EVENT_TABLE()
};

#endif

