/*=============================================================================
	Cascade.cpp: 'Cascade' particle editor
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "Cascade.h"
#include "CurveEd.h"
#include "EngineMaterialClasses.h"
#include "PropertyWindow.h"
#include "UnEdTran.h"
#include "FileHelpers.h"

#if WITH_APEX_PARTICLES
#include "ApexEditorWidgets.h"
#endif

IMPLEMENT_CLASS(UCascadeOptions);
IMPLEMENT_CLASS(UCascadeConfiguration);
IMPLEMENT_CLASS(UCascadePreviewComponent);

/*-----------------------------------------------------------------------------
	UCascadeParticleSystemComponent
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UCascadeParticleSystemComponent);

UBOOL UCascadeParticleSystemComponent::SingleLineCheck(FCheckResult& Hit, AActor* SourceActor, const FVector& End, const FVector& Start, DWORD TraceFlags, const FVector& Extent)
{
	if (bWarmingUp == FALSE)
	{
		if (CascadePreviewViewportPtr && CascadePreviewViewportPtr->FloorComponent && (CascadePreviewViewportPtr->FloorComponent->HiddenEditor == FALSE))
		{
			Hit = FCheckResult(1.f);

			const UBOOL bHit = !CascadePreviewViewportPtr->FloorComponent->LineCheck(Hit, End, Start, Extent, TraceFlags);
			if (bHit)
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

void UCascadeParticleSystemComponent::UpdateLODInformation()
{
	if (GetLODLevel() != EditorLODLevel)
	{
		SetLODLevel(EditorLODLevel);
	}
}

/*-----------------------------------------------------------------------------
	FCascadeNotifyHook
-----------------------------------------------------------------------------*/
void FCascadeNotifyHook::NotifyDestroy(void* Src)
{
	if (WindowOfInterest == Src)
	{
		if (Cascade->PreviewVC && Cascade->PreviewVC->FloorComponent)
		{
			// Save the information in the particle system
			Cascade->PartSys->FloorPosition = Cascade->PreviewVC->FloorComponent->Translation;
			Cascade->PartSys->FloorRotation = Cascade->PreviewVC->FloorComponent->Rotation;
			Cascade->PartSys->FloorScale = Cascade->PreviewVC->FloorComponent->Scale;
			Cascade->PartSys->FloorScale3D = Cascade->PreviewVC->FloorComponent->Scale3D;

			if (Cascade->PreviewVC->FloorComponent->StaticMesh)
			{
				Cascade->PartSys->FloorMesh = Cascade->PreviewVC->FloorComponent->StaticMesh->GetPathName();
			}
			else
			{
				warnf(TEXT("Unable to locate Cascade floor mesh outer..."));
				Cascade->PartSys->FloorMesh = TEXT("");
			}
			Cascade->PartSys->MarkPackageDirty();
		}
	}
}

/*-----------------------------------------------------------------------------
	WxCascade
-----------------------------------------------------------------------------*/

UBOOL				WxCascade::bParticleModuleClassesInitialized = FALSE;
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

	bParticleModuleClassesInitialized = TRUE;
}

bool WxCascade::DuplicateEmitter(UParticleEmitter* SourceEmitter, UParticleSystem* DestSystem, UBOOL bShare)
{
	if (bIsSoloing == TRUE)
	{
		FString Description = LocalizeUnrealEd(TEXT("DuplicateEmitter"));
		if (PromptForCancellingSoloingMode(Description) == FALSE)
		{
			return FALSE;
		}
	}
	UObject* SourceOuter = SourceEmitter->GetOuter();
	if (SourceOuter != DestSystem)
	{
		if (bShare == TRUE)
		{
			warnf(TEXT("Can't share modules across particle systems!"));
			bShare = FALSE;
		}
	}

	INT InsertionIndex = -1;
	if (SourceOuter == DestSystem)
	{
		UParticleSystem* SourcePSys = Cast<UParticleSystem>(SourceOuter);
		if (SourcePSys)
		{
			// Find the source emitter in the SourcePSys emitter array
			for (INT CheckSourceIndex = 0; CheckSourceIndex < SourcePSys->Emitters.Num(); CheckSourceIndex++)
			{
				if (SourcePSys->Emitters(CheckSourceIndex) == SourceEmitter)
				{
					InsertionIndex = CheckSourceIndex + 1;
					break;
				}
			}
		}
	}

	// Find desired class of new module.
    UClass* NewEmitClass = SourceEmitter->GetClass();
	if (NewEmitClass == UParticleSpriteEmitter::StaticClass())
	{
		// Construct it
		UParticleEmitter* NewEmitter = ConstructObject<UParticleEmitter>(NewEmitClass, DestSystem, NAME_None, RF_Transactional);

		check(NewEmitter);

		FString	NewName = SourceEmitter->GetEmitterName().ToString();
		NewEmitter->SetEmitterName(FName(*NewName));
 		NewEmitter->EmitterEditorColor = FColor::MakeRandomColor();
 		NewEmitter->EmitterEditorColor.A = 255;

		//	'Private' data - not required by the editor
		UObject*			DupObject;
		UParticleLODLevel*	SourceLODLevel;
		UParticleLODLevel*	NewLODLevel;
		UParticleLODLevel*	PrevSourceLODLevel = NULL;
		UParticleLODLevel*	PrevLODLevel = NULL;

		NewEmitter->LODLevels.InsertZeroed(0, SourceEmitter->LODLevels.Num());
		for (INT LODIndex = 0; LODIndex < SourceEmitter->LODLevels.Num(); LODIndex++)
		{
			SourceLODLevel	= SourceEmitter->LODLevels(LODIndex);
			NewLODLevel		= ConstructObject<UParticleLODLevel>(UParticleLODLevel::StaticClass(), NewEmitter, NAME_None, RF_Transactional);
			check(NewLODLevel);

			NewLODLevel->Level					= SourceLODLevel->Level;
			NewLODLevel->bEnabled				= SourceLODLevel->bEnabled;

			// The RequiredModule
			if (bShare)
			{
				NewLODLevel->RequiredModule = SourceLODLevel->RequiredModule;
			}
			else
			{
				if ((LODIndex > 0) && (PrevSourceLODLevel->RequiredModule == SourceLODLevel->RequiredModule))
				{
					PrevLODLevel->RequiredModule->LODValidity |= (1 << LODIndex);
					NewLODLevel->RequiredModule = PrevLODLevel->RequiredModule;
				}
				else
				{
					DupObject = GEditor->StaticDuplicateObject(SourceLODLevel->RequiredModule, SourceLODLevel->RequiredModule, DestSystem, TEXT("None"));
					check(DupObject);
					NewLODLevel->RequiredModule						= Cast<UParticleModuleRequired>(DupObject);
					NewLODLevel->RequiredModule->ModuleEditorColor	= FColor::MakeRandomColor();
					NewLODLevel->RequiredModule->LODValidity		= (1 << LODIndex);
				}
			}

			// The SpawnModule
			if (bShare)
			{
				NewLODLevel->SpawnModule = SourceLODLevel->SpawnModule;
			}
			else
			{
				if ((LODIndex > 0) && (PrevSourceLODLevel->SpawnModule == SourceLODLevel->SpawnModule))
				{
					PrevLODLevel->SpawnModule->LODValidity |= (1 << LODIndex);
					NewLODLevel->SpawnModule = PrevLODLevel->SpawnModule;
				}
				else
				{
					DupObject = GEditor->StaticDuplicateObject(SourceLODLevel->SpawnModule, SourceLODLevel->SpawnModule, DestSystem, TEXT("None"));
					check(DupObject);
					NewLODLevel->SpawnModule					= Cast<UParticleModuleSpawn>(DupObject);
					NewLODLevel->SpawnModule->ModuleEditorColor	= FColor::MakeRandomColor();
					NewLODLevel->SpawnModule->LODValidity		= (1 << LODIndex);
				}
			}

			// Copy each module
			NewLODLevel->Modules.InsertZeroed(0, SourceLODLevel->Modules.Num());
			for (INT ModuleIndex = 0; ModuleIndex < SourceLODLevel->Modules.Num(); ModuleIndex++)
			{
				UParticleModule* SourceModule = SourceLODLevel->Modules(ModuleIndex);
				if (bShare)
				{
					NewLODLevel->Modules(ModuleIndex) = SourceModule;
				}
				else
				{
					if ((LODIndex > 0) && (PrevSourceLODLevel->Modules(ModuleIndex) == SourceLODLevel->Modules(ModuleIndex)))
					{
						PrevLODLevel->Modules(ModuleIndex)->LODValidity |= (1 << LODIndex);
						NewLODLevel->Modules(ModuleIndex) = PrevLODLevel->Modules(ModuleIndex);
					}
					else
					{
						DupObject = GEditor->StaticDuplicateObject(SourceModule, SourceModule, DestSystem, TEXT("None"));
						if (DupObject)
						{
							UParticleModule* Module				= Cast<UParticleModule>(DupObject);
							Module->ModuleEditorColor			= FColor::MakeRandomColor();
							NewLODLevel->Modules(ModuleIndex)	= Module;
						}
					}
				}
			}

			// TypeData module as well...
			if (SourceLODLevel->TypeDataModule)
			{
				if (bShare)
				{
					NewLODLevel->TypeDataModule = SourceLODLevel->TypeDataModule;
				}
				else
				{
					if ((LODIndex > 0) && (PrevSourceLODLevel->TypeDataModule == SourceLODLevel->TypeDataModule))
					{
						PrevLODLevel->TypeDataModule->LODValidity |= (1 << LODIndex);
						NewLODLevel->TypeDataModule = PrevLODLevel->TypeDataModule;
					}
					else
					{
						DupObject = GEditor->StaticDuplicateObject(SourceLODLevel->TypeDataModule, SourceLODLevel->TypeDataModule, DestSystem, TEXT("None"));
						if (DupObject)
						{
							UParticleModule* Module		= Cast<UParticleModule>(DupObject);
							Module->ModuleEditorColor	= FColor::MakeRandomColor();
							NewLODLevel->TypeDataModule	= Module;
						}
					}
				}
			}
			NewLODLevel->ConvertedModules		= TRUE;
			NewLODLevel->PeakActiveParticles	= SourceLODLevel->PeakActiveParticles;

			NewEmitter->LODLevels(LODIndex)		= NewLODLevel;

			PrevLODLevel = NewLODLevel;
			PrevSourceLODLevel = SourceLODLevel;
		}

		//@todo. Compare against the destination system, and insert appropriate LOD levels where necessary
		// Generate all the levels that are present in other emitters...
		// NOTE: Big assumptions - the highest and lowest are 0,100 respectively and they MUST exist.
		if (DestSystem->Emitters.Num() > 0)
		{
			UParticleEmitter* DestEmitter = DestSystem->Emitters(0);
			INT DestLODCount = DestEmitter->LODLevels.Num();
			INT NewLODCount = NewEmitter->LODLevels.Num();
			if (DestLODCount != NewLODCount)
			{
				debugf(TEXT("Generating existing LOD levels..."));

				if (DestLODCount < NewLODCount)
				{
					for (INT DestEmitIndex = 0; DestEmitIndex < DestSystem->Emitters.Num(); DestEmitIndex++)
					{
						UParticleEmitter* DestEmitter = DestSystem->Emitters(DestEmitIndex);
						for (INT InsertIndex = DestLODCount; InsertIndex < NewLODCount; InsertIndex++)
						{
							DestEmitter->CreateLODLevel(InsertIndex);
						}
						DestEmitter->UpdateModuleLists();
					}
				}
				else
				{
					for (INT InsertIndex = NewLODCount; InsertIndex < DestLODCount; InsertIndex++)
					{
						NewEmitter->CreateLODLevel(InsertIndex);
					}
				}
			}
		}

        NewEmitter->UpdateModuleLists();

		// Add to selected emitter
		if ((InsertionIndex >= 0) && (InsertionIndex < DestSystem->Emitters.Num()))
		{
			DestSystem->Emitters.InsertItem(NewEmitter, InsertionIndex);
		}
		else
		{
	        DestSystem->Emitters.AddItem(NewEmitter);
		}
	}
	else
	{
		appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Prompt_4"), *NewEmitClass->GetDesc()));
		return FALSE;
	}

	DestSystem->SetupSoloing();

	return TRUE;
}

// Undo/Redo support
bool WxCascade::BeginTransaction(const TCHAR* pcTransaction)
{
	if (TransactionInProgress())
	{
		FString kError(*LocalizeUnrealEd("Error_FailedToBeginTransaction"));
		kError += kTransactionName;
		checkf(0, TEXT("%s"), *kError);
		return FALSE;
	}

	CascadeTrans->Begin(pcTransaction);
	kTransactionName = FString(pcTransaction);
	bTransactionInProgress = TRUE;

	return TRUE;
}

bool WxCascade::EndTransaction(const TCHAR* pcTransaction)
{
	if (!TransactionInProgress())
	{
		FString kError(*LocalizeUnrealEd("Error_FailedToEndTransaction"));
		kError += kTransactionName;
		checkf(0, TEXT("%s"), *kError);
		return FALSE;
	}

	if (appStrcmp(*kTransactionName, pcTransaction) != 0)
	{
		debugf(TEXT("Cascade -   EndTransaction = %s --- Curr = %s"), 
			pcTransaction, *kTransactionName);
		return FALSE;
	}

	CascadeTrans->End();

	kTransactionName = TEXT("");
	bTransactionInProgress = FALSE;

	return TRUE;
}

bool WxCascade::TransactionInProgress()
{
	return bTransactionInProgress;
}

void WxCascade::ModifySelectedObjects()
{
	if (SelectedEmitter)
	{
		ModifyEmitter(SelectedEmitter);
	}
	if (SelectedModule)
	{
		SelectedModule->Modify();
	}
}

/**
 *	Call Modify on the particle systems and component
 *
 *	@param	bINModifyEmitters		If TRUE, also modify each Emitter in the PSys.
 */
void WxCascade::ModifyParticleSystem(UBOOL bInModifyEmitters)
{
	PartSys->Modify();
	if (bInModifyEmitters == TRUE)
	{
		for (INT EmitterIdx = 0; EmitterIdx < PartSys->Emitters.Num(); EmitterIdx++)
		{
			UParticleEmitter* Emitter = PartSys->Emitters(EmitterIdx);
			if (Emitter != NULL)
			{
				ModifyEmitter(Emitter);
			}
		}
	}
	PartSysComp->Modify();
}

void WxCascade::ModifyEmitter(UParticleEmitter* Emitter)
{
	if (Emitter)
	{
		Emitter->Modify();
		for (INT LODIndex = 0; LODIndex < Emitter->LODLevels.Num(); LODIndex++)
		{
			UParticleLODLevel* LODLevel = Emitter->LODLevels(LODIndex);
			if (LODLevel)
			{
				LODLevel->Modify();
			}
		}
	}
}

void WxCascade::CascadeUndo()
{
	if (CascadeTrans->Undo())
	{
		CascadeTouch();
		wxCommandEvent DummyEvent;
		OnResetInLevel(DummyEvent);
	}
}

void WxCascade::CascadeRedo()
{
	if (CascadeTrans->Redo())
	{
		CascadeTouch();
		wxCommandEvent DummyEvent;
		OnResetInLevel(DummyEvent);
	}
}

void WxCascade::CascadeTouch()
{
	// Touch the module lists in each emitter.
	PartSys->UpdateAllModuleLists();
	UpdateLODLevelControls();
	PartSysComp->ResetParticles();
	PartSysComp->InitializeSystem();
	// 'Refresh' the viewport
	EmitterEdVC->Viewport->Invalidate();
	CurveEd->CurveChanged();
}

void WxCascade::UpdateLODLevelControls()
{
	INT CurrentLODLevel = GetCurrentlySelectedLODLevelIndex();
	SetLODValue(CurrentLODLevel);
}

// PostProces
/**
 *	Update the post process chain according to the show options
 */
void WxCascade::UpdatePostProcessChain()
{
}

/**
 *	Return the currently selected LOD level
 *
 *	@return	INT		The currently selected LOD level...
 */
INT WxCascade::GetCurrentlySelectedLODLevelIndex()
{
	if (ToolBar)
	{
		FString CurrLODText = ToolBar->LODCurrent->GetValue().c_str();
		INT SetLODLevelIndex = appAtoi(*CurrLODText) - 1;
		if (SetLODLevelIndex < 0)
		{
			SetLODLevelIndex = 0;
		}
		else
		{
			if (PartSys && (PartSys->Emitters.Num() > 0))
			{
				UParticleEmitter* Emitter = PartSys->Emitters(0);
				if (Emitter)
				{
					if (SetLODLevelIndex >= Emitter->LODLevels.Num())
					{
						SetLODLevelIndex = Emitter->LODLevels.Num() - 1;
					}
				}
			}
			else
			{
				SetLODLevelIndex = 0;
			}
		}

		return SetLODLevelIndex;
	}

	return INDEX_NONE;
}

/**
 *	Return the currently selected LOD level
 *
 *	@return	UParticleLODLevel*	The currently selected LOD level...
 */
UParticleLODLevel* WxCascade::GetCurrentlySelectedLODLevel()
{
	INT CurrentLODLevel = GetCurrentlySelectedLODLevelIndex();
	if ((CurrentLODLevel >= 0) && SelectedEmitter)
	{
		return SelectedEmitter->GetLODLevel(CurrentLODLevel);
	}

	return NULL;
}

/**
 *	Return the currently selected LOD level
 *
 *	@param	InEmitter			The emitter to retrieve it from.
 *	@return	UParticleLODLevel*	The currently selected LOD level.
 */
UParticleLODLevel* WxCascade::GetCurrentlySelectedLODLevel(UParticleEmitter* InEmitter)
{
	if (InEmitter)
	{
		UParticleEmitter* SaveSelectedEmitter = SelectedEmitter;
		SelectedEmitter = InEmitter;
		UParticleLODLevel* ReturnLODLevel = GetCurrentlySelectedLODLevel();
		SelectedEmitter = SaveSelectedEmitter;
		return ReturnLODLevel;
	}

	return NULL;
}

/**
 *	Is the module of the given name suitable for the right-click module menu?
 */
UBOOL WxCascade::IsModuleSuitableForModuleMenu(FString& InModuleName)
{
	INT RejectIndex;
	check(EditorConfig);
	return (EditorConfig->ModuleMenu_ModuleRejections.FindItem(InModuleName, RejectIndex) == FALSE);
}

/**
 *	Is the base module of the given name suitable for the right-click module menu
 *	given the currently selected emitter TypeData?
 */
UBOOL WxCascade::IsBaseModuleTypeDataPairSuitableForModuleMenu(FString& InModuleName)
{
	INT RejectIndex;
	check(EditorConfig);

	FString TDName(TEXT("None"));
	if (SelectedEmitter)
	{
		UParticleLODLevel* LODLevel = GetCurrentlySelectedLODLevel();
		if (LODLevel && LODLevel->TypeDataModule)
		{
			TDName = LODLevel->TypeDataModule->GetClass()->GetName();
		}
	}

	FModuleMenuMapper* Mapper = NULL;
	for (INT MapIndex = 0; MapIndex < EditorConfig->ModuleMenu_TypeDataToBaseModuleRejections.Num(); MapIndex++)
	{
		if (EditorConfig->ModuleMenu_TypeDataToBaseModuleRejections(MapIndex).ObjName == TDName)
		{
			Mapper = &(EditorConfig->ModuleMenu_TypeDataToBaseModuleRejections(MapIndex));
			break;
		}
	}

	if (Mapper)
	{
		if (Mapper->InvalidObjNames.FindItem(InModuleName, RejectIndex) == TRUE)
		{
			return FALSE;
		}
	}

	return TRUE;
}

/**
 *	Is the base module of the given name suitable for the right-click module menu
 *	given the currently selected emitter TypeData?
 */
UBOOL WxCascade::IsModuleTypeDataPairSuitableForModuleMenu(FString& InModuleName)
{
	INT RejectIndex;
	check(EditorConfig);

	FString TDName(TEXT("None"));
	if (SelectedEmitter)
	{
		UParticleLODLevel* LODLevel = GetCurrentlySelectedLODLevel();
		if (LODLevel && LODLevel->TypeDataModule)
		{
			TDName = LODLevel->TypeDataModule->GetClass()->GetName();
		}
	}

	FModuleMenuMapper* Mapper = NULL;
	for (INT MapIndex = 0; MapIndex < EditorConfig->ModuleMenu_TypeDataToSpecificModuleRejections.Num(); MapIndex++)
	{
		if (EditorConfig->ModuleMenu_TypeDataToSpecificModuleRejections(MapIndex).ObjName == TDName)
		{
			Mapper = &(EditorConfig->ModuleMenu_TypeDataToSpecificModuleRejections(MapIndex));
			break;
		}
	}

	if (Mapper)
	{
		if (Mapper->InvalidObjNames.FindItem(InModuleName, RejectIndex) == TRUE)
		{
			return FALSE;
		}
	}

	return TRUE;
}

/**
 *	Update the memory information of the particle system
 */
