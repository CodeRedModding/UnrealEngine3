//------------------------------------------------------------------------------
// Unreal-specific FaceFX Face Graph node to support animating scalar material
// parameters.
//
// Owner: Jamie Redmond
//
// Copyright (c) 2002-2005 OC3 Entertainment, Inc.
//------------------------------------------------------------------------------

#include "UnFaceFXMaterialNode.h"

using namespace OC3Ent;
using namespace Face;

//------------------------------------------------------------------------------
// FUnrealFaceFXMaterialParameterNode.
//------------------------------------------------------------------------------

#define kCurrentFUnrealFaceFXMaterialParameterNodeVersion 0

FX_IMPLEMENT_CLASS(FUnrealFaceFXMaterialParameterNode, kCurrentFUnrealFaceFXMaterialParameterNodeVersion, FUnrealFaceFXNode)

FUnrealFaceFXMaterialParameterNode::FUnrealFaceFXMaterialParameterNode()
{
	_isPlaceable = FxTrue;
	FxFaceGraphNodeUserProperty materialSlotProperty(UPT_Integer);
	materialSlotProperty.SetName("Material Slot Id");
	materialSlotProperty.SetIntegerProperty(0);
	AddUserProperty(materialSlotProperty);
	FxFaceGraphNodeUserProperty materialParameterNameProperty(UPT_String);
	materialParameterNameProperty.SetName("Parameter Name");
	materialParameterNameProperty.SetStringProperty(FxString("Param"));
	AddUserProperty(materialParameterNameProperty);
}

FUnrealFaceFXMaterialParameterNode::~FUnrealFaceFXMaterialParameterNode()
{
}

FxFaceGraphNode* FUnrealFaceFXMaterialParameterNode::Clone( void )
{
	FUnrealFaceFXMaterialParameterNode* pNode = new FUnrealFaceFXMaterialParameterNode();
	CopyData(pNode);
	return pNode;
}

void FUnrealFaceFXMaterialParameterNode::CopyData( FxFaceGraphNode* pOther )
{
	Super::CopyData(pOther);
}

void FUnrealFaceFXMaterialParameterNode::Serialize( FxArchive& arc )
{
	Super::Serialize(arc);

	arc.SerializeClassVersion("FUnrealFaceFXMaterialParameterNode");
}
