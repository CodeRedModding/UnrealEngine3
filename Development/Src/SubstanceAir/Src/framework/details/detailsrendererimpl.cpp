//! @file detailsrendererimpl.cpp
//! @brief The Substance Air renderer class implementation
//! @author Christophe Soum - Allegorithmic
//! @date 20111103
//! @copyright Allegorithmic. All rights reserved.

#include "Engine.h"

#include "framework/details/detailsduplicatejob.h"
#include "framework/details/detailsrendererimpl.h"
#include "framework/details/detailsrenderjob.h"
#include "framework/details/detailsoutputsfilter.h"

#include "framework/renderer.h"

#include <algorithm>


//! @brief Default constructor
SubstanceAir::Details::RendererImpl::RendererImpl() :
	mCurrentJob(NULL),
	mState(State_UserWait),
	mHold(false),
	mCallbacks(NULL)
{
	Sync::mutex::scoped_lock slock(mMainMutex);
	
	// Create thread
	mThread = Sync::thread(&renderThread,this);
	
	// Wait to be wakeup by rendering thread (start hand shake)
	mCondVarUser.wait(slock);
}


//! @brief Destructor
SubstanceAir::Details::RendererImpl::~RendererImpl()
{
	// Switch to exiting state (wake up render thread if necessary)
	{
		Sync::mutex::scoped_lock slock(mMainMutex);
		wakeup(State_Exiting);
	}
	
	// Stop engine
	mEngine.stop();

	// Wait for thread stop
	mThread.join();
	
	// Delete render jobs
	for (size_t i=0; i<mRenderJobs.size(); i++)
		delete mRenderJobs[i];
}


//! @brief Push graph instance current changes to render
//! @param graphInstance The instance to push dirty outputs
//! @return Return true if at least one dirty output
BOOL SubstanceAir::Details::RendererImpl::push(FGraphInstance* graphInstance)
{
	if (mRenderJobs.empty() || 
		mRenderJobs.back()->getState()!=RenderJob::State_Setup)
	{
		// Create setup render job if necessary
		mRenderJobs.push_back(new RenderJob(mCallbacks));
	}

	return mRenderJobs.back()->push(mStates[graphInstance],graphInstance);
}


