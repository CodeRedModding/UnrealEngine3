/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "UnrealEdCLR.h"
#include "NewMapShared.h"

#ifdef __cplusplus_cli

#include "ManagedCodeSupportCLR.h"
#include "WPFFrameCLR.h"
#include "ThumbnailToolsCLR.h"

using namespace System::Windows::Media::Imaging;

typedef const TArray<UTemplateMapMetadata*>* TemplateMapMetadataListPtr;

/** Panel for the new map screen */
ref class MNewMapPanel : public MWPFPanel
{
public:
	/**
	 * Constructor
	 *
	 * @param	InXaml	XAML file to use for the panel
	 */
	MNewMapPanel( String^ InXaml )
		: MWPFPanel( InXaml )
	{
		// Hook up the close button event
		Button^ CloseWindowButton = safe_cast< Button^ >(LogicalTreeHelper::FindLogicalNode( this, "CloseWindowButton"));
		check( CloseWindowButton != nullptr );
		CloseWindowButton->Click += gcnew RoutedEventHandler(this, &MNewMapPanel::OnCloseClicked);

		ListBox^ TemplatesStackPanelCol0 = safe_cast< ListBox^ >( LogicalTreeHelper::FindLogicalNode( this, "TemplatesStackPanelCol0") );
		check( TemplatesStackPanelCol0 != nullptr );
		TemplatesStackPanelCol0->SelectionChanged += gcnew SelectionChangedEventHandler(this, &MNewMapPanel::OnSelect);

		ListBox^ TemplatesStackPanelCol1 = safe_cast< ListBox^ >( LogicalTreeHelper::FindLogicalNode( this, "TemplatesStackPanelCol1") );
		check( TemplatesStackPanelCol1 != nullptr );
		TemplatesStackPanelCol1->SelectionChanged += gcnew SelectionChangedEventHandler(this, &MNewMapPanel::OnSelect);
	}

	/**
	 * Callback when the parent frame is set to hook up custom events to its widgets
	 */
	virtual void SetParentFrame( MWPFFrame^ InParentFrame ) override
	{
		ListBox^ TemplatesStackPanelCol0 = safe_cast< ListBox^ >( LogicalTreeHelper::FindLogicalNode( this, "TemplatesStackPanelCol0") );
		check( TemplatesStackPanelCol0 != nullptr );
		TemplatesStackPanelCol0->SelectedItem = nullptr;
		ListBox^ TemplatesStackPanelCol1 = safe_cast< ListBox^ >( LogicalTreeHelper::FindLogicalNode( this, "TemplatesStackPanelCol1") );
		check( TemplatesStackPanelCol1 != nullptr );
		TemplatesStackPanelCol1->SelectedItem = nullptr;
		SelectedTemplate = nullptr;

		if (GetParentFrame() != InParentFrame)
		{
			MWPFPanel::SetParentFrame( InParentFrame );

			PopulateTemplateList();
		}
	}

	/**
	 * List of templates to show the user. Set by the calling code before the screen is shown
	 */
	property TemplateMapMetadataListPtr Templates;

	/**
	 * The template selected by the user. Set internally or left empty if the user cancels or selects to use a blank map
	 */
	property String^ SelectedTemplate;

private:

	/** Called when the user clicks the close button */
	void OnCloseClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		SelectedTemplate = nullptr;
		ParentFrame->Close( 0 );	// close with result==0 => no new map selected
	}

	/** Called when the user clicks on an item in the main metadata list */
	void OnSelect( Object^ Owner, SelectionChangedEventArgs^ Args )
	{
		ListBox^ TemplatesStackPanel = (ListBox^)Owner;
		if (nullptr != TemplatesStackPanel->SelectedItem)
		{
			UnrealEd::TemplateMapMetadata^ Item = (UnrealEd::TemplateMapMetadata^)TemplatesStackPanel->SelectedItem;
			if (nullptr != Item)
			{
				SelectedTemplate = Item->PackageName; // this will be empty for the blank map item and a valid template map name for the others
				ParentFrame->Close( 1 ); // close with result==1 => the user selected a map and didn't cancel or close the screen
			}
		}
	}

	/** Called when the screen initialises - adds an item to the panel for each template in the Templates property and a blank map item */
	void PopulateTemplateList()
	{
		// There are two controls, one for each column on the panel
		ItemsControl^ EvenTemplatesStackPanel = safe_cast< ItemsControl^ >( LogicalTreeHelper::FindLogicalNode( this, "TemplatesStackPanelCol0") );
		check( EvenTemplatesStackPanel != nullptr );
		ItemsControl^ OddTemplatesStackPanel = safe_cast< ItemsControl^ >( LogicalTreeHelper::FindLogicalNode( this, "TemplatesStackPanelCol1") );
		check( OddTemplatesStackPanel != nullptr );

		// Create a dummy item for the blank map option using NewMap_BlankMap.png
		UnrealEd::TemplateMapMetadata^ BlankMap = gcnew UnrealEd::TemplateMapMetadata();
		BlankMap->PackageName = nullptr;	// NULL name for this one to tell client code that this is the blank map option
		BlankMap->DisplayName = CLRTools::ToString(TEXT("Blank Map"));
		BlankMap->Thumbnail = gcnew BitmapImage(
			gcnew Uri( CLRTools::ToString(*FString::Printf(TEXT("%swxres\\NewMap_BlankMap.png"), *GetEditorResourcesDir())), UriKind::Absolute));

		// Create a default image to use if any metadata items lack their own thumbnail
		BitmapImage^ DefaultThumbnail = gcnew BitmapImage(
			gcnew Uri( CLRTools::ToString(*FString::Printf(TEXT("%swxres\\NewMap_Default.png"), *GetEditorResourcesDir())), UriKind::Absolute));

		for (INT TemplIdx = 0; TemplIdx < Templates->Num(); TemplIdx++)
		{
			UTemplateMapMetadata* NativeMetadata = (*Templates)(TemplIdx);

			UnrealEd::TemplateMapMetadata^ TemplateMap = gcnew UnrealEd::TemplateMapMetadata();
			TemplateMap->PackageName = CLRTools::ToString(NativeMetadata->GetName());
			TemplateMap->Thumbnail = DefaultThumbnail;	// if the thumbnail creation code below fails this default value will be used
			if (NULL != NativeMetadata->Thumbnail)
			{
				// Attempt to create a thumbnail using the standard method used by the content browser
				// Because the thumbnail property is a Texture2D object the thumbnail will be the texture image
				FObjectThumbnail* Tex2DObjectThumbnail = ThumbnailTools::GenerateThumbnailForObject(NativeMetadata->Thumbnail);
				if (NULL != Tex2DObjectThumbnail)
				{
					TemplateMap->Thumbnail = ThumbnailToolsCLR::CreateBitmapSourceForThumbnail(*Tex2DObjectThumbnail);
				}
			}
			// Display names for items are looked-up in the EditorMapTemplates localization files.
			// Missing items in the localization files will generate a warning and the usual missing localized name 
			TemplateMap->DisplayName = CLRTools::ToString(Localize(TEXT("TemplateMapMetadata.DisplayNames"),
																	*NativeMetadata->GetName(),
																	TEXT("EditorMapTemplates"),
																	UObject::GetLanguage(),
																	FALSE));

			// Place items in the correct column - evens on the left, odds on the right
			if (0 == (TemplIdx % 2))
			{
				EvenTemplatesStackPanel->Items->Add(TemplateMap);
			}
			else
			{
				OddTemplatesStackPanel->Items->Add(TemplateMap);
			}
		}

		// Finally, place the blank map item in the correct column - even on the left, odd on the right
		if (0 == (EvenTemplatesStackPanel->Items->Count % 2))
		{
			EvenTemplatesStackPanel->Items->Add(BlankMap);
		}
		else
		{
			OddTemplatesStackPanel->Items->Add(BlankMap);
		}
	}
};

