/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEdCLR.h"
#include "ManagedCodeSupportCLR.h"
#include "WPFFrameCLR.h"
#include "ConsolidateWindowShared.h"
#include "ContentBrowserShared.h"
#include "FileHelpers.h"

using namespace System::Windows::Input;

//////////////////////////////////////////////////////////////////////////
// MConsolidateObjectsPanel												//
//////////////////////////////////////////////////////////////////////////

/**
 * Custom panel that allows the user to select which object out of several should become the
 * "object to consolidate to." All other assets then become consolidated to that object.
 */
ref class MConsolidateObjectsPanel : public MWPFPanel
{
public:

	/**
	 * Constructor; Construct an MConsolidateObjectsPanel
	 *
	 * @param	InXamlName	XAML file that specifies the panel
	 */
	MConsolidateObjectsPanel( String^ InXamlName );

	/**
	 * Attempt to add the provided objects to the consolidation panel; Only adds objects which are compatible with objects already existing within the panel, if any
	 *
	 * @param	InObjects			Objects to attempt to add to the panel
	 * @param	InResourceTypes		Generic browser types associated with the provided objects
	 *
	 * @return	The number of objects successfully added to the consolidation panel
	 */
	INT AddConsolidationObjects( const TArray<UObject*>& InObjects, const TArray<UGenericBrowserType*>& InResourceTypes );

	/**
	 * Fills the provided array with all of the UObjects referenced by the consolidation panel, for the purpose of serialization
	 *
	 * @param	[out]OutSerializableObjects	Array to fill with all of the UObjects referenced by the consolidation panel
	 */
	void QuerySerializableObjects( TArray<UObject*>& OutSerializableObjects );

	/**
	 * Determine the compatibility of the passed in objects with the objects already present in the consolidation panel
	 *
	 * @param	InProposedObjects		Objects to check compatibility with vs. the objects already present in the consolidation panel
	 * @param	OutCompatibleObjects	[out]Objects from the passed in array which are compatible with those already present in the
	 *									consolidation panel, if any
	 *
	 * @return	TRUE if all of the passed in objects are compatible, FALSE otherwise
	 */
	UBOOL DetermineAssetCompatibility( const TArray<UObject*>& InProposedObjects, TArray<UObject*>& OutCompatibleObjects );

	/** Removes all consolidation objects from the consolidation panel */
	void ClearConsolidationObjects();

	/**
	 * Overridden implementation of SetParentFrame in order to support custom behavior for clicking the Windows close button
	 *
	 * @param	InParentFrame	WPF frame that acts as the panel's parent
	 */
	virtual void SetParentFrame( MWPFFrame^ InParentFrame ) override;

private:
	/**
	 * Verifies if all of the consolidation objects in the panel are of the same class or not
	 *
	 * @return	TRUE if all of the classes of the consolidation objects are the same; FALSE otherwise
	 */
	UBOOL AreObjClassesHomogeneous();
	
	/** Delete all of the dropped asset data for drag-drop support */
	void ClearDroppedAssets();
	
	/** Reset the consolidate panel's error panel to its default state */
	void ResetErrorPanel();

	/** Update the consolidate panel's buttons based on the current state of the consolidation objects */
	void UpdateButtons();

	/** Remove the currently selected object from the consolidation panel */
	void RemoveSelectedObject();

	/**
	 * Display a message in the consolidation panel's "error" panel; Naive implementation, wipes out any pre-existing message
	 *
	 * @param	bError			If TRUE, change the error panel's styling to indicate the severity of the message; if FALSE, use a lighter style
	 * @param	ErrorMessage	Message to display in the "error" panel
	 *
	 * @note	The current implementation is naive and will wipe out any pre-existing message when called. The current needs of the window don't require
	 *			anything more sophisticated, but in the future perhaps the messages should be appended, multiple panel types should exist, etc.
	 */
	void DisplayMessage( UBOOL bError, const FString& ErrorMessage );
	
	// Button Responses

	/** Called in response to the user clicking the "X" button on the error panel; dismisses the error panel */
	void OnDismissErrorPanelButtonClicked( Object^ Sender, RoutedEventArgs^ Args );

	/** Called in response to the user clicking the "Consolidate Objects"/OK button; performs asset consolidation */
	void OnConsolidateButtonClicked( Object^ Sender, RoutedEventArgs^ Args );

	/** Called in response to the user clicking the cancel button; dismisses the panel w/o consolidating objects */
	void OnCancelButtonClicked( Object^ Sender, RoutedEventArgs^ Args );
	
	// Drag-drop Support

	/** Called in response to the user beginning to drag something over the consolidation panel; parses the drop data into dropped assets, if possible */
	void OnDragEnter( Object^ Sender, DragEventArgs^ Args );

	/** Called in response to the user's drag operation exiting the consolidation panel; deletes any dropped asset data */
	void OnDragLeave( Object^ Sender, DragEventArgs^ Args );
	
	/** Called in response to the user performing a drop operation in the consolidation panel; adds the dropped objects to the panel */
	void OnDrop( Object^ Sender, DragEventArgs^ Args );

	/** Called while the user is dragging something over the consolidation panel; provides visual feedback on whether a drop is allowed or not */
	void OnDragOver( Object^ Sender, DragEventArgs^ Args );

	// Input Responses

	/** Called in response to the user releasing a keyboard key while the consolidation panel has keyboard focus */
	void OnKeyUp( Object^ Sender, KeyEventArgs^ Args );

	/** Called in response to the user clicking the mouse down in the consolidation panel; grants the panel keyboard focus */
	void OnMouseDown( Object^ Sender, MouseButtonEventArgs^ Args );

	/** Called in response to a selection in the consolidation object list box changing */
	void OnListBoxSelectionChanged( Object^ Sender, SelectionChangedEventArgs^ Args );

	/** Track if the panel has already warned the user about consolidating assets with different types, so as not to repeatedly (and annoyingly) warn */
	UBOOL bAlreadyWarnedAboutTypes;

	/** List box for displaying the consolidation objects */
	ListBox^ ConsolidationObjectsListBox;

	/** Panel for displaying error messages */
	Panel^ ErrorPanel;

	/** Text block that appears in the error panel */
	TextBlock^ ErrorMessageTextBlock;

	/** Button on the error panel that the user can click to dismiss the error message */
	Button^ DismissErrorPanelButton;

	/** Checkbox that, if checked, signifies that after a consolidation operation, an attempt will be made to save the packages dirtied by the operation */ 
	CheckBox^ SaveCheckBox;

	/** Consolidate/ok button; triggers asset consolidation */
	Button^ ConsolidateButton;

	/** Cancel button; exits dialog w/o triggering asset consolidation */
	Button^ CancelButton;

	/** List of strings to appear in the list box */
	List<String^>^ ConsolidationObjectNames;

	/** Array of consolidation objects */
	MScopedNativePointer< TArray< UObject* > > ConsolidationObjects;

	/** Set of generic browser objects relevant to the consolidation objects */
	MScopedNativePointer< TSet< UGenericBrowserType* > > ConsolidationResourceTypes;

	/** Array of dropped asset data for supporting drag-and-drop */
	MScopedNativePointer< TArray< FSelectedAssetInfo > > DroppedAssets;
};

