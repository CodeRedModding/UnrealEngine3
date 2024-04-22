/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

#include "UnrealEd.h"
#include "Factories.h"
#include "PhAT.h"
#include "UnTerrain.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineSoundClasses.h"
#include "EngineAnimClasses.h"
#include "EngineAIClasses.h"
#include "EngineDecalClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineMaterialClasses.h"
#include "EnginePhysicsClasses.h"
#include "EnginePrefabClasses.h"
#include "EngineInterpolationClasses.h"
#include "EngineUIPrivateClasses.h"
#include "EngineProcBuildingClasses.h"
#include "EngineSoundClasses.h"
#include "EngineFoliageClasses.h"
#include "LensFlare.h"
#include "AnimSetViewer.h"
#include "UnLinkedObjEditor.h"
#include "SoundCueEditor.h"
#include "SoundClassEditor.h"
#include "DlgSoundNodeWaveOptions.h"
#include "DialogueManager.h"
#include "AnimTreeEditor.h"
#include "PostProcessEditor.h"
#include "UnrealEdPrivateClasses.h"			// required for declaration of UUILayerRoot
#include "Cascade.h"
#include "Facade.h"
#include "StaticMeshEditor.h"
#include "NewMaterialEditor.h"
#include "MaterialInstanceConstantEditor.h"
#include "MaterialInstanceTimeVaryingEditor.h"
#include "PropertyWindow.h"
#include "InterpEditor.h"
#include "LensFlareEditor.h"
#include "TrackableWindow.h"
#include "UnFracturedStaticMesh.h"
#include "EngineMeshClasses.h"
#include "UnObjectTools.h"
#include "ScopedTransaction.h"
#include "ExportMeshUtils.h"
#include "TextureStatsBrowser.h"
#include "PackageHelperFunctions.h"
#include "ApexEditorWidgets.h"
#include "ImageUtils.h"
#if WITH_MANAGED_CODE
#include "ConsolidateWindowShared.h"
#include "ContentBrowserShared.h"
#endif // #if WITH_MANAGED_CODE

#if WITH_SUBSTANCE_AIR == 1
#include "SubstanceAirTypedefs.h"
#include "SubstanceAirEdFactoryClasses.h"
#include "SubstanceAirEdHelpers.h"
#include "SubstanceAirTextureClasses.h"
#endif //WITH_SUBSTANCE_AIR

#if WITH_FACEFX_STUDIO
#include "../../../External/FaceFX/Studio/Main/Inc/FxStudioApp.h"
#include "../FaceFX/FxRenderWidgetUE3.h"
#endif // WITH_FACEFX_STUDIO

#include "..\..\Launch\Resources\resource.h"

#include "UnConsoleSupportContainer.h"

/*------------------------------------------------------------------------------
Implementations.
------------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UGenericBrowserType)
IMPLEMENT_CLASS(UGenericBrowserType_All)
IMPLEMENT_CLASS(UGenericBrowserType_AnimTree)
IMPLEMENT_CLASS(UGenericBrowserType_Animation)
IMPLEMENT_CLASS(UGenericBrowserType_Archetype)
IMPLEMENT_CLASS(UGenericBrowserType_CameraAnim)
IMPLEMENT_CLASS(UGenericBrowserType_CurveEdPresetCurve)
IMPLEMENT_CLASS(UGenericBrowserType_Custom)
IMPLEMENT_CLASS(UGenericBrowserType_DecalMaterial)
IMPLEMENT_CLASS(UGenericBrowserType_FaceFXAnimSet)
IMPLEMENT_CLASS(UGenericBrowserType_FaceFXAsset)
IMPLEMENT_CLASS(UGenericBrowserType_Font)
IMPLEMENT_CLASS(UGenericBrowserType_FracturedStaticMesh)
IMPLEMENT_CLASS(UGenericBrowserType_FractureMaterial)
IMPLEMENT_CLASS(UGenericBrowserType_LensFlare)
IMPLEMENT_CLASS(UGenericBrowserType_Material)
IMPLEMENT_CLASS(UGenericBrowserType_MaterialFunction)
IMPLEMENT_CLASS(UGenericBrowserType_MaterialInstanceConstant)
IMPLEMENT_CLASS(UGenericBrowserType_MaterialInstanceTimeVarying)
IMPLEMENT_CLASS(UGenericBrowserType_MorphTargetSet)
IMPLEMENT_CLASS(UGenericBrowserType_MorphWeightSequence)
IMPLEMENT_CLASS(UGenericBrowserType_ParticleSystem)
IMPLEMENT_CLASS(UGenericBrowserType_PhysicalMaterial)
IMPLEMENT_CLASS(UGenericBrowserType_PhysXParticleSystem)
IMPLEMENT_CLASS(UGenericBrowserType_ApexDestructibleAsset)
IMPLEMENT_CLASS(UGenericBrowserType_ApexClothingAsset)
IMPLEMENT_CLASS(UGenericBrowserType_ApexDestructibleDamageParameters)
IMPLEMENT_CLASS(UGenericBrowserType_ApexGenericAsset)
IMPLEMENT_CLASS(UGenericBrowserType_PhysicsAsset)
IMPLEMENT_CLASS(UGenericBrowserType_PostProcess);
IMPLEMENT_CLASS(UGenericBrowserType_Prefab)
IMPLEMENT_CLASS(UGenericBrowserType_ProcBuildingRuleset)
IMPLEMENT_CLASS(UGenericBrowserType_RenderTexture)
IMPLEMENT_CLASS(UGenericBrowserType_Sequence)
IMPLEMENT_CLASS(UGenericBrowserType_SkeletalMesh)
IMPLEMENT_CLASS(UGenericBrowserType_SoundCue)
IMPLEMENT_CLASS(UGenericBrowserType_SoundClass)
IMPLEMENT_CLASS(UGenericBrowserType_SoundMode)
IMPLEMENT_CLASS(UGenericBrowserType_SoundWave)
IMPLEMENT_CLASS(UGenericBrowserType_Sounds)
IMPLEMENT_CLASS(UGenericBrowserType_SpeechRecognition)
IMPLEMENT_CLASS(UGenericBrowserType_StaticMesh)
IMPLEMENT_CLASS(UGenericBrowserType_TemplateMapMetadata)
IMPLEMENT_CLASS(UGenericBrowserType_TerrainLayer)
IMPLEMENT_CLASS(UGenericBrowserType_TerrainMaterial)
IMPLEMENT_CLASS(UGenericBrowserType_Texture)
IMPLEMENT_CLASS(UGenericBrowserType_TextureCube)
IMPLEMENT_CLASS(UGenericBrowserType_TextureMovie)
IMPLEMENT_CLASS(UGenericBrowserType_LandscapeLayer)
IMPLEMENT_CLASS(UGenericBrowserType_InstancedFoliageSettings)

/*-----------------------------------------------------------------------------
WxPropertyQuickInfoWindow.
-----------------------------------------------------------------------------*/

/**
* Displays read-only properties of the object being referenced.  Used for texture dimensions.
*/
class WxPropertyQuickInfoWindow : public wxPanel, public FDeferredInitializationWindow
{
public:
	DECLARE_DYNAMIC_CLASS(WxPropertyQuickInfoWindow);

	/** Destroy a quick info window */
	virtual ~WxPropertyQuickInfoWindow(void);

	/**
	 *	Initialize this property window.  Must be the first function called after creation.
	 *
	 * @param	parent			The parent window.
	 */
	virtual void Create( wxWindow* parent);

	/** Removes all info strings */
	void ClearChildren();

	/**
	 *	Add a string of information to QuickInfoText
	 *
	 * @param	infoToAdd			The text to be displayed
	 */
	void AddInfoString( const wxString& infoToAdd );

	DECLARE_EVENT_TABLE();

private:
	/** Array of static text objects added to the wxSizer */
	TArray<wxStaticText*> LinesOfText;

};

IMPLEMENT_DYNAMIC_CLASS(WxPropertyQuickInfoWindow,wxPanel);

BEGIN_EVENT_TABLE( WxPropertyQuickInfoWindow, wxPanel )
END_EVENT_TABLE()

/** Destroy a quick info window */
WxPropertyQuickInfoWindow::~WxPropertyQuickInfoWindow(void)
{
}

/** 
 * Setup for listening for favorites changes
 *
 * @param parent				Parent window.  Should only be used by PropertyWindowHost
 */
void WxPropertyQuickInfoWindow::Create( wxWindow* InParent)
{
	const bool bWasCreationSuccessful = wxWindow::Create( InParent, -1, wxDefaultPosition, wxDefaultSize, wxCLIP_CHILDREN );

	SetSizer(new wxBoxSizer(wxVERTICAL));
}


/** Removes all info string */
void WxPropertyQuickInfoWindow::ClearChildren()
{
	for( INT i = 0; i < LinesOfText.Num(); i++ )
	{
		wxStaticText* CurrentSizer = LinesOfText( i );
		GetSizer()->Detach( CurrentSizer );
		CurrentSizer->Destroy();
	}
	LinesOfText.Empty();
}

/**
 *	Add a string of information to be displayed as static text
 *
 * @param	infoToAdd			The text to be displayed
 */
void WxPropertyQuickInfoWindow::AddInfoString( const wxString& infoToAdd )
{
	wxStaticText* TextToAdd = new wxStaticText(this, wxID_STATIC, infoToAdd, wxDefaultPosition, wxDefaultSize, 0);

	//Position this line downward by the total number of lines.  The *1.4 adds some space between each line
	TextToAdd->SetPosition(wxPoint(wxDefaultPosition.x, TextToAdd->GetSize().y * LinesOfText.Num() * 1.4));
	LinesOfText.Push( TextToAdd );

	GetSizer()->Add( TextToAdd, 0, wxEXPAND, 0 );
}

/*-----------------------------------------------------------------------------
WxDlgSoundCueProperties.
-----------------------------------------------------------------------------*/

/**
 * Trackable dialog class to allow the viewing of sound cue properties and
 * its attenuation nodes
 */
class WxDlgSoundCueProperties : public WxTrackableDialog, public FNotifyHook
{
	struct NodeSet
	{
		UClass* NodeType;
		TArray<USoundNode*> NodeList;
		WxPropertyWindowHost* SoundNodesPropWindow;

		NodeSet():NodeType(NULL), NodeList(), SoundNodesPropWindow(NULL){}
	};

	/** List of NodeSets, in one NodeSet all nodes of the same class are kept */
	TArray<NodeSet*> NodeSetList;
	
	/** The scrolled window with all properties windows */
	wxScrolledWindow* ScrolledWindow;

	/** The window with sound properties */
	WxPropertyWindowHost * SoundCueWindow;

	// Inicjalization - helper functions:

	/** Assign all nodes from all SoundCues to proper NodeSet */
	void FillNodeSetList(TArray<USoundCue*>& SoundCueList);
	/** Assign all nodes to proper NodeSet */
	void AssignAllNodes(USoundNode* Root, TMap<UClass*, NodeSet*>& NodeSetMap);
	/** Creates inner windows - one for SoundCues and One for each NodeSet */
	void CreateInnerWindows();
	/** Bind SoundCues and nodes to proper PropertyWindows */
	void BindObjectsToWindows(TArray<USoundCue*>& SoundCueList);

public:
	/** Construct a sound cue properties dialog 
	 * @param	SoundCueList	List of sounds whose properties are to be displayed
	 */
	WxDlgSoundCueProperties(TArray<USoundCue*>& SoundCueList);

	/** Destroy a sound cue properties dialog */
	~WxDlgSoundCueProperties();

	/////////////////////////
	// FNotify interface.

	/**
	 * Respond to the destruction of the dialog's windows
	 *
	 * @param	Src	The destroyed object
	 */
	virtual void NotifyDestroy( void* Src );

	/**
	 * Show/hide the dialog, based upon the first parameter
	 *
	 * @param	InShow			Whether to show or hide the dialog (TRUE means to show)
	 *
	 * @return	TRUE if the requested action (show/hide) was executed, FALSE if the dialog was
	 *			already in the requested state
	 */
	bool Show(bool bShow)
	{
		return WxTrackableDialog::Show(bShow);
	}
private:
	using wxDialog::Show;		// Hide parent implementation

	/**
	 * Called in response to the dialog's close button being pressed by the user;
	 * Destroys the dialog
	 *
	 * @param	In	Command event from wxWidgets signifying the button press
	 */
	void OnOK( wxCommandEvent& In );

	/**
	 * Called in response to the dialog being resized
	 *
	 * @param	In	Size event from wxWidgets signifying the dialog being resized
	 */
	void OnSize( wxSizeEvent& In );

	/**
	 * Called in response to the user closing the dialog (such as via the X)
	 *
	 * @param	In	Close event from wxWidgets signifying the dialog has been closed
	 */
	void OnClose( wxCloseEvent& In );

	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(WxDlgSoundCueProperties, WxTrackableDialog)
EVT_BUTTON( wxID_OK, WxDlgSoundCueProperties::OnOK )
EVT_SIZE( WxDlgSoundCueProperties::OnSize )
EVT_CLOSE( WxDlgSoundCueProperties::OnClose )
END_EVENT_TABLE()

/** Creates inner windows - one for SoundCues and One for each NodeSet */
void WxDlgSoundCueProperties::CreateInnerWindows()
{
	ScrolledWindow = new wxScrolledWindow( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL );
	wxBoxSizer* Sizer = new wxBoxSizer( wxVERTICAL );

	wxStaticBoxSizer* StaticBoxSizer = new wxStaticBoxSizer( new wxStaticBox( ScrolledWindow, wxID_ANY, *LocalizeUnrealEd("Properties") ), wxVERTICAL );

	SoundCueWindow = new WxPropertyWindowHost;
	SoundCueWindow->Create(ScrolledWindow, this, wxID_ANY, FALSE);
	StaticBoxSizer->Add( SoundCueWindow, 1, wxEXPAND |wxALL, 2 );
	Sizer->Add( StaticBoxSizer, 1, wxEXPAND |wxALL, 2 );

	if(0 != NodeSetList.Num())
	{
		for ( INT NodeSetIndex = 0 ; NodeSetIndex < NodeSetList.Num() ; ++NodeSetIndex )
		{
			NodeSet * NSet = NodeSetList(NodeSetIndex);
			if(NULL != NSet)
			{
				check(NULL == NSet->SoundNodesPropWindow);
				FString Label = FString::Printf( TEXT("%s - %i"), *(NSet->NodeType->GetName()), NSet->NodeList.Num() );
				StaticBoxSizer = new wxStaticBoxSizer( new wxStaticBox( ScrolledWindow, wxID_ANY, *Label ), wxVERTICAL );

				NSet->SoundNodesPropWindow = new WxPropertyWindowHost;
				NSet->SoundNodesPropWindow->Create(ScrolledWindow, this, wxID_ANY, FALSE);

				StaticBoxSizer->Add( NSet->SoundNodesPropWindow, 1, wxEXPAND |wxALL, 2 );
				Sizer->Add( StaticBoxSizer, 1, wxEXPAND |wxALL, 2 );
			}
		}
	}

	ScrolledWindow->SetSizer( Sizer );
	ScrolledWindow->SetScrollRate( 5, 5 );
	ScrolledWindow->Layout();

	Sizer->Fit( ScrolledWindow );
}

/** Assign all nodes to proper NodeSet */
void WxDlgSoundCueProperties::AssignAllNodes(USoundNode* Root, TMap<UClass*, NodeSet*>& NodeSetMap)
{
	TArray<USoundNode*> NodesToProcess;
	NodesToProcess.AddItem( Root );

	while ( NodesToProcess.Num() > 0 )
	{
		USoundNode* CurNode = NodesToProcess.Pop();
		if ( CurNode )
		{
			// Push child nodes.
			for ( INT ChildIndex = 0 ; ChildIndex < CurNode->ChildNodes.Num() ; ++ChildIndex )
			{
				NodesToProcess.AddItem( CurNode->ChildNodes(ChildIndex) );
			}

			UClass* NodeClass = CurNode->GetClass();
			check(NULL != NodeClass);
			NodeSet* FoundNodeSet = NodeSetMap.FindRef(NodeClass);
			if(NULL == FoundNodeSet)
			{
				FoundNodeSet = new NodeSet();
				FoundNodeSet->NodeType = NodeClass;
				NodeSetMap.Set(NodeClass, FoundNodeSet);
				NodeSetList.AddUniqueItem(FoundNodeSet);
			}

			FoundNodeSet->NodeList.AddUniqueItem(CurNode);
		}
	}
}

/** Assign all nodes from all SoundCues to proper NodeSet */
void WxDlgSoundCueProperties::FillNodeSetList(TArray<USoundCue*>& SoundCueList)
{
	TMap<UClass*, NodeSet*> NodeSetMap;
	for ( INT SoundCueIndex = 0 ; SoundCueIndex < SoundCueList.Num() ; ++SoundCueIndex )
	{
		USoundCue* SoundCue = SoundCueList(SoundCueIndex);
		AssignAllNodes( SoundCue->FirstNode, NodeSetMap );
	}
}

/** Bind SoundCues and nodes to proper PropertyWindows */
void WxDlgSoundCueProperties::BindObjectsToWindows(TArray<USoundCue*>& SoundCueList)
{
	SoundCueWindow->SetObjectArray( SoundCueList, EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories | EPropertyWindowFlags::UseTypeAsTitle );

	for ( INT NodeSetIndex = 0 ; NodeSetIndex < NodeSetList.Num() ; ++NodeSetIndex )
	{
		NodeSet * NSet = NodeSetList(NodeSetIndex);
		if(NULL != NSet && NULL != NSet->SoundNodesPropWindow && NSet->NodeList.Num() > 0)
	{
			NSet->SoundNodesPropWindow->SetObjectArray( NSet->NodeList, EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories | EPropertyWindowFlags::UseTypeAsTitle );
		}
	}
}
/** Construct a sound cue properties dialog */
WxDlgSoundCueProperties::WxDlgSoundCueProperties(TArray<USoundCue*>& SoundCueList)
{
	check( SoundCueList.Num() >= 1 );
	ScrolledWindow = NULL;
	SoundCueWindow = NULL;

	FillNodeSetList(SoundCueList);

	//Create window
	{
		FString Title;
	if( SoundCueList.Num() == 1 )
	{
		USoundCue* FirstSoundCue = SoundCueList(0);
			Title = FString::Printf( LocalizeSecure(LocalizeUnrealEd("SoundCuePropertiesCaption"), *(FirstSoundCue->GetPathName())) );
	}
	else
	{// Multiple sound cues selected
		check( SoundCueList.Num() > 1 );
			Title = FString::Printf( LocalizeSecure(LocalizeUnrealEd("SoundCueMultiplePropertiesCaption"), SoundCueList.Num()) );
		}
		const bool bSuccess = wxDialog::Create(GApp->EditorFrame, wxID_ANY, *Title, wxDefaultPosition, wxDefaultSize, wxCAPTION|wxRESIZE_BORDER|wxSYSTEM_MENU|wxCLOSE_BOX );
		check( bSuccess );
		FWindowUtil::LoadPosSize( TEXT("DlgSoundProperties"), this );
		FLocalizeWindow( this );
		SetTitle(*Title);
	}

	CreateInnerWindows();

	wxBoxSizer* Sizer = new wxBoxSizer( wxVERTICAL );
	Sizer->Add( ScrolledWindow, 1, wxEXPAND | wxALL, 5 );
	wxButton* CloseButton = new wxButton( this, wxID_OK,  *LocalizeUnrealEd( TEXT("Close") ), wxDefaultPosition, wxDefaultSize, 0 );
	Sizer->Add( CloseButton, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, 5 );
	this->SetSizer( Sizer );
	this->Layout();
	this->Centre( wxBOTH );

	BindObjectsToWindows(SoundCueList);
}

/** Destroy a sound cue properties dialog */
WxDlgSoundCueProperties::~WxDlgSoundCueProperties()
{
	FWindowUtil::SavePosSize( TEXT("DlgSoundProperties"), this );

	for ( INT NodeSetIndex = 0 ; NodeSetIndex < NodeSetList.Num() ; ++NodeSetIndex )
	{
		delete NodeSetList(NodeSetIndex);
	}
}

/**
 * Called in response to the dialog's close button being pressed by the user;
 * Destroys the dialog
 *
 * @param	In	Command event from wxWidgets signifying the button press
 */
void WxDlgSoundCueProperties::OnOK( wxCommandEvent& In )
{
	SoundCueWindow->FlushLastFocused();
	for ( INT NodeSetIndex = 0 ; NodeSetIndex < NodeSetList.Num() ; ++NodeSetIndex )
	{
		NodeSet * NSet = NodeSetList(NodeSetIndex);
		if(NULL != NSet && NULL != NSet->SoundNodesPropWindow)
		{
			NSet->SoundNodesPropWindow->FlushLastFocused();
		}
	}
	Destroy();
}

/**
 * Called in response to the dialog being resized
 *
 * @param	In	Size event from wxWidgets signifying the dialog being resized
 */
void WxDlgSoundCueProperties::OnSize( wxSizeEvent& In )
{
	// Must call Layout() ourselves unless we set wxWindow::SetAutoLayout
	Layout();
	if(NULL != ScrolledWindow)
	{
		ScrolledWindow->Layout();
	}
	In.Skip();
}

/**
 * Called in response to the user closing the dialog (such as via the X)
 *
 * @param	In	Close event from wxWidgets signifying the dialog has been closed
 */
void WxDlgSoundCueProperties::OnClose( wxCloseEvent& In )
{
	SoundCueWindow->FlushLastFocused();
	for ( INT NodeSetIndex = 0 ; NodeSetIndex < NodeSetList.Num() ; ++NodeSetIndex )
	{
		NodeSet * NSet = NodeSetList(NodeSetIndex);
		if(NULL != NSet && NULL != NSet->SoundNodesPropWindow)
		{
			NSet->SoundNodesPropWindow->FlushLastFocused();
		}
	}
	Destroy();
}

/**
 * Respond to the destruction of the dialog's windows
 *
 * @param	Src	The destroyed object
 */
void WxDlgSoundCueProperties::NotifyDestroy( void* Src )
{
	if(Src == SoundCueWindow)
	{
		SoundCueWindow = NULL;
		return;
	}
	for ( INT NodeSetIndex = 0 ; NodeSetIndex < NodeSetList.Num() ; ++NodeSetIndex )
	{
		NodeSet * NSet = NodeSetList(NodeSetIndex);
		if(NULL != NSet && Src == NSet->SoundNodesPropWindow)
	{
			NSet->SoundNodesPropWindow = NULL;
			break;
		}
	}
}

/*-----------------------------------------------------------------------------
WxTexturePropertiesFrame.
-----------------------------------------------------------------------------*/

/**
 * Trackable frame class to allow the viewing of texture properties alongside
 * a preview of the texture
 */
class WxTexturePropertiesFrame : public WxTrackableFrame, 
	FViewportClient, 
	public FNotifyHook,
	// Interface for event handling
	public FCallbackEventDevice,
	public FSerializableObject
{
public:
	/**
	 * Construct a texture properties frame 
	 * @param	InTexture	Texture whose properties/preview will be displayed
	 */
	WxTexturePropertiesFrame(UTexture* InTexture);

	/** Destroy a texture properties frame */
	~WxTexturePropertiesFrame();
	
public:

	/////////////////////////
	// FViewportClient interface.

	/**
	 * Draw the texture within the frame
	 *
	 * @param	Viewport	The viewport being drawn in
	 * @param	Canvas		The render interface to draw with
	 */
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas);

	/////////////////////////
	// FNotify interface.

	/**
	 * Respond to the destruction of the frame's windows
	 *
	 * @param	Src	The destroyed object
	 */
	virtual void NotifyDestroy( void* Src );

	/**
	 * Respond to a property being changed
	 *
	 * @param	Src						The changed object
	 * @param	PropertyThatChanged		The property that changed
	 */
	virtual void NotifyPostChange( void* Src, UProperty* PropertyThatChanged );

	/* === FCallbackEventDevice interface === */
	/**
	 * Called when the viewport has been resized
	 *
	 * @param	InType					The event type
	 * @param	InViewport				The viewport that changed
	 * @param	InMessage				Message enum.  Unused here.
	 */
	virtual void Send( ECallbackEventType InType, FViewport* InViewport, UINT InMessage);

	/* === FCallbackEventDevice interface === */
	/**
	 * Called when UObject is changed
	 *
	 * @param	InType					The event type
	 * @param	InObject				The UObject that changed
	 */
	virtual void Send(ECallbackEventType InType,UObject* InObject);
	/* === FSerializableObject interface === */
	/**
	 * Serialize out any UObject references held by the window
	 *
	 * @param	Ar	The archive to serialize with
	 */
	virtual void Serialize( FArchive& Ar );

private:
	/** Toolbar class for the texture properties frame */
	class WxTexturePropertiesToolBar : public WxToolBar
	{
	public:
		/**
		 * Constructor
		 *
		 * @param	InParent		Parent window of the toolbar
		 * @param	InID			Window ID of the toolbar
		 * @param	InitialPaddingX	Initial amount of padding to display in the x-padding spin control
		 * @param	InitialPaddingY	Initial amount of padding to display in the y-padding spin control
		 */
		WxTexturePropertiesToolBar( wxWindow* InParent, wxWindowID InID, INT InitialPaddingX, INT InitialPaddingY );

	private:

		/** Images for the toolbar buttons */
		WxBitmap BackgroundB, CheckerboardB, BorderB, ExpandToFrameB;
		WxBitmap RedChannelImage, GreenChannelImage, BlueChannelImage, AlphaChannelImage, DesaturationImage;
	};

	/** The texture being previewed */
	UTexture* Texture;

	/** Texture for displaying monochrome thumbnails which cannot be rendered on the GPU and must be converted to RGB first */
	UTexture2D* MonochromeThumbnail;

	/**Splitter to allow resizing of the preview and the properties*/
	wxSplitterWindow* LeftRightSplitter;

	/** The viewport frame for the texture preview */
	WxViewportHolder* ViewportHolder;

	/** A viewport previewing the texture */
	FViewport* Viewport;
	
	/** A window with read-only stats about the texture */
	WxPropertyQuickInfoWindow* QuickInfoWindow;

	/** A secondary window with read-only stats about the texture */
	WxPropertyQuickInfoWindow* QuickInfoWindowB;

	/** Texture's property window */
	WxPropertyWindowHost* PropertyWindow;

	/** The maximum width at which the texture will render in the preview window */
	DWORD PreviewEffectiveTextureWidth;

	/** The maximum height at which the texture will render in the preview window */
	DWORD PreviewEffectiveTextureHeight;

	/** Xbox LOD settings */
	FTextureLODSettings	LODSettings360;

	/** PS3 LOD settings */
	FTextureLODSettings	LODSettingsPS3;

	/**Desired position of the splitter sash*/
	INT IdealSplitterPos;

	/**Guard around setting ideal splitter position based on OnSize*/
	UBOOL bAllowChangeSplitterPos;

	/** If enabled, scale the texture up as necessary to always fill the bounds of the viewport frame */
	UBOOL bExpandTextureToFrame;

	/** Padding in the preview viewport in the X-direction, if any */
	INT PreviewPaddingX;

	/** Padding in the preview viewport in the Y-direction, if any */
	INT PreviewPaddingY;
	
	/** Color channel filters to isolate selected colour channels in loaded texture */
	UBOOL RedColorChannelEnabled, GreenColorChannelEnabled, BlueColorChannelEnabled, AlphaColorChannelEnabled, DesaturationEnabled;

	ESimpleElementBlendMode ColourChannelBlendMode;

	/** Color to use as the preview viewport's background */
	FColor BackgroundFillColor;

	/** Color to use for the texture border, if enabled */
	FColor TextureBorderColor;

	/** Whether or not to display the texture border */
	UBOOL bDisplayTextureBorder;
	
	/** Whether or not to display the background checkerboard */
	UBOOL bDisplayCheckerboard;

	/** Whether or not to have the checkerboard fill the entire background */
	UBOOL bUseCheckerboardBackground;

	/** Number of pixels in each direction to use for one checker tile for the checkerboard texture (the larger the value, the fewer times the checker will repeat) */
	INT NumPixelsPerCheckerTile;

	/** First color to use to make up the checkerboard texture */
	FColor CheckerboardColorOne;

	/** Second color to use to make up the checkerboard texture */
	FColor CheckerboardColorTwo;

	/** Checkerboard texture */
	UTexture2D* CheckerboardTexture;

	/** Minimum number of pixels in each direction to use as the basis of a tile in the checkerboard texture */
	static const INT MIN_CHECKER_PIXELS = 4;

	/**
	 * Called in response to the dialog's button being pressed by the user;
	 * Destroys the frame
	 *
	 * @param	In	Command event from wxWidgets signifying the button press
	 */
	void OnOK(wxCommandEvent& In);

	/**
	 * Called in response to the frame's 'Reimport Texture' button being pressed by the user;
	 * Re-imports the texture from disk.
	 *
	 * @param	In	Command event from wxWidgets signifying the button press
	 */
	void OnReloadPressed(wxCommandEvent& In);

	/**
	 * Get the color channel filters which should be drawn/hidden on the texture, and return the appropriate 
	 * enum so it can be ultimately passed through to BatchedElements::Draw to select the appropriate pixel shader
	 *
	 * @param	r	Red
	 * @param	g	Green
	 * @param	b	Blue
	 * @param	a	Alpha
	 * @param	d	Desaturation	
	 */
	ESimpleElementBlendMode GetSimpleElementBlendMode(UBOOL r, UBOOL g, UBOOL b, UBOOL a, UBOOL d );

	/**
	 * Called in response to the user opting to show/hide a colour channel through the toggle buttons
	 * 
	 * @param	In	Command event from wxWidgets signifying the button press
	 */
	void OnChannelPressed(wxCommandEvent& In);
	
	/**
	 * Called in response to the frame's 'Compress Texture' button being pressed by the user;
	 * Re-imports the texture from disk.
	 *
	 * @param	In	Command event from wxWidgets signifying the button press
	 */
	void OnCompressNowPressed(wxCommandEvent& In);

	/**
	 * Determines if the compress-now button should be enabled or not
	 */
	void UpdateCompressNowButton(wxUpdateUIEvent& In);

	/** Called when sash position changes so we can remember the sash position. */
	void OnSplitterPosChange(wxSplitterEvent& In);

	/**Event to disable double clicking*/
	void OnSplitterDblClk( wxSplitterEvent& InEvent );

	/** Called when the user clicks the background button on the toolbar; displays the color picker */
	void OnBackgroundButtonClick( wxCommandEvent& In );

	/** Called when the user toggles the expand to frame option */
	void OnExpandToFrameButtonClick( wxCommandEvent& In );

	/** Automatically called by wxWidgets to update the UI for the expand to frame option accordingly */
	void OnExpandToFrameButton_UpdateUI( wxUpdateUIEvent& In );
	
	/** Called when the user toggles the display of the checkerboard on/off */
	void OnCheckerboardButtonClick( wxCommandEvent& In );

	/** Called when the user right-clicks on the checkerboard button; displays a menu with advanced options */
	void OnCheckerboardButtonRightClick( wxCommandEvent& In );

	/** Called automatically by wxWidgets to update the UI related to the checkerboard button */
	void OnCheckerboardButton_UpdateUI( wxUpdateUIEvent& In );

	/** Called automatically by wxWidgets to update the UI related to the checkerboard num pixels menu item */
	void OnCheckerboardCheckerPixelNum_UpdateUI( wxUpdateUIEvent& In );

	/** Called when the user selects a number of checker pixels menu option */
	void OnCheckerboardCheckerPixelNumMenu( wxCommandEvent& In );

	/** Called when the user selects a menu option to change a checkerboard color; prompts the user with the color picker */
	void OnCheckerboardColorChangeMenu( wxCommandEvent& In );

	/** Called when the user toggles the menu option to use the checkerboard as the background */
	void OnCheckerboardUseAsBGMenu( wxCommandEvent& In );

	/** Called automatically by wxWidgets to update the UI for the checkerboard as background menu option */
	void OnCheckerboardUseAsBG_UpdateUI( wxUpdateUIEvent& In );

	/** Called when the user toggles the border display option */
	void OnBorderButtonClick( wxCommandEvent& In );

	/** Called when the user right clicks on the border button; displays a menu of advanced options */
	void OnBorderButtonRightClick( wxCommandEvent& In );

	/** Called when the user selects the menu option to change the border color; prompts the user with the color picker */
	void OnBorderColorChangeMenu( wxCommandEvent& In );

	/** Called automatically by wxWidgets to update the UI for the border button */
	void OnBorderButton_UpdateUI( wxUpdateUIEvent& In );

	/** Called whenever the user clicks the padding spin control arrows */
	void OnPaddingSpinCtrlClick( wxSpinEvent& In );

	/** Called when the user presses enter in the padding spin control textboxes */
	void OnPaddingSpinCtrlEnterPressed( wxCommandEvent& In );

	/**
	 * Called in response to the dialog being resized
	 *
	 * @param	In	Size event from wxWidgets signifying the dialog being resized
	 */
	void OnSize( wxSizeEvent& In );

	//resize the texture preview viewport
	void ResizeViewportWindow(void);

	/**
	 * Called in response to the user closing the dialog (such as via the X)
	 *
	 * @param	In	Close event from wxWidgets signifying the dialog has been closed
	 */
	void OnClose( wxCloseEvent& In );
	
