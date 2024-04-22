// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "UnrealEdCLR.h"
#include "NewProjectShared.h"

#pragma unmanaged
#include "NewProject.h"
#pragma managed

#ifdef __cplusplus_cli

#include "ManagedCodeSupportCLR.h"
#include "WPFFrameCLR.h"


/** Helper macro to declare a property that maps to an existing settings string. */
#define DECLARE_WIZARD_PROPERTY_STRING( InPropertyName ) \
		property String^ InPropertyName \
		{ \
			String^ get() { return CLRTools::ToString( FNewUDKProjectSettings::Get().##InPropertyName ); } \
			void set( String^ value ) \
			{ \
				FNewUDKProjectSettings::Get().##InPropertyName = CLRTools::ToFString( value ); \
				OnPropertyChanged( #InPropertyName ); \
			} \
		}


/**  
 *	MTemplateTargetListWrapper: Managed wrapper for FUserProjectSettingsListInfo 
 */
ref class MProjectSettingsListWrapper : public INotifyPropertyChanged
{
	int ListIndex;
	FUserProjectSettingsListInfo& ProjectSettingsInfo;

public:
	virtual event PropertyChangedEventHandler^ PropertyChanged;

	MProjectSettingsListWrapper( INT InIndex, FUserProjectSettingsListInfo& InListInfo )
		:	ListIndex( InIndex )
		,	ProjectSettingsInfo( InListInfo )
	{
	}
	
	// Properties for binding
	property int Index { int get() { return ListIndex; } }
	property String^ SettingKey 
	{ 
		String^ get() 
		{ 
			return CLRTools::ToString( ProjectSettingsInfo.SettingData->SettingKey ); 
		} 
		void set(String^ value) 
		{

		}
	}
	property String^ SettingKeyDisplayName
	{ 
		String^ get() 
		{ 
			return CLRTools::ToString( ProjectSettingsInfo.SettingData->GetSettingDisplayName() ); 
		} 
		void set(String^ value) 
		{

		}
	}
	property String^ SettingValue 
	{ 
		String^ get() 
		{ 
			return CLRTools::ToString( ProjectSettingsInfo.SettingData->SettingValue ); 
		}
		void set(String^ value) 
		{ 
			ProjectSettingsInfo.SettingData->SettingValue = CLRTools::ToFString( value );
		}
	}

	property bool IsSelected		
	{ 
		bool get() 
		{ 
			return ProjectSettingsInfo.bIsSelected != 0; 
		}  
		void set(bool value) 
		{ 
			ProjectSettingsInfo.bIsSelected = value;
			PropertyChanged(this, gcnew PropertyChangedEventArgs("IsSelected")); 
		}
	}
};


/**  
 *	MTemplateTargetListWrapper: Managed wrapper for FTemplateTargetListInfo 
 */
ref class MTemplateTargetListWrapper : public INotifyPropertyChanged
{
	int ListIndex;
	FTemplateTargetListInfo& TemplateTargetInfo;

public:
	virtual event PropertyChangedEventHandler^ PropertyChanged;

	MTemplateTargetListWrapper( INT InIndex, FTemplateTargetListInfo& InTargetInfo )
		:	ListIndex( InIndex )
		,	TemplateTargetInfo( InTargetInfo )
	{
	}

	// Properties for binding
	property int Index { int get() { return ListIndex; } }

	property String^ TargetName			{ String^ get() { return CLRTools::ToString( TemplateTargetInfo.TemplateData->GetName() ); } }

	property String^ Description	
	{ 
		String^ get() 
		{
			return CLRTools::ToString( TemplateTargetInfo.TemplateData->GetDescription() );
		}
	}
	property String^ KiloByteSize
	{
		String^ get()
		{
			TCHAR* SizeDescription = TEXT("KByte");
			FLOAT ResourceSize = TemplateTargetInfo.TemplateData->GetInstallSize() / 1024.0f;

			// If the install size is one megabyte or more we use MByte.
			if( ResourceSize > 1024.0f )
			{
				SizeDescription = TEXT("MByte");
				ResourceSize /= 1024.0f;
			}

			// If the install size is on gigabyte or more we use GByte
			if( ResourceSize > 1024.0f )
			{
				SizeDescription = TEXT("GByte");
				ResourceSize /= 1024.0f;
			}
			return CLRTools::ToString( FString::Printf( TEXT("%5.2f %s"),  ResourceSize,  SizeDescription ) );
		}
	}

	property bool IsSelected		
	{ 
		bool get() 
		{ 
			return TemplateTargetInfo.bIsSelected != 0; 
		}  
		void set(bool value) 
		{ 
			TemplateTargetInfo.bIsSelected = value;
			PropertyChanged(this, gcnew PropertyChangedEventArgs("IsSelected")); 
		}
	}
};

typedef MEnumerableTArrayWrapper<MTemplateTargetListWrapper,FTemplateTargetListInfo> MTemplateTargets;

typedef MEnumerableTArrayWrapper<MProjectSettingsListWrapper,FUserProjectSettingsListInfo> MUserSettings;


/** Panel for the new project screen */
ref class MNewProjectPanel : public MWPFPanel
{

private:
	/** Stores a reference to the tab control */
	TabControl^ NavigationTabControl;

public:
	
	property FNewUDKProjectWizard* ProjWizSystem;
	property UBOOL ShowSuccessScreen;
	ComboBox^ TemplateTargetComboBox;
	
	Button^ YesButton;
	Button^ NoButton;
	Button^ CloseWindowButton;
	Button^ FinishButton;
	Button^ NextPanelButton;
	Button^ PrevPanelButton;
	
	
	DECLARE_WIZARD_PROPERTY_STRING( ProjectNameSetting );
	DECLARE_WIZARD_PROPERTY_STRING( ShortNameSetting );
	DECLARE_WIZARD_PROPERTY_STRING( InstallDirectorySetting );
	 
	// Template Targets
	MTemplateTargets^ TemplateTargetList;
	DECLARE_MAPPED_NOTIFY_PROPERTY(MTemplateTargets^, TemplateTargetProperty, MTemplateTargets^, TemplateTargetList);

	MUserSettings^ UserSettingsList;
	DECLARE_MAPPED_NOTIFY_PROPERTY(MUserSettings^, UserProjectSettingsProperty, MUserSettings^, UserSettingsList);

	/**
	 * Property used to tell us if we should enable the next button.
	 */
	property bool IsNotEndOfTabList
	{
		bool get()
		{
			if(NavigationTabControl->SelectedIndex == NavigationTabControl->Items->Count - 3 )
			{
				return false;
			}
			return true;
		}
	}

	/**
	 * Property used to tell us if we should enable the next button.
	 */
	property bool IsNotStartOfTabList
	{
		bool get()
		{
			if(NavigationTabControl->SelectedIndex == 0)
			{
				return false;
			}
			return true;
		}
	}	

public:
	/**
	 * Constructor
	 *
	 * @param InXaml	XAML file to use for the panel
	 */
	MNewProjectPanel( String^ InXaml )
		: MWPFPanel( InXaml )
	{

		// Main Window
		this->PropertyChanged += gcnew ComponentModel::PropertyChangedEventHandler( this, &MNewProjectPanel::OnNewProjectPropertyChanged );

		YesButton = safe_cast< Button^ >(LogicalTreeHelper::FindLogicalNode( this, "YesButton"));
		check( YesButton != nullptr );
		YesButton->Click += gcnew RoutedEventHandler(this, &MNewProjectPanel::OnFinishClicked);

		NoButton = safe_cast< Button^ >(LogicalTreeHelper::FindLogicalNode( this, "NoButton"));
		check( NoButton != nullptr );
		NoButton->Click += gcnew RoutedEventHandler(this, &MNewProjectPanel::OnCloseClicked);

		CloseWindowButton = safe_cast< Button^ >(LogicalTreeHelper::FindLogicalNode( this, "CloseWindowButton"));
		check( CloseWindowButton != nullptr );
		CloseWindowButton->Click += gcnew RoutedEventHandler(this, &MNewProjectPanel::OnCloseClicked);

		FinishButton = safe_cast< Button^ >(LogicalTreeHelper::FindLogicalNode( this, "FinishButton"));
		check( FinishButton != nullptr );
		FinishButton->Click += gcnew RoutedEventHandler(this, &MNewProjectPanel::OnFinishClicked);

		NavigationTabControl = safe_cast< TabControl^ >(LogicalTreeHelper::FindLogicalNode( this, "NavigationTabControl"));
		check( NavigationTabControl != nullptr );
		NavigationTabControl->SelectionChanged += gcnew SelectionChangedEventHandler(this, &MNewProjectPanel::OnTabSelectionChanged);

		NextPanelButton = safe_cast< Button^ >(LogicalTreeHelper::FindLogicalNode( this, "NextPanelButton"));
		check( NextPanelButton != nullptr );
		UnrealEd::Utils::CreateBinding(
			NextPanelButton,
			Button::IsEnabledProperty, this, "IsNotEndOfTabList" );
		NextPanelButton->Click += gcnew RoutedEventHandler(this, &MNewProjectPanel::OnNextPanelButtonClicked);

		PrevPanelButton = safe_cast< Button^ >(LogicalTreeHelper::FindLogicalNode( this, "PrevPanelButton"));
		check( PrevPanelButton != nullptr );
		UnrealEd::Utils::CreateBinding(
			PrevPanelButton,
			Button::IsEnabledProperty, this, "IsNotStartOfTabList" );
		PrevPanelButton->Click += gcnew RoutedEventHandler(this, &MNewProjectPanel::OnPrevPanelButtonClicked);

		// Panel 1
		TemplateTargetComboBox = safe_cast< ComboBox^ >( LogicalTreeHelper::FindLogicalNode( this, "TemplateTargetList"));

		// Panel 2
		TextBox^ ProjectNameTextBox = safe_cast< TextBox^ >( LogicalTreeHelper::FindLogicalNode( this, "ProjectNameTextBox" ) );
		UnrealEd::Utils::CreateBinding(ProjectNameTextBox, TextBox::TextProperty, this, "ProjectNameSetting" );

		TextBox^ ShortNameTextBox = safe_cast< TextBox^ >( LogicalTreeHelper::FindLogicalNode( this, "ShortNameTextBox" ) );
		UnrealEd::Utils::CreateBinding(ShortNameTextBox, TextBox::TextProperty, this, "ShortNameSetting" );

		TextBox^ InstallDirectoryTextBox = safe_cast< TextBox^ >( LogicalTreeHelper::FindLogicalNode( this, "InstallDirectoryTextBox" ) );
		UnrealEd::Utils::CreateBinding(InstallDirectoryTextBox, TextBox::TextProperty, this, "InstallDirectorySetting" );

		Button^ InstallDirectoryBrowseButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "InstallDirectoryBrowseButton" ) );
		InstallDirectoryBrowseButton->Click += gcnew RoutedEventHandler( this, &MNewProjectPanel::InstallDirectoryBrowseButtonClicked );

	}

	/**
	* Callback when the parent frame is set to hook up custom events to its widgets
	*
	* @param InParentFrame	The parent frame to set.
	*/
	virtual void SetParentFrame( MWPFFrame^ InParentFrame ) override
	{
		NavigationTabControl->SelectedIndex = 0;

		if (GetParentFrame() != InParentFrame)
		{
			MWPFPanel::SetParentFrame( InParentFrame );

			SetupItems();
		}
		SetDefaults();
		RefreshAllProperties();

	}

	/** Triggers a refresh of the template target list. */
	void RefreshTemplateTargetListProperties()
	{
		TemplateTargetProperty->NotifyChanged();
	}


protected:

	/** Called when a new project window property is changed */
	void OnNewProjectPropertyChanged( Object^ Owner, ComponentModel::PropertyChangedEventArgs^ Args )
	{
		MNewProjectPanel^ EventWindow = safe_cast<MNewProjectPanel^>(Owner);
	}

private:

	/** Used to setup items we didn't have enough info about in the ctor. */
	void SetupItems()
	{
		check(ProjWizSystem);
		TemplateTargetList = gcnew MTemplateTargets(ProjWizSystem->GetTemplateTargetList());
		TemplateTargetList->PropertyChanged += gcnew PropertyChangedEventHandler( this, &MNewProjectPanel::OnTemplateTargetPropertyChanged );
		UnrealEd::Utils::CreateBinding(TemplateTargetComboBox, ComboBox::ItemsSourceProperty, this, "TemplateTargetProperty");

		UserSettingsList = gcnew MUserSettings(ProjWizSystem->GetUserSettingsList());
		UserSettingsList->PropertyChanged += gcnew PropertyChangedEventHandler( this, &MNewProjectPanel::OnUserSettingListPropertyChanged );
		ListBox^ UserProjectSettingsListBox = safe_cast< ListBox^ >( LogicalTreeHelper::FindLogicalNode( this, "UserProjectSettingsListBox" ) );
		UnrealEd::Utils::CreateBinding(UserProjectSettingsListBox, ListBox::ItemsSourceProperty, this, "UserProjectSettingsProperty");

	}

	/** Used to set default values every time the dialog is shown. */
	void SetDefaults()
	{
		if(ShowSuccessScreen)
		{
			NavigationTabControl->SelectedIndex = 3;
			CloseWindowButton->Visibility = Windows::Visibility::Collapsed;
			FinishButton->Visibility = Windows::Visibility::Collapsed;

			YesButton->Visibility = Windows::Visibility::Visible;
			NoButton->Visibility = Windows::Visibility::Visible;
		}
		else
		{
			NavigationTabControl->SelectedIndex = 0;
			CloseWindowButton->Visibility = Windows::Visibility::Visible;
			FinishButton->Visibility = Windows::Visibility::Visible;

			YesButton->Visibility = Windows::Visibility::Collapsed;
			NoButton->Visibility = Windows::Visibility::Collapsed;
		}
	}

	/** Called when the user clicks the close button */
	void OnCloseClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		ParentFrame->Close( FNewProjectScreen::PROJWIZ_Cancel );
	}

	/** Called when the user clicks the finish button */
	void OnFinishClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		ParentFrame->Close( FNewProjectScreen::PROJWIZ_Finish );
	}

	/** Called when the user clicks the close button */
	void OnNextPanelButtonClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		Button^ NextPanelButton = safe_cast< Button^ >(Owner);

		INT TabCount = NavigationTabControl->Items->Count;
		if( NavigationTabControl->SelectedIndex < TabCount-2 )
		{
			NavigationTabControl->SelectedIndex++;
		}
	}

	/** Called when the user clicks the close button */
	void OnPrevPanelButtonClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		Button^ PrevPanelButton = safe_cast< Button^ >(Owner);
		if( NavigationTabControl->SelectedIndex > 0)
		{
			NavigationTabControl->SelectedIndex--;
		}
	}

	/** Called when the ... button is clicked for the target install path.  This will bring up a file dialog window. */
	void InstallDirectoryBrowseButtonClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		PromptForInstallDirectory();
	}


	/** Called when the user clicks on an tab in our navigation list */
	void OnTabSelectionChanged( Object^ Owner, SelectionChangedEventArgs^ Args )
	{
		TabControl^ LocalNavigationTabControl = (TabControl^)Owner;
		if (nullptr != LocalNavigationTabControl->SelectedItem)
		{
			RefreshAllProperties();
		}
	}

	/** Called when texture paint target is changed */
	void OnTemplateTargetPropertyChanged( Object^ Owner, PropertyChangedEventArgs^ Args )
	{
		RefreshAllProperties();
	}

	/** Called when a user setting item is changed */
	void OnUserSettingListPropertyChanged( Object^ Owner, PropertyChangedEventArgs^ Args )
	{
		
	}

	/** Helper method to prompt the user for a directory */
	void PromptForInstallDirectory()
	{
		// Start at one level above the current app root directory.  For example, in the case of a default UDK install, this would result in c:\\UDK
		String^ StartDirectory = CLRTools::ToString( appCollapseRelativeDirectories( ( appRootDir() * "..\\" ) ) );

		wxDirDialog ChooseDirDialog(
			GApp->EditorFrame,
			*LocalizeUnrealEd("ChooseADirectory"),
			*CLRTools::ToFString( StartDirectory )
			);

		if ( ChooseDirDialog.ShowModal() == wxID_OK )
		{
			InstallDirectorySetting = CLRTools::ToString( FString( ChooseDirDialog.GetPath() ) );
		}
	}
};


