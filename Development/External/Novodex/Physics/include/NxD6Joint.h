#ifndef NX_PHYSICS_NXD6Joint
#define NX_PHYSICS_NXD6Joint
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/
#include "Nxp.h"
#include "NxJoint.h"

class NxD6JointDesc;

/**
 A 6D joint is a general constraint between two actors.
*/
class NxD6Joint: public NxJoint
{
 
	public:
 
	/**
	use this for changing a significant number of joint parameters at once.
	Use the set() methods for changing only a single property at once.
	*/
 
	virtual void loadFromDesc(const NxD6JointDesc&) = 0;
 
	/**
	writes all of the object's attributes to the desc struct  
	*/
 
	virtual void saveToDesc(NxD6JointDesc&) = 0;
 
	 
	/**
	Set the drive position
	*/
 
	virtual void setDrivePosition(const NxVec3 &position) = 0;
 
	/**
	Set the drive orientation
	*/
 
	virtual void setDriveOrientation(const NxQuat &orientation) = 0; 
 
	/**
	Set the drive linear velocity
	*/
 
	virtual void setDriveLinearVelocity(const NxVec3 &linVel) = 0;
 
	/**
	Set the drive angular velocity
	*/
 
	virtual void setDriveAngularVelocity(const NxVec3 &angVel) = 0;

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