	/** 
	 * Find the effective in-game resolution of the texture.
	 *
	 *  @param	LODBias					Default LOD at which the texture renders. Platform dependent, call FTextureLODSettings::CalculateLODBias(Texture)
	 *  @param	EffectiveTextureWidth	The imported width scaled to the current mip
	 *  @param	EffectiveTextureHeight	The imported height scaled to the current mip
	 */
	void CalculateEffectiveTextureDimensions(INT LODBias, DWORD& EffectiveTextureWidth, DWORD& EffectiveTextureHeight);

	/**
	 *  Determine the draw dimensions of the texture in the preview window
	 *
	 *  @param	EffectiveTextureWidth	The imported width scaled to the current mip
	 *  @param	EffectiveTextureHeight	The imported height scaled to the current mip
	 *  @param	Width					The width of the texture in the preview window
	 *  @param	Height					The height of the texture in the preview window
	 *  @param	ImportedWidth			The full resolution width of the texture
	 *  @param	ImportedHeight			The full resolution height of the texture
	 */
	void CalculateTextureDimensions( DWORD EffectiveTextureWidth, DWORD EffectiveTextureHeight, DWORD& Width, DWORD& Height, DWORD& ImportedWidth, DWORD& ImportedHeight );

	/**
	 *  Clears and recreates the stats in the quick info window
	 */
	void UpdateQuickInfoWindows();

	/** Initialize the checkerboard texture for the texture preview, if necessary */
	void SetupCheckerboardTexture();

	/** Modifies the checkerboard texture's data to match the colors currently specified by CheckerboardColorOne and CheckerboardColorTwo */
	void ModifyCheckerboardTextureColors();

	/** Destroy the checkerboard texture if one exists */
	void DestroyCheckerboardTexture();

	/**
	 * Prompt the user with the color picker to pick a color for the provided FColor pointer
	 *
	 * @param	InColorToPick	Data to populate with the user picked color, if any
	 *
	 * @return	TRUE if the user picked a color; FALSE if they did not
	 */
	UBOOL PickProvidedColor( FColor* InColorToPick );

	/** Load user preferences associated with the preview window from INI file */
	void LoadPreviewPreferencesFromINI();

	/** Save user preferences associated with the preview window to the INI file */
	void SavePreviewPreferencesToINI();

	DECLARE_EVENT_TABLE()
};

/**
 * Constructor
 *
 * @param	InParent		Parent window of the toolbar
 * @param	InID			Window ID of the toolbar
 * @param	InitialPaddingX	Initial amount of padding to display in the x-padding spin control
 * @param	InitialPaddingY	Initial amount of padding to display in the y-padding spin control
 */
WxTexturePropertiesFrame::WxTexturePropertiesToolBar::WxTexturePropertiesToolBar( wxWindow* InParent, wxWindowID InID, INT InitialPaddingX, INT InitialPaddingY )
:	WxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_NO_TOOLTIPS | wxTAB_TRAVERSAL )
{

	RedChannelImage.Load(TEXT("Btn_TextureProperties_Red_On.png"));
	wxToolBarToolBase* cb = AddCheckTool( ID_DLG_TEXTURE_TOGGLE_R_CHANNEL, TEXT(""), RedChannelImage, RedChannelImage, *LocalizeUnrealEd("TextureProperties_ToggleRed") );
	cb->Toggle();
	GreenChannelImage.Load(TEXT("Btn_TextureProperties_Green_On.png"));
	cb = AddCheckTool( ID_DLG_TEXTURE_TOGGLE_G_CHANNEL, TEXT(""), GreenChannelImage, GreenChannelImage, *LocalizeUnrealEd("TextureProperties_ToggleGreen") );
	cb->Toggle();
	BlueChannelImage.Load(TEXT("Btn_TextureProperties_Blue_On.png"));
	cb = AddCheckTool( ID_DLG_TEXTURE_TOGGLE_B_CHANNEL, TEXT(""), BlueChannelImage, BlueChannelImage, *LocalizeUnrealEd("TextureProperties_ToggleBlue") );
	cb->Toggle();
	AlphaChannelImage.Load(TEXT("Btn_TextureProperties_Alpha_On.png"));
	cb = AddCheckTool( ID_DLG_TEXTURE_TOGGLE_A_CHANNEL, TEXT(""), AlphaChannelImage, AlphaChannelImage, *LocalizeUnrealEd("TextureProperties_ToggleAlpha") );
	cb->Toggle();
	DesaturationImage.Load(TEXT("Btn_TextureProperties_Sat_On.png"));
	AddCheckTool( ID_DLG_TEXTURE_TOGGLE_DESATURATION, TEXT(""), DesaturationImage, DesaturationImage, *LocalizeUnrealEd("TextureProperties_ToggleDesaturation") );

	AddSeparator();

	// Load the images for the toolbar buttons
	BackgroundB.Load( TEXT("TextureProperties_BG.png") );
	CheckerboardB.Load( TEXT("TextureProperties_Checker.png") );
	BorderB.Load( TEXT("TextureProperties_Border.png") );
	ExpandToFrameB.Load( TEXT("TextureProperties_Expand.png") );

	SetToolBitmapSize( wxSize( 18, 18 ) );

	// Add the various toolbar controls

#if WITH_MANAGED_CODE
	AddTool( ID_DLG_TEXTURE_BACKGROUND, TEXT(""), BackgroundB, *LocalizeUnrealEd("TexturePropertiesFrame_ChangeBackground_ToolTip") );
#endif // #if WITH_MANAGED_CODE

	AddCheckTool( ID_DLG_TEXTURE_EXPAND_TO_FRAME, TEXT(""), ExpandToFrameB, ExpandToFrameB, *LocalizeUnrealEd("TexturePropertiesFrame_ExpandToViewportFrame_ToolTip") );
	AddCheckTool( ID_DLG_TEXTURE_CHECKERBOARD, TEXT(""), CheckerboardB, CheckerboardB, *LocalizeUnrealEd("TexturePropertiesFrame_Checkerboard_ToolTip") );
	AddCheckTool( ID_DLG_TEXTURE_BORDER, TEXT(""), BorderB, BorderB, *LocalizeUnrealEd("TexturePropertiesFrame_Border_ToolTip") );

	AddSeparator();

	wxStaticText* PaddingXLabel = new wxStaticText( this, wxID_ANY, *LocalizeUnrealEd("TexturePropertiesFrame_PaddingX"), wxDefaultPosition, wxSize( 120, -1 ), wxALIGN_RIGHT );
	wxStaticText* PaddingYLabel = new wxStaticText( this, wxID_ANY, *LocalizeUnrealEd("TexturePropertiesFrame_PaddingY"), wxDefaultPosition, wxSize( 20, -1 ), wxALIGN_RIGHT );
	wxSpinCtrl* PaddingX = new wxSpinCtrl( this, ID_DLG_TEXTURE_PADDINGX, TEXT(""), wxDefaultPosition, wxSize( 50, -1 ), wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER, 0, 500, InitialPaddingX );
	wxSpinCtrl* PaddingY = new wxSpinCtrl( this, ID_DLG_TEXTURE_PADDINGY, TEXT(""), wxDefaultPosition, wxSize( 50, -1 ), wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER, 0, 500, InitialPaddingY );
	PaddingX->SetToolTip( *LocalizeUnrealEd("TexturePropertiesFrame_Padding_ToolTip") );
	PaddingY->SetToolTip( *LocalizeUnrealEd("TexturePropertiesFrame_Padding_ToolTip") );

	AddControl( PaddingXLabel );
	AddControl( PaddingX );
	AddControl( PaddingYLabel );
	AddControl( PaddingY );

	Realize();
}

BEGIN_EVENT_TABLE( WxTexturePropertiesFrame, WxTrackableFrame )
EVT_BUTTON( wxID_OK, WxTexturePropertiesFrame::OnOK )
EVT_SIZE( WxTexturePropertiesFrame::OnSize )
EVT_CLOSE( WxTexturePropertiesFrame::OnClose )
EVT_SPLITTER_SASH_POS_CHANGED( ID_DLG_TEXTURE_SPLITTER, WxTexturePropertiesFrame::OnSplitterPosChange )
EVT_SPLITTER_SASH_POS_CHANGING( ID_DLG_TEXTURE_SPLITTER, WxTexturePropertiesFrame::OnSplitterPosChange )
EVT_SPLITTER_DCLICK( ID_DLG_TEXTURE_SPLITTER, WxTexturePropertiesFrame::OnSplitterDblClk )
EVT_MENU_RANGE( ID_DLG_TEXTURE_TOGGLE_R_CHANNEL, ID_DLG_TEXTURE_TOGGLE_DESATURATION, WxTexturePropertiesFrame::OnChannelPressed )
EVT_TOOL( ID_DLG_TEXTURE_BACKGROUND, WxTexturePropertiesFrame::OnBackgroundButtonClick )
EVT_TOOL( ID_DLG_TEXTURE_EXPAND_TO_FRAME, WxTexturePropertiesFrame::OnExpandToFrameButtonClick )
EVT_UPDATE_UI( ID_DLG_TEXTURE_EXPAND_TO_FRAME, WxTexturePropertiesFrame::OnExpandToFrameButton_UpdateUI )
EVT_TOOL( ID_DLG_TEXTURE_CHECKERBOARD, WxTexturePropertiesFrame::OnCheckerboardButtonClick )
EVT_TOOL_RCLICKED( ID_DLG_TEXTURE_CHECKERBOARD, WxTexturePropertiesFrame::OnCheckerboardButtonRightClick )
EVT_UPDATE_UI( ID_DLG_TEXTURE_CHECKERBOARD, WxTexturePropertiesFrame::OnCheckerboardButton_UpdateUI )
EVT_UPDATE_UI_RANGE( ID_DLG_TEXTURE_CHECKERBOARD_CHECKERNUM_START, ID_DLG_TEXTURE_CHECKERBOARD_CHECKERNUM_END, WxTexturePropertiesFrame::OnCheckerboardCheckerPixelNum_UpdateUI )
EVT_MENU_RANGE( ID_DLG_TEXTURE_CHECKERBOARD_CHECKERNUM_START, ID_DLG_TEXTURE_CHECKERBOARD_CHECKERNUM_END, WxTexturePropertiesFrame::OnCheckerboardCheckerPixelNumMenu )
EVT_MENU_RANGE( ID_DLG_TEXTURE_CHECKERBOARD_COLOR_ONE, ID_DLG_TEXTURE_CHECKERBOARD_COLOR_TWO, WxTexturePropertiesFrame::OnCheckerboardColorChangeMenu )
EVT_MENU( ID_DLG_TEXTURE_CHECKERBOARD_USE_AS_BG, WxTexturePropertiesFrame::OnCheckerboardUseAsBGMenu )
EVT_UPDATE_UI( ID_DLG_TEXTURE_CHECKERBOARD_USE_AS_BG, WxTexturePropertiesFrame::OnCheckerboardUseAsBG_UpdateUI )
EVT_TOOL( ID_DLG_TEXTURE_BORDER, WxTexturePropertiesFrame::OnBorderButtonClick )
EVT_TOOL_RCLICKED( ID_DLG_TEXTURE_BORDER, WxTexturePropertiesFrame::OnBorderButtonRightClick )
EVT_MENU( ID_DLG_TEXTURE_BORDER_COLOR, WxTexturePropertiesFrame::OnBorderColorChangeMenu )
EVT_UPDATE_UI( ID_DLG_TEXTURE_BORDER, WxTexturePropertiesFrame::OnBorderButton_UpdateUI )
EVT_SPINCTRL( ID_DLG_TEXTURE_PADDINGX, WxTexturePropertiesFrame::OnPaddingSpinCtrlClick )
EVT_SPINCTRL( ID_DLG_TEXTURE_PADDINGY, WxTexturePropertiesFrame::OnPaddingSpinCtrlClick )
EVT_TEXT_ENTER( ID_DLG_TEXTURE_PADDINGX, WxTexturePropertiesFrame::OnPaddingSpinCtrlEnterPressed )
EVT_TEXT_ENTER( ID_DLG_TEXTURE_PADDINGY, WxTexturePropertiesFrame::OnPaddingSpinCtrlEnterPressed )
END_EVENT_TABLE()

/** Construct a texture properties frame */
WxTexturePropertiesFrame::WxTexturePropertiesFrame(UTexture* InTexture)
:	Viewport( NULL ),
	Texture( InTexture ),
	MonochromeThumbnail( NULL ),
	LeftRightSplitter( NULL ),
	QuickInfoWindow( NULL ),
	QuickInfoWindowB( NULL ),
	bAllowChangeSplitterPos( TRUE ),
	bExpandTextureToFrame( FALSE ),
	PreviewPaddingX( 5 ),
	PreviewPaddingY( 5 ),
	BackgroundFillColor( 0, 0, 0 ),
	TextureBorderColor( 255, 255, 255 ),
	bDisplayTextureBorder( TRUE ),
	bDisplayCheckerboard( TRUE ),
	bUseCheckerboardBackground( FALSE ),
	NumPixelsPerCheckerTile( 16 ),
	CheckerboardColorOne( 120, 120, 120 ),
	CheckerboardColorTwo( 60, 60, 60 ),
	CheckerboardTexture( NULL )
{
	PropertyWindow = NULL;
	ViewportHolder = NULL;

	const bool bSuccess = wxXmlResource::Get()->LoadFrame( this, GApp->EditorFrame, TEXT("ID_DLG_TEXTURE_PROPERTIES") );
	check( bSuccess );

	//stop window updates

	Freeze();
	//add sizer to main window
	wxBoxSizer* DialogSizer = new wxBoxSizer(wxHORIZONTAL);
	SetSizer(DialogSizer);

	//Initialize console-specific vars
	const FString XenonEngineConfigFilename = FString::Printf( TEXT("%s%sEngine.ini"), *appGameConfigDir(), XENON_DEFAULT_INI_PREFIX);
	LODSettings360.Initialize(*XenonEngineConfigFilename, TEXT("SystemSettings"));
	const FString PS3EngineConfigFilename = FString::Printf( TEXT("%s%sEngine.ini"), *appGameConfigDir(), PS3_DEFAULT_INI_PREFIX);
	LODSettingsPS3.Initialize(*PS3EngineConfigFilename, TEXT("SystemSettings"));

	FLocalizeWindow( this );

	//Add a splitter intermediate
	LeftRightSplitter = new wxSplitterWindow( this, ID_DLG_TEXTURE_SPLITTER, wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE | wxSP_BORDER );

	//load the left side of the texture viewer
	wxPanel* TexturePreviewHolderPanel = wxXmlResource::Get()->LoadPanel( LeftRightSplitter, TEXT("ID_DLG_TEXTURE_PREVIEW_HOLDER") );
	check( TexturePreviewHolderPanel );

	// setup the initial color channel states
	RedColorChannelEnabled = GreenColorChannelEnabled = BlueColorChannelEnabled = AlphaColorChannelEnabled = TRUE;
	DesaturationEnabled = FALSE;

	// Set the blend mode for our color channel filter to initially show the red, green, blue and alpha. Desaturation is disabled as default
	ColourChannelBlendMode = GetSimpleElementBlendMode( RedColorChannelEnabled, GreenColorChannelEnabled, BlueColorChannelEnabled, AlphaColorChannelEnabled, DesaturationEnabled);

	//load the right side of the texture viewer
	wxPanel* TextureInfoHolderPanel = wxXmlResource::Get()->LoadPanel(LeftRightSplitter, TEXT("ID_DLG_TEXTURE_INFO_HOLDER") );
	check( TextureInfoHolderPanel );

	wxWindow* InfoBoxPanel = FindWindow(XRCID("ID_DLG_TEXTURE_INFO_HOLDER_CAPTION"));
	check(InfoBoxPanel);
	InfoBoxPanel->SetLabel(*(LocalizeUnrealEd("TextureProperties_QuickInfoPaneLabel")));

	// Viewport to show the texture in (done before split so the resize works correctly
	wxWindow* win = wxDynamicCast(FindWindow( XRCID( "ID_VIEWPORT_WINDOW" ) ), wxWindow);
	wxBoxSizer* ViewportSizer = new wxBoxSizer(wxHORIZONTAL);
	win->SetSizer(ViewportSizer);

	ViewportHolder = new WxViewportHolder( (wxWindow*)win, -1, 0 );
	check( win != NULL );
	wxRect rc = win->GetRect();
	Viewport = GEngine->Client->CreateWindowChildViewport( this, (HWND)ViewportHolder->GetHandle(), rc.width, rc.height );
	Viewport->CaptureJoystickInput(false);
	ViewportHolder->SetViewport( Viewport );

	//add viewport window to the sizer
	ViewportSizer->Add(ViewportHolder, 1, wxGROW | wxALL, 0);
	
	// Property windows for info about the texture
	win = FindWindow( XRCID( "ID_QUICK_INFO_WINDOW" ) );
	check( win != NULL );
	QuickInfoWindow = new WxPropertyQuickInfoWindow;
	QuickInfoWindow->Create(win);
	rc = win->GetRect();
	QuickInfoWindow->SetSize( 0, 0, 175, 95 );
	win->Show();

	win = FindWindow( XRCID( "ID_QUICK_INFO_WINDOWB" ) );
	check( win != NULL );
	QuickInfoWindowB = new WxPropertyQuickInfoWindow;
	QuickInfoWindowB->Create(win);
	rc = win->GetRect();
	QuickInfoWindowB->SetSize( 0, 0, 175, 95 );
	win->Show();

	// Property window for editing the texture
	win = FindWindow( XRCID( "ID_PROPERTY_WINDOW" ) );
	check( win != NULL );
	//add sizer to property window
	wxBoxSizer* PropertySizer = new wxBoxSizer(wxHORIZONTAL);
	win->SetSizer(PropertySizer);
	PropertyWindow = new WxPropertyWindowHost;
	PropertyWindow->Create(win, this);
	//add property window to the sizer
	PropertySizer->Add(PropertyWindow, 1, wxGROW | wxALL, 0);
	//rc = win->GetRect();
	//PropertyWindow->SetSize( 0, 0, rc.GetWidth(), rc.GetHeight() );
	win->Show();

	// Set the correct names for the reimport and compress-now buttons, and wire up their actions
	wxButton* ReloadButton = wxDynamicCast(FindWindow(XRCID("ID_DLG_TEXTURE_RELOAD_TEXTURE")), wxButton);
	check(ReloadButton != NULL);
	ReloadButton->SetLabel(*(LocalizeUnrealEd("TextureProperties_ReimportTexture")));
	ReloadButton->SetToolTip(*(LocalizeUnrealEd("TextureProperties_ReimportTexture_Tooltip")));

	ADDEVENTHANDLER( XRCID("ID_DLG_TEXTURE_RELOAD_TEXTURE"), wxEVT_COMMAND_BUTTON_CLICKED, &WxTexturePropertiesFrame::OnReloadPressed);
	
	wxButton* CompressButton = wxDynamicCast(FindWindow(XRCID("ID_DLG_TEXTURE_COMPRESS_TEXTURE")), wxButton);
	check(CompressButton != NULL);
	CompressButton->SetLabel(*(LocalizeUnrealEd("TextureProperties_CompressNow")));
	
	ADDEVENTHANDLER(XRCID("ID_DLG_TEXTURE_COMPRESS_TEXTURE"), wxEVT_COMMAND_BUTTON_CLICKED, &WxTexturePropertiesFrame::OnCompressNowPressed);
	ADDEVENTHANDLER(XRCID("ID_DLG_TEXTURE_COMPRESS_TEXTURE"), wxEVT_UPDATE_UI, wxStaticCastEvent(wxUpdateUIEventFunction, &WxTexturePropertiesFrame::UpdateCompressNowButton));

	//default position

	PropertyWindow->SetObject( Texture, EPropertyWindowFlags::Sorted );
	// Display QuickInfo Stats
	UpdateQuickInfoWindows();
	SetTitle( *FString::Printf( LocalizeSecure(LocalizeUnrealEd("TexturePropertiesCaption"), *Texture->GetPathName()) ) );

	//add splitter window to the sizer
	DialogSizer->Add(LeftRightSplitter, 1, wxGROW | wxALL, 0);

	//split the window based with use a hardcoded position that will be overridden very shortly
	LeftRightSplitter->SplitVertically(TexturePreviewHolderPanel, TextureInfoHolderPanel, 400);

	//load size of window from ini file
	FWindowUtil::LoadPosSize( TEXT("DlgTextureProperties"), this );

	//Now that the full size is loaded, reposition the splitter
	IdealSplitterPos = 400;	//from the right
	GConfig->GetInt(TEXT("TextureProperties"), TEXT("SplitterPos"), IdealSplitterPos, GEditorUserSettingsIni);
	LeftRightSplitter->SetSashPosition(GetClientRect().width - IdealSplitterPos);

	LoadPreviewPreferencesFromINI();
	SetupCheckerboardTexture();

	WxTexturePropertiesToolBar* ToolBar = new WxTexturePropertiesToolBar( this, wxID_ANY, PreviewPaddingX, PreviewPaddingY );
	SetToolBar( ToolBar );

	Show();

	Thaw();

	GCallbackEvent->Register(CALLBACK_ViewportResized, this);
	GCallbackEvent->Register(CALLBACK_TextureModified, this);
}

/** Destroy a texture properties dialog */
WxTexturePropertiesFrame::~WxTexturePropertiesFrame()
{
#if WITH_MANAGED_CODE
	UnBindColorPickers( this );
#endif

	GCallbackEvent->UnregisterAll(this);

	FWindowUtil::SavePosSize( TEXT("DlgTextureProperties"), this );
	
	SavePreviewPreferencesToINI();

	//save splitter position
	GConfig->SetInt(TEXT("TextureProperties"), TEXT("SplitterPos"), IdealSplitterPos, GEditorUserSettingsIni);

	GEngine->Client->CloseViewport(Viewport); 
	Viewport = NULL;
	delete QuickInfoWindow;
	delete QuickInfoWindowB;
	delete PropertyWindow;

	// Delete the thumbnail resource and mark the thumbnail for GC
	if( MonochromeThumbnail && MonochromeThumbnail->Resource )
	{
		if( MonochromeThumbnail->Resource )
		{
			MonochromeThumbnail->ReleaseResource();
		}

		MonochromeThumbnail->MarkPendingKill();
	}

	DestroyCheckerboardTexture();
}

/**
 * Called in response to the frame's button being pressed by the user;
 * Destroys the dialog
 *
 * @param	In	Command event from wxWidgets signifying the button press
 */
void WxTexturePropertiesFrame::OnOK( wxCommandEvent& In )
{
	PropertyWindow->FlushLastFocused();
	Destroy();
}

/**
 * Called in response to the frame's 'Reimport Texture' button being pressed by the user;
 * Re-imports the texture from disk.
 *
 * @param	In	Command event from wxWidgets signifying the button press
 */
void WxTexturePropertiesFrame::OnReloadPressed( wxCommandEvent& In )
{
	// Prevent the texture from being compressed immediately, so the user can see the results
	INT SavedCompressionSetting = Texture->DeferCompression;
	Texture->DeferCompression = TRUE;

	// Reduce the year to make sure the texture will always be reloaded even if it hasn't changed on disk
	FFileManager::FTimeStamp timestamp;
	FFileManager::FTimeStamp::FStringToTimestamp(Texture->SourceFileTimestamp, /*out*/ timestamp);
	--timestamp.Year;
	FFileManager::FTimeStamp::TimestampToFString(timestamp, /*out*/ Texture->SourceFileTimestamp);

	// Reimport the texture
	if (FReimportManager::Instance()->Reimport(Texture) == FALSE)
	{
		// Failed, restore the compression flag
		Texture->DeferCompression = SavedCompressionSetting;
	}
	else
	{
		// Invalidate the viewport and quick info windows
		NotifyPostChange(this, NULL);
	}
}

/**
 * Get the color channel filters which should be drawn/hidden on the texture, and return the appropriate 
 * enum so it can be ultimately passed through to BatchedElements::Draw to select the appropriate pixel shader
 *
 * @param	r	Red
 * @param	g	Green
 * @param	b	Blue
 * @param	a	Alpha
 * @param	d	Desaturation	
 */
ESimpleElementBlendMode WxTexturePropertiesFrame::GetSimpleElementBlendMode(UBOOL r, UBOOL g, UBOOL b, UBOOL a, UBOOL d)
{
	// Add the red, green, blue, alpha and desaturation flags to the enum to identify the chosen filters
	UINT mask = (UINT) SE_BLEND_RGBA_MASK_START;
	mask += r ? ( 1 << 0 ) : 0;
	mask += g ? ( 1 << 1 ) : 0;
	mask += b ? ( 1 << 2 ) : 0;
	mask += a ? ( 1 << 3 ) : 0;
	mask += d ? ( 1 << 4 ) : 0;
	
	return (ESimpleElementBlendMode)mask;
}


/**
 * Called in response to the user opting to show/hide a colour channel through the toggle buttons
 * 
 * @param	In	Command event from wxWidgets signifying the button press
 */
void WxTexturePropertiesFrame::OnChannelPressed( wxCommandEvent& In )
{
	RedColorChannelEnabled	= In.GetId() == ID_DLG_TEXTURE_TOGGLE_R_CHANNEL ? !RedColorChannelEnabled: RedColorChannelEnabled;
	GreenColorChannelEnabled= In.GetId() == ID_DLG_TEXTURE_TOGGLE_G_CHANNEL ? !GreenColorChannelEnabled: GreenColorChannelEnabled;
	BlueColorChannelEnabled = In.GetId() == ID_DLG_TEXTURE_TOGGLE_B_CHANNEL ? !BlueColorChannelEnabled: BlueColorChannelEnabled;
	AlphaColorChannelEnabled= In.GetId() == ID_DLG_TEXTURE_TOGGLE_A_CHANNEL ? !AlphaColorChannelEnabled: AlphaColorChannelEnabled;
	DesaturationEnabled		= In.GetId() == ID_DLG_TEXTURE_TOGGLE_DESATURATION ? !DesaturationEnabled: DesaturationEnabled;

	ColourChannelBlendMode = GetSimpleElementBlendMode( RedColorChannelEnabled, GreenColorChannelEnabled, BlueColorChannelEnabled, AlphaColorChannelEnabled, DesaturationEnabled );

	Viewport->Invalidate();
}

/**
 * Called in response to the frame's 'Compress Now' button being pressed by the user;
 * Compresses the texture if it is not already compressed.
 *
 * @param	In	Command event from wxWidgets signifying the button press
 */
void WxTexturePropertiesFrame::OnCompressNowPressed( wxCommandEvent& In )
{
	if (Texture->DeferCompression)
	{
		// Turn off defer compression and compress the texture
		Texture->DeferCompression = FALSE;

		Texture->CompressSourceArt();
		Texture->PostEditChange();

		// Invalidate the viewport and quick info windows
		NotifyPostChange(this, NULL);
	}
}

/**
 * Determines if the compress-now button should be enabled or not
 */
void WxTexturePropertiesFrame::UpdateCompressNowButton(wxUpdateUIEvent& In)
{
	In.Enable(Texture->DeferCompression == TRUE);
}

/** Called when sash position changes so we can remember the sash position. */
void WxTexturePropertiesFrame::OnSplitterPosChange(wxSplitterEvent& In)
{
	if (bAllowChangeSplitterPos)
	{
		//store from the right side
		IdealSplitterPos = GetClientRect().GetWidth() - In.GetSashPosition();
	} 

	//force viewport to update it's position
	ResizeViewportWindow();
}

/**Event to disable double clicking*/
void WxTexturePropertiesFrame::OnSplitterDblClk( wxSplitterEvent& InEvent )
{
	// Always disallow double-clicking on the splitter bars. Default behavior is otherwise to unsplit.
	InEvent.Veto();
}

/** Called when the user clicks the background button on the toolbar; displays the color picker */
void WxTexturePropertiesFrame::OnBackgroundButtonClick( wxCommandEvent& In )
{
	// Allow the user to use the color picker to select a new background color
	if ( PickProvidedColor( &BackgroundFillColor ) )
	{
		Viewport->Invalidate();
	}
}

/** Called when the user toggles the expand to frame option */
void WxTexturePropertiesFrame::OnExpandToFrameButtonClick( wxCommandEvent& In )
{
	bExpandTextureToFrame = !bExpandTextureToFrame;
	UpdateQuickInfoWindows();
	Viewport->Invalidate();
}

/** Automatically called by wxWidgets to update the UI for the expand to frame option accordingly */
void WxTexturePropertiesFrame::OnExpandToFrameButton_UpdateUI( wxUpdateUIEvent& In )
{
	In.Check( bExpandTextureToFrame == TRUE );
}

/** Called when the user toggles the display of the checkerboard on/off */
void WxTexturePropertiesFrame::OnCheckerboardButtonClick( wxCommandEvent& In )
{
	bDisplayCheckerboard = In.IsChecked();
	Viewport->Invalidate();
}

/** Called when the user right-clicks on the checkerboard button; displays a menu with advanced options */
void WxTexturePropertiesFrame::OnCheckerboardButtonRightClick( wxCommandEvent& In )
{
	wxMenu* CheckerboardMenu = new wxMenu();

	// Only allow the option to change checkerboard colors when compiling with managed code (or else the color picker won't work)
#if WITH_MANAGED_CODE
	CheckerboardMenu->Append( ID_DLG_TEXTURE_CHECKERBOARD_COLOR_ONE, *LocalizeUnrealEd("TexturePropertiesFrame_ChangeCheckerboardColorOne") );
	CheckerboardMenu->Append( ID_DLG_TEXTURE_CHECKERBOARD_COLOR_TWO, *LocalizeUnrealEd("TexturePropertiesFrame_ChangeCheckerboardColorTwo") );
#endif // #if WITH_MANAGED_CODE

	CheckerboardMenu->AppendCheckItem( ID_DLG_TEXTURE_CHECKERBOARD_USE_AS_BG, *LocalizeUnrealEd("TexturePropertiesFrame_UseCheckerboardAsBG") );

	// Add options for changing the base number of pixels per tile
	wxMenu* CheckerPixelMenu = new wxMenu();
	for ( INT CheckerIndex = 0; CheckerIndex < ( ID_DLG_TEXTURE_CHECKERBOARD_CHECKERNUM_END - ID_DLG_TEXTURE_CHECKERBOARD_CHECKERNUM_START ); ++CheckerIndex )
	{
		const INT PixelNum = MIN_CHECKER_PIXELS << CheckerIndex;
		CheckerPixelMenu->AppendCheckItem( ID_DLG_TEXTURE_CHECKERBOARD_CHECKERNUM_START + CheckerIndex, *appItoa( PixelNum ) );
	}
	CheckerboardMenu->Append( wxID_ANY, *LocalizeUnrealEd("TexturePropertiesFrame_CheckerNumPixels"), CheckerPixelMenu );

	// Display the advanced options menu for the checkerboard
	FTrackPopupMenu PopupMenu( this, CheckerboardMenu );
	PopupMenu.Show();
	delete CheckerboardMenu;
}

/** Called automatically by wxWidgets to update the UI related to the checkerboard button */
void WxTexturePropertiesFrame::OnCheckerboardButton_UpdateUI( wxUpdateUIEvent& In )
{
	In.Check( bDisplayCheckerboard == TRUE );
}

/** Called automatically by wxWidgets to update the UI related to the checkerboard num pixels menu item */
void WxTexturePropertiesFrame::OnCheckerboardCheckerPixelNum_UpdateUI( wxUpdateUIEvent& In )
{
	In.Check( ( MIN_CHECKER_PIXELS <<( In.GetId() - ID_DLG_TEXTURE_CHECKERBOARD_CHECKERNUM_START ) ) == NumPixelsPerCheckerTile );
}

/** Called when the user selects a number of checker pixels menu option */
void WxTexturePropertiesFrame::OnCheckerboardCheckerPixelNumMenu( wxCommandEvent& In )
{
	const INT SelectedNumPixels =  MIN_CHECKER_PIXELS << ( In.GetId() - ID_DLG_TEXTURE_CHECKERBOARD_CHECKERNUM_START );
	
	// If the user selected an option different from what is already applied, destroy the old texture and make a new one
	// of the correct size
	if ( NumPixelsPerCheckerTile != SelectedNumPixels )
	{
		NumPixelsPerCheckerTile = SelectedNumPixels;
		FlushRenderingCommands();
		DestroyCheckerboardTexture();
		SetupCheckerboardTexture();
	}
}

/** Called when the user selects a menu option to change a checkerboard color; prompts the user with the color picker */
void WxTexturePropertiesFrame::OnCheckerboardColorChangeMenu( wxCommandEvent& In )
{
	// If the user picked a color, modify the colors in the checkerboard texture
	if ( PickProvidedColor( In.GetId() == ID_DLG_TEXTURE_CHECKERBOARD_COLOR_ONE ? &CheckerboardColorOne : &CheckerboardColorTwo ) )
	{
		FlushRenderingCommands();
		ModifyCheckerboardTextureColors();
	}
}

/** Called when the user toggles the menu option to use the checkerboard as the background */
void WxTexturePropertiesFrame::OnCheckerboardUseAsBGMenu( wxCommandEvent& In )
{
	bUseCheckerboardBackground = !bUseCheckerboardBackground;
	Viewport->Invalidate();
}

/** Called automatically by wxWidgets to update the UI for the checkerboard as background menu option */
void WxTexturePropertiesFrame::OnCheckerboardUseAsBG_UpdateUI( wxUpdateUIEvent& In )
{
	In.Check( bUseCheckerboardBackground == TRUE );
}

/** Called when the user toggles the border display option */
void WxTexturePropertiesFrame::OnBorderButtonClick( wxCommandEvent& In )
{
	bDisplayTextureBorder = In.IsChecked();
	Viewport->Invalidate();
}