void WxCascade::UpdateMemoryInformation()
{
	if (PartSys != NULL)
	{
		FArchiveCountMem MemCount(PartSys);
		ParticleSystemRootSize = MemCount.GetMax();

		ParticleModuleMemSize = 0;
		TMap<UParticleModule*,UBOOL> ModuleList;
		for (INT EmitterIdx = 0; EmitterIdx < PartSys->Emitters.Num(); EmitterIdx++)
		{
			UParticleEmitter* Emitter = PartSys->Emitters(EmitterIdx);
			if (Emitter != NULL)
			{
				for (INT LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
				{
					UParticleLODLevel* LODLevel = Emitter->LODLevels(LODIdx);
					if (LODLevel != NULL)
					{
						ModuleList.Set(LODLevel->RequiredModule, TRUE);
						ModuleList.Set(LODLevel->SpawnModule, TRUE);
						for (INT ModuleIdx = 0; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
						{
							ModuleList.Set(LODLevel->Modules(ModuleIdx), TRUE);
						}
					}
				}
			}
		}
		for (TMap<UParticleModule*,UBOOL>::TIterator ModuleIt(ModuleList); ModuleIt; ++ModuleIt)
		{
			UParticleModule* Module = ModuleIt.Key();
			FArchiveCountMem ModuleCount(Module);
			ParticleModuleMemSize += ModuleCount.GetMax();
		}
	}
	if (PartSysComp != NULL)
	{
		FArchiveCountMem ComponentMemCount(PartSysComp);
		PSysCompRootSize = ComponentMemCount.GetMax();
		PSysCompResourceSize = PartSysComp->GetResourceSize();
	}
}

/**
 */
void WxCascade::SetLODValue(INT LODSetting)
{
	if (ToolBar)
	{
		FString	ValueStr = FString::Printf(TEXT("%d"), LODSetting + 1);
		ToolBar->LODCurrent->SetValue(*ValueStr);
		INT LODCount = PartSys ? (PartSys->Emitters.Num() > 0) ? PartSys->Emitters(0)->LODLevels.Num() : -1 : -2;
		ValueStr = FString::Printf(TEXT("Total = %d"), LODCount);
		ToolBar->LODTotal->SetValue(*ValueStr);
	}

	if (LODSetting >= 0)
	{
		if (PartSys)
		{
			PartSys->EditorLODSetting	= LODSetting;
		}
		if (PartSysComp)
		{
			const INT OldEditorLODLevel = PartSysComp->EditorLODLevel;
			PartSysComp->EditorLODLevel = LODSetting;
			PartSysComp->SetLODLevel(LODSetting);

			if (PartSysComp->EditorLODLevel < PartSys->LODSettings.Num()
				&& OldEditorLODLevel < PartSys->LODSettings.Num()
				&& PartSysComp->EditorLODLevel != OldEditorLODLevel
				&& PartSys->LODSettings(OldEditorLODLevel).bLit != PartSys->LODSettings(PartSysComp->EditorLODLevel).bLit)
			{
				if (PartSys->LODSettings(PartSysComp->EditorLODLevel).bLit)
				{
					PartSysComp->SetLightEnvironment(ParticleLightEnv);
				}
				else
				{
					PartSysComp->SetLightEnvironment(NULL);
				}
			}
		}
	}

	// Don't set the LODs of in-level particle systems if the particle LOD preview mode is enabled
	extern UBOOL GbEnableEditorPSysRealtimeLOD;
	if (!GbEnableEditorPSysRealtimeLOD)
	{
		for (TObjectIterator<UParticleSystemComponent> It;It;++It)
		{
			if (It->Template == PartSysComp->Template)
			{
				It->EditorLODLevel = LODSetting;
				It->SetLODLevel(LODSetting);
			}
		}
	}
}

BEGIN_EVENT_TABLE(WxCascade, WxTrackableFrame)
	EVT_SIZE(WxCascade::OnSize)
	EVT_MENU(IDM_CASCADE_RENAME_EMITTER, WxCascade::OnRenameEmitter)
	EVT_MENU_RANGE(	IDM_CASCADE_NEW_EMITTER_START, IDM_CASCADE_NEW_EMITTER_END, WxCascade::OnNewEmitter )
	EVT_MENU(IDM_CASCADE_SELECT_PARTICLESYSTEM, WxCascade::OnSelectParticleSystem)
	EVT_MENU(IDM_CSACADE_PSYS_REMOVE_DUPLICATE_MODULES, WxCascade::OnRemoveDuplicateModules)
	EVT_MENU(IDM_CASCADE_PSYS_NEW_EMITTER_BEFORE, WxCascade::OnNewEmitterBefore)
	EVT_MENU(IDM_CASCADE_PSYS_NEW_EMITTER_AFTER, WxCascade::OnNewEmitterAfter) 
	EVT_MENU_RANGE( IDM_CASCADE_NEW_MODULE_START, IDM_CASCADE_NEW_MODULE_END, WxCascade::OnNewModule )
	EVT_MENU(IDM_CASCADE_ADD_SELECTED_MODULE, WxCascade::OnAddSelectedModule)
	EVT_MENU(IDM_CASCADE_COPY_MODULE, WxCascade::OnCopyModule)
	EVT_MENU(IDM_CASCADE_PASTE_MODULE, WxCascade::OnPasteModule)
	EVT_MENU( IDM_CASCADE_DELETE_MODULE, WxCascade::OnDeleteModule )
	EVT_MENU( IDM_CASCADE_ENABLE_MODULE, WxCascade::OnEnableModule )
	EVT_MENU( IDM_CASCADE_RESET_MODULE, WxCascade::OnResetModule )
	EVT_MENU( IDM_CASCADE_REFRESH_MODULE, WxCascade::OnRefreshModule )
	EVT_MENU( IDM_CASCADE_MODULE_SYNCMATERIAL,  WxCascade::OnModuleSyncMaterial )
	EVT_MENU( IDM_CASCADE_MODULE_USEMATERIAL, WxCascade::OnModuleUseMaterial )
	EVT_MENU( IDM_CASCADE_MODULE_DUPHIGHEST, WxCascade::OnModuleDupHighest )
	EVT_MENU( IDM_CASCADE_MODULE_SHAREHIGH, WxCascade::OnModuleShareHigher )
	EVT_MENU( IDM_CASCADE_MODULE_DUPHIGH, WxCascade::OnModuleDupHigher )
	EVT_MENU( IDM_CASCADE_MODULE_SETRANDOMSEED, WxCascade::OnModuleSetRandomSeed )
	EVT_MENU( IDM_CASCADE_MODULE_CONVERTTOSEEDED, WxCascade::OnModuleConvertToSeeded )
	EVT_MENU( IDM_CASCADE_MODULE_CUSTOM0, WxCascade::OnModuleCustom )
	EVT_MENU( IDM_CASCADE_MODULE_CUSTOM1, WxCascade::OnModuleCustom )
	EVT_MENU( IDM_CASCADE_MODULE_CUSTOM2, WxCascade::OnModuleCustom )
	EVT_MENU(IDM_CASCADE_DUPLICATE_EMITTER, WxCascade::OnDuplicateEmitter)
	EVT_MENU(IDM_CASCADE_DUPLICATE_SHARE_EMITTER, WxCascade::OnDuplicateEmitter)
	EVT_MENU( IDM_CASCADE_DELETE_EMITTER, WxCascade::OnDeleteEmitter )
	EVT_MENU(IDM_CASCADE_EXPORT_EMITTER, WxCascade::OnExportEmitter)
	EVT_MENU(IDM_CASCADE_EXPORT_ALL, WxCascade::OnExportAll)
	EVT_MENU_RANGE( IDM_CASCADE_SIM_PAUSE, IDM_CASCADE_SIM_NORMAL, WxCascade::OnMenuSimSpeed )
	EVT_MENU( IDM_CASCADE_SAVECAM, WxCascade::OnSaveCam )
#if defined(_CASCADE_ENABLE_MODULE_DUMP_)
	EVT_MENU( IDM_CASCADE_VIEW_DUMP, WxCascade::OnViewModuleDump)
#endif	//#if defined(_CASCADE_ENABLE_MODULE_DUMP_)
	EVT_MENU( IDM_CASCADE_RESETSYSTEM, WxCascade::OnResetSystem )
	EVT_MENU( IDM_CASCADE_RESETINLEVEL, WxCascade::OnResetInLevel )
	EVT_MENU( IDM_CASCADE_SYNCGENERICBROWSER, WxCascade::OnSyncGenericBrowser )
	EVT_TOOL( IDM_CASCADE_ORBITMODE, WxCascade::OnOrbitMode )
	EVT_TOOL(IDM_CASCADE_MOTION, WxCascade::OnMotionMode)
	EVT_TOOL(IDM_CASCADE_VIEWMODE, WxCascade::OnViewMode)
	EVT_TOOL_RCLICKED(IDM_CASCADE_VIEWMODE, WxCascade::OnViewModeRightClick)
	EVT_TOOL(IDM_WIREFRAME, WxCascade::OnSetViewMode)
	EVT_TOOL(IDM_UNLIT, WxCascade::OnSetViewMode)
	EVT_TOOL(IDM_LIT, WxCascade::OnSetViewMode)
	EVT_TOOL(IDM_TEXTUREDENSITY, WxCascade::OnSetViewMode)
	EVT_TOOL(IDM_SHADERCOMPLEXITY, WxCascade::OnSetViewMode)
	EVT_TOOL(IDM_CASCADE_BOUNDS, WxCascade::OnBounds)
	EVT_TOOL_RCLICKED(IDM_CASCADE_BOUNDS, WxCascade::OnBoundsRightClick)
//	EVT_TOOL_RCLICKED(IDM_CASCADE_POSTPROCESS, WxCascade::OnPostProcess)
	EVT_TOOL(IDM_CASCADE_POSTPROCESS, WxCascade::OnPostProcess)
	EVT_TOOL(IDM_CASCADE_TOGGLEGRID, WxCascade::OnToggleGrid)
	EVT_TOOL(IDM_CASCADE_PLAY, WxCascade::OnPlay)
	EVT_TOOL(IDM_CASCADE_PAUSE, WxCascade::OnPause)
	EVT_TOOL(IDM_CASCADE_SET_SPEED,	WxCascade::OnSetSpeed)
	EVT_TOOL_RCLICKED(IDM_CASCADE_SET_SPEED, WxCascade::OnSetSpeedRightClick)
	EVT_TOOL(IDM_CASCADE_SPEED_100,	WxCascade::OnSpeed)
	EVT_TOOL(IDM_CASCADE_SPEED_50, WxCascade::OnSpeed)
	EVT_TOOL(IDM_CASCADE_SPEED_25, WxCascade::OnSpeed)
	EVT_TOOL(IDM_CASCADE_SPEED_10, WxCascade::OnSpeed)
	EVT_TOOL(IDM_CASCADE_SPEED_1, WxCascade::OnSpeed)
	EVT_TOOL(IDM_CASCADE_LOOPSYSTEM, WxCascade::OnLoopSystem)
	EVT_TOOL(IDM_CASCADE_REALTIME, WxCascade::OnRealtime)
	EVT_TOOL(IDM_CASCADE_BACKGROUND_COLOR, WxCascade::OnBackgroundColor)
	EVT_TOOL(IDM_CASCADE_TOGGLE_WIRE_SPHERE, WxCascade::OnToggleWireSphere)
	EVT_TOOL(IDM_CASCADE_UNDO, WxCascade::OnUndo)
	EVT_TOOL(IDM_CASCADE_REDO, WxCascade::OnRedo)
	EVT_TOOL(IDM_CASCADE_DETAILMODE, WxCascade::OnDetailMode)
	EVT_TOOL_RCLICKED(IDM_CASCADE_DETAILMODE, WxCascade::OnDetailModeRightClick)
	EVT_UPDATE_UI(IDM_CASCADE_DETAILMODE, WxCascade::UI_ViewDetailMode)
	EVT_TOOL(IDM_CASCADE_LOD_LOW, WxCascade::OnLODLow)
	EVT_TOOL(IDM_CASCADE_LOD_LOWER, WxCascade::OnLODLower)
	EVT_TOOL(IDM_CASCADE_LOD_ADDBEFORE, WxCascade::OnLODAddBefore)
	EVT_TOOL(IDM_CASCADE_LOD_ADDAFTER, WxCascade::OnLODAddAfter)
	EVT_TOOL(IDM_CASCADE_LOD_HIGHER, WxCascade::OnLODHigher)
	EVT_TOOL(IDM_CASCADE_LOD_HIGH, WxCascade::OnLODHigh)
	EVT_TOOL(IDM_CASCADE_LOD_DELETE, WxCascade::OnLODDelete)
	EVT_TOOL(IDM_CASCADE_LOD_REGEN, WxCascade::OnRegenerateLowestLOD)
	EVT_TOOL(IDM_CASCADE_LOD_REGENDUP, WxCascade::OnRegenerateLowestLODDuplicateHighest)
	EVT_MENU( IDM_CASCADE_VIEW_AXES, WxCascade::OnViewAxes )
	EVT_MENU(IDM_CASCADE_VIEW_COUNTS, WxCascade::OnViewCounts)
	EVT_MENU(IDM_CASCADE_VIEW_TIMES, WxCascade::OnViewTimes)
	EVT_MENU(IDM_CASCADE_VIEW_EVENTS, WxCascade::OnViewEvents)
	EVT_MENU(IDM_CASCADE_VIEW_DISTANCE, WxCascade::OnViewDistance)
	EVT_MENU(IDM_CASCADE_VIEW_MEMORY, WxCascade::OnViewMemory)
	EVT_MENU(IDM_CASCADE_VIEW_GEOMETRY, WxCascade::OnViewGeometry)
	EVT_MENU(IDM_CASCADE_VIEW_GEOMETRY_PROPERTIES, WxCascade::OnViewGeometryProperties)
	EVT_MENU(IDM_CASCADE_RESET_PEAK_COUNTS, WxCascade::OnResetPeakCounts)
	EVT_MENU(IDM_CASCADE_CONVERT_TO_UBER, WxCascade::OnUberConvert)
	EVT_MENU(IDM_CASCADE_REGENERATE_LOWESTLOD, WxCascade::OnRegenerateLowestLOD)
	EVT_MENU(IDM_CASCADE_SAVE_PACKAGE, WxCascade::OnSavePackage)
	EVT_MENU(IDM_CASCADE_SIM_RESTARTONFINISH, WxCascade::OnLoopSimulation )
	EVT_MENU(IDM_CASCADE_SET_MOTION_RADIUS, WxCascade::OnSetMotionRadius)
	EVT_MENU(IDM_CASCADE_DETAILMODE_LOW, WxCascade::OnViewDetailModeLow)
	EVT_MENU(IDM_CASCADE_DETAILMODE_MEDIUM, WxCascade::OnViewDetailModeMedium)
	EVT_MENU(IDM_CASCADE_DETAILMODE_HIGH, WxCascade::OnViewDetailModeHigh)
	EVT_UPDATE_UI(IDM_CASCADE_DETAILMODE_LOW, WxCascade::UI_ViewDetailModeLow)
	EVT_UPDATE_UI(IDM_CASCADE_DETAILMODE_MEDIUM, WxCascade::UI_ViewDetailModeMedium)
	EVT_UPDATE_UI(IDM_CASCADE_DETAILMODE_HIGH, WxCascade::UI_ViewDetailModeHigh)
END_EVENT_TABLE()


#define CASCADE_NUM_SASHES		4

WxCascade::WxCascade(wxWindow* InParent, wxWindowID InID, UParticleSystem* InPartSys) : 
	WxTrackableFrame(InParent, InID, TEXT(""), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR, 
		InPartSys ? *(InPartSys->GetPathName()) : TEXT("EMPTY")),
	FDockingParent(this),
	MenuBar(NULL),
	ToolBar(NULL),
	PreviewVC(NULL)
{
	check(InPartSys);
	InPartSys->EditorLODSetting	= 0;
	InPartSys->SetupLODValidity();

	CascadeTrans = new UTransBuffer(8*1024*1024);

	DefaultPostProcessName = TEXT("");
	DefaultPostProcess = NULL;

	EditorOptions = ConstructObject<UCascadeOptions>(UCascadeOptions::StaticClass());
	check(EditorOptions);
	EditorConfig = ConstructObject<UCascadeConfiguration>(UCascadeConfiguration::StaticClass());
	check(EditorConfig);

	// Make sure we have a list of available particle modules
	WxCascade::InitParticleModuleClasses();

	// Set up pointers to interp objects
	PartSys = InPartSys;

	// Set up for undo/redo!
	PartSys->SetFlags(RF_Transactional);

	for (INT ii = 0; ii < PartSys->Emitters.Num(); ii++)
	{
		UParticleEmitter* Emitter = PartSys->Emitters(ii);
		if (Emitter)
		{
			Emitter->SetFlags(RF_Transactional);
			for (INT LODIndex = 0; LODIndex < Emitter->LODLevels.Num(); LODIndex++)
			{
				UParticleLODLevel* LODLevel = Emitter->GetLODLevel(LODIndex);
				if (LODLevel)
				{
					LODLevel->SetFlags(RF_Transactional);
					check(LODLevel->RequiredModule);
					LODLevel->RequiredModule->SetFlags(RF_Transactional);
					check(LODLevel->SpawnModule);
					LODLevel->SpawnModule->SetFlags(RF_Transactional);
					for (INT jj = 0; jj < LODLevel->Modules.Num(); jj++)
					{
						UParticleModule* pkModule = LODLevel->Modules(jj);
						pkModule->SetFlags(RF_Transactional);
					}
				}
			}
		}
	}

	// Nothing selected initially
	SelectedEmitter = NULL;
	bIsSoloing = FALSE;

	SelectedModule = NULL;

	CopyModule = NULL;
	CopyEmitter = NULL;

	CurveToReplace = NULL;

	bResetOnFinish = TRUE;
	bPendingReset = FALSE;
	bOrbitMode = TRUE;
	bMotionMode = FALSE;
	MotionModeRadius = EditorOptions->MotionModeRadius;
	AccumulatedMotionTime = 0.0f;
	bWireframe = FALSE;
	bBounds = FALSE;
	SimSpeed = 1.0f;

	bTransactionInProgress = FALSE;

	const INT PropertyWindowID = -1;
	const UBOOL bShowPropertyWindowTools = FALSE;
	
	ApexPropertyWindow = 0;
	ApexCurveWindow = 0;
#if WITH_APEX_EDITOR
	// create apex generic properties editor...
	ApexPropertyWindow = new ApexGenericEditorPanel(*this);
	
	// create apex curve editor...
	ApexCurveWindow = new ApexCurveEditorPanel(*this);
	
	// point the property editor at the curve editor...
	ApexPropertyWindow->SetCurveEditor(ApexCurveWindow);
#endif

	PropertyWindow = new WxPropertyWindowHost;
	PropertyWindow->Create(this, this, PropertyWindowID, bShowPropertyWindowTools);

	// Create particle system preview
	WxCascadePreview* PreviewWindow = new WxCascadePreview( this, -1, this );
	PreviewVC = PreviewWindow->CascadePreviewVC;
	PreviewVC->SetPreviewCamera(PartSys->ThumbnailAngle, PartSys->ThumbnailDistance);
	PreviewVC->SetViewLocationForOrbiting( FVector(0.f,0.f,0.f) );
	if (EditorOptions->bShowGrid == TRUE)
	{
		PreviewVC->ShowFlags |= SHOW_Grid;
	}
	else
	{
		PreviewVC->ShowFlags &= ~SHOW_Grid;
	}

	PreviewVC->bDrawParticleCounts = EditorOptions->bShowParticleCounts;
	PreviewVC->bDrawParticleTimes = EditorOptions->bShowParticleTimes;
	PreviewVC->bDrawSystemDistance = EditorOptions->bShowParticleDistance;
	PreviewVC->bDrawParticleEvents = EditorOptions->bShowParticleEvents;
	PreviewVC->bDrawParticleMemory = EditorOptions->bShowParticleMemory;
	PreviewVC->ParticleMemoryUpdateTime = EditorOptions->ParticleMemoryUpdateTime;
	if (appIsNearlyZero(PreviewVC->ParticleMemoryUpdateTime))
	{
		PreviewVC->ParticleMemoryUpdateTime = 2.5f;
	}

	// Load the desired window position from .ini file
	FWindowUtil::LoadPosSize(TEXT("CascadeEditor"), this, 256, 256, 1024, 768);

	// Load the preview scene
	PreviewVC->PreviewScene.LoadSettings(TEXT("CascadeEditor"));

	UpdatePostProcessChain();

	// Create new curve editor setup if not already done
	if (!PartSys->CurveEdSetup)
	{
		PartSys->CurveEdSetup = ConstructObject<UInterpCurveEdSetup>( UInterpCurveEdSetup::StaticClass(), PartSys, NAME_None, RF_NotForClient | RF_NotForServer | RF_Transactional );
	}

	// Create graph editor to work on systems CurveEd setup.
	CurveEd = new WxCurveEditor( this, -1, PartSys->CurveEdSetup );
	// Register this window with the Curve editor so we will be notified of various things.
	CurveEd->SetNotifyObject(this);

	// Create emitter editor
	EmitterEdWindow = new WxCascadeEmitterEd(this, -1, this);
	EmitterEdVC = EmitterEdWindow->EmitterEdVC;

	// Create Docking Windows
	{
	#if WITH_APEX_EDITOR
		if(ApexPropertyWindow)
		{
			AddDockingWindow(ApexPropertyWindow, FDockingParent::DH_Bottom, TEXT("APEX Properties"), TEXT("APEX Properties") );
		}
		if(ApexCurveWindow)
		{
			AddDockingWindow(ApexCurveWindow, FDockingParent::DH_Bottom, TEXT("APEX Curve Editor"), TEXT("APEX Curve Editor") );
		}
	#endif
		
		AddDockingWindow(PropertyWindow, FDockingParent::DH_Bottom, *FString::Printf( LocalizeSecure(LocalizeUnrealEd(TEXT("PropertiesCaption_F")), *PartSys->GetName()) ), *LocalizeUnrealEd(TEXT("Properties")) );
		AddDockingWindow(CurveEd, FDockingParent::DH_Bottom, *FString::Printf( LocalizeSecure(LocalizeUnrealEd(TEXT("CurveEditorCaption_F")), *PartSys->GetName()) ), *LocalizeUnrealEd(TEXT("CurveEditor")) );
		AddDockingWindow(PreviewWindow, FDockingParent::DH_Left, *FString::Printf( LocalizeSecure(LocalizeUnrealEd(TEXT("PreviewCaption_F")), *PartSys->GetName()) ), *LocalizeUnrealEd(TEXT("Preview")) );
		
		SetDockHostSize(FDockingParent::DH_Left, 500);

		AddDockingWindow( EmitterEdWindow, FDockingParent::DH_None, NULL );

		// Try to load a existing layout for the docking windows.
		LoadDockingLayout();
	}

	// Create menu bar
	MenuBar = new WxCascadeMenuBar(this);
	AppendWindowMenu(MenuBar);
	SetMenuBar( MenuBar );

	// Create tool bar
	ToolBar	= NULL;
	ToolBar = new WxCascadeToolBar( this, -1 );
	SetToolBar( ToolBar );

	if ((PreviewVC->ShowFlags & SHOW_PostProcess) != 0)
	{
		ToolBar->ToggleTool(IDM_CASCADE_POSTPROCESS, TRUE);
	}
	else
	{
		ToolBar->ToggleTool(IDM_CASCADE_POSTPROCESS, FALSE);
	}

	//@todo.SAS. Remember the last setting...
	wxCommandEvent DummyLitEvent;
	DummyLitEvent.SetId(IDM_LIT);
	OnSetViewMode(DummyLitEvent);

	// Set window title to particle system we are editing.
	SetTitle( *FString::Printf( LocalizeSecure(LocalizeUnrealEd("CascadeCaption_F"), *PartSys->GetName()) ) );

	// Set emitter to use the particle system we are editing.
	PartSysComp->SetTemplate(PartSys);

	// Create a new emitter if the particle system is empty...
	if (PartSys->Emitters.Num() == 0)
	{
		wxCommandEvent DummyEvent;
		OnNewEmitter(DummyEvent);
	}

	SetSelectedModule(NULL, NULL);

	PreviewVC->BackgroundColor = EditorOptions->BackgroundColor;

	// Setup the accelerator table
	TArray<wxAcceleratorEntry> Entries;
	// Allow derived classes an opportunity to register accelerator keys.
	// Bind SPACE to reset.
	if (EditorOptions->bUseSpaceBarResetInLevel == FALSE)
	{
		Entries.AddItem(wxAcceleratorEntry(wxACCEL_NORMAL, WXK_SPACE, IDM_CASCADE_RESETSYSTEM));
	}
	else
	{
		Entries.AddItem(wxAcceleratorEntry(wxACCEL_NORMAL, WXK_SPACE, IDM_CASCADE_RESETINLEVEL));
	}
	Entries.AddItem(wxAcceleratorEntry(wxACCEL_CTRL,	90,	IDM_CASCADE_UNDO));	// CTRL-Z
	Entries.AddItem(wxAcceleratorEntry(wxACCEL_CTRL,	89,	IDM_CASCADE_REDO));	// CTRL-Y

	// Create the new table with these.
	SetAcceleratorTable(wxAcceleratorTable(Entries.Num(),Entries.GetTypedData()));

	PartSysComp->InitializeSystem();
	PartSysComp->ActivateSystem();

	// Memory information
	UpdateMemoryInformation();

	UpdateLODLevelControls();

	CascadeTrans->Reset( TEXT("Clear Undo Buffer at initialization") );
}

WxCascade::~WxCascade()
{
	// Reset the detail mode values
	for (TObjectIterator<UParticleSystemComponent> It;It;++It)
	{
		if (It->Template == PartSysComp->Template)
		{
			It->EditorDetailMode = -1;
		}
	}

	if (PartSys != NULL)
	{
		PartSys->TurnOffSoloing();
	}
	CascadeTrans->Reset(TEXT("QuitCascade"));

	SaveDockingLayout();

	// Save the preview scene
	check(PreviewVC);
	PreviewVC->PreviewScene.SaveSettings(TEXT("CascadeEditor"));

	// Save the desired window position to the .ini file
	FWindowUtil::SavePosSize(TEXT("CascadeEditor"), this);

	// Destroy preview viewport before we destroy the level.
	GEngine->Client->CloseViewport(PreviewVC->Viewport);
	PreviewVC->Viewport = NULL;

	if (PreviewVC->FloorComponent)
	{
		EditorOptions->FloorPosition = PreviewVC->FloorComponent->Translation;
		EditorOptions->FloorRotation = PreviewVC->FloorComponent->Rotation;
		EditorOptions->FloorScale = PreviewVC->FloorComponent->Scale;
		EditorOptions->FloorScale3D = PreviewVC->FloorComponent->Scale3D;

		if (PreviewVC->FloorComponent->StaticMesh)
		{
			if (PreviewVC->FloorComponent->StaticMesh->GetOuter())
			{
				EditorOptions->FloorMesh = PreviewVC->FloorComponent->StaticMesh->GetOuter()->GetName();
				EditorOptions->FloorMesh += TEXT(".");
			}
			else
			{
				warnf(TEXT("Unable to locate Cascade floor mesh outer..."));
				EditorOptions->FloorMesh = TEXT("");
			}

			EditorOptions->FloorMesh += PreviewVC->FloorComponent->StaticMesh->GetName();
		}
		else
		{
			EditorOptions->FloorMesh += FString::Printf(TEXT("EditorMeshes.AnimTreeEd_PreviewFloor"));
		}

		FString	Name;

		Name = EditorOptions->FloorMesh;

		EditorOptions->SaveConfig();
	}

#if WITH_MANAGED_CODE
	UnBindColorPickers(this);
#endif

	delete PreviewVC;
	PreviewVC = NULL;

	delete PropertyWindow;
#if WITH_APEX_PARTICLES
	if(ApexPropertyWindow) 
		delete ApexPropertyWindow;
#endif
}

/**
 * This function is called when the window has been selected from within the ctrl + tab dialog.
 */
void WxCascade::OnSelected()
{
	Raise();
}

wxToolBar* WxCascade::OnCreateToolBar(long style, wxWindowID id, const wxString& name)
{
	if (name == TEXT("Cascade"))
		return new WxCascadeToolBar(this, -1);

	wxToolBar*	ReturnToolBar = OnCreateToolBar(style, id, name);
	if (ReturnToolBar)
	{
		UpdateLODLevelControls();
	}

	return ReturnToolBar;
}

void WxCascade::Serialize(FArchive& Ar)
{
	PreviewVC->Serialize(Ar);

	Ar << EditorOptions;
	Ar << EditorConfig;
	Ar << CascadeTrans;
	Ar << ParticleLightEnv;
}

/**
 * Pure virtual that must be overloaded by the inheriting class. It will
 * be called from within UnLevTick.cpp after ticking all actors.
 *
 * @param DeltaTime	Game time passed since the last call.
 */
static const DOUBLE ResetInterval = 0.5f;
void WxCascade::Tick( FLOAT DeltaTime )
{
	if(GIsPlayInEditorWorld)
	{
		return;
	}
	
	// Use the detail mode associated w/ the Cascade editor
	INT EditorDetailMode = GSystemSettings.DetailMode;
	GSystemSettings.DetailMode = PreviewVC->DetailMode;

	static FLOAT LastMemUpdateTime = 0.0f;
	UBOOL bCurrentlySoloing = FALSE;
	if (PartSys)
	{
		for (INT EmitterIdx = 0; EmitterIdx < PartSys->Emitters.Num(); EmitterIdx++)
		{
			UParticleEmitter* Emitter = PartSys->Emitters(EmitterIdx);
			if (Emitter && (Emitter->bIsSoloing == TRUE))
			{
				bCurrentlySoloing = TRUE;
				break;
			}
		}

		LastMemUpdateTime += DeltaTime;
		if (LastMemUpdateTime > PreviewVC->ParticleMemoryUpdateTime)
		{
			UpdateMemoryInformation();
			LastMemUpdateTime = 0.0f;
		}
	}

	// Don't bother ticking at all if paused.
	if (PreviewVC->TimeScale > KINDA_SMALL_NUMBER)
	{
		const FLOAT fSaveUpdateDelta = PartSys->UpdateTime_Delta;
		if (PreviewVC->TimeScale < 1.0f)
		{
			PartSys->UpdateTime_Delta *= PreviewVC->TimeScale;
		}

		FLOAT CurrDeltaTime = PreviewVC->TimeScale * DeltaTime;

		if (bMotionMode == TRUE)
		{
			AccumulatedMotionTime += CurrDeltaTime;
			FVector Position;
			Position.X = MotionModeRadius * appSin(AccumulatedMotionTime);
			Position.Y = MotionModeRadius * appCos(AccumulatedMotionTime);
			Position.Z = 0.0f;
			PartSysComp->LocalToWorld = FTranslationMatrix(Position);
		}

		PartSysComp->Tick(CurrDeltaTime);

		PreviewVC->TotalTime += CurrDeltaTime;

		PartSys->UpdateTime_Delta = fSaveUpdateDelta;
	}

	// If we are doing auto-reset
	if(bResetOnFinish)
	{
		UParticleSystemComponent* PartComp = PartSysComp;

		// If system has finish, pause for a bit before resetting.
		if(bPendingReset)
		{
			if(PreviewVC->TotalTime > ResetTime)
			{
				PartComp->ResetParticles();
				PartComp->ActivateSystem();

				bPendingReset = FALSE;
			}
		}
		else
		{
			if( PartComp->HasCompleted() )
			{
				bPendingReset = TRUE;
				ResetTime = PreviewVC->TotalTime + ResetInterval;
			}
		}
	}

	if (bCurrentlySoloing != bIsSoloing)
	{
		bIsSoloing = bCurrentlySoloing;
		EmitterEdVC->Viewport->Invalidate();
	}

	// Restore the global detail mode setting
	GSystemSettings.DetailMode = EditorDetailMode;
}

// FCurveEdNotifyInterface
/**
 *	PreEditCurve
 *	Called by the curve editor when N curves are about to change
 *
 *	@param	CurvesAboutToChange		An array of the curves about to change
 */
void WxCascade::PreEditCurve(TArray<UObject*> CurvesAboutToChange)
{
	BeginTransaction(*LocalizeUnrealEd("CurveEdit"));
	ModifyParticleSystem();
	ModifySelectedObjects();

	// Call Modify on all tracks with keys selected
	for (INT i = 0; i < CurvesAboutToChange.Num(); i++)
	{
		// If this keypoint is from a distribution, call Modify on it to back up its state.
		UDistributionFloat* DistFloat = Cast<UDistributionFloat>(CurvesAboutToChange(i));
		if (DistFloat)
		{
			DistFloat->SetFlags(RF_Transactional);
			DistFloat->Modify();
		}
		UDistributionVector* DistVector = Cast<UDistributionVector>(CurvesAboutToChange(i));
		if (DistVector)
		{
			DistVector->SetFlags(RF_Transactional);
			DistVector->Modify();
		}
	}
}

/**
 *	PostEditCurve
 *	Called by the curve editor when the edit has completed.
 */
void WxCascade::PostEditCurve()
{
	this->EndTransaction(*LocalizeUnrealEd("CurveEdit"));
}

/**
 *	MovedKey
 *	Called by the curve editor when a key has been moved.
 */
void WxCascade::MovedKey()
{
}

/**
 *	DesireUndo
 *	Called by the curve editor when an Undo is requested.
 */
void WxCascade::DesireUndo()
{
	CascadeUndo();
}

/**
 *	DesireRedo
 *	Called by the curve editor when an Redo is requested.
 */
void WxCascade::DesireRedo()
{
	CascadeRedo();
}

void WxCascade::OnSize( wxSizeEvent& In )
{
	In.Skip();
	Refresh();
}

///////////////////////////////////////////////////////////////////////////////////////
// Menu Callbacks

void WxCascade::OnRenameEmitter(wxCommandEvent& In)
{
	if (!SelectedEmitter)
		return;

	BeginTransaction(TEXT("EmitterRename"));

	PartSys->PreEditChange(NULL);
	PartSysComp->PreEditChange(NULL);

	FName& CurrentName = SelectedEmitter->GetEmitterName();

	WxDlgGenericStringEntry dlg;
	if (dlg.ShowModal(TEXT("RenameEmitter"), TEXT("Name"), *CurrentName.ToString()) == wxID_OK)
	{
		FName newName = FName(*(dlg.GetEnteredString()));
		SelectedEmitter->SetEmitterName(newName);
	}

	PartSysComp->PostEditChange();
	PartSys->PostEditChange();

	EndTransaction(TEXT("EmitterRename"));

	// Refresh viewport
	EmitterEdVC->Viewport->Invalidate();
}

void WxCascade::OnNewEmitter(wxCommandEvent& In)
{
	if (bIsSoloing == TRUE)
	{
		FString Description = LocalizeUnrealEd(TEXT("NewEmitter"));
		if (PromptForCancellingSoloingMode(Description) == FALSE)
		{
			return;
		}
	}

	BeginTransaction(TEXT("NewEmitter"));
	PartSys->PreEditChange(NULL);
	PartSysComp->PreEditChange(NULL);

	UClass* NewEmitClass = UParticleSpriteEmitter::StaticClass();

	// Construct it
	UParticleEmitter* NewEmitter = ConstructObject<UParticleEmitter>(NewEmitClass, PartSys, NAME_None, RF_Transactional);
	UParticleLODLevel* LODLevel	= NewEmitter->GetLODLevel(0);
	if (LODLevel == NULL)
	{
		// Generate the HighLOD level, and the default lowest level
		INT Index = NewEmitter->CreateLODLevel(0);
		LODLevel = NewEmitter->GetLODLevel(0);
	}

	check(LODLevel);

	NewEmitter->EmitterEditorColor = FColor::MakeRandomColor();
	NewEmitter->EmitterEditorColor.A = 255;
	
    // Set to sensible default values
	NewEmitter->SetToSensibleDefaults();

    // Handle special cases...
	if (NewEmitClass == UParticleSpriteEmitter::StaticClass())
	{
		// For handyness- use currently selected material for new emitter (or default if none selected)
		UParticleSpriteEmitter* NewSpriteEmitter = (UParticleSpriteEmitter*)NewEmitter;
		GCallbackEvent->Send( CALLBACK_LoadSelectedAssetsIfNeeded );
		UMaterialInterface* CurrentMaterial = GEditor->GetSelectedObjects()->GetTop<UMaterialInterface>();
		if (CurrentMaterial)
		{
			LODLevel->RequiredModule->Material = CurrentMaterial;
		}
		else
		{
			LODLevel->RequiredModule->Material = LoadObject<UMaterialInterface>(NULL, TEXT("EngineMaterials.DefaultParticle"), NULL, LOAD_None, NULL);
		}
	}

	// Generate all the levels that are present in other emitters...
	if (PartSys->Emitters.Num() > 0)
	{
		UParticleEmitter* ExistingEmitter = PartSys->Emitters(0);
		
		if (ExistingEmitter->LODLevels.Num() > 1)
		{
			if (NewEmitter->AutogenerateLowestLODLevel(PartSys->bRegenerateLODDuplicate) == FALSE)
			{
				warnf(TEXT("Failed to autogenerate lowest LOD level!"));
			}
		}

		if (ExistingEmitter->LODLevels.Num() > 2)
		{
			debugf(TEXT("Generating existing LOD levels..."));

			// Walk the LOD levels of the existing emitter...
			UParticleLODLevel* ExistingLOD;
			UParticleLODLevel* NewLOD_Prev = NewEmitter->LODLevels(0);
			UParticleLODLevel* NewLOD_Next = NewEmitter->LODLevels(1);

			check(NewLOD_Prev);
			check(NewLOD_Next);

			for (INT LODIndex = 1; LODIndex < ExistingEmitter->LODLevels.Num() - 1; LODIndex++)
			{
				ExistingLOD = ExistingEmitter->LODLevels(LODIndex);

				// Add this one
				INT ResultIndex = NewEmitter->CreateLODLevel(ExistingLOD->Level, TRUE);

				UParticleLODLevel* NewLODLevel	= NewEmitter->LODLevels(ResultIndex);
				check(NewLODLevel);
				NewLODLevel->UpdateModuleLists();
			}
		}
	}

	NewEmitter->UpdateModuleLists();

	NewEmitter->PostEditChange();
	if (NewEmitter)
	{
		NewEmitter->SetFlags(RF_Transactional);
		for (INT LODIndex = 0; LODIndex < NewEmitter->LODLevels.Num(); LODIndex++)
		{
			UParticleLODLevel* LODLevel = NewEmitter->GetLODLevel(LODIndex);
			if (LODLevel)
			{
				LODLevel->SetFlags(RF_Transactional);
				check(LODLevel->RequiredModule);
				LODLevel->RequiredModule->SetFlags(RF_Transactional);
				check(LODLevel->SpawnModule);
				LODLevel->SpawnModule->SetFlags(RF_Transactional);
				for (INT jj = 0; jj < LODLevel->Modules.Num(); jj++)
				{
					UParticleModule* pkModule = LODLevel->Modules(jj);
					pkModule->SetFlags(RF_Transactional);
				}
			}
		}
	}

    // Add to selected emitter
    PartSys->Emitters.AddItem(NewEmitter);

	// Setup the LOD distances
	if (PartSys->LODDistances.Num() == 0)
	{
		UParticleEmitter* Emitter = PartSys->Emitters(0);
		if (Emitter)
		{
			PartSys->LODDistances.Add(Emitter->LODLevels.Num());
			for (INT LODIndex = 0; LODIndex < PartSys->LODDistances.Num(); LODIndex++)
			{
				PartSys->LODDistances(LODIndex) = LODIndex * 2500.0f;
			}
		}
	}
	if (PartSys->LODSettings.Num() == 0)
	{
		UParticleEmitter* Emitter = PartSys->Emitters(0);
		if (Emitter)
		{
			PartSys->LODSettings.Add(Emitter->LODLevels.Num());
			for (INT LODIndex = 0; LODIndex < PartSys->LODSettings.Num(); LODIndex++)
			{
				PartSys->LODSettings(LODIndex) = FParticleSystemLOD::CreateParticleSystemLOD();
			}
		}
	}

	PartSysComp->PostEditChange();
	PartSys->PostEditChange();

	PartSys->SetupSoloing();

	EndTransaction(TEXT("NewEmitter"));

	// Refresh viewport
	EmitterEdVC->Viewport->Invalidate();
}

void WxCascade::OnSelectParticleSystem( wxCommandEvent& In )
{
	SetSelectedEmitter(NULL);
}

void WxCascade::OnRemoveDuplicateModules( wxCommandEvent& In )
{
	if (PartSys != NULL)
	{
		BeginTransaction(TEXT("RemoveDuplicateModules"));
		ModifyParticleSystem(TRUE);

		PartSys->RemoveAllDuplicateModules(FALSE, NULL);

		check(TransactionInProgress());
		EndTransaction(TEXT("RemoveDuplicateModules"));

		PartSys->MarkPackageDirty();
		CascadeTouch();

		wxCommandEvent DummyEvent;
		OnResetInLevel(DummyEvent);
	}
}

void WxCascade::OnNewEmitterBefore( wxCommandEvent& In )
{
	if ((SelectedEmitter != NULL) && (PartSys != NULL))
	{
		INT EmitterCount = PartSys->Emitters.Num();
		INT EmitterIndex = -1;
		for (INT Index = 0; Index < EmitterCount; Index++)
		{
			UParticleEmitter* CheckEmitter = PartSys->Emitters(Index);
			if (SelectedEmitter == CheckEmitter)
			{
				EmitterIndex = Index;
				break;
			}
		}

		if (EmitterIndex != -1)
		{
			debugf(TEXT("Insert New Emitter Before %d"), EmitterIndex);

			// Fake create it at the end
			wxCommandEvent DummyIn;
			DummyIn.SetId(IDM_CASCADE_NEW_EMITTER_START);
			OnNewEmitter(DummyIn);

			if (EmitterCount + 1 == PartSys->Emitters.Num())
			{
				UParticleEmitter* NewEmitter = PartSys->Emitters(EmitterCount);
				SetSelectedEmitter(NewEmitter);
				MoveSelectedEmitter(EmitterIndex - EmitterCount);
			}
		}
	}
}

void WxCascade::OnNewEmitterAfter( wxCommandEvent& In )
{
	if ((SelectedEmitter != NULL) && (PartSys != NULL))
	{
		INT EmitterCount = PartSys->Emitters.Num();
		INT EmitterIndex = -1;
		for (INT Index = 0; Index < EmitterCount; Index++)
		{
			UParticleEmitter* CheckEmitter = PartSys->Emitters(Index);
			if (SelectedEmitter == CheckEmitter)
			{
				EmitterIndex = Index;
				break;
			}
		}

		if (EmitterIndex != -1)
		{
			debugf(TEXT("Insert New Emitter After  %d"), EmitterIndex);

			// Fake create it at the end
			wxCommandEvent DummyIn;
			DummyIn.SetId(IDM_CASCADE_NEW_EMITTER_START);
			OnNewEmitter(DummyIn);

			if (EmitterCount + 1 == PartSys->Emitters.Num())
			{
				UParticleEmitter* NewEmitter = PartSys->Emitters(EmitterCount);
				SetSelectedEmitter(NewEmitter);
				if (EmitterIndex + 1 < EmitterCount)
				{
					MoveSelectedEmitter(EmitterIndex - EmitterCount + 1);
				}
			}
		}
	}
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

	UBOOL bShare = FALSE;
	if (In.GetId() == IDM_CASCADE_DUPLICATE_SHARE_EMITTER)
	{
		bShare = TRUE;
	}

	BeginTransaction(TEXT("EmitterDuplicate"));

	PartSys->PreEditChange(NULL);
	PartSysComp->PreEditChange(NULL);

	if (!DuplicateEmitter(SelectedEmitter, PartSys, bShare))
	{
	}
	PartSysComp->PostEditChange();
	PartSys->PostEditChange();

	EndTransaction(TEXT("EmitterDuplicate"));

	// Refresh viewport
	EmitterEdVC->Viewport->Invalidate();
}

void WxCascade::OnDeleteEmitter(wxCommandEvent& In)
{
	DeleteSelectedEmitter();
}

void WxCascade::OnExportEmitter(wxCommandEvent& In)
{
	ExportSelectedEmitter();
}

void WxCascade::OnExportAll(wxCommandEvent& In)
{
	ExportAllEmitters();
}

void WxCascade::OnAddSelectedModule(wxCommandEvent& In)
{
}

void WxCascade::OnCopyModule(wxCommandEvent& In)
{
	if (SelectedModule)
	{
		SetCopyModule(SelectedEmitter, SelectedModule);
	}
}

void WxCascade::OnPasteModule(wxCommandEvent& In)
{
	if (!CopyModule)
	{
		appMsgf(AMT_OK, *LocalizeUnrealEd("Prompt_5"));
		return;
	}

	if (SelectedEmitter && CopyEmitter && (SelectedEmitter == CopyEmitter))
	{
		// Can't copy onto ourselves... Or can we
		appMsgf(AMT_OK, *LocalizeUnrealEd("Prompt_6"));
		return;
	}

	PasteCurrentModule();
}

void WxCascade::OnDeleteModule(wxCommandEvent& In)
{
	DeleteSelectedModule();
}

void WxCascade::OnEnableModule(wxCommandEvent& In)
{
	EnableSelectedModule();
}

void WxCascade::OnResetModule(wxCommandEvent& In)
{
	ResetSelectedModule();
}

void WxCascade::OnRefreshModule(wxCommandEvent& In)
{
	RefreshSelectedModule();
}

/** Sync the sprite material in the generic browser */
void WxCascade::OnModuleSyncMaterial( wxCommandEvent& In )
{
	TArray<UObject*> Objects;

	if (SelectedModule)
	{
		UParticleModuleRequired* RequiredModule = Cast<UParticleModuleRequired>(SelectedModule);
		if (RequiredModule)
		{
			Objects.AddItem(RequiredModule->Material);
		}
	}

	// Sync the generic browser to the object list.
	GApp->EditorFrame->SyncBrowserToObjects(Objects);
}

/** Assign the selected material to the sprite material */
void WxCascade::OnModuleUseMaterial( wxCommandEvent& In )
{
	if (SelectedModule && SelectedEmitter)
	{
		UParticleModuleRequired* RequiredModule = Cast<UParticleModuleRequired>(SelectedModule);
		if (RequiredModule)
		{
			GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);
			UObject* Obj = GEditor->GetSelectedObjects()->GetTop(UMaterialInterface::StaticClass());
			if (Obj)
			{
				UMaterialInterface* SelectedMaterial = Cast<UMaterialInterface>(Obj);
				if (SelectedMaterial)
				{
					RequiredModule->Material = SelectedMaterial;
					SelectedEmitter->PostEditChange();
				}
			}
		}
	}
}

void WxCascade::OnModuleDupHighest( wxCommandEvent& In )
{
	DupHighestSelectedModule();
}

void WxCascade::OnModuleDupHigher( wxCommandEvent& In )
{
	DupHigherSelectedModule();
}

/**
 *	Set the module to the SAME module in the next higher LOD level.
 */
void WxCascade::OnModuleShareHigher( wxCommandEvent& In )
{
	ShareHigherSelectedModule();
}

/** Set the random seed on a seeded module */
void WxCascade::OnModuleSetRandomSeed( wxCommandEvent& In )
{
	if ((SelectedModule != NULL) && (SelectedModule->SupportsRandomSeed()))
	{
		BeginTransaction(TEXT("CASC_SetRandomSeed"));

		PartSys->PreEditChange(NULL);
		PartSysComp->PreEditChange(NULL);

		INT RandomSeed = appRound(RAND_MAX * appSRand());
		if (SelectedModule->SetRandomSeedEntry(0, RandomSeed) == FALSE)
		{
			warnf(NAME_Warning, TEXT("Failed to set random seed entry on module %s"), *(SelectedModule->GetClass()->GetName()));
		}

		PartSysComp->PostEditChange();
		PartSys->PostEditChange();

		EndTransaction(TEXT("CASC_SetRandomSeed"));

		// Refresh viewport
		EmitterEdVC->Viewport->Invalidate();
		PropertyWindow->Rebuild();
	}
}

/** Convert the selected module to the seeded version of itself */
void WxCascade::OnModuleConvertToSeeded( wxCommandEvent& In )
{
	if ((SelectedModule != NULL) && (SelectedModule->SupportsRandomSeed() == FALSE))
	{
		// See if there is a seeded version of this module...
		UClass* CurrentClass = SelectedModule->GetClass();
		check(CurrentClass);
		FString ClassName = CurrentClass->GetName();
		debugf(TEXT("Non-seeded module %s"), *ClassName);
		// This only works if the seeded version is names <ClassName>_Seeded!!!!
		FString SeededClassName = ClassName + TEXT("_Seeded");
		UClass* SeededClass = FindObject<UClass>(ANY_PACKAGE, *SeededClassName);
		if (SeededClass != NULL)
		{
			// Find the module index
			UParticleLODLevel* BaseLODLevel = GetCurrentlySelectedLODLevel();
			if (BaseLODLevel != NULL)
			{
				check(BaseLODLevel->Level == 0);

				INT ConvertModuleIdx = INDEX_NONE;
				for (INT CheckModuleIdx = 0; CheckModuleIdx < BaseLODLevel->Modules.Num(); CheckModuleIdx++)
				{
					if (BaseLODLevel->Modules(CheckModuleIdx) == SelectedModule)
					{
						ConvertModuleIdx = CheckModuleIdx;
						break;
					}
				}

				check(ConvertModuleIdx != INDEX_NONE);

				// We need to do this for *all* copies of this module.
				BeginTransaction(TEXT("CASC_ConvertToSeeded"));
				if (PartSys->ConvertModuleToSeeded(SelectedEmitter, ConvertModuleIdx, SeededClass, TRUE) == FALSE)
				{
					warnf(NAME_Warning, TEXT("Failed to convert module!"));
				}
				EndTransaction(TEXT("CASC_ConvertToSeeded"));
			}
		}
	}
}

/** Handle custom module menu options */
void WxCascade::OnModuleCustom( wxCommandEvent& In )
{
	if (SelectedModule != NULL)
	{
		INT Id = In.GetId();
		if ((Id >= IDM_CASCADE_MODULE_CUSTOM0) && (Id <= IDM_CASCADE_MODULE_CUSTOM2))
		{
			// Parse the custom command number to run
			INT CustomCommand = Id - IDM_CASCADE_MODULE_CUSTOM0;
			// Run it on the selected module
			if (SelectedModule->PerformCustomMenuEntry(CustomCommand) == TRUE)
			{
// 				PropertyWindow->Rebuild();
// 				PropertyWindow->Update();
				UParticleModule* SaveModule = SelectedModule;
				SetSelectedModule(SelectedEmitter, NULL);
				SetSelectedModule(SelectedEmitter, SaveModule);
			}
		}
	}
}

void WxCascade::OnMenuSimSpeed(wxCommandEvent& In)
{
	INT Id = In.GetId();

	if (Id == IDM_CASCADE_SIM_PAUSE)
	{
		PreviewVC->TimeScale = 0.f;
		ToolBar->ToggleTool(IDM_CASCADE_PLAY, FALSE);
		ToolBar->ToggleTool(IDM_CASCADE_PAUSE, TRUE);
	}
	else
	{
		if ((Id == IDM_CASCADE_SIM_1PERCENT) || 
			(Id == IDM_CASCADE_SIM_10PERCENT) || 
			(Id == IDM_CASCADE_SIM_25PERCENT) || 
			(Id == IDM_CASCADE_SIM_50PERCENT) || 
			(Id == IDM_CASCADE_SIM_NORMAL))
		{
			ToolBar->ToggleTool(IDM_CASCADE_PLAY, TRUE);
			ToolBar->ToggleTool(IDM_CASCADE_PAUSE, FALSE);
		}

		if (Id == IDM_CASCADE_SIM_1PERCENT)
		{
			PreviewVC->TimeScale = 0.01f;
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_1, TRUE);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_10, FALSE);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_25, FALSE);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_50, FALSE);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_100, FALSE);
		}
		else if (Id == IDM_CASCADE_SIM_10PERCENT)
		{
			PreviewVC->TimeScale = 0.1f;
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_1, FALSE);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_10, TRUE);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_25, FALSE);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_50, FALSE);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_100, FALSE);
		}
		else if (Id == IDM_CASCADE_SIM_25PERCENT)
		{
			PreviewVC->TimeScale = 0.25f;
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_1, FALSE);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_10, FALSE);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_25, TRUE);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_50, FALSE);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_100, FALSE);
		}
		else if (Id == IDM_CASCADE_SIM_50PERCENT)
		{
			PreviewVC->TimeScale = 0.5f;
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_1, FALSE);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_10, FALSE);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_25, FALSE);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_50, TRUE);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_100, FALSE);
		}
		else if (Id == IDM_CASCADE_SIM_NORMAL)
		{
			PreviewVC->TimeScale = 1.f;
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_1, FALSE);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_10, FALSE);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_25, FALSE);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_50, FALSE);
			ToolBar->ToggleTool(IDM_CASCADE_SPEED_100, TRUE);
		}
	}
}