/**
 * Constructor; Construct an MConsolidateObjectsPanel
 *
 * @param	InXamlName	XAML file that specifies the panel
 */
MConsolidateObjectsPanel::MConsolidateObjectsPanel( String^ InXamlName )
:	MWPFPanel( InXamlName ),
	bAlreadyWarnedAboutTypes( FALSE )
{
	// Configure the native pointers
	ConsolidationObjects.Reset( new TArray<UObject*>() );
	ConsolidationResourceTypes.Reset( new TSet<UGenericBrowserType*>() );
	DroppedAssets.Reset( new TArray<FSelectedAssetInfo>() );
	ConsolidationObjectNames = gcnew List<String^>();

	// Set the array of object names as the item source for the list box so they can properly be displayed
	// as radio button options
	ConsolidationObjectsListBox = safe_cast<ListBox^>( LogicalTreeHelper::FindLogicalNode( this, "ConsolidateObjectsListBox" ) );
	ConsolidationObjectsListBox->ItemsSource = ConsolidationObjectNames;
	ConsolidationObjectsListBox->SelectionChanged += gcnew System::Windows::Controls::SelectionChangedEventHandler( this, &MConsolidateObjectsPanel::OnListBoxSelectionChanged ); 

	// Configure panel controls, hook up event handlers, etc.
	ErrorPanel = safe_cast<Panel^>( LogicalTreeHelper::FindLogicalNode( this, "ErrorPanel") );
	ErrorMessageTextBlock = safe_cast<TextBlock^>( LogicalTreeHelper::FindLogicalNode( this, "ErrorMessageTextBlock") );
	DismissErrorPanelButton = safe_cast<Button^>( LogicalTreeHelper::FindLogicalNode( this, "DismissErrorPanelButton") );
	DismissErrorPanelButton->Click += gcnew RoutedEventHandler( this, &MConsolidateObjectsPanel::OnDismissErrorPanelButtonClicked );

	SaveCheckBox = safe_cast<CheckBox^>( LogicalTreeHelper::FindLogicalNode( this, "SaveCheckBox") );

	ConsolidateButton = safe_cast<Button^>( LogicalTreeHelper::FindLogicalNode( this, "OKButton" ) );
	ConsolidateButton->Click += gcnew RoutedEventHandler( this, &MConsolidateObjectsPanel::OnConsolidateButtonClicked );

	CancelButton = safe_cast<Button^>( LogicalTreeHelper::FindLogicalNode( this, "CancelButton" ) );
	CancelButton->Click += gcnew RoutedEventHandler( this, &MConsolidateObjectsPanel::OnCancelButtonClicked );

	ErrorPanel->Visibility = System::Windows::Visibility::Collapsed;

	this->AllowDrop = TRUE;
	this->DragEnter += gcnew DragEventHandler( this, &MConsolidateObjectsPanel::OnDragEnter );
	this->DragLeave += gcnew DragEventHandler( this, &MConsolidateObjectsPanel::OnDragLeave );
	this->Drop += gcnew DragEventHandler( this, &MConsolidateObjectsPanel::OnDrop );
	this->DragOver += gcnew DragEventHandler( this, &MConsolidateObjectsPanel::OnDragOver );
	this->KeyUp += gcnew KeyEventHandler( this, &MConsolidateObjectsPanel::OnKeyUp );
	this->MouseDown += gcnew MouseButtonEventHandler( this, &MConsolidateObjectsPanel::OnMouseDown );
}

