//! @file SubstanceAirEdNewGraphInstanceDlg.h
//! @brief Substance Air new graph instance dialog box
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_AIR_CREATE_IMG_INPUT_DLG_H
#define _SUBSTANCE_AIR_CREATE_IMG_INPUT_DLG_H

#include "UnrealEd.h"
#include "UnObjectTools.h"


class WxDlgCreateImageInput : public wxDialog
{
public:
    WxDlgCreateImageInput();

	int ShowModal(const FString& InName, const FString& InPackage, const FString& InGroup);

	UPackage* InstanceOuter;

	FString& getInstanceName() {return InstanceName;}


protected:
	void EvaluatePackageAndGroup();

	void OnOK(wxCommandEvent& In)
	{
		EvaluatePackageAndGroup();
	}


	FString Package, Group, InstanceName;

	wxBoxSizer* PGNSizer;
	wxPanel* PGNPanel;
	WxPkgGrpNameCtrl* PGNCtrl;

	DECLARE_EVENT_TABLE()

private:
	using wxDialog::ShowModal;		// Hide parent implementation
};

#endif // _SUBSTANCE_AIR_CREATE_IMG_INPUT_DLG_H
