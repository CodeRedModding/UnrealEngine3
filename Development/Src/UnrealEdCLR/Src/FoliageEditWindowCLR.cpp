/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEdCLR.h"
#include "ConvertersCLR.h"
#include "ManagedCodeSupportCLR.h"
#include "FoliageEditWindowShared.h"
#include "WPFWindowWrapperCLR.h"
#include "WPFFrameCLR.h"
#include "ThumbnailToolsCLR.h"

using namespace System;
using namespace System::ComponentModel;
using namespace System::Windows::Controls::Primitives;
using namespace System::Windows::Media::Imaging;
using namespace System::Windows::Input;
using namespace System::Deployment;

#pragma unmanaged
#include "FoliageEdMode.h"
#include "EngineFoliageClasses.h"
#pragma managed

#define DECLARE_FOLIAGE_PROPERTY( InPropertyType, InPropertyName ) \
		property InPropertyType InPropertyName \
		{ \
			InPropertyType get() { return FoliageEditSystem->UISettings.Get##InPropertyName(); } \
			void set( InPropertyType Value ) \
			{ \
				if( FoliageEditSystem->UISettings.Get##InPropertyName() != Value ) \
				{ \
					FoliageEditSystem->UISettings.Set##InPropertyName(Value); \
				} \
			} \
		}


#define DECLARE_FOLIAGE_MESH_PROPERTY( InPropertyType, InPropertyName ) \
		property InPropertyType InPropertyName { InPropertyType get() { return settings->InPropertyName; } void set(InPropertyType value) { settings->InPropertyName = value; } }

#define DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( InPropertyName ) \
	property bool InPropertyName { bool get() { return settings->InPropertyName?true:false; }	void set(bool value) { settings->InPropertyName = value ? 1 : 0; } }

#define DECLARE_FOLIAGE_MESH_CLUSTER_PROPERTY_BOOL( InPropertyName ) \
	property bool InPropertyName { bool get() { return settings->InPropertyName; }	void set(bool value) { settings->InPropertyName = value;  UpdateClusterSettings(); } }

/**
 * MFoliageMeshWrapper: Managed wrapper for FFoliageMeshUISettings
 */
ref class MFoliageMeshWrapper : public INotifyPropertyChanged
{
	int index;
	FFoliageMeshUIInfo& mesh;
	UInstancedFoliageSettings* settings;
	BitmapSource^ bitmap;
public:
	virtual event PropertyChangedEventHandler^ PropertyChanged;

	MFoliageMeshWrapper( INT InIndex, FFoliageMeshUIInfo& InMesh )
	:	index(InIndex)
	,	mesh(InMesh)
	,	settings(mesh.MeshInfo->Settings)
	{
		bitmap = ThumbnailToolsCLR::GetBitmapSourceForObject(mesh.StaticMesh);
	}

	// Main properties
	UStaticMesh* GetStaticMesh() { return mesh.StaticMesh; }
	property int Index { int get() { return index; } }
	property BitmapSource^ Bitmap { BitmapSource^ get() { return bitmap; } }
	property String^ StaticMeshName { 
		String^ get() 
		{ 
			return CLRTools::ToString(mesh.StaticMesh->GetName()); 
		} 
	}
	property String^ SettingsObjectName { 
		String^ get() 
		{ 
			return CLRTools::ToString(settings->GetOuter()->IsA(UPackage::StaticClass()) ? *settings->GetPathName() : LocalizeUnrealEd(TEXT("FoliageMode_NotSharedSettingsObject"))); 
		} 
	}
	property bool IsUsingSharedSettingsObject { bool get() {return settings->GetOuter()->IsA(UPackage::StaticClass()) ? true : false; } }

	// Painting properties
	DECLARE_FOLIAGE_MESH_PROPERTY( float, Density )
	DECLARE_FOLIAGE_MESH_PROPERTY( float, Radius )
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( AlignToNormal )
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( RandomYaw )
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( UniformScale )
	DECLARE_FOLIAGE_MESH_PROPERTY( float, ScaleMinX )
	DECLARE_FOLIAGE_MESH_PROPERTY( float, ScaleMinY )
	DECLARE_FOLIAGE_MESH_PROPERTY( float, ScaleMinZ )
	DECLARE_FOLIAGE_MESH_PROPERTY( float, ScaleMaxX )
	DECLARE_FOLIAGE_MESH_PROPERTY( float, ScaleMaxY )
	DECLARE_FOLIAGE_MESH_PROPERTY( float, ScaleMaxZ )
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( LockScaleX )
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( LockScaleY )
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( LockScaleZ )
	DECLARE_FOLIAGE_MESH_PROPERTY( float, ZOffsetMin )
	DECLARE_FOLIAGE_MESH_PROPERTY( float, ZOffsetMax )
	DECLARE_FOLIAGE_MESH_PROPERTY( float, AlignMaxAngle )
	DECLARE_FOLIAGE_MESH_PROPERTY( float, RandomPitchAngle )
	DECLARE_FOLIAGE_MESH_PROPERTY( float, GroundSlope )
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( IsSelected )
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( ShowNothing )
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( ShowPaintSettings )
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( ShowInstanceSettings )

	// Reapply properties
	DECLARE_FOLIAGE_MESH_PROPERTY( float, ReapplyDensityAmount )	
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( ReapplyDensity )
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( ReapplyRadius )
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( ReapplyAlignToNormal )
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( ReapplyRandomYaw )
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( ReapplyScaleX )
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( ReapplyScaleY )
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( ReapplyScaleZ )
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( ReapplyZOffset )	
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( ReapplyRandomPitchAngle )
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( ReapplyGroundSlope )
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( ReapplyHeight )
	DECLARE_FOLIAGE_MESH_PROPERTY_BOOL( ReapplyLandscapeLayer )	

	property String^ HeightMin	{ 
		String^ get() { return CLRTools::ToString(settings->HeightMin==-HALF_WORLD_MAX ? FString(TEXT("")) : FString::Printf(TEXT("%f"), settings->HeightMin)); }
		void set(String^ value) { settings->HeightMin = value->Length == 0 ? -HALF_WORLD_MAX : (FLOAT)Convert::ToDouble(value); }
	}
	property String^ HeightMax	{ 
		String^ get() { return CLRTools::ToString(settings->HeightMax==HALF_WORLD_MAX ? FString(TEXT("")) : FString::Printf(TEXT("%1.0f"), settings->HeightMax)); }
		void set(String^ value) { settings->HeightMax = value->Length == 0 ? HALF_WORLD_MAX : (FLOAT)Convert::ToDouble(value); }
	}
	property String^ LandscapeLayer { 
		String^ get() { return settings->LandscapeLayer==NAME_None ? gcnew String("") : CLRTools::ToString(settings->LandscapeLayer.ToString()); } 
		void set(String^ value) { settings->LandscapeLayer = FName(*CLRTools::ToFString(value)); }
	}

	// Cluster settings
	property int MaxInstancesPerCluster	{ int get() { return settings->MaxInstancesPerCluster; }	void set(int value) { settings->MaxInstancesPerCluster = value; ReallocateClusters(); } }
	property float MaxClusterRadius		{ float get() { return settings->MaxClusterRadius; }		void set(float value) { settings->MaxClusterRadius = value; ReallocateClusters(); } }
	property int StartCullDistance		{ int get() { return settings->StartCullDistance; }			void set(int value) { settings->StartCullDistance = value; UpdateClusterSettings(); } }
	property int EndCullDistance		{ int get() { return settings->EndCullDistance; }			void set(int value) { settings->EndCullDistance = value; UpdateClusterSettings(); } }
	property int DetailMode				{ int get() { return settings->DetailMode; }				void set(int value) { settings->DetailMode = value; ReallocateClusters(); } }


	DECLARE_FOLIAGE_MESH_CLUSTER_PROPERTY_BOOL( CastShadow )
	DECLARE_FOLIAGE_MESH_CLUSTER_PROPERTY_BOOL( bCastDynamicShadow )
	DECLARE_FOLIAGE_MESH_CLUSTER_PROPERTY_BOOL( bCastStaticShadow )
	DECLARE_FOLIAGE_MESH_CLUSTER_PROPERTY_BOOL( bSelfShadowOnly )
	DECLARE_FOLIAGE_MESH_CLUSTER_PROPERTY_BOOL( bNoModSelfShadow )
	DECLARE_FOLIAGE_MESH_CLUSTER_PROPERTY_BOOL( bAcceptsDynamicDominantLightShadows )
	DECLARE_FOLIAGE_MESH_CLUSTER_PROPERTY_BOOL( bCastHiddenShadow )
	DECLARE_FOLIAGE_MESH_CLUSTER_PROPERTY_BOOL( bCastShadowAsTwoSided )
	DECLARE_FOLIAGE_MESH_CLUSTER_PROPERTY_BOOL( bAcceptsLights )
	DECLARE_FOLIAGE_MESH_CLUSTER_PROPERTY_BOOL( bAcceptsDynamicLights )
	DECLARE_FOLIAGE_MESH_CLUSTER_PROPERTY_BOOL( bUseOnePassLightingOnTranslucency )
	DECLARE_FOLIAGE_MESH_CLUSTER_PROPERTY_BOOL( bUsePrecomputedShadows )
	
	DECLARE_FOLIAGE_MESH_CLUSTER_PROPERTY_BOOL( bCollideActors )
	DECLARE_FOLIAGE_MESH_CLUSTER_PROPERTY_BOOL( bBlockActors )
	DECLARE_FOLIAGE_MESH_CLUSTER_PROPERTY_BOOL( bBlockNonZeroExtent )
	DECLARE_FOLIAGE_MESH_CLUSTER_PROPERTY_BOOL( bBlockZeroExtent )

	// Current instance stats
	property int InstanceCount			{ int get() { return mesh.MeshInfo->Instances.Num() - mesh.MeshInfo->FreeInstanceIndices.Num(); } }
	property int ClusterCount			{ int get() { return mesh.MeshInfo->InstanceClusters.Num(); } }

private:

	void UpdateClusterSettings()
	{
		mesh.MeshInfo->UpdateClusterSettings(AInstancedFoliageActor::GetInstancedFoliageActor()); 
	}

	void ReallocateClusters()
	{
		mesh.MeshInfo->ReallocateClusters(AInstancedFoliageActor::GetInstancedFoliageActor(), mesh.StaticMesh); 
		PropertyChanged(this, gcnew PropertyChangedEventArgs("InstanceCount"));
		PropertyChanged(this, gcnew PropertyChangedEventArgs("ClusterCount"));
	}
};

typedef MEnumerableTArrayWrapper<MFoliageMeshWrapper,FFoliageMeshUIInfo> MFoliageMeshes;


/**
 * Foliage Edit window control (managed)
 */
ref class MFoliageEditWindow
	: public MWPFWindowWrapper,
	  public ComponentModel::INotifyPropertyChanged
{
public:

	// Properties
	DECLARE_FOLIAGE_PROPERTY(bool, PaintToolSelected);
	DECLARE_FOLIAGE_PROPERTY(bool, ReapplyToolSelected);
	DECLARE_FOLIAGE_PROPERTY(bool, SelectToolSelected);
	DECLARE_FOLIAGE_PROPERTY(bool, LassoSelectToolSelected);
	DECLARE_FOLIAGE_PROPERTY(bool, PaintBucketToolSelected);
	DECLARE_FOLIAGE_PROPERTY(bool, ReapplyPaintBucketToolSelected);
	DECLARE_FOLIAGE_PROPERTY(FLOAT, Radius);
	DECLARE_FOLIAGE_PROPERTY(FLOAT, PaintDensity);
	DECLARE_FOLIAGE_PROPERTY(FLOAT, UnpaintDensity);
	DECLARE_FOLIAGE_PROPERTY(bool, FilterLandscape);
	DECLARE_FOLIAGE_PROPERTY(bool, FilterStaticMesh);
	DECLARE_FOLIAGE_PROPERTY(bool, FilterBSP);
	DECLARE_FOLIAGE_PROPERTY(bool, FilterTerrain);
	
	MFoliageMeshes^ FoliageMeshesValue;
	DECLARE_MAPPED_NOTIFY_PROPERTY(MFoliageMeshes^, FoliageMeshesProperty, MFoliageMeshes^, FoliageMeshesValue);

	// tor
	MFoliageEditWindow( FEdModeFoliage* InFoliageEditSystem)
	:	FoliageEditSystem(InFoliageEditSystem)
	{
		check( InFoliageEditSystem != NULL );
	}

	/**
	 * Initialize the Foliage edit window
	 *
	 * @param	InFoliageEditSystem	Foliage edit system that owns us
	 * @param	InParentWindowHandle	Parent window handle
	 *
	 * @return	TRUE if successful
	 */
	UBOOL InitFoliageEditWindow( const HWND InParentWindowHandle )
	{
		String^ WindowTitle = CLRTools::LocalizeString( "FoliageEditWindow_WindowTitle" );
		String^ WPFXamlFileName = "FoliageEditWindow.xaml";

		// We draw our own title bar so tell the window about it's height
		const int FakeTitleBarHeight = 28;
		const UBOOL bIsTopMost = FALSE;

		// Read the saved size/position
		INT x,y,w,h;
		FoliageEditSystem->UISettings.GetWindowSizePos(x,y,w,h);

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

		// Register for property change callbacks from our properties object
		this->PropertyChanged += gcnew ComponentModel::PropertyChangedEventHandler( this, &MFoliageEditWindow::OnFoliageEditPropertyChanged );

		// Setup bindings
		Visual^ RootVisual = InteropWindow->RootVisual;

		FrameworkElement^ WindowContentElement = safe_cast<FrameworkElement^>(RootVisual);
		WindowContentElement->DataContext = this;

		// Close button
		Button^ CloseButton = safe_cast<Button^>( LogicalTreeHelper::FindLogicalNode( RootVisual, "TitleBarCloseButton" ) );
		CloseButton->Click += gcnew RoutedEventHandler( this, &MFoliageEditWindow::OnClose );
		FakeTitleBarButtonWidth += CloseButton->ActualWidth;

		// Assign tool bitmaps
		CustomControls::ImageRadioButton^ btnPaint = safe_cast< CustomControls::ImageRadioButton^ >(LogicalTreeHelper::FindLogicalNode( RootVisual, "Tool_Paint" ) );
		btnPaint->CheckedImage   = gcnew BitmapImage( gcnew Uri( gcnew System::String(*FString::Printf(TEXT("%swxres\\Landscape_Tool_Paint_active.png"), *GetEditorResourcesDir())), UriKind::Absolute) );
		btnPaint->UncheckedImage = gcnew BitmapImage( gcnew Uri( gcnew System::String(*FString::Printf(TEXT("%swxres\\Landscape_Tool_Paint_inactive.png"), *GetEditorResourcesDir())), UriKind::Absolute) );
		UnrealEd::Utils::CreateBinding(btnPaint, CustomControls::ImageRadioButton::IsActuallyCheckedProperty, this, "PaintToolSelected" );
		btnPaint->Click += gcnew RoutedEventHandler( this, &MFoliageEditWindow::ToolButton_Click );

		CustomControls::ImageRadioButton^ btnReapply = safe_cast< CustomControls::ImageRadioButton^ >(LogicalTreeHelper::FindLogicalNode( RootVisual, "Tool_Reapply" ) );
		btnReapply->CheckedImage   = gcnew BitmapImage( gcnew Uri( gcnew System::String(*FString::Printf(TEXT("%swxres\\Landscape_Tool_Reapply_active.png"), *GetEditorResourcesDir())), UriKind::Absolute) );
		btnReapply->UncheckedImage = gcnew BitmapImage( gcnew Uri( gcnew System::String(*FString::Printf(TEXT("%swxres\\Landscape_Tool_Reapply_inactive.png"), *GetEditorResourcesDir())), UriKind::Absolute) );
		UnrealEd::Utils::CreateBinding(btnReapply, CustomControls::ImageRadioButton::IsActuallyCheckedProperty, this, "ReapplyToolSelected" );
		btnReapply->Click += gcnew RoutedEventHandler( this, &MFoliageEditWindow::ToolButton_Click );

		CustomControls::ImageRadioButton^ btnSelect = safe_cast< CustomControls::ImageRadioButton^ >(LogicalTreeHelper::FindLogicalNode( RootVisual, "Tool_Select" ) );
		btnSelect->CheckedImage   = gcnew BitmapImage( gcnew Uri( gcnew System::String(*FString::Printf(TEXT("%swxres\\Landscape_Tool_Selection_active.png"), *GetEditorResourcesDir())), UriKind::Absolute) );
		btnSelect->UncheckedImage = gcnew BitmapImage( gcnew Uri( gcnew System::String(*FString::Printf(TEXT("%swxres\\Landscape_Tool_Selection_inactive.png"), *GetEditorResourcesDir())), UriKind::Absolute) );
		UnrealEd::Utils::CreateBinding(btnSelect, CustomControls::ImageRadioButton::IsActuallyCheckedProperty, this, "SelectToolSelected" );
		btnSelect->Click += gcnew RoutedEventHandler( this, &MFoliageEditWindow::ToolButton_Click );

		CustomControls::ImageRadioButton^ btnLassoSelect = safe_cast< CustomControls::ImageRadioButton^ >(LogicalTreeHelper::FindLogicalNode( RootVisual, "Tool_LassoSelect" ) );
		btnLassoSelect->CheckedImage   = gcnew BitmapImage( gcnew Uri( gcnew System::String(*FString::Printf(TEXT("%swxres\\Landscape_Tool_Mask_active.png"), *GetEditorResourcesDir())), UriKind::Absolute) );
		btnLassoSelect->UncheckedImage = gcnew BitmapImage( gcnew Uri( gcnew System::String(*FString::Printf(TEXT("%swxres\\Landscape_Tool_Mask_inactive.png"), *GetEditorResourcesDir())), UriKind::Absolute) );
		UnrealEd::Utils::CreateBinding(btnLassoSelect, CustomControls::ImageRadioButton::IsActuallyCheckedProperty, this, "LassoSelectToolSelected" );
		btnLassoSelect->Click += gcnew RoutedEventHandler( this, &MFoliageEditWindow::ToolButton_Click );

		CustomControls::ImageRadioButton^ btnPaintBucket = safe_cast< CustomControls::ImageRadioButton^ >(LogicalTreeHelper::FindLogicalNode( RootVisual, "Tool_PaintBucket" ) );
		btnPaintBucket->CheckedImage   = gcnew BitmapImage( gcnew Uri( gcnew System::String(*FString::Printf(TEXT("%swxres\\Landscape_Tool_PaintBucket_active.png"), *GetEditorResourcesDir())), UriKind::Absolute) );
		btnPaintBucket->UncheckedImage = gcnew BitmapImage( gcnew Uri( gcnew System::String(*FString::Printf(TEXT("%swxres\\Landscape_Tool_PaintBucket_inactive.png"), *GetEditorResourcesDir())), UriKind::Absolute) );
		UnrealEd::Utils::CreateBinding(btnPaintBucket, CustomControls::ImageRadioButton::IsActuallyCheckedProperty, this, "PaintBucketToolSelected" );
		btnPaintBucket->Click += gcnew RoutedEventHandler( this, &MFoliageEditWindow::ToolButton_Click );

		CustomControls::ImageRadioButton^ btnReapplyPaintBucket = safe_cast< CustomControls::ImageRadioButton^ >(LogicalTreeHelper::FindLogicalNode( RootVisual, "Tool_ReapplyPaintBucket" ) );
		btnReapplyPaintBucket->CheckedImage   = gcnew BitmapImage( gcnew Uri( gcnew System::String(*FString::Printf(TEXT("%swxres\\Landscape_Tool_PaintBucketReapply_active.png"), *GetEditorResourcesDir())), UriKind::Absolute) );
		btnReapplyPaintBucket->UncheckedImage = gcnew BitmapImage( gcnew Uri( gcnew System::String(*FString::Printf(TEXT("%swxres\\Landscape_Tool_PaintBucketReapply_inactive.png"), *GetEditorResourcesDir())), UriKind::Absolute) );
		UnrealEd::Utils::CreateBinding(btnReapplyPaintBucket, CustomControls::ImageRadioButton::IsActuallyCheckedProperty, this, "ReapplyPaintBucketToolSelected" );
		btnReapplyPaintBucket->Click += gcnew RoutedEventHandler( this, &MFoliageEditWindow::ToolButton_Click );

		// Bind controls
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "BrushRadiusSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "Radius" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PaintDensitySlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "PaintDensity" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "UnpaintDensitySlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "UnpaintDensity" );

		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "FilterLandscapeCheckbox" ) ),
			CheckBox::IsCheckedProperty, this, "FilterLandscape" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "FilterStaticMeshCheckbox" ) ),
			CheckBox::IsCheckedProperty, this, "FilterStaticMesh" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "FilterBSPCheckbox" ) ),
			CheckBox::IsCheckedProperty, this, "FilterBSP" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "FilterTerrainCheckbox" ) ),
			CheckBox::IsCheckedProperty, this, "FilterTerrain" );

		// Meshes
		FoliageMeshesValue = gcnew MFoliageMeshes( &FoliageEditSystem->GetFoliageMeshList() );
		ListBox^ FoliageMeshesListBox = safe_cast< ListBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "FoliageMeshesListBox" ) );
		UnrealEd::Utils::CreateBinding(FoliageMeshesListBox, ListBox::ItemsSourceProperty, this, "FoliageMeshesProperty");

		GroupBox^ FoliageMeshesGroupBox = safe_cast<GroupBox^>(LogicalTreeHelper::FindLogicalNode( RootVisual, "FoliageMeshesGroupBox"));
		FoliageMeshesGroupBox->AllowDrop = true;
		FoliageMeshesGroupBox->DragOver += gcnew DragEventHandler( this, &MFoliageEditWindow::OnDragOver );
		FoliageMeshesGroupBox->Drop += gcnew DragEventHandler( this, &MFoliageEditWindow::OnDrop );
		FoliageMeshesGroupBox->DragEnter += gcnew DragEventHandler( this, &MFoliageEditWindow::OnDragEnter );
		FoliageMeshesGroupBox->DragLeave += gcnew DragEventHandler( this, &MFoliageEditWindow::OnDragLeave );

		// Replace
		RoutedCommand^ FoliageMeshReplaceCommand = safe_cast< RoutedCommand^ >(WindowContentElement->FindResource("FoliageMeshReplaceCommand"));
		WindowContentElement->CommandBindings->Add(
			gcnew CommandBinding(FoliageMeshReplaceCommand, gcnew ExecutedRoutedEventHandler( this, &MFoliageEditWindow::FoliageMeshReplaceButton_Click) ) );
		// Sync content browser
		RoutedCommand^ FoliageMeshSyncCommand = safe_cast< RoutedCommand^ >(WindowContentElement->FindResource("FoliageMeshSyncCommand"));
		WindowContentElement->CommandBindings->Add(
			gcnew CommandBinding(FoliageMeshSyncCommand, gcnew ExecutedRoutedEventHandler( this, &MFoliageEditWindow::FoliageMeshSync_Click) ) );
		// Remove mesh
		RoutedCommand^ FoliageMeshRemoveCommand = safe_cast< RoutedCommand^ >(WindowContentElement->FindResource("FoliageMeshRemoveCommand"));
		WindowContentElement->CommandBindings->Add(
			gcnew CommandBinding(FoliageMeshRemoveCommand, gcnew ExecutedRoutedEventHandler( this, &MFoliageEditWindow::FoliageMeshRemoveButton_Click) ) );

		// Use/Save/Copy Settings
		WindowContentElement->CommandBindings->Add(
			gcnew CommandBinding(safe_cast<RoutedCommand^>(WindowContentElement->FindResource("FoliageMeshUseSettings")),
			gcnew ExecutedRoutedEventHandler( this, &MFoliageEditWindow::FoliageMeshUseSettings_Click) ) );
		WindowContentElement->CommandBindings->Add(
			gcnew CommandBinding(safe_cast<RoutedCommand^>(WindowContentElement->FindResource("FoliageMeshCopySettings")),
			gcnew ExecutedRoutedEventHandler( this, &MFoliageEditWindow::FoliageMeshCopySettings_Click) ) );
		WindowContentElement->CommandBindings->Add(
			gcnew CommandBinding(safe_cast<RoutedCommand^>(WindowContentElement->FindResource("FoliageMeshSaveSettings")),
			gcnew ExecutedRoutedEventHandler( this, &MFoliageEditWindow::FoliageMeshSaveSettings_Click) ) );
	

		// Show the window!
		ShowWindow( true );

		return TRUE;
	}

	/** Called when an editor event such as new level or undo occurs, to recreate the mesh list. */
	void RefreshMeshList()
	{
		FoliageEditSystem->UpdateFoliageMeshList();
		FoliageMeshesProperty->NotifyChanged();
	}

	void NotifyNewCurrentLevel()
	{
		FoliageEditSystem->NotifyNewCurrentLevel();
	};

	/** Called from edit mode after painting to ensure calculated properties are up to date. */
	void RefreshMeshListProperties()
	{
		FoliageMeshesProperty->NotifyChanged();
	}

	/** INotifyPropertyChanged: Exposed event from INotifyPropertyChanged::PropertyChanged */
	virtual event ComponentModel::PropertyChangedEventHandler^ PropertyChanged;

	/** Copy the window position back to the edit mode before the window closes */
	void SaveWindowSettings()
	{
		RECT rect;
		GetWindowRect( GetWindowHandle(), &rect );	
		FoliageEditSystem->UISettings.SetWindowSizePos(rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top);
	}

	/** Refresh all properties */
	void RefreshAllProperties()
	{
		// Pass null here which tells WPF that any or all properties may have changed
		OnPropertyChanged( nullptr );
	}

