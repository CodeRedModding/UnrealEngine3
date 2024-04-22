/*=============================================================================
	SkelMeshDecalVertexFactory.h: Base class for vertex factories of decals on skeletal meshes.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SKELMESHDECALVERTEXFACTORY_H__
#define __SKELMESHDECALVERTEXFACTORY_H__

/** Baseclass for vertex factories of decals on skeletal meshes. */
class FSkelMeshDecalVertexFactoryBase : public FDecalVertexFactoryBase
{
public:
	virtual FSkelMeshDecalVertexFactoryBase* CastToFSkelMeshDecalVertexFactoryBase()=0;
};

#endif // __SKELMESHDECALVERTEXFACTORY_H__