/** Called when the user right clicks on the border button; displays a menu of advanced options */
void WxTexturePropertiesFrame::OnBorderButtonRightClick( wxCommandEvent& In )
{
#if WITH_MANAGED_CODE
	wxMenu* BorderMenu = new wxMenu();
	BorderMenu->Append( ID_DLG_TEXTURE_BORDER_COLOR, *LocalizeUnrealEd("TexturePropertiesFrame_ChangeBorderColor") );

	FTrackPopupMenu PopupMenu( this, BorderMenu );
	PopupMenu.Show();
	delete BorderMenu;
#endif // #if WITH_MANAGED_CODE
}

/** Called when the user selects the menu option to change the border color; prompts the user with the color picker */
void WxTexturePropertiesFrame::OnBorderColorChangeMenu( wxCommandEvent& In )
{
	if ( PickProvidedColor( &TextureBorderColor ) )
	{
		Viewport->Invalidate();
	}
}

/** Called automatically by wxWidgets to update the UI for the border button */
void WxTexturePropertiesFrame::OnBorderButton_UpdateUI( wxUpdateUIEvent& In )
{
	In.Check( bDisplayTextureBorder == TRUE );
}

/** Called whenever the user clicks the padding spin control arrows */
void WxTexturePropertiesFrame::OnPaddingSpinCtrlClick( wxSpinEvent& In )
{
	if ( In.GetId() == ID_DLG_TEXTURE_PADDINGX )
	{
		PreviewPaddingX = In.GetPosition();
	}
	else
	{
		PreviewPaddingY = In.GetPosition();
	}
	UpdateQuickInfoWindows();
	Viewport->Invalidate();
}

/** Called when the user presses enter in the padding spin control textboxes */
void WxTexturePropertiesFrame::OnPaddingSpinCtrlEnterPressed( wxCommandEvent& In )
{
	// Set focus to the texture frame so that the spin control will lose focus
	// and update its value
	SetFocus();
	UpdateQuickInfoWindows();
}

/**
 * Called in response to the dialog being resized
 *
 * @param	In	Size event from wxWidgets signifying the dialog being resized
 */
void WxTexturePropertiesFrame::OnSize( wxSizeEvent& In )
{
	//stop ideal splitter position from being set during sash movement.
	bAllowChangeSplitterPos = FALSE;

	ResizeViewportWindow();

	if (LeftRightSplitter)
	{
		//assume we're going to the desired position
		INT SplitterPos = GetClientRect().width - IdealSplitterPos;
		const INT LeftMargin = 20;
		const INT RightMargin = GetClientSize().GetWidth() - 40;
		//Move from the right side first.  We should always see a bit of the info panel
		SplitterPos = Min(RightMargin, SplitterPos);
		//Now make sure you can see a portion of the texture
		SplitterPos = Max(LeftMargin, SplitterPos);

		LeftRightSplitter->SetSashPosition(SplitterPos);
	}

	In.Skip();

	//allow ideal splitter position to be changed only when the sash is grabbed directly
	bAllowChangeSplitterPos = TRUE;
}

void WxTexturePropertiesFrame::ResizeViewportWindow(void)
{
	// Must call Layout() ourselves unless we set wxWindow::SetAutoLayout
	Layout();

	// Update viewport dimensions
	if( Viewport != NULL )
	{
		Viewport->Invalidate();
	}
}



/**
 * Called in response to the user closing the dialog (such as via the X)
 *
 * @param	In	Close event from wxWidgets signifying the dialog has been closed
 */
void WxTexturePropertiesFrame::OnClose( wxCloseEvent& In )
{
	QuickInfoWindow->ClearChildren();
	QuickInfoWindowB->ClearChildren();
	PropertyWindow->FlushLastFocused();
	Destroy();
}

/**
 * Draw the texture within the frame
 *
 * @param	Viewport	The viewport being drawn in
 * @param	Canvas		The render interface to draw with
 */
void WxTexturePropertiesFrame::Draw(FViewport* Viewport,FCanvas* Canvas)
{
	Clear( Canvas, BackgroundFillColor );

	// Get the rendering info for this object
	FThumbnailRenderingInfo* RenderInfo =
		GUnrealEd->GetThumbnailManager()->GetRenderingInfo(Texture);

	// If there is an object configured to handle it, draw the thumbnail
	if (RenderInfo != NULL && RenderInfo->Renderer != NULL)
	{
		// Fully stream in the texture before drawing it.
		UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
		if ( Texture2D )
		{
			Texture2D->SetForceMipLevelsToBeResident(30.0f);
			Texture2D->WaitForStreaming();
		}

		// Figure out the size we need
		DWORD Width, Height, ImportedWidth, ImportedHeight;
		CalculateTextureDimensions(PreviewEffectiveTextureWidth, PreviewEffectiveTextureHeight, Width, Height, ImportedWidth, ImportedHeight);

		// The texture we will be rendering
		UTexture* TextureToRender = Texture;

		if( Texture2D && RenderInfo->Renderer->SupportsCPUGeneratedThumbnail( Texture ) )
		{
			if( !MonochromeThumbnail )
			{
				FObjectThumbnail Thumbnail;
				Thumbnail.SetImageSize( Width, Height );

				const EObjectFlags ObjectFlags = RF_NotForClient | RF_NotForServer | RF_Transient;
				MonochromeThumbnail = CastChecked< UTexture2D >( UObject::StaticConstructObject( UTexture2D::StaticClass(), UObject::GetTransientPackage(), NAME_None, ObjectFlags ) );
				MonochromeThumbnail->Init( Width, Height, PF_A8R8G8B8 );
				
				RenderInfo->Renderer->DrawCPU( Texture2D, Thumbnail );
				BYTE* DestData = ( BYTE* )MonochromeThumbnail->Mips(0).Data.Lock( LOCK_READ_WRITE );
				appMemcpy( DestData, &Thumbnail.AccessImageData()(0), sizeof(BYTE)*4*Width*Height);
				MonochromeThumbnail->Mips(0).Data.Unlock();
				MonochromeThumbnail->UpdateResource();
			}

			TextureToRender = MonochromeThumbnail;
		}

		// Draw the background checkerboard pattern in the same size/position as the render texture so it will show up anywhere
		// the texture has transparency
		if ( bDisplayCheckerboard )
		{
			// Handle case of using the checkerboard as a background
			if ( bUseCheckerboardBackground )
			{
				DrawTile( Canvas, 0.0f, 0.0f, Viewport->GetSizeX(), Viewport->GetSizeY(), 0.0f, 0.0f,
					Viewport->GetSizeX()/CheckerboardTexture->GetOriginalSurfaceWidth(),
					Viewport->GetSizeY()/CheckerboardTexture->GetOriginalSurfaceHeight(),
					FLinearColor::White,
					CheckerboardTexture->Resource );
			}
			else
			{
				DrawTile( Canvas, PreviewPaddingX, PreviewPaddingY, Width, Height, 0.0f, 0.0f, 
					Width/CheckerboardTexture->GetOriginalSurfaceWidth(),
					Height/CheckerboardTexture->GetOriginalSurfaceHeight(),
					FLinearColor::White,
					CheckerboardTexture->Resource );
			}
		}

		//Draw the selected texture, uses ColourChannelBlend mode parameter to filter colour channels and apply grayscale
		DrawTile(Canvas,PreviewPaddingX,PreviewPaddingY,Width,Height,0.f,0.f,1.f,1.f,FLinearColor::White,
			TextureToRender->Resource,ColourChannelBlendMode);

		// Draw a white border around the texture to show its extents
		if ( bDisplayTextureBorder )
		{
			DrawBox2D( Canvas, FVector2D( PreviewPaddingX, PreviewPaddingY ), FVector2D( Width + PreviewPaddingX, Height + PreviewPaddingY ), TextureBorderColor );
		}
	}
}

/**
 * Respond to the destruction of the frame's windows
 *
 * @param	Src	The destroyed object
 */
void WxTexturePropertiesFrame::NotifyDestroy( void* Src )
{
	if( Src == PropertyWindow )
	{
		PropertyWindow = NULL;
	}
	else if( Src == QuickInfoWindow )
	{
		QuickInfoWindow = NULL;
	}
	else if( Src == QuickInfoWindowB )
	{
		QuickInfoWindowB = NULL;
	}
}

/**
 * Respond to a property being changed
 *
 * @param	Src						The changed object
 * @param	PropertyThatChanged		The property that changed
 */
void WxTexturePropertiesFrame::NotifyPostChange( void* Src, UProperty* PropertyThatChanged )
{
	Viewport->Invalidate();
	UpdateQuickInfoWindows();
}


/**
 *  Find the effective in-game resolution of the texture.
 */
void WxTexturePropertiesFrame::CalculateEffectiveTextureDimensions(INT LODBias, DWORD& EffectiveTextureWidth, DWORD& EffectiveTextureHeight)
{
	//Calculate in-game max resolution and store in EffectiveTextureWidth, EffectiveTextureHeight
	GSystemSettings.TextureLODSettings.ComputeInGameMaxResolution(LODBias, *Texture, (UINT &)EffectiveTextureWidth, (UINT &)EffectiveTextureHeight);
}

/**
 *  Determine the draw dimensions of the texture in the preview window
 *
 *  @param Width			The width of the texture in the preview window
 *  @param Height			The height of the texture in the preview window
 *  @param ImportedWidth	The full resolution width of the texture
 *  @param ImportedHeight	The full resolution height of the texture
 */
void WxTexturePropertiesFrame::CalculateTextureDimensions( DWORD EffectiveTextureWidth, DWORD EffectiveTextureHeight, DWORD& Width, DWORD& Height, DWORD& ImportedWidth, DWORD& ImportedHeight )
{
	ImportedWidth = Texture->GetOriginalSurfaceWidth();
	ImportedHeight = Texture->GetOriginalSurfaceHeight();

	// If Original Width and Height are 0, use the saved current width and height
	if(ImportedWidth == 0 && ImportedHeight == 0)
	{
		ImportedWidth = Texture->GetSurfaceWidth();
		ImportedHeight = Texture->GetSurfaceHeight();
	}

	Width = ImportedWidth;
	Height = ImportedHeight;

	// See if we need to uniformly scale it to fit in viewport
	// Cap the size to effective dimensions
	DWORD ViewportW = Viewport->GetSizeX();
	DWORD ViewportH = Viewport->GetSizeY();
	DWORD MaxWidth; 
	DWORD MaxHeight; 

	const INT PaddedPixelsX = PreviewPaddingX << 1;
	const INT PaddedPixelsY = PreviewPaddingY << 1;

	if( !bExpandTextureToFrame )
	{
		// Subtract off the viewport space devoted to padding (2 * PreviewPadding)
		// so that the texture is padded on all sides
		MaxWidth = Min( ViewportW - PaddedPixelsX, EffectiveTextureWidth );
		MaxHeight = Min( ViewportH - PaddedPixelsY, EffectiveTextureHeight );
		if( Width > MaxWidth )
		{
			Height = Height * MaxWidth / Width;
			Width = MaxWidth;
		}
		if( Height > MaxHeight )
		{
			Width = Width * MaxHeight / Height;
			Height = MaxHeight;
		}
	}
	else
	{
		// Subtract off the viewport space devoted to padding (2 * PreviewPadding)
		// so that the texture is padded on all sides
		MaxWidth = ViewportW - PaddedPixelsX;
		MaxHeight = ViewportH - PaddedPixelsY;

		// First, scale up based on the size of the viewport
		if( MaxWidth > MaxHeight )
		{
			Height = Height * MaxWidth / Width;
			Width = MaxWidth;
		}
		else
		{
			Width = Width * MaxHeight / Height;
			Height = MaxHeight;
		}
		// then, scale again if our width and height is impacted by the scaling
		if( Width > MaxWidth )
		{
			Height = Height * MaxWidth / Width;
			Width = MaxWidth;
		}
		if( Height > MaxHeight )
		{
			Width = Width * MaxHeight / Height;
			Height = MaxHeight;
		}
	}
}

/**
 *  Clears and recreates the stats in the quick info window
 */
void WxTexturePropertiesFrame::UpdateQuickInfoWindows()
{
	check(QuickInfoWindow);
	check(QuickInfoWindowB);

	QuickInfoWindow->ClearChildren();
	QuickInfoWindowB->ClearChildren();

	// Figure out the sizes for the texture
	DWORD Width, Height, ImportedWidth, ImportedHeight;
	UINT LODBiasRegular = UINT( GSystemSettings.TextureLODSettings.CalculateLODBias(Texture) );
	CalculateEffectiveTextureDimensions(LODBiasRegular, PreviewEffectiveTextureWidth, PreviewEffectiveTextureHeight);
	CalculateTextureDimensions( PreviewEffectiveTextureWidth, PreviewEffectiveTextureHeight, Width, Height, ImportedWidth, ImportedHeight );

	// Imported Dimensions
	const wxString& ImportedResString = wxString(*FString::Printf( LocalizeSecure(LocalizeUnrealEd("TextureQuickInfoImportedRes"), ImportedWidth, ImportedHeight) ));
	QuickInfoWindow->AddInfoString( ImportedResString );

	
	// Displayed Dimensions
	const wxString& DisplayResString = wxString(*FString::Printf( LocalizeSecure(LocalizeUnrealEd("TextureQuickInfoDisplayedRes"), Width, Height) ));
	QuickInfoWindow->AddInfoString( DisplayResString );
	
	// Game Dimensions
	const wxString& EffectiveResString = wxString(*FString::Printf( LocalizeSecure(LocalizeUnrealEd("TextureQuickInfoEffectiveRes"), PreviewEffectiveTextureWidth, PreviewEffectiveTextureHeight) ));
	QuickInfoWindow->AddInfoString( EffectiveResString );

	// IsStreamed
	const wxString& MethodString = wxString(*FString::Printf( LocalizeSecure(LocalizeUnrealEd("TextureQuickInfoStreamedMethod"), 
												Texture->NeverStream ? 
												*LocalizeUnrealEd("TextureQuickInfoIsNotStreamed") :
												*LocalizeUnrealEd("TextureQuickInfoIsStreamed")) ));
	QuickInfoWindow->AddInfoString( MethodString );

	// Pixel Format
	UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
	if( Texture2D )
	{
		const wxString& FormatString = wxString(*FString::Printf( LocalizeSecure(LocalizeUnrealEd("TextureQuickInfoPixelFormat"), GPixelFormats[Texture2D->Format].Name) ));
		QuickInfoWindow->AddInfoString( FormatString );
	}

	// LOD Bias
	const wxString& LODBiasString = wxString(*FString::Printf( LocalizeSecure(LocalizeUnrealEd("TextureQuickInfoLODBias"), LODBiasRegular) ));
	QuickInfoWindowB->AddInfoString( LODBiasString );

	// Xbox 360 LOD Bias
	INT LODBias360 = LODSettings360.CalculateLODBias(Texture);
	const wxString& TextureLODBias360String = wxString(*FString::Printf( LocalizeSecure(LocalizeUnrealEd("TextureQuickInfo360LODBias"), LODBias360) ));
	QuickInfoWindowB->AddInfoString( TextureLODBias360String );

	// Xbox 360 In-game Dimensions
	DWORD TextureWidth360, TextureHeight360;
	CalculateEffectiveTextureDimensions(LODBias360, TextureWidth360, TextureHeight360);
	const wxString& TextureSize360String = wxString(*FString::Printf( LocalizeSecure(LocalizeUnrealEd("TextureQuickInfo360EffectiveRes"), TextureWidth360, TextureHeight360) ));
	QuickInfoWindowB->AddInfoString( TextureSize360String );

	// PS3 LOD Bias
	INT LODBiasPS3 = LODSettingsPS3.CalculateLODBias(Texture);
	const wxString& TextureLODBiasPS3String =  wxString(*FString::Printf( LocalizeSecure(LocalizeUnrealEd("TextureQuickInfoPS3LODBias"), LODBiasPS3) ));
	QuickInfoWindowB->AddInfoString( TextureLODBiasPS3String );

	// PS3 In-game Dimensions
	DWORD TextureWidthPS3, TextureHeightPS3;
	CalculateEffectiveTextureDimensions(LODBiasPS3, TextureWidthPS3, TextureHeightPS3);
	const wxString& TextureSizePS3String = wxString(*FString::Printf( LocalizeSecure(LocalizeUnrealEd("TextureQuickInfoPS3EffectiveRes"), TextureWidthPS3, TextureHeightPS3) ));
	QuickInfoWindowB->AddInfoString( TextureSizePS3String );
}

/** Initialize the checkerboard texture for the texture preview, if necessary */
void WxTexturePropertiesFrame::SetupCheckerboardTexture()
{
	if ( !CheckerboardTexture )
	{	
		// Construct a new checkerboard texture
		CheckerboardTexture = ConstructObject<UTexture2D>( UTexture2D::StaticClass(), UObject::GetTransientPackage(), NAME_None );
		CheckerboardTexture->Init( NumPixelsPerCheckerTile, NumPixelsPerCheckerTile, PF_A8R8G8B8 );
		
		// Don't need compression or mip updates
		CheckerboardTexture->CompressionNone = TRUE;
		CheckerboardTexture->MipGenSettings = TMGS_LeaveExistingMips;
		
		// Setup the texture colors
		ModifyCheckerboardTextureColors();
	}
	check( CheckerboardTexture );
}

/** Modifies the checkerboard texture's data to match the colors currently specified by CheckerboardColorOne and CheckerboardColorTwo */
void WxTexturePropertiesFrame::ModifyCheckerboardTextureColors()
{
	const INT HalfPixelNum = NumPixelsPerCheckerTile >> 1;

	// Lock the checkerboard texture so it can be modified
	FColor* MipData = static_cast<FColor*>( CheckerboardTexture->Mips(0).Data.Lock( LOCK_READ_WRITE ) );
	
	// Fill in the colors in a checkerboard pattern
	for ( INT RowNum = 0; RowNum < NumPixelsPerCheckerTile; ++RowNum )
	{
		for ( INT ColNum = 0; ColNum < NumPixelsPerCheckerTile; ++ColNum )
		{
			FColor& CurColor = MipData[( ColNum + ( RowNum * NumPixelsPerCheckerTile ) )];

			if ( ColNum < HalfPixelNum )
			{
				CurColor = ( RowNum < HalfPixelNum ) ? CheckerboardColorOne : CheckerboardColorTwo;
			}
			else
			{
				CurColor = ( RowNum < HalfPixelNum ) ? CheckerboardColorTwo : CheckerboardColorOne;
			}
		}
	}

	// Unlock the texture
	CheckerboardTexture->Mips(0).Data.Unlock();
	CheckerboardTexture->PostEditChange();
	Viewport->Invalidate();
}

/** Destroy the checkerboard texture if one exists */
void WxTexturePropertiesFrame::DestroyCheckerboardTexture()
{
	if ( CheckerboardTexture )
	{
		if ( CheckerboardTexture->Resource )
		{
			CheckerboardTexture->ReleaseResource();
		}
		CheckerboardTexture->MarkPendingKill();
		CheckerboardTexture = NULL;
	}
}

/**
 * Prompt the user with the color picker to pick a color for the provided FColor pointer
 *
 * @param	InColorToPick	Data to populate with the user picked color, if any
 *
 * @return	TRUE if the user picked a color; FALSE if they did not
 */
UBOOL WxTexturePropertiesFrame::PickProvidedColor( FColor* InColorToPick )
{
	// Configure color picker options
	FPickColorStruct PickColorStruct;
	PickColorStruct.RefreshWindows.AddItem( this );
	PickColorStruct.DWORDColorArray.AddItem( InColorToPick );
	PickColorStruct.bModal = FALSE;

	return ( PickColor( PickColorStruct ) == ColorPickerConstants::ColorAccepted );
}

/** Load user preferences associated with the preview window from INI file */
void WxTexturePropertiesFrame::LoadPreviewPreferencesFromINI()
{
	GConfig->GetInt( TEXT("TextureProperties"), TEXT("PreviewPaddingX"), PreviewPaddingX, GEditorUserSettingsIni );
	GConfig->GetInt( TEXT("TextureProperties"), TEXT("PreviewPaddingY"), PreviewPaddingY, GEditorUserSettingsIni );
	Clamp<INT>( PreviewPaddingX, 0, PreviewPaddingX );
	Clamp<INT>( PreviewPaddingY, 0, PreviewPaddingY );

	GConfig->GetColor( TEXT("TextureProperties"), TEXT("BackgroundFillColor"), BackgroundFillColor, GEditorUserSettingsIni );
	GConfig->GetColor( TEXT("TextureProperties"), TEXT("TextureBorderColor"), TextureBorderColor, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("TextureProperties"), TEXT("ExpandTextureToViewportFrame"), bExpandTextureToFrame, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("TextureProperties"), TEXT("ShowTextureBorder"), bDisplayTextureBorder, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("TextureProperties"), TEXT("ShowCheckerboard"), bDisplayCheckerboard, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("TextureProperties"), TEXT("UseCheckerboardAsBG"), bUseCheckerboardBackground, GEditorUserSettingsIni );
	GConfig->GetInt( TEXT("TextureProperties"), TEXT("CheckerboardCheckerPixelNum"), NumPixelsPerCheckerTile, GEditorUserSettingsIni );
	NumPixelsPerCheckerTile = appRoundUpToPowerOfTwo( NumPixelsPerCheckerTile );

	GConfig->GetColor( TEXT("TextureProperties"), TEXT("CheckerboardColorOne"), CheckerboardColorOne, GEditorUserSettingsIni );
	GConfig->GetColor( TEXT("TextureProperties"), TEXT("CheckerboardColorTwo"), CheckerboardColorTwo, GEditorUserSettingsIni );
}

/** Save user preferences associated with the preview window to the INI file */
void WxTexturePropertiesFrame::SavePreviewPreferencesToINI()
{
	GConfig->SetInt( TEXT("TextureProperties"), TEXT("PreviewPaddingX"), PreviewPaddingX, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("TextureProperties"), TEXT("PreviewPaddingY"), PreviewPaddingY, GEditorUserSettingsIni );
	GConfig->SetColor( TEXT("TextureProperties"), TEXT("BackgroundFillColor"), BackgroundFillColor, GEditorUserSettingsIni );
	GConfig->SetColor( TEXT("TextureProperties"), TEXT("TextureBorderColor"), TextureBorderColor, GEditorUserSettingsIni );
	GConfig->SetBool( TEXT("TextureProperties"), TEXT("ExpandTextureToViewportFrame"), bExpandTextureToFrame, GEditorUserSettingsIni );
	GConfig->SetBool( TEXT("TextureProperties"), TEXT("ShowTextureBorder"), bDisplayTextureBorder, GEditorUserSettingsIni );
	GConfig->SetBool( TEXT("TextureProperties"), TEXT("ShowCheckerboard"), bDisplayCheckerboard, GEditorUserSettingsIni );
	GConfig->SetBool( TEXT("TextureProperties"), TEXT("UseCheckerboardAsBG"), bUseCheckerboardBackground, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("TextureProperties"), TEXT("CheckerboardCheckerPixelNum"), appRoundUpToPowerOfTwo( NumPixelsPerCheckerTile ), GEditorUserSettingsIni );
	GConfig->SetColor( TEXT("TextureProperties"), TEXT("CheckerboardColorOne"), CheckerboardColorOne, GEditorUserSettingsIni );
	GConfig->SetColor( TEXT("TextureProperties"), TEXT("CheckerboardColorTwo"), CheckerboardColorTwo, GEditorUserSettingsIni );
}

/**
 * Called when the viewport has been resized
 *
 * @param	InType					The event type
 * @param	InViewport				The viewport that changed
 * @param	InMessage				Message enum.  Unused here.
 */
void WxTexturePropertiesFrame::Send(ECallbackEventType InType, FViewport* InViewport, UINT InMessage)
{
	if ( InType == CALLBACK_ViewportResized )
	{
		// Update the quick info window
		if( (QuickInfoWindow != NULL) && (QuickInfoWindowB != NULL) && (Texture != NULL) )
		{
			UpdateQuickInfoWindows();
		}
	}
}

/* === FCallbackEventDevice interface === */
/**
 * Called when UObject is changed
 *
 * @param	InType					The event type
 * @param	InObject				The UObject that changed
 */
void WxTexturePropertiesFrame::Send(ECallbackEventType InType,UObject* InObject)
{
	if ( InType == CALLBACK_TextureModified )
	{ 
		if(InObject == Texture)
		{
			this->RedrawRequested(this->Viewport);
		}
	}
}

/**
 * Serialize out any UObject references held by the window
 *
 * @param	Ar	The archive to serialize with
 */
void WxTexturePropertiesFrame::Serialize( FArchive& Ar )
{
	Ar << Texture << MonochromeThumbnail << CheckerboardTexture;
}

/*------------------------------------------------------------------------------
	Source Asset Helper functions
------------------------------------------------------------------------------*/

/**
 * Helper function to quickly add commands related to source assets
 *
 * @param	OutCommands		Array to add source asset commands to
 * @param	bEnabled		Whether the source asset commands should be enabled or not
 */
void AddSourceAssetCommands( TArray<FObjectSupportedCommandType>& OutCommands, UBOOL bEnabled )
{
	const INT ParentIndex = OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Source, *LocalizeUnrealEd("Source") ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_FindSourceInExplorer, *LocalizeUnrealEd("FindInExplorer"), bEnabled, ParentIndex ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_OpenSourceInExternalEditor, *LocalizeUnrealEd("OpenInExternalEditor"), bEnabled, ParentIndex ) );
}

/**
 * Helper function that "explores" the folder of the provided file, assuming the file exists
 *
 * @param	SourceFileName	Name of the file whose folder should be explored
 */
void ExploreSourceAssetFolder( const FString& SourceFileName )
{
	// Verify that the file exists prior to asking for its folder to be explored
	if ( SourceFileName.Len() && GFileManager->FileSize( *SourceFileName ) != INDEX_NONE )
	{
		appExploreFolder( *( FFilename( SourceFileName ).GetPath() ) );
	}
}

/**
 * Helper function that attempts to open the provided file in its default external editor/application,
 * provided that the file exists
 *
 * @param	SourceFileName	Name of the file to attempt to open in its default external editor
 */
void OpenSourceAssetInExternalEditor( const FString& SourceFileName )
{
	// Verify that the file exists prior to attempting to launch its default external editor
	if ( SourceFileName.Len() && GFileManager->FileSize( *SourceFileName ) != INDEX_NONE )
	{
		appLaunchFileInDefaultExternalApplication( *SourceFileName );
	}
}

/*------------------------------------------------------------------------------
UGenericBrowserType
------------------------------------------------------------------------------*/

/**
* Clear out any old data before calling Init() again
*/
void UGenericBrowserType::Clear()
{
	SupportInfo.Empty();
}

UBOOL UGenericBrowserType::Supports( UObject* InObject ) const
{
	check( InObject );
	for( INT x = 0 ; x < SupportInfo.Num() ; ++x )
	{
		if( SupportInfo(x).Supports(InObject) )
		{
			return TRUE;
		}
	}

	return 0;
}

FColor UGenericBrowserType::GetBorderColor( UObject* InObject )
{
	check( InObject );
	for( INT x = 0 ; x < SupportInfo.Num() ; ++x )
	{
		if( SupportInfo(x).Supports(InObject) )
		{
			return SupportInfo(x).BorderColor;
		}
	}

	return FColor(255,255,255);
}


/**
* Factorizes out the creation of a new property window frame for the
* UGenericBrowserType::ShowOBjectProperties(...) family of methods.
*/
static inline void GenericBrowser_CreateNewPropertyWindowFrame()
{
	if(!GApp->ObjectPropertyWindow)
	{
		GApp->ObjectPropertyWindow = new WxPropertyWindowFrame;
		GApp->ObjectPropertyWindow->Create( GApp->EditorFrame, -1, GUnrealEd );
		GApp->ObjectPropertyWindow->SetSize( 64,64, 350,600 );
	}
}

/**
* Generic implementation for opening a property window for an object.
*/
UBOOL UGenericBrowserType::ShowObjectProperties( UObject* InObject )
{
	GenericBrowser_CreateNewPropertyWindowFrame();
	GApp->ObjectPropertyWindow->SetObject( InObject, EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories  );
	GApp->ObjectPropertyWindow->Show();
	GApp->ObjectPropertyWindow->Raise();
	return TRUE;
}

/**
* Generic implementation for opening a property window for a set of objects.
*/
UBOOL UGenericBrowserType::ShowObjectProperties( const TArray<UObject*>& InObjects )
{
	GenericBrowser_CreateNewPropertyWindowFrame();
	GApp->ObjectPropertyWindow->SetObjectArray( InObjects, EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories  );
	GApp->ObjectPropertyWindow->Show();
	GApp->ObjectPropertyWindow->Raise();
	return TRUE;
}

/**
* Invokes the editor for all selected objects.
*/
UBOOL UGenericBrowserType::ShowObjectEditor()
{
	UBOOL bAbleToShowEditor = FALSE;

	// Loop through all of the selected objects and see if any of our support classes support this object.
	// If one of them does, show the editor specific to this browser type and continue on to the next object.
	for ( USelection::TObjectIterator It( GEditor->GetSelectedObjects()->ObjectItor() ) ; It ; ++It )
	{
		UObject* Object = *It;

		for( INT SupportIdx = 0 ; SupportIdx < SupportInfo.Num() ; ++SupportIdx )
		{
			if( Object && SupportInfo(SupportIdx).Supports( Object ) && ShowObjectEditor( Object ) )
			{
				bAbleToShowEditor =  TRUE;
				break;
			}
		}
	}

	return bAbleToShowEditor;
}

/**
* Displays the object properties window for all selected objects that this GenericBrowserType supports.
*/
UBOOL UGenericBrowserType::ShowObjectProperties()
{
	TArray<UObject*> SelectedObjects;
	GEditor->GetSelectedObjects()->GetSelectedObjects(SelectedObjects);
	return ShowObjectProperties(SelectedObjects);
}

void UGenericBrowserType::InvokeCustomCommand(INT InCommand)
{
	for( INT i = 0 ; i < SupportInfo.Num() ; ++i )
	{
		TArray <UObject*> ValidObjects;
		for ( USelection::TObjectIterator It( GEditor->GetSelectedObjects()->ObjectItor() ) ; It ; ++It )
		{
			UObject* Object = *It;
			if ( Object && SupportInfo(i).Supports( Object ) )
			{
				ValidObjects.AddItem(Object);
			}
		}
		if (ValidObjects.Num())
		{
			InvokeCustomCommand( InCommand, ValidObjects );
		}
	}
}

void UGenericBrowserType::DoubleClick()
{
	for( INT i = 0 ; i < SupportInfo.Num() ; ++i )
	{
		for ( USelection::TObjectIterator It( GEditor->GetSelectedObjects()->ObjectItor() ) ; It ; ++It )
		{
			UObject* Object = *It;
			if ( Object && SupportInfo(i).Supports( Object ) )
			{
				DoubleClick( Object );
			}
		}
	}
}

void UGenericBrowserType::DoubleClick( UObject* InObject )
{
	// This is what most types want to do with a double click so we
	// handle this in the base class.  Override this function to
	// implement a custom behavior.

	if ( Supports(InObject) )
	{
		ShowObjectEditor( InObject );
	}
}

void UGenericBrowserType::GetSelectedObjects( TArray<UObject*>& Objects )
{
	for ( INT i = 0; i < SupportInfo.Num(); i++ )
	{
		for ( USelection::TObjectIterator It( GEditor->GetSelectedObjects()->ObjectItor() ) ; It ; ++It )
		{
			UObject* Object = *It;
			if ( Object && SupportInfo(i).Supports( Object ) )
			{
				Objects.AddUniqueItem( Object );
			}
		}
	}
}

/**
* Determines whether the specified package is allowed to be saved.
*/
UBOOL UGenericBrowserType::IsSavePackageAllowed( UPackage* PackageToSave )
{
	UBOOL bResult = TRUE;

	if ( PackageToSave != NULL )
	{
		TArray<UObject*> StandaloneObjects;
		for ( TObjectIterator<UObject> It; It; ++It )
		{
			if ( It->IsIn(PackageToSave) && It->HasAnyFlags(RF_Standalone) && It->GetClass() != UObjectRedirector::StaticClass() )
			{
				StandaloneObjects.AddItem(*It);
			}
		}

		for( INT SupportIdx = 0 ; SupportIdx < SupportInfo.Num() ; ++SupportIdx )
		{
			UGenericBrowserType* InfoType = SupportInfo(SupportIdx).BrowserType;
			if ( InfoType != NULL && !InfoType->IsSavePackageAllowed(PackageToSave, StandaloneObjects) )
			{
				bResult = FALSE;
				break;
			}
		}
	}

	return bResult;
}



/**
* Static: Returns true if any of the specified objects have already been cooked
*/
UBOOL UGenericBrowserType::AnyObjectsAreCooked( USelection* InObjects )
{
	// Iterate over the selection set looking for a cooked object.
	UBOOL bFoundCookedObject = FALSE;
	for ( USelection::TObjectConstIterator It( InObjects->ObjectConstItor() ) ; It ; ++It )
	{
		UObject* Object = *It;
		if ( Object )
		{
			const UPackage* ObjectPackage = Object->GetOutermost();
			if ( ObjectPackage->PackageFlags & PKG_Cooked )
			{
				bFoundCookedObject = TRUE;
				break;
			}
		}
	}

	return bFoundCookedObject;
}



