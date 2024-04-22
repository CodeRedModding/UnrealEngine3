//! @file detailsrendererimpl.h
//! @brief The Substance renderer concrete implementation class
//! @author Christophe Soum - Allegorithmic
//! @date 20111102
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSRENDERERIMPL_H
#define _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSRENDERERIMPL_H

#include "detailsstates.h"
#include "detailsengine.h"
#include "detailssync.h"

#include "SubstanceAirGraph.h"
#include "Engine.h"

#include <deque>


namespace SubstanceAir
{
namespace Details
{


//! @brief Concrete renderer implementation
class RendererImpl
{
public:
	//! @brief Default constructor
	RendererImpl();
	
	//! @brief Destructor
	~RendererImpl();
	
	//! @brief Push graph instance current changes to render
	//! @param graphInstance The instance to push dirty outputs
	//! @return Return true if at least one dirty output
	BOOL push(FGraphInstance* graphInstance);
	
	//! @brief Launch computation
	//! @param options Renderer::RunOption flags combination
	//! @return Return UID of render job or 0 if not pushed computation to run
	UINT run(unsigned int options);
	
	//! @brief Cancel a computation or all computations
	//! @param runUid UID of the render job to cancel (returned by run()), set
	//!		to 0 to cancel ALL jobs.
	//! @return Return true if the job is retrieved (pending)
	BOOL cancel(UINT runUid = 0);
	
	//! @brief Return if a computation is pending
	//! @param runUid UID of the render job to retrieve state (returned by run())
	BOOL isPending(UINT runUid) const;
	
	//! @brief Hold rendering
	void hold();
	
	//! @brief Continue held rendering
	void resume();
	
	//! @brief Set user callbacks
	//! @param callbacks Pointer on the user callbacks concrete structure 
	//! 	instance or NULL.
	//! @warning The callbacks instance must remains valid until all
	//!		render job created w/ this callback instance set are processed.
	void setCallbacks(Callbacks* callbacks);
	
protected:
	//! @brief States enumeration
	enum State
	{
		State_UserWait,   //!< User thread waiting (blocked in mCondVarUser)
		State_OnGoing,    //!< Processing/ready to process
		State_RenderWait, //!< Render thread waiting (blocked in mCondVarRender)
		State_Exiting     //!< Destructing
	};

	//! @brief Render jobs list container
	typedef std::deque<RenderJob*> RenderJobs;
	

	//! @brief Current graphes state
	States mStates;
	
	//! @brief Engine
	Engine mEngine;
	
	//! @brief Render jobs list
	//! @note Container modification are always done in user thread
	RenderJobs mRenderJobs;
	
	//! @brief First render job to proceed by render thread
	//! R/W access thread safety ensure by mMainMutex.
	//! If NULL Render thread is waiting for pending render job (rendering 
	//! thread is blocked in mCondVarRender).
	RenderJob *volatile mCurrentJob;
	
	//! @brief Current renderer state (hold, exiting, etc.)
	//! R/W access thread safety ensure by mMainMutex.
	volatile State mState;
	
	//! @brief Currently hold
	volatile BOOL mHold;
	
	//! @brief Condition variable used by render thread for waiting
	Sync::condition_variable mCondVarRender;
	
	//! @brief Condition variable used by user thread for waiting
	Sync::condition_variable mCondVarUser;

	//! @brief Mutex for mCurrentJob R/W access
	Sync::mutex mMainMutex;
	
	//! @brief Rendering thread
	Sync::thread mThread;
	
	//! @brief Current User callbacks instance (can be NULL, none)
	Callbacks* mCallbacks;
	
	
	//! @brief Wake up rendering thread if necessary
	//! @param state New state
	//! @pre mMainMutex Must be locked
	//! @note Called from user thread
	void wakeup(State state);
	
	//! @brief Clean consumed render jobs
	//! @note Called from user thread
	void cleanup();
	
	//! @brief Rendering thread call function
	static void renderThread(RendererImpl*);
	
	//! @brief Rendering loop function
	void renderLoop();
};


} // namespace Details
} // namespace SubstanceAir

#endif // _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSRENDERERIMPL_H
