/*=============================================================================
	ExportLocalizationWindowCLR.cpp: Dialog prompting user for about basic export
	localization options.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEdCLR.h"
#include "ExportLocalizationWindowShared.h"

#ifdef __cplusplus_cli

#include "ManagedCodeSupportCLR.h"
#include "WPFFrameCLR.h"
#include "GameAssetDatabaseShared.h"

using namespace System::ComponentModel;
using namespace System::Collections::ObjectModel;
using namespace System::IO;

enum EExportLocalizationResults
{
	ELR_Accepted,
	ELR_Canceled
};

/** Helper class to act as list view items for the localization export panel */
ref class ExportFilterCheckBoxListViewItem : public INotifyPropertyChanged
{
public:
	// String that should appear in the list view
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( String^, Text );

	// Whether this item is selected or not
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( bool, Selected );

	/**
	 * Constructor
	 *
	 * @param	InText	String that should appear for the item in the list view
	 */
	ExportFilterCheckBoxListViewItem( String^ InText )
	{
		Text = InText;
		Selected = FALSE;
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

/** Class to display basic localization export options */
ref class MExportLocalizationPanel : public MWPFPanel
{
public:

	MExportLocalizationPanel( String^ InXamlName, ExportLocalizationWindow::FExportLocalizationOptions& OutOptions )
		:	MWPFPanel( InXamlName ),
			OptionsToPopulate( OutOptions )
	{
		// Hook up events for the OK button
		OKButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "OKButton" ) );
		check( OKButton != nullptr );
		OKButton->Click += gcnew RoutedEventHandler( this, &MExportLocalizationPanel::OKClicked );
		OKButton->IsEnabled = FALSE;

		// Hook up events for the Cancel button
		Button^ CancelButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "CancelButton" ) );
		check( CancelButton != nullptr );
		CancelButton->Click += gcnew RoutedEventHandler( this, &MExportLocalizationPanel::CancelClicked );

		Button^ BrowseButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "BrowseButton") );
		check( BrowseButton != nullptr );
		BrowseButton->Click += gcnew RoutedEventHandler( this, &MExportLocalizationPanel::BrowseClicked );

		ExportBinariesCheckBox = safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( this, "ExportBinariesCheckBox") );
		check( ExportBinariesCheckBox != nullptr );

		CompareDiffsCheckBox = safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( this, "CompareDiffsCheckBox") );
		check( CompareDiffsCheckBox != nullptr );

		ExportPathTextBox = safe_cast< TextBox^ >( LogicalTreeHelper::FindLogicalNode( this, "ExportPathTextBox") );
		check( ExportPathTextBox != nullptr );
		ExportPathTextBox->TextChanged += gcnew TextChangedEventHandler( this, &MExportLocalizationPanel::ExportPathChanged );

		FilterTagsListView = safe_cast< ListView^ >( LogicalTreeHelper::FindLogicalNode( this, "FilterTagsListView" ) );
		check( FilterTagsListView != nullptr );

		// Find and hook up events to the "check all" checkbox by digging into the gridview column headers
		//GridView^ FilterTagsGridView = safe_cast< GridView^ >( FilterTagsListView->View );
		//check( FilterTagsGridView != nullptr && FilterTagsGridView->Columns->Count > 0 );

		//GridViewColumnHeader^ CheckboxColumnHeader = safe_cast< GridViewColumnHeader^ >( FilterTagsGridView->Columns[0]->Header );
		//check( CheckboxColumnHeader != nullptr );

		//CheckAllCheckBox = safe_cast< CheckBox^ >( CheckboxColumnHeader->Content );
		//check( CheckAllCheckBox != nullptr );
		//CheckAllCheckBox->Click += gcnew RoutedEventHandler( this, &MExportLocalizationPanel::CheckAllClicked );

		NoFilteringRadioButton = safe_cast< RadioButton^ >( LogicalTreeHelper::FindLogicalNode( this, "NoFilteringRadioButton") );
		check( NoFilteringRadioButton != nullptr );
		NoFilteringRadioButton->IsChecked = TRUE;

		MatchAnyRadioButton = safe_cast< RadioButton^ >( LogicalTreeHelper::FindLogicalNode( this, "MatchAnyRadioButton") );
		check( MatchAnyRadioButton != nullptr );

		MatchAllRadioButton = safe_cast< RadioButton^ >( LogicalTreeHelper::FindLogicalNode( this, "MatchAllRadioButton") );
		check( MatchAllRadioButton != nullptr );

		ListViewItemSource = gcnew ObservableCollection<ExportFilterCheckBoxListViewItem^>();
		InitializeFilter();
		FilterTagsListView->ItemsSource = ListViewItemSource;

		PromptForDirectory();
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
		Button^ CloseButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( InParentFrame->GetRootVisual(), "TitleBarCloseButton" ) );
		CloseButton->Click += gcnew RoutedEventHandler( this, &MExportLocalizationPanel::CancelClicked );
	}

