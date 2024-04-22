//! @file SubstanceAirEdTextureFactory.cpp
//! @brief Factory to create Substances
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include <UnrealEd.h>
#include <Factories.h>
#include <UnObjectTools.h>

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirPackage.h"
#include "SubstanceAirTextureClasses.h"
#include "SubstanceAirInstanceFactoryClasses.h"
#include "SubstanceAirEdHelpers.h"
#include "SubstanceAirHelpers.h"

#include "framework/renderer.h"

#include "SubstanceAirEdFactoryClasses.h"
#include "SubstanceAirEdPreset.h"

struct InstanceBackup
{
	preset_t Preset;
	USubstanceAirGraphInstance* InstanceParent;
};

namespace local
{
	UBOOL bIsPerformingReimport = FALSE;
}

typedef SubstanceAir::List<output_inst_t> Outputs_t;
typedef SubstanceAir::List<output_inst_t>::TIterator itOutputs_t;


void USubstanceAirImportFactory::StaticConstructor()
{
	new( GetClass(), TEXT( "Create Default Material" ), RF_Public ) 
		UBoolProperty( CPP_PROPERTY( bCreateMaterial ), TEXT( "Substance" ), CPF_Edit );
	new( GetClass(), TEXT( "Create Default Graph Instance" ), RF_Public ) 
		UBoolProperty( CPP_PROPERTY( bCreateDefaultInstance ), TEXT( "Substance" ), CPF_Edit );
	new( GetClass(), TEXT( "Specify Instance Destination" ), RF_Public ) 
		UBoolProperty( CPP_PROPERTY( bSpecifyInstancePackage ), TEXT( "Substance" ), CPF_Edit );

	new( GetClass()->HideCategories ) FName(NAME_Object);
}


void USubstanceAirImportFactory::InitializeIntrinsicPropertyValues()
{
	// the factory builds a USubstanceAirPackage, then this package
	// will in turn create the textures
	SupportedClass	= USubstanceAirInstanceFactory::StaticClass();
	Description		= TEXT("Substance texture package");

	// format of the file to import
	new(Formats)FString(TEXT("sbsar;Substance Texture"));

	// imports binary data via FactoryCreateBinary
	bText			= FALSE;
	bCreateNew		= FALSE;
	AutoPriority	= -1;
	bEditorImport   = 1;

	// create the default instance by default
	bCreateDefaultInstance = TRUE;
	bCreateMaterial = FALSE;
	bSpecifyInstancePackage = FALSE;
}


UObject* USubstanceAirImportFactory::FactoryCreateBinary(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	EObjectFlags		Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const BYTE*&		Buffer,
	const BYTE*			BufferEnd,
	FFeedbackContext*	Warn)
{
	USubstanceAirInstanceFactory* Factory = NULL;

	UBOOL bRemoveParentFromRoot = FALSE;

	// check if an already existing instance factory with the same name exists,
	// its references would be replaced by the new one, which would break its 
	// instances. Offer the user to rename the new one
	if ( !local::bIsPerformingReimport && NULL != (Factory = FindObject<USubstanceAirInstanceFactory>(InParent, *Name.ToString())))
	{
		if (appMsgf(
				AMT_OKCancel,
				*FString::Printf( TEXT("An object with that name (%s) already exists. Do you want to rename the new ?" ), *Name.ToString())))
		{
			INT Count = 0;
			FString NewName = Name.ToString() + FString::Printf(TEXT("_%i"),Count);

			// increment the instance number as long as there is already a package with that name
			while (FindObject<USubstanceAirInstanceFactory>(InParent, *NewName))
			{
				NewName = Name.ToString() + FString::Printf(TEXT("_%i"),Count++);
			}
			Name = FName(*NewName);
		}
		else
		{
			return NULL;
		}
	}

	Factory = CastChecked<USubstanceAirInstanceFactory>(
		StaticConstructObject(
			USubstanceAirInstanceFactory::StaticClass(),
			InParent,
			Name,
			RF_Standalone|RF_Public));

	if (bRemoveParentFromRoot)
	{
		InParent->RemoveFromRoot();
	}

	const INT BufferLength = BufferEnd - Buffer;

	Factory->SubstancePackage = new package_t;
	Factory->SubstancePackage->Parent = Factory;
	Factory->SubstancePackage->Guid = appCreateGuid();

	// and load the data in its associated Package
	Factory->SubstancePackage->SetData(
		Buffer,
		BufferLength,
		CurrentFilename);

	// if the operation failed
	if (FALSE == Factory->SubstancePackage->IsValid())
	{
		// mark the package for garbage collect
		Factory->ClearFlags(RF_Standalone);
		return NULL;
	}

	if (bCreateDefaultInstance)
	{
		// instantiate the content of the package and render its content
		SubstanceAir::List<graph_inst_t*> Instances =
			SubstanceAir::Helpers::InstantiateGraphs(
				Factory->SubstancePackage->Graphs,
				NULL,
				!bSpecifyInstancePackage,
				TRUE, // create outputs
				FALSE); // do not show material checkbox as it was already show in previous dialog box

		SubstanceAir::Helpers::RenderSync(Instances);
		SubstanceAir::Helpers::UpdateTextures(Instances);

		// GIsUCC is on when importing in the ImportCommandlet,
		// in that case we always want a material
		if (bCreateMaterial || GIsUCC)
		{
			for (INT Idx = 0 ; Idx < Instances.Num() ; ++Idx)
			{
				FString MatName =
					Factory->SubstancePackage->Graphs[Idx]->Label + "_MAT";

				SubstanceAir::Helpers::CreateMaterial(
					Factory,
					Instances(Idx),
					MatName);
			}
		}
	}

	return Factory;
}