#endif // #ifdef __cplusplus_cli

// Initialize static instance to NULL
FNewProjectScreen* FNewProjectScreen::Instance = NULL;

/** 
* Display the new project screen
*
* @return	TRUE if the user clicked finish, FALSE if the user canceled
*/
BOOL FNewProjectScreen::DisplayNewProjectScreen( FNewUDKProjectWizard* InProjWizSystem, UBOOL bShowSuccessScreen )
{
	BOOL rtn = FALSE;

#ifdef __cplusplus_cli
	FNewProjectScreen& Instance = GetInternalInstance();
	Instance.NewProjectPanel->ProjWizSystem = InProjWizSystem;
	Instance.NewProjectPanel->ShowSuccessScreen = bShowSuccessScreen;
	rtn = PROJWIZ_Finish == Instance.NewProjectScreenFrame->SetContentAndShowModal(Instance.NewProjectPanel, 0) ? TRUE : FALSE;

	Instance.NewProjectPanel->ProjWizSystem = NULL;
	Instance.NewProjectPanel->ShowSuccessScreen = FALSE;

#endif // #ifdef __cplusplus_cli

	GApp->EditorFrame->Raise(); // seem to need this to stop other windows activating and hiding the main editor frame
	return rtn;
}


/** Shut down the new project screen singleton */
void FNewProjectScreen::Shutdown()
{
	delete Instance;
	Instance = NULL;
}