/**
 * Attempt to add the provided objects to the consolidation panel; Only adds objects which are compatible with objects already existing within the panel, if any
 *
 * @param	InObjects			Objects to attempt to add to the panel
 * @param	InResourceTypes		Generic browser types associated with the provided objects
 *
 * @return	The number of objects successfully added to the consolidation panel
 */
INT MConsolidateObjectsPanel::AddConsolidationObjects( const TArray<UObject*>& InObjects, const TArray<UGenericBrowserType*>& InResourceTypes )
{
	// First check the passed in objects for compatibility; allowing cross-type consolidation would result in disaster
	TArray<UObject*> CompatibleObjects;
	DetermineAssetCompatibility( InObjects, CompatibleObjects );

	// Iterate over each compatible object, adding it to the panel if it's not already there 
	for ( TArray<UObject*>::TConstIterator CompatibleObjIter( CompatibleObjects ); CompatibleObjIter; ++CompatibleObjIter )
	{
		UObject* CurObj = *CompatibleObjIter;
		check( CurObj );

		// Don't allow an object to be added to the panel twice
		if ( !ConsolidationObjects->ContainsItem( CurObj ) )
		{
			ConsolidationObjectNames->Add( CLRTools::ToString( CurObj->GetFullName() ) );
			ConsolidationObjects->AddItem( CurObj );
		}
	}

	// Refresh the list box, as new items have been added
	ConsolidationObjectsListBox->Items->Refresh();

	// Add resource types relevant to the passed in objects; there's a potential for non-related types to be added here, but
	// it's not really a big deal
	for ( TArray<UGenericBrowserType*>::TConstIterator ResourceIter( InResourceTypes ); ResourceIter; ++ResourceIter )
	{
		ConsolidationResourceTypes->Add( *ResourceIter );
	}

	// Check if all of the consolidation objects share the same type. If they don't, and the user hasn't been prompted about it before,
	// display a warning message informing them of the potential danger.
	if ( !AreObjClassesHomogeneous() && !bAlreadyWarnedAboutTypes )
	{
		DisplayMessage( FALSE, LocalizeUnrealEd("ConsolidateWindow_Warning_SameClass") );
		bAlreadyWarnedAboutTypes = TRUE;
	}

	// Update the consolidate button's enabled status based on the state of the consolidation objects
	UpdateButtons();

	return CompatibleObjects.Num();
}

/**
 * Fills the provided array with all of the UObjects referenced by the consolidation panel, for the purpose of serialization
 *
 * @param	[out]OutSerializableObjects	Array to fill with all of the UObjects referenced by the consolidation panel
 */
void MConsolidateObjectsPanel::QuerySerializableObjects( TArray<UObject*>& OutSerializableObjects )
{
	OutSerializableObjects.Empty( ConsolidationObjects->Num() );

	// Add all of the consolidation objects to the array
	OutSerializableObjects.Append( *( ConsolidationObjects.Get() ) );

	// Add each generic browser type to the array
	for ( TSet<UGenericBrowserType*>::TConstIterator ConsolResourceIter( *( ConsolidationResourceTypes.Get() ) ); ConsolResourceIter; ++ConsolResourceIter )
	{
		OutSerializableObjects.AddUniqueItem( *ConsolResourceIter );
	}

	// Add each drop data info object to the array
	for ( TArray<FSelectedAssetInfo>::TConstIterator DroppedAssetsIter( *( DroppedAssets.Get() ) ); DroppedAssetsIter; ++DroppedAssetsIter )
	{
		const FSelectedAssetInfo& CurInfo = *DroppedAssetsIter;
		OutSerializableObjects.AddUniqueItem( CurInfo.Object );
	}
}

/**
 * Determine the compatibility of the passed in objects with the objects already present in the consolidation panel
 *
 * @param	InProposedObjects		Objects to check compatibility with vs. the objects already present in the consolidation panel
 * @param	OutCompatibleObjects	[out]Objects from the passed in array which are compatible with those already present in the
 *									consolidation panel, if any
 *
 * @return	TRUE if all of the passed in objects are compatible, FALSE otherwise
 */