protected:

	void OnClose( Object^ Owner, RoutedEventArgs^ Args )
	{
		// Deactivate foliage mode when the close button is pressed
		GEditorModeTools().DeactivateMode( EM_Foliage );		
	}

	void ToolButton_Click( Object^ Owner, RoutedEventArgs^ Args )
	{
		FoliageEditSystem->NotifyToolChanged();
	}

	/** Called when a landscape edit window property is changed */
	void OnFoliageEditPropertyChanged( Object^ Owner, PropertyChangedEventArgs^ Args )
	{
	}

	/** Array of dropped asset data for supporting drag-and-drop */
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
				if( CurInfo.ObjectClass != UStaticMesh::StaticClass() )
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

				UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(NULL, *CurInfo.ObjectPathName, NULL, LOAD_None, NULL);
				if( StaticMesh )
				{		
					// Add item
					FoliageEditSystem->AddFoliageMesh( StaticMesh );
					FoliageMeshesProperty->NotifyChanged();
				}
			}

			DroppedAssets->Empty();
			DroppedAssets.Reset(NULL);
		}

		Args->Handled = TRUE;
	}

	void FoliageMeshReplaceButton_Click( Object^ Owner, ExecutedRoutedEventArgs^ Args )
	{
		// Get current selection from content browser
		GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);
		USelection* SelectedSet = GEditor->GetSelectedSet( UStaticMesh::StaticClass() );
		UStaticMesh* SelectedStaticMesh = Cast<UStaticMesh>( SelectedSet->GetTop( UStaticMesh::StaticClass() ) );
		if( SelectedStaticMesh != NULL )
		{
			MFoliageMeshWrapper^ Mesh = safe_cast<MFoliageMeshWrapper^>(Args->Parameter);
			FoliageEditSystem->ReplaceStaticMesh(Mesh->GetStaticMesh(), SelectedStaticMesh);
			FoliageMeshesProperty->NotifyChanged();
		}
	}


	// Sync content browser
	void FoliageMeshSync_Click( Object^ Owner, ExecutedRoutedEventArgs^ Args )
	{
		MFoliageMeshWrapper^ Mesh = safe_cast<MFoliageMeshWrapper^>(Args->Parameter);
		TArray<UObject*> Objects;
		Objects.AddItem(Mesh->GetStaticMesh());
		GApp->EditorFrame->SyncBrowserToObjects(Objects);
	}

	// Remove the current mesh
	void FoliageMeshRemoveButton_Click( Object^ Owner, ExecutedRoutedEventArgs^ Args )
	{
		MFoliageMeshWrapper^ Mesh = safe_cast<MFoliageMeshWrapper^>(Args->Parameter);
		FoliageEditSystem->RemoveFoliageMesh(Mesh->GetStaticMesh());
		FoliageMeshesProperty->NotifyChanged();
	}

	void FoliageMeshUseSettings_Click( Object^ Owner, ExecutedRoutedEventArgs^ Args )
	{
		GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);
		USelection* SelectedSet = GEditor->GetSelectedSet( UInstancedFoliageSettings::StaticClass() );
		UInstancedFoliageSettings* SelectedSettings = Cast<UInstancedFoliageSettings>( SelectedSet->GetTop( UInstancedFoliageSettings::StaticClass() ) );
		if( SelectedSettings != NULL )
		{
			MFoliageMeshWrapper^ Mesh = safe_cast<MFoliageMeshWrapper^>(Args->Parameter);
			FoliageEditSystem->ReplaceSettingsObject(Mesh->GetStaticMesh(), SelectedSettings);
			FoliageMeshesProperty->NotifyChanged();
		}
	}

	void FoliageMeshCopySettings_Click( Object^ Owner, ExecutedRoutedEventArgs^ Args )
	{
		MFoliageMeshWrapper^ Mesh = safe_cast<MFoliageMeshWrapper^>(Args->Parameter);
		FoliageEditSystem->CopySettingsObject(Mesh->GetStaticMesh());
		FoliageMeshesProperty->NotifyChanged();
	}

	void FoliageMeshSaveSettings_Click( Object^ Owner, ExecutedRoutedEventArgs^ Args )
	{
		MFoliageMeshWrapper^ Mesh = safe_cast<MFoliageMeshWrapper^>(Args->Parameter);
		FoliageEditSystem->SaveSettingsObject(Mesh->GetStaticMesh());
		FoliageMeshesProperty->NotifyChanged();
	}

	void OnMeshDataChanged( Object^ Owner, ComponentModel::PropertyChangedEventArgs^ Args )
	{
	}

	/** Called when a property has changed */
	virtual void OnPropertyChanged( String^ Info )
	{
		PropertyChanged( this, gcnew ComponentModel::PropertyChangedEventArgs( Info ) );
	}

	/** Pointer to the Foliage edit system that owns us */
	FEdModeFoliage* FoliageEditSystem;

	/** Windows message hook - handle resize border */
	virtual IntPtr VirtualMessageHookFunction( IntPtr HWnd, int Msg, IntPtr WParam, IntPtr LParam, bool% OutHandled ) override
	{
		const INT FakeTopBottomResizeHeight = 5;

		OutHandled = false;
		int RetVal = 0;

		switch( Msg )
		{
		case WM_KILLFOCUS:
			{
				// Update any focused text box when this window loses focus.
				// Not sure why WPF doesn't catch this itself, but it seems to be interation with wx. 
				// This is necessary for changes you make to text boxes to be reflected if you immediately start painting.
				if( InteropWindow->RootVisual != nullptr )
				{
					TextBox^ FocusedTextBox = dynamic_cast< TextBox^ >(FocusManager::GetFocusedElement(InteropWindow->RootVisual));
					if ( FocusedTextBox != nullptr )
					{
						BindingExpression^ BindingExpr = FocusedTextBox->GetBindingExpression(FocusedTextBox->TextProperty);
						if( BindingExpr != nullptr )
						{
							BindingExpr->UpdateSource();
						}
					}
				}
			}
			break;
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
		}

		if( OutHandled )
		{
			return (IntPtr)RetVal;
		}

		return MWPFWindowWrapper::VirtualMessageHookFunction( HWnd, Msg, WParam, LParam, OutHandled );
	}
};



