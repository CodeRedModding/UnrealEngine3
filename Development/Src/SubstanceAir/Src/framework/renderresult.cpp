//! @file renderresult.cpp
//! @author Antoine Gonzalez - Allegorithmic
//! @date 20110105
//! @copyright Allegorithmic. All rights reserved.

#include "framework/renderresult.h"


SubstanceAir::RenderResult::RenderResult(
		SubstanceContext* context,
		const SubstanceTexture& texture) :
	mContext(context),
	mSubstanceTexture(texture)
{
}


//! @brief Destructor
//! Free the buffer contained in mSubstanceTexture if not previosly grabbed
//! by releaseBuffer().
SubstanceAir::RenderResult::~RenderResult()
{
	if (mSubstanceTexture.buffer!=NULL)
	{
		substanceContextMemoryFree(getContext(),mSubstanceTexture.buffer);
	}
}


//! @brief Grab the pixel data buffer, the pointer is set to NULL
//! @warning The ownership of the buffer is transferred to the caller.
//! 	The buffer must be free by substanceContextMemoryFree(), see
//!		substance/context.h for further information.
//! @return The buffer, or NULL if already released
void* SubstanceAir::RenderResult::releaseBuffer()
{
	void* buffer = mSubstanceTexture.buffer;
	mSubstanceTexture.buffer = NULL;
	return buffer;
}
