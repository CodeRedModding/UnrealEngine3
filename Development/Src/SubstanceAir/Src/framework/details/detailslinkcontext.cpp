//! @file detailslinkcontext.cpp
//! @brief Substance Air Framework Linking context implementation
//! @author Christophe Soum - Allegorithmic
//! @date 20111118
//! @copyright Allegorithmic. All rights reserved.
//!

#include "SubstanceAirTypedefs.h"

#include "framework/details/detailslinkcontext.h"
#include "framework/details/detailslinkdata.h"
#include "framework/details/detailsgraphbinary.h"

#include <algorithm>

	
//! @brief Constructor from content
SubstanceAir::Details::LinkContext::LinkContext(
		SubstanceLinkerHandle *hnd,
		GraphBinary& gbinary,
		UINT uid) :
	handle(hnd), 
	graphBinary(gbinary), 
	stateUid(uid)
{
}


//! @brief Record Linker Collision UID 
//! @param collisionType Output or input collision flag
//! @param previousUid Initial UID that collide
//! @param newUid New UID generated
//! @note Called by linker callback
void SubstanceAir::Details::LinkContext::notifyLinkerUIDCollision(
	SubstanceLinkerUIDCollisionType collisionType,
	UINT previousUid,
	UINT newUid)
{
	UINT* truid = getTranslated(
		collisionType==Substance_Linker_UIDCollision_Output,
		previousUid);
		
	if (truid!=NULL)
	{
		*truid = newUid;
	}
}


//! @return Return pointer on translated UID following stacking
//! @param isOutput Set to true if output UID searched, false if input
//! @param previousUid Initial UID of the I/O to search for
//! @param disabledValid If false, return NULL if I/O disabled by stacking
//!		(connection, fuse, disable)
//! @return Return the pointer on translated UID value (R/W) or NULL if not
//!		found or found in stack and disabledValid is false
UINT* SubstanceAir::Details::LinkContext::getTranslated(
	bool isOutput,
	UINT previousUid,
	bool disabledValid) const
{
	// Search if I/O not disabled by stack
	// translate previous UID in function of stacking UID collisions
	SBS_VECTOR_REVERSE_FOREACH (StackLevel* level,stackLevels)
	{
		if (isOutput==!level->isPost())
		{
			// Output of pre graph OR Input of post graph
			{
				// Search if disabled at this level
				UidTranslates *uidtr = isOutput ? 
					&level->trPreOutputs : 
					&level->trPostInputs;
				const UidTranslates::iterator ite = std::lower_bound(
					uidtr->begin(),
					uidtr->end(),
					UidTranslates::value_type(previousUid,0));
				if (ite!=uidtr->end() && ite->first==previousUid)
				{
					return disabledValid ?  
						&ite->second :      // return it
						NULL;               // not valid return NULL
				}
			}
			
			// Follow stack collision
			const UidTranslates *uidtr = isOutput ? 
				&level->linkData.mTrPreOutputs : 
				&level->linkData.mTrPostInputs;
			previousUid = translate(*uidtr,previousUid).first;
		}
	}

	// Search in graph binary
	GraphBinary::Entries *entries = isOutput ?
		&graphBinary.outputs :
		&graphBinary.inputs;
	GraphBinary::Entries::iterator ite = std::lower_bound(
		entries->begin(),
		entries->end(),
		previousUid,
		GraphBinary::InitialUidOrder());
	
	if (ite!=entries->end() && ite->uidInitial==previousUid)
	{
		// Present in binary
		return &ite->uidTranslated;
	}
	else
	{
		// unused I/O (multi-graph case)
		return NULL;
	}
}


//! @return Return translated UID following stacking
//! @param isOutput Set to true if output UID searched, false if input
//! @param previousUid Initial UID of the I/O to search for
//! @param disabledValid If false, return false if I/O disabled by stacking
//!		(connection, fuse, disable)
//! @return Return a pair w/ translated UID value or previousUid if not
//!		found, and boolean set to false if not found or found in stack and
//!		disabledValid is false
SubstanceAir::Details::LinkContext::TrResult 
SubstanceAir::Details::LinkContext::translateUid(
	bool isOutput,
	UINT previousUid,
	bool disabledValid) const
{
	UINT* truid = getTranslated(
		isOutput,
		previousUid,
		disabledValid);
		
	return truid!=NULL ?
		TrResult(*truid,true) :
		TrResult(previousUid,false);
}


//! @return Search for translated UID 
//! @param uidTranslates Container of I/O translated UIDs
//! @param previousUid Initial UID of the I/O to search for
//! @return Return a pair w/ translated UID value or previousUid if not
//!		found, and boolean set to true if found
SubstanceAir::Details::LinkContext::TrResult 
SubstanceAir::Details::LinkContext::translate(
	const UidTranslates& uidTranslates,
	UINT previousUid)
{
	const UidTranslates::const_iterator ite = std::lower_bound(
		uidTranslates.begin(),
		uidTranslates.end(),
		UidTranslates::value_type(previousUid,0));
	return ite!=uidTranslates.end() && ite->first==previousUid ?
		TrResult(ite->second,true) :
		TrResult(previousUid,false);
}


//! @brief Constructor from context and stacking link data
SubstanceAir::Details::LinkContext::StackLevel::StackLevel(LinkContext &cxt,const LinkDataStacking &data) : 
	linkData(data), 
	mLinkContext(cxt), 
	mIsPost(false)
{
	mLinkContext.stackLevels.push_back(this);
	
	// Fill trPostInputs and trPreOutputs
	
	// From connections
	SBS_VECTOR_FOREACH (
		const ConnectionsOptions::PairInOut& c,
		linkData.mOptions.mConnections)
	{
		trPostInputs.push_back(std::make_pair(c.second,c.second));
		trPreOutputs.push_back(std::make_pair(c.first,c.first));
	}
	
	// From non connected outputs 
	SBS_VECTOR_FOREACH (UINT uid,linkData.mDisabledOutputs)
	{
		trPreOutputs.push_back(std::make_pair(uid,uid));
	}
	
	// From fused inputs
	SBS_VECTOR_FOREACH (
		const ConnectionsOptions::PairInOut &fuse,
		linkData.mFuseInputs)
	{
		trPostInputs.push_back(std::make_pair(fuse.second,fuse.second));
	}
	
	// Sort
	std::sort(trPostInputs.begin(),trPostInputs.end());
	std::sort(trPreOutputs.begin(),trPreOutputs.end());
}


//! @brief Destructor, pop this level from stack
SubstanceAir::Details::LinkContext::StackLevel::~StackLevel()
{
	check(mLinkContext.stackLevels.back()==this);
	mLinkContext.stackLevels.pop_back();
}


//! @brief Notify that pre graph is pushed, about to push post graph
void SubstanceAir::Details::LinkContext::StackLevel::beginPost()
{
	mIsPost = true;
}