void GiveImageInputsBack(
	graph_inst_t* NewInstance,
	SubstanceAir::List< std::tr1::shared_ptr<input_inst_t> >& PrevImageInputs)
{
	SubstanceAir::List< std::tr1::shared_ptr<input_inst_t> >::TIterator
		ItPrev(PrevImageInputs.itfront());

	for (;ItPrev;++ItPrev)
	{
		SubstanceAir::List< std::tr1::shared_ptr<input_inst_t> >::TIterator 
			ItNew(NewInstance->Inputs.itfront());
		std::tr1::shared_ptr<input_inst_t> InputMatch;

		for (;ItNew;++ItNew)
		{
			if ((*ItNew)->IsNumerical())
			{
				continue;
			}

			// by uid
			if((*ItNew)->Uid == (*ItPrev)->Uid)
			{
				InputMatch = (*ItNew);
				break;
			}
		}

		// skip if the image input was changed
		if (InputMatch.get() == NULL)
		{
			continue;
		}

		input_desc_t* Desc = InputMatch->Desc;
		*InputMatch.get() = *(*ItPrev).get();
		InputMatch->Desc = Desc;

		((img_input_inst_t*)(InputMatch.get()))->ImageSource = 
			((img_input_inst_t*)(*ItPrev).get())->ImageSource;

		((img_input_inst_t*)(InputMatch.get()))->ImageInput = 
			((img_input_inst_t*)(*ItPrev).get())->ImageInput;
	}
}

//! @brief find an output matching another in a given list of outputs
//! @param WantedOutput, the output to match
//! @param CandidateOutputs, the container to search into
//! @param ExactMatch, does the match have to be exact ? 
//!        True will match by uid
//!        False will match by role
//! @return the index of a match in the outputs container
INT FindMatchingOutput(itOutputs_t& WantedOutput, Outputs_t& CandidateOutputs, UBOOL ExactMatch = TRUE)
{
	itOutputs_t ItOther(CandidateOutputs.itfront());

	for (;ItOther;++ItOther)
	{
		if((*WantedOutput).Uid == (*ItOther).Uid)
		{
			return ItOther.GetIndex();
		}
	}

	/*
	Not exact matching is currently not possible as the previous 
	graph desc are destroyed before this step, which means we cannot 
	compare identifiers, labels and channels...
	
	if (FALSE == ExactMatch)
	{
		ItOther.Reset();
		for (;ItOther;++ItOther)
		{
			output_desc_t* DescOutput = (*WantedOutput).GetOutputDesc();
			output_desc_t* DescOtherOutput = (*ItOther).GetOutputDesc();

			if (!DescOtherOutput && !DescOutput)
			{
				return -1;
			}

			if (DescOutput->mIdentifier.Len() &&
				DescOutput->mIdentifier == DescOtherOutput->mIdentifier)
			{
				return ItOther.GetIndex();
			}
			else if (DescOutput->mLabel.Len() &&
				DescOutput->mLabel == DescOtherOutput->mLabel)
			{
				return ItOther.GetIndex();
			}
			else if (DescOutput->mChannel == DescOtherOutput->mChannel)
			{
				return ItOther.GetIndex();
			}
		}
	}*/
	
	return -1;
}


