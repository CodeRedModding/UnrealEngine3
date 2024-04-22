//! @file SubstanceAirEdGenericBrowser.cpp
//! @brief Substance Air generic browser implementation
//! @contact antoine.gonzalez@allegorithmic.com
//! @copyright Allegorithmic. All rights reserved.

#include "UnrealEd.h"
#include "UnEdTran.h"
#include "UnObjectTools.h"

#include "SubstanceAirEdGraphInstanceEditorWindowShared.h"

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirPackage.h"
#include "SubstanceAirEdPreset.h"
#include "SubstanceAirHelpers.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirInstanceFactoryClasses.h"
#include "SubstanceAirTextureClasses.h"
#include "SubstanceAirImageInputClasses.h"

#include "SubstanceAirEdPreset.h"
#include "SubstanceAirEdHelpers.h"
#include "SubstanceAirEdBrowserClasses.h"


#define AIR_MAX_GRAPH_LIST_SIZE (10)

namespace
{
#if WITH_MANAGED_CODE
	/** GraphInstanceEditor window */
	TScopedPointer< class FGraphInstanceEditorWindow > GraphInstanceEditorWindow;
#endif
}

void GetGraphFromPackage(package_t* ParentPackage, 
	SubstanceAir::List<graph_desc_t*>& Graph)
{
	for (UINT Idx = 0 ; Idx < ParentPackage->Graphs.size() ; ++Idx)
	{
		if (Graph.size() < AIR_MAX_GRAPH_LIST_SIZE)
		{
			Graph.push(ParentPackage->Graphs[Idx]);
		}
		else
		{
			return;
		}
	}

	if (Graph.size() >= AIR_MAX_GRAPH_LIST_SIZE)
	{
		return;
	}
}


UBOOL IsSubstanceAirInstanceFactory( UObject * InObject)
{
	return Cast<USubstanceAirInstanceFactory>(InObject) ? TRUE : FALSE;
}


void UGenericBrowserType_SubstanceAirInstanceFactory::Init()
{
	SupportInfo.AddItem(
		FGenericBrowserTypeInfo(
			USubstanceAirInstanceFactory::StaticClass(),
			FColor(255,0,0),
			0,
			this,
			IsSubstanceAirInstanceFactory));
}


void UGenericBrowserType_SubstanceAirInstanceFactory::QuerySupportedCommands(
	USelection* InObjects, 
	TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	SubstanceAir::List<graph_desc_t*> GraphList;

	if (InObjects->Num())
	{
		for (FSelectionIterator SelIter(*InObjects); SelIter != NULL; ++SelIter)
		{
			UObject* SelObject = *SelIter;
			if (SelObject->IsA(USubstanceAirInstanceFactory::StaticClass()))
			{
				GetGraphFromPackage(
					CastChecked<USubstanceAirInstanceFactory>(SelObject)->SubstancePackage,
					GraphList);
			}

		if (GraphList.size() >= AIR_MAX_GRAPH_LIST_SIZE)
		{
			break;
		}
	}

		if (GraphList.size())
		{
			for (UINT Idx=0 ; Idx < GraphList.size() ; ++Idx)
			{
				OutCommands.AddItem( 
					FObjectSupportedCommandType( 
					IDMN_Factory_InstantiateGraph_0 + Idx,
					*(Localize(TEXT("Editor"), 
					TEXT("InstantiateGraph"),
					TEXT("SubstanceAir"), NULL, 0) +
					TEXT(" ") + GraphList[Idx]->Label)));
			}

			OutCommands.AddItem(FObjectSupportedCommandType(INDEX_NONE, ""));
		}
	}

	// display instantiate all only if their are more than one graph in the sbs
	if (GraphList.Num() > 1)
	{	
		OutCommands.AddItem( 
			FObjectSupportedCommandType( 
				IDMN_Factory_InstantiateAll,
				*Localize(TEXT("Editor"),
					TEXT("InstantiateAllGraphs"),
					TEXT("SubstanceAir"), NULL, 0)));
	}

	OutCommands.AddItem( 
		FObjectSupportedCommandType( 
			IDMN_ObjectContext_Reimport, 
			*Localize(TEXT("Editor"), 
				TEXT("ObjectContext_ReimportSubstance"),
				TEXT("SubstanceAir"), NULL, 0),
			!bAnythingCooked ) );

	OutCommands.AddItem( 
		FObjectSupportedCommandType( 
			IDMN_Delete_All_SbsObjects, 
			*Localize(TEXT("Editor"),
				TEXT("DeleteAllIOP"),
				TEXT("SubstanceAir"), NULL, 0),
			!bAnythingCooked));
}