/**
* Static: Returns a list of standard context menu commands supported by the specified objects
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType::QueryStandardSupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands )
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );
	INT NumNonNullObjects = 0;

	// Check to see what type of renaming is supported by these objects
	UBOOL bSupportsNormalRename = FALSE;
	UBOOL bSupportsLocalizedRename = FALSE;

	// Check to see if consolidation is supported by these objects
	UBOOL bSupportsConsolidation = FALSE;
	UBOOL bSupportsSubArchetypeCreation = FALSE;
	TArray<UObject*> QueryObjects;

	{
		INT NormalRenamingCount = 0;
		INT LocRenamingCount = 0;
		for( USelection::TObjectIterator It( InObjects->ObjectItor() ); It; ++It )
		{
			UObject* Object = *It;
			if (Object)
			{
				if(Object->IsA(USoundNodeWave::StaticClass()))
				{
					++LocRenamingCount;
				}
				else
				{
					++NormalRenamingCount;
				}

				UClass* ObjectClass = Object->GetClass();
				if( !(ObjectClass->ClassFlags & CLASS_Deprecated) &&
					!(ObjectClass->ClassFlags & CLASS_Abstract) &&
					!ObjectClass->IsChildOf(UUIRoot::StaticClass()) )
				{
					//Only support subarchetypes right now
					if(Object->HasAnyFlags(RF_ArchetypeObject))
					{
						bSupportsSubArchetypeCreation = TRUE;
					}
				}

				QueryObjects.AddItem(Object);
				NumNonNullObjects++;
			}
		}

		if( NormalRenamingCount == NumNonNullObjects )
		{
			bSupportsNormalRename = TRUE;
		}
		if( LocRenamingCount == NumNonNullObjects )
		{
			bSupportsLocalizedRename = TRUE;
		}
	}

#if WITH_MANAGED_CODE
	// Consolidation is only possible if all objects are compatible with objects already in the consolidation window
	TArray<UObject*> ConsolidatableObjects;
	if ( FConsolidateWindow::DetermineAssetCompatibility( QueryObjects, ConsolidatableObjects ) )
	{
		bSupportsConsolidation = ( ConsolidatableObjects.Num() > 0 );
	}
#endif // #if WITH_MANAGED_CODE

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_DuplicateWithRefs, *LocalizeUnrealEd( "ObjectContext_Duplicate" ), !bAnythingCooked ) );
	if( bSupportsNormalRename )
	{
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_RenameWithRefs, *LocalizeUnrealEd( "ObjectContext_MoveOrRename" ), !bAnythingCooked ) );
	}
	else if( bSupportsLocalizedRename )
	{
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_RenameLocWithRefs, *LocalizeUnrealEd( "ObjectContext_MoveOrRenameLoc" ), !bAnythingCooked ) );
	}
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Delete, *LocalizeUnrealEd( "ObjectContext_Delete" ), !bAnythingCooked ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_DeleteWithRefs, *LocalizeUnrealEd( "ObjectContext_ForceDelete" ), !bAnythingCooked ) );

	if ( bSupportsConsolidation )
	{
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_ConsolidateObjs, *LocalizeUnrealEd( "ObjectContext_ConsolidateObjs"), !bAnythingCooked ) );
	}

	OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );

	if( NumNonNullObjects == 1 )
	{
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_SelectInLevel, *LocalizeUnrealEd( "ObjectContext_SelectActorsUsingThis" ) ) );
		OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );
	}

	if( NumNonNullObjects == 1 && bSupportsSubArchetypeCreation )
	{
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CreateArchetype, LocalizeUnrealEd( "ObjectContext_CreateSubArchetype" ) ) );
	}

	// @todo CB [reviewed; discuss]: Should clean up Show/Copy refs... Use a text field panel that you can copy from yourself (provide a 'copy to clipboard' button)
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CopyReference, *LocalizeUnrealEd( "ObjectContext_CopyReferencesToClipboard" ) ) );
	if( NumNonNullObjects == 1 )
	{
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_ShowRefObjs, *LocalizeUnrealEd( "ObjectContext_ShowReferences" ) ) );
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_GenerateResourceCollection, *LocalizeUnrealEd( "ObjectContext_ResourceCollection" ) ) );
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_ShowRefs, *LocalizeUnrealEd( "ObjectContext_ShowReferencers" ) ) );
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_ShowReferenceGraph, *LocalizeUnrealEd( "ObjectContext_ShowReferenceGraph" ) ) );
	}

	OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Export, *LocalizeUnrealEd( "ObjectContext_ExportToFile" ), !bAnythingCooked ) );
	if( NumNonNullObjects > 1 )
	{
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_BulkExport, *LocalizeUnrealEd( "ObjectContext_BulkExport" ), !bAnythingCooked ) );
	}
}



/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	// NOTE: This implementation should be overridden in derived classes where appropriate!

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Properties, *LocalizeUnrealEd( "ObjectContext_EditProperties" ) ) );
}

/**
* Returns the default command to be executed given the selected object.
*
* @param	InObject		The objects to query the default command for
*
* @return The ID of the default action command (i.e. command that happens on double click or enter).
*/
INT UGenericBrowserType::QueryDefaultCommand( TArray<UObject*>& InObjects ) const
{
	return INDEX_NONE;
}



/*------------------------------------------------------------------------------
UGenericBrowserType_All
------------------------------------------------------------------------------*/

/**
* Does any initial set up that the type requires.
*/
void UGenericBrowserType_All::Init()
{
	for( TObjectIterator<UGenericBrowserType> It ; It ; ++It )
	{
		UGenericBrowserType* BrowserType = *It;

		if( !Cast<UGenericBrowserType_All>(BrowserType) && !Cast<UGenericBrowserType_Custom>(BrowserType) )
		{
			for( INT SupportIdx = 0 ; SupportIdx < BrowserType->SupportInfo.Num() ; ++SupportIdx )
			{
				const FGenericBrowserTypeInfo &TypeInfo = BrowserType->SupportInfo(SupportIdx);
				const UClass* ResourceClass = TypeInfo.Class;
				INT InsertIdx = -1;
				UBOOL bAddItem = TRUE;

				// Loop through all the existing classes checking for duplicate classes and making sure the point of insertion for a class
				// is before any of its parents.
				for(INT ClassIdx = 0; ClassIdx < SupportInfo.Num(); ClassIdx++)
				{
					UClass* PotentialParent = SupportInfo(ClassIdx).Class;

					if(PotentialParent == ResourceClass)
					{
						bAddItem = FALSE;
						break;
					}

					if(ResourceClass->IsChildOf(PotentialParent) == TRUE)
					{
						InsertIdx = ClassIdx;
						break;
					}
				}

				if(bAddItem == TRUE)
				{
					if(InsertIdx == -1)
					{
						SupportInfo.AddItem( TypeInfo );
					}
					else
					{
						SupportInfo.InsertItem( TypeInfo, InsertIdx );
					}
				}

			}
		}
	}
}

/*------------------------------------------------------------------------------
UGenericBrowserType_Custom
------------------------------------------------------------------------------*/

/**
* Invokes the editor for all selected objects.
*/
UBOOL UGenericBrowserType_Custom::ShowObjectEditor()
{
	UBOOL bAbleToShowEditor = FALSE;

	// Loop through all of the selected objects and see if any of our support classes support this object.
	// If one of them does, show the editor specific to this browser type and continue on to the next object.
	for ( USelection::TObjectIterator It( GEditor->GetSelectedObjects()->ObjectItor() ) ; It ; ++It )
	{
		UObject* Object = *It;

		if ( Supports(Object) )
		{
			for( INT SupportIdx = 0 ; SupportIdx < SupportInfo.Num() ; ++SupportIdx )
			{
				FGenericBrowserTypeInfo& Info = SupportInfo(SupportIdx);

				if( Info.Supports(Object) )
				{
					if(Info.BrowserType == NULL)
					{
						for( TObjectIterator<UGenericBrowserType> It ; It ; ++It )
						{
							UGenericBrowserType* BrowserType = *It;

							if( !BrowserType->IsA(UGenericBrowserType_Custom::StaticClass()) && BrowserType->Supports(Object) )
							{
								bAbleToShowEditor = BrowserType->ShowObjectEditor(Object);
								break;
							}
						}
					}
					else
					{
						UGenericBrowserType* BrowserType = Info.BrowserType;
						if( !BrowserType->IsA(UGenericBrowserType_Custom::StaticClass()) && BrowserType->Supports(Object) )
						{
							bAbleToShowEditor = BrowserType->ShowObjectEditor(Object);
						}
					}

					break;
				}
			}
		}
	}

	return bAbleToShowEditor;
}

/**
* Invokes the editor for an object.  The default behaviour is to
* open a property window for the object.  Dervied classes can override
* this with eg an editor which is specialized for the object's class.
*
* This version loops through all of the supported classes for the custom type and 
* calls the appropriate implementation of the function.
*
* @param	InObject	The object to invoke the editor for.
*/
UBOOL UGenericBrowserType_Custom::ShowObjectEditor( UObject* InObject )
{
	UBOOL bAbleToShowEditor = FALSE;

	if ( Supports(InObject) == TRUE )
	{
		for( INT SupportIdx = 0 ; SupportIdx < SupportInfo.Num() ; ++SupportIdx )
		{
			FGenericBrowserTypeInfo& Info = SupportInfo(SupportIdx);

			if( Info.Supports(InObject) )
			{
				if(Info.BrowserType == NULL)
				{
					for( TObjectIterator<UGenericBrowserType> It ; It ; ++It )
					{
						UGenericBrowserType* BrowserType = *It;

						if( !BrowserType->IsA(UGenericBrowserType_Custom::StaticClass()) && BrowserType->Supports(InObject) )
						{
							bAbleToShowEditor = BrowserType->ShowObjectEditor(InObject);
							break;
						}
					}
				}
				else
				{
					UGenericBrowserType* BrowserType = Info.BrowserType;
					if( !BrowserType->IsA(UGenericBrowserType_Custom::StaticClass()) && BrowserType->Supports(InObject) )
					{
						bAbleToShowEditor = BrowserType->ShowObjectEditor(InObject);
					}
				}

				break;
			}
		}
	}

	return bAbleToShowEditor;
}

/**
* Opens a property window for the specified objects.  By default, GEditor's
* notify hook is used on the property window.  Derived classes can override
* this method in order to eg provide their own notify hook.
*
* @param	InObjects	The objects to invoke the property window for.
*/
UBOOL UGenericBrowserType_Custom::ShowObjectProperties( const TArray<UObject*>& InObjects )
{
	UBOOL bShowedProperties = FALSE;
	const INT NumObjects = InObjects.Num();

	for( TObjectIterator<UGenericBrowserType> It ; It ; ++It )
	{
		UGenericBrowserType* BrowserType = *It;
		TArray<UObject*> Objects;

		if( !Cast<UGenericBrowserType_Custom>(BrowserType) )
		{
			for(INT ObjectIdx = 0; ObjectIdx < NumObjects; ObjectIdx++)
			{
				UObject* Object = InObjects(ObjectIdx);

				if(Object != NULL && Supports(Object) && BrowserType->Supports(Object))
				{
					Objects.AddUniqueItem(Object);
				}
			}
		}

		if( Objects.Num() > 0 && BrowserType->ShowObjectProperties(Objects) )
		{
			bShowedProperties = TRUE;
		}
	}

	return bShowedProperties;
}

/**
* Invokes a custom menu item command for every selected object
* of a supported class.
*
* @param InCommand		The command to execute
*/
void UGenericBrowserType_Custom::InvokeCustomCommand( INT InCommand )
{
	// Loop through all of the selected objects and see if any of our support classes support this object.
	// If one of them does, call the invoke function for the object and exit out.
	for ( USelection::TObjectIterator ObjectIt( GEditor->GetSelectedObjects()->ObjectItor() ) ; ObjectIt ; ++ObjectIt )
	{
		UObject* Object = *ObjectIt;

		if ( Supports(Object) )
		{
			for( INT SupportIdx = 0 ; SupportIdx < SupportInfo.Num() ; ++SupportIdx )
			{
				FGenericBrowserTypeInfo& Info = SupportInfo(SupportIdx);

				if( Info.Supports(Object) )
				{
					if(Info.BrowserType == NULL)
					{
						for( TObjectIterator<UGenericBrowserType> It ; It ; ++It )
						{
							UGenericBrowserType* BrowserType = *It;

							if( !BrowserType->IsA(UGenericBrowserType_Custom::StaticClass()) && BrowserType->Supports(Object) )
							{
								TArray<UObject*> SoloArray;
								SoloArray.AddItem(Object);
								BrowserType->InvokeCustomCommand(InCommand, SoloArray);
								break;
							}
						}
					}
					else
					{
						UGenericBrowserType* BrowserType = Info.BrowserType;

						if( !BrowserType->IsA(UGenericBrowserType_Custom::StaticClass()) && BrowserType->Supports(Object) )
						{
							TArray<UObject*> SoloArray;
							SoloArray.AddItem(Object);
							BrowserType->InvokeCustomCommand(InCommand, SoloArray);
						}
					}

					break;
				}
			}
		}
	}
}

/**
* Calls the virtual "DoubleClick" function for each object
* of a supported class.
*/
void UGenericBrowserType_Custom::DoubleClick()
{
	// Loop through all of the selected objects and see if any of our support classes support this object.
	// If one of them does, call the double click function for the object and continue on.
	for ( USelection::TObjectIterator ObjectIt( GEditor->GetSelectedObjects()->ObjectItor() ) ; ObjectIt ; ++ObjectIt )
	{
		UObject* Object = *ObjectIt;

		if ( Supports(Object) == TRUE )
		{
			for( INT SupportIdx = 0 ; SupportIdx < SupportInfo.Num() ; ++SupportIdx )
			{
				FGenericBrowserTypeInfo& Info = SupportInfo(SupportIdx);

				if( Info.Supports(Object) )
				{
					if(Info.BrowserType == NULL)
					{
						for( TObjectIterator<UGenericBrowserType> It ; It ; ++It )
						{
							UGenericBrowserType* BrowserType = *It;

							if( !BrowserType->IsA(UGenericBrowserType_Custom::StaticClass()) && BrowserType->Supports(Object) )
							{
								BrowserType->DoubleClick(Object);
								break;
							}
						}
					}
					else
					{
						UGenericBrowserType* BrowserType = Info.BrowserType;
						if( !BrowserType->IsA(UGenericBrowserType_Custom::StaticClass()) && BrowserType->Supports(Object) )
						{
							BrowserType->DoubleClick(Object);
						}
					}

					break;
				}
			}
		}
	}
}

/*------------------------------------------------------------------------------
UGenericBrowserType_Animation
------------------------------------------------------------------------------*/

void UGenericBrowserType_Animation::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UAnimSet::StaticClass(), FColor(192,128,255) , 0, this) );
}

UBOOL UGenericBrowserType_Animation::ShowObjectEditor( UObject* InObject )
{
	UAnimSet* AnimSet = Cast<UAnimSet>(InObject);

	if( !AnimSet )
	{
		return 0;
	}

	WxAnimSetViewer* AnimSetViewer = new WxAnimSetViewer( (wxWindow*)GApp->EditorFrame, -1, NULL, AnimSet, NULL );
	AnimSetViewer->Show(1);

	return 0;
}



/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_Animation::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingAnimSetViewer" ) ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Properties, *LocalizeUnrealEd( "ObjectContext_EditProperties" ) ) );
}



/*------------------------------------------------------------------------------
UGenericBrowserType_AnimTree
------------------------------------------------------------------------------*/

void UGenericBrowserType_AnimTree::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UAnimTree::StaticClass(), FColor(255,128,192), 0, this ) );
}

UBOOL UGenericBrowserType_AnimTree::ShowObjectEditor( UObject* InObject )
{
	UAnimTree* AnimTree = Cast<UAnimTree>(InObject);
	if(AnimTree)
	{
		if(AnimTree->bBeingEdited)
		{
			// If being edited, find the window and restore it/put to front
			TArray<FTrackableEntry> TrackableWindows;
			WxTrackableWindow::GetTrackableWindows( TrackableWindows );
			for(INT WinIdx=0; WinIdx<TrackableWindows.Num(); WinIdx++)
			{
				wxWindow* Window = TrackableWindows(WinIdx).Window;
				WxAnimTreeEditor* AnimTreeEditor = wxDynamicCast(Window, WxAnimTreeEditor);
				if(AnimTreeEditor && AnimTreeEditor->AnimTree == AnimTree)
				{
					AnimTreeEditor->Raise();
					AnimTreeEditor->Maximize(false);
				}
			}
		}
		else
		{
			WxAnimTreeEditor* AnimTreeEditor = new WxAnimTreeEditor( (wxWindow*)GApp->EditorFrame, -1, AnimTree );
			AnimTreeEditor->Show(1);

			return 1;
		}
	}


	return 0;
}



/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_AnimTree::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingAnimTreeEditor" ) ) );
}


/*------------------------------------------------------------------------------
UGenericBrowserType_PostProcess
------------------------------------------------------------------------------*/

void UGenericBrowserType_PostProcess::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UPostProcessChain::StaticClass(), FColor(255,128,192), 0, this ) );
}

UBOOL UGenericBrowserType_PostProcess::ShowObjectEditor( UObject* InObject )
{
	UPostProcessChain* PostProcess = Cast<UPostProcessChain>(InObject);
	if(PostProcess)
	{
		WxPostProcessEditor* PostProcessEditor = new WxPostProcessEditor( (wxWindow*)GApp->EditorFrame, -1, PostProcess );
		PostProcessEditor->Show(1);

		return 1;
	}


	return 0;
}


/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_PostProcess::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingPostProcessEditor" ) ) );
}


/*------------------------------------------------------------------------------
UGenericBrowserType_LensFlare
------------------------------------------------------------------------------*/
//## BEGIN PROPS GenericBrowserType_LensFlare
//## END PROPS GenericBrowserType_LensFlare

void UGenericBrowserType_LensFlare::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( ULensFlare::StaticClass(), FColor(0,255,0), 0, this ) );
}

UBOOL UGenericBrowserType_LensFlare::ShowObjectEditor( UObject* InObject )
{
	ULensFlare* LensFlare = Cast<ULensFlare>(InObject);
	if (LensFlare == NULL)
	{
		return FALSE;
	}

	check(GApp->EditorFrame);
	const wxWindowList& ChildWindows = GApp->EditorFrame->GetChildren();
	// Find targets in splitter window and send the event to them
	wxWindowList::compatibility_iterator node = ChildWindows.GetFirst();
	while (node)
	{
		wxWindow* child = (wxWindow*)node->GetData();
		if (child->IsKindOf(CLASSINFO(wxFrame)))
		{
			if (appStricmp(child->GetName(), *(LensFlare->GetPathName())) == 0)
			{
				debugf(TEXT("LensFlareEditor already open for %s"), *(LensFlare->GetPathName()));
				child->Show(1);
				child->Raise();
				//@todo. If minimized, we should restore it...
				return FALSE;
			}
		}
		node = node->GetNext();
	}
	WxLensFlareEditor* LensFlareEditor = new WxLensFlareEditor(GApp->EditorFrame, -1, LensFlare);
	check(LensFlareEditor);
	LensFlareEditor->Show(1);

	return TRUE;
}


/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_LensFlare::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingLensFlareEditor" ) ) );

	OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Properties, *LocalizeUnrealEd( "ObjectContext_EditProperties" ) ) );
}


/*------------------------------------------------------------------------------
Helper functions for instancing a material editor.
------------------------------------------------------------------------------*/

/**
* Opens the specified material in the material editor.
*/
void OpenMaterialInMaterialEditor(UMaterial* MaterialToEdit)
{
	if (MaterialToEdit)
	{
		WxMaterialEditor* MaterialEditor = WxMaterialEditor::ActiveMaterialEditors.FindRef(MaterialToEdit->GetPathName());
		if (!MaterialEditor)
		{
			MaterialEditor = new WxMaterialEditor( (wxWindow*)GApp->EditorFrame,-1,MaterialToEdit );
			MaterialEditor->Show();
		}
		else
		{
			MaterialEditor->Show();
			MaterialEditor->Maximize(false);
			MaterialEditor->Raise();
		}
	}
}

void OpenMaterialFunctionInMaterialEditor(UMaterialFunction* MaterialFunctionToEdit)
{
	if (MaterialFunctionToEdit)
	{
		WxMaterialEditor* MaterialEditor = WxMaterialEditor::ActiveMaterialEditors.FindRef(MaterialFunctionToEdit->GetPathName());
		if (!MaterialEditor)
		{
			MaterialEditor = new WxMaterialEditor( (wxWindow*)GApp->EditorFrame,-1,MaterialFunctionToEdit );
			MaterialEditor->Show();
		}
		else
		{
			MaterialEditor->Show();
			MaterialEditor->Maximize(false);
			MaterialEditor->Raise();
		}
	}
}

/*------------------------------------------------------------------------------
	Helper functions for compiling materialinterfaces for other platforms.
------------------------------------------------------------------------------*/
/**
 *	Checks for other shader platform compilation being enabled, and if so, adds the
 *	options to the given command list.
 *
 * @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
 * @param	OutCommands		The list of custom commands to support
 */
void GenericBrowserTypeHelper_AddMaterialCompileCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands )
{
	UBOOL bAllowMaterialCompileForOtherPlatforms = FALSE;
	GConfig->GetBool(TEXT("UnrealEd.ContentBrowser"), TEXT("bAllowMaterialCompileForOtherPlatforms"), bAllowMaterialCompileForOtherPlatforms, GEditorIni);
	if (bAllowMaterialCompileForOtherPlatforms == TRUE)
	{
		FConsoleSupportContainer* ConsoleSupportContainer = FConsoleSupportContainer::GetConsoleSupportContainer();
		if (ConsoleSupportContainer != NULL)
		{
			// Make sure all supported consoles are loaded
			ConsoleSupportContainer->LoadAllConsoleSupportModules();

			// Add the submenu for 'Compile materials for:'
			const INT ParentIndex = OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CompileMaterialForPlatform, *LocalizeUnrealEd("ObjectContext_CompileForPlatform") ) );

			// Add each platform found
			for (INT ShaderPlatformIdx = 0; ShaderPlatformIdx < EShaderPlatform::SP_NumPlatforms; ShaderPlatformIdx++)
			{
				EShaderPlatform Platform = (EShaderPlatform)ShaderPlatformIdx;
				UE3::EPlatformType UE3Platform = UE3PlatformFromShaderPlatform(Platform);
				FString UE3PlatformName = appPlatformTypeToString(UE3Platform);
				// Support for DX9 & DX11 is 'built-in'
				INT ControlID = IDMN_ObjectContext_CompileMaterialForPlatform_START + ShaderPlatformIdx;
				if (ControlID <= IDMN_ObjectContext_CompileMaterialForPlatform_END)
				{
					UBOOL bPlatformIsInstalled = TRUE;
					if ((Platform != SP_PCD3D_SM3) && (Platform != SP_PCD3D_SM4) && (Platform != SP_PCD3D_SM5))
					{
						FConsoleSupport* ConsoleSupport = ConsoleSupportContainer->GetConsoleSupport(*UE3PlatformName);
						if (ConsoleSupport == NULL)
						{
							bPlatformIsInstalled = FALSE;
						}
					}

					if (bPlatformIsInstalled == TRUE)
					{
						const TCHAR* ShaderPlatformName = ShaderPlatformToText(Platform, TRUE);
						if (appStrstr(ShaderPlatformName, TEXT("Unknown")) == 0)
						{
							OutCommands.AddItem( FObjectSupportedCommandType(ControlID, ShaderPlatformName, TRUE, ParentIndex));
						}
					}
				}
				else
				{
					warnf(NAME_Error, TEXT("GenericBrowser: ShaderPlatform entry count is too small!"));
				}
			}
		}
	}
}

/**
 *	Internal helper function, compile the selected material instances for WiiU.
 *
 *	@param	InObjects	Array of UObjects, should contain material instances
 */
extern void DumpShaderCompileErrors(FMaterialResource* MaterialResource, FString* OutputString);

static void CompileMaterialsForPlatform(EShaderPlatform InShaderPlatform, TArray<UObject*> InObjects)
{
	const TCHAR* ShaderPlatformName = ShaderPlatformToText(InShaderPlatform, TRUE);
	if (appStrstr(ShaderPlatformName, TEXT("Unknown")) == 0)
	{
		warnf(NAME_Log, TEXT("Compiling materials for platform %s"), ShaderPlatformName);

		FString ShaderPlatformName = ShaderPlatformToText(InShaderPlatform, TRUE);
		UE3::EPlatformType Platform = UE3PlatformFromShaderPlatform(InShaderPlatform);
		FString PlatformName = appPlatformTypeToString(Platform);
		// Find the target platform PC-side support implementation.
		FConsoleSupport* ConsoleSupport = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport(*PlatformName);
		if (!ConsoleSupport)
		{
			appDebugMessagef(TEXT("Couldn't bind to support DLL for %s."), *PlatformName);
			return;
		}

		// Create the platform specific shader compiler.
		if (GConsoleShaderPrecompilers[InShaderPlatform] == NULL)
		{
			GConsoleShaderPrecompilers[InShaderPlatform] = ConsoleSupport->GetGlobalShaderPrecompiler();
		}

		// DX9 and DX11 don't need an entry in GConsoleShaderPrecompilers...
		if ((GConsoleShaderPrecompilers[InShaderPlatform] == NULL) && (PlatformName != TEXT("PC")))
		{
			appDebugMessagef(TEXT("Couldn't find GlobalShaderPrecompiler for %s."), *PlatformName);
			return;
		}

		// Reattach all components to ensure no issues with materials that were modified by this operation.
		FGlobalComponentReattachContext GlobalReattach;
		// Flush the rendering pipeline to ensure any materials we recompile aren't being used.
		FlushRenderingCommands();

		extern UBOOL GbForceMaterialNonPersistent;
		// Compile each selected material interface
		for (TArray<UObject*>::TConstIterator ObjIter(InObjects); ObjIter; ++ObjIter)
		{
			UMaterial* Material = Cast<UMaterial>(*ObjIter);
			UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(*ObjIter);

			if ((Material != NULL) && (!Material->HasAnyFlags(RF_ClassDefaultObject)))
			{
				GbForceMaterialNonPersistent = TRUE;

				FString OutputString;
				OutputString = FString::Printf(TEXT("%s compilation for %s"), *PlatformName, *(Material->GetPathName()));
				GWarn->BeginSlowTask(*OutputString, TRUE);
				OutputString += TEXT("\n");

				// compile the materials shaders for the target platform
				Material->CacheResourceShaders(InShaderPlatform, TRUE);
				FMaterialResource* MaterialResource = Material->GetMaterialResource();
				check(MaterialResource);
				TRefCountPtr<FMaterialShaderMap> MaterialShaderMapRef = MaterialResource->GetShaderMap();
				if (MaterialShaderMapRef)
				{
					OutputString += FString::Printf(TEXT("  Successfully compiled for %s.\n"), *ShaderPlatformName);
					TArray<FString> Descriptions;
					TArray<INT> InstructionCounts;
					MaterialResource->GetRepresentativeInstructionCounts(Descriptions, InstructionCounts);

					for (INT InstructionIndex = 0; InstructionIndex < Descriptions.Num(); InstructionIndex++)
					{
						OutputString += FString::Printf(TEXT("    %s: %u instructions\n"), *Descriptions(InstructionIndex), InstructionCounts(InstructionIndex));
					}

					MaterialResource->SetShaderMap(NULL);
				}
				else
				{
					OutputString += FString::Printf(TEXT("Failed to compile Material %s for platform %s, Default Material will be used in game.\n"), 
						*Material->GetPathName(), 
						ShaderPlatformToText(InShaderPlatform));
					DumpShaderCompileErrors(MaterialResource, &OutputString);
				}
				GbForceMaterialNonPersistent = FALSE;

				GWarn->EndSlowTask();

				warnf(NAME_Log, *OutputString);
				appMsgf(AMT_OK, *OutputString);

				// Restore it for the editor...
				Material->CacheResourceShaders(GRHIShaderPlatform, TRUE);
			}
			else if ((MaterialInstance != NULL) && (MaterialInstance->Parent != NULL) && (!MaterialInstance->HasAnyFlags(RF_ClassDefaultObject)))
			{
				GbForceMaterialNonPersistent = TRUE;

				FString OutputString;
				OutputString = FString::Printf(TEXT("%s compilation for %s"), *PlatformName, *(MaterialInstance->GetPathName()));
				GWarn->BeginSlowTask(*OutputString, TRUE);
				OutputString += TEXT("\n");

				//only process if the material instance has a static permutation 
				if (MaterialInstance->bHasStaticPermutationResource)
				{
					// compile the material instance's shaders for the target platform
					MaterialInstance->CacheResourceShaders(InShaderPlatform, TRUE);
					FMaterialResource* MaterialResource = MaterialInstance->GetMaterialResource();
					check(MaterialResource);
					TRefCountPtr<FMaterialShaderMap> MaterialShaderMapRef = MaterialResource->GetShaderMap();
					if (MaterialShaderMapRef)
					{
						OutputString += FString::Printf(TEXT("  Successfully compiled for %s.\n"), *ShaderPlatformName);
						TArray<FString> Descriptions;
						TArray<INT> InstructionCounts;
						MaterialResource->GetRepresentativeInstructionCounts(Descriptions, InstructionCounts);

						for (INT InstructionIndex = 0; InstructionIndex < Descriptions.Num(); InstructionIndex++)
						{
							OutputString += FString::Printf(TEXT("    %s: %u instructions\n"), *Descriptions(InstructionIndex), InstructionCounts(InstructionIndex));
						}

						MaterialResource->SetShaderMap(NULL);
					}
					else
					{
						const UMaterial* BaseMaterial = MaterialInstance->GetMaterial();
						OutputString += FString::Printf(TEXT("Failed to compile Material Instance %s with Base %s for platform %s, Default Material will be used in game.\n"), 
							*MaterialInstance->GetPathName(), 
							BaseMaterial ? *BaseMaterial->GetName() : TEXT("Null"), 
							ShaderPlatformToText(InShaderPlatform));
						DumpShaderCompileErrors(MaterialResource, &OutputString);
					}
				}
				GbForceMaterialNonPersistent = FALSE;

				GWarn->EndSlowTask();

				warnf(NAME_Log, *OutputString);
				appMsgf(AMT_OK, *OutputString);

				// Restore it for the editor...
				MaterialInstance->CacheResourceShaders(GRHIShaderPlatform, TRUE);
			}
		}
	}
}

/*------------------------------------------------------------------------------
UGenericBrowserType_Material
------------------------------------------------------------------------------*/

void UGenericBrowserType_Material::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( 
		UMaterial::StaticClass(), 
		FColor(0,255,0), 
		0, 
		this,
		UGenericBrowserType_Material::ShouldDisplayCallback )
		);
}

/**
* Callback to register whether or not the object should be displayed
* @param InObject - object that will be displayed in the GB
* @return TRUE if should be displayed
*/ 
UBOOL UGenericBrowserType_Material::ShouldDisplayCallback( UObject* InObject )
{
	return TRUE;
}

UBOOL UGenericBrowserType_Material::ShowObjectEditor( UObject* InObject )
{
	OpenMaterialInMaterialEditor( CastChecked<UMaterial>(InObject) );
	return TRUE;
}


/*------------------------------------------------------------------------------
UGenericBrowserType_MaterialFunction
------------------------------------------------------------------------------*/

void UGenericBrowserType_MaterialFunction::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( 
		UMaterialFunction::StaticClass(), 
		FColor(0,175,175), 
		0, 
		this,
		UGenericBrowserType_MaterialFunction::ShouldDisplayCallback )
		);
}

/**
* Callback to register whether or not the object should be displayed
* @param InObject - object that will be displayed in the GB
* @return TRUE if should be displayed
*/ 
UBOOL UGenericBrowserType_MaterialFunction::ShouldDisplayCallback( UObject* InObject )
{
	return TRUE;
}

UBOOL UGenericBrowserType_MaterialFunction::ShowObjectEditor( UObject* InObject )
{
	OpenMaterialFunctionInMaterialEditor( CastChecked<UMaterialFunction>(InObject) );
	return TRUE;
}

