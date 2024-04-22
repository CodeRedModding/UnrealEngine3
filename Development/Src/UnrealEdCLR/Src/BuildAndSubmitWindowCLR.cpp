/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEdCLR.h"
#include "BuildAndSubmitWindowShared.h"
#include "EditorBuildUtils.h"

#if HAVE_SCC

#ifdef __cplusplus_cli
#include "ManagedCodeSupportCLR.h"
#include "WPFFrameCLR.h"

using namespace System::Text;
using namespace System::ComponentModel;
using namespace System::Collections::ObjectModel;
using namespace System::Windows::Data;

/**
 * Helper class for the build and submit dialog. Acts as an item in the dialog's list of checked
 * out packages to select packages that should be checked back in
 */
ref class BuildAndSubmitCheckBoxListViewItem : public INotifyPropertyChanged
{
public:
	// String that should appear in the list view
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( String^, Text );

	// Wheather this item is enabled or not
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( bool, IsEnabled );

	// Whether this item is selected or not
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( bool, IsSelected );

	// The current state of this item in source control
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( ESourceControlState, SourceControlState );

	/**
	 * Constructor
	 *
	 * @param	InText	String that should appear for the item in the list view
	 */
	BuildAndSubmitCheckBoxListViewItem( String^ InText )
	{
		Text = InText;
		IsEnabled = TRUE;
		IsSelected = FALSE;
		SourceControlState = SCC_DontCare;
	}

	/** Overriden Version of ToString() */
	virtual String^ ToString() override
	{
		return Text;
	}

	/** INotifyPropertyChanged: Exposed event from INotifyPropertychanged::PropertyChanged */ 
	virtual event PropertyChangedEventHandler^ PropertyChanged;

	/**
	 * Called when a property has changed
	 *
	 * @param Info	Name of the changed property
	 */
	virtual void OnPropertyChanged( String^ Info )
	{
		PropertyChanged( this, gcnew PropertyChangedEventArgs( Info ) );
	}
};

/** 
 * Simple panel to allow the user to provide some basic build options and a changelist description before kicking off
 * a full build followed by an automatic saving and submission of all map files.
 */
