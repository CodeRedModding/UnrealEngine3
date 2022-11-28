/*=============================================================================
	Cascade.cpp: 'Cascade' particle editor
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"
#include "EngineParticleClasses.h"

IMPLEMENT_CLASS(ACascadePreviewActor);
IMPLEMENT_CLASS(UCascadeOptions);
IMPLEMENT_CLASS(UCascadePreviewComponent);

/*-----------------------------------------------------------------------------
	WxCascade
-----------------------------------------------------------------------------*/

UBOOL				WxCascade::bParticleModuleClassesInitialized = false;
TArray<UClass*>		WxCascade::ParticleModuleClasses;
TArray<UClass*>		WxCascade::ParticleModuleBaseClasses;

// On init, find all particle module classes. Will use later on to generate menus.
void WxCascade::InitParticleModuleClasses()
{
	if (bParticleModuleClassesInitialized)
		return;

	for(TObjectIterator<UClass> It; It; ++It)
	{
		// Find all ParticleModule classes (ignoring abstract or ParticleTrailModule classes
		if (It->IsChildOf(UParticleModule::StaticClass()))
		{
			if (!(It->ClassFlags & CLASS_Abstract))
			{
				ParticleModuleClasses.AddItem(*It);
			}
			else
			{
				ParticleModuleBaseClasses.AddItem(*It);
			}
		}
	}

	bParticleModuleClassesInitialized = true;
}

UBOOL				WxCascade::bParticleEmitterClassesInitialized = false;
TArray<UClass*>		WxCascade::ParticleEmitterClasses;

// On init, find all particle module classes. Will use later on to generate menus.
void WxCascade::InitParticleEmitterClasses()
{
	if (bParticleEmitterClassesInitialized)
		return;

	for (TObjectIterator<UClass> It; It; ++It)
	{
		// Find all ParticleModule classes (ignoring abstract classes)
		if (It->IsChildOf(UParticleEmitter::StaticClass()) && !(It->ClassFlags & CLASS_Abstract))
		{
			ParticleEmitterClasses.AddItem(*It);
		}
	}

	bParticleEmitterClassesInitialized = true;
}

BEGIN_EVENT_TABLE( WxCascade, wxFrame )
	EVT_SIZE( WxCascade::OnSize )
	EVT_MENU(IDM_CASCADE_RENAME_EMITTER, WxCascade::OnRenameEmitter)
//	EVT_MENU_RANGE( IDM_CASCADE_NEW_SPRITEEMITTER, IDM_CASCADE_NEW_TRAILEMITTER, WxCascade::OnNewEmitter )
	EVT_MENU_RANGE(	IDM_CASCADE_NEW_EMITTER_START, IDM_CASCADE_NEW_EMITTER_END, WxCascade::OnNewEmitter )
	EVT_MENU_RANGE( IDM_CASCADE_NEW_MODULE_START, IDM_CASCADE_NEW_MODULE_END, WxCascade::OnNewModule )
	EVT_MENU(IDM_CASCADE_ADD_SELECTED_MODULE, WxCascade::OnAddSelectedModule)
	EVT_MENU(IDM_CASCADE_COPY_MODULE, WxCascade::OnCopyModule)
	EVT_MENU(IDM_CASCADE_PASTE_MODULE, WxCascade::OnPasteModule)
	EVT_MENU( IDM_CASCADE_DELETE_MODULE, WxCascade::OnDeleteModule )
	EVT_MENU(IDM_CASCADE_DUPLICATE_EMITTER, WxCascade::OnDuplicateEmitter)
	EVT_MENU( IDM_CASCADE_DELETE_EMITTER, WxCascade::OnDeleteEmitter )
	EVT_MENU_RANGE( IDM_CASCADE_SIM_PAUSE, IDM_CASCADE_SIM_NORMAL, WxCascade::OnMenuSimSpeed )
	EVT_MENU( IDM_CASCADE_SAVECAM, WxCascade::OnSaveCam )
	EVT_MENU( IDM_CASCADE_VIEW_DUMP, WxCascade::OnViewModuleDump)
	EVT_MENU( IDM_CASCADE_RESETSYSTEM, WxCascade::OnResetSystem )
	EVT_TOOL( IDM_CASCADE_ORBITMODE, WxCascade::OnOrbitMode )
	EVT_TOOL(IDM_CASCADE_PLAY, WxCascade::OnPlay)
	EVT_TOOL(IDM_CASCADE_PAUSE, WxCascade::OnPause)
	EVT_TOOL(IDM_CASCADE_SPEED_100,	WxCascade::OnSpeed)
	EVT_TOOL(IDM_CASCADE_SPEED_50, WxCascade::OnSpeed)
	EVT_TOOL(IDM_CASCADE_SPEED_25, WxCascade::OnSpeed)
	EVT_TOOL(IDM_CASCADE_SPEED_10, WxCascade::OnSpeed)
	EVT_TOOL(IDM_CASCADE_SPEED_1, WxCascade::OnSpeed)
	EVT_TOOL(IDM_CASCADE_LOOPSYSTEM, WxCascade::OnLoopSystem)
	EVT_TREE_SEL_CHANGED(IDM_CASCADE_MODULE_TREE, WxCascade::OnTreeItemSelected)
	EVT_TREE_ITEM_ACTIVATED(IDM_CASCADE_MODULE_TREE, WxCascade::OnTreeItemActivated)
	EVT_TREE_ITEM_EXPANDING(IDM_CASCADE_MODULE_TREE, WxCascade::OnTreeExpanding)
	EVT_TREE_ITEM_COLLAPSED(IDM_CASCADE_MODULE_TREE, WxCascade::OnTreeCollapsed)

	EVT_MENU( IDM_CASCADE_VIEW_AXES, WxCascade::OnViewAxes )
	EVT_MENU(IDM_CASCADE_VIEW_COUNTS, WxCascade::OnViewCounts)
	EVT_MENU(IDM_CASCADE_RESET_PEAK_COUNTS, WxCascade::OnResetPeakCounts)
	EVT_MENU( IDM_CASCADE_SIM_RESTARTONFINISH, WxCascade::OnLoopSimulation )
