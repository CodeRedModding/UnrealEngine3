//! @file SubstanceAirStructures.cpp
//! @author Antoine Gonzalez - Allegorithmic
//! @date 20110105
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirPackage.h"
#include "SubstanceAirInstanceFactoryClasses.h"
#include "SubstanceAirHelpers.h"
#include "SubstanceAirInput.h"
#include "SubstanceAirImageInputClasses.h"

#pragma pack ( push, 8 )
#include <substance/substance.h>
#pragma pack ( pop )

#include <algorithm>

#include <ScopedTransaction.h>

namespace SubstanceAir
{

FGraphDesc::~FGraphDesc()
{
	OutputDescs.Empty();
	InputDescs.Empty();

	SubstanceAir::List<graph_inst_t*> InstancesToDelete(LoadedInstances);

	// the graph can be deleted before their instance
	SubstanceAir::List<graph_inst_t*>::TIterator ItInst(InstancesToDelete.itfront());

	for (;ItInst;++ItInst)
	{
		UnSubscribe(*ItInst);
	}
	
	Parent = NULL;
}


input_hash_t FGraphDesc::getDefaultHeavyInputHash() const
{
	input_hash_t Hash;

	for (UINT Idx=0 ; Idx<InputDescs.size() ; ++Idx)
	{
		if (InputDescs(Idx)->IsHeavyDuty)
		{
			switch((SubstanceInputType)InputDescs(Idx)->Type)
			{
			case Substance_IType_Float:
				{
					FNumericalInputDesc<float_t>* Input = 
						(FNumericalInputDesc<float_t>*)InputDescs(Idx).get();

					Hash += FString::Printf(
						TEXT("%f;"), 
						Input->DefaultValue);
				}
				break;
			case Substance_IType_Float2:
				{
					FNumericalInputDesc<vec2float_t>* Input = 
						(FNumericalInputDesc<vec2float_t>*)InputDescs(Idx).get();

					Hash += FString::Printf(
						TEXT("%f%f;"),
						Input->DefaultValue.X,
						Input->DefaultValue.Y);
				}
				break;
			case Substance_IType_Float3:
				{
					FNumericalInputDesc<vec3float_t>* Input = 
						(FNumericalInputDesc<vec3float_t>*)InputDescs(Idx).get();

					Hash += FString::Printf(
						TEXT("%f%f%f;"),
						Input->DefaultValue.X,
						Input->DefaultValue.Y,
						Input->DefaultValue.Z);
				}
				break;
			case Substance_IType_Float4:
				{
					FNumericalInputDesc<vec4float_t>* Input = 
						(FNumericalInputDesc<vec4float_t>*)InputDescs(Idx).get();

					Hash += FString::Printf(
						TEXT("%f%f%f%f;"),
						Input->DefaultValue.X,
						Input->DefaultValue.Y,
						Input->DefaultValue.Z,
						Input->DefaultValue.W);
				}
				break;
			case Substance_IType_Integer:
				{
					FNumericalInputDesc<int_t>* Input = 
						(FNumericalInputDesc<int_t>*)InputDescs(Idx).get();

					Hash += FString::Printf(
						TEXT("%d;"), 
						(INT)Input->DefaultValue);
				}
				break;
			case Substance_IType_Integer2:
				{
					FNumericalInputDesc<vec2int_t>* Input = 
						(FNumericalInputDesc<vec2int_t>*)InputDescs(Idx).get();

					Hash += FString::Printf(
						TEXT("%d%d;"),
						(INT)Input->DefaultValue.X,
						(INT)Input->DefaultValue.Y);
				}
				break;
			case Substance_IType_Integer3:
				{
					FNumericalInputDesc<vec3int_t>* Input = 
						(FNumericalInputDesc<vec3int_t>*)InputDescs(Idx).get();

					Hash += FString::Printf(
						TEXT("%d%d%d;"),
						(INT)Input->DefaultValue.X,
						(INT)Input->DefaultValue.Y,
						(INT)Input->DefaultValue.Z);
				}
				break;
			case Substance_IType_Integer4:
				{
					FNumericalInputDesc<vec4int_t>* Input = 
						(FNumericalInputDesc<vec4int_t>*)InputDescs(Idx).get();

					Hash += FString::Printf(
						TEXT("%d%d%d%d;"),
						(INT)Input->DefaultValue.X,
						(INT)Input->DefaultValue.Y,
						(INT)Input->DefaultValue.Z,
						(INT)Input->DefaultValue.W);
				}
				break;
			case Substance_IType_Image:
				{
					FImageInputInstance* Input = 
						(FImageInputInstance*)InputDescs(Idx).get();

					if (Input->ImageSource != NULL && Input->ImageSource->IsPendingKill())
					{
						Hash += Input->ImageSource->GetFullName();
					}
					else
					{
						Hash += TEXT("NoImage;");
					}
				}
				break;
			default:
				break;
			}
		}
	}

	return Hash;
}


FOutputDesc* FGraphDesc::GetOutputDesc(const uint_t Uid)
{
	for (UINT IdxOut=0 ; IdxOut<OutputDescs.size() ; ++IdxOut)
	{
		if (Uid == OutputDescs(IdxOut).Uid)
		{
			return &OutputDescs(IdxOut);
		}
	}
	
	return NULL;
}


input_desc_ptr FGraphDesc::GetInputDesc(const uint_t Uid)
{
	SubstanceAir::List<input_desc_ptr>::TIterator 
		ItIn(InputDescs.itfront());

	for (; ItIn ; ++ItIn)
	{
		if (Uid == (*ItIn)->Uid)
		{
			return *ItIn;
		}
	}

	return input_desc_ptr(NULL);
}


graph_inst_t* FGraphDesc::Instantiate(
	USubstanceAirGraphInstance* Parent, 
	UBOOL bCreateOutputs,
	UBOOL bSubscribeInstance)
{
	graph_inst_t* NewInstance = new graph_inst_t(this, Parent);

	// register the new instance to the desc
	InstanceUids.AddUniqueItem(NewInstance->InstanceGuid);

	if (bCreateOutputs)
	{
		Helpers::CreateTextures(NewInstance);
	}

	if (bSubscribeInstance)
	{
		Subscribe(NewInstance);
	}

	return NewInstance;
}


void FGraphDesc::Subscribe(graph_inst_t* Inst)
{
	INT DummyIdx = INDEX_NONE;
	check(FALSE == LoadedInstances.FindItem(Inst, DummyIdx));
	check(INDEX_NONE == DummyIdx);

	LoadedInstances.push(Inst);

	if (InstanceUids.FindItemIndex(Inst->InstanceGuid) == INDEX_NONE)
	{
		appDebugMessagef(TEXT("#air: it seems that the instance has not been properly registered to the package, your data might be corrupted."));
		InstanceUids.AddUniqueItem(Inst->InstanceGuid);
	}

	Parent->InstanceSubscribed();
}


void FGraphDesc::UnSubscribe(graph_inst_t* Inst)
{
	check(this == Inst->Desc);

	LoadedInstances.RemoveItem(Inst);
	Parent->InstanceUnSubscribed();
	Inst->Desc = 0;

	// flag the instance container for deletion if it still exists
	if (Inst->ParentInstance)
	{
		Inst->ParentInstance->ClearFlags(RF_Standalone);
	}
}


template< typename T > std::tr1::shared_ptr<input_inst_t> copyInputInstance(
	std::tr1::shared_ptr<input_inst_t> Input)
{
	std::tr1::shared_ptr<input_inst_t> Instance = 
		std::tr1::shared_ptr<input_inst_t>(new FNumericalInputInstance<T>);

	*(FNumericalInputInstance<T>*)Instance.Get() =
		*(FNumericalInputInstance<T>*)Input.Get();

	return Instance;
}


UBOOL SetImageInputHelper(
	List<input_desc_ptr>::TIterator &ItIn,
	UObject* InValue,
	FGraphInstance* Instance) 
{	
	int_t ModifiedOuputs = 0;
	input_desc_t* InputDesc = (*ItIn).get();
	input_inst_t* InputInst = Instance->Inputs[ItIn.GetIndex()].get();

	// instances and descriptions should be stored in the same order
	check(InputDesc->Uid == InputInst->Uid);
	check(FALSE==InputInst->IsNumerical());

	static_cast<img_input_inst_t*>(InputInst)->SetImageInput(InValue, Instance);

	if (!Instance->bHasPendingImageInputRendering)
	{
		return 0;
	}

	for (UINT Idx=0 ; Idx<InputDesc->AlteredOutputUids.size() ; ++Idx)
	{
		output_inst_t* OutputModified = Instance->GetOutput(InputDesc->AlteredOutputUids(Idx));

		if (OutputModified && OutputModified->bIsEnabled)
		{
			OutputModified->flagAsDirty();
			++ModifiedOuputs;
		}
	}

	if (ModifiedOuputs)
	{
		Instance->ParentInstance->MarkPackageDirty();
		return ModifiedOuputs;
	}

	return 0;
}


FGraphInstance::FGraphInstance(
	FGraphDesc* GraphDesc,
	USubstanceAirGraphInstance* Parent):
		Desc(GraphDesc),
		ParentInstance(Parent),
		bIsFreezed(TRUE), 
		bIsBaked(FALSE),
		ParentUrl(GraphDesc->PackageUrl)
{
	check(ParentInstance);
	check(GraphDesc);
	check(GraphDesc->Parent);
	check(ParentInstance->Instance == 0);

	for (UINT Idx=0 ; Idx<GraphDesc->OutputDescs.size() ; ++Idx)
	{
		Outputs.push(GraphDesc->OutputDescs(Idx).Instantiate());
	}

	// link the outputs with the graph instance
	SubstanceAir::List<output_inst_t>::TIterator ItOut(Outputs.itfront());
	for (; ItOut ; ++ItOut)
	{
		ItOut->ParentInstance = ParentInstance;
	}

	for (UINT Idx=0 ; Idx<GraphDesc->InputDescs.size() ; ++Idx)
	{
		Inputs.push(
			std::tr1::shared_ptr<input_inst_t>(
			GraphDesc->InputDescs(Idx)->Instantiate()));
		Inputs.Last().get()->Parent = this;
	}

	InstanceGuid = appCreateGuid();
	ParentInstance->Instance = this;
	ParentInstance->Parent = GraphDesc->Parent->Parent;
}


FGraphInstance::~FGraphInstance()
{
	Outputs.Empty();
	Inputs.Empty();

	Desc = NULL;

	// Notify all renderers that this instance will be deleted
	SBS_VECTOR_FOREACH (Details::States* states,States)
	{
		states->notifyDeleted(InstanceGuid);
	}
}


int_t FGraphInstance::UpdateInput(
	const uint_t& Uid,
	class UObject* InValue)
{
	graph_desc_t* ParentGraph = Outputs(0).GetParentGraph();
	List<input_desc_ptr>::TIterator 
		ItIn(ParentGraph->InputDescs.itfront());

