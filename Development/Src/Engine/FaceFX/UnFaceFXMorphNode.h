//------------------------------------------------------------------------------
// Unreal-specific FaceFX Face Graph node to support animating morph targets.
//
// Owner: Jamie Redmond
//
// Copyright (c) 2002-2005 OC3 Entertainment, Inc.
//------------------------------------------------------------------------------

#ifndef UnFaceFXMorphNode_H__
#define UnFaceFXMorphNode_H__

#include "UnFaceFXNode.h"

using OC3Ent::Face::FxFaceGraphNode;
using OC3Ent::Face::FxArchive;

#define FX_MORPH_NODE_TARGET_NAME_INDEX 0

// An Unreal morph target FaceFX Face Graph node.
class FUnrealFaceFXMorphNode : public FUnrealFaceFXNode
{
	// Declare the class.
	FX_DECLARE_CLASS(FUnrealFaceFXMorphNode, FUnrealFaceFXNode)
	// Disable copy construction and assignment.
	FX_NO_COPY_OR_ASSIGNMENT(FUnrealFaceFXMorphNode)
public:
	// Constructor.
	FUnrealFaceFXMorphNode();
	// Destructor.
	virtual ~FUnrealFaceFXMorphNode();

	// Clones the material parameter node.
	virtual FxFaceGraphNode* Clone( void );

	// Copies the data from this object into the other object.
	virtual void CopyData( FxFaceGraphNode* pOther );

	// Serializes an FUnrealFaceFXMorphNode to an archive.
	virtual void Serialize( FxArchive& arc );
};

#endif