void WxCascade::OnSaveCam(wxCommandEvent& In)
{
	PartSys->ThumbnailAngle = PreviewVC->PreviewAngle;
	PartSys->ThumbnailDistance = PreviewVC->PreviewDistance;
	PartSys->PreviewComponent = NULL;

	PreviewVC->bCaptureScreenShot = TRUE;
}

void WxCascade::OnResetSystem(wxCommandEvent& In)
{
	if (PartSysComp)
	{
		PartSysComp->ResetParticles();
		PartSysComp->ActivateSystem();
		PartSysComp->Template->bShouldResetPeakCounts = TRUE;
		PartSysComp->bIsViewRelevanceDirty = TRUE;
		PartSysComp->CachedViewRelevanceFlags.Empty();
		PartSysComp->CacheViewRelevanceFlags();
		if (PreviewVC)
		{
			PreviewVC->PreviewScene.RemoveComponent(PartSysComp);
			PreviewVC->PreviewScene.AddComponent(PartSysComp, FMatrix::Identity);
		}
	}

	if (PartSys)
	{
		PartSys->CalculateMaxActiveParticleCounts();
	}

	UpdateMemoryInformation();
}

void WxCascade::OnResetInLevel(wxCommandEvent& In)
{
	OnResetSystem(In);
	for (TObjectIterator<UParticleSystemComponent> It;It;++It)
	{
		if (It->Template == PartSysComp->Template)
		{
			UParticleSystemComponent* PSysComp = *It;

			// Check for a valid template
			check(PSysComp->Template);

			// Force a recache of the view relevance
			PSysComp->bIsViewRelevanceDirty = TRUE;

			PSysComp->ResetParticles();
			PSysComp->bIsViewRelevanceDirty = TRUE;
			PSysComp->CachedViewRelevanceFlags.Empty();
			PSysComp->Template->bShouldResetPeakCounts = TRUE;
			PSysComp->ActivateSystem();
			PSysComp->CacheViewRelevanceFlags();

			PSysComp->BeginDeferredReattach();
		}
	}
}

