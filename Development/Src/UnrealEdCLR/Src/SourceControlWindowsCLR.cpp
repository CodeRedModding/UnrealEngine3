/*=============================================================================
	PerforceSourceControl.cpp: Perforce specific source control API
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEdCLR.h"
#include "SourceControlWindowsShared.h"

#if HAVE_SCC

//namespace FSourceControlWindows
//{

#ifdef __cplusplus_cli

#include "ManagedCodeSupportCLR.h"
#include "WPFFrameCLR.h"

//Get popup controls
using namespace System::ComponentModel;
using namespace System::Collections::ObjectModel;
using namespace System::Windows::Data;
using namespace System::Windows::Controls::Primitives;

//-------------------------------------
//Source Control Window Constants
//-------------------------------------
namespace SourceControlWindowConstants
{
	enum SubmitResults
	{
		SubmitAccepted,
		SubmitCanceled
	};

	enum ERevertResults
	{
		RevertAccepted,
		RevertCanceled
	};
}

//-----------------------------------------------
//Source Control Check in Helper Struct
//-----------------------------------------------
class FChangeListDescription
{
public:
	TArray<FString> FilesForAdd;
	TArray<FString> FilesForSubmit;
	FString Description;
};

//-----------------------------------------------
// Perforce Login Panel
//-----------------------------------------------
ref class MSourceControlSubmitPanel : public MWPFPanel
{
public:
	MSourceControlSubmitPanel(String^ InXamlName, TArray<FString>& InAddFiles, TArray<FString>& InOpenFiles)
		: MWPFPanel(InXamlName)
	{
		//hook up button events
		OKButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "OKButton" ) );
		OKButton->Click += gcnew RoutedEventHandler( this, &MSourceControlSubmitPanel::OKClicked );

		Button^ CancelButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "CancelButton" ) );
		CancelButton->Click += gcnew RoutedEventHandler( this, &MSourceControlSubmitPanel::CancelClicked );

		ChangeListDescriptionTextCtrl = safe_cast< TextBox^ >( LogicalTreeHelper::FindLogicalNode( this, "ChangeListDescription" ) );
		ChangeListDescriptionTextCtrl->TextChanged += gcnew TextChangedEventHandler( this, &MSourceControlSubmitPanel::OnDescriptionTextChanged );

		WarningPanel = safe_cast< CustomControls::InfoPanel^ >( LogicalTreeHelper::FindLogicalNode( this, "mInvalidDescriptionWarning" ) );
		UpdateValidity();

		AllChecks = gcnew List<CheckBox^>();
		FileListView = safe_cast< ListView^ >( LogicalTreeHelper::FindLogicalNode( this, "FileListView" ) );
		for (INT i = 0; i < InAddFiles.Num(); ++i)
		{
			AddFileToListView (InAddFiles(i));
		}
		for (INT i = 0; i < InOpenFiles.Num(); ++i)
		{
			AddFileToListView (InOpenFiles(i));
		}
	}

	/**
	 * Callback when the parent frame is set to hook up custom events to its widgets
	 */
	virtual void SetParentFrame (MWPFFrame^ InParentFrame) override
	{
		MWPFPanel::SetParentFrame(InParentFrame);
		
		//Make sure to treat the close as a cancel
		Button^ CloseButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode(InParentFrame->GetRootVisual(), "TitleBarCloseButton" ) );
		CloseButton->Click += gcnew RoutedEventHandler( this, &MSourceControlSubmitPanel::CancelClicked );
	}


	/**Gets the requested files and the change list description*/
	void FillChangeListDescription (FChangeListDescription& OutDesc, TArray<FString>& InAddFiles, TArray<FString>& InOpenFiles)
	{
		OutDesc.Description = CLRTools::ToFString(ChangeListDescriptionTextCtrl->Text);
		check(AllChecks->Count == InAddFiles.Num() + InOpenFiles.Num());

		for (INT i = 0; i < InAddFiles.Num(); ++i)
		{
			if ((bool)AllChecks[i]->IsChecked)
			{
				OutDesc.FilesForAdd.AddItem(InAddFiles(i));
			}
		}
		for (INT i = 0; i < InOpenFiles.Num(); ++i)
		{
			if ((bool)AllChecks[i + InAddFiles.Num()]->IsChecked)
			{
				OutDesc.FilesForSubmit.AddItem(InOpenFiles(i));
			}
		}
	}

