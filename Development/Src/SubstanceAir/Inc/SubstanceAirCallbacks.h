//! @file SubstanceAirCallbacks.h
//! @brief Substance Air helper function declaration
//! @date 20120120
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_AIR_CALLBACKS_H
#define _SUBSTANCE_AIR_CALLBACKS_H

#include "framework/callbacks.h"

#include "framework/details/detailssync.h"

namespace SubstanceAir
{

struct FOutputInstance;
struct FGraphInstance;

struct RenderCallbacks : public SubstanceAir::Callbacks
{
public:
	//! @brief Output computed callback (render result available) OVERLOAD
	//! Overload of SubstanceAir::Callbacks::outputComputed
	void outputComputed(
		UINT Uid,
		const SubstanceAir::FGraphInstance*,
		SubstanceAir::FOutputInstance*);

	static SubstanceAir::List<output_inst_t*> getComputedOutputs();

protected:
	static SubstanceAir::List<output_inst_t*> mOutputQueue;
	static SubstanceAir::Details::Sync::mutex mMutex;
};

} // namespace SubstanceAir

#endif //_SUBSTANCE_AIR_CALLBACKS_H
