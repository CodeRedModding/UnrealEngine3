/**
 * These are useful when you have a space that wasn't explored by a pylon but it safely enclosed in the pylon
 * (e.g. a ledge that a mantle check didn't explore properly but should be able to have jump down edges) but you don't
 * feel it's necessary to add a whole new pylon/navmesh/heavyweight thing it's just an exploration hint for the path generator
 * or mesh generator 
 *
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class PylonSeed extends Actor
	placeable
	native(AI)
	implements(Interface_NavMeshPathObject)
	ClassGroup(Navigation);

cpptext
{
	virtual UBOOL Supports( const FNavMeshPathParams& PathParams,
							FNavMeshPolyBase* CurPoly,
							FNavMeshPathObjectEdge* Edge,
							FNavMeshEdgeBase* PredecessorEdge)
	{
		return FALSE;
	}

	virtual void AddAuxSeedPoints( APylon* Py )
	{
		if (Py != NULL && Py->IsPtWithinExpansionBounds(Location,50.f))
		{
			Py->NextPassSeedList.AddItem(Location);
		}
	}
}

defaultproperties
{
	Begin Object Class=CylinderComponent Name=CollisionCylinder LegacyClassName=NavigationPoint_NavigationPointCylinderComponent_Class
		CollisionRadius=+0050.000000
		CollisionHeight=+0050.000000
	End Object
	CollisionComponent=CollisionCylinder
	Components.Add(CollisionCylinder)

	bCollideActors=FALSE
	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.S_NavP'
		HiddenGame=true
		HiddenEditor=false
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Navigation"
	End Object
	Components.Add(Sprite)
}