UBOOL MConsolidateObjectsPanel::DetermineAssetCompatibility( const TArray<UObject*>& InProposedObjects, TArray<UObject*>& OutCompatibleObjects )
{
	UBOOL bAllAssetsValid = TRUE;

	OutCompatibleObjects.Empty();

	if ( InProposedObjects.Num() > 0 )
	{
		// If the consolidation panel is currently empty, use the first member of the proposed objects as the object whose class should be checked against.
		// Otherwise, use the first consolidation object.
		const UObject* ComparisonObject = ConsolidationObjects->Num() > 0 ? ( *ConsolidationObjects.Get() )( 0 ) : InProposedObjects( 0 );
		check( ComparisonObject );

		const UClass* ComparisonClass = ComparisonObject->GetClass();
		check( ComparisonClass );

		// Iterate over each proposed consolidation object, checking if each shares a common class with the consolidation objects, or at least, a common base that
		// is allowed as an exception (currently only exceptions made for textures and materials).
		for ( TArray<UObject*>::TConstIterator ProposedObjIter( InProposedObjects ); ProposedObjIter; ++ProposedObjIter )
		{
			UObject* CurProposedObj = *ProposedObjIter;
			check( CurProposedObj );

			if ( CurProposedObj->GetClass() != ComparisonClass )
			{
				const UClass* NearestCommonBase = CurProposedObj->FindNearestCommonBaseClass( ComparisonClass );
				
				// If the proposed object doesn't share a common class or a common base that is allowed as an exception, it is not a compatible object
				if ( !( NearestCommonBase->IsChildOf( UTexture::StaticClass() ) )  && ! ( NearestCommonBase->IsChildOf( UMaterialInterface::StaticClass() ) ) )
				{
					bAllAssetsValid = FALSE;
					continue;
				}
			}
			
			// If the proposed object is already in the panel, it is not a compatible object
			if ( ConsolidationObjects->ContainsItem( CurProposedObj ) )
			{
				bAllAssetsValid = FALSE;
				continue;
			}

			// If execution has gotten this far, the current proposed object is compatible
			OutCompatibleObjects.AddItem( CurProposedObj );
		}
	}

	return bAllAssetsValid;
}

/** Removes all consolidation objects from the consolidation panel */
void MConsolidateObjectsPanel::ClearConsolidationObjects()
{
	ConsolidationObjectNames->Clear();
	ConsolidationObjectsListBox->Items->Refresh();
	ConsolidationObjects->Empty();
}

/**
 * Overridden implementation of SetParentFrame in order to support custom behavior for clicking the Windows close button
 *
 * @param	InParentFrame	WPF frame that acts as the panel's parent
 */
void MConsolidateObjectsPanel::SetParentFrame( MWPFFrame^ InParentFrame )
{
	MWPFPanel::SetParentFrame( InParentFrame );

	// Treat clicking the windows close button the same as the user clicking the cancel button by adding an event handler to the close button
	Button^ CloseButton = safe_cast<Button^>( LogicalTreeHelper::FindLogicalNode( InParentFrame->GetRootVisual(), "TitleBarCloseButton" ) );
	CloseButton->Click += gcnew RoutedEventHandler( this, &MConsolidateObjectsPanel::OnCancelButtonClicked );
}

/**
 * Verifies if all of the consolidation objects in the panel are of the same class or not
 *
 * @return	TRUE if all of the classes of the consolidation objects are the same; FALSE otherwise
 */
UBOOL MConsolidateObjectsPanel::AreObjClassesHomogeneous()
{
	UBOOL bAllClassesSame = TRUE;

	if ( ConsolidationObjects->Num() > 1 )
	{
		TArray<UObject*>::TConstIterator ConsolidationObjIter( *ConsolidationObjects.Get() );
		const UObject* FirstObj = *ConsolidationObjIter;
		check( FirstObj );
		
		// Store the class of the first consolidation object for comparison purposes
		const UClass* FirstObjClass = FirstObj->GetClass();
		check( FirstObjClass );
		
		// Starting from the second consolidation object, iterate through all consolidation objects
		// to see if they all share a common class
		++ConsolidationObjIter;
		for ( ConsolidationObjIter; ConsolidationObjIter; ++ConsolidationObjIter )
		{
			const UObject* CurObj = *ConsolidationObjIter;
			check( CurObj );

			const UClass* CurObjClass = CurObj->GetClass();
			check( CurObjClass );

			if ( CurObjClass != FirstObjClass )
			{
				bAllClassesSame = FALSE;
				break;
			}
		}
	}

	return bAllClassesSame;
}

/** Delete all of the dropped asset data for drag-drop support */
void MConsolidateObjectsPanel::ClearDroppedAssets()
{
	DroppedAssets->Empty();
}