void UGenericBrowserType_SubstanceAirInstanceFactory::InvokeCustomCommand(
	INT InCommand,
	TArray<UObject*>& InObjects)
{
	SubstanceAir::List<graph_inst_t*> NewInstances;

	USubstanceAirInstanceFactory* Factory = NULL;

	if (IDMN_Factory_InstantiateAll == InCommand)
	{
		for (TArray<UObject*>::TIterator Iter(InObjects); Iter!=NULL; ++Iter)
		{
			Factory = Cast<USubstanceAirInstanceFactory>(*Iter);

			if (Factory)
			{
				// instantiate the content of the package
				NewInstances =
					SubstanceAir::Helpers::InstantiateGraphs(
						Factory->SubstancePackage->Graphs);
			}
		}
	}
	else if(InCommand >= IDMN_Factory_InstantiateGraph_0 &&
			InCommand <= IDMN_Factory_InstantiateGraph_9)
	{
		SubstanceAir::List<graph_desc_t*> SelectionGraphList;

		for (TArray<UObject*>::TIterator Iter(InObjects); Iter!=NULL; ++Iter)
		{
			Factory = Cast<USubstanceAirInstanceFactory>(*Iter);

			if (Factory)
			{
				GetGraphFromPackage(
					Factory->SubstancePackage,
					SelectionGraphList);
			}

			if (SelectionGraphList.size() >= AIR_MAX_GRAPH_LIST_SIZE)
			{
				break;
			}
		}

		if (InCommand >= IDMN_Factory_InstantiateGraph_0 &&
			InCommand <= IDMN_Factory_InstantiateGraph_9)
		{
			INT IdxGraph = InCommand - IDMN_Factory_InstantiateGraph_0;
			graph_desc_t* Graph = SelectionGraphList[IdxGraph];
			SelectionGraphList.Empty();
			SelectionGraphList.AddUniqueItem(Graph);

			NewInstances =
				SubstanceAir::Helpers::InstantiateGraphs(SelectionGraphList);
		}
	}
	else if (InCommand == IDMN_ObjectContext_Reimport)
	{
		for (TArray<UObject*>::TIterator Iter(InObjects); Iter!=NULL; ++Iter)
		{
			Factory = Cast<USubstanceAirInstanceFactory>(*Iter);

			if (Factory && FReimportManager::Instance()->Reimport(Factory))
			{
				SubstanceAir::List<graph_desc_t*>::TIterator DescIt(
					Factory->SubstancePackage->Graphs.itfront());

				for (;DescIt;++DescIt)
				{
					NewInstances += (*DescIt)->LoadedInstances;
				}
			}
		}
	}
	else if (IDMN_Delete_All_SbsObjects == InCommand)
	{
		for (TArray<UObject*>::TIterator Iter(InObjects); Iter!=NULL; ++Iter)
		{
			Factory = Cast<USubstanceAirInstanceFactory>(*Iter);

			if (Factory)
			{
				if (FGraphInstanceEditorWindow::GetEditedInstance() && 					
					FGraphInstanceEditorWindow::GetEditedInstance()->ParentInstance->Parent == Factory)
				{
					appMsgf( AMT_OK, *Localize(TEXT("Editor"),
						TEXT("CannotDeleteSubstanceObject"),
						TEXT("SubstanceAir"), NULL, 0) );
				}
				else
				{
					SubstanceAir::Helpers::RegisterForDeletion(Factory);
				}				
			}
		}
	}

	if (NewInstances.size())
	{
		SubstanceAir::Helpers::RenderAsync(NewInstances);
		SubstanceAir::Helpers::FlagRefreshContentBrowser();

		if (Factory)
		{
			Factory->MarkPackageDirty();
		}
	}
}


UBOOL UGenericBrowserType_SubstanceAirInstanceFactory::NotifyPreDeleteObject(UObject* ObjectToDelete)
{
	graph_inst_t* EditedInstance = FGraphInstanceEditorWindow::GetEditedInstance();

	USubstanceAirInstanceFactory* InstFactory = Cast<USubstanceAirInstanceFactory>(ObjectToDelete);

	if (EditedInstance != NULL && InstFactory && EditedInstance->ParentInstance->Parent == InstFactory)
	{
		appMsgf( AMT_OK, 
			*Localize(TEXT("Editor"),
				TEXT("CannotDeleteSubstanceObject"),
				TEXT("SubstanceAir"), NULL, 0) );
		return FALSE;
	}

	return TRUE;
}


