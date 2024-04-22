/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DLGOPENPACKAGES_H__
#define __DLGOPENPACKAGES_H__

class PackageTreePath : public wxTreeItemData
{
public:
	PackageTreePath( FString InPath );

	FString Path;
};

class WxDlgOpenPackages : public wxDialog
{
public:
	WxDlgOpenPackages();
	virtual ~WxDlgOpenPackages();

	void OnOK( wxCommandEvent& In );

	TArray<FFilename> SelectedItems;

private:

	wxTreeCtrl* PackageTreeCtrl;

	DECLARE_EVENT_TABLE()
};

#endif // __DLGOPENPACKAGES_H__