private:
	/** Called when the settings of the dialog are to be accepted*/
	void OKClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		ParentFrame->Close(SourceControlWindowConstants::SubmitAccepted);
	}

	/** Called when the settings of the dialog are to be ignored*/
	void CancelClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		ParentFrame->Close(SourceControlWindowConstants::SubmitCanceled);
	}

	/**Called when the description field has changed*/
	void OnDescriptionTextChanged( Object^ Owner, TextChangedEventArgs^ Args )
	{
		UpdateValidity();
	}

	/**Called when the description field has changed*/
	void UpdateValidity()
	{
		bool bValidDescription = (ChangeListDescriptionTextCtrl->Text->Length > 0);
		//OK is only available when setting a description
		OKButton->IsEnabled = bValidDescription;
		if (bValidDescription)
		{
			WarningPanel->Hide();
		}
		else
		{
			String^ WarningText = CLRTools::ToString(*LocalizeUnrealEd("SourceControlSubmit_DescriptionValidityWarning"));
			WarningPanel->SetWarningText(WarningText);
			WarningPanel->Show();
		}
	}

	/**Helper function to append a check box and label to the list view*/
	void AddFileToListView (const FString& InFileName)
	{
		DockPanel^ ParentPanel = gcnew DockPanel();
		FileListView->Items->Add(ParentPanel);

		CheckBox^ EnabledCheckBox = gcnew CheckBox();
		//default to being selected
		EnabledCheckBox->IsChecked = true;
		ParentPanel->Children->Add(EnabledCheckBox);

		//label that displays the file name
		TextBlock^ TitleLabel = gcnew TextBlock();
		TitleLabel->Text = CLRTools::ToString(InFileName);
		ParentPanel->Children->Add(TitleLabel);

		//save this ordering for getting the results
		AllChecks->Add(EnabledCheckBox);
	}

private:
	/** Internal widgets to save having to get in multiple places*/
	Button^ OKButton;
	TextBox^ ChangeListDescriptionTextCtrl;
	ListView^ FileListView;

	List<CheckBox^>^ AllChecks;

	CustomControls::InfoPanel^ WarningPanel;
};

/*
			if ( bCheckedSomethingIn )
			{
				// Sychronize source control state.
				FSourceControl::ConvertPackageNamesToSourceControlPaths(PackageNames);
				FSourceControl::CheckIn(NULL, PackageNames);

				LeftContainer->RequestSCCUpdate();
			}
*/

/** 
 * Helper class for the revert dialog. Acts as an item in the dialog's list view to allow the list view items
 * to bind to the values of enabled, selected, etc. to simplify correctly handling selection/enabled state, etc.
 */