INT UGenericBrowserType_SubstanceAirInstanceFactory::QueryDefaultCommand( TArray<UObject*>& InObjects ) const
{
	return IDMN_ObjectContext_Editor;
}


UBOOL UGenericBrowserType_SubstanceAirInstanceFactory::ShowObjectProperties( UObject* InObject )
{
	return TRUE;
}


UBOOL IsSubstanceAirGraphInstance( UObject * InObject)
{
	return Cast<USubstanceAirGraphInstance>(InObject) ? TRUE : FALSE;
}


void UGenericBrowserType_SubstanceAirGraphInstance::Init()
{
	SupportInfo.AddItem(
		FGenericBrowserTypeInfo(
			USubstanceAirGraphInstance::StaticClass(),
			FColor(128,128,0),
			0,
			this,
			IsSubstanceAirGraphInstance));
}


void UGenericBrowserType_SubstanceAirGraphInstance::QuerySupportedCommands(
	USelection* InObjects, 
	TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked(InObjects);

	OutCommands.AddItem(
		FObjectSupportedCommandType(
			IDMN_ObjectContext_Editor,
			*Localize(TEXT("Editor"),
				TEXT("EditorGraphInstance"),
				TEXT("SubstanceAir"), NULL, 0)) );

	OutCommands.AddItem(FObjectSupportedCommandType(INDEX_NONE, ""));

	OutCommands.AddItem(
		FObjectSupportedCommandType( 
			IDMN_GraphInstance_CreateDefaultMaterial, 
			*Localize(TEXT("Editor"), 
				TEXT("CreateMaterialGraphInstance"),
				TEXT("SubstanceAir"), NULL, 0)));

	OutCommands.AddItem( 
		FObjectSupportedCommandType( 
			IDMN_GraphInstance_ExportAsPreset, 
			*Localize(
				TEXT("Editor"), 
				TEXT("ExportPreset"),
				TEXT("SubstanceAir"), NULL, 0)));

	OutCommands.AddItem( 
		FObjectSupportedCommandType( 
			IDMN_GraphInstance_ApplyPreset, 
			*Localize(
				TEXT("Editor"),
				TEXT("ImportPreset"),
				TEXT("SubstanceAir"), NULL, 0)));

	OutCommands.AddItem( 
		FObjectSupportedCommandType( 
			IDMN_Delete_All_SbsObjects, 
			*Localize(
				TEXT("Editor"),
				TEXT("DeleteAllIO"),
				TEXT("SubstanceAir"), NULL, 0),
			!bAnythingCooked));
}


UBOOL UGenericBrowserType_SubstanceAirGraphInstance::ShowObjectEditor(UObject* InObject)
{
#if WITH_MANAGED_CODE
	USubstanceAirGraphInstance* Instance = Cast<USubstanceAirGraphInstance>(InObject);
	if (!Instance || !Instance->Instance)
	{
		appDebugMessagef(TEXT("Substance: Impossible to show the Graph Instance editor, there is something wrong with the data."));
		return FALSE;
	}

	if (!Instance->Instance->Desc || !Instance->Parent)
	{
		appDebugMessagef(TEXT("Substance: Impossible to show the Graph Instance editor, there is something wrong with the data. The description (package) is missing."));
		return FALSE;
	}

	HWND EditorFrameWindowHandle = (HWND)GApp->EditorFrame->GetHandle();

	// make sure the instance and its textures are loaded
	SubstanceAir::Helpers::FullyLoadInstance(Instance);

	GraphInstanceEditorWindow.Reset(
		FGraphInstanceEditorWindow::CreateGraphInstanceEditorWindow(
			Instance->Instance,
			EditorFrameWindowHandle));

	check(GraphInstanceEditorWindow.IsValid());

	return TRUE;
#else
	appDebugMessagef(TEXT("Substance: Impossible to show the Graph Instance editor, the editor has been compiled without managed code support. "));
	return FALSE
#endif
}