static void CreateNewMaterialInstanceConstant(UObject* InObject)
{
	// Create a new material containing a texture material expression referring to the texture.
	const FString Package	= InObject->GetOutermost()->GetName();
	const FString Group		= InObject->GetOuter()->GetOuter() ? InObject->GetFullGroupName( 1 ) : TEXT("");
	const FString Name		= FString::Printf( TEXT("%s_INST"), *InObject->GetName() );

	WxDlgPackageGroupName dlg;
	dlg.SetTitle( *LocalizeUnrealEd("CreateNewMaterialInstanceConstant") );

	if( dlg.ShowModal( Package, Group, Name ) == wxID_OK )
	{
		FString Pkg;
		// Was a group specified?
		if( dlg.GetGroup().Len() > 0 )
		{
			Pkg = FString::Printf(TEXT("%s.%s"),*dlg.GetPackage(),*dlg.GetGroup());
		}
		else
		{
			Pkg = FString::Printf(TEXT("%s"),*dlg.GetPackage());
		}
		UObject* ExistingPackage = UObject::FindPackage(NULL, *Pkg);
		FString Reason;

		// Verify the package an object name.
		if(!dlg.GetPackage().Len() || !dlg.GetObjectName().Len())
		{
			appMsgf(AMT_OK,*LocalizeUnrealEd("Error_InvalidInput"));
		}
		// 		// Verify the object name.
		// 		else if( !FIsValidObjectName( *dlg.GetObjectName(), Reason ))
		// 		{
		// 			appMsgf( AMT_OK, *Reason );
		// 		}
		// Verify the object name is unique withing the package.
		else if (ExistingPackage && !FIsUniqueObjectName(*dlg.GetObjectName(), ExistingPackage, Reason))
		{
			appMsgf(AMT_OK, *Reason);
		}
		else
		{
			UMaterialInterface* InMaterialParent = Cast<UMaterialInterface>(InObject);
			check(InMaterialParent);

			UMaterialInstanceConstant* MaterialInterface = ConstructObject<UMaterialInstanceConstant>( UMaterialInstanceConstant::StaticClass(), UObject::CreatePackage(NULL,*Pkg), *dlg.GetObjectName(), RF_Public|RF_Transactional|RF_Standalone );
			check(MaterialInterface);

			MaterialInterface->Modify();
			MaterialInterface->SetParent(InMaterialParent);

			// Tell the Content Browser that we created a new asset
			GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, CBR_ObjectCreated, MaterialInterface));

			// Refresh
			const DWORD UpdateMask = CBR_UpdatePackageList|CBR_UpdateAssetList;
			GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, UpdateMask, MaterialInterface));

			// Show the editor with the new material instance.
			wxFrame* MaterialInstanceEditor = new WxMaterialInstanceConstantEditor( (wxWindow*)GApp->EditorFrame,-1, MaterialInterface );
			MaterialInstanceEditor->Show();
		}
	}
}


static void CreateNewMaterialInstanceTimeVarying(UObject* InObject)
{
	// Create a new material containing a texture material expression referring to the texture.
	const FString Package	= InObject->GetOutermost()->GetName();
	const FString Group		= InObject->GetOuter()->GetOuter() ? InObject->GetFullGroupName( 1 ) : TEXT("");
	const FString Name		= FString::Printf( TEXT("%s_INST"), *InObject->GetName() );

	WxDlgPackageGroupName dlg;
	dlg.SetTitle( *LocalizeUnrealEd("CreateNewMaterialInstanceTimeVarying") );

	if( dlg.ShowModal( Package, Group, Name ) == wxID_OK )
	{
		FString Pkg;
		// Was a group specified?
		if( dlg.GetGroup().Len() > 0 )
		{
			Pkg = FString::Printf(TEXT("%s.%s"),*dlg.GetPackage(),*dlg.GetGroup());
		}
		else
		{
			Pkg = FString::Printf(TEXT("%s"),*dlg.GetPackage());
		}
		UObject* ExistingPackage = UObject::FindPackage(NULL, *Pkg);
		FString Reason;

		// Verify the package an object name.
		if(!dlg.GetPackage().Len() || !dlg.GetObjectName().Len())
		{
			appMsgf(AMT_OK,*LocalizeUnrealEd("Error_InvalidInput"));
		}
		// 		// Verify the object name.
		// 		else if( !FIsValidObjectName( *dlg.GetObjectName(), Reason ))
		// 		{
		// 			appMsgf( AMT_OK, *Reason );
		// 		}
		// Verify the object name is unique withing the package.
		else if (ExistingPackage && !FIsUniqueObjectName(*dlg.GetObjectName(), ExistingPackage, Reason))
		{
			appMsgf(AMT_OK, *Reason);
		}
		else
		{
			UMaterialInterface* InMaterialParent = Cast<UMaterialInterface>(InObject);
			check(InMaterialParent);

			UMaterialInstanceTimeVarying* MaterialInterface = ConstructObject<UMaterialInstanceTimeVarying>( UMaterialInstanceTimeVarying::StaticClass(), UObject::CreatePackage(NULL,*Pkg), *dlg.GetObjectName(), RF_Public|RF_Transactional|RF_Standalone );
			check(MaterialInterface);

			MaterialInterface->Modify();
			MaterialInterface->SetParent(InMaterialParent);

			// Tell the Content Browser that we created a new asset
			GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, CBR_ObjectCreated, MaterialInterface));

			// Refresh
			const DWORD UpdateMask = CBR_UpdatePackageList|CBR_UpdateAssetList;
			GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, UpdateMask, MaterialInterface));

			// Show the editor with the new material instance.
			wxFrame* MaterialInstanceEditor = new WxMaterialInstanceTimeVaryingEditor( (wxWindow*)GApp->EditorFrame,-1, MaterialInterface );
			MaterialInstanceEditor->Show();
		}
	}
}



/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_Material::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingMaterialEditor" ) ) );
    OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_RecompileMaterial, *LocalizeUnrealEd( "ObjectContext_RecompileMaterial" ) ) );

	OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CreateNewMaterialInstanceConstant, *LocalizeUnrealEd( "ObjectContext_CreateNewMaterialInstanceConstant" ), !bAnythingCooked ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CreateNewMaterialInstanceTimeVarying, *LocalizeUnrealEd( "ObjectContext_CreateNewMaterialInstanceTimeVarying" ), !bAnythingCooked ) );

	GenericBrowserTypeHelper_AddMaterialCompileCommands(InObjects, OutCommands);
}



void UGenericBrowserType_Material::InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects )
{
	for (INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex)
	{
		UObject* Object = InObjects(ObjIndex);
		if ( InCommand == IDMN_ObjectContext_CreateNewMaterialInstanceConstant )
		{
			CreateNewMaterialInstanceConstant( Object );
		}
		else if ( InCommand == IDMN_ObjectContext_CreateNewMaterialInstanceTimeVarying )
		{
			CreateNewMaterialInstanceTimeVarying( Object );
		}
		else if ( InCommand == IDMN_ObjectContext_RecompileMaterial )
		{
			Object->PreEditChange(NULL);
			Object->PostEditChange();
		}
		else if ((InCommand >= IDMN_ObjectContext_CompileMaterialForPlatform_START) &&
			(InCommand <= IDMN_ObjectContext_CompileMaterialForPlatform_END))
		{
			EShaderPlatform ShaderPlatform = (EShaderPlatform)(InCommand - IDMN_ObjectContext_CompileMaterialForPlatform_START);
			CompileMaterialsForPlatform(ShaderPlatform, InObjects);
			break;
		}
	}
}


/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_MaterialFunction::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingMaterialEditor" ) ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_FindMaterialsUsingFunction, *LocalizeUnrealEd( "ObjectContext_FindMaterialsUsingFunction" ) ) );
}

void UGenericBrowserType_MaterialFunction::InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects )
{
	TArray<UObject*> ObjectsToSync;

	for (INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex)
	{
		UObject* Object = InObjects(ObjIndex);
		UMaterialFunction* Function = Cast<UMaterialFunction>(Object);

		if (Function && InCommand == IDMN_ObjectContext_FindMaterialsUsingFunction)
		{
			for (TObjectIterator<UMaterial> It; It; ++It)
			{
				UMaterial* CurrentMaterial = *It;
				
				for (INT FunctionIndex = 0; FunctionIndex < CurrentMaterial->MaterialFunctionInfos.Num(); FunctionIndex++)
				{
					if (CurrentMaterial->MaterialFunctionInfos(FunctionIndex).Function == Function)
					{
						ObjectsToSync.AddItem(CurrentMaterial);
						break;
					}
				}
			}
		}
	}

	if (ObjectsToSync.Num() > 0)
	{
		GApp->EditorFrame->SyncBrowserToObjects(ObjectsToSync);
	}
}


/*------------------------------------------------------------------------------
UGenericBrowserType_DecalMaterial
------------------------------------------------------------------------------*/

void UGenericBrowserType_DecalMaterial::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UDecalMaterial::StaticClass(), FColor(192,255,192), 0, this ) );
}

UBOOL UGenericBrowserType_DecalMaterial::ShowObjectEditor( UObject* InObject )
{
	OpenMaterialInMaterialEditor( CastChecked<UDecalMaterial>(InObject) );
	return TRUE;
}

/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_DecalMaterial::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingMaterialEditor" ) ) );

	OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CreateNewMaterialInstanceConstant, *LocalizeUnrealEd( "ObjectContext_CreateNewMaterialInstanceConstant" ), !bAnythingCooked ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CreateNewMaterialInstanceTimeVarying, *LocalizeUnrealEd( "ObjectContext_CreateNewMaterialInstanceTimeVarying" ), !bAnythingCooked ) );

	GenericBrowserTypeHelper_AddMaterialCompileCommands(InObjects, OutCommands);
}


void UGenericBrowserType_DecalMaterial::InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects )
{
	for (INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex)
	{
		UObject* Object = InObjects(ObjIndex);
		if ( InCommand == IDMN_ObjectContext_CreateNewMaterialInstanceConstant)
		{
			CreateNewMaterialInstanceConstant( Object );
		}
		else if ( InCommand == IDMN_ObjectContext_CreateNewMaterialInstanceTimeVarying )
		{
			CreateNewMaterialInstanceTimeVarying( Object );
		}
		else if ((InCommand >= IDMN_ObjectContext_CompileMaterialForPlatform_START) &&
			(InCommand <= IDMN_ObjectContext_CompileMaterialForPlatform_END))
		{
			EShaderPlatform ShaderPlatform = (EShaderPlatform)(InCommand - IDMN_ObjectContext_CompileMaterialForPlatform_START);
			CompileMaterialsForPlatform(ShaderPlatform, InObjects);
		}
	}
}

/*------------------------------------------------------------------------------
UGenericBrowserType_MaterialInstance
------------------------------------------------------------------------------*/

/**
 * Internal helper function, syncs the generic/content browser to the parents
 * of the provided material instances
 *
 * @param	InObjects	Array of UObjects, should contain material instances
 */
static void SyncToParentsOfMaterialInstances( TArray<UObject*> InObjects )
{
	TArray<UObject*> ObjectsToSyncTo;

	// Find all valid parents of any provided material instances
	for ( TArray<UObject*>::TConstIterator ObjIter( InObjects ); ObjIter; ++ObjIter )
	{
		UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>( *ObjIter );
		if ( MaterialInstance && MaterialInstance->Parent )
		{
			ObjectsToSyncTo.AddUniqueItem( MaterialInstance->Parent );
		}
	}

	// Sync the respective browser to the valid parents
	if ( ObjectsToSyncTo.Num() > 0 )
	{
		GApp->EditorFrame->SyncBrowserToObjects( ObjectsToSyncTo );
	}
}

/**
 *	Internal helper function, copies the parameter settings of the provided
 *	material instances to the clipboard.
 *
 *	@param	InObjects	Array of UObjects, should contain material instances
 */
static void CopyMaterialInstanceParametersToClipboard(TArray<UObject*> InObjects)
{
	FString ClipboardCopyContents(TEXT(""));
	// Find all valid parents of any provided material instances
	for (TArray<UObject*>::TConstIterator ObjIter(InObjects); ObjIter; ++ObjIter)
	{
		UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(*ObjIter);
		if ((MaterialInstance != NULL) && (MaterialInstance->Parent != NULL))
		{
			ClipboardCopyContents += FString::Printf(TEXT("%s\n"), *(MaterialInstance->GetPathName()));

			// Gather the static parameter settings...
			FStaticParameterSet TempStaticParameterSet;

			FMaterialResource* MtrlRes = MaterialInstance->GetMaterialResource();
			if ((MtrlRes != NULL) && (MtrlRes->GetShaderMap() != NULL))
			{
				TempStaticParameterSet = MtrlRes->GetShaderMap()->GetMaterialId();
				if (TempStaticParameterSet.StaticSwitchParameters.Num() > 0)
				{
					ClipboardCopyContents += TEXT("\tStaticSwitchParameters\n");
					for (INT StaticSwitchIdx = 0; StaticSwitchIdx < TempStaticParameterSet.StaticSwitchParameters.Num(); StaticSwitchIdx++)
					{
						FStaticSwitchParameter& Param = TempStaticParameterSet.StaticSwitchParameters(StaticSwitchIdx);
						ClipboardCopyContents += FString::Printf(TEXT("\t\t%s %s %s\n"),
							Param.bOverride ? TEXT(" OVERRIDE") : TEXT("!OVERRIDE"),
							Param.Value ? TEXT(" TRUE") : TEXT("FALSE"),
							*(Param.ParameterName.ToString()));
					}
				}

				if (TempStaticParameterSet.StaticComponentMaskParameters.Num() > 0)
				{
					ClipboardCopyContents += TEXT("\tStaticComponentMaskParameters\n");
					for (INT StaticMaskIdx = 0; StaticMaskIdx < TempStaticParameterSet.StaticComponentMaskParameters.Num(); StaticMaskIdx++)
					{
						FStaticComponentMaskParameter& Param = TempStaticParameterSet.StaticComponentMaskParameters(StaticMaskIdx);
						ClipboardCopyContents += FString::Printf(TEXT("\t\t%s %s%s%s%s %s\n"),
							Param.bOverride ? TEXT(" OVERRIDE") : TEXT("!OVERRIDE"),
							Param.R ? TEXT("R") : TEXT(" "), 
							Param.G ? TEXT("G") : TEXT(" "), 
							Param.B ? TEXT("B") : TEXT(" "), 
							Param.A ? TEXT("A") : TEXT(" "), 
							*(Param.ParameterName.ToString()));
					}
				}

				if (TempStaticParameterSet.NormalParameters.Num() > 0)
				{
					ClipboardCopyContents += TEXT("\tNormalParameters\n");
					for (INT NormalIdx = 0; NormalIdx < TempStaticParameterSet.NormalParameters.Num(); NormalIdx++)
					{
						FNormalParameter& Param = TempStaticParameterSet.NormalParameters(NormalIdx);
						ClipboardCopyContents += FString::Printf(TEXT("\t\t%s %2d %s\n"),
							Param.bOverride ? TEXT(" OVERRIDE") : TEXT("!OVERRIDE"),
							Param.CompressionSettings,
							*(Param.ParameterName.ToString()));
					}
				}

				if (TempStaticParameterSet.TerrainLayerWeightParameters.Num() > 0)
				{
					ClipboardCopyContents += TEXT("\tTerrainLayerWeightParameters\n");
					for (INT TLWIdx = 0; TLWIdx < TempStaticParameterSet.TerrainLayerWeightParameters.Num(); TLWIdx++)
					{
						FStaticTerrainLayerWeightParameter& Param = TempStaticParameterSet.TerrainLayerWeightParameters(TLWIdx);
						ClipboardCopyContents += FString::Printf(TEXT("\t\t%s %2d %s\n"),
							Param.bOverride ? TEXT(" OVERRIDE") : TEXT("!OVERRIDE"),
							Param.WeightmapIndex,
							*(Param.ParameterName.ToString()));
					}
				}
			}
			
			// Gather the other parameter settings
			UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(MaterialInstance);
			if (MIC != NULL)
			{
				if (MIC->ScalarParameterValues.Num() > 0)
				{
					ClipboardCopyContents += TEXT("\tScalarParameterValues\n");
					for (INT ParamIdx = 0; ParamIdx < MIC->ScalarParameterValues.Num(); ParamIdx++)
					{
						FScalarParameterValue& Param = MIC->ScalarParameterValues(ParamIdx);
						ClipboardCopyContents += FString::Printf(TEXT("\t\t%12.5f %s\n"),
							Param.ParameterValue,
							*(Param.ParameterName.ToString()));
					}
				}
				if (MIC->VectorParameterValues.Num() > 0)
				{
					ClipboardCopyContents += TEXT("\tVectorParameterValues\n");
					for (INT ParamIdx = 0; ParamIdx < MIC->VectorParameterValues.Num(); ParamIdx++)
					{
						FVectorParameterValue& Param = MIC->VectorParameterValues(ParamIdx);
						ClipboardCopyContents += FString::Printf(TEXT("\t\t%12.5f,%12.5f,%12.5f,%12.5f %s\n"),
							Param.ParameterValue.R,
							Param.ParameterValue.G,
							Param.ParameterValue.B,
							Param.ParameterValue.A,
							*(Param.ParameterName.ToString()));
					}
				}
				if (MIC->TextureParameterValues.Num() > 0)
				{
					ClipboardCopyContents += TEXT("\tTextureParameterValues\n");
					for (INT ParamIdx = 0; ParamIdx < MIC->TextureParameterValues.Num(); ParamIdx++)
					{
						FTextureParameterValue& Param = MIC->TextureParameterValues(ParamIdx);
						ClipboardCopyContents += FString::Printf(TEXT("\t\t%32s %s\n"),
							*(Param.ParameterName.ToString()),
							Param.ParameterValue ? *(Param.ParameterValue->GetPathName()) : TEXT("*** NULL TEXTURE ***")
							);
					}
				}
				if (MIC->FontParameterValues.Num() > 0)
				{
					ClipboardCopyContents += TEXT("\tFontParameterValues\n");
					for (INT ParamIdx = 0; ParamIdx < MIC->FontParameterValues.Num(); ParamIdx++)
					{
						FFontParameterValue& Param = MIC->FontParameterValues(ParamIdx);
						ClipboardCopyContents += FString::Printf(TEXT("\t\t%32s %2d %s\n"),
							Param.FontPage,
							*(Param.ParameterName.ToString()),
							Param.FontValue ? *(Param.FontValue->GetPathName()) : TEXT("*** NULL FONT ***")
							);
					}
				}
			}
		}
	}

	if (ClipboardCopyContents.Len() > 0)
	{
		appClipboardCopy(*ClipboardCopyContents);

		appMsgf(AMT_OK, *ClipboardCopyContents);
	}
}

void UGenericBrowserType_MaterialInstanceConstant::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UMaterialInstanceConstant::StaticClass(), FColor(0,128,0), 0, this ) );
}

UBOOL UGenericBrowserType_MaterialInstanceConstant::ShowObjectEditor( UObject* InObject )
{
	// Show the material instance editor.
	UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(InObject);

	if(MaterialInterface)
	{
		wxFrame* MaterialInstanceEditor = new WxMaterialInstanceConstantEditor( (wxWindow*)GApp->EditorFrame,-1, MaterialInterface );
		MaterialInstanceEditor->Show();
	}

	return TRUE;
}

/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_MaterialInstanceConstant::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingMaterialInstanceEditor" ) ) );

	OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CreateNewMaterialInstanceConstant, *LocalizeUnrealEd( "ObjectContext_CreateNewMaterialInstanceConstant" ), !bAnythingCooked ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CreateNewMaterialInstanceTimeVarying, *LocalizeUnrealEd( "ObjectContext_CreateNewMaterialInstanceTimeVarying" ), !bAnythingCooked ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_FindParentMaterial, *LocalizeUnrealEd("ObjectContext_FindParentMaterial") ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CopyParametersToClipboard, *LocalizeUnrealEd("ObjectContext_CopyParametersToClipboard") ) );

	GenericBrowserTypeHelper_AddMaterialCompileCommands(InObjects, OutCommands);
}

void UGenericBrowserType_MaterialInstanceConstant::InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects )
{
	for (INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex)
	{
		UObject* Object = InObjects(ObjIndex);
		if( InCommand == IDMN_ObjectContext_CreateNewMaterialInstanceConstant )
		{
			CreateNewMaterialInstanceConstant( Object );
		}
		else if ( InCommand == IDMN_ObjectContext_CreateNewMaterialInstanceTimeVarying )
		{
			CreateNewMaterialInstanceTimeVarying( Object );
		}

		// If the user has selected to find the parent material of the selected instances, attempt to sync to the parents
		// in the browser
		else if ( InCommand == IDMN_ObjectContext_FindParentMaterial )
		{
			SyncToParentsOfMaterialInstances( InObjects );
			break;
		}
		else if (InCommand == IDMN_ObjectContext_CopyParametersToClipboard)
		{
			CopyMaterialInstanceParametersToClipboard(InObjects);
			break;
		}
		else if ((InCommand >= IDMN_ObjectContext_CompileMaterialForPlatform_START) &&
				 (InCommand <= IDMN_ObjectContext_CompileMaterialForPlatform_END))
		{
			EShaderPlatform ShaderPlatform = (EShaderPlatform)(InCommand - IDMN_ObjectContext_CompileMaterialForPlatform_START);
			CompileMaterialsForPlatform(ShaderPlatform, InObjects);
		}
		// If none of the commands were matched, there is no point in searching through the rest of the objects
		else
		{
			break;
		}
	}
}


/*------------------------------------------------------------------------------
UGenericBrowserType_MaterialInstance
------------------------------------------------------------------------------*/

void UGenericBrowserType_MaterialInstanceTimeVarying::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UMaterialInstanceTimeVarying::StaticClass(), FColor(0,128,0), 0, this ) );
}

UBOOL UGenericBrowserType_MaterialInstanceTimeVarying::ShowObjectEditor( UObject* InObject )
{
	// Show the material instance editor.
	UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(InObject);

	if(MaterialInterface)
	{
		wxFrame* MaterialInstanceEditor = new WxMaterialInstanceTimeVaryingEditor( (wxWindow*)GApp->EditorFrame,-1, MaterialInterface );
		MaterialInstanceEditor->Show();
	}

	return TRUE;
}


/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_MaterialInstanceTimeVarying::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingMaterialInstanceEditor" ) ) );

	OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CreateNewMaterialInstanceTimeVarying, *LocalizeUnrealEd( "ObjectContext_CreateNewMaterialInstanceTimeVarying" ), !bAnythingCooked ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_FindParentMaterial, *LocalizeUnrealEd("ObjectContext_FindParentMaterial") ) );
}


void UGenericBrowserType_MaterialInstanceTimeVarying::InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects )
{
	for (INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex)
	{
		UObject* Object = InObjects(ObjIndex);
		if( InCommand == IDMN_ObjectContext_CreateNewMaterialInstanceTimeVarying )
		{
			CreateNewMaterialInstanceTimeVarying( Object );
		}

		// If the user has selected to find the parent material of the selected instances, attempt to sync to the parents
		// in the browser
		else if ( InCommand == IDMN_ObjectContext_FindParentMaterial )
		{
			SyncToParentsOfMaterialInstances( InObjects );
			break;
		}
		else if ((InCommand >= IDMN_ObjectContext_CompileMaterialForPlatform_START) &&
			(InCommand <= IDMN_ObjectContext_CompileMaterialForPlatform_END))
		{
			EShaderPlatform ShaderPlatform = (EShaderPlatform)(InCommand - IDMN_ObjectContext_CompileMaterialForPlatform_START);
			CompileMaterialsForPlatform(ShaderPlatform, InObjects);
		}
		// If none of the commands were matched, there is no point in searching through the rest of the objects
		else
		{
			break;
		}
	}
}


/*------------------------------------------------------------------------------
UGenericBrowserType_ParticleSystem
------------------------------------------------------------------------------*/

void UGenericBrowserType_ParticleSystem::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UParticleSystem::StaticClass(), FColor(255,255,255), 0, this ) );
}

UBOOL UGenericBrowserType_ParticleSystem::ShowObjectEditor( UObject* InObject )
{
	UParticleSystem* PSys = Cast<UParticleSystem>(InObject);
	if (PSys == NULL)
	{
		return FALSE;
	}

	check(GApp->EditorFrame);
	const wxWindowList& ChildWindows = GApp->EditorFrame->GetChildren();
	// Find targets in splitter window and send the event to them
	wxWindowList::compatibility_iterator node = ChildWindows.GetFirst();
	while (node)
	{
		wxWindow* child = (wxWindow*)node->GetData();
		if (child->IsKindOf(CLASSINFO(wxFrame)))
		{
			if (appStricmp(child->GetName(), *(PSys->GetPathName())) == 0)
			{
				// Bring the existing dialog to the front
				WxCascade* CascadeHandle = (WxCascade*)child;
				if( CascadeHandle )
				{
					CascadeHandle->Show();
					CascadeHandle->Restore();
					CascadeHandle->SetFocus();
				}
				else
				{
					child->Show(1);
					child->Raise();
				}

				return FALSE;
			}
		}
		node = node->GetNext();
	}
	WxCascade* ParticleEditor = new WxCascade(GApp->EditorFrame, -1, PSys);
	ParticleEditor->Show(1);

	return TRUE;
}



/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_ParticleSystem::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingCascade" ) ) );

	OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Properties, *LocalizeUnrealEd( "ObjectContext_EditProperties" ) ) );

	OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CopyParticleParameters, *LocalizeUnrealEd( "ObjectContext_CopyParticleParameters" ) ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_ConvertToSeeded, *LocalizeUnrealEd( "ObjectContext_ConvertToSeeded" ) ) );

}



void UGenericBrowserType_ParticleSystem::InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects )
{
	for (INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex)
	{
		UObject* Object = InObjects(ObjIndex);
		UParticleSystem* PSys = Cast<UParticleSystem>(Object);
		if (PSys)
		{
			if (InCommand == IDMN_ObjectContext_CopyParticleParameters)
			{
				debugf(TEXT("Copy particle parameters - %s"), *(PSys->GetPathName()));
				TArray<TArray<FString>> ParticleSysParamList;
				TArray<TArray<FString>> ParticleParameterList;
				PSys->GetParametersUtilized(ParticleSysParamList, ParticleParameterList);

				FString ClipboardString;
				ClipboardString = FString::Printf(TEXT("ParticleSystem parameters for %s\n"), *(PSys->GetPathName()));
				for (INT EmitterIndex = 0; EmitterIndex < PSys->Emitters.Num(); EmitterIndex++)
				{
					ClipboardString += FString::Printf(TEXT("\tEmitter %2d - "), EmitterIndex);
					UParticleEmitter* Emitter = PSys->Emitters(EmitterIndex);
					if (Emitter)
					{
						ClipboardString += FString::Printf(TEXT("%s\n"), *(Emitter->GetEmitterName().ToString()));
					}
					else
					{
						ClipboardString += FString(TEXT("* EMPTY *\n"));
					}

					TArray<FString>& ParticleSysParams = ParticleSysParamList(EmitterIndex);
					for (INT PSPIndex = 0; PSPIndex < ParticleSysParams.Num(); PSPIndex++)
					{
						if (PSPIndex == 0)
						{
							ClipboardString += FString(TEXT("\t\tParticleSysParam List\n"));
						}
						ClipboardString += FString::Printf(TEXT("\t\t\t%s"), *(ParticleSysParams(PSPIndex)));
					}

					TArray<FString>& ParticleParameters = ParticleParameterList(EmitterIndex);
					for (INT PPIndex = 0; PPIndex < ParticleParameters.Num(); PPIndex++)
					{
						if (PPIndex == 0)
						{
							ClipboardString += FString(TEXT("\t\tParticleParameter List\n"));
						}
						ClipboardString += FString::Printf(TEXT("\t\t\t%s"), *(ParticleParameters(PPIndex)));
					}
				}

				appClipboardCopy(*ClipboardString);
			}
			else if (InCommand == IDMN_ObjectContext_ConvertToSeeded)
			{
				PSys->ConvertAllModulesToSeeded();
				// Need to see if Cascade is open w/ this particle system
				const wxWindowList& ChildWindows = GApp->EditorFrame->GetChildren();
				// Find targets in splitter window and send the event to them
				wxWindowList::compatibility_iterator node = ChildWindows.GetFirst();
				while (node)
				{
					wxWindow* child = (wxWindow*)node->GetData();
					if (child->IsKindOf(CLASSINFO(wxFrame)))
					{
						if (appStricmp(child->GetName(), *(PSys->GetPathName())) == 0)
						{
							child->Refresh();
							break;
						}
					}
					node = node->GetNext();
				}
			}
		}
	}
}

/*------------------------------------------------------------------------------
UGenericBrowserType_PhysXParticleSystem
------------------------------------------------------------------------------*/
void UGenericBrowserType_PhysXParticleSystem::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UPhysXParticleSystem::StaticClass(), FColor(200,192,128), 0, this ) );
}

UBOOL UGenericBrowserType_PhysXParticleSystem::ShowObjectEditor( UObject* InObject )
{
	WxPropertyWindowFrame* Properties = new WxPropertyWindowFrame;
	Properties->Create( GApp->EditorFrame, -1 );

	Properties->AllowClose();
	Properties->SetObject( InObject, EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories  );
	Properties->SetTitle( *FString::Printf( TEXT("PhysXParticleSystem") ) );
	Properties->Show();

	return 1;
}

/*------------------------------------------------------------------------------
	UGenericBrowserType_ApexDestructibleDamageParameters
------------------------------------------------------------------------------*/
void UGenericBrowserType_ApexDestructibleDamageParameters::Init()
{
#if WITH_APEX
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UApexDestructibleDamageParameters::StaticClass(), FColor(200,192,128), 0, this ) );
#endif
}

UBOOL UGenericBrowserType_ApexDestructibleDamageParameters::ShowObjectEditor( UObject* InObject )
{
	UBOOL ret = FALSE;
#if WITH_APEX
	WxPropertyWindowFrame* Properties = new WxPropertyWindowFrame;
	Properties->Create( GApp->EditorFrame, -1, 0 );

	Properties->AllowClose();
	Properties->SetObject( InObject, EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories );
	Properties->SetTitle( *FString::Printf( TEXT("ApexDestructibleDamageParameters") ) );
	Properties->Show();

	ret = TRUE;
#endif
	return ret;
}


/*------------------------------------------------------------------------------
UGenericBrowserType_PhysicsAsset
------------------------------------------------------------------------------*/

void UGenericBrowserType_PhysicsAsset::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UPhysicsAsset::StaticClass(), FColor(255,192,128), 0, this ) );
}

UBOOL UGenericBrowserType_PhysicsAsset::ShowObjectEditor( UObject* InObject )
{
	UPhysicsAsset* PhysAsset = CastChecked<UPhysicsAsset>(InObject);

	WxPhAT* AssetEditor = new WxPhAT(GApp->EditorFrame, -1, PhysAsset);
	AssetEditor->Show(1);		

	return 1;
}



/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_PhysicsAsset::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingPhAT" ) ) );
}



/*------------------------------------------------------------------------------
UGenericBrowserType_Sequence
------------------------------------------------------------------------------*/

void UGenericBrowserType_Sequence::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( USequence::StaticClass(), FColor(255,255,255), 0, this, UGenericBrowserType_Sequence::IsSequenceTypeSupported ) );
}

/**
* Determines whether the specified object is a USequence class that should be handled by this generic browser type.
*
* @param	Object	a pointer to a USequence object.
*
* @return	TRUE if this generic browser type supports to object specified.
*/
UBOOL UGenericBrowserType_Sequence::IsSequenceTypeSupported( UObject* Object )
{
	USequence* SequenceObj = Cast<USequence>(Object);

	if ( SequenceObj != NULL )
	{
		return TRUE;
	}

	return FALSE;
}

/*------------------------------------------------------------------------------
UGenericBrowserType_SkeletalMesh
------------------------------------------------------------------------------*/

void UGenericBrowserType_SkeletalMesh::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( USkeletalMesh::StaticClass(), FColor(255,0,255), 0, this ) );
}

UBOOL UGenericBrowserType_SkeletalMesh::ShowObjectEditor( UObject* InObject )
{
	WxAnimSetViewer* AnimSetViewer = new WxAnimSetViewer( (wxWindow*)GApp->EditorFrame, -1, CastChecked<USkeletalMesh>(InObject), NULL, NULL );
	AnimSetViewer->Show();

	return 1;
}



/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_SkeletalMesh::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	// Determine if any of the selected skeletal meshes have a source asset that exists on disk; used to decide if the source asset
	// commands should be enabled or not
	UBOOL bHaveSourceAsset = FALSE;
	for( USelection::TObjectConstIterator It( InObjects->ObjectConstItor() ); It; ++It )
	{
		USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>( *It );
		if( SkeletalMesh && SkeletalMesh->SourceFilePath.Len() && GFileManager->FileSize( *( SkeletalMesh->SourceFilePath ) ) != INDEX_NONE )
		{
			bHaveSourceAsset = TRUE;
			break;
		}
	}

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingAnimSetViewer" ) ) );

	OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Reimport, *LocalizeUnrealEd( "ObjectContext_ReimportSkeletalMesh" ), !bAnythingCooked ) );
	AddSourceAssetCommands( OutCommands, bHaveSourceAsset );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CreateNewPhysicsAsset, *LocalizeUnrealEd( "ObjectContext_CreateNewPhysicsAsset" ), !bAnythingCooked ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CreateNewFaceFXAsset, *LocalizeUnrealEd( "ObjectContext_CreateFaceFXAsset" ), !bAnythingCooked ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_SkeletalMeshContext_CreateNewAnimSet, *LocalizeUnrealEd( "ObjectContext_CreateNewAnimSetFromMesh" ) ) );
}


/**
 * Invokes a custom menu item command.
 *
 * @param InCommand		The command to execute
 * @param InObjects		The objects to invoke the command against
 */