ref class RevertCheckBoxListViewItem : public INotifyPropertyChanged
{
public:
	// String that should appear in the list view
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( String^, Text );

	// Whether this item is enabled or not
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( bool, IsEnabled );

	// Whether this item is selected or not
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( bool, IsSelected );

	/**
	 * Constructor
	 *
	 * @param	InText	String that should appear for the item in the list view
	 */
	RevertCheckBoxListViewItem( String^ InText )
	{
		Text = InText;
		IsEnabled = TRUE;
		IsSelected = FALSE;
	}

	/** Overridden version of ToString() */
	virtual String^ ToString() override
	{
		return Text;
	}

	/** INotifyPropertyChanged: Exposed event from INotifyPropertyChanged::PropertyChanged */
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
 * Source control panel for reverting files. Allows the user to select which files should be reverted, as well as
 * provides the option to only allow unmodified files to be reverted.
 */
ref class MSourceControlRevertPanel : public MWPFPanel
{
public:

	/**
	 * Constructor.
	 *
	 * @param	InXamlName		Name of the XAML file defining this panel
	 * @param	InPackageNames	Names of the packages to be potentially reverted
	 */
	MSourceControlRevertPanel(String^ InXamlName, const TArray<FString>& InPackageNames )
		:	MWPFPanel( InXamlName ),
			bAlreadyQueriedModifiedFiles( FALSE )
	{
		// Hook up events for the OK button
		Button^ OKButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "OKButton" ) );
		check( OKButton != nullptr );
		OKButton->Click += gcnew RoutedEventHandler( this, &MSourceControlRevertPanel::OKClicked );

		// Hook up events for the Cancel button
		Button^ CancelButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "CancelButton" ) );
		check( CancelButton != nullptr );
		CancelButton->Click += gcnew RoutedEventHandler( this, &MSourceControlRevertPanel::CancelClicked );

		// Hook up events for the Revert Unchanged check box
		RevertUnchangedCheckBox = safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( this, "RevertUnchangedCheckBox" ) );
		check( RevertUnchangedCheckBox != nullptr );
		RevertUnchangedCheckBox->Checked += gcnew RoutedEventHandler( this, &MSourceControlRevertPanel::RevertUnchangedToggled );
		RevertUnchangedCheckBox->Unchecked += gcnew RoutedEventHandler( this, &MSourceControlRevertPanel::RevertUnchangedToggled );

		// Hook up events for the list view
		RevertListView = safe_cast< ListView^ >( LogicalTreeHelper::FindLogicalNode( this, "RevertListView" ) );
		RevertListView->AddHandler( GridViewColumnHeader::ClickEvent, gcnew RoutedEventHandler( &MSourceControlRevertPanel::ColumnHeaderClicked ) );
		check( RevertListView != nullptr );

		// Store the provided package names in an observable collection and set them as the item source for
		// the list view
		ListViewItemSource = gcnew ObservableCollection<RevertCheckBoxListViewItem^>();
		for ( TArray<FString>::TConstIterator PackageIter( InPackageNames ); PackageIter; ++PackageIter )
		{
			ListViewItemSource->Add( gcnew RevertCheckBoxListViewItem( CLRTools::ToString( *PackageIter ) ) );
		}
		RevertListView->ItemsSource = ListViewItemSource;
	}

	/**
	 * Callback when the parent frame is set to hook up custom events to its widgets
	 *
	 * @param	InParentFrame	Parent frame of this panel
	 */
	virtual void SetParentFrame( MWPFFrame^ InParentFrame ) override
	{
		MWPFPanel::SetParentFrame( InParentFrame );

		// Make sure to treat the close as a cancel
		Button^ CloseButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode(InParentFrame->GetRootVisual(), "TitleBarCloseButton" ) );
		CloseButton->Click += gcnew RoutedEventHandler( this, &MSourceControlRevertPanel::CancelClicked );
	}

	/**
	 * Populates the provided array with the names of the packages the user elected to revert, if any.
	 *
	 * @param	OutPackagesToRevert	Array of package names to revert, as specified by the user in the dialog
	 */
	void GetPackagesToRevert( TArray<FString>& OutPackagesToRevert )
	{
		for each ( RevertCheckBoxListViewItem^ CurSelectedPackage in RevertListView->SelectedItems )
		{
			OutPackagesToRevert.AddItem( CLRTools::ToFString( CurSelectedPackage->Text ) );
		}
	}