#endif // #ifdef __cplusplus_cli

// Initialize static instance to NULL
FNewMapScreen* FNewMapScreen::Instance = NULL;

/** Display the new map screen
*
* @param	Templates - List of templates to show in the new map screen (not including the blank map option)
*
* @param	OutTemplateName	- (out) The template selected by the user. Empty if blank map selected.
*
* @return	TRUE if the user selected a valid item, FALSE if the user cancelled
*
*/
BOOL FNewMapScreen::DisplayNewMapScreen(const TArray<UTemplateMapMetadata*>& Templates, FString& OutTemplateName)
{
	OutTemplateName.Empty();
	BOOL rtn = FALSE;

#ifdef __cplusplus_cli
	FNewMapScreen& Instance = GetInternalInstance();
	Instance.NewMapPanel->Templates = &Templates;
	rtn = 0 != Instance.NewMapScreenFrame->SetContentAndShowModal(Instance.NewMapPanel, 0) ? TRUE : FALSE;
	if (rtn && !String::IsNullOrEmpty(Instance.NewMapPanel->SelectedTemplate))
	{
		OutTemplateName = CLRTools::ToFString(Instance.NewMapPanel->SelectedTemplate);
	}
	Instance.NewMapPanel->Templates = NULL;
#endif // #ifdef __cplusplus_cli

	GApp->EditorFrame->Raise(); // seem to need this to stop other windows activating and hiding the main editor frame
	return rtn;
}

/** Shut down the new map screen singleton */
void FNewMapScreen::Shutdown()
{
	delete Instance;
	Instance = NULL;
}

/** Constructor */
FNewMapScreen::FNewMapScreen()
{
#ifdef __cplusplus_cli
	// Initialize settings for the WPF frame
	WPFFrameInitStruct^ Settings = gcnew WPFFrameInitStruct;
	Settings->WindowTitle = CLRTools::ToString( *LocalizeUnrealEd("NewMapScreen_Title") );
	Settings->bCenterWindow = TRUE;
	Settings->bUseSaveLayout = FALSE;

	// Construct a WPF frame for the new map screen
	NewMapScreenFrame = gcnew MWPFFrame( NULL, Settings, TEXT("NewMapScreen") );
	check( NewMapScreenFrame );

	// Construct a custom new map screen panel
	NewMapPanel = gcnew MNewMapPanel( CLRTools::ToString( TEXT("NewMapScreen.xaml") ) );
	check( NewMapPanel );

	delete Settings;
#endif // #ifdef __cplusplus_cli
}

/** Destructor */
FNewMapScreen::~FNewMapScreen()
{
	delete NewMapPanel;
	delete NewMapScreenFrame;
	NewMapPanel = NULL;
	NewMapScreenFrame = NULL;
}

/**
 * Return internal singleton instance of the class
 *
 * @return	Reference to the internal singleton instance of the class
 */
FNewMapScreen& FNewMapScreen::GetInternalInstance()
{
	if ( Instance == NULL )
	{
		Instance = new FNewMapScreen();
	}
	check( Instance );
	return *Instance;
}