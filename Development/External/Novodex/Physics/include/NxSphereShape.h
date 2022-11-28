#ifndef NX_COLLISION_NXSPHERESHAPE
#define NX_COLLISION_NXSPHERESHAPE
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

#include "Nxp.h"
#include "NxShape.h"

class NxSphereShapeDesc;

/**
A sphere shaped collision detection primitive.


Each shape is owned by an actor that it is attached to.

An instance can be created by calling the createShape() method of the NxActor object
that should own it, with a NxSphereShapeDesc object as the parameter, or by adding the 
shape descriptor into the NxActorDesc class before creating the actor.

The shape is deleted by calling NxActor::releaseShape() on the owning actor.
*/

class NxSphereShape : public NxShape
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
	Call this to initialize or alter the sphere. If this is not called,
	then the default settings create a unit sphere at the origin.
	*/
	virtual void setRadius(NxReal radius) = 0;

	/**
	Retrieves the radius of the sphere.
	*/

	virtual NxReal getRadius() const = 0;

	virtual bool saveToDesc(NxSphereShapeDesc&)	const = 0;
	};
#endif
