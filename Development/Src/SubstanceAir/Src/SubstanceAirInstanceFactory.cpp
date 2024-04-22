//! @file SubstanceAirFactory.cpp
//! @brief Implementation of the USubstanceAirInstanceFactory class
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirPackage.h"
#include "SubstanceAirInstanceFactoryClasses.h"
#include "SubstanceAirHelpers.h"

#include "framework/details/detailslinkdata.h"

#if WITH_EDITOR
#include "SubstanceAirEdHelpers.h"
#endif // WITH_EDITOR


IMPLEMENT_CLASS( USubstanceAirInstanceFactory )


void USubstanceAirInstanceFactory::InitializeIntrinsicPropertyValues()
{
	SubstancePackage = 0;
}


void USubstanceAirInstanceFactory::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		if (!SubstancePackage)
		{
			SubstancePackage = new SubstanceAir::FPackage();
			SubstancePackage->Parent = this;
		}
	}

	if (SubstancePackage)
	{
		Ar << SubstancePackage;
	}
}


void USubstanceAirInstanceFactory::BeginDestroy()
{
	Super::BeginDestroy();

	if (SubstancePackage)
	{
		delete SubstancePackage;
		SubstancePackage = 0;
	}
}


void USubstanceAirInstanceFactory::PostDuplicate()
{
#if WITH_EDITOR == 1
	check(SubstancePackage);

	// create a new Guid
	SubstancePackage->Guid = appCreateGuid();

	// create instances 
	SubstanceAir::List<graph_desc_t*>::TIterator ItG(
		SubstancePackage->Graphs.itfront());

	for (;ItG;++ItG)
	{
		SubstanceAir::List<guid_t> RefInstances = (*ItG)->InstanceUids;
		(*ItG)->InstanceUids.Empty();

		SubstanceAir::List<guid_t>::TIterator ItGuid(RefInstances.itfront());

		for (;ItGuid;++ItGuid)
		{
			for (TObjectIterator<USubstanceAirGraphInstance> It; It; ++It)
			{
				if ((*It)->Instance->InstanceGuid == *ItGuid)
				{
					UBOOL bUseDefaultPackage = TRUE;
					UBOOL bCreateOutputs = FALSE;

					graph_inst_t* NewInstance = SubstanceAir::Helpers::InstantiateGraph(
						(*ItG), NULL, bUseDefaultPackage, bCreateOutputs, (*It)->GetName());

					// copy values from previous
					SubstanceAir::Helpers::CopyInstance((*It)->Instance, NewInstance);
					SubstanceAir::Helpers::FlagRefreshContentBrowser(NewInstance->Outputs.Num());
					SubstanceAir::Helpers::RenderAsync(NewInstance);
					break;
				}
			}
		}
	}
#endif
}


INT USubstanceAirInstanceFactory::GetResourceSize()
{
	if (SubstancePackage)
	{
		return SubstancePackage->LinkData->getSize();
	}
	
	return 0;
}
