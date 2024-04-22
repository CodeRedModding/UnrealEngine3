//! @file detailsengine.cpp
//! @brief Substance Air Framework Engine/Linker wrapper implementation
//! @author Christophe Soum - Allegorithmic
//! @date 20111116
//! @copyright Allegorithmic. All rights reserved.
//!

#include "Engine.h"

#include "framework/details/detailsengine.h"
#include "framework/details/detailsrenderjob.h"
#include "framework/details/detailsrenderpushio.h"
#include "framework/details/detailsgraphbinary.h"
#include "framework/details/detailsgraphstate.h"
#include "framework/details/detailsinputimagetoken.h"
#include "framework/details/detailslinkgraphs.h"
#include "framework/details/detailslinkcontext.h"
#include "framework/details/detailslinkdata.h"

#include "framework/renderresult.h"

#pragma pack ( push, 8 )
#include <substance/version.h>
#include <substance/linker/linker.h>
#pragma pack ( pop )

#include <algorithm>
#include <iterator>
#include <utility>

#include <memory.h>

using namespace SubstanceAir::Details;


//! @brief Substance_Linker_Callback_UIDCollision linker callback impl.
//! Fill GraphBinary translated UIDs
SUBSTANCE_EXTERNC static void SUBSTANCE_CALLBACK 
substanceAirDetailsLinkerCallbackUIDCollision(
	SubstanceLinkerHandle *handle,
	SubstanceLinkerUIDCollisionType collisionType,
	unsigned int previousUid,
	unsigned int newUid)
{
	unsigned int res;
	(void)res;
	SubstanceAir::Details::Engine *engine = NULL;
	res = substanceLinkerHandleGetUserData(handle,(size_t*)&engine);
	check(res==0);

	engine->callbackLinkerUIDCollision(
		collisionType,
		previousUid,
		newUid);
}


//! @brief Substance_Callback_OutputCompleted engine callback impl.
SUBSTANCE_EXTERNC static void SUBSTANCE_CALLBACK
substanceAirDetailsEngineCallbackOutputCompleted(
	SubstanceHandle *handle,
	unsigned int outputIndex,
	size_t jobUserData)
{
	SubstanceAir::Details::RenderPushIO *pushio = 
		(SubstanceAir::Details::RenderPushIO*)jobUserData;
	check(pushio!=NULL);
	
	// Grab result
	SubstanceTexture texture;
	unsigned int res = substanceHandleGetOutputs(
		handle,
		Substance_OutOpt_OutIndex,
		outputIndex,
		1,
		&texture);
	(void)res;
	check(res==0);
	
	// Get parent context
	SubstanceContext* context = substanceHandleGetParentContext(handle);
	check(context!=NULL);
	
	// Create render result
	SubstanceAir::RenderResult *renderresult = 
		new SubstanceAir::RenderResult(context,texture);
	
	// Transmit it to Push I/O
	pushio->callbackOutputComplete(outputIndex,renderresult);
}

	
//! @brief Substance_Callback_JobCompleted engine callback impl.
SUBSTANCE_EXTERNC static void SUBSTANCE_CALLBACK
substanceAirDetailsEngineCallbackJobCompleted(
	SubstanceHandle*,
	size_t jobUserData)
{
	SubstanceAir::Details::RenderPushIO *pushio = 
		(SubstanceAir::Details::RenderPushIO*)jobUserData;
	if (pushio!=NULL)
	{
		pushio->callbackJobComplete();
	}
}


//! @brief Substance_Callback_InputImageLock engine callback impl.
SUBSTANCE_EXTERNC static void SUBSTANCE_CALLBACK
substanceAirDetailsEngineCallbackInputImageLock(
	SubstanceHandle*,
	size_t,
	unsigned int inputIndex,
	SubstanceTextureInput **currentTextureInputDesc,
	const SubstanceTextureInput*)
{
	ImageInputToken *const inptkn = 
		static_cast<ImageInputToken*>(*currentTextureInputDesc);

	if (inptkn!=NULL)
	{
		inptkn->lock();
		*currentTextureInputDesc = inptkn;
	}
}


//! @brief Substance_Callback_InputImageUnlock engine callback impl.
SUBSTANCE_EXTERNC static void SUBSTANCE_CALLBACK
substanceAirDetailsEngineCallbackInputImageUnlock(
	SubstanceHandle*,
	size_t,
	unsigned int,
	SubstanceTextureInput* textureInputDesc)
{
	ImageInputToken *const inptkn = 
		static_cast<ImageInputToken*>(textureInputDesc);
	if (inptkn!=NULL)
	{
		inptkn->unlock();
	}
}	