//! @brief Launch computation
//! @param options RunOptions flags combination
//! @return Return UID of render job or 0 if not pushed computation to run
UINT SubstanceAir::Details::RendererImpl::run(
	unsigned int options)
{
	// Cleanup deprecated jobs
	cleanup();

	if (mRenderJobs.empty() ||
		mRenderJobs.back()->getState()!=RenderJob::State_Setup ||
		mRenderJobs.back()->isEmpty())
	{
		// No pushed jobs
		return 0;
	}

	// Skip already canceled jobs
	// and currently computed job if Run_PreserveRun flag is present
	RenderJob *curjob = mCurrentJob;    // Current job (can diverge)
	RenderJob *lastjob = curjob;        // Last job in render list
	while (curjob!=NULL && 
		(curjob->isCanceled() ||
		((options&Renderer::Run_PreserveRun)!=0 &&
		curjob->getState()==RenderJob::State_Computing)))	
	{
		lastjob = curjob;
		curjob = curjob->getNextJob();
	}

	// Retrieve last job in render list
	while (lastjob!=NULL && lastjob->getNextJob()!=NULL)
	{
		lastjob = lastjob->getNextJob();
	}
	
	RenderJob *newjob = mRenderJobs.back(); // Currently pushed render job
	RenderJob *begjob = newjob;             // List of new jobs: begin
	
	// Fill list of graphs to link
	newjob->snapshotStates(mStates);
		
	if (curjob!=NULL && 
		(options&(Renderer::Run_Replace|Renderer::Run_First))!=0 &&
		curjob->cancel(true))      // Proceed only if really canceled (diverge)
	{
		// Non empty render job list
		// And must cancel previous computation
		
		if ((options&Renderer::Run_PreserveRun)==0)
		{
			// Stop engine if current computation is not preserved
			mEngine.stop();
		}
		
		const bool newfirst = (options&Renderer::Run_First)!=0; 
		std::auto_ptr<OutputsFilter> filter((options&Renderer::Run_Replace)!=0 ? 
			new OutputsFilter(*newjob) : 
			NULL);
			
		// Duplicate job context / accumulation structure
		DuplicateJob dup(
			newjob->getLinkGraphs(),
			filter.get(),
			mStates,
			newfirst);   // Update state if new first
		
		if (!newfirst)
		{
			mRenderJobs.pop_back();       // Insert other jobs before
		}
		
		// Accumulate reversed state -> just before curjob
		RenderJob *srcjob;
		for (srcjob=curjob;srcjob!=NULL;srcjob=srcjob->getNextJob()) 
		{
			srcjob->prependRevertedDelta(dup);
		}
		
		// Duplicate job
		// Iterate on canceled jobs
		RenderJob *prevdupjob = NULL;
		for (srcjob=curjob;srcjob!=NULL;srcjob=srcjob->getNextJob()) 
		{
			// Duplicate canceled
			RenderJob *dupjob = new RenderJob(*srcjob,dup,mCallbacks);
				
			if (dupjob->isEmpty())
			{
				// All outputs filtered
				delete dupjob;
			}
			else 
			{
				// Valid, at least one output
				if (prevdupjob==NULL)
				{
					// First job created, update inputs state
					if (newfirst)
					{	
						// Activate duplicated job w/ current as previous
						dupjob->activate(newjob);
					}
					else
					{
						// Record as first
						begjob = dupjob;
					}   
				}
				else
				{
					check(prevdupjob!=NULL);
					dupjob->activate(prevdupjob);
				}
				
				prevdupjob = dupjob;      // Store prev for next activation
			}	   
		}
		
		// Push new job at the end except if First flag is set
		if (!newfirst)
		{
			// New job at the end of list
			mRenderJobs.push_back(newjob);   // At the end
			
			// Activate
			if (begjob!=newjob)              
			{
				// Not in case that nothing pushed
				check(prevdupjob!=NULL);
				newjob->activate(prevdupjob);  // Activate (last, not first)
			}
			
			check(!dup.hasDelta());         // State must be reverted
		}
	}
		
	// Activate the first job (render thread can view and consume them)
	// Active the first in LAST! Otherwise thread unsafe behavior
	begjob->activate(lastjob);
		
	{
		// Thread safe modifications	
		Sync::mutex::scoped_lock slock(mMainMutex);
		if (mCurrentJob==NULL &&                           // No more computation
			begjob->getState()==RenderJob::State_Pending)  // Not already processed
		{
			// Set as current job if no current computations
			mCurrentJob = begjob;
		}
		
		const bool synchrun = (options&Renderer::Run_Asynchronous)==0;
		mHold = mHold && !synchrun;
		if (!mHold)
		{
			// Wake up render thread if necessary
			wakeup(synchrun ? 
				State_UserWait : 
				State_OnGoing);
		}
		if (synchrun)
		{
			// If synchronous run
			// Wait to be wakeup by rendering thread (render finished)
			mCondVarUser.wait(slock);
		}
	}
	
	return newjob->getUid();
}


//! @brief Cancel a computation or all computations
//! @param runUid UID of the render job to cancel (returned by run()), set
//!		to 0 to cancel ALL jobs.
//! @return Return true if the job is retrieved (pending)
BOOL SubstanceAir::Details::RendererImpl::cancel(UINT runUid)
{
	BOOL hasCancel = FALSE;
	Sync::mutex::scoped_lock slock(mMainMutex);
	
	if (mCurrentJob!=NULL)
	{
		if (runUid!=0)
		{	
			// Search for job to cancel
			for (RenderJob *rjob=mCurrentJob;rjob!=NULL;rjob=rjob->getNextJob())
			{
				if (runUid==rjob->getUid())
				{
					// Cancel this job
					hasCancel = rjob->cancel();
					break;
				}
			}
		}
		else
		{
			// Cancel all
			hasCancel = mCurrentJob->cancel(TRUE);
		}
		
		if (hasCancel)
		{
			// Stop engine if necessary
			mEngine.stop();
		}
	}
	
	return hasCancel;
}


