//! @file SubstanceAirStructures.cpp
//! @author Antoine Gonzalez - Allegorithmic
//! @date 20110105
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirPackage.h"
#include "SubstanceAirInstanceFactoryClasses.h"

#include "framework/details/detailsrendertoken.h"

#pragma pack ( push, 8 )
#include <substance/substance.h>
#pragma pack ( pop )

#if WITH_EDITOR
#include "SubstanceAirEdHelpers.h"
#include "ScopedTransaction.h"
#endif

#include <ScopedTransaction.h>

namespace SubstanceAir
{

FOutputDesc::FOutputDesc():
	Uid(0),
	Format(0),
	Channel(0)
{

}


FOutputDesc::FOutputDesc(const FOutputDesc & O):
	Identifier(O.Identifier),
	Label(O.Label),
	Uid(O.Uid),
	Format(O.Format),
	Channel(O.Channel),
	AlteringInputUids(O.AlteringInputUids)
{

}


FOutputInstance FOutputDesc::Instantiate() const
{
	output_inst_t Instance;
	Instance.Uid = Uid;
	Instance.Format = Format;
	
	return Instance;
}


FOutputInstance::FOutputInstance():
	Uid(0),
	Format(0),
	OutputGuid(appCreateGuid()),
	bIsEnabled(FALSE),
	bIsDirty(TRUE),
	Texture(std::tr1::shared_ptr<USubstanceAirTexture2D*>(new USubstanceAirTexture2D*))
{
	*(Texture.get()) = NULL;
}


FOutputInstance::FOutputInstance(const FOutputInstance& Other)
{
	Uid = Other.Uid;
	Format = Other.Format;
	OutputGuid = Other.OutputGuid;
	bIsEnabled = Other.bIsEnabled;
	ParentInstance = Other.ParentInstance;

	std::tr1::shared_ptr<USubstanceAirTexture2D*> prevTexture = Texture;

	// the new output instance now owns the texture
	Texture = Other.Texture;

	if (prevTexture.get() && prevTexture.unique())
	{
		Helpers::Clear(prevTexture);
	}
}


graph_inst_t* FOutputInstance::GetParentGraphInstance() const
{
	// the outer of the texture, it is the USubstanceAirGraphInstance
	USubstanceAirGraphInstance* Outer = GetOuter();

	if (Outer && Outer->Instance)
	{
		return Outer->Instance;
	}

	return NULL;
}


graph_desc_t* FOutputInstance::GetParentGraph() const
{
	// the outer of the texture, it is the USubstanceAirGraphInstance
	USubstanceAirGraphInstance* Outer = GetOuter();

	if (Outer && Outer->Instance && Outer->Instance->Desc)
	{
		return Outer->Instance->Desc;
	}
	
	return NULL;
}


output_desc_t* FOutputInstance::GetOutputDesc() const
{
	// the outer of the texture, it is the USubstanceAirGraphInstance
	USubstanceAirGraphInstance* Outer = GetOuter();

	if (Outer && Outer->Instance && Outer->Instance->Desc)
	{
		SubstanceAir::List<output_desc_t>::TIterator
			ItOut(Outer->Instance->Desc->OutputDescs.itfront());

		for (;ItOut;++ItOut)
		{
			if ((*ItOut).Uid == Uid)
			{
				return &(*ItOut);
			}
		}
	}

	return NULL;
}


void FOutputInstance::push(const Token& rtokenptr)
{
	RenderTokens.push_back(rtokenptr);
}


UBOOL FOutputInstance::queueRender()
{
	bool res = bIsDirty;
	bIsDirty = FALSE;
	return res;
}


std::tr1::shared_ptr<FInputDescBase> FindInput(
	SubstanceAir::List< std::tr1::shared_ptr<FInputDescBase> >& Inputs, 
	const uint_t uid)
{
	for (int_t i = 0 ; i < Inputs.Num() ; ++i)
	{
		if (uid == Inputs(i)->Uid)
		{
			return Inputs(i);
		}
	}

	return std::tr1::shared_ptr<FInputDescBase>(NULL);
}


std::tr1::shared_ptr<FInputInstanceBase> FindInput(
	SubstanceAir::List< std::tr1::shared_ptr<FInputInstanceBase> >& Inputs, 
	const uint_t uid)
{
	for (int_t i = 0 ; i < Inputs.Num() ; ++i)
	{
		if (uid == Inputs(i)->Uid)
		{
			return Inputs(i);
		}
	}

	return std::tr1::shared_ptr<FInputInstanceBase>(NULL);
}


USubstanceAirGraphInstance* FOutputInstance::GetOuter() const
{
	return ParentInstance;
}


FOutputInstance::Result FOutputInstance::grabResult()
{
	Result res;

	// Remove all canceled render results
	while (RenderTokens.size() != 0 &&
		(*RenderTokens.itfront()).get()->canRemove())
	{
		RenderTokens.pop_front();
	}

	// Get first valid
	for (RenderTokens_t::TIterator ite(RenderTokens.itfront());
		res.get()==NULL && ite ;
		++ite)
	{
		res.reset((*ite)->grabResult());
	}

	return res;
}


template< typename T > std::tr1::shared_ptr<input_inst_t> instantiateNumericalInput(input_desc_t* Input)
{
	std::tr1::shared_ptr< input_inst_t > Instance = std::tr1::shared_ptr< input_inst_t >(new FNumericalInputInstance<T>(Input));
	FNumericalInputInstance< T >* I = (FNumericalInputInstance< T >*)Instance.get();
	I->Value = ((FNumericalInputDesc< T >*)Input)->DefaultValue;

	return Instance;
}


std::tr1::shared_ptr< input_inst_t > FInputDescBase::Instantiate()
{
	std::tr1::shared_ptr< input_inst_t > Instance;

	switch((SubstanceInputType)Type)
	{
		case Substance_IType_Float:
		{
			Instance = instantiateNumericalInput<float_t>(this);
		}
		break;
	case Substance_IType_Float2:
		{
			Instance = instantiateNumericalInput<vec2float_t>(this);
		}
		break;
	case Substance_IType_Float3:
		{
			Instance = instantiateNumericalInput<vec3float_t>(this);
		}
		break;
	case Substance_IType_Float4:
		{
			Instance = instantiateNumericalInput<vec4float_t>(this);
		}
		break;
	case Substance_IType_Integer:
		{
			Instance = instantiateNumericalInput<int_t>(this);
		}
		break;
	case Substance_IType_Integer2:
		{
			Instance = instantiateNumericalInput<vec2int_t>(this);
		}
		break;
	case Substance_IType_Integer3:
		{
			Instance = instantiateNumericalInput<vec3int_t>(this);
		}
		break;
	case Substance_IType_Integer4:
		{
			Instance = instantiateNumericalInput<vec4int_t>(this);
		}
		break;

	case Substance_IType_Image:
		{
			Instance = std::tr1::shared_ptr<input_inst_t>( new FImageInputInstance(this) );
		}
		break;
	}

	Instance->Desc = this;

	return Instance;
}


template< typename T > std::tr1::shared_ptr<input_inst_t> cloneInstance(FInputInstanceBase* Input)
{
	std::tr1::shared_ptr<input_inst_t> Instance = std::tr1::shared_ptr<input_inst_t>( new FNumericalInputInstance<T>);
	FNumericalInputInstance<T>* I = (FNumericalInputInstance<T>*)Instance.get();
	I->Value = ((FNumericalInputInstance<T>*)Input)->Value;

	return Instance;
}


std::tr1::shared_ptr<input_inst_t> FInputInstanceBase::Clone()
{
	std::tr1::shared_ptr<input_inst_t> Instance;

	switch((SubstanceInputType)Type)
	{
	case Substance_IType_Float:
		{
			Instance = cloneInstance<float_t>(this);
		}
		break;
	case Substance_IType_Float2:
		{
			Instance = cloneInstance<vec2float_t>(this);
		}
		break;
	case Substance_IType_Float3:
		{
			Instance = cloneInstance<vec3float_t>(this);
		}
		break;
	case Substance_IType_Float4:
		{
			Instance = cloneInstance<vec4float_t>(this);
		}
		break;
	case Substance_IType_Integer:
		{
			Instance = cloneInstance<int_t>(this);
		}
		break;
	case Substance_IType_Integer2:
		{
			Instance = cloneInstance<vec2int_t>(this);
		}
		break;
	case Substance_IType_Integer3:
		{
			Instance = cloneInstance<vec3int_t>(this);
		}
		break;
	case Substance_IType_Integer4:
		{
			Instance = cloneInstance<vec4int_t>(this);
		}
		break;

	case Substance_IType_Image:
		{
			Instance = std::tr1::shared_ptr<input_inst_t>( new FImageInputInstance );
		}
		break;
	}

	Instance->Uid = Uid;
	Instance->Type = Type;
	Instance->IsHeavyDuty = IsHeavyDuty;
	Instance->Parent = Parent;

	return Instance;
}


UBOOL FInputDescBase::IsNumerical() const
{
	return Type != Substance_IType_Image;
}


FNumericalInputDescComboBox::FNumericalInputDescComboBox(FNumericalInputDesc<int_t>* Desc)
{
	Identifier = Desc->Identifier;
	Label = Desc->Label;
	Widget = Desc->Widget;
	Uid = Desc->Uid;
	IsHeavyDuty = Desc->IsHeavyDuty;
	Type = Desc->Type;
	AlteredOutputUids = Desc->AlteredOutputUids;
	IsClamped = Desc->IsClamped;
	DefaultValue = Desc->DefaultValue;
	Min = Desc->Min;
	Max = Desc->Max;
	Group = Desc->Group;
}


UBOOL FInputInstanceBase::IsNumerical() const
{
	return Type != Substance_IType_Image;
}


FInputInstanceBase::FInputInstanceBase(input_desc_t* InputDesc):
	UseCache(FALSE),
	Desc(InputDesc),
	Parent(NULL)
{
	if (InputDesc)
	{
		Type = InputDesc->Type;
		Uid = InputDesc->Uid;
		IsHeavyDuty = InputDesc->IsHeavyDuty;
	}
}


UBOOL FNumericalInputInstanceBase::isModified(const void* numeric) const
{
	return appMemcmp(getRawData(), numeric, getRawSize()) ? TRUE : FALSE;
}


template < typename T > void FNumericalInputInstance<T>::Reset()
{
	appMemZero(Value);
}


FImageInputInstance::FImageInputInstance(FInputDescBase* Input):
	FInputInstanceBase(Input),
		PtrModified(FALSE),
		ImageSource(NULL)
{

}


FImageInputInstance::FImageInputInstance(const FImageInputInstance& Other)
{
	PtrModified = Other.PtrModified;
	Uid = Other.Uid;
	IsHeavyDuty = Other.IsHeavyDuty;
	Type = Other.Type;
	ImageSource = Other.ImageSource;
	ImageInput = Other.ImageInput;
	Parent = Other.Parent;
}


FImageInputInstance& FImageInputInstance::operator =(const FImageInputInstance& Other)
{
	PtrModified = Other.PtrModified;
	Uid = Other.Uid;
	IsHeavyDuty = Other.IsHeavyDuty;
	Type = Other.Type;
	ImageSource = Other.ImageSource;
	ImageInput = Other.ImageInput;
	Parent = Other.Parent;

	return *this;
}


//! @brief Internal use only
UBOOL FImageInputInstance::isModified(const void* numeric) const
{
	UBOOL res = ImageInput!=NULL &&
		ImageInput->resolveDirty();
	res = PtrModified || res;
	PtrModified = FALSE;
	return res;
}


void FImageInputInstance::SetImageInput(
	UObject* InValue,
	FGraphInstance* Parent,
	UBOOL unregisterOutput,
	UBOOL isTransacting)
{
	std::tr1::shared_ptr<SubstanceAir::ImageInput> NewInput = 
		Helpers::PrepareImageInput(InValue, this, Parent);

	if (NewInput!=ImageInput || NULL == NewInput.get())
	{
		if (InValue && NULL == NewInput.get())
		{
			return;
		}

		if (GIsEditor && FALSE == isTransacting)
		{
#if WITH_EDITOR
			FScopedTransaction Transaction(
				*Localize(TEXT("Editor"), 
				TEXT("ModifiedInput"),
				TEXT("SubstanceAir"), NULL, 0));
			Parent->ParentInstance->Modify();
#endif
		}

		if (InValue != ImageSource || unregisterOutput)
		{
			if (Cast<USubstanceAirTexture2D>(ImageSource) && unregisterOutput)
			{
				SubstanceAir::Helpers::UnregisterOutputAsImageInput(
					Cast<USubstanceAirTexture2D>(ImageSource), FALSE, FALSE);
			}

			if (Cast<USubstanceAirTexture2D>(InValue))
			{
				SubstanceAir::Helpers::RegisterOutputAsImageInput(
					Parent->ParentInstance, 
					Cast<USubstanceAirTexture2D>(InValue));
			}
		}

		ImageInput = NewInput;
		PtrModified = TRUE;
		ImageSource = InValue;
		Parent->bHasPendingImageInputRendering = TRUE;
	}
}


SIZE_T getComponentsCount(SubstanceInputType type)
{
	switch (type)
	{
	case Substance_IType_Float   :
	case Substance_IType_Integer : return 1;
	case Substance_IType_Float2  :
	case Substance_IType_Integer2: return 2;
	case Substance_IType_Float3  :
	case Substance_IType_Integer3: return 3;
	case Substance_IType_Float4  :
	case Substance_IType_Integer4: return 4;
	default                      : return 0;
	}
}

} // namespace SubstanceAir
