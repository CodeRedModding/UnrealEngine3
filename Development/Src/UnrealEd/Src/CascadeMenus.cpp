/*=============================================================================
	CascadeMenus.cpp: 'Cascade' particle editor menus
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"
#include "EngineParticleClasses.h"

/*-----------------------------------------------------------------------------
	WxCascadeMenuBar
-----------------------------------------------------------------------------*/

WxCascadeMenuBar::WxCascadeMenuBar(WxCascade* Cascade)
{
	EditMenu = new wxMenu();
	Append( EditMenu, TEXT("Edit") );
	EditMenu->Append(IDM_CASCADE_RESET_PEAK_COUNTS, TEXT("Reset Peak Counts"), TEXT("") );

	ViewMenu = new wxMenu();
	Append( ViewMenu, TEXT("View") );

	ViewMenu->AppendCheckItem( IDM_CASCADE_VIEW_AXES, TEXT("View Origin Axes"), TEXT("") );
	ViewMenu->AppendCheckItem( IDM_CASCADE_VIEW_COUNTS, TEXT("View Particle Counts"), TEXT(""));
    ViewMenu->Check( IDM_CASCADE_VIEW_COUNTS, Cascade->PreviewVC->bDrawParticleCounts ? true : false );
	ViewMenu->AppendSeparator();
	ViewMenu->Append( IDM_CASCADE_SAVECAM, TEXT("Save Cam Position"), TEXT("") );
	ViewMenu->AppendCheckItem(IDM_CASCADE_VIEW_DUMP, TEXT("View Module Dump"), TEXT(""));
	ViewMenu->Check(IDM_CASCADE_VIEW_DUMP, Cascade->EditorOptions->bShowModuleDump);
	ViewMenu->AppendSeparator();
}

WxCascadeMenuBar::~WxCascadeMenuBar()
{

}

/*-----------------------------------------------------------------------------
	WxMBCascadeModule
-----------------------------------------------------------------------------*/

WxMBCascadeModule::WxMBCascadeModule(WxCascade* Cascade)
{
	Append(IDM_CASCADE_COPY_MODULE, TEXT("Copy Module"), TEXT(""));
	Append( IDM_CASCADE_DELETE_MODULE, TEXT("Delete Module"), TEXT("") );
}

WxMBCascadeModule::~WxMBCascadeModule()
{

}

/*-----------------------------------------------------------------------------
	WxMBCascadeEmitterBkg
-----------------------------------------------------------------------------*/
UBOOL WxMBCascadeEmitterBkg::InitializedModuleEntries = false;
TArray<FString>	WxMBCascadeEmitterBkg::TypeDataModuleEntries;
TArray<INT>		WxMBCascadeEmitterBkg::TypeDataModuleIndices;
TArray<FString>	WxMBCascadeEmitterBkg::ModuleEntries;
TArray<INT>		WxMBCascadeEmitterBkg::ModuleIndices;

