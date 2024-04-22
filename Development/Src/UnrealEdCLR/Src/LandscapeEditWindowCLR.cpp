/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEdCLR.h"
#include "ConvertersCLR.h"
#include "ManagedCodeSupportCLR.h"
#include "LandscapeEditWindowShared.h"
#include "WPFWindowWrapperCLR.h"
#include "WPFFrameCLR.h"
#include "ThumbnailToolsCLR.h"

using namespace System::Windows::Controls::Primitives;
using namespace System::Windows::Media::Imaging;
using namespace System::Windows::Input;
using namespace System::Deployment;
using namespace System::ComponentModel;
using namespace WPF_Landscape;

#pragma unmanaged
#include "LandscapeEdMode.h"
#if WITH_EDITOR
#include "LandscapeRender.h"
#endif
#pragma managed

#define DECLARE_LANDSCAPE_PROPERTY( InPropertyType, InPropertyName ) \
		property InPropertyType InPropertyName \
		{ \
		InPropertyType get() { return LandscapeEditSystem->UISettings.Get##InPropertyName(); } \
			void set( InPropertyType Value ) \
			{ \
				if( LandscapeEditSystem->UISettings.Get##InPropertyName() != Value ) \
				{ \
					LandscapeEditSystem->UISettings.Set##InPropertyName(Value); \
				} \
			} \
		}

#define DECLARE_ENUM_LANDSCAPE_PROPERTY( InPropertyType, InPropertyName, EnumType, EnumVariable, EnumValue ) \
		property InPropertyType InPropertyName \
		{ \
			InPropertyType get() { return (LandscapeEditSystem->UISettings.Get##EnumVariable() == EnumValue ? true : false); } \
			void set( InPropertyType Value ) \
			{ \
				if( Value != InPropertyName ) \
				{ \
					LandscapeEditSystem->UISettings.Set##EnumVariable( (Value == true ? EnumValue : (EnumType)-1) ); \
				} \
			} \
		}

#define DECLARE_ENUM_NOTIFY_LANDSCAPE_PROPERTY( InPropertyType, InPropertyName, EnumType, EnumVariable, EnumValue ) \
		property InPropertyType InPropertyName \
		{ \
			InPropertyType get() { return (LandscapeEditSystem->UISettings.Get##EnumVariable() == EnumValue ? true : false); } \
			void set( InPropertyType Value ) \
			{ \
				if( Value != InPropertyName ) \
				{ \
					LandscapeEditSystem->UISettings.Set##EnumVariable( (Value == true ? EnumValue : (EnumType)-1) ); \
					OnPropertyChanged( #EnumVariable ); \
				} \
			} \
		}


ref class MLandscapeTargetListWrapper : public INotifyPropertyChanged
{
	int index;
	FLandscapeTargetListInfo& targetinfo;
	BitmapSource^ bitmap;
public:
	virtual event PropertyChangedEventHandler^ PropertyChanged;

	MLandscapeTargetListWrapper( INT InIndex, FLandscapeTargetListInfo& InTargetInfo )
	:	index(InIndex)
	,	targetinfo(InTargetInfo)
	{
		switch( targetinfo.TargetType )
		{
		case LET_Heightmap:
			bitmap = gcnew BitmapImage( gcnew Uri( gcnew System::String(*FString::Printf(TEXT("%swxres\\Landscape_Target_Heightmap.png"), *GetEditorResourcesDir())), UriKind::Absolute) );
			break;
		case LET_Weightmap:
			bitmap = ThumbnailToolsCLR::GetBitmapSourceForObject(targetinfo.LayerInfo->ThumbnailMIC);
			break;
		}	
	}

	// Accessors for remove event
	ELandscapeToolTargetType GetTargetType()
	{
		return targetinfo.TargetType;
	}
	FName GetLayerName()
	{
		return (targetinfo.LayerInfo && targetinfo.LayerInfo->LayerInfoObj) ? targetinfo.LayerInfo->LayerInfoObj->LayerName : NAME_None;
	}
	// To lookup layer debug color channel
	property int Index { int get() { return index; } }
	property int DebugColor { int get() { return targetinfo.LayerInfo ? targetinfo.LayerInfo->DebugColorChannel : 0; } }

	// Properties for binding
	property BitmapSource^ Bitmap   { BitmapSource^ get() { return bitmap; } }
	property String^ TargetName		{ String^ get() { return CLRTools::ToString(targetinfo.TargetName); } }
	property float Hardness			
	{ 
		float get() { return (targetinfo.LayerInfo && targetinfo.LayerInfo->LayerInfoObj) ? targetinfo.LayerInfo->LayerInfoObj->Hardness : 0.f; }			
		void set(float value) { if (targetinfo.LayerInfo && targetinfo.LayerInfo->LayerInfoObj) { targetinfo.LayerInfo->LayerInfoObj->Hardness = Clamp<FLOAT>(value, 0.f, 1.f); } } 
	}
	property bool IsNoWeightBlend	
	{
		bool get() { return targetinfo.LayerInfo && targetinfo.LayerInfo->LayerInfoObj && targetinfo.LayerInfo->LayerInfoObj->bNoWeightBlend != 0; }		
		void set(bool  value) { if (targetinfo.LayerInfo && targetinfo.LayerInfo->LayerInfoObj) { targetinfo.LayerInfo->LayerInfoObj->bNoWeightBlend = value; } } 
	}
	property bool IsSelected		
	{ 
		bool get() { return targetinfo.bSelected != 0; }
		void set(bool  value) { targetinfo.bSelected = value; if (targetinfo.LayerInfo) { targetinfo.LayerInfo->bSelected = value; } PropertyChanged(this, gcnew PropertyChangedEventArgs("IsSelected")); } 
	}
	property String^ PhysMaterial	
	{ 
		String^ get()
		{
			return targetinfo.LayerInfo && targetinfo.LayerInfo->LayerInfoObj && targetinfo.LayerInfo->LayerInfoObj->PhysMaterial
				? CLRTools::ToString(targetinfo.LayerInfo->LayerInfoObj->PhysMaterial->GetPathName()) : gcnew String("None");
		}
		void set(String^ value)
		{
			if( targetinfo.LayerInfo && targetinfo.LayerInfo->LayerInfoObj )
			{ 
				targetinfo.LayerInfo->LayerInfoObj->PhysMaterial = FindObject<UPhysicalMaterial>(ANY_PACKAGE,*CLRTools::ToFString(value),FALSE);
				PropertyChanged(this, gcnew PropertyChangedEventArgs("PhysMaterial"));
			} 
		}
	}

	property String^ SourceFilePath
	{
		String^ get()
		{
			return (
				targetinfo.TargetType == LET_Heightmap ? 
				(targetinfo.LandscapeInfo && targetinfo.LandscapeInfo->HeightmapFilePath.Len() ? CLRTools::ToString(*targetinfo.LandscapeInfo->HeightmapFilePath) : gcnew String("None"))
				: (targetinfo.LayerInfo && targetinfo.LayerInfo->LayerInfoObj && targetinfo.LayerInfo->SourceFilePath.Len() ? CLRTools::ToString(*targetinfo.LayerInfo->SourceFilePath) : gcnew String("None"))
				);
		}
		void set(String^ value)
		{
			if (targetinfo.TargetType == LET_Heightmap )
			{
				if (targetinfo.LandscapeInfo)
				{
					targetinfo.LandscapeInfo->HeightmapFilePath = CLRTools::ToFString(value);
					PropertyChanged(this, gcnew PropertyChangedEventArgs("SourceFilePath"));
				}
			}
			else if( targetinfo.LayerInfo && targetinfo.LayerInfo->LayerInfoObj )
			{ 
				targetinfo.LayerInfo->SourceFilePath = CLRTools::ToFString(value);
				PropertyChanged(this, gcnew PropertyChangedEventArgs("SourceFilePath"));
			} 
		}
	}

	property bool ViewmodeR	{ bool get() { return targetinfo.LayerInfo && (targetinfo.LayerInfo->DebugColorChannel & 1); }	void set(bool value) { if(targetinfo.LayerInfo) { if( value ) { targetinfo.LayerInfo->DebugColorChannel = 1; } else { targetinfo.LayerInfo->DebugColorChannel = 0; } PropertyChanged(this, gcnew PropertyChangedEventArgs("Viewmode")); } } }
	property bool ViewmodeG	{ bool get() { return targetinfo.LayerInfo && (targetinfo.LayerInfo->DebugColorChannel & 2); }	void set(bool value) { if(targetinfo.LayerInfo) { if( value ) { targetinfo.LayerInfo->DebugColorChannel = 2; } else { targetinfo.LayerInfo->DebugColorChannel = 0; } PropertyChanged(this, gcnew PropertyChangedEventArgs("Viewmode")); } } }
	property bool ViewmodeB	{ bool get() { return targetinfo.LayerInfo && (targetinfo.LayerInfo->DebugColorChannel & 4); }	void set(bool value) { if(targetinfo.LayerInfo) { if( value ) { targetinfo.LayerInfo->DebugColorChannel = 4; } else { targetinfo.LayerInfo->DebugColorChannel = 0; } PropertyChanged(this, gcnew PropertyChangedEventArgs("Viewmode")); } } }
	property bool ViewmodeNone { bool get() { return targetinfo.LayerInfo && targetinfo.LayerInfo->DebugColorChannel==0; }	void set(bool value) { if(targetinfo.LayerInfo) { if( value ) { targetinfo.LayerInfo->DebugColorChannel = 0; } } } }

};

typedef MEnumerableTArrayWrapper<MLandscapeTargetListWrapper,FLandscapeTargetListInfo> MLandscapeTargets;

ref class MLandscapeListWrapper : public INotifyPropertyChanged
{
	int index;
	FLandscapeListInfo& landscapeinfo;
public:
	//virtual event NotifyCollectionChangedEventHandler^ CollectionChanged;
	virtual event PropertyChangedEventHandler^ PropertyChanged;

	MLandscapeListWrapper( INT InIndex, FLandscapeListInfo& InInfo )
		:	index(InIndex)
		,	landscapeinfo(InInfo)
	{
	}
	// Properties for binding
	property String^ LandscapeName		{ String^ get() { return CLRTools::ToString(landscapeinfo.LandscapeName); } }
	property String^ LandscapeGuid		{ String^ get() { return CLRTools::ToString(landscapeinfo.Guid.String()); } }
	property String^ LandscapeData		{ String^ get() { return CLRTools::ToString(landscapeinfo.LandscapeName + 
			FString::Printf(TEXT(" - #Comps:%d/%d, #CompQuad:%d, #Subsect:%d"), landscapeinfo.Info ? landscapeinfo.Info->XYtoComponentMap.Num() : 0, 
								landscapeinfo.Info ? landscapeinfo.Info->XYtoCollisionComponentMap.Num() : 0, landscapeinfo.ComponentQuads, landscapeinfo.NumSubsections ) ); } }
	property String^ LandscapeCompNum	{ String^ get() { return CLRTools::ToString(FString::Printf(TEXT("%d"), landscapeinfo.Info ? landscapeinfo.Info->XYtoComponentMap.Num() : 0)); } }
};

typedef MEnumerableTArrayWrapper<MLandscapeListWrapper,FLandscapeListInfo> MLandscapeLists;

/**
 * MGizmoHistoryWrapper: Managed wrapper for FGizmoHistory
 */
ref class MGizmoHistoryWrapper : public INotifyPropertyChanged
{
	int index;
	FGizmoHistory& gizmo;
public:
	MGizmoHistoryWrapper( INT InIndex, FGizmoHistory& InGizmo )
	:	index(InIndex)
	,	gizmo(InGizmo)
	{
	}

	virtual event PropertyChangedEventHandler^ PropertyChanged;

	ALandscapeGizmoActor* GetGizmo() { return gizmo.Gizmo; }
	property int Index { int get() { return index; } }
	
	// Expose native mesh struct variables as properties
	property String^ GizmoName { 
		String^ get() 
		{ 
			// strip package name
			INT i = gizmo.GizmoName.InStr(TEXT("."), TRUE);
			return CLRTools::ToString(gizmo.GizmoName.Mid(i+1)); 
		} 
	}
};

typedef MEnumerableTArrayWrapper<MGizmoHistoryWrapper,FGizmoHistory> MGizmoHistories;

ref class MGizmoDataWrapper : public INotifyPropertyChanged
{
	int index;
	FGizmoData& data;
	BitmapSource^ bitmap;
public:
	MGizmoDataWrapper( INT InIndex, FGizmoData& InData )
		:	index(InIndex)
		,	data(InData)
	{
	}

	virtual event PropertyChangedEventHandler^ PropertyChanged;

	property int Index { int get() { return index; } }
};

typedef MEnumerableTArrayWrapper<MGizmoDataWrapper,FGizmoData> MGizmoDataList;

/**
 * MGizmoImportLayerWrapper: Managed wrapper for FGizmoImportLayer
 */
ref class MGizmoImportLayerWrapper : public INotifyPropertyChanged
{
	int index;
	FGizmoImportLayer& layer;
public:
	MGizmoImportLayerWrapper( INT InIndex, FGizmoImportLayer& InLayer )
	:	index(InIndex)
	,	layer(InLayer)
	{
	}

	virtual event PropertyChangedEventHandler^ PropertyChanged;

	property int Index { int get() { return index; } }
	
	// Expose native mesh struct variables as properties
	property String^ LayerFilename { 
		String^ get() 
		{ 
			return CLRTools::ToString(*layer.LayerFilename); 
		} 
		void set(String^ value)
		{
			layer.LayerFilename = CLRTools::ToFString(value);
		}
	}

	property String^ LayerName { 
		String^ get() 
		{ 
			return CLRTools::ToString(*layer.LayerName); 
		} 
		void set(String^ value)
		{
			layer.LayerName = CLRTools::ToFString(value);
		}
	}

	property bool NoImport { 
		bool get() 
		{ 
			return layer.bNoImport ? true : false; 
		}
		void set(bool value)
		{
			layer.bNoImport = value;
		}
	}
};

typedef MEnumerableTArrayWrapper<MGizmoImportLayerWrapper,FGizmoImportLayer> MGizmoImportLayers;


/**
 * Landscape Edit window control (managed)
 */
ref class MLandscapeEditWindow
	: public MWPFWindowWrapper,
	  public INotifyPropertyChanged
{

private:
	Button^ ImportButton;
	Button^ GizmoLayerRemoveButton;
	Button^ GizmoImportButton;
	Button^ ChangeComponentSizeButton;
	Button^ UpdateLODBiasButton;
	CheckBox^ MaskEnableCheckBox;
	CheckBox^ InvertMaskCheckBox;

	// Landscapes
	//DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(List<String^>^, Landscapes);
	MLandscapeLists^ LandscapeListsValue;
	ComboBox^ LandscapeComboBox;

	Label^ LandscapeCompNum;
	Label^ CollisionCompNum;
	Label^ CompQuadNum;
	Label^ QuadPerSection;
	Label^ SubsectionNum;

	TextBlock^ CurrentWidth;
	TextBlock^ CurrentHeight;

	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(INT, CurrentLandscapeIndex)
	DECLARE_MAPPED_NOTIFY_PROPERTY(MLandscapeLists^, LandscapeListsProperty, MLandscapeLists^, LandscapeListsValue);

	// Import
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(String^, HeightmapFileNameString);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(INT, Width);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(INT, Height);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(INT, SectionSize);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(INT, NumSections);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(INT, TotalComponents);
	LandscapeImportLayers^ LandscapeImportLayersValue;
	DECLARE_MAPPED_NOTIFY_PROPERTY(LandscapeImportLayers^, LandscapeImportLayersProperty, LandscapeImportLayers^, LandscapeImportLayersValue)

	// Convert
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(INT, ConvertWidth);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(INT, ConvertHeight);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(INT, ConvertSectionSize);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(INT, ConvertNumSections);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(INT, ConvertTotalComponents);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(INT, ConvertCompQuadNum);

	// Gizmo Import
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(String^, GizmoHeightmapFileNameString);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(INT, GizmoImportWidth);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(INT, GizmoImportHeight);
	MGizmoImportLayers^ GizmoImportLayersValue;
	DECLARE_MAPPED_NOTIFY_PROPERTY(MGizmoImportLayers^, GizmoImportLayersProperty, MGizmoImportLayers^, GizmoImportLayersValue);

	// Export
	UBOOL bExportLayers;
	DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY( ExportLayers, bExportLayers );

	// Targets
	MLandscapeTargets^ LandscapeTargetsValue;
	ListBox^ TargetListBox;
	DECLARE_MAPPED_NOTIFY_PROPERTY(MLandscapeTargets^, LandscapeTargetsProperty, MLandscapeTargets^, LandscapeTargetsValue);

	// Gizmo
	ListBox^ GizmoListBox;
	MGizmoHistories^ GizmoHistoriesValue;
	DECLARE_MAPPED_NOTIFY_PROPERTY(MGizmoHistories^, GizmoHistoriesProperty, MGizmoHistories^, GizmoHistoriesValue);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(INT, CurrentGizmoIndex);

	ListBox^ GizmoDataListBox;
	MGizmoDataList^ GizmoDataValue;
	DECLARE_MAPPED_NOTIFY_PROPERTY(MGizmoDataList^, GizmoDataProperty, MGizmoDataList^, GizmoDataValue);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(INT, CurrentGizmoDataIndex);

	// Add/Remove...
	UBOOL bNoBlending;
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(String^, AddLayerNameString);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE(FLOAT, Hardness);
	DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY( NoBlending, bNoBlending );

	// Tools/Brushes
	DECLARE_LANDSCAPE_PROPERTY(FLOAT, ToolStrength);
	DECLARE_LANDSCAPE_PROPERTY(FLOAT, WeightTargetValue);
	DECLARE_LANDSCAPE_PROPERTY(UBOOL, bUseWeightTargetValue);

	CustomControls::DragSlider^ BrushRadiusSlider;
	DECLARE_LANDSCAPE_PROPERTY(FLOAT, BrushRadius);
	CustomControls::DragSlider^ BrushSizeSlider;
	DECLARE_LANDSCAPE_PROPERTY(FLOAT, BrushComponentSize);
	DECLARE_LANDSCAPE_PROPERTY(FLOAT, BrushFalloff);
	DECLARE_LANDSCAPE_PROPERTY(UBOOL, bUseClayBrush);
	DECLARE_LANDSCAPE_PROPERTY(FLOAT, AlphaBrushScale);
	DECLARE_LANDSCAPE_PROPERTY(FLOAT, AlphaBrushRotation);
	DECLARE_LANDSCAPE_PROPERTY(FLOAT, AlphaBrushPanU);
	DECLARE_LANDSCAPE_PROPERTY(FLOAT, AlphaBrushPanV);
	Button^ AlphaTextureUseRButton;
	Button^ AlphaTextureUseGButton;
	Button^ AlphaTextureUseBButton;
	Button^ AlphaTextureUseAButton;
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( BitmapSource^, AlphaTexture );
	DECLARE_LANDSCAPE_PROPERTY(FLOAT, bSmoothGizmoBrush);

	// Flatten Tool
	DECLARE_ENUM_LANDSCAPE_PROPERTY( bool, IsFlattenModeBoth, ELandscapeToolNoiseMode::Type, FlattenMode, ELandscapeToolNoiseMode::Both );
	DECLARE_ENUM_LANDSCAPE_PROPERTY( bool, IsFlattenModeAdd, ELandscapeToolNoiseMode::Type, FlattenMode, ELandscapeToolNoiseMode::Add );
	DECLARE_ENUM_LANDSCAPE_PROPERTY( bool, IsFlattenModeSub, ELandscapeToolNoiseMode::Type, FlattenMode, ELandscapeToolNoiseMode::Sub );
	DECLARE_LANDSCAPE_PROPERTY(UBOOL, bUseSlopeFlatten);
	DECLARE_LANDSCAPE_PROPERTY(UBOOL, bPickValuePerApply);
	// Erode Tool
	DECLARE_LANDSCAPE_PROPERTY(INT, ErodeThresh);
	DECLARE_LANDSCAPE_PROPERTY(INT, ErodeIterationNum);
	DECLARE_LANDSCAPE_PROPERTY(INT, ErodeSurfaceThickness);
	DECLARE_ENUM_LANDSCAPE_PROPERTY( bool, IsErosionNoiseModeBoth, ELandscapeToolNoiseMode::Type, ErosionNoiseMode, ELandscapeToolNoiseMode::Both );
	DECLARE_ENUM_LANDSCAPE_PROPERTY( bool, IsErosionNoiseModeAdd, ELandscapeToolNoiseMode::Type, ErosionNoiseMode, ELandscapeToolNoiseMode::Add );
	DECLARE_ENUM_LANDSCAPE_PROPERTY( bool, IsErosionNoiseModeSub, ELandscapeToolNoiseMode::Type, ErosionNoiseMode, ELandscapeToolNoiseMode::Sub );
	DECLARE_LANDSCAPE_PROPERTY(FLOAT, ErosionNoiseScale);
	// Hydra Erosion Tool
	DECLARE_LANDSCAPE_PROPERTY(INT, RainAmount);
	DECLARE_LANDSCAPE_PROPERTY(FLOAT, SedimentCapacity);
	DECLARE_LANDSCAPE_PROPERTY(INT, HErodeIterationNum);
	DECLARE_ENUM_LANDSCAPE_PROPERTY( bool, IsRainDistModeBoth, ELandscapeToolNoiseMode::Type, RainDistMode, ELandscapeToolNoiseMode::Both );
	DECLARE_ENUM_LANDSCAPE_PROPERTY( bool, IsRainDistModeAdd, ELandscapeToolNoiseMode::Type, RainDistMode, ELandscapeToolNoiseMode::Add );
	DECLARE_LANDSCAPE_PROPERTY(FLOAT, RainDistScale);
	DECLARE_LANDSCAPE_PROPERTY(UBOOL, bHErosionDetailSmooth);
	DECLARE_LANDSCAPE_PROPERTY(FLOAT, HErosionDetailScale);
	// Noise Tool
	DECLARE_ENUM_LANDSCAPE_PROPERTY( bool, IsNoiseModeBoth, ELandscapeToolNoiseMode::Type, NoiseMode, ELandscapeToolNoiseMode::Both );
	DECLARE_ENUM_LANDSCAPE_PROPERTY( bool, IsNoiseModeAdd, ELandscapeToolNoiseMode::Type, NoiseMode, ELandscapeToolNoiseMode::Add );
	DECLARE_ENUM_LANDSCAPE_PROPERTY( bool, IsNoiseModeSub, ELandscapeToolNoiseMode::Type, NoiseMode, ELandscapeToolNoiseMode::Sub );
	DECLARE_LANDSCAPE_PROPERTY(FLOAT, NoiseScale);
	// Detail Preserving Smooth
	DECLARE_LANDSCAPE_PROPERTY(UBOOL, bDetailSmooth);
	DECLARE_LANDSCAPE_PROPERTY(FLOAT, DetailScale);
	// Copy/Paste Tool
	DECLARE_ENUM_LANDSCAPE_PROPERTY( bool, IsPasteModeBoth, ELandscapeToolNoiseMode::Type, PasteMode, ELandscapeToolNoiseMode::Both );
	DECLARE_ENUM_LANDSCAPE_PROPERTY( bool, IsPasteModeAdd, ELandscapeToolNoiseMode::Type, PasteMode, ELandscapeToolNoiseMode::Add );
	DECLARE_ENUM_LANDSCAPE_PROPERTY( bool, IsPasteModeSub, ELandscapeToolNoiseMode::Type, PasteMode, ELandscapeToolNoiseMode::Sub );

	// Region
	DECLARE_LANDSCAPE_PROPERTY(UBOOL, bUseSelectedRegion);
	DECLARE_LANDSCAPE_PROPERTY(UBOOL, bUseNegativeMask);
	DECLARE_LANDSCAPE_PROPERTY(UBOOL, bMaskEnabled);

	// Others
	DECLARE_LANDSCAPE_PROPERTY(UBOOL, bApplyToAllTargets);

	// ViewMode
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsViewModeNormal, ELandscapeViewMode::Type, GLandscapeViewMode, ELandscapeViewMode::Normal );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsViewModeEditLayer, ELandscapeViewMode::Type, GLandscapeViewMode, ELandscapeViewMode::EditLayer );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsViewModeDebugLayer, ELandscapeViewMode::Type, GLandscapeViewMode, ELandscapeViewMode::DebugLayer );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsViewModeLayerDensity, ELandscapeViewMode::Type, GLandscapeViewMode, ELandscapeViewMode::LayerDensity );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsViewModeLOD, ELandscapeViewMode::Type, GLandscapeViewMode, ELandscapeViewMode::LOD );

	// ConvertMode
	DECLARE_ENUM_NOTIFY_LANDSCAPE_PROPERTY( bool, IsConvertModeExpand, ELandscapeConvertMode::Type, ConvertMode, ELandscapeConvertMode::Expand );
	DECLARE_ENUM_NOTIFY_LANDSCAPE_PROPERTY( bool, IsConvertModeClip, ELandscapeConvertMode::Type, ConvertMode, ELandscapeConvertMode::Clip );

	// LOD Bias
	CustomControls::DragSlider^ LODBiasThresholdSlider;
	DECLARE_LANDSCAPE_PROPERTY(FLOAT, LODBiasThreshold);

	INT HeightmapFileSize;
	INT GizmoFileSize;
	String^ LastImportPath;

public:

	// tor
	MLandscapeEditWindow( FEdModeLandscape* InLandscapeEditSystem)
	:	LandscapeEditSystem(InLandscapeEditSystem)
	,	bExportLayers(FALSE)
	,	bNoBlending(FALSE)
	{
		check( InLandscapeEditSystem != NULL );
		Hardness = 0.5f;
	}

	/**
	 * Initialize the landscape edit window
	 *
	 * @param	InLandscapeEditSystem	Landscape edit system that owns us
	 * @param	InParentWindowHandle	Parent window handle
	 *
	 * @return	TRUE if successful
	 */
	UBOOL InitLandscapeEditWindow( const HWND InParentWindowHandle )
	{
		String^ WindowTitle = CLRTools::LocalizeString( "LandscapeEditWindow_WindowTitle" );
		String^ WPFXamlFileName = "LandscapeEditWindow.xaml";

		HeightmapFileSize = INDEX_NONE;
		GizmoFileSize = INDEX_NONE;
		LastImportPath = CLRTools::ToString(GApp->LastDir[LD_GENERIC_IMPORT]);


		// We draw our own title bar so tell the window about it's height
		const int FakeTitleBarHeight = 28;
		const UBOOL bIsTopMost = FALSE;


		// Read the saved size/position
		INT x,y,w,h;
		LandscapeEditSystem->UISettings.GetWindowSizePos(x,y,w,h);

		// If we don't have an initial position yet then default to centering the new window
		bool bCenterWindow = x == -1 || y == -1;

		// Call parent implementation's init function to create the actual window
		if( !InitWindow( InParentWindowHandle,
						 WindowTitle,
						 WPFXamlFileName,
						 x,
						 y,
						 w,
						 h,
						 bCenterWindow,
						 FakeTitleBarHeight,
						 bIsTopMost ) )
		{
			return FALSE;
		}

		// Reset the active landscape in the EdMode so the initial combo box setting propagates to the layers correctly.
		LandscapeEditSystem->CurrentToolTarget.LandscapeInfo = NULL;


		// Register for property change callbacks from our properties object
		this->PropertyChanged += gcnew PropertyChangedEventHandler( this, &MLandscapeEditWindow::OnLandscapeEditPropertyChanged );

		// Setup bindings
		Visual^ RootVisual = InteropWindow->RootVisual;

		FrameworkElement^ WindowContentElement = safe_cast<FrameworkElement^>(RootVisual);
		WindowContentElement->DataContext = this;

		// Close button
		Button^ CloseButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "TitleBarCloseButton" ) );
		CloseButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::OnClose );	
		FakeTitleBarButtonWidth += CloseButton->ActualWidth;

		// Lookup any items we hold onto first, to avoid crashes in OnPropertyChanged
		ImportButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ImportButton" ) );
		GizmoImportButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "GizmoImportButton" ) );
		ChangeComponentSizeButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ChangeComponentSizeButton" ) );
		UpdateLODBiasButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "UpdateLODBiasButton" ) );

		//
		// Editing items
		//
		LandscapeComboBox = safe_cast< ComboBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "LandscapeCombo" ) );
		LandscapeComboBox->SelectionChanged += gcnew SelectionChangedEventHandler( this, &MLandscapeEditWindow::OnLandscapeListSelectionChanged );
		LandscapeListsValue = gcnew MLandscapeLists(LandscapeEditSystem->GetLandscapeList());

		LandscapeCompNum = safe_cast< Label^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "LandscapeCompNum" ) );
		CollisionCompNum = safe_cast< Label^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "CollisionCompNum" ) );
		CompQuadNum = safe_cast< Label^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "CompQuadNum" ) );
		QuadPerSection = safe_cast< Label^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "QuadPerSection" ) );
		SubsectionNum = safe_cast< Label^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "SubsectionNum" ) );

		UnrealEd::Utils::CreateBinding(LandscapeComboBox, ComboBox::ItemsSourceProperty, this, "LandscapeListsProperty");
		//UnrealEd::Utils::CreateBinding(LandscapeComboBox, ListBox::SelectedIndexProperty, this, "CurrentLandscapeIndex", gcnew IntToIntOffsetConverter( 0 ) );

		// To add 'Tool' buttons
		//UniformGrid^ ToolGrid = safe_cast< UniformGrid^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ToolGrid" ) );
		//ToolGrid->Columns = 4;
		for( INT ToolIdx=0;ToolIdx<LandscapeEditSystem->LandscapeToolSets.Num();ToolIdx++ )
		{
			FLandscapeToolSet* ToolSet = LandscapeEditSystem->LandscapeToolSets(ToolIdx);

			CustomControls::ImageRadioButton^ btn = safe_cast< CustomControls::ImageRadioButton^ >(LogicalTreeHelper::FindLogicalNode( RootVisual, gcnew System::String(ToolSet->GetToolSetName())) );
			//ToolGrid->Children->Add(btn);
			btn->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::ToolButton_Click );
			BitmapImage^ ImageChecked = gcnew BitmapImage( gcnew Uri( gcnew System::String(*FString::Printf(TEXT("%swxres\\Landscape_Tool_%s_active.png"), *GetEditorResourcesDir(),
				LandscapeEditSystem->LandscapeToolSets(ToolIdx)->GetIconString())), UriKind::Absolute) );
			BitmapImage^ ImageUnchecked = gcnew BitmapImage( gcnew Uri( gcnew System::String(*FString::Printf(TEXT("%swxres\\Landscape_Tool_%s_inactive.png"), *GetEditorResourcesDir(),
				LandscapeEditSystem->LandscapeToolSets(ToolIdx)->GetIconString())), UriKind::Absolute) );
			btn->ToolTip = gcnew System::String(*LandscapeEditSystem->LandscapeToolSets(ToolIdx)->GetTooltipString());

			// Fallback in case this brush is not the currently selected one
			btn->CheckedImage = ImageChecked;
			btn->UncheckedImage = ImageUnchecked;
			if (LandscapeEditSystem->MoveToLevelToolIndex == ToolIdx)
			{
				BitmapImage^ ImageDisabled = gcnew BitmapImage( gcnew Uri( gcnew System::String(*FString::Printf(TEXT("%swxres\\Landscape_Tool_%s_disabled.png"), *GetEditorResourcesDir(),
					LandscapeEditSystem->LandscapeToolSets(ToolIdx)->GetIconString())), UriKind::Absolute) );
				btn->DisabledImage = ImageDisabled;
			}

			if( ToolSet == LandscapeEditSystem->CurrentToolSet )
			{
				btn->IsChecked = TRUE;
				LandscapeEditSystem->CurrentToolIndex = ToolIdx;
			}
		}
		// Tool properties
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ToolStrengthSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "ToolStrength" );

		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ToolWeightTargetValueSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "WeightTargetValue" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "UseWeightTargetValueCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "bUseWeightTargetValue" );

		// Flatten
		StackPanel^ FlattenPanel = safe_cast< StackPanel^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "FlattenPanel" ) );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "FlattenModeBoth" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsFlattenModeBoth" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "FlattenModeAdd" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsFlattenModeAdd" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "FlattenModeSub" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsFlattenModeSub" );
		//FlattenPanel->Visibility = Visibility::Collapsed;
		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "UseSlopeCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "bUseSlopeFlatten" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PickValuePerApplyCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "bPickValuePerApply" );

		// Erode Tool properties
		StackPanel^ ErosionPanel = safe_cast< StackPanel^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ErosionPanel" ) );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ErodeThreshSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "ErodeThresh" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ErodeIterationNumSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "ErodeIterationNum" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ErodeSurfaceThicknessSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "ErodeSurfaceThickness" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ErosionNoiseModeBoth" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsErosionNoiseModeBoth" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ErosionNoiseModeAdd" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsErosionNoiseModeAdd" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ErosionNoiseModeSub" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsErosionNoiseModeSub" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ErosionNoiseScaleSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "ErosionNoiseScale" );
		//ErosionPanel->Visibility = Visibility::Collapsed;

		// Hydraulic Erode Tool properties
		StackPanel^ HydraErosionPanel = safe_cast< StackPanel^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "HydraulicErosionPanel" ) );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "RainAmountSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "RainAmount" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "SedimentCapacitySlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "SedimentCapacity" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "HErodeIterationNumSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "HErodeIterationNum" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "RainDistModeBoth" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsRainDistModeBoth" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "RainDistModeAdd" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsRainDistModeAdd" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "RainDistScaleSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "RainDistScale" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "HErosoinDetailSmoothCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "bHErosionDetailSmooth" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^  >( LogicalTreeHelper::FindLogicalNode( RootVisual, "HErosionDetailScaleSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "HErosionDetailScale" );
		//HydraErosionPanel->Visibility = Visibility::Collapsed;

		// Noise Tool properties
		StackPanel^ NoisePanel = safe_cast< StackPanel^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "NoisePanel" ) );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "NoiseModeBoth" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsNoiseModeBoth" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "NoiseModeAdd" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsNoiseModeAdd" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "NoiseModeSub" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsNoiseModeSub" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "NoiseScaleSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "NoiseScale" );
		//NoisePanel->Visibility = Visibility::Collapsed;

		// Detail Preserving Smooth Tool properties
		StackPanel^ SmoothOptionPanel = safe_cast< StackPanel^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "SmoothOptionPanel" ) );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "DetailSmoothCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "bDetailSmooth" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^  >( LogicalTreeHelper::FindLogicalNode( RootVisual, "DetailScaleSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "DetailScale" );
		//SmoothOptionPanel->Visibility = Visibility::Collapsed;

		// Selection Tool
		Button^ ClearSelectionButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ClearSelectionButton" ) );
		ClearSelectionButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::ClearSelectionButton_Click );

		Button^ ClearMaskButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ClearMaskButton" ) );
		ClearMaskButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::ClearMaskButton_Click );

		// Copy/Paste Tool
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PasteModeBoth" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsPasteModeBoth" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PasteModeAdd" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsPasteModeAdd" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PasteModeSub" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsPasteModeSub" );

		// Gizmo
		Button^ ClearGizmoDataButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ClearGizmoDataButton" ) );
		ClearGizmoDataButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::ClearGizmoDataButton_Click );

		Button^ FitToSelectionButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "FitToSelectionButton" ) );
		FitToSelectionButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::FitToSelectionButton_Click );

		Button^ FitToGizmoButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "FitToGizmoButton" ) );
		FitToGizmoButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::FitToGizmoButton_Click );

		Button^ CopyToGizmoButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "CopyToGizmoButton" ) );
		CopyToGizmoButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::CopyToGizmoButton_Click );
		
		// Layer View Mode
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "EditmodeNone" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsViewModeNormal" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "EditmodeEdit" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsViewModeEditLayer" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "EditmodeDebugView" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsViewModeDebugLayer" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "EditmodeLayerDensity" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsViewModeLayerDensity" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "EditmodeLOD" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsViewModeLOD" );

		// Brushes
		UniformGrid^ BrushGrid = safe_cast< UniformGrid^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "BrushGrid" ) );

		for( INT BrushSetIdx=0;BrushSetIdx<LandscapeEditSystem->LandscapeBrushSets.Num();BrushSetIdx++ )
		{
			FLandscapeBrushSet& BrushSet = LandscapeEditSystem->LandscapeBrushSets(BrushSetIdx);

			CustomControls::ToolDropdownRadioButton^ btn = safe_cast< CustomControls::ToolDropdownRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, gcnew System::String(*BrushSet.BrushSetName)) );
			btn->ToolTip = gcnew System::String(*BrushSet.ToolTip);
			for( INT BrushIdx=0;BrushIdx < BrushSet.Brushes.Num();BrushIdx++ )
			{
				BitmapImage^ ImageUnchecked = gcnew BitmapImage( gcnew Uri( gcnew System::String(*FString::Printf(TEXT("%swxres\\Landscape_Brush_%s_inactive.png"), *GetEditorResourcesDir(), BrushSet.Brushes(BrushIdx)->GetIconString())), UriKind::Absolute) );
				BitmapImage^ ImageChecked = gcnew BitmapImage( gcnew Uri( gcnew System::String(*FString::Printf(TEXT("%swxres\\Landscape_Brush_%s_active.png"), *GetEditorResourcesDir(), BrushSet.Brushes(BrushIdx)->GetIconString())), UriKind::Absolute) );
				BitmapImage^ ImageDisabled = gcnew BitmapImage( gcnew Uri( gcnew System::String(*FString::Printf(TEXT("%swxres\\Landscape_Brush_%s_disabled.png"), *GetEditorResourcesDir(), BrushSet.Brushes(BrushIdx)->GetIconString())), UriKind::Absolute) );

				btn->ListItems->Add( gcnew CustomControls::ToolDropdownItem(ImageUnchecked, ImageChecked, ImageDisabled, gcnew System::String(*BrushSet.Brushes(BrushIdx)->GetTooltipString())) );
				btn->ToolSelectionChanged += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::BrushButton_SelectionChanged );

				// Fallback in case this brush is not the currently selected one
				if( BrushIdx == 0 )
				{
					btn->CheckedImage = ImageChecked;
					btn->UncheckedImage = ImageUnchecked;
					btn->DisabledImage = ImageDisabled;
					btn->SelectedIndex = 0;
				}

				if( BrushSet.Brushes(BrushIdx) == LandscapeEditSystem->CurrentBrush )
				{
					btn->IsChecked = TRUE;
					btn->CheckedImage = ImageChecked;
					btn->UncheckedImage = ImageUnchecked;
					btn->DisabledImage = ImageDisabled;
					btn->SelectedIndex = BrushIdx;
					LandscapeEditSystem->CurrentBrushIndex = BrushSetIdx;
				}
			}
		}

		// Brush properties
		BrushRadiusSlider = safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "BrushRadiusSlider" ) );
		UnrealEd::Utils::CreateBinding(
			BrushRadiusSlider,
			CustomControls::DragSlider::ValueProperty, this, "BrushRadius" );
		BrushSizeSlider = safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "BrushSizeSlider" ) );
		UnrealEd::Utils::CreateBinding(
			BrushSizeSlider,
			CustomControls::DragSlider::ValueProperty, this, "BrushComponentSize" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "BrushFalloffSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "BrushFalloff" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "UseClayBrushCheckbox" ) ),
			CheckBox::IsCheckedProperty, this, "bUseClayBrush" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "AlphaBrushScaleSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "AlphaBrushScale" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "AlphaBrushRotationSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "AlphaBrushRotation" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "AlphaBrushPanUSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "AlphaBrushPanU" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "AlphaBrushPanVSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "AlphaBrushPanV" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "SmoothGizmoBrushCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "bSmoothGizmoBrush" );

		AlphaTextureUseRButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "AlphaTextureUseRButton" ) );
		AlphaTextureUseRButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::AlphaTextureUseButton_Click );
		AlphaTextureUseGButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "AlphaTextureUseGButton" ) );
		AlphaTextureUseGButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::AlphaTextureUseButton_Click );
		AlphaTextureUseBButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "AlphaTextureUseBButton" ) );
		AlphaTextureUseBButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::AlphaTextureUseButton_Click );
		AlphaTextureUseAButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "AlphaTextureUseAButton" ) );
		AlphaTextureUseAButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::AlphaTextureUseButton_Click );
		AlphaTexture = MakeAlphaTextureBitmap();
		Button^ SyncContentBrowserButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "SyncContentBrowserButton" ) );
		SyncContentBrowserButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::SyncContentBrowserButton_Click );
		
		//
		// Target list
		//
		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ApplyAllCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "bApplyToAllTargets" );

		LandscapeTargetsValue = gcnew MLandscapeTargets(LandscapeEditSystem->GetTargetList());
		LandscapeTargetsValue->PropertyChanged += gcnew PropertyChangedEventHandler( this, &MLandscapeEditWindow::OnLandscapeTargetLayerPropertyChanged );

		TargetListBox = safe_cast< ListBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "TargetListBox" ) );
		UnrealEd::Utils::CreateBinding(TargetListBox, ListBox::ItemsSourceProperty, this, "LandscapeTargetsProperty");
		TargetListBox->AllowDrop = true;
		TargetListBox->DragOver += gcnew DragEventHandler( this, &MLandscapeEditWindow::OnDragOver );
		TargetListBox->Drop += gcnew DragEventHandler( this, &MLandscapeEditWindow::OnDrop );
		TargetListBox->DragEnter += gcnew DragEventHandler( this, &MLandscapeEditWindow::OnDragEnter );
		TargetListBox->DragLeave += gcnew DragEventHandler( this, &MLandscapeEditWindow::OnDragLeave );

		// Edit Target Layers...
		RoutedCommand^ EditTargetLayerCommand = safe_cast< RoutedCommand^ >(WindowContentElement->FindResource("LandscapeEditTargetLayerCommand"));
		WindowContentElement->CommandBindings->Add(
			gcnew CommandBinding(EditTargetLayerCommand, gcnew ExecutedRoutedEventHandler( this, &MLandscapeEditWindow::OnLandscapeTargetLayerChanged) ) );
		// Remove Layer
		RoutedCommand^ RemoveLayerCommand = safe_cast< RoutedCommand^ >(WindowContentElement->FindResource("LandscapeTargetLayerRemoveCommand"));
		WindowContentElement->CommandBindings->Add(
			gcnew CommandBinding(RemoveLayerCommand, gcnew ExecutedRoutedEventHandler( this, &MLandscapeEditWindow::TargetLayerRemoveButton_Click) ) );
		// Sync Layer
		RoutedCommand^ SyncLayerCommand = safe_cast< RoutedCommand^ >(WindowContentElement->FindResource("LandscapeTargetLayerSyncCommand"));
		WindowContentElement->CommandBindings->Add(
			gcnew CommandBinding(SyncLayerCommand, gcnew ExecutedRoutedEventHandler( this, &MLandscapeEditWindow::TargetLayerSyncButton_Click) ) );
		// Reimport Target
		RoutedCommand^ ReimportCommand = safe_cast< RoutedCommand^ >(WindowContentElement->FindResource("LandscapeTargetReimportCommand"));
		WindowContentElement->CommandBindings->Add(
			gcnew CommandBinding(ReimportCommand, gcnew ExecutedRoutedEventHandler( this, &MLandscapeEditWindow::TargetReimportButton_Click) ) );

		// Set Phys Material
		RoutedCommand^ SetPhysMaterialCommand = safe_cast< RoutedCommand^ >(WindowContentElement->FindResource("LandscapeTargetSetPhysMaterialCommand"));
		WindowContentElement->CommandBindings->Add(
			gcnew CommandBinding(SetPhysMaterialCommand, gcnew ExecutedRoutedEventHandler( this, &MLandscapeEditWindow::TargetLayerSetPhysMaterialButton_Click) ) );

		// Set SourceFilePath
		RoutedCommand^ SetFilePathCommand = safe_cast< RoutedCommand^ >(WindowContentElement->FindResource("LandscapeTargetSetFilePathCommand"));
		WindowContentElement->CommandBindings->Add(
			gcnew CommandBinding(SetFilePathCommand, gcnew ExecutedRoutedEventHandler( this, &MLandscapeEditWindow::TargetSetFilePathButton_Click) ) );

		// Gizmo list
		GizmoHistoriesValue = gcnew MGizmoHistories( &LandscapeEditSystem->UISettings.GetGizmoHistories() );
		GizmoListBox = safe_cast< ListBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "GizmoListBox" ) );
		UnrealEd::Utils::CreateBinding(GizmoListBox, ListBox::ItemsSourceProperty, this, "GizmoHistoriesProperty");
		UnrealEd::Utils::CreateBinding(GizmoListBox, ListBox::SelectedIndexProperty, this, "CurrentGizmoIndex", gcnew IntToIntOffsetConverter( 0 ) );

		RoutedCommand^ EditGizmoCommand = safe_cast< RoutedCommand^ >(WindowContentElement->FindResource("LandscapeEditGizmoCommand"));
		WindowContentElement->CommandBindings->Add(
			gcnew CommandBinding(EditGizmoCommand, gcnew ExecutedRoutedEventHandler( this, &MLandscapeEditWindow::OnLandscapeGizmoChanged) ) );

		GizmoDataValue = gcnew MGizmoDataList( &LandscapeEditSystem->UISettings.GetGizmoData() );
		GizmoDataListBox = safe_cast< ListBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "GizmoDataListBox" ) );
		UnrealEd::Utils::CreateBinding(GizmoDataListBox, ListBox::ItemsSourceProperty, this, "GizmoDataProperty");
		UnrealEd::Utils::CreateBinding(GizmoDataListBox, ListBox::SelectedIndexProperty, this, "CurrentGizmoDataIndex", gcnew IntToIntOffsetConverter( 0 ) );

		// Add Layer
		Button^ AddLayerButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "AddLayerButton" ) );
		AddLayerButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::AddLayerButton_Click );

		TextBox^ AddLayerNameTextBox = safe_cast< TextBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "AddLayerNameTextBox" ) );
		TextBox^ AddHardnessTextBox = safe_cast< TextBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "AddHardnessTextBox" ) );

		UnrealEd::Utils::CreateBinding(AddLayerNameTextBox, TextBox::TextProperty, this, "AddLayerNameString" );
		AddLayerNameString = ""; // this is to work around a strange crash.
		UnrealEd::Utils::CreateBinding(AddHardnessTextBox, TextBox::TextProperty, this, "Hardness" );

		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "AddNoBlendingCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "NoBlending" );

		//
		// Import items
		//
		Button^ HeightmapFileButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "HeightmapFileButton" ) );
		HeightmapFileButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::HeightmapFileButton_Click );

		TextBox^ HeightmapFileNameTextBox = safe_cast< TextBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "HeightmapFileNameTextBox" ) );
		UnrealEd::Utils::CreateBinding(HeightmapFileNameTextBox, TextBox::TextProperty, this, "HeightmapFileNameString" );
		TextBox^ WidthTextBox = safe_cast< TextBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "WidthTextBox" ) );
		UnrealEd::Utils::CreateBinding(WidthTextBox, TextBox::TextProperty, this, "Width" );
		TextBox^ HeightTextBox = safe_cast< TextBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "HeightTextBox" ) );
		UnrealEd::Utils::CreateBinding(HeightTextBox, TextBox::TextProperty, this, "Height" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< ComboBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "SectionSizeCombo" ) ),
			ComboBox::SelectedIndexProperty, this, "SectionSize", gcnew IntToIntOffsetConverter( 0 ) );
		UnrealEd::Utils::CreateBinding(
			safe_cast< ComboBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "NumSectionsCombo" ) ),
			ComboBox::SelectedIndexProperty, this, "NumSections", gcnew IntToIntOffsetConverter( 0 ) );
		UnrealEd::Utils::CreateBinding(
			safe_cast< TextBlock^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "TotalComponentsLabel" ) ),
			TextBlock::TextProperty, this, "TotalComponents" );

		LandscapeImportLayersValue = gcnew LandscapeImportLayers();
		ItemsControl^ ImportLayersItemsControl = safe_cast< ItemsControl^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ImportLayersItemsControl" ) );
		UnrealEd::Utils::CreateBinding(ImportLayersItemsControl, ItemsControl::ItemsSourceProperty, this, "LandscapeImportLayersProperty");

		RoutedCommand^ ImportLayersFilenameCommand = safe_cast< RoutedCommand^ >(WindowContentElement->FindResource("LandscapeImportLayersFilenameCommand"));
		WindowContentElement->CommandBindings->Add(
			gcnew CommandBinding(ImportLayersFilenameCommand, gcnew ExecutedRoutedEventHandler( this, &MLandscapeEditWindow::LayerFileButton_Click) ) );

		RoutedCommand^ ImportLayersRemoveCommand = safe_cast< RoutedCommand^ >(WindowContentElement->FindResource("LandscapeImportLayersRemoveCommand"));
		WindowContentElement->CommandBindings->Add(
			gcnew CommandBinding(ImportLayersRemoveCommand, gcnew ExecutedRoutedEventHandler( this, &MLandscapeEditWindow::LayerRemoveButton_Click) ) );

		ImportButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::ImportButton_Click );
		ImportButton->IsEnabled = FALSE;

		// Change Component Size
		CurrentWidth = safe_cast< TextBlock^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "WidthTextBlock" ) );
		CurrentHeight = safe_cast< TextBlock^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "HeightTextBlock" ) );

		TextBlock^ WidthTextBlock = safe_cast< TextBlock^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ConvertWidthTextBlock" ) );
		UnrealEd::Utils::CreateBinding(WidthTextBlock, TextBlock::TextProperty, this, "ConvertWidth" );
		TextBlock^ HeightTextBlock = safe_cast< TextBlock^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ConvertHeightTextBlock" ) );
		UnrealEd::Utils::CreateBinding(HeightTextBlock, TextBlock::TextProperty, this, "ConvertHeight" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< ComboBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ConvertSectionSizeCombo" ) ),
			ComboBox::SelectedIndexProperty, this, "ConvertSectionSize", gcnew IntToIntOffsetConverter( 0 ) );
		UnrealEd::Utils::CreateBinding(
			safe_cast< ComboBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ConvertNumSectionsCombo" ) ),
			ComboBox::SelectedIndexProperty, this, "ConvertNumSections", gcnew IntToIntOffsetConverter( 0 ) );
		UnrealEd::Utils::CreateBinding(
			safe_cast< Label^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ConvertTotalComponentsLabel" ) ),
			Label::ContentProperty, this, "ConvertTotalComponents" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< Label^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ConvertCompQuadNumLabel" ) ),
			Label::ContentProperty, this, "ConvertCompQuadNum" );

		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ConvertModeExpand" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsConvertModeExpand" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ConvertModeClip" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsConvertModeClip" );

		ChangeComponentSizeButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::ChangeComponentSizeButton_Click );
		ChangeComponentSizeButton->IsEnabled = FALSE;

		// Gizmo
		RoutedCommand^ GizmoPinCommand = safe_cast< RoutedCommand^ >(WindowContentElement->FindResource("LandscapeGizmoPinCommand"));
		WindowContentElement->CommandBindings->Add(
			gcnew CommandBinding(GizmoPinCommand, gcnew ExecutedRoutedEventHandler( this, &MLandscapeEditWindow::LandscapeGizmoPin_Click) ) );

		RoutedCommand^ GizmoDeleteCommand = safe_cast< RoutedCommand^ >(WindowContentElement->FindResource("LandscapeGizmoDeleteCommand"));
		WindowContentElement->CommandBindings->Add(
			gcnew CommandBinding(GizmoDeleteCommand, gcnew ExecutedRoutedEventHandler( this, &MLandscapeEditWindow::LandscapeGizmoDelete_Click) ) );

		RoutedCommand^ GizmoMoveToCommand = safe_cast< RoutedCommand^ >(WindowContentElement->FindResource("LandscapeGizmoMoveToCommand"));
		WindowContentElement->CommandBindings->Add(
			gcnew CommandBinding(GizmoMoveToCommand, gcnew ExecutedRoutedEventHandler( this, &MLandscapeEditWindow::LandscapeGizmoMoveTo_Click) ) );

		// Gizmo Import
		Button^ GizmoHeightmapFileButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "GizmoHeightmapFileButton" ) );
		GizmoHeightmapFileButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::GizmoHeightmapFileButton_Click );

		TextBox^ GizmoHeightmapFileNameTextBox = safe_cast< TextBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "GizmoHeightmapFileNameTextBox" ) );
		TextBox^ GizmoWidthTextBox = safe_cast< TextBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "GizmoWidthTextBox" ) );
		TextBox^ GizmoHeightTextBox = safe_cast< TextBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "GizmoHeightTextBox" ) );

		UnrealEd::Utils::CreateBinding(GizmoHeightmapFileNameTextBox, TextBox::TextProperty, this, "GizmoHeightmapFileNameString" );
		UnrealEd::Utils::CreateBinding(GizmoWidthTextBox, TextBox::TextProperty, this, "GizmoImportWidth" );
		UnrealEd::Utils::CreateBinding(GizmoHeightTextBox, TextBox::TextProperty, this, "GizmoImportHeight" );

		GizmoImportLayersValue = gcnew MGizmoImportLayers( &LandscapeEditSystem->UISettings.GetGizmoImportLayers() );
		ItemsControl^ GizmoImportLayersItemsControl = safe_cast< ItemsControl^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "GizmoImportLayersItemsControl" ) );
		UnrealEd::Utils::CreateBinding(GizmoImportLayersItemsControl, ItemsControl::ItemsSourceProperty, this, "GizmoImportLayersProperty");

		RoutedCommand^ LandscapeGizmoImportLayersFilenameCommand = safe_cast< RoutedCommand^ >(WindowContentElement->FindResource("LandscapeGizmoImportLayersFilenameCommand"));
		WindowContentElement->CommandBindings->Add(
			gcnew CommandBinding(LandscapeGizmoImportLayersFilenameCommand, gcnew ExecutedRoutedEventHandler( this, &MLandscapeEditWindow::GizmoLayerFileButton_Click) ) );

		RoutedCommand^ LandscapeGizmoImportLayersRemoveCommand = safe_cast< RoutedCommand^ >(WindowContentElement->FindResource("LandscapeGizmoImportLayersRemoveCommand"));
		WindowContentElement->CommandBindings->Add(
			gcnew CommandBinding(LandscapeGizmoImportLayersRemoveCommand, gcnew ExecutedRoutedEventHandler( this, &MLandscapeEditWindow::GizmoLayerRemoveButton_Click) ) );

		Button^ GizmoLayerAddButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "GizmoLayerAddButton" ) );
		GizmoLayerAddButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::GizmoLayerAddButton_Click );

		GizmoLayerRemoveButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "GizmoLayerRemoveButton" ) );
		GizmoLayerRemoveButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::GizmoLastLayerRemoveButton_Click );
		GizmoLayerRemoveButton->IsEnabled = FALSE;

		GizmoImportButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::GizmoImportButton_Click );
		GizmoImportButton->IsEnabled = FALSE;

		// Region
		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "UseMaskCheckbox" ) ),
			CheckBox::IsCheckedProperty, this, "bUseSelectedRegion" );

		InvertMaskCheckBox = safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "UseInvertMaskCheckbox" ) );
		UnrealEd::Utils::CreateBinding(
			InvertMaskCheckBox,
			CheckBox::IsCheckedProperty, this, "bUseNegativeMask" );

		MaskEnableCheckBox = safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "MaskEnabled" ) );
		UnrealEd::Utils::CreateBinding(
			MaskEnableCheckBox,
			CheckBox::IsCheckedProperty, this, "bMaskEnabled" );

		//
		// Export
		//
		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ExportLayersCheckbox" ) ),
			CheckBox::IsCheckedProperty, this, "ExportLayers" );

		Button^ ExportButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ExportButton" ) );
		ExportButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::ExportButton_Click );

		Button^ ExportGizmoButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ExportGizmoButton" ) );
		ExportGizmoButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::ExportGizmoButton_Click );


		//
		// Convert Terrain
		//
		Button^ ConvertTerrainButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ConvertTerrainButton" ) );
		ConvertTerrainButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::ConvertTerrainButton_Click );

		// LOD Bias
		LODBiasThresholdSlider = safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "LODBiasThresholdSlider" ) );
		UnrealEd::Utils::CreateBinding(
			LODBiasThresholdSlider,
			CustomControls::DragSlider::ValueProperty, this, "LODBiasThreshold" );

		UpdateLODBiasButton->Click += gcnew RoutedEventHandler( this, &MLandscapeEditWindow::UpdateLODBiasButton_Click );
		UpdateLODBiasButton->IsEnabled = TRUE;

		//
		// Landscape list
		//
		UpdateLandscapeList();

		// Show the window!
		ShowWindow( true );

		return TRUE;
	}

