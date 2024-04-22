/*=============================================================================
	CascadeMenus.cpp: 'Cascade' particle editor menus
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "Cascade.h"

/*-----------------------------------------------------------------------------
	WxCascadeMenuBar
-----------------------------------------------------------------------------*/

WxCascadeMenuBar::WxCascadeMenuBar(WxCascade* Cascade)
{
	EditMenu = new wxMenu();
	Append( EditMenu, *LocalizeUnrealEd("Edit") );
#if 0
	EditMenu->Append(IDM_CASCADE_RESET_PEAK_COUNTS,		*LocalizeUnrealEd("ResetPeakCounts"),		TEXT("") );
	EditMenu->AppendSeparator();
	EditMenu->Append(IDM_CASCADE_CONVERT_TO_UBER,		*LocalizeUnrealEd("UberConvert"),			TEXT("") );
	EditMenu->AppendSeparator();
#endif
	EditMenu->Append(IDM_CASCADE_REGENERATE_LOWESTLOD,	*LocalizeUnrealEd("CascadeRegenLowestLOD"),	TEXT("") );
	EditMenu->AppendSeparator();
	EditMenu->Append(IDM_CASCADE_SAVE_PACKAGE,			*LocalizeUnrealEd("CascadeSavePackage"),	TEXT("") );

	ViewMenu = new wxMenu();
	Append( ViewMenu, *LocalizeUnrealEd("View") );

	ViewMenu->AppendCheckItem( IDM_CASCADE_VIEW_AXES, *LocalizeUnrealEd("ViewOriginAxes"), TEXT("") );
	ViewMenu->AppendCheckItem(IDM_CASCADE_VIEW_COUNTS, *LocalizeUnrealEd("ViewParticleCounts"), TEXT(""));
	ViewMenu->Check(IDM_CASCADE_VIEW_COUNTS, Cascade->PreviewVC->bDrawParticleCounts == TRUE);
	ViewMenu->AppendCheckItem(IDM_CASCADE_VIEW_EVENTS, *LocalizeUnrealEd("ViewParticleEvents"), TEXT(""));
	ViewMenu->Check(IDM_CASCADE_VIEW_EVENTS, Cascade->PreviewVC->bDrawParticleEvents == TRUE);
	ViewMenu->AppendCheckItem(IDM_CASCADE_VIEW_TIMES, *LocalizeUnrealEd("ViewParticleTimes"), TEXT(""));
	ViewMenu->Check(IDM_CASCADE_VIEW_TIMES, Cascade->PreviewVC->bDrawParticleTimes == TRUE);
	ViewMenu->AppendCheckItem(IDM_CASCADE_VIEW_DISTANCE, *LocalizeUnrealEd("ViewParticleDistance"), TEXT(""));
	ViewMenu->Check(IDM_CASCADE_VIEW_DISTANCE, Cascade->PreviewVC->bDrawSystemDistance == TRUE);
	ViewMenu->AppendCheckItem(IDM_CASCADE_VIEW_MEMORY, *LocalizeUnrealEd("ViewParticleMemory"), TEXT(""));
	ViewMenu->Check(IDM_CASCADE_VIEW_MEMORY, Cascade->PreviewVC->bDrawParticleMemory == TRUE);
	ViewMenu->AppendCheckItem(IDM_CASCADE_VIEW_GEOMETRY, *LocalizeUnrealEd("ViewParticleGeometry"), TEXT(""));
	ViewMenu->Check(IDM_CASCADE_VIEW_GEOMETRY, Cascade->EditorOptions->bShowFloor == TRUE);
	ViewMenu->Append(IDM_CASCADE_VIEW_GEOMETRY_PROPERTIES, *LocalizeUnrealEd("Cascade_ViewGeometryProperties"),	TEXT("") );
	ViewMenu->AppendSeparator();
	ViewMenu->Append( IDM_CASCADE_SAVECAM, *LocalizeUnrealEd("SaveCamPosition"), TEXT("") );
#if defined(_CASCADE_ENABLE_MODULE_DUMP_)
	ViewMenu->AppendCheckItem(IDM_CASCADE_VIEW_DUMP, *LocalizeUnrealEd("ViewModuleDump"), TEXT(""));
	ViewMenu->Check(IDM_CASCADE_VIEW_DUMP, Cascade->EditorOptions->bShowModuleDump == TRUE);
#endif	//#if defined(_CASCADE_ENABLE_MODULE_DUMP_)
	ViewMenu->AppendSeparator();
	ViewMenu->AppendCheckItem(IDM_CASCADE_SET_MOTION_RADIUS, *LocalizeUnrealEd("SetMotionRadius"), TEXT(""));
	ViewMenu->AppendSeparator();
	DetailModeMenu = new wxMenu();
	DetailModeMenu->AppendCheckItem( IDM_CASCADE_DETAILMODE_LOW, *LocalizeUnrealEd("Low"), *LocalizeUnrealEd("Low") );
	DetailModeMenu->AppendCheckItem( IDM_CASCADE_DETAILMODE_MEDIUM, *LocalizeUnrealEd("Medium"), *LocalizeUnrealEd("Medium") );
	DetailModeMenu->AppendCheckItem( IDM_CASCADE_DETAILMODE_HIGH, *LocalizeUnrealEd("High"), *LocalizeUnrealEd("High") );
	ViewMenu->Append( IDM_VIEW_DETAILMODE, *LocalizeUnrealEd("DetailMode"), DetailModeMenu );
}