WxMBCascadeEmitterBkg::WxMBCascadeEmitterBkg(WxCascade* Cascade, WxMBCascadeEmitterBkg::Mode eMode)
{
	// Don't show options if no emitter selected.
	if (!Cascade->SelectedEmitter)
		return;

    INT i, j;
	UBOOL bFound;
    UBOOL bFoundTypeData = false;
	UParticleModule* DefModule;

	if (!InitializedModuleEntries)
	{
		TypeDataModuleEntries.Empty();
		TypeDataModuleIndices.Empty();
		ModuleEntries.Empty();
		ModuleIndices.Empty();

		// add the data type modules to the menu
		for(i = 0; i < Cascade->ParticleModuleClasses.Num(); i++)
		{
			DefModule = (UParticleModule*)Cascade->ParticleModuleClasses(i)->GetDefaultObject();

			bFound = false;
			for (j = 0; j < DefModule->AllowedEmitterClasses.Num(); j++)
			{
				if (Cascade->ParticleModuleClasses(i)->IsChildOf(UParticleModuleTypeDataBase::StaticClass()))
				{
					UClass* pkClass = DefModule->AllowedEmitterClasses(j);
					if (Cascade->SelectedEmitter->IsA(pkClass))
					{
						bFound = true;
						bFoundTypeData = true;
					}
					break;
				}
			}

			if (bFound)
			{
				FString NewModuleString = FString::Printf( TEXT("New %s"), *Cascade->ParticleModuleClasses(i)->GetDescription() );
				TypeDataModuleEntries.AddItem(NewModuleString);
				TypeDataModuleIndices.AddItem(i);
			}
		}

		// Add each module type to menu.
		for(i = 0; i < Cascade->ParticleModuleClasses.Num(); i++)
		{
			DefModule = (UParticleModule*)Cascade->ParticleModuleClasses(i)->GetDefaultObject();

			bFound = false;
			for (j = 0; j < ((UParticleModule*)Cascade->ParticleModuleClasses(i))->AllowedEmitterClasses.Num(); j++)
			{
				if (Cascade->ParticleModuleClasses(i)->IsChildOf(UParticleModuleTypeDataBase::StaticClass()) == false)
				{
					UClass* pkClass = DefModule->AllowedEmitterClasses(j);
					if (Cascade->SelectedEmitter->IsA(pkClass))
					{
						bFound = true;
					}
					break;
				}
			}

			if (bFound)
			{
				FString NewModuleString = FString::Printf( TEXT("New %s"), *Cascade->ParticleModuleClasses(i)->GetDescription() );
				ModuleEntries.AddItem(NewModuleString);
				ModuleIndices.AddItem(i);
			}
		}
		InitializedModuleEntries = true;
	}

	check(TypeDataModuleEntries.Num() == TypeDataModuleIndices.Num());
	check(ModuleEntries.Num() == ModuleIndices.Num());

	if ((UINT)eMode & EMITTER_ONLY)
	{
	    // add the rename emitter option
		FString RenameEmitterString = FString("Rename Emitter");
		Append(IDM_CASCADE_RENAME_EMITTER, *RenameEmitterString, TEXT(""));
		AppendSeparator();
	}

	//
	INT iTreeSel = Cascade->GetTreeSelectedModule();
	if (iTreeSel != -1)
	{
		DefModule = (UParticleModule*)Cascade->ParticleModuleClasses(iTreeSel)->GetDefaultObject();
		FString NewModuleString = FString::Printf( TEXT("New %s"), *Cascade->ParticleModuleClasses(iTreeSel)->GetDescription() );
		Append(IDM_CASCADE_ADD_SELECTED_MODULE, *NewModuleString, TEXT(""));
	    AppendSeparator();
	}

	if ((UINT)eMode & TYPEDATAS_ONLY)
	{
		// add the data type modules to the menu
		for (i = 0; i < TypeDataModuleEntries.Num(); i++)
		{
			Append(IDM_CASCADE_NEW_MODULE_START + TypeDataModuleIndices(i), *TypeDataModuleEntries(i), TEXT(""));
		}
		if (TypeDataModuleEntries.Num())
			AppendSeparator();
	}

	if ((UINT)eMode & TYPEDATAS_ONLY)
	{
		// Add each module type to menu.
		for (i = 0; i < ModuleEntries.Num(); i++)
		{
			Append(IDM_CASCADE_NEW_MODULE_START + ModuleIndices(i), *ModuleEntries(i), TEXT(""));
		}

		if (ModuleEntries.Num())
			AppendSeparator();
	}

	if ((UINT)eMode & EMITTER_ONLY)
	{
		Append(IDM_CASCADE_DUPLICATE_EMITTER, TEXT("Duplicate Emitter"), TEXT(""));
		Append(IDM_CASCADE_DELETE_EMITTER, TEXT("Delete Emitter"), TEXT(""));
		if (Cascade->CopyModule)
		{
			AppendSeparator();
			Append(IDM_CASCADE_PASTE_MODULE, TEXT("Paste Module"), TEXT(""));
		}
	}
}

WxMBCascadeEmitterBkg::~WxMBCascadeEmitterBkg()
{

}

/*-----------------------------------------------------------------------------
	WxMBCascadeBkg
-----------------------------------------------------------------------------*/

