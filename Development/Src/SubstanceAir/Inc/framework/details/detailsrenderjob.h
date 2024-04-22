//! @file detailsrenderjob.h
//! @brief Substance Air Framework Render Job definition
//! @author Christophe Soum - Allegorithmic
//! @date 20111027
//! @copyright Allegorithmic. All rights reserved.
//!
 
#ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSRENDERJOB_H
#define _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSRENDERJOB_H

#include "detailslinkgraphs.h"
#include "detailsstates.h"

#include <vector>
#include <map>

namespace SubstanceAir
{

struct FGraphInstance;
struct Callbacks;

namespace Details
{

class Computation;
struct DuplicateJob;
class Engine;
struct OutputsFilter;
class RenderPushIO;


//! @brief Render job: set of push Input/Output to render
//! One render job corresponds to all Renderer::push beetween two run 
class RenderJob : Uncopyable
{
public:	
	//! @brief States enumeration
	enum State
	{
		State_Setup,     //!< Job currently built, initial state
		State_Pending,   //!< Not already processed by engine (to run)
		State_Computing, //!< Pushed into engine render list, not finished
		State_Done       //!< Completely processed, to destroy in user thread
	};
	

	//! @brief Constructor
	//! @param callbacks User callbacks instance (or NULL if none)
	RenderJob(Callbacks *callbacks);
	
	//! @brief Constructor from job to duplicate and outputs filtering
	//! @param src The canceled job to copy
	//! @param dup Duplicate job context
	//! @param callbacks User callbacks instance (or NULL if none)
	//!
	//! Build a pending job from a canceled one and optionnaly pointer on an
	//! other job used to filter outputs
	//! Push again SRC render tokens (of not filtered outputs).
	//! @warning Resulting job can be empty if all outputs are filtered. In this
	//!		case this job is no more usefull and can be removed.
	//! @note Called from user thread
	RenderJob(const RenderJob& src,DuplicateJob& dup,Callbacks *callbacks);

	//! @brief Destructor
	~RenderJob();
	
	//! @brief Take states snapshot (used by linker)
	//! @param states Used to take a snapshot states to use at link time
	//!
	//! Must be done before activate it
	void snapshotStates(const States &states);

	//! @brief Push I/O to render: from current state & current instance
	//! @param graphState The current graph state
	//! @param graphInstance The pushed graph instance (not keeped)
	//! @pre Job must be in 'Setup' state
	//! @note Called from user thread 
	//! @return Return true if at least one dirty output
	//!
	//! Update states, create render tokens.
	bool push(GraphState &graphState, FGraphInstance* graphInstance);
	
	//! @brief Put the job in render state, build the render chained list
	//! @param previous Previous job in render list or NULL if first
	//! @pre Activation must be done in reverse order, this next job is already
	//!		activated.
	//! @pre Job must be in 'Setup' state
	//! @post Job is in 'Pending' state
	//! @note Called from user thread
	void activate(RenderJob* previous);
	
	//! @brief Process current job and next ones
	//! @param engine The engine used for rendering
	//! @note Called from render thread
	//! @post Job is in 'Done' state
	//! @warning The job can be deleted by user thread BEFORE returning process().
	//! @return Return next job to proceed if the job is fully completed 
	//!		(may be NULL) and is possible to jump to next job. Return THIS if
	//!		NOT completed.
	RenderJob* process(Engine& engine);
	
	//! @brief Cancel this job (or this job and next ones)
	//! @param cancelList If true this job and next ones (mNextJob chained list)
	//! 	are canceled (in reverse order).
	//! @note Called from user thread
	//! @return Return true if at least one job is effectively canceled
	//!
	//! Cancel if Pending or Computing.
	//! Outputs of canceled jobs are not computed, only inputs are set (state
	//! coherency). RenderToken's are notified as canceled.
	BOOL cancel(BOOL cancelList = FALSE);
	
	//! @brief Prepend reverted input delta into duplicate job context
	//! @param dup The duplicate job context to accumulate reversed delta
	//! Use to restore the previous state of copied jobs.
	//! @note Called from user thread
	void prependRevertedDelta(DuplicateJob& dup) const;
	
	//! @brief Fill filter outputs structure from this job
	//! @param[in,out] filter The filter outputs structure to fill
	void fill(OutputsFilter& filter) const;
	
	//! @brief Get render job UID
	UINT getUid() const { return mUid; }
	
	//! @brief Return the current job state
	State getState() const { return mState; }
	
	//! @brief Return if the current job is canceled
	bool isCanceled() const { return mCanceled; }

	//! @brief Accessor on next job to process
	//! @note Called from render thread 
	RenderJob* getNextJob() const { return mNextJob; }
	
	//! @brief Return if the job is empty (no pushed RenderPushIOs)
	BOOL isEmpty() const { return mRenderPushIOs.empty(); }
	
	//! @brief Accessor on link graphs (Read only)
	const LinkGraphs& getLinkGraphs() const { return mLinkGraphs; }
	
	//! @brief Accessor on User callbacks instance (can be NULL if none)
	Callbacks* getCallbacks() const { return mCallbacks; }
	
protected:

	//! @brief Vector of push I/O
	typedef std::vector<RenderPushIO*> RenderPushIOs;
	
	//! @brief Map of UIDs -> count
	typedef std::map<UINT,UINT> UidsMap;
	
	
	//! @brief Render job UID
	const UINT mUid;
	
	//! @brief Current state
	volatile State mState;
	
	//! @brief Canceled by user, push only Inputs
	volatile bool mCanceled;
	
	//! @brief Pointer on next job to process
	//! Used by render thread to avoid container thread safety stuff
	RenderJob*volatile mNextJob;
	
	//! @brief Vector of push I/O (this instance ownership)
	//! In sequential engine push order
	RenderPushIOs mRenderPushIOs;
	
	//! @brief Per graph states UID usage count (used to determinate seq. index)
	UidsMap mStateUsageCount;
	
	//! @brief All graph states valid at render job creation to use at link time
	//! Allows to keep ownership on datas required at link time
	LinkGraphs mLinkGraphs;
	
	//! @brief User callbacks instance (can be NULL if none)
	Callbacks* mCallbacks;
	
	
	//! @brief Push input and output in engine handle
	void pull(Computation &computation);
	
	//! @brief Is render job complete
	//! @return Return if all push I/O completed
	bool isComplete() const;
	
	//! @brief In linking needed
	//! @return Return if at least one graph state need to be linked
	bool isLinkNeeded() const;
	
};  // class RenderJob


} // namespace Details
} // namespace SubstanceAir

#endif // ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSRENDERJOB_H