void UGenericBrowserType_SubstanceAirGraphInstance::InvokeCustomCommand(
	INT InCommand, 
	TArray<UObject*>& InObjects)
{
	SubstanceAir::List<graph_inst_t*> InstancesToUpdate;

	if (IDMN_GraphInstance_ExportAsPreset == InCommand)
	{
		for (TArray<UObject*>::TIterator Iter(InObjects); Iter ; ++Iter)
		{			
			preset_t Preset;
			
			USubstanceAirGraphInstance* Instance = 
				Cast<USubstanceAirGraphInstance>(*Iter);

			Preset.ReadFrom(Instance->Instance);

			SubstanceAir::Helpers::SavePresetFile(Preset);
		}
	}
	else if (IDMN_GraphInstance_ApplyPreset == InCommand)
	{
		for (TArray<UObject*>::TIterator Iter(InObjects); Iter ; ++Iter)
		{
			FString PresetContent = SubstanceAir::Helpers::ImportPresetFile();

			if (PresetContent.Len())
			{
				presets_t Presets;
				SubstanceAir::ParsePresets(Presets, PresetContent);
				 
				//! @todo: ask the user which preset to use
				INT PresetIdx = 0;

				if (Presets.Num() > 0)
				{
					USubstanceAirGraphInstance* Instance = 
						Cast<USubstanceAirGraphInstance>(*Iter);
						
					UBOOL Modified = Presets(PresetIdx).Apply(
						Instance->Instance,SubstanceAir::FPreset::Apply_Merge);

					if (Modified)
					{
						InstancesToUpdate.push(Instance->Instance);
					}
				}
			}
		}
	}
	else if (IDMN_GraphInstance_CreateDefaultMaterial == InCommand)
	{
		for (TArray<UObject*>::TIterator Iter(InObjects); Iter ; ++Iter)
		{
			USubstanceAirGraphInstance* Instance = 
				Cast<USubstanceAirGraphInstance>(*Iter);

			FString MatName = 
				Instance->Instance->Desc->Label + "_MAT";

			SubstanceAir::Helpers::CreateMaterial(
				Instance->Parent,
				Instance->Instance,
				MatName);
		}
	}
	else if (IDMN_Delete_All_SbsObjects == InCommand)
	{
		for (TArray<UObject*>::TIterator Iter(InObjects); Iter ; ++Iter)
		{
			USubstanceAirGraphInstance* InstanceContainer = 
				Cast<USubstanceAirGraphInstance>(*Iter);

			if (FGraphInstanceEditorWindow::GetEditedInstance() &&
				(FGraphInstanceEditorWindow::GetEditedInstance()->ParentInstance == InstanceContainer ||
					FGraphInstanceEditorWindow::GetEditedInstance()->ParentInstance->Parent == InstanceContainer->Parent))
			{
				appMsgf( AMT_OK, 
					*Localize(TEXT("Editor"),
						TEXT("CannotDeleteSubstanceObject"),
						TEXT("SubstanceAir"), NULL, 0) );
			}
			else
			{
				SubstanceAir::Helpers::RegisterForDeletion(InstanceContainer);
			}
		}
	}

	if (InstancesToUpdate.size())
	{
		SubstanceAir::Helpers::RenderAsync(InstancesToUpdate);
		SubstanceAir::List<graph_inst_t*>::TIterator ItG(InstancesToUpdate.itfront());
		for (; ItG ; ++ItG)
		{
			UObject* Outer = (*ItG)->ParentInstance;
			Outer->MarkPackageDirty(TRUE);
			GCallbackEvent->Send(
				FCallbackEventParameters(
				NULL,
				CALLBACK_RefreshContentBrowser,
				CBR_UpdatePackageList|CBR_ObjectCreated|CBR_UpdateAssetListUI, 
				Outer));
		}
	}
}


UBOOL UGenericBrowserType_SubstanceAirGraphInstance::NotifyPreDeleteObject(UObject * ObjectToDelete)
{
	graph_inst_t* EditedInstance = FGraphInstanceEditorWindow::GetEditedInstance();

	USubstanceAirGraphInstance* Graph = Cast<USubstanceAirGraphInstance>(ObjectToDelete);

	if (EditedInstance != NULL && Graph && EditedInstance == Graph->Instance)
	{
		appMsgf( AMT_OK, 
			*Localize(TEXT("Editor"),
				TEXT("CannotDeleteSubstanceObject"),
				TEXT("SubstanceAir"), NULL, 0) );
		return FALSE;
	}

	return TRUE;
}


UBOOL IsSubstanceAirTexture2D( UObject * InObject)
{
	return Cast<USubstanceAirTexture2D>(InObject) ? TRUE : FALSE;
}


void UGenericBrowserType_SubstanceAirTexture2D::Init()
{
	SupportInfo.AddItem(
		FGenericBrowserTypeInfo(
			USubstanceAirTexture2D::StaticClass(),
			FColor(128,128,0),
			0,
			this,
			IsSubstanceAirTexture2D));
}