	int_t ModifiedOuputs = 0;

	for ( ; ItIn ; ++ItIn)
	{
		if ((*ItIn)->Uid == Uid)
		{
			ModifiedOuputs += SetImageInputHelper(ItIn, InValue, this);
		}
	}

	return ModifiedOuputs;
}


int_t FGraphInstance::UpdateInput(
	const FString& ParameterName,
	class UObject* InValue)
{
	graph_desc_t* ParentGraph = Outputs[0].GetParentGraph();
	List<input_desc_ptr>::TIterator 
		ItIn(ParentGraph->InputDescs.itfront());

	int_t ModifiedOuputs = 0;

	for ( ; ItIn ; ++ItIn)
	{
		if ((*ItIn)->Identifier == ParameterName)
		{
			ModifiedOuputs += SetImageInputHelper(ItIn, InValue, this);
		}
	}

	return ModifiedOuputs;
}

	
output_inst_t* FGraphInstance::GetOutput(const uint_t Uid)
{
	for (int_t Idx=0 ; Idx<Outputs.Num() ; ++Idx)
	{
		if (Uid == Outputs(Idx).Uid)
		{
			return &Outputs(Idx);
		}
	}
	return NULL;
}


input_inst_t* FGraphInstance::GetInput(const FString& Name)
{
	for (int_t Idx=0 ; Idx<Inputs.Num() ; ++Idx)
	{
		if (Name == Inputs(Idx)->Desc->Identifier)
		{
			return Inputs(Idx).get();
		}
	}
	return NULL;
}


input_inst_t* FGraphInstance::GetInput(const uint_t Uid)
{
	for (int_t Idx=0 ; Idx<Inputs.Num() ; ++Idx)
	{
		if (Uid == Inputs(Idx)->Uid)
		{
			return Inputs(Idx).get();
		}
	}
	return NULL;
}


void FGraphInstance::plugState(Details::States* states)
{
	check(std::find(States.begin(),States.end(),states)==States.end());
	States.push_back(states);
}


void FGraphInstance::unplugState(Details::States* states)    
{
	if (States.size())
	{
		States_t::iterator ite = std::find(States.begin(),States.end(),states);
		check(ite!=States.end());
		States.erase(ite);
	}
}

} // namespace SubstanceAir
