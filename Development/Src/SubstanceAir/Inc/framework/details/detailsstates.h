//! @file detailsstates.h
//! @brief Substance Air Framework All Instances global State definition
//! @author Christophe Soum - Allegorithmic
//! @date 20111031
//! @copyright Allegorithmic. All rights reserved.
//!
 

#ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSSTATES_H
#define _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSSTATES_H

#include "SubstanceAirTypedefs.h"
#include "framework/callbacks.h"

#include <map>

#include <SubstanceAirUncopyable.h>

namespace SubstanceAir
{

struct FGraphInstance;

namespace Details
{

class GraphState;
struct LinkGraphs;


//! @brief All Graph Instances global States
//! Represent the state of all pushed Graph instances. 
//! Each graph state is associated to one or several instances.
class States : Uncopyable
{
public:

	//! @brief Destructor, remove pointer on this in recorded instances
	~States();

	//! @brief Get the GraphState associated to this graph instance
	//! @param graphInstance The source graph instance
	//! @note Called from user thread 
	//!
	//! Create new state from graph instance if necessary (undefined state):
	//! record the state into the instance.
	GraphState& operator[](FGraphInstance* graphInstance);
	
	//! @brief Return a graph instance from its UID or NULL if deleted 
	//! @note Called from user thread 
	FGraphInstance* getInstanceFromUid(const guid_t& uid) const;
	
	//! @brief Notify that an instance is deleted
	//! @param uid The uid of the deleted graph instance
	//! @note Called from user thread 
	void notifyDeleted(const guid_t& uid);
	
	//! @brief Fill active graph snapshot: all active graph instance to link
	//! @param snapshot The active graphs array to fill
	void fill(LinkGraphs& snapshot) const;
	
protected:

	//! @brief Pair of GraphState, GraphInstance
	struct Instance
	{
		//! @brief Pointer on graph state
		std::tr1::shared_ptr<GraphState> state;
		
		//! @brief Pointer on corresponding instance.
		//! Used for deleting pointer on this
		//! @note Only used from user thread 
		FGraphInstance* instance;    
		
	};  // struct Instance

	//! @brief Map of UID / instances
	typedef std::map<guid_t,Instance, guid_t_comp> InstancesMap_t;
	
	//! @brief GraphInstance per instance UID dictionnary
	//! GraphState can be referenced several times per different instances
	//! Pointer on GraphState kept by StatesSnapshot while useful.
	InstancesMap_t InstancesMap;
	
};  // class States


} // namespace Details
} // namespace SubstanceAir

#endif // ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSSTATES_H
