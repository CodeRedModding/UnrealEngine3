//! @file SubstanceAirSequenceClasses.cpp
//! @brief Substance Air kismet sequence action implementation
//! @contact antoine.gonzalez@allegorithmic.com
//! @copyright Allegorithmic. All rights reserved.

#include <Engine.h>
#include <EngineSequenceClasses.h>

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirHelpers.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirSequenceClasses.h"

#if WITH_EDITOR
#include "SubstanceAirEdPreset.h"
#include "SubstanceAirEdHelpers.h"
#endif

SubstanceAir::List<graph_inst_t*> GGraphToUpdate;

void USeqAct_SubstanceSetInputFloat::Activated()
{
	if (InputValue.Num() && InputName.ToString().Len())
	{
		TArray<USubstanceAirGraphInstance*>::TIterator GraphIt(GraphInstances);

		for (;GraphIt;++GraphIt)
		{
			if (!(*GraphIt)->IsPendingKill())
			{
#if WITH_EDITOR
				if (GIsEditor)
				{
					SubstanceAir::Helpers::SaveInitialValues((*GraphIt)->Instance);					
				}
#endif
				if ((*GraphIt)->Instance->UpdateInput(InputName.ToString(), InputValue))
				{
					GGraphToUpdate.AddUniqueItem((*GraphIt)->Instance);
				}
			}
		}
	}

	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints
}


void USeqAct_SubstanceSetInputInt::Activated()
{
	if (InputValue.Num() && InputName.ToString().Len())
	{
		TArray<USubstanceAirGraphInstance*>::TIterator GraphIt(GraphInstances);

		for (;GraphIt;++GraphIt)
		{
#if WITH_EDITOR
			if (GIsEditor)
			{
				SubstanceAir::Helpers::SaveInitialValues((*GraphIt)->Instance);
			}
#endif
			if (!(*GraphIt)->IsPendingKill())
			{
				if ((*GraphIt)->Instance->UpdateInput(InputName.ToString(), InputValue))
				{
					GGraphToUpdate.AddUniqueItem((*GraphIt)->Instance);
				}
			}
		}
	}

	USequenceOp::Activated(); // Required for triggering Kismet visual debugger breakpoints
}


void USeqAct_SubstanceSetInputImg::Activated()
{
	if (InputName.ToString().Len())
	{
		if (SubstanceAir::Helpers::IsSupportedImageInput(InputValue))
		{
			TArray<USubstanceAirGraphInstance*>::TIterator GraphIt(GraphInstances);

			for (;GraphIt;++GraphIt)
			{
#if WITH_EDITOR
				if (GIsEditor)
				{
					SubstanceAir::Helpers::SaveInitialValues((*GraphIt)->Instance);
				}
#endif
				if (!(*GraphIt)->IsPendingKill())
				{
					if ((*GraphIt)->Instance->UpdateInput(InputName.ToString(), InputValue))
					{
						GGraphToUpdate.AddUniqueItem((*GraphIt)->Instance);
					}
				}
			}
		}
	}

	USequenceOp::Activated(); // Required to trigger Kismet visual debugger breakpoints
}


void USeqAct_SubstanceRender::Activated()
{
	if (this->bSynchronousRendering)
	{
		SubstanceAir::Helpers::RenderSync(GGraphToUpdate);
		SubstanceAir::Helpers::UpdateTextures(GGraphToUpdate);
	}
	else
	{
		SubstanceAir::Helpers::RenderAsync(GGraphToUpdate);
	}

	GGraphToUpdate.Empty();
}


IMPLEMENT_CLASS(USeqAct_SubstanceSetInputFloat)
IMPLEMENT_CLASS(USeqAct_SubstanceSetInputInt)
IMPLEMENT_CLASS(USeqAct_SubstanceSetInputImg)
IMPLEMENT_CLASS(USeqAct_SubstanceRender)
