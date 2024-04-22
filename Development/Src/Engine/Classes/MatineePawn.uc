/**
 * MatineePawn - used only to preview in Matinee
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class MatineePawn extends Pawn
	native(Pawn);

cpptext
{
	// Sets Mesh to PreviewMesh
	void ReplacePreviewMesh(USkeletalMesh * NewPreviewMesh);
};

// Preview Mesh
var() editoronly SkeletalMesh PreviewMesh;

defaultproperties
{
	Begin Object Class=SkeletalMeshComponent Name=PawnMesh
		// to match your in-game character
		// set this to be same
		Translation=(Z=-72)
	End Object
	Mesh=PawnMesh
	Components.Add(PawnMesh)

	Physics = PHYS_Falling

	Begin Object Name=CollisionCylinder
		CollisionRadius=+0030.0000
		CollisionHeight=+0072.000000
	End Object
}
