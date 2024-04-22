/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEdCLR.h"

#include "ManagedCodeSupportCLR.h"
#include "MeshPaintWindowShared.h"
#include "ColorPickerShared.h"
#include "WPFFrameCLR.h"
#include "FileHelpers.h"
#include "ThumbnailToolsCLR.h"

#pragma unmanaged
#include "MeshPaintEdMode.h"
#pragma managed

#include "ConvertersCLR.h"

using namespace System::Windows::Media::Imaging;

ref class MImportColorsScreenPanel : public MWPFPanel
{
public:
	/**
	* Constructor
	*
	* @param	InXaml	XAML file to use for the panel
	*/
	MImportColorsScreenPanel( String^ InXaml )
		: MWPFPanel( InXaml )
	{
		TextBox^ ImportTGAFileNameTextBox = safe_cast< TextBox^ >( LogicalTreeHelper::FindLogicalNode( this, "ImportTGAFileNameText" ) );
		UnrealEd::Utils::CreateBinding(ImportTGAFileNameTextBox, TextBox::TextProperty, this, "ImportTGAFileName" );

		Button^ ImportFileButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "ImportTGAFileButton" ) );
		ImportFileButton->Click += gcnew RoutedEventHandler( this, &MImportColorsScreenPanel::ImportTGAFileButton_Click );

		UnrealEd::Utils::CreateBinding(
			safe_cast< ComboBox^ >( LogicalTreeHelper::FindLogicalNode( this, "ImportLODCombo" ) ),
			ComboBox::SelectedIndexProperty, this, "ImportLOD", gcnew IntToIntOffsetConverter( 0 ) );

		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( this, "ImportRedCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "ImportRed" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( this, "ImportGreenCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "ImportGreen" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( this, "ImportBlueCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "ImportBlue" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( this, "ImportAlphaCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "ImportAlpha" );

		Button^ ImportVertexColorsButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "ImportVertexColors" ) );
		ImportVertexColorsButton->Click += gcnew RoutedEventHandler( this, &MImportColorsScreenPanel::ImportVertexColorsButton_Click );

		// Hook up the buttons
		Button^ CloseButton = safe_cast< Button^ >(LogicalTreeHelper::FindLogicalNode(this, "ImportColorsCloseButton" ) );
	}

	/**
	* Callback when the parent frame is set to hook up custom events to its widgets
	*/
	virtual void SetParentFrame( MWPFFrame^ InParentFrame ) override
	{
		MWPFPanel::SetParentFrame( InParentFrame );

		Button^ CloseButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode(InParentFrame->GetRootVisual(), "TitleBarCloseButton" ) );
		CloseButton->Click += gcnew RoutedEventHandler( this, &MImportColorsScreenPanel::OnCloseClicked );
	}
	/** Import Vertex Values from Texture properties */
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(String^, ImportTGAFileName);

	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(INT, ImportUV);

	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(INT, ImportLOD);

	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(bool, ImportRed);

	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(bool, ImportGreen);

	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(bool, ImportBlue);

	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(bool, ImportAlpha);
private:

	/** Called when the user clicks the close button */
	void OnCloseClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		ParentFrame->Close( 0 );
	}
public:
	/** Called when browsing for TGA */
	void ImportTGAFileButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		WxFileDialog FileDialog( GApp->EditorFrame,
			*LocalizeUnrealEd("Open"),
			TEXT(""),
			TEXT(""),
			TEXT("Tga files|*.tga"),
			wxOPEN | wxFILE_MUST_EXIST | wxMULTIPLE,
			wxDefaultPosition);


		if( FileDialog.ShowModal() == wxID_OK )
		{

			wxArrayString FilePaths;
			FileDialog.GetPaths( FilePaths );

			// Get the set of selected paths from the dialog
			FFilename OpenFilename((const TCHAR*)FilePaths[0]);
			ImportTGAFileName =  CLRTools::ToString(OpenFilename);	
		}

	}
	/** Called when importing Vertex Values form TGA */
	void ImportVertexColorsButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		ImportVertexTextureHelper ImportVertex;
		FString Path = *CLRTools::ToFString(ImportTGAFileName);
		BYTE ColorMask =0;
		if(ImportRed)
		{
			ColorMask |= ImportVertexTextureHelper::ChannelsMask::ERed ;
		}
		if(ImportGreen)
		{
			ColorMask |= ImportVertexTextureHelper::ChannelsMask::EGreen ;
		}
		if(ImportBlue)
		{
			ColorMask |= ImportVertexTextureHelper::ChannelsMask::EBlue ;
		}
		if(ImportAlpha)
		{
			ColorMask |= ImportVertexTextureHelper::ChannelsMask::EAlpha ;
		}
		ImportVertex.Apply(Path,ImportUV ,ImportLOD, ColorMask);
	}
};


// Initialize static instance to NULL
FImportColorsScreen* FImportColorsScreen::Instance = NULL;


/** Display the ImportColors screen */
void FImportColorsScreen::DisplayImportColorsScreen()
{
#ifdef __cplusplus_cli
	FImportColorsScreen& Instance = GetInternalInstance();
	Instance.ImportColorsScreenFrame->SetContentAndShow( Instance.ImportColorsScreenPanel );
#endif // #ifdef __cplusplus_cli
}

/** Shut down the ImportColors screen singleton */
void FImportColorsScreen::Shutdown()
{
	delete Instance;
	Instance = NULL;
}

/** Constructor */
FImportColorsScreen::FImportColorsScreen()
{
	// Register to find out about other windows going modal
	GCallbackEvent->Register( CALLBACK_EditorPreModal, this );
	GCallbackEvent->Register( CALLBACK_EditorPostModal, this );

	// Initialize settings for the WPF frame
	WPFFrameInitStruct^ Settings = gcnew WPFFrameInitStruct;
	String^ WindowTitle = CLRTools::LocalizeString( "ImportColorsWindow_WindowTitle" );
	Settings->WindowTitle = WindowTitle;
	Settings->bCenterWindow = TRUE;
	Settings->bUseSaveLayout = FALSE;

	// Construct a WPF frame for the ImportColors screen
	ImportColorsScreenFrame = gcnew MWPFFrame( NULL, Settings, TEXT("ImportColorsScreen") );
	check( ImportColorsScreenFrame );

	// Construct a custom ImportColors screen panel
	ImportColorsScreenPanel = gcnew MImportColorsScreenPanel( CLRTools::ToString( TEXT("ImportColorsWindow.xaml") ) );
	check( ImportColorsScreenPanel );

	delete Settings;
}

/** Destructor */
FImportColorsScreen::~FImportColorsScreen()
{
	// Unregister global callbacks
	GCallbackEvent->UnregisterAll( this );

	delete ImportColorsScreenPanel;
	delete ImportColorsScreenFrame;
	ImportColorsScreenPanel = NULL;
	ImportColorsScreenFrame = NULL;
}

/**
* Return internal singleton instance of the class
*
* @return	Reference to the internal singleton instance of the class
*/
FImportColorsScreen& FImportColorsScreen::GetInternalInstance()
{
	if ( Instance == NULL )
	{
		Instance = new FImportColorsScreen();
	}
	check( Instance );
	return *Instance;
}

/** Override from FCallbackEventDevice to handle events */
void FImportColorsScreen::Send( ECallbackEventType Event )
{
	switch ( Event )
	{
	case CALLBACK_EditorPreModal:
		ImportColorsScreenFrame->EnableWindow( false );
		break;

	case CALLBACK_EditorPostModal:
		ImportColorsScreenFrame->EnableWindow( true );
		break;
	}
}


/** Converts a Total Weight Count value to a Visibility state based on the Paint Weight Index specified
    in the converter's parameter */
[ValueConversion(int::typeid, Visibility::typeid)]
ref class TotalWeightCountToVisibilityConverter : public IValueConverter
{

public:
    
	virtual Object^ Convert( Object^ value, Type^ targetType, Object^ parameter, Globalization::CultureInfo^ culture )
    {
		int TotalWeightCount = (int)value;
		int PaintWeightIndex = (int)parameter;
		
		if( PaintWeightIndex < TotalWeightCount )
		{
			return Visibility::Visible;
		}

		return Visibility::Collapsed;
    }

    virtual Object^ ConvertBack( Object^ value, Type^ targetType, Object^ parameter, Globalization::CultureInfo^ culture )
    {
		// Not supported
		return nullptr;
    }
};


/**  
 *	MTextureTargetListWrapper: Managed wrapper for FTextureTargetListInfo 
 */