void WxCascade::OnSyncGenericBrowser(wxCommandEvent& In)
{
	// Sync the particle system in the generic browser
	if (PartSys)
	{
		TArray<UObject*> Objects;
		Objects.AddItem(PartSys);

		GApp->EditorFrame->SyncBrowserToObjects(Objects);
	}
}

void WxCascade::OnResetPeakCounts(wxCommandEvent& In)
{
	PartSysComp->ResetParticles();
/***
	for (INT i = 0; i < PartSysComp->Template->Emitters.Num(); i++)
	{
		UParticleEmitter* Emitter = PartSysComp->Template->Emitters(i);
		for (INT LODIndex = 0; LODIndex < Emitter->LODLevels.Num(); LODIndex++)
		{
			UParticleLODLevel* LODLevel = Emitter->LODLevels(LODIndex);
			LODLevel->PeakActiveParticles = 0;
		}
	}
***/
	PartSysComp->Template->bShouldResetPeakCounts = TRUE;
	PartSysComp->InitParticles();
}

void WxCascade::OnUberConvert(wxCommandEvent& In)
{
	if (!SelectedEmitter)
	{
		appMsgf(AMT_OK, *LocalizeUnrealEd("Error_MustSelectEmitter"));
		return;
	}

	if (appMsgf(AMT_YesNo, *LocalizeUnrealEd("UberModuleConvertConfirm")))
	{
		BeginTransaction(TEXT("EmitterUberConvert"));

		// Find the best uber module
		UParticleModuleUberBase* UberModule	= Cast<UParticleModuleUberBase>(
			UParticleModuleUberBase::DetermineBestUberModule(SelectedEmitter));
		if (!UberModule)
		{
			appMsgf(AMT_OK, *LocalizeUnrealEd("Error_FailedToFindUberModule"));
			EndTransaction(TEXT("EmitterUberConvert"));
			return;
		}

		// Convert it
		if (UberModule->ConvertToUberModule(SelectedEmitter) == FALSE)
		{
			appMsgf(AMT_OK, *LocalizeUnrealEd("Error_FailedToConverToUberModule"));
			EndTransaction(TEXT("EmitterUberConvert"));
			return;
		}

		EndTransaction(TEXT("EmitterUberConvert"));

		// Mark package as dirty
		SelectedEmitter->MarkPackageDirty();

		// Redraw the module window
		EmitterEdVC->Viewport->Invalidate();
	}
}

/**
 *	OnRegenerateLowestLOD
 *	This function is supplied to remove all LOD levels and regenerate the lowest.
 *	It is intended for use once the artists/designers decide on a suitable baseline
 *	for the lowest LOD generation parameters.
 */
void WxCascade::OnRegenerateLowestLOD(wxCommandEvent& In)
{
	if ((PartSys == NULL) || (PartSys->Emitters.Num() == 0))
	{
		return;
	}

	PartSys->bRegenerateLODDuplicate = FALSE;

	FString	WarningMessage(TEXT(""));

	WarningMessage += *LocalizeUnrealEd("CascadeRegenLowLODWarningLine1");
	WarningMessage += TEXT("\n");
	WarningMessage += *LocalizeUnrealEd("CascadeRegenLowLODWarningLine2");
	WarningMessage += TEXT("\n");
	WarningMessage += *LocalizeUnrealEd("CascadeRegenLowLODWarningLine3");
	WarningMessage += TEXT("\n");
	WarningMessage += *LocalizeUnrealEd("CascadeRegenLowLODWarningLine4");

	if (appMsgf(AMT_YesNo, *WarningMessage) == TRUE)
	{
		debugf(TEXT("Regenerate Lowest LOD levels!"));

		BeginTransaction(TEXT("CascadeRegenerateLowestLOD"));
		ModifyParticleSystem(TRUE);

		// Delete all LOD levels from each emitter...
		for (INT EmitterIndex = 0; EmitterIndex < PartSys->Emitters.Num(); EmitterIndex++)
		{
			UParticleEmitter*	Emitter	= PartSys->Emitters(EmitterIndex);
			if (Emitter)
			{
				for (INT LODIndex = Emitter->LODLevels.Num() - 1; LODIndex > 0; LODIndex--)
				{
					Emitter->LODLevels.Remove(LODIndex);
				}
				if (Emitter->AutogenerateLowestLODLevel(PartSys->bRegenerateLODDuplicate) == FALSE)
				{
					warnf(TEXT("Failed to autogenerate lowest LOD level!"));
				}

				Emitter->UpdateModuleLists();
			}
		}

		// Reset the LOD distances
		PartSys->LODDistances.Empty();
		PartSys->LODSettings.Empty();
		UParticleEmitter* SourceEmitter = PartSys->Emitters(0);
		if (SourceEmitter)
		{
			PartSys->LODDistances.Add(SourceEmitter->LODLevels.Num());
			for (INT LODIndex = 0; LODIndex < PartSys->LODDistances.Num(); LODIndex++)
			{
				PartSys->LODDistances(LODIndex) = LODIndex * 2500.0f;
			}
			PartSys->LODSettings.Add(SourceEmitter->LODLevels.Num());
			for (INT LODIndex = 0; LODIndex < PartSys->LODSettings.Num(); LODIndex++)
			{
				PartSys->LODSettings(LODIndex) = FParticleSystemLOD::CreateParticleSystemLOD();
			}
		}

		PartSys->SetupSoloing();

		check(TransactionInProgress());
		EndTransaction(TEXT("CascadeRegenerateLowestLOD"));

		// Re-fill the LODCombo so that deleted LODLevels are removed.
		EmitterEdVC->Viewport->Invalidate();
		PropertyWindow->Rebuild();

		wxCommandEvent DummyEvent;
		OnResetInLevel(DummyEvent);
	}
	else
	{
		debugf(TEXT("CANCELLED Regenerate Lowest LOD levels!"));
	}

	UpdateLODLevelControls();
}

/**
 *	OnRegenerateLowestLODDuplicateHighest
 *	This function is supplied to remove all LOD levels and regenerate the lowest.
 *	It is intended for use once the artists/designers decide on a suitable baseline
 *	for the lowest LOD generation parameters.
 *	It will duplicate the highest LOD as the lowest.
 */
void WxCascade::OnRegenerateLowestLODDuplicateHighest(wxCommandEvent& In)
{
	if ((PartSys == NULL) || (PartSys->Emitters.Num() == 0))
	{
		return;
	}

	PartSys->bRegenerateLODDuplicate = TRUE;

	FString	WarningMessage(TEXT(""));

	WarningMessage += *LocalizeUnrealEd("CascadeRegenLowLODWarningLine1");
	WarningMessage += TEXT("\n");
	WarningMessage += *LocalizeUnrealEd("CascadeRegenLowLODWarningLine2");
	WarningMessage += TEXT("\n");
	WarningMessage += *LocalizeUnrealEd("CascadeRegenLowLODWarningLine3");
	WarningMessage += TEXT("\n");
	WarningMessage += *LocalizeUnrealEd("CascadeRegenLowLODWarningLine4");

	if (appMsgf(AMT_YesNo, *WarningMessage) == TRUE)
	{
		debugf(TEXT("Regenerate Lowest LOD levels!"));

		BeginTransaction(TEXT("CascadeRegenerateLowestLOD"));
		ModifyParticleSystem(TRUE);

		// Delete all LOD levels from each emitter...
		for (INT EmitterIndex = 0; EmitterIndex < PartSys->Emitters.Num(); EmitterIndex++)
		{
			UParticleEmitter*	Emitter	= PartSys->Emitters(EmitterIndex);
			if (Emitter)
			{
				for (INT LODIndex = Emitter->LODLevels.Num() - 1; LODIndex > 0; LODIndex--)
				{
					Emitter->LODLevels.Remove(LODIndex);
				}
				if (Emitter->AutogenerateLowestLODLevel(PartSys->bRegenerateLODDuplicate) == FALSE)
				{
					warnf(TEXT("Failed to autogenerate lowest LOD level!"));
				}

				Emitter->UpdateModuleLists();
			}
		}

		// Reset the LOD distances
		PartSys->LODDistances.Empty();
		PartSys->LODSettings.Empty();
		UParticleEmitter* SourceEmitter = PartSys->Emitters(0);
		if (SourceEmitter)
		{
			PartSys->LODDistances.Add(SourceEmitter->LODLevels.Num());
			for (INT LODIndex = 0; LODIndex < PartSys->LODDistances.Num(); LODIndex++)
			{
				PartSys->LODDistances(LODIndex) = LODIndex * 2500.0f;
			}
			PartSys->LODSettings.Add(SourceEmitter->LODLevels.Num());
			for (INT LODIndex = 0; LODIndex < PartSys->LODSettings.Num(); LODIndex++)
			{
				PartSys->LODSettings(LODIndex) = FParticleSystemLOD::CreateParticleSystemLOD();
			}
		}

		PartSys->SetupSoloing();

		wxCommandEvent DummyEvent;
		OnResetInLevel(DummyEvent);

		check(TransactionInProgress());
		EndTransaction(TEXT("CascadeRegenerateLowestLOD"));

		// Re-fill the LODCombo so that deleted LODLevels are removed.
		EmitterEdVC->Viewport->Invalidate();
		PropertyWindow->Rebuild();
		if (PartSysComp)
		{
			PartSysComp->ResetParticles();
			PartSysComp->InitializeSystem();
		}
	}
	else
	{
		debugf(TEXT("CANCELLED Regenerate Lowest LOD levels!"));
	}

	UpdateLODLevelControls();
}