WxMBCascadeBkg::WxMBCascadeBkg(WxCascade* Cascade)
{
	for(INT i=0; i<Cascade->ParticleEmitterClasses.Num(); i++)
	{
		UParticleEmitter* DefEmitter = 
			(UParticleEmitter*)Cascade->ParticleEmitterClasses(i)->GetDefaultObject();
		if ((Cascade->ParticleEmitterClasses(i) == UParticleSpriteEmitter::StaticClass()) &&
			(Cascade->ParticleEmitterClasses(i) != UParticleSpriteSubUVEmitter::StaticClass()))
		{
			FString NewModuleString = FString::Printf( TEXT("New %s"), *Cascade->ParticleEmitterClasses(i)->GetDescription() );
			Append(IDM_CASCADE_NEW_EMITTER_START+i, *NewModuleString, TEXT(""));

			if ((i + 1) == IDM_CASCADE_NEW_EMITTER_END - IDM_CASCADE_NEW_EMITTER_START)
			{
				appMsgf(0, TEXT("ERROR!\nThere are more emitter types than available menu slots!"));
				break;
			}
		}
	}

	if (Cascade->SelectedEmitter)
	{
		AppendSeparator();
		// Only put the delete option in if there is a selected emitter!
		Append( IDM_CASCADE_DELETE_EMITTER, TEXT("Delete Emitter"), TEXT("") );
	}
}

WxMBCascadeBkg::~WxMBCascadeBkg()
{

}

/*-----------------------------------------------------------------------------
	WxCascadeToolBar
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxCascadeToolBar, wxToolBar )
END_EVENT_TABLE()

WxCascadeToolBar::WxCascadeToolBar( wxWindow* InParent, wxWindowID InID )
: wxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_3DBUTTONS )
{
	AddSeparator();

	SaveCamB.Load( TEXT("CASC_SaveCam") );
	ResetSystemB.Load( TEXT("CASC_ResetSystem") );
	OrbitModeB.Load(TEXT("CASC_OrbitMode"));
    
	PlayB.Load(TEXT("CASC_Speed_Play"));
	PauseB.Load(TEXT("CASC_Speed_Pause"));
	Speed1B.Load(TEXT("CASC_Speed_1"));
	Speed10B.Load(TEXT("CASC_Speed_10"));
	Speed25B.Load(TEXT("CASC_Speed_25"));
	Speed50B.Load(TEXT("CASC_Speed_50"));
	Speed100B.Load(TEXT("CASC_Speed_100"));

	LoopSystemB.Load(TEXT("CASC_LoopSystem"));

	SetToolBitmapSize( wxSize( 18, 18 ) );

	AddTool( IDM_CASCADE_RESETSYSTEM, ResetSystemB, TEXT("Restart Sim") );
	//AddCheckTool( IDM_CASCADE_SIM_RESTARTONFINISH, ResetSystemB, NULL, TEXT("Loop Simulation") )
	AddSeparator();
	AddTool( IDM_CASCADE_SAVECAM, SaveCamB, TEXT("Save Camera Position") );
	AddSeparator();
	AddCheckTool(IDM_CASCADE_ORBITMODE, TEXT("Toggle Orbit Mode"), OrbitModeB);
	ToggleTool(IDM_CASCADE_ORBITMODE, true);

	AddSeparator();

	AddRadioTool(IDM_CASCADE_PLAY, TEXT("Play"), PlayB);
	AddRadioTool(IDM_CASCADE_PAUSE, TEXT("Pause"), PauseB);
	ToggleTool(IDM_CASCADE_PLAY, true);

	AddSeparator();

	AddRadioTool(IDM_CASCADE_SPEED_100,	TEXT("Full Speed"), Speed100B);
	AddRadioTool(IDM_CASCADE_SPEED_50,	TEXT("50% Speed"), Speed50B);
	AddRadioTool(IDM_CASCADE_SPEED_25,	TEXT("25% Speed"), Speed25B);
	AddRadioTool(IDM_CASCADE_SPEED_10,	TEXT("10% Speed"), Speed10B);
	AddRadioTool(IDM_CASCADE_SPEED_1,	TEXT("1% Speed"), Speed1B);
	ToggleTool(IDM_CASCADE_SPEED_100, true);

	AddSeparator();

	AddCheckTool(IDM_CASCADE_LOOPSYSTEM, TEXT("Toggle Loop System"), LoopSystemB);
	ToggleTool(IDM_CASCADE_LOOPSYSTEM, true);

	Realize();
}

WxCascadeToolBar::~WxCascadeToolBar()
{
}
