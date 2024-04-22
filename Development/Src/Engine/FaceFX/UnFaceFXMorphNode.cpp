//------------------------------------------------------------------------------
// Unreal-specific FaceFX Face Graph node to support animating morph targets.
//
// Owner: Jamie Redmond
//
// Copyright (c) 2002-2005 OC3 Entertainment, Inc.
//------------------------------------------------------------------------------

#include "UnFaceFXMorphNode.h"

using namespace OC3Ent;
using namespace Face;

//------------------------------------------------------------------------------
// FUnrealFaceFXMorphNode.
//------------------------------------------------------------------------------

#define kCurrentFUnrealFaceFXMorphNodeVersion 0

FX_IMPLEMENT_CLASS(FUnrealFaceFXMorphNode, kCurrentFUnrealFaceFXMorphNodeVersion, FUnrealFaceFXNode)

FUnrealFaceFXMorphNode::FUnrealFaceFXMorphNode()
{
	_isPlaceable = FxTrue;
	FxFaceGraphNodeUserProperty targetNameProperty(UPT_String);
	targetNameProperty.SetName("Target Name");
	targetNameProperty.SetStringProperty(FxString("Target"));
	AddUserProperty(targetNameProperty);
}

FUnrealFaceFXMorphNode::~FUnrealFaceFXMorphNode()
{
}

FxFaceGraphNode* FUnrealFaceFXMorphNode::Clone( void )
{
	FUnrealFaceFXMorphNode* pNode = new FUnrealFaceFXMorphNode();
	CopyData(pNode);
	return pNode;
}

void FUnrealFaceFXMorphNode::CopyData( FxFaceGraphNode* pOther )
{
	Super::CopyData(pOther);
}

void FUnrealFaceFXMorphNode::Serialize( FxArchive& arc )
{
	Super::Serialize(arc);

	arc.SerializeClassVersion("FUnrealFaceFXMorphNode");
}
