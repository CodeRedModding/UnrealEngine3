/*=============================================================================
	AnimSetViewerMain.cpp: AnimSet viewer main
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"
#include "EngineAnimClasses.h"
#include "AnimSetViewer.h"

IMPLEMENT_CLASS(AASVActor);


FASVViewportClient::FASVViewportClient( WxAnimSetViewer* InASV )
{
	AnimSetViewer = InASV;
	
	Level = AnimSetViewer->PreviewLevel;

	SkyLightComponent = ConstructObject<USkyLightComponent>(USkyLightComponent::StaticClass());
	SkyLightComponent->Scene = Level;
	SkyLightComponent->Brightness = 0.25f;
	SkyLightComponent->SetParentToWorld( FMatrix::Identity );
	SkyLightComponent->Created();

	DirectionalLightComponent = ConstructObject<UDirectionalLightComponent>(UDirectionalLightComponent::StaticClass());
	DirectionalLightComponent->Scene = Level;
	DirectionalLightComponent->Brightness = 1.0f;
	DirectionalLightComponent->SetParentToWorld( FRotationMatrix(FRotator(-8192,8192,0)) );
	DirectionalLightComponent->Created();

	EditorComponent = ConstructObject<UEditorComponent>(UEditorComponent::StaticClass());
	EditorComponent->Scene = Level;
	EditorComponent->bDrawPivot = false;
	EditorComponent->bDrawWorldBox = false;
	EditorComponent->GridColorHi = FColor(80,80,80);
	EditorComponent->GridColorLo = FColor(72,72,72);
	EditorComponent->PerspectiveGridSize = 32767;
	EditorComponent->Created();

	ShowFlags = SHOW_DefaultEditor;

	bShowBoneNames = false;
	bShowFloor = false;

	Realtime = true;
}

FASVViewportClient::~FASVViewportClient()
{
	// Destroy SkyLightComponent
	if(SkyLightComponent->Initialized)
	{
		SkyLightComponent->Destroyed();
	}

	delete SkyLightComponent;
	SkyLightComponent = NULL;

	// Destroy DirectionalLightComponent
	if(DirectionalLightComponent->Initialized)
	{
		DirectionalLightComponent->Destroyed();
	}

	delete DirectionalLightComponent;
	DirectionalLightComponent = NULL;

	// Destroy EditorComponent
	if(EditorComponent->Initialized)
	{
		EditorComponent->Destroyed();
	}

	delete EditorComponent;
	EditorComponent = NULL;
}

void FASVViewportClient::Serialize(FArchive& Ar)
{ 
	Ar << DirectionalLightComponent << SkyLightComponent << EditorComponent << Input; 
}

FColor FASVViewportClient::GetBackgroundColor()
{
	return FColor(64,64,64);
}

void FASVViewportClient::Draw(FChildViewport* Viewport, FRenderInterface* RI)
{
	if(bShowFloor)
		AnimSetViewer->PreviewFloor->bHiddenEd = false;
	else
		AnimSetViewer->PreviewFloor->bHiddenEd = true;

	// Do main viewport drawing stuff.
	FEditorLevelViewportClient::Draw(Viewport, RI);

	UBOOL bHitTesting = RI->IsHitTesting();

	INT XL, YL;
	RI->StringSize( GEngine->SmallFont,  XL, YL, TEXT("L") );

	// TODO: Add top information line writing code here.

	FSceneView	View;
	CalcSceneView(&View);
	USkeletalMeshComponent* MeshComp = AnimSetViewer->PreviewSkelComp;

	if(bShowBoneNames)
	{
		for(INT i=0; i< MeshComp->SpaceBases.Num(); i++)
		{
			FVector BonePos = MeshComp->LocalToWorld.TransformFVector( MeshComp->SpaceBases(i).GetOrigin() );

			FPlane proj = View.Project( BonePos );
			if(proj.W > 0.f)
			{
				INT HalfX = Viewport->GetSizeX()/2;
				INT HalfY = Viewport->GetSizeY()/2;
				INT XPos = HalfX + ( HalfX * proj.X );
				INT YPos = HalfY + ( HalfY * (proj.Y * -1) );

				FName BoneName = MeshComp->SkeletalMesh->RefSkeleton(i).Name;
				FString BoneString = FString::Printf( TEXT("%d: %s"), i, *BoneName );

				RI->DrawString(XPos, YPos, *BoneString, GEngine->SmallFont, FColor(255,255,255));
			}
		}
	}


	// Draw anim notify viewer.
	if(AnimSetViewer->SelectedAnimSeq)
	{
		INT NotifyViewBorderX = 10;
		INT NotifyViewBorderY = 10;
		INT NotifyViewEndHeight = 10;

		INT SizeX = Viewport->GetSizeX();
		INT SizeY = Viewport->GetSizeY();

		UAnimSequence* AnimSeq = AnimSetViewer->SelectedAnimSeq;

		FLOAT PixelsPerSecond = (FLOAT)(SizeX - (2*NotifyViewBorderX))/AnimSeq->SequenceLength;

		// Draw ends of track
		RI->DrawLine2D( NotifyViewBorderX, SizeY - NotifyViewBorderY - NotifyViewEndHeight, NotifyViewBorderX, SizeY - NotifyViewBorderY, FColor(255,255,255) );
		RI->DrawLine2D( SizeX - NotifyViewBorderX, SizeY - NotifyViewBorderY - NotifyViewEndHeight, SizeX - NotifyViewBorderX, SizeY - NotifyViewBorderY, FColor(255,255,255) );

		// Draw track itself
		INT TrackY = SizeY - NotifyViewBorderY - (0.5f*NotifyViewEndHeight);
		RI->DrawLine2D( NotifyViewBorderX, TrackY, SizeX - NotifyViewBorderX, TrackY, FColor(255,255,255) );

		// Draw notifies on the track
		for(INT i=0; i<AnimSeq->Notifies.Num(); i++)
		{
			INT NotifyX = NotifyViewBorderX + (PixelsPerSecond * AnimSeq->Notifies(i).Time);

			RI->DrawLine2D( NotifyX, TrackY - (0.5f*NotifyViewEndHeight), NotifyX, TrackY, FColor(255,200,200) );

			if(AnimSeq->Notifies(i).Comment != NAME_None)
			{
				RI->DrawString( NotifyX - 2, TrackY + 2, *(AnimSeq->Notifies(i).Comment), GEngine->SmallFont, FColor(255,200,200) );
			}
		}
			
		// Draw current position on the track.
		INT CurrentPosX = NotifyViewBorderX + (PixelsPerSecond * AnimSetViewer->PreviewAnimNode->CurrentTime);
		RI->DrawTriangle2D( FIntPoint(CurrentPosX, TrackY-1), 0.f, 0.f, FIntPoint(CurrentPosX+5, TrackY-6), 0.f, 0.f, FIntPoint(CurrentPosX-5, TrackY-6), 0.f, 0.f, FColor(255,255,255) );
	}
}

void FASVViewportClient::Tick(FLOAT DeltaSeconds)
{
	FEditorLevelViewportClient::Tick(DeltaSeconds);

	AnimSetViewer->TickViewer(DeltaSeconds);
}

/*-----------------------------------------------------------------------------
	WxAnimSetViewer
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxAnimSetViewer, wxFrame )
	EVT_COMBOBOX( IDM_ANIMSET_SKELMESHCOMBO, WxAnimSetViewer::OnSkelMeshComboChanged )
	EVT_COMBOBOX( IDM_ANIMSET_ANIMSETCOMBO, WxAnimSetViewer::OnAnimSetComboChanged )
	EVT_LISTBOX( IDM_ANIMSET_ANIMSEQLIST, WxAnimSetViewer::OnAnimSeqListChanged )
	EVT_MENU( IDM_ANIMSET_IMPORTPSA, WxAnimSetViewer::OnImportPSA )
	EVT_MENU( IDM_ANIMSET_NEWANISMET, WxAnimSetViewer::OnNewAnimSet )
	EVT_COMMAND_SCROLL( IDM_ANIMSET_TIMESCRUB, WxAnimSetViewer::OnTimeScrub )
	EVT_MENU( IDM_ANIMSET_VIEWBONES, WxAnimSetViewer::OnViewBones )
	EVT_MENU( IDM_ANIMSET_VIEWBONENAMES, WxAnimSetViewer::OnViewBoneNames )
	EVT_MENU( IDM_ANIMSET_VIEWFLOOR, WxAnimSetViewer::OnViewFloor )
	EVT_MENU( IDM_ANIMSET_VIEWWIREFRAME, WxAnimSetViewer::OnViewWireframe )
	EVT_MENU( IDM_ANIMSET_VIEWGRID, WxAnimSetViewer::OnViewGrid )
	EVT_MENU( IDM_ANIMSET_VIEWREFPOSE, WxAnimSetViewer::OnViewRefPose )
	EVT_BUTTON( IDM_ANIMSET_LOOPBUTTON, WxAnimSetViewer::OnLoopAnim )
	EVT_BUTTON( IDM_ANIMSET_PLAYBUTTON, WxAnimSetViewer::OnPlayAnim )
	EVT_MENU( IDM_ANIMSET_EMPTYSET, WxAnimSetViewer::OnEmptySet )
	EVT_MENU( IDM_ANIMSET_RENAMESEQ, WxAnimSetViewer::OnRenameSequence )
	EVT_MENU( IDM_ANIMSET_DELETESEQ, WxAnimSetViewer::OnDeleteSequence )
	EVT_MENU( IDM_ANIMSET_SEQAPPLYROT, WxAnimSetViewer::OnSequenceApplyRotation )
	EVT_MENU( IDM_ANIMSET_SEQREZERO, WxAnimSetViewer::OnSequenceReZero )
	EVT_MENU( IDM_ANIMSET_SEQDELBEFORE, WxAnimSetViewer::OnSequenceCrop )
	EVT_MENU( IDM_ANIMSET_SEQDELAFTER, WxAnimSetViewer::OnSequenceCrop )
	EVT_MENU( IDM_ANIMSET_NOTIFYSORT, WxAnimSetViewer::OnNotifySort )
	EVT_MENU( IDM_ANIMSET_COPYSEQNAME, WxAnimSetViewer::OnCopySequenceName )
	EVT_MENU( IDM_ANIMSET_COPYMESHBONES, WxAnimSetViewer::OnCopyMeshBoneNames )	
END_EVENT_TABLE()


static INT PreviewLevelCount = 0;
static INT ControlBorder = 4;

WxAnimSetViewer::WxAnimSetViewer(wxWindow* InParent, wxWindowID InID, USkeletalMesh* InSkelMesh, UAnimSet* InAnimSet)
: wxFrame( InParent, InID, TEXT("AnimSetViewer"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR )
{
	SelectedSkelMesh = NULL;
	SelectedAnimSet = NULL;
	SelectedAnimSeq = NULL;


	// Create level we are going to view into
	FString NewLevelName = FString::Printf( TEXT("ASVLevel%d"), PreviewLevelCount );
	PreviewLevel = new( UObject::GetTransientPackage(), *NewLevelName )ULevel( GUnrealEd, 1 );
	PreviewLevelCount++;

	// Spawn preview actor
	PreviewActor = (AASVActor*)PreviewLevel->SpawnActor( AASVActor::StaticClass() );
	PreviewSkelComp = PreviewActor->SkelComponent;
	PreviewAnimNode = CastChecked<UAnimNodeSequence>(PreviewActor->SkelComponent->Animations);

	PreviewFloor = (APhATFloor*)PreviewLevel->SpawnActor( APhATFloor::StaticClass() );

	PreviewLevel->GetLevelInfo()->bBegunPlay = true; // Animation doesn't play unless level has begun. Bit of a hack here...

	/////////////////////////////////
	// Create all the GUI stuff.

	PlayB.Load( TEXT("ASV_Play") );
	StopB.Load( TEXT("ASV_Stop") );
	LoopB.Load( TEXT("ASV_Loop") );
	NoLoopB.Load( TEXT("ASV_NoLoop") );

	this->SetBackgroundColour( wxColor(224, 223, 227) );

	wxBoxSizer* TopSizerH = new wxBoxSizer( wxHORIZONTAL );
	this->SetSizer(TopSizerH);
	this->SetAutoLayout(true);

	//// LEFT PANEL
	wxBoxSizer* LeftSizerV = new wxBoxSizer( wxVERTICAL );
	TopSizerH->Add( LeftSizerV, 0, wxGROW|wxALL, ControlBorder );

	// SkeletalMesh
	wxStaticText* SkelMeshLabel = new wxStaticText( this, -1, TEXT("Skeletal Mesh:") );
	LeftSizerV->Add( SkelMeshLabel, 0, wxALIGN_LEFT|wxALL|wxADJUST_MINSIZE, ControlBorder );

	SkelMeshCombo = new wxComboBox( this, IDM_ANIMSET_SKELMESHCOMBO, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY | wxCB_SORT );
	LeftSizerV->Add( SkelMeshCombo, 0, wxGROW|wxALL, ControlBorder );

	// AnimSet
	wxStaticText* AnimSetLabel = new wxStaticText( this, -1, TEXT("AnimSet:") );
	LeftSizerV->Add( AnimSetLabel, 0, wxALIGN_LEFT|wxALL|wxADJUST_MINSIZE, ControlBorder );

	AnimSetCombo = new wxComboBox( this, IDM_ANIMSET_ANIMSETCOMBO, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY | wxCB_SORT );
	LeftSizerV->Add( AnimSetCombo, 0, wxGROW|wxALL, ControlBorder );

	// AnimSequence
	wxStaticText* AnimSeqLabel = new wxStaticText( this, -1, TEXT("Animation Sequences:") );
	LeftSizerV->Add( AnimSeqLabel, 0, wxALIGN_LEFT|wxALL|wxADJUST_MINSIZE, ControlBorder );

	AnimSeqList = new wxListBox( this, IDM_ANIMSET_ANIMSEQLIST, wxDefaultPosition, wxSize(250, -1), 0, NULL, wxLB_SINGLE );
	LeftSizerV->Add( AnimSeqList, 1, wxGROW|wxALL, ControlBorder  );


	//// MIDDLE PANEL
	wxBoxSizer* MidSizerV = new wxBoxSizer( wxVERTICAL );
	TopSizerH->Add( MidSizerV, 1, wxGROW|wxALL, ControlBorder );

	// 3D Preview
	WxASVPreview* PreviewWindow = new WxASVPreview( this, -1, this );
	MidSizerV->Add( PreviewWindow, 1, wxGROW|wxALL, ControlBorder );

	// Sizer for controls at bottom
	wxBoxSizer* ControlSizerH = new wxBoxSizer( wxHORIZONTAL );
	MidSizerV->Add( ControlSizerH, 0, wxGROW|wxALL, ControlBorder );

	// Stop/Play/Loop buttons
	LoopButton = new wxBitmapButton( this, IDM_ANIMSET_LOOPBUTTON, NoLoopB, wxDefaultPosition, wxDefaultSize, 0 );
	ControlSizerH->Add( LoopButton, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5 );

	PlayButton = new wxBitmapButton( this, IDM_ANIMSET_PLAYBUTTON, PlayB, wxDefaultPosition, wxDefaultSize, 0 );
	ControlSizerH->Add( PlayButton, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5 );

	// Time scrubber
	TimeSlider = new wxSlider( this, IDM_ANIMSET_TIMESCRUB, 0, 0, ASV_SCRUBRANGE );
	ControlSizerH->Add( TimeSlider, 1, wxGROW|wxALL, 0 );


	//// RIGHT PANEL

	PropNotebook = new wxNotebook( this, -1, wxDefaultPosition, wxSize(350, -1) );
	TopSizerH->Add( PropNotebook, 0, wxGROW|wxALL, ControlBorder );

	// Mesh properties
	wxPanel* MeshPropsPanel = new wxPanel( PropNotebook, -1 );
	PropNotebook->AddPage( MeshPropsPanel, TEXT("Mesh"), true );

	wxBoxSizer* MeshPropsPanelSizer = new wxBoxSizer(wxVERTICAL);
	MeshPropsPanel->SetSizer(MeshPropsPanelSizer);
	MeshPropsPanel->SetAutoLayout(true);

	MeshProps = new WxPropertyWindow( MeshPropsPanel, this );
	MeshPropsPanelSizer->Add( MeshProps, 1, wxGROW|wxALL, 0 );


	// AnimSet properties
	wxPanel* AnimSetPropsPanel = new wxPanel( PropNotebook, -1 );
	PropNotebook->AddPage( AnimSetPropsPanel, TEXT("AnimSet"), true );

	wxBoxSizer* AnimSetPropsPanelSizer = new wxBoxSizer(wxVERTICAL);
	AnimSetPropsPanel->SetSizer(AnimSetPropsPanelSizer);
	AnimSetPropsPanel->SetAutoLayout(true);

	AnimSetProps = new WxPropertyWindow( AnimSetPropsPanel, this );
	AnimSetPropsPanelSizer->Add( AnimSetProps, 1, wxGROW|wxALL, 0 );


	// AnimSequence properties
	wxPanel* AnimSeqPropsPanel = new wxPanel( PropNotebook, -1 );
	PropNotebook->AddPage( AnimSeqPropsPanel, TEXT("AnimSequence"), true );

	wxBoxSizer* AnimSeqPropsPanelSizer = new wxBoxSizer(wxVERTICAL);
	AnimSeqPropsPanel->SetSizer(AnimSeqPropsPanelSizer);
	AnimSeqPropsPanel->SetAutoLayout(true);

	AnimSeqProps = new WxPropertyWindow( AnimSeqPropsPanel, this );
	AnimSeqPropsPanelSizer->Add( AnimSeqProps, 1, wxGROW|wxALL, 0 );

	// Create menu bar
	MenuBar = new WxASVMenuBar();
	SetMenuBar( MenuBar );

	// Create tool bar
	ToolBar = new WxASVToolBar( this, -1 );
	//SetToolBar( ToolBar );

	// Create status bar
	StatusBar = new WxASVStatusBar( this, -1 );
	SetStatusBar( StatusBar );

	// Store ref to preview VC
	ASVPreviewVC = PreviewWindow->ASVPreviewVC;

	// Set camera location
	// TODO: Set the automatically based on bounds? Update when changing mesh? Store per-mesh?
	ASVPreviewVC->ViewLocation = FVector(0,-256,0);
	ASVPreviewVC->ViewRotation = FRotator(0,16384,0);	


	// Fill in skeletal mesh combo box with all loaded skeletal meshes.
	for(TObjectIterator<USkeletalMesh> It; It; ++It)
	{
		USkeletalMesh* ItSkelMesh = *It;
		SkelMeshCombo->Append( ItSkelMesh->GetName(), ItSkelMesh );
	}

	// Select the skeletal mesh we were passed in.
	SetSelectedSkelMesh(InSkelMesh);

	// If an animation set was passed in, try and select it. If not, select first one.
	if(InAnimSet)
	{
		SetSelectedAnimSet(InAnimSet);
	}
	else
	{
		if( AnimSetCombo->GetCount() > 0)
		{
			UAnimSet* DefaultAnimSet = (UAnimSet*)AnimSetCombo->GetClientData(0);
			SetSelectedAnimSet(DefaultAnimSet);
		}
	}
}

WxAnimSetViewer::~WxAnimSetViewer()
{
	GEngine->Client->CloseViewport(ASVPreviewVC->Viewport);
	ASVPreviewVC->Viewport = NULL;

	delete ASVPreviewVC;
	ASVPreviewVC = NULL;
	
	if(PreviewLevel)
	{
		PreviewLevel->CleanupLevel();
		PreviewLevel = NULL;
	}
}

/**
 * Serializes the preview level so it isn't garbage collected
 *
 * @param Ar The archive to serialize with
 */