ref class MTextureTargetListWrapper : public INotifyPropertyChanged
{
	int ListIndex;
	FTextureTargetListInfo& TextureTargetInfo;
	BitmapSource^ BitmapSrc;
public:
	virtual event PropertyChangedEventHandler^ PropertyChanged;

	MTextureTargetListWrapper( INT InIndex, FTextureTargetListInfo& InTargetInfo )
		:	ListIndex(InIndex)
		,	TextureTargetInfo(InTargetInfo)
		, BitmapSrc(nullptr)
	{
	}

	UTexture2D* GetTargetTexture() { return TextureTargetInfo.TextureData; }

	// Properties for binding
	property int Index { int get() { return ListIndex; } }
	property BitmapSource^ Bitmap   
	{ 
		BitmapSource^ get() 
		{ 
			BitmapSrc = ThumbnailToolsCLR::GetBitmapSourceForObject( TextureTargetInfo.TextureData );
			return BitmapSrc; 
		} 
	}
	property String^ TargetName			{ String^ get() { return CLRTools::ToString( TextureTargetInfo.TextureData->GetName() ); } }
	property String^ TargetPathName		
	{ 
		String^ get() 
		{	
			FString PathName = TextureTargetInfo.TextureData->GetPathName();
			INT DotSeperator = PathName.InStr(TEXT("."), TRUE);
			if( DotSeperator > 0 )
			{
				PathName = PathName.Left(DotSeperator);
			}
			return CLRTools::ToString( PathName ); 
		} 
	}
	property String^ DimensionAndFormat	
	{ 
		String^ get() 
		{
			FString DimensionAndFormatStr = FString::Printf( TEXT("%s[%s]"), *TextureTargetInfo.TextureData->GetDetailedDescription(0), *TextureTargetInfo.TextureData->GetDetailedDescription(1) );
			return CLRTools::ToString( DimensionAndFormatStr );
		}
	}
	property String^ KiloByteSize
	{
		String^ get()
		{

			FLOAT ResourceSize = (FLOAT)TextureTargetInfo.TextureData->GetResourceSize();

			TCHAR* SizeDescription = TEXT("KByte");
			ResourceSize /= 1024.0f;

			// If the texture is one megabyte or more we use MByte.
			if( ResourceSize > 1024.0f )
			{
				SizeDescription = TEXT("MByte");
				ResourceSize /= 1024.0f;
			}
			return CLRTools::ToString( FString::Printf( TEXT("%5.2f %s"),  ResourceSize,  SizeDescription ) );
		}
	}
	property String^ UndoCount
	{
		String^ get()
		{
			if(TextureTargetInfo.UndoCount > 0)
			{
				return CLRTools::ToString( FString::Printf( TEXT("Available Undo Count: %d"), TextureTargetInfo.UndoCount));
			}
			else
			{
				return CLRTools::ToString( FString( TEXT("") ) );
			}
		}
	}
	property bool IsSelected		
	{ 
		bool get() 
		{ 
			return TextureTargetInfo.bIsSelected != 0; 
		}  
		void set(bool value) 
		{ 
			TextureTargetInfo.bIsSelected = value;
			PropertyChanged(this, gcnew PropertyChangedEventArgs("IsSelected")); 
		}
	}
};


typedef MEnumerableTArrayWrapper<MTextureTargetListWrapper,FTextureTargetListInfo> MTexturePaintTargets;

/**
 * Mesh Paint Frame (managed)
 */
ref class MMeshPaintFrame
	: public MWPFFrame
{

public:
		/**
	 * Sets up a top-level WPF Window, but does not finalize.  Finalize should be called when all child windows have been appended
	 * @param InMeshPaintSystem - Mesh paint system that owns us
	 * @param InParentWindow - The parent window
	 * @param InSettings - The initialization structure for the window
	 * @param InContextName - Used for saving windows layout
	 */
	MMeshPaintFrame(FEdModeMeshPaint* InMeshPaintSystem, wxWindow* InParentWindow, WPFFrameInitStruct^ InSettings, const FString& InContextName)
		:	MWPFFrame( InParentWindow, InSettings, InContextName )
	{
		MeshPaintSystem = InMeshPaintSystem;
	}

protected:

	/** Called when the HwndSource receives a windows message, override of the default implementation. */
	virtual IntPtr VirtualMessageHookFunction( IntPtr HWnd, int Msg, IntPtr WParam, IntPtr LParam, bool% OutHandled ) override
	{
		OutHandled = false;
		int RetVal = 0;
		switch( Msg )
		{

			case WM_HOTKEY:
			case WM_KEYDOWN:
			case WM_KEYUP:
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			{
				// Filtering Numeric Key Inputs...
				const LPARAM NativeWParam = ( WPARAM )( PTRINT )WParam;
				const LPARAM NativeLParam = ( WPARAM )( PTRINT )LParam;
				UINT KeyCode = NativeWParam;
				if ( !((KeyCode >= VK_NUMPAD0 && KeyCode <= VK_NUMPAD9) || (KeyCode >= '0' && KeyCode <= '9')) )
				{
					MeshPaintSystem->AddWindowMessage( Msg, NativeWParam, NativeLParam );
				}
				break;
			}
			
			default:
				break;
		}

		// If we handled this message then we can bail.  Otherwise we will give the base WPFframe implementation a chance to handle it.
		if( OutHandled )
		{
			return (IntPtr)RetVal;
		}
		else
		{
			return MWPFFrame::VirtualMessageHookFunction( HWnd, Msg, WParam, LParam, OutHandled );
		}

	}

	/** Pointer to the mesh paint system that owns us */
	FEdModeMeshPaint* MeshPaintSystem;
};

/**
 * Mesh Paint window control (managed)
 */