private:

	/** Called when the settings of the dialog are to be accepted */
	void OKClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		ParentFrame->Close( SourceControlWindowConstants::RevertAccepted );
	}

	/** Called when the settings of the dialog are to be ignored */
	void CancelClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		ParentFrame->Close( SourceControlWindowConstants::RevertCanceled );
	}

	/** Called when the user checks or unchecks the revert unchanged checkbox; updates the list view accordingly */
	void RevertUnchangedToggled( Object^ Owner, RoutedEventArgs^ Args )
	{
		const UBOOL bChecked = RevertUnchangedCheckBox->IsChecked.HasValue && RevertUnchangedCheckBox->IsChecked.Value;
		
		// If this is the first time the user has checked the "Revert Unchanged" checkbox, query source control to find
		// the packages that are modified from the version on the server. Due to the fact that this is a synchronous and potentially
		// slow operation, we only do it once upfront, and then cache the results so that future toggling is nearly instant.
		if ( bChecked && !bAlreadyQueriedModifiedFiles )
		{
			TArray<FString> PackagesToCheck;
			for each ( RevertCheckBoxListViewItem^ CurItem in ListViewItemSource )
			{
				PackagesToCheck.AddItem( CLRTools::ToFString( CurItem->Text ) );
			}
			FSourceControl::ConvertPackageNamesToSourceControlPaths( PackagesToCheck );

			// Find the files modified from the server version
			TArray<FString> ChangedFiles;
			FSourceControl::GetFilesModifiedFromServer( NULL, PackagesToCheck, ChangedFiles );
			ModifiedPackages = CLRTools::ToStringArray( ChangedFiles );
			
			bAlreadyQueriedModifiedFiles = TRUE;
		}

		// Iterate over each list view item, setting its enabled/selected state appropriately based on whether "Revert Unchanged" is
		// checked or not
		for each( RevertCheckBoxListViewItem^ CurItem in ListViewItemSource )
		{
			UBOOL bItemIsModified = FALSE;
			for ( INT ModifiedIndex = 0; ModifiedIndex < ModifiedPackages->Count; ++ModifiedIndex )
			{
				if ( CurItem->Text->CompareTo( ModifiedPackages[ModifiedIndex] ) == 0 )
				{
					bItemIsModified = TRUE;
					break;
				}
			}

			// Disable the item if the checkbox is checked and the item is modified
			CurItem->IsEnabled = !( bItemIsModified && bChecked );
			CurItem->IsSelected = CurItem->IsEnabled ? CurItem->IsSelected : FALSE;
		}
	}

	/**
	 * Semi-hacky solution to handle the fact that obtaining GridViewColumnHeaders within code is not very straightforward.
	 * Called whenever a column header is clicked, or in the case of the dialog, also when the "Check/Uncheck All" column header
	 * checkbox is called, because its event bubbles to the column header. If the user clicks the column header representing
	 * package names, the list is sorted by package name.
	 */
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
					for each( RevertCheckBoxListViewItem^ CurListViewItem in OwnerListView->ItemsSource )
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

	/** Cache whether the dialog has already queried source control for modified files or not as an optimization */
	UBOOL bAlreadyQueriedModifiedFiles;

	/** CheckBox that allows the user to specify whether only unchanged files should be allowed for reverting or not */
	CheckBox^ RevertUnchangedCheckBox;

	/** ListView for the packages the user can revert */
	ListView^ RevertListView;

	/** Collection of items serving as the data source for the list view */
	ObservableCollection<RevertCheckBoxListViewItem^>^ ListViewItemSource;

	/** List of package names that are modified from the versions stored in source control; Used as an optimization */
	List<String^>^ ModifiedPackages;
};

/**
 * Managed mirror of FSourceControl::FSourceControlFileRevisionInfo. Designed to represent a revision of
 * a file within a WPF list view.
 */