protected:

	void OnClose( Object^ Owner, RoutedEventArgs^ Args )
	{
		// Deactivate landscape mode when the close button is pressed
		GEditorModeTools().DeactivateMode( EM_Landscape );		
	}

	// For drag and drop
	MScopedNativePointer< TArray< FSelectedAssetInfo > > DroppedAssets;

	void OnDragEnter( Object^ Sender, DragEventArgs^ Args )
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
			DroppedAssets.Reset( new TArray<FSelectedAssetInfo>() );
			DroppedAssets->Empty( DroppedAssetStrings.Num() );
			for ( TArray<FString>::TConstIterator StringIter( DroppedAssetStrings ); StringIter; ++StringIter )
			{
				new( *( DroppedAssets.Get() ) ) FSelectedAssetInfo( *StringIter );
			}
			Args->Handled = TRUE;
		}
	}

	/** Called in response to the user's drag operation exiting the consolidation panel; deletes any dropped asset data */
	void OnDragLeave( Object^ Sender, DragEventArgs^ Args )
	{
		if( DroppedAssets.Get() != NULL )
		{
			DroppedAssets->Empty();
			DroppedAssets.Reset(NULL);
		}
		Args->Handled = TRUE;
	}


	void OnDragOver( Object^ Owner, DragEventArgs^ Args )
	{
		Args->Effects = DragDropEffects::Copy;

		if( DroppedAssets.Get() != NULL )
		{
			for ( TArray<FSelectedAssetInfo>::TConstIterator DroppedAssetsIter( *(DroppedAssets.Get()) ); DroppedAssetsIter; ++DroppedAssetsIter )
			{
				const FSelectedAssetInfo& CurInfo = *DroppedAssetsIter;
				if( CurInfo.ObjectClass != ULandscapeLayerInfoObject::StaticClass() )
				{
					Args->Effects = DragDropEffects::None;
					break;
				}
			}
		}

		Args->Handled = TRUE;
	}

	void OnDrop( Object^ Owner, DragEventArgs^ Args )
	{
		if( DroppedAssets.Get() != NULL )
		{
			for ( TArray<FSelectedAssetInfo>::TConstIterator DroppedAssetsIter( *(DroppedAssets.Get()) ); DroppedAssetsIter; ++DroppedAssetsIter )
			{
				const FSelectedAssetInfo& CurInfo = *DroppedAssetsIter;

				ULandscapeLayerInfoObject* LayerInfo = LoadObject<ULandscapeLayerInfoObject>(NULL, *CurInfo.ObjectPathName, NULL, LOAD_None, NULL);
				if( LayerInfo )
				{		
					// Add item
					LandscapeEditSystem->AddLayerInfo( LayerInfo );
					LandscapeTargetsProperty->NotifyChanged();
				}
			}

			DroppedAssets->Empty();
			DroppedAssets.Reset(NULL);
		}

		Args->Handled = TRUE;
	}


	void UpdateToolButtons()
	{
		if (LandscapeEditSystem->CurrentToolTarget.LandscapeInfo)
		{
			Visual^ RootVisual = InteropWindow->RootVisual;
			// Test Landscape Actor is available
			if (!Cast<ALandscape>(LandscapeEditSystem->CurrentToolTarget.LandscapeInfo->LandscapeProxy))
			{
				if (LandscapeEditSystem->CurrentToolIndex == LandscapeEditSystem->MoveToLevelToolIndex)
				{
					// Change CurrentToolIndex to paint tool
					LandscapeEditSystem->SetCurrentTool(0);
				}

				// Disable MoveToLevel
				if (LandscapeEditSystem->MoveToLevelToolIndex < LandscapeEditSystem->LandscapeToolSets.Num())
				{
					FLandscapeToolSet* ToolSet = LandscapeEditSystem->LandscapeToolSets(LandscapeEditSystem->MoveToLevelToolIndex);
					CustomControls::ImageRadioButton^ btn = safe_cast< CustomControls::ImageRadioButton^ >(LogicalTreeHelper::FindLogicalNode( RootVisual, gcnew System::String(ToolSet->GetToolSetName())) );
					btn->IsEnabled = FALSE;
				}
			}
			else if (LandscapeEditSystem->MoveToLevelToolIndex < LandscapeEditSystem->LandscapeToolSets.Num())
			{
				// Enable MoveToLevel
				FLandscapeToolSet* ToolSet = LandscapeEditSystem->LandscapeToolSets(LandscapeEditSystem->MoveToLevelToolIndex);
				CustomControls::ImageRadioButton^ btn = safe_cast< CustomControls::ImageRadioButton^ >(LogicalTreeHelper::FindLogicalNode( RootVisual, gcnew System::String(ToolSet->GetToolSetName())) );
				btn->IsEnabled = TRUE;
			}
		}
	}

	/** Called when a landscape edit window property is changed */
	void OnLandscapeEditPropertyChanged( Object^ Owner, PropertyChangedEventArgs^ Args )
	{
		UBOOL RecalcSizes = FALSE;
		
		if( String::Compare(Args->PropertyName, "HeightmapFileNameString")==0 )
		{
			HeightmapFileSize = GFileManager->FileSize( *CLRTools::ToFString(HeightmapFileNameString) );

			if( HeightmapFileSize == INDEX_NONE )
			{
				Width = 0;
				Height = 0;
				SectionSize = 0;
				NumSections = 0;
				TotalComponents = 0;
			}
			else
			{
				// Guess dimensions from filesize
				INT w = appTrunc(appSqrt((FLOAT)(HeightmapFileSize>>1)));
				INT h = (HeightmapFileSize>>1) / w;

				// Keep searching for the most squarelike size
				while( (w * h * 2) != HeightmapFileSize )
				{
					w--;
					if( w <= 0 )
					{
						w = 0;
						h = 0;
						SectionSize = 0;
						NumSections = 0;
						TotalComponents = 0;
						break;
					}
					h = (HeightmapFileSize>>1) / w;
				}
				Width = w;
				Height = h;
			}
			RecalcSizes = TRUE;
		}

		if( String::Compare(Args->PropertyName, "Width")==0 )
		{
			if( Width > 0 && HeightmapFileSize > 0)
			{
				INT h = (HeightmapFileSize>>1) / Width;
				if( (Width * h * 2) == HeightmapFileSize )
				{
					Height = h;
				}
			}
			RecalcSizes = TRUE;
		}

		if( String::Compare(Args->PropertyName, "Height")==0 )
		{
			if( Height > 0 && HeightmapFileSize > 0)
			{
				INT w = (HeightmapFileSize>>1) / Height;
				if( (w * Height * 2) == HeightmapFileSize )
				{
					Width = w;
				}
			}
			RecalcSizes = TRUE;
		}

		// Gizmo Import
		if( String::Compare(Args->PropertyName, "GizmoHeightmapFileNameString")==0 )
		{
			GizmoFileSize = GFileManager->FileSize( *CLRTools::ToFString(GizmoHeightmapFileNameString) );

			if( GizmoFileSize == INDEX_NONE )
			{
				GizmoImportWidth = 0;
				GizmoImportHeight = 0;
				GizmoImportButton->IsEnabled = FALSE;
			}
			else
			{
				// Guess dimensions from filesize
				INT w = appTrunc(appSqrt((FLOAT)(GizmoFileSize>>1)));
				INT h = (GizmoFileSize>>1) / w;

				// Keep searching for the most squarelike size
				while( (w * h * 2) != GizmoFileSize )
				{
					w--;
					if( w <= 0 )
					{
						w = 0;
						h = 0;
						break;
					}
					h = (GizmoFileSize>>1) / w;
				}
				GizmoImportWidth = w;
				GizmoImportHeight = h;
				GizmoImportButton->IsEnabled = TRUE;
			}
		}

		if( String::Compare(Args->PropertyName, "GizmoImportWidth")==0 )
		{
			if( GizmoImportWidth > 0 && GizmoFileSize > 0)
			{
				INT h = (GizmoFileSize>>1) / GizmoImportWidth;
				if( (GizmoImportWidth * h * 2) == GizmoFileSize )
				{
					GizmoImportHeight = h;
				}
			}
		}

		if( String::Compare(Args->PropertyName, "GizmoImportHeight")==0 )
		{
			if( GizmoImportHeight > 0 && GizmoFileSize > 0)
			{
				INT w = (GizmoFileSize>>1) / GizmoImportHeight;
				if( (w * GizmoImportHeight * 2) == GizmoFileSize )
				{
					GizmoImportWidth = w;
				}
			}
		}

		if( RecalcSizes )
		{
			TotalComponents = 0;
			UBOOL bFoundMatch = FALSE;
			if( Width > 0 && Height > 0 )
			{
				// Try to find a section size and number of sections that matches the dimensions of the heightfield
				for( INT SectionSizeItem=1;SectionSizeItem<7;SectionSizeItem++ )
				{
					for(INT NumSectionsItem=2;NumSectionsItem>=1;NumSectionsItem-- )
					{
						// ss is 255, 127, 63, or 31, 15 or 7 quads per section
						INT ss = (256 >> (SectionSizeItem-1)) - 1;
						// ns is 2 or 1
						INT ns = NumSectionsItem;

						if( ((Width-1) % (ss * ns)) == 0 &&
							((Height-1) % (ss * ns)) == 0 )
						{
							bFoundMatch = TRUE;
							// Update combo boxes
							SectionSize = SectionSizeItem;
							NumSections = NumSectionsItem;
							TotalComponents = (Width-1) * (Height-1) / Square(ss * ns);
							break;
						}					
					}
					if( bFoundMatch )
					{
						break;
					}
				}
				if( !bFoundMatch )
				{
					SectionSize = 0;
					NumSections = 0;
					TotalComponents = 0;
				}
			}
		}

		if( String::Compare(Args->PropertyName, "SectionSize")==0 )
		{
			UBOOL bFoundMatch = FALSE;
			if( SectionSize > 0 )
			{
				for(INT NumSectionsItem=2;NumSectionsItem>=1;NumSectionsItem-- )
				{
					// ss is 255, 127, 63, or 31, 15 or 7 quads per section
					INT ss = (256 >> (SectionSize-1)) - 1;
					// ns is 2 or 1
					INT ns = NumSectionsItem;

					if( ((Width-1) % (ss * ns)) == 0 &&
						((Height-1) % (ss * ns)) == 0 )
					{
						TotalComponents = (Width-1) * (Height-1) / Square(ss * ns);
						NumSections = NumSectionsItem;
						bFoundMatch = TRUE;
						break;
					}
				}
			}
			if( !bFoundMatch )
			{
				TotalComponents = 0;
				NumSections = 0;
			}
		}

		if( String::Compare(Args->PropertyName, "NumSections")==0 )
		{
			UBOOL bFoundMatch = FALSE;
			if( NumSections > 0 )
			{
				for( INT SectionSizeItem=1;SectionSizeItem<7;SectionSizeItem++ )
				{
					// ss is 255, 127, 63, or 31, 15 or 7 quads per section
					INT ss = (256 >> (SectionSizeItem-1)) - 1;
					// ns is 2 or 1
					INT ns = NumSections;

					if( ((Width-1) % (ss * ns)) == 0 &&
						((Height-1) % (ss * ns)) == 0 )
					{
						TotalComponents = (Width-1) * (Height-1) / Square(ss * ns);
						SectionSize = SectionSizeItem;
						bFoundMatch  = TRUE;
						break;
					}		
				}
			}
			if( !bFoundMatch  )
			{
				SectionSize = 0;
				TotalComponents = 0;
			}
		}

		INT SectionSizeQuads = (256 >> (SectionSize-1))-1;
		if( ((HeightmapFileSize > 0 && (Width * Height * 2) == HeightmapFileSize) || HeightmapFileSize == INDEX_NONE ) &&
			Width > 0 && Height > 0 && SectionSize > 0 && NumSections > 0 &&		
			((Width-1) % (SectionSizeQuads * NumSections)) == 0 &&
			((Height-1) % (SectionSizeQuads * NumSections)) == 0)
		{
			ImportButton->IsEnabled = TRUE;
		}
		else
		{
			ImportButton->IsEnabled = FALSE;
		}

		ChangeComponentSizeButton->IsEnabled = FALSE;

		if( String::Compare(Args->PropertyName, "ConvertNumSections")==0 || String::Compare(Args->PropertyName, "ConvertSectionSize")==0 || String::Compare(Args->PropertyName, "ConvertMode")==0 )
		{
			ULandscapeInfo* LandscapeInfo = LandscapeEditSystem->CurrentToolTarget.LandscapeInfo;
			if( LandscapeInfo && ConvertSectionSize > 0 && ConvertNumSections > 0)
			{
				INT MinX, MinY, MaxX, MaxY;
				if (LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
				{
					INT SectSize = (256 >> (ConvertSectionSize-1)) - 1;
					INT NumSect = ConvertNumSections;
					INT DestSize = SectSize * NumSect;
					if (IsConvertModeExpand)
					{
						ConvertWidth = appCeil((FLOAT)(MaxX - MinX) / DestSize) * DestSize + 1;
						ConvertHeight = appCeil((FLOAT)(MaxY - MinY) / DestSize) * DestSize + 1;
					}
					else // Clip
					{
						ConvertWidth = Max(1, appFloor((FLOAT)(MaxX - MinX) / DestSize)) * DestSize + 1;
						ConvertHeight = Max(1, appFloor((FLOAT)(MaxY - MinY) / DestSize)) * DestSize + 1;
					}

					ConvertTotalComponents = (ConvertWidth-1) * (ConvertHeight-1) / Square(DestSize);
					ConvertCompQuadNum = DestSize;
					ChangeComponentSizeButton->IsEnabled = TRUE;
				}
			}
		}

		if( String::Compare(Args->PropertyName, "IsViewModeDebugLayer")==0 )
		{
			// Heightmap
			if (GLandscapeViewMode == ELandscapeViewMode::DebugLayer && LandscapeEditSystem->CurrentToolTarget.LandscapeInfo)
			{
				LandscapeEditSystem->CurrentToolTarget.LandscapeInfo->UpdateDebugColorMaterial();
			}
		}
	}

	void OnLandscapeGizmoPropertyChanged( Object^ Owner, PropertyChangedEventArgs^ Args )
	{
		if( String::Compare(Args->PropertyName, "GizmoName")==0 )
		{
		}
	}

	void OnLandscapeListSelectionChanged( Object^ Owner, SelectionChangedEventArgs^ Args )
	{
		//ComboBox^ LandscapeCombo = safe_cast<ComboBox^>(Owner);
		ULandscapeInfo* PrevLandscapeInfo = LandscapeEditSystem->CurrentToolTarget.LandscapeInfo;
		LandscapeEditSystem->CurrentToolTarget.LandscapeInfo = NULL;
		TArray<FLandscapeListInfo>* LandscapeList = LandscapeEditSystem->GetLandscapeList();

		if( LandscapeComboBox->SelectedIndex >= 0 && LandscapeList && LandscapeComboBox->SelectedIndex < LandscapeList->Num())
		{
			FLandscapeListInfo& ListInfo = (*LandscapeList)(LandscapeComboBox->SelectedIndex);
			LandscapeEditSystem->CurrentToolTarget.LandscapeInfo = ListInfo.Info;
			UpdateToolButtons();
			// Update labels
			LandscapeCompNum->Content = CLRTools::ToString(FString::Printf(TEXT("%d"), ListInfo.Info ? ListInfo.Info->XYtoComponentMap.Num() : 0));
			CollisionCompNum->Content = CLRTools::ToString(FString::Printf(TEXT("%d"), ListInfo.Info ? ListInfo.Info->XYtoCollisionComponentMap.Num() : 0));
			CompQuadNum->Content = CLRTools::ToString(FString::Printf(TEXT("%d"), ListInfo.ComponentQuads));
			QuadPerSection->Content = CLRTools::ToString(FString::Printf(TEXT("%d"), ListInfo.ComponentQuads / ListInfo.NumSubsections));
			SubsectionNum->Content = CLRTools::ToString(FString::Printf(TEXT("%d"), Square(ListInfo.NumSubsections)));

			CurrentWidth->Text = CLRTools::ToString(FString::Printf(TEXT("%d"), ListInfo.Width));
			CurrentHeight->Text = CLRTools::ToString(FString::Printf(TEXT("%d"), ListInfo.Height));
		}

		if( LandscapeEditSystem->CurrentToolTarget.LandscapeInfo && LandscapeEditSystem->CurrentToolTarget.LandscapeInfo != PrevLandscapeInfo )
		{
			UpdateTargets();
		}

		// Update Gizmo Location
		if (LandscapeEditSystem->CurrentGizmoActor)
		{
			LandscapeEditSystem->CurrentGizmoActor->SetTargetLandscape(LandscapeEditSystem->CurrentToolTarget.LandscapeInfo);
		}
	}

	void OnLandscapeTargetLayerPropertyChanged( Object^ Owner, PropertyChangedEventArgs^ Args )
	{
		MLandscapeTargetListWrapper^ Item = safe_cast<MLandscapeTargetListWrapper^>(Owner);

		if ( String::Compare(Args->PropertyName, "Viewmode")==0 )
		{
			// Should update debug color channels to prevent 
			INT DebugColor = Item->DebugColor;
			UBOOL ChangedOthers = FALSE;
			TArray<FLandscapeTargetListInfo>* TargetList = LandscapeEditSystem->GetTargetList();
			if (TargetList)
			{
				for (int i = 0; i < TargetList->Num(); ++i )
				{
					if (i != Item->Index)
					{
						FLandscapeLayerStruct* LayerInfo = (*TargetList)(i).LayerInfo;
						if (LayerInfo)
						{
							INT& DebugColorChannel = LayerInfo->DebugColorChannel;
							if (DebugColor & DebugColorChannel)
							{
								DebugColorChannel -= (DebugColor & DebugColorChannel);
								ChangedOthers = TRUE;
							}
						}
					}
				}
				if (ChangedOthers)
				{
					LandscapeTargetsValue->NotifyChanged();
				}
			}
			LandscapeEditSystem->CurrentToolTarget.LandscapeInfo->UpdateDebugColorMaterial();
		}
		else if( String::Compare(Args->PropertyName, "IsSelected")==0 )
		{
			if( Item->IsSelected )
			{
				LandscapeEditSystem->CurrentToolTarget.TargetType = Item->GetTargetType();
				LandscapeEditSystem->CurrentToolTarget.LayerName = Item->GetLayerName();
			}
		}
		else if( String::Compare(Args->PropertyName, "PhysMaterial")==0 )
		{
			if (LandscapeEditSystem->CurrentToolTarget.LandscapeInfo &&
				LandscapeEditSystem->CurrentToolTarget.LandscapeInfo->LandscapeProxy)
			{
				LandscapeEditSystem->CurrentToolTarget.LandscapeInfo->LandscapeProxy->ChangedPhysMaterial();
			}
		}
	}

	void AlphaTextureUseButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		INT Channel =	Owner==AlphaTextureUseGButton ? 1 :
						Owner==AlphaTextureUseBButton ? 2 :
						Owner==AlphaTextureUseAButton ? 3 :
						0;

		// Get current selection from content brrowser
		GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);
		USelection* SelectedSet = GEditor->GetSelectedSet( UTexture2D::StaticClass() );
		// If an object of that type is selected, use it.
		UTexture2D* SelectedTexture = Cast<UTexture2D>( SelectedSet->GetTop( UTexture2D::StaticClass() ) );
		
		LandscapeEditSystem->UISettings.SetAlphaTexture( SelectedTexture ? *SelectedTexture->GetPathName() : NULL, Channel);
		AlphaTexture = MakeAlphaTextureBitmap();
	}

	void SyncContentBrowserButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		TArray<UObject*> Objects;
		Objects.AddItem(LandscapeEditSystem->UISettings.GetAlphaTexture());
		GApp->EditorFrame->SyncBrowserToObjects(Objects);
	}

	Imaging::BitmapSource^ MakeAlphaTextureBitmap()
	{
		INT SizeX = LandscapeEditSystem->UISettings.GetAlphaTextureSizeX();
		INT SizeY = LandscapeEditSystem->UISettings.GetAlphaTextureSizeY();
		const BYTE* Data = LandscapeEditSystem->UISettings.GetAlphaTextureData();

		System::IntPtr ImageDataPtr( ( PTRINT )Data );

		Imaging::BitmapSource^ MyBitmapSource = nullptr;

		MyBitmapSource = Imaging::BitmapSource::Create(
			SizeX,		// Width
			SizeY,		// Height
			96,									// Horizontal DPI
			96,									// Vertical DPI
			PixelFormats::Gray8,				// Pixel format
			nullptr,							// Palette
			ImageDataPtr,						// Image data
			SizeX * SizeY,						// Size of image data
			SizeX );							// Stride

		return MyBitmapSource;
	}

	void OnLandscapeTargetLayerChanged( Object^ Owner, ExecutedRoutedEventArgs^ Args )
	{
		TargetListBox->Focus();
	}

	void OnLandscapeGizmoChanged( Object^ Owner, ExecutedRoutedEventArgs^ Args )
	{
		GizmoListBox->Focus();
	}

	void LandscapeGizmoPin_Click( Object^ Owner, ExecutedRoutedEventArgs^ Args )
	{
		if (LandscapeEditSystem->CurrentGizmoActor)
		{
			new(LandscapeEditSystem->UISettings.GetGizmoHistories()) FGizmoHistory(LandscapeEditSystem->CurrentGizmoActor->SpawnGizmoActor());
			GizmoHistoriesProperty->NotifyChanged();
		}
	}

	void LandscapeGizmoDelete_Click( Object^ Owner, ExecutedRoutedEventArgs^ Args )
	{
		TArray<FGizmoHistory>& Histories = LandscapeEditSystem->UISettings.GetGizmoHistories();
		if (CurrentGizmoIndex >= 0 && CurrentGizmoIndex < Histories.Num())
		{
			ALandscapeGizmoActor* History = Histories(CurrentGizmoIndex).Gizmo;
			GWorld->DestroyActor(History);
			Histories.Remove(CurrentGizmoIndex);
			GizmoHistoriesProperty->NotifyChanged();
		}
	}

	void LandscapeGizmoMoveTo_Click( Object^ Owner, ExecutedRoutedEventArgs^ Args )
	{
		if (LandscapeEditSystem->CurrentGizmoActor )
		{
			TArray<FGizmoHistory>& Histories = LandscapeEditSystem->UISettings.GetGizmoHistories();
			if (CurrentGizmoIndex >= 0 && CurrentGizmoIndex < Histories.Num())
			{
				Histories(CurrentGizmoIndex).Gizmo->Duplicate(LandscapeEditSystem->CurrentGizmoActor);
			}
		}
	}

	/** Called in response to the user clicking the File/Open button */
	void HeightmapFileButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		// Prompt the user for the filenames
		WxFileDialog OpenFileDialog(GApp->EditorFrame, 
			*LocalizeUnrealEd("Import"),
			*CLRTools::ToFString(LastImportPath),
			TEXT(""),
			TEXT("Heightmap files (*.raw,*.r16)|*.raw;*.r16|All files (*.*)|*.*"),
			wxOPEN | wxFILE_MUST_EXIST,
			wxDefaultPosition
			);

		if( OpenFileDialog.ShowModal() == wxID_OK )
		{
			wxArrayString OpenFilePaths;
			OpenFileDialog.GetPaths(OpenFilePaths);	
			FFilename OpenFilename((const TCHAR*)OpenFilePaths[0]);
			HeightmapFileNameString = CLRTools::ToString(OpenFilename);
			LastImportPath = CLRTools::ToString(*OpenFilename.GetPath());
		}
	}

	void LayerFileButton_Click( Object^ Owner, ExecutedRoutedEventArgs^ Args )
	{
		LandscapeImportLayer^ Layer = safe_cast<LandscapeImportLayer^>(Args->Parameter);

		// Prompt the user for the filenames
		WxFileDialog OpenFileDialog(GApp->EditorFrame, 
			*LocalizeUnrealEd("Import"),
			*CLRTools::ToFString(LastImportPath),
			TEXT(""),
			TEXT("Layer files (*.raw,*.r8)|*.raw;*.r8|All files (*.*)|*.*"),
			wxOPEN | wxFILE_MUST_EXIST,
			wxDefaultPosition
			);

		if( OpenFileDialog.ShowModal() == wxID_OK )
		{
			wxArrayString OpenFilePaths;
			OpenFileDialog.GetPaths(OpenFilePaths);	
			FFilename OpenFilename((const TCHAR*)OpenFilePaths[0]);
			Layer->LayerFilename = CLRTools::ToString(OpenFilename);
			Layer->LayerName = CLRTools::ToString(*OpenFilename.GetBaseFilename());
			LastImportPath = CLRTools::ToString(*OpenFilename.GetPath());
			LandscapeImportLayersProperty->CheckNeedNewEntry();
		}
	}

	/** Called in response to the user clicking the File/Open button */
	void GizmoHeightmapFileButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		// Prompt the user for the filenames
		WxFileDialog OpenFileDialog(GApp->EditorFrame, 
			*LocalizeUnrealEd("Import"),
			*CLRTools::ToFString(LastImportPath),
			TEXT(""),
			TEXT("Heightmap files (*.raw,*.r16)|*.raw;*.r16|All files (*.*)|*.*"),
			wxOPEN | wxFILE_MUST_EXIST,
			wxDefaultPosition
			);

		if( OpenFileDialog.ShowModal() == wxID_OK )
		{
			wxArrayString OpenFilePaths;
			OpenFileDialog.GetPaths(OpenFilePaths);	
			FFilename OpenFilename((const TCHAR*)OpenFilePaths[0]);
			GizmoHeightmapFileNameString = CLRTools::ToString(OpenFilename);
			LastImportPath = CLRTools::ToString(*OpenFilename.GetPath());
		}
	}

	void GizmoLayerFileButton_Click( Object^ Owner, ExecutedRoutedEventArgs^ Args )
	{
		MGizmoImportLayerWrapper^ Layer = safe_cast<MGizmoImportLayerWrapper^>(Args->Parameter);

		// Prompt the user for the filenames
		WxFileDialog OpenFileDialog(GApp->EditorFrame, 
			*LocalizeUnrealEd("Import"),
			*CLRTools::ToFString(LastImportPath),
			TEXT(""),
			TEXT("Layer files (*.raw,*.r8)|*.raw;*.r8|All files (*.*)|*.*"),
			wxOPEN | wxFILE_MUST_EXIST,
			wxDefaultPosition
			);

		if( OpenFileDialog.ShowModal() == wxID_OK )
		{
			wxArrayString OpenFilePaths;
			OpenFileDialog.GetPaths(OpenFilePaths);	
			FFilename OpenFilename((const TCHAR*)OpenFilePaths[0]);
			Layer->LayerFilename = CLRTools::ToString(OpenFilename);
			Layer->LayerName = CLRTools::ToString(*OpenFilename.GetBaseFilename());
			LastImportPath = CLRTools::ToString(*OpenFilename.GetPath());
			GizmoImportLayersProperty->NotifyChanged();
		}
	}

	void GizmoLayerRemoveButton_Click( Object^ Owner, ExecutedRoutedEventArgs^ Args )
	{
		MGizmoImportLayerWrapper^ Layer = safe_cast<MGizmoImportLayerWrapper^>(Args->Parameter);
		LandscapeEditSystem->UISettings.GetGizmoImportLayers().Remove( Layer->Index );
		GizmoImportLayersProperty->NotifyChanged();
		if (LandscapeEditSystem->UISettings.GetGizmoImportLayers().Num() == 0)
		{
			GizmoLayerRemoveButton->IsEnabled = FALSE;
		}
	}


	void LayerRemoveButton_Click( Object^ Owner, ExecutedRoutedEventArgs^ Args )
	{
		LandscapeImportLayer^ Layer = safe_cast<LandscapeImportLayer^>(Args->Parameter);
		LandscapeImportLayersProperty->Remove(Layer);
		LandscapeImportLayersProperty->CheckNeedNewEntry();
	}

	void TargetLayerRemoveButton_Click( Object^ Owner, ExecutedRoutedEventArgs^ Args )
	{
		MLandscapeTargetListWrapper^ Target = safe_cast<MLandscapeTargetListWrapper^>(Args->Parameter);

		ULandscapeInfo* LandscapeInfo = LandscapeEditSystem->CurrentToolTarget.LandscapeInfo;
		if( LandscapeInfo )
		{
			if( appMsgf( AMT_YesNo, LocalizeSecure(LocalizeUnrealEd("LandscapeMode_RemoveLayer"), *Target->GetLayerName().ToString())) )
			{
				LandscapeInfo->DeleteLayer(Target->GetLayerName());
			}
		}

		UpdateTargets();
	}

	void TargetLayerSyncButton_Click( Object^ Owner, ExecutedRoutedEventArgs^ Args )
	{
		MLandscapeTargetListWrapper^ Target = safe_cast<MLandscapeTargetListWrapper^>(Args->Parameter);

		ULandscapeInfo* LandscapeInfo = LandscapeEditSystem->CurrentToolTarget.LandscapeInfo;
		if( LandscapeInfo )
		{
			FLandscapeLayerStruct* LayerInfo = LandscapeInfo->LayerInfoMap.FindRef(Target->GetLayerName());
			if (LayerInfo && LayerInfo->LayerInfoObj)
			{
				TArray<UObject*> Objects;
				Objects.AddItem(LayerInfo->LayerInfoObj);
				GApp->EditorFrame->SyncBrowserToObjects(Objects);
			}
		}
		//UpdateTargets();
	}

	void TargetReimportButton_Click( Object^ Owner, ExecutedRoutedEventArgs^ Args )
	{
		MLandscapeTargetListWrapper^ Target = safe_cast<MLandscapeTargetListWrapper^>(Args->Parameter);

		ULandscapeInfo* LandscapeInfo = LandscapeEditSystem->CurrentToolTarget.LandscapeInfo;
		if( LandscapeInfo )
		{
			if ( Target->GetTargetType() == LET_Heightmap )
			{
				if (LandscapeInfo->HeightmapFilePath.Len())
				{
					TArray<BYTE> Data;
					appLoadFileToArray(Data, *LandscapeInfo->HeightmapFilePath);
					if (!LandscapeInfo->ReimportHeightmap(Data.Num(), (WORD*)&Data(0)))
					{
						appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("LandscapeReImport_BadFileSize"), *LandscapeInfo->HeightmapFilePath) );
						return;
					}
				}
				else
				{
					appMsgf( AMT_OK, *LocalizeUnrealEd(TEXT("LandscapeReImport_BadFileName")) );
					return;
				}
			}
			else
			{
				FLandscapeLayerStruct* LayerInfo = LandscapeInfo->LayerInfoMap.FindRef(Target->GetLayerName());
				if (LayerInfo && LayerInfo->LayerInfoObj && LayerInfo->SourceFilePath.Len())
				{
					TArray<BYTE> Data;
					appLoadFileToArray(Data, *LayerInfo->SourceFilePath);
					if (!LandscapeInfo->ReimportLayermap(LayerInfo->LayerInfoObj->LayerName, Data))
					{
						appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("LandscapeReImport_BadFileSize"), *LayerInfo->SourceFilePath) );
						return;
					}
				}
				else
				{
					appMsgf( AMT_OK, *LocalizeUnrealEd(TEXT("LandscapeReImport_BadFileName")) );
					return;
				}
			}
		}
		//UpdateTargets();
	}

	void TargetLayerSetPhysMaterialButton_Click( Object^ Owner, ExecutedRoutedEventArgs^ Args )
	{
		// Get current PhysMaterial from content browser
		GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);
		USelection* SelectedSet = GEditor->GetSelectedSet( UPhysicalMaterial::StaticClass() );
		UPhysicalMaterial* SelectedPhysMat = Cast<UPhysicalMaterial>( SelectedSet->GetTop( UPhysicalMaterial::StaticClass() ) );
		if( SelectedPhysMat != NULL )
		{
			MLandscapeTargetListWrapper^ Target = safe_cast<MLandscapeTargetListWrapper^>(Args->Parameter);
			Target->PhysMaterial = CLRTools::ToString(SelectedPhysMat->GetPathName());
		}
	}

	void TargetSetFilePathButton_Click( Object^ Owner, ExecutedRoutedEventArgs^ Args )
	{
		// Prompt the user for the filenames
		WxFileDialog OpenFileDialog(GApp->EditorFrame, 
			*LocalizeUnrealEd("Import"),
			*CLRTools::ToFString(LastImportPath),
			TEXT(""),
			TEXT("Layer files (*.raw,*.r8)|*.raw;*.r8|All files (*.*)|*.*"),
			wxOPEN | wxFILE_MUST_EXIST,
			wxDefaultPosition
			);

		if( OpenFileDialog.ShowModal() == wxID_OK )
		{
			wxArrayString OpenFilePaths;
			OpenFileDialog.GetPaths(OpenFilePaths);	
			FFilename OpenFilename((const TCHAR*)OpenFilePaths[0]);

			MLandscapeTargetListWrapper^ Target = safe_cast<MLandscapeTargetListWrapper^>(Args->Parameter);
			Target->SourceFilePath = CLRTools::ToString(OpenFilename);
		}
	}

	void AddLayerButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		Hardness = Clamp<FLOAT>(Hardness, 0.f, 1.f);
		ULandscapeInfo* Info = LandscapeEditSystem->CurrentToolTarget.LandscapeInfo;
		if( Info )
		{
			FName LayerFName = CLRTools::ToFName(AddLayerNameString);
			if( LayerFName == NAME_None )
			{
				appMsgf( AMT_OK, *LocalizeUnrealEd("LandscapeMode_AddLayerEnterName") );
				return;
			}
			// check for duplicate
			if( Info->LayerInfoMap.FindRef(LayerFName) || LayerFName == ALandscape::DataWeightmapName )
			{
				appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("LandscapeMode_AddLayerDuplicate"), *LayerFName.ToString()) );
				return;
			}

			if (Info->LandscapeProxy)
			{
				ULandscapeLayerInfoObject* LayerInfo = Info->LandscapeProxy->GetLayerInfo(*LayerFName.ToString());
				if (LayerInfo)
				{
					LayerInfo->LayerName = LayerFName;
					LayerInfo->Hardness = Hardness;
					LayerInfo->bNoWeightBlend = bNoBlending;
				}
			}
		}
		AddLayerNameString = "";
		UpdateTargets();
	}

	void GizmoLayerAddButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		new(LandscapeEditSystem->UISettings.GetGizmoImportLayers()) FGizmoImportLayer();
		GizmoImportLayersProperty->NotifyChanged();
		GizmoLayerRemoveButton->IsEnabled = TRUE;
	}

	void GizmoLastLayerRemoveButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		if (LandscapeEditSystem->UISettings.GetGizmoImportLayers().Num() - 1 >= 0)
		{
			LandscapeEditSystem->UISettings.GetGizmoImportLayers().Remove( LandscapeEditSystem->UISettings.GetGizmoImportLayers().Num() - 1 );
			GizmoImportLayersProperty->NotifyChanged();
		}
		if (LandscapeEditSystem->UISettings.GetGizmoImportLayers().Num() == 0)
		{
			GizmoLayerRemoveButton->IsEnabled = FALSE;
		}
	}

	void GizmoImportButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		if (!LandscapeEditSystem->CurrentGizmoActor)
		{
			return;
		}

		TArray<BYTE> Data;
		// Need to Import data to Gizmo...
		if( GizmoFileSize > 0 )
		{
			appLoadFileToArray(Data, *CLRTools::ToFString(GizmoHeightmapFileNameString));
		}

		if (GizmoFileSize <= 0)
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("LandscapeImport_BadHeightmapSize") );
			return; //
		}

		TArray<FName> LayerNames;
		TArray<TArray<BYTE> > LayerDataArrays;
		TArray<BYTE*> LayerDataPtrs;

		for( INT LayerIndex=0;LayerIndex<LandscapeEditSystem->UISettings.GetGizmoImportLayers().Num();LayerIndex++ )
		{
			FGizmoImportLayer Layer = LandscapeEditSystem->UISettings.GetGizmoImportLayers()(LayerIndex);
			FString LayerName = Layer.LayerName.Replace(TEXT(" "),TEXT(""));

			if( Layer.LayerFilename != TEXT("") && !Layer.bNoImport )
			{
				TArray<BYTE>* LayerData = new(LayerDataArrays)(TArray<BYTE>);
				appLoadFileToArray(*LayerData, *Layer.LayerFilename);

				if( LayerData->Num() != (GizmoImportWidth*GizmoImportHeight) )
				{
					appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("LandscapeImport_BadLayerSize"), *Layer.LayerFilename) );
					return;
				}
				if( LayerName==TEXT("") )
				{
					appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("LandscapeImport_BadLayerName"), *Layer.LayerFilename) );
					return;
				}

				new(LayerNames) FName(*LayerName);
				LayerDataPtrs.AddItem(&(*LayerData)(0));
			}
			else if( LayerName!=TEXT("") )
			{
				// Ignore...
				return;
			}
		}

		LandscapeEditSystem->CurrentGizmoActor->Import( GizmoImportWidth, GizmoImportHeight, (WORD*)&Data(0), LayerNames, LayerDataPtrs.Num() ? &LayerDataPtrs(0) : NULL);

		// Clear out parameters
		//GizmoHeightmapFileNameString = "";
		//GizmoImportWidth = 0;
		//GizmoImportHeight = 0;
		//GizmoImportLayersProperty->NotifyChanged();
	}

	/** Called when the Import button is clicked */
	void ImportButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		INT SectionSizeQuads = (256 >> (SectionSize-1))-1;
		TArray<BYTE> Data;
		FString HeightmapName;
		if( HeightmapFileSize > 0 )
		{
			HeightmapName = CLRTools::ToFString(HeightmapFileNameString);
			appLoadFileToArray(Data, *HeightmapName);
		}
		else
		{
			// Initialize blank heightmap data
			Data.Add(Width*Height*2);
			WORD* WordData = (WORD*)&Data(0);
			for( INT i=0;i<Width*Height;i++ )
			{
				WordData[i] = 32768;
			}						
		}
		TArray<FLandscapeLayerInfo> LayerInfos;
		TArray<TArray<BYTE> > LayerDataArrays;
		TArray<BYTE*> LayerDataPtrs;

		for( INT LayerIndex=0;LayerIndex<LandscapeImportLayersProperty->Count;LayerIndex++ )
		{
			LandscapeImportLayer^ Layer = LandscapeImportLayersProperty[LayerIndex];

			FString LayerFilename = CLRTools::ToFString(Layer->LayerFilename);
			FString LayerName = CLRTools::ToFString(Layer->LayerName).Replace(TEXT(" "),TEXT(""));
			FLOAT Hardness = Clamp<FLOAT>(Layer->Hardness, 0.f, 1.f);
			BOOL NoBlending = Layer->NoBlending;
			
			if( LayerFilename != TEXT("") )
			{
				TArray<BYTE>* LayerData = new(LayerDataArrays)(TArray<BYTE>);
				appLoadFileToArray(*LayerData, *LayerFilename);

				if( LayerData->Num() != (Width*Height) )
				{
					appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("LandscapeImport_BadLayerSize"), *LayerFilename) );
					return;
				}
				if( LayerName==TEXT("") )
				{
					appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("LandscapeImport_BadLayerName"), *LayerFilename) );
					return;
				}

				new(LayerInfos) FLandscapeLayerInfo(FName(*LayerName), Hardness, NoBlending, *LayerFilename);
				LayerDataPtrs.AddItem(&(*LayerData)(0));
			}
			else
			if( LayerName!=TEXT("") )
			{
				// Add blank layer
				TArray<BYTE>* LayerData = new(LayerDataArrays)(TArray<BYTE>);
				LayerData->AddZeroed(Width*Height);
				new(LayerInfos) FLandscapeLayerInfo(FName(*LayerName), Hardness, NoBlending);
				LayerDataPtrs.AddItem(&(*LayerData)(0));
			}
		}

		ALandscape* Landscape = Cast<ALandscape>( GWorld->SpawnActor( ALandscape::StaticClass(), NAME_None, FVector(-64*Width,-64*Height,0) ) );
		Landscape->Import( Width,Height,SectionSizeQuads*NumSections,NumSections,SectionSizeQuads,(WORD*)&Data(0), *HeightmapName, LayerInfos,LayerDataPtrs.Num() ? &LayerDataPtrs(0) : NULL);

		// Clear out parameters
		HeightmapFileNameString = "";
		Width = 0;
		Height = 0;
		SectionSize = 0;
		NumSections = 0;
		TotalComponents = 0;
		LandscapeImportLayersProperty->Clear();
		LandscapeImportLayersProperty->CheckNeedNewEntry();
		UpdateLandscapeList();
	}

	void ChangeComponentSizeButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		ULandscapeInfo* LandscapeInfo = LandscapeEditSystem->CurrentToolTarget.LandscapeInfo;
		if( LandscapeInfo )
		{
			INT SectionSizeQuads = (256 >> (ConvertSectionSize-1))-1;
			LandscapeInfo->ChangeComponentSetting(ConvertWidth, ConvertHeight, ConvertNumSections, SectionSizeQuads);

			// Clear out parameters
			ConvertWidth = 0;
			ConvertHeight = 0;
			ConvertSectionSize = 0;
			ConvertNumSections = 0;
			ConvertTotalComponents = 0;
			ConvertCompQuadNum = 0;
			UpdateLandscapeList();
		}
	}

	void UpdateLODBiasButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		ULandscapeInfo* LandscapeInfo = LandscapeEditSystem->CurrentToolTarget.LandscapeInfo;
		if( LandscapeInfo )
		{
			LandscapeInfo->UpdateLODBias(LODBiasThresholdSlider->Value);
		}
	}

	void ExportButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		ULandscapeInfo* LandscapeInfo = LandscapeEditSystem->CurrentToolTarget.LandscapeInfo;
		if( LandscapeInfo )
		{
			TArray<FName> Layernames;
			TArray<FString> Filenames;
			{
				TMap<FName, FLandscapeLayerStruct*>::TIterator It(LandscapeInfo->LayerInfoMap);
				for( INT i=-1; i < (bExportLayers ? LandscapeInfo->LayerInfoMap.Num() : 0); i++ )
				{
					FString SaveDialogTitle;
					FString DefaultFileName;
					FString FileTypes;

					if( i < 0 )
					{
						SaveDialogTitle = *LocalizeUnrealEd("LandscapeExport_HeightmapFilename");
						DefaultFileName = TEXT("Heightmap.raw");
						FileTypes = TEXT("Heightmap .raw files|*.raw|Heightmap .r16 files|*.r16|All files|*.*");
					}
					else
					{
						if (It.Value() && It.Value()->LayerInfoObj)
						{
							ULandscapeLayerInfoObject* LayerInfoObj = It.Value()->LayerInfoObj;
							SaveDialogTitle = *FString::Printf(LocalizeSecure(LocalizeUnrealEd("LandscapeExport_LayerFilename"), *(LayerInfoObj->LayerName.ToString())));
							DefaultFileName = *FString::Printf(TEXT("%s.raw"), *(LayerInfoObj->LayerName.ToString()));
						}
						FileTypes = TEXT("Layer .raw files|*.raw|Layer .r8 files|*.r8|All files|*.*");
					}

					// Prompt the user for the filenames
					WxFileDialog SaveFileDialog(GApp->EditorFrame, 
						*SaveDialogTitle,
						*CLRTools::ToFString(LastImportPath),
						*DefaultFileName,
						*FileTypes,
						wxSAVE | wxOVERWRITE_PROMPT,
						wxDefaultPosition
						);

					if( SaveFileDialog.ShowModal() != wxID_OK )
					{
						return;
					}

					wxArrayString SaveFilePaths;
					SaveFileDialog.GetPaths(SaveFilePaths);	
					FFilename SaveFilename((const TCHAR*)SaveFilePaths[0]);
					Filenames.AddItem(SaveFilename);
					LastImportPath = CLRTools::ToString(*SaveFilename.GetPath());

					if (i >= 0 && It.Value() && It.Value()->LayerInfoObj)
					{
						Layernames.AddItem( It.Key() );
						++It;
					}
				}
			}

			LandscapeInfo->Export(Layernames, Filenames);
		}
	}

	void ExportGizmoButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		ALandscapeGizmoActiveActor* Gizmo = LandscapeEditSystem->CurrentGizmoActor;
		if( Gizmo && Gizmo->TargetLandscapeInfo && Gizmo->SelectedData.Num() )
		{
			INT TargetIndex = -1;
			ULandscapeInfo* LandscapeInfo = Gizmo->TargetLandscapeInfo;
			TArray<FString> Filenames;

			// Local set for export
			TSet<FName> LayerNameSet;
			for (int i = 0; i < Gizmo->LayerNames.Num(); ++i)
			{
				if (LandscapeEditSystem->CurrentToolTarget.TargetType == LET_Weightmap && LandscapeEditSystem->CurrentToolTarget.LayerName == Gizmo->LayerNames(i))
				{
					TargetIndex = i;
				}
				LayerNameSet.Add(Gizmo->LayerNames(i));
			}

			{
				for( INT i=-1; i < Gizmo->LayerNames.Num(); i++ )
				{
					if (!bApplyToAllTargets && i != TargetIndex)
					{
						continue;
					}
					FString SaveDialogTitle;
					FString DefaultFileName;
					FString FileTypes;

					if( i < 0 )
					{
						if (!(Gizmo->DataType & LGT_Height))
						{
							continue;
						}
						SaveDialogTitle = *LocalizeUnrealEd("LandscapeExport_HeightmapFilename");
						DefaultFileName = TEXT("Heightmap.raw");
						FileTypes = TEXT("Heightmap .raw files|*.raw|Heightmap .r16 files|*.r16|All files|*.*");
					}
					else
					{
						if (!(Gizmo->DataType & LGT_Weight))
						{
							continue;
						}

						FName LayerName = Gizmo->LayerNames(i);
						SaveDialogTitle = *FString::Printf(LocalizeSecure(LocalizeUnrealEd("LandscapeExport_LayerFilename"), *LayerName.ToString()));
						DefaultFileName = *FString::Printf(TEXT("%s.raw"), *LayerName.ToString());
						FileTypes = TEXT("Layer .raw files|*.raw|Layer .r8 files|*.r8|All files|*.*");
					}

					// Prompt the user for the filenames
					WxFileDialog SaveFileDialog(GApp->EditorFrame, 
						*SaveDialogTitle,
						*CLRTools::ToFString(LastImportPath),
						*DefaultFileName,
						*FileTypes,
						wxSAVE | wxOVERWRITE_PROMPT,
						wxDefaultPosition
						);

					if( SaveFileDialog.ShowModal() != wxID_OK )
					{
						return;
					}

					wxArrayString SaveFilePaths;
					SaveFileDialog.GetPaths(SaveFilePaths);	
					FFilename SaveFilename((const TCHAR*)SaveFilePaths[0]);
					Filenames.AddItem(SaveFilename);
					LastImportPath = CLRTools::ToString(*SaveFilename.GetPath());
				}
			}

			Gizmo->Export(TargetIndex, Filenames);
		}
	}

	/** Called when the Convert terrain button is clicked */
	void ConvertTerrainButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		// Temporary
		for (FActorIterator It; It; ++It)
		{
			ATerrain* OldTerrain = Cast<ATerrain>(*It);
			if( OldTerrain )
			{
				ALandscape* Landscape = Cast<ALandscape>( GWorld->SpawnActor( ALandscape::StaticClass(), NAME_None, OldTerrain->Location ) );
				if( Landscape->ImportFromOldTerrain(OldTerrain) )
				{
					GWorld->DestroyActor(OldTerrain);
				}
				else
				{
					GWorld->DestroyActor(Landscape);
				}
			}
		}
		UpdateLandscapeList();
	}

	void ClearSelectionButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		ULandscapeInfo* LandscapeInfo = LandscapeEditSystem->CurrentToolTarget.LandscapeInfo;
		if( LandscapeInfo )
		{
			LandscapeInfo->ClearSelectedRegion(TRUE);
			LandscapeEditSystem->SetMaskEnable(LandscapeInfo->SelectedRegion.Num());
		}
	}

	void ClearMaskButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		ULandscapeInfo* LandscapeInfo = LandscapeEditSystem->CurrentToolTarget.LandscapeInfo;
		if( LandscapeInfo )
		{
			LandscapeInfo->ClearSelectedRegion(FALSE);
			LandscapeEditSystem->SetMaskEnable(LandscapeInfo->SelectedRegion.Num());
		}
	}

	void ClearGizmoDataButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		ALandscapeGizmoActiveActor* Gizmo = LandscapeEditSystem->CurrentGizmoActor;
		if( Gizmo && Gizmo->TargetLandscapeInfo )
		{
			Gizmo->ClearGizmoData();
		}
	}

	void FitToSelectionButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		ALandscapeGizmoActiveActor* Gizmo = LandscapeEditSystem->CurrentGizmoActor;
		if( Gizmo && Gizmo->TargetLandscapeInfo )
		{
			Gizmo->FitToSelection();
		}
	}

	void FitToGizmoButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		ALandscapeGizmoActiveActor* Gizmo = LandscapeEditSystem->CurrentGizmoActor;
		if( Gizmo && Gizmo->TargetLandscapeInfo )
		{
			Gizmo->FitMinMaxHeight();
		}
	}

	void CopyToGizmoButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		if (LandscapeEditSystem)
		{
			LandscapeEditSystem->CopyDataToGizmo();
		}
	}

	void ToolButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		Visual^ RootVisual = InteropWindow->RootVisual;
		//UniformGrid^ ToolGrid = safe_cast< UniformGrid^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ToolGrid" ) );
		ToggleButton^ ClickedButton = static_cast<ToggleButton^>(Owner);

		//if( ButtonIndex != -1 )
		{
			LandscapeEditSystem->SetCurrentTool(CLRTools::ToFName(ClickedButton->Name));

			// Change Brush Index to enabled one
			{
				UniformGrid^ ToolGrid = safe_cast< UniformGrid^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "BrushGrid" ) );
				INT BrushIndex = LandscapeEditSystem->CurrentBrushIndex;

				if (BrushIndex >= 0 && BrushIndex < LandscapeEditSystem->LandscapeBrushSets.Num())
				{
					FLandscapeBrushSet& BrushSet = LandscapeEditSystem->LandscapeBrushSets(BrushIndex);
					CustomControls::ToolDropdownRadioButton^ btn = safe_cast< CustomControls::ToolDropdownRadioButton^ >(LogicalTreeHelper::FindLogicalNode( RootVisual, gcnew System::String(*BrushSet.BrushSetName)) );
					if (btn->IsEnabled == true)
					{
						if( BrushSet.Brushes.IsValidIndex(btn->SelectedIndex) )
						{
							FLandscapeBrush* NewBrush = BrushSet.Brushes(btn->SelectedIndex);
							if( LandscapeEditSystem->CurrentBrush != NewBrush )
							{
								LandscapeEditSystem->CurrentBrush->LeaveBrush();
								LandscapeEditSystem->CurrentBrush = NewBrush;
								LandscapeEditSystem->CurrentBrushIndex = BrushIndex;
								LandscapeEditSystem->CurrentBrush->EnterBrush();
								btn->ToolTip  = gcnew System::String(*NewBrush->GetTooltipString());
								btn->IsChecked = TRUE;
							}
							return;
						}
					}
				}

				for( INT BrushIdx=0;BrushIdx<LandscapeEditSystem->LandscapeBrushSets.Num();BrushIdx++ )
				{
					FLandscapeBrushSet& BrushSet = LandscapeEditSystem->LandscapeBrushSets(BrushIdx);
					CustomControls::ToolDropdownRadioButton^ btn = safe_cast< CustomControls::ToolDropdownRadioButton^ >(LogicalTreeHelper::FindLogicalNode( RootVisual, gcnew System::String(*BrushSet.BrushSetName)) );
					if (btn->IsEnabled == true)
					{
						if( BrushSet.Brushes.IsValidIndex(btn->SelectedIndex) )
						{
							LandscapeEditSystem->CurrentBrushIndex = BrushIdx;
							FLandscapeBrush* NewBrush = BrushSet.Brushes(btn->SelectedIndex);
							if( LandscapeEditSystem->CurrentBrush != NewBrush )
							{
								LandscapeEditSystem->CurrentBrush->LeaveBrush();
								LandscapeEditSystem->CurrentBrush = NewBrush;
								LandscapeEditSystem->CurrentBrushIndex = BrushIdx;
								LandscapeEditSystem->CurrentBrush->EnterBrush();
								btn->ToolTip  = gcnew System::String(*NewBrush->GetTooltipString());
								btn->IsChecked = TRUE;
							}
							return;
						}
					}
				}

				check(FALSE && TEXT("No available brush index for this Tool type")); // should not be happened
			}

		}
	}

	void BrushButton_SelectionChanged( Object^ Owner, RoutedEventArgs^ Args )
	{
		Visual^ RootVisual = InteropWindow->RootVisual;
		UniformGrid^ BrushGrid = safe_cast< UniformGrid^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "BrushGrid" ) );

		CustomControls::ToolDropdownRadioButton^ Button = static_cast<CustomControls::ToolDropdownRadioButton^>(Owner);
		INT ButtonIndex = BrushGrid->Children->IndexOf(Button);
		if( ButtonIndex != -1 )
		{
			FLandscapeBrushSet& BrushSet = LandscapeEditSystem->LandscapeBrushSets(ButtonIndex);

			if( BrushSet.Brushes.IsValidIndex(Button->SelectedIndex) )
			{
				FLandscapeBrush* NewBrush = BrushSet.Brushes(Button->SelectedIndex);
				if( LandscapeEditSystem->CurrentBrush != NewBrush )
				{
					LandscapeEditSystem->CurrentBrush->LeaveBrush();
					LandscapeEditSystem->CurrentBrush = NewBrush;
					LandscapeEditSystem->CurrentBrushIndex = ButtonIndex;
					LandscapeEditSystem->CurrentBrush->EnterBrush();
					if (LandscapeEditSystem->CurrentToolSet)
					{
						LandscapeEditSystem->CurrentToolSet->PreviousBrushIndex = ButtonIndex;
					}
					Button->ToolTip  = gcnew System::String(*NewBrush->GetTooltipString());

					// Change Tool Index
					{
						//UniformGrid^ ToolGrid = safe_cast< UniformGrid^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ToolGrid" ) );
						INT ToolIndex = LandscapeEditSystem->CurrentToolIndex;

						if (ToolIndex >= 0 && ToolIndex < LandscapeEditSystem->LandscapeToolSets.Num())
						{
							FLandscapeToolSet* ToolSet = LandscapeEditSystem->LandscapeToolSets(ToolIndex);
							CustomControls::ImageRadioButton^ btn = safe_cast< CustomControls::ImageRadioButton^ >(LogicalTreeHelper::FindLogicalNode( RootVisual, gcnew System::String(ToolSet->GetToolSetName())) );
							if (btn->Visibility == Visibility::Visible)
							{
								// No tool select change
								return;
							}
						}

						for( INT ToolIdx=0;ToolIdx<LandscapeEditSystem->LandscapeToolSets.Num();ToolIdx++ )
						{
							FLandscapeToolSet* ToolSet = LandscapeEditSystem->LandscapeToolSets(ToolIdx);
							CustomControls::ImageRadioButton^ btn = safe_cast< CustomControls::ImageRadioButton^ >(LogicalTreeHelper::FindLogicalNode( RootVisual, gcnew System::String(ToolSet->GetToolSetName())) );
							if (btn->Visibility == Visibility::Visible)
							{
								LandscapeEditSystem->SetCurrentTool(ToolIdx);
								return;
							}
						}

						check(FALSE && TEXT("No available tool index for this Brush type")); // should not be happened
					}
				}
			}
		}
	}