void UGenericBrowserType_SkeletalMesh::InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects )
{
	for (INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex)
	{
		UObject* Object = InObjects(ObjIndex);
		if( InCommand == IDMN_ObjectContext_CreateNewPhysicsAsset )
		{
			USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(Object);
			if( SkelMesh )
			{
				// Create a new physics asset from the selected skeletal mesh. Defaults to being in the same package/group as the skeletal mesh.

				FString DefaultPackage = SkelMesh->GetOutermost()->GetName();
				FString DefaultGroup = SkelMesh->GetOuter()->GetOuter() ? SkelMesh->GetFullGroupName( TRUE ) : TEXT("");

				// Make default name by appending '_Physics' to the SkeletalMesh name.
				FString DefaultAssetName = FString( FString::Printf( TEXT("%s_Physics"), *SkelMesh->GetName() ) );

				// First of all show the dialog for choosing a new package file, group and asset name:
				WxDlgPackageGroupName PackageDlg;
				if( PackageDlg.ShowModal(DefaultPackage, DefaultGroup, DefaultAssetName) == wxID_OK )
				{
					// Get the full name of where we want to create the physics asset.
					FString PackageName;
					if(PackageDlg.GetGroup().Len() > 0)
					{
						PackageName = PackageDlg.GetPackage() + TEXT(".") + PackageDlg.GetGroup();
					}
					else
					{
						PackageName = PackageDlg.GetPackage();
					}

					// Then find/create it.
					UPackage* Package = UObject::CreatePackage(NULL, *PackageName);
					check(Package);

					// Now show the 'asset creation' options dialog
					WxDlgNewPhysicsAsset AssetDlg;
					if( AssetDlg.ShowModal( SkelMesh, false ) == wxID_OK )
					{			
						UPhysicsAsset* NewAsset = ConstructObject<UPhysicsAsset>( UPhysicsAsset::StaticClass(), Package, *PackageDlg.GetObjectName(), RF_Public|RF_Standalone|RF_Transactional );
						if(NewAsset)
						{
							// Do automatic asset generation.
							UBOOL bSuccess = NewAsset->CreateFromSkeletalMesh( SkelMesh, AssetDlg.Params );
							if(bSuccess)
							{
								NewAsset->MarkPackageDirty();
								if(AssetDlg.bOpenAssetNow)
								{
									WxPhAT* AssetEditor = new WxPhAT(GApp->EditorFrame, -1, NewAsset);
									AssetEditor->SetSize(256,256,1024,768);
									AssetEditor->Show(1);
								}
								GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, CBR_ObjectCreated, NewAsset));
							}
							else
							{
								appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_FailedToCreatePhysAssetFromSM"), *PackageDlg.GetObjectName(), *SkelMesh->GetName()) );
								NewAsset->ClearFlags( RF_Public| RF_Standalone );
							}
						}
						else
						{
							appMsgf( AMT_OK, *LocalizeUnrealEd("Error_FailedToCreateNewPhysAsset") );
						}
					}
				}
			}
		}
		else if( InCommand == IDMN_ObjectContext_CreateNewFaceFXAsset )
		{
			USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(Object);
			if( SkelMesh )
			{
				if( SkelMesh->FaceFXAsset )
				{
					//@todo Localize this string.
					wxMessageBox(wxString(TEXT("That Skeletal Mesh already has a valid FaceFX Asset!  Please remove the existing one first.")), wxString(TEXT("FaceFX")));
				}
				else
				{
					// Create a new FaceFX asset from the selected skeletal mesh. 
					// Defaults to being in the same package/group as the skeletal mesh.
					FString DefaultPackage = SkelMesh->GetOutermost()->GetName();
					FString DefaultGroup = SkelMesh->GetOuter()->GetOuter() ? SkelMesh->GetFullGroupName(TRUE) : TEXT("");

					// Make default name by appending '_FaceFX' to the SkeletalMesh name.
					FString DefaultAssetName = FString(FString::Printf( TEXT("%s_FaceFX"), *SkelMesh->GetName()));

					// First of all show the dialog for choosing a new package file, group and asset name:
					WxDlgPackageGroupName PackageDlg;
					if( PackageDlg.ShowModal(DefaultPackage, DefaultGroup, DefaultAssetName) == wxID_OK )
					{
						// Get the full name of where we want to create the FaceFX asset.
						FString PackageName;
						if( PackageDlg.GetGroup().Len() > 0 )
						{
							PackageName = PackageDlg.GetPackage() + TEXT(".") + PackageDlg.GetGroup();
						}
						else
						{
							PackageName = PackageDlg.GetPackage();
						}

						// Then find/create it.
						UPackage* Package = UObject::CreatePackage(NULL, *PackageName);
						check(Package);

						FString ExistingAssetPath = PackageName;
						ExistingAssetPath += ".";
						ExistingAssetPath += *PackageDlg.GetObjectName();
						UFaceFXAsset* ExistingAsset = LoadObject<UFaceFXAsset>(NULL, *ExistingAssetPath, NULL, LOAD_NoWarn, NULL);
						if( ExistingAsset )
						{
							//@todo Localize this string.
							wxMessageBox(wxString(TEXT("That FaceFXAsset already exists!  Please remove the existing one first.")), wxString(TEXT("FaceFX")));
						}
						else
						{
							UFaceFXAsset* NewAsset = ConstructObject<UFaceFXAsset>(UFaceFXAsset::StaticClass(), Package, *PackageDlg.GetObjectName(), RF_Public|RF_Standalone|RF_Transactional);
							if( NewAsset )
							{
								NewAsset->CreateFxActor(SkelMesh);
								NewAsset->MarkPackageDirty();

								GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, CBR_ObjectCreated, NewAsset));
							}
						}
					}
				}
			}
		}
		else if ( InCommand == IDMN_SkeletalMeshContext_CreateNewAnimSet )
		{
			USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(Object);
			if( SkelMesh )
			{
				// Create a new AnimSet asset from the selected skeletal mesh. 
				// Defaults to being in the same package/group as the skeletal mesh.
				FString DefaultPackage = SkelMesh->GetOutermost()->GetName();
				FString DefaultGroup = SkelMesh->GetOuter()->GetOuter() ? SkelMesh->GetFullGroupName(TRUE) : TEXT("");

				// Make default name by appending '_Anims' to the SkeletalMesh name.
				FString DefaultAssetName = FString( FString::Printf( TEXT("%s_Anims"), *SkelMesh->GetName() ) );

				// First of all show the dialog for choosing a new package file, group and asset name:
				WxDlgPackageGroupName PackageDlg;
				if( PackageDlg.ShowModal(DefaultPackage, DefaultGroup, DefaultAssetName) == wxID_OK )
				{
					// Get the full name of where we want to create the AnimSet asset.
					FString PackageName;
					if(PackageDlg.GetGroup().Len() > 0)
					{
						PackageName = PackageDlg.GetPackage() + TEXT(".") + PackageDlg.GetGroup();
					}
					else
					{
						PackageName = PackageDlg.GetPackage();
					}

					// Then find/create it.
					UPackage* Package = UObject::CreatePackage(NULL, *PackageName);
					check(Package);

					// Create the new AnimSet in the user-selected package
					UAnimSet* NewAsset = ConstructObject<UAnimSet>( UAnimSet::StaticClass(), Package, *PackageDlg.GetObjectName(), RF_Public|RF_Standalone );
					if ( NewAsset )
					{
						// Update the content browser to show the new asset
						NewAsset->MarkPackageDirty();
						GCallbackEvent->Send(FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_ObjectCreated, NewAsset ) );
						
						// Open the AnimSet viewer with the new AnimSet and the selected mesh
						WxAnimSetViewer* AnimSetViewer = new WxAnimSetViewer( GApp->EditorFrame, -1, SkelMesh, NewAsset, NULL );
						AnimSetViewer->Show();
					}
					else
					{
						appMsgf( AMT_OK, *LocalizeUnrealEd("Error_FailedToCreateNewAnimSetAsset") );
					}
				}
			}
		}
		// Attempt to re-import the mesh from its source (if such meta-data exists)
		else if ( InCommand == IDMN_ObjectContext_Reimport )
		{
			FReimportManager::Instance()->Reimport( Object );
		}
		// Attempt to explore the folder of the mesh's source asset
		else if ( InCommand == IDMN_ObjectContext_FindSourceInExplorer )
		{
			USkeletalMesh* SkelMesh = Cast<USkeletalMesh>( Object );
			if ( SkelMesh )
			{
				ExploreSourceAssetFolder( SkelMesh->SourceFilePath );
			}
		}
		// Attempt to open the mesh's source asset in its default external editor
		else if ( InCommand == IDMN_ObjectContext_OpenSourceInExternalEditor )
		{
			USkeletalMesh* SkelMesh = Cast<USkeletalMesh>( Object );
			if ( SkelMesh )
			{
				OpenSourceAssetInExternalEditor( SkelMesh->SourceFilePath );
			}
		}
	}
}

/*------------------------------------------------------------------------------
UGenericBrowserType_Sounds
------------------------------------------------------------------------------*/

void UGenericBrowserType_Sounds::Init( void )
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( USoundCue::StaticClass(), FColor( 0, 175, 255 ), 0, this ) );
	SupportInfo.AddItem( FGenericBrowserTypeInfo( USoundNodeWave::StaticClass(), FColor( 0, 0, 255 ), 0, this ) );
	SupportInfo.AddItem( FGenericBrowserTypeInfo( USoundClass::StaticClass(), FColor( 255, 175, 0 ), 0, this ) );
	SupportInfo.AddItem( FGenericBrowserTypeInfo( USoundMode::StaticClass(), FColor( 175, 0, 255 ), 0, this ) );
}

UBOOL UGenericBrowserType_Sounds::ShowObjectEditor( UObject* InObject )
{
	USoundCue* SoundCue = Cast<USoundCue>( InObject );
	USoundNodeWave* SoundNodeWave = Cast<USoundNodeWave>( InObject );
	USoundClass* SoundClass = Cast<USoundClass>( InObject );
	if( SoundCue )
	{
		WxSoundCueEditor* SoundCueEditor = new WxSoundCueEditor( GApp->EditorFrame, -1, CastChecked<USoundCue>( InObject ) );
		SoundCueEditor->InitEditor();
		SoundCueEditor->Show( true );
	}
	else if( SoundNodeWave )
	{
		//@todo cb: GB conversion
		if( !SoundNodeWave->bUseTTS && SoundNodeWave->ChannelSizes.Num() == 0 && !WxDlgSoundNodeWaveOptions::bQualityPreviewerActive )
		{
			// Create an instance of the Wave Node options dialog.
			WxDlgSoundNodeWaveOptions* DlgSoundNodeWave = new WxDlgSoundNodeWaveOptions( GApp->EditorFrame, SoundNodeWave );
		}
		else
		{
			if( SoundNodeWave->bUseTTS )
			{
				appMsgf( AMT_OK, TEXT( "There are no available quality settings for TextToSpeech (TTS)." ) );
			}
			else if( SoundNodeWave->ChannelSizes.Num() > 0 )
			{
				appMsgf( AMT_OK, TEXT( "Sound quality preview is unavailable for multichannel sounds." ) );
			}
		}
	}
	else if( SoundClass )
	{
		UAudioDevice* AudioDevice = GEditor->Client ? GEditor->Client->GetAudioDevice() : NULL;
		if( AudioDevice && AudioDevice->GetSoundClass( ( FName )NAME_Master ) )
		{
			WxSoundClassEditor* SoundClassEditor = new WxSoundClassEditor( GApp->EditorFrame, -1 );
			SoundClassEditor->InitEditor();
			SoundClassEditor->Show( true );
		}
		else
		{
			appMsgf( AMT_OK, TEXT( "Cannot create a child sound class without a master sound class." ) );
		}
	}

	return TRUE;
}

/**
 * Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
 *
 * @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
 * @param	OutCommands		The list of custom commands to support
 */
void UGenericBrowserType_Sounds::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	// Figure out what type of Sound objects we're dealing with
	UBOOL bAllAreSoundNodeWaves = TRUE;
	UBOOL bAllAreSoundCues = TRUE;
	UBOOL bAnyAreSoundClasses = FALSE;
	UBOOL bAnyAreSoundModes = FALSE;
	TArray<USoundNodeWave*> SelectedSoundWaves;
	{
		for( USelection::TObjectIterator It( InObjects->ObjectItor() ); It; ++It )
		{
			UObject* Object = *It;
			if( !Object )
			{
				continue;
			}

			// Track selected sound node waves, so we can later verify if they have source assets specified
			USoundNodeWave* ObjAsSoundNodeWave = Cast<USoundNodeWave>( Object );
			if ( ObjAsSoundNodeWave )
			{
				SelectedSoundWaves.AddUniqueItem( ObjAsSoundNodeWave );
			}
			else
			{
				bAllAreSoundNodeWaves = FALSE;
			}

			if( !Object->IsA( USoundCue::StaticClass() ) )
			{
				bAllAreSoundCues = FALSE;
			}

			if( Object->IsA( USoundClass::StaticClass() ) )
			{
				bAnyAreSoundClasses = TRUE;
			}

			if( Object->IsA( USoundMode::StaticClass() ) )
			{
				bAnyAreSoundModes = TRUE;
			}
		}
	}

	if( bAllAreSoundNodeWaves )
	{
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingSoundPreviewer" ) ) );
	}
	else if( bAllAreSoundCues )
	{
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingSoundCueEditor" ) ) );
		
		GEditor->CreateSoundClassMenuForContentBrowser( OutCommands );

#if !UDK
		// Gears cue fixup hacks
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_InsertRadioChirp, LocalizeUnrealEd( "PackageContext_InsertRadioChirp" ) ) );

		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_InsertMatureDialog, LocalizeUnrealEd( "ObjectContext_InsertMatureDialog" ) ) );
#endif
	}
	else if( bAnyAreSoundClasses )
	{
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingSoundClassEditor" ) ) );
	}

	OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );

	// Add menu option to re-import sound node waves
	if ( bAllAreSoundNodeWaves )
	{
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Reimport, *LocalizeUnrealEd( "ObjectContext_ReimportSoundNodeWave" ), !bAnythingCooked ) );
		
		// Determine if any of the selected sound node waves have source assets that exist on disk; used to determine if the source asset commands
		// should be enabled or not
		UBOOL bHaveSourceAsset = FALSE;
		for ( TArray<USoundNodeWave*>::TConstIterator NodeIter( SelectedSoundWaves ); NodeIter; ++NodeIter )
		{
			USoundNodeWave* CurSoundNodeWave = *NodeIter;
			if ( CurSoundNodeWave->SourceFilePath.Len() && GFileManager->FileSize( *( CurSoundNodeWave->SourceFilePath ) ) != INDEX_NONE )
			{
				bHaveSourceAsset = TRUE;
				break;
			}
		}
		AddSourceAssetCommands( OutCommands, bHaveSourceAsset );
	}

	if( bAllAreSoundCues )
	{
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_EditNodes, *LocalizeUnrealEd( "ObjectContext_EditSoundCueNodes" ), !bAnythingCooked ) );
	}
	else if ( bAllAreSoundNodeWaves )
	{
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Properties, *LocalizeUnrealEd( "ObjectContext_EditSoundWaveNodes" ) ) );
	}
	else
	{
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Properties, *LocalizeUnrealEd( "ObjectContext_EditProperties" ) ) );
	}

	if( !bAnyAreSoundClasses && !bAnyAreSoundModes )
	{
		OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );

		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Sound_Play, *LocalizeUnrealEd( "ObjectContext_PlaySound" ) ) );
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Sound_Stop, *LocalizeUnrealEd( "ObjectContext_StopSound" ) ) );
	}
}

INT UGenericBrowserType_Sounds::QueryDefaultCommand( TArray<UObject*>& InObjects ) const
{
	if( InObjects.Num() == 1 )
	{
		if( InObjects( 0 )->IsA( USoundClass::StaticClass() ) )
		{
			return IDMN_ObjectContext_Editor;
		}

		if( InObjects( 0 )->IsA( USoundMode::StaticClass() ) )
		{
			return IDMN_ObjectContext_Properties;
		}
	}

	return IDMN_ObjectContext_Sound_Play;
}

void UGenericBrowserType_Sounds::InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects )
{
	for (INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex)
	{
		UObject* Object = InObjects(ObjIndex);
		USoundCue* SoundCue = Cast<USoundCue>( Object );
		USoundNodeWave* SoundNodeWave = Cast<USoundNodeWave>( Object );

		if( InCommand == IDMN_ObjectContext_Sound_Play )
		{
			if( SoundCue )
			{
				Play( SoundCue );
			}
			else if( SoundNodeWave )
			{
				Play( SoundNodeWave );
			}
		}
		else if( InCommand == IDMN_ObjectContext_Sound_Stop )
		{
			Stop();
		}
		else if( SoundCue && InCommand >= IDMN_ObjectContext_SoundCue_SoundClasses_START && InCommand < IDMN_ObjectContext_SoundCue_SoundClasses_END )
		{
			FName SoundClassName = NAME_None;

			UAudioDevice* AudioDevice = GEditor && GEditor->Client ? GEditor->Client->GetAudioDevice() : NULL;
			if( AudioDevice )
			{
				SoundClassName = AudioDevice->GetSoundClass( InCommand );
			}

			// Mark package dirty
			SoundCue->Modify();

			// Set sound group.
			SoundCue->SoundClass = SoundClassName;
			// Set the appropriate enum value based on the name we just set.
			SoundCue->Fixup();

			SoundCue->PostEditChange();
		}
	}
}

void UGenericBrowserType_Sounds::DoubleClick( UObject* InObject )
{
	USoundCue* SoundCue = Cast<USoundCue>( InObject );
	USoundNode* SoundNodeWave = Cast<USoundNodeWave>( InObject );
	if( SoundCue )
	{
		Play( SoundCue );
	}
	else if( SoundNodeWave )
	{
		Play( SoundNodeWave );
	}
	else
	{
		ShowObjectProperties();
	}
}

void UGenericBrowserType_Sounds::Play( USoundNode* InSound )
{
	if( !WxDlgSoundNodeWaveOptions::bQualityPreviewerActive )
	{
		UAudioComponent* AudioComponent = GEditor->GetPreviewAudioComponent( NULL, InSound );
		if( AudioComponent )
		{
			AudioComponent->Stop();

			// Create the ogg vorbis or TTS data for this file if it doesn't exist
			AudioComponent->SoundCue->ValidateData();

			AudioComponent->bUseOwnerLocation = FALSE;
			AudioComponent->bAutoDestroy = FALSE;
			AudioComponent->Location = FVector( 0.0f, 0.0f, 0.0f );
			AudioComponent->bIsUISound = TRUE;
			AudioComponent->bAllowSpatialization = FALSE;
			AudioComponent->bReverb = FALSE;
			AudioComponent->bCenterChannelOnly = FALSE;

			AudioComponent->Play();	
		}
	}
}

void UGenericBrowserType_Sounds::Play( USoundCue* InSound )
{
	if( !WxDlgSoundNodeWaveOptions::bQualityPreviewerActive )
	{
		UAudioComponent* AudioComponent = GEditor->GetPreviewAudioComponent( InSound, NULL );
		if( AudioComponent )
		{
			AudioComponent->Stop();

			// Create the ogg vorbis data for this file if it doesn't exist
			AudioComponent->SoundCue->ValidateData();
			
			AudioComponent->SoundCue = InSound;
			AudioComponent->bUseOwnerLocation = FALSE;
			AudioComponent->bAutoDestroy = FALSE;
			AudioComponent->Location = FVector( 0.0f, 0.0f, 0.0f );
			AudioComponent->bIsUISound = TRUE;
			AudioComponent->bAllowSpatialization	= FALSE;
			AudioComponent->bReverb = FALSE;
			AudioComponent->bCenterChannelOnly = FALSE;

			AudioComponent->Play();	
		}
	}
}

bool UGenericBrowserType_Sounds::IsPlaying( USoundCue* InSound )
{
	UAudioComponent* AudioComponent = GEditor->GetPreviewAudioComponent( InSound, NULL );
	return AudioComponent && AudioComponent->IsPlaying();
}

bool UGenericBrowserType_Sounds::IsPlaying( USoundNode* InSound )
{
	UAudioComponent* AudioComponent = GEditor->GetPreviewAudioComponent( NULL, InSound );
	return AudioComponent && AudioComponent->IsPlaying();
}

void UGenericBrowserType_Sounds::Stop( void )
{
	UAudioComponent* AudioComponent = GEditor->GetPreviewAudioComponent( NULL, NULL );
	if( AudioComponent )
	{
		AudioComponent->Stop();
		AudioComponent->SoundCue = NULL;
	}
}

/*------------------------------------------------------------------------------
UGenericBrowserType_SoundCue
------------------------------------------------------------------------------*/

void UGenericBrowserType_SoundCue::Init( void )
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( USoundCue::StaticClass(), FColor( 0, 175, 255 ), 0, this ) );
}

/**
 * Invokes a custom menu item command for every selected object
 * of a supported class.
 *
 * @param InCommand		The command to execute
 * @param InObjects		The objects to invoke the command against
 */
void UGenericBrowserType_SoundCue::InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects )
{
	for (INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex)
	{
		if( InCommand == IDMN_ObjectContext_EditNodes )
		{
			// Get all selected objects
			TArray<UObject*>		SelectedObjects;
			GEditor->GetSelectedObjects()->GetSelectedObjects(SelectedObjects);

			// Generate the list of selected sound cues
			TArray<USoundCue*>		SoundCueList;
			for ( INT SelectedObjectIndex = 0 ; SelectedObjectIndex < SelectedObjects.Num() ; ++SelectedObjectIndex )
			{
				USoundCue* SoundCue = Cast<USoundCue>( SelectedObjects(SelectedObjectIndex) );
				if ( SoundCue )
				{
					SoundCueList.AddItem( SoundCue );
				}
			}

			// Create one sound cue properties window using all of the sound cues
			if( SoundCueList.Num() > 0 )
			{
				WxDlgSoundCueProperties* dlg = new WxDlgSoundCueProperties(SoundCueList);
				dlg->Show(1);
			}
			break;
		}
	}
}

/*------------------------------------------------------------------------------
UGenericBrowserType_SoundMode
------------------------------------------------------------------------------*/

void UGenericBrowserType_SoundMode::Init( void )
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( USoundMode::StaticClass(), FColor( 175, 0, 255 ), 0, this ) );
}

UBOOL UGenericBrowserType_SoundMode::NotifyPreDeleteObject( UObject* ObjectToDelete )
{
	UAudioDevice* AudioDevice = GEditor && GEditor->Client ? GEditor->Client->GetAudioDevice() : NULL;
	if( AudioDevice )
	{
		USoundMode* SoundMode = Cast<USoundMode>( ObjectToDelete );
		AudioDevice->RemoveMode( SoundMode );
	}

	return( TRUE );
}

void UGenericBrowserType_SoundMode::NotifyPostDeleteObject()
{
	UAudioDevice* AudioDevice = GEditor && GEditor->Client ? GEditor->Client->GetAudioDevice() : NULL;
	if( AudioDevice )
	{
		AudioDevice->InitSoundModes();
	}
}

/*------------------------------------------------------------------------------
UGenericBrowserType_SoundClass
------------------------------------------------------------------------------*/

void UGenericBrowserType_SoundClass::Init( void )
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( USoundClass::StaticClass(), FColor( 255, 175, 0 ), 0, this ) );
}

UBOOL UGenericBrowserType_SoundClass::NotifyPreDeleteObject( UObject* ObjectToDelete )
{
	UAudioDevice* AudioDevice = GEditor && GEditor->Client ? GEditor->Client->GetAudioDevice() : NULL;
	if( AudioDevice )
	{
		USoundClass* SoundClass = Cast<USoundClass>( ObjectToDelete );
		AudioDevice->RemoveClass( SoundClass );

		ObjectTools::RefreshResourceType( UGenericBrowserType_SoundCue::StaticClass() );
		ObjectTools::RefreshResourceType( UGenericBrowserType_Sounds::StaticClass() );
	}

	return( TRUE );
}

void UGenericBrowserType_SoundClass::NotifyPostDeleteObject()
{
	UAudioDevice* AudioDevice = GEditor && GEditor->Client ? GEditor->Client->GetAudioDevice() : NULL;
	if( AudioDevice )
	{
		AudioDevice->InitSoundClasses();
	}
}

/**
* Called when a property value from a member struct or array has been changed in the editor.
*/
void USoundClass::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ObjectTools::RefreshResourceType( UGenericBrowserType_SoundCue::StaticClass() );
	ObjectTools::RefreshResourceType( UGenericBrowserType_Sounds::StaticClass() );
}


/*------------------------------------------------------------------------------
UGenericBrowserType_SoundWave
------------------------------------------------------------------------------*/

void UGenericBrowserType_SoundWave::Init( void )
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( USoundNodeWave::StaticClass(), FColor( 0, 0, 255 ), 0, this ) );
}

/**
 * Invokes a custom menu item command for every selected object
 * of a supported class.
 *
 * @param InCommand		The command to execute
 * @param InObjects		The objects to invoke the command against
 */
void UGenericBrowserType_SoundWave::InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects )
{
	if ( InCommand == IDMN_ObjectContext_Reimport )
	{
		for ( INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex )
		{
			UObject* Object = InObjects(ObjIndex);
			USoundNodeWave* SoundNodeWave = Cast<USoundNodeWave>( Object );
			
			// Handle reimporting sound node waves
			if ( SoundNodeWave )
			{	
				FReimportManager::Instance()->Reimport( Object );
			}
		}
	}

	// Attempt to explore the folder of each source asset
	else if ( InCommand == IDMN_ObjectContext_FindSourceInExplorer )
	{
		for ( INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex )
		{
			USoundNodeWave* SoundNodeWave = Cast<USoundNodeWave>( InObjects( ObjIndex ) );
			if ( SoundNodeWave )
			{
				ExploreSourceAssetFolder( SoundNodeWave->SourceFilePath );
			}
		}
	}

	// Attempt to open each source asset in its default external editor
	else if ( InCommand == IDMN_ObjectContext_OpenSourceInExternalEditor )
	{
		for ( INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex )
		{
			USoundNodeWave* SoundNodeWave = Cast<USoundNodeWave>( InObjects( ObjIndex ) );
			if ( SoundNodeWave )
			{
				OpenSourceAssetInExternalEditor( SoundNodeWave->SourceFilePath );
			}
		}
	}
}

/*------------------------------------------------------------------------------
UGenericBrowserType_SpeechRecognition
------------------------------------------------------------------------------*/

void UGenericBrowserType_SpeechRecognition::Init( void )
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( USpeechRecognition::StaticClass(), FColor( 0, 0, 255 ), 0, this ) );
}

/*------------------------------------------------------------------------------
UGenericBrowserType_StaticMesh
------------------------------------------------------------------------------*/

void UGenericBrowserType_StaticMesh::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UFracturedStaticMesh::StaticClass(), FColor(96,200,255), 0, this ) );
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UStaticMesh::StaticClass(), FColor(0,255,255), 0, this ) );
}

UBOOL UGenericBrowserType_StaticMesh::ShowObjectEditor( UObject* InObject )
{
	WxStaticMeshEditor* StaticMeshEditor = new WxStaticMeshEditor( GApp->EditorFrame, -1, CastChecked<UStaticMesh>(InObject), FALSE /*bForceSimplificationWindowVisible*/ );
	StaticMeshEditor->Show(1);

	return 1;
}



/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_StaticMesh::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingStaticMeshEditor" ) ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Reimport, *LocalizeUnrealEd( "ObjectContext_ReimportStaticMesh" ), !bAnythingCooked ) );
	{
		UBOOL bHaveNonSimplifiedStaticMesh = FALSE;
		UBOOL bHaveSimplifiedStaticMesh = FALSE;
		UBOOL bHaveSourceAsset = FALSE;

		INT NumSelectedObjects = 0;
		for( FSelectionIterator SelIter( *InObjects ); SelIter != NULL; ++SelIter )
		{
			UObject* SelObject = *SelIter;
			if( SelObject->IsA( UStaticMesh::StaticClass() ) )
			{
				UStaticMesh* StaticMesh = CastChecked<UStaticMesh>( SelObject );

				// Check to see if the mesh is already simplified.  If it has a source mesh name we can
				// assume that it is.
				if( StaticMesh->HighResSourceMeshName.Len() == 0 )
				{
					bHaveNonSimplifiedStaticMesh = TRUE;
				}
				else
				{
					bHaveSimplifiedStaticMesh = TRUE;
				}
				
				// Check if any of the selected static meshes have source assets on disk; used to determine if the source asset commands should be enabled or not
				if ( !bHaveSourceAsset && StaticMesh->SourceFilePath.Len() && ( GFileManager->FileSize( *( StaticMesh->SourceFilePath ) ) != INDEX_NONE ) )
				{
					bHaveSourceAsset = TRUE;
				}
			}

			++NumSelectedObjects;
		}

		AddSourceAssetCommands( OutCommands, bHaveSourceAsset );

		// Only show this option when there is a single static mesh selected
		if( NumSelectedObjects == 1 )
		{
			// Check to see if we have any meshes that can be simplified
			if( bHaveNonSimplifiedStaticMesh )
			{
				OutCommands.AddItem( FObjectSupportedCommandType( IDMN_StaticMeshContext_SimplifyMesh, *LocalizeUnrealEd( "ObjectContext_SimplifyMesh" ), !bAnythingCooked ) );
			}
			else if( bHaveSimplifiedStaticMesh )
			{
				OutCommands.AddItem( FObjectSupportedCommandType( IDMN_StaticMeshContext_SimplifyMesh, *LocalizeUnrealEd( "ObjectContext_ResimplifyMesh" ), !bAnythingCooked ) );
			}
		}

		//NOTE - The execution of these commands happens in ContentBrowserCLR.cpp because it has to work on MULTIPLE meshes and I only want to ask the user about the directory once.
		if (NumSelectedObjects)
		{
			OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_ExportLightMapMeshes, *LocalizeUnrealEd( "ObjectContext_ExportLightMapMeshes" ), !bAnythingCooked ) );
			OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_ImportLightMapMeshes, *LocalizeUnrealEd( "ObjectContext_ImportLightMapMeshes" ), !bAnythingCooked ) );
		}
	}
}


void UGenericBrowserType_StaticMesh::InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects )
{
	if( InCommand == IDMN_StaticMeshContext_SimplifyMesh )
	{
		for (INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex)
		{
			// Make sure we have a static mesh
			UStaticMesh* StaticMesh = Cast<UStaticMesh>( InObjects(ObjIndex) );
			if( StaticMesh != NULL )
			{
				// OK, launch the static mesh editor (in 'simplify mode') with the selected mesh
				WxStaticMeshEditor* StaticMeshEditor =
					new WxStaticMeshEditor( GApp->EditorFrame, -1, StaticMesh, TRUE /*bForceSimplificationWindowVisible*/ );
				StaticMeshEditor->Show( TRUE );
			}
		}
	}
	else if ((InCommand == IDMN_ObjectContext_ExportLightMapMeshes) || (InCommand == IDMN_ObjectContext_ImportLightMapMeshes))
	{
		TArray< UStaticMesh* > StaticMeshArray;

		for (INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex)
		{
			UStaticMesh* StaticMesh = Cast<UStaticMesh>( InObjects(ObjIndex) );
			if (StaticMesh != NULL)
			{
				StaticMeshArray.AddItem(StaticMesh);
			}
		}
		FString Directory;
		if (InCommand == IDMN_ObjectContext_ExportLightMapMeshes) 
		{
			if (PromptUserForDirectory(Directory, *LocalizeUnrealEd("StaticMeshEditor_ExportToPromptTitle"), *GApp->LastDir[LD_MESH_IMPORT_EXPORT]))
			{
				GApp->LastDir[LD_MESH_IMPORT_EXPORT] = Directory; // Save path as default for next time.

				UBOOL bAnyErrorOccurred = ExportMeshUtils::ExportAllLightmapModels(StaticMeshArray, Directory, FALSE);
				if (bAnyErrorOccurred) {
					appMsgf( AMT_OK, *(LocalizeUnrealEd("StaticMeshEditor_LightmapExportFailure")));
				}
			}
		}
		else
		{
			if (PromptUserForDirectory(Directory, *LocalizeUnrealEd("StaticMeshEditor_ImportToPromptTitle"), *GApp->LastDir[LD_MESH_IMPORT_EXPORT]))
			{
				GApp->LastDir[LD_MESH_IMPORT_EXPORT] = Directory; // Save path as default for next time.

				UBOOL bAnyErrorOccurred = ExportMeshUtils::ImportAllLightmapModels(StaticMeshArray, Directory, FALSE);
				if (bAnyErrorOccurred) {
					appMsgf( AMT_OK, *(LocalizeUnrealEd("StaticMeshEditor_LightmapImportFailure")));
				}
			}
		}
	}
	else if( InCommand == IDMN_ObjectContext_Reimport )
	{
		for (INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex)
		{
			UObject* Object = InObjects(ObjIndex);
			if( InCommand == IDMN_ObjectContext_Reimport )
			{
				FReimportManager::Instance()->Reimport(Object);
			}
		}
	}
	// Handle a request to explore to the folder of the mesh's source asset
	else if ( InCommand == IDMN_ObjectContext_FindSourceInExplorer )
	{
		for ( INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex )
		{
			UStaticMesh* StaticMesh = Cast<UStaticMesh>( InObjects(ObjIndex) );
			if ( StaticMesh && StaticMesh->SourceFilePath.Len() && GFileManager->FileSize( *( StaticMesh->SourceFilePath ) ) != INDEX_NONE )
			{
				ExploreSourceAssetFolder( StaticMesh->SourceFilePath );
			}
		}
	}
	// Handle a request to open the mesh's source asset in its default editor
	else if ( InCommand == IDMN_ObjectContext_OpenSourceInExternalEditor )
	{
		for ( INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex )
		{
			UStaticMesh* StaticMesh = Cast<UStaticMesh>( InObjects(ObjIndex) );
			if ( StaticMesh && StaticMesh->SourceFilePath.Len() && GFileManager->FileSize( *( StaticMesh->SourceFilePath ) ) != INDEX_NONE )
			{
				OpenSourceAssetInExternalEditor( StaticMesh->SourceFilePath );
			}
		}
	}
}





/*------------------------------------------------------------------------------
UGenericBrowserType_FracuredStaticMesh
------------------------------------------------------------------------------*/