void WxCascade::OnSavePackage(wxCommandEvent& In)
{
	if (!PartSys)
	{
		appMsgf(AMT_OK, TEXT("No particle system active..."));
		return;
	}

	UPackage* Package = Cast<UPackage>(PartSys->GetOutermost());
	if (Package)
	{
		FString FileTypes( TEXT("All Files|*.*") );
		
		for (INT i=0; i<GSys->Extensions.Num(); i++)
		{
			FileTypes += FString::Printf( TEXT("|(*.%s)|*.%s"), *GSys->Extensions(i), *GSys->Extensions(i) );
		}

		if (FindObject<UWorld>(Package, TEXT("TheWorld")))
		{
			appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_CantSaveMapViaCascade"), *Package->GetName()));
		}
		else
		{
			// Prompt the user to check out the package if necessary, and then save the packages!
			TArray<UPackage*> PkgsToSave;
			PkgsToSave.AddItem(Package);
			FEditorFileUtils::PromptForCheckoutAndSave(PkgsToSave, false, false);
		}

		if (PartSys)
		{
			PartSys->PostEditChange();
		}
	}
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

void WxCascade::OnMotionMode(wxCommandEvent& In)
{
	bMotionMode = In.IsChecked();
	if (bMotionMode == FALSE)
	{
		if (PartSysComp)
		{
			// Reset the component system to the origin
			FVector Position(0.0f);
			PartSysComp->LocalToWorld = FTranslationMatrix(Position);
		}
	}
}

void WxCascade::OnViewMode(wxCommandEvent& In)
{
	if (PreviewVC)
	{
		EShowFlags CurrentShowFlags = PreviewVC->ShowFlags & SHOW_ViewMode_Mask;
		INT NewViewModeId = IDM_UNLIT;

		// ShaderComplexity has to come BEFORE Lighting since ViewMode_ShaderComplexity contains the SHOW_Lighting flag!
		if ((CurrentShowFlags & SHOW_ShaderComplexity) != 0)
		{
			NewViewModeId = IDM_WIREFRAME;
		}
		else if ((CurrentShowFlags & SHOW_TextureDensity) != 0)
		{
			NewViewModeId = IDM_SHADERCOMPLEXITY;
		}
		else if ((CurrentShowFlags & SHOW_Lighting) != 0)
		{
			NewViewModeId = IDM_TEXTUREDENSITY;
		}
		else if ((CurrentShowFlags & (SHOW_Wireframe|SHOW_Lighting|SHOW_TextureDensity|SHOW_ShaderComplexity)) == 0)
		{
			NewViewModeId = IDM_LIT;
		}
		else if ((CurrentShowFlags & SHOW_Wireframe) != 0)
		{
			NewViewModeId = IDM_UNLIT;
		}

		wxCommandEvent DummyEvent;
		DummyEvent.SetId(NewViewModeId);
		OnSetViewMode(DummyEvent);
	}
}

void WxCascade::OnViewModeRightClick(wxCommandEvent& In)
{
	WxCascadeViewModeMenu* menu = new WxCascadeViewModeMenu(this);
	if (menu)
	{
		FTrackPopupMenu tpm(this, menu);
		tpm.Show();
		delete menu;
	}
}

void WxCascade::OnSetViewMode(wxCommandEvent& In)
{
	INT Id = In.GetId();
	EShowFlags NewShowFlags = 0;
	switch (Id)
	{
	case IDM_WIREFRAME:
		NewShowFlags = SHOW_ViewMode_Wireframe;
		ToolBar->SetToolNormalBitmap(IDM_CASCADE_VIEWMODE, ToolBar->WireframeB);
		break;
	case IDM_UNLIT:
		NewShowFlags = SHOW_ViewMode_Unlit;
		ToolBar->SetToolNormalBitmap(IDM_CASCADE_VIEWMODE, ToolBar->UnlitB);
		break;
	case IDM_LIT:
		NewShowFlags = SHOW_ViewMode_Lit;
		ToolBar->SetToolNormalBitmap(IDM_CASCADE_VIEWMODE, ToolBar->LitB);
		break;
	case IDM_TEXTUREDENSITY:
		NewShowFlags = SHOW_ViewMode_TextureDensity;
		ToolBar->SetToolNormalBitmap(IDM_CASCADE_VIEWMODE, ToolBar->TextureDensityB);
		break;
	case IDM_SHADERCOMPLEXITY:
		NewShowFlags = SHOW_ViewMode_ShaderComplexity;
		ToolBar->SetToolNormalBitmap(IDM_CASCADE_VIEWMODE, ToolBar->ShaderComplexityB);
		break;
	}

	if (NewShowFlags != 0)
	{
		PreviewVC->ShowFlags &= ~SHOW_ViewMode_Mask;
		PreviewVC->ShowFlags |= NewShowFlags;
		PreviewVC->Invalidate();
	}
}

void WxCascade::OnBounds(wxCommandEvent& In)
{
	bBounds = In.IsChecked();
	PreviewVC->bBounds = bBounds;
}

void WxCascade::OnBoundsRightClick(wxCommandEvent& In)
{
	if ((PartSys != NULL) && (PartSysComp != NULL))
	{
		if (appMsgf(AMT_YesNo, *LocalizeUnrealEd("Casc_SetFixedBounds")))
		{
			BeginTransaction(TEXT("CascadeSetFixedBounds"));

			// Grab the current bounds of the PSysComp & set it on the PSystem itself
			PartSys->FixedRelativeBoundingBox.Min = PartSysComp->Bounds.GetBoxExtrema(0);
			PartSys->FixedRelativeBoundingBox.Max = PartSysComp->Bounds.GetBoxExtrema(1);
			PartSys->FixedRelativeBoundingBox.IsValid = TRUE;
			PartSys->bUseFixedRelativeBoundingBox = TRUE;

			PartSys->MarkPackageDirty();

			EndTransaction(TEXT("CascadeSetFixedBounds"));

			if ((SelectedModule == NULL) && (SelectedEmitter == NULL))
			{
				PropertyWindow->SetObject(NULL, EPropertyWindowFlags::ShouldShowCategories | EPropertyWindowFlags::Sorted);
				PropertyWindow->SetObject(PartSys, EPropertyWindowFlags::ShouldShowCategories | EPropertyWindowFlags::Sorted);
			}
			PropertyWindow->Update();
		}
	}
}

/**
 *	Handler for turning post processing on and off.
 *
 *	@param	In	wxCommandEvent
 */
void WxCascade::OnPostProcess(wxCommandEvent& In)
{
	PreviewVC->bShowPostProcess = !PreviewVC->bShowPostProcess;
}

/**
 *	Handler for turning the grid on and off.
 *
 *	@param	In	wxCommandEvent
 */
void WxCascade::OnToggleGrid(wxCommandEvent& In)
{
	bool bShowGrid = In.IsChecked();

	if (PreviewVC)
	{
		// Toggle the grid and worldbox.
		EditorOptions->bShowGrid = bShowGrid;
		EditorOptions->SaveConfig();
		PreviewVC->DrawHelper.bDrawGrid = bShowGrid;
		if (bShowGrid)
		{
			PreviewVC->ShowFlags |= SHOW_Grid;
		}
		else
		{
			PreviewVC->ShowFlags &= ~SHOW_Grid;
		}
	}
}

void WxCascade::OnViewAxes(wxCommandEvent& In)
{
	PreviewVC->bDrawOriginAxes = In.IsChecked();
}

void WxCascade::OnViewCounts(wxCommandEvent& In)
{
	PreviewVC->bDrawParticleCounts = In.IsChecked();
	EditorOptions->bShowParticleCounts = PreviewVC->bDrawParticleCounts;
	EditorOptions->SaveConfig();
}

void WxCascade::OnViewEvents(wxCommandEvent& In)
{
	PreviewVC->bDrawParticleEvents = In.IsChecked();
	EditorOptions->bShowParticleEvents = PreviewVC->bDrawParticleEvents;
	EditorOptions->SaveConfig();
}

void WxCascade::OnViewTimes(wxCommandEvent& In)
{
	PreviewVC->bDrawParticleTimes = In.IsChecked();
	EditorOptions->bShowParticleTimes = PreviewVC->bDrawParticleTimes;
	EditorOptions->SaveConfig();
}

void WxCascade::OnViewDistance(wxCommandEvent& In)
{
	PreviewVC->bDrawSystemDistance = In.IsChecked();
	EditorOptions->bShowParticleDistance = PreviewVC->bDrawSystemDistance;
	EditorOptions->SaveConfig();
}

void WxCascade::OnViewMemory(wxCommandEvent& In)
{
	PreviewVC->bDrawParticleMemory = In.IsChecked();
	EditorOptions->bShowParticleMemory = PreviewVC->bDrawParticleMemory;
	EditorOptions->SaveConfig();
}
void WxCascade::OnViewGeometry(wxCommandEvent& In)
{
	if (PreviewVC->FloorComponent)
	{
		PreviewVC->FloorComponent->HiddenEditor = !In.IsChecked();
		PreviewVC->FloorComponent->HiddenGame = PreviewVC->FloorComponent->HiddenEditor;

		EditorOptions->bShowFloor = !PreviewVC->FloorComponent->HiddenEditor;
		EditorOptions->SaveConfig();

		PreviewVC->PreviewScene.RemoveComponent(PreviewVC->FloorComponent);
		PreviewVC->PreviewScene.AddComponent(PreviewVC->FloorComponent, FMatrix::Identity);
	}
}

void WxCascade::OnViewGeometryProperties(wxCommandEvent& In)
{
	if (PreviewVC->FloorComponent)
	{
		WxPropertyWindowFrame* Properties = new WxPropertyWindowFrame;
		Properties->Create(this, -1, &PropWindowNotifyHook);
		Properties->AllowClose();
		Properties->SetObject(PreviewVC->FloorComponent,EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories);
		Properties->SetTitle(*FString::Printf(TEXT("Properties: %s"), *PreviewVC->FloorComponent->GetPathName()));
		Properties->Show();
		PropWindowNotifyHook.Cascade = this;
		PropWindowNotifyHook.WindowOfInterest = (void*)(Properties->GetPropertyWindowForCallbacks());
	}
}

void WxCascade::OnLoopSimulation(wxCommandEvent& In)
{
	bResetOnFinish = In.IsChecked();

	if (!bResetOnFinish)
		bPendingReset = FALSE;
}

void WxCascade::OnSetMotionRadius( wxCommandEvent& In )
{
	// Set the default text to the current input/output.
	FString DefaultNum = FString::Printf(TEXT("%4.2f"), EditorOptions->MotionModeRadius);
	FString TitleString = FString("SetValue");
	FString CaptionString = FString("MotionRadius");

	// Show generic string entry dialog box
	WxDlgGenericStringEntry dlg;
	INT Result = dlg.ShowModal(*TitleString, *CaptionString, *DefaultNum);
	if (Result != wxID_OK)
	{
		return;
	}

	// Convert from string to float (if we can).
	DOUBLE dNewNum;
	const UBOOL bIsNumber = dlg.GetStringEntry().GetValue().ToDouble(&dNewNum);
	if (!bIsNumber)
	{
		return;
	}
	EditorOptions->MotionModeRadius = (FLOAT)dNewNum;
	EditorOptions->SaveConfig();
	MotionModeRadius = EditorOptions->MotionModeRadius;
}

void WxCascade::OnViewDetailModeLow( wxCommandEvent& In )
{
	OnSetDetailMode(DM_Low);
}

void WxCascade::OnViewDetailModeMedium( wxCommandEvent& In )
{
	OnSetDetailMode(DM_Medium);
}

void WxCascade::OnViewDetailModeHigh( wxCommandEvent& In )
{
	OnSetDetailMode(DM_High);
}

void WxCascade::UI_ViewDetailModeLow( wxUpdateUIEvent& In )
{
	In.Check(PreviewVC->DetailMode == DM_Low);
}

void WxCascade::UI_ViewDetailModeMedium( wxUpdateUIEvent& In )
{
	In.Check(PreviewVC->DetailMode == DM_Medium);
}

void WxCascade::UI_ViewDetailModeHigh( wxUpdateUIEvent& In )
{
	In.Check(PreviewVC->DetailMode == DM_High);
}

#if defined(_CASCADE_ENABLE_MODULE_DUMP_)
void WxCascade::OnViewModuleDump(wxCommandEvent& In)
{
	EmitterEdVC->bDrawModuleDump = !EmitterEdVC->bDrawModuleDump;
	EditorOptions->bShowModuleDump = EmitterEdVC->bDrawModuleDump;
	EditorOptions->SaveConfig();

	EmitterEdVC->Viewport->Invalidate();
}
#endif	//#if defined(_CASCADE_ENABLE_MODULE_DUMP_)

void WxCascade::OnPlay(wxCommandEvent& In)
{
	PreviewVC->TimeScale = SimSpeed;
}

void WxCascade::OnPause(wxCommandEvent& In)
{
	PreviewVC->TimeScale = 0.f;
}

void WxCascade::OnSetSpeed(wxCommandEvent& In)
{
	INT SimID = IDM_CASCADE_SPEED_100;
	if (SimSpeed == 1.0f)
	{
		SimID = IDM_CASCADE_SPEED_50;
	}
	else if (SimSpeed == 0.5f)
	{
		SimID = IDM_CASCADE_SPEED_25;
	}
	else if (SimSpeed == 0.25f)
	{
		SimID = IDM_CASCADE_SPEED_10;
	}
	else if (SimSpeed == 0.1f)
	{
		SimID = IDM_CASCADE_SPEED_1;
	}
	else if (SimSpeed == 0.01f)
	{
		SimID = IDM_CASCADE_SPEED_100;
	}

	wxCommandEvent DummyEvent;
	DummyEvent.SetId(SimID);
	OnSpeed(DummyEvent);
}

void WxCascade::OnSetSpeedRightClick(wxCommandEvent& In)
{
	WxCascadeSimSpeedMenu* menu = new WxCascadeSimSpeedMenu(this);
	if (menu)
	{
		FTrackPopupMenu tpm(this, menu);
		tpm.Show();
		delete menu;
	}
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
		ToolBar->SetToolNormalBitmap(IDM_CASCADE_SET_SPEED, ToolBar->Speed1B);
		break;
	case IDM_CASCADE_SPEED_10:
		NewSimSpeed = 0.1f;
		SimID = IDM_CASCADE_SIM_10PERCENT;
		ToolBar->SetToolNormalBitmap(IDM_CASCADE_SET_SPEED, ToolBar->Speed10B);
		break;
	case IDM_CASCADE_SPEED_25:
		NewSimSpeed = 0.25f;
		SimID = IDM_CASCADE_SIM_25PERCENT;
		ToolBar->SetToolNormalBitmap(IDM_CASCADE_SET_SPEED, ToolBar->Speed25B);
		break;
	case IDM_CASCADE_SPEED_50:
		NewSimSpeed = 0.5f;
		SimID = IDM_CASCADE_SIM_50PERCENT;
		ToolBar->SetToolNormalBitmap(IDM_CASCADE_SET_SPEED, ToolBar->Speed50B);
		break;
	case IDM_CASCADE_SPEED_100:
		NewSimSpeed = 1.0f;
		SimID = IDM_CASCADE_SIM_NORMAL;
		ToolBar->SetToolNormalBitmap(IDM_CASCADE_SET_SPEED, ToolBar->Speed100B);
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

void WxCascade::OnRealtime(wxCommandEvent& In)
{
	PreviewVC->SetRealtime(In.IsChecked());
}

void WxCascade::OnBackgroundColor(wxCommandEvent& In)
{
	FPickColorStruct PickColorStruct;
	PickColorStruct.RefreshWindows.AddItem(this);
	if (PartSys)
	{
		PickColorStruct.DWORDColorArray.AddItem(&(PartSys->BackgroundColor));
	}
	else
	{
		PickColorStruct.DWORDColorArray.AddItem(&(PreviewVC->BackgroundColor));
		PickColorStruct.DWORDColorArray.AddItem(&(EditorOptions->BackgroundColor));
	}

	PickColor(PickColorStruct);
}

void WxCascade::OnToggleWireSphere(wxCommandEvent& In)
{
	PreviewVC->bDrawWireSphere = !PreviewVC->bDrawWireSphere;
	if (PreviewVC->bDrawWireSphere)
	{
		// display a dialog box asking fort the radius of the sphere
		WxDlgGenericStringEntry Dialog;
		INT Result = Dialog.ShowModal(TEXT("CascadeToggleWireSphere"), TEXT("SphereRadius"), *FString::Printf(TEXT("%f"), PreviewVC->WireSphereRadius));
		if (Result != wxID_OK)
		{
			// dialog was canceled
			PreviewVC->bDrawWireSphere = FALSE;
			ToolBar->ToggleTool(IDM_CASCADE_TOGGLE_WIRE_SPHERE, FALSE);
		}
		else
		{
			FLOAT NewRadius = appAtof(*Dialog.GetEnteredString());
			// if an invalid number was entered, cancel
			if (NewRadius < KINDA_SMALL_NUMBER)
			{
				PreviewVC->bDrawWireSphere = FALSE;
				ToolBar->ToggleTool(IDM_CASCADE_TOGGLE_WIRE_SPHERE, FALSE);
			}
			else
			{
				PreviewVC->WireSphereRadius = NewRadius;
			}
		}
	}
}

void WxCascade::OnUndo(wxCommandEvent& In)
{
	CascadeUndo();
}

void WxCascade::OnRedo(wxCommandEvent& In)
{
	CascadeRedo();
}

void WxCascade::OnDetailMode(wxCommandEvent& In)
{
	if (PreviewVC->DetailMode == DM_High)
	{
		OnSetDetailMode(DM_Medium);
	}
	else if (PreviewVC->DetailMode == DM_Medium)
	{
		OnSetDetailMode(DM_Low);
	}
	else if (PreviewVC->DetailMode == DM_Low)
	{
		OnSetDetailMode(DM_High);
	}
}

void WxCascade::OnSetDetailMode(INT DetailMode)
{
	if (PreviewVC && ToolBar)
	{
		UBOOL bResetSystem = FALSE;
		if (DetailMode != PreviewVC->DetailMode)
		{
			bResetSystem = TRUE;
		}
		if (DetailMode == DM_High)
		{
			ToolBar->SetToolNormalBitmap(IDM_CASCADE_DETAILMODE, ToolBar->DetailHigh);
			PreviewVC->DetailMode = DM_High;
		}
		else if (DetailMode == DM_Medium)
		{
			ToolBar->SetToolNormalBitmap(IDM_CASCADE_DETAILMODE, ToolBar->DetailMedium);
			PreviewVC->DetailMode = DM_Medium;
		}
		else if (DetailMode == DM_Low)
		{
			ToolBar->SetToolNormalBitmap(IDM_CASCADE_DETAILMODE, ToolBar->DetailLow);
			PreviewVC->DetailMode = DM_Low;
		}
		PreviewVC->Invalidate();

		if (bResetSystem == TRUE)
		{
			wxCommandEvent DummyEvent;
			OnResetSystem(DummyEvent);
		}

		// Set the detail mode values on in-level particle systems
		extern UBOOL GbEnableEditorPSysRealtimeLOD;
		for (TObjectIterator<UParticleSystemComponent> It;It;++It)
		{
			if (It->Template == PartSysComp->Template)
			{
				It->EditorDetailMode = GbEnableEditorPSysRealtimeLOD? GSystemSettings.DetailMode: PreviewVC->DetailMode;
			}
		}
	}
}

void WxCascade::OnDetailModeRightClick(wxCommandEvent& In)
{
	WxCascadeDetailModeMenu* menu = new WxCascadeDetailModeMenu(this);
	if (menu)
	{
		FTrackPopupMenu tpm(this, menu);
		tpm.Show();
		delete menu;
	}
}

void WxCascade::UI_ViewDetailMode( wxUpdateUIEvent& In )
{
	if (PreviewVC && (PreviewVC->GlobalDetailMode != GSystemSettings.DetailMode))
	{
		PreviewVC->GlobalDetailMode = GSystemSettings.DetailMode;
		OnSetDetailMode(GSystemSettings.DetailMode);
	}
}

void WxCascade::OnLODLow(wxCommandEvent& In)
{
	if (!ToolBar || !PartSys || (PartSys->Emitters.Num() == 0))
	{
		return;
	}

	INT	Value = PartSys->Emitters(0)->LODLevels.Num() - 1;

	SetLODValue(Value);
	SetSelectedModule(SelectedEmitter, SelectedModule);
	EmitterEdVC->Viewport->Invalidate();
}

void WxCascade::OnLODLower(wxCommandEvent& In)
{
	if (!ToolBar || !PartSys || (PartSys->Emitters.Num() == 0))
	{
		return;
	}

	INT	LODValue = GetCurrentlySelectedLODLevelIndex();
	// Find the next lower LOD...
	// We can use any emitter, since they will all have the same number of LOD levels
	UParticleEmitter* Emitter	= PartSys->Emitters(0);
	if (Emitter)
	{
		for (INT LODIndex = 0; LODIndex < Emitter->LODLevels.Num(); LODIndex++)
		{
			UParticleLODLevel* LODLevel	= Emitter->LODLevels(LODIndex);
			if (LODLevel)
			{
				if (LODLevel->Level > LODValue)
				{
					SetLODValue(LODLevel->Level);
					SetSelectedModule(SelectedEmitter, SelectedModule);
					EmitterEdVC->Viewport->Invalidate();
					break;
				}
			}
		}
	}
}

void WxCascade::OnLODAddBefore(wxCommandEvent& In)
{
	if (PartSys == NULL)
	{
		return;
	}

	if (bIsSoloing == TRUE)
	{
		FString Description = LocalizeUnrealEd(TEXT("CascadeLODAddBefore"));
		if (PromptForCancellingSoloingMode(Description) == FALSE)
		{
			return;
		}
	}

	// See if there is already a LOD level for this value...
	if (PartSys->Emitters.Num() > 0)
	{
		UParticleEmitter* FirstEmitter = PartSys->Emitters(0);
		if (FirstEmitter)
		{
			if (FirstEmitter->LODLevels.Num() >= 8)
			{
				appMsgf(AMT_OK, *(LocalizeUnrealEd("CascadeTooManyLODs")));
				return;
			}
		}

		INT CurrentLODIndex = GetCurrentlySelectedLODLevelIndex();
		if (CurrentLODIndex < 0)
		{
			return;
		}

		debugf(TEXT("Inserting LOD level at %d"), CurrentLODIndex);

		BeginTransaction(TEXT("CascadeLODAddBefore"));
		ModifyParticleSystem(TRUE);

		for (INT EmitterIndex = 0; EmitterIndex < PartSys->Emitters.Num(); EmitterIndex++)
		{
			UParticleEmitter* Emitter = PartSys->Emitters(EmitterIndex);
			if (Emitter)
			{
				Emitter->CreateLODLevel(CurrentLODIndex);
			}
		}

		PartSys->LODDistances.InsertZeroed(CurrentLODIndex, 1);
		if (CurrentLODIndex == 0)
		{
			PartSys->LODDistances(CurrentLODIndex) = 0.0f;
		}
		else
		{
			PartSys->LODDistances(CurrentLODIndex) = PartSys->LODDistances(CurrentLODIndex - 1);
		}

		PartSys->LODSettings.InsertZeroed(CurrentLODIndex, 1);
		if (CurrentLODIndex == 0)
		{
			PartSys->LODSettings(CurrentLODIndex) = FParticleSystemLOD::CreateParticleSystemLOD();
		}
		else
		{
			PartSys->LODSettings(CurrentLODIndex) = PartSys->LODSettings(CurrentLODIndex - 1);
		}

		PartSys->SetupSoloing();

		check(TransactionInProgress());
		EndTransaction(TEXT("CascadeLODAddBefore"));

		UpdateLODLevelControls();
		SetSelectedModule(SelectedEmitter, SelectedModule);
		CascadeTouch();

		wxCommandEvent DummyEvent;
		OnResetInLevel(DummyEvent);
	}
}

void WxCascade::OnLODAddAfter(wxCommandEvent& In)
{
	if (PartSys == NULL)
	{
		return;
	}

	if (bIsSoloing == TRUE)
	{
		FString Description = LocalizeUnrealEd(TEXT("CascadeLODAddAfter"));
		if (PromptForCancellingSoloingMode(Description) == FALSE)
		{
			return;
		}
	}

	// See if there is already a LOD level for this value...
	if (PartSys->Emitters.Num() > 0)
	{
		UParticleEmitter* FirstEmitter = PartSys->Emitters(0);
		if (FirstEmitter)
		{
			if (FirstEmitter->LODLevels.Num() >= 8)
			{
				appMsgf(AMT_OK, *(LocalizeUnrealEd("CascadeTooManyLODs")));
				return;
			}
		}

		INT CurrentLODIndex = GetCurrentlySelectedLODLevelIndex();
		CurrentLODIndex++;

		debugf(TEXT("Inserting LOD level at %d"), CurrentLODIndex);

		BeginTransaction(TEXT("CascadeLODAddAfter"));
		ModifyParticleSystem(TRUE);

		for (INT EmitterIndex = 0; EmitterIndex < PartSys->Emitters.Num(); EmitterIndex++)
		{
			UParticleEmitter* Emitter = PartSys->Emitters(EmitterIndex);
			if (Emitter)
			{
				Emitter->CreateLODLevel(CurrentLODIndex);
			}
		}

		PartSys->LODDistances.InsertZeroed(CurrentLODIndex, 1);
		if (CurrentLODIndex == 0)
		{
			PartSys->LODDistances(CurrentLODIndex) = 0.0f;
		}
		else
		{
			PartSys->LODDistances(CurrentLODIndex) = PartSys->LODDistances(CurrentLODIndex - 1);
		}

		PartSys->LODSettings.InsertZeroed(CurrentLODIndex, 1);
		if (CurrentLODIndex == 0)
		{
			PartSys->LODSettings(CurrentLODIndex) = FParticleSystemLOD::CreateParticleSystemLOD();
		}
		else
		{
			PartSys->LODSettings(CurrentLODIndex) = PartSys->LODSettings(CurrentLODIndex - 1);
		}

		PartSys->SetupSoloing();

		check(TransactionInProgress());
		EndTransaction(TEXT("CascadeLODAddAfter"));

		UpdateLODLevelControls();
		SetSelectedModule(SelectedEmitter, SelectedModule);
		CascadeTouch();

		wxCommandEvent DummyEvent;
		OnResetInLevel(DummyEvent);
	}
}

void WxCascade::OnLODHigher(wxCommandEvent& In)
{
	if (!ToolBar || !PartSys || (PartSys->Emitters.Num() == 0))
	{
		return;
	}

	INT	LODValue = GetCurrentlySelectedLODLevelIndex();

	// Find the next higher LOD...
	// We can use any emitter, since they will all have the same number of LOD levels
	UParticleEmitter* Emitter	= PartSys->Emitters(0);
	if (Emitter)
	{
		// Go from the low to the high...
		for (INT LODIndex = Emitter->LODLevels.Num() - 1; LODIndex >= 0; LODIndex--)
		{
			UParticleLODLevel* LODLevel	= Emitter->LODLevels(LODIndex);
			if (LODLevel)
			{
				if (LODLevel->Level < LODValue)
				{
					SetLODValue(LODLevel->Level);
					SetSelectedModule(SelectedEmitter, SelectedModule);
					EmitterEdVC->Viewport->Invalidate();
					break;
				}
			}
		}
	}
}

void WxCascade::OnLODHigh(wxCommandEvent& In)
{
	if (!ToolBar || !PartSys || (PartSys->Emitters.Num() == 0))
	{
		return;
	}

	INT	Value = 0;

	SetLODValue(Value);
	SetSelectedModule(SelectedEmitter, SelectedModule);
	EmitterEdVC->Viewport->Invalidate();
}

void WxCascade::OnLODDelete(wxCommandEvent& In)
{
	if (!ToolBar || !PartSys)
	{
		return;
	}

	UParticleEmitter* Emitter = PartSys->Emitters(0);
	if (Emitter == NULL)
	{
		return;
	}

	if (bIsSoloing == TRUE)
	{
		FString Description = LocalizeUnrealEd(TEXT("CascadeLODDelete"));
		if (PromptForCancellingSoloingMode(Description) == FALSE)
		{
			return;
		}
	}

	INT	Selection = GetCurrentlySelectedLODLevelIndex();
	if ((Selection < 0) || ((Selection == 0) && (Emitter->LODLevels.Num() == 1)))
	{
		appMsgf(AMT_OK, *(LocalizeUnrealEd("CascadeCantDeleteLOD")));
		return;
	}

	// Delete the setting...
	BeginTransaction(TEXT("CascadeDeleteLOD"));
	ModifyParticleSystem(TRUE);

	// Remove the LOD entry from the distance array
	for (INT LODIndex = 0; LODIndex < Emitter->LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* LODLevel	= Emitter->LODLevels(LODIndex);
		if (LODLevel)
		{
			if ((LODLevel->Level == Selection) && (PartSys->LODDistances.Num() > LODLevel->Level))
			{
				PartSys->LODDistances.Remove(LODLevel->Level);
				break;
			}
		}
	}

	for (INT LODIndex = 0; LODIndex < Emitter->LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* LODLevel	= Emitter->LODLevels(LODIndex);
		if (LODLevel)
		{
			if ((LODLevel->Level == Selection) && (PartSys->LODSettings.Num() > LODLevel->Level))
			{
				PartSys->LODSettings.Remove(LODLevel->Level);
				break;
			}
		}
	}

	// Remove the level from each emitter in the system
	for (INT EmitterIndex = 0; EmitterIndex < PartSys->Emitters.Num(); EmitterIndex++)
	{
		Emitter = PartSys->Emitters(EmitterIndex);
		if (Emitter)
		{
			for (INT LODIndex = 0; LODIndex < Emitter->LODLevels.Num(); LODIndex++)
			{
				UParticleLODLevel* LODLevel	= Emitter->LODLevels(LODIndex);
				if (LODLevel)
				{
					if (LODLevel->Level == Selection)
					{
						// Clear out the flags from the modules.
						LODLevel->RequiredModule->LODValidity &= ~(1 << LODLevel->Level);
						LODLevel->SpawnModule->LODValidity &= ~(1 << LODLevel->Level);
						if (LODLevel->TypeDataModule)
						{
							LODLevel->TypeDataModule->LODValidity &= ~(1 << LODLevel->Level);
						}

						for (INT ModuleIndex = 0; ModuleIndex < LODLevel->Modules.Num(); ModuleIndex++)
						{
							UParticleModule* PModule = LODLevel->Modules(ModuleIndex);
							if (PModule)
							{
								PModule->LODValidity  &= ~(1 << LODLevel->Level);
							}
						}

						// Delete it and shift all down
						Emitter->LODLevels.Remove(LODIndex);

						for (; LODIndex < Emitter->LODLevels.Num(); LODIndex++)
						{
							UParticleLODLevel* RemapLODLevel	= Emitter->LODLevels(LODIndex);
							if (RemapLODLevel)
							{
								RemapLODLevel->SetLevelIndex(RemapLODLevel->Level - 1);
							}
						}
						break;
					}
				}
			}
		}
	}

	PartSys->SetupSoloing();

	check(TransactionInProgress());
	EndTransaction(TEXT("CascadeDeleteLOD"));

	CascadeTouch();

	PropertyWindow->Rebuild();

	wxCommandEvent DummyEvent;
	OnResetInLevel(DummyEvent);
}

///////////////////////////////////////////////////////////////////////////////////////
// Properties window NotifyHook stuff

void WxCascade::NotifyDestroy( void* Src )
{

}

void WxCascade::NotifyPreChange( void* Src, UProperty* PropertyAboutToChange )
{

}

void WxCascade::NotifyPostChange( void* Src, UProperty* PropertyThatChanged )
{
	FPropertyChangedEvent PropertyEvent(PropertyThatChanged);
	if (SelectedModule)
	{
		SelectedModule->PostEditChangeProperty(PropertyEvent);
	}
	else
	if (SelectedEmitter)
	{
		SelectedEmitter->PostEditChangeProperty(PropertyEvent);
	}
	else
	if (PartSys)
	{
		PartSys->PostEditChangeProperty(PropertyEvent);
	}

	wxCommandEvent DummyEvent;
	OnResetInLevel(DummyEvent);
}

void WxCascade::NotifyPreChange( void* Src, FEditPropertyChain* PropertyChain )
{
	BeginTransaction(TEXT("CascadePropertyChange"));
	ModifyParticleSystem();

	CurveToReplace = NULL;

	// get the property that is being edited
	UObjectProperty* ObjProp = Cast<UObjectProperty>(PropertyChain->GetActiveNode()->GetValue());
	if (ObjProp && 
		(ObjProp->PropertyClass->IsChildOf(UDistributionFloat::StaticClass()) || 
		 ObjProp->PropertyClass->IsChildOf(UDistributionVector::StaticClass()))
		 )
	{
		UParticleModuleParameterDynamic* DynParamModule = Cast<UParticleModuleParameterDynamic>(SelectedModule);
		if (DynParamModule)
		{
			// Grab the curves...
			DynParamModule->GetCurveObjects(DynParamCurves);
		}
		else
		{
			UObject* EditedObject = NULL;
			if (SelectedModule)
			{
				EditedObject = SelectedModule;
			}
			else
	//		if (SelectedEmitter)
			{
				EditedObject = SelectedEmitter;
			}

			// calculate offset from object to property being edited
			DWORD Offset = 0;
			UObject* BaseObject = EditedObject;
			for (FEditPropertyChain::TIterator It(PropertyChain->GetHead()); It; ++It )
			{
				Offset += It->Offset;

				// don't go past the active property
				if (*It == ObjProp)
				{
					break;
				}

				// If it is an object property, then reset our base pointer/offset
				if (It->IsA(UObjectProperty::StaticClass()))
				{
					BaseObject = *(UObject**)((BYTE*)BaseObject + Offset);
					Offset = 0;
				}
			}

			BYTE* CurvePointer = (BYTE*)BaseObject + Offset;
			UObject** ObjPtrPtr = (UObject**)CurvePointer;
			UObject* ObjPtr = *(ObjPtrPtr);
			CurveToReplace = ObjPtr;

			check(CurveToReplace); // These properties are 'noclear', so should always have a curve here!
		}
	}

	if (SelectedModule)
	{
		if (PropertyChain->GetActiveNode()->GetValue()->GetName() == TEXT("InterpolationMethod"))
		{
			UParticleModuleRequired* ReqMod = Cast<UParticleModuleRequired>(SelectedModule);
			if (ReqMod)
			{
				PreviousInterpolationMethod = (EParticleSubUVInterpMethod)(ReqMod->InterpolationMethod);
			}
		}
	}
}

void WxCascade::NotifyPostChange( void* Src, FEditPropertyChain* PropertyChain )
{
	UParticleModuleParameterDynamic* DynParamModule = Cast<UParticleModuleParameterDynamic>(SelectedModule);
	if (DynParamModule)
	{
		if (DynParamCurves.Num() > 0)
		{
			// Grab the curves...
			TArray<FParticleCurvePair> DPCurves;
			DynParamModule->GetCurveObjects(DPCurves);

			check(DPCurves.Num() == DynParamCurves.Num());
			for (INT CurveIndex = 0; CurveIndex < DynParamCurves.Num(); CurveIndex++)
			{
				UObject* OldCurve = DynParamCurves(CurveIndex).CurveObject;
				UObject* NewCurve = DPCurves(CurveIndex).CurveObject;
				if (OldCurve != NewCurve)
				{
					PartSys->CurveEdSetup->ReplaceCurve(OldCurve, NewCurve);
					CurveEd->CurveChanged();
				}
			}
			DynParamCurves.Empty();
		}
	}

	if (CurveToReplace)
	{
		// This should be the same property we just got in NotifyPreChange!
		UObjectProperty* ObjProp = Cast<UObjectProperty>(PropertyChain->GetActiveNode()->GetValue());
		check(ObjProp);
		check(ObjProp->PropertyClass->IsChildOf(UDistributionFloat::StaticClass()) || ObjProp->PropertyClass->IsChildOf(UDistributionVector::StaticClass()));

		UObject* EditedObject = NULL;
		if (SelectedModule)
		{
			EditedObject = SelectedModule;
		}
		else
//		if (SelectedEmitter)
		{
			EditedObject = SelectedEmitter;
		}

		// calculate offset from object to property being edited
		DWORD Offset = 0;
		UObject* BaseObject = EditedObject;
		for (FEditPropertyChain::TIterator It(PropertyChain->GetHead()); It; ++It )
		{
			Offset += It->Offset;

			// don't go past the active property
			if (*It == ObjProp)
			{
				break;
			}

			// If it is an object property, then reset our base pointer/offset
			if (It->IsA(UObjectProperty::StaticClass()))
			{
				BaseObject = *(UObject**)((BYTE*)BaseObject + Offset);
				Offset = 0;
			}
		}

		BYTE* CurvePointer = (BYTE*)BaseObject + Offset;
		UObject** ObjPtrPtr = (UObject**)CurvePointer;
		UObject* NewCurve = *(ObjPtrPtr);
		
		if (NewCurve)
		{
			PartSys->CurveEdSetup->ReplaceCurve(CurveToReplace, NewCurve);
			CurveEd->CurveChanged();
		}
	}

	if (SelectedModule || SelectedEmitter)
	{
		if (PropertyChain->GetActiveNode()->GetValue()->GetName() == TEXT("InterpolationMethod"))
		{
			UParticleModuleRequired* ReqMod = Cast<UParticleModuleRequired>(SelectedModule);
			if (ReqMod && SelectedEmitter)
			{
				if (ReqMod->InterpolationMethod != PreviousInterpolationMethod)
				{
					INT CurrentLODLevel = GetCurrentlySelectedLODLevelIndex();
					if (CurrentLODLevel == 0)
					{
						// The main on is being changed...
						// Check all other LOD levels...
						for (INT LODIndex = 1; LODIndex < SelectedEmitter->LODLevels.Num(); LODIndex++)
						{
							UParticleLODLevel* CheckLOD = SelectedEmitter->LODLevels(LODIndex);
							if (CheckLOD)
							{
								UParticleModuleRequired* CheckReq = CheckLOD->RequiredModule;
								if (CheckReq)
								{
									if (ReqMod->InterpolationMethod == PSUVIM_None)
									{
										CheckReq->InterpolationMethod = PSUVIM_None;
									}
									else
									{
										if (CheckReq->InterpolationMethod == PSUVIM_None)
										{
											CheckReq->InterpolationMethod = ReqMod->InterpolationMethod;
										}
									}
								}
							}
						}
					}
					else
					{
						// The main on is being changed...
						// Check all other LOD levels...
						UParticleLODLevel* CheckLOD = SelectedEmitter->LODLevels(0);
						if (CheckLOD)
						{
							UBOOL bWarn = FALSE;
							UParticleModuleRequired* CheckReq = CheckLOD->RequiredModule;
							if (CheckReq)
							{
								if (ReqMod->InterpolationMethod == PSUVIM_None)
								{
									if (CheckReq->InterpolationMethod != PSUVIM_None)
									{
										ReqMod->InterpolationMethod = PreviousInterpolationMethod;
										bWarn = TRUE;
									}
								}
								else
								{
									if (CheckReq->InterpolationMethod == PSUVIM_None)
									{
										ReqMod->InterpolationMethod = PreviousInterpolationMethod;
										bWarn = TRUE;
									}
								}
							}

							if (bWarn == TRUE)
							{
								appMsgf(AMT_OK, *(LocalizeUnrealEd("Cascade_InterpolationMethodLODWarning")));
								PropertyWindow->Rebuild();
							}
						}
					}
				}
			}
		}

		FPropertyChangedEvent PropertyEvent(PropertyChain->GetActiveNode()->GetValue());
		PartSys->PostEditChangeProperty(PropertyEvent);

		if (SelectedModule)
		{
			if (SelectedModule->IsDisplayedInCurveEd(CurveEd->EdSetup))
			{
				TArray<FParticleCurvePair> Curves;
				SelectedModule->GetCurveObjects(Curves);

				for (INT i=0; i<Curves.Num(); i++)
				{
					CurveEd->EdSetup->ChangeCurveColor(Curves(i).CurveObject, SelectedModule->ModuleEditorColor);
				}
			}
		}
	}

	PartSys->ThumbnailImageOutOfDate = TRUE;

	check(TransactionInProgress());
	EndTransaction(TEXT("CascadePropertyChange"));

	CurveEd->CurveChanged();
	EmitterEdVC->Viewport->Invalidate();

	wxCommandEvent DummyEvent;
	OnResetInLevel(DummyEvent);
}

void WxCascade::NotifyExec( void* Src, const TCHAR* Cmd )
{
	GUnrealEd->NotifyExec(Src, Cmd);
}

///////////////////////////////////////////////////////////////////////////////////////
// Utils
void WxCascade::CreateNewModule(INT ModClassIndex)
{
	if (SelectedEmitter == NULL)
	{
		return;
	}

	INT CurrLODLevel = GetCurrentlySelectedLODLevelIndex();
	if (CurrLODLevel != 0)
	{
		// Don't allow creating modules if not at highest LOD
		appMsgf(AMT_OK, *(LocalizeUnrealEd(TEXT("CascadeLODAddError"))));
		return;
	}

	UClass* NewModClass = ParticleModuleClasses(ModClassIndex);
	check(NewModClass->IsChildOf(UParticleModule::StaticClass()));

	UBOOL bIsEventGenerator = FALSE;

	if (NewModClass->IsChildOf(UParticleModuleTypeDataBase::StaticClass()))
	{
		// Make sure there isn't already a TypeData module applied!
		UParticleLODLevel* LODLevel = SelectedEmitter->GetLODLevel(0);
		if (LODLevel->TypeDataModule != 0)
		{
			appMsgf(AMT_OK, *LocalizeUnrealEd("Error_TypeDataModuleAlreadyPresent"));
			return;
		}
	}
	else
	if (NewModClass == UParticleModuleEventGenerator::StaticClass())
	{
		bIsEventGenerator = TRUE;
		// Make sure there isn't already an EventGenerator module applied!
		UParticleLODLevel* LODLevel = SelectedEmitter->GetLODLevel(0);
		if (LODLevel->EventGenerator != NULL)
		{
			appMsgf(AMT_OK, *LocalizeUnrealEd("Error_EventGeneratorModuleAlreadyPresent"));
			return;
		}
	}
	else
	if (NewModClass == UParticleModuleParameterDynamic::StaticClass())
	{
		// Make sure there isn't already an DynamicParameter module applied!
		UParticleLODLevel* LODLevel = SelectedEmitter->GetLODLevel(0);
		for (INT CheckMod = 0; CheckMod < LODLevel->Modules.Num(); CheckMod++)
		{
			UParticleModuleParameterDynamic* DynamicParamMod = Cast<UParticleModuleParameterDynamic>(LODLevel->Modules(CheckMod));
			if (DynamicParamMod)
			{
				appMsgf(AMT_OK, *LocalizeUnrealEd("Error_DynamicParameterModuleAlreadyPresent"));
				return;
			}
		}
	}

	BeginTransaction(TEXT("CreateNewModule"));
	ModifyParticleSystem();
	ModifySelectedObjects();

	PartSys->PreEditChange(NULL);
	PartSysComp->PreEditChange(NULL);

	// Construct it and add to selected emitter.
	UParticleModule* NewModule = ConstructObject<UParticleModule>(NewModClass, PartSys, NAME_None, RF_Transactional);
	NewModule->ModuleEditorColor = FColor::MakeRandomColor();
	NewModule->SetToSensibleDefaults(SelectedEmitter);
	NewModule->LODValidity = 1;

	UParticleLODLevel* LODLevel	= SelectedEmitter->GetLODLevel(0);
	if (bIsEventGenerator == TRUE)
	{
		LODLevel->Modules.InsertItem(NewModule, 0);
		LODLevel->EventGenerator = Cast<UParticleModuleEventGenerator>(NewModule);
	}
	else
	{
		LODLevel->Modules.AddItem(NewModule);
	}

	for (INT LODIndex = 1; LODIndex < SelectedEmitter->LODLevels.Num(); LODIndex++)
	{
		LODLevel = SelectedEmitter->GetLODLevel(LODIndex);
		NewModule->LODValidity |= (1 << LODIndex);
		if (bIsEventGenerator == TRUE)
		{
			LODLevel->Modules.InsertItem(NewModule, 0);
			LODLevel->EventGenerator = Cast<UParticleModuleEventGenerator>(NewModule);
		}
		else
		{
			LODLevel->Modules.AddItem(NewModule);
		}
	}

	SelectedEmitter->UpdateModuleLists();

	PartSysComp->PostEditChange();
	PartSys->PostEditChange();

	EndTransaction(TEXT("CreateNewModule"));

	PartSys->MarkPackageDirty();

	// Refresh viewport
	EmitterEdVC->Viewport->Invalidate();
}

void WxCascade::PasteCurrentModule()
{
	if (!SelectedEmitter)
	{
		appMsgf(AMT_OK, *LocalizeUnrealEd("Error_MustSelectEmitter"));
		return;
	}

	INT CurrLODIndex = GetCurrentlySelectedLODLevelIndex();
	if (CurrLODIndex != 0)
	{
		// Don't allow pasting modules if not at highest LOD
		appMsgf(AMT_OK, *(LocalizeUnrealEd(TEXT("CascadeLODPasteError"))));
		return;
	}

	check(CopyModule);

	UObject* pkDupObject = 
		GEditor->StaticDuplicateObject(CopyModule, CopyModule, PartSys, TEXT("None"));
	if (pkDupObject)
	{
		UParticleModule* Module	= Cast<UParticleModule>(pkDupObject);
		Module->ModuleEditorColor = FColor::MakeRandomColor();
		UParticleLODLevel* LODLevel	= SelectedEmitter->GetLODLevel(0);
		InsertModule(Module, SelectedEmitter, LODLevel->Modules.Num());
	}
}

void WxCascade::CopyModuleToEmitter(UParticleModule* pkSourceModule, UParticleEmitter* pkTargetEmitter, UParticleSystem* pkTargetSystem, INT TargetIndex)
{
    check(pkSourceModule);
    check(pkTargetEmitter);
	check(pkTargetSystem);

	INT CurrLODIndex = GetCurrentlySelectedLODLevelIndex();
	if (CurrLODIndex != 0)
	{
		// Don't allow copying modules if not at highest LOD
		appMsgf(AMT_OK, *(LocalizeUnrealEd(TEXT("CascadeLODCopyError"))));
		return;
	}

	UObject* DupObject = GEditor->StaticDuplicateObject(pkSourceModule, pkSourceModule, pkTargetSystem, TEXT("None"));
	if (DupObject)
	{
		UParticleModule* Module	= Cast<UParticleModule>(DupObject);
		Module->ModuleEditorColor = FColor::MakeRandomColor();

		UParticleLODLevel* LODLevel;

		if (EmitterEdVC->DraggedModule == pkSourceModule)
		{
			EmitterEdVC->DraggedModules(0) = Module;
			// If we are dragging, we need to copy all the LOD modules
			for (INT LODIndex = 1; LODIndex < pkTargetEmitter->LODLevels.Num(); LODIndex++)
			{
				LODLevel	= pkTargetEmitter->GetLODLevel(LODIndex);

				UParticleModule* CopySource = EmitterEdVC->DraggedModules(LODIndex);
				if (CopySource)
				{
					DupObject = GEditor->StaticDuplicateObject(CopySource, CopySource, pkTargetSystem, TEXT("None"));
					if (DupObject)
					{
						UParticleModule* NewModule	= Cast<UParticleModule>(DupObject);
						NewModule->ModuleEditorColor = Module->ModuleEditorColor;
						EmitterEdVC->DraggedModules(LODIndex) = NewModule;
					}
				}
				else
				{
					warnf(TEXT("Missing dragged module!"));
				}
			}
		}

		LODLevel	= pkTargetEmitter->GetLODLevel(0);
		InsertModule(Module, pkTargetEmitter, (TargetIndex != INDEX_NONE) ? TargetIndex : LODLevel->Modules.Num(), FALSE);
	}
}

void WxCascade::SetSelectedEmitter( UParticleEmitter* NewSelectedEmitter )
{
	SetSelectedModule(NewSelectedEmitter, NULL);
}

void WxCascade::SetSelectedModule( UParticleEmitter* NewSelectedEmitter, UParticleModule* NewSelectedModule )
{
#if WITH_APEX_EDITOR
	if(ApexPropertyWindow && SelectedModule != NewSelectedModule)
	{
		ApexPropertyWindow->ClearProperties();
		UParticleModuleTypeDataApex *ApexTypeDataModule = Cast<UParticleModuleTypeDataApex>(NewSelectedModule);
		if(ApexTypeDataModule && ApexTypeDataModule->ApexIOFX && ApexTypeDataModule->ApexEmitter)
		{
			ApexTypeDataModule->ConditionalInitialize();
			ApexPropertyWindow->AddObject(*ApexTypeDataModule->ApexIOFX);
			ApexPropertyWindow->AddObject(*ApexTypeDataModule->ApexEmitter);
		}
	}
#endif
	
	SelectedEmitter = NewSelectedEmitter;

	INT CurrLODIndex = GetCurrentlySelectedLODLevelIndex();
	if (CurrLODIndex < 0)
	{
		return;
	}

	UParticleLODLevel* LODLevel = NULL;
	// Make sure it's the correct LOD level...
	if (SelectedEmitter)
	{
		LODLevel = SelectedEmitter->GetLODLevel(CurrLODIndex);
		if (NewSelectedModule)
		{
			INT	ModuleIndex	= INDEX_NONE;
			for (INT LODLevelCheck = 0; LODLevelCheck < SelectedEmitter->LODLevels.Num(); LODLevelCheck++)
			{
				UParticleLODLevel* CheckLODLevel	= SelectedEmitter->LODLevels(LODLevelCheck);
				if (LODLevel)
				{
					// Check the type data...
					if (CheckLODLevel->TypeDataModule &&
						(CheckLODLevel->TypeDataModule == NewSelectedModule))
					{
						ModuleIndex = INDEX_TYPEDATAMODULE;
					}

					// Check the required module...
					if (ModuleIndex == INDEX_NONE)
					{
						if (CheckLODLevel->RequiredModule == NewSelectedModule)
						{
							ModuleIndex = INDEX_REQUIREDMODULE;
						}
					}

					// Check the spawn...
					if (ModuleIndex == INDEX_NONE)
					{
						if (CheckLODLevel->SpawnModule == NewSelectedModule)
						{
							ModuleIndex = INDEX_SPAWNMODULE;
						}
					}

					// Check the rest...
					if (ModuleIndex == INDEX_NONE)
					{
						for (INT ModuleCheck = 0; ModuleCheck < CheckLODLevel->Modules.Num(); ModuleCheck++)
						{
							if (CheckLODLevel->Modules(ModuleCheck) == NewSelectedModule)
							{
								ModuleIndex = ModuleCheck;
								break;
							}
						}
					}
				}

				if (ModuleIndex != INDEX_NONE)
				{
					break;
				}
			}

			switch (ModuleIndex)
			{
			case INDEX_NONE:
				break;
			case INDEX_TYPEDATAMODULE:
				NewSelectedModule = LODLevel->TypeDataModule;
				break;
			case INDEX_REQUIREDMODULE:
				NewSelectedModule = LODLevel->RequiredModule;
				break;
			case INDEX_SPAWNMODULE:
				NewSelectedModule = LODLevel->SpawnModule;
				break;
			default:
				NewSelectedModule = LODLevel->Modules(ModuleIndex);
				break;
			}
			SelectedModuleIndex	= ModuleIndex;
		}
	}

	SelectedModule = NewSelectedModule;

	UBOOL bReadOnly = FALSE;
	UObject* PropObj = PartSys;
	if (SelectedEmitter)
	{
		if (SelectedModule)
		{
			if (LODLevel != NULL)
			{
				if (bReadOnly == FALSE)
				{
					if (LODLevel->Level != CurrLODIndex)
					{
						bReadOnly = TRUE;
					}
					else
					{
						bReadOnly = !LODLevel->IsModuleEditable(SelectedModule);
					}
				}
			}
			PropObj = SelectedModule;
		}
		else
		{
			if (bReadOnly == FALSE)
			{
				// Only allowing editing the SelectedEmitter 
				// properties when at the highest LOD level.
				if (!(LODLevel && (LODLevel->Level == 0)))
				{
					bReadOnly = TRUE;
				}
			}
			PropObj = SelectedEmitter;
		}

		// If soloing and NOT an emitter that is soloing, mark it Read-only!
		if (bIsSoloing == TRUE)
		{
			if (SelectedEmitter->bIsSoloing == FALSE)
			{
				bReadOnly = TRUE;
			}
		}
	}
	PropertyWindow->SetObject(PropObj, EPropertyWindowFlags::ShouldShowCategories | EPropertyWindowFlags::Sorted | (bReadOnly ? EPropertyWindowFlags::ReadOnly : 0));

	SetSelectedInCurveEditor();
	EmitterEdVC->Viewport->Invalidate();
}

// Insert the selected module into the target emitter at the desired location.
UBOOL WxCascade::InsertModule(UParticleModule* Module, UParticleEmitter* TargetEmitter, INT TargetIndex, UBOOL bSetSelected)
{
	if (!Module || !TargetEmitter || TargetIndex == INDEX_NONE)
	{
		return FALSE;
	}

	INT CurrLODIndex = GetCurrentlySelectedLODLevelIndex();
	if (CurrLODIndex != 0)
	{
		// Don't allow moving modules if not at highest LOD
		warnf(*(LocalizeUnrealEd(TEXT("CascadeLODMoveError"))));
		return FALSE;
	}

	// Cannot insert the same module more than once into the same emitter.
	UParticleLODLevel* LODLevel	= TargetEmitter->GetLODLevel(0);
	for(INT i = 0; i < LODLevel->Modules.Num(); i++)
	{
		if (LODLevel->Modules(i) == Module)
		{
			appMsgf(AMT_OK, *LocalizeUnrealEd("Error_ModuleCanOnlyBeUsedInEmitterOnce"));
			return FALSE;
		}
	}

	if (Module->IsA(UParticleModuleParameterDynamic::StaticClass()))
	{
		// Make sure there isn't already an DynamicParameter module applied!
		UParticleLODLevel* LODLevel = TargetEmitter->GetLODLevel(0);
		for (INT CheckMod = 0; CheckMod < LODLevel->Modules.Num(); CheckMod++)
		{
			UParticleModuleParameterDynamic* DynamicParamMod = Cast<UParticleModuleParameterDynamic>(LODLevel->Modules(CheckMod));
			if (DynamicParamMod)
			{
				appMsgf(AMT_OK, *LocalizeUnrealEd("Error_DynamicParameterModuleAlreadyPresent"));
				return FALSE;
			}
		}
	}

	// If the Spawn or Required modules are being 're-inserted', do nothing!
	if ((LODLevel->SpawnModule == Module) ||
		(LODLevel->RequiredModule == Module))
	{
		return FALSE;
	}

	BeginTransaction(TEXT("InsertModule"));
	ModifyEmitter(TargetEmitter);
	ModifyParticleSystem();

	// Insert in desired location in new Emitter
	PartSys->PreEditChange(NULL);

	if (Module->IsA(UParticleModuleTypeDataBase::StaticClass()))
	{
		UBOOL bInsert = TRUE;
		if (LODLevel->TypeDataModule != NULL)
		{
			// Prompt to ensure they want to replace the TDModule
			bInsert = appMsgf(AMT_YesNo, *LocalizeUnrealEd("Cascade_ReplaceTypeDataModule"));
		}

		if (bInsert == TRUE)
		{
			LODLevel->TypeDataModule = Module;

			if (EmitterEdVC->DraggedModules.Num() > 0)
			{
				// Swap the modules in all the LOD levels
				for (INT LODIndex = 1; LODIndex < TargetEmitter->LODLevels.Num(); LODIndex++)
				{
					UParticleLODLevel*	LODLevel	= TargetEmitter->GetLODLevel(LODIndex);
					UParticleModule*	Module		= EmitterEdVC->DraggedModules(LODIndex);

					LODLevel->TypeDataModule	= Module;
				}
			}
		}
	}
	else if (Module->IsA(UParticleModuleSpawn::StaticClass()))
	{
		// There can be only one...
		LODLevel->SpawnModule = CastChecked<UParticleModuleSpawn>(Module);
		if (EmitterEdVC->DraggedModules.Num() > 0)
		{
			// Swap the modules in all the LOD levels
			for (INT LODIndex = 1; LODIndex < TargetEmitter->LODLevels.Num(); LODIndex++)
			{
				UParticleLODLevel* LODLevel	= TargetEmitter->GetLODLevel(LODIndex);
				UParticleModuleSpawn* Module = CastChecked<UParticleModuleSpawn>(EmitterEdVC->DraggedModules(LODIndex));
				LODLevel->SpawnModule = Module;
			}
		}
	}
	else if (Module->IsA(UParticleModuleRequired::StaticClass()))
	{
		// There can be only one...
		LODLevel->RequiredModule = CastChecked<UParticleModuleRequired>(Module);
		if (EmitterEdVC->DraggedModules.Num() > 0)
		{
			// Swap the modules in all the LOD levels
			for (INT LODIndex = 1; LODIndex < TargetEmitter->LODLevels.Num(); LODIndex++)
			{
				UParticleLODLevel* LODLevel	= TargetEmitter->GetLODLevel(LODIndex);
				UParticleModuleRequired* Module = CastChecked<UParticleModuleRequired>(EmitterEdVC->DraggedModules(LODIndex));
				LODLevel->RequiredModule = Module;
			}
		}
	}
	else
	{
		INT NewModuleIndex = Clamp<INT>(TargetIndex, 0, LODLevel->Modules.Num());
		LODLevel->Modules.Insert(NewModuleIndex);
		LODLevel->Modules(NewModuleIndex) = Module;

		if (EmitterEdVC->DraggedModules.Num() > 0)
		{
			// Swap the modules in all the LOD levels
			for (INT LODIndex = 1; LODIndex < TargetEmitter->LODLevels.Num(); LODIndex++)
			{
				UParticleLODLevel*	LODLevel	= TargetEmitter->GetLODLevel(LODIndex);
				UParticleModule*	Module		= EmitterEdVC->DraggedModules(LODIndex);

				LODLevel->Modules.Insert(NewModuleIndex);
				LODLevel->Modules(NewModuleIndex)	= Module;
			}
		}
	}
	EmitterEdVC->DraggedModules.Empty();

	TargetEmitter->UpdateModuleLists();

	PartSys->PostEditChange();

	// Update selection
    if (bSetSelected)
    {
        SetSelectedModule(TargetEmitter, Module);
    }

	EndTransaction(TEXT("InsertModule"));

	PartSys->MarkPackageDirty();
	EmitterEdVC->Viewport->Invalidate();

	return TRUE;
}

// Delete entire Emitter from System
// Garbage collection will clear up any unused modules.
void WxCascade::DeleteSelectedEmitter()
{
	if (!SelectedEmitter)
		return;

	check(PartSys->Emitters.ContainsItem(SelectedEmitter));

	INT	CurrLODSetting	= GetCurrentlySelectedLODLevelIndex();
	if (SelectedEmitter->IsLODLevelValid(CurrLODSetting) == FALSE)
	{
		return;
	}

	if (SelectedEmitter->bCollapsed == TRUE)
	{
		appMsgf(AMT_OK, *LocalizeUnrealEd("EmitterDeleteCollapsed"));
		return;
	}

	if (bIsSoloing == TRUE)
	{
		FString Description = LocalizeUnrealEd(TEXT("DeleteEmitter"));
		if (PromptForCancellingSoloingMode(Description) == FALSE)
		{
			return;
		}
	}

	// If there are differences in the enabled states of the LOD levels for an emitter,
	// prompt the user to ensure they want to delete it...
	UParticleLODLevel* LODLevel = SelectedEmitter->LODLevels(0);
	UBOOL bEnabledStateDifferent = FALSE;
	UBOOL bEnabled = LODLevel->bEnabled;
	for (INT LODIndex = 1; (LODIndex < SelectedEmitter->LODLevels.Num()) && !bEnabledStateDifferent; LODIndex++)
	{
		LODLevel = SelectedEmitter->LODLevels(LODIndex);
		if (bEnabled != LODLevel->bEnabled)
		{
			bEnabledStateDifferent = TRUE;
		}
		else
		{
			if (LODLevel->IsModuleEditable(LODLevel->RequiredModule))
			{
				bEnabledStateDifferent = TRUE;
			}
			if (LODLevel->IsModuleEditable(LODLevel->SpawnModule))
			{
				bEnabledStateDifferent = TRUE;
			}
			if (LODLevel->TypeDataModule && LODLevel->IsModuleEditable(LODLevel->TypeDataModule))
			{
				bEnabledStateDifferent = TRUE;
			}

			for (INT CheckModIndex = 0; CheckModIndex < LODLevel->Modules.Num(); CheckModIndex++)
			{
				if (LODLevel->IsModuleEditable(LODLevel->Modules(CheckModIndex)))
				{
					bEnabledStateDifferent = TRUE;
				}
			}
		}
	}

	if (bEnabledStateDifferent == TRUE)
	{
		if (appMsgf(AMT_YesNo, *LocalizeUnrealEd("EmitterDeleteConfirm")) == FALSE)
		{
			return;
		}
	}

	BeginTransaction(TEXT("DeleteSelectedEmitter"));
	ModifyParticleSystem();

	PartSys->PreEditChange(NULL);

	SelectedEmitter->RemoveEmitterCurvesFromEditor(CurveEd->EdSetup);
	CurveEd->CurveChanged();

	PartSys->Emitters.RemoveItem(SelectedEmitter);

	PartSys->PostEditChange();

	SetSelectedEmitter(NULL);

	PartSys->SetupSoloing();

	EndTransaction(TEXT("DeleteSelectedEmitter"));

	PartSys->MarkPackageDirty();
	EmitterEdVC->Viewport->Invalidate();
}

// Move the selected amitter by MoveAmount in the array of Emitters.
void WxCascade::MoveSelectedEmitter(INT MoveAmount)
{
	if (!SelectedEmitter)
		return;

	if (bIsSoloing == TRUE)
	{
		FString Description = LocalizeUnrealEd(TEXT("MoveEmitter"));
		if (PromptForCancellingSoloingMode(Description) == FALSE)
		{
			return;
		}
	}

	BeginTransaction(TEXT("MoveSelectedEmitter"));
	ModifyParticleSystem();

	INT CurrentEmitterIndex = PartSys->Emitters.FindItemIndex(SelectedEmitter);
	check(CurrentEmitterIndex != INDEX_NONE);

	INT NewEmitterIndex = Clamp<INT>(CurrentEmitterIndex + MoveAmount, 0, PartSys->Emitters.Num() - 1);

	if (NewEmitterIndex != CurrentEmitterIndex)
	{
		PartSys->PreEditChange(NULL);

		PartSys->Emitters.RemoveItem(SelectedEmitter);
		PartSys->Emitters.InsertZeroed(NewEmitterIndex);
		PartSys->Emitters(NewEmitterIndex) = SelectedEmitter;

		PartSys->PostEditChange();

		PartSys->SetupSoloing();

		EmitterEdVC->Viewport->Invalidate();
	}

	EndTransaction(TEXT("MoveSelectedEmitter"));

	PartSys->MarkPackageDirty();
}

/**
 *	Toggle the enabled setting on the selected emitter
 */
void WxCascade::ToggleEnableOnSelectedEmitter(UParticleEmitter* InEmitter)
{
	if (!InEmitter)
	{
		return;
	}

	if (bIsSoloing == TRUE)
	{
		FString Description = LocalizeUnrealEd(TEXT("ToggleEnableEmitter"));
		if (PromptForCancellingSoloingMode(Description) == FALSE)
		{
			return;
		}

		// Make them toggle again in this case as the setting may/maynot be what they think it is...
		PartSys->SetupSoloing();
		return;
	}

	UParticleLODLevel* LODLevel = GetCurrentlySelectedLODLevel(InEmitter);
	if (LODLevel)
	{
		BeginTransaction(TEXT("ToggleEnableOnSelectedEmitter"));
		
		ModifyParticleSystem();
		PartSys->PreEditChange(NULL);

		LODLevel->bEnabled	= !LODLevel->bEnabled;
		LODLevel->RequiredModule->bEnabled = LODLevel->bEnabled;

		PartSys->PostEditChange();
		PartSys->SetupSoloing();

		wxCommandEvent DummyEvent;
		OnResetInLevel(DummyEvent);

		EmitterEdVC->Viewport->Invalidate();

		EndTransaction(TEXT("ToggleEnableOnSelectedEmitter"));
		PartSys->MarkPackageDirty();
	}
}

/** 
 *	Toggle the solo setting on the given emitter.
 *
 *	@param	InEmitter		The emitter to toggle the solo setting on.
 *	@param	bInSetSelected	If TRUE, set the emitter as selected.
 */
void WxCascade::ToggleSoloOnEmitter(UParticleEmitter* InEmitter, UBOOL bInSetSelected)
{
	if ((PartSys == NULL) || (InEmitter == NULL))
	{
		return;
	}
	bIsSoloing = PartSys->ToggleSoloing(InEmitter);
	if (bInSetSelected == TRUE)
	{
		SetSelectedEmitter(InEmitter);
	}
}

// Export the selected emitter for importing into another particle system
void WxCascade::ExportSelectedEmitter()
{
	if (!SelectedEmitter)
	{
		appMsgf(AMT_OK, *LocalizeUnrealEd("Error_NoEmitterSelectedForExport"));
		return;
	}

	for ( USelection::TObjectIterator Itor = GEditor->GetSelectedObjects()->ObjectItor() ; Itor ; ++Itor )
	{
		UParticleSystem* DestPartSys = Cast<UParticleSystem>(*Itor);
		if (DestPartSys && (DestPartSys != PartSys))
		{
			INT NewCount = 0;
			if (DestPartSys->Emitters.Num() > 0)
			{
				UParticleEmitter* DestEmitter0 = DestPartSys->Emitters(0);

				NewCount = DestEmitter0->LODLevels.Num() - SelectedEmitter->LODLevels.Num();
				if (NewCount > 0)
				{
					// There are more LODs in the destination than the source... Add enough to cover.
					INT StartIndex = SelectedEmitter->LODLevels.Num();
					for (INT InsertIndex = 0; InsertIndex < NewCount; InsertIndex++)
					{
						SelectedEmitter->CreateLODLevel(StartIndex + InsertIndex, TRUE);
					}
					SelectedEmitter->UpdateModuleLists();
				}
				else
				if (NewCount < 0)
				{
					INT InsertCount = -NewCount;
					// There are fewer LODs in the destination than the source... Add enough to cover.
					INT StartIndex = DestEmitter0->LODLevels.Num();
					for (INT EmitterIndex = 0; EmitterIndex < DestPartSys->Emitters.Num(); EmitterIndex++)
					{
						UParticleEmitter* DestEmitter = DestPartSys->Emitters(EmitterIndex);
						if (DestEmitter)
						{
							for (INT InsertIndex = 0; InsertIndex < InsertCount; InsertIndex++)
							{
								DestEmitter->CreateLODLevel(StartIndex + InsertIndex, FALSE);
							}
							DestEmitter->UpdateModuleLists();
						}
					}

					// Add the slots in the LODDistances array
					DestPartSys->LODDistances.AddZeroed(InsertCount);
					for (INT DistIndex = StartIndex; DistIndex < DestPartSys->LODDistances.Num(); DistIndex++)
					{
						DestPartSys->LODDistances(DistIndex) = DistIndex * 2500.0f;
					}
					DestPartSys->LODSettings.AddZeroed(InsertCount);
					for (INT DistIndex = StartIndex; DistIndex < DestPartSys->LODSettings.Num(); DistIndex++)
					{
						DestPartSys->LODSettings(DistIndex) = FParticleSystemLOD::CreateParticleSystemLOD();
					}
				}
			}
			else
			{
				INT InsertCount = SelectedEmitter->LODLevels.Num();
				// Add the slots in the LODDistances array
				DestPartSys->LODDistances.AddZeroed(InsertCount);
				for (INT DistIndex = 0; DistIndex < InsertCount; DistIndex++)
				{
					DestPartSys->LODDistances(DistIndex) = DistIndex * 2500.0f;
				}
				DestPartSys->LODSettings.AddZeroed(InsertCount);
				for (INT DistIndex = 0; DistIndex < InsertCount; DistIndex++)
				{
					DestPartSys->LODSettings(DistIndex) = FParticleSystemLOD::CreateParticleSystemLOD();
				}
			}

			if (!DuplicateEmitter(SelectedEmitter, DestPartSys))
			{
				appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_FailedToCopy"), 
					*SelectedEmitter->GetEmitterName().ToString(),
					*DestPartSys->GetName()));
			}

			DestPartSys->MarkPackageDirty();

			// If we temporarily inserted LOD levels into the selected emitter,
			// we need to remove them now...
			if (NewCount > 0)
			{
				INT CurrCount = SelectedEmitter->LODLevels.Num();
				for (INT RemoveIndex = CurrCount - 1; RemoveIndex >= (CurrCount - NewCount); RemoveIndex--)
				{
					SelectedEmitter->LODLevels.Remove(RemoveIndex);
				}
				SelectedEmitter->UpdateModuleLists();
			}

			// Find instances of this particle system and reset them...
			for (TObjectIterator<UParticleSystemComponent> It;It;++It)
			{
				if (It->Template == DestPartSys)
				{
					UParticleSystemComponent* PSysComp = *It;

					// Force a recache of the view relevance
					PSysComp->bIsViewRelevanceDirty = TRUE;
					UBOOL bIsActive = It->bIsActive;
					It->DeactivateSystem();
					It->ResetParticles();
					if (bIsActive)
					{
						It->ActivateSystem();
					}
					It->BeginDeferredReattach();
				}
			}

			//find and refresh the open cascade window if it contains our destination psys
			{
				check(GApp->EditorFrame);
				const wxWindowList& ChildWindows = GApp->EditorFrame->GetChildren();
				wxWindowList::compatibility_iterator Node = ChildWindows.GetFirst();
				while (Node)
				{
					wxWindow* Child = (wxWindow*)Node->GetData();
					if (Child->IsKindOf(CLASSINFO(wxFrame)))
					{
						if (appStricmp(Child->GetName(), *(DestPartSys->GetPathName())) == 0)
						{
							//Refresh the (destation) cascade window
							WxCascade* CascadeHandle = (WxCascade*)Child;
							CascadeHandle->CascadeTouch();
						}
					}
					Node = Node->GetNext();
				}
			}
		}
	}
}