END_EVENT_TABLE()

static INT CascadeLevelCount = 0;

WxCascade::WxCascade( wxWindow* InParent, wxWindowID InID, UParticleSystem* InPartSys )
: wxFrame( InParent, InID, TEXT(""), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR )
{
	EditorOptions = ConstructObject<UCascadeOptions>(UCascadeOptions::StaticClass());
	check(EditorOptions);

	// Make sure we have a list of available particle modules
	WxCascade::InitParticleModuleClasses();
	WxCascade::InitParticleEmitterClasses();

	// Set up pointers to interp objects
	PartSys = InPartSys;

	// Nothing selected initially
	SelectedEmitter = NULL;
	SelectedModule = NULL;

	CopyModule = NULL;
	CopyEmitter = NULL;

	CurveToReplace = NULL;

	bResetOnFinish = true;
	bPendingReset = false;
	bOrbitMode = true;
	SimSpeed = 1.0f;

	// Create level we are going to view into
	FString NewLevelName = FString::Printf( TEXT("CascadeLevel%d"), CascadeLevelCount );
	PreviewLevel = new( UObject::GetTransientPackage(), *NewLevelName )ULevel( GUnrealEd, 1 );
	CascadeLevelCount++;
	check(PreviewLevel);

	// Create Emitter actor for previewing particle syste,
	PreviewEmitter = (ACascadePreviewActor*)PreviewLevel->SpawnActor(ACascadePreviewActor::StaticClass());
	check(PreviewEmitter);
	check(PreviewEmitter->PartSysComp);
	check(PreviewEmitter->CascPrevComp);

	// Set emitter to use the particle system we are editing.
	PreviewEmitter->PartSysComp->SetTemplate(PartSys);

	// Create splitters
	INT WinX, WinY;
	GetClientSize(&WinX, &WinY);

	wxBoxSizer* item2 = new wxBoxSizer(wxVERTICAL);
	SetSizer(item2);
	SetAutoLayout(TRUE);

	TopSplitterWnd = new wxSplitterWindow(this, ID_SPLITTERWINDOW, wxDefaultPosition, wxSize(100, 100), wxSP_3DBORDER|wxSP_FULLSASH );
	TopSplitterWnd->SetMinimumPaneSize(20);

	PreviewSplitterWnd = new wxSplitterWindow(TopSplitterWnd, ID_SPLITTERWINDOW, wxDefaultPosition, wxSize(100, 100), wxSP_3DBORDER|wxSP_FULLSASH );
	PreviewSplitterWnd->SetMinimumPaneSize(20);
	EditorSplitterWnd = new wxSplitterWindow(TopSplitterWnd, ID_SPLITTERWINDOW, wxDefaultPosition, wxSize(100, 100), wxSP_3DBORDER|wxSP_FULLSASH );
	EditorSplitterWnd->SetMinimumPaneSize(20);

	TopSplitterWnd->SplitVertically(PreviewSplitterWnd, EditorSplitterWnd, 400);

	// Create property editor
	TreeSplitterWnd = new wxSplitterWindow(PreviewSplitterWnd, ID_CASCADE_PROPERTY_MODULE_TREE_SPLITTER, wxDefaultPosition, wxSize(100, 100), wxSP_3DBORDER|wxSP_FULLSASH );

	PropertyWindow = new WxPropertyWindow(TreeSplitterWnd, this);

	ModuleTreeImages = new wxImageList(16, 15);
	ModuleTree = new wxTreeCtrl(TreeSplitterWnd, IDM_CASCADE_MODULE_TREE, wxDefaultPosition, wxSize(100,100), wxTR_HAS_BUTTONS, wxDefaultValidator, TEXT("ModuleTree"));
	ModuleTree->AssignImageList(ModuleTreeImages);

	TreeSelectedModuleIndex = -1;

	TreeSplitterWnd->SplitHorizontally(PropertyWindow, ModuleTree, 250);

	// Create particle system preview
	WxCascadePreview* PreviewWindow = new WxCascadePreview( PreviewSplitterWnd, -1, this );
	PreviewVC = PreviewWindow->CascadePreviewVC;
	PreviewVC->SetPreviewCamera(PartSys->ThumbnailAngle, PartSys->ThumbnailDistance);

	PreviewSplitterWnd->SplitHorizontally(PreviewWindow, TreeSplitterWnd, 350);

	// Create new curve editor setup if not already done
	if (!PartSys->CurveEdSetup)
	{
		PartSys->CurveEdSetup = ConstructObject<UInterpCurveEdSetup>( UInterpCurveEdSetup::StaticClass(), PartSys, NAME_None, RF_NotForClient | RF_NotForServer );
	}

	// Create graph editor to work on systems CurveEd setup.
	CurveEd = new WxCurveEditor( EditorSplitterWnd, -1, PartSys->CurveEdSetup );

	// Create emitter editor
	WxCascadeEmitterEd* EmitterEdWindow = new WxCascadeEmitterEd( EditorSplitterWnd, -1, this );
	EmitterEdVC = EmitterEdWindow->EmitterEdVC;

	EditorSplitterWnd->SplitHorizontally( EmitterEdWindow, CurveEd, 400 );

	GetSizer()->Fit(this);
	GetSizer()->SetSizeHints(this);
	Centre();

	// Create menu bar
	MenuBar = new WxCascadeMenuBar(this);
	SetMenuBar( MenuBar );

	// Create tool bar
	ToolBar = new WxCascadeToolBar( this, -1 );
	SetToolBar( ToolBar );

	// Setup the module tree control
	ModuleTreeImages->Add(WxBitmap("Btn_FolderClosed"), wxColor(192,192,192));	// 0
	ModuleTreeImages->Add(WxBitmap("Btn_FolderOpen"), wxColor(192,192,192));		// 1
	ModuleTreeImages->Add(WxBitmap("CASC_ModuleIcon"), wxColor(192,192,192));	// 2

	// Initialise the tree control.
	RebuildTreeControl();

	// Set window title to particle system we are editing.
	SetTitle( *FString::Printf( TEXT("UnrealCascade: %s"), PartSys->GetName() ) );

	SetSelectedModule(NULL, NULL);
}