ref class MHistoryRevisionListViewItem : public INotifyPropertyChanged
{
public:
	/** Changelist description */
	property String^ Description;

	/** User name of submitter */
	property String^ UserName;

	/** Clientspec/workspace of submitter */
	property String^ ClientSpec;

	/** File action for this revision (branch, delete, edit, etc.) */
	property String^ Action;

	/** Date of this revision (UTC Time) */
	property DateTime Date;

	/** Number of this revision */
	property int RevisionNumber;

	/** Changelist number */
	property int ChangelistNumber;

	/** Filesize for this revision (0 in the event of a deletion) */
	property int FileSize;

	/** Whether this item is selected in the listview or not */
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( bool, IsSelected );

	/**
	 * Constructor
	 *
	 * @param	File revision info. to populate this managed mirror with
	 */
	MHistoryRevisionListViewItem( const FSourceControl::FSourceControlFileRevisionInfo& InRevisionInfo )
	{
		Description = CLRTools::ToString( InRevisionInfo.Description );
		UserName = CLRTools::ToString( InRevisionInfo.UserName );
		ClientSpec = CLRTools::ToString( InRevisionInfo.ClientSpec );
		Action = CLRTools::ToString( InRevisionInfo.Action );

		// FSourceControlFileRevisionInfo stores the date as number of seconds from the UNIX epoch,
		// so first start with a time representing the UNIX epoch (UTC) and then add the specified number
		// of seconds to it
		DateTime UnixEpochTime = DateTime( 1970, 1, 1, 0, 0, 0, 0, DateTimeKind::Utc );
		Date = UnixEpochTime.AddSeconds( InRevisionInfo.Date ).ToLocalTime();
		RevisionNumber = InRevisionInfo.RevisionNumber;
		ChangelistNumber = InRevisionInfo.ChangelistNumber;
		FileSize = InRevisionInfo.FileSize;
		IsSelected = FALSE;
	}

	/** INotifyPropertyChanged: Exposed event from INotifyPropertyChanged::PropertyChanged */
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
 * Managed mirror of FSourceControlFileHistoryInfo. Designed to represent the history of a file in
 * a listview.
 */
ref class MHistoryFileListViewItem
{
public:

	/** Depot name of the file */
	property String^ FileName;

	/** All revisions of this particular file */
	property ObservableCollection<MHistoryRevisionListViewItem^>^ FileRevisions;

	/**
	 * Constructor
	 *
	 * @param	InHistoryInfo	History file info to populate this managed mirror with
	 */
	MHistoryFileListViewItem( const FSourceControl::FSourceControlFileHistoryInfo& InHistoryInfo )
	{
		FileName = CLRTools::ToString( InHistoryInfo.FileName );
		FileRevisions = gcnew ObservableCollection<MHistoryRevisionListViewItem^>();

		// Add each file revision
		const TArray<FSourceControl::FSourceControlFileRevisionInfo>& Revisions = InHistoryInfo.FileRevisions;
		for ( TArray<FSourceControl::FSourceControlFileRevisionInfo>::TConstIterator RevIter( Revisions ); RevIter; ++RevIter )
		{
			const FSourceControl::FSourceControlFileRevisionInfo& CurRevision = *RevIter;
			FileRevisions->Add( gcnew MHistoryRevisionListViewItem( CurRevision ) );
		}
	}
};

/** Panel designed to display the revision history of a package */
ref class MSourceControlHistoryPanel : public MWPFPanel
{
public:
	/**
	 * Constructor
	 *
	 * @param	InXamlName		Name of the XAML file specifying this panel
	 * @param	InHistoryInfo	History information to display in this panel
	 */
	MSourceControlHistoryPanel( String^ InXamlName, const TArray<FSourceControl::FSourceControlFileHistoryInfo>& InHistoryInfo )
		:	MWPFPanel( InXamlName ),
			LastSelectedRevisionItem( nullptr )
	{
		MainHistoryListView = safe_cast<ListView^>( LogicalTreeHelper::FindLogicalNode( this, "MainHistoryListView" ) );
		check( MainHistoryListView != nullptr );

		// Construct a new observable collection to serve as the items source for the main list view. It will contain each history file item.
		HistoryCollection = gcnew ObservableCollection<MHistoryFileListViewItem^>();
		for ( TArray<FSourceControl::FSourceControlFileHistoryInfo>::TConstIterator HistoryIter( InHistoryInfo ); HistoryIter; ++HistoryIter )
		{
			const FSourceControl::FSourceControlFileHistoryInfo& CurFileHistory = *HistoryIter;
			HistoryCollection->Add( gcnew MHistoryFileListViewItem( CurFileHistory ) );
		}

		// Hook up a new property changed event handler for every revision list view item in the panel. This is a workaround to prevent
		// needing events specified in the XAML. It is used to detect when a new revision item has become selected.
		for each( MHistoryFileListViewItem^ FileItem in HistoryCollection )
		{
			for each( MHistoryRevisionListViewItem^ RevisionItem in FileItem->FileRevisions )
			{
				RevisionItem->PropertyChanged += gcnew PropertyChangedEventHandler( this, &MSourceControlHistoryPanel::OnRevisionPropertyChanged );
			}
		}
		MainHistoryListView->ItemsSource = HistoryCollection;

		AdditionalInfoItemsControl = safe_cast<ItemsControl^>( LogicalTreeHelper::FindLogicalNode( this, "AdditionalInfoItemsControl" ) );
		check( AdditionalInfoItemsControl != nullptr );

		// Construct a new observable collection to serve as the items source for the "additional information" sub-panel. It will contain only
		// the last selected revision item.
		LastSelectedRevisionItem = gcnew ObservableCollection<MHistoryRevisionListViewItem^>();
		AdditionalInfoItemsControl->ItemsSource = LastSelectedRevisionItem;
	}

private:
	/**
	 * Called whenever the IsSelected property on a MHistoryRevisionListViewItem changes. Used to specify the last selected revision item.
	 *
	 * @param	Owner	Object which triggered the event
	 * @param	Args	Event arguments for the property change
	 */
	void OnRevisionPropertyChanged( Object^ Owner, PropertyChangedEventArgs^ Args )
	{
		MHistoryRevisionListViewItem^ OwnerListItem = safe_cast<MHistoryRevisionListViewItem^>( Owner );
		
		// If the object firing the event has become selected, specify it as the "last selected" revision item,
		// deselecting the previously stored one, if any, such that only one revision item can ever be selected
		// at a time.
		if ( OwnerListItem->IsSelected && String::Compare(Args->PropertyName, "IsSelected") == 0 )
		{
			if ( LastSelectedRevisionItem->Count > 0 )
			{
				LastSelectedRevisionItem[0]->IsSelected = FALSE;
			}
			LastSelectedRevisionItem->Clear();
			LastSelectedRevisionItem->Add(OwnerListItem);
		}
	}

	/** Main list view of the panel, displays each file history item */
	ListView^ MainHistoryListView;

	/** Items control for the "additional information" subpanel */
	ItemsControl^ AdditionalInfoItemsControl;

	/** All file history items the panel should display */
	ObservableCollection<MHistoryFileListViewItem^>^ HistoryCollection;

	/** The last selected revision item; Displayed in the "additional information" subpanel */
	ObservableCollection<MHistoryRevisionListViewItem^>^ LastSelectedRevisionItem;
};

#endif //__cplusplus_cli

void FindFilesForCheckIn(const TArray<FString>& InPackagesNames, TArray<FString>& OutAddFiles, TArray<FString>& OutOpenFiles)
{
	for( INT PackageIndex = 0 ; PackageIndex < InPackagesNames.Num() ; ++PackageIndex )
	{
		INT SCCState = GPackageFileCache->GetSourceControlState(*InPackagesNames(PackageIndex));
		if( SCCState == SCC_CheckedOut )
		{
			// Only call "CheckIn" once since the user can submit as many files as they want to in a single check-in dialog.
			//GApp->SCC->CheckIn( Package );
			OutOpenFiles.AddItem(InPackagesNames(PackageIndex));
		}
		else
		{
			if( SCCState == SCC_NotInDepot )
			{
				// Only call "CheckIn" once since the user can submit as many files as they want to in a single check-in dialog.
				//GApp->SCC->CheckIn( Package );
				OutAddFiles.AddItem(InPackagesNames(PackageIndex));
			}
		}
	}
}


UBOOL SourceControlWindows::PromptForCheckin(FSourceControlEventListener* InEventListener, const TArray<FString>& InPackageNames)
{
	UBOOL bCheckInSuccess = FALSE;

#ifdef __cplusplus_cli
	TArray<FString> AddFiles;
	TArray<FString> OpenFiles;
	FindFilesForCheckIn(InPackageNames, OUT AddFiles, OUT OpenFiles);

	if (AddFiles.Num() || OpenFiles.Num())
	{
		WPFFrameInitStruct^ Settings = gcnew WPFFrameInitStruct;
		Settings->WindowTitle = CLRTools::ToString(*LocalizeUnrealEd("SourceControlSubmitTitle"));

		SourceControlWindowConstants::SubmitResults Result= SourceControlWindowConstants::SubmitCanceled;
		MWPFFrame^ SubmitFrame = gcnew MWPFFrame(GApp->EditorFrame, Settings, TEXT("SourceControlSubmit"));

		//No need to append, as the constructor does that automatically
		MSourceControlSubmitPanel^ SubmitPanel = gcnew MSourceControlSubmitPanel(CLRTools::ToString(TEXT("SourceControlSubmitWindow.xaml")), AddFiles, OpenFiles);

		//make the window modal until they either accept or cancel
		Result = (SourceControlWindowConstants::SubmitResults)SubmitFrame->SetContentAndShowModal(SubmitPanel, SourceControlWindowConstants::SubmitCanceled);
		if (Result == SourceControlWindowConstants::SubmitAccepted)
		{
			//try the actual submission
			FChangeListDescription Description;

			//Get description from the dialog
			SubmitPanel->FillChangeListDescription(Description, AddFiles, OpenFiles);

			FSourceControl::ConvertPackageNamesToSourceControlPaths(Description.FilesForAdd);
			FSourceControl::ConvertPackageNamesToSourceControlPaths(Description.FilesForSubmit);

			//revert all unchanged files that were submitted
			FSourceControl::RevertUnchanged(InEventListener, Description.FilesForSubmit);

			//make sure all files are still checked out
			for (INT VerifyIndex = Description.FilesForSubmit.Num()-1; VerifyIndex >= 0; --VerifyIndex)
			{
				FFilename TempFilename = Description.FilesForSubmit(VerifyIndex);
				FString SimpleFileName = TempFilename.GetBaseFilename();
				INT SCCState = GPackageFileCache->GetSourceControlState(*SimpleFileName);
				if( SCCState != SCC_CheckedOut )
				{
					Description.FilesForSubmit.Remove(VerifyIndex);
				}
			}

			bCheckInSuccess = FSourceControl::CheckIn(InEventListener, Description.FilesForAdd, Description.FilesForSubmit, Description.Description);

			//LoginPanel->GetResult(InOutServerName, InOutUserName, InOutClientSpecName);
		}

		//de-allocate the frame
		delete Settings;
		delete SubmitFrame;
	}


#endif //__cplusplus_cli
	return bCheckInSuccess;
}


/**
 * Display file revision history for the provided packages
 *
 * @param	InPackageNames	Names of packages to display file revision history for
 */
void SourceControlWindows::DisplayRevisionHistory( const TArray<FString>& InPackageNames )
{
#ifdef __cplusplus_cli
	TArray<FString> SourceControlPaths( InPackageNames );
	FSourceControl::ConvertPackageNamesToSourceControlPaths( SourceControlPaths );

	// Query for the file history for the provided packages
	TArray<FSourceControl::FSourceControlFileHistoryInfo> HistoryInfo;
	FSourceControl::GetFileHistory( SourceControlPaths, HistoryInfo );

	WPFFrameInitStruct^ Settings = gcnew WPFFrameInitStruct;
	Settings->WindowTitle = CLRTools::ToString( *LocalizeUnrealEd("SourceControlHistoryTitle") );

	// Display the file history using the history panel
	MWPFFrame^ HistoryFrame = gcnew MWPFFrame( GApp->EditorFrame, Settings, TEXT("SourceControlHistory") );
	MSourceControlHistoryPanel^ HistoryPanel = gcnew MSourceControlHistoryPanel( CLRTools::ToString( TEXT("SourceControlHistoryWindow.xaml") ), HistoryInfo );
	HistoryFrame->SetContentAndShow( HistoryPanel );
#endif // #ifdef __cplusplus_cli
}

//};	//namespace FSourceControlWindows


/**
 * Prompt the user with a revert files dialog, allowing them to specify which packages, if any, should be reverted.
 *
 * @param	InEventListener	Object which should receive the source control callback
 * @param	InPackageNames	Names of the packages to consider for reverting
 *
 * @param	TRUE if the files were reverted; FALSE if the user canceled out of the dialog
 */
UBOOL SourceControlWindows::PromptForRevert( FSourceControlEventListener* InEventListener, const TArray<FString>& InPackageNames )
{
	UBOOL bReverted = FALSE;

#ifdef __cplusplus_cli

	// Only add packages that are actually already checked out to the prompt
	TArray<FString> CheckedOutPackages;
	for ( TArray<FString>::TConstIterator PackageIter( InPackageNames ); PackageIter; ++PackageIter )
	{
		const INT CurPackageSCCState = GPackageFileCache->GetSourceControlState( **PackageIter );
		if( CurPackageSCCState == SCC_CheckedOut )
		{
			CheckedOutPackages.AddItem( *PackageIter );
		}
	}

	// If any of the packages are checked out, provide the revert prompt
	if ( CheckedOutPackages.Num() > 0 )
	{
		// Set up the wpf frame/panel for the revert dialog
		WPFFrameInitStruct^ Settings = gcnew WPFFrameInitStruct;
		Settings->WindowTitle = CLRTools::ToString(*LocalizeUnrealEd("SourceControlRevertTitle"));

		SourceControlWindowConstants::ERevertResults Result = SourceControlWindowConstants::RevertCanceled;
		MWPFFrame^ RevertFrame = gcnew MWPFFrame( GApp->EditorFrame, Settings, TEXT("SourceControlRevert") );

		MSourceControlRevertPanel^ RevertPanel = gcnew MSourceControlRevertPanel( CLRTools::ToString( TEXT("SourceControlRevertWindow.xaml") ), CheckedOutPackages );

		Result = static_cast<SourceControlWindowConstants::ERevertResults>( RevertFrame->SetContentAndShowModal( RevertPanel, SourceControlWindowConstants::RevertCanceled ) );
		
		// If the user decided to revert some packages, go ahead and do revert the ones they selected
		if ( Result == SourceControlWindowConstants::RevertAccepted )
		{
			TArray<FString> PackagesToRevert;
			RevertPanel->GetPackagesToRevert( PackagesToRevert );
			check( PackagesToRevert.Num() > 0 );

			FSourceControl::ConvertPackageNamesToSourceControlPaths( PackagesToRevert );
			FSourceControl::Revert( InEventListener, PackagesToRevert );

			bReverted = TRUE;
		}

		//de-allocate the frame
		delete Settings;
		delete RevertFrame;
	}

#endif //__cplusplus_cli

	return bReverted;
}

#endif // HAVE_SCC
