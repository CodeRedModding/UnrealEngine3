//! @file SubstanceAirEdNewGraphInstanceDlg.h
//! @brief Substance Air new graph instance dialog box
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_AIR_NEW_GRAPH_INSTANCE_DLG_H
#define _SUBSTANCE_AIR_NEW_GRAPH_INSTANCE_DLG_H

#include "UnrealEd.h"
#include "UnObjectTools.h"


class WxDlgNewGraphInstance : public wxDialog
{
public:
    WxDlgNewGraphInstance(UBOOL ShowCreateMaterialCheckBox=TRUE);

	int ShowModal(const FString& InName, const FString& InPackage, const FString& InGroup);

	UPackage* InstanceOuter;

	FString& getInstanceName() {return InstanceName;}

	UBOOL GetCreateMaterial()
	{
		return CreateMaterial;
	}

protected:
	void EvaluatePackageAndGroup();

	void OnOK(wxCommandEvent& In)
	{
		EvaluatePackageAndGroup();
	}


	FString Package, Group, InstanceName;
	UBOOL CreateMaterial;

	wxBoxSizer* PGNSizer;
	wxPanel* PGNPanel;
	WxPkgGrpNameSbsCtrl* PGNCtrl;

	DECLARE_EVENT_TABLE()

private:
	using wxDialog::ShowModal;		// Hide parent implementation
};

#endif // _SUBSTANCE_AIR_NEW_GRAPH_INSTANCE_DLG_H
