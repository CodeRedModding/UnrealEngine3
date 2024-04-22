//! @file renderresult.h
//! @brief Substance Air rendering result
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_AIR_RENDERRESULT_H
#define _SUBSTANCE_AIR_RENDERRESULT_H

#pragma pack ( push, 8 )
#include <substance/texture.h>
#include <substance/context.h>
#pragma pack ( pop )

#include "SubstanceAirUncopyable.h"

namespace SubstanceAir
{

//! @brief Substance rendering result struct
struct RenderResult : Uncopyable
{
	//! @brief Constructor, internal use only
	RenderResult(SubstanceContext*,const SubstanceTexture&);

	//! @brief Destructor
	//! Free the buffer contained in mSubstanceTexture if not previously grabbed
	//! by releaseBuffer().
	~RenderResult();

	//! @brief Accessor on the result texture
	//! Contains pixel data buffer.
	//! @warning Do not free the buffer manually, use releaseBuffer() or destroy
	//!		this class instance.
	//! @see substance/texture.h (BLEND platform version)
	const SubstanceTexture& getTexture() const { return mSubstanceTexture; }

	//! @brief Grab the pixel data buffer, the pointer is set to NULL
	//! @warning The ownership of the buffer is transferred to the caller.
	//! 	The buffer must be freed by substanceContextMemoryFree(), see
	//!		substance/context.h for further information.
	//! @return Return the buffer, or NULL if already released
	void* releaseBuffer();

	//! @brief Accessor on the Substance context to use for releasing the buffer
	SubstanceContext* getContext() const { return mContext; }
	
protected:
	SubstanceTexture mSubstanceTexture;
	SubstanceContext* mContext;
};

} // namespace SubstanceAir

#endif //_SUBSTANCE_AIR_RENDERRESULT_H