WxCascade::~WxCascade()
{
	// Destroy preview viewport before we destroy the level.
	GEngine->Client->CloseViewport(PreviewVC->Viewport);
	PreviewVC->Viewport = NULL;

	delete PreviewVC;
	PreviewVC = NULL;

	// Destroy emitter actor
	PreviewEmitter = NULL;

	if (PreviewLevel)
	{
		PreviewLevel->CleanupLevel();
		PreviewLevel = NULL;
	}

	delete PropertyWindow;
}

wxToolBar* WxCascade::OnCreateToolBar(long style, wxWindowID id, const wxString& name)
{
	if (name == TEXT("Cascade"))
		return new WxCascadeToolBar(this, -1);

	return OnCreateToolBar(style, id, name);
}

void WxCascade::Serialize(FArchive& Ar)
{
	Ar << PreviewLevel;

	PreviewVC->Serialize(Ar);

	Ar << EditorOptions;
}

void WxCascade::OnSize( wxSizeEvent& In )
{
	if ( TopSplitterWnd )
	{
		wxRect rc = GetClientRect();
		rc.y -= 30;
		TopSplitterWnd->SetSize( rc );
	}

	In.Skip();
}

///////////////////////////////////////////////////////////////////////////////////////
// Menu Callbacks

void WxCascade::OnRenameEmitter(wxCommandEvent& In)
{
	if (!SelectedEmitter)
		return;

	PartSys->PreEditChange();
	PreviewEmitter->PartSysComp->PreEditChange();

	FName& CurrentName = SelectedEmitter->GetEmitterName();

	WxDlgGenericStringEntry dlg;
	if (dlg.ShowModal(TEXT("Rename Emitter"), TEXT("Name:"), *CurrentName) == wxID_OK)
	{
//		element->Modify();
		FName newName = FName(*(dlg.EnteredString));
		SelectedEmitter->SetEmitterName(newName);
//		element->Rename(*newElementName,Container);
	}

	PreviewEmitter->PartSysComp->PostEditChange(NULL);
	PartSys->PostEditChange(NULL);

	// Refresh viewport
	EmitterEdVC->Viewport->Invalidate();
}