void WxCascade::ExportAllEmitters()
{
	if ((PartSys == NULL) || (PartSys->Emitters.Num() <= 0))
	{
		// Can't export empty PSys!
		return;
	}

	UParticleEmitter* SaveSelectedEmitter = SelectedEmitter;
	// There are more LODs in the destination than the source... Add enough to cover.
	for (INT SrcIndex = 0; SrcIndex < PartSys->Emitters.Num(); SrcIndex++)
	{
		UParticleEmitter* SrcEmitter = PartSys->Emitters(SrcIndex);
		if (SrcEmitter)
		{
			UBOOL bSkipIt = TRUE;
			for (INT LODIndex = 0; LODIndex < SrcEmitter->LODLevels.Num(); LODIndex++)
			{
				UParticleLODLevel* LODLevel = SrcEmitter->LODLevels(LODIndex);
				if (LODLevel && LODLevel->bEnabled)
				{
					bSkipIt = FALSE;
					break;
				}
			}

			if (!bSkipIt)
			{
				SelectedEmitter = SrcEmitter;
				ExportSelectedEmitter();
			}
		}
	}
	SelectedEmitter = SaveSelectedEmitter;
}

// Delete selected module from selected emitter.
// Module may be used in other Emitters, so we don't destroy it or anything - garbage collection will handle that.
void WxCascade::DeleteSelectedModule(UBOOL bConfirm)
{
	if (!SelectedModule || !SelectedEmitter)
	{
		return;
	}

	if (SelectedEmitter->bCollapsed == TRUE)
	{
		// Should never get in here!
		return;
	}

	if (SelectedModuleIndex == INDEX_NONE)
	{
		return;
	}

	if ((SelectedModuleIndex == INDEX_REQUIREDMODULE) || 
		(SelectedModuleIndex == INDEX_SPAWNMODULE))
	{
		appMsgf(AMT_OK, *LocalizeUnrealEd(TEXT("Cascade_NoDeleteRequiredOrSpawn")));
		return;
	}

	INT	CurrLODSetting	= GetCurrentlySelectedLODLevelIndex();
	if (CurrLODSetting != 0)
	{
		// Don't allow deleting modules if not at highest LOD
		appMsgf(AMT_OK, *LocalizeUnrealEd("Cascade_ModuleDeleteLODWarning"));
		return;
	}

	// If there are differences in the enabled states of the LOD levels for an emitter,
	// prompt the user to ensure they want to delete it...
	UParticleLODLevel* LODLevel = SelectedEmitter->LODLevels(0);
	UParticleModule* CheckModule;
	UBOOL bEnabledStateDifferent = FALSE;
	UBOOL bEnabled = SelectedModule->bEnabled;
	for (INT LODIndex = 1; (LODIndex < SelectedEmitter->LODLevels.Num()) && !bEnabledStateDifferent; LODIndex++)
	{
		LODLevel = SelectedEmitter->LODLevels(LODIndex);
		switch (SelectedModuleIndex)
		{
		case INDEX_TYPEDATAMODULE:
			CheckModule = LODLevel->TypeDataModule;
			break;
		default:
			CheckModule = LODLevel->Modules(SelectedModuleIndex);
			break;
		}

		check(CheckModule);

		if (LODLevel->IsModuleEditable(CheckModule))
		{
			bEnabledStateDifferent = TRUE;
		}
	}

	if ((bConfirm == TRUE) && (bEnabledStateDifferent == TRUE))
	{
		if (appMsgf(AMT_YesNo, *LocalizeUnrealEd("ModuleDeleteConfirm")) == FALSE)
		{
			return;
		}
	}

	BeginTransaction(TEXT("DeleteSelectedModule"));
	ModifySelectedObjects();
	ModifyParticleSystem();

	PartSys->PreEditChange(NULL);

	// Find the module index...
	INT	DeleteModuleIndex	= -1;

	UParticleLODLevel* HighLODLevel	= SelectedEmitter->GetLODLevel(0);
	check(HighLODLevel);
	for (INT ModuleIndex = 0; ModuleIndex < HighLODLevel->Modules.Num(); ModuleIndex++)
	{
		UParticleModule* CheckModule = HighLODLevel->Modules(ModuleIndex);
		if (CheckModule == SelectedModule)
		{
			DeleteModuleIndex = ModuleIndex;
			break;
		}
	}

	if (SelectedModule->IsDisplayedInCurveEd(CurveEd->EdSetup) && !ModuleIsShared(SelectedModule))
	{
		// Remove it from the curve editor!
		SelectedModule->RemoveModuleCurvesFromEditor(CurveEd->EdSetup);
		CurveEd->CurveChanged();
	}

	// Check all the others...
	for (INT LODIndex = 1; LODIndex < SelectedEmitter->LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* LODLevel	= SelectedEmitter->GetLODLevel(LODIndex);
		if (LODLevel)
		{
			UParticleModule* Module;

			if (DeleteModuleIndex >= 0)
			{
				Module = LODLevel->Modules(DeleteModuleIndex);
			}
			else
			{
				Module	= LODLevel->TypeDataModule;
			}

			if (Module)
			{
				Module->RemoveModuleCurvesFromEditor(CurveEd->EdSetup);
				CurveEd->CurveChanged();
			}
		}
			
	}
	CurveEd->CurveEdVC->Viewport->Invalidate();

	if (SelectedEmitter)
	{
		UBOOL bNeedsListUpdated = FALSE;

		for (INT LODIndex = 0; LODIndex < SelectedEmitter->LODLevels.Num(); LODIndex++)
		{
			UParticleLODLevel* LODLevel	= SelectedEmitter->GetLODLevel(LODIndex);

			// See if it is in this LODs level...
			UParticleModule* CheckModule;

			if (DeleteModuleIndex >= 0)
			{
				CheckModule = LODLevel->Modules(DeleteModuleIndex);
			}
			else
			{
				CheckModule = LODLevel->TypeDataModule;
			}

			if (CheckModule)
			{
				if (CheckModule->IsA(UParticleModuleTypeDataBase::StaticClass()))
				{
					check(LODLevel->TypeDataModule == CheckModule);
					LODLevel->TypeDataModule = NULL;
				}
				else
				if (CheckModule->IsA(UParticleModuleEventGenerator::StaticClass()))
				{
					LODLevel->EventGenerator = NULL;
				}
				LODLevel->Modules.RemoveItem(CheckModule);
				bNeedsListUpdated = TRUE;
			}
		}

		if (bNeedsListUpdated)
		{
			SelectedEmitter->UpdateModuleLists();
		}
	}
	else
	{
		// Assume that it's in the module dump...
		ModuleDumpList.RemoveItem(SelectedModule);
	}

	PartSys->PostEditChange();

	EndTransaction(TEXT("DeleteSelectedModule"));

	SetSelectedEmitter(SelectedEmitter);

	EmitterEdVC->Viewport->Invalidate();

	PartSys->MarkPackageDirty();
}

