/*=============================================================================
	UnitTestWindowCLR.cpp: Helper window to aid in running editor-side unit tests
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEdCLR.h"
#include "UnitTestWindowShared.h"

#if USE_UNIT_TESTS

#ifdef __cplusplus_cli

#include "ManagedCodeSupportCLR.h"
#include "WPFFrameCLR.h"
#include "BusyCursor.h"

using namespace System::ComponentModel;
using namespace System::Collections::ObjectModel;

/** Enumeration of unit test status for special dialog */
enum class EUnitTestStatus
{
	NotScheduled,	// Unit test was not scheduled to run by the user
	Fail,			// Unit test was run and failed
	Success,		// Unit test was run and succeeded
};

/** Helper class to serve as a list view item for the unit test dialog */
ref class UnitTestListViewItem : public INotifyPropertyChanged
{
public:
	
	/** Whether the item is selected or not */
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( bool, IsSelected );

	/** The status of the unit test */
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( EUnitTestStatus^, Status );

	/** Simplified output from the text */
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( String^, OutputText );

	/** Name of the unit test represented by the list view item */
	property String^ Name;

	/** INotifyPropertyChanged: Exposed event from INotifyPropertyChanged::PropertyChanged */
	virtual event PropertyChangedEventHandler^ PropertyChanged;

	/**
	 * Constructor
	 *
	 * @param	InTestName	Name of the unit test the list view item will represent
	 */
	UnitTestListViewItem( String^ InTestName )
	{
		IsSelected = false;
		Status = EUnitTestStatus::NotScheduled;
		Name = InTestName;
		OutputText = String::Empty;
	}

	/** Reset the state of the list view item */
	void Reset()
	{
		Status = EUnitTestStatus::NotScheduled;
		OutputText = String::Empty;
		IsSelected = false;
	}

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

/** Main panel for the unit test dialog */
ref class MUnitTestPanel : public MWPFPanel
{
public:

	/**
	 * Constructor
	 *
	 * @param	InXamlName	Name of the XAML file specifying the layout of the panel
	 */
	MUnitTestPanel( String^ InXamlName )
		: MWPFPanel( InXamlName )
	{
		// Hook up events to the various controls on the panel
		Button^ OKButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "OKButton" ) );
		check( OKButton != nullptr );
		OKButton->Click += gcnew RoutedEventHandler( this, &MUnitTestPanel::OKClicked );

		Button^ CancelButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "CancelButton" ) );
		check( CancelButton != nullptr );
		CancelButton->Click += gcnew RoutedEventHandler( this, &MUnitTestPanel::CancelClicked );

		ExportButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "ExportButton" ) );
		check( ExportButton != nullptr );
		ExportButton->Click += gcnew RoutedEventHandler( this, &MUnitTestPanel::ExportClicked );
		ExportButton->IsEnabled = false;

		DetailedOutputExpander = safe_cast< Expander^ >( LogicalTreeHelper::FindLogicalNode( this, "DetailedOutputExpander" ) );
		check( DetailedOutputExpander != nullptr );

		DetailedOutputViewer = safe_cast< FlowDocumentScrollViewer^ >( LogicalTreeHelper::FindLogicalNode( this, "DetailedOutputViewer") );
		check( DetailedOutputViewer != nullptr );

		// Construct a new flow document to serve as the holder for unit test output
		OutputDocument = gcnew System::Windows::Documents::FlowDocument();
		OutputDocument->FontFamily = gcnew System::Windows::Media::FontFamily( TEXT("Calibri, Tahoma, Verdana") );
		DetailedOutputViewer->Document = OutputDocument;

		ListView^ UnitTestListView = safe_cast< ListView^ >( LogicalTreeHelper::FindLogicalNode( this, "UnitTestListView" ) );
		check( UnitTestListView != nullptr );

		// Find and hook up events to the "check all" checkbox by digging into the gridview column headers
		GridView^ UnitTestGridView = safe_cast< GridView^ >( UnitTestListView->View );
		check( UnitTestGridView != nullptr && UnitTestGridView->Columns->Count > 0 );
		
		GridViewColumnHeader^ CheckboxColumnHeader = safe_cast< GridViewColumnHeader^ >( UnitTestGridView->Columns[0]->Header );
		check( CheckboxColumnHeader != nullptr );

		CheckAllCheckBox = safe_cast< CheckBox^ >( CheckboxColumnHeader->Content );
		check( CheckAllCheckBox != nullptr );
		CheckAllCheckBox->Click += gcnew RoutedEventHandler( this, &MUnitTestPanel::CheckAllClicked );

		ListViewItemSource = gcnew ObservableCollection<UnitTestListViewItem^>();

		// Add each valid editor test to the dialog
		TArray<FString> ValidTests;
		FUnitTestFramework::GetInstance().GetValidTestNames( ValidTests );
		List<String^>^ ValidTestsList = CLRTools::ToStringArray( ValidTests );

		for each ( String^ CurTest in ValidTestsList )
		{
			ListViewItemSource->Add( gcnew UnitTestListViewItem( CurTest ) );
		}

		UnitTestListView->ItemsSource = ListViewItemSource;
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
		CloseButton->Click += gcnew RoutedEventHandler( this, &MUnitTestPanel::CancelClicked );
	}