/** Reset the consolidate panel's error panel to its default state */
void MConsolidateObjectsPanel::ResetErrorPanel()
{
	bAlreadyWarnedAboutTypes = FALSE;
	ErrorPanel->Visibility = System::Windows::Visibility::Collapsed;
	ErrorMessageTextBlock->Text = "";
}

/** Update the consolidate panel's buttons based on the current state of the consolidation objects */
void MConsolidateObjectsPanel::UpdateButtons()
{
	// Only enable the consolidate button if there are two or more consolidation objects and one of which is selected
	ConsolidateButton->IsEnabled = ConsolidationObjects->Num() > 1 && ConsolidationObjectsListBox->SelectedIndex >= 0 ? TRUE : FALSE;
}

/** Remove the currently selected object from the consolidation panel */
void MConsolidateObjectsPanel::RemoveSelectedObject()
{
	const INT SelectedIndex = ConsolidationObjectsListBox->SelectedIndex;

	// Ensure there's currently a valid selection
	if ( ConsolidationObjects->IsValidIndex( SelectedIndex ) )
	{
		// If the selection was valid, remove the consolidation object from the panel
		ConsolidationObjects->Remove( SelectedIndex );
		ConsolidationObjectNames->RemoveAt( SelectedIndex );

		// Refresh the list box to display the change in contents
		ConsolidationObjectsListBox->Items->Refresh();

		// If prior to the removal the consolidation objects contained multiple classes but now only
		// contain one, remove the warning about the presence of multiple classes
		// NOTE: This works because of the limited number of messages utilized by the window. If more errors are added,
		// simply resetting the error panel here might confuse the user.
		if ( bAlreadyWarnedAboutTypes && AreObjClassesHomogeneous() )
		{
			ResetErrorPanel();
		}

		UpdateButtons();
	}
}

/**
 * Display a message in the consolidation panel's "error" panel; Naive implementation, wipes out any pre-existing message
 *
 * @param	bError			If TRUE, change the error panel's styling to indicate the severity of the message; if FALSE, use a lighter style
 * @param	ErrorMessage	Message to display in the "error" panel
 *
 * @note	The current implementation is naive and will wipe out any pre-existing message when called. The current needs of the window don't require
 *			anything more sophisticated, but in the future perhaps the messages should be appended, multiple panel types should exist, etc.
 */
void MConsolidateObjectsPanel::DisplayMessage( UBOOL bError, const FString& ErrorMessage )
{
	// If the message represents an error, change the panel background and text block foreground to indicate the severity of the message
	if ( bError )
	{
		ErrorPanel->Background = safe_cast<Brush^>( System::Windows::Application::Current->Resources["Slate_Error_Background"] );
		ErrorMessageTextBlock->Foreground = safe_cast<Brush^>( System::Windows::Application::Current->Resources["Slate_Error_Foreground"] );
	}
	// If the message is just a warning, change the panel background and text block foreground to use a lighter style
	else
	{
		ErrorPanel->Background = safe_cast<Brush^>( System::Windows::Application::Current->Resources["Slate_Warning_Background"] );
		ErrorMessageTextBlock->Foreground = safe_cast<Brush^>( System::Windows::Application::Current->Resources["Slate_Warning_Foreground"] );
	}

	// Update the error text block to display the requested message
	ErrorMessageTextBlock->Text = CLRTools::ToString( ErrorMessage );

	// Show the error panel
	ErrorPanel->Visibility = System::Windows::Visibility::Visible;
}

/** Called in response to the user clicking the "X" button on the error panel; dismisses the error panel */
void MConsolidateObjectsPanel::OnDismissErrorPanelButtonClicked( Object^ Sender, RoutedEventArgs^ Args )
{
	// Hide the error panel
	ErrorPanel->Visibility = System::Windows::Visibility::Collapsed;
}

