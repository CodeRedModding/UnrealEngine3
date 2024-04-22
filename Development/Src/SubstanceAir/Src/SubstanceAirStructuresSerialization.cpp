//! @file SubstanceAirStructuresSerialization.cpp
//! @author Antoine Gonzalez - Allegorithmic
//! @date 20110105
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirPackage.h"
#include "SubstanceAirInput.h"
#include "SubstanceAirImageInputClasses.h"
#include "SubstanceAirHelpers.h"

#include "framework/details/detailslinkdata.h"

#pragma pack ( push, 8 )
#include <substance/substance.h>
#pragma pack ( pop )

using SubstanceAir::FNumericalInputInstance;
using SubstanceAir::FNumericalInputDesc;

FArchive& operator<<(FArchive& Ar, SubstanceAir::FPackage*& P)
{
	Ar << P->SubstanceUids << P->Guid;
	Ar << P->SourceFilePath << P->SourceFileTimestamp;

	TArray<BYTE> arArchive;
	if (Ar.IsLoading())
	{
		arArchive.BulkSerialize(Ar);
		P->LinkData.reset(
			new SubstanceAir::Details::LinkDataAssembly(
				&arArchive(0),
				arArchive.Num()));
	}
	else if(Ar.IsSaving())
	{
		SubstanceAir::Details::LinkDataAssembly *linkdata = 
			static_cast<SubstanceAir::Details::LinkDataAssembly*>(
				P->LinkData.get());

		arArchive.AddZeroed(linkdata->getAssembly().size());
		appMemcpy(
			&arArchive(0),
			(void*)&linkdata->getAssembly()[0],
			linkdata->getAssembly().size());

		arArchive.BulkSerialize(Ar);
	}

	INT GraphCount = 0;

	if (Ar.IsSaving())
	{
		GraphCount =  P->Graphs.Num();
	}
	
	Ar << GraphCount;

	if (Ar.IsLoading())
	{
		P->Graphs.AddZeroed(GraphCount);
	}

	for (int_t Idx=0 ; Idx<GraphCount ; ++Idx)
	{
		Ar << P->Graphs(Idx);

		if (Ar.IsLoading())
		{
			P->Graphs(Idx)->Parent = P;

			SubstanceAir::List<output_desc_t>::TIterator itO(P->Graphs(Idx)->OutputDescs.itfront());
			for (; itO ; ++itO)
			{
				SubstanceAir::Details::LinkDataAssembly *linkdata = 
					(SubstanceAir::Details::LinkDataAssembly *)P->LinkData.get();

				linkdata->setOutputFormat(
					(*itO).Uid,
					(*itO).Format);
			}
		}
	}

	return Ar;
}


template <class T> void serialize(
	FArchive& Ar,
	std::tr1::shared_ptr< SubstanceAir::FInputDescBase >& I)
{
	FNumericalInputDesc<T>* Input = 
		(FNumericalInputDesc<T>*)I.get();

	// replace combobox by special struct
	if (I->Widget == SubstanceAir::Input_Combobox)
	{
		// when loading, the refcount_ptr<input_desc_t> has to be rebuilt
		// with the special type FNumericalInputDescComboBox
		if (Ar.IsLoading())
		{
			input_desc_ptr NewI(
				new SubstanceAir::FNumericalInputDescComboBox(
				(FNumericalInputDesc<int_t>*)Input));		
			I = NewI;

			// and the Input ptr updated !
			Input = (FNumericalInputDesc<T>*)I.get();
		}

		SubstanceAir::FNumericalInputDescComboBox* InputComboBox = 
			(SubstanceAir::FNumericalInputDescComboBox*)I.get();
		Ar<<InputComboBox->ValueText;
	}

	INT Clamped = Input->IsClamped;
	Ar<<Clamped;
	Input->IsClamped = Clamped;

	Ar<<Input->DefaultValue;
	Ar<<Input->Min;
	Ar<<Input->Max;
	Ar<<Input->Group;
}