ref class MMeshPaintWindow
	: public MWPFPanel
{
public:
	/**
	 * Initialize the mesh paint window
	 *
	 * @param	InMeshPaintSystem		Mesh paint system that owns us
	 * @param	InParentWindowHandle	Parent window handle
	 *
	 * @return	TRUE if successful
	 */
	MMeshPaintWindow( MWPFFrame^ InFrame, FEdModeMeshPaint* InMeshPaintSystem, String^ XamlFileName, FPickColorStruct& PaintColorStruct, FPickColorStruct& EraseColorStruct )
		: MWPFPanel(XamlFileName)
	{
		check( InMeshPaintSystem != NULL );
		MeshPaintSystem = InMeshPaintSystem;

		// Setup bindings
		Visual^ RootVisual = this;

		FrameworkElement^ WindowContentElement = safe_cast<FrameworkElement^>(RootVisual);
		WindowContentElement->DataContext = this;

		RadioTexturePaint = safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ResourceTypeRadio_Texture"));
		RadioPaintMode_Colors = safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PaintModeRadio_Colors" ) );

		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "VertexPaintTargetRadio_ComponentInstance" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsVertexPaintTargetComponentInstance" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "VertexPaintTargetRadio_Mesh" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsVertexPaintTargetMesh" );


		UnrealEd::Utils::CreateBinding(
			safe_cast< TextBlock^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "InstanceVertexColorsText" ) ),
			TextBlock::TextProperty, this, "InstanceVertexColorsText" );
		
		Button^ RemoveInstanceVertexColorsButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "RemoveInstanceVertexColorsButton" ) );
		UnrealEd::Utils::CreateBinding(
			RemoveInstanceVertexColorsButton,
			Button::IsEnabledProperty, this, "HasInstanceVertexColors" );
		RemoveInstanceVertexColorsButton->Click += gcnew RoutedEventHandler( this, &MMeshPaintWindow::RemoveInstanceVertexColorsButton_Click );
		
		Button^ FixInstanceVertexColorsButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "FixupInstanceVertexColorsButton" ) );
		UnrealEd::Utils::CreateBinding(
			FixInstanceVertexColorsButton,
			Button::IsEnabledProperty, this, "RequiresInstanceVertexColorsFixup" );
		FixInstanceVertexColorsButton->Click += gcnew RoutedEventHandler( this, &MMeshPaintWindow::FixInstanceVertexColorsButton_Click );

		Button^ CopyInstanceVertexColorsButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "CopyInstanceVertexColorsButton" ) );
		UnrealEd::Utils::CreateBinding(
			CopyInstanceVertexColorsButton,
			Button::IsEnabledProperty, this, "CanCopyToColourBufferCopy" );
		CopyInstanceVertexColorsButton->Click += gcnew RoutedEventHandler( this, &MMeshPaintWindow::CopyInstanceVertexColorsButton_Click );

		Button^ PasteInstanceVertexColorsButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PasteInstanceVertexColorsButton" ) );
		UnrealEd::Utils::CreateBinding(
			PasteInstanceVertexColorsButton,
			Button::IsEnabledProperty, this, "CanPasteFromColourBufferCopy" );
		PasteInstanceVertexColorsButton->Click += gcnew RoutedEventHandler( this, &MMeshPaintWindow::PasteInstanceVertexColorsButton_Click );

		Button^ FillInstanceVertexColorsButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "FillInstanceVertexColorsButton" ) );
		FillInstanceVertexColorsButton->Click += gcnew RoutedEventHandler( this, &MMeshPaintWindow::FillInstanceVertexColorsButton_Click );

		Button^ PushInstanceVertexColorsToMeshButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PushInstanceVertexColorsToMeshButton" ) );
		PushInstanceVertexColorsToMeshButton->Click += gcnew RoutedEventHandler( this, &MMeshPaintWindow::PushInstanceVertexColorsToMeshButton_Click );
		//hide push to mesh button when editing an assets colors
		UnrealEd::Utils::CreateBinding(
			safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PushInstanceVertexColorsToMeshButton" ) ),
			Button::VisibilityProperty, this, "IsVertexPaintTargetComponentInstance", gcnew BooleanToVisibilityConverter() );	

		Button^ ImportVertexColorsFromTGAButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ImportVertexColorsFromTGAButton" ) );
		ImportVertexColorsFromTGAButton->Click += gcnew RoutedEventHandler( this, &MMeshPaintWindow::ImportVertexColorsFromTGAButton_Click );

		Button^ SaveVertexPaintPackageButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "SaveVertexPaintPackageButton" ) );
		UnrealEd::Utils::CreateBinding(
			SaveVertexPaintPackageButton,
			Button::IsEnabledProperty, this, "IsSelectedSourceMeshDirty" );
		SaveVertexPaintPackageButton->Click += gcnew RoutedEventHandler( this, &MMeshPaintWindow::SaveVertexPaintPackageButton_Click );

		Button^ FindVertexPaintMeshInContentBrowserButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "FindVertexPaintMeshInContentBrowserButton" ) );
		FindVertexPaintMeshInContentBrowserButton->Click += gcnew RoutedEventHandler( this, &MMeshPaintWindow::FindVertexPaintMeshInContentBrowserButton_Click );

 		Button^ DuplicateMatTexButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "DuplicateInstanceMaterialAndTextureButton" ) );
		UnrealEd::Utils::CreateBinding(
			DuplicateMatTexButton,
			Button::IsEnabledProperty, this, "CanCreateInstanceMaterialAndTexture" );
 		DuplicateMatTexButton->Click += gcnew RoutedEventHandler( this, &MMeshPaintWindow::DuplicateTextureMaterialButton_Click );

		Button^ CreateNewTextureButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "CreateNewTextureButton" ) );
 		CreateNewTextureButton->Click += gcnew RoutedEventHandler( this, &MMeshPaintWindow::CreateNewTextureButton_Click );

		UVChannelComboBox = safe_cast< ComboBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "UVChannelCombo" ) );
		UnrealEd::Utils::CreateBinding( UVChannelComboBox, ComboBox::IsEnabledProperty, this, "IsSelectedTextureValid" );
		UnrealEd::Utils::CreateBinding( UVChannelComboBox, ComboBox::SelectedIndexProperty, this, "UVChannel", gcnew IntToIntOffsetConverter( 0 ) );
		UnrealEd::Utils::CreateBinding( UVChannelComboBox, ComboBox::ItemsSourceProperty, this, "UVChannelItems" );
		UnrealEd::Utils::CreateBinding( UVChannelComboBox, ComboBox::VisibilityProperty, this, "IsSelectedTextureValid", gcnew BooleanToVisibilityConverter( ) );

		// This is the dummy combobox that shows up inplace of the real one when the user does not have an object with a valid paint texture selected
		ComboBox^ UVChannelComboBoxNoSelection = safe_cast< ComboBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "UVChannelComboNoSelection" ) );
		UnrealEd::Utils::CreateBinding( UVChannelComboBoxNoSelection, ComboBox::VisibilityProperty, this, "IsSelectedTextureValid", gcnew CustomControls::NegatedBooleanToVisibilityConverter() );

		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PaintModeRadio_Colors" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsPaintModeColors" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PaintModeRadio_Weights" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsPaintModeWeights" );	



		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "BrushRadiusDragSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "BrushRadius" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "BrushFalloffAmountDragSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "BrushFalloffAmount" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "BrushStrengthDragSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "BrushStrength" );
 		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "EnableFlowCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "EnableFlow" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "FlowAmountDragSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "FlowAmount" );
 		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "IgnoreBackFacingCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "IgnoreBackFacing" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "EnableSeamPaintingCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "EnableSeamPainting" );


		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "WriteRedCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "WriteRed" );
 		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "WriteGreenCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "WriteGreen" );
 		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "WriteBlueCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "WriteBlue" );
 		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "WriteAlphaCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "WriteAlpha" );



		// Setup auto-collapsing sub panel bindings
		// Xaml: Visibility="{Binding Path=IsPaintModeColors, Converter={StaticResource BoolToVisConverter}}"
		UnrealEd::Utils::CreateBinding(
			safe_cast< Grid^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PaintColorsGrid" ) ),
			Grid::VisibilityProperty, this, "IsPaintModeColors", gcnew BooleanToVisibilityConverter() );	
		UnrealEd::Utils::CreateBinding(
			safe_cast< Grid^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PaintWeightsGrid" ) ),
			Grid::VisibilityProperty, this, "IsPaintModeWeights", gcnew BooleanToVisibilityConverter() );	


