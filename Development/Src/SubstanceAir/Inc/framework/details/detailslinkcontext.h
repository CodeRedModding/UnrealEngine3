//! @file detailslinkcontext.h
//! @brief Substance Air Framework Linking context definition
//! @author Christophe Soum - Allegorithmic
//! @date 20111102
//! @copyright Allegorithmic. All rights reserved.
//!

#ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSLINKCONTEXT_H
#define _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSLINKCONTEXT_H

#include "framework/details/detailsgraphbinary.h"

#pragma pack ( push, 8 )
#include <substance/linker/handle.h>
#pragma pack ( pop )

#include <vector>
#include <utility>


namespace SubstanceAir
{
namespace Details
{

class LinkDataStacking;

//! @brief Substance Link context structure used to push LinkData
struct LinkContext
{
	//! @brief UID translation container (initial,translated pair)
	typedef std::vector<std::pair<UINT,UINT> > UidTranslates;
	
	//! @brief One stack level definition
	struct StackLevel
	{
		//! @brief Correponding stacking link data
		const LinkDataStacking &linkData;
		
		//! @brief Inputs UID translations disabled at this level
		//! Initialy filled w/ connected+fused
		UidTranslates trPostInputs;

		//! @brief Outputs UID translations disabled at this level
		//! Initialy filled w/ connected+disabled
		UidTranslates trPreOutputs;
		
		
		//! @brief Constructor from context and stacking link data
		StackLevel(LinkContext &cxt,const LinkDataStacking &data);
		
		//! @brief Destructor, pop this level from stack
		~StackLevel();
	
		//! @brief Notify that pre graph is pushed, about to push post graph
		void beginPost();
		
		//! @brief Accessor level is actually pushing post
		bool isPost() const { return mIsPost; }
		
	protected:
		LinkContext &mLinkContext;   //!< Reference on link context
		bool mIsPost;                //!< Currently pushing Pre/Post flag 
	};
	
	//! Stacks levels container type
	typedef std::vector<StackLevel*> StackLevels;
	
	//! @brief Result of UID translation type
	typedef std::pair<UINT,bool> TrResult;
	
	
	//! @brief Substance Linker Handle
	SubstanceLinkerHandle *const handle;
	
	//! @brief Filled graph binary (receive alt. UID if collision)
	GraphBinary& graphBinary;
	
	//! @brief UID of graph state
	const UINT stateUid;
	
	//! @brief Stacked levels hierarchy
	StackLevels stackLevels;
	
	
	//! @brief Constructor from content
	LinkContext(SubstanceLinkerHandle *hnd,GraphBinary& gbinary,UINT uid);
	
	//! @brief Record Linker Collision UID 
	//! @param collisionType Output or input collision flag
	//! @param previousUid Initial UID that collide
	//! @param newUid New UID generated
	//! @note Called by linker callback
	void notifyLinkerUIDCollision(
		SubstanceLinkerUIDCollisionType collisionType,
		UINT previousUid,
		UINT newUid);
		
	//! @return Return translated UID following stacking
	//! @param isOutput Set to true if output UID searched, false if input
	//! @param previousUid Initial UID of the I/O to search for
	//! @param disabledValid If false, return false if I/O disabled by stacking
	//!		(connection, fuse, disable)
	//! @return Return a pair w/ translated UID value or previousUid if not
	//!		found, and boolean set to false if not found or found in stack and
	//!		disabledValid is false
	TrResult translateUid(
		bool isOutput,
		UINT previousUid,
		bool disabledValid) const;
	
	//! @return Search for translated UID 
	//! @param uidTranslates Container of I/O translated UIDs
	//! @param previousUid Initial UID of the I/O to search for
	//! @return Return a pair w/ translated UID value or previousUid if not
	//!		found, and boolean set to true if found
	static TrResult translate(
		const UidTranslates& uidTranslates,
		UINT previousUid);
		
protected:
	//! @return Return pointer on translated UID following stacking
	//! @param isOutput Set to true if output UID searched, false if input
	//! @param previousUid Initial UID of the I/O to search for
	//! @param disabledValid If false, return NULL if I/O disabled by stacking
	//!		(connection, fuse, disable)
	//! @return Return the pointer on translated UID value (R/W) or NULL if not
	//!		found or found in stack and disabledValid is false
	UINT* getTranslated(
		bool isOutput,
		UINT previousUid,
		bool disabledValid = true) const;
		
};  // struct LinkContext


} // namespace Details
} // namespace SubstanceAir

#endif // ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSLINKCONTEXT_H
