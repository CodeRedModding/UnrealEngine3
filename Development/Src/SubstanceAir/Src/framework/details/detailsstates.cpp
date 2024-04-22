//! @file detailsstates.cpp
//! @brief Substance Air Framework All Instances global State impl.
//! @author Christophe Soum - Allegorithmic
//! @date 20111031
//! @copyright Allegorithmic. All rights reserved.
//!

#include "SubstanceAirGraph.h"

#include "framework/details/detailsstates.h"
#include "framework/details/detailsgraphstate.h"
#include "framework/details/detailslinkgraphs.h"

#include <algorithm>
#include <utility>
#include <set>


//! @brief Destructor
SubstanceAir::Details::States::~States()
{

}


//! @brief Get the GraphState associated to this graph instance
//! @param graphInstance The source graph instance
//! @note Called from user thread 
//!
//! Create new state from graph instance if necessary (undefined state):
//! record the state into the instance.
SubstanceAir::Details::GraphState& 
	SubstanceAir::Details::States::operator[](FGraphInstance* graphInstance)
{
	InstancesMap_t::iterator ite = InstancesMap.find(graphInstance->InstanceGuid);
	if (ite==InstancesMap.end())
	{
		Instance inst;
		inst.state.reset(new GraphState(graphInstance));
		inst.instance = graphInstance;
		ite = InstancesMap.insert(
			std::make_pair(graphInstance->InstanceGuid,inst)).first;
		graphInstance->plugState(this);
	}
	
	return *ite->second.state.get();
}


//! @brief Return a graph instance from its UID or NULL if deleted 
//! @note Called from user thread 
SubstanceAir::FGraphInstance* 
SubstanceAir::Details::States::getInstanceFromUid(const guid_t& uid) const
{
	InstancesMap_t::const_iterator ite = InstancesMap.find(uid);
	return ite!=InstancesMap.end() ? ite->second.instance : NULL;
}


//! @brief Notify that an instance is deleted
//! @param uid The uid of the deleted graph instance
//! @note Called from user thread 
void SubstanceAir::Details::States::notifyDeleted(const guid_t& uid)
{
	InstancesMap_t::iterator ite = InstancesMap.find(uid);
	check(ite!=InstancesMap.end());
	InstancesMap.erase(ite);
}


//! @brief Fill active graph snapshot: all active graph instance to link
//! @param snapshot The active graphs array to fill
void SubstanceAir::Details::States::fill(LinkGraphs& snapshot) const
{
	snapshot.graphStates.reserve(InstancesMap.size());
	std::set<UINT> pushedstates; 

	InstancesMap_t::const_iterator it;
	for (it=InstancesMap.begin(); it!=InstancesMap.end(); it++)
	{
		const InstancesMap_t::value_type &inst = *it;

		if (pushedstates.insert(inst.second.state->getUid()).second)
		{
			snapshot.graphStates.push_back(inst.second.state);
		}
	}
	
	// Sort list of states per State UID
	std::sort(
		snapshot.graphStates.begin(),
		snapshot.graphStates.end(),
		LinkGraphs::OrderPredicate());
}

