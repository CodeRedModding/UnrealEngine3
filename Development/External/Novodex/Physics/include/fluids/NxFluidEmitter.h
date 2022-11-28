#ifndef NX_FLUIDS_NXFLUIDEMITTER
#define NX_FLUIDS_NXFLUIDEMITTER
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/
#include "Nxp.h"
#include "NxPhysicsSDK.h"
#include "NxAllocateable.h"

#include "NxFluidEmitterDesc.h"

class NxFluid;

/**
Not available in the current release.
The fluid emitter class. It represents an emitter (fluid source) which is associated with one fluid.
*/
class NxFluidEmitter
	{
	protected:
	NX_INLINE				NxFluidEmitter() : userData(NULL)	{}
	virtual					~NxFluidEmitter()	{}

	public:

	/**
	Not available in the current release.
	Returns the owner fluid.
	*/
	virtual		NxFluid &	getFluid() const = 0;

	/**
	Not available in the current release.
	Sets the pose of the emitter in world space.
	*/
	virtual		void		setGlobalPose(const NxMat34&)					= 0;

	/**
	Not available in the current release.
	Sets the position of the emitter in world space.
	*/
	virtual		void		setGlobalPosition(const NxVec3&)				= 0;

	/**
	Not available in the current release.
	Sets the orientation of the emitter in world space.
	*/
	virtual		void		setGlobalOrientation(const NxMat33&)			= 0;

	/**
	Not available in the current release.
	The get*Val() methods work just like the get*() methods, except they return the 
	desired values instead of copying them to destination variables.
	*/
#ifndef DOXYGEN
	virtual		NxMat34		getGlobalPoseVal()						const	= 0;
	virtual		NxVec3		getGlobalPositionVal()					const	= 0;
	virtual		NxMat33		getGlobalOrientationVal()				const	= 0;
#endif

	/**
	Not available in the current release.
	Returns the pose of the emitter in world space.
	*/
	NX_INLINE	NxMat34		getGlobalPose()							const	{ return getGlobalPoseVal();		}

	/**
	Not available in the current release.
	Returns the position of the emitter in world space.
	*/
	NX_INLINE	NxVec3		getGlobalPosition()						const	{ return getGlobalPositionVal();	}

	/**
	Not available in the current release.
	Returns the orientation of the emitter in world space.
	*/
	NX_INLINE	NxMat33		getGlobalOrientation()					const	{ return getGlobalOrientationVal();	}

	/**
	Not available in the current release.
	Sets the pose of the emitter relative to the frameActors coordinate frame. 
	If the frameActor is NULL, world space is used.
	*/
	virtual		void		setLocalPose(const NxMat34&)					= 0;

	/**
	Not available in the current release.
	Sets the position of the emitter relative to the frameActors coordinate frame. 
	If the frameActor is NULL, world space is used.
	*/
	virtual		void		setLocalPosition(const NxVec3&)					= 0;

	/**
	Not available in the current release.
	Sets the orientation of the emitter relative to the frameActors coordinate frame. 
	If the frameActor is NULL, world space is used.
	*/
	virtual		void		setLocalOrientation(const NxMat33&)				= 0;

	/**
	Not available in the current release.
	The get*Val() methods work just like the get*() methods, except they return the 
	desired values instead of copying them to the destination variables.
	*/
#ifndef DOXYGEN
	virtual		NxMat34		getLocalPoseVal()						const	= 0;
	virtual		NxVec3		getLocalPositionVal()					const	= 0;
	virtual		NxMat33		getLocalOrientationVal()				const	= 0;
#endif

	/**
	Not available in the current release.
	Returns the pose of the emitter relative to the frameActors coordinate frame. 
	If the frameActor is NULL, world space is used.
	*/
	NX_INLINE	NxMat34		getLocalPose()							const	{ return getLocalPoseVal();		}
	
	/**
	Not available in the current release.
	Returns the position of the emitter relative to the frameActors coordinate frame. 
	If the frameActor is NULL, world space is used.
	*/
	NX_INLINE	NxVec3		getLocalPosition()						const	{ return getLocalPositionVal();	}

	/**
	Not available in the current release.
	Returns the orientation of the emitter relative to the frameActors coordinate frame. 
	If the frameActor is NULL, world space is used.
	*/
	NX_INLINE	NxMat33		getLocalOrientation()					const	{ return getLocalOrientationVal();	}

	/**
	Not available in the current release.
	Sets the frame actor. Can be set to NULL.
	*/
	virtual		void 		setFrameActor(NxActor*)							= 0;

	/**
	Not available in the current release.
	Returns the frame actor. May be NULL.
	*/
	virtual		NxActor * 	getFrameActor()							const	= 0;

	/**
	Not available in the current release.
	Returns the radius of the emitter along the x axis.
	*/
	virtual		NxReal 			getDimensionX()						const	= 0;

	/**
	Not available in the current release.
	Returns the radius of the emitter along the y axis.
	*/
	virtual		NxReal 			getDimensionY()						const	= 0;

	/**
	Not available in the current release.
	Sets the maximal random displacement in every dimesion.
	*/
	virtual		void 			setRandomPos(NxVec3)						= 0;

	/**
	Not available in the current release.
	Returns the maximal random displacement in every dimesion.
	*/
	virtual		NxVec3 			getRandomPos()						const	= 0;

	/**
	Not available in the current release.
	Sets the maximal random angle offset. 
	*/
	virtual		void 			setRandomAngle(NxReal)						= 0;

	/**
	Not available in the current release.
	Returns the maximal random angle offset. 
	*/
	virtual		NxReal 			getRandomAngle()					const	= 0;

	/**
	Not available in the current release.
	Sets the velocity magnitude of the emitted particles. 
	*/
	virtual		void 			setFluidVelocityMagnitude(NxReal)			= 0;

	/**
	Not available in the current release.
	Returns the velocity magnitude of the emitted particles. 
	*/
	virtual		NxReal 			getFluidVelocityMagnitude()			const	= 0;

	/**
	Not available in the current release.
	Sets the emission rate (particles/second). Only used if the NxEmitterType is 
	NX_FE_CONSTANT_FLOW_RATE.
	*/
	virtual		void 			setRate(NxReal)								= 0;

	/**
	Not available in the current release.
	Returns the emission rate.
	*/
	virtual		NxReal 			getRate()							const	= 0;

	/**
	Not available in the current release.
	Sets the particle lifetime.
	*/
	virtual		void 			setParticleLifetime(NxReal)					= 0;

	/**
	Not available in the current release.
	Returns the particle lifetime.
	*/
	virtual		NxReal 			getParticleLifetime()				const	= 0;

	/**
	Not available in the current release.
	Sets the emitter flags. See the list of flags in NxFluidEmitterDesc.h.
	*/
	virtual		void			setFlag(NxFluidEmitterFlag, bool)			= 0;

	/**
	Not available in the current release.
	Returns the emitter flags. See the list of flags in NxFluidEmitterDesc.h.
	*/
	virtual		NX_BOOL			getFlag(NxFluidEmitterFlag)			const	= 0;

	/**
	Not available in the current release.
	Get the emitter shape. See the list of methods in NxFluidEmitterDesc.h.
	*/
	virtual		NX_BOOL			getShape(NxEmitterShape)			const	= 0;

	/**
	Not available in the current release.
	Get the emitter type. See the list of actions in NxFluidEmitterDesc.h.
	*/
	virtual		NX_BOOL			getType(NxEmitterType)				const	= 0;


	/**
	Not available in the current release.
	Sets a name string for the object that can be retrieved with getName().  This is for debugging and is not used
	by the SDK.  The string is not copied by the SDK, only the pointer is stored.
	*/
	virtual		void			setName(const char*)		= 0;

	/**
	Not available in the current release.
	retrieves the name string set with setName().
	*/
	virtual		const char*		getName()			const	= 0;

	void*				userData;	//!< user can assign this to whatever, usually to create a 1:1 relationship with a user object.
	};
#endif