void UGenericBrowserType_FracturedStaticMesh::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UFracturedStaticMesh::StaticClass(), FColor(96,200,255), 0, this ) );
}




/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_FracturedStaticMesh::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingStaticMeshEditor" ) ) );

	OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_ResliceFracturedMeshes, *LocalizeUnrealEd( "ObjectContext_ResliceFracturedMeshes" ), !bAnythingCooked ) );
}



void UGenericBrowserType_FracturedStaticMesh::InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects )
{
	for (INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex)
	{
		UObject* Object = InObjects(ObjIndex);
		if( InCommand == IDMN_ObjectContext_ResliceFracturedMeshes )
		{
			UFracturedStaticMesh* FracturedStaticMesh = Cast<UFracturedStaticMesh>( Object );
			if(FracturedStaticMesh)
			{
				// Open a static mesh editor window for the mesh.
				WxStaticMeshEditor* StaticMeshEditor = new WxStaticMeshEditor( GApp->EditorFrame, -1, FracturedStaticMesh, FALSE /*bForceSimplificationWindowVisible*/ );

				// Reslice the mesh.
				StaticMeshEditor->SliceMesh();

				// Close the static mesh editor window.
				StaticMeshEditor->Close();
			}
		}

	}
}

/*------------------------------------------------------------------------------
UGenericBrowserType_TerrainLayer
------------------------------------------------------------------------------*/

void UGenericBrowserType_TerrainLayer::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UTerrainLayerSetup::StaticClass(), FColor(128,192,255), 0, this ) );
}

/*------------------------------------------------------------------------------
UGenericBrowserType_TerrainMaterial
------------------------------------------------------------------------------*/

void UGenericBrowserType_TerrainMaterial::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UTerrainMaterial::StaticClass(), FColor(192,255,192), 0, this ) );
}


/*------------------------------------------------------------------------------
UGenericBrowserType_Texture
------------------------------------------------------------------------------*/

void UGenericBrowserType_Texture::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( 
		UTexture::StaticClass(), 
		FColor(255,0,0), 
		0, 
		this,
		UGenericBrowserType_Texture::ShouldDisplayCallback) );	
}

/**
* Callback to register whether or not the object should be displayed
* @param InObject - object that will be displayed in the GB
* @return TRUE if should be displayed
*/ 
UBOOL UGenericBrowserType_Texture::ShouldDisplayCallback( UObject* InObject )
{
	// don't display texture if its outer is a UFont
	UBOOL bResult = !(Cast<UTexture>(InObject) && Cast<UFont>(InObject->GetOuter()));
	return bResult;
}

UBOOL UGenericBrowserType_Texture::ShowObjectEditor( UObject* InObject )
{
	WxTexturePropertiesFrame* dlg = new WxTexturePropertiesFrame(Cast<UTexture>(InObject));

	return 1;
}



/*------------------------------------------------------------------------------
GenericBrowserType_TextureMovie
------------------------------------------------------------------------------*/

void UGenericBrowserType_TextureMovie::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UTextureMovie::StaticClass(), FColor(0,255,0), 0, this ) );
}



/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_Texture::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	// Count the number of textures selected, and determine if any could be compressed
	UBOOL bAnyDeferredCompression = FALSE;
	UINT NumSelectedTextures = 0;
	UBOOL bHasAnyAutoFlattenedTextures = FALSE;

	UBOOL bAllTexturesHaveSourceArt = TRUE;
	UBOOL bHaveSourceAsset = FALSE;
	{
		for (USelection::TObjectConstIterator It( InObjects->ObjectConstItor() ); It; ++It)
		{
			UTexture* Texture = Cast<UTexture>( *It );
			if (Texture != NULL)
			{
				++NumSelectedTextures;

				// Check if any of the selected textures have source assets on disk; used to determine if the source asset commands should be
				// enabled or not
				if ( !bHaveSourceAsset && Texture->SourceFilePath.Len() && GFileManager->FileSize( *( Texture->SourceFilePath ) ) != INDEX_NONE )
				{
					bHaveSourceAsset = TRUE;
				}

				bAllTexturesHaveSourceArt &= Texture->HasSourceArt();

				if (Texture->DeferCompression)
				{
					bAnyDeferredCompression = TRUE;
				}

				if( !bHasAnyAutoFlattenedTextures && Texture->GetName().InStr( TEXT("_Flattened"), TRUE, TRUE ) != INDEX_NONE )
				{
					bHasAnyAutoFlattenedTextures = TRUE;
				}
			}
		}
	}

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingTextureViewer" ) ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Properties, *LocalizeUnrealEd( "ObjectContext_EditProperties" ) ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_TextureContext_FindInTextureStatsBrowser, *LocalizeUnrealEd("TextureContextMenu_FindInTextureStatsBrowser" ) ) );

	if( NumSelectedTextures == 1 )
	{
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_TextureContext_FindAllMaterialsUsingThis, *LocalizeUnrealEd("TextureContextMenu_FindAllMaterialsUsingThis" ) ) );
	}

	OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Reimport, *LocalizeUnrealEd( "ObjectContext_ReimportTexture" ), !bAnythingCooked ) );
	AddSourceAssetCommands( OutCommands, bHaveSourceAsset );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_TextureContext_TossFlash, *LocalizeUnrealEd( "TossFlashCache" ) ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CreateNewMaterial, *LocalizeUnrealEd( "ObjectContext_CreateNewMaterial" ), !bAnythingCooked ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_TextureContext_CompressNow, *LocalizeUnrealEd( "TextureContextMenu_CompressNow" ), !bAnythingCooked && bAnyDeferredCompression) );

	// Enable the reflatten command if we have at least one flattened texture
	if( bHasAnyAutoFlattenedTextures )
	{
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_TextureContext_Reflatten, *LocalizeUnrealEd( "TextureContextMenu_ReflattenTexture" ) , !bAnythingCooked ) );
	}

	OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );

#if WITH_SUBSTANCE_AIR == 1
	if( bAllTexturesHaveSourceArt )
	{
		OutCommands.AddItem( FObjectSupportedCommandType( IDMN_TextureContext_ConvertToSbsImageInput, *LocalizeUnrealEd( "TextureContextMenu_ConvertToSbsImageInput" ) ) );
		OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );
	}
#endif //WITH_SUBSTANCE_AIR

	// Check to see if we have any texture adjustments in the clipboard
	UBOOL bHaveTextureAdjustmentsInClipboard = FALSE;
	{
		const FString ClipboardString = appClipboardPaste();
		if( ClipboardString.StartsWith( TEXT( "TextureAdjustment" ) ) )
		{
			bHaveTextureAdjustmentsInClipboard = TRUE;
		}
	}

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_TextureContext_CopyTextureAdjustments, *LocalizeUnrealEd( "ObjectContext_CopyTextureAdjustments" ), ( NumSelectedTextures == 1 ) ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_TextureContext_PasteTextureAdjustments, *LocalizeUnrealEd( "ObjectContext_PasteTextureAdjustments" ), bHaveTextureAdjustmentsInClipboard && ( NumSelectedTextures >= 1 ) ) );


	// @todo CB [reviewed; discuss]: Add support for this (see GB's use of SetStreamingBoundsTexture)
	// OutCommands.AddCheckItem( FObjectSupportedCommandType( IDMN_TextureContext_ShowStreamingBounds, *LocalizeUnrealEd( "ObjectContext_ToggleStreamingBounds" ) ) );
}


void ConditionalFlattenMaterial(UMaterialInterface* MaterialInterface, UBOOL bReflattenAutoFlattened, const UBOOL bInForceFlatten);

void UGenericBrowserType_Texture::InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects )
{
	UBOOL bDoingSlowTask = FALSE;

	if (InCommand == IDMN_TextureContext_PasteTextureAdjustments) 
	{
		bDoingSlowTask = TRUE; 
		GWarn->BeginSlowTask( *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT( "ObjectContext_PasteTextureAdjustments_Applying_F" ) ), InObjects.Num() ) ), bDoingSlowTask );
	}
	else if( InCommand ==IDMN_TextureContext_Reflatten )
	{
		if( InObjects.Num() > 1 )
		{
			bDoingSlowTask = TRUE; 
			GWarn->BeginSlowTask( TEXT("Reflattening Textures"), bDoingSlowTask );
		}
	}
	else if (InCommand == IDMN_TextureContext_CompressNow)
	{
		bDoingSlowTask = TRUE; 
		GWarn->BeginSlowTask( *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT( "ObjectContext_CompressTexturesNow_F" ) ), InObjects.Num() ) ), bDoingSlowTask );
	}
	else if( InCommand == IDMN_TextureContext_FindInTextureStatsBrowser )
	{
		WxTextureStatsBrowser* TextureStatsBrowser = GUnrealEd->GetBrowser<WxTextureStatsBrowser>(TEXT("TextureStats"));
		if ( TextureStatsBrowser != NULL )
		{
			// Focus the texture stats browser, select all textures found in the browser and scroll to them
			GUnrealEd->GetBrowserManager()->ShowWindow(TextureStatsBrowser->GetDockID(), TRUE);
			TextureStatsBrowser->SyncToTextures( InObjects, TRUE );
		}
	}
	else if( InCommand == IDMN_TextureContext_FindAllMaterialsUsingThis )
	{
#if WITH_MANAGED_CODE
		// There should only be one selected object if we get here
		check( InObjects.Num() == 1 );

		// Create a game asset database helper so we can query the database.
		FGADHelper* GADHelper = new FGADHelper();
		if( GADHelper->Initialize() )
		{
			GWarn->BeginSlowTask( TEXT("Searching materials"), TRUE, TRUE );
		
			// The texture we are searching for
			UTexture* TextureToSearch = CastChecked<UTexture>( InObjects(0) );

			// List of materials found that use the above texture.  List of UObjects for compatibility with collection methods.
			TArray<UObject*> MaterialsUsingTexture;

			// The list of full material names returned from the game asset database
			TArray<FString> MaterialsToSearch;
		
			// Create a list of material classes we should find.
			TArray<FString> MaterialClassTags;
			MaterialClassTags.AddItem( TEXT("[ObjectType]Material") );
			MaterialClassTags.AddItem( TEXT("[ObjectType]MaterialInstanceConstant") );
			MaterialClassTags.AddItem( TEXT("[ObjectType]MaterialInstanceTimeVarying") );
			MaterialClassTags.AddItem( TEXT("[ObjectType]DecalMaterial") );


			// Query the database for each object type.
			for( INT ClassTagIndex = 0; ClassTagIndex < MaterialClassTags.Num(); ++ClassTagIndex )
			{
				// List of system tags passed to the game asset database
				TArray<FString> Tags;

				// Set the object type we are searching for in the collection
				Tags.AddItem( MaterialClassTags( ClassTagIndex ) );

				TArray<FString> FullAssetNames;
				// Query the database for objects
				GADHelper->QueryAssetsWithAllTags( Tags, FullAssetNames );
				// Add to the list of found materials
				MaterialsToSearch.Append( FullAssetNames );
			}

			// We must now find or load each found material so that we can search it for the specified texture
			for( INT AssetIndex = 0; AssetIndex < MaterialsToSearch.Num(); ++AssetIndex )
			{
				if( GWarn->ReceivedUserCancel() )
				{
					break;
				}
				GWarn->StatusUpdatef(AssetIndex, MaterialsToSearch.Num(), *FString::Printf( TEXT("Searching Material: %d/%d"), AssetIndex+1, MaterialsToSearch.Num() ) );
				// Fill name of asset including the class name
				const FString& AssetNameWithClass = MaterialsToSearch( AssetIndex );
				// Extract class name, we don't need it
				INT SpacePos = AssetNameWithClass.InStr( TEXT(" ") );
				FString AssetName = AssetNameWithClass.Right( AssetNameWithClass.Len() - (SpacePos + 1) );

				// Attempt to find the object
				UObject* Object = UObject::StaticFindObject( UMaterialInterface::StaticClass(), ANY_PACKAGE, *AssetName, FALSE );
				if( !Object )
				{
					// It was not found so it must be loaded.
					Object = UObject::StaticLoadObject( UMaterialInterface::StaticClass(), NULL, *AssetName, NULL, LOAD_NoRedirects, NULL );
				}

				UMaterialInterface* Material = Cast<UMaterialInterface>( Object );
				if( Material )
				{
					// Find all textures used by the material
					TArray<UTexture*> UsedTextures;
					Material->GetUsedTextures( UsedTextures );
					if( UsedTextures.ContainsItem( TextureToSearch ) )
					{
						// One of the textures in this material is using the specified texture
						MaterialsUsingTexture.AddItem( Object );
					}
				}
			}

			GWarn->EndSlowTask();

			if( !GWarn->ReceivedUserCancel() )
			{
				debugf( TEXT("Materials using texture %d"), MaterialsUsingTexture.Num() );
				if( MaterialsUsingTexture.Num() > 0 )
				{
					const FString NewCollectionName = FString::Printf( TEXT("Materials using %s"), *TextureToSearch->GetName() );

					// Clear the current filter
					GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_ClearFilter ) );
					// Create a local collection with all the materials we found
					FContentBrowser& ActiveInstance = FContentBrowser::GetActiveInstance();
					// Wipe out any existing local collection with the same name
					ActiveInstance.DestroyLocalCollection( NewCollectionName );
					// Create the collection.
					ActiveInstance.CreateLocalCollection( NewCollectionName );
					// Add all materials we found to the collection
					if ( ActiveInstance.AddAssetsToLocalCollection( NewCollectionName, MaterialsUsingTexture ) )
					{
						// Select the newly created collection
						ActiveInstance.SelectCollection( NewCollectionName, EGADCollection::Type::Local );
					}
					else
					{
						// Wipe out any existing local collection with the same name
						ActiveInstance.DestroyLocalCollection( NewCollectionName );
					}
				}
			}
		}

		delete GADHelper;
#endif
	}

	for (INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex)
	{
		UObject* Object = InObjects(ObjIndex);
		if( InCommand == IDMN_ObjectContext_Reimport )
		{
			FReimportManager::Instance()->Reimport(Object);
		}
		// Handle request to explore texture's source asset's folder
		else if ( InCommand == IDMN_ObjectContext_FindSourceInExplorer )
		{
			UTexture2D* Texture = Cast<UTexture2D>( Object );
			if ( Texture )
			{
				ExploreSourceAssetFolder( Texture->SourceFilePath );
			}
		}
		// Handle request to open texture's source asset in its default external editor
		else if ( InCommand == IDMN_ObjectContext_OpenSourceInExternalEditor )
		{
			UTexture2D* Texture = Cast<UTexture2D>( Object );
			if ( Texture )
			{
				OpenSourceAssetInExternalEditor( Texture->SourceFilePath );
			}
		}
		else if( InCommand == IDMN_ObjectContext_CreateNewMaterial )
		{
			// Create a new material containing a texture material expression referring to the texture.
			const FString Package	= Object->GetOutermost()->GetName();
			const FString Group		= Object->GetOuter()->GetOuter() ? Object->GetFullGroupName( 1 ) : TEXT("");
			const FString Name		= FString::Printf( TEXT("%s_Mat"), *Object->GetName() );

			WxDlgPackageGroupName dlg;
			dlg.SetTitle( *LocalizeUnrealEd("CreateNewMaterial") );

			if( dlg.ShowModal( Package, Group, Name ) == wxID_OK )
			{
				FString Pkg;
				// Was a group specified?
				if( dlg.GetGroup().Len() > 0 )
				{
					Pkg = FString::Printf(TEXT("%s.%s"),*dlg.GetPackage(),*dlg.GetGroup());
				}
				else
				{
					Pkg = FString::Printf(TEXT("%s"),*dlg.GetPackage());
				}
				UObject* ExistingPackage = UObject::FindPackage(NULL, *Pkg);
				FString Reason;

				// Verify the package an object name.
				if(!dlg.GetPackage().Len() || !dlg.GetObjectName().Len())
				{
					appMsgf(AMT_OK,*LocalizeUnrealEd("Error_InvalidInput"));
				}
				// 			// Verify the object name.
				// 			else if( !FIsValidObjectName( *dlg.GetObjectName(), Reason ))
				// 			{
				// 				appMsgf( AMT_OK, *Reason );
				// 			}
				// Verify the object name is unique withing the package.
				else if (ExistingPackage && !FIsUniqueObjectName(*dlg.GetObjectName(), ExistingPackage, Reason))
				{
					appMsgf(AMT_OK, *Reason);
				}
				else
				{
					UMaterialFactoryNew* Factory = new UMaterialFactoryNew;
					UMaterial* Material = (UMaterial*)Factory->FactoryCreateNew( UMaterial::StaticClass(), UObject::CreatePackage(NULL,*Pkg), *dlg.GetObjectName(), RF_Public|RF_Transactional|RF_Standalone, NULL, GWarn );
					check( Material );
					UMaterialExpressionTextureSample* Expression = ConstructObject<UMaterialExpressionTextureSample>( UMaterialExpressionTextureSample::StaticClass(), Material );
					Material->Expressions.AddItem( Expression );
					Expression->Texture = CastChecked<UTexture>( Object );
					Expression->PostEditChange();
					// We pass in the created material as refresh parameter so that it becomes focused.
					GCallbackEvent->Send( FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, CBR_ObjectCreated|CBR_SyncAssetView, Material) );
				}
			}
		}
		else if( InCommand == IDMN_TextureContext_ShowStreamingBounds )
		{
			UTexture2D* Texture = Cast<UTexture2D>(Object);
			if (Texture == GEditor->StreamingBoundsTexture)
			{
				GEditor->SetStreamingBoundsTexture(NULL);
			}
			else
			{
				GEditor->SetStreamingBoundsTexture(Texture);
			}
		}
		else if( InCommand == IDMN_TextureContext_CopyTextureAdjustments )
		{
			UTexture* Texture = Cast<UTexture>( Object );
			if( Texture != NULL )
			{
				FString ClipboardString;

				// "TextureAdjustment <ObjectPathName>"
				// Note: This token is used to identify valid texture adjustment data in the clipboard
				ClipboardString += FString::Printf( TEXT( "TextureAdjustment %s\n" ), *Texture->GetPathName() );

				// Texture adjustment data
				ClipboardString += FString::Printf( TEXT( "AdjustBrightness %f\n" ), Texture->AdjustBrightness );
				ClipboardString += FString::Printf( TEXT( "AdjustBrightnessCurve %f\n" ), Texture->AdjustBrightnessCurve );
				ClipboardString += FString::Printf( TEXT( "AdjustSaturation %f\n" ), Texture->AdjustSaturation );
				ClipboardString += FString::Printf( TEXT( "AdjustVibrance %f\n" ), Texture->AdjustVibrance );
				ClipboardString += FString::Printf( TEXT( "AdjustRGBCurve %f\n" ), Texture->AdjustRGBCurve );
				ClipboardString += FString::Printf( TEXT( "AdjustHue %f\n" ), Texture->AdjustHue );
				ClipboardString += FString::Printf( TEXT( "MipGenSettings %s\n" ), UTexture::GetMipGenSettingsString((TextureMipGenSettings)Texture->MipGenSettings) );

				appClipboardCopy( *ClipboardString );
			}
		}
		else if( InCommand == IDMN_TextureContext_PasteTextureAdjustments )
		{
			UTexture* Texture = Cast<UTexture>( Object );
			if( Texture != NULL )
			{
				const FString ClipboardString = appClipboardPaste();

				if( ClipboardString.StartsWith( TEXT( "TextureAdjustment" ) ) )
				{
					// Split the string up by line
					TArray< FString > ClipboardLines;

					const UBOOL bCullEmpty = FALSE;
					ClipboardString.ParseIntoArray( &ClipboardLines, TEXT( "\n" ), bCullEmpty );

					{
						// @todo: Add support for undo
						// FScopedTransaction Transaction( *LocalizeUnrealEd( TEXT( "ObjectContext_PasteTextureAdjustments_Undo" ) ) );

						// Start modifying this texture.  This will dirty the asset package.
						Texture->Modify();


						// Start with the second text line since the first line is just the header text
						INT CurLineIndex = 1;

						if( ClipboardLines.Num() > CurLineIndex )
						{
							appSSCANF( *ClipboardLines( CurLineIndex++ ), TEXT( "AdjustBrightness %f" ), &Texture->AdjustBrightness );
						}

						if( ClipboardLines.Num() > CurLineIndex )
						{
							appSSCANF( *ClipboardLines( CurLineIndex++ ), TEXT( "AdjustBrightnessCurve %f" ), &Texture->AdjustBrightnessCurve );
						}

						if( ClipboardLines.Num() > CurLineIndex )
						{
							appSSCANF( *ClipboardLines( CurLineIndex++ ), TEXT( "AdjustSaturation %f" ), &Texture->AdjustSaturation );
						}

						if( ClipboardLines.Num() > CurLineIndex )
						{
							appSSCANF( *ClipboardLines( CurLineIndex++ ), TEXT( "AdjustVibrance %f" ), &Texture->AdjustVibrance );
						}

						if( ClipboardLines.Num() > CurLineIndex )
						{
							appSSCANF( *ClipboardLines( CurLineIndex++ ), TEXT( "AdjustRGBCurve %f" ), &Texture->AdjustRGBCurve );
						}

						if( ClipboardLines.Num() > CurLineIndex )
						{
							appSSCANF( *ClipboardLines( CurLineIndex++ ), TEXT( "AdjustHue %f" ), &Texture->AdjustHue );
						}

						if( ClipboardLines.Num() > CurLineIndex )
						{
							TCHAR Value[1024];
#if USE_SECURE_CRT
							appSSCANF( *ClipboardLines( CurLineIndex++ ), TEXT( "MipGenSettings %s" ), Value, ARRAY_COUNT( Value ) );
#else
							appSSCANF( *ClipboardLines( CurLineIndex++ ), TEXT( "MipGenSettings %s" ), Value );
#endif // USE_SECURE_CRT
							Texture->MipGenSettings = UTexture::GetMipGenSettingsFromString(Value, FALSE);
						}
					}

					// Call PostEditChange so the texture can re-compress itself.  This may take awhile!
					Texture->PostEditChange();
				}
			}
		}
		else if( InCommand == IDMN_TextureContext_CompressNow )
		{
			UTexture* Texture = Cast<UTexture>( Object );

			if (Texture->DeferCompression)
			{
				// Turn off defer compression and compress the texture
				Texture->DeferCompression = FALSE;

				Texture->CompressSourceArt();
				Texture->PostEditChange();		

			}
		}
		else if( InCommand == IDMN_TextureContext_Reflatten )
		{
			UTexture2D* Texture = Cast<UTexture2D>( Object );

			// Get the name of the texture and see if its an auto-flattened one
			FString TextureName = Texture->GetName();

			FString Flattened = TEXT("_Flattened");
			FString FlattenedPart  = TextureName.Right( Flattened.Len() );

			//should catch normals and diffuse flattened textures
			if ( TextureName.InStr(Flattened) != INDEX_NONE )
			{
				// Chop off the _Flattened part of the texture to get the Material name.
				FString MaterialName = TextureName.Left( TextureName.InStr( FlattenedPart ) );

				// Load the material that generated the flattened texture and reflatten it
				UMaterialInterface* Material = LoadObject<UMaterialInterface>( Texture->GetOuter(), *MaterialName, NULL, LOAD_None, NULL );
				if( Material )
				{
					ConditionalFlattenMaterial( Material, TRUE, TRUE );
					Texture->MarkPackageDirty();
				}
				else
				{
					debugf(TEXT("Material: %s not found"), *MaterialName);
				}
			}
		}
		else if (InCommand == IDMN_TextureContext_TossFlash)
		{
			UTexture2D* Texture = Cast<UTexture2D>(Object);
			if (Texture != NULL)
			{
				Texture->CachedFlashMips.RemoveBulkData();
				Texture->CachedFlashMipsMaxResolution = 0;
				Texture->MarkPackageDirty();
			}
		}
#if WITH_SUBSTANCE_AIR
		else if (InCommand == IDMN_TextureContext_ConvertToSbsImageInput)
		{
			UTexture2D* Texture = Cast<UTexture2D>(Object);
			USubstanceAirTexture2D* AirTexture = Cast<USubstanceAirTexture2D>(Object);
			if (Texture != NULL && AirTexture == NULL )
			{
				SubstanceAir::Helpers::CreateImageInput(Texture);
			}
		}
#endif

		// Update the progress report as another texture was processed
		if (bDoingSlowTask) 
		{
			GWarn->UpdateProgress(ObjIndex+1, InObjects.Num());
		}
	}

	if (bDoingSlowTask) 
	{
		GWarn->EndSlowTask();
	}
}

/*------------------------------------------------------------------------------
UGenericBrowserType_RenderTexture
------------------------------------------------------------------------------*/

void UGenericBrowserType_RenderTexture::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UTextureRenderTarget2D::StaticClass(), FColor(255,0,0), 0, this ) );
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UTextureRenderTargetCube::StaticClass(), FColor(255,0,0), 0, this ) );		
}

static void Handle_RTCreateStaticTexture( UObject* InObject )
{
	if( !InObject &&
		!(InObject->IsA(UTextureRenderTarget2D::StaticClass()) || InObject->IsA(UTextureRenderTargetCube::StaticClass())) )
	{
		return;
	}

	// create dialog for naming the new object and its package
	WxDlgPackageGroupName dlg;
	dlg.SetTitle( *LocalizeUnrealEd("CreateStaticTextureE") );
	FString Package = InObject->GetOutermost()->GetName();
	FString Group = InObject->GetOuter()->GetOuter() ? InObject->GetFullGroupName( 1 ) : TEXT("");
	FString Name = FString::Printf( TEXT("%s_Tex"), *InObject->GetName() );

	// show the dialog
	if( dlg.ShowModal( Package, Group, Name ) == wxID_OK )
	{
		FString Pkg,Reason;
		// parse the package and group name
		if( dlg.GetGroup().Len() > 0 )
		{
			Pkg = FString::Printf(TEXT("%s.%s"),*dlg.GetPackage(),*dlg.GetGroup());
		}
		else
		{
			Pkg = FString::Printf(TEXT("%s"),*dlg.GetPackage());
		}
		// check for an exsiting package
		UObject* ExistingPackage = UObject::FindPackage(NULL, *Pkg);

		// make sure package and object names were specified
		if(!dlg.GetPackage().Len() || !dlg.GetObjectName().Len())
		{
			appMsgf(AMT_OK,*LocalizeUnrealEd("Error_InvalidInput"));
		}
		// 		// check for a valid object name for the new static texture
		// 		else if( !FIsValidObjectName( *dlg.GetObjectName(), Reason ))
		// 		{
		// 			appMsgf( AMT_OK, *Reason );
		// 		}
		// check for a duplicate object
		else if (ExistingPackage && !FIsUniqueObjectName(*dlg.GetObjectName(), ExistingPackage, Reason))
		{
			appMsgf(AMT_OK, *Reason);
		}
		else
		{
			UObject* NewObj = NULL;
			UTextureRenderTarget2D* TexRT = Cast<UTextureRenderTarget2D>(InObject);
			UTextureRenderTargetCube* TexRTCube = Cast<UTextureRenderTargetCube>(InObject);
			if( TexRTCube )
			{
				// create a static cube texture as well as its 6 faces
				NewObj = TexRTCube->ConstructTextureCube( UObject::CreatePackage(NULL,*Pkg), dlg.GetObjectName(), InObject->GetMaskedFlags(~0), TRUE );
			}
			else if( TexRT )
			{
				// create a static 2d texture
				NewObj = TexRT->ConstructTexture2D( UObject::CreatePackage(NULL,*Pkg), dlg.GetObjectName(), InObject->GetMaskedFlags(~0), CTF_Default, NULL, TRUE );
			}

			if( NewObj )
			{
				// package needs saving
				NewObj->MarkPackageDirty();
			}
		}
	}
}



/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_RenderTexture::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingTextureViewer" ) ) );

	OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_RTCreateStaticTexture, *LocalizeUnrealEd( "ObjectContext_CreateStaticTexture" ), !bAnythingCooked ) );
}



void UGenericBrowserType_RenderTexture::InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects )
{
	for (INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex)
	{
		UObject* Object = InObjects(ObjIndex);
		// create a static texture from a dynamic render target texture
		if( InCommand == IDMN_ObjectContext_RTCreateStaticTexture )
		{
			Handle_RTCreateStaticTexture(Object);        
		}
	}
}

/*------------------------------------------------------------------------------
	UGenericBrowserType_TextureCube
------------------------------------------------------------------------------*/
void UGenericBrowserType_TextureCube::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UTextureCube::StaticClass(), FColor(255,0,0), 0, this ) );
}

/*------------------------------------------------------------------------------
UGenericBrowserType_Font
------------------------------------------------------------------------------*/

#include "FontPropertiesDlg.h"

/**
* Adds the font information to the support info
*/
void UGenericBrowserType_Font::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UFont::StaticClass(), FColor(255,0,0), 0, this ) );
}

/**
* Displays the font properties window for editing & importing/exporting of
* font pages
*
* @param InObject the object being edited
*/
UBOOL UGenericBrowserType_Font::ShowObjectEditor(UObject* InObject)
{
	// Cast to the font so we have access to the texture data
	UFont* Font = Cast<UFont>(InObject);
	if (Font != NULL)
	{
		WxFontPropertiesDlg* dlg = new WxFontPropertiesDlg();
		dlg->Show(1,Font);
	}
	return 1;
}


/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_Font::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingFontEditor" ) ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Reimport, *LocalizeUnrealEd( "ObjectContext_ReimportFont" ), !bAnythingCooked ) );
}

/**
 * Invokes a custom menu item command.
 *
 * @param InCommand		The command to execute
 * @param InObject		The object to invoke the command against
 */
void UGenericBrowserType_Font::InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects )
{
	for (INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex)
	{
		UObject* Object = InObjects(ObjIndex);
		if ( InCommand == IDMN_ObjectContext_Reimport )
		{
			FReimportManager::Instance()->Reimport( Object );
		}
	}
}



/*------------------------------------------------------------------------------
UGenericBrowserType_PhysicalMaterial
------------------------------------------------------------------------------*/

void UGenericBrowserType_PhysicalMaterial::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UPhysicalMaterial::StaticClass(), FColor(200,192,128), 0, this ) );
}

UBOOL UGenericBrowserType_PhysicalMaterial::ShowObjectEditor( UObject* InObject )
{
	WxPropertyWindowFrame* Properties = new WxPropertyWindowFrame;
	Properties->Create( GApp->EditorFrame, -1 );

	Properties->AllowClose();
	Properties->SetObject( InObject, EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories  );
	Properties->SetTitle( *FString::Printf( LocalizeSecure(LocalizeUnrealEd(TEXT("PhysicalMaterial_F")), *InObject->GetPathName()) ) );
	Properties->Show();

	return 1;
}

/*------------------------------------------------------------------------------
UGenericBrowserType_Archetype
------------------------------------------------------------------------------*/
void UGenericBrowserType_Archetype::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UObject::StaticClass(), FColor(255,0,0), RF_ArchetypeObject, this, UGenericBrowserType_Archetype::IsArchetypeSupported) );
}

/**
* Determines whether the specified object is an archetype that should be handled by this generic browser type.
*
* @param	Object	a pointer to a object with the RF_ArchetypeObject flag
*
* @return	TRUE if this generic browser type supports to object specified.
*/
UBOOL UGenericBrowserType_Archetype::IsArchetypeSupported( UObject* Object )
{
	return Object != NULL && Object->HasAllFlags(RF_ArchetypeObject);
}


/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_Archetype::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Properties, *LocalizeUnrealEd( "ObjectContext_EditProperties" ) ) );
}

INT UGenericBrowserType_Archetype::QueryDefaultCommand( TArray<UObject*>& InObjects ) const
{
	return IDMN_ObjectContext_Properties;
}


/*------------------------------------------------------------------------------
UGenericBrowserType_Prefab
------------------------------------------------------------------------------*/
void UGenericBrowserType_Prefab::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UPrefab::StaticClass(), FColor(255,128,128), 0, this ) );
}

UBOOL UGenericBrowserType_Prefab::ShowObjectEditor( UObject* InObject )
{
	ShowObjectProperties( InObject );
	return true;
}

/*------------------------------------------------------------------------------
UGenericBrowserType_MorphTargetSet
------------------------------------------------------------------------------*/

void UGenericBrowserType_MorphTargetSet::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UMorphTargetSet::StaticClass(), FColor(192,128,0), 0, this ) );
}

UBOOL UGenericBrowserType_MorphTargetSet::ShowObjectEditor( UObject* InObject )
{
	// open AnimSetViewer

	UMorphTargetSet* MorphSet = Cast<UMorphTargetSet>(InObject);
	if( !MorphSet )
	{
		return 0;
	}

	WxAnimSetViewer* AnimSetViewer = new WxAnimSetViewer( (wxWindow*)GApp->EditorFrame, -1, NULL, NULL, MorphSet );
	AnimSetViewer->Show(1);

	return 1;
}


/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_MorphTargetSet::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingAnimSetViewer" ) ) );
}


/*------------------------------------------------------------------------------
UGenericBrowserType_MorphWeightSequence
------------------------------------------------------------------------------*/

void UGenericBrowserType_MorphWeightSequence::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UMorphWeightSequence::StaticClass(), FColor(128,192,0), 0, this ) );
}

UBOOL UGenericBrowserType_MorphWeightSequence::ShowObjectEditor( UObject* InObject )
{
	// Do nothing...
	return true;
}

