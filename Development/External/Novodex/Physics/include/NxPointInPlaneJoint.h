#ifndef NX_PHYSICS_NXPOINTINPLANEJOINT
#define NX_PHYSICS_NXPOINTINPLANEJOINT
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

#include "Nxp.h"
#include "NxJoint.h"

class NxPointInPlaneJointDesc;

/**
 A point in plane joint constrains a point on one body to only move inside
 a plane attached to another body.

 The starting point of the point is defined as the anchor point. The plane
 through this point is specified by its normal which is the joint axis vector.
*/
class NxPointInPlaneJoint : public NxJoint
	{
	public:
	/**
	use this for changing a significant number of joint parameters at once.
	Use the set() methods for changing only a single property at once.
	*/
	virtual void loadFromDesc(const NxPointInPlaneJointDesc&) = 0;

	/**
	writes all of the object's attributes to the desc struct  
	*/
	virtual void saveToDesc(NxPointInPlaneJointDesc&) = 0;

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
