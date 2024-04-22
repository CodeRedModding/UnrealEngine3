//=============================================================================
// DynamicPylon
//
// Represents a navigation mesh which is based on a moveable actor
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class DynamicPylon extends Pylon
	placeable
	native(inherit);

// indicates this Pylon is moving (and thus not connected to the rest of the mesh)
var bool bMoving;

function PostBeginPlay()
{
	super.PostBeginPlay();
	RebuildDynamicEdges();
}
cpptext
{
   /**
	 * indicates whether static cross-pylon edges should be built for this pylon (pylons that move should return false)
	 */
	virtual UBOOL NeedsStaticCrossPylonEdgesBuilt(){ return FALSE; } 

	
	/**
	 * Called from UpdateComponentsInternal when a transform update is needed (when this pylon has moved)
	 */
	virtual void PylonMoved();

	virtual void PreBeginPlay();

	virtual void FindBase();

	/** 
	 *  is this pylon compatible with dynamic obstacles? (e.g. moving meshes aren't)
	 */
	virtual UBOOL CompatibleWithDynamicObstacles(){return FALSE;}

	void ApplyDynamicSnap();

#if WITH_EDITOR
	virtual void CheckForErrors();
#endif
};

/**
 *  will wipe all dynamic edges for this pylon, and rebuild them from the currently position (use this sparingly, it's not cheap)
 *  good time to call this is after the pylon is finished moving, or comes to rest
 */
native function RebuildDynamicEdges();

/**
 * will remove all dynamic edges associated with this pylon
 */
native function FlushDynamicEdges();

event StartedMoving()
{
	`log(self@GetFuncName()@"-----------");
	bMoving=true;
	FlushDynamicEdges();
}

event StoppedMoving()
{
	`log(self@GetFuncName()@"-----------");
	bMoving=false;
	RebuildDynamicEdges();
}

defaultproperties
{
	Begin Object Name=Sprite
		Sprite=Texture2D'EditorResources.DynamicPylon'
	End Object

	bStatic=FALSE
}