UBOOL UGenericBrowserType_SubstanceAirTexture2D::NotifyPreDeleteObject(UObject * ObjectToDelete)
{
	USubstanceAirTexture2D* Texture = Cast<USubstanceAirTexture2D>(ObjectToDelete);

	SubstanceAir::Helpers::UnregisterOutputAsImageInput(Texture, TRUE, FALSE);

	graph_inst_t* EditedInstance = FGraphInstanceEditorWindow::GetEditedInstance();

	if (EditedInstance)
	{
		SubstanceAir::List<output_inst_t>::TIterator ItOut(EditedInstance->Outputs.itfront());
		
		for (;ItOut;++ItOut)
		{
			if ((*ItOut).ParentInstance == EditedInstance->ParentInstance)
			{
				appMsgf( AMT_OK, 
					*Localize(TEXT("Editor"),
						TEXT("CannotDeleteSubstanceObject"),
						TEXT("SubstanceAir"), NULL, 0) );
				return FALSE;
			}
		}
	}

	return TRUE;
}


void UGenericBrowserType_SubstanceAirTexture2D::QuerySupportedCommands( 
	USelection* InObjects,
	TArray< FObjectSupportedCommandType >& OutCommands) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked(InObjects);

	OutCommands.AddItem( 
		FObjectSupportedCommandType( 
			IDMN_ObjectContext_Editor, 
			*LocalizeUnrealEd( "ObjectContext_EditUsingTextureViewer" ) ) );

	OutCommands.AddItem( 
		FObjectSupportedCommandType( 
		IDMN_ObjectContext_CreateNewMaterial, 
		*LocalizeUnrealEd( "ObjectContext_CreateNewMaterial" ), 
		!bAnythingCooked ) );

	OutCommands.AddItem( 
		FObjectSupportedCommandType( 
			IDMN_TextureFindParentInstance,
			*Localize(TEXT("Editor"), 
				TEXT("FindParentInstance"),
				TEXT("SubstanceAir"), NULL, 0),
			!bAnythingCooked));
}


void UGenericBrowserType_SubstanceAirTexture2D::InvokeCustomCommand( 
	INT InCommand, TArray<UObject*>& InObjects )
{
	if (InCommand == IDMN_TextureFindParentInstance)
	{
		for (TArray<UObject*>::TIterator Iter(InObjects); Iter!=NULL; ++Iter)
		{
			USubstanceAirTexture2D* Texture = Cast<USubstanceAirTexture2D>(*Iter);

			if (Texture != NULL)
			{
				TArray<UObject*> Objects;
				Objects.AddItem(Texture->ParentInstance);
				GApp->EditorFrame->SyncBrowserToObjects(Objects);
			}
		}
	}
}


UBOOL IsSubstanceAirImageInput(UObject * InObject)
{
	return Cast<USubstanceAirImageInput>(InObject) ? TRUE : FALSE;
}


void UGenericBrowserType_SubstanceAirImageInput::Init()
{
	SupportInfo.AddItem(
		FGenericBrowserTypeInfo(
			USubstanceAirImageInput::StaticClass(),
			FColor(128,64,128),
			0,
			this,
			IsSubstanceAirImageInput));
}


void UGenericBrowserType_SubstanceAirImageInput::QuerySupportedCommands( 
	class USelection* InObjects,
	TArray< FObjectSupportedCommandType >& OutCommands ) const
{
	const UBOOL bAnythingCooked = AnyObjectsAreCooked( InObjects );

	OutCommands.AddItem( 
		FObjectSupportedCommandType( 
			IDMN_ObjectContext_Reimport, 
			*Localize(TEXT("Editor"), 
				TEXT("ObjectContext_ReimportImageInput"),
				TEXT("SubstanceAir"), NULL, 0),
			!bAnythingCooked));
}


void UGenericBrowserType_SubstanceAirImageInput::InvokeCustomCommand(
	INT InCommand, TArray<UObject*>& InObjects )
{
	if (InCommand == IDMN_ObjectContext_Reimport)
	{
		for (TArray<UObject*>::TIterator Iter(InObjects); Iter!=NULL; ++Iter)
		{
			UObject* Object = *Iter;
			FReimportManager::Instance()->Reimport(Object);
		}
	}
}


UBOOL UGenericBrowserType_SubstanceAirImageInput::ShowObjectProperties(UObject* InObject)
{
	return TRUE;
}

IMPLEMENT_CLASS(UGenericBrowserType_SubstanceAirGraphInstance)
IMPLEMENT_CLASS(UGenericBrowserType_SubstanceAirInstanceFactory)
IMPLEMENT_CLASS(UGenericBrowserType_SubstanceAirImageInput)
IMPLEMENT_CLASS(UGenericBrowserType_SubstanceAirTexture2D)
