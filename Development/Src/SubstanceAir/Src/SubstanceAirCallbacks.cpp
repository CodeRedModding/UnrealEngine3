//! @file SubstanceAirCallbacks.h
//! @brief Substance Air render callbacks implementation
//! @date 20120120
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceAirCallbacks.h"

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirGraph.h"
#include "SubstanceAirOutput.h"

SubstanceAir::List<output_inst_t*> SubstanceAir::RenderCallbacks::mOutputQueue;
SubstanceAir::Details::Sync::mutex SubstanceAir::RenderCallbacks::mMutex;


void SubstanceAir::RenderCallbacks::outputComputed(
	UINT Uid,
	const SubstanceAir::FGraphInstance* graph,
	SubstanceAir::FOutputInstance* output)
{
	SubstanceAir::Details::Sync::mutex::scoped_lock slock(mMutex);
	mOutputQueue.push(output);
}


SubstanceAir::List<SubstanceAir::FOutputInstance*> SubstanceAir::RenderCallbacks::getComputedOutputs()
{
	SubstanceAir::Details::Sync::mutex::scoped_lock slock(mMutex);
	SubstanceAir::List<SubstanceAir::FOutputInstance*> outputs = mOutputQueue;
	mOutputQueue.Empty();
	return outputs;
}