void WxCascade::EnableSelectedModule()
{
	if (!SelectedModule && !SelectedEmitter)
	{
		return;
	}

	INT	CurrLODSetting	= GetCurrentlySelectedLODLevelIndex();
	if (SelectedEmitter->IsLODLevelValid(CurrLODSetting) == FALSE)
	{
		return;
	}

	UParticleLODLevel* DestLODLevel = SelectedEmitter->GetLODLevel(CurrLODSetting);
	if (DestLODLevel->Level == 0)
	{
		// High LOD modules are ALWAYS enabled.
		return;
	}

	UParticleLODLevel* SourceLODLevel = SelectedEmitter->GetLODLevel(DestLODLevel->Level - 1);
	check(SourceLODLevel);

	BeginTransaction(TEXT("EnableSelectedModule"));
	ModifySelectedObjects();
	ModifyParticleSystem();

	PartSys->PreEditChange(NULL);

	if (SelectedModule)
	{
		// Store the index of the selected module...
		// Force copy the source module...
		UParticleModule* NewModule = SelectedModule->GenerateLODModule(SourceLODLevel, DestLODLevel, 100.0f, FALSE, TRUE);
		check(NewModule);

		// Turn off the LOD validity in the original module...
		SelectedModule->LODValidity &= ~(1 << DestLODLevel->Level);

		// Store the new module
		switch (SelectedModuleIndex)
		{
		case INDEX_NONE:
			break;
		case INDEX_REQUIREDMODULE:
			DestLODLevel->RequiredModule = CastChecked<UParticleModuleRequired>(NewModule);
			break;
		case INDEX_SPAWNMODULE:
			DestLODLevel->SpawnModule = CastChecked<UParticleModuleSpawn>(NewModule);
			break;
		case INDEX_TYPEDATAMODULE:
			DestLODLevel->TypeDataModule = NewModule;
			break;
		default:
			DestLODLevel->Modules(SelectedModuleIndex) = NewModule;
			break;
		}

		SelectedModule = NewModule;
	}

	PartSys->PostEditChange();

	EndTransaction(TEXT("EnableSelectedModule"));

	SetSelectedModule(SelectedEmitter, SelectedModule);

	EmitterEdVC->Viewport->Invalidate();

	PartSys->MarkPackageDirty();
}

