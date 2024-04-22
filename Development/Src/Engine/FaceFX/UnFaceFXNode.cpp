//------------------------------------------------------------------------------
// Unreal-specific FaceFX Face Graph node type.
//
// Owner: Jamie Redmond
//
// Copyright (c) 2002-2005 OC3 Entertainment, Inc.
//------------------------------------------------------------------------------

#include "UnFaceFXNode.h"

using namespace OC3Ent;
using namespace Face;

//------------------------------------------------------------------------------
// FUnrealFaceFXNode.
//------------------------------------------------------------------------------

#define kCurrentFUnrealFaceFXNodeVersion 0

FX_IMPLEMENT_CLASS(FUnrealFaceFXNode, kCurrentFUnrealFaceFXNodeVersion, FxGenericTargetNode)

FUnrealFaceFXNode::FUnrealFaceFXNode()
{
	_shouldLink = FxFalse;
}

FUnrealFaceFXNode::~FUnrealFaceFXNode()
{
}

FxFaceGraphNode* FUnrealFaceFXNode::Clone( void )
{
	FUnrealFaceFXNode* pNode = new FUnrealFaceFXNode();
	CopyData(pNode);
	return pNode;
}

void FUnrealFaceFXNode::CopyData( FxFaceGraphNode* pOther )
{
	Super::CopyData(pOther);
}

FxBool FUnrealFaceFXNode::ShouldLink( void ) const
{
	return _shouldLink;
}

void FUnrealFaceFXNode::SetShouldLink( FxBool shouldLink )
{
	_shouldLink = shouldLink;
}

void FUnrealFaceFXNode::
ReplaceUserProperty( const FxFaceGraphNodeUserProperty& userProperty )
{
	Super::ReplaceUserProperty(userProperty);
	// Force a re-link during the next render.
	_shouldLink = FxTrue;
}

void FUnrealFaceFXNode::Serialize( FxArchive& arc )
{
	Super::Serialize(arc);

	arc.SerializeClassVersion("FUnrealFaceFXNode");
}
