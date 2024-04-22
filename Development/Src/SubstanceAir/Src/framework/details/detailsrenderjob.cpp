//! @file detailsrenderjob.cpp
//! @brief Substance Air Framework Render Job implementation
//! @author Christophe Soum - Allegorithmic
//! @date 20111107
//! @copyright Allegorithmic. All rights reserved.
//!

#include "SubstanceAirGraph.h"

#include "framework/details/detailsrenderjob.h"
#include "framework/details/detailsduplicatejob.h"
#include "framework/details/detailsrenderpushio.h"
#include "framework/details/detailsgraphstate.h"
#include "framework/details/detailsstates.h"
#include "framework/details/detailsengine.h"
#include "framework/details/detailscomputation.h"

#include <algorithm>


namespace SubstanceAir
{
namespace Details
{

static UINT gInstanceUid = 0x0;

}   // namespace Details
}   // namespace SubstanceAir



//! @brief Constructor
//! @param callbacks User callbacks instance (or NULL if none)
SubstanceAir::Details::RenderJob::RenderJob(
		Callbacks *callbacks) :
	mUid((++Details::gInstanceUid)|0x80000000u),
	mState(State_Setup),
	mCanceled(false),
	mNextJob(NULL),
	mCallbacks(callbacks)
{
}


//! @brief Constructor from job to duplicate and outputs filtering
//! @param src The canceled job to copy
//! @param dup Duplicate job context
//! @param callbacks User callbacks instance (or NULL if none)
//!
//! Build a pending job from a canceled one and optionnaly pointer on an
//! other job used to filter outputs.
//! Push again SRC render tokens (of not filtered outputs).
//! @warning Resulting job can be empty if all outputs are filtered. In this
//!		case this job is no more usefull and can be removed.
//! @note Called from user thread
SubstanceAir::Details::RenderJob::RenderJob(
		const RenderJob& src,
		DuplicateJob& dup,
		Callbacks *callbacks) :
	mUid(src.getUid()),
	mState(State_Setup),
	mCanceled(false),
	mNextJob(NULL),
	mLinkGraphs(dup.linkGraphs),
	mCallbacks(callbacks)
{
	mRenderPushIOs.reserve(src.mRenderPushIOs.size());
	
	SBS_VECTOR_FOREACH (RenderPushIO *srcpushio,src.mRenderPushIOs)
	{
		RenderPushIO *newpushio = 
			new RenderPushIO(*this,*srcpushio,dup);
		if (newpushio->hasOutputs())
		{
			// Has outputs, push it in the list
			mRenderPushIOs.push_back(newpushio);
		}
		else
		{
			// No outputs, all filtered
			delete newpushio;
		}
	}
}


//! @brief Destructor
SubstanceAir::Details::RenderJob::~RenderJob()
{
	// Delete RenderPushIO elements
	for (size_t i=0; i<mRenderPushIOs.size(); i++)
	{
		delete mRenderPushIOs[i];
	}
}


//! @brief Take states snapshot (used by linker)
//! @param states Used to take a snapshot states to use at link time
//!
//! Must be done before activate it
void SubstanceAir::Details::RenderJob::snapshotStates(const States &states)
{
	states.fill(mLinkGraphs);        // Create snapshot from current states
	check(!mLinkGraphs.graphStates.empty());
}


//! @brief Push I/O to render: from current state & current instance
//! @param graphState The current graph state
//! @param graphInstance The pushed graph instance (not keeped)
//! @pre Job must be in 'Setup' state
//! @note Called from user thread 
//! @return Return true if at least one dirty output
//!
//! Update states, create render tokens.
bool SubstanceAir::Details::RenderJob::push(
	GraphState &graphState,
	FGraphInstance* graphInstance)
{
	check(State_Setup==mState);

	// Get push IO index
	UINT pushioindex = mStateUsageCount[graphState.getUid()]++;
	
	check(pushioindex<=mRenderPushIOs.size());
	const bool newpushio = mRenderPushIOs.size()==(size_t)pushioindex;
	if (newpushio)
	{
		// Create new push IO
		mRenderPushIOs.push_back(new RenderPushIO(*this));
	}
	
	// Push!
	if (mRenderPushIOs.at(pushioindex)->push(graphState,graphInstance))
	{
		return true;
	}
	else if (newpushio)
	{
		// No dirty outputs: Remove just created, not necessary
		delete mRenderPushIOs.back();
		
		mRenderPushIOs.pop_back();
		--mStateUsageCount[graphState.getUid()];
	}
	
	return false;
}


//! @brief Put the job in render state, build the render chained list
//! @param previous Previous job in render list or NULL if first
//! @pre Activation must be done in reverse order, this next job is already
//!		activated.
//! @pre Job must be in 'Setup' state
//! @post Job is in 'Pending' state
//! @note Called from user thread
void SubstanceAir::Details::RenderJob::activate(RenderJob* previous)
{
	check(State_Setup==mState);
	check(!mLinkGraphs.graphStates.empty());
	
	mState = State_Pending;
	
	if (previous!=NULL)
	{
		check(previous->mNextJob==NULL);
		previous->mNextJob = this;
	}
}


