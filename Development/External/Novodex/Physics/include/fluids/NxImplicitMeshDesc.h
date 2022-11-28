#ifndef NX_PHYSICS_NX_IMPLICITMESHDESC
#define NX_PHYSICS_NX_IMPLICITMESHDESC
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

#include "fluids/NxImplicitMesh.h"
#include "fluids/NxMeshData.h"
/**
Not available in the current release.
Specifies the parameters for implict mesh generation.
*/
class NxImplicitMeshDesc
	{
	public:

	/**
	Not available in the current release.
	The maximal distance at which points are considered for surface generation. 
	This parameter has a large impact on the performance of surface generation. 
	*/
	NxReal		surfaceRadius;

	/**
	Not available in the current release.
	The distance between the fluid surface and one separated 
	particle. Has to be smaller than the surfaceRadius.
	*/
	NxReal		singleRadius;

	/**
	Not available in the current release.
	Specifies how the surface around particles bulges. Smaller give less bumpy surfaces 
	which have more temporal incontinuities (popping). 
	*/
	NxReal		blend;

	/**
	Not available in the current release.
	Sets the spatial resolution of the surface mesh generated.
  	*/
	NxReal		triangleSize;

	/**
	Not available in the current release.
 	Sets the number of smoothing iterations performed on the mesh.
	*/
	NxU32		smoothingIterations;

	/**
	Not available in the current release.
	Defines how the generated mesh is written to user mesh buffers.
	*/
	NxMeshData	meshData;

	void*		userData;		//!< Will be copied to NxImplicitMesh::userData.
	const char*	name;			//!< Possible debug name. The string is not copied by the SDK, only the pointer is stored.

	NX_INLINE ~NxImplicitMeshDesc();
	/**
	Not available in the current release.
	(Re)sets the structure to the default.	
	*/
	NX_INLINE void setToDefault();
	/**
	Not available in the current release.
	Returns true if the current settings are valid
	*/
	NX_INLINE bool isValid() const;

	/**
	Not available in the current release.
	Constructor sets to default.
	*/
	NX_INLINE	NxImplicitMeshDesc();
	};


NX_INLINE NxImplicitMeshDesc::NxImplicitMeshDesc()
	{
	setToDefault();
	}

NX_INLINE NxImplicitMeshDesc::~NxImplicitMeshDesc()
	{
	}

NX_INLINE void NxImplicitMeshDesc::setToDefault()
	{
	surfaceRadius					= 0.02f;
	singleRadius					= 0.001f;
	blend							= 0.01f;
	triangleSize					= 0.01f;
	smoothingIterations				= 4;

	meshData						.setToDefault();

	userData						= NULL;
	name							= NULL;
	}

NX_INLINE bool NxImplicitMeshDesc::isValid() const
	{
	if (blend < 0.0f) return false;
	if (singleRadius <= 0.0f) return false;
	if (surfaceRadius < singleRadius) return false;
	if (triangleSize <= 0.0f) return false;
	if (triangleSize > surfaceRadius) return false;

	if (!meshData.isValid()) return false;
	
	return true;
	}


#endif