void TransferOutput(itOutputs_t& ItNew, INT IdxMatch, Outputs_t& PrevOutputs)
{
	itOutputs_t ItPrev = PrevOutputs.itfront()+=IdxMatch;

	UBOOL FormatChanged = FALSE;
	if ((*ItPrev).Format != (*ItNew).Format)
	{
		FormatChanged = TRUE;
	}

	// need to delete the new output's texture
	// do this by disabling it first
	SubstanceAir::Helpers::Disable(&(*ItNew));

	uint_t NewUid = (*ItNew).Uid;
	int NewFormat = (*ItNew).Format;

	*ItNew = *ItPrev;
	(*ItNew).Uid = NewUid;
	(*ItNew).Format = NewFormat;
	(*ItNew).bIsDirty = TRUE;

	// the texture has to be rebuild if the format changed
	if (FormatChanged)
	{
		UTexture2D* Texture = *(*ItNew).Texture.get();
		SubstanceAir::Helpers::CreateTexture2D(&(*ItNew), Texture->GetName());
	}

	PrevOutputs.Remove(IdxMatch);
}


void DeleteOutput(itOutputs_t& ItPrev)
{
	SubstanceAir::Helpers::Disable(&(*ItPrev));
}


void GiveOutputsBack(graph_inst_t* NewInstance, 
	SubstanceAir::List<output_inst_t>& PrevOutputs)
{
	// for each output of the new instance
	// look for a matching previous one
	SubstanceAir::List<output_inst_t> NoMatch;
	itOutputs_t ItNew(NewInstance->Outputs.itfront());
	
	for (;ItNew;++ItNew)
	{
		INT IdxMatch = FindMatchingOutput(ItNew, PrevOutputs);

		if (-1 == IdxMatch)
		{
			NoMatch.push(*ItNew);
		}
		else
		{
			TransferOutput(ItNew, IdxMatch, PrevOutputs);
		}
	}

	SubstanceAir::Helpers::PerformDelayedDeletion();

	itOutputs_t ItPrev(PrevOutputs.itfront());

	while (ItPrev)
	{
		INT IdxMatch = FindMatchingOutput(
			ItPrev, NoMatch, FALSE);

		if (-1 == IdxMatch)
		{
			DeleteOutput(ItPrev);
			PrevOutputs.getArray().Remove(ItPrev.GetIndex());
			ItPrev.Reset();
			continue;
		}
		else
		{
			TransferOutput(ItPrev, IdxMatch, NewInstance->Outputs);
			NoMatch.Remove(IdxMatch);
		}

		ItPrev++;
	}

	SubstanceAir::Helpers::PerformDelayedDeletion();
}