//! @brief Context singleton instance
std::tr1::weak_ptr<SubstanceAir::Details::Engine::Context>
	SubstanceAir::Details::Engine::mContextGlobal;


//! @brief Constructor
SubstanceAir::Details::Engine::Engine() :
	mLinkerCacheData(NULL),
	mHandle(NULL),
	mLinkerContext(NULL),
	mLinkerHandle(NULL),
	mCurrentLinkContext(NULL)
{
	// Create/reuse context
	if (mContextGlobal.expired())
	{
		mContextInstance.reset(new Context);
		mContextGlobal = mContextInstance;
	}
	else
	{
		mContextInstance = mContextGlobal.lock();
	}
	
	// Create Linker
	
	UINT res;
	
		// Get Engine platform
		SubstanceVersion verengine;
		substanceGetCurrentVersion(&verengine);
	
		// Create linker context
		res = substanceLinkerContextInit(
			&mLinkerContext,
			SUBSTANCE_LINKER_API_VERSION,
			verengine.platformImplEnum);
		check(res==0);
	
	
	
		// Set global fusing option
		SubstanceLinkerOptionValue optvalue;
		optvalue.uinteger = Substance_Linker_Global_FuseNone;  // No fusing
		res = substanceLinkerContextSetOption(
			mLinkerContext,
			Substance_Linker_Option_GlobalTweaks,
			optvalue);
		check(res==0);
	
	
	// Set UID collision callback
	res = substanceLinkerContextSetCallback(
		mLinkerContext,
		Substance_Linker_Callback_UIDCollision,
		(void*)substanceAirDetailsLinkerCallbackUIDCollision);
	check(res==0);	
	
	// Create linker handle
	res = substanceLinkerHandleInit(&mLinkerHandle,mLinkerContext);
	check(res==0);
	
	// Set this as user data for callbacks
	res = substanceLinkerHandleSetUserData(mLinkerHandle,(size_t)this);
	check(res==0);
}


//! @brief Destructor
SubstanceAir::Details::Engine::~Engine()
{
	// Release engine handle
	unsigned int res;
	(void)res;
	if (mHandle!=NULL)
	{
		res = substanceHandleRelease(mHandle);
		check(res==0);
	}

	// Release linker
	res = substanceLinkerHandleRelease(mLinkerHandle);
	check(res==0);
	res = substanceLinkerContextRelease(mLinkerContext);
	check(res==0);
}