void WxCascade::OnNewEmitter(wxCommandEvent& In)
{
	PartSys->PreEditChange();
	PreviewEmitter->PartSysComp->PreEditChange();

	// Find desired class of new module.
	INT NewEmitClassIndex = In.GetId() - IDM_CASCADE_NEW_EMITTER_START;
	check( NewEmitClassIndex >= 0 && NewEmitClassIndex < ParticleEmitterClasses.Num() );

	UClass* NewEmitClass = ParticleEmitterClasses(NewEmitClassIndex);
	if (NewEmitClass == UParticleSpriteEmitter::StaticClass())
	{
		check( NewEmitClass->IsChildOf(UParticleEmitter::StaticClass()) );

		// Construct it
		UParticleEmitter* NewEmitter = ConstructObject<UParticleEmitter>( NewEmitClass, PartSys );
		NewEmitter->EmitterEditorColor = MakeRandomColor();

        // Set to sensible default values
		NewEmitter->SetToSensibleDefaults();

        // Handle special cases...
		if (NewEmitClass == UParticleSpriteEmitter::StaticClass())
		{
			// For handyness- use currently selected material for new emitter (or default if none selected)
			UParticleSpriteEmitter* NewSpriteEmitter = (UParticleSpriteEmitter*)NewEmitter;
/***
            for (INT i = 0; i < NewSpriteEmitter->Modules.Num(); i++)
            {
                UParticleModule* Module = NewSpriteEmitter->Modules(i);
                if (Module->IsA(UParticleModuleTypeDataBase::StaticClass()))
                {
                    UParticleModuleTypeDataBase* TypeDataModule = Cast<UParticleModuleTypeDataBase>(Module);
                    TypeDataModule->SetToSensibleDefaults();
                }
            }
***/
			UMaterialInstance* CurrentMaterial = GSelectionTools.GetTop<UMaterialInstance>();
			if (CurrentMaterial)
				NewSpriteEmitter->Material = CurrentMaterial;
			else
				NewSpriteEmitter->Material = LoadObject<UMaterialInstance>(NULL, TEXT("EngineMaterials.DefaultParticle"), NULL, LOAD_NoFail, NULL);
		}

        // Add to selected emitter
        PartSys->Emitters.AddItem(NewEmitter);
	}
	else
	{
		appMsgf(0, TEXT("%s support coming soon."), NewEmitClass->GetDesc());
	}

	PreviewEmitter->PartSysComp->PostEditChange(NULL);
	PartSys->PostEditChange(NULL);

	// Refresh viewport
	EmitterEdVC->Viewport->Invalidate();
}

void WxCascade::OnNewModule(wxCommandEvent& In)
{
	if (!SelectedEmitter)
		return;

	// Find desired class of new module.
	INT NewModClassIndex = In.GetId() - IDM_CASCADE_NEW_MODULE_START;
	check( NewModClassIndex >= 0 && NewModClassIndex < ParticleModuleClasses.Num() );

	CreateNewModule(NewModClassIndex);
}

void WxCascade::OnDuplicateEmitter(wxCommandEvent& In)
{
	// Make sure there is a selected emitter
	if (!SelectedEmitter)
		return;
/***
	PreviewEmitter->PartSysComp->PreEditChange();

	// Copy it...
	UParticleEmitter* NewEmitter = ConstructObject<UParticleEmitter>(
		SelectedEmitter->GetClass(), PartSys, NAME_None, 
		RF_Public|RF_Transactional, SelectedEmitter);
	NewEmitter->EmitterEditorColor = MakeRandomColor();

	PartSys->Emitters.AddItem(NewEmitter);

	PreviewEmitter->PartSysComp->PostEditChange(NULL);
***/
	for( FObjectIterator It; It; ++It )
		It->ClearFlags( RF_TagImp | RF_TagExp );

	FStringOutputDevice Ar;
	UExporter::ExportToOutputDevice(SelectedEmitter, NULL, Ar, TEXT("T3D"), 0 );
	appClipboardCopy(*Ar);
}

void WxCascade::OnDeleteEmitter(wxCommandEvent& In)
{
	DeleteSelectedEmitter();
}

void WxCascade::OnAddSelectedModule(wxCommandEvent& In)
{
	if (GetTreeSelectedModule() != -1)
	{
		CreateNewModule(GetTreeSelectedModule());
	}
}

void WxCascade::OnCopyModule(wxCommandEvent& In)
{
	if (SelectedModule)
		SetCopyModule(SelectedEmitter, SelectedModule);
}

void WxCascade::OnPasteModule(wxCommandEvent& In)
{
	check(CopyModule);

	if (SelectedEmitter && CopyEmitter && (SelectedEmitter == CopyEmitter))
	{
		// Can't copy onto ourselves... Or can we
		appMsgf(0, TEXT("Duplicate Module on Same Emitter?"));
		return;
	}
	appMsgf(0, TEXT("Paste module!"));
}

void WxCascade::OnDeleteModule(wxCommandEvent& In)
{
	DeleteSelectedModule();
}

