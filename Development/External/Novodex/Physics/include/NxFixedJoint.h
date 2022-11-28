#ifndef NX_PHYSICS_NXFIXEDJOINT
#define NX_PHYSICS_NXFIXEDJOINT
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

#include "Nxp.h"
#include "NxJoint.h"

class NxFixedJointDesc;

/**
 A fixed joint permits no relative movement between two bodies.
*/
class NxFixedJoint: public NxJoint
	{
	public:
	/**
	use this for changing a significant number of joint parameters at once.
	Use the set() methods for changing only a single property at once.
	*/
	virtual void loadFromDesc(const NxFixedJointDesc&) = 0;

	/**
	writes all of the object's attributes to the desc struct  
	*/
	virtual void saveToDesc(NxFixedJointDesc&) = 0;

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
