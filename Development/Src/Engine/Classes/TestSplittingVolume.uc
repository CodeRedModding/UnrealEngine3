/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class TestSplittingVolume extends Volume
	implements(Interface_NavMeshPathObject)
	placeable
	native(AI);


cpptext
{
			/**
	 * this function describes a 'cookie cutter' edge that will be used to split the mesh beneath it
	 * for example if you have a cost inflicting volume that needs to conform the mesh to itself
	 * you could return an array of verts along the bottom boundary of the volume, and a height up to the top of the volume
	 * and then you have poly boundaries exactly along the border of the volume with which you can add 
	 * edges which affect cost
	 * @param Poly - array of vectors that describe bottom of 'cookie cutter' volume (should be CW and convex)
	 * @param PolyHeight - height above the poly within which polys should be split (extrude passed poly up by this amount and use faces for clip)
	 * @return - TRUE if this shape should be used to split the mesh FALSE if no splitting is necessary
	 */
	virtual UBOOL GetMeshSplittingPoly( TArray<FVector>& out_PolyShape, FLOAT& PolyHeight );
};