void WxAnimSetViewer::Serialize(FArchive& Ar)
{
	PreviewLevel->Serialize(Ar);
	ASVPreviewVC->Serialize(Ar);
}

///////////////////////////////////////////////////////////////////////////////////////
// Properties window NotifyHook stuff

void WxAnimSetViewer::NotifyDestroy( void* Src )
{

}

void WxAnimSetViewer::NotifyPreChange( void* Src, UProperty* PropertyAboutToChange )
{

}

void WxAnimSetViewer::NotifyPostChange( void* Src, UProperty* PropertyThatChanged )
{
	// In case we change Origin/RotOrigin, update the .
	PreviewActor->UpdateComponents();
}

void WxAnimSetViewer::NotifyExec( void* Src, const TCHAR* Cmd )
{
	GUnrealEd->NotifyExec(Src, Cmd);
}

/*-----------------------------------------------------------------------------
	XASVPreview
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxASVPreview, wxWindow )
	EVT_SIZE( WxASVPreview::OnSize )
END_EVENT_TABLE()

WxASVPreview::WxASVPreview( wxWindow* InParent, wxWindowID InID, WxAnimSetViewer* InASV )
: wxWindow( InParent, InID )
{
	ASVPreviewVC = new FASVViewportClient(InASV);
	ASVPreviewVC->Viewport = GEngine->Client->CreateWindowChildViewport(ASVPreviewVC, (HWND)GetHandle());
	ASVPreviewVC->Viewport->CaptureJoystickInput(false);
}

WxASVPreview::~WxASVPreview()
{

}

void WxASVPreview::OnSize( wxSizeEvent& In )
{
	wxRect rc = GetClientRect();
	::MoveWindow( (HWND)ASVPreviewVC->Viewport->GetWindow(), 0, 0, rc.GetWidth(), rc.GetHeight(), 1 );
}