//! @brief Rebuild an instance based on a desc and
//! @note used when the desc has been changed for exampled
graph_inst_t* RebuildInstance(
	graph_desc_t* Desc,
	InstanceBackup& Backup)
{
	check(Desc);
	check(Backup.InstanceParent);

	preset_t& Preset = Backup.Preset;
	USubstanceAirGraphInstance* Parent = Backup.InstanceParent;

	// save the outputs of the previous instance
	SubstanceAir::List<output_inst_t> PrevOutputs = Parent->Instance->Outputs;

	// save the image inputs as they are not part of the preset
	SubstanceAir::List< std::tr1::shared_ptr<input_inst_t> > PrevImageInputs;

	SubstanceAir::List< std::tr1::shared_ptr<input_inst_t> >::TIterator
		ItInput(Parent->Instance->Inputs.itfront());

	for (;ItInput;++ItInput)
	{
		if (FALSE == (*ItInput)->IsNumerical())
		{
			PrevImageInputs.push(*ItInput);
		}
	}

	// empty the previous instance container
	delete Parent->Instance;
	Parent->SetFlags(RF_Standalone); // reset the standalone flag, deleted in the destructor

	Parent->Instance = 0;
	Parent->Parent = 0;

	// recreate an instance
	graph_inst_t* NewInstance = Desc->Instantiate(Parent);
	NewInstance->Desc = Desc;

	// apply the preset
	Preset.Apply(NewInstance);

	GiveOutputsBack(NewInstance, PrevOutputs);
	GiveImageInputsBack(NewInstance, PrevImageInputs);
	Parent->Instance = NewInstance;

	return NewInstance;
}


// look for a desc matching the preset
graph_desc_t* FindMatch(SubstanceAir::List<graph_desc_t*>& Desc,
	const preset_t& Preset)
{
	SubstanceAir::List<graph_desc_t*>::TIterator ItDesc(Desc.itfront());

	for (;ItDesc;++ItDesc)
	{
		if (Preset.mPackageUrl == (*ItDesc)->PackageUrl)
		{
			return *ItDesc;
		}
	}

	ItDesc.Reset();
	for (;ItDesc;++ItDesc)
	{
		// preset label actually are the Instance's Name, there is little
		// chance something will match here
		if (Preset.mLabel == (*ItDesc)->Label)
		{
			return *ItDesc;
		}
	}

	// last attempt, look for a desc with more inputs than the preset,
	// a scenario for reimporting substance is when an input was added
	ItDesc.Reset();
	for (;ItDesc;++ItDesc)
	{
		// preset label actually are the Instance's Name, there is little
		// chance something will match here
		if (Preset.mInputValues.Num() <= (*ItDesc)->InputDescs.Num())
		{
			return *ItDesc;
		}
	}

	return NULL;
}


UReimportSubstanceAirImportFactory::UReimportSubstanceAirImportFactory():
pOriginalSubstance(NULL)
{

}


void UReimportSubstanceAirImportFactory::StaticConstructor()
{
	new( GetClass()->HideCategories ) FName(NAME_Object);
}


void UReimportSubstanceAirImportFactory::InitializeIntrinsicPropertyValues()
{
	SupportedClass	= USubstanceAirInstanceFactory::StaticClass();
	Description		= TEXT("Substance texture package");

	// imports binary data via FactoryCreateBinary
	bText			= FALSE;
	bCreateNew		= FALSE;
	AutoPriority	= -1;
	bEditorImport   = 1;

	// create the default instance by default
	bCreateDefaultInstance = FALSE;
	bCreateMaterial = FALSE;
	bSpecifyInstancePackage = FALSE;
}