void WxCascade::OnMenuSimSpeed(wxCommandEvent& In)
{
	INT Id = In.GetId();

	if (Id == IDM_CASCADE_SIM_PAUSE)
	{
		PreviewVC->TimeScale = 0.f;
		ToolBar->ToggleTool(IDM_CASCADE_PLAY, false);
		ToolBar->ToggleTool(IDM_CASCADE_PAUSE, true);
	}
	else
	{
		if ((Id == IDM_CASCADE_SIM_1PERCENT) || 
			(Id == IDM_CASCADE_SIM_10PERCENT) || 
			(Id == IDM_CASCADE_SIM_25PERCENT) || 
			(Id == IDM_CASCADE_SIM_50PERCENT) || 
			(Id == IDM_CASCADE_SIM_NORMAL))
		{
			ToolBar->ToggleTool(IDM_CASCADE_PLAY, true);
			ToolBar->ToggleTool(IDM_CASCADE_PAUSE, false);
		}

		if (Id == IDM_CASCADE_SIM_1PERCENT)
		{
			PreviewVC->TimeScale = 0.01f;
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_1, true);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_10, false);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_25, false);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_50, false);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_100, false);
		}
		else if (Id == IDM_CASCADE_SIM_10PERCENT)
		{
			PreviewVC->TimeScale = 0.1f;
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_1, false);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_10, true);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_25, false);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_50, false);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_100, false);
		}
		else if (Id == IDM_CASCADE_SIM_25PERCENT)
		{
			PreviewVC->TimeScale = 0.25f;
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_1, false);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_10, false);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_25, true);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_50, false);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_100, false);
		}
		else if (Id == IDM_CASCADE_SIM_50PERCENT)
		{
			PreviewVC->TimeScale = 0.5f;
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_1, false);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_10, false);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_25, false);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_50, true);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_100, false);
		}
		else if (Id == IDM_CASCADE_SIM_NORMAL)
		{
			PreviewVC->TimeScale = 1.f;
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_1, false);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_10, false);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_25, false);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_50, false);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_100, true);
		}
	}
}

void WxCascade::OnSaveCam(wxCommandEvent& In)
{
	PartSys->ThumbnailAngle = PreviewVC->PreviewAngle;
	PartSys->ThumbnailDistance = PreviewVC->PreviewDistance;
	PartSys->PreviewComponent = NULL;
}

void WxCascade::OnResetSystem(wxCommandEvent& In)
{
	PreviewEmitter->PartSysComp->ResetParticles();
	PreviewEmitter->PartSysComp->InitializeSystem();
}

void WxCascade::OnResetPeakCounts(wxCommandEvent& In)
{
	PreviewEmitter->PartSysComp->ResetParticles();
	for (INT i = 0; i < PreviewEmitter->PartSysComp->Template->Emitters.Num(); i++)
	{
		UParticleEmitter* Emitter = PreviewEmitter->PartSysComp->Template->Emitters(i);
		Emitter->PeakActiveParticles = 0;
	}
	PreviewEmitter->PartSysComp->InitParticles();
}

void WxCascade::OnOrbitMode(wxCommandEvent& In)
{
	bOrbitMode = In.IsChecked();

	//@todo. actually handle this...
	if (bOrbitMode)
	{
		PreviewVC->SetPreviewCamera(PreviewVC->PreviewAngle, PreviewVC->PreviewDistance);
	}
}

void WxCascade::OnViewAxes(wxCommandEvent& In)
{
	PreviewVC->bDrawOriginAxes = In.IsChecked();
}

void WxCascade::OnViewCounts(wxCommandEvent& In)
{
	PreviewVC->bDrawParticleCounts = In.IsChecked();
}

void WxCascade::OnLoopSimulation(wxCommandEvent& In)
{
	bResetOnFinish = In.IsChecked();

	if (!bResetOnFinish)
		bPendingReset = false;
}

void WxCascade::OnViewModuleDump(wxCommandEvent& In)
{
	EmitterEdVC->bDrawModuleDump = !EmitterEdVC->bDrawModuleDump;
	EditorOptions->bShowModuleDump = EmitterEdVC->bDrawModuleDump;
	EditorOptions->SaveConfig();

	EmitterEdVC->Viewport->Invalidate();
}

void WxCascade::OnPlay(wxCommandEvent& In)
{
	PreviewVC->TimeScale = SimSpeed;
}

void WxCascade::OnPause(wxCommandEvent& In)
{
	PreviewVC->TimeScale = 0.f;
}

void WxCascade::OnSpeed(wxCommandEvent& In)
{
	INT Id = In.GetId();

	FLOAT NewSimSpeed = 0.0f;
	INT SimID;

	switch (Id)
	{
	case IDM_CASCADE_SPEED_1:
		NewSimSpeed = 0.01f;
		SimID = IDM_CASCADE_SIM_1PERCENT;
		break;
	case IDM_CASCADE_SPEED_10:
		NewSimSpeed = 0.1f;
		SimID = IDM_CASCADE_SIM_10PERCENT;
		break;
	case IDM_CASCADE_SPEED_25:
		NewSimSpeed = 0.25f;
		SimID = IDM_CASCADE_SIM_25PERCENT;
		break;
	case IDM_CASCADE_SPEED_50:
		NewSimSpeed = 0.5f;
		SimID = IDM_CASCADE_SIM_50PERCENT;
		break;
	case IDM_CASCADE_SPEED_100:
		NewSimSpeed = 1.0f;
		SimID = IDM_CASCADE_SIM_NORMAL;
		break;
	}

	if (NewSimSpeed != 0.0f)
	{
		SimSpeed = NewSimSpeed;
		if (PreviewVC->TimeScale != 0.0f)
		{
			PreviewVC->TimeScale = SimSpeed;
		}
	}
}

void WxCascade::OnLoopSystem(wxCommandEvent& In)
{
	OnLoopSimulation(In);
}

///////////////////////////////////////////////////////////////////////////////////////
// Properties window NotifyHook stuff

void WxCascade::NotifyDestroy( void* Src )
{

}

