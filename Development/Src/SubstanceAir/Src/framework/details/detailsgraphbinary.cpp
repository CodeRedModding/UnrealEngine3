//! @file detailsgraphbinary.cpp
//! @brief Substance Air Framework Graph binay information implementation
//! @author Christophe Soum - Allegorithmic
//! @date 20111115
//! @copyright Allegorithmic. All rights reserved.
//!

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirGraph.h"

#include "framework/details/detailsgraphbinary.h"

#include <algorithm>

//! @brief Invalid index
const UINT SubstanceAir::Details::GraphBinary::invalidIndex = 0xFFFFFFFFu;


//! Create from graph instance
//! @param graphInstance The graph instance, used to fill UIDs
SubstanceAir::Details::GraphBinary::GraphBinary(FGraphInstance* graphInstance) :
	mLinked(false)
{
	// Inputs
	inputs.reserve(graphInstance->Inputs.size());

	SubstanceAir::List<std::tr1::shared_ptr<input_inst_t>>::TIterator 
		inpinst(graphInstance->Inputs.itfront());

	for (;inpinst;++inpinst)
	{
		inputs.resize(inputs.size()+1);
		Entry &entry = inputs.back(); 
		entry.uidInitial = entry.uidTranslated = inpinst->get()->Desc->Uid;
		entry.index = invalidIndex;
	}
	
	// Outputs
	outputs.reserve(graphInstance->Outputs.size());

	SubstanceAir::List<output_inst_t>::TIterator 
		outinst(graphInstance->Outputs.itfront());

	for (;outinst;++outinst)
	{
		outputs.resize(outputs.size()+1);
		Entry &entry = outputs.back(); 
		entry.uidInitial = entry.uidTranslated = outinst->GetOutputDesc()->Uid;
		entry.index = invalidIndex;
	}
}


//! Reset translated UIDs before new link process
void SubstanceAir::Details::GraphBinary::resetTranslatedUids()
{
	SBS_VECTOR_FOREACH (Entry& entry,inputs)
	{
		entry.uidTranslated = entry.uidInitial;
		entry.index = invalidIndex;
	}
	
	SBS_VECTOR_FOREACH (Entry& entry,outputs)
	{
		entry.uidTranslated = entry.uidInitial;
		entry.index = invalidIndex;
	}
}


//! @brief Notify that this binary is linked
void SubstanceAir::Details::GraphBinary::linked()
{
	mLinked = TRUE;
}