//! @brief Return if a computation is pending
//! @param runUid UID of the render job to retreive state (returned by run())
BOOL SubstanceAir::Details::RendererImpl::isPending(UINT runUid) const
{
	SBS_VECTOR_REVERSE_FOREACH (const RenderJob*const renderJob,mRenderJobs)
	{
		if (renderJob->getUid()==runUid)
		{
			return (renderJob->getState()==RenderJob::State_Pending || 
				renderJob->getState()==RenderJob::State_Computing) &&
				!renderJob->isCanceled();
		}
	}
	
	return false;
}


//! @brief Hold rendering
void SubstanceAir::Details::RendererImpl::hold()
{
	mHold = TRUE;
	mEngine.stop();
}


//! @brief Continue held rendering
void SubstanceAir::Details::RendererImpl::resume()
{
	Sync::mutex::scoped_lock slock(mMainMutex);
	
	if (mHold && mCurrentJob!=NULL)
	{
		wakeup(State_OnGoing);
	}
	
	mHold = FALSE;
}


//! @brief Set user callbacks
//! @param callbacks Pointer on the user callbacks concrete structure 
//! 	instance or NULL.
//! @warning The callbacks instance must remains valid until all
//!		render job created w/ this callback instance set are processed.
void SubstanceAir::Details::RendererImpl::setCallbacks(Callbacks* callbacks)
{
	mCallbacks = callbacks;
}


//! @brief Wake up rendering thread if necessary
//! @param state New state
//! @pre mMainMutex Must be locked
//! @note Called from user thread
void SubstanceAir::Details::RendererImpl::wakeup(State state)
{
	State prevState = mState;
	mState = state;
	if (prevState==State_RenderWait)
	{
		mCondVarRender.notify_one(); 
	}
}


//! @brief Clean consumed render jobs
//! @note Called from user thread
void SubstanceAir::Details::RendererImpl::cleanup()
{
	while (!mRenderJobs.empty() &&
		mRenderJobs.front()->getState()==RenderJob::State_Done)
	{
		delete mRenderJobs.front();
		mRenderJobs.pop_front();
	}
}


//! @brief Rendering thread call function
void SubstanceAir::Details::RendererImpl::renderThread(RendererImpl* impl)
{
	impl->renderLoop();
}


//! @brief Rendering loop function
void SubstanceAir::Details::RendererImpl::renderLoop()
{
	RenderJob* nextJob = NULL;       // Next job to proceeed
	bool nextAvailable = FALSE;      // Flag: next job available
	
	while (1)
	{
		// Job processing loop, exit when Renderer is deleted
		{
			// Thread safe operations
			Sync::mutex::scoped_lock slock(mMainMutex); // mState/mHold safety
			
			if (nextAvailable)
			{
				// Jump to next job
				check(mCurrentJob!=NULL);
				mCurrentJob = nextJob;
				nextAvailable = FALSE;
			}
			
			while (mState==State_Exiting || mHold || mCurrentJob==NULL)
			{
				switch (mState)
				{
					case State_UserWait:      // Wake up user thread
						mCondVarUser.notify_one(); break;   
					case State_Exiting:       // Exit function
						return;
				}

				mState = State_RenderWait;
				mCondVarRender.wait(slock);               // Wait to be wakeup
			} 
		}
		
		// Process current job
		nextJob = mCurrentJob->process(mEngine);
		nextAvailable = nextJob!=mCurrentJob;
	}
}

