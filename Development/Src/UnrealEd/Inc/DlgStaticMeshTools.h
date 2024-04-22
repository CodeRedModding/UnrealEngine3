/*=============================================================================
	DlgStaticMeshTools.h: UnrealEd dialog for displaying static mesh mode options
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DLGSTATICMESHTOOLS_H
#define __DLGSTATICMESHTOOLS_H

/**
  * UnrealEd dialog with various static mesh mode related tools on it.
  */
class WxDlgStaticMeshTools : public wxDialog
{
public:
	WxDlgStaticMeshTools( wxWindow* InParent );
	virtual ~WxDlgStaticMeshTools();

	class WxPropertyWindowHost* PropertyWindow;
	void OnClose( wxCloseEvent& In );
private:

	DECLARE_EVENT_TABLE()
};

#endif

