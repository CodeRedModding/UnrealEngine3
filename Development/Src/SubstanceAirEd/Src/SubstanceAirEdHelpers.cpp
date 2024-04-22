//! @file SubstanceAirHelpers.cpp
//! @author Antoine Gonzalez - Allegorithmic
//! @date 20110105
//! @copyright Allegorithmic. All rights reserved.

#include <UnrealEd.h>
#include <Factories.h>
#include <UnEdTran.h>

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirPackage.h"
#include "SubstanceAirInstanceFactoryClasses.h"
#include "SubstanceAirImageInputClasses.h"

#include "SubstanceAirEdFactoryClasses.h"
#include "SubstanceAirEdPreset.h"
#include "SubstanceAirEdNewGraphInstanceDlg.h"
#include "SubstanceAirEdHelpers.h"
#include "SubstanceAirEdCreateImageInputDlg.h"
#include "SubstanceAirEdBrowserClasses.h"

namespace local
{
	TArray<USubstanceAirGraphInstance*> InstancesToDelete;
	TArray<USubstanceAirInstanceFactory*> FactoriesToDelete;
	TArray<UObject*> TexturesToDelete;
}

namespace SubstanceAir
{
namespace Helpers
{

void CreateMaterialExpression(
	output_inst_t* OutputInst,
	output_desc_t* OutputDesc,
	UMaterial* UnrealMaterial);


//! @brief Create an Unreal Material for the given graph-instance
UMaterial* CreateMaterial(USubstanceAirInstanceFactory* Parent, 
						  graph_inst_t* GraphInstance,
						  const FString & BaseName,
						  UBOOL bFocusInObjectBrowser)
{
	// set where to place the materials
	UObject* Package = GraphInstance->ParentInstance->CreatePackage(
		GraphInstance->ParentInstance->GetOuter(),
		ANSI_TO_TCHAR("Materials"));

	FString MaterialName = BaseName;
	INT Count = 0;

	// increment the instance number as long as there is already a package with that name
	while (FindObject<UMaterial>(Package, *MaterialName))
	{
		MaterialName = BaseName + FString::Printf(TEXT("_%i"),Count++);
	}

	// create an unreal material asset
	UMaterialFactoryNew* MaterialFactory = new UMaterialFactoryNew;

	UMaterial* UnrealMaterial = (UMaterial*)MaterialFactory->FactoryCreateNew(
		UMaterial::StaticClass(), Package, *MaterialName, RF_Standalone|RF_Public, NULL, GWarn );

	SubstanceAir::List<output_inst_t>::TIterator 
		ItOut(GraphInstance->Outputs.itfront());

	// textures and properties
	for ( ; ItOut ; ++ItOut)
	{
		output_inst_t* OutputInst = &(*ItOut);
		output_desc_t* OutputDesc = GraphInstance->Desc->GetOutputDesc(
			OutputInst->Uid);

		CreateMaterialExpression(
			OutputInst,
			OutputDesc,
			UnrealMaterial);
	}

	//remove any memory copies of shader files, so they will be reloaded from disk
	//this way the material editor can be used for quick shader iteration
	FlushShaderFileCache();

	// let the material update itself if necessary
	UnrealMaterial->PreEditChange(NULL);
	UnrealMaterial->PostEditChange();

	if (!GIsUCC && bFocusInObjectBrowser)
	{
		DWORD RefreshFlags = CBR_UpdatePackageList|CBR_UpdateAssetList;
		FCallbackEventParameters Parms( NULL, CALLBACK_RefreshContentBrowser, RefreshFlags );

	GCallbackEvent->Send(Parms);

		TArray<UObject*> Objects;
		Objects.AddItem(UnrealMaterial);
		GApp->EditorFrame->SyncBrowserToObjects(Objects);
	}
	
	return UnrealMaterial;
}


void CreateMaterialExpression(output_inst_t* OutputInst,
							  output_desc_t* OutputDesc,
							  UMaterial* UnrealMaterial)
{
	FExpressionInput * MaterialInput = NULL;

	switch(OutputDesc->Channel)
	{
	case CHAN_Diffuse:
		MaterialInput = &UnrealMaterial->DiffuseColor;
		break;

	case CHAN_Emissive:
		MaterialInput = &UnrealMaterial->EmissiveColor;
		break;

	case CHAN_Normal:
		MaterialInput = &UnrealMaterial->Normal;
		break;

	case CHAN_Specular:
	case CHAN_SpecularColor:
		MaterialInput = &UnrealMaterial->SpecularColor;
		break;

	case CHAN_Glossiness:
	case CHAN_SpecularLevel:
		//! @todo multiply this by 255 and plug this in the specular power entry
		MaterialInput = &UnrealMaterial->SpecularPower;
		break;

	case CHAN_Mask:
		MaterialInput = &UnrealMaterial->OpacityMask;
		break;

	case CHAN_Opacity:
		MaterialInput = &UnrealMaterial->Opacity;
		break;

	case CHAN_AnisotropyAngle:
		MaterialInput = &UnrealMaterial->AnisotropicDirection;
		break;

	case CHAN_Transmissive:
		MaterialInput = &UnrealMaterial->TwoSidedLightingColor;
		break;

	default:
		// nothing relevant to plug, skip it
		return;
		break;
	}

	UTexture* UnrealTexture = *OutputInst->Texture;

	if (UnrealTexture)
	{
		// and link it to the material 
		UMaterialExpressionTextureSampleParameter2D* UnrealTextureExpression =
			ConstructObject<UMaterialExpressionTextureSampleParameter2D>(
			UMaterialExpressionTextureSampleParameter2D::StaticClass(),
			UnrealMaterial );

		UnrealTextureExpression->MaterialExpressionEditorX = 300;
		UnrealTextureExpression->MaterialExpressionEditorY = UnrealMaterial->Expressions.Num() * 140;

		UnrealMaterial->Expressions.AddItem( UnrealTextureExpression );
		MaterialInput->Expression = UnrealTextureExpression;
		UnrealTextureExpression->Texture = UnrealTexture;
		UnrealTextureExpression->ParameterName = *OutputDesc->Identifier;
	}

	if (MaterialInput->Expression)
	{
		TArray<FExpressionOutput> Outputs;
		Outputs = MaterialInput->Expression->GetOutputs();
		FExpressionOutput* Output = &Outputs(0);
		MaterialInput->Mask = Output->Mask;
		MaterialInput->MaskR = Output->MaskR;
		MaterialInput->MaskG = Output->MaskG;
		MaterialInput->MaskB = Output->MaskB;
		MaterialInput->MaskA = Output->MaskA;
	}
}


graph_inst_t* InstantiateGraph(
	graph_desc_t* Graph,
	UObject* Outer,
	UBOOL bUseDefaultPackage,
	UBOOL bCreateOutputs,
	FString InstanceName,
	UBOOL bShowCreateMaterialCheckBox)
{
	UBOOL bCreateMaterial = FALSE;
	graph_inst_t* NewInstance = NULL;

	USubstanceAirInstanceFactory* Parent = Graph->Parent->Parent;
	UPackage* Package = Parent->GetOutermost();
	FString PackageName = Package->GetName();

	if (InstanceName.Len() == 0)
	{
		InstanceName = 
			FString::Printf( TEXT("%s_INST"), *Graph->Label);

		InstanceName.ReplaceInline(TEXT(" "), TEXT("_"));
		InstanceName.ReplaceInline(TEXT("."), TEXT("_"));
	}

	FString GroupName;

	if (Outer)
	{
		Package = CastChecked<UPackage>(Outer);
		InstanceName = 
			GetSuitableNameT<USubstanceAirGraphInstance>(
			InstanceName,
			Package);
	}
	else if (FALSE == bUseDefaultPackage)
	{
		WxDlgNewGraphInstance InstanceDlg(bShowCreateMaterialCheckBox);
		INT Result = 
			InstanceDlg.ShowModal(InstanceName, PackageName, GroupName);

		if (Result == wxID_OK)
		{
			Package = InstanceDlg.InstanceOuter ? 
				InstanceDlg.InstanceOuter :
			Package;
			InstanceName = InstanceDlg.getInstanceName();
			bCreateMaterial = InstanceDlg.GetCreateMaterial();
		}
		else
		{
			return NULL;
		}
	}
	else
	{
		Package = Cast<UPackage>(Parent->GetOuter());
		Package = Package->CreatePackage(
			Package,
			*GroupName);

		if (InstanceName.Len() == 0)
		{
			InstanceName = 
				GetSuitableNameT<USubstanceAirGraphInstance>(
				InstanceName,
				Package);
		}
	}

	USubstanceAirGraphInstance* GraphInstance = 
		CastChecked<USubstanceAirGraphInstance>(
			UObject::StaticConstructObject(
				USubstanceAirGraphInstance::StaticClass(),
				Package,
				*InstanceName,
				RF_Standalone|RF_Public|RF_Transactional));

	NewInstance = Graph->Instantiate(GraphInstance, bCreateOutputs);

	if (bCreateMaterial)
	{
		FString MatName =
			InstanceName + "_MAT";

		SubstanceAir::Helpers::CreateMaterial(
			Parent,
			NewInstance,
			MatName);
	}

	return NewInstance;
}


SubstanceAir::List<graph_inst_t*> InstantiateGraphs(
	SubstanceAir::List<graph_desc_t*>& Graphs,
	UObject* Outer,
	UBOOL bUseDefaultPackage,
	UBOOL bCreateOutputs,
	UBOOL bShowCreateMaterialCheckBox)
{
	SubstanceAir::List<graph_inst_t*> NewInstances;
	SubstanceAir::List<graph_desc_t*>::TIterator GraphIt(Graphs.itfront());

	for (; GraphIt ; ++GraphIt)
	{
		graph_inst_t* inst = 
			InstantiateGraph(*GraphIt, Outer, bUseDefaultPackage, bCreateOutputs, FString(), bShowCreateMaterialCheckBox);
		
		if (inst)
		{
			NewInstances.push(inst);
		}
	}

	return NewInstances;
}


void SavePresetFile(preset_t& Preset)
{
	WxFileDialog ExportFileDialog( NULL, 
		TEXT("Export Preset"),
		*(GApp->LastDir[LD_GENERIC_EXPORT]),
		TEXT(""),
		TEXT("Export Types (*.sbsprs)|*.sbsprs;|All Files|*.*"),
		wxSAVE | wxOVERWRITE_PROMPT,
		wxDefaultPosition);

	FString PresetContent;

	// Display the Open dialog box.
	if( ExportFileDialog.ShowModal() == wxID_OK )
	{
		FString Path(ExportFileDialog.GetPath());
		GApp->LastDir[LD_GENERIC_EXPORT] = Path;

		FString PresetContent;
		SubstanceAir::WritePreset(Preset, PresetContent);
		appSaveStringToFile(PresetContent, *Path);
	}
}


FString ImportPresetFile()
{
	WxFileDialog ImportFileDialog( NULL, 
		TEXT("Import Preset"),
		*(GApp->LastDir[LD_GENERIC_IMPORT]),
		TEXT(""),
		TEXT("Import Types (*.sbsprs)|*.sbsprs;|All Files|*.*"),
		wxOPEN | wxFILE_MUST_EXIST,
		wxDefaultPosition);

	FString PresetContent;

	// Display the Open dialog box.
	if( ImportFileDialog.ShowModal() == wxID_OK )
	{
		FString Path(ImportFileDialog.GetPath());
		GApp->LastDir[LD_GENERIC_IMPORT] = Path;
		appLoadFileToString(PresetContent, *Path);
	}

	return PresetContent;
}
	

void CreateImageInput(UTexture2D* Texture)
{
	check(Texture != NULL);

	FString GroupName;
	FString PackageName = Texture->GetOutermost()->GetName();

	//! @todo: look for an unused name
	FString InstanceName = FString::Printf( TEXT("%s_IMG"), *Texture->GetName());

	WxDlgCreateImageInput ImageInputDlg;
	INT Result = 
		ImageInputDlg.ShowModal(InstanceName, PackageName, GroupName);

	if (Result == wxID_OK)
	{
		UPackage* Package = ImageInputDlg.InstanceOuter ? 
			ImageInputDlg.InstanceOuter :
			Texture->GetOutermost();

		InstanceName = ImageInputDlg.getInstanceName();

		UFactory* Factory = ConstructObject<UFactory>(
			USubstanceAirImageInputFactory::StaticClass());

		TArray<BYTE> UncompressedSourceArt;
		Texture->GetUncompressedSourceArt(UncompressedSourceArt);

		if (0 == UncompressedSourceArt.Num())
		{
			GWarn->Log( TEXT("-- Cannot create Substance Image Input: texture is missing uncompressed source art."));
			return;
		}

		const BYTE* Ptr = (BYTE*)&UncompressedSourceArt(0);
		const BYTE* PtrPlus = Ptr+UncompressedSourceArt.Num();

		Factory->FactoryCreateBinary(
			USubstanceAirImageInput::StaticClass(),
			Package,
			*InstanceName,
			RF_Standalone|RF_Public,
			Texture,
			*Texture->GetFullName(),
			Ptr,
			PtrPlus,
			NULL );

		Package->MarkPackageDirty(TRUE);
		GCallbackEvent->Send(
			FCallbackEventParameters(
				NULL,
				CALLBACK_RefreshContentBrowser,
				CBR_UpdatePackageList|CBR_ObjectCreated|CBR_UpdateAssetListUI, 
				Package));
	}
}


FString GetSuitableName(output_inst_t* Instance, UObject* Outer)
{
	graph_desc_t* Graph = Instance->ParentInstance->Instance->Desc;
	UObject* TextureOuter = Outer ? Outer : Instance->ParentInstance->GetOuter();

	for (UINT IdxOut=0 ; IdxOut<Graph->OutputDescs.size() ; ++IdxOut)
	{
		//if we found the original description
		if (Graph->OutputDescs(IdxOut).Uid == Instance->Uid)
		{
			FString BaseName = 
				FString::Printf(
				TEXT("%s_%s"),
				*Instance->ParentInstance->GetName(),
				*Graph->OutputDescs(IdxOut).Identifier);

			return GetSuitableNameT<USubstanceAirTexture2D>(
				BaseName, 
				TextureOuter);
		}
	}

	debugf(TEXT("Substance: error, unable to get texture name, something is wrong"));

	// still got to find a name, and still got to have it unique
	FString BaseName(TEXT("TEXTURE_NAME_NOT_FOUND"));
	return GetSuitableNameT<USubstanceAirTexture2D>(
		BaseName,
		TextureOuter);
}


void RegisterForDeletion( USubstanceAirGraphInstance* InstanceContainer )
{
	InstanceContainer->GetOutermost()->FullyLoad();
	local::InstancesToDelete.AddItem(InstanceContainer);
}


void RegisterForDeletion( USubstanceAirInstanceFactory* Factory )
{
	Factory->GetOutermost()->FullyLoad();
	local::FactoriesToDelete.AddItem(Factory);
}


void RegisterForDeletion( USubstanceAirTexture2D* Texture)
{
	local::TexturesToDelete.AddItem((UObject*)Texture);
}


void PerformDelayedDeletion()
{
	UBOOL DeletedSomething = FALSE;

	// start by deleting the textures
	if (local::TexturesToDelete.Num())
	{
		TArray<UGenericBrowserType*> GenericBrowserTypes;
		GenericBrowserTypes.AddItem(ConstructObject<UGenericBrowserType>( 
			UGenericBrowserType_SubstanceAirTexture2D::StaticClass()));

		TArray<UObject*>::TIterator itT(local::TexturesToDelete);
		for (;itT;++itT)
		{
			if (!(*itT)->IsPendingKill() && !(*itT)->HasAnyFlags(RF_RootSet))
			{
				(*itT)->ClearFlags(RF_Standalone);				
			}
			else
			{
				local::TexturesToDelete.Remove(itT.GetIndex());
			}
		}

		if (local::TexturesToDelete.Num())
		{
			ObjectTools::ForceDeleteObjects(local::TexturesToDelete, GenericBrowserTypes);
			local::TexturesToDelete.Empty();
			DeletedSomething = TRUE;
		}
	}

	//! @todo: check all the instances are loaded before performing the deletion
	// collect the instances 
	TArray<UObject*> Factories;

	TArray<USubstanceAirInstanceFactory*>::TIterator ItFact(local::FactoriesToDelete);
	for(;ItFact;++ItFact)
	{
		if ((*ItFact)->IsPendingKill() || (*ItFact)->HasAnyFlags(RF_RootSet))
		{
			local::FactoriesToDelete.Remove(ItFact.GetIndex());
			continue;
		}

		USubstanceAirInstanceFactory* Factory = *ItFact;
		Factories.AddItem(Factory);
		
		SubstanceAir::List<graph_desc_t*>::TIterator 
			ItDesc(Factory->SubstancePackage->Graphs.itfront());

		for(; ItDesc ; ++ItDesc)
		{
			SubstanceAir::List<graph_inst_t*>::TIterator 
				ItInst((*ItDesc)->LoadedInstances.itfront());

			for (;ItInst;++ItInst)
			{
				local::InstancesToDelete.AddItem((*ItInst)->ParentInstance);
			}
		}
	}

	TArray<USubstanceAirGraphInstance*>::TIterator ItInst(local::InstancesToDelete);
	for(;ItInst;++ItInst)
	{
		if ((*ItInst)->IsPendingKill() || (*ItInst)->HasAnyFlags(RF_RootSet))
		{
			local::InstancesToDelete.Remove(ItInst.GetIndex());
			continue;
		}

		USubstanceAirGraphInstance* InstanceContainer = *ItInst;

		{
			TArray<UObject*> Objects;
			SubstanceAir::List<output_inst_t>::TIterator 
				ItOut(InstanceContainer->Instance->Outputs.itfront());

			for(; ItOut ; ++ItOut)
			{
				if(*((*ItOut).Texture).get())
				{
					Objects.AddItem(*((*ItOut).Texture).get());
				}
			}

			// delete every child textures
			TArray<UGenericBrowserType*> GenericBrowserTypes;
			GenericBrowserTypes.AddItem(ConstructObject<UGenericBrowserType>( 
				UGenericBrowserType_SubstanceAirTexture2D::StaticClass()));
			ObjectTools::ForceDeleteObjects(Objects, GenericBrowserTypes);

			DeletedSomething = TRUE;
		}

		// and the instances themselves
		{
			TArray<UObject*> Objects;
			TArray<UGenericBrowserType*> GenericBrowserTypes;
			Objects.AddItem(InstanceContainer);

			GenericBrowserTypes.AddItem(ConstructObject<UGenericBrowserType>( 
				UGenericBrowserType_SubstanceAirGraphInstance::StaticClass()));
			ObjectTools::ForceDeleteObjects(Objects, GenericBrowserTypes);

			DeletedSomething = TRUE;
		}
	}

	// conclude by deleting factories
	if (Factories.Num())
	{
		TArray<UGenericBrowserType*> GenericBrowserTypes;
		GenericBrowserTypes.AddItem(ConstructObject<UGenericBrowserType>( 
			UGenericBrowserType_SubstanceAirInstanceFactory::StaticClass()));
		ObjectTools::ForceDeleteObjects(Factories, GenericBrowserTypes);

		DeletedSomething = TRUE;
	}

	local::InstancesToDelete.Empty();
	local::FactoriesToDelete.Empty();

	if (DeletedSomething)
	{
		GCallbackEvent->Send(CALLBACK_RefreshContentBrowser);
	}
}


void DuplicateGraphInstance(
	graph_inst_t* Instance, 
	SubstanceAir::List<graph_inst_t*> &OutInstances,
	UObject* Outer,
	UBOOL bUseDefaultPackage,
	UBOOL bCopyOutputs )
{
	UBOOL bCreateOutputs = FALSE;

	graph_inst_t* NewInstance = 
		SubstanceAir::Helpers::InstantiateGraph(
			Instance->Desc, Outer, bUseDefaultPackage, bCreateOutputs);

	if (NewInstance)
	{
		CopyInstance(Instance, NewInstance, bCopyOutputs);
		OutInstances.push(NewInstance);
	}
}


void CopyInstance( 
	graph_inst_t* RefInstance, 
	graph_inst_t* NewInstance,
	UBOOL bCopyOutputs) 
{
	// copy values from previous
	preset_t Preset;
	Preset.ReadFrom(RefInstance);
	Preset.Apply(NewInstance, SubstanceAir::FPreset::Apply_Merge);

	if (bCopyOutputs)
	{
		//! create same outputs as ref instance
		for(UINT Idx=0 ; Idx<NewInstance->Outputs.size() ; ++Idx)
		{
			output_inst_t* OutputRefInstance = &RefInstance->Outputs(Idx);
			output_inst_t* OutputInstance = &NewInstance->Outputs(Idx);

			// if no texture associated to that Output
			if (OutputRefInstance->bIsEnabled)
			{
				// find the description 
				SubstanceAir::Helpers::CreateTexture2D(OutputInstance);
			}
		}
	}
}


// map containing a initial values (sbsprs) of each modified graph instance
// used when loading a kismet sequence
TMap< graph_inst_t*, FString > GInitialGraphValues;


void SaveInitialValues(graph_inst_t *Graph)
{
	if (FALSE == GInitialGraphValues.HasKey(Graph))
	{	
		FString PresetValue;
		preset_t InitialValuesPreset;
		InitialValuesPreset.ReadFrom(Graph);
		WritePreset(InitialValuesPreset, PresetValue);
		GInitialGraphValues.Set(Graph, PresetValue);
	}
}


void RestoreGraphInstances()
{
	TMap< graph_inst_t*, FString >::TIterator ItSavedPreset(GInitialGraphValues);

	for (; ItSavedPreset ; ++ItSavedPreset)
	{
		presets_t Presets;
		ParsePresets(Presets, ItSavedPreset.Value());

		if (Presets(0).Apply(ItSavedPreset.Key()))
		{
			SubstanceAir::Helpers::PushDelayedRender(ItSavedPreset.Key());
		}
	}
}


void FullyLoadInstance(USubstanceAirGraphInstance* Instance)
{
	Instance->GetOutermost()->FullyLoad();

	SubstanceAir::List<output_inst_t>::TIterator 
		ItOut(Instance->Instance->Outputs.itfront());

	for(; ItOut ; ++ItOut)
	{
		if(*((*ItOut).Texture).get())
		{
			(*((*ItOut).Texture).get())->GetOutermost()->FullyLoad();
		}
	}
}

} // namespace Helpers
} // namespace SubstanceAir