void WxCascade::ResetSelectedModule()
{
}

void WxCascade::RefreshSelectedModule()
{
	if (PartSys && SelectedModule && SelectedEmitter)
	{
		SelectedModule->RefreshModule(PartSys->CurveEdSetup, SelectedEmitter, GetCurrentlySelectedLODLevelIndex());
		PropertyWindow->Rebuild();
	}
}

void WxCascade::RefreshAllModules()
{
}

/**
 *	Set the module to an exact duplicate (copy) of the same module in the highest LOD level.
 */
void WxCascade::DupHighestSelectedModule()
{
	if (!SelectedModule && !SelectedEmitter)
	{
		return;
	}

	INT	CurrLODSetting	= GetCurrentlySelectedLODLevelIndex();
	if (SelectedEmitter->IsLODLevelValid(CurrLODSetting) == FALSE)
	{
		return;
	}

	if (CurrLODSetting == 0)
	{
		// High LOD modules don't allow this.
		return;
	}

	UParticleLODLevel* SourceLODLevel = SelectedEmitter->GetLODLevel(0);
	check(SourceLODLevel);
	UParticleModule* HighestModule = SourceLODLevel->GetModuleAtIndex(SelectedModuleIndex);
	if (HighestModule == NULL)
	{
		// Couldn't find the highest module???
		return;
	}

	BeginTransaction(TEXT("DupHighestSelectedModule"));
	ModifySelectedObjects();
	ModifyParticleSystem();

	PartSys->PreEditChange(NULL);

	UBOOL bIsShared = ModuleIsShared(SelectedModule);
	UParticleModule* NewModule = NULL;
	UParticleLODLevel* DestLODLevel;

	// Store the index of the selected module...
	// Force copy the source module...
	DestLODLevel = SelectedEmitter->GetLODLevel(CurrLODSetting);
	NewModule = HighestModule->GenerateLODModule(SourceLODLevel, DestLODLevel, 100.0f, FALSE, TRUE);
	check(NewModule);

	for (INT LODIndex = CurrLODSetting; LODIndex < SelectedEmitter->LODLevels.Num(); LODIndex++)
	{
		DestLODLevel = SelectedEmitter->GetLODLevel(LODIndex);
		if (SelectedModule->IsUsedInLODLevel(LODIndex))
		{
			if (bIsShared == FALSE)
			{
				// Turn off the LOD validity in the original module... only if it wasn't shared!
				SelectedModule->LODValidity &= ~(1 << LODIndex);
			}
			// Turn on the LOD validity in the new module...
			NewModule->LODValidity |= (1 << LODIndex);

			// Store the new module
			switch (SelectedModuleIndex)
			{
			case INDEX_NONE:
				break;
			case INDEX_REQUIREDMODULE:
				DestLODLevel->RequiredModule = CastChecked<UParticleModuleRequired>(NewModule);
				break;
			case INDEX_SPAWNMODULE:
				DestLODLevel->SpawnModule = CastChecked<UParticleModuleSpawn>(NewModule);
				break;
			case INDEX_TYPEDATAMODULE:
				DestLODLevel->TypeDataModule = NewModule;
				break;
			default:
				DestLODLevel->Modules(SelectedModuleIndex) = NewModule;
				break;
			}
		}
	}

	SelectedModule = NewModule;
	if (SelectedEmitter)
	{
		SelectedEmitter->UpdateModuleLists();
	}

	PartSys->PostEditChange();

	EndTransaction(TEXT("DupHighestSelectedModule"));

	SetSelectedModule(SelectedEmitter, SelectedModule);
	CascadeTouch();

	EmitterEdVC->Viewport->Invalidate();

	PartSys->MarkPackageDirty();
}

/**
 *	Set the module to the same module in the next higher LOD level.
 */
void WxCascade::DupHigherSelectedModule()
{
	if (!SelectedModule && !SelectedEmitter)
	{
		return;
	}

	INT	CurrLODSetting	= GetCurrentlySelectedLODLevelIndex();
	if (SelectedEmitter->IsLODLevelValid(CurrLODSetting) == FALSE)
	{
		return;
	}

	if (CurrLODSetting == 0)
	{
		// High LOD modules don't allow this.
		return;
	}

	UParticleLODLevel* SourceLODLevel = SelectedEmitter->GetLODLevel(CurrLODSetting - 1);
	check(SourceLODLevel);
	UParticleModule* HighModule = SourceLODLevel->GetModuleAtIndex(SelectedModuleIndex);
	if (HighModule == NULL)
	{
		// Couldn't find the highest module???
		return;
	}

	BeginTransaction(TEXT("DupHighSelectedModule"));
	ModifySelectedObjects();
	ModifyParticleSystem();

	PartSys->PreEditChange(NULL);

	UParticleModule* NewModule = NULL;
	UParticleLODLevel* DestLODLevel;

	UBOOL bIsShared = ModuleIsShared(SelectedModule);
	// Store the index of the selected module...
	// Force copy the source module...
	DestLODLevel = SelectedEmitter->GetLODLevel(CurrLODSetting);
	NewModule = HighModule->GenerateLODModule(SourceLODLevel, DestLODLevel, 100.0f, FALSE, TRUE);
	check(NewModule);

	for (INT LODIndex = CurrLODSetting; LODIndex < SelectedEmitter->LODLevels.Num(); LODIndex++)
	{
		DestLODLevel = SelectedEmitter->GetLODLevel(LODIndex);
		if (SelectedModule->IsUsedInLODLevel(LODIndex))
		{
			if (bIsShared == FALSE)
			{
				// Turn off the LOD validity in the original module... only if it wasn't shared!
				SelectedModule->LODValidity &= ~(1 << LODIndex);
			}
			// Turn on the LOD validity int he new module...
			NewModule->LODValidity |= (1 << LODIndex);

			// Store the new module
			switch (SelectedModuleIndex)
			{
			case INDEX_NONE:
				break;
			case INDEX_REQUIREDMODULE:
				DestLODLevel->RequiredModule = CastChecked<UParticleModuleRequired>(NewModule);
				break;
			case INDEX_SPAWNMODULE:
				DestLODLevel->SpawnModule = CastChecked<UParticleModuleSpawn>(NewModule);
				break;
			case INDEX_TYPEDATAMODULE:
				DestLODLevel->TypeDataModule = NewModule;
				break;
			default:
				DestLODLevel->Modules(SelectedModuleIndex) = NewModule;
				break;
			}
		}
	}

	SelectedModule = NewModule;
	if (SelectedEmitter)
	{
		SelectedEmitter->UpdateModuleLists();
	}

	PartSys->PostEditChange();

	SetSelectedModule(SelectedEmitter, SelectedModule);

	EndTransaction(TEXT("DupHighSelectedModule"));
	CascadeTouch();

	PartSys->MarkPackageDirty();
	EmitterEdVC->Viewport->Invalidate();
}

void WxCascade::ShareHigherSelectedModule()
{
	if (!SelectedModule && !SelectedEmitter)
	{
		return;
	}

	INT	CurrLODSetting	= GetCurrentlySelectedLODLevelIndex();
	if (SelectedEmitter->IsLODLevelValid(CurrLODSetting) == FALSE)
	{
		return;
	}

	if (CurrLODSetting == 0)
	{
		// High LOD modules don't allow this.
		return;
	}

	UParticleLODLevel* SourceLODLevel = SelectedEmitter->GetLODLevel(CurrLODSetting - 1);
	check(SourceLODLevel);
	UParticleModule* HighModule = SourceLODLevel->GetModuleAtIndex(SelectedModuleIndex);
	if (HighModule == NULL)
	{
		// Couldn't find the highest module???
		return;
	}

	BeginTransaction(TEXT("ShareHigherSelectedModule"));
	ModifySelectedObjects();
	ModifyParticleSystem();

	PartSys->PreEditChange(NULL);

	UParticleModule* NewModule = NULL;
	UParticleLODLevel* DestLODLevel;
	UBOOL bIsShared = ModuleIsShared(SelectedModule);
	// Store the index of the selected module...
	// Force copy the source module...
	DestLODLevel = SelectedEmitter->GetLODLevel(CurrLODSetting);
	NewModule = HighModule->GenerateLODModule(SourceLODLevel, DestLODLevel, 100.0f, FALSE, FALSE);
	check(NewModule);

	for (INT LODIndex = CurrLODSetting; LODIndex < SelectedEmitter->LODLevels.Num(); LODIndex++)
	{
		DestLODLevel = SelectedEmitter->GetLODLevel(LODIndex);
		if (SelectedModule->IsUsedInLODLevel(LODIndex))
		{
			if (bIsShared == FALSE)
			{
				// Turn off the LOD validity in the original module...
				SelectedModule->LODValidity &= ~(1 << DestLODLevel->Level);
			}
			// Turn on the LOD validity int he new module...
			NewModule->LODValidity |= (1 << LODIndex);

			// Store the new module
			switch (SelectedModuleIndex)
			{
			case INDEX_NONE:
				break;
			case INDEX_REQUIREDMODULE:
				DestLODLevel->RequiredModule = CastChecked<UParticleModuleRequired>(NewModule);
				break;
			case INDEX_SPAWNMODULE:
				DestLODLevel->SpawnModule = CastChecked<UParticleModuleSpawn>(NewModule);
				break;
			case INDEX_TYPEDATAMODULE:
				DestLODLevel->TypeDataModule = NewModule;
				break;
			default:
				DestLODLevel->Modules(SelectedModuleIndex) = NewModule;
				break;
			}
		}
	}

	SelectedModule = NewModule;
	if (SelectedEmitter)
	{
		SelectedEmitter->UpdateModuleLists();
	}

	PartSys->PostEditChange();

	CascadeTouch();
	SetSelectedModule(SelectedEmitter, SelectedModule);

	EndTransaction(TEXT("ShareHigherSelectedModule"));

	PartSys->MarkPackageDirty();
	EmitterEdVC->Viewport->Invalidate();
}

UBOOL WxCascade::ModuleIsShared(UParticleModule* InModule)
{
	INT FindCount = 0;

	UParticleModuleSpawn* SpawnModule = Cast<UParticleModuleSpawn>(InModule);
	UParticleModuleRequired* RequiredModule = Cast<UParticleModuleRequired>(InModule);
	UParticleModuleTypeDataBase* TypeDataModule = Cast<UParticleModuleTypeDataBase>(InModule);

	INT	CurrLODSetting	= GetCurrentlySelectedLODLevelIndex();
	if (CurrLODSetting < 0)
	{
		return FALSE;
	}

	for (INT i = 0; i < PartSys->Emitters.Num(); i++)
	{
		UParticleEmitter* Emitter = PartSys->Emitters(i);
		UParticleLODLevel* LODLevel = Emitter->GetLODLevel(CurrLODSetting);
		if (LODLevel == NULL)
		{
			continue;
		}

		if (SpawnModule)
		{
			if (SpawnModule == LODLevel->SpawnModule)
			{
				FindCount++;
				if (FindCount >= 2)
				{
					return TRUE;
				}
			}
		}
		else if (RequiredModule)
		{
			if (RequiredModule == LODLevel->RequiredModule)
            {
                FindCount++;
                if (FindCount >= 2)
                {
	                return TRUE;
                }
            }
		}
		else if (TypeDataModule)
		{
			if (TypeDataModule == LODLevel->TypeDataModule)
			{
				FindCount++;
				if (FindCount >= 2)
				{
					return TRUE;
				}
			}
		}
		else
		{
			for (INT j = 0; j < LODLevel->Modules.Num(); j++)
			{
				if (LODLevel->Modules(j) == InModule)
				{
					FindCount++;
					if (FindCount == 2)
					{
						return TRUE;
					}
				}
			}
		}
	}

	return FALSE;
}

void WxCascade::AddSelectedToGraph()
{
	if (!SelectedEmitter)
		return;

	INT	CurrLODSetting	= GetCurrentlySelectedLODLevelIndex();
	if (SelectedEmitter->IsLODLevelValid(CurrLODSetting) == FALSE)
	{
		return;
	}

	if (SelectedModule)
	{
		UParticleLODLevel* LODLevel = SelectedEmitter->GetLODLevel(CurrLODSetting);
		if (LODLevel->IsModuleEditable(SelectedModule))
		{
			SelectedModule->AddModuleCurvesToEditor( PartSys->CurveEdSetup );
			CurveEd->CurveChanged();
		}
	}

	SetSelectedInCurveEditor();
	CurveEd->CurveEdVC->Viewport->Invalidate();
}

void WxCascade::SetSelectedInCurveEditor()
{
	CurveEd->ClearAllSelectedCurves();
	if (SelectedModule)
	{
		TArray<FParticleCurvePair> Curves;
		SelectedModule->GetCurveObjects(Curves);
		for (INT CurveIndex = 0; CurveIndex < Curves.Num(); CurveIndex++)
		{
			UObject* Distribution = Curves(CurveIndex).CurveObject;
			if (Distribution)
			{
				CurveEd->SetCurveSelected(Distribution, TRUE);
			}
		}
		CurveEd->ScrollToFirstSelected();
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

/**
 *	This function returns the name of the docking parent.  This name is used for saving and loading the layout files.
 *  @return A string representing a name to use for this docking parent.
 */
const TCHAR* WxCascade::GetDockingParentName() const
{
	return TEXT("Cascade");
}

/**
 * @return The current version of the docking parent, this value needs to be increased every time new docking windows are added or removed.
 */
const INT WxCascade::GetDockingParentVersion() const
{
	return 0;
}

/**
 *	Prompt the user for cancelling soloing mode to perform the selected operation.
 *
 *	@param	InOperationDesc		The description of the operation being attempted.
 *
 *	@return	UBOOL				TRUE if they opted to cancel and continue. FALSE if not.
 */
UBOOL WxCascade::PromptForCancellingSoloingMode(FString& InOperationDesc)
{
	if (PartSys == NULL)
	{
		return FALSE;
	}

	FString DisplayMessage;

	DisplayMessage = LocalizeUnrealEd("CASCADE_CancelSoloing");
	DisplayMessage += TEXT("\n");
	DisplayMessage += InOperationDesc;
	UBOOL bCancelSoloing = appMsgf(AMT_YesNo, *DisplayMessage);
	if (bCancelSoloing)
	{
		PartSys->TurnOffSoloing();
		bIsSoloing = FALSE;
	}
	return bCancelSoloing;
}

//
// UCascadeOptions
// 