void WxCascade::NotifyPreChange( void* Src, UProperty* PropertyAboutToChange )
{
	CurveToReplace = NULL;

	UObjectProperty* ObjProp = Cast<UObjectProperty>(PropertyAboutToChange);
	if ( ObjProp )
	{
		UObject* EditedObject = NULL;
		if (SelectedModule)
		{
			EditedObject = SelectedModule;
		}
		else if (SelectedEmitter)
		{
			EditedObject = SelectedEmitter;
		}

		if ( EditedObject && (ObjProp->PropertyClass->IsChildOf(UDistributionFloat::StaticClass()) || ObjProp->PropertyClass->IsChildOf(UDistributionVector::StaticClass())) )
		{
			UClass* EditedClass = CastChecked<UClass>(ObjProp->GetOuter());
			check( EditedObject->IsA(EditedClass) );

			CurveToReplace = *((UObject**)(((BYTE*)EditedObject) + ObjProp->Offset));		
			check(CurveToReplace); // These properties are 'noclear', so should always have a curve here!
		}
	}
}

void WxCascade::NotifyPostChange( void* Src, UProperty* PropertyThatChanged )
{
	if (CurveToReplace)
	{
		UObject* EditedObject = NULL;
		if (SelectedModule)
		{
			EditedObject = SelectedModule;
		}
		else if (SelectedEmitter)
		{
			EditedObject = SelectedEmitter;
		}
		check(EditedObject);

		// This should be the same property we just got in NotifyPreChange!
		UObjectProperty* ObjProp = Cast<UObjectProperty>(PropertyThatChanged);
		check(ObjProp);
		check(ObjProp->PropertyClass->IsChildOf(UDistributionFloat::StaticClass()) || ObjProp->PropertyClass->IsChildOf(UDistributionVector::StaticClass()));

		UClass* EditedClass = CastChecked<UClass>(ObjProp->GetOuter());
		check( EditedObject->IsA(EditedClass) );

		UObject* NewCurve = *((UObject**)(((BYTE*)EditedObject) + ObjProp->Offset));		
		check(NewCurve);

		PartSys->CurveEdSetup->ReplaceCurve(CurveToReplace, NewCurve);
		CurveEd->CurveChanged();
	}

	if (SelectedModule || SelectedEmitter)
	{
		PartSys->PostEditChange(PropertyThatChanged);
	}

	CurveEd->CurveChanged();
	EmitterEdVC->Viewport->Invalidate();
}

void WxCascade::NotifyExec( void* Src, const TCHAR* Cmd )
{
	GUnrealEd->NotifyExec(Src, Cmd);
}

///////////////////////////////////////////////////////////////////////////////////////
// Utils
void WxCascade::CreateNewModule(INT ModClassIndex)
{
	UClass* NewModClass = ParticleModuleClasses(ModClassIndex);
	check(NewModClass->IsChildOf(UParticleModule::StaticClass()));

	UBOOL bTypeData = false;

	if (NewModClass->IsChildOf(UParticleModuleTypeDataBase::StaticClass()))
	{
		// Make sure there isn't already a TypeData module applied!
		if (SelectedEmitter->TypeDataModule != 0)
		{
			appMsgf(0, TEXT("A TypeData module is already present.\nPlease remove it first."));
			return;
		}
		bTypeData = true;
	}

	if (bTypeData)
	{
		PartSys->PreEditChange();
		PreviewEmitter->PartSysComp->PreEditChange();
	}

	// Construct it and add to selected emitter.
	UParticleModule* NewModule = ConstructObject<UParticleModule>( NewModClass, PartSys );
	NewModule->ModuleEditorColor = MakeRandomColor();

	SelectedEmitter->Modules.AddItem(NewModule);
	SelectedEmitter->UpdateModuleLists();

	if (bTypeData)
	{
		PreviewEmitter->PartSysComp->PostEditChange(NULL);
		PartSys->PostEditChange(NULL);
	}

	PartSys->MarkPackageDirty();

	// Refresh viewport
	EmitterEdVC->Viewport->Invalidate();
}

void WxCascade::SetSelectedEmitter( UParticleEmitter* NewSelectedEmitter )
{
	SelectedEmitter = NewSelectedEmitter;
	SelectedModule = NULL;

	if (NewSelectedEmitter)
	{
		PropertyWindow->SetObject(SelectedEmitter, 1, 0, 1);
	}
	else
	{
		PropertyWindow->SetObject(PartSys, 1, 0, 1);
	}

	EmitterEdVC->Viewport->Invalidate();
}

void WxCascade::SetSelectedModule( UParticleEmitter* NewSelectedEmitter, UParticleModule* NewSelectedModule )
{
	SelectedEmitter = NewSelectedEmitter;
	SelectedModule = NewSelectedModule;

	if (SelectedModule)
	{
		PropertyWindow->SetObject(SelectedModule, 1, 0, 1);
	}
	else
	if (SelectedEmitter)
	{
		PropertyWindow->SetObject(SelectedEmitter, 1, 0, 1);
	}
	else
	{
		PropertyWindow->SetObject(PartSys, 1, 0, 1);
	}

	EmitterEdVC->Viewport->Invalidate();
}

