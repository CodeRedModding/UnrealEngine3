#ifndef NX_FLUIDS_NXFLUIDEMITTERDESC
#define NX_FLUIDS_NXFLUIDEMITTERDESC
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

#define MAXPARTICLES 32000

/**
Not available in the current release.

NX_FEF_VISUALIZATION:
Flags whether the emitter should be visualized for debugging or not.

NX_FEF_BROKEN_ACTOR_REF:
The emitter may reference an NxActor instance in order to maintain a relative coordinate 
frame. If this NxActor instance is released, the NX_FEF_BROKEN_ACTOR_REF flag is set.

FORCE_ON_ACTOR:
This flag specifies whether the emission should cause a force on 
the actor, the emitter is attached to.

NX_FEF_ADD_ACTOR_VELOCITY:
If set, the velocity of the emitter is added to the emitted particle velocity. This is the default behaviour.

NX_FEF_ENABLED
Flag to start and stop the emission. On default the emission is enabled.
*/
enum NxFluidEmitterFlag
	{
	NX_FEF_VISUALIZATION		= (1<<0),
	NX_FEF_BROKEN_ACTOR_REF		= (1<<1),
	NX_FEF_FORCE_ON_ACTOR		= (1<<2),
	NX_FEF_ADD_ACTOR_VELOCITY	= (1<<3),
	NX_FEF_ENABLED				= (1<<4),
	};

/**
Not available in the current release.
Flags to specify the shape of the area of emission.
*/
enum NxEmitterShape
	{
	NX_FE_RECTANGULAR		= (1<<0),
	NX_FE_ELLIPSE			= (1<<1)
	};

/**
Not available in the current release.
Flags to specify the emitter mode of operation.
*/
enum NxEmitterType
	{
	NX_FE_CONSTANT_PRESSURE		= (1<<0),
	NX_FE_CONSTANT_FLOW_RATE	= (1<<1)
	};

#include "fluids/NxFluidEmitter.h"

/**
Not available in the current release.
Descriptor for NxFluidEmitter class. Used for saving and loading the emitter state.
*/
class NxFluidEmitterDesc
	{
	public:

	/**
	Not available in the current release.
	The emitters pose relative to the frame actors coordinate frame, or the world, if the frame 
	actor is set to NULL. The third axis of the orientation frame sets the flow direction.
	*/
    NxMat34					relPose;
	
	/**
	Not available in the current release.
	A pointer to the NxActor to which the emitter is attached to. If this 
	pointer is set to NULL, the world frame is used to attach the emitter to. The actor 
	has to be in the same scene as the emitter. 
	*/
	NxActor*				frameActor;

	/**
	Not available in the current release.
	NxEmitterType flags:
	The type specifies the emitters mode of operation. Either the simulation enforces constant 
	pressure or constant flow rate at the emission site, given the velocity of emitted particles.
	*/
	NxU32					type;

	/**
	Not available in the current release.
	The maximum number of particles which are emitted from that emitter. If the total number of particles 
	in the fluid allready hit the maxParticles parameter of the fluid the maximal number specified by the 
	emitter can't be reached. 
	*/
	NxU32					maxParticles;

	/**
	Not available in the current release.
	NxEmitterShape flags:
	The emitter can either be rectangular or an elliptical shaped.
	*/
	NxU32					shape;

	/**
	Not available in the current release.
	The sizes of the emitter in the directions of the first and the second axis of its orientation 
	frame. The dimensions are actually the radii of the size.
	*/
	NxReal					dimensionX;
	NxReal					dimensionY;

	/**
	Not available in the current release.
	Random vector with values for each axis direction of the emitter orientation. The values have 
	to be positive and describe the maximal random particle displacement in each dimension.
	The z value describes the randomization in emission direction.
	*/
    NxVec3					randomPos;

	/**
	Not available in the current release.
	Random angle deviation from emission direction.
	*/
	NxReal					randomAngle;

	/**
	Not available in the current release.
	This parameter defines the velocity magnitude of the emitted fluid particles.
	*/
	NxReal					fluidVelocityMagnitude;

	/**
	Not available in the current release.
	The rate specifies how many particles are emitted per second. The rate is only considered 
	in the simulation if the type is set to constant flow rate.
	*/
	NxReal					rate;
	
	/**
	Not available in the current release.
	This specifies the time in seconds an emitted particle lives.
	*/
	NxReal					particleLifetime;

	/**
	Not available in the current release.
	A combination of NxFluidEmitterFlags.
	*/
	NxU32					flags;

	void*					userData;		//!< Will be copied to NxShape::userData.
	const char*				name;			//!< Possible debug name. The string is not copied by the SDK, only the pointer is stored.

	NX_INLINE ~NxFluidEmitterDesc();
	/**
	Not available in the current release.
	(Re)sets the structure to the default.	
	*/
	NX_INLINE void setToDefault();
	/**
	Not available in the current release.
	Returns true if the current settings are valid
	*/
	NX_INLINE bool isValid() const;

	/**
	Not available in the current release.
	Constructor sets to default.
	*/
	NX_INLINE	NxFluidEmitterDesc();
	};


NX_INLINE NxFluidEmitterDesc::NxFluidEmitterDesc()
	{
	setToDefault();
	}

NX_INLINE NxFluidEmitterDesc::~NxFluidEmitterDesc()
	{
	}

NX_INLINE void NxFluidEmitterDesc::setToDefault()
	{
    relPose							.id();
    frameActor						= NULL;
	type							= NX_FE_CONSTANT_PRESSURE;
	maxParticles					= MAXPARTICLES;
	shape							= NX_FE_RECTANGULAR;
	dimensionX						= 1.0f;
	dimensionY						= 1.0f;
	randomPos						.zero();
	randomAngle						= 0.0f;
	fluidVelocityMagnitude			= 1.0f;
	rate							= 100.0f;
	particleLifetime				= 0.0f;
	flags							= NX_FEF_ENABLED|NX_FEF_VISUALIZATION|NX_FEF_ADD_ACTOR_VELOCITY;
	
	userData						= NULL;
	name							= NULL;
	}

NX_INLINE bool NxFluidEmitterDesc::isValid() const
	{
	if (!relPose.isFinite()) return false;
	
	if (dimensionX < 0.0f) return false;
	if (dimensionY < 0.0f) return false;

	if (randomPos.x < 0.0f) return false;
	if (randomPos.y < 0.0f) return false;
	if (randomPos.z < 0.0f) return false;
	if (!randomPos.isFinite()) return false;

	if (randomAngle < 0.0f) return false;

	if (flags & NX_FEF_BROKEN_ACTOR_REF) return false;
	if (!((shape & NX_FE_ELLIPSE) ^ (shape & NX_FE_RECTANGULAR))) return false;
	if (!((type & NX_FE_CONSTANT_FLOW_RATE) ^ (shape & NX_FE_CONSTANT_PRESSURE))) return false;

	if (rate < 0.0f) return false;
	if (fluidVelocityMagnitude < 0.0f) return false;
	if (particleLifetime < 0.0f) return false;

	return true;
	}


#endif
