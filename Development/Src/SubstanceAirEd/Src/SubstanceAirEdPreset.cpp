//! @file preset.cpp
//! @brief Substance Air preset implementation
//! @author Christophe Soum - Allegorithmic
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirInput.h"
#include "SubstanceAirOutput.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirPackage.h"
#include "SubstanceAirImageInputClasses.h"
#include "SubstanceAirEdPreset.h"
#include "SubstanceAirEdXmlHelper.h"

namespace SubstanceAir
{

//! @brief Apply this preset to a graph instance
//! @param graph The target graph instance to apply this preset
//! @param mode Reset to default other inputs of merge w/ previous values
//! @return Return true whether at least one input value is applied
UBOOL FPreset::Apply(graph_inst_t* Instance, FPreset::ApplyMode mode) const
{
	UBOOL AtLeastOneSet = FALSE;
	graph_desc_t* Desc = Instance->Desc;
	
	for(int_t Idx = 0 ; Idx < mInputValues.Num() ; ++Idx)
	{
		const FInputValue& inp_value = mInputValues(Idx);

		// Found by Input UID
		std::tr1::shared_ptr<input_inst_t> inp_found =
			FindInput(Instance->Inputs, inp_value.mUid);

		if (inp_found.get() &&
		    inp_found->Type == inp_value.mType)
		{
			// Process only numerical (additional check)
			if (inp_found->IsNumerical())
			{
				TArray<FString> StrValueArray;
				inp_value.mValue.ParseIntoArray(&StrValueArray,TEXT(","),TRUE);

				TArray<FLOAT> ValueArray;

				for (TArray<FString>::TIterator ItV(StrValueArray); ItV; ++ItV)
				{
					ValueArray.AddItem(appAtof(*(*	ItV)));
				}

				Instance->UpdateInput(inp_found->Uid, ValueArray);
				AtLeastOneSet = TRUE;
			}
			else
			{
				if (inp_value.mValue != FString(TEXT("NULL")))
				{
					UObject* Object = FindObject<UTexture2D>(NULL, *inp_value.mValue, NULL);
					if (!Object)
					{
						Object = FindObject<USubstanceAirImageInput>(NULL, *inp_value.mValue, NULL);
					}

					if (Object)
					{
						Object->ConditionalPostLoad();
						Instance->UpdateInput(inp_found->Uid, Object);
					}
					else
					{
						debugf(
							TEXT("Unable to restore graph instance image input, object not found: %s"),
							*inp_value.mValue);
					}
				}
				else
				{
					Instance->UpdateInput(inp_found->Uid, NULL);
				}
			}
		}
		else
		{
			//! @todo: look by identifier instead
		}
	}
	
	return AtLeastOneSet;
}


void FPreset::ReadFrom(graph_inst_t* Graph)
{
	mPackageUrl = Graph->Desc->PackageUrl;
	mLabel = Graph->ParentInstance->GetName();
	mDescription = Graph->Desc->Description;

	for (SubstanceAir::List<std::tr1::shared_ptr<input_inst_t>>::TIterator 
		ItInp(Graph->Inputs.itfront()) ; ItInp ; ++ItInp)
	{
		INT Idx = mInputValues.AddZeroed(1);
		FPreset::FInputValue& inpvalue = mInputValues(Idx);

		inpvalue.mUid = (*ItInp)->Uid;
		inpvalue.mIdentifier = 
			Graph->Desc->GetInputDesc((*ItInp)->Uid)->Identifier;
		inpvalue.mValue = Helpers::GetValueString(*ItInp);
		inpvalue.mType = (SubstanceInputType)(*ItInp)->Type;
	}
}


UBOOL ParsePresets(
	presets_t& Presets,
	const FString& XmlPreset)
{
	const INT initialsize = Presets.Num();

	Helpers::ParseSubstanceXmlPreset(Presets,XmlPreset,NULL);
		
	return Presets.Num()>initialsize;
}


void WritePreset(preset_t& Preset, FString& XmlPreset)
{
	Helpers::WriteSubstanceXmlPreset(Preset,XmlPreset);
}

} // namespace SubstanceAir