// Insert the selected module into the target emitter at the desired location.
void WxCascade::InsertModule(UParticleModule* Module, UParticleEmitter* TargetEmitter, INT TargetIndex)
{
	if (!Module || !TargetEmitter || TargetIndex == INDEX_NONE)
		return;

	// Cannot insert the same module more than once into the same emitter.
	for(INT i=0; i<TargetEmitter->Modules.Num(); i++)
	{
		if ( TargetEmitter->Modules(i) == Module )
		{
			appMsgf( 0, TEXT("A Module can only be used in each Emitter once.") );
			return;
		}
	}

	// Insert in desired location in new Emitter
	PartSys->PreEditChange();

	if (Module->IsA(UParticleModuleTypeDataBase::StaticClass()))
	{
		if (TargetEmitter->TypeDataModule)
		{
		}
		TargetEmitter->TypeDataModule = Module;
	}
	else
	{
		INT NewModuleIndex = Clamp<INT>(TargetIndex, 0, TargetEmitter->Modules.Num());
		TargetEmitter->Modules.Insert(NewModuleIndex);
		TargetEmitter->Modules(NewModuleIndex) = Module;
	}

	TargetEmitter->UpdateModuleLists();

	PartSys->PostEditChange(NULL);

	// Update selection
	SetSelectedModule(TargetEmitter, Module);

	EmitterEdVC->Viewport->Invalidate();

	PartSys->MarkPackageDirty();
}

// Delete entire Emitter from System
// Garbage collection will clear up any unused modules.
void WxCascade::DeleteSelectedEmitter()
{
	if (!SelectedEmitter)
		return;

	check(PartSys->Emitters.ContainsItem(SelectedEmitter));

	PartSys->PreEditChange();

	SelectedEmitter->RemoveEmitterCurvesFromEditor(CurveEd->EdSetup);

	PartSys->Emitters.RemoveItem(SelectedEmitter);

	PartSys->PostEditChange(NULL);

	SetSelectedEmitter(NULL);

	EmitterEdVC->Viewport->Invalidate();

	PartSys->MarkPackageDirty();
}

// Move the selected amitter by MoveAmount in the array of Emitters.
void WxCascade::MoveSelectedEmitter(INT MoveAmount)
{
	if (!SelectedEmitter)
		return;

	INT CurrentEmitterIndex = PartSys->Emitters.FindItemIndex(SelectedEmitter);
	check(CurrentEmitterIndex != INDEX_NONE);

	INT NewEmitterIndex = Clamp<INT>(CurrentEmitterIndex + MoveAmount, 0, PartSys->Emitters.Num() - 1);

	if (NewEmitterIndex != CurrentEmitterIndex)
	{
		PartSys->PreEditChange();

		PartSys->Emitters.RemoveItem(SelectedEmitter);
		PartSys->Emitters.InsertZeroed(NewEmitterIndex);
		PartSys->Emitters(NewEmitterIndex) = SelectedEmitter;

		PartSys->PostEditChange(NULL);

		EmitterEdVC->Viewport->Invalidate();
	}

	PartSys->MarkPackageDirty();
}

// Delete selected module from selected emitter.
// Module may be used in other Emitters, so we don't destroy it or anything - garbage collection will handle that.
void WxCascade::DeleteSelectedModule()
{
	if (!SelectedModule)
		return;

	if (SelectedEmitter)
	{
		if (!SelectedModule->IsA(UParticleModuleTypeDataBase::StaticClass()))
		{
			check(SelectedEmitter->Modules.ContainsItem(SelectedModule));
		}
		else
		{
			check(SelectedEmitter->TypeDataModule == SelectedModule);
		}
	}
	else
	{
		check(ModuleDumpList.ContainsItem(SelectedModule));
	}

	PartSys->PreEditChange();

	if (SelectedModule->IsDisplayedInCurveEd(CurveEd->EdSetup) && !ModuleIsShared(SelectedModule))
	{
		// Remove it from the curve editor!
		SelectedModule->RemoveModuleCurvesFromEditor(CurveEd->EdSetup);
		CurveEd->CurveEdVC->Viewport->Invalidate();
	}

	if (SelectedEmitter)
	{
		if (SelectedModule->IsA(UParticleModuleTypeDataBase::StaticClass()))
		{
			check(SelectedEmitter->TypeDataModule == SelectedModule);
			SelectedEmitter->TypeDataModule = NULL;
		}
		else
		{
			SelectedEmitter->Modules.RemoveItem(SelectedModule);
		}

		SelectedEmitter->UpdateModuleLists();
	}
	else
	{
		ModuleDumpList.RemoveItem(SelectedModule);
	}

	PartSys->PostEditChange(NULL);

	SetSelectedEmitter(SelectedEmitter);

	EmitterEdVC->Viewport->Invalidate();

	PartSys->MarkPackageDirty();
}