// 		if( 0 )
// 		{
// 			// NOTE: WPF currently has a bug where RadioButtons won't propagate "unchecked" state to bound properties.
// 			//		 We work around this by using an event handler to make sure our properties are in sync.
// 			RadioButton^ Radio = safe_cast< RadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PaintModeRadio_Colors" ) );
// 			Radio->Checked += gcnew RoutedEventHandler( this, &MMeshPaintWindow::OnPaintModeRadioButtonChecked );
// 			Radio->Unchecked += gcnew RoutedEventHandler( this, &MMeshPaintWindow::OnPaintModeRadioButtonChecked );
// 			Radio->IsChecked = this->IsPaintModeColors;
// 		}


 		UnrealEd::Utils::CreateBinding(
			safe_cast< ComboBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "TotalWeightCountCombo" ) ),
			ComboBox::SelectedIndexProperty, this, "TotalWeightCount", gcnew IntToIntOffsetConverter( -2 ) );


		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PaintWeightIndexRadio_1" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsPaintWeightIndex1" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PaintWeightIndexRadio_2" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsPaintWeightIndex2" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PaintWeightIndexRadio_3" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsPaintWeightIndex3" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PaintWeightIndexRadio_4" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsPaintWeightIndex4" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PaintWeightIndexRadio_5" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsPaintWeightIndex5" );

		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "EraseWeightIndexRadio_1" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsEraseWeightIndex1" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "EraseWeightIndexRadio_2" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsEraseWeightIndex2" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "EraseWeightIndexRadio_3" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsEraseWeightIndex3" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "EraseWeightIndexRadio_4" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsEraseWeightIndex4" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "EraseWeightIndexRadio_5" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsEraseWeightIndex5" );

		// Auto-collapse Paint Weight Index radio buttons based on the Total Weight Count
		UnrealEd::Utils::CreateBinding(
			safe_cast< RadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PaintWeightIndexRadio_3" ) ),
			RadioButton::VisibilityProperty, this, "TotalWeightCount", gcnew TotalWeightCountToVisibilityConverter(), 2 );	
		UnrealEd::Utils::CreateBinding(
			safe_cast< RadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PaintWeightIndexRadio_4" ) ),
			RadioButton::VisibilityProperty, this, "TotalWeightCount", gcnew TotalWeightCountToVisibilityConverter(), 3 );
		UnrealEd::Utils::CreateBinding(
			safe_cast< RadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PaintWeightIndexRadio_5" ) ),
			RadioButton::VisibilityProperty, this, "TotalWeightCount", gcnew TotalWeightCountToVisibilityConverter(), 4 );

		// Auto-collapse Erase Weight Index radio buttons based on the Total Weight Count
		UnrealEd::Utils::CreateBinding(
			safe_cast< RadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "EraseWeightIndexRadio_3" ) ),
			RadioButton::VisibilityProperty, this, "TotalWeightCount", gcnew TotalWeightCountToVisibilityConverter(), 2 );	
		UnrealEd::Utils::CreateBinding(
			safe_cast< RadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "EraseWeightIndexRadio_4" ) ),
			RadioButton::VisibilityProperty, this, "TotalWeightCount", gcnew TotalWeightCountToVisibilityConverter(), 3 );
		UnrealEd::Utils::CreateBinding(
			safe_cast< RadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "EraseWeightIndexRadio_5" ) ),
			RadioButton::VisibilityProperty, this, "TotalWeightCount", gcnew TotalWeightCountToVisibilityConverter(), 4 );


		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ResourceTypeRadio_VertexColors" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsResourceTypeVertexColors" );

		UnrealEd::BindableRadioButton^ ResourceTypePaintRadioButton = safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ResourceTypeRadio_Texture" ) );
		UnrealEd::Utils::CreateBinding( ResourceTypePaintRadioButton, UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsResourceTypeTexture" );
		ResourceTypePaintRadioButton->Click += gcnew RoutedEventHandler( this, &MMeshPaintWindow::ResourceTypePaintRadioButton_Click );

		//LogicalTreeHelper::FindLogicalNode( RootVisual, "ResourceTypeRadio_Texture" ) = TRUE;
		UnrealEd::Utils::CreateBinding(
			safe_cast< Grid^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "UndoBufferWarningGrid" ) ),
			Grid::VisibilityProperty, this, "IsBreechingUndoBuffer", gcnew BooleanToVisibilityConverter() );	

		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ColorViewModeRadio_Normal" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsColorViewModeNormal" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ColorViewModeRadio_RGB" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsColorViewModeRGB" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ColorViewModeRadio_Alpha" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsColorViewModeAlpha" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ColorViewModeRadio_Red" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsColorViewModeRed" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ColorViewModeRadio_Green" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsColorViewModeGreen" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ColorViewModeRadio_Blue" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsColorViewModeBlue" );


		// Register for property change callbacks from our properties object
		this->PropertyChanged += gcnew ComponentModel::PropertyChangedEventHandler( this, &MMeshPaintWindow::OnMeshPaintPropertyChanged );


		Button^ SwapPaintAndEraseColorButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "SwapPaintAndEraseColorButton" ) );
		SwapPaintAndEraseColorButton->Click += gcnew RoutedEventHandler( this, &MMeshPaintWindow::SwapPaintColorButton_Click );

		Button^ SwapPaintAndEraseWeightIndexButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "SwapPaintAndEraseWeightIndexButton" ) );
		SwapPaintAndEraseWeightIndexButton->Click += gcnew RoutedEventHandler( this, &MMeshPaintWindow::SwapPaintWeightIndexButton_Click );

		//which paint color to edit
		RadioButton^ PaintColorButton = safe_cast< RadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PaintColorButton" ) );
		PaintColorButton->Click += gcnew RoutedEventHandler( this, &MMeshPaintWindow::EditPaintColor);
		RadioButton^ EraseColorButton = safe_cast< RadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "EraseColorButton" ) );
		EraseColorButton->Click += gcnew RoutedEventHandler( this, &MMeshPaintWindow::EditEraseColor);

		//tell the color picker panels that they don't need ok/cancel buttons or previous color previews
		const UBOOL bIntegratedPanel = TRUE;

		//make paint sub panel
		PaintColorPanel = gcnew MColorPickerPanel(PaintColorStruct, CLRTools::ToString(TEXT("ColorPickerWindow.xaml")), bIntegratedPanel);
		PaintColorStruct.FLOATColorArray.AddItem(&(FMeshPaintSettings::Get().PaintColor));
		PaintColorPanel->BindData();
		PaintColorPanel->SetParentFrame(InFrame);
		//make erase sub panel
		EraseColorPanel = gcnew MColorPickerPanel(EraseColorStruct, CLRTools::ToString(TEXT("ColorPickerWindow.xaml")), bIntegratedPanel);
		EraseColorStruct.FLOATColorArray.AddItem(&(FMeshPaintSettings::Get().EraseColor));
		EraseColorPanel->BindData();
		EraseColorPanel->SetParentFrame(InFrame);

		//bind preview color update to color picker callback
		PaintColorPanel->AddCallbackDelegate(gcnew ColorPickerPropertyChangeFunction(this, &MMeshPaintWindow::UpdatePreviewColors));
		EraseColorPanel->AddCallbackDelegate(gcnew ColorPickerPropertyChangeFunction(this, &MMeshPaintWindow::UpdatePreviewColors));

		//make erase sub panel
		UpdatePreviewColors();

		//embed color pickers
		Border^ PaintColorBorder = safe_cast< Border^ >( LogicalTreeHelper::FindLogicalNode( this, "PaintColorBorder" ) );
		PaintColorBorder->Child = PaintColorPanel;
		Border^ EraseColorBorder = safe_cast< Border^ >( LogicalTreeHelper::FindLogicalNode( this, "EraseColorBorder" ) );
		EraseColorBorder->Child = EraseColorPanel;
		EraseColorPanel->Visibility = System::Windows::Visibility::Collapsed;

		Button^ FindInContentBrowserButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "FindTextureInContentBrowserButton" ) );
		UnrealEd::Utils::CreateBinding(
			FindInContentBrowserButton,
			Button::IsEnabledProperty, this, "IsSelectedTextureValid" );
		FindInContentBrowserButton->Click += gcnew RoutedEventHandler( this, &MMeshPaintWindow::FindTextureInContentBrowserButton_Click );

		Button^ SaveTextureButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "SaveTextureButton" ) );
		UnrealEd::Utils::CreateBinding(
			SaveTextureButton,
			Button::IsEnabledProperty, this, "IsSelectedTextureDirty" );
		SaveTextureButton->Click += gcnew RoutedEventHandler( this, &MMeshPaintWindow::SaveTextureButton_Click );

		TexturePaintTargetList = gcnew MTexturePaintTargets(MeshPaintSystem->GetTexturePaintTargetList());
		TexturePaintTargetList->PropertyChanged += gcnew PropertyChangedEventHandler( this, &MMeshPaintWindow::OnTexturePaintTargetPropertyChanged );

		TexturePaintTargetComboBox = safe_cast< ComboBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PaintTextureTargetList"));
		UnrealEd::Utils::CreateBinding( TexturePaintTargetComboBox, ComboBox::IsEnabledProperty, this, "IsSelectedTextureValid" );
		UnrealEd::Utils::CreateBinding( TexturePaintTargetComboBox,	ComboBox::VisibilityProperty, this, "IsSelectedTextureValid", gcnew BooleanToVisibilityConverter() );

		// This is the dummy combobox that shows up inplace of the real one when the user does not have an object with a valid paint texture selected
		ComboBox^ PaintTextureTargetListNoSelection = safe_cast< ComboBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PaintTextureTargetListNoSelection"));
		UnrealEd::Utils::CreateBinding( PaintTextureTargetListNoSelection, TextBlock::VisibilityProperty, this, "IsSelectedTextureValid", gcnew CustomControls::NegatedBooleanToVisibilityConverter() );
		
		Button^ CommitTextureChangesButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "CommitTextureChangesButton" ) );
		UnrealEd::Utils::CreateBinding(
			CommitTextureChangesButton,
			Button::IsEnabledProperty, this, "AreThereChangesToCommit" );
		CommitTextureChangesButton->Click += gcnew RoutedEventHandler( this, &MMeshPaintWindow::CommitTextureChangesButton_Click );

		// This is equivalent to doing the following in xaml: ItemsSource="{Binding Path=TexturePaintTargetProperty}"
		//UnrealEd::Utils::CreateBinding(TexturePaintTargetComboBox, ComboBox::ItemsSourceProperty, this, "TexturePaintTargetProperty");

		RefreshAllProperties();
	}

	virtual ~MMeshPaintWindow(void)
	{
		delete PaintColorPanel;
		delete EraseColorPanel;
	}

	virtual void SetParentFrame( MWPFFrame^ InParentFrame ) override
	{
		MWPFPanel::SetParentFrame( InParentFrame );

		// Setup a handler for when the closed button is pressed
		Button^ CloseButton = safe_cast<Button^>( LogicalTreeHelper::FindLogicalNode( InParentFrame->GetRootVisual(), "TitleBarCloseButton" ) );
		CloseButton->Click += gcnew RoutedEventHandler( this, &MMeshPaintWindow::OnClose );
	}

	/** Called when the close button on the title bar is pressed */
	void OnClose( Object^ Sender, RoutedEventArgs^ Args )
	{
		// Deactivate mesh paint when the close button is pressed
		GEditorModeTools().DeactivateMode( EM_MeshPaint );		
		FImportColorsScreen::Shutdown();
	}

	/** Called when an editor event requires us to recreate the texture paint target list. */
	void RefreshTextureTargetsList()
	{
		// Update the Texture Target list to include any new textures based on the selection
		MeshPaintSystem->UpdateTexturePaintTargetList();

		// Refresh the target box and choose an appropriate Target Index
		RefreshTextureTargetListProperties();
		TexturePaintTargetComboBox->SelectedIndex = MeshPaintSystem->GetCurrentTextureTargetIndex();
	}

	/** Called from edit mode after painting to ensure texture properties are up to date. */
	void RefreshTextureTargetListProperties()
	{
		TexturePaintTargetProperty->NotifyChanged();
	}

	/** Updates UVChannel combo box with the given parameter */
	void UpdateUVChannels()
	{
		// Create a temp list of UVs which will be assigned to the ComboBox items property
		List<int>^ NewUVChannelItems = gcnew List<int>();
		
		// Get the max number of UV Sets in the current selection
		for (INT UVSet=0; UVSet < MeshPaintSystem->GetMaxNumUVSets(); ++UVSet)
		{
			NewUVChannelItems->Add(UVSet);
		}

		// Save out original UVChannel as it will automatically update to the default once we change the bound array (UVChannelItems)
		INT OriginalUVChannel = UVChannel;
		UVChannelItems = NewUVChannelItems;

		// Restore UVChannel, as long as it's within bounds
		if( OriginalUVChannel >=0 && OriginalUVChannel < UVChannelItems->Count )
		{
			UVChannel = OriginalUVChannel;
		}
	}
	
	/** Called when an editor event requires us to commit texture paint changes. */
	void CommitPaintChanges(bool bShouldTriggerUIRefresh)
	{
		MeshPaintSystem->CommitAllPaintedTextures(bShouldTriggerUIRefresh);
	}

	/** Inform the mesh paint system of an event that requires us to restore rendertargets. */
	void RestoreRenderTargets()
	{
		MeshPaintSystem->RestoreRenderTargets();
	}

	/** Caches the settings for all actors in the current selection */
	void SaveSettingsForSelectedActors()
	{
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = CastChecked<AActor>( *It );
			MeshPaintSystem->SaveSettingsForActor( Actor );
		}
	}

