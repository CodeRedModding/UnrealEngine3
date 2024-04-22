//! @file detailslinkdata.cpp
//! @brief Substance Air Framework Link data classes implementation
//! @author Christophe Soum - Allegorithmic
//! @date 20111116
//! @copyright Allegorithmic. All rights reserved.
//!
 
#include "SubstanceAirTypedefs.h"

#include "SubstanceAirGraph.h"

#include "framework/details/detailslinkdata.h"
#include "framework/details/detailslinkcontext.h"
#include "framework/details/detailsgraphbinary.h"


//! @brief Destructor
SubstanceAir::Details::LinkData::~LinkData()
{
}


//! @brief Constructor from assembly data
//! @param ptr Pointer on assembly data
//! @param size Size of assembly data in bytes
SubstanceAir::Details::LinkDataAssembly::LinkDataAssembly(
		const BYTE* ptr,
		UINT size) :
	mAssembly((const char*)ptr,size)
{
}


//! @brief Push data to link
//! @param cxt Used to push link data
UBOOL SubstanceAir::Details::LinkDataAssembly::push(LinkContext& cxt) const
{
	int err;
	
	// Push assembly
	{
		SubstanceLinkerPush srcpush;
		srcpush.uid = cxt.stateUid;
		srcpush.srcType = Substance_Linker_Src_Buffer;
		srcpush.src.buffer.ptr = mAssembly.data();
		srcpush.src.buffer.size = mAssembly.size();

		err = substanceLinkerHandlePushAssemblyEx(
			cxt.handle,
			&srcpush);
	}

	if (err)
	{
		// TODO2 error pushing assembly to linker
		check(0);
		return false;
	}

	// set output formats/mipmaps
	SBS_VECTOR_FOREACH (const OutputFormat& outfmt, mOutputFormats)
	{
		LinkContext::TrResult trres = cxt.translateUid(true,outfmt.uid,false);
		if (trres.second)
		{
			err = substanceLinkerHandleSetOutputFormat(
				cxt.handle,
				trres.first,
				outfmt.format,
				outfmt.mipmap);
				
			if (err)
			{
				// TODO2 Warning: output UID not valid
				check(0);
			}
		}
	}

	return true;
}
	
	
//! @brief Force output format/mipmap
//! @param uid Output uid
//! @param format New output format
//! @param mipmap New mipmap count
void SubstanceAir::Details::LinkDataAssembly::setOutputFormat(
	UINT uid,
	int format,
	UINT mipmap)
{
	OutputFormat outfmt = {uid,format,mipmap};
	mOutputFormats.push_back(outfmt);
}
	
	
//! @brief Construct from pre and post Graphs, and connection options.
//! @param preGraph Source pre graph.
//! @param postGraph Source post graph.
//! @param connOptions Connections options.
SubstanceAir::Details::LinkDataStacking::LinkDataStacking(
		const FGraphDesc* preGraph,
		const FGraphDesc* postGraph,
		const ConnectionsOptions& connOptions) :
	mOptions(connOptions),
	mPreLinkData(preGraph->Parent->LinkData),
	mPostLinkData(postGraph->Parent->LinkData)
{
}
	
	
//! @brief Push data to link
//! @param cxt Used to push link data
UBOOL SubstanceAir::Details::LinkDataStacking::push(LinkContext& cxt) const
{
	// Push this stack level in context, popped at deletion (RAII)
	LinkContext::StackLevel level(cxt,*this);

	// Push pre to link
	if (mPreLinkData.get()==NULL || !mPreLinkData->push(cxt))
	{
		// TODO2 Error: cannot push pre
		check(0);
		return false;
	}
	
	// Switch to post
	level.beginPost();

	// Push post to link
	if (mPostLinkData.get()==NULL || !mPostLinkData->push(cxt))
	{
		// TODO2 Error: cannot push post
		check(0);
		return false;
	}
	
	// Push connections
	SBS_VECTOR_FOREACH (const ConnectionsOptions::PairInOut& c,mOptions.mConnections)
	{
		int err = substanceLinkerConnectOutputToInput(
			cxt.handle,
			LinkContext::translate(level.trPostInputs,c.second).first,
			LinkContext::translate(level.trPreOutputs,c.first).first);
			
		if (err)
		{
			// TODO2 Warning: connect failed
			check(0);
		}
	}
	
	// Fuse all $ prefixed inputs
	SBS_VECTOR_FOREACH (const ConnectionsOptions::PairInOut &fuse,mFuseInputs)
	{
		LinkContext::TrResult trres = cxt.translateUid(false,fuse.first,true);
		check(trres.second);
		if (trres.second)
		{
			substanceLinkerFuseInputs(
				cxt.handle,
				trres.first,
				LinkContext::translate(level.trPostInputs,fuse.second).first);
		}
	}
	
	return true;
}
