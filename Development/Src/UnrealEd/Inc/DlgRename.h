/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DLGRENAME_H__
#define __DLGRENAME_H__

class WxDlgRename : public wxDialog
{
public:
	WxDlgRename();
	virtual ~WxDlgRename();

	const FString& GetNewPackage() const
	{
		return NewPackage;
	}
	const FString& GetNewGroup() const
	{
		return NewGroup;
	}
	const FString& GetNewName() const
	{
		return NewName;
	}

	int ShowModal(const FString& InPackage, const FString& InGroup, const FString& InName);
private:
	using wxDialog::ShowModal;		// Hide parent implementation
public:

	/////////////////////////
	// wxWindow interface.

	virtual bool Validate();

private:
	FString OldPackage, OldGroup, OldName;
	FString NewPackage, NewGroup, NewName;

	wxBoxSizer* PGNSizer;
	wxPanel* PGNPanel;
	WxPkgGrpNameCtrl* PGNCtrl;
};

#endif // __DLGRENAME_H__
