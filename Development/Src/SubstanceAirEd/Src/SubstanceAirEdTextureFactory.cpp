//! @file SubstanceAirEdTextureFactory.cpp
//! @brief Factory to create Substances
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include <UnrealEd.h>
#include <Factories.h>

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirPackage.h"
#include "SubstanceAirTextureClasses.h"
#include "SubstanceAirInstanceFactoryClasses.h"
#include "SubstanceAirEdHelpers.h"
#include "SubstanceAirHelpers.h"
#include "SubstanceAirUpdater.h"
#include "SubstanceAirRenderingThread.h"
#include "SubstanceAirRenderer.h"

#include "SubstanceAirEdFactoryClasses.h"
#include "SubstanceAirEdPreset.h"


void USubstanceAirTexture2DFactory::StaticConstructor()
{
	new( GetClass(), TEXT( "Create Default Material" ), RF_Public ) 
		UBoolProperty( CPP_PROPERTY( bCreateMaterial ), TEXT( "Substance" ), CPF_Edit );
	new( GetClass(), TEXT( "Create Default Instance" ), RF_Public ) 
		UBoolProperty( CPP_PROPERTY( bCreateDefaultInstance ), TEXT( "Substance" ), CPF_Edit );
	new( GetClass(), TEXT( "Specify Instance Destination" ), RF_Public ) 
		UBoolProperty( CPP_PROPERTY( bSpecifyInstancePackage ), TEXT( "Substance" ), CPF_Edit );

	new( GetClass()->HideCategories ) FName(NAME_Object);
}


void USubstanceAirTexture2DFactory::InitializeIntrinsicPropertyValues()
{
	// the factory builds a USubstanceAirPackage, then this package
	// will in turn create the textures
	SupportedClass	= USubstanceAirInstanceFactory::StaticClass();
	Description		= TEXT("Substance texture package");

	// format of the file to import
	new(Formats)FString(TEXT("sbsar;Texture"));

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


UObject* USubstanceAirTexture2DFactory::FactoryCreateBinary(
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

	// create a Factory for that file
	Factory = CastChecked<USubstanceAirInstanceFactory>(
		StaticConstructObject(
			USubstanceAirInstanceFactory::StaticClass(),
			InParent,
			Name,
			RF_Standalone|RF_Public));

	const INT BufferLength = BufferEnd - Buffer;

	Factory->SubstancePackage = new package_t;
	Factory->SubstancePackage->mParent = Factory;
	Factory->SubstancePackage->mGuid = appCreateGuid();

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
		TArray<graph_inst_t*> Instances =
			SubstanceAir::Helpers::InstantiateGraphs(
				Factory->SubstancePackage->mGraphs,
				!bSpecifyInstancePackage );

		// synchronous rendering
		SubstanceAir::Helpers::PrepareCommands(Instances);
		SubstanceAir::GSubstanceAirUpdater->PushCommands();
		GSubstanceAirRenderCompleteEvent->Wait();
		SubstanceAir::GSubstanceAirUpdater->UpdateTextures();

		// GIsUCC is on when importing in the ImportCommandlet,
		// in that case we always want a material
		if (bCreateMaterial || GIsUCC)
		{
			for (INT Idx = 0 ; Idx < Instances.Num() ; ++Idx)
			{
				FString MatName =
					Factory->SubstancePackage->mGraphs(Idx)->mLabel + "_MAT";

				SubstanceAir::Helpers::CreateMaterial(
					Factory,
					Instances(Idx),
					MatName);
			}
		}
	}

	return Factory;
}


UReimportSubstanceAirTextureFactory::UReimportSubstanceAirTextureFactory():
	pOriginalSubstance(NULL)
{

}


void UReimportSubstanceAirTextureFactory::StaticConstructor()
{
	new( GetClass(), TEXT( "Create Default Material" ), RF_Public ) 
		UBoolProperty( CPP_PROPERTY( bCreateMaterial ), TEXT( "Substance" ), CPF_Edit );
	new( GetClass(), TEXT( "Create Default Instance" ), RF_Public ) 
		UBoolProperty( CPP_PROPERTY( bCreateDefaultInstance ), TEXT( "Substance" ), CPF_Edit );
	new( GetClass(), TEXT( "Specify Instance Destination" ), RF_Public ) 
		UBoolProperty( CPP_PROPERTY( bSpecifyInstancePackage ), TEXT( "Substance" ), CPF_Edit );

	new( GetClass()->HideCategories ) FName(NAME_Object);
}


