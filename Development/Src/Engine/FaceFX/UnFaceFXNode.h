//------------------------------------------------------------------------------
// Unreal-specific FaceFX Face Graph node type.
//
// Owner: Jamie Redmond
//
// Copyright (c) 2002-2005 OC3 Entertainment, Inc.
//------------------------------------------------------------------------------

#ifndef UnFaceFXNode_H__
#define UnFaceFXNode_H__

#include "../../../External/FaceFX/FxSDK/Inc/FxGenericTargetNode.h"

using OC3Ent::Face::FxBool;
using OC3Ent::Face::FxGenericTargetNode;
using OC3Ent::Face::FxFaceGraphNode;
using OC3Ent::Face::FxFaceGraphNodeUserProperty;
using OC3Ent::Face::FxArchive;

// An Unreal-specific FaceFX Face Graph node.
class FUnrealFaceFXNode : public FxGenericTargetNode
{
	// Declare the class.
	FX_DECLARE_CLASS(FUnrealFaceFXNode, FxGenericTargetNode)
	// Disable copy construction and assignment.
	FX_NO_COPY_OR_ASSIGNMENT(FUnrealFaceFXNode)
public:
	// Constructor.
	FUnrealFaceFXNode();
	// Destructor.
	virtual ~FUnrealFaceFXNode();

	// Clones the node.
	virtual FxFaceGraphNode* Clone( void );

	// Copies the data from this object into the other object.
	virtual void CopyData( FxFaceGraphNode* pOther );

	// Returns FxTrue if the engine should perform a linkup during the next
	// render.
	FxBool ShouldLink( void ) const;
	// Sets whether or not the engine should perform a linkup during the next
	// render.
	void SetShouldLink( FxBool shouldLink );

	// Replaces any user property with the same name as the supplied user 
	// property with the values in the supplied user property.  If the
	// user property does not exist, nothing happens.
	virtual void ReplaceUserProperty( const FxFaceGraphNodeUserProperty& userProperty );

	// Serializes an FUnrealFaceFXNode to an archive.
	virtual void Serialize( FxArchive& arc );

protected:
	// FxTrue if the engine should perform a linkup during the next render.
	FxBool _shouldLink;
};

#endif
