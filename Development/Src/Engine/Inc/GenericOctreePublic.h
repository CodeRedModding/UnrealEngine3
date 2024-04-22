/*=============================================================================
	GenericOctreePublic.h: Generic octree definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __GENERIC_OCTREE_PUBLIC_H__
#define __GENERIC_OCTREE_PUBLIC_H__

/** 
 *	An identifier for an element in the octree. 
 *	If this changes, please update OctreeElementId in Object.uc
 */
class FOctreeElementId
{
public:

	template<typename,typename>
	friend class TOctree;

	/** Default constructor. */
	FOctreeElementId()
		:	Node(NULL)
		,	ElementIndex(INDEX_NONE)
	{}

	/** @return a boolean value representing whether the id is NULL. */
	UBOOL IsValidId() const
	{
		return Node != NULL;
	}

private:

	/** The node the element is in. */
	const void* Node;

	/** The index of the element in the node's element array. */
	INT ElementIndex;

	/** Initialization constructor. */
	FOctreeElementId(const void* InNode,INT InElementIndex)
		:	Node(InNode)
		,	ElementIndex(InElementIndex)
	{}

	/** Implicit conversion to the element index. */
	operator INT() const
	{
		return ElementIndex;
	}
};

#endif // __GENERIC_OCTREE_PUBLIC_H__
