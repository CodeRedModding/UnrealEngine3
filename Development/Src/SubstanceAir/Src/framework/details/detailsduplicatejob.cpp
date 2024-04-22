//! @file detailsduplicatejob.cpp
//! @brief Substance Air Framework Render Job duplication context structure impl.
//! @author Christophe Soum - Allegorithmic
//! @date 20111115
//! @copyright Allegorithmic. All rights reserved.
//!

#include "framework/details/detailsduplicatejob.h"
#include "framework/details/detailsstates.h"
#include "framework/details/detailsoutputsfilter.h"
#include "framework/details/detailslinkgraphs.h"

//! @brief Constructor
//! @param linkGraphs_ Snapshot on graphs states to use at link time 
//! @param filter_ Used to filter outputs (outputs replace) or NULL if none.
//! @param states_ Used to retrieve graph instances
//! @param newFirst_ True if new job must be run before canceled ones
SubstanceAir::Details::DuplicateJob::DuplicateJob(
		const LinkGraphs& linkGraphs_,
		const OutputsFilter* filter_,
		const States &states_,
		bool newFirst_) :
	linkGraphs(linkGraphs_),
	filter(filter_),
	states(states_),
	newFirst(newFirst_)
{
}


//! @brief Append a pruned instance delta state
//! @param instanceUid The GraphInstance UID corr. to delta state
//! @param deltaState The delta state to append (values overriden, delete
//!		input entry if return to identity)
void SubstanceAir::Details::DuplicateJob::append(
	const guid_t& instanceUid,
	const DeltaState& deltaState)
{
	DeltaStates::iterator ite = mDeltaStates.find(instanceUid);
	if (ite!=mDeltaStates.end())
	{
		ite->second.append(deltaState,DeltaState::Append_Override);
		if (ite->second.getInputs().empty())
		{
			// Remove if all identity
			mDeltaStates.erase(ite);
		}
	}
	else
	{
		mDeltaStates.insert(std::make_pair(instanceUid,deltaState));
	}
}


//! @brief Prepend a canceled instance delta state
//! @param instanceUid The GraphInstance UID corr. to delta state
//! @param deltaState The delta state to prepend (reverse values merged)
void SubstanceAir::Details::DuplicateJob::prepend(
	const guid_t& instanceUid,
	const DeltaState& deltaState)
{
	DeltaState& dst = mDeltaStates[instanceUid];
	dst.append(deltaState,DeltaState::Append_Reverse);
}


//! @brief Fix instance input values w/ accumulated delta state
//! @param instanceUid The GraphInstance UID corr. to delta state
//! @param[in,out] deltaState The delta state to fix (merge)
void SubstanceAir::Details::DuplicateJob::fix(
	const guid_t& instanceUid,
	DeltaState& deltaState)
{
	DeltaStates::iterator ite = mDeltaStates.find(instanceUid);
	if (ite!=mDeltaStates.end())
	{
		deltaState.append(ite->second,DeltaState::Append_Default);
		mDeltaStates.erase(ite);
	}
}

