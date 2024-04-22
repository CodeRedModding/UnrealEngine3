/*=============================================================================
	DlgNewGeneric.h: UnrealEd's new generic dialog.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include <wx/fontdlg.h>

class WxPropertyWindowHost;

class WxDlgNewGeneric :
	public wxDialog,
	public FSerializableObject
{
public:
	WxDlgNewGeneric();

	int ShowModal( const FString& InPackage, const FString& InGroup, UClass* DefaultFactoryClass=NULL, TArray< UGenericBrowserType* >* InBrowsableObjectTypeList = NULL );
private:
	using wxDialog::ShowModal;		// Hide parent implementation
public:

	/////////////////////////
	// wxWindow interface.
	virtual bool Validate();

	/**
	 * Since we create a UFactory object, it needs to be serialized
	 *
	 * @param Ar The archive to serialize with
	 */
	virtual void Serialize(FArchive& Ar);

	/**
	 * Accessor for retrieving the chosen factory class
	 */
	UClass* GetFactoryClass() const
	{
		return Factory ? Factory->GetClass() : NULL;
	}

	/**
	 * Accessor for retrieving the created object.
	 *
	 * @return	UObject		The created object using this dialog. Can be NULL if accessed before 
	 *						the user hits OK or if the object creation was unsuccessful.
	 */
	UObject* GetCreatedObject() const
	{
		return CreatedObject;
	}

private:
	FString Package, Group, Name;
	UFactory* Factory;
	WxPropertyWindowHost* PropertyWindow;

	/** The object that was created with this dialog. */
	UObject* CreatedObject;

	wxBoxSizer* PGNSizer;
	WxPkgGrpNameCtrl* PGNCtrl;
	wxComboBox *FactoryCombo;

	TArray< UGenericBrowserType* > BrowsableObjectTypeList;

	/**
	 * The child ids for the font controls
	 */
	enum EFontControlIDs
	{
		ID_CHOOSE_FONT,
		IDCB_FACTORY,
		ID_PKGGRPNAME,
		ID_PROPERTY_WINDOW
	};
	/**
	 * Cached sizer so that we can add the font controls to it if needed
	 */
	wxStaticBoxSizer* FactorySizer;
	/**
	 * This sizer contains the font specific controls that are adding when
	 * creating a new font
	 */
	wxBoxSizer* FontSizer;
	/**
	 * The button that was added
	 */
	wxButton* ChooseFontButton;
	/**
	 * The static text that will show the font
	 */
	wxStaticText* FontStaticText;

	/**
	 * Adds the specific font controls to the dialog
	 */
	void AppendFontControls(void);

	/**
	 * Removes the font controls from the dialog
	 */
	void RemoveFontControls(void);

	/**
	 * Lets the user select the font that is going to be imported
	 */
	void OnChooseFont(wxCommandEvent& In);

	void OnFactorySelChange( wxCommandEvent& In );

	/**
	 * Fetches the package and outermost package, given the name/group, returns false if failed
	 */
	bool GetPackages( const FString& rPackageIn, const FString& rGroupIn, UPackage*& prPkgOut, UPackage*& prOtmPkgOut);

	DECLARE_EVENT_TABLE()

public:
	FString GetPackageName()	{	return Package;	}
	FString GetGroupName()		{	return Group;	}
	FString GetObjectName()		{	return Name;	}
};
