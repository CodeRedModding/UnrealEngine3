#ifndef NX_COLLISION_NXTRIANGLEMESHSHAPEDESC
#define NX_COLLISION_NXTRIANGLEMESHSHAPEDESC
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

#include "NxShapeDesc.h"
#include "NxTriangleMeshShape.h"

/**
Descriptor class for NxTriangleMeshShape.
*/
class NxTriangleMeshShapeDesc : public NxShapeDesc
	{
	public:
	NxTriangleMesh*	meshData;	//!< References the triangle mesh that we want to instance.
	NxU32			meshFlags;	//!< Combination of ::NxMeshShapeFlag
#ifdef NX_SUPPORT_MESH_SCALE
	NxReal			scale;
#endif
	/**
	constructor sets to default.
	*/
	NX_INLINE					NxTriangleMeshShapeDesc();	
	/**
	(re)sets the structure to the default.	
	*/
	NX_INLINE virtual	void	setToDefault();
	/**
	returns true if the current settings are valid
	*/
	NX_INLINE virtual	bool	isValid() const;
	};

NX_INLINE NxTriangleMeshShapeDesc::NxTriangleMeshShapeDesc() : NxShapeDesc(NX_SHAPE_MESH)	//constructor sets to default
	{
	setToDefault();
	}

NX_INLINE void NxTriangleMeshShapeDesc::setToDefault()
	{
	NxShapeDesc::setToDefault();
	meshData	= NULL;
	meshFlags	= 0;
#ifdef NX_SUPPORT_MESH_SCALE
	scale		= 1.0f;
#endif
	}

NX_INLINE bool NxTriangleMeshShapeDesc::isValid() const
	{
	if(!meshData)	return false;
#ifdef NX_SUPPORT_MESH_SCALE
	if(scale<=0.0f)	return false;
#endif
	return NxShapeDesc::isValid();
	}

#endif