FArchive& operator<<(
	FArchive& Ar,
	std::tr1::shared_ptr< SubstanceAir::FInputDescBase >& I)
{
	Ar<<I->Identifier<<I->Label;
	Ar<<I->Uid<<I->Type;

	if (Ar.Ver() >= VER_ALG_SBS_INPUT_INDEX)
	{
		Ar << I->Index;
	}

	I->AlteredOutputUids.BulkSerialize( Ar );

	INT UseHints = 0;
	INT HeavyDuty = I->IsHeavyDuty;
	INT Widget = I->Widget;

	Ar<<UseHints<<HeavyDuty<<Widget;
	I->IsHeavyDuty = HeavyDuty;
	I->Widget = (SubstanceAir::InputWidget)Widget;

	switch((SubstanceInputType)I->Type)
	{
	case Substance_IType_Float:
		{
			serialize<float_t>(Ar, I);
		}
		break;
	case Substance_IType_Float2:
		{
			serialize<vec2float_t>(Ar, I);
		}
		break;
	case Substance_IType_Float3:
		{
			serialize<vec3float_t>(Ar, I);
		}
		break;
	case Substance_IType_Float4:
		{
			serialize<vec4float_t>(Ar, I);
		}
		break;
	case Substance_IType_Integer:
		{
			serialize<int_t>(Ar, I);
		}
		break;
	case Substance_IType_Integer2:
		{
			serialize<vec2int_t>(Ar, I);
		}
		break;
	case Substance_IType_Integer3:
		{
			serialize<vec3int_t>(Ar, I);
		}
		break;
	case Substance_IType_Integer4:
		{
			serialize<vec4int_t>(Ar, I);
		}
		break;

	case Substance_IType_Image:
		{
			SubstanceAir::FImageInputDesc* ImgInput =
				(SubstanceAir::FImageInputDesc*)I.get();

			Ar << ImgInput->Desc << ImgInput->Label ;

			INT Usage = ImgInput->Usage;

			Ar << Usage;

			ImgInput->Usage = (SubstanceAir::ChannelUse) Usage;
		}
		break;
	}

	return Ar;
}


FArchive& operator<<(FArchive& Ar, SubstanceAir::FOutputDesc& O)
{
	Ar << O.Identifier << O.Label << O.Uid << O.Format << O.Channel;
	O.AlteringInputUids.BulkSerialize( Ar );

	return Ar;
}


FArchive& operator<<(FArchive& Ar, SubstanceAir::FGraphInstance& G)
{
	if (Ar.IsTransacting())
	{
		for (INT Idx=0 ; Idx<G.Inputs.Num() ; ++Idx)
		{
			Ar << G.Inputs(Idx);
		}	

		Ar << G.Outputs.getArray();

		return Ar;
	}
	
	if (Ar.IsSaving())
	{
		INT Count = G.Inputs.Num();
		Ar << Count;

		for (INT Idx=0 ; Idx<G.Inputs.Num() ; ++Idx)
		{
			// start by the type to be able to allocate 
			// the good type when loading back in memory
			Ar << G.Inputs(Idx)->Type;
			Ar << G.Inputs(Idx);
		}

		Count = G.Outputs.Num();
		Ar << Count;
		Ar << G.Outputs.getArray();
	}
	else if (Ar.IsLoading())
	{
		INT Count;
		Ar << Count;
		
		G.Inputs.AddZeroed(Count);

		for (INT Idx=0 ; Idx<G.Inputs.Num() ; ++Idx)
		{
			INT Type;
			Ar << Type;

			switch((SubstanceInputType)Type)
			{
			case Substance_IType_Float:
				G.Inputs(Idx) = std::tr1::shared_ptr<input_inst_t>(
					new FNumericalInputInstance<float_t>());
				break;
			case Substance_IType_Float2:
				G.Inputs(Idx) = std::tr1::shared_ptr<input_inst_t>(
					new FNumericalInputInstance<vec2float_t>());
				break;
			case Substance_IType_Float3:
				G.Inputs(Idx) = std::tr1::shared_ptr<input_inst_t>(
					new FNumericalInputInstance<vec3float_t>());
				break;
			case Substance_IType_Float4:
				G.Inputs(Idx) = std::tr1::shared_ptr<input_inst_t>(
					new FNumericalInputInstance<vec4float_t>());
				break;
			case Substance_IType_Integer:
				G.Inputs(Idx) = std::tr1::shared_ptr<input_inst_t>(
					new FNumericalInputInstance<int_t>());
				break;
			case Substance_IType_Integer2:
				G.Inputs(Idx) = std::tr1::shared_ptr<input_inst_t>(
					new FNumericalInputInstance<vec2int_t>());
				break;
			case Substance_IType_Integer3:
				G.Inputs(Idx) = std::tr1::shared_ptr<input_inst_t>(
					new FNumericalInputInstance<vec3int_t>());
				break;
			case Substance_IType_Integer4:
				G.Inputs(Idx) = std::tr1::shared_ptr<input_inst_t>(
					new FNumericalInputInstance<vec4int_t>());
				break;
			case Substance_IType_Image:
				G.Inputs(Idx) = std::tr1::shared_ptr<input_inst_t>(
					new SubstanceAir::FImageInputInstance());
				break;
			default:
				break;
			}

			// the type is known so it will
			// serialize the good amount of data
			Ar<<G.Inputs(Idx);
		}

		Ar << Count;
		G.Outputs.AddZeroed(Count);
		Ar << G.Outputs.getArray();
	}
	else
	{
		for (INT Idx=0 ; Idx<G.Inputs.Num() ; ++Idx)
		{
			Ar<<G.Inputs(Idx);
		}
	}

	Ar << G.InstanceGuid;
	Ar << G.ParentUrl;

	INT DummyBool = G.bIsBaked;
	Ar << DummyBool;
	G.bIsBaked = DummyBool;

	DummyBool = G.bIsFreezed;
	Ar << DummyBool;
	G.bIsFreezed = DummyBool;

	if (!Ar.IsSaving() && !Ar.IsLoading())
	{
		Ar << G.ParentInstance; // to make sure it does not get garbage collected
	}

	return Ar;
}


