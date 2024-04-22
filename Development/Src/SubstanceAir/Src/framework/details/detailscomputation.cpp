//! @file detailscomputation.cpp
//! @brief Substance Air Framework Computation action implementation
//! @author Christophe Soum - Allegorithmic
//! @date 20111116
//! @copyright Allegorithmic. All rights reserved.
//!

#include "framework/details/detailscomputation.h"
#include "framework/details/detailsengine.h"
#include "framework/details/detailsinputstate.h"

#include <algorithm>
#include <iterator>
	
//! @brief Constructor from engine
//! @pre Engine is valid and correctly linked
//! @post Engine render queue is flushed
SubstanceAir::Details::Computation::Computation(Engine& engine) :
	mEngine(engine)
{
	check(mEngine.getHandle()!=NULL);
	mEngine.beginPush();
}


//! @brief Push input
//! @param index SBSBIN index
//! @param inputState Input type, value and other flags
//! @param imgToken Image token pointer, used if input image
//! @return Return if input really pushed
bool SubstanceAir::Details::Computation::pushInput(
	UINT index,
	const InputState& inputState,
	ImageInputToken* imgToken)
{
	bool res = false;
	check((inputState.isImage()&&imgToken!=NULL) || imgToken==NULL);
	
	if (!inputState.isCacheOnly())
	{
		// Push input value
		int err = substanceHandlePushSetInput(
			mEngine.getHandle(),
			0,
			index,
			inputState.getType(),
			inputState.isImage() ? 
				imgToken : 
				const_cast<void*>((const void*)inputState.value.numeric),
			mUserData);
			
		if (err)
		{
			// TODO2 Warning: push input failed
			check(0);
		}
		else
		{
			res = true;
		}
	}
	
	if (inputState.isCacheEnabled())
	{
		// To push as hint
		mHintInputs.push_back(getHintInput(inputState.getType(),index));
	}
	
	return res;
}

	
//! @brief Push outputs SBSBIN indices to compute 
void SubstanceAir::Details::Computation::pushOutputs(const Indices& indices)
{
	int err = substanceHandlePushOutputs(
		mEngine.getHandle(),
		0,
		&indices[0],
		indices.size(),
		mUserData);
		
	if (err)
	{
		// TODO2 Warning: pust outputs failed
		check(0);
	}
	
	// To push as hint
	mHintOutputs.insert(mHintOutputs.end(),indices.begin(),indices.end());
}

	
//! @brief Run computation
//! Push hints I/O and run
void SubstanceAir::Details::Computation::run()
{
	SubstanceHandle* handle = mEngine.getHandle();
	
	if (!mHintInputs.empty())
	{
		// Make unique
		std::sort(mHintInputs.begin(),mHintInputs.end());
		mHintInputs.erase(std::unique(
			mHintInputs.begin(),mHintInputs.end()),mHintInputs.end());
	}

	SBS_VECTOR_FOREACH (UINT hintinp,mHintInputs)
	{
		// Push input hint
		int err = substanceHandlePushSetInput(
			handle,
			Substance_PushOpt_HintOnly,
			getIndex(hintinp),
			getType(hintinp),
			NULL,
			NULL);
			
		if (err)
		{
			// TODO2 Warning: push input failed
			check(0);
		}
	}
	
	if (!mHintOutputs.empty())
	{
		// Make unique
		std::sort(mHintOutputs.begin(),mHintOutputs.end());
		const size_t newsize = std::distance(mHintOutputs.begin(),
			std::unique(mHintOutputs.begin(),mHintOutputs.end()));
	
		// Push output hints
		int err = substanceHandlePushOutputs(
			handle,
			Substance_PushOpt_HintOnly,
			&mHintOutputs[0],
			newsize,
			NULL);
			
		if (err)
		{
			// TODO2 Warning: push outputs failed
			check(0);
		}
	}
	
	// Start computation
	int err = substanceHandleStart(handle,Substance_Sync_Synchronous);
	
	if (err)
	{
		// TODO2 Error: start failed
		check(0);
	}
}

