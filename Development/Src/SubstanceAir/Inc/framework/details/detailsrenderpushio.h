//! @file detailsrenderpushio.h
//! @brief Substance Air Framework Render Push Input/Output definition
//! @author Christophe Soum - Allegorithmic
//! @date 20111027
//! @copyright Allegorithmic. All rights reserved.
//!
 

#ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSRENDERPUSHIO_H
#define _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSRENDERPUSHIO_H

#include "detailsdeltastate.h"
#include "detailsrenderjob.h"

#pragma pack ( push, 8 )
#include <substance/callbacks.h>
#pragma pack ( pop )

#include <vector>
#include <set>

namespace SubstanceAir
{
namespace Details
{

//! @brief Render Push Input/Output batch
//! Contains decription of which input and outputs to push for several instances
class RenderPushIO : Uncopyable
{
public:
	//! @brief Complete return enumeration
	enum Complete
	{
		Complete_No,
		Complete_Yes,
		Complete_DontKnow   //!< No push I/O commands 
	};

	//! @brief Create from render job
	//! @param renderJob Parent render job that owns this instance
	//! @note Called from user thread 
	RenderPushIO(RenderJob &renderJob);
	
	//! @brief Constructor from push I/O to duplicate
	//! @param renderJob Parent render job that owns this instance
	//! @param src The canceled push I/O to copy
	//! @param dup Duplicate job context
	//! Build a push I/O from a canceled one and optionnaly filter outputs.
	//! Push again SRC render tokens (of not filtered outputs).
	//! @warning Resulting push I/O can be empty (hasOutputs()==false) if all
	//!		outputs are filtered OR already computed. In this case this push IO 
	//!		is no more usefull and can be removed.
	//! @note Called from user thread
	RenderPushIO(RenderJob &renderJob,const RenderPushIO& src,DuplicateJob& dup);
	
	//! @brief Destructor
	//! @note Called from user thread 
	~RenderPushIO();
		
	//! @brief Create a push I/O pair from current state & current instance
	//! @param graphState The current graph state
	//! @param graphInstance The pushed graph instance (not keeped)
	//! @note Called from user thread 
	//! @return Return true if at least one dirty output
	bool push(GraphState &graphState, FGraphInstance* graphInstance);
	
	//! @brief Prepend reverted input delta into duplicate job context
	//! @param dup The duplicate job context to accumulate reversed delta
	//! Use to restore the previous state of copied jobs.
	//! @note Called from user thread
	void prependRevertedDelta(DuplicateJob& dup) const;

	//! @brief Push input and output in engine handle
	//! @param inputsOnly If true only inputs are pushed
	//! @post Push I/O state is reverted to correct State_xxxPending(s)
	//! @note Called from Render queue thread
	void pull(Computation &computation,bool inputsOnly);
	
	//! @brief Fill filter outputs structure from this push IO
	//! @param[in,out] filter The filter outputs structure to fill
	void fill(OutputsFilter& filter) const;
	
	//! @brief Cancel notification, decrease RenderToken counters
	void cancel();
	
	//! @brief In linking needed
	//! @return Return if at least one graph state need to be linked
	bool isLinkNeeded() const;
	
	//! @brief Accessor: At least one output to compute
	//! Check all render tokens if not already filled
	bool hasOutputs() const;
	
	//! @brief Return if the current Push I/O is completed
	//! @param inputOnly Only inputs are required
	//! @note Called from Render queue thread
	//! @return Can return "don't know" if no pending I/O
	Complete isComplete(bool inputOnly) const;
	
	//! @brief Engine job completed
	//! @note Called by engine callback from Render queue thread
	void callbackJobComplete();
	
	//! @brief Engine output completed
	//! @param index Output SBSBIN index of completed output
	//! @param renderResult The result texture, ownership transfered
	//! @note Called by engine callback from Render queue thread
	void callbackOutputComplete(UINT index,RenderResult* renderResult);
	
protected:
	//! @brief States enumeration
	//! Can be combined
	enum State
	{
		State_None            = 0x0,      //!< Nothing to push / pushed
		State_InputsPending   = 0x1,      //!< Input(s) to push
		State_InputsPushed    = 0x2,      //!< All inputs push commands resolved
		State_OutputsPending  = 0x4,      //!< Output(s) to compute
		State_OutputsComputed = 0x8,      //!< All outputs computed
	};

	//! @brief One output description
	struct Output
	{
		//! @brief GraphState/GraphInstance order index
		UINT index;
		
		//! @brief Pointer on destination render token to fill
		std::tr1::shared_ptr<RenderToken> renderToken;
		
		//! @brief Pointer on corresponding graph instance
		//! @warning Callback purpose only, may be dandling pointer
		const FGraphInstance* graphInstance;

		//! @brief Pointer on corresponding output instance
		//! @warning Callback purpose only, may be dandling pointer
		output_inst_t* outputInstance;
		
	};  // struct Output
	
	//! @brief Input/Output to push for one instance
	//! Contains decription of which input and outputs to push one 
	//! GraphState (reference contained in DeltaState)
	struct Instance
	{
		//! @brief Container of outputs
		typedef std::vector<Output> Outputs;
	
		//! @brief Build a push I/O pair from current state & current instance
		//! @param renderJob Parent render job that owns this instance
		//! @param state The current graph state
		//! @param graphInstance The pushed graph instance (not keeped)
		Instance(
			const RenderJob &renderJob,
			GraphState &state,
			FGraphInstance* graphInstance);
			
		//! @brief Duplicate instance (except outputs)
		Instance(const Instance &src);
			
		//! @brief GraphState associated to this instance
		GraphState &graphState;
			
		//! @brief UID of the GraphInstance used to build this
		const guid_t instanceGuid;
			
		//! @brief Description of input w/ values to push
		DeltaState deltaState;
		
		//! @brief List of outputs indices/destination to push
		//! GraphState/GraphInstance order indices.
		Outputs outputs;
		
		//! @brief Accessor: At least one output to compute
		//! Check all render tokens if not already filled
		bool hasOutputs() const;
		
	};  // struct Instance

	//! @brief Vector of Instances (this instance ownership)
	typedef std::vector<Instance*> Instances;
	
	
	//! @brief Parent render job
	RenderJob &mRenderJob;
	
	//! @brief Current state
	volatile UINT mState;
	
	//! @brief I/O of all instances
	Instances mInstances;
	
	//! @brief Outputs array (engine index order)
	//! Filled when pushed into Engine (non required outputs set to NULL)
	std::vector<const Output*> mDestOutputs;
	
	//! @brief Number of inputs Engine job pushed to proceed 
	//! Filled when pushed into Engine
	//! Decreased by callbackJobComplete, can be negative: last job is output
	//! computation.
	int mInputJobPendingCount;
	
};  // class RenderPushIO


} // namespace Details
} // namespace SubstanceAir

#endif // ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSRENDERPUSHIO_H