/** Called in response to the user clicking the "Consolidate Objects"/OK button; performs asset consolidation */
void MConsolidateObjectsPanel::OnConsolidateButtonClicked( Object^ Sender, RoutedEventArgs^ Args )
{
	const INT SelectedIndex = ConsolidationObjectsListBox->SelectedIndex;
	check( SelectedIndex >= 0 && ConsolidationObjects->Num() > 1 );

	// Find which object the user has elected to be the "object to consolidate to"
	UObject* ObjectToConsolidateTo = ( *ConsolidationObjects.Get() )( SelectedIndex );
	check( ObjectToConsolidateTo );

	// Compose an array of the objects to consolidate, removing the "object to consolidate to" from the array
	// NOTE: We cannot just use the array held on the panel, because the references need to be cleared prior to the consolidation
	// attempt or else they will interfere and cause problems.
	TArray<UObject*> FinalConsolidationObjects = *ConsolidationObjects.Get();
	FinalConsolidationObjects.RemoveSingleItem( ObjectToConsolidateTo );

	// Compose an array of relevant generic browser types
	TArray<UGenericBrowserType*> ResourceTypes;
	for ( TSet<UGenericBrowserType*>::TConstIterator ResourceIter( *ConsolidationResourceTypes.Get() ); ResourceIter; ++ResourceIter )
	{
		ResourceTypes.AddItem( *ResourceIter );
	}

	// Close the window while the consolidation operation occurs
	ParentFrame->Close( FConsolidateWindow::CR_Consolidate );

	// Reset the panel back to its default state so that post-consolidation the panel appears as it would from a fresh launch
	ResetErrorPanel();

	// The consolidation objects must be cleared from the panel, lest they interfere with the consolidation
	ClearConsolidationObjects();

	// Perform the object consolidation
	ObjectTools::FConsolidationResults ConsResults = ObjectTools::ConsolidateObjects( ObjectToConsolidateTo, FinalConsolidationObjects, ResourceTypes );
	
	// Check if the user has specified if they'd like to save the dirtied packages post-consolidation
	if ( SaveCheckBox->IsChecked.HasValue )
	{
		// If the consolidation went off successfully with no failed objects, prompt the user to checkout/save the packages dirtied by the operation
		if ( ConsResults.DirtiedPackages.Num() > 0 && ConsResults.FailedConsolidationObjs.Num() == 0 && SaveCheckBox->IsChecked.Value == TRUE )
		{
			FEditorFileUtils::PromptForCheckoutAndSave( ConsResults.DirtiedPackages, FALSE, TRUE );
		}
		// If the consolidation resulted in failed (partially consolidated) objects, do not save, and inform the user no save attempt was made
		else if ( ConsResults.FailedConsolidationObjs.Num() > 0 && SaveCheckBox->IsChecked.Value == TRUE )
		{
			DisplayMessage( TRUE, LocalizeUnrealEd("ConsolidateWindow_Error_PartialConsolidationNoSave") );
		}
	}

	ParentFrame->SetContentAndShow( this );
}

/** Called in response to the user clicking the cancel button; dismisses the panel w/o consolidating objects */
void MConsolidateObjectsPanel::OnCancelButtonClicked( Object^ Sender, RoutedEventArgs^ Args )
{
	// Close the window and clear out all the consolidation assets/dropped assets/etc.
	ParentFrame->Close( FConsolidateWindow::CR_Cancel );
	ClearConsolidationObjects();
	ClearDroppedAssets();
	ResetErrorPanel();
}

/** Called in response to the user beginning to drag something over the consolidation panel; parses the drop data into dropped assets, if possible */
void MConsolidateObjectsPanel::OnDragEnter( Object^ Sender, DragEventArgs^ Args )
{
	// Assets being dropped from content browser should be parsable from a string format
	if ( Args->Data->GetDataPresent( DataFormats::StringFormat ) )
	{
		const TCHAR AssetDelimiter[] = { AssetMarshalDefs::AssetDelimiter, TEXT('\0') };
		
		// Extract the string being dragged over the panel
		String^ DroppedData = safe_cast<String^>( Args->Data->GetData( DataFormats::StringFormat ) );
		FString SourceData = CLRTools::ToFString( DroppedData );

		// Parse the dropped string into separate asset strings
		TArray<FString> DroppedAssetStrings;
		SourceData.ParseIntoArray( &DroppedAssetStrings, AssetDelimiter, TRUE );

		// Construct drop data info for each parsed asset string
		DroppedAssets->Empty( DroppedAssetStrings.Num() );
		for ( TArray<FString>::TConstIterator StringIter( DroppedAssetStrings ); StringIter; ++StringIter )
		{
			new( *( DroppedAssets.Get() ) ) FSelectedAssetInfo( *StringIter );
		}
		Args->Handled = TRUE;
	}
}

/** Called in response to the user's drag operation exiting the consolidation panel; deletes any dropped asset data */
void MConsolidateObjectsPanel::OnDragLeave( Object^ Sender, DragEventArgs^ Args )
{
	ClearDroppedAssets();
	Args->Handled = TRUE;
}

