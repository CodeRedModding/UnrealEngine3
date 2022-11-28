/*=============================================================================
	Dialogs.h: Dialog boxes used within the editor

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

/*-----------------------------------------------------------------------------
	WxDlgAbout.
-----------------------------------------------------------------------------*/

class WxDlgAbout : public wxDialog
{
public:
	WxDlgAbout();
	~WxDlgAbout();

	WxBitmap Bitmap;
	wxStaticBitmap* BitmapStatic;
	wxStaticText* VersionStatic;

	void OnOK( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgRename.
-----------------------------------------------------------------------------*/

class WxDlgRename : public wxDialog
{
public:
	WxDlgRename();
	~WxDlgRename();

	FString OldPackage, OldGroup, OldName, NewPackage, NewGroup, NewName;
	wxTextCtrl *PackageEdit, *GroupEdit, *NameEdit;

	int ShowModal( FString InPackage, FString InGroup, FString InName );

	virtual bool Validate();

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgPackageGroupName.
-----------------------------------------------------------------------------*/

class WxDlgPackageGroupName : public wxDialog
{
public:
	WxDlgPackageGroupName();
	~WxDlgPackageGroupName();

	FString Package, Group, Name;
	wxTextCtrl *PackageEdit, *GroupEdit, *NameEdit;

	int ShowModal( FString InPackage, FString InGroup, FString InName );

	void OnOK( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgAddSpecial.
-----------------------------------------------------------------------------*/

class WxDlgAddSpecial : public wxDialog
{
public:
	WxDlgAddSpecial();
	~WxDlgAddSpecial();

	wxComboBox *PrefabsCombo;
	wxCheckBox *AntiPortalCheck, *PortalCheck, *MirrorCheck, *InvisibleCheck, *TwoSidedCheck;
	wxRadioButton *SolidRadio, *SemiSolidRadio, *NonSolidRadio;

	void OnOK( wxCommandEvent& In );
	void OnPrefabsSelChange( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgImportBrush.
-----------------------------------------------------------------------------*/

class WxDlgImportBrush : public wxDialog
{
public:
	WxDlgImportBrush();
	~WxDlgImportBrush();

	wxCheckBox *MergeFacesCheck;
	wxRadioButton *SolidRadio, *NonSolidRadio;

	FString Filename;

	int ShowModal( FString InFilename );

	void OnOK( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgImportBase.
-----------------------------------------------------------------------------*/

class WxDlgImportBase : public wxDialog
{
public:
	WxDlgImportBase();
	WxDlgImportBase( FString InResourceName );
	~WxDlgImportBase();

	FString Package, Group, Name;
	TArray<FFilename>* Filenames;

	UBOOL bOKToAll;
	INT iCurrentFilename;

	wxStaticText *FilenameStatic;
	wxTextCtrl *PackageText, *GroupText, *NameText;

	virtual UBOOL GetDataFromUser( void );
	virtual void ImportFile( FString Filename );

	void SetNextFilename();
	virtual void RefreshName();

	int ShowModal( FString InPackage, FString InGroup, TArray<FFilename>* InFilenames );

	void OnOK( wxCommandEvent& In );
	void OnOKAll( wxCommandEvent& In );
	void OnSkip( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgGroup.
-----------------------------------------------------------------------------*/

class WxDlgGroup : public wxDialog
{
public:
	WxDlgGroup();
	~WxDlgGroup();

	FString Name;
	wxTextCtrl *NameEdit;

	int ShowModal( UBOOL InNew, FString InName );

	void OnOK( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgNewGeneric.
-----------------------------------------------------------------------------*/

class WxDlgNewGeneric : public wxDialog
{
public:
	WxDlgNewGeneric();
	~WxDlgNewGeneric();

	FString Package, Group, Name;
	UFactory* Factory;
	WxPropertyWindow* PropertyWindow;

	wxTextCtrl *PackageEdit, *GroupEdit, *NameEdit;
	wxComboBox *FactoryCombo;
	wxBoxSizer* PropsPanelSizer;

	int ShowModal( FString InPackage, FString InGroup, UClass* DefaultFactoryClass=NULL );
	virtual bool Validate();

	void OnFactorySelChange( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgImportGeneric.
-----------------------------------------------------------------------------*/

class WxDlgImportGeneric : public wxDialog
{
public:
	WxDlgImportGeneric();
	~WxDlgImportGeneric();

	FString Package, Group, Name;
	UFactory* Factory;
	UClass* Class;
	FFilename Filename;
	WxPropertyWindow* PropertyWindow;

	wxTextCtrl *PackageEdit, *GroupEdit, *NameEdit;
	wxComboBox *FactoryCombo;
	wxBoxSizer* PropsPanelSizer;

	int ShowModal( FFilename InFilename, FString InPackage, FString InGroup, UClass* InClass, UFactory* InFactory );

	void OnOK( wxCommandEvent& In );
	void OnOKAll( wxCommandEvent& In );
	void DoImport( UBOOL bOKToAll );
	void OnSize( wxSizeEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgExportGeneric.
-----------------------------------------------------------------------------*/

class WxDlgExportGeneric : public wxDialog
{
public:
	WxDlgExportGeneric();
	~WxDlgExportGeneric();

	UObject* Object;
	UExporter* Exporter;
	FFilename Filename;
	WxPropertyWindow* PropertyWindow;

	wxTextCtrl *NameText;

	int ShowModal( FFilename InFilename, UObject* InObject, UExporter* InExporter );

	void OnOK( wxCommandEvent& In );
	void DoExport();
	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgTextureProperties.
-----------------------------------------------------------------------------*/

class WxDlgTextureProperties : public wxDialog, FViewportClient, public FNotifyHook
{
public:
	WxDlgTextureProperties();
	~WxDlgTextureProperties();

	UTexture* Texture;
	EViewportHolder* ViewportHolder;
	FChildViewport* Viewport;
	WxPropertyWindow* PropertyWindow;

	virtual void Draw(FChildViewport* Viewport,FRenderInterface* RI);

	bool Show( const bool InShow, UTexture* InTexture );

	void OnOK( wxCommandEvent& In );
	void OnSize( wxSizeEvent& In );

	// FNotify interface

	void NotifyDestroy( void* Src );
	void NotifyPreChange( void* Src, UProperty* PropertyAboutToChange );
	void NotifyPostChange( void* Src, UProperty* PropertyThatChanged );
	void NotifyExec( void* Src, const TCHAR* Cmd );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgBrushBuilder.
-----------------------------------------------------------------------------*/

class WxDlgBrushBuilder : public wxDialog
{
public:
	WxDlgBrushBuilder();
	~WxDlgBrushBuilder();

	UBrushBuilder* BrushBuilder;
	WxPropertyWindow* PropertyWindow;

	UBOOL Show( UBrushBuilder* InBrushBuilder, UBOOL InShow = 1 );

	void OnOK( wxCommandEvent& In );
	void OnClose( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgGenericOptions.
-----------------------------------------------------------------------------*/

class WxDlgGenericOptions : public wxDialog
{
public:
	WxDlgGenericOptions();
	~WxDlgGenericOptions();

	UObject* Object;
	FString Caption;
	WxPropertyWindow* PropertyWindow;

	UBOOL ShowModal( UObject* InObject, FString InCaption );

	void OnOK( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgGenericStringEntry
-----------------------------------------------------------------------------*/

class WxDlgGenericStringEntry : public wxDialog
{
public:
	WxDlgGenericStringEntry();
	~WxDlgGenericStringEntry();

	FString			EnteredString;
	wxTextCtrl		*StringEntry;
	wxStaticText	*StringCaption;

	int ShowModal( const TCHAR* DialogTitle, const TCHAR* Caption, const TCHAR* DefaultString );
	void OnOK( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgGenericComboEntry
-----------------------------------------------------------------------------*/

class WxDlgGenericComboEntry : public wxDialog
{
public:
	WxDlgGenericComboEntry();
	~WxDlgGenericComboEntry();

	FString			SelectedString;
	wxComboBox*		ComboBox;
	wxStaticText*	ComboCaption;

	int ShowModal(const TCHAR* InDialogTitle, const TCHAR* INComboCaption, TArray<FString>& InComboOptions);
	void OnOK( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgActorFactory
-----------------------------------------------------------------------------*/

class WxDlgActorFactory : public wxDialog
{
public:
	WxDlgActorFactory();
	~WxDlgActorFactory();

	UActorFactory* Factory;
	WxPropertyWindow* PropertyWindow;
	wxStaticText *NameText;

	int ShowModal( UActorFactory* InFactory );
	void OnOK( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxSurfPropPage1
-----------------------------------------------------------------------------*/

class WxSurfPropPage1 : public wxPanel
{
public:
	WxSurfPropPage1( wxWindow* InParent );
	~WxSurfPropPage1();

	wxPanel* Panel;

	wxRadioButton *SimpleScaleButton;
	wxComboBox *SimpleCB;
	wxStaticText *CustomULabel, *CustomVLabel;
	wxTextCtrl *CustomUEdit, *CustomVEdit;
	wxCheckBox *RelativeCheck;
	wxComboBox *LightMapResCombo;

	void RefreshPage();
	void Pan( INT InU, INT InV );
	void Scale( FLOAT InScaleU, FLOAT InScaleV, UBOOL InRelative );

	void OnU1( wxCommandEvent& In );
	void OnU4( wxCommandEvent& In );
	void OnU16( wxCommandEvent& In );
	void OnU64( wxCommandEvent& In );
	void OnV1( wxCommandEvent& In );
	void OnV4( wxCommandEvent& In );
	void OnV16( wxCommandEvent& In );
	void OnV64( wxCommandEvent& In );
	void OnFlipU( wxCommandEvent& In );
	void OnFlipV( wxCommandEvent& In );
	void OnRot45( wxCommandEvent& In );
	void OnRot90( wxCommandEvent& In );
	void OnApply( wxCommandEvent& In );
	void OnScaleSimple( wxCommandEvent& In );
	void OnScaleCustom( wxCommandEvent& In );
	void OnLightMapResSelChange( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxSurfPropPage2
-----------------------------------------------------------------------------*/

class WxSurfPropPage2 : public wxPanel
{
public:
	WxSurfPropPage2( wxWindow* InParent );
	~WxSurfPropPage2();

	wxPanel* Panel;
	WxPropertyWindow* PropertyWindow;
	wxListBox *AlignList;

	void RefreshPage();

	void OnAlignSelChange( wxCommandEvent& In );
	void OnApply( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgSurfaceProperties
-----------------------------------------------------------------------------*/

class WxDlgSurfaceProperties : public wxDialog
{
public:
	WxDlgSurfaceProperties();
	~WxDlgSurfaceProperties();

	wxNotebook *Notebook;
	WxSurfPropPage1* Page1;
	WxSurfPropPage2* Page2;

	void RefreshPages();

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