public:

	/** INotifyPropertyChanged: Exposed event from INotifyPropertyChanged::PropertyChanged */
	virtual event PropertyChangedEventHandler^ PropertyChanged;


	/** Refresh all properties */
	void RefreshAllProperties()
	{
		// Pass null here which tells WPF that any or all properties may have changed
		//OnPropertyChanged( nullptr );
		UpdateTargets();
	}

	void UpdateLandscapeList()
	{
		//LandscapeComboBox->SelectedIndex = -1;
		INT CurrentIndex = LandscapeEditSystem->UpdateLandscapeList();
		LandscapeListsProperty->NotifyChanged();
		LandscapeComboBox->SelectedIndex = CurrentIndex;		// Ordering is important because of event trigger order
		UpdateTargets();
	}

	void UpdateTargets()
	{
		LandscapeEditSystem->UpdateTargetList();
		INT SelectedIndex = TargetListBox->SelectedIndex;
		TArray<FLandscapeTargetListInfo>* TargetList = LandscapeEditSystem->GetTargetList();
		if (SelectedIndex < 0 && TargetList->Num() > 0)
		{
			SelectedIndex = 0;
			for (int i = 1 ; i < TargetList->Num(); ++i)
			{
				if ((*TargetList)(i).bSelected)
				{
					SelectedIndex = i;
					break;
				}
			}
		}
		LandscapeTargetsProperty->NotifyChanged();
		TargetListBox->SelectedIndex = SelectedIndex;
		for (int i = 0 ; i < TargetList->Num(); ++i)
		{
			(*TargetList)(i).bSelected = (i == SelectedIndex);
			if (i == SelectedIndex)
			{
				LandscapeEditSystem->CurrentToolTarget.TargetType = (i > 0) ? LET_Weightmap : LET_Heightmap;
				LandscapeEditSystem->CurrentToolTarget.LayerName  = FName(*(*TargetList)(i).TargetName);
			}
		}
	}

	void NotifyCurrentToolChanged( String^ ToolSetName )
	{
		Visual^ RootVisual = InteropWindow->RootVisual;
		CustomControls::ImageRadioButton^ btn = safe_cast< CustomControls::ImageRadioButton^ >(LogicalTreeHelper::FindLogicalNode( RootVisual, ToolSetName) );
		btn->IsChecked = TRUE;
	}

	void NotifyMaskEnableChanged( bool MaskEnabled )
	{
		bMaskEnabled = MaskEnabled;
		MaskEnableCheckBox->IsChecked = MaskEnabled;
		if (MaskEnabled == false)
		{
			bUseNegativeMask = TRUE;
			InvertMaskCheckBox->IsChecked = TRUE;
		}
	}

	void NotifyBrushSizeChanged( float Radius )
	{
		BrushRadiusSlider->Value = Radius;
	}

	void NotifyBrushComponentSizeChanged( int Size )
	{
		BrushSizeSlider->Value = Size;
	}

	/** Called when a property has changed */
	virtual void OnPropertyChanged( String^ Info )
	{
		PropertyChanged( this, gcnew PropertyChangedEventArgs( Info ) );
	}

	/** Copy the window position back to the edit mode before the window closes */
	void SaveWindowSettings()
	{
		RECT rect;
		GetWindowRect( GetWindowHandle(), &rect );	
		LandscapeEditSystem->UISettings.SetWindowSizePos(rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top);
	}