WxCascadeMenuBar::~WxCascadeMenuBar()
{

}

/*-----------------------------------------------------------------------------
	WxMBCascadeModule
-----------------------------------------------------------------------------*/

WxMBCascadeModule::WxMBCascadeModule(WxCascade* Cascade)
{
	Append(IDM_CASCADE_DELETE_MODULE,	*LocalizeUnrealEd("DeleteModule"),	TEXT(""));
	Append(IDM_CASCADE_REFRESH_MODULE, *LocalizeUnrealEd("Casc_RefreshModule"), TEXT(""));

	if (Cascade->SelectedModule != NULL)
	{
		if (Cascade->SelectedModule->IsA(UParticleModuleRequired::StaticClass()))
		{
			AppendSeparator();
			Append(IDM_CASCADE_MODULE_SYNCMATERIAL,	*LocalizeUnrealEd("Casc_SyncMaterial"),	TEXT(""));
			Append(IDM_CASCADE_MODULE_USEMATERIAL,	*LocalizeUnrealEd("Casc_UseMaterial"),	TEXT(""));
		}

		INT CurrLODLevel = Cascade->GetCurrentlySelectedLODLevelIndex();
		if (CurrLODLevel > 0)
		{
			UBOOL bAddDuplicateOptions = TRUE;
			if (Cascade->SelectedModule)
			{
				if (Cascade->ModuleIsShared(Cascade->SelectedModule) == TRUE)
				{
					bAddDuplicateOptions = FALSE;
				}
			}

			if (bAddDuplicateOptions == TRUE)
			{
				AppendSeparator();
				Append(IDM_CASCADE_MODULE_DUPHIGH,		*LocalizeUnrealEd("DuplicateFromHigher"),	TEXT(""));
				Append(IDM_CASCADE_MODULE_SHAREHIGH,	*LocalizeUnrealEd("ShareWithHigher"),		TEXT(""));
				Append(IDM_CASCADE_MODULE_DUPHIGHEST,	*LocalizeUnrealEd("DuplicateFromHighest"),	TEXT(""));
			}
			else
			{
				// It's shared... add an unshare option
			}
		}

		if (Cascade->SelectedModule->SupportsRandomSeed())
		{
			AppendSeparator();
			Append(IDM_CASCADE_MODULE_SETRANDOMSEED, *LocalizeUnrealEd("CASC_ModuleSetRandomSeed"), TEXT(""));
		}
		else
		{
			if (CurrLODLevel == 0)
			{
				// See if there is a seeded version of this module...
				FString ClassName = Cascade->SelectedModule->GetClass()->GetName();
				debugf(TEXT("Non-seeded module %s"), *ClassName);
				// This only works if the seeded version is names <ClassName>_Seeded!!!!
				FString SeededClassName = ClassName + TEXT("_Seeded");
				if (FindObject<UClass>(ANY_PACKAGE, *SeededClassName) != NULL)
				{
					AppendSeparator();
					Append(IDM_CASCADE_MODULE_CONVERTTOSEEDED,	*LocalizeUnrealEd("CASC_ModuleConvertToSeeded"), TEXT(""));
				}
			}
		}

		INT CustomEntryCount = Cascade->SelectedModule->GetNumberOfCustomMenuOptions();
		if (CustomEntryCount > 0)
		{
			AppendSeparator();
			for (INT EntryIdx = 0; EntryIdx < CustomEntryCount; EntryIdx++)
			{
				FString DisplayString;
				if (Cascade->SelectedModule->GetCustomMenuEntryDisplayString(EntryIdx, DisplayString) == TRUE)
				{
					Append(IDM_CASCADE_MODULE_CUSTOM0 + EntryIdx, *DisplayString, TEXT(""));
				}
			}
		}
	}
}

WxMBCascadeModule::~WxMBCascadeModule()
{

}

/*-----------------------------------------------------------------------------
	WxMBCascadeEmitterBkg
-----------------------------------------------------------------------------*/
UBOOL WxMBCascadeEmitterBkg::InitializedModuleEntries = FALSE;
TArray<FString>	WxMBCascadeEmitterBkg::TypeDataModuleEntries;
TArray<INT>		WxMBCascadeEmitterBkg::TypeDataModuleIndices;
TArray<FString>	WxMBCascadeEmitterBkg::ModuleEntries;
TArray<INT>		WxMBCascadeEmitterBkg::ModuleIndices;