protected:

	/** Called when a mesh paint window property is changed */
	void OnMeshPaintPropertyChanged( Object^ Owner, ComponentModel::PropertyChangedEventArgs^ Args )
	{
		MMeshPaintWindow^ EventWindow = safe_cast<MMeshPaintWindow^>(Owner);

		// ...
		if( Args->PropertyName == nullptr || Args->PropertyName->StartsWith( "TotalWeightCount" ) )
		{
			// Make sure that a valid paint weight index is selected after the total weight count has changed
			if( IsPaintWeightIndex3 && TotalWeightCount < 3 )
			{
				IsPaintWeightIndex2 = true;
			}
			else if( IsPaintWeightIndex4 && TotalWeightCount < 4 )
			{
				IsPaintWeightIndex3 = true;
			}
			else if( IsPaintWeightIndex5 && TotalWeightCount < 5 )
			{
				IsPaintWeightIndex4 = true;
			}
		}

		// If we get into this state, we know we are changing to texture paint mode, but the UI is currently setup to display some blend weight controls.
		if( EventWindow && Args->PropertyName == "IsResourceTypeTexture" && EventWindow->IsResourceTypeTexture && !EventWindow->IsPaintModeColors)
		{
			// Since we dont support blend weights in texture paint mode we want to switch from blend weights to colors.
			EventWindow->IsPaintModeWeights = FALSE;
			EventWindow->IsPaintModeColors = TRUE;
		}
	}

	/** Called when texture paint target is changed */
	void OnTexturePaintTargetPropertyChanged( Object^ Owner, PropertyChangedEventArgs^ Args )
	{
		//MTextureTargetListWrapper^ Item = safe_cast<MTextureTargetListWrapper^>(Owner);

		// Call the property changed handler ourself.  We are only interested in updating the texture specific properties though
		RefreshAllProperties();

	}

	/** Called when the swap paint color button is clicked */
	void SwapPaintColorButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		FLinearColor TempColor = FMeshPaintSettings::Get().PaintColor;
		FMeshPaintSettings::Get().PaintColor = FMeshPaintSettings::Get().EraseColor;
		FMeshPaintSettings::Get().EraseColor = TempColor;

		PaintColorPanel->BindData();
		EraseColorPanel->BindData();
		UpdatePreviewColors();
	}

	/**
	 * Hides erase color picker and shows the paint color picker
	 */
	void EditPaintColor( Object^ Owner, RoutedEventArgs^ Args )
	{
		PaintColorPanel->Visibility = System::Windows::Visibility::Visible;
		EraseColorPanel->Visibility = System::Windows::Visibility::Collapsed;
	}

	/**
	 * Hides paint color picker and shows the erase color picker
	 */
	void EditEraseColor( Object^ Owner, RoutedEventArgs^ Args )
	{
		PaintColorPanel->Visibility = System::Windows::Visibility::Collapsed;
		EraseColorPanel->Visibility = System::Windows::Visibility::Visible;
	}

	/**
	* Updates the preview color (original color)
	 */
	void UpdatePreviewColors()
	{
		FLinearColor PaintColor = FMeshPaintSettings::Get().PaintColor;
		FLinearColor EraseColor = FMeshPaintSettings::Get().EraseColor;

		//PAINT
		System::Windows::Shapes::Rectangle^ PaintRect = safe_cast< System::Windows::Shapes::Rectangle^ >( LogicalTreeHelper::FindLogicalNode( this, "PaintColorRectNoAlpha" ) );
		PaintRect->Fill = gcnew SolidColorBrush( MColorPickerPanel::GetBrushColor(1.0f, PaintColor.R, PaintColor.G, PaintColor.B, TRUE) );

		System::Windows::Shapes::Rectangle^ PaintRectNoAlpha = safe_cast< System::Windows::Shapes::Rectangle^ >( LogicalTreeHelper::FindLogicalNode( this, "PaintColorRect" ) );
		PaintRectNoAlpha->Fill = gcnew SolidColorBrush( MColorPickerPanel::GetBrushColor(PaintColor.A, PaintColor.R, PaintColor.G, PaintColor.B, TRUE) );

		//ERASE
		System::Windows::Shapes::Rectangle^ EraseRect = safe_cast< System::Windows::Shapes::Rectangle^ >( LogicalTreeHelper::FindLogicalNode( this, "EraseColorRectNoAlpha" ) );
		EraseRect->Fill = gcnew SolidColorBrush( MColorPickerPanel::GetBrushColor(1.0f, EraseColor.R, EraseColor.G, EraseColor.B, TRUE) );

		System::Windows::Shapes::Rectangle^ EraseRectNoAlpha = safe_cast< System::Windows::Shapes::Rectangle^ >( LogicalTreeHelper::FindLogicalNode( this, "EraseColorRect" ) );
		EraseRectNoAlpha->Fill = gcnew SolidColorBrush( MColorPickerPanel::GetBrushColor(EraseColor.A, EraseColor.R, EraseColor.G, EraseColor.B, TRUE) );
	}

	/**
	 * Saves the packages associated with passed in objects.
	 */
	UBOOL SavePackagesForObjects( TArray<UObject*>& InObjects )
	{
		if( InObjects.Num() > 0 )
		{
			BOOL bShouldUpdateCB = FALSE;
			TArray<UPackage*> PackagesToSave;
			TArray< UPackage* > PackagesWithExternalRefs;
			FString PackageNamesWithExternalRefs;

			// Find all the dirty packages that these objects belong to
			for(INT ObjIdx = 0; ObjIdx < InObjects.Num(); ++ObjIdx )
			{
				const UObject* CurrentObj = InObjects( ObjIdx );
				if( CurrentObj->GetOutermost()->IsDirty() )
				{
					PackagesToSave.AddUniqueItem( CurrentObj->GetOutermost() );
				}
			}

			if( PackagesToSave.Num() > 0 )
			{
				if( PackageTools::CheckForReferencesToExternalPackages( &PackagesToSave, &PackagesWithExternalRefs ) )
				{
					for(INT PkgIdx = 0; PkgIdx < PackagesWithExternalRefs.Num(); ++PkgIdx)
					{
						PackageNamesWithExternalRefs += FString::Printf(TEXT("%s\n"), *PackagesWithExternalRefs( PkgIdx )->GetName());
					}

					UBOOL bProceed = appMsgf( AMT_YesNo, LocalizeSecure( LocalizeUnrealEd("Warning_ExternalPackageRef"), *PackageNamesWithExternalRefs ) );

					if(!bProceed)
					{
						return FALSE;
					}
				}

				TArray< UPackage*> PackagesNotNeedingCheckout;
				UBOOL bResult = FEditorFileUtils::PromptToCheckoutPackages( FALSE, PackagesToSave, &PackagesNotNeedingCheckout );

				bShouldUpdateCB |= bResult;

				if( bResult || PackagesNotNeedingCheckout.Num() > 0 )
				{
					// Even if the user canceled the checkout dialog, we should still save packages that didn't need to be checked out.
					const TArray< UPackage* >& FinalSaveList = bResult ? PackagesToSave : PackagesNotNeedingCheckout;
					bResult = PackageTools::SavePackages( FinalSaveList, FALSE, NULL, NULL);
					if ( bResult )
					{
						bShouldUpdateCB = TRUE;
					}
				}

				if( bShouldUpdateCB == TRUE )
				{
					// Refresh the content browser: Note - If changing these flags, be careful because the following will not update properly without GAD enabled (CBR_UpdateAssetList | CBR_UpdatePackageList | CBR_UpdateSCCState)
					FCallbackEventParameters Parms(NULL, CALLBACK_RefreshContentBrowser, CBR_UpdatePackageListUI | CBR_UpdateSCCState | CBR_InternalAssetUpdate );
					GCallbackEvent->Send(Parms);
				}
			}
		}
			
		return TRUE;
	}


	
	/** Called when the swap paint weight index button is clicked */
	void SwapPaintWeightIndexButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		int TempWeightIndex = FMeshPaintSettings::Get().PaintWeightIndex;
		FMeshPaintSettings::Get().PaintWeightIndex = FMeshPaintSettings::Get().EraseWeightIndex;
		FMeshPaintSettings::Get().EraseWeightIndex = TempWeightIndex;

		// Call the property changed handler ourself since we circumvented the get/set accessors
		RefreshAllProperties();
	}


	
	/** Called when the remove instance vertex colors button is clicked */
	void RemoveInstanceVertexColorsButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		MeshPaintSystem->RemoveInstanceVertexColors();
	}

	/** Called when the fixup instance vertex colors button is clicked */
	void FixInstanceVertexColorsButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		MeshPaintSystem->FixupInstanceVertexColors();
	}

	/** Called when the copy instance vertex colors button is clicked */
	void CopyInstanceVertexColorsButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		MeshPaintSystem->CopyInstanceVertexColors();
	}

	/** Called when the paste instance vertex colors button is clicked */
	void PasteInstanceVertexColorsButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		MeshPaintSystem->PasteInstanceVertexColors();
	}


	/** Called to flood fill the vertex color with the current paint color */
	void FillInstanceVertexColorsButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		MeshPaintSystem->FillInstanceVertexColors();
	}

	/** Called to push instance vertex color to the mesh */
	void PushInstanceVertexColorsToMeshButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		MeshPaintSystem->PushInstanceVertexColorsToMesh();
	}

	/** Called to push instance vertex color to the mesh */
	void ImportVertexColorsFromTGAButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		FImportColorsScreen::DisplayImportColorsScreen();
	}

	/** Called to save the dirty packages of selected vertex paint objects. */
	void SaveVertexPaintPackageButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		TArray<UObject*> StaticMeshesToSave;
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = CastChecked<AActor>( *It );

			UStaticMeshComponent* StaticMeshComponent = NULL;
			AStaticMeshActor* StaticMeshActor = Cast< AStaticMeshActor >( Actor );
			if( StaticMeshActor != NULL )
			{
				StaticMeshComponent = StaticMeshActor->StaticMeshComponent;
			}
			else
			{
				ADynamicSMActor* DynamicSMActor = Cast< ADynamicSMActor >( Actor );
				if( DynamicSMActor != NULL )
				{
					StaticMeshComponent = DynamicSMActor->StaticMeshComponent;
				}
			}

			if( StaticMeshComponent != NULL && StaticMeshComponent->StaticMesh != NULL )
			{
				StaticMeshesToSave.AddItem( StaticMeshComponent->StaticMesh );
			}
		}

		if( StaticMeshesToSave.Num() > 0 )
		{
			SavePackagesForObjects( StaticMeshesToSave );
			RefreshAllProperties();
		}
	}

	/** Called to find selected mesh in content browser, only works for first selected object */
	void FindVertexPaintMeshInContentBrowserButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		GApp->EditorFrame->SyncToContentBrowser();
	}

	/** Called when the create material/texture button is clicked */
	void CreateInstanceMaterialAndTextureButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		MeshPaintSystem->CreateInstanceMaterialAndTexture();
	}

	/** Called when the remove material/texture button is clicked */
	void RemoveInstanceMaterialAndTextureButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		MeshPaintSystem->RemoveInstanceMaterialAndTexture();
	}

	/** Called when the Texture Paint radio button is clicked */
	void ResourceTypePaintRadioButton_Click(Object^ Owner, RoutedEventArgs^ Args)
	{
		// Let the mesh paint selection change handle updating any properties
		MeshPaintSystem->ActorSelectionChangeNotify();
	}

	/** 
	 * Called when the find in content browser button is clicked. This function
	 * will find the currently selected paint target texture in the content browser
	 */
	void FindTextureInContentBrowserButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		MeshPaintSystem->FindSelectedTextureInContentBrowser();
	}

	/** 
	 * Called when the user wants to initiate the texture/material duplicate automation process.
	 */
	void DuplicateTextureMaterialButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		MeshPaintSystem->DuplicateTextureMaterialCombo();
	}

	/** 
	 * Called when the user wants to initiate creating a new texture.
	 */
	void CreateNewTextureButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		MeshPaintSystem->CreateNewTexture();
	}

	/** 
	 * Called when the save texture button is clicked.  This function will save the
	 *  package of the currently selected paint target texture.
	 */
	void SaveTextureButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		UTexture2D* SelectedTexture = MeshPaintSystem->GetSelectedTexture();
		
		if( NULL != SelectedTexture )
		{
			TArray<UObject*> TexturesToSaveArray;
			TexturesToSaveArray.AddItem( SelectedTexture );

			SavePackagesForObjects( TexturesToSaveArray );
			RefreshAllProperties();
		}
		
	}

	/** 
	 * Called when the commit texture button is clicked.  This function will push paint changes to the paint target texture.
	 */
	void CommitTextureChangesButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		CommitPaintChanges(TRUE);
	}