UBOOL WxCascade::ModuleIsShared(UParticleModule* InModule)
{
	INT FindCount = 0;

	for(INT i=0; i<PartSys->Emitters.Num(); i++)
	{
		UParticleEmitter* Emitter = PartSys->Emitters(i);
		for(INT j=0; j<Emitter->Modules.Num(); j++)
		{
			if (Emitter->Modules(j) == InModule)
			{
				FindCount++;

				if (FindCount == 2)
					return true;
			}
		}
	}

	return false;
}

void WxCascade::AddSelectedToGraph()
{
	if (!SelectedEmitter)
		return;

	TArray<FInterpCurveFloat*> FloatCurves;
	TArray<FString> FloatCurveNames;

	if (SelectedModule)
	{
		SelectedModule->AddModuleCurvesToEditor( PartSys->CurveEdSetup );
	}
	else
	{
		SelectedEmitter->AddEmitterCurvesToEditor( PartSys->CurveEdSetup );
	}
	CurveEd->CurveEdVC->Viewport->Invalidate();
}

void WxCascade::SetCopyEmitter(UParticleEmitter* NewEmitter)
{
	CopyEmitter = NewEmitter;
}

void WxCascade::SetCopyModule(UParticleEmitter* NewEmitter, UParticleModule* NewModule)
{
	CopyEmitter = NewEmitter;
	CopyModule = NewModule;
}

void WxCascade::RemoveModuleFromDump(UParticleModule* Module)
{
	for (INT i = 0; i < ModuleDumpList.Num(); i++)
	{
		if (ModuleDumpList(i) == Module)
		{
			ModuleDumpList.Remove(i);
			break;
		}
	}
}

//
// Tree control interface
//
INT WxCascade::GetTreeSelectedModule()
{
	return TreeSelectedModuleIndex;
}

void WxCascade::RebuildTreeControl()
{
	check(ModuleTree);

	TreeSelectedModuleIndex = -1;

	// Empty whats currently there.
	ModuleTree->DeleteAllItems();
	
	wxTreeItemId RootId;
	wxTreeItemId TypeDataId;
	
	// Add the root
	RootId = ModuleTree->AddRoot(wxString(TEXT("Modules")), 0, 0, 0);
	
	// Add the TypeData module header and any of that type...
	TypeDataId = ModuleTree->AppendItem(RootId, wxString(TEXT("TypeData")), 0, 0, 0);

    UBOOL bFoundTypeData = false;
	INT i, j;

    // first, add the data type modules to the menu
	for (i = 0; i < ParticleModuleClasses.Num(); i++)
	{
        if (ParticleModuleClasses(i)->IsChildOf(UParticleModuleTypeDataBase::StaticClass()))
        {
			wxTreeItemId ModuleId = ModuleTree->AppendItem(TypeDataId, *ParticleModuleClasses(i)->GetDescription(), 2, 2, new WxCascadeModuleTreeLeafData(i));
			bFoundTypeData = true;
		}
		if (bFoundTypeData)
		{
			ModuleTree->SortChildren(TypeDataId);
		}
	}

	// Now, for each module base type, add another branch
	for (i = 0; i < ParticleModuleBaseClasses.Num(); i++)
	{
		if ((ParticleModuleBaseClasses(i) != UParticleModuleTypeDataBase::StaticClass()) &&
			(ParticleModuleBaseClasses(i) != UParticleModule::StaticClass()))
		{
			bFoundTypeData = false;
			// Add the 'label'
			TypeDataId = ModuleTree->AppendItem(RootId, *ParticleModuleBaseClasses(i)->GetDescription(), 0, 0, 0);

			// Search for all modules of this type
			for (j = 0; j < ParticleModuleClasses.Num(); j++)
			{
				if (ParticleModuleClasses(j)->IsChildOf(ParticleModuleBaseClasses(i)))
				{
					wxTreeItemId ModuleId = ModuleTree->AppendItem(TypeDataId, 
						*(ParticleModuleClasses(j)->GetDescription()), 
						2, 2, new WxCascadeModuleTreeLeafData(j));
					bFoundTypeData = true;
				}
			}
			if (bFoundTypeData)
			{
				ModuleTree->SortChildren(TypeDataId);
			}
		}
	}

	ModuleTree->Expand(RootId);
}

void WxCascade::OnTreeItemSelected(wxTreeEvent& In)
{
	wxTreeItemId TreeId = In.GetItem();
	WxCascadeModuleTreeLeafData* LeafData = (WxCascadeModuleTreeLeafData*)(ModuleTree->GetItemData(TreeId));
	if (LeafData)
	{
		TreeSelectedModuleIndex = LeafData->Data;
	}
	else
	{
		TreeSelectedModuleIndex = -1;
	}
}

void WxCascade::OnTreeItemActivated(wxTreeEvent& In)
{
	wxTreeItemId TreeId = In.GetItem();
	WxCascadeModuleTreeLeafData* LeafData = (WxCascadeModuleTreeLeafData*)(ModuleTree->GetItemData(TreeId));
	if (LeafData)
	{
		CreateNewModule(LeafData->Data);
	}
}

void WxCascade::OnTreeExpanding(wxTreeEvent& In)
{
}

void WxCascade::OnTreeCollapsed(wxTreeEvent& In)
{
}

//
// UCascadeOptions
// 
