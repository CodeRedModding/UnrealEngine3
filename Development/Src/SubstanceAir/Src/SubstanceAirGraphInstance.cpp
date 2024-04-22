//! @file SubstanceAirGraphInstance.cpp
//! @brief Implementation of the USubstanceAirGraphInstance class
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirOutput.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirPackage.h"
#include "SubstanceAirResource.h"
#include "SubstanceAirTextureClasses.h"
#include "SubstanceAirInstanceFactoryClasses.h"
#include "SubstanceAirHelpers.h"

#if WITH_EDITOR
#include "SubstanceAirEdPreset.h"
#include "SubstanceAirEdHelpers.h"
#endif

IMPLEMENT_CLASS( USubstanceAirGraphInstance )

namespace sbsGraphInstance
{
	TArray<std::tr1::shared_ptr<USubstanceAirTexture2D*>> SavedTextures;
}


void USubstanceAirGraphInstance::InitializeIntrinsicPropertyValues()
{
	Instance = 0;
}


void USubstanceAirGraphInstance::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading() && !Ar.IsTransacting())
	{
		check(NULL == Instance);
		Instance = new SubstanceAir::FGraphInstance();
	}

	if(Instance != NULL)
	{
		Ar << *Instance;
	}
	
	Ar << Parent;

	if (Ar.Ver() < VER_ALG_SBS_INPUT_INDEX)
	{
		// undo/redo now supported
		this->SetFlags(RF_Transactional);
		Instance->bIsFreezed = FALSE;
	}
}


void USubstanceAirGraphInstance::BeginDestroy()
{
	// Route BeginDestroy.
	Super::BeginDestroy();

#if WITH_EDITOR
	if (GIsEditor)
	{
		GCallbackEvent->Send( CALLBACK_ForcePropertyWindowRebuild, this );
	}
#endif

	SubstanceAir::Helpers::UnregisterOutputAsImageInput(this);

	if (Instance)
	{
		SubstanceAir::Helpers::Cleanup(this);
	}
}


void USubstanceAirGraphInstance::PostDuplicate()
{
#if WITH_EDITOR
	// after duplication, we need to recreate a parent instance and set it as outer
	// look for the original object, using the GUID
	USubstanceAirGraphInstance* RefInstance = NULL;

	for (TObjectIterator<USubstanceAirGraphInstance> It; It; ++It)
	{
		if ((*It)->Instance->InstanceGuid == 
			Instance->InstanceGuid && (*It)->Instance != this->Instance)
		{
			RefInstance = *It;
			break;
		}
	}

	check(RefInstance);

	// create a new instance 
	UBOOL bCreateOutputs = FALSE;
	UBOOL bSubscribeInstance = FALSE;

	Instance = NULL;
	Instance = RefInstance->Instance->Desc->Instantiate(
		this, bCreateOutputs, bSubscribeInstance);

	SubstanceAir::Helpers::CopyInstance(RefInstance->Instance, Instance);
	SubstanceAir::Helpers::FlagRefreshContentBrowser(Instance->Outputs.Num());
	SubstanceAir::Helpers::RenderAsync(Instance);
#endif

	Super::PostDuplicate();
}