void UReimportSubstanceAirTextureFactory::InitializeIntrinsicPropertyValues()
{
	// the factory builds a USubstanceAirPackage, then this package
	// will in turn create the textures
	SupportedClass	= USubstanceAirInstanceFactory::StaticClass();
	Description		= TEXT("Substance texture package");

	// format of the file to import
	new(Formats)FString(TEXT("sbsar;Texture"));

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


struct InstanceBackup
{
	preset_t Preset;
	USubstanceAirGraphInstance* InstanceParent;
};


void GiveImageInputsBack(
	graph_inst_t* NewInstance,
	TArray< refcount_ptr<input_inst_t> >& PrevImageInputs)
{
	TArray< refcount_ptr<input_inst_t> >::TIterator ItPrev(PrevImageInputs);

	for (;ItPrev;++ItPrev)
	{
		TArray< refcount_ptr<input_inst_t> >::TIterator ItNew(NewInstance->mInputs);
		refcount_ptr<input_inst_t> InputMatch;

		// by uid
		for (;ItNew;++ItNew)
		{
			if ((*ItNew)->IsNumerical())
			{
				continue;
			}

			if((*ItNew)->mUid == (*ItPrev)->mUid)
			{
				InputMatch = (*ItNew);
				break;
			}
		}

		*InputMatch.Get() = *(*ItPrev).Get();
	}
}


void GiveOutputsBack(graph_inst_t* NewInstance, TArray<output_inst_t>& PrevOutputs)
{
	// for each output of the new instance
	// look for a matching previous one
	TArray<output_inst_t>::TIterator ItNew(NewInstance->mOutputs);

	INT OutputMatchCount = 0;

	for (;ItNew;++ItNew)
	{
		TArray<output_inst_t>::TIterator ItPrev(PrevOutputs);
		output_inst_t* PrevMatch = NULL;

		// first by uid
		for (;ItPrev;++ItPrev)
		{
			if((*ItNew).mUid == (*ItPrev).mUid)
			{
				PrevMatch = &(*ItPrev);
				break;
			}
		}

		//! @todo: need to look for other kind of matches,
		//! name, role...

		if (PrevMatch)
		{
			// there are some special steps when the format
			// has been changed
			if ((*ItPrev).mFormat != (*ItNew).mFormat)
			{
				// NYI
				check(0);
			}

			// need to delete the new output's texture
			// do this by disabling it first
			SubstanceAir::Helpers::Disable(&(*ItNew));
			*ItNew = *ItPrev;

			// means the texture is out of date
			(*ItNew).mTimestamp++;

			++OutputMatchCount;
		}
		else
		{
			// the new output did not match any previous output
			check(0);
		}
	}

	if (OutputMatchCount != PrevOutputs.Num())
	{
		//! @todo: add some fallback match if there are 
		//! some unmatched outputs
		check(0);
	}
}


//! @brief Rebuild an instance based on a desc and
//! @note used when the desc has been changed for exampled
void RebuildInstance(
	graph_desc_t* Desc,
	InstanceBackup& Backup)
{
	check(Desc);
	check(Backup.InstanceParent);

	preset_t& Preset = Backup.Preset;
	USubstanceAirGraphInstance* Parent = Backup.InstanceParent;

	// save the outputs of the previous instance
	TArray<output_inst_t> PrevOutputs = Parent->Instance->mOutputs;

	// save the image inputs as they are not part of the preset
	TArray< refcount_ptr<input_inst_t> > PrevImageInputs;

	TArray< refcount_ptr<input_inst_t> >::TIterator ItInput(Parent->Instance->mInputs);
	for (;ItInput;++ItInput)
	{
		if (FALSE == (*ItInput)->IsNumerical())
		{
			PrevImageInputs.AddItem(*ItInput);
		}
	}

	// empty the previous instance container
	delete Parent->Instance;
	Parent->Instance = 0;
	Parent->Parent = 0;

	// recreate an instance
	graph_inst_t* NewInstance = Desc->Instantiate(Parent);

	// apply the preset
	Preset.Apply(NewInstance);

	GiveOutputsBack(NewInstance, PrevOutputs);
	GiveImageInputsBack(NewInstance, PrevImageInputs);
}


// look for a desc matching the preset
graph_desc_t* FindMatch(TArray<graph_desc_t*>& Desc, const preset_t& Preset)
{
	TArray<graph_desc_t*>::TIterator ItDesc(Desc);

	for (;ItDesc;++ItDesc)
	{
		if (Preset.mPackageUrl == (*ItDesc)->mPackageUrl)
		{
			return *ItDesc;
		}
	}

	ItDesc.Reset();
	for (;ItDesc;++ItDesc)
	{
		// preset label actually are the Instance's Name, there is little
		// chance something will match here
		if (Preset.mLabel == (*ItDesc)->mLabel)
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
		if (Preset.mInputValues.Num() <= (*ItDesc)->mInputDescs.Num())
		{
			return *ItDesc;
		}
	}

	return NULL;
}


UBOOL UReimportSubstanceAirTextureFactory::Reimport( UObject* Obj )
{
	if(!Obj || !Obj->IsA(USubstanceAirInstanceFactory::StaticClass()))
	{
		return FALSE;
	}

	pOriginalSubstance = Cast<USubstanceAirInstanceFactory>(Obj);;

	FString SourceFilePath = 
		pOriginalSubstance->SubstancePackage->mSourceFilePath;

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
				USubstanceAirTexture2DFactory::StaticClass() );

		UBOOL bNewSourceFound = FALSE;
		FString NewFileName;

		if (ObjectTools::FindFileFromFactory (
			Factory, LocalizeUnrealEd("Import_SourceFileNotFound"), NewFileName))
		{
			SourceFilePath = GFileManager->ConvertToRelativePath(*NewFileName);
			bNewSourceFound = GFileManager->GetTimestamp( *SourceFilePath, TS );
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
		pOriginalSubstance->SubstancePackage->mSourceFileTimestamp, /*out*/ MyTS);

	if (MyTS < TS)
	{
		GWarn->Log( TEXT("-- file on disk exists and is newer.  Attempting import."));

		this->bCreateDefaultInstance = FALSE;
		this->bCreateMaterial =	FALSE;
		this->bSpecifyInstancePackage= FALSE;

		const INT InstanceCount = 
			SubstanceAir::Helpers::GetInstanceCount(
				pOriginalSubstance->SubstancePackage);
		const INT LoadedInstanceCount = 
			pOriginalSubstance->SubstancePackage->mLoadedInstancesCount;

		// check all instances are loaded
		if (LoadedInstanceCount != InstanceCount)
		{
			GWarn->Log( TEXT("-- Unable to perform re-import if not all instances are full-loaded."));
			return FALSE;
		}

		// backup the instances values before recreating the desc
		TArray<InstanceBackup> OriginalInstances;
		TArray<graph_desc_t*>::TIterator ItGraph(pOriginalSubstance->SubstancePackage->mGraphs);
		for (; ItGraph ; ++ItGraph)
		{
			TArray<graph_inst_t*>::TIterator ItInst((*ItGraph)->mLoadedInstances);

			for (;ItInst;++ItInst)
			{
				InstanceBackup& Backup = 
					OriginalInstances(OriginalInstances.AddZeroed(1));

				Backup.InstanceParent = (*ItInst)->mParent;
				Backup.Preset.ReadFrom(*ItInst);
			}
		}

		// make sure no rendering is in progress while we are doing this
		//! @todo: we also need to make sure no commands are still pending
		FScopeLock RendererScopeLock(&GSubstanceAirRendererMutex);

		USubstanceAirInstanceFactory* NewSubstance =
			Cast<USubstanceAirInstanceFactory>(UFactory::StaticImportObject(
				pOriginalSubstance->GetClass(),
				pOriginalSubstance->GetOuter(),
				*pOriginalSubstance->GetName(),
				RF_Public|RF_Standalone,
				*(SourceFilePath),
				NULL,
				this));

		if (NewSubstance)
		{

			for (TArray<InstanceBackup>::TIterator 
					ItBack(OriginalInstances) ; ItBack ; ++ItBack)
			{
				graph_desc_t* DescMatch = 
					FindMatch(
						NewSubstance->SubstancePackage->mGraphs,
						(*ItBack).Preset);

				if (!DescMatch)
				{
					//! @todo: cleanup the instance and its container

					FString Name = (*ItBack).Preset.mLabel;

					GWarn->Log(FString::Printf(TEXT("-- no match for the instance %s"), *Name));
					continue;
				}

				RebuildInstance(DescMatch,*ItBack);
			}

			GWarn->Log(TEXT("-- imported successfully"));
		}
		else
		{
			GWarn->Log( TEXT("-- import failed") );
		}
	}
	else
	{
		GWarn->Log( TEXT("-- Substance is already up to date."));
	}

	return TRUE;
}

IMPLEMENT_CLASS( USubstanceAirTexture2DFactory )
IMPLEMENT_CLASS( UReimportSubstanceAirTextureFactory )