/** Static: Allocate and initialize Foliage edit window */
FFoliageEditWindow* FFoliageEditWindow::CreateFoliageEditWindow( FEdModeFoliage* InFoliageEditSystem, const HWND InParentWindowHandle )
{
	FFoliageEditWindow* NewFoliageEditWindow = new FFoliageEditWindow();

	if( !NewFoliageEditWindow->InitFoliageEditWindow( InFoliageEditSystem, InParentWindowHandle ) )
	{
		delete NewFoliageEditWindow;
		return NULL;
	}

	return NewFoliageEditWindow;
}



/** Constructor */
FFoliageEditWindow::FFoliageEditWindow()
{
	// Register to find out about other windows going modal
	GCallbackEvent->Register( CALLBACK_EditorPreModal, this );
	GCallbackEvent->Register( CALLBACK_EditorPostModal, this );
	GCallbackEvent->Register( CALLBACK_MapChange, this );
	GCallbackEvent->Register( CALLBACK_WorldChange, this );
	GCallbackEvent->Register( CALLBACK_ObjectPropertyChanged, this );
	GCallbackEvent->Register( CALLBACK_NewCurrentLevel, this );
	GCallbackEvent->Register( CALLBACK_Undo, this );
}



/** Destructor */
FFoliageEditWindow::~FFoliageEditWindow()
{
	// Unregister callbacks
	GCallbackEvent->UnregisterAll( this );

	// Dispose of WindowControl
	WindowControl.reset();
}