FArchive& operator<<(FArchive& Ar, SubstanceAir::FOutputInstance& O)
{
	Ar << O.Uid << O.Format << O.OutputGuid;

	INT IsEnabled = O.bIsEnabled;
	Ar << IsEnabled;
	O.bIsEnabled = IsEnabled;
	
	return Ar;
}


FArchive& operator<<(FArchive& Ar, SubstanceAir::FImageInputInstance* Instance)
{
	if (Ar.IsTransacting())
	{
		Ar << Instance->ImageSource;

		if (Ar.IsLoading())
		{
			Instance->SetImageInput(Instance->ImageSource, Instance->Parent, FALSE, TRUE);
		}

		return Ar;
	}

	if (!Ar.IsSaving() && !Ar.IsLoading())
	{
		Ar << Instance->ImageSource;
	}
	else
	{
		INT bHasImage = 0;

	if (Ar.IsSaving() && Instance->ImageSource != NULL)
	{
		bHasImage = 1;
	}
			
	Ar << bHasImage;

	if (bHasImage)
	{
		Ar << Instance->ImageSource;

			if (Ar.IsLoading() && !Ar.IsTransacting())
			{
				USubstanceAirImageInput* BmpImageInput = 
					Cast<USubstanceAirImageInput>(Instance->ImageSource);

				if (BmpImageInput)
				{	// link the image input with the input
					SubstanceAir::Helpers::LinkImageInput(BmpImageInput, Instance);
				}
			}
		}
	}

	return Ar;
}


