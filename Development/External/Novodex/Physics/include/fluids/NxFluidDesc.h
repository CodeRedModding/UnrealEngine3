#ifndef NX_FLUIDS_NXFLUIDACTORDESC
#define NX_FLUIDS_NXFLUIDACTORDESC
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

#include "fluids/NxMeshData.h"
#include "fluids/NxParticleData.h"
#include "fluids/NxFluidEmitterDesc.h"
#include "fluids/NxImplicitMeshDesc.h"

#include "NxArray.h"
#define MAX_EMITTERS 20

/**
Not available in the current release.
Particles can be treated in two ways, either they are simulated considering 
interparticular forces (SPH) or they are simulated independently. 
*/
enum NxFluidSimulationMethod
	{
	NX_F_SPH						= (1<<0),	
	NX_F_NO_PARTICLE_INTERACTION	= (1<<1),
	};

/**
Not available in the current release.
There are several interesting particle events during the simulation:
-	Particle separates completely from other particles
-	Particle collides with static or dynamic environment
-	Particle runs out of lifetime
-	Particle exceeds certain speed
-	Particle drained
-	Particle stands still
-	...
On one hand these events are stored as flags and can be retreived from the simulation 
along with other particle states such as position, velocity and lifetime. On the other 
hand its possible that the simulation takes certain actions when particles enter or 
leave these states.
TURN_DEAD means that the particle is removed from the simulation. 
TURN_SIMPLE means that the particle is now simulated without considering any particle neighbors. 
This will be useful (e.g.) to remove particles which are separated from the 
simulation or turn them into simple particles, which will lower the computational 
cost for the sph simulation as well as for the surface mesh generation.
*/
enum NxFluidParticleAction
	{
	NX_F_TURN_DEAD		= (1<<1),
	NX_F_TURN_SIMPLE	= (1<<2),
	};

/**
Not available in the current release.
The NxFluid instance can be selected for collision with both static and dynamic shapes. 
One can also define whether the NxFluid collision with a dynamic actor should have any 
impact on the momentum of the actor or not.
*/
enum NxFluidCollisionMethod
	{
	NX_F_STATIC					= (1<<0),	
	NX_F_DYNAMIC 				= (1<<1),
	NX_F_DYNAMIC_ACTOR_REACTION	= (1<<2),
	};

/**
Not available in the current release.
Fluid flags.
NX_FF_VISUALIZATION enables/disables debug visualization for the NxFluid.
NX_FF_MESH_IGNORE_SEPARATED specifies that single separated particles should be included 
in the surface generation
*/
enum NxFluidFlag
	{
	NX_FF_VISUALIZATION							= (1<<0),
	NX_FF_MESH_INCLUDE_SEPARATED				= (1<<1),	
	};

/**
Not available in the current release.
Describes an NxFluid. 
*/
class NxFluidDesc
	{
	public:

	/**
	Not available in the current release.
	Array of emitter descriptors that describe emitters which emit fluid into this 
	fluid actor. A fluid actor can have any number of emitters.
	*/ 
	NxArray<NxFluidEmitterDesc>	emitters;
	
	/**
	Not available in the current release.
	Describes the data buffers with particles which are added to the fluid initially. 
	The pointers to the buffers are invalidated after the initial particles are generated.
	*/ 
	NxParticleData				initialParticleData;
	
	/**
	Not available in the current release.
	Sets the maximal number of particles for the fluid used in the simulation. 
	If more particles are added directly, or more particles are emitted into the 
	fluid after this limit is reached, they are just ignored.
	*/ 
	NxU32						maxParticles;

	/**
	Not available in the current release.
	The particle resolution given as particles per linear meter measured when the fluid is 
	in its rest state (relaxed). Even if the particle system is simulated without 
	particle interactions, this parameter defines the emission desity if the emitters.
	*/ 
	NxReal						restParticlesPerMeter;

	/**
	Not available in the current release.
	Target density for the fluid, water is about 1000. Even if the particle system is simulated without 
	particle interactions, this parameter defines indirectly in combination with 
	restParticlesPerMeter the mass of one particle. The mass influences the repulsion effect of the 
	emitters on actors. 
	*/ 
	NxReal						restDensity;

	/**
	Not available in the current release.
	Radius of sphere of influence for particle interaction. This
	parameter is relative to the rest spacing of the particles, 
	which is given by the parameter restParticlesPerMeter.
	*/ 
	NxReal						kernelRadiusMultiplier;

	/**
	Not available in the current release.
	The stiffness of the particle interaction relative to the
	pressure. This parameter is related to the gas constant of a 
	compressible fluid.
	*/ 
	NxReal						stiffness;

	/**
	Not available in the current release.
	The viscosity of the fluid.
	*/ 
	NxReal						viscosity;

	/**
	Not available in the current release.
	Velocity damping constant.
	*/ 
	NxReal						damping;

	/**
	Not available in the current release.
	Defines the restitution coefficient, which is used for 
	collisions of the fluid particles with static shapes.
	*/ 
	NxReal						staticCollisionRestitution;
	
	/**
	Not available in the current release.
	Defines the "friction" of the fluid regarding the 
	surface of a static shape.
	*/ 
	NxReal						staticCollisionAdhesion;
	
	/**
	Not available in the current release.
	Defines the restitution coefficient, which is used for 
	collisions of the fluid particles with dynamic shapes.
	*/ 
	NxReal						dynamicCollisionRestitution;
	
	/**
	Not available in the current release.
	Defines the "friction" of the fluid regarding the surface 
	of a dynamic shape.
	*/ 
	NxReal						dynamicCollisionAdhesion;

	/**
	Not available in the current release.
	NxFluidSimulationMethod flags. Defines whether particle interactions 
	are considered in the simulation, or not.
	*/ 
	NxU32						simulationMethod;


	/**
	Not available in the current release.
	NxFluidCollisionMethod flags. Selects whether static collision, dynamic 
	collision with the environment is performed. If NX_ACTOR_REACTION is set, 
	fluid particles act on rigid body actors. Collisions can be overruled 
	on a per actor basis.
	*/ 
	NxU32						collisionMethod;

	/**
	Not available in the current release.
	NxFluidParticleAction flags
	Particle events which can be configured with a particle action.
	*/ 
	NxU32						onSeparation;
	NxU32						onCollision;
	NxU32						onLifetimeExpired;	
	
	/**
	Not available in the current release.
	The surface mesh descriptor which is used to create 
	an NxImplicitMesh which serves fluid mesh generation. 
	If NULL, no surface mesh will be generated.
	*/
	NxImplicitMeshDesc*			surfaceMeshDesc;
	
	/**
	Not available in the current release.
	Defines the user data buffers which are used to 
	store particle data, which can be used for rendering.
	*/ 
	NxParticleData				particlesWriteData;

	/**
	Not available in the current release.
	Contains only debug visualization flag at the moment.
	*/ 
	NxU32						flags;

#if NX_USE_FLUID_API
	/**
	Not available in the current release.
	Defines the group, equivalent to the actor group.
	*/ 
	NxFluidGroup				fluidGroup;
#endif

	/**
	Not available in the current release.
	Defines the group, equal to the shape groups, in the same space.
	*/ 
	NxCollisionGroup			collisionGroup;	


	void*						userData;	//!< Will be copied to NxFluid::userData
	const char*					name;		//!< Possible debug name.  The string is not copied by the SDK, only the pointer is stored.

	/**
	Not available in the current release.
	Constructor sets to default.
	*/
	NX_INLINE NxFluidDesc();	
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
	};