private:

	/** Called when the settings of the dialog are to be accepted */
	void OKClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		PopulateOptions();
		ParentFrame->Close( ELR_Accepted );
	}

	/** Called when the settings of the dialog are to be ignored */
	void CancelClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		ParentFrame->Close( ELR_Canceled );
	}

	/** Called when the user clicks the browse button */
	void BrowseClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		PromptForDirectory();
	}

	/** Called whenever the export path is changed by the user. Validates the directory. */
	void ExportPathChanged( Object^ Owner, TextChangedEventArgs^ Args )
	{
		OKButton->IsEnabled = Directory::Exists( ExportPathTextBox->Text );
	}

	/** Helper method to prompt the user for a directory */
	void PromptForDirectory()
	{
		wxDirDialog ChooseDirDialog(
			GApp->EditorFrame,
			*LocalizeUnrealEd("ChooseADirectory"),
			*CLRTools::ToFString( ExportPathTextBox->Text )
			);
		
		if ( ChooseDirDialog.ShowModal() == wxID_OK )
		{
			ExportPathTextBox->Text = CLRTools::ToString( FString( ChooseDirDialog.GetPath() ) );
		}
	}

	//void CheckAllClicked( Object^ Owner, RoutedEventArgs^ Args )
	//{
	//	if ( CheckAllCheckBox->IsChecked.HasValue && CheckAllCheckBox->IsChecked.Value )
	//	{
	//		FilterTagsListView->SelectAll();	
	//	}
	//	else
	//	{
	//		FilterTagsListView->UnselectAll();
	//	}
	//}

	/** Helper method to initialize the tag filter */
	void InitializeFilter()
	{
		if ( FGameAssetDatabase::IsInitialized() )
		{
			const FGameAssetDatabase& GAD = FGameAssetDatabase::Get();

			// Query the game asset database for all user and collection tags in order
			// to display them in the filter list view
			List<String^>^ UserTags = nullptr;
			GAD.QueryAllTags( UserTags, ETagQueryOptions::UserTagsOnly );

			List<String^>^ CollectionTags = nullptr;
			GAD.QueryAllTags( CollectionTags, ETagQueryOptions::CollectionsOnly );

			List<String^>^ TagsToDisplay = gcnew List<String^>( UserTags );
			for each( String^ CollectionTag in CollectionTags )
			{
				// If the collection is a private collection, make sure the current user is allowed to use that collection
				if ( !GAD.IsCollectionTag( CollectionTag, EGADCollection::Private ) || GAD.IsMyPrivateCollection( CollectionTag ) )
				{
					TagsToDisplay->Add( CollectionTag );
				}
			}

			// Sort the tags alphabetically and add them to the list view
			TagsToDisplay->Sort();

			for each( String^ Tag in TagsToDisplay )
			{
				ListViewItemSource->Add( gcnew ExportFilterCheckBoxListViewItem( Tag ) );
			}
		}
	}

	/** Helper method to populate the referenced export options (todo: use binding to make this unnecessary) */
	void PopulateOptions()
	{
		OptionsToPopulate.bExportBinaries = ExportBinariesCheckBox->IsChecked.HasValue ? ExportBinariesCheckBox->IsChecked.Value : FALSE;
		OptionsToPopulate.bCompareAgainstDefaults = CompareDiffsCheckBox->IsChecked.HasValue ? CompareDiffsCheckBox->IsChecked.Value : FALSE;
		OptionsToPopulate.ExportPath = CLRTools::ToFString( ExportPathTextBox->Text );
		
		FLocalizationExportFilter::ETagFilterType SelectedFilterType = FLocalizationExportFilter::TFT_None;
		if ( MatchAllRadioButton->IsChecked.HasValue && MatchAllRadioButton->IsChecked.Value )
		{
			SelectedFilterType = FLocalizationExportFilter::TFT_MatchAll;
		}
		else if ( MatchAnyRadioButton->IsChecked.HasValue && MatchAnyRadioButton->IsChecked.Value )
		{
			SelectedFilterType = FLocalizationExportFilter::TFT_MatchAny;
		}
		OptionsToPopulate.Filter.SetTagFilterType( SelectedFilterType );

		TArray<FString>& FilterTags = OptionsToPopulate.Filter.GetFilterTags();
		for each( ExportFilterCheckBoxListViewItem^ CurItem in ListViewItemSource )
		{
			if ( CurItem->Selected )
			{
				FilterTags.AddItem( CLRTools::ToFString( CurItem->Text ) );
			}
		}
	}

	/** Button the user clicks to indicate they'd like to export */
	Button^ OKButton;

	//CheckBox^ CheckAllCheckBox;

	/** Checkbox allowing the user to export binaries along with the localization data */
	CheckBox^ ExportBinariesCheckBox;

	/** Checkbox allowing the user to specify whether to check properties vs. their defaults when chosing to export them or not */
	CheckBox^ CompareDiffsCheckBox;

	/** Textbox housing the export path */
	TextBox^ ExportPathTextBox;

	/** Radiuo button specifying no filtering */
	RadioButton^ NoFilteringRadioButton;

	/** Radio button specifying the "match any" filter type */
	RadioButton^ MatchAnyRadioButton;

	/** Radio button specifying the "match all" filter type */
	RadioButton^ MatchAllRadioButton;

	/** List view of the filter tags */
	ListView^ FilterTagsListView;

	/** Reference to the options being edited */
	ExportLocalizationWindow::FExportLocalizationOptions& OptionsToPopulate;

	/** Collection of items serving as the data source for the list view */
	ObservableCollection<ExportFilterCheckBoxListViewItem^>^ ListViewItemSource;
};