FArchive& operator<<(FArchive& Ar, std::tr1::shared_ptr<SubstanceAir::FInputInstanceBase>& I)
{
	INT HeavyDuty = I->IsHeavyDuty;
	Ar << I->Uid << I->Type << HeavyDuty;
	I->IsHeavyDuty = HeavyDuty;
	
	switch((SubstanceInputType)I->Type)
	{
	case Substance_IType_Float:
		{
			FNumericalInputInstance<float_t>* Instance = (FNumericalInputInstance<float_t>*)I.get();
			Ar << Instance->Value;
		}
		break;
	case Substance_IType_Float2:
		{
			FNumericalInputInstance<vec2float_t>* Instance = (FNumericalInputInstance<vec2float_t>*)I.get();
			Ar << Instance->Value;
		}
		break;
	case Substance_IType_Float3:
		{
			FNumericalInputInstance<vec3float_t>* Instance = (FNumericalInputInstance<vec3float_t>*)I.get();
			Ar << Instance->Value;
		}
		break;
	case Substance_IType_Float4:
		{
			FNumericalInputInstance<vec4float_t>* Instance = (FNumericalInputInstance<vec4float_t>*)I.get();
			Ar << Instance->Value;
		}
		break;
	case Substance_IType_Integer:
		{
			FNumericalInputInstance<int_t>* Instance = (FNumericalInputInstance<int_t>*)I.get();
			Ar << Instance->Value;
		}
		break;
	case Substance_IType_Integer2:
		{
			FNumericalInputInstance<vec2int_t>* Instance = (FNumericalInputInstance<vec2int_t>*)I.get();
			Ar << Instance->Value;
		}
		break;
	case Substance_IType_Integer3:
		{
			FNumericalInputInstance<vec3int_t>* Instance = (FNumericalInputInstance<vec3int_t>*)I.get();
			Ar << Instance->Value;
		}
		break;
	case Substance_IType_Integer4:
		{
			FNumericalInputInstance<vec4int_t>* Instance = (FNumericalInputInstance<vec4int_t>*)I.get();
			Ar << Instance->Value;
		}
		break;

	case Substance_IType_Image:
		{
			SubstanceAir::FImageInputInstance* Instance = (SubstanceAir::FImageInputInstance*)I.get();
			Ar << Instance;
		}
		break;
	}

	return Ar;
}


FArchive& operator<<(FArchive& Ar, SubstanceAir::FGraphDesc*& G)
{
	if (Ar.IsLoading())
	{
		G = new SubstanceAir::FGraphDesc();
	}

	Ar << G->PackageUrl << G->Label << G->Description;
	Ar << G->OutputDescs.getArray() << G->InstanceUids.getArray();

	if (Ar.IsSaving())
	{
		INT Count = G->InputDescs.Num();
		Ar << Count;

		for (INT Idx=0 ; Idx<G->InputDescs.Num() ; ++Idx)
		{
			// start by the type to be able to allocate 
			// the good type when loading back in memory
			Ar << G->InputDescs(Idx)->Type;
			Ar << G->InputDescs(Idx);
		}
	}
	else if (Ar.IsLoading())
	{
		INT InputCount;
		Ar << InputCount;
		
		G->InputDescs.AddZeroed(InputCount);

		for (INT Idx=0 ; Idx<G->InputDescs.Num() ; ++Idx)
		{
			INT Type;
			Ar << Type;

			switch((SubstanceInputType)Type)
			{
			case Substance_IType_Float:
				G->InputDescs(Idx) = std::tr1::shared_ptr<input_desc_t>(new FNumericalInputDesc<float_t>());
				break;
			case Substance_IType_Float2:
				G->InputDescs(Idx) = std::tr1::shared_ptr<input_desc_t>(new FNumericalInputDesc<vec2float_t>());
				break;
			case Substance_IType_Float3:
				G->InputDescs(Idx) = std::tr1::shared_ptr<input_desc_t>(new FNumericalInputDesc<vec3float_t>());
				break;
			case Substance_IType_Float4:
				G->InputDescs(Idx) = std::tr1::shared_ptr<input_desc_t>(new FNumericalInputDesc<vec4float_t>());
				break;
			case Substance_IType_Integer:
				G->InputDescs(Idx) = std::tr1::shared_ptr<input_desc_t>(new FNumericalInputDesc<int_t>());
				break;
			case Substance_IType_Integer2:
				G->InputDescs(Idx) = std::tr1::shared_ptr<input_desc_t>(new FNumericalInputDesc<vec2int_t>());
				break;
			case Substance_IType_Integer3:
				G->InputDescs(Idx) = std::tr1::shared_ptr<input_desc_t>(new FNumericalInputDesc<vec3int_t>());
				break;
			case Substance_IType_Integer4:
				G->InputDescs(Idx) = std::tr1::shared_ptr<input_desc_t>(new FNumericalInputDesc<vec4int_t>());
				break;
			case Substance_IType_Image:
				G->InputDescs(Idx) = std::tr1::shared_ptr<input_desc_t>(new SubstanceAir::FImageInputDesc());
				break;
			default:
				break;
			}

			// the type is known so it will
			// serialize the good amount of data
			Ar << G->InputDescs(Idx);

			if (Ar.Ver() < VER_ALG_SBS_INPUT_INDEX)
			{
				G->InputDescs(Idx)->Index = Idx;
			}
		}
	}

	return Ar;
}