//! @brief Link/Relink all states in pending render jobs
//! @param renderJobBegin Begin of chained list of render jobs to link
//! @return Return true if link process succeed
bool SubstanceAir::Details::Engine::link(RenderJob* renderJobBegin)
{
	unsigned int res;
	(void)res;
	
	// Merge all graph states
	LinkGraphs linkgraphs;
	for (;renderJobBegin!=NULL;renderJobBegin=renderJobBegin->getNextJob())
	{
		linkgraphs.merge(renderJobBegin->getLinkGraphs());
	}
	
	// Disable all outputs by default
	res = substanceLinkerHandleSelectOutputs(
		mLinkerHandle,
		Substance_Linker_Select_UnselectAll,
		0);
	check(res==0);

	// Push all states to link
	SBS_VECTOR_FOREACH (
		const LinkGraphs::GraphStatePtr& graphstateptr,
		linkgraphs.graphStates)
	{
		LinkContext linkContext(
			mLinkerHandle,
			graphstateptr->getBinary(),
			graphstateptr->getUid());
		linkContext.graphBinary.resetTranslatedUids();  // Reset translated UID
		mCurrentLinkContext = &linkContext;  // Set as current context to fill
		graphstateptr->getLinkData()->push(linkContext);
		
		// Select outputs
		SBS_VECTOR_FOREACH (
			const GraphBinary::Entry& entry,
			linkContext.graphBinary.outputs)
		{		
			res = substanceLinkerHandleSelectOutputs(
				mLinkerHandle,
				Substance_Linker_Select_Select,
				entry.uidTranslated);
				
			if (res)
			{
// 				TODO2 Warning: output UID not valid
				check(0);
			}
		}
	}
	
	// Link, Grab assembly
	size_t sbsbindataindex = mSbsbinDatas[0].empty() ? 0 : 1;
	std::string &sbsbindata = mSbsbinDatas[sbsbindataindex];
	check(mSbsbinDatas[0].empty()||mSbsbinDatas[1].empty());
	{
		const unsigned char* resultData = NULL;
		size_t resultSize = 0;
		res = substanceLinkerHandleLink(
			mLinkerHandle,
			&resultData,
			&resultSize);
		check(res==0);
	
		sbsbindata.assign((const char*)resultData,resultSize);
	}
	
	// Grab new cache data blob
	res = substanceLinkerHandleGetCacheMapping(
		mLinkerHandle,
		&mLinkerCacheData,
		NULL,
		mLinkerCacheData);
	check(res==0);
	
	// Set memory budget
	SubstanceHardResources hardrsc;
	memset(&hardrsc,0,sizeof(SubstanceHardResources));

	// memory limitation for the handle
	const int_t MaxMemoryBudgetMb= 2048;
	const int_t MinMemoryBudgetMb = 32;

	int_t BudgetMb = SBS_DEFAULT_MEMBUDGET_MB;

	GConfig->GetInt(
		TEXT("SubstanceAir"),
		TEXT("MemBudgetMb"),
		BudgetMb,
		GEngineIni);

	BudgetMb = Clamp(BudgetMb, MinMemoryBudgetMb, MaxMemoryBudgetMb);
	hardrsc.systemMemoryBudget = BudgetMb * 1024 * 1024;

	// allow to use all but #FreeCore CPU cores
	INT FreeCore = 1;
	GConfig->GetInt(TEXT("SubstanceAir"), TEXT("FreeCore"),FreeCore,GEngineIni );
	FreeCore = Clamp((UINT)FreeCore, (UINT)0, GNumHardwareThreads-1);

	for (UINT Idx = 0 ; Idx < SUBSTANCE_CPU_COUNT_MAX ; ++Idx)
	{
		if (Idx < GNumHardwareThreads-FreeCore)
		{
			hardrsc.cpusUse[Idx] = Substance_Resource_FullUse;
		}
		else
		{
			hardrsc.cpusUse[Idx] = Substance_Resource_DoNotUse;
		}
	}
	
	// Create new handle
	SubstanceHandle *newhandle = NULL;
	res = substanceHandleInit(
		&newhandle,
		mContextInstance->getContext(),
		(const unsigned char*)sbsbindata.data(),
		sbsbindata.size(),
		&hardrsc);
	check(res==0);
	
	// Switch handle
	{
		// Scoped modification
		Sync::mutex::scoped_lock slock(mMutexHandle);
		
		if (mHandle!=NULL)
		{
			// Transfer cache
			res = substanceHandleTransferCache(
				newhandle,
				mHandle,
				mLinkerCacheData);
			check(res==0);
	
			// Delete previous handle
			res = substanceHandleRelease(mHandle);
			check(res==0);
			
			// Erase previous sbsbin data
			mSbsbinDatas[sbsbindataindex^1].resize(0);
		}
		
		// Use new one
		mHandle = newhandle;
	}
	
	// Fill Graph binary SBSBIN indices
	fillIndices(linkgraphs);
	
	return true;
}


//! @brief Function called just before pushing I/O to compute
//! @post Engine render queue is flushed
void SubstanceAir::Details::Engine::beginPush()
{
	check(mHandle!=NULL);
	
	unsigned int res = substanceHandleFlush(mHandle);
	(void)res;
	check(res==0);
}


//! @brief Stop any generation
//! Thread safe stop call on current handle if present 
//! @note Called from user thread
void SubstanceAir::Details::Engine::stop()
{
	Sync::mutex::scoped_lock slock(mMutexHandle);
	if (mHandle!=NULL)
	{
		// Stop, may be not very usefull, do not check return code!
		substanceHandleStop(mHandle,Substance_Sync_Asynchronous);
	}
}


//! @brief Linker Collision UID callback implementation
//! @param collisionType Output or input collision flag
//! @param previousUid Initial UID that collide
//! @param newUid New UID generated
//! @note Called by linker callback
void SubstanceAir::Details::Engine::callbackLinkerUIDCollision(
	SubstanceLinkerUIDCollisionType collisionType,
	UINT previousUid,
	UINT newUid)
{
	check(mCurrentLinkContext!=NULL);
	mCurrentLinkContext->notifyLinkerUIDCollision(
		collisionType,
		previousUid,
		newUid);
}


