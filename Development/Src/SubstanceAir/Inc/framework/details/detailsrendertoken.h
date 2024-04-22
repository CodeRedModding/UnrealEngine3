//! @file detailsrendertoken.h
//! @brief Substance Air rendering result token
//! @author Christophe Soum - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSRENDERTOKEN_H
#define _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSRENDERTOKEN_H

#include "SubstanceAirUncopyable.h"
#include "framework/renderresult.h"

namespace SubstanceAir
{

namespace Details
{

//! @brief Substance rendering result shell struct
//! Queued in OutputInstance
class RenderToken : Uncopyable
{
public:

	//! @brief Constructor
	RenderToken();

	//! @brief Destructor
	//! Delete render result if present
	~RenderToken();

	//! @brief Fill render result (grab ownership)
	void fill(RenderResult* renderResult);
	
	//! @brief Increment push and render counter
	void incrRef();
	
	//! @brief Decrement render counter
	void canceled();
	
	//! @brief Return true if can be removed from OutputInstance queue
	//! Can be removed if NULL filled or push count is >1 or render count ==0
	//! @post Decrement push counter
	UBOOL canRemove();
	
	//! @brief Return if already computed
	UBOOL isComputed() const { return mFilled; }
	
	//! @brief Return render result or NULL if pending, transfer ownership
	//! @post mRenderResult becomes NULL
	RenderResult* grabResult();
	
protected:

	//! @brief The pointer on render result or NULL if grabbed/skipped/pending
	RenderResult *volatile mRenderResult;

	//! @brief True if filled, can be removed in OutputInstance
	volatile UBOOL mFilled;
	
	//! @brief Pushed in OutputInstance count
	size_t mPushCount;
	
	//! @brief Render pending count (not canceled)
	size_t mRenderCount;

};  // class RenderToken


} // namespace Details
} // namespace SubstanceAir

#endif //_SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSRENDERTOKEN_H