private:

	/** Called when the OK button is clicked; Runs the tests selected by the user */
	void OKClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		// Run each test selected by the user in the dialog
		List<UnitTestListViewItem^>^ TestsToRun = gcnew List<UnitTestListViewItem^>();
		for each ( UnitTestListViewItem^ CurItem in ListViewItemSource )
		{
			if ( CurItem->IsSelected )
			{
				TestsToRun->Add( CurItem );
			}
			else
			{
				CurItem->Reset();
			}
		}

		RunTests( TestsToRun );
	}

	/** Called when the cancel button is clicked; Closes the dialog */
	void CancelClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		ParentFrame->Close( 0 );
	}

	/** Called when the "Export Output" button is clicked; Gives the user the chance to export the unit test output to a file */
	void ExportClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		using System::Windows::Documents::TextRange;

		// Set up a save dialog supporting txt or rtf formats
		Microsoft::Win32::SaveFileDialog^ SaveDialog = gcnew Microsoft::Win32::SaveFileDialog();
		SaveDialog->Filter = "Text Document|*.txt|Rich Text Format (RTF)|*.rtf";
		SaveDialog->FileName = "UnitTestExport";

		// Prompt the user to save
		Nullable<bool> DialogResult = SaveDialog->ShowDialog();
		if ( DialogResult.HasValue && DialogResult.Value )
		{
			String^ FileNameToUse = SaveDialog->FileName;
			check ( !String::IsNullOrEmpty( FileNameToUse ) );

			// Use a textrange to extract the content from the dialog's flow document
			TextRange^ OutputTextRange = gcnew TextRange( OutputDocument->ContentStart, OutputDocument->ContentEnd );
			System::IO::FileStream^ OutputFileStream = gcnew System::IO::FileStream( FileNameToUse, System::IO::FileMode::Create );
			check( OutputFileStream != nullptr );

			// Save the textrange to the user provided file name
			String^ DataFormat = FileNameToUse->EndsWith( TEXT("rtf") ) ? DataFormats::Rtf : DataFormats::Text;
			if ( OutputTextRange->CanSave( DataFormat ) )
			{
				OutputTextRange->Save( OutputFileStream, DataFormat );
			}
			OutputFileStream->Close();
		}
	}

	/** Called when the "check all" checkbox is toggled; Selects/deselects all unit tests */
	void CheckAllClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		if ( CheckAllCheckBox->IsChecked.HasValue )
		{
			for each ( UnitTestListViewItem^ CurItem in ListViewItemSource )
			{
				CurItem->IsSelected = CheckAllCheckBox->IsChecked.Value;
			}
		}
	}

	/**
	 * Helper method to add formatted text for a particular unit test execution info sub-item (errors, warnings, or log items) to a flow document section
	 *
	 * @param	InSection		Flow document section to be added to
	 * @param	InHeaderString	Header for newly formatted text to be added
	 * @param	InBrush			Brush to use for coloring the formatted header (such as red for error, yellow for warning, etc.)
	 * @param	InSubInfo		Array of sub-info to add to the provided section (list of errors, warnings, or log items)
	 */
	void AddExecutionSubInfoToSection( System::Windows::Documents::Section^ InSection, const FString& InHeaderString, System::Windows::Media::Brush^ InBrush, const TArray<FString>& InSubInfo )
	{
		using System::Windows::Documents::Run;
		using System::Windows::Documents::Bold;
		using System::Windows::Documents::Paragraph;
		using System::Windows::Documents::List;
		using System::Windows::Documents::ListItem;

		if ( InSubInfo.Num() > 0 )
		{
			// Create a header item with a color specified by the provided brush
			Run^ HeaderText = gcnew Run( CLRTools::ToString( InHeaderString ) );
			HeaderText->Foreground = InBrush;

			// Add the header item, in bold, to the provided section
			InSection->Blocks->Add( gcnew Paragraph( gcnew Bold( HeaderText ) ) );

			// Construct a list to store the sub-info items
			List^ SubInfoList = gcnew List();
			SubInfoList->MarkerStyle = TextMarkerStyle::Disc;

			// Add each sub-info item to the list
			for ( TArray<FString>::TConstIterator SubInfoIter( InSubInfo ); SubInfoIter; ++SubInfoIter )
			{
				SubInfoList->ListItems->Add( gcnew ListItem( gcnew Paragraph( gcnew Run( CLRTools::ToString( *SubInfoIter ) ) ) ) );
			}

			// Add the list to the provided section
			InSection->Blocks->Add( SubInfoList );
		}
	}

	/**
	 * Helper method to add the output of a unit test to the detailed output panel
	 *
	 * @param	InTestName	Name of the test whose output is being added
	 * @param	bSuccessful	Whether the test being added successfully executed or not
	 * @param	InInfo		Execution info for the test, including any errors, warnings, etc. generated during execution
	 */
	void AddUnitTestOutputToPanel( const FString& InTestName, UBOOL bSuccessful, const FUnitTestExecutionInfo& InInfo )
	{
		using System::Windows::Documents::Bold;
		using System::Windows::Documents::Underline;
		using System::Windows::Documents::Run;
		using System::Windows::Documents::Paragraph;
		using System::Windows::Documents::Section;
		using System::Windows::Documents::List;
		using System::Windows::Documents::ListItem;

		// Create a new flow document section for this unit test
		Section^ TestSection = gcnew Section();
		TestSection->Foreground = System::Windows::Media::Brushes::White;

		// Construct the header string for the unit test based on whether it succeeded or failed
		String^ HeaderString;
		if ( bSuccessful )
		{
			HeaderString = CLRTools::ToString( FString::Printf( TEXT("%s: %s"), *InTestName, *LocalizeUnrealEd("UnitTest_Success") ) );
		}
		else
		{
			HeaderString = CLRTools::ToString( FString::Printf( TEXT("%s: %s"), *InTestName, *LocalizeUnrealEd("UnitTest_Fail") ) );
		}

		TestSection->Blocks->Add( gcnew Paragraph( gcnew Bold( gcnew Underline( gcnew Run( HeaderString ) ) ) ) );
		
		// Add the test's errors to the panel
		AddExecutionSubInfoToSection( TestSection, LocalizeUnrealEd("UnitTest_Errors"), System::Windows::Media::Brushes::Red, InInfo.Errors );

		// Add the test's warnings to the panel
		AddExecutionSubInfoToSection( TestSection, LocalizeUnrealEd("UnitTest_Warnings"), System::Windows::Media::Brushes::Gold, InInfo.Warnings );

		// Add the test's log items to the panel
		AddExecutionSubInfoToSection( TestSection, LocalizeUnrealEd("UnitTest_LogItems"), System::Windows::Media::Brushes::LightSteelBlue, InInfo.LogItems );

		OutputDocument->Blocks->Add( TestSection );
	}

	/**
	 * Run all of the unit tests specified by the parameter
	 *
	 * @param	InTestsToRun	List of unit tests to run
	 */
	void RunTests( List<UnitTestListViewItem^>^ InTestsToRun )
	{
		// Display a busy cursor as the tests may run long and we can't use the standard
		// GWarn progress bar (the unit tests hijack GWarn)
		FScopedBusyCursor Cursor;

		// Clear any pre-existing output and enable the ability to export output
		ExportButton->IsEnabled = true;
		OutputDocument->Blocks->Clear();

		for each ( UnitTestListViewItem^ CurItem in InTestsToRun )
		{
			FString TestName = CLRTools::ToFString( CurItem->Name );
			
			// Run each test
			FUnitTestExecutionInfo ExecutionInfo;
			const UBOOL bSuccessful = FUnitTestFramework::GetInstance().RunTestByName( TestName, ExecutionInfo );
			CurItem->Status =  bSuccessful ? EUnitTestStatus::Success : EUnitTestStatus::Fail;
			
			// Display a simplified output next to the unit test
			FString SimpleOutputText = FString::Printf( LocalizeSecure( LocalizeUnrealEd("UnitTest_SimpleOutput"), ExecutionInfo.Errors.Num(), ExecutionInfo.Warnings.Num(), ExecutionInfo.LogItems.Num() ) );
			CurItem->OutputText = CLRTools::ToString( SimpleOutputText );

			// Display detailed output in the special panel made for that purpose
			AddUnitTestOutputToPanel( TestName, bSuccessful, ExecutionInfo );
			::wxSafeYield();
		}

		// Make sure the detailed output panel is visible
		DetailedOutputExpander->IsExpanded = true;
	}

	/** Expander housing the detailed output panel */
	Expander^ DetailedOutputExpander;

	/** Button used to export output */
	Button^ ExportButton;

	/** Checkbox to check/uncheck all tests in the dialog */
	CheckBox^ CheckAllCheckBox;

	/** Viewer for the detailed output flow document */
	FlowDocumentScrollViewer^ DetailedOutputViewer;

	/** Flow document to store detailed output, allows for rich text, etc. */
	System::Windows::Documents::FlowDocument^ OutputDocument;

	/** Collection of items serving as the data source for the list view */
	ObservableCollection<UnitTestListViewItem^>^ ListViewItemSource;
};

#endif //__cplusplus_cli

/** Display the WPF dialog that allows unit tests to be run */
void UnitTestWindow::DisplayUnitTestWindow()
{
#ifdef __cplusplus_cli
	WPFFrameInitStruct^ Settings = gcnew WPFFrameInitStruct;
	Settings->WindowTitle = CLRTools::ToString( *LocalizeUnrealEd("UnitTest_WindowTitle") );
	MWPFFrame^ UnitTestFrame = gcnew MWPFFrame( GApp->EditorFrame, Settings, TEXT("UnitTestWindow") );
	MUnitTestPanel^ UnitTestPanel = gcnew MUnitTestPanel( CLRTools::ToString( TEXT("UnitTestWindow.xaml") ) );
	UnitTestFrame->SetContentAndShow( UnitTestPanel );
#endif // #ifdef __cplusplus_cli
}

#endif // #if USE_UNIT_TESTS
