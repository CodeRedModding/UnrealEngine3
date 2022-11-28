#ifndef NX_PHYSICS_NXSPHERICALJOINT
#define NX_PHYSICS_NXSPHERICALJOINT
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

#include "NxSphericalJointDesc.h"

#include "NxJoint.h"

/**
 A sphere joint constrains two points on two bodies to coincide.
 This point, specified in world space (this guarantees that the points coincide 
 to start with) is the only parameter that has to be specified.
*/
class NxSphericalJoint: public NxJoint
	{
	public:

	/**
	use this for changing a significant number of joint parameters at once.
	Use the set() methods for changing only a single property at once.
	*/
	virtual void loadFromDesc(const NxSphericalJointDesc &) = 0;

	/**
	writes all of the object's attributes to the desc struct  
	*/
	virtual void saveToDesc(NxSphericalJointDesc &) = 0;

	/**
	sets the flags to enable/disable the spring/motor/limit.	This is a combination of the bits defined by ::NxRevoluteJointFlag.
	*/
	virtual void setFlags(NxU32 flags) = 0;

	/**
	returns the current flag settings.
	*/
	virtual NxU32 getFlags() = 0;

	/**
	sets the joint projection mode.
	*/
	virtual void setProjectionMode(NxJointProjectionMode projectionMode) = 0;

	/**
	returns the current flag settings.
	*/
	virtual NxJointProjectionMode getProjectionMode() = 0;

	/**
	\deprecated { casts to a superclass are now implicit }
	casts to joint type
	*/ 
	NX_INLINE operator NxJoint &()		{ return *this; }

	/**
	\deprecated { casts to a superclass are now implicit }
	casts to joint type
	*/ 
	NX_INLINE NxJoint & getJoint()		{ return *this; };

	};
#endif