/** Called in response to the user performing a drop operation in the consolidation panel; adds the dropped objects to the panel */
void MConsolidateObjectsPanel::OnDrop( Object^ Sender, DragEventArgs^ Args )
{
	if ( FContentBrowser::IsInitialized() )
	{
		FContentBrowser& ContentBrowserInstance = FContentBrowser::GetActiveInstance();
		const TMap< UClass*, TArray< UGenericBrowserType* > >& TypeMap = ContentBrowserInstance.GetBrowsableObjectTypeMap();

		TArray<UGenericBrowserType*> ResourceTypes;
		TArray<UObject*> DroppedObjects;

		// Iterate over all of the dropped asset data, checking for valid assets to drop into the consolidation panel
		for ( TArray<FSelectedAssetInfo>::TIterator DroppedAssetsIter( *( DroppedAssets.Get() ) ); DroppedAssetsIter; ++DroppedAssetsIter )
		{
			FSelectedAssetInfo& CurSelectedAssetInfo = *DroppedAssetsIter;
			if ( CurSelectedAssetInfo.Object == NULL )
			{
				// If the object specified by the dropped data is NULL, try to find it first
				CurSelectedAssetInfo.Object = UObject::StaticFindObject( CurSelectedAssetInfo.ObjectClass, ANY_PACKAGE, *CurSelectedAssetInfo.ObjectPathName );
				if ( CurSelectedAssetInfo.Object == NULL && FContentBrowser::IsInitialized() && FContentBrowser::IsAssetValidForLoading( CurSelectedAssetInfo.ObjectPathName ) )
				{
					// If the object is still NULL, try to load the object
					CurSelectedAssetInfo.Object = UObject::StaticLoadObject(CurSelectedAssetInfo.ObjectClass, NULL, *CurSelectedAssetInfo.ObjectPathName, NULL, LOAD_NoWarn|LOAD_Quiet, NULL, FALSE );
					if ( CurSelectedAssetInfo.Object )
					{
						// If the load was successful, update the content browser to show the change in loaded status
						FCallbackEventParameters Parms( NULL, CALLBACK_RefreshContentBrowser, CBR_UpdateAssetListUI | CBR_UpdatePackageList, CurSelectedAssetInfo.Object );
						GCallbackEvent->Send( Parms );
					}
				}
			}

			// If the object was successfully found, add it to the consolidation panel, along with its associated generic browser types
			if ( CurSelectedAssetInfo.Object )
			{
				DroppedObjects.AddItem( CurSelectedAssetInfo.Object );
				const TArray<UGenericBrowserType*>* TypeArray = TypeMap.Find( CurSelectedAssetInfo.Object->GetClass() );
				if ( TypeArray )
				{
					for ( TArray<UGenericBrowserType*>::TConstIterator TypeIter( *TypeArray ); TypeIter; ++TypeIter )
					{
						ResourceTypes.AddUniqueItem( *TypeIter );
					}
				}
			}
		}
		
		AddConsolidationObjects( DroppedObjects, ResourceTypes );				
	}

	// Clear out the drop data, as the drop is over
	ClearDroppedAssets();
	Keyboard::Focus( this );
	Args->Handled = TRUE;
}

/** Called while the user is dragging something over the consolidation panel; provides visual feedback on whether a drop is allowed or not */
void MConsolidateObjectsPanel::OnDragOver( Object^ Sender, DragEventArgs^ Args )
{
	Args->Effects = DragDropEffects::None;

	// Construct an array of objects that would be dropped upon the consolidation panel
	TArray<UObject*> DroppedObjects;
	for ( TArray<FSelectedAssetInfo>::TIterator DroppedAssetsIter( *( DroppedAssets.Get() ) ); DroppedAssetsIter; ++DroppedAssetsIter )
	{
		const FSelectedAssetInfo& CurInfo = *DroppedAssetsIter;

		// If the object wasn't found, utilize the class default object, as we're only interested in the class type
		UObject* CurObject = CurInfo.Object ? CurInfo.Object : CurInfo.ObjectClass->GetDefaultObject();
		DroppedObjects.AddUniqueItem( CurObject );
	}

	// If all of the dragged over assets are compatible, update the mouse cursor to signify a drop is possible
	TArray<UObject*> CompatibleAssets;
	if ( DroppedObjects.Num() > 0 && DetermineAssetCompatibility( DroppedObjects, CompatibleAssets ) )
	{
		Args->Effects = DragDropEffects::Copy;
	}

	Args->Handled = TRUE;
}

/** Called in response to the user releasing a keyboard key while the consolidation panel has keyboard focus */
void MConsolidateObjectsPanel::OnKeyUp( Object^ Sender, KeyEventArgs^ Args )
{
	// If the user presses delete, remove the currently selected asset from the consolidation panel
	if ( Args->Key == Key::Delete )
	{
		RemoveSelectedObject();
		Args->Handled = TRUE;
	}
}

/** Called in response to the user clicking the mouse down in the consolidation panel; grants the panel keyboard focus */
void MConsolidateObjectsPanel::OnMouseDown( Object^ Sender, MouseButtonEventArgs^ Args )
{
	Keyboard::Focus( this );
}

/** Called in response to a selection in the consolidation object list box changing */
void MConsolidateObjectsPanel::OnListBoxSelectionChanged( Object^ Sender, SelectionChangedEventArgs^ Args )
{
	UpdateButtons();
	Keyboard::Focus( this );
}

//////////////////////////////////////////////////////////////////////////
// FConsolidateWindow													//
//////////////////////////////////////////////////////////////////////////

// Initialize static instance to NULL
FConsolidateWindow* FConsolidateWindow::Instance = NULL;

/** Shutdown the singleton, freeing the allocated memory */
void FConsolidateWindow::Shutdown()
{
	delete Instance;
	Instance = NULL;
}

