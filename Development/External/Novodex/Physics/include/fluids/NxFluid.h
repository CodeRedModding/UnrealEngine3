#ifndef NX_PHYSICS_NX_FLUIDACTOR
#define NX_PHYSICS_NX_FLUIDACTOR
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/
#include "Nxp.h"
#include "NxArray.h"
#include "NxBounds3.h"
#include "NxFluidDesc.h"
#include "NxPhysicsSDK.h"

class NxFluidEmitterDesc;
class NxFluidEmitter;

/**
Not available in the current release.
The fluid class. It represents a dynamic sized set of particles, which potentially may interact 
among each other. Associated with a fluid is a possibly empty list of emitters. An associated 
NxImplicitMesh serves the generation of a fluid surface mesh. All runtime modifiable parameters 
have setter methods.
*/
class NxFluid
	{
	protected:
	NX_INLINE					NxFluid() : userData(NULL)	{}
	virtual						~NxFluid()	{}

	public:

	/**
	Not available in the current release.
	Creates an emitter for this fluid. NxFluidEmitterDesc::isValid() must return true.
	*/
	virtual		NxFluidEmitter*		createEmitter(const NxFluidEmitterDesc&)				= 0;

	/**
	Not available in the current release.
	Deletes the specified emitter. The emitter must belong to this fluid.
	Do not keep a reference to the deleted instance.
	*/
	virtual		void				releaseEmitter(NxFluidEmitter&)							= 0;

	/**
	Not available in the current release.
	Returns the number of emitters.
	*/
	virtual		NxU32				getNbEmitters()									const	= 0;

	/**
	Not available in the current release.
	Returns an array of emitter pointers with size getNbEmitters().
	*/
	virtual		NxFluidEmitter**	getEmitters()									const	= 0;

	/**
	Not available in the current release.
	Creates a surface mesh for this fluid. NxImplicitMeshDesc::isValid() must return true. If a surface mesh
	allready exists for this fluid, it's released.
	*/ 
	virtual		NxImplicitMesh*		createSurfaceMesh(const NxImplicitMeshDesc&)			= 0;

	/**
	Not available in the current release.
	Deletes the surface mesh. Do not keep a reference to the deleted instance.
	*/
	virtual		void				releaseSurfaceMesh()									= 0;

	/**
	Not available in the current release.
	Returns a pointer to the surface mesh of the fluid
	*/
	virtual		NxImplicitMesh*		getSurfaceMesh()								const	= 0;

	/**
	Not available in the current release.
	Adds particles to the simualtion specified with the user buffer wrapper. The SDK only accesses 
	the wrapped buffers until the function returns.
	*/ 
	virtual		void 				addParticles(const NxParticleData&)						= 0;

 	/**
	Not available in the current release.
	Sets the wrapper for user buffers, which configure where particle data is written to.
	*/
	virtual		void 				setParticlesWriteData(const NxParticleData&)			= 0;

 	/**
	Not available in the current release.
	Returns a copy of the wrapper which was set by setParticlesWriteData.
	*/
	virtual		NxParticleData 		getParticlesWriteData()							const	= 0;

	/**
	Not available in the current release.
	Returns the fluid stiffness.
	*/ 
	virtual		NxReal				getStiffness()									const	= 0;

	/**
	Not available in the current release.
	Sets the fluid stiffness, has to be positive.
	*/ 
	virtual		void 				setStiffness(NxReal)									= 0;

	/**
	Not available in the current release.
	Returns the fluid viscosity.
	*/ 
	virtual		NxReal				getViscosity()									const	= 0;

	/**
	Not available in the current release.
	Sets the fluid viscosity, has to be positive.
	*/ 
	virtual		void 				setViscosity(NxReal)									= 0;

	/**
	Not available in the current release.
	Returns the fluid damping.
	*/
	virtual		NxReal				getDamping()									const	= 0;

	/**
	Not available in the current release.
	Sets the fluid damping, has to be positive.
	*/
	virtual		void 				setDamping(NxReal)										= 0;

	/**
	Not available in the current release.
	Returns the fluid collision restitution, used for collision with static actors.
	*/
	virtual		NxReal				getStaticCollisionRestitution()					const	= 0;

	/**
	Not available in the current release.
	Sets the fluid collision restitution, used for collision with static actors. Has to between 0 and 1.
	*/
	virtual		void 				setStaticCollisionRestitution(NxReal)					= 0;

	/**
	Not available in the current release.
	Returns the fluid collision adhesion (friction), used for collision with static actors.
	*/
	virtual		NxReal				getStaticCollisionAdhesion()					const	= 0;

	/**
	Not available in the current release.
	Sets the fluid collision adhesion (friction), used for collision with static actors. Has to between 0 and 1.
	*/
	virtual		void 				setStaticCollisionAdhesion(NxReal)						= 0;

	/**
	Not available in the current release.
	Returns the fluid collision restitution, used for collision with dynamic actors.
	*/
	virtual		NxReal				getDynamicCollisionRestitution()				const	= 0;

	/**
	Not available in the current release.
	Sets the fluid collision restitution, used for collision with dynamic actors. Has to between 0 and 1.
	*/
	virtual		void 				setDynamicCollisionRestitution(NxReal)					= 0;

	/**
	Not available in the current release.
	Returns the fluid collision adhesion (friction), used for collision with dynamic actors.
	*/
	virtual		NxReal				getDynamicCollisionAdhesion()					const	= 0;

	/**
	Not available in the current release.
	Sets the fluid collision adhesion (friction), used for collision with dynamic actors. Has to between 0 and 1.
	*/
	virtual		void 				setDynamicCollisionAdhesion(NxReal)						= 0;

	/**
	Not available in the current release.
	Sets actor flags. See the list of flags in NxFluidDesc.h.
	*/
	virtual		void				setFlag(NxFluidFlag, bool)								= 0;

	/**
	Not available in the current release.
	Returns actor flags. See the list of flags in NxFluidDesc.h.
	*/
	virtual		NX_BOOL				getFlag(NxFluidFlag)							const	= 0;

	/**
	Not available in the current release.
	Sets the fluids collision group, a shared group space with up to 32 groups (5 bit):
	Note that shape groups use the same adress space.  
	*/
	virtual		void				setCollisionGroup(NxCollisionGroup group)				= 0;

	/**
	Not available in the current release.
	Returns the collision group set with setCollisionGroup(NxCollisionGroup group)
	*/
	virtual		NxU32				getCollisionGroup()								const	= 0;

#if NX_USE_FLUID_API

	/**
	Not available in the current release.
	Sets the fluid group, a group space which is independent from the actor group space, (16 bit):
	*/
	virtual		void				setFluidGroup(NxFluidGroup group)						= 0;

	/**
	Not available in the current release.
	Gets the fluid group set by setFluidGroup(NxFluidGroup group).
	*/
	virtual		NxU32				getFluidGroup()									const	= 0;
#endif

	/**
	Not available in the current release.
	Sets the particle action which is applied when the particle separates from its neighbors. Separation
	does not occure if the particle system is simulated with no particle interactions.
	See the list of actions in NxFluidDesc.h.
	*/
	virtual		void 				setOnSeparationAction(NxFluidParticleAction, bool)		= 0;

	/**
	Not available in the current release.
	Returns the on separation action which is set by setOnSeparationAction(NxFluidParticleAction, bool).
	*/
	virtual		NX_BOOL				getOnSeparationAction(NxFluidParticleAction)	const	= 0;
	
	/**
	Not available in the current release.
	Sets the particle action which is applied when the particle collides with actors.
	See the list of actions in NxFluidDesc.h.
	*/
	virtual		void 				setOnCollision(NxFluidParticleAction, bool)				= 0;

	/**
	Not available in the current release.
	Returns the on separation action which is set by getOnCollision(NxFluidParticleAction).
	*/
	virtual		NX_BOOL				getOnCollision(NxFluidParticleAction)			const	= 0;
	
	/**
	Not available in the current release.
	Sets the particle action which is applied when the particle lifetime expires.
	See the list of actions in NxFluidDesc.h.
	*/
	virtual		void 				setOnLifetimeExpired(NxFluidParticleAction, bool)		= 0;

	/**
	Not available in the current release.
	Returns the on separation action which is set by setOnLifetimeExpired(NxFluidParticleAction, bool).
	*/
	virtual		NX_BOOL				getOnLifetimeExpired(NxFluidParticleAction)		const	= 0;

	/**
	Not available in the current release.
	Returns the maximal number of particles for this fluid.
	*/
	virtual		NxU32 				getMaxParticles()							const	= 0;

	/**
	Not available in the current release.
	Returns the kernel radius multiplier (the particle interact within a radius of 
	getRestParticleDistance() * getKernelRadiusMultiplier().
	*/
	virtual		NxReal				getKernelRadiusMultiplier()					const	= 0;

	/**
	Not available in the current release.
	Returns the number of particles per meter in the relaxed state of the fluid.
	*/
	virtual		NxReal				getRestParticlesPerMeter()					const	= 0;

	/**
	Not available in the current release.
	Returns the density in the relaxed state of the fluid. 
	*/
	virtual		NxReal				getRestDensity()							const	= 0;

	/**
	Not available in the current release.
	Returns the inter particle distance in the relaxed state of the fluid.
	*/
	virtual		NxReal				getRestParticleDistance()					const	= 0;

	/**
	Not available in the current release.
	Returns the mass of a particle. This value is dependent on the rest inter particle distance and the
	rest density of the fluid.
	*/
	virtual		NxReal				getParticleMass()							const	= 0;

	/**
	Not available in the current release.
	Returns the simulation method of the fluid. See NxFluidSimulationMethod for the possible methods.
	*/
	virtual		NxU32				getSimulationMethod()						const	= 0;
	
	/**
	Not available in the current release.
	Returns the collision method of the fluid. See NxFluidCollisionMethod for the possible values.
	*/
	virtual		NxU32				getCollisionMethod()						const	= 0;

	/**
	Not available in the current release.
	Returns the minimal (exact) AABB including all simulated particles.
	*/ 
	virtual		void				getWorldBounds(NxBounds3& dest)				const	= 0;

	/**
	Not available in the current release.
	Sets a name string for the object that can be retrieved with getName().  This is for debugging and is not used
	by the SDK.  The string is not copied by the SDK, only the pointer is stored.
	*/
	virtual	void			setName(const char*)		= 0;

	/**
	Not available in the current release.
	retrieves the name string set with setName().
	*/
	virtual	const char*		getName()			const	= 0;

	//public variables:
	void*			userData;	//!< user can assign this to whatever, usually to create a 1:1 relationship with a user object.
	};

#endif