UBOOL UReimportSubstanceAirImportFactory::Reimport( UObject* Obj )
{
	if(!Obj || !Obj->IsA(USubstanceAirInstanceFactory::StaticClass()))
	{
		return FALSE;
	}

	pOriginalSubstance = Cast<USubstanceAirInstanceFactory>(Obj);;

	FString SourceFilePath = 
		pOriginalSubstance->SubstancePackage->SourceFilePath;

	if (!(SourceFilePath.Len()))
	{
		return FALSE;
	}

	GWarn->Log( 
		FString::Printf(
			TEXT("Performing atomic reimport of [%s]"),*SourceFilePath) );

	FFileManager::FTimeStamp TS,MyTS;
	if (!GFileManager->GetTimestamp( *SourceFilePath, TS ))
	{
		GWarn->Log( TEXT("-- cannot reimport: source file cannot be found."));

		UFactory* Factory = 
			ConstructObject<UFactory>(
				USubstanceAirImportFactory::StaticClass() );

		UBOOL bNewSourceFound = FALSE;
		FString NewFileName;

		if (ObjectTools::FindFileFromFactory(
				Factory, 
				LocalizeUnrealEd("Import_SourceFileNotFound"), 
				NewFileName))
		{
			SourceFilePath = GFileManager->ConvertToRelativePath(*NewFileName);
			bNewSourceFound = GFileManager->GetTimestamp(*SourceFilePath, TS);
		}

		// If a new source wasn't found or the user canceled out of the dialog,
		// we cannot proceed, but this reimport factory has still technically 
		// "handled" the reimport, so return TRUE instead of FALSE
		if (!bNewSourceFound)
		{
			return TRUE;
		}
	}

	// Pull the timestamp from the user readable string.
	// It would be nice if this was stored directly, and maybe it will be if
	// its decided that UTC dates are too confusing to the users.
	FFileManager::FTimeStamp::FStringToTimestamp(
		pOriginalSubstance->SubstancePackage->SourceFileTimestamp, /*out*/ MyTS);

	if (MyTS < TS)
	{
		GWarn->Log( TEXT("-- File on disk exists and is newer. Attempting re-import."));

		this->bCreateDefaultInstance = FALSE;
		this->bCreateMaterial =	FALSE;
		this->bSpecifyInstancePackage= FALSE;

		const INT InstanceCount = 
				pOriginalSubstance->SubstancePackage->GetInstanceCount();
		const INT LoadedInstanceCount = 
			pOriginalSubstance->SubstancePackage->LoadedInstancesCount;

		// check all instances are loaded
		if (LoadedInstanceCount != InstanceCount)
		{
			appMsgf(AMT_OK,TEXT("Impossible to perform re-import if all instances are not fully loaded."));
			return FALSE;
		}

		// backup the instances values before recreating the desc
		SubstanceAir::List<InstanceBackup> OriginalInstances;
		SubstanceAir::List<graph_desc_t*>::TIterator 
			ItGraph(pOriginalSubstance->SubstancePackage->Graphs.itfront());
		for (; ItGraph ; ++ItGraph)
		{
			SubstanceAir::List<graph_inst_t*>::TIterator 
				ItInst((*ItGraph)->LoadedInstances.itfront());

			for (;ItInst;++ItInst)
			{
				InstanceBackup& Backup = 
					OriginalInstances(OriginalInstances.AddZeroed(1));

				Backup.InstanceParent = (*ItInst)->ParentInstance;
				Backup.Preset.ReadFrom(*ItInst);
			}
		}

		local::bIsPerformingReimport = TRUE;

		USubstanceAirInstanceFactory* NewSubstance =
			Cast<USubstanceAirInstanceFactory>(UFactory::StaticImportObject(
				pOriginalSubstance->GetClass(),
				pOriginalSubstance->GetOuter(),
				*pOriginalSubstance->GetName(),
				RF_Public|RF_Standalone,
				*(SourceFilePath),
				NULL,
				this));

		local::bIsPerformingReimport = FALSE;

		if (NewSubstance)
		{
			SubstanceAir::List<graph_inst_t*> Instances;

			for (SubstanceAir::List<InstanceBackup>::TIterator 
					ItBack(OriginalInstances.itfront()) ; ItBack ; ++ItBack)
			{
				graph_desc_t* DescMatch = 
					FindMatch(
						NewSubstance->SubstancePackage->Graphs,
						(*ItBack).Preset);

				if (!DescMatch)
				{
					//! @todo: cleanup the instance and its container

					FString Name = (*ItBack).Preset.mLabel;

					GWarn->Log(FString::Printf(TEXT("-- no match for the instance %s"), *Name));
					continue;
				}

				Instances.push(RebuildInstance(DescMatch,*ItBack));
			}

			SubstanceAir::Helpers::RenderSync(Instances);
			SubstanceAir::Helpers::UpdateTextures(Instances);

			GCallbackEvent->Send( CALLBACK_ForcePropertyWindowRebuild, this );
			GWarn->Log(TEXT("-- imported successfully"));
		}
		else
		{
			GWarn->Log( TEXT("-- import failed") );
			return FALSE;
		}
	}
	else
	{
		GWarn->Log( TEXT("-- Substance Package already up to date."));
		return FALSE;
	}

	return TRUE;
}

IMPLEMENT_CLASS( USubstanceAirImportFactory )
IMPLEMENT_CLASS( UReimportSubstanceAirImportFactory )
