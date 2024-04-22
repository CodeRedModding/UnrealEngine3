//! @file detailsdeltastate.cpp
//! @brief Substance Air Framework Instance Inputs Delta State definition
//! @author Christophe Soum - Allegorithmic
//! @date 20111115
//! @copyright Allegorithmic. All rights reserved.
//!
 
#include "framework/details/detailsdeltastate.h"
#include "framework/details/detailsrenderjob.h"
#include "framework/details/detailsgraphstate.h"

#include "SubstanceAirGraph.h"

#include <algorithm>
#include <iterator>

	
//! @brief Fill a delta state from current state & current instance values
//! @param graphState The current graph state used to generate delta
//! @param graphInstance The pushed graph instance (not keeped)
void SubstanceAir::Details::DeltaState::fill(
	const GraphState &graphState,
	const FGraphInstance* graphInstance)
{
	size_t inpindex = 0;
	SubstanceAir::List<std::tr1::shared_ptr<input_inst_t>>::TConstIterator
		ItIn(graphInstance->Inputs.itfrontconst());
	
	for	(;ItIn;++ItIn)
	{
		const InputState &inpst = graphState[inpindex];

		check((*ItIn).get()->Desc->Type==inpst.getType());
		const UBOOL ismod = (*ItIn).get()->isModified(inpst.value.numeric);
		
		if (ismod || (*ItIn).get()->UseCache)
		{
			// Different or cache forced, create delta entry
			mInputs.resize(mInputs.size()+1);
			Input &inpdelta = mInputs.back();
			inpdelta.index = inpindex;
			inpdelta.modified.flags = inpst.getType();
			inpdelta.previous = inpst;
			
			if (inpst.isImage() && 
				inpst.value.imagePtrIndex!=InputState::invalidIndex)
			{
				// Save previous pointer
				inpdelta.previous.value.imagePtrIndex = mImageInputPtrs.size();
				mImageInputPtrs.push_back(
					graphState.getInputImage(inpst.value.imagePtrIndex));
			}
			
			if (ismod)
			{
				// Really modified, 
				inpdelta.modified.fillValue(ItIn->get(), mImageInputPtrs);
			}
			else
			{
				// Only for cache
				inpdelta.modified.value = inpdelta.previous.value;
				inpdelta.modified.flags |= InputState::Flag_CacheOnly;
			}
			
			if (!(*ItIn).get()->IsHeavyDuty)
			{
				inpdelta.modified.flags |= InputState::Flag_Cache;
			}
		}
		
		++inpindex;
	}
}


//! @brief Append a delta state to current
//! @param src Source delta state to append
//! @param mode Append policy flag
void SubstanceAir::Details::DeltaState::append(
	const DeltaState &src,
	AppendMode mode)
{
	Inputs::const_iterator previte = mInputs.begin();
	Inputs newinp;
	newinp.reserve(src.getInputs().size()+mInputs.size());
	
	SBS_VECTOR_FOREACH (const Input& srcinp,src.getInputs())
	{
		while (previte!=mInputs.end() && previte->index<srcinp.index)
		{
			// Copy directly non modified items
			newinp.push_back(*previte);
			++previte;
		}
		
		if (previte!=mInputs.end() && previte->index==srcinp.index)
		{
			// Collision
			switch (mode)
			{
				case Append_Default:
					newinp.push_back(*previte);   // Existing used
				break;
				
				case Append_Reverse:
					newinp.push_back(*previte);    // Existing used for modified
					newinp.back().previous = srcinp.modified; // Update previous
					recordPtr(newinp.back().previous,src.mImageInputPtrs);
				break;
				
				case Append_Override:
					if (!isEqual(
						srcinp.modified,
						src.mImageInputPtrs,
						previte->previous,
						mImageInputPtrs))
					{
						// Combine only if no identity
						newinp.push_back(*previte); // Existing used for previous
						newinp.back().modified = srcinp.modified; // Update modified
						recordPtr(newinp.back().modified,src.mImageInputPtrs);
					}
				break;
			}
			
			++previte;
		}
		else
		{
			// Insert (into newinp)
			newinp.push_back(srcinp);
			if (mode==Append_Reverse)
			{
				std::swap(newinp.back().previous,newinp.back().modified);
			}

			recordPtr(newinp.back().modified,src.mImageInputPtrs);
			recordPtr(newinp.back().previous,src.mImageInputPtrs);
		}
	}

	std::copy(
		previte,
		const_cast<const Inputs&>(mInputs).end(),
		std::back_inserter(newinp));    // copy remains
	mInputs = newinp;                   // replace inputs
}


//! @brief Record new image pointer if necessary
//! @param[in,out] inputState The state to record image pointer
//! @param srcImgPtrs Source image pointers (used if dst is valid image)
void SubstanceAir::Details::DeltaState::recordPtr(
	InputState& inputState,
	const ImageInputPtrs &srcImgPtrs)
{
	const size_t srcindex = inputState.value.imagePtrIndex;
	if (inputState.isImage() && srcindex!=InputState::invalidIndex)
	{
		inputState.value.imagePtrIndex = mImageInputPtrs.size();
		mImageInputPtrs.push_back(srcImgPtrs[srcindex]);
	}
}


//! @brief Return if two input states are equal
//! @param a The first input state to compare
//! @param aImgPtrs a image pointers (used if a is valid image)
//! @param b The second input state to compare
//! @param bImgPtrs b image pointers (used if b is valid image)
UBOOL SubstanceAir::Details::DeltaState::isEqual(
	const InputState& a,
	const ImageInputPtrs &aImgPtrs,
	const InputState& b,
	const ImageInputPtrs &bImgPtrs)
{
	check(a.isImage()==b.isImage());

	if (a.isImage())
	{
		// Image pointer equality
		const bool avalid = a.value.imagePtrIndex!=InputState::invalidIndex;
		if (avalid!=(b.value.imagePtrIndex!=InputState::invalidIndex))
		{
			return false;        // One valid, not the other
		}
		
		return !avalid ||
			aImgPtrs[a.value.imagePtrIndex]==bImgPtrs[b.value.imagePtrIndex];
	}
	else
	{
		// Numeric equality
		return std::equal(
			a.value.numeric,
			a.value.numeric+getComponentsCount(a.getType()),
			b.value.numeric) ? TRUE : FALSE;
	}
}