public:

	/** BrushRadius property */
	DECLARE_MAPPED_NOTIFY_PROPERTY( double, BrushRadius, FLOAT, FMeshPaintSettings::Get().BrushRadius );

	/** BrushFalloffAmount property */
	DECLARE_MAPPED_NOTIFY_PROPERTY( double, BrushFalloffAmount, FLOAT, FMeshPaintSettings::Get().BrushFalloffAmount );

	/** BrushStrength property */
	DECLARE_MAPPED_NOTIFY_PROPERTY( double, BrushStrength, FLOAT, FMeshPaintSettings::Get().BrushStrength );

	/** EnableFlow property */
	DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY( EnableFlow, FMeshPaintSettings::Get().bEnableFlow );

	/** FlowAmount property */
	DECLARE_MAPPED_NOTIFY_PROPERTY( double, FlowAmount, FLOAT, FMeshPaintSettings::Get().FlowAmount );

	/** IgnoreBackFacing property */
	DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY( IgnoreBackFacing, FMeshPaintSettings::Get().bOnlyFrontFacingTriangles );

	/** EnableSeamPainting property */
	DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY( EnableSeamPainting, FMeshPaintSettings::Get().bEnableSeamPainting );

	/** Radio button properties for Resource Type */
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsResourceTypeVertexColors, EMeshPaintResource::Type, FMeshPaintSettings::Get().ResourceType, EMeshPaintResource::VertexColors );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsResourceTypeTexture, EMeshPaintResource::Type, FMeshPaintSettings::Get().ResourceType, EMeshPaintResource::Texture );

	/** Radio button properties for Vertex Paint Target */
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsVertexPaintTargetComponentInstance, EMeshVertexPaintTarget::Type, FMeshPaintSettings::Get().VertexPaintTarget, EMeshVertexPaintTarget::ComponentInstance );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsVertexPaintTargetMesh, EMeshVertexPaintTarget::Type, FMeshPaintSettings::Get().VertexPaintTarget, EMeshVertexPaintTarget::Mesh );

	/** UVChannel property */
	DECLARE_MAPPED_NOTIFY_PROPERTY( int, UVChannel, INT, FMeshPaintSettings::Get().UVChannel );
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(List<int>^, UVChannelItems);

	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( bool, IsBreechingUndoBuffer );

	/** InstanceVertexColorBytes property */
	property String^ InstanceVertexColorsText
	{
		String^ get()
		{
			String^ Text = UnrealEd::Utils::Localize( "MeshPaintWindow_InstanceVertexColorsText_NoData" );

			INT NumBaseVertexColorBytes = 0;
			INT NumInstanceVertexColorBytes = 0;
			UBOOL bHasInstanceMaterialAndTexture = FALSE;
			MeshPaintSystem->GetSelectedMeshInfo( NumBaseVertexColorBytes, NumInstanceVertexColorBytes, bHasInstanceMaterialAndTexture );	// Out

			if( NumInstanceVertexColorBytes > 0 )
			{
				FLOAT VertexKiloBytes = NumInstanceVertexColorBytes / 1000.0f;
				Text = String::Format( UnrealEd::Utils::Localize( "MeshPaintWindow_InstanceVertexColorsText_NumBytes" ), VertexKiloBytes );
			}

			return Text;
		}
	}


	/** HasInstanceVertexColors property.  Returns true if the selected mesh has any instance vertex colors. */
	property bool HasInstanceVertexColors
	{
		bool get()
		{
			INT NumBaseVertexColorBytes = 0;
			INT NumInstanceVertexColorBytes = 0;
			UBOOL bHasInstanceMaterialAndTexture = FALSE;
			MeshPaintSystem->GetSelectedMeshInfo( NumBaseVertexColorBytes, NumInstanceVertexColorBytes, bHasInstanceMaterialAndTexture );	// Out
			return ( NumInstanceVertexColorBytes > 0 );
		}
	}
	
	/** RequiresInstanceVertexColorsFixup property. Returns true if a selected mesh needs its instance vertex colors fixed-up. */
	property bool RequiresInstanceVertexColorsFixup
	{
		bool get()
		{
			return ( MeshPaintSystem->RequiresInstanceVertexColorsFixup() == TRUE );
		}
	}

	/** CanCopyToColourBufferCopy property. Returns true if we can copy this the selected mesh's colour buffer. */
	property bool CanCopyToColourBufferCopy
	{
		bool get()
		{
			// only allow copying of a single mesh's color data
			if( GEditor->GetSelectedActors()->Num() != 1 )
			{
				return false;
			}

			// check to see whether or not this mesh has instanced color data...
			INT NumBaseVertexColorBytes = 0;
			INT NumInstanceVertexColorBytes = 0;
			UBOOL bHasInstanceMaterialAndTexture = FALSE;
			MeshPaintSystem->GetSelectedMeshInfo( NumBaseVertexColorBytes, NumInstanceVertexColorBytes, bHasInstanceMaterialAndTexture );	// Out
			INT NumVertexColorBytes = NumBaseVertexColorBytes + NumInstanceVertexColorBytes;

			// if there is any instanced color data, we can copy it...
			return( NumVertexColorBytes > 0 );
		}
	}

	/** CanPasteFromColourBufferCopy property. Returns true if we can paste to the selected meshes' colour buffers. */
	property bool CanPasteFromColourBufferCopy
	{
		bool get()
		{
			return ( TRUE == MeshPaintSystem->CanPasteVertexColors() );
		}
	}

	/** CanCreateInstanceMaterialAndTexture property.  Returns true a mesh is selected and we can create a new material/texture instance. */
	property bool CanCreateInstanceMaterialAndTexture
	{
		bool get()
		{
			INT NumBaseVertexColorBytes = 0;
			INT NumInstanceVertexColorBytes = 0;
			UBOOL bHasInstanceMaterialAndTexture = FALSE;
			UBOOL bAnyValidMeshes = MeshPaintSystem->GetSelectedMeshInfo( NumBaseVertexColorBytes, NumInstanceVertexColorBytes, bHasInstanceMaterialAndTexture );	// Out
			return bAnyValidMeshes && !bHasInstanceMaterialAndTexture;
		}
	}


	/** HasInstanceMaterialAndTexture property.  Returns true if the selected mesh has a paintable material/texture instance. */
	property bool HasInstanceMaterialAndTexture
	{
		bool get()
		{
			INT NumBaseVertexColorBytes = 0;
			INT NumInstanceVertexColorBytes = 0;
			UBOOL bHasInstanceMaterialAndTexture = FALSE;
			MeshPaintSystem->GetSelectedMeshInfo( NumBaseVertexColorBytes, NumInstanceVertexColorBytes, bHasInstanceMaterialAndTexture );	// Out

			return bHasInstanceMaterialAndTexture ? true : false;
		}
	}

	/** IsSelectedTextureDirty property.  Returns true if any packages, that are associated with the currently selected actor source meshes, are dirty. */
	property bool IsSelectedSourceMeshDirty
	{
		bool get()
		{
			for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ); It; ++It )
			{
				AActor* Actor = CastChecked<AActor>( *It );

				UStaticMeshComponent* StaticMeshComponent = NULL;
				AStaticMeshActor* StaticMeshActor = Cast< AStaticMeshActor >( Actor );
				if( StaticMeshActor != NULL )
				{
					StaticMeshComponent = StaticMeshActor->StaticMeshComponent;
				}
				else
				{
					ADynamicSMActor* DynamicSMActor = Cast< ADynamicSMActor >( Actor );
					if( DynamicSMActor != NULL )
					{
						StaticMeshComponent = DynamicSMActor->StaticMeshComponent;
					}
				}

				if( StaticMeshComponent != NULL && StaticMeshComponent->StaticMesh != NULL )
				{
					if( StaticMeshComponent->StaticMesh->GetOutermost()->IsDirty() )
					{
						return true;
					}
				}
			}
			return false;
		}
	}

	/** IsSelectedTextureDirty property.  Returns true if the selected paint target texture is dirty. */
	property bool IsSelectedTextureDirty
	{
		bool get()
		{
			UTexture2D* SelectedTexture = MeshPaintSystem->GetSelectedTexture();
			if( NULL != SelectedTexture )
			{
				return SelectedTexture->GetOutermost()->IsDirty() ? true : false;
			}

			return false;
		}
	}

	/** AreThereChangesToCommit property.  Returns true if there are any pending paint changes to commit to any texture. */
	property bool AreThereChangesToCommit
	{
		bool get()
		{
			return MeshPaintSystem->GetNumberOfPendingPaintChanges() > 0;
		}
	}
	
	/** IsSelectedTextureValid property.  Returns true if the selected paint target texture is valid. */
	property bool IsSelectedTextureValid
	{
		bool get()
		{
			UTexture2D* SelectedTexture = MeshPaintSystem->GetSelectedTexture();
			bool RetVal = SelectedTexture != NULL ? true : false;
			return RetVal;
		}
	}
	


	/** Radio button properties for Paint Mode */
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsPaintModeColors, EMeshPaintMode::Type, FMeshPaintSettings::Get().PaintMode, EMeshPaintMode::PaintColors );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsPaintModeWeights, EMeshPaintMode::Type, FMeshPaintSettings::Get().PaintMode, EMeshPaintMode::PaintWeights );


	/** ColorChannels properties */
	DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY( WriteRed, FMeshPaintSettings::Get().bWriteRed );
	DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY( WriteGreen, FMeshPaintSettings::Get().bWriteGreen );
	DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY( WriteBlue, FMeshPaintSettings::Get().bWriteBlue );
	DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY( WriteAlpha, FMeshPaintSettings::Get().bWriteAlpha );


	/** TotalWeightCount property */
	DECLARE_MAPPED_NOTIFY_PROPERTY( int, TotalWeightCount, INT, FMeshPaintSettings::Get().TotalWeightCount );

	/** Radio button properties for PaintWeightIndex */
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsPaintWeightIndex1, INT, FMeshPaintSettings::Get().PaintWeightIndex, 0 );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsPaintWeightIndex2, INT, FMeshPaintSettings::Get().PaintWeightIndex, 1 );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsPaintWeightIndex3, INT, FMeshPaintSettings::Get().PaintWeightIndex, 2 );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsPaintWeightIndex4, INT, FMeshPaintSettings::Get().PaintWeightIndex, 3 );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsPaintWeightIndex5, INT, FMeshPaintSettings::Get().PaintWeightIndex, 4 );

	/** Radio button properties for EraseWeightIndex */
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsEraseWeightIndex1, INT, FMeshPaintSettings::Get().EraseWeightIndex, 0 );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsEraseWeightIndex2, INT, FMeshPaintSettings::Get().EraseWeightIndex, 1 );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsEraseWeightIndex3, INT, FMeshPaintSettings::Get().EraseWeightIndex, 2 );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsEraseWeightIndex4, INT, FMeshPaintSettings::Get().EraseWeightIndex, 3 );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsEraseWeightIndex5, INT, FMeshPaintSettings::Get().EraseWeightIndex, 4 );


	/** Radio button properties for Color View Mode */
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsColorViewModeNormal, EMeshPaintColorViewMode::Type, FMeshPaintSettings::Get().ColorViewMode, EMeshPaintColorViewMode::Normal );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsColorViewModeRGB, EMeshPaintColorViewMode::Type, FMeshPaintSettings::Get().ColorViewMode, EMeshPaintColorViewMode::RGB );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsColorViewModeAlpha, EMeshPaintColorViewMode::Type, FMeshPaintSettings::Get().ColorViewMode, EMeshPaintColorViewMode::Alpha );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsColorViewModeRed, EMeshPaintColorViewMode::Type, FMeshPaintSettings::Get().ColorViewMode, EMeshPaintColorViewMode::Red );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsColorViewModeGreen, EMeshPaintColorViewMode::Type, FMeshPaintSettings::Get().ColorViewMode, EMeshPaintColorViewMode::Green );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsColorViewModeBlue, EMeshPaintColorViewMode::Type, FMeshPaintSettings::Get().ColorViewMode, EMeshPaintColorViewMode::Blue );

	// TexturePaint Targets
	MTexturePaintTargets^ TexturePaintTargetList;
	ComboBox^ TexturePaintTargetComboBox;
	ComboBox^ UVChannelComboBox;

	DECLARE_MAPPED_NOTIFY_PROPERTY(MTexturePaintTargets^, TexturePaintTargetProperty, MTexturePaintTargets^, TexturePaintTargetList);


	// @todo MeshPaint: Mapping to a non managed var may not be allowed.
	DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY(IsSelectionLocked, GEdSelectionLock);