//! @brief Constructor, create Substance Context
SubstanceAir::Details::Engine::Context::Context() :
	mContext(NULL)
{
	unsigned int res;
	(void)res;

	// Create context
	SubstanceDevice dummydevice;      // BLEND platform dummy device 
	res = substanceContextInit(&mContext,&dummydevice);
	check(res==0);
	
	// Plug callbacks
	res = substanceContextSetCallback(
		mContext,
		Substance_Callback_OutputCompleted,
		(void*)substanceAirDetailsEngineCallbackOutputCompleted);
	check(res==0);
	
	res = substanceContextSetCallback(
		mContext,
		Substance_Callback_JobCompleted,
		(void*)substanceAirDetailsEngineCallbackJobCompleted);
	check(res==0);
	
	res = substanceContextSetCallback(
		mContext,
		Substance_Callback_InputImageLock,
		(void*)substanceAirDetailsEngineCallbackInputImageLock);
	check(res==0);
	
	res = substanceContextSetCallback(
		mContext,
		Substance_Callback_InputImageUnlock,
		(void*)substanceAirDetailsEngineCallbackInputImageUnlock);
	check(res==0);
}


//! @brief Destructor, release Substance Context
SubstanceAir::Details::Engine::Context::~Context()
{
	unsigned int res = substanceContextRelease(mContext);
	(void)res;
	check(res==0);
}


//! @brief Fill Graph binaries w/ new Engine handle SBSBIN indices
//! @param linkGraphs Contains Graph binaries to fill indices
void SubstanceAir::Details::Engine::fillIndices(LinkGraphs& linkGraphs) const
{
	typedef std::vector<std::pair<UINT,UINT> > UidIndexPairs;
	UidIndexPairs trinputs,troutputs;
	
	// Get indices/UIDs pairs
	{
		unsigned int dindex;
		SubstanceDataDesc datadesc;
		unsigned int res = substanceHandleGetDesc(
			mHandle,
			&datadesc);
		(void)res;
		check(res==0);
		
		// Parse Inputs
		trinputs.reserve(datadesc.inputsCount);
		for (dindex=0;dindex<datadesc.inputsCount;++dindex)
		{
			SubstanceInputDesc indesc;
			res = substanceHandleGetInputDesc(
				mHandle,
				dindex,
				&indesc);
			check(res==0);
		
			trinputs.push_back(std::make_pair(indesc.inputId,dindex));
		}
		
		// Parse outputs
		troutputs.reserve(datadesc.outputsCount);
		for (dindex=0;dindex<datadesc.outputsCount;++dindex)
		{
			SubstanceOutputDesc outdesc;
			res = substanceHandleGetOutputDesc(
				mHandle,
				dindex,
				&outdesc);
			check(res==0);
		
			troutputs.push_back(std::make_pair(outdesc.outputId,dindex));
		}
	}
	
	typedef std::vector<std::pair<UINT,GraphBinary::Entry*> > UidEntryPairs;
	UidEntryPairs einputs,eoutputs;

	// Get graph binaries entries per translated uids
	SBS_VECTOR_FOREACH (
		const LinkGraphs::GraphStatePtr& graphstateptr,
		linkGraphs.graphStates)
	{
		GraphBinary& binary = graphstateptr->getBinary();
		
		// Get all input entries
		SBS_VECTOR_FOREACH (GraphBinary::Entry& entry,binary.inputs)
		{
			einputs.push_back(std::make_pair(entry.uidTranslated,&entry));
		}
		
		// Get all output entries
		SBS_VECTOR_FOREACH (GraphBinary::Entry& entry,binary.outputs)
		{
			eoutputs.push_back(std::make_pair(entry.uidTranslated,&entry));
		}
		
		// Mark as linked
		binary.linked();
	}
	
	// Sort all, per translated UID
	std::sort(trinputs.begin(),trinputs.end());
	std::sort(troutputs.begin(),troutputs.end());
	std::sort(einputs.begin(),einputs.end());
	std::sort(eoutputs.begin(),eoutputs.end());
	
	// Fill input entries w/ indices
	UidIndexPairs::const_iterator trite = trinputs.begin();
	SBS_VECTOR_FOREACH (UidEntryPairs::value_type& epair,einputs)
	{
		// Skip unused inputs (multi-graph case)
		while (trite!=trinputs.end() && trite->first<epair.first)
		{
			++trite;
		}	
		check(trite!=trinputs.end() && trite->first==epair.first);
		if (trite!=trinputs.end() && trite->first==epair.first)
		{
			epair.second->index = (trite++)->second;
		}
	}
	
	// Fill output entries w/ indices
	trite = troutputs.begin();
	SBS_VECTOR_FOREACH (UidEntryPairs::value_type& epair,eoutputs)
	{
		check(trite!=troutputs.end() && trite->first==epair.first);
		while (trite!=troutputs.end() && trite->first<epair.first)
		{
			++trite;
		}	
		if (trite!=troutputs.end() && trite->first==epair.first)
		{
			epair.second->index = (trite++)->second;
		}
	}
}

