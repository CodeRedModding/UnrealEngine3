#ifndef NX_PHYSICS_NX_IMPLICITMESH
#define NX_PHYSICS_NX_IMPLICITMESH
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/
#include "Nxp.h"
#include "NxPhysicsSDK.h"

class NxImplicitMeshDesc;
class NxParticleData;
class NxMeshData;

/**
Not available in the current release.
The implicit mesh class, used to generate a fluid surface mesh or a
an implicte mesh defined by a set of user points. The user mesh
buffers (indices and vertices) have to be specified using the 
NxMeshData wrapper in order to generate a mesh.
If particles are specified using setParticles an implicite mesh
for these particles is generated.
If the Fluid data mesh is owned by an NxFluid instance, a mesh is 
only generated for the fluid and the particles specified with setParticles()
are ignored.
*/
class NxImplicitMesh
	{
	protected:
	NX_INLINE						NxImplicitMesh() : userData(NULL)	{}
	virtual							~NxImplicitMesh()	{}

	public:

	/**
	Not available in the current release.
	Sets particles which are used for custom implicit mesh generation. This is done 
	by passing a NxParticleData user buffer wrapper. The specified buffers are only 
	read until the function returns. Calling this method when the NxImpliciteMesh 
	instance is owned by an NxFluid instance has no effect. 
	*/
	virtual		void 				setParticles(NxParticleData&) const = 0;

	/**
	Not available in the current release.
	Sets the user buffer wrapper for the implict mesh.
	*/
	virtual		void 				setMeshData(NxMeshData&) = 0;

	/**
	Not available in the current release.
	Returns a copy of the user buffer wrapper for the implict mesh.
	*/
	virtual		NxMeshData			getMeshData() = 0;

	/**
	Not available in the current release.
	Returns the maximal distance of the mesh to the particles.
	*/
	virtual		NxReal				getSurfaceRadius() const = 0;

	/**
	Not available in the current release.
	Sets the maximal distance of the mesh to the particles.
	*/
	virtual		void 				setSurfaceRadius(NxReal) = 0;

	/**
	Not available in the current release.
	Sets the minimal distance of the mesh to the particles (Distance of the mesh to one separated particle).
	*/
	virtual		NxReal				getSingleRadius() const = 0;

	/**
	Not available in the current release.
	Returns the minimal distance of the mesh to the particles (Distance of the mesh to one separated particle).
	*/
	virtual		void 				setSingleRadius(NxReal) = 0;

	/**
	Not available in the current release.
	Returns the the blend parameter of the implicit mesh.
	*/
	virtual		NxReal				getBlend() const = 0;

	/**
	Not available in the current release.
	Sets the the blend parameter of the implicit mesh.
	*/
	virtual		void 				setBlend(NxReal) = 0;

	/**
	Not available in the current release.
	Return the spatial resolution of the mesh.
	*/
	virtual		NxReal				getTriangleSize() const = 0;

	/**
	Not available in the current release.
	Sets the spatial resolution of the mesh.
	*/
	virtual		void 				setTriangleSize(NxReal) = 0;

	/**
	Not available in the current release.
	Return the number of smoothing iterations performed.
	*/
	virtual		NxU32				getSmoothingIterations() const = 0;

	/**
	Not available in the current release.
	Sets the number of smoothing iterations performed.
	*/
	virtual		void 				setSmoothingIterations(NxU32) = 0;

	/**
	Not available in the current release.
	Sets a name string for the object that can be retrieved with getName().  This is for debugging and is not used
	by the SDK.  The string is not copied by the SDK, only the pointer is stored.
	*/
	virtual	void			setName(const char*)		= 0;

	/**
	Not available in the current release.
	Retrieves the name string set with setName().
	*/
	virtual	const char*		getName()			const	= 0;

	//public variables:
	void*					userData;	//!< user can assign this to whatever, usually to create a 1:1 relationship with a user object.
	};

#endif