ref class MBuildAndSubmitPanel : public MWPFPanel
{
public:
	/**
	 * Constructor
	 *
	 * @param	InXamlName		Name of the XAML file specifying this panel
	 * @param	InEventListener	Event listener to notify of source control actions
	 */
	MBuildAndSubmitPanel( String^ InXamlName, FSourceControlEventListener* InEventListener )
		:	MWPFPanel( InXamlName ),
			SCEventListener( InEventListener ),
			bAnyLevelsHidden( FALSE )
	{
		// Set up all of the panel's controls as necessary

		Button^ OKButton = safe_cast<Button^>( LogicalTreeHelper::FindLogicalNode( this, "OKButton" ) );
		check( OKButton != nullptr );
		OKButton->Click += gcnew RoutedEventHandler( this, &MBuildAndSubmitPanel::OKClicked );
	
		Button^ CancelButton = safe_cast<Button^>( LogicalTreeHelper::FindLogicalNode( this, "CancelButton") );
		check( CancelButton != nullptr );
		CancelButton->Click += gcnew RoutedEventHandler( this, &MBuildAndSubmitPanel::CancelClicked );

		Button^ DismissErrorPanelButton = safe_cast<Button^>( LogicalTreeHelper::FindLogicalNode( this, "DismissErrorPanelButton" ) );
		check( DismissErrorPanelButton != nullptr );
		DismissErrorPanelButton->Click += gcnew RoutedEventHandler( this, &MBuildAndSubmitPanel::DismissErrorPanelClicked );

		BuildErrorCheckBox = safe_cast<CheckBox^>( LogicalTreeHelper::FindLogicalNode( this, "BuildErrorCheckBox") );
		check( BuildErrorCheckBox != nullptr );

		AddNotInDepotCheckBox = safe_cast<CheckBox^>( LogicalTreeHelper::FindLogicalNode( this, "AddNotInDepotCheckBox") );
		check( AddNotInDepotCheckBox != nullptr );

		IncludeUnsourcedPackagesCheckBox = safe_cast<CheckBox^>( LogicalTreeHelper::FindLogicalNode( this, "IncludeUnsourcedPackagesCheckBox") );
		check( IncludeUnsourcedPackagesCheckBox != nullptr );
		IncludeUnsourcedPackagesCheckBox->Click += gcnew RoutedEventHandler( this, &MBuildAndSubmitPanel::IncludeUnsourcedPackagesClick );

		SaveErrorCheckBox = safe_cast<CheckBox^>( LogicalTreeHelper::FindLogicalNode( this, "SaveErrorCheckBox") );
		check( SaveErrorCheckBox != nullptr );

		ChangelistDescriptionTextBox = safe_cast<TextBox^>( LogicalTreeHelper::FindLogicalNode( this, "ChangelistDescriptionTextBox") );
		check( ChangelistDescriptionTextBox != nullptr );

		ErrorPanel = safe_cast<DockPanel^>( LogicalTreeHelper::FindLogicalNode( this, "ErrorPanel" ) );
		check( ErrorPanel != nullptr );

		ErrorPanelTextBlock = safe_cast<TextBlock^>( LogicalTreeHelper::FindLogicalNode( this, "ErrorPanelTextBlock") );
		check( ErrorPanelTextBlock != nullptr );

		SubmitListView = safe_cast< ListView^ >( LogicalTreeHelper::FindLogicalNode(this, "SubmitListView") );
		SubmitListView->AddHandler( GridViewColumnHeader::ClickEvent, gcnew RoutedEventHandler( &MBuildAndSubmitPanel::ColumnHeaderClicked ) );
		check( SubmitListView != nullptr );

		ListViewItemSource = gcnew ObservableCollection<BuildAndSubmitCheckBoxListViewItem^>();

		const TArray<FString> Packages (GPackageFileCache->GetPackageFileList());
		for ( TArray<FString>::TConstIterator PackageIter( Packages ); PackageIter; ++PackageIter )
		{
			FFilename Filename = *PackageIter;
			FFilename BaseFileName = Filename.GetBaseFilename();
			const ESourceControlState CurPackageSCCState = (ESourceControlState)GPackageFileCache->GetSourceControlState( *BaseFileName );

			// Only include non-map packages that are currently checked out or packages not under source control
			if( (CurPackageSCCState == SCC_CheckedOut || CurPackageSCCState == SCC_NotInDepot) && !CLRTools::IsMapPackageAsset( *Filename ) )
			{
				bool bSelected = FALSE;
				if( CurPackageSCCState == SCC_CheckedOut )
				{
					// Checked out packages must be dirty to be included
					UPackage* CheckedOutPkg = UObject::FindPackage( NULL, *BaseFileName );
					if( CheckedOutPkg == NULL || !CheckedOutPkg->IsDirty() )
					{
						continue;
					}
					// Checked out packages are selected by default
					bSelected = TRUE;
				}
				BuildAndSubmitCheckBoxListViewItem^ PackageItem = gcnew BuildAndSubmitCheckBoxListViewItem( CLRTools::ToString( *BaseFileName) );
				PackageItem->SourceControlState = CurPackageSCCState;
				PackageItem->IsSelected = bSelected;
				ListViewItemSource->Add( PackageItem );
			}
		}
		SubmitListView->ItemsSource = ListViewItemSource;

		// Provide filtering based on package source control state
		ListItemCollectionView = CollectionViewSource::GetDefaultView( ListViewItemSource );
		ListItemCollectionView->Filter = gcnew Predicate<Object^>(this, &MBuildAndSubmitPanel::FilterPackageList);

		// See if any levels are currently hidden and warn the user they will be forced visible if the build proceeds
		WarnOfHiddenLevels();
	}

	/**
	 * Callback when the parent frame is set to hook up custom events to its widgets
	 */
	virtual void SetParentFrame( MWPFFrame^ InParentFrame ) override
	{
		MWPFPanel::SetParentFrame( InParentFrame );

		// Make sure to treat the close as a cancel
		Button^ CloseButton = safe_cast<Button^>( LogicalTreeHelper::FindLogicalNode( InParentFrame->GetRootVisual(), "TitleBarCloseButton" ) );
		CloseButton->Click += gcnew RoutedEventHandler( this, &MBuildAndSubmitPanel::CancelClicked );
	}

private:
	// Only show currently checked out packages. If requested, also show unsourced packages.
	bool FilterPackageList(Object^ item)
	{
		BuildAndSubmitCheckBoxListViewItem^ PackageItem = safe_cast< BuildAndSubmitCheckBoxListViewItem^ >(item);
		return (PackageItem != nullptr) && ( PackageItem->SourceControlState == SCC_CheckedOut ||
			( PackageItem->SourceControlState == SCC_NotInDepot && IncludeUnsourcedPackagesCheckBox->IsChecked.Value ));
	}

	/** Called when the Build/OK button is clicked by the user */
	void OKClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		// Configure build settings for the automated build
		FEditorBuildUtils::FEditorAutomatedBuildSettings BuildSettings;
		
		// Set the settings that are based on user checkboxes in the UI
		BuildSettings.bAutoAddNewFiles = AddNotInDepotCheckBox->IsChecked.HasValue && AddNotInDepotCheckBox->IsChecked.Value && AddNotInDepotCheckBox->IsEnabled;
		BuildSettings.bCheckInPackages = TRUE;
		BuildSettings.BuildErrorBehavior = ( BuildErrorCheckBox->IsChecked.HasValue && BuildErrorCheckBox->IsChecked.Value ) ? FEditorBuildUtils::ABB_FailOnError : FEditorBuildUtils::ABB_ProceedOnError;
		BuildSettings.FailedToSaveBehavior = ( SaveErrorCheckBox->IsChecked.HasValue && SaveErrorCheckBox->IsChecked.Value ) ? FEditorBuildUtils::ABB_FailOnError : FEditorBuildUtils::ABB_ProceedOnError;
		
		for each( BuildAndSubmitCheckBoxListViewItem^ Item in SubmitListView->Items)
		{
			if(Item->IsSelected)
			{
				BuildSettings.PackagesToCheckIn.AddItem(CLRTools::ToFString(Item->Text));
			}
		}

		BuildSettings.ChangeDescription = CLRTools::ToFString( ChangelistDescriptionTextBox->Text );
		
		// The editor shouldn't be shutdown while using this special editor window
		BuildSettings.bShutdownEditorOnCompletion = FALSE;
		BuildSettings.SCCEventListener = SCEventListener.Get();
		
		// Prompt the user on what to do if unsaved maps are detected or if a file can't be checked out for some reason
		BuildSettings.NewMapBehavior = FEditorBuildUtils::ABB_PromptOnError;
		BuildSettings.UnableToCheckoutFilesBehavior = FEditorBuildUtils::ABB_PromptOnError;

		// Attempt the automated build process
		FString ErrorMessages;
		UBOOL bBuildSuccessful = FEditorBuildUtils::EditorAutomatedBuildAndSubmit( BuildSettings, ErrorMessages );

		// If the build ran successfully, close the dialog
		if ( bBuildSuccessful )
		{
			ParentFrame->Close( 0 );
		}
		// If the build failed, display any relevant error message to the user
		else if ( ErrorMessages.Len() > 0 )
		{
			DisplayErrorMessage( CLRTools::ToString( ErrorMessages ) );
		}
	}

	/** Called when the user cancels out of the dialog */
	void CancelClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		ParentFrame->Close( 0 );
	}

	/** Called when the user manually dismisses the error panel */
	void DismissErrorPanelClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		ErrorPanel->Visibility = System::Windows::Visibility::Collapsed;
		ErrorPanelTextBlock->Text = String::Empty;
	}

	/** Called when the user clicks the include unsourced packages checkbox */
	void IncludeUnsourcedPackagesClick( Object^ Owner, RoutedEventArgs^ Args )
	{
		AddNotInDepotCheckBox->IsEnabled = IncludeUnsourcedPackagesCheckBox->IsChecked.Value;
		ListItemCollectionView->Refresh();
	}

	/**
	 * Displays a message in the window's error panel, forcing the panel visible if it already isn't
	 *
	 * @param	InErrorMsg	Message to show on the error panel
	 */
	void DisplayErrorMessage( String^ InErrorMsg )
	{
		ErrorPanel->Visibility = System::Windows::Visibility::Visible;
		ErrorPanelTextBlock->Text = InErrorMsg;
	}

	/** Helper function to alert the user if any levels are currently hidden */
	void WarnOfHiddenLevels()
	{
		// Check if any of the levels are hidden
		bAnyLevelsHidden = !FLevelUtils::IsLevelVisible( GWorld->PersistentLevel );
		if ( !bAnyLevelsHidden )
		{
			for ( TArray<ULevelStreaming*>::TConstIterator LevelIter( GWorld->GetWorldInfo()->StreamingLevels ); LevelIter; ++LevelIter )
			{
				ULevelStreaming* CurStreamingLevel = *LevelIter;
				if ( CurStreamingLevel && !FLevelUtils::IsLevelVisible( CurStreamingLevel ) )
				{
					bAnyLevelsHidden = TRUE;
					break;
				}
			}
		}

		// If any levels are hidden, alert the user that they will be forcibly made visible if the build continues
		if ( bAnyLevelsHidden )
		{
			DisplayErrorMessage( CLRTools::ToString( LocalizeUnrealEd("BuildSubmitWindow_Error_HiddenLevels") ) );
		}
	}

	static void ColumnHeaderClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		// See if the "Check/Uncheck All" checkbox was toggled by the user by checking if the original source in
		// the event arguments is a checkbox named "CheckAllCheckBox" and that the owner is a ListView.
		if ( Args->OriginalSource->GetType() == CheckBox::typeid && Owner->GetType() == ListView::typeid )
		{
			CheckBox^ ClickedCheckBox = safe_cast<CheckBox^>( Args->OriginalSource );
			ListView^ OwnerListView = safe_cast<ListView^>( Owner );
			check( ClickedCheckBox != nullptr && OwnerListView != nullptr );

			if ( ClickedCheckBox->Name->CompareTo("CheckAllCheckBox") == 0 )
			{
				const UBOOL bChecked = ClickedCheckBox->IsChecked.HasValue && ClickedCheckBox->IsChecked.Value;

				// If the user checked the "Check All" check box, select all list view items that aren't disabled
				if ( bChecked )
				{
					for each( BuildAndSubmitCheckBoxListViewItem^ CurListViewItem in OwnerListView->ItemsSource )
					{
						if ( CurListViewItem->IsEnabled )
						{
							CurListViewItem->IsSelected = TRUE;
						}
					}
				}
				// If the user unchecked the "Check All" check box, deselect everything
				else
				{
					OwnerListView->UnselectAll();
				}
			}
		}

		// See if the header for package name was clicked, and if so, sort the list
		else if ( Args->OriginalSource->GetType() == GridViewColumnHeader::typeid && Owner->GetType() == ListView::typeid )
		{
			ListView^ OwnerListView = safe_cast<ListView^>( Owner );
			GridViewColumnHeader^ ClickedHeader = safe_cast<GridViewColumnHeader^>( Args->OriginalSource );
			check( ClickedHeader != nullptr );

			if ( ClickedHeader->Name->CompareTo("PackageNameHeader") == 0 )
			{
				ICollectionView^ DefaultDataView = CollectionViewSource::GetDefaultView( OwnerListView->ItemsSource );
				ListSortDirection SortDirectionToUse = ListSortDirection::Descending;
				if ( DefaultDataView->SortDescriptions->Count > 0 )
				{
					SortDirectionToUse = ( DefaultDataView->SortDescriptions[0].Direction == ListSortDirection::Ascending ) ? ListSortDirection::Descending : ListSortDirection::Ascending;
				}
				DefaultDataView->SortDescriptions->Clear();
				DefaultDataView->SortDescriptions->Add( SortDescription( "Text", SortDirectionToUse ) );
				DefaultDataView->Refresh();
			}
		}
		Args->Handled = TRUE;
	}

	/** Track whether any levels are hidden in the level browser */
	UBOOL bAnyLevelsHidden;

	/** Checkbox allowing the user to specify if map check errors should prevent SCC submission or not */
	CheckBox^ BuildErrorCheckBox;

	/** Checkbox allowing the user to specify if files not in source control should be auto-added to the depot or not */
	CheckBox^ AddNotInDepotCheckBox;

	/** Checkbox allowing the user to specify if map saving errors should prevent SCC submission or not */
	CheckBox^ SaveErrorCheckBox;

	/** Checkbox allowing the user to include unsourced packages in the main package list */
	CheckBox^ IncludeUnsourcedPackagesCheckBox;

	/** Text box to allow the user to enter a changelist description */
	TextBox^ ChangelistDescriptionTextBox;

	/** Error panel */
	DockPanel^ ErrorPanel;

	/** Text block within the error panel to display messages upon */
	TextBlock^ ErrorPanelTextBlock;

	/** Event listener to send to source control actions */
	MNativePointer<FSourceControlEventListener> SCEventListener;
	
	/** ListView for the packages the user can submit */
	ListView^ SubmitListView;

	/** Collection of items serving as the data source for the list view */
	ObservableCollection<BuildAndSubmitCheckBoxListViewItem^>^ ListViewItemSource;

	/** Collection view for our package list. Used for filtering purposes. */
	ICollectionView^ ListItemCollectionView;
};

#endif // #ifdef __cplusplus_cli

/**
 * Prompt the user with the build all and submit dialog, allowing the user
 * to enter a changelist description to use for a source control submission before
 * kicking off a full build and submitting the map files to source control
 *
 * @param	InEventListener	Event listener to be updated to source control commands
 */
void BuildWindows::PromptForBuildAndSubmit( FSourceControlEventListener* InEventListener)
{
#ifdef __cplusplus_cli

	// Initialize and display the build-all-and-submit frame
	WPFFrameInitStruct^ Settings = gcnew WPFFrameInitStruct;
	Settings->WindowTitle = CLRTools::ToString( LocalizeUnrealEd("BuildSubmitWindow_Title") );

	MWPFFrame^ BuildFrame = gcnew MWPFFrame( GApp->EditorFrame, Settings, TEXT("BuildSubmitWindow") );
	MBuildAndSubmitPanel^ BuildPanel = gcnew MBuildAndSubmitPanel( CLRTools::ToString( TEXT("BuildAndSubmitWindow.xaml") ), InEventListener );
	BuildFrame->SetContentAndShowModal( BuildPanel, 0 );

	delete Settings;
	delete BuildFrame;

#endif // #ifdef __cplusplus_cli
}

#endif // #if HAVE_SCC