//! @brief Process current job and next ones
//! @param engine The engine used for rendering
//! @note Called from render thread
//! @post Job is in 'Done' state
//! @warning The job can be deleted by user thread BEFORE returning process().
//! @return Return next job to proceed if the job is fully completed 
//!		(may be NULL) and is possible to jump to next job. Return THIS if
//!		NOT completed.
SubstanceAir::Details::RenderJob*
SubstanceAir::Details::RenderJob::process(Engine& engine)
{
	// Iterate on jobs to render, find last one
	// Check if link required
	RenderJob* lastjob = this;
	{
		bool linkRequired = false;
		do
		{
			linkRequired = linkRequired || lastjob->isLinkNeeded();
			if (lastjob->getNextJob()==NULL)
			{
				break;
			}
			lastjob = lastjob->getNextJob();
		}
		while (true);
		
		if (linkRequired)
		{
			// Link if necessary (can diverge, render job list can be longer)
			if (false == engine.link(this))
			{
				debugf(TEXT("Substance: failed to link Substance, skipping rendering."));
				return NULL;
			}
		}
	}
	
	// Compute
	{
		Computation computation(engine);
		
		// Push all I/O to engine
		RenderJob* curjob = this;
		do
		{
			// Push I/O
			curjob->pull(computation);
		}
		while (curjob!=lastjob && (curjob=curjob->getNextJob())!=NULL);

		computation.run();
	}
	
	// Mark completed jobs as Done
	bool allcomplete = false;
	RenderJob* curjob = this;
	while (!allcomplete && curjob->isComplete())
	{
		RenderJob* nextjob = curjob->getNextJob();
		curjob->mState = State_Done;       // may be destroyed just here!
		allcomplete = curjob==lastjob;
		curjob = nextjob;
	}

	return curjob;
}


//! @brief Cancel this job (or this job and next ones)
//! @param cancelList If true this job and next ones (mNextJob chained list)
//! 	are canceled (in reverse order).
//! @note Called from user thread
//! @return Return true if at least one job is effectively canceled
//!
//! Cancel if Pending or Computing.
//! Outputs of canceled jobs are not computed, only inputs are set (state
//! coherency). RenderToken's are notified as canceled.
BOOL SubstanceAir::Details::RenderJob::cancel(BOOL cancelList)
{
	BOOL res = false;
	if (cancelList && mNextJob!=NULL)
	{
		// Recursive call, reverse chained list order canceling
		res = mNextJob->cancel(TRUE);
	}
	
	if (!mCanceled)
	{
		res = TRUE;
		mCanceled = TRUE;
		
		// notify cancel for each push I/O (needed for RenderToken counter decr)
		SBS_VECTOR_REVERSE_FOREACH (RenderPushIO *pushio,mRenderPushIOs)
		{
			pushio->cancel();
		}
	}
	
	return res;
}


//! @brief Prepend reverted input delta into duplicate job context
//! @param dup The duplicate job context to accumulate reversed delta
//! Use to restore the previous state of copied jobs.
//! @note Called from user thread
void SubstanceAir::Details::RenderJob::prependRevertedDelta(
	DuplicateJob& dup) const
{
	SBS_VECTOR_FOREACH (const RenderPushIO *pushio,mRenderPushIOs)
	{
		pushio->prependRevertedDelta(dup);
	}
}


//! @brief Fill filter outputs structure from this job
//! @param[in,out] filter The filter outputs structure to fill
void SubstanceAir::Details::RenderJob::fill(OutputsFilter& filter) const
{
	SBS_VECTOR_FOREACH (RenderPushIO *pushio,mRenderPushIOs)
	{
		pushio->fill(filter);
	}
}


//! @brief Push input and output in engine handle
void SubstanceAir::Details::RenderJob::pull(Computation &computation)
{
	mState = State_Computing;
	const bool canceled = mCanceled; 
	
	SBS_VECTOR_FOREACH (RenderPushIO *pushio,mRenderPushIOs)
	{
		pushio->pull(computation,canceled);    // If canceled, only inputs
	}
}


//! @brief Is render job complete
//! @return Return if all push I/O completed
bool SubstanceAir::Details::RenderJob::isComplete() const
{
	SBS_VECTOR_REVERSE_FOREACH (const RenderPushIO *pushio,mRenderPushIOs)
	{
		// Reverse test, ordered computation
		const RenderPushIO::Complete complete = pushio->isComplete(mCanceled);
		
		// Complete if last push I/O complete or only Inputs pushed only if
		// job canceled or no outputs to push
		if (RenderPushIO::Complete_DontKnow!=complete)
		{
			return RenderPushIO::Complete_Yes==complete;
		}
	}
	
	return true;	
}


//! @brief In linking needed
//! @return Return if at least one graph state need to be linked
bool SubstanceAir::Details::RenderJob::isLinkNeeded() const
{
	if (!mRenderPushIOs.empty())
	{
		// Test only first one, others use graph state subset
		return mRenderPushIOs.front()->isLinkNeeded();
	}
	
	return false;
}