WxMBCascadeEmitterBkg::WxMBCascadeEmitterBkg(WxCascade* Cascade, WxMBCascadeEmitterBkg::Mode eMode)
{
	// Don't show options if no emitter selected.
	if (!Cascade->SelectedEmitter)
		return;

	EmitterMenu = 0;
	PSysMenu = NULL;
	SelectedModuleMenu = 0;
	TypeDataMenu = 0;
	NonTypeDataMenus.Empty();

	InitializeModuleEntries(Cascade);

	check(TypeDataModuleEntries.Num() == TypeDataModuleIndices.Num());
	check(ModuleEntries.Num() == ModuleIndices.Num());

	AddEmitterEntries(Cascade, eMode);
	AddSelectedModuleEntries(Cascade, eMode);
	AddPSysEntries(Cascade, eMode);
	AddTypeDataEntries(Cascade, eMode);
	AddNonTypeDataEntries(Cascade, eMode);
}

WxMBCascadeEmitterBkg::~WxMBCascadeEmitterBkg()
{

}

void WxMBCascadeEmitterBkg::InitializeModuleEntries(WxCascade* Cascade)
{
    INT i;
    UBOOL bFoundTypeData = FALSE;
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
			FString ClassName = Cascade->ParticleModuleClasses(i)->GetName();
			if (Cascade->ParticleModuleClasses(i)->IsChildOf(UParticleModuleTypeDataBase::StaticClass()))
			{
				if (Cascade->IsModuleSuitableForModuleMenu(ClassName) == TRUE)
				{
					bFoundTypeData = TRUE;
					FString NewModuleString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("New_F"), *Cascade->ParticleModuleClasses(i)->GetDescription()) );
					TypeDataModuleEntries.AddItem(NewModuleString);
					TypeDataModuleIndices.AddItem(i);
				}
			}
		}

		// Add each module type to menu.
		for(i = 0; i < Cascade->ParticleModuleClasses.Num(); i++)
		{
			DefModule = (UParticleModule*)Cascade->ParticleModuleClasses(i)->GetDefaultObject();
			FString ClassName = Cascade->ParticleModuleClasses(i)->GetName();
			if (Cascade->ParticleModuleClasses(i)->IsChildOf(UParticleModuleTypeDataBase::StaticClass()) == FALSE)
			{
				if (Cascade->IsModuleSuitableForModuleMenu(ClassName) == TRUE)
				{
					FString NewModuleString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("New_F"), *Cascade->ParticleModuleClasses(i)->GetDescription()) );
					ModuleEntries.AddItem(NewModuleString);
					ModuleIndices.AddItem(i);
				}
			}
		}
		InitializedModuleEntries = TRUE;
	}
}

void WxMBCascadeEmitterBkg::AddEmitterEntries(WxCascade* Cascade, WxMBCascadeEmitterBkg::Mode eMode)
{
	if ((UINT)eMode & EMITTER_ONLY)
	{
		if (Cascade->EditorOptions->bUseSubMenus == FALSE)
		{
			Append(IDM_CASCADE_RENAME_EMITTER, *LocalizeUnrealEd("RenameEmitter"), TEXT(""));
			Append(IDM_CASCADE_DUPLICATE_EMITTER, *LocalizeUnrealEd("DuplicateEmitter"), TEXT(""));
			Append(IDM_CASCADE_DUPLICATE_SHARE_EMITTER, *LocalizeUnrealEd("DuplicateShareEmitter"), TEXT(""));
			Append(IDM_CASCADE_DELETE_EMITTER, *LocalizeUnrealEd("DeleteEmitter"), TEXT(""));
			Append(IDM_CASCADE_EXPORT_EMITTER, *LocalizeUnrealEd("ExportEmitter"), TEXT(""));
			Append(IDM_CASCADE_EXPORT_ALL, *LocalizeUnrealEd("ExportAllEmitters"), TEXT(""));
		}
		else
		{
			EmitterMenu = new wxMenu();
			EmitterMenu->Append(IDM_CASCADE_RENAME_EMITTER, *LocalizeUnrealEd("RenameEmitter"), TEXT(""));
			EmitterMenu->Append(IDM_CASCADE_DUPLICATE_EMITTER, *LocalizeUnrealEd("DuplicateEmitter"), TEXT(""));
			EmitterMenu->Append(IDM_CASCADE_DUPLICATE_SHARE_EMITTER, *LocalizeUnrealEd("DuplicateShareEmitter"), TEXT(""));
			EmitterMenu->Append(IDM_CASCADE_DELETE_EMITTER, *LocalizeUnrealEd("DeleteEmitter"), TEXT(""));
			EmitterMenu->Append(IDM_CASCADE_EXPORT_EMITTER, *LocalizeUnrealEd("ExportEmitter"), TEXT(""));
			EmitterMenu->Append(IDM_CASCADE_EXPORT_ALL, *LocalizeUnrealEd("ExportAllEmitters"), TEXT(""));
			Append(IDMENU_CASCADE_POPUP_EMITTER, *LocalizeUnrealEd("Emitter"), EmitterMenu);
		}

		AppendSeparator();

	}
}

