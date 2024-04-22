//------------------------------------------------------------------------------
// Unreal-specific FaceFX Face Graph node to support animating scalar material
// parameters.
//
// Owner: Jamie Redmond
//
// Copyright (c) 2002-2005 OC3 Entertainment, Inc.
//------------------------------------------------------------------------------

#ifndef UnFaceFXMaterialNode_H__
#define UnFaceFXMaterialNode_H__

#include "UnFaceFXNode.h"

using OC3Ent::Face::FxFaceGraphNode;
using OC3Ent::Face::FxArchive;

#define FX_MATERIAL_PARAMETER_NODE_MATERIAL_SLOT_ID_INDEX 0
#define FX_MATERIAL_PARAMETER_NODE_PARAMETER_NAME_INDEX   1

// An Unreal scalar material parameter FaceFX Face Graph node.
class FUnrealFaceFXMaterialParameterNode : public FUnrealFaceFXNode
{
	// Declare the class.
	FX_DECLARE_CLASS(FUnrealFaceFXMaterialParameterNode, FUnrealFaceFXNode)
	// Disable copy construction and assignment.
	FX_NO_COPY_OR_ASSIGNMENT(FUnrealFaceFXMaterialParameterNode)
public:
	// Constructor.
	FUnrealFaceFXMaterialParameterNode();
	// Destructor.
	virtual ~FUnrealFaceFXMaterialParameterNode();

	// Clones the material parameter node.
	virtual FxFaceGraphNode* Clone( void );

	// Copies the data from this object into the other object.
	virtual void CopyData( FxFaceGraphNode* pOther );

	// Serializes an FUnrealFaceFXMaterialParameterNode to an archive.
	virtual void Serialize( FxArchive& arc );
};

#endif