/** Initialize the Foliage edit window */
UBOOL FFoliageEditWindow::InitFoliageEditWindow( FEdModeFoliage* InFoliageEditSystem, const HWND InParentWindowHandle )
{
	WindowControl = gcnew MFoliageEditWindow(InFoliageEditSystem);

	UBOOL bSuccess = WindowControl->InitFoliageEditWindow( InParentWindowHandle );

	return bSuccess;
}

/** Refresh all properties */
void FFoliageEditWindow::RefreshAllProperties()
{
	WindowControl->RefreshAllProperties();
}


/** Saves window settings to the Foliage Edit settings structure */
void FFoliageEditWindow::SaveWindowSettings()
{
	WindowControl->SaveWindowSettings();
}

/** Returns true if the mouse cursor is over the Foliage edit window */
UBOOL FFoliageEditWindow::IsMouseOverWindow()
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
void FFoliageEditWindow::Send( ECallbackEventType Event )
{
	switch(Event)
	{
	case CALLBACK_Undo:
		WindowControl->RefreshMeshList();
		break;
	case CALLBACK_NewCurrentLevel:
		WindowControl->NotifyNewCurrentLevel();
		WindowControl->RefreshMeshList();
		break;
	default:
		break;
	}
}

/** Called from edit mode after painting to ensure calculated properties are up to date. */
void FFoliageEditWindow::RefreshMeshListProperties()
{
	WindowControl->RefreshMeshListProperties();
}