void WxMBCascadeEmitterBkg::AddPSysEntries(WxCascade* Cascade, Mode eMode)
{
	if ((UINT)eMode & PSYS_ONLY)
	{
		if (Cascade->EditorOptions->bUseSubMenus == FALSE)
		{
			Append(IDM_CASCADE_SELECT_PARTICLESYSTEM, *LocalizeUnrealEd("SelectParticleSystem"), TEXT(""));
			if (Cascade->SelectedEmitter != NULL)
			{
				Append(IDM_CASCADE_PSYS_NEW_EMITTER_BEFORE, *LocalizeUnrealEd("NewEmitterLeft"), TEXT(""));
				Append(IDM_CASCADE_PSYS_NEW_EMITTER_AFTER, *LocalizeUnrealEd("NewEmitterRight"), TEXT(""));
			}
			Append(IDM_CSACADE_PSYS_REMOVE_DUPLICATE_MODULES, *LocalizeUnrealEd("RemoveDuplicateModules"), TEXT(""));
		}
		else
		{
			PSysMenu = new wxMenu();
			PSysMenu->Append(IDM_CASCADE_SELECT_PARTICLESYSTEM, *LocalizeUnrealEd("SelectParticleSystem"), TEXT(""));
			if (Cascade->SelectedEmitter != NULL)
			{
				PSysMenu->Append(IDM_CASCADE_PSYS_NEW_EMITTER_BEFORE, *LocalizeUnrealEd("NewEmitterLeft"), TEXT(""));
				PSysMenu->Append(IDM_CASCADE_PSYS_NEW_EMITTER_AFTER, *LocalizeUnrealEd("NewEmitterRight"), TEXT(""));
			}
			PSysMenu->Append(IDM_CSACADE_PSYS_REMOVE_DUPLICATE_MODULES, *LocalizeUnrealEd("RemoveDuplicateModules"), TEXT(""));
			Append(IDMENU_CASCADE_POPUP_PSYS, *LocalizeUnrealEd("PSys"), PSysMenu);
		}

		AppendSeparator();
	}
}

void WxMBCascadeEmitterBkg::AddSelectedModuleEntries(WxCascade* Cascade, WxMBCascadeEmitterBkg::Mode eMode)
{
}

void WxMBCascadeEmitterBkg::AddTypeDataEntries(WxCascade* Cascade, WxMBCascadeEmitterBkg::Mode eMode)
{
	if ((UINT)eMode & TYPEDATAS_ONLY)
	{
		if (TypeDataModuleEntries.Num())
		{
			if (Cascade->EditorOptions->bUseSubMenus == FALSE)
			{
				// add the data type modules to the menu
				for (INT i = 0; i < TypeDataModuleEntries.Num(); i++)
				{
					Append(IDM_CASCADE_NEW_MODULE_START + TypeDataModuleIndices(i), 
						*TypeDataModuleEntries(i), TEXT(""));
				}
			}
			else
			{
				TypeDataMenu = new wxMenu();
				for (INT i = 0; i < TypeDataModuleEntries.Num(); i++)
				{
					TypeDataMenu->Append(IDM_CASCADE_NEW_MODULE_START + TypeDataModuleIndices(i), 
						*TypeDataModuleEntries(i), TEXT(""));
				}
				Append(IDMENU_CASCADE_POPUP_TYPEDATA, *LocalizeUnrealEd("TypeData"), TypeDataMenu);
			}
			AppendSeparator();
		}
	}
}

