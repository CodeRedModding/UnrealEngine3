/*=============================================================================
	DlgMoveAssets.h: UnrealEd dialog for moving assets.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DLGMOVEASSETS_H__
#define __DLGMOVEASSETS_H__

// Forward declarations.
class WxPkgGrpNameTxtCtrl;

/** Dialog for moving assets e.g. between packages. */
class WxDlgMoveAssets : public wxDialog
{
public:
	WxDlgMoveAssets();
	virtual ~WxDlgMoveAssets();

	const FString& GetNewPackage() const
	{
		return NewPackage;
	}
	const FString& GetNewGroup() const
	{
		return NewGroup;
	}
	/**
	 * Sets the output parameter to have the value of the name field.
	 * @return		TRUE if the name field value is a suffix, FALSE if it's a replacement name.
	 */
	UBOOL GetNewName(FString& OutNewName) const
	{
		OutNewName = NewName;
		return bTreatNameAsSuffix;
	}

	/** @return		TRUE if the 'Include References?' checkbox is checked. */
	UBOOL GetIncludeRefs() const
	{
		return bIncludeRefs;
	}

	/** @return		TRUE if the user specified that packages should be checked out after the move/dup operation. */
	UBOOL GetCheckoutPackages() const
	{
		return bCheckoutPackages;
	}

	/** @return		TRUE if the user specified that packages should be saved after the move/dup operation. */
	UBOOL GetSavePackages() const
	{
		return bSavePackages;
	}

	/** Displays the dialog in a blocking fashion */
	int ShowModal( const FString& InPackage, const FString& InGroup, const FString& InName, UBOOL bAllowPackageRename );
private:
	using wxDialog::ShowModal;		// Hide parent implementation
public:

	/** Sets whether or not the 'name' field can be used as a name suffix. */
	void ConfigureNameField(UBOOL bEnableTreatNameAsSuffix);

	/////////////////////////
	// wxWindow interface.

	virtual bool Validate();

	/**
	 * Determines a class-specific target package and group for the specified object.
	*/
	void DetermineClassPackageAndGroup(const UObject* InObject, FString& OutPackage, FString& OutGroup) const;

private:
	FString NewPackage, NewGroup, NewName;

	/** If TRUE, the 'Include References?' checkbox is checked. */
	UBOOL	bIncludeRefs;
	/** If TRUE, the 'Treat Name as Suffix?' checkbox is checked. */
	UBOOL	bTreatNameAsSuffix;
	/** If TRUE, the user specified that packages should be checked out after the move/dup operation. */
	UBOOL	bCheckoutPackages;
	/** If TRUE, the user specified that packages should be saved after the move/dup operation. */
	UBOOL	bSavePackages;

	WxPkgGrpNameTxtCtrl* PGNCtrl;

	DECLARE_EVENT_TABLE();
	void OnOKToAll(wxCommandEvent& In);
};

#endif // __DLGMOVEASSETS_H__