protected:

	/** Pointer to the landscape edit system that owns us */
	FEdModeLandscape* LandscapeEditSystem;

	/** Windows message hook - handle resize border */
	virtual IntPtr VirtualMessageHookFunction( IntPtr HWnd, int Msg, IntPtr WParam, IntPtr LParam, bool% OutHandled ) override
	{
		const INT FakeTopBottomResizeHeight = 5;

		OutHandled = false;
		int RetVal = 0;

		switch( Msg )
		{
		case WM_NCHITTEST:
			// Override the client area detection to allow resizing with the top and bottom border.
			{
				if( FakeTopBottomResizeHeight > 0 )
				{
					const HWND NativeHWnd = ( HWND )( PTRINT )HWnd;
					const LPARAM NativeWParam = ( WPARAM )( PTRINT )WParam;
					const LPARAM NativeLParam = ( WPARAM )( PTRINT )LParam;

					// Let Windows perform the true hit test
					RetVal = DefWindowProc( NativeHWnd, Msg, NativeWParam, NativeLParam );

					// Did the user click in the client area?
					if( RetVal == HTCLIENT )
					{
						// Grab the size of our window
						RECT WindowRect;
						::GetWindowRect( NativeHWnd, &WindowRect );

						int CursorXPos = GET_X_LPARAM( NativeLParam );
						int CursorYPos = GET_Y_LPARAM( NativeLParam );

						if( FakeTopBottomResizeHeight > 0 && CursorYPos >= WindowRect.top && CursorYPos < WindowRect.top + FakeTopBottomResizeHeight )
						{
							// Trick Windows into thinking the user interacted with top resizing border
							RetVal = HTTOP;
							OutHandled = true;
						}
						else
						if( FakeTopBottomResizeHeight > 0 && CursorYPos <= WindowRect.bottom && CursorYPos > WindowRect.bottom - FakeTopBottomResizeHeight )
						{
							// Trick Windows into thinking the user interacted with bottom resizing border
							RetVal = HTBOTTOM;
							OutHandled = true;
						}
					}
				}
			}
			break;
		case WM_GETMINMAXINFO:
			// Limit minimum vertical size to 150 pixels
			{
				const HWND NativeHWnd = ( HWND )( PTRINT )HWnd;
				const LPARAM NativeWParam = ( WPARAM )( PTRINT )WParam;
				const LPARAM NativeLParam = ( WPARAM )( PTRINT )LParam;
				RetVal = DefWindowProc( NativeHWnd, Msg, NativeWParam, NativeLParam );
				OutHandled = true;

				((MINMAXINFO*)(NativeLParam))->ptMinTrackSize.y = 150;
			}
			break;
		case WM_MOUSEACTIVATE:
			// Bring window to front when clicking on it.
			{
				const HWND NativeHWnd = ( HWND )( PTRINT )HWnd;
				BringWindowToTop(NativeHWnd);
			}
			break;
		case WM_HOTKEY:
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
			{
				TextBox^ FocusedTextBox = dynamic_cast< TextBox^ >(FocusManager::GetFocusedElement(InteropWindow->RootVisual));
				if ( FocusedTextBox == nullptr )
				{
					const LPARAM NativeWParam = ( WPARAM )( PTRINT )WParam;
					const LPARAM NativeLParam = ( WPARAM )( PTRINT )LParam;
					UINT KeyCode = NativeWParam;
					LandscapeEditSystem->AddWindowMessage(Msg, NativeWParam, NativeLParam);
				}
			}
			break;
		}

		if( OutHandled )
		{
			return (IntPtr)RetVal;
		}

		return MWPFWindowWrapper::VirtualMessageHookFunction( HWnd, Msg, WParam, LParam, OutHandled );
	}
};



