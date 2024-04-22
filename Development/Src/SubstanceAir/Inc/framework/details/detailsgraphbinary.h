//! @file detailsgraphbinary.h
//! @brief Substance Air Framework Graph binay information definition
//! @author Christophe Soum - Allegorithmic
//! @date 20111026
//! @copyright Allegorithmic. All rights reserved.
//!

#ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSGRAPHBINARY_H
#define _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSGRAPHBINARY_H

#include <vector>

namespace SubstanceAir
{

struct FGraphInstance;

namespace Details
{

//! @brief Graph binary information
//! Contains initial UIDs and UID/index translation of a graph state into 
//! linked SBSBIN.
//!
//! Filled during link process (UID collision) and at Engine handle creation 
//! time (indexes).
struct GraphBinary
{
	//! @brief One input/output binary description
	//! Only uidInitial is valid if graph not already linked
	struct Entry
	{
		UINT uidInitial;     //! Initial UID
		UINT uidTranslated;  //! Translated UID (uid collision at link time)
		UINT index;          //! Index in SBSBIN
	};  // struct Entry
	
	//! @brief Entry array 
	typedef std::vector<Entry> Entries;
	
	//! @brief Predicate used for sorting/searching entries per initial UID
	struct InitialUidOrder
	{
		bool operator()(const Entry& a,const Entry& b) const { 
			return a.uidInitial<b.uidInitial; }
		bool operator()(UINT a,const Entry& b) const { return a<b.uidInitial; }
		bool operator()(const Entry& a,UINT b) const { return a.uidInitial<b; }
	};  // struct InitialUidOrder
	
	
	//! @brief Inputs in GraphState/GraphInstance order (== UID initial order)
	Entries inputs;
	
	//! @brief Outputs in GraphState/GraphInstance order (== UID initial order)
	Entries outputs;
	
	//! @brief Invalid index
	static const UINT invalidIndex;
	

	//! Create from graph instance
	//! @param graphInstance The graph instance, used to fill UIDs
	GraphBinary(FGraphInstance* graphInstance);
	
	//! Reset translated UIDs before new link process
	void resetTranslatedUids();
	
	//! @brief This binary is currently linked (present in current SBSBIN data)
	UBOOL isLinked() const { return mLinked; }
		
	//! @brief Notify that this binary is linked
	void linked();
	
protected:
	UBOOL mLinked;                     //!< Is linked flag
	
};  // struct GraphBinary


} // namespace Details
} // namespace SubstanceAir

#endif // ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSGRAPHBINARY_H
