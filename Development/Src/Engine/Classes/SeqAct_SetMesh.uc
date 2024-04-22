/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_SetMesh extends SequenceAction
	native(Sequence);

enum EMeshType
{
	MeshType_StaticMesh,
	MeshType_SkeletalMesh,
};

/** New SkeletalMesh to use for the target actor */
var()	SkeletalMesh	NewSkeletalMesh;
/** New StaticMesh to use for the target actor */
var()	StaticMesh		NewStaticMesh;
/** Type of mesh to set */
var()	EMeshType	MeshType;
/** if True then the mesh will be treated as if it is movable */
var()	bool		bIsAllowedToMove;
/** if True then any decals attached to the previous mesh will be reattached to the new mesh */
var()	bool		bAllowDecalsToReattach;

defaultproperties
{
	ObjName="Set Mesh"
	ObjCategory="Actor"
}