/** Static: Allocate and initialize landscape edit window */
FLandscapeEditWindow* FLandscapeEditWindow::CreateLandscapeEditWindow( FEdModeLandscape* InLandscapeEditSystem, const HWND InParentWindowHandle )
{
	FLandscapeEditWindow* NewLandscapeEditWindow = new FLandscapeEditWindow();

	if( !NewLandscapeEditWindow->InitLandscapeEditWindow( InLandscapeEditSystem, InParentWindowHandle ) )
	{
		delete NewLandscapeEditWindow;
		return NULL;
	}

	return NewLandscapeEditWindow;
}



/** Constructor */
FLandscapeEditWindow::FLandscapeEditWindow()
{
	// Register to find out about other windows going modal
	GCallbackEvent->Register( CALLBACK_EditorPreModal, this );
	GCallbackEvent->Register( CALLBACK_EditorPostModal, this );
	GCallbackEvent->Register( CALLBACK_MapChange, this );
	GCallbackEvent->Register( CALLBACK_WorldChange, this );
	GCallbackEvent->Register( CALLBACK_ObjectPropertyChanged, this );
	GCallbackEvent->Register( CALLBACK_PostLandscapeLayerUpdated, this );
}



/** Destructor */
FLandscapeEditWindow::~FLandscapeEditWindow()
{
	// Unregister callbacks
	GCallbackEvent->UnregisterAll( this );

	// @todo WPF: This is probably redundant, but I'm still not sure if AutoGCRoot destructor will get
	//   called when native code destroys an object that has a non-virtual (or no) destructor

	// Dispose of WindowControl
	WindowControl.reset();
}