void USubstanceAirGraphInstance::PostLoad()
{
	// subscribe to the parent object
	if(!Parent || !Parent->SubstancePackage)
	{
		appDebugMessagef(
				*FString::Printf(
				TEXT("Error, Impossible to find parent package. You should delete the object: %s"), 
				*GetFullName()));

		Super::PostLoad();
		return;
	}

	// sanity check
	if(!Instance)
	{
		appDebugMessagef(
			*FString::Printf(
			TEXT("Error, no actual instance associated to the object. You should delete: %s"), 
			*GetFullName()));

		Super::PostLoad();
		return;
	}

	// link the instance to its desc and parent
	Instance->ParentInstance = this;
	Instance->Desc = SubstanceAir::Helpers::FindParentGraph(
		Parent->SubstancePackage->Graphs,
		Instance);

	// if nothing was found, we lookup by url and bind the object to the factory
	// this can happen when the factory and the instance are not in the same
	// package and this factory's package was not saved after instancing
	if(NULL == Instance->Desc)
	{
		Instance->Desc = SubstanceAir::Helpers::FindParentGraph(
			Parent->SubstancePackage->Graphs,
			Instance->ParentUrl);
		Instance->Desc->InstanceUids.AddUniqueItem(
			Instance->InstanceGuid); // do this manually in that case
	}

	// could be broken if instance and package are in different package not saved synchronously
	if (NULL == Instance->Desc)
	{
		appDebugMessagef(
			*FString::Printf(
			TEXT("Error, Impossible to find parent desc, the package seems broken. You should delete \"%s\"."), 
			*GetFullName()));

		Super::PostLoad();
		return;
	}

	Instance->Desc->Subscribe(Instance);

	// link the outputs with the graph instance
	TArray<output_inst_t>::TIterator ItOut(Instance->Outputs.itfront());
	for (; ItOut ; ++ItOut)
	{
		ItOut->ParentInstance = this;
	}

	// and the input instances with the graph and their desc
	TArray<std::tr1::shared_ptr<input_inst_t>>::TIterator ItInInst(Instance->Inputs.itfront());
	for (; ItInInst ; ++ItInInst)
	{
		TArray<input_desc_ptr>::TIterator InIntDesc(Instance->Desc->InputDescs.itfront());
		for (; InIntDesc ; ++InIntDesc)
		{
			if ((*InIntDesc)->Uid == (*ItInInst)->Uid)
			{
				ItInInst->get()->Desc = InIntDesc->get();
			}
		}

		if (ItInInst->get()->Desc && ItInInst->get()->Desc->IsImage())
		{
			Instance->bHasPendingImageInputRendering = TRUE;

			img_input_inst_t* ImgInput = (img_input_inst_t*)(ItInInst->get());
			if (ImgInput->ImageSource)
			{
				ImgInput->ImageSource->ConditionalPostLoad();

				// delay the set image input, the source is not necessarily ready
				SubstanceAir::Helpers::PushDelayedImageInput(ImgInput, Instance);
			}
		}

		(*ItInInst)->Parent = Instance;
	}

	if (Instance->bHasPendingImageInputRendering)
	{
		SubstanceAir::Helpers::RenderPush(Instance);
	}

	// in seekfree loading mode, textures could be missing (deleted during cooking etc.)
	// disable the outputs and let their textures switch them back on
	if (Instance && GUseSeekFreeLoading)
	{
		ItOut.Reset();

		for (; ItOut ; ++ItOut)
		{
			if ( *(ItOut->Texture.get()) == NULL)
			{
				ItOut->bIsEnabled = FALSE;
			}
		}
	}

	Super::PostLoad();
}


void USubstanceAirGraphInstance::PreEditUndo()
{
	// serialization of outputs does not include the shared ptr to textures,
	// save them before undo/redo

	SubstanceAir::List<output_inst_t>::TIterator itOut(Instance->Outputs.itfront());
	for (;itOut;++itOut)
	{
		sbsGraphInstance::SavedTextures.AddItem(itOut->Texture);
	}
}


void USubstanceAirGraphInstance::PostEditUndo()
{
	SubstanceAir::List<output_inst_t>::TIterator itOut(Instance->Outputs.itfront());
	TArray<std::tr1::shared_ptr<USubstanceAirTexture2D*>>::TIterator SavedTexturesIt(sbsGraphInstance::SavedTextures);

	for (;itOut && SavedTexturesIt;++itOut, ++SavedTexturesIt)
	{
		itOut->Texture = std::tr1::shared_ptr<USubstanceAirTexture2D*>(*SavedTexturesIt);
	}

	sbsGraphInstance::SavedTextures.Empty();

	SubstanceAir::Helpers::RenderAsync(Instance);

#if WITH_EDITOR
	if (GIsEditor)
	{
		GCallbackEvent->Send(CALLBACK_ObjectPropertyChanged, this);
	}
#endif
}


UBOOL USubstanceAirGraphInstance::SetInputInt(const FString& Name, const TArray<INT>& Value)
{
	if (Instance)
	{
		Instance->UpdateInput< INT >(
			Name,
			Value);

		SubstanceAir::Helpers::RenderAsync(Instance);
		return TRUE;
	}

	return FALSE;
}


