#ifndef NX_PHYSICS_NXDISTANCEJOINT
#define NX_PHYSICS_NXDISTANCEJOINT
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

#include "Nxp.h"
#include "NxJoint.h"

class NxDistanceJointDesc;

/**
 A distance joint maintains a certain distance between two points on two actors.
*/
class NxDistanceJoint : public NxJoint
	{
	public:
	/**
	use this for changing a significant number of joint parameters at once.
	Use the set() methods for changing only a single property at once.
	*/
	virtual void loadFromDesc(const NxDistanceJointDesc&) = 0;

	/**
	writes all of the object's attributes to the desc struct  
	*/
	virtual void saveToDesc(NxDistanceJointDesc&) = 0;

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