/** Initialize the landscape edit window */
UBOOL FLandscapeEditWindow::InitLandscapeEditWindow( FEdModeLandscape* InLandscapeEditSystem, const HWND InParentWindowHandle )
{
	WindowControl = gcnew MLandscapeEditWindow(InLandscapeEditSystem);

	UBOOL bSuccess = WindowControl->InitLandscapeEditWindow( InParentWindowHandle );

	return bSuccess;
}



/** Refresh all properties */
void FLandscapeEditWindow::RefreshAllProperties()
{
	WindowControl->RefreshAllProperties();
}



/** Saves window settings to the Landscape Edit settings structure */
void FLandscapeEditWindow::SaveWindowSettings()
{
	WindowControl->SaveWindowSettings();
}



/** Returns true if the mouse cursor is over the landscape edit window */
UBOOL FLandscapeEditWindow::IsMouseOverWindow()
{
	if( WindowControl.get() != nullptr )
	{
		FrameworkElement^ WindowContentElement = safe_cast<FrameworkElement^>( WindowControl->GetRootVisual() );
		if( WindowContentElement->IsMouseOver )
		{
			return TRUE;
		}
	}

	return FALSE;
}

/** FCallbackEventDevice: Called when a parameterless global event we've registered for is fired */
void FLandscapeEditWindow::Send( ECallbackEventType Event )
{
	if( WindowControl.get() != nullptr )
	{
		FrameworkElement^ WindowContentElement = safe_cast<FrameworkElement^>( WindowControl->GetRootVisual() );

		switch ( Event )
		{
			case CALLBACK_EditorPreModal:
				WindowContentElement->IsEnabled = false;
				break;

			case CALLBACK_EditorPostModal:
				WindowContentElement->IsEnabled = true;
				break;

			case CALLBACK_MapChange:
			case CALLBACK_WorldChange:
			case CALLBACK_ObjectPropertyChanged:
				WindowControl->UpdateLandscapeList();
				break;
			case CALLBACK_PostLandscapeLayerUpdated:
				WindowControl->UpdateTargets();
				break;
		}
	}
}

/* User changed the current tool - update the button state */
void FLandscapeEditWindow::NotifyCurrentToolChanged( const FString& ToolSetName )
{
	if( WindowControl.get() != nullptr )
	{
		return WindowControl->NotifyCurrentToolChanged(CLRTools::ToString(ToolSetName));
	}
}

/* User changed the current tool - update the button state */
void FLandscapeEditWindow::NotifyMaskEnableChanged( UBOOL bMaskEnable )
{
	if( WindowControl.get() != nullptr )
	{
		return WindowControl->NotifyMaskEnableChanged(bMaskEnable ? true : false);
	}
}

/* User changed the current tool - update the button state */
void FLandscapeEditWindow::NotifyBrushSizeChanged( FLOAT Radius )
{
	if( WindowControl.get() != nullptr )
	{
		return WindowControl->NotifyBrushSizeChanged( Radius );
	}
}

/* User changed the current tool - update the button state */
void FLandscapeEditWindow::NotifyBrushComponentSizeChanged( INT Size )
{
	if( WindowControl.get() != nullptr )
	{
		return WindowControl->NotifyBrushComponentSizeChanged( Size );
	}
}

