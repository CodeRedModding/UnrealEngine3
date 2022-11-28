#ifndef NX_PHYSICS_NXSCENEDESC
#define NX_PHYSICS_NXSCENEDESC
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

enum NxBroadPhaseType
	{
	NX_BROADPHASE_QUADRATIC,	//!< No acceleration structure (O(n^2)) [debug purpose]
	NX_BROADPHASE_FULL,			//!< Brute-force algorithm, performing the full job each time (O(n))
	NX_BROADPHASE_COHERENT,		//!< Incremental algorithm, using temporal coherence (O(n))

	NX_BROADPHASE_FORCE_DWORD = 0x7fffffff
	};

enum NxTimeStepMethod
	{
	NX_TIMESTEP_FIXED,				//!< The simulation automatically subdivides the passed elapsed time into maxTimeStep-sized substeps.
	NX_TIMESTEP_VARIABLE,			//!< The simulation uses the elapsed time that the user passes as-is, substeps (maxTimeStep, maxIter) are not used.
	NX_NUM_TIMESTEP_METHODS
	};

class NxUserNotify;
class NxUserTriggerReport;
class NxUserContactReport;
class NxBounds3;

// PT: those limits are not complete yet. They should include a lot more information than this.
class NxSceneLimits
	{
	public:
	NxU32					maxNbActors;		//!< Expected max number of actors
	NxU32					maxNbBodies;		//!< Expected max number of bodies
	NxU32					maxNbStaticShapes;	//!< Expected max number of static shapes
	NxU32					maxNbDynamicShapes;	//!< Expected max number of dynamic shapes
	NxU32					maxNbJoints;		//!< Expected max number of joints

	NX_INLINE NxSceneLimits();
	};

NX_INLINE NxSceneLimits::NxSceneLimits()	//constructor sets to default
	{
	maxNbActors			= 0;
	maxNbBodies			= 0;
	maxNbStaticShapes	= 0;
	maxNbDynamicShapes	= 0;
	maxNbJoints			= 0;
	}

/**
Descriptor class for scenes.
*/

class NxSceneDesc
	{
	public:
	NxBroadPhaseType		broadPhase;			//!< Broad phase algorithm
	NxVec3					gravity;			//!< Gravity vector
	NxUserNotify*			userNotify;			//!< Possible notification callback
	NxUserTriggerReport*	userTriggerReport;	//!< Possible trigger callback
	NxUserContactReport*	userContactReport;	//!< Possible contact callback
	NxReal					maxTimestep;		//!< Maximum integration time step size
	NxU32					maxIter;			//!< Maximum number of substeps to take
	NxTimeStepMethod		timeStepMethod;		//!< Integration method, see ::NxTimeStepMethod
	NxBounds3*				maxBounds;			//!< Max scene bounds, if available
	NxSceneLimits*			limits;				//!< Expected scene limits (or NULL)
	bool					groundPlane;		//!< Enable/disable default ground plane
	bool					boundsPlanes;		//!< Enable/disable 6 planes around maxBounds (if available)
	bool					collisionDetection;	//!< Enable/disable collision detection
	void*					userData;			//!< Will be copied to NxShape::userData

	/**
	constructor sets to default (no gravity, no ground plane, collision detection on).
	*/
	NX_INLINE NxSceneDesc();
	/**
	(re)sets the structure to the default (no gravity, no ground plane, collision detection on).	
	*/
	NX_INLINE void setToDefault();
	/**
	returns true if the current settings are valid (returns always true).
	*/
	NX_INLINE bool isValid() const;
	};

NX_INLINE NxSceneDesc::NxSceneDesc()	//constructor sets to default
	{
	setToDefault();
	}

NX_INLINE void NxSceneDesc::setToDefault()
	{
	broadPhase			= NX_BROADPHASE_COHERENT;
	gravity.zero();
	userNotify			= NULL;
	userTriggerReport	= NULL;
	userContactReport	= NULL;

	maxTimestep			= 1.0f/60.0f;
	maxIter				= 8;
	timeStepMethod		= NX_TIMESTEP_FIXED;

	maxBounds			= NULL;
	limits				= NULL;

	groundPlane			= false;
	boundsPlanes		= false;
	collisionDetection	= true;
	userData			= NULL;
	}

NX_INLINE bool NxSceneDesc::isValid() const
	{
	if(maxTimestep <= 0 || maxIter < 1 || timeStepMethod > NX_NUM_TIMESTEP_METHODS)
		return false;
	if(boundsPlanes && !maxBounds)
		return false;
	return true;
	}

#endif