void WxMBCascadeEmitterBkg::AddNonTypeDataEntries(WxCascade* Cascade, WxMBCascadeEmitterBkg::Mode eMode)
{
	if ((UINT)eMode & NON_TYPEDATAS)
	{
		if (ModuleEntries.Num())
		{
			if (Cascade->EditorOptions->bUseSubMenus == FALSE)
			{
				// Add each module type to menu.
				for (INT i = 0; i < ModuleEntries.Num(); i++)
				{
					Append(IDM_CASCADE_NEW_MODULE_START + ModuleIndices(i), *ModuleEntries(i), TEXT(""));
				}
			}
			else
			{
				UBOOL bFoundTypeData = FALSE;
				INT i, j;
				INT iIndex = 0;
				FString ModuleName;

				// Now, for each module base type, add another branch
				for (i = 0; i < Cascade->ParticleModuleBaseClasses.Num(); i++)
				{
					ModuleName = Cascade->ParticleModuleBaseClasses(i)->GetName();
					if (Cascade->IsModuleSuitableForModuleMenu(ModuleName) &&
						Cascade->IsBaseModuleTypeDataPairSuitableForModuleMenu(ModuleName))
					{
						// Add the 'label'
						wxMenu* pkNewMenu = new wxMenu();

						// Search for all modules of this type
						for (j = 0; j < Cascade->ParticleModuleClasses.Num(); j++)
						{
							if (Cascade->ParticleModuleClasses(j)->IsChildOf(Cascade->ParticleModuleBaseClasses(i)))
							{
								ModuleName = Cascade->ParticleModuleClasses(j)->GetName();
								if (Cascade->IsModuleSuitableForModuleMenu(ModuleName) &&
									Cascade->IsModuleTypeDataPairSuitableForModuleMenu(ModuleName))
								{
									pkNewMenu->Append(
										IDM_CASCADE_NEW_MODULE_START + j, 
										*(Cascade->ParticleModuleClasses(j)->GetDescription()), TEXT(""));
								}
							}
						}

						Append(IDMENU_CASCADE_POPUP_NONTYPEDATA_START + iIndex, 
							*(Cascade->ParticleModuleBaseClasses(i)->GetDescription()), pkNewMenu);
						NonTypeDataMenus.AddItem(pkNewMenu);

						iIndex++;
						check(IDMENU_CASCADE_POPUP_NONTYPEDATA_START + iIndex < IDMENU_CASCADE_POPUP_NONTYPEDATA_END);
					}
				}
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	WxMBCascadeBkg
-----------------------------------------------------------------------------*/

WxMBCascadeBkg::WxMBCascadeBkg(WxCascade* Cascade)
{
	UParticleEmitter* DefEmitter = Cast<UParticleEmitter>(UParticleSpriteEmitter::StaticClass()->GetDefaultObject());
	FString NewModuleString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("New_F"), *(UParticleSpriteEmitter::StaticClass()->GetDescription())) );
	Append(IDM_CASCADE_NEW_EMITTER_START, *NewModuleString, TEXT(""));
	if (Cascade->SelectedEmitter)
	{
		AppendSeparator();
		// Only put the delete option in if there is a selected emitter!
		Append( IDM_CASCADE_DELETE_EMITTER, *LocalizeUnrealEd("DeleteEmitter"), TEXT("") );
	}
}

WxMBCascadeBkg::~WxMBCascadeBkg()
{

}

/*-----------------------------------------------------------------------------
  WxCascadeViewModeMenu
-----------------------------------------------------------------------------*/
WxCascadeViewModeMenu::WxCascadeViewModeMenu(WxCascade* Cascade)
{
	check(Cascade);

	ViewModeFlagData.AddItem(FCascViewModeFlagData(IDM_WIREFRAME,		*LocalizeUnrealEd("Wireframe")));
	ViewModeFlagData.AddItem(FCascViewModeFlagData(IDM_UNLIT,			*LocalizeUnrealEd("Unlit")));
	ViewModeFlagData.AddItem(FCascViewModeFlagData(IDM_LIT,				*LocalizeUnrealEd("Lit")));
	ViewModeFlagData.AddItem(FCascViewModeFlagData(IDM_TEXTUREDENSITY,	*LocalizeUnrealEd("TextureDensity")));
	ViewModeFlagData.AddItem(FCascViewModeFlagData(IDM_SHADERCOMPLEXITY,*LocalizeUnrealEd("ShaderComplexity")));

	EShowFlags CurrentShowFlags = Cascade->PreviewVC ? (Cascade->PreviewVC->ShowFlags & SHOW_ViewMode_Mask) : ~EShowFlags();
	for (INT i = 0; i < ViewModeFlagData.Num(); ++i)
	{
		const FCascViewModeFlagData& VMFlagData = ViewModeFlagData(i);

		AppendCheckItem(VMFlagData.ID, *VMFlagData.Name);
		if (Cascade->PreviewVC)
		{
			EShowFlags CheckShowFlags = GetShowFlag(VMFlagData.ID) & SHOW_ViewMode_Mask;
			if (CurrentShowFlags == CheckShowFlags)
			{
				Check(VMFlagData.ID, TRUE);
			}
			else
			{
				Check(VMFlagData.ID, FALSE);
			}
		}
		else
		{
			Check(VMFlagData.ID, FALSE);
		}
	}
}

WxCascadeViewModeMenu::~WxCascadeViewModeMenu()
{
}

/*-----------------------------------------------------------------------------
  WxCascadeDetailModeMenu
-----------------------------------------------------------------------------*/
WxCascadeDetailModeMenu::WxCascadeDetailModeMenu(WxCascade* Cascade)
{
	check(Cascade && Cascade->PreviewVC);

	CascadeWindow = Cascade;

	wxEvtHandler* EvtHandler = GetEventHandler();

	AppendCheckItem(DM_Low, *LocalizeUnrealEd("Low"));
	Check(DM_Low, (CascadeWindow->PreviewVC->DetailMode == DM_Low));
	EvtHandler->Connect(DM_Low, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(WxCascadeDetailModeMenu::OnDetailModeButton));
	AppendCheckItem(DM_Medium, *LocalizeUnrealEd("Medium"));
	Check(DM_Medium, (CascadeWindow->PreviewVC->DetailMode == DM_Medium));
	EvtHandler->Connect(DM_Medium, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(WxCascadeDetailModeMenu::OnDetailModeButton));
	AppendCheckItem(DM_High, *LocalizeUnrealEd("High"));
	Check(DM_High, (CascadeWindow->PreviewVC->DetailMode == DM_High));
	EvtHandler->Connect(DM_High, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(WxCascadeDetailModeMenu::OnDetailModeButton));
}

WxCascadeDetailModeMenu::~WxCascadeDetailModeMenu()
{
}

void WxCascadeDetailModeMenu::OnDetailModeButton( wxCommandEvent& In )
{
	CascadeWindow->OnSetDetailMode(In.GetId());
}

/*-----------------------------------------------------------------------------
	WxCascadeSimSpeedMenu
-----------------------------------------------------------------------------*/
WxCascadeSimSpeedMenu::WxCascadeSimSpeedMenu(WxCascade* Cascade)
{
	TArray<FCascSimSpeedFlagData> SimSpeedFlagData;
	check(Cascade);

	SimSpeedFlagData.AddItem(FCascSimSpeedFlagData(IDM_CASCADE_SPEED_100,	*LocalizeUnrealEd("FullSpeed")));
	SimSpeedFlagData.AddItem(FCascSimSpeedFlagData(IDM_CASCADE_SPEED_50,	*LocalizeUnrealEd("50Speed")));
	SimSpeedFlagData.AddItem(FCascSimSpeedFlagData(IDM_CASCADE_SPEED_25,	*LocalizeUnrealEd("25Speed")));
	SimSpeedFlagData.AddItem(FCascSimSpeedFlagData(IDM_CASCADE_SPEED_10,	*LocalizeUnrealEd("10Speed")));
	SimSpeedFlagData.AddItem(FCascSimSpeedFlagData(IDM_CASCADE_SPEED_1,		*LocalizeUnrealEd("1Speed")));

	INT CurrentID = IDM_CASCADE_SPEED_100;
	FLOAT CurrentSpeed = Cascade->PreviewVC ? Cascade->PreviewVC->TimeScale : 1.0f;
	if (CurrentSpeed == 1.0f)
	{
		CurrentID = IDM_CASCADE_SPEED_100;
	}
	else
	if (CurrentSpeed == 0.5f)
	{
		CurrentID = IDM_CASCADE_SPEED_50;
	}
	else
	if (CurrentSpeed == 0.25f)
	{
		CurrentID = IDM_CASCADE_SPEED_25;
	}
	else
	if (CurrentSpeed == 0.1f)
	{
		CurrentID = IDM_CASCADE_SPEED_10;
	}
	else
	if (CurrentSpeed == 0.01f)
	{
		CurrentID = IDM_CASCADE_SPEED_1;
	}

	for (INT i = 0; i < SimSpeedFlagData.Num(); ++i)
	{
		const FCascSimSpeedFlagData& SpeedFlagData = SimSpeedFlagData(i);

		AppendCheckItem(SpeedFlagData.ID, *SpeedFlagData.Name);
		if (CurrentID == SpeedFlagData.ID)
		{
			Check(SpeedFlagData.ID, TRUE);
		}
		else
		{
			Check(SpeedFlagData.ID, FALSE);
		}
	}
}

WxCascadeSimSpeedMenu::~WxCascadeSimSpeedMenu()
{
}

/*-----------------------------------------------------------------------------
	WxCascadeToolBar
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxCascadeToolBar, WxToolBar )
END_EVENT_TABLE()

WxCascadeToolBar::WxCascadeToolBar( wxWindow* InParent, wxWindowID InID )
: WxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_3DBUTTONS )
{
	Cascade	= (WxCascade*)InParent;

	AddSeparator();

	SaveCamB.Load( TEXT("CASC_SaveCam") );
	ResetSystemB.Load( TEXT("CASC_ResetSystem") );
	RestartInLevelB.Load( TEXT("CASC_ResetInLevel") );
	SyncGenericBrowserB.Load(TEXT("CASC_Prop_Browse"));
	OrbitModeB.Load(TEXT("CASC_OrbitMode"));
	MotionB.Load(TEXT("CASC_Motion"));
	WireframeB.Load(TEXT("CASC_Wireframe"));
	UnlitB.Load(TEXT("CASC_Unlit"));
	LitB.Load(TEXT("CASC_Lit"));
	TextureDensityB.Load(TEXT("CASC_TextureDensity"));
	ShaderComplexityB.Load(TEXT("CASC_ShaderComplexity"));
	BoundsB.Load(TEXT("CASC_Bounds"));
	PostProcessB.Load(TEXT("CASC_PostProcess"));
	ToggleGridB.Load(TEXT("CASC_ToggleGrid"));

	BackgroundColorB.Load(TEXT("CASC_BackColor"));
	WireSphereB.Load(TEXT("CASC_Sphere"));

	UndoB.Load(TEXT("CASC_Undo"));
	RedoB.Load(TEXT("CASC_Redo"));
    
	PlayB.Load(TEXT("CASC_Speed_Play"));
	PauseB.Load(TEXT("CASC_Speed_Pause"));
	Speed1B.Load(TEXT("CASC_Speed_1"));
	Speed10B.Load(TEXT("CASC_Speed_10"));
	Speed25B.Load(TEXT("CASC_Speed_25"));
	Speed50B.Load(TEXT("CASC_Speed_50"));
	Speed100B.Load(TEXT("CASC_Speed_100"));

	LoopSystemB.Load(TEXT("CASC_LoopSystem"));

	DetailLow.Load(TEXT("CASC_DetailLow"));
	DetailMedium.Load(TEXT("CASC_DetailMedium"));
	DetailHigh.Load(TEXT("CASC_DetailHigh"));

	LODHigh.Load(TEXT("CASC_LODHigh"));
	LODHigher.Load(TEXT("CASC_LODHigher"));
	LODLower.Load(TEXT("CASC_LODLower"));
	LODLow.Load(TEXT("CASC_LODLow"));

	LODAddBefore.Load(TEXT("CASC_LODAddBefore"));
	LODAddAfter.Load(TEXT("CASC_LODAddAfter"));

	LODDelete.Load(TEXT("CASC_LODDelete"));
	LODRegenerate.Load(TEXT("CASC_LODRegen"));
	LODRegenerateDuplicate.Load(TEXT("CASC_LODRegenDup"));

	RealtimeB.Load(TEXT("CASC_Realtime"));

	SetToolBitmapSize( wxSize( 18, 18 ) );

	AddTool( IDM_CASCADE_RESETSYSTEM, ResetSystemB, *LocalizeUnrealEd("RestartSim") );
	AddTool(IDM_CASCADE_RESETINLEVEL, RestartInLevelB, *LocalizeUnrealEd("RestartInLevel"));
	AddSeparator();
	AddTool(IDM_CASCADE_SYNCGENERICBROWSER, SyncGenericBrowserB, *LocalizeUnrealEd("SyncContentBrowser"));
	AddSeparator();
	AddTool( IDM_CASCADE_SAVECAM, SaveCamB, *LocalizeUnrealEd("SaveCameraPosition") );
	AddSeparator();
	AddCheckTool(IDM_CASCADE_ORBITMODE, *LocalizeUnrealEd("ToggleOrbitMode"), OrbitModeB, wxNullBitmap, *LocalizeUnrealEd("ToggleOrbitMode"));
	ToggleTool(IDM_CASCADE_ORBITMODE, TRUE);
	AddCheckTool(IDM_CASCADE_MOTION, *LocalizeUnrealEd("ToggleMotionMode"), MotionB, wxNullBitmap, *LocalizeUnrealEd("ToggleMotionMode"));
	ToggleTool(IDM_CASCADE_MOTION, FALSE);
	AddTool(IDM_CASCADE_VIEWMODE, LitB, *LocalizeUnrealEd("ChangeViewMode"));
	AddCheckTool(IDM_CASCADE_BOUNDS, *LocalizeUnrealEd("ToggleBounds"), BoundsB, wxNullBitmap, *LocalizeUnrealEd("ToggleBounds"));
	ToggleTool(IDM_CASCADE_BOUNDS, FALSE);
	AddCheckTool(IDM_CASCADE_POSTPROCESS, *LocalizeUnrealEd("TogglePostProcess"), PostProcessB, wxNullBitmap, *LocalizeUnrealEd("TogglePostProcess"));
	AddCheckTool(IDM_CASCADE_TOGGLEGRID, *LocalizeUnrealEd("Casc_ToggleGrid"), ToggleGridB, wxNullBitmap, *LocalizeUnrealEd("Casc_ToggleGrid"));
	ToggleTool(IDM_CASCADE_TOGGLEGRID, TRUE);

	AddSeparator();

	AddRadioTool(IDM_CASCADE_PLAY, *LocalizeUnrealEd("Play"), PlayB, wxNullBitmap, *LocalizeUnrealEd("Play"));
	AddRadioTool(IDM_CASCADE_PAUSE, *LocalizeUnrealEd("Pause"), PauseB, wxNullBitmap, *LocalizeUnrealEd("Pause"));
	ToggleTool(IDM_CASCADE_PLAY, TRUE);
	AddTool(IDM_CASCADE_SET_SPEED, Speed100B, *LocalizeUnrealEd("SetSpeed"));

	AddSeparator();

	AddCheckTool(IDM_CASCADE_LOOPSYSTEM, *LocalizeUnrealEd("ToggleLoopSystem"), LoopSystemB, wxNullBitmap, *LocalizeUnrealEd("ToggleLoopSystem"));
	ToggleTool(IDM_CASCADE_LOOPSYSTEM, TRUE);

	AddCheckTool(IDM_CASCADE_REALTIME, *LocalizeUnrealEd("ToggleRealtime"), RealtimeB, wxNullBitmap, *LocalizeUnrealEd("ToggleRealtime"));
	ToggleTool(IDM_CASCADE_REALTIME, TRUE);
	bRealtime	= TRUE;

	AddSeparator();
	AddTool(IDM_CASCADE_BACKGROUND_COLOR, BackgroundColorB, *LocalizeUnrealEd("BackgroundColor"));
	AddCheckTool(IDM_CASCADE_TOGGLE_WIRE_SPHERE, *LocalizeUnrealEd("CascadeToggleWireSphere"), WireSphereB, wxNullBitmap, *LocalizeUnrealEd("CascadeToggleWireSphere"));

	AddSeparator();
	AddTool(IDM_CASCADE_UNDO, UndoB, *LocalizeUnrealEd("Undo"));
	AddTool(IDM_CASCADE_REDO, RedoB, *LocalizeUnrealEd("Redo"));

	AddSeparator();
	AddSeparator();
	AddTool(IDM_CASCADE_LOD_REGENDUP,	LODRegenerateDuplicate,	*LocalizeUnrealEd("CascadeLODRegenerateDuplicate"));
	AddTool(IDM_CASCADE_LOD_REGEN,		LODRegenerate,			*LocalizeUnrealEd("CascadeLODRegenerate"));
	AddSeparator();

	WxMaskedBitmap* DetailBitmap = &DetailHigh;
	if (GSystemSettings.DetailMode == DM_Medium)
	{
		DetailBitmap = &DetailMedium;
	}
	else if (GSystemSettings.DetailMode == DM_Low)
	{
		DetailBitmap = &DetailLow;
	}

	AddTool(IDM_CASCADE_DETAILMODE, *DetailBitmap, *LocalizeUnrealEd("CascadeChangeDetailMode"));
	AddSeparator();

	LODCurrent = new wxTextCtrl(this, IDM_CASCADE_LOD_CURRENT, TEXT("0"));
	check(LODCurrent);
	wxSize CurrentToolSize = LODCurrent->GetSize();
	LODCurrent->SetSize(CurrentToolSize.GetWidth() / 4, CurrentToolSize.GetHeight());
	LODCurrent->SetEditable(FALSE);
	LODTotal = new wxTextCtrl(this, IDM_CASCADE_LOD_TOTAL, TEXT("of -1"));
	check(LODTotal);
	LODTotal->SetSize(CurrentToolSize.GetWidth() / 2, CurrentToolSize.GetHeight());
	LODTotal->SetEditable(FALSE);

	AddTool(IDM_CASCADE_LOD_HIGH,		LODHigh,		*LocalizeUnrealEd("CascadeLODHigh"));
	AddTool(IDM_CASCADE_LOD_HIGHER,		LODHigher,		*LocalizeUnrealEd("CascadeLODHigher"));
	AddTool(IDM_CASCADE_LOD_ADDBEFORE,	LODAddBefore,	*LocalizeUnrealEd("CascadeLODAddBefore"));
	AddControl(LODCurrent);
	AddControl(LODTotal);
	AddTool(IDM_CASCADE_LOD_ADDAFTER,	LODAddAfter,	*LocalizeUnrealEd("CascadeLODAddAfter"));
	AddTool(IDM_CASCADE_LOD_LOWER,		LODLower,		*LocalizeUnrealEd("CascadeLODLower"));
	AddTool(IDM_CASCADE_LOD_LOW,		LODLow,			*LocalizeUnrealEd("CascadeLODLow"));

	AddSeparator();
	AddTool(IDM_CASCADE_LOD_DELETE,		LODDelete,		*LocalizeUnrealEd("CascadeLODDelete"));

	Realize();

	if (Cascade && Cascade->EditorOptions)
	{
		if (Cascade->EditorOptions->bShowGrid == TRUE)
		{
			ToggleTool(IDM_CASCADE_TOGGLEGRID, TRUE);
		}
		else
		{
			ToggleTool(IDM_CASCADE_TOGGLEGRID, FALSE);
		}
	}
}

WxCascadeToolBar::~WxCascadeToolBar()
{
}