protected:

	//color picker widgets
	MColorPickerPanel^ PaintColorPanel;
	MColorPickerPanel^ EraseColorPanel;
	FPickColorStruct* PaintColorStruct;
	FPickColorStruct* EraseColorStruct;

	/** Pointer to the mesh paint system that owns us */
	FEdModeMeshPaint* MeshPaintSystem;

	/** True if a color picker is currently visible */
	bool IsColorPickerVisible;

	UnrealEd::BindableRadioButton^ RadioTexturePaint;
	UnrealEd::BindableRadioButton^ RadioPaintMode_Colors;
};



/** Static: Allocate and initialize mesh paint window */
FMeshPaintWindow* FMeshPaintWindow::CreateMeshPaintWindow( FEdModeMeshPaint* InMeshPaintSystem )
{
	FMeshPaintWindow* NewMeshPaintWindow = new FMeshPaintWindow();

	if( !NewMeshPaintWindow->InitMeshPaintWindow( InMeshPaintSystem) )
	{
		delete NewMeshPaintWindow;
		return NULL;
	}

	return NewMeshPaintWindow;
}



/** Constructor */
FMeshPaintWindow::FMeshPaintWindow()
{
	// Register to find out about other windows going modal
	GCallbackEvent->Register( CALLBACK_EditorPreModal, this );
	GCallbackEvent->Register( CALLBACK_EditorPostModal, this );
	GCallbackEvent->Register( CALLBACK_ObjectPropertyChanged, this );
	GCallbackEvent->Register( CALLBACK_TexturePreSave, this );
	GCallbackEvent->Register( CALLBACK_PackageSaved, this );
	GCallbackEvent->Register( CALLBACK_ViewportResized, this);
	GCallbackEvent->Register( CALLBACK_Undo, this );
}