/*------------------------------------------------------------------------------
UGenericBrowserType_CurveEdPresetCurve
------------------------------------------------------------------------------*/

void UGenericBrowserType_CurveEdPresetCurve::Init()
{
	SupportInfo.AddItem(FGenericBrowserTypeInfo(UCurveEdPresetCurve::StaticClass(), FColor(200,128,128), 0, this));
}

/*------------------------------------------------------------------------------
UGenericBrowserType_FaceFXAsset
------------------------------------------------------------------------------*/

void UGenericBrowserType_FaceFXAsset::Init()
{
	SupportInfo.AddItem(FGenericBrowserTypeInfo(UFaceFXAsset::StaticClass(), FColor(200,128,128), 0, this));
}

#if WITH_FACEFX_STUDIO
extern OC3Ent::Face::FxStudioMainWin* GFaceFXStudio;
#endif // WITH_FACEFX_STUDIO

UBOOL UGenericBrowserType_FaceFXAsset::ShowObjectEditor( UObject* InObject )
{
#if WITH_FACEFX_STUDIO
	UFaceFXAsset* FaceFXAsset = Cast<UFaceFXAsset>(InObject);
	if( FaceFXAsset )
	{
		//@todo Replace these strings with localized strings at some point.
		USkeletalMesh* SkelMesh = FaceFXAsset->DefaultSkelMesh;
		OC3Ent::Face::FxActor* Actor = FaceFXAsset->GetFxActor();
		if( SkelMesh && Actor )
		{
			OC3Ent::Face::FxBool bShouldSetActor = FxTrue;
			OC3Ent::Face::FxStudioMainWin* FaceFXStudio = 
				static_cast<OC3Ent::Face::FxStudioMainWin*>(OC3Ent::Face::FxStudioApp::GetMainWindow());
			// This is only here to force the FaceFX Studio libraries to link.
			GFaceFXStudio = FaceFXStudio;
			if( !FaceFXStudio )
			{
				OC3Ent::Face::FxStudioApp::CheckForSafeMode();
				FaceFXStudio = new OC3Ent::Face::FxStudioMainWin(FxFalse, GApp->EditorFrame);
				OC3Ent::Face::FxStudioApp::SetMainWindow(FaceFXStudio);
				FaceFXStudio->Show();
				FaceFXStudio->Layout();
				FaceFXStudio->LoadOptions();
			}
			else
			{
				if( wxNO == wxMessageBox(wxString(TEXT("FaceFX Studio is already open and only one window may be open at once.  Do you want to use the existing window?")), 
					wxString(TEXT("FaceFX")), wxYES_NO) )
				{
					bShouldSetActor = FxFalse;
				}
			}
			if( FaceFXStudio && bShouldSetActor )
			{
				if( FaceFXStudio->GetSession().SetActor(Actor, FaceFXAsset) )
				{
					OC3Ent::Face::FxRenderWidgetUE3* RenderWidgetUE3 = static_cast<OC3Ent::Face::FxRenderWidgetUE3*>(FaceFXStudio->GetRenderWidget());
					if( RenderWidgetUE3 )
					{
						RenderWidgetUE3->SetSkeletalMesh(SkelMesh);
					}
				}
				FaceFXStudio->SetFocus();
			}
			return 1;
		}
		else
		{
			wxMessageBox(wxString(TEXT("The FaceFX Asset does not reference a Skeletal Mesh.  Use the AnimSet Viewer to set a Skeletal Mesh's FaceFXAsset property.")), wxString(TEXT("FaceFX")));
		}
	}
#endif // WITH_FACEFX_STUDIO
	return 0;
}

UBOOL UGenericBrowserType_FaceFXAsset::ShowObjectProperties( UObject* InObject )
{
#if WITH_FACEFX_STUDIO
	WxPropertyWindowFrame* ObjectPropertyWindow = NULL;
	OC3Ent::Face::FxStudioMainWin* FaceFXStudio = 
		static_cast<OC3Ent::Face::FxStudioMainWin*>(OC3Ent::Face::FxStudioApp::GetMainWindow());
	if( FaceFXStudio )
	{
		OC3Ent::Face::FxRenderWidgetUE3* RenderWidgetUE3 = static_cast<OC3Ent::Face::FxRenderWidgetUE3*>(FaceFXStudio->GetRenderWidget());
		if( RenderWidgetUE3 )
		{
			ObjectPropertyWindow = new WxPropertyWindowFrame;
			ObjectPropertyWindow->Create(GApp->EditorFrame, -1, RenderWidgetUE3);
		}
	}
	else
	{
		ObjectPropertyWindow = new WxPropertyWindowFrame;
		ObjectPropertyWindow->Create(GApp->EditorFrame, -1, GUnrealEd);
	}
#else
	WxPropertyWindowFrame* ObjectPropertyWindow = new WxPropertyWindowFrame;
	ObjectPropertyWindow->Create(GApp->EditorFrame, -1, GUnrealEd);
#endif // WITH_FACEFX_STUDIO
	if( ObjectPropertyWindow )
	{
		ObjectPropertyWindow->SetSize(64,64, 350,600);
		ObjectPropertyWindow->SetObject(InObject, EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories );
		ObjectPropertyWindow->Show();
	}
	return TRUE;
}

UBOOL UGenericBrowserType_FaceFXAsset::ShowObjectProperties( const TArray<UObject*>& InObjects )
{
#if WITH_FACEFX_STUDIO
	WxPropertyWindowFrame* ObjectPropertyWindow = NULL;
	OC3Ent::Face::FxStudioMainWin* FaceFXStudio = 
		static_cast<OC3Ent::Face::FxStudioMainWin*>(OC3Ent::Face::FxStudioApp::GetMainWindow());
	if( FaceFXStudio )
	{
		OC3Ent::Face::FxRenderWidgetUE3* RenderWidgetUE3 = static_cast<OC3Ent::Face::FxRenderWidgetUE3*>(FaceFXStudio->GetRenderWidget());
		if( RenderWidgetUE3 )
		{
			ObjectPropertyWindow = new WxPropertyWindowFrame;
			ObjectPropertyWindow->Create(GApp->EditorFrame, -1, RenderWidgetUE3);
		}
	}
	else
	{
		ObjectPropertyWindow = new WxPropertyWindowFrame;
		ObjectPropertyWindow->Create(GApp->EditorFrame, -1, GUnrealEd);
	}
#else
	WxPropertyWindowFrame* ObjectPropertyWindow = new WxPropertyWindowFrame;
	ObjectPropertyWindow->Create(GApp->EditorFrame, -1, GUnrealEd);
#endif // WITH_FACEFX_STUDIO
	if( ObjectPropertyWindow )
	{
		ObjectPropertyWindow->SetSize(64,64, 350,600);
		ObjectPropertyWindow->SetObjectArray(InObjects, EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories );
		ObjectPropertyWindow->Show();
	}
	return TRUE;
}



/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_FaceFXAsset::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingFaceFXStudio" ) ) );

	OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Properties, *LocalizeUnrealEd( "ObjectContext_EditProperties" ) ) );

	OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CreateNewFaceFXAnimSet, *LocalizeUnrealEd( "ObjectContext_CreateFaceFXAnimSet" ), !bAnythingCooked ) );

	OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_ImportFaceFXAsset, *LocalizeUnrealEd( "ObjectContext_ImportFaceFXAssetFromFXA" ), !bAnythingCooked ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_ExportFaceFXAsset, *LocalizeUnrealEd( "ObjectContext_ExportFaceFXAssetToFXA" ), !bAnythingCooked ) );
}



void UGenericBrowserType_FaceFXAsset::InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects )
{
#if WITH_FACEFX
	for (INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex)
	{
		UObject* Object = InObjects(ObjIndex);
		UFaceFXAsset* FaceFXAsset = Cast<UFaceFXAsset>(Object);
		if( FaceFXAsset )
		{
			OC3Ent::Face::FxActor* Actor = FaceFXAsset->GetFxActor();
			if( Actor )
			{
				if( Actor->IsOpenInStudio() )
				{
					//@todo Replace these strings with localized strings at some point.
					wxMessageBox(wxString(TEXT("The FaceFX Asset is currently open in FaceFX Studio.  Please close FaceFX Studio and try again.")), wxString(TEXT("FaceFX")));
				}
				else
				{
					if( InCommand == IDMN_ObjectContext_CreateNewFaceFXAnimSet )
					{
						UFaceFXAsset* FaceFXAsset = Cast<UFaceFXAsset>(Object);
						if( FaceFXAsset )
						{
							// Create a new FaceFXAnimSet from the selected FaceFXAsset. 
							// Defaults to being in the same package/group as the FaceFXAsset.
							FString DefaultPackage = *FaceFXAsset->GetOutermost()->GetName();
							FString DefaultGroup = FaceFXAsset->GetOuter()->GetOuter() ? FaceFXAsset->GetFullGroupName(TRUE) : TEXT("");

							// Make default name by appending '_AnimSet' to the FaceFXAsset name.
							FString DefaultAnimSetName = FString(FString::Printf( TEXT("%s_AnimSet"), *FaceFXAsset->GetName()));

							// First of all show the dialog for choosing a new package file, group and asset name:
							WxDlgPackageGroupName PackageDlg;
							if( PackageDlg.ShowModal(DefaultPackage, DefaultGroup, DefaultAnimSetName) == wxID_OK )
							{
								// Get the full name of where we want to create the FaceFXAnimSet.
								FString PackageName;
								if( PackageDlg.GetGroup().Len() > 0 )
								{
									PackageName = PackageDlg.GetPackage() + TEXT(".") + PackageDlg.GetGroup();
								}
								else
								{
									PackageName = PackageDlg.GetPackage();
								}

								// Then find/create it.
								UPackage* Package = UObject::CreatePackage(NULL, *PackageName);
								check(Package);

								FString ExistingAnimSetPath = PackageName;
								ExistingAnimSetPath += ".";
								ExistingAnimSetPath += *PackageDlg.GetObjectName();
								UFaceFXAnimSet* ExistingAnimSet = LoadObject<UFaceFXAnimSet>(NULL, *ExistingAnimSetPath, NULL, LOAD_NoWarn, NULL);
								if( ExistingAnimSet )
								{
									//@todo Localize this string.
									wxMessageBox(wxString(TEXT("That FaceFXAnimSet already exists!  Please remove the existing one first.")), wxString(TEXT("FaceFX")));
									return;
								}

								UFaceFXAnimSet* NewAnimSet = ConstructObject<UFaceFXAnimSet>(UFaceFXAnimSet::StaticClass(), Package, *PackageDlg.GetObjectName(), RF_Public|RF_Standalone|RF_Transactional);
								if( NewAnimSet )
								{
									NewAnimSet->CreateFxAnimSet(FaceFXAsset);
									NewAnimSet->MarkPackageDirty();
									GCallbackEvent->Send( FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, CBR_ObjectCreated|CBR_SyncAssetView, FaceFXAsset ) );
								}
							}
						}
					}
					else if( InCommand == IDMN_ObjectContext_ImportFaceFXAsset )
					{
						WxFileDialog OpenFileDialog(GApp->EditorFrame, 
							*LocalizeUnrealEd("Import"),
							TEXT( "" ),
							TEXT(""),
							wxT("FaceFX Actor (*.fxa)|*.fxa|All Files (*.*)|*.*"),
							wxOPEN|wxFILE_MUST_EXIST);

						if( OpenFileDialog.ShowModal() == wxID_OK )
						{
							// Import the actor.
							OC3Ent::Face::FxString ActorFilename(OpenFileDialog.GetPath().mb_str(wxConvLibc));
							if( OC3Ent::Face::FxLoadActorFromFile(*Actor, ActorFilename.GetData(), FxTrue) )
							{
								// Update the RawFaceFXActorBytes.
								OC3Ent::Face::FxByte* ActorMemory = NULL;
								OC3Ent::Face::FxSize  NumActorMemoryBytes = 0;
								if( !OC3Ent::Face::FxSaveActorToMemory(*Actor, ActorMemory, NumActorMemoryBytes) )
								{
									warnf(TEXT("FaceFX: Failed to save actor for %s"), *(FaceFXAsset->GetPathName()));
								}
								else
								{
									FaceFXAsset->RawFaceFXActorBytes.Empty();
									FaceFXAsset->RawFaceFXActorBytes.Add(NumActorMemoryBytes);
									appMemcpy(FaceFXAsset->RawFaceFXActorBytes.GetData(), ActorMemory, NumActorMemoryBytes);
									OC3Ent::Face::FxFree(ActorMemory, NumActorMemoryBytes);
									FaceFXAsset->ReferencedSoundCues.Empty();
									FaceFXAsset->FixupReferencedSoundCues();
									Actor->SetShouldClientRelink(FxTrue);
									// The package has changed so mark it dirty.
									FaceFXAsset->MarkPackageDirty();
								}
							}
							else
							{
								warnf(TEXT("FaceFX: Failed to import %s for %s"), ANSI_TO_TCHAR(ActorFilename.GetData()), *(FaceFXAsset->GetPathName()));
							}
						}
					}
					else if( InCommand == IDMN_ObjectContext_ExportFaceFXAsset )
					{
						WxFileDialog SaveFileDialog(GApp->EditorFrame, 
							*FString::Printf(LocalizeSecure(LocalizeUnrealEd("Save_F"), *FaceFXAsset->GetName())),
							TEXT( "" ),
							*FaceFXAsset->GetName(),
							wxT("FaceFX Actor (*.fxa)|*.fxa|All Files (*.*)|*.*"),
							wxSAVE|wxOVERWRITE_PROMPT);

						if( SaveFileDialog.ShowModal() == wxID_OK )
						{
							// Save the actor.
							OC3Ent::Face::FxString ActorFilename(SaveFileDialog.GetPath().mb_str(wxConvLibc));
							if( !OC3Ent::Face::FxSaveActorToFile(*Actor, ActorFilename.GetData()) )
							{
								warnf(TEXT("FaceFX: Failed to export %s for %s"), ANSI_TO_TCHAR(ActorFilename.GetData()), *(FaceFXAsset->GetPathName()));
							}
						}
					}
				}
			}
		}
	}
#endif // WITH_FACEFX
}

/*------------------------------------------------------------------------------
UGenericBrowserType_FaceFXAnimSet
------------------------------------------------------------------------------*/

void UGenericBrowserType_FaceFXAnimSet::Init()
{
	SupportInfo.AddItem(FGenericBrowserTypeInfo(UFaceFXAnimSet::StaticClass(), FColor(200,128,128), 0, this));
}

UBOOL UGenericBrowserType_FaceFXAnimSet::ShowObjectEditor( UObject* InObject )
{
#if WITH_FACEFX_STUDIO
	UFaceFXAnimSet* FaceFXAnimSet = Cast<UFaceFXAnimSet>(InObject);
	if( FaceFXAnimSet )
	{
		UFaceFXAsset* FaceFXAsset = FaceFXAnimSet->DefaultFaceFXAsset;
		if( FaceFXAsset )
		{
			//@todo Replace these strings with localized strings at some point.
			USkeletalMesh* SkelMesh = FaceFXAsset->DefaultSkelMesh;
			OC3Ent::Face::FxActor* Actor = FaceFXAsset->GetFxActor();
			if( SkelMesh && Actor )
			{
				OC3Ent::Face::FxBool bShouldSetActor = FxTrue;
				OC3Ent::Face::FxStudioMainWin* FaceFXStudio = 
					static_cast<OC3Ent::Face::FxStudioMainWin*>(OC3Ent::Face::FxStudioApp::GetMainWindow());
				// This is only here to force the FaceFX Studio libraries to link.
				GFaceFXStudio = FaceFXStudio;
				if( !FaceFXStudio )
				{
					OC3Ent::Face::FxStudioApp::CheckForSafeMode();
					FaceFXStudio = new OC3Ent::Face::FxStudioMainWin(FxFalse, GApp->EditorFrame);
					OC3Ent::Face::FxStudioApp::SetMainWindow(FaceFXStudio);
					FaceFXStudio->Show();
					FaceFXStudio->Layout();
					FaceFXStudio->LoadOptions();
				}
				else
				{
					if( wxNO == wxMessageBox(wxString(TEXT("FaceFX Studio is already open and only one window may be open at once.  Do you want to use the existing window?")), 
						wxString(TEXT("FaceFX")), wxYES_NO) )
					{
						bShouldSetActor = FxFalse;
					}
				}
				if( FaceFXStudio && bShouldSetActor )
				{
					if( FaceFXStudio->GetSession().SetActor(Actor, FaceFXAsset, FaceFXAnimSet) )
					{
						OC3Ent::Face::FxRenderWidgetUE3* RenderWidgetUE3 = static_cast<OC3Ent::Face::FxRenderWidgetUE3*>(FaceFXStudio->GetRenderWidget());
						if( RenderWidgetUE3 )
						{
							RenderWidgetUE3->SetSkeletalMesh(SkelMesh);
						}
					}
					FaceFXStudio->SetFocus();
				}
				return 1;
			}
			else
			{
				wxMessageBox(wxString(TEXT("The FaceFX Asset does not reference a Skeletal Mesh.  Use the AnimSet Viewer to set a Skeletal Mesh's FaceFXAsset property.")), wxString(TEXT("FaceFX")));
			}
		}
		else
		{
			wxMessageBox(wxString(TEXT("The FaceFX AnimSet does not reference a FaceFX Asset.  Right click the FaceFX AnimSet and use the property editor to set the FaceFXAsset property.")), wxString(TEXT("FaceFX")));
		}
	}
#endif // WITH_FACEFX_STUDIO
	return 0;
}



/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_FaceFXAnimSet::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingFaceFXStudio" ) ) );

	OutCommands.AddItem( FObjectSupportedCommandType( INDEX_NONE, "" ) );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Properties, *LocalizeUnrealEd( "ObjectContext_EditProperties" ) ) );
}


void UGenericBrowserType_FaceFXAnimSet::InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects )
{
#if WITH_FACEFX
	for (INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex)
	{
		UObject* Object = InObjects(ObjIndex);
		UFaceFXAnimSet* FaceFXAnimSet = Cast<UFaceFXAnimSet>(Object);
		if( FaceFXAnimSet )
		{
			//@todo Implement this!
		}
	}
#endif // WITH_FACEFX
}

/*------------------------------------------------------------------------------
UGenericBrowserType_CameraAnim
------------------------------------------------------------------------------*/

void UGenericBrowserType_CameraAnim::Init()
{
	SupportInfo.AddItem(FGenericBrowserTypeInfo(UCameraAnim::StaticClass(), FColor(200,128,128), 0, this));
}

UBOOL UGenericBrowserType_CameraAnim::ShowObjectEditor( UObject* InObject )
{
	UCameraAnim* InCamAnim = Cast<UCameraAnim>(InObject);

	if (InCamAnim)
	{
		// get a Kismet window
		WxKismet::OpenKismet(NULL, FALSE, (wxWindow*)GApp->EditorFrame);
		WxKismet* KismetWindow = GApp->KismetWindows.Last();
		// @todo: minimize/hide this temp window?  might not be a temp window, though.

		// construct a temporary SeqAct_Interp object.
		USeqAct_Interp* TempInterp = (USeqAct_Interp*)KismetWindow->NewSequenceObject(USeqAct_Interp::StaticClass(), 0, 0, TRUE);
		TempInterp->SetFlags(RF_Transient);
		TempInterp->ObjComment = TEXT("TEMP FOR CAMERAANIM EDITING, DO NOT EDIT BY HAND");

		// @todo, find a way to mark TempInterp and attached InterpData so they don't 
		// get drawn int he window and cannot be edited by hand

		// changed the actor type, but don't want to lose any properties from previous
		// so duplicate from old, but with new class
		if (!InCamAnim->CameraInterpGroup->IsA(UInterpGroupCamera::StaticClass()))
		{
			InCamAnim->CameraInterpGroup = CastChecked<UInterpGroupCamera>(StaticDuplicateObject(InCamAnim->CameraInterpGroup, InCamAnim->CameraInterpGroup, InCamAnim, TEXT(""), 0, UInterpGroupCamera::StaticClass()));
		}

		UInterpGroupCamera* NewInterpGroup = InCamAnim->CameraInterpGroup;
		check(NewInterpGroup);

		// attach temp interpgroup to the interpdata object
		UInterpData* InterpData = TempInterp->FindInterpDataFromVariable();
		if (InterpData)
		{
			InterpData->SetFlags(RF_Transient);
			InterpData->InterpLength = InCamAnim->AnimLength;

			if (NewInterpGroup)
			{
				InterpData->InterpGroups.AddItem(NewInterpGroup);
			}
		}

		// create a CameraActor and connect it to the Interp.  will create this at the perspective viewport's location and rotation
		ACameraActor* TempCameraActor = NULL;
		{
			FVector ViewportCamLocation(0.f);
			FRotator ViewportCamRotation;
			{
				for(INT i=0; i<GApp->EditorFrame->ViewportConfigData->GetViewportCount(); i++)
				{
					// find perspective window and note the camera location
					const WxLevelViewportWindow* LevelVC = GApp->EditorFrame->ViewportConfigData->GetViewport(i).ViewportWindow;
					if(LevelVC->ViewportType == LVT_Perspective)
					{
						ViewportCamLocation = LevelVC->ViewLocation;
						ViewportCamRotation = LevelVC->ViewRotation;
						break;
					}
				}
			}

			TempCameraActor = (ACameraActor*)GWorld->SpawnActor( ACameraActor::StaticClass(), NAME_None, ViewportCamLocation, ViewportCamRotation, NULL, TRUE );
			check(TempCameraActor);

			// copy data from the CamAnim to the CameraActor
			TempCameraActor->FOVAngle = InCamAnim->BaseFOV;
			TempCameraActor->CamOverridePostProcess = InCamAnim->BasePPSettings;
			TempCameraActor->CamOverridePostProcessAlpha = InCamAnim->BasePPSettingsAlpha;
		}

		// set up the group actor
		TempInterp->InitGroupActorForGroup(NewInterpGroup, TempCameraActor);

		// this will create the instances for everything
		TempInterp->InitInterp();

		// open up the matinee window in camera anim mode
		{
			// If already in Matinee mode, exit out before going back in with new Interpolation.
			if( GEditorModeTools().IsModeActive( EM_InterpEdit ) )
			{
				GEditorModeTools().ActivateMode( EM_Default );
			}

			GEditorModeTools().ActivateMode( EM_InterpEdit );

			FEdModeInterpEdit* InterpEditMode = (FEdModeInterpEdit*)GEditorModeTools().GetActiveMode( EM_InterpEdit );

			WxCameraAnimEd* const CamAnimEd = InterpEditMode->InitCameraAnimMode( TempInterp );

			// start out looking through the camera
			CamAnimEd->LockCamToGroup(NewInterpGroup);
		}

		// all good
		return TRUE;
	}

	return Super::ShowObjectEditor(InObject);
}

/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_CameraAnim::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingMatinee" ) ) );
}


/*------------------------------------------------------------------------------
UGenericBrowserType_ProcBuildingRuleset
------------------------------------------------------------------------------*/

void UGenericBrowserType_ProcBuildingRuleset::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UProcBuildingRuleset::StaticClass(), FColor(222,255,135), 0, this ) );
}

UBOOL UGenericBrowserType_ProcBuildingRuleset::ShowObjectEditor( UObject* InObject )
{

	UProcBuildingRuleset* Ruleset = Cast<UProcBuildingRuleset>(InObject);
	if(Ruleset)
	{
		if(Ruleset->bBeingEdited)
		{
			// If being edited, find the window and restore it/put to front
			TArray<FTrackableEntry> TrackableWindows;
			WxTrackableWindow::GetTrackableWindows( TrackableWindows );
			for(INT WinIdx=0; WinIdx<TrackableWindows.Num(); WinIdx++)
			{
				wxWindow* Window = TrackableWindows(WinIdx).Window;
				WxFacade* Facade = wxDynamicCast(Window, WxFacade);
				if(Facade && Facade->Ruleset == Ruleset)
				{
					Facade->Raise();
					Facade->Maximize(false);
				}
			}
		}
		else
		{
			WxFacade* Facade = new WxFacade( (wxWindow*)GApp->EditorFrame, -1, Ruleset );
			Facade->Show(1);

			return 1;
		}
	}

	return 0;
}

void UGenericBrowserType_ProcBuildingRuleset::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingRulesetEditor" ) ) );
}

/*------------------------------------------------------------------------------
UGenericBrowserType_ApexDestructibleAsset
------------------------------------------------------------------------------*/
void UGenericBrowserType_ApexDestructibleAsset::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UApexDestructibleAsset::StaticClass(), FColor(255,192,128), 0, this ) );
}
void UGenericBrowserType_ApexDestructibleAsset::QuerySupportedCommands( class USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
#if WITH_APEX
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Reimport, *LocalizeUnrealEd( "ObjectContext_ReimportApexAsset" ), !bAnythingCooked ));
#endif
}

void UGenericBrowserType_ApexDestructibleAsset::InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects )
{
#if WITH_APEX
	for (INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex)
	{
		UObject* InObject = InObjects(ObjIndex);
		if (InCommand == IDMN_ObjectContext_Reimport)
		{
			if (!FReimportManager::Instance()->Reimport(InObject))
			{
				appMsgf(AMT_OK, TEXT("Error reimporting ApexAsset \"%s\"."), *Cast<UApexAsset>(InObject)->SourceFilePath);
			}
		}
	}
#endif
}
void UGenericBrowserType_ApexClothingAsset::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UApexClothingAsset::StaticClass(), FColor(255,192,128), 0, this ) );
}
void UGenericBrowserType_ApexClothingAsset::QuerySupportedCommands( class USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
#if WITH_APEX
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Reimport, *LocalizeUnrealEd( "ObjectContext_ReimportApexAsset" ), !bAnythingCooked ));
#endif
}

void UGenericBrowserType_ApexClothingAsset::InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects )
{
#if WITH_APEX
	for (INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex)
	{
		UObject* InObject = InObjects(ObjIndex);
		if (InCommand == IDMN_ObjectContext_Reimport)
		{
			if (!FReimportManager::Instance()->Reimport(InObject))
			{
				appMsgf(AMT_OK, TEXT("Error reimporting ApexAsset \"%s\"."), *Cast<UApexAsset>(InObject)->SourceFilePath);
			}
		}
	}
#endif
}
void UGenericBrowserType_ApexGenericAsset::Init()
{
#if WITH_APEX_PARTICLES
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UApexGenericAsset::StaticClass(), FColor(192,128,255), 0, this) );
#endif
}

UBOOL UGenericBrowserType_ApexGenericAsset::ShowObjectEditor( UObject* InObject)
{
	UBOOL ret = TRUE;
#if WITH_APEX_PARTICLES
	if ( InObject )
	{
		UApexGenericAsset *asset = static_cast< UApexGenericAsset *>(InObject);
		CreateApexEditorWidget(asset);
	}
#endif
	return ret;
}

/**
* Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
*
* @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
* @param	OutCommands		The list of custom commands to support
*/
void UGenericBrowserType_ApexGenericAsset::QuerySupportedCommands( USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const
{
#if WITH_APEX_PARTICLES
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Properties, *LocalizeUnrealEd( "ObjectContext_EditProperties" ) ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Editor, *LocalizeUnrealEd( "ObjectContext_EditUsingApexEditorWidgets" ) ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CreateNewApexEmitter, *LocalizeUnrealEd( "ObjectContext_CreateNewApexEmitter" ) ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CreateNewGroundEmitter, *LocalizeUnrealEd( "ObjectContext_CreateNewGroundEmitter" ) ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CreateNewImpactEmitter, *LocalizeUnrealEd( "ObjectContext_CreateNewImpactEmitter" ) ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CreateNewBasicIos, *LocalizeUnrealEd( "ObjectContext_CreateNewBasicIos" ) ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CreateNewFluidIos, *LocalizeUnrealEd( "ObjectContext_CreateNewFluidIos" ) ) );
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_CreateNewIofx, *LocalizeUnrealEd( "ObjectContext_CreateNewIofx" ) ) );
#else
	OutCommands.AddItem( FObjectSupportedCommandType( IDMN_ObjectContext_Properties, *LocalizeUnrealEd( "ObjectContext_EditProperties" ) ) );
#endif
}

void UGenericBrowserType_ApexGenericAsset::InvokeCustomCommand( INT InCommand)
{

}


void UGenericBrowserType_ApexGenericAsset::InvokeCustomCommand( INT InCommand, TArray<UObject*>&InObjects )
{
#if WITH_APEX_PARTICLES
	INT type = 0;

	switch ( InCommand )
	{
		case IDMN_ObjectContext_CreateNewApexEmitter:
			type = AAT_APEX_EMITTER;
			break;
		case IDMN_ObjectContext_CreateNewGroundEmitter:
			type = AAT_GROUND_EMITTER;
			break;
		case IDMN_ObjectContext_CreateNewImpactEmitter:
			type = AAT_IMPACT_EMITTER;
			break;
		case IDMN_ObjectContext_CreateNewBasicIos:
			type = AAT_BASIC_IOS;
			break;
		case IDMN_ObjectContext_CreateNewFluidIos:
			type = AAT_FLUID_IOS;
			break;
		case IDMN_ObjectContext_CreateNewIofx:
			type = AAT_IOFX;
			break;
	}

	for (INT ObjIndex = 0; ObjIndex < InObjects.Num(); ++ObjIndex)
	{
		UObject* Object = InObjects(ObjIndex);
		if ( type != 0  )
		{
			UApexGenericAsset* apexGenericAsset = Cast<UApexGenericAsset>(Object);
			if( apexGenericAsset )
			{
				// Create a new AnimSet asset from the selected skeletal mesh. 
				// Defaults to being in the same package/group as the skeletal mesh.
				FString DefaultPackage = apexGenericAsset->GetOutermost()->GetName();
				FString DefaultGroup = apexGenericAsset->GetOuter()->GetOuter() ? apexGenericAsset->GetFullGroupName(TRUE) : TEXT("");

				// Make default name by appending '_Anims' to the SkeletalMesh name.
				FString DefaultAssetName;
				for (INT i=1; i<100; i++)
				{
					DefaultAssetName = FString( FString::Printf( TEXT("%s_%02d"), *apexGenericAsset->GetName(), i ) );
					FName findName(*DefaultAssetName);
					UObject *FindObject = StaticFindObjectFast( UApexGenericAsset::StaticClass(), apexGenericAsset->GetOuter(), findName, FALSE, FALSE );
					if ( FindObject == NULL )
						break;
				}

				// First of all show the dialog for choosing a new package file, group and asset name:
				WxDlgPackageGroupName PackageDlg;
				if( PackageDlg.ShowModal(DefaultPackage, DefaultGroup, DefaultAssetName) == wxID_OK )
				{
					// Get the full name of where we want to create the AnimSet asset.
					FString PackageName;
					if(PackageDlg.GetGroup().Len() > 0)
					{
						PackageName = PackageDlg.GetPackage() + TEXT(".") + PackageDlg.GetGroup();
					}
					else
					{
						PackageName = PackageDlg.GetPackage();
					}

					// Then find/create it.
					UPackage* Package = UObject::CreatePackage(NULL, *PackageName);
					check(Package);

					// Create the new AnimSet in the user-selected package
					UApexGenericAsset* NewAsset = ConstructObject<UApexGenericAsset>( UApexGenericAsset::StaticClass(), Package, *PackageDlg.GetObjectName(), RF_Public|RF_Standalone );
					if ( NewAsset )
					{
						// Update the content browser to show the new asset
						NewAsset->MarkPackageDirty();
						NewAsset->CreateDefaultAssetType(type,NULL);
						GCallbackEvent->Send(FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_ObjectCreated, NewAsset ) );
						// Open the AnimSet viewer with the new AnimSet and the selected mesh
						CreateApexEditorWidget(NewAsset);
					}
					else
					{
						appMsgf( AMT_OK, *LocalizeUnrealEd("Error_FailedToCreateNewApexAsset") );
					}
				}
			}
		}
	}
#endif
}

void UGenericBrowserType_ApexGenericAsset::DoubleClick()
{

}

void UGenericBrowserType_ApexGenericAsset::DoubleClick( UObject* InObject )
{

}

INT UGenericBrowserType_ApexGenericAsset::QueryDefaultCommand( TArray<UObject*>& InObjects ) const
{
#if WITH_APEX_PARTICLES
	return IDMN_ObjectContext_Editor;
#else
	return IDMN_ObjectContext_Properties;
#endif
}


/*------------------------------------------------------------------------------
UGenericBrowserType_FractureMaterial
------------------------------------------------------------------------------*/
void UGenericBrowserType_FractureMaterial::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UFractureMaterial::StaticClass(), FColor(255,192,128), 0, this ) );
}

/*------------------------------------------------------------------------------
UGenericBrowserType_TemplateMapMetadata
------------------------------------------------------------------------------*/
void UGenericBrowserType_TemplateMapMetadata::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UTemplateMapMetadata::StaticClass(), FColor(128,255,128), 0, this ) );
}

/*------------------------------------------------------------------------------
UGenericBrowserType_LandscapeLayer
------------------------------------------------------------------------------*/

void UGenericBrowserType_LandscapeLayer::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( ULandscapeLayerInfoObject::StaticClass(), FColor(128,192,255), 0, this ) );
}

/*------------------------------------------------------------------------------
UGenericBrowserType_InstancedFoliageSettings
------------------------------------------------------------------------------*/

void UGenericBrowserType_InstancedFoliageSettings::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UInstancedFoliageSettings::StaticClass(), FColor(128,192,255), 0, this ) );
}

