#ifndef NX_SCENE_STATS
#define NX_SCENE_STATS

/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

#include "Nxp.h"

#define PROFILE_LEVEL 1 // used to designation how detailed the profiling on this build should be.

class NxSceneStats
	{
	public:
	NxI32   numContacts;
	NxI32   maxContacts;
	NxI32   numActors;
	NxI32   numJoints;
	NxI32   numAwake;
	NxI32   numAsleep;
	NxI32   numStaticShapes;

	NxSceneStats()
		{
		reset();
		}

	void reset()
		{
		numContacts = 0;
		maxContacts = 0;
		numAwake = 0;
		numAsleep = 0;
		numActors = 0;
		numJoints = 0;
		numStaticShapes = 0;
		}
	};

enum NxPerformanceFlag
	{
	NX_PF_LOGCPU	= (1<<0),
	NX_PF_LOGCOUNT	= (1<<1),
	NX_PF_LOGMEAN	= (1<<2),
	NX_PF_ENABLE	= (1<<3),
	};

class NxPerformanceInspector
	{
	public:
	virtual	int		initEntry(const char* pname, NxPerformanceFlag flags)=0;
	virtual	void	logEvent(const char* fmt, ...) = 0;
	virtual	void	markBegin(int entry) = 0;
	virtual	void	markEnd(int entry) = 0;
	virtual	void	setCount(int entry, int count) = 0;
	};
#endif