#endif // #ifdef __cplusplus_cli

/**
 * Helper method to prompt the user for localization export options
 *
 * @param	OutOptions	Options specified by user via the dialog
 *
 * @return	TRUE if the user closed the dialog by specifying to export; FALSE if the user canceled out of the dialog
 */
UBOOL ExportLocalizationWindow::PromptForExportLocalizationOptions( ExportLocalizationWindow::FExportLocalizationOptions& OutOptions )
{
	UBOOL bSuccessful = FALSE;
#ifdef __cplusplus_cli
	WPFFrameInitStruct^ Settings = gcnew WPFFrameInitStruct;
	Settings->WindowTitle = CLRTools::ToString( *LocalizeUnrealEd("ExportLocalizationTitle") );

	// Prompt the user about export options
	MWPFFrame^ ExportLocalizationFrame = gcnew MWPFFrame( GApp->EditorFrame, Settings, TEXT("ExportLocalizationWindow") );
	MExportLocalizationPanel^ ExportLocalizationPanel = gcnew MExportLocalizationPanel( CLRTools::ToString( TEXT("ExportLocalizationWindow.xaml") ), OutOptions );
	const EExportLocalizationResults Result = static_cast<EExportLocalizationResults>( ExportLocalizationFrame->SetContentAndShowModal( ExportLocalizationPanel, ELR_Canceled ) );

	bSuccessful = ( Result != ELR_Canceled );
#endif // #ifdef __cplusplus_cli
	return bSuccessful;
}