NX_INLINE NxFluidDesc::NxFluidDesc()
	{
	setToDefault();
	}

NX_INLINE void NxFluidDesc::setToDefault()
	{
	emitters					.clear();

	maxParticles				= MAXPARTICLES;
	restParticlesPerMeter		= 50.0f;
    restDensity					= 1000.0f;
    kernelRadiusMultiplier		= 1.2f;
	stiffness					= 20.0f;
    viscosity					= 6.0f;
    damping						= 0.0f;
    staticCollisionRestitution	= 1.0f;
	staticCollisionAdhesion		= 0.05f;
	dynamicCollisionRestitution	= 1.0f;
	dynamicCollisionAdhesion	= 0.5f;

	simulationMethod			= NX_F_NO_PARTICLE_INTERACTION;
	collisionMethod				= NX_F_STATIC|NX_F_DYNAMIC;
	
	onSeparation				= 0;
	onCollision					= 0;
	onLifetimeExpired			= 0;

	surfaceMeshDesc				= NULL;
	particlesWriteData			.setToDefault();

	flags						= NX_FF_VISUALIZATION;
	
	userData					= NULL;
	name						= NULL;
	}

NX_INLINE bool NxFluidDesc::isValid() const
	{
	if (kernelRadiusMultiplier < 1.0f) return false;
	if (restDensity <= 0.0f) return false;
	if (restParticlesPerMeter <= 0.0f) return false;
	if (stiffness <= 0.0f) return false;
	if (viscosity <= 0.0f) return false;

	if (!(simulationMethod & NX_F_NO_PARTICLE_INTERACTION ^ simulationMethod & NX_F_SPH)) return false;
	
	if (damping < 0.0f || damping > 1.0f) return false;
	if (dynamicCollisionAdhesion < 0.0f || dynamicCollisionAdhesion > 1.0f) return false;
	if (dynamicCollisionRestitution < 0.0f || dynamicCollisionRestitution > 1.0f) return false;
	if (staticCollisionAdhesion < 0.0f || staticCollisionAdhesion > 1.0f) return false;
	if (staticCollisionRestitution < 0.0f || staticCollisionRestitution > 1.0f) return false;
	
	if (onCollision && !(onCollision & NX_F_TURN_DEAD ^ onCollision & NX_F_TURN_SIMPLE)) return false;
	if (onSeparation && !(onSeparation & NX_F_TURN_DEAD ^ onSeparation & NX_F_TURN_SIMPLE)) return false;
	if (onLifetimeExpired && !(onLifetimeExpired & NX_F_TURN_DEAD ^ onLifetimeExpired & NX_F_TURN_SIMPLE)) return false;

	if (!initialParticleData.isValid()) return false;
	if (!particlesWriteData.isValid()) return false;
	if (surfaceMeshDesc && !surfaceMeshDesc->isValid()) return false;

	if (maxParticles > MAXPARTICLES) return false;
	if (emitters.size() > MAX_EMITTERS)
		return false;

	for (unsigned i = 0; i < emitters.size(); i++)
		if (!emitters[i].isValid()) return false;

	return true;
	}

#endif