/**
 * Attempt to add objects to the consolidation window
 *
 * @param	InObjects			Objects to attempt to add to the consolidation window
 * @param	InResourceTypes		Generic browser types associated with the passed in objects	
 */
void FConsolidateWindow::AddConsolidationObjects( const TArray<UObject*>& InObjects, const TArray<UGenericBrowserType*>& InResourceTypes )
{
	// Attempt to add the objects to the consolidation panel
	FConsolidateWindow& InternalInstance = GetInternalInstance();
	const INT NumAdded = InternalInstance.ConsolidateObjPanel->AddConsolidationObjects( InObjects, InResourceTypes );

	// If the frame isn't currently shown, display the frame in a modeless manner 
	const UBOOL bWindowShown = ::IsWindowVisible( InternalInstance.ConsolidateObjFrame->GetWindowHandle() );
	if ( NumAdded && !bWindowShown )
	{
		InternalInstance.ConsolidateObjFrame->SetContentAndShow( InternalInstance.ConsolidateObjPanel );
	}
}

/**
 * Determine the compatibility of the passed in objects with the objects already present in the consolidation window
 *
 * @param	InProposedObjects		Objects to check compatibility with vs. the objects already present in the consolidation window
 * @param	OutCompatibleObjects	[out]Objects from the passed in array which are compatible with those already present in the
 *									consolidation window, if any
 *
 * @return	TRUE if all of the passed in objects are compatible, FALSE otherwise
 */
UBOOL FConsolidateWindow::DetermineAssetCompatibility( const TArray<UObject*>& InProposedObjects, TArray<UObject*>& OutCompatibleObjects )
{
	FConsolidateWindow& InternalInstance = GetInternalInstance();
	return InternalInstance.ConsolidateObjPanel->DetermineAssetCompatibility( InProposedObjects, OutCompatibleObjects );
}

/**
 * FSerializableObject interface; Serialize object references
 *
 * @param	Ar	Archive used to serialize with
 */
void FConsolidateWindow::Serialize( FArchive& Ar )
{
	// Query all of the serializable objects the consolidation panel is holding onto so they can be properly
	// serialized
	TArray<UObject*> SerializableObjects;
	ConsolidateObjPanel->QuerySerializableObjects( SerializableObjects );
	
	// Serialize all of the objects from the consolidation panel
	for ( TArray<UObject*>::TConstIterator SerializeIter( SerializableObjects ); SerializeIter; ++SerializeIter )
	{
		UObject* CurObject = *SerializeIter;
		Ar << CurObject;
	}
}

/**
 * FCallbackEventDevice interface; Respond to map change callbacks by clearing the consolidation window
 *
 * @param	InType	Type of callback event
 * @param	InFlag	Flag associated with the callback
 */
void FConsolidateWindow::Send( ECallbackEventType InType, DWORD InFlag )
{
	// If a map change is occurring, clear all of the consolidation objects so that no references are
	// held on to
	if ( InType == CALLBACK_MapChange && InFlag != MapChangeEventFlags::Default )
	{
		ConsolidateObjPanel->ClearConsolidationObjects();
	}
}

/** Constructor; Construct an FConsolidateWindow */
FConsolidateWindow::FConsolidateWindow()
{
	// Register for map change events
	GCallbackEvent->Register( CALLBACK_MapChange, this );

	// Initialize settings for the WPF frame
	WPFFrameInitStruct^ Settings = gcnew WPFFrameInitStruct;
	Settings->WindowTitle = CLRTools::ToString( *LocalizeUnrealEd("ConsolidateWindow_Title") );
	Settings->bForceToFront = TRUE;
	Settings->bUseWxDialog = FALSE;

	// Construct a WPF frame for the custom consolidation panel
	ConsolidateObjFrame = gcnew MWPFFrame( NULL, Settings, TEXT("ConsolidateWindow") );
	check( ConsolidateObjFrame );

	// Construct a consolidation panel
	ConsolidateObjPanel = gcnew MConsolidateObjectsPanel( CLRTools::ToString( TEXT("ConsolidateWindow.xaml") ) );
	check( ConsolidateObjPanel );

	delete Settings;
}

/** Destructor; Destruct an FConsolidateWindow */
FConsolidateWindow::~FConsolidateWindow()
{
	delete ConsolidateObjFrame;
	delete ConsolidateObjPanel;
	ConsolidateObjFrame = NULL;
	ConsolidateObjPanel = NULL;
}

/**
 * Accessor for the private instance of the singleton; allocates the instance if it is NULL
 *
 * @note	Not really thread-safe, but should never be an issue given the use
 * @return	The private instance of the singleton
 */
FConsolidateWindow& FConsolidateWindow::GetInternalInstance()
{
	// Allocate the private instance if it's not currently allocated
	if ( Instance == NULL )
	{
		Instance = new FConsolidateWindow();
	}

	check( Instance );
	return *Instance;
}