/** Destructor */
FMeshPaintWindow::~FMeshPaintWindow()
{
	// Unregister callbacks
	GCallbackEvent->UnregisterAll( this );

	// @todo WPF: This is probably redundant, but I'm still not sure if AutoGCRoot destructor will get
	//   called when native code destroys an object that has a non-virtual (or no) destructor

	// Dispose of WindowControl
	MeshPaintFrame.reset();
	MeshPaintPanel.reset();
}



/** Initialize the mesh paint window */
UBOOL FMeshPaintWindow::InitMeshPaintWindow( FEdModeMeshPaint* InMeshPaintSystem )
{
	WPFFrameInitStruct^ Settings = gcnew WPFFrameInitStruct;
	Settings->WindowTitle = CLRTools::LocalizeString( "MeshPaintWindow_WindowTitle" );
	Settings->bShowCloseButton = TRUE;

	MeshPaintFrame = gcnew MMeshPaintFrame(InMeshPaintSystem, GApp->EditorFrame, Settings, TEXT("MeshPaint"));

	PaintColorStruct.FLOATColorArray.AddItem(&(FMeshPaintSettings::Get().PaintColor));
	EraseColorStruct.FLOATColorArray.AddItem(&(FMeshPaintSettings::Get().EraseColor));
	MeshPaintPanel = gcnew MMeshPaintWindow(MeshPaintFrame.get(), InMeshPaintSystem, CLRTools::ToString(TEXT("MeshPaintWindow.xaml")), PaintColorStruct, EraseColorStruct);

	MeshPaintFrame->SetContentAndShow(MeshPaintPanel.get());

	return TRUE;
}



/** Refresh all properties */
void FMeshPaintWindow::RefreshAllProperties()
{
	MeshPaintPanel->RefreshTextureTargetsList();
	MeshPaintPanel->UpdateUVChannels();
	MeshPaintPanel->RefreshAllProperties();
}

/** Returns true if the mouse cursor is over the mesh paint window */
UBOOL FMeshPaintWindow::IsMouseOverWindow()
{
	if( MeshPaintPanel.get() != nullptr )
	{
		FrameworkElement^ WindowContentElement = safe_cast<FrameworkElement^>( MeshPaintFrame->GetRootVisual() );
		if( WindowContentElement->IsMouseOver )
		{
			return TRUE;
		}
	}

	return FALSE;
}

/** FCallbackEventDevice: Called when a parameterless global event we've registered for is fired */
void FMeshPaintWindow::Send( ECallbackEventType Event )
{
	if( MeshPaintFrame.get() != nullptr )
	{
		FrameworkElement^ WindowContentElement = safe_cast<FrameworkElement^>( MeshPaintFrame->GetRootVisual() );

		switch ( Event )
		{
			case CALLBACK_EditorPreModal:
				WindowContentElement->IsEnabled = false;
				break;

			case CALLBACK_EditorPostModal:
				WindowContentElement->IsEnabled = true;
				break;

			case CALLBACK_ObjectPropertyChanged:
				RefreshAllProperties();
				break;

			case CALLBACK_Undo:
				MeshPaintPanel->SaveSettingsForSelectedActors();
				break;

			default:
				break;

		}
	}
}


/** FCallbackEventDevice: Called when a global event we've registered for is fired with an attached object*/
void FMeshPaintWindow::Send( ECallbackEventType Event, UObject* EventObject )
{
	if( MeshPaintFrame.get() != nullptr )
	{
		switch ( Event )
		{

		case CALLBACK_TexturePreSave:
			MeshPaintPanel->CommitPaintChanges(FALSE);
			break;

		default:
			break;

		}
	}
}

void FMeshPaintWindow::Send( ECallbackEventType Event, class FViewport* EventViewport, UINT InMessage)
{
	if( MeshPaintFrame.get() != nullptr )
	{
		switch ( Event )
		{
		case CALLBACK_ViewportResized:
			MeshPaintPanel->RestoreRenderTargets();
			break;
		default:
			break;
		}

	}
}

/**
 * Notifies all observers that are registered for this event type
 * that the event has fired
 *
 * @param InType the event that was fired
 * @param InString the string information associated with this event
 * @param InObject the object associated with this event
 */
void FMeshPaintWindow::Send(ECallbackEventType InType,const FString& InString, UObject* InObject)
{
	if( MeshPaintFrame.get() != nullptr )
	{
		switch ( InType )
		{
		case CALLBACK_PackageSaved:
			RefreshAllProperties();
			break;
		default:
			break;
		}
	}
}

/** Called from edit mode when actor selection is changed. */
void FMeshPaintWindow::RefreshTextureTargetList()
{
	MeshPaintPanel->RefreshTextureTargetsList();
}

/** Called from edit mode after painting to ensure texture properties are up to date. */
void FMeshPaintWindow::RefreshTextureTargetListProperties()
{
	MeshPaintPanel->RefreshTextureTargetListProperties();
}

/** Called from edit mode when the transaction buffer size grows too large. */
void FMeshPaintWindow::TransactionBufferSizeBreech(bool bIsBreeched)
{
	MeshPaintPanel->IsBreechingUndoBuffer = bIsBreeched;
}
