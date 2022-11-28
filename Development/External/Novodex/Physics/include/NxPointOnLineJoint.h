#ifndef NX_PHYSICS_NXPOINTONLINEJOINT
#define NX_PHYSICS_NXPOINTONLINEJOINT
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

#include "Nxp.h"

#include "NxJoint.h"
class NxPointOnLineJointDesc;

/**
 A point on line joint constrains a point on one body to only move along
 a line attached to another body.

 The starting point of the point is defined as the anchor point. The line
 through this point is specified by its direction (axis) vector.
*/
class NxPointOnLineJoint: public NxJoint
	{
	public:
	/**
	use this for changing a significant number of joint parameters at once.
	Use the set() methods for changing only a single property at once.
	*/
	virtual void loadFromDesc(const NxPointOnLineJointDesc&) = 0;

	/**
	writes all of the object's attributes to the desc struct  
	*/
	virtual void saveToDesc(NxPointOnLineJointDesc&) = 0;

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