UBOOL USubstanceAirGraphInstance::SetInputFloat(const FString& Name, const TArray<FLOAT>& Value)
{
	if (Instance)
	{
		Instance->UpdateInput< FLOAT >(
			Name,
			Value);

		SubstanceAir::Helpers::RenderAsync(Instance);
		return TRUE;
	}

	return FALSE;
}


UBOOL USubstanceAirGraphInstance::SetInputImg(const FString& Name, class UObject* Value)
{
	if (Instance)
	{
		if (SubstanceAir::Helpers::IsSupportedImageInput(Value))
		{
			Instance->UpdateInput(Name, Value);
			SubstanceAir::Helpers::RenderAsync(Instance);
			return TRUE;
		}
	}

	return FALSE;
}


TArray<FString> USubstanceAirGraphInstance::GetInputNames()
{
	TArray<FString> Names;

	TArray<std::tr1::shared_ptr<input_inst_t>>::TIterator ItInInst(Instance->Inputs.itfront());
	for (; ItInInst ; ++ItInInst)
	{
		if (ItInInst->get()->Desc)
		{
			Names.AddItem(ItInInst->get()->Desc->Identifier);
		}
	}

	return Names;
}


BYTE USubstanceAirGraphInstance::GetInputType(const FString& Name)
{
	TArray<std::tr1::shared_ptr<input_inst_t>>::TIterator ItInInst(Instance->Inputs.itfront());
	for (; ItInInst ; ++ItInInst)
	{
		if (ItInInst->get()->Desc->Identifier == Name)
		{
			return (BYTE)(ItInInst->get()->Desc->Type);
		}
	}

	return (BYTE)SIT_MAX;
}


TArray<INT> USubstanceAirGraphInstance::GetInputInt(const FString& Name)
{
	TArray<INT> DummyValue;

	TArray<std::tr1::shared_ptr<input_inst_t>>::TIterator ItInInst(Instance->Inputs.itfront());
	for (; ItInInst ; ++ItInInst)
	{
		if (ItInInst->get()->Desc->Identifier == Name &&
			(ItInInst->get()->Desc->Type == Substance_IType_Integer ||
		     ItInInst->get()->Desc->Type == Substance_IType_Integer2 ||
			 ItInInst->get()->Desc->Type == Substance_IType_Integer3 ||
			 ItInInst->get()->Desc->Type == Substance_IType_Integer4))
		{
			return SubstanceAir::Helpers::GetValueInt(*ItInInst);
		}
	}

	return DummyValue;
}


TArray<FLOAT> USubstanceAirGraphInstance::GetInputFloat(const FString& Name)
{
	TArray<FLOAT> DummyValue;

	TArray<std::tr1::shared_ptr<input_inst_t>>::TIterator ItInInst(Instance->Inputs.itfront());
	for (; ItInInst ; ++ItInInst)
	{
		if (ItInInst->get()->Desc->Identifier == Name &&
			(ItInInst->get()->Desc->Type == Substance_IType_Float ||
			ItInInst->get()->Desc->Type == Substance_IType_Float2 ||
			ItInInst->get()->Desc->Type == Substance_IType_Float3 ||
			ItInInst->get()->Desc->Type == Substance_IType_Float4))
		{
			return SubstanceAir::Helpers::GetValueFloat(*ItInInst);
		}
	}

	return DummyValue;
}


class UObject* USubstanceAirGraphInstance::GetInputImg(const FString& Name)
{
	TArray<std::tr1::shared_ptr<input_inst_t>>::TIterator ItInInst(Instance->Inputs.itfront());
	for (; ItInInst ; ++ItInInst)
	{
		if (ItInInst->get()->Desc->Identifier == Name &&
			ItInInst->get()->Desc->Type == Substance_IType_Image)
		{
			SubstanceAir::FImageInputInstance* TypedInst =
				(SubstanceAir::FImageInputInstance*) &(*ItInInst->get());

			return TypedInst->ImageSource;
		}
	}

	return NULL;
}
