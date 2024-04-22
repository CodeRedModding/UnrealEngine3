/**
 * Base class for static actors which contain StaticMeshComponents.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class StaticMeshActorBase extends Actor
	ClassGroup(StaticMeshes)
	native
	abstract;

cpptext
{
	/**
	 * Initializes this actor when play begins.  This version marks the actor as ready to execute script, but skips
	 * the rest of the stuff that actors normally do in PostBeginPlay().
	 */
	virtual void PostBeginPlay();
}

DefaultProperties
{
	bEdShouldSnap=true
	bStatic=true
	bMovable=false
	bCollideActors=true
	bBlockActors=true
	bWorldGeometry=true
	bGameRelevant=true
	bRouteBeginPlayEvenIfStatic=false
	bCollideWhenPlacing=false
}
