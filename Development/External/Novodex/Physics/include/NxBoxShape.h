#ifndef NX_COLLISION_NXBOXSHAPE
#define NX_COLLISION_NXBOXSHAPE
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

#include "Nxp.h"
#include "NxShape.h"

class NxBox;
class NxBoxShapeDesc;

/**
A box shaped collision detection primitive.

Each shape is owned by an actor that it is attached to.

An instance can be created by calling the createShape() method of the NxActor object
that should own it, with a NxBoxShapeDesc object as the parameter, or by adding the 
shape descriptor into the NxActorDesc class before creating the actor.

The shape is deleted by calling NxActor::releaseShape() on the owning actor.
*/
class NxBoxShape: public NxShape
	{
	public:

	/**
	\deprecated { casts to a superclass are now implicit }
	casts to shape type
	*/ 
	NX_INLINE operator NxShape &()					{ return *this; }

	/**
	\deprecated { casts to a superclass are now implicit }
	casts to shape type
	*/ 
	NX_INLINE NxShape & getShape()					{ return *this; }
	NX_INLINE const NxShape & getShape() const		{ return *this; }

	/**
	The dimensions are the 'radii' of the box, meaning 1/2 extents in x dimension, 
	1/2 extents in y dimension, 1/2 extents in z dimension. 
	*/
	virtual void setDimensions(const NxVec3&) = 0;

	/**
	Retrieves the dimensions.
	*/
	virtual const NxVec3& getDimensions() const = 0;

	/**
	Gets the box data in world space.
	*/
	virtual void getWorldOBB(NxBox&) const = 0;

	virtual	bool saveToDesc(NxBoxShapeDesc&) const = 0;
	};
#endif
