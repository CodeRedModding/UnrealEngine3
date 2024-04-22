//! @file detailsrendertoken.cpp
//! @brief Substance Air rendering result token
//! @author Christophe Soum - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include "Engine.h"

#include "framework/renderresult.h"
#include "framework/details/detailsrendertoken.h"
#include "framework/details/detailssync.h"


//! @brief Constructor
SubstanceAir::Details::RenderToken::RenderToken() :
	mRenderResult(NULL),
	mFilled(FALSE),
	mPushCount(1),
	mRenderCount(1)
{
}


//! @brief Destructor
//! Delete render result if present
SubstanceAir::Details::RenderToken::~RenderToken()
{
	if (mRenderResult!=NULL)
	{
		delete mRenderResult;
	}
}


//! @brief Fill render result (grab ownership)
void SubstanceAir::Details::RenderToken::fill(RenderResult* renderResult)
{
	RenderResult* prevrres = (RenderResult*)Sync::interlockedSwapPointer(
		(void*volatile*)&mRenderResult,
		renderResult);

	mFilled = true;              // do not change affectation order
	
	if (prevrres!=NULL)
	{
		delete prevrres;
	}
}


//! @brief Increment push and render counter
void SubstanceAir::Details::RenderToken::incrRef()
{
	++mPushCount;
	++mRenderCount;
}


//! @brief Decrement render counter
void SubstanceAir::Details::RenderToken::canceled()
{
	check(mRenderCount>0);
	--mRenderCount;
}


//! @brief Return true if can be removed from OutputInstance queue
//! Can be removed if NULL filled or push count is >1 or render count ==0
//! @post Decrement push counter
UBOOL SubstanceAir::Details::RenderToken::canRemove()
{
	if ((mFilled && mRenderResult==NULL) ||    // do not change && order
		mPushCount>1 || 
		mRenderCount==0)
	{
		check(mPushCount>0);
		--mPushCount;
		return TRUE;
	}
	
	return FALSE;
}


//! @brief Return render result or NULL if pending, transfer ownership
//! @post mRenderResult becomes NULL
SubstanceAir::RenderResult* SubstanceAir::Details::RenderToken::grabResult()
{
	RenderResult* res = NULL;
	if (mRenderResult!=NULL && mRenderCount>0)
	{
		res = (RenderResult*)Sync::interlockedSwapPointer(
			(void*volatile*)&mRenderResult,
			NULL);
	}
	
	return res;
}