/** Constructor */
FNewProjectScreen::FNewProjectScreen()
{
#ifdef __cplusplus_cli
	// Initialize settings for the WPF frame
	WPFFrameInitStruct^ Settings = gcnew WPFFrameInitStruct;
	Settings->WindowTitle = CLRTools::ToString( *LocalizeUnrealEd("NewProjectWindow_Title") );
	Settings->bCenterWindow = TRUE;
	Settings->bUseSaveLayout = FALSE;

	// Construct a WPF frame for the new project screen
	NewProjectScreenFrame = gcnew MWPFFrame( NULL, Settings, TEXT("NewProjectScreen") );
	check( NewProjectScreenFrame );

	// Construct a custom new project screen panel
	NewProjectPanel = gcnew MNewProjectPanel( CLRTools::ToString( TEXT("NewProjectScreen.xaml") ) );
	check( NewProjectPanel );

	delete Settings;
#endif // #ifdef __cplusplus_cli
}

/** Destructor */
FNewProjectScreen::~FNewProjectScreen()
{
	delete NewProjectPanel;
	delete NewProjectScreenFrame;
	NewProjectPanel = NULL;
	NewProjectScreenFrame = NULL;
}

/**
 * Return internal singleton instance of the class
 *
 * @return	Reference to the internal singleton instance of the class
 */
FNewProjectScreen& FNewProjectScreen::GetInternalInstance()
{
	if ( Instance == NULL )
	{
		Instance = new FNewProjectScreen();
	}
	check( Instance );
	return *Instance;
}