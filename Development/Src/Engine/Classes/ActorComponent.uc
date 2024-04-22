/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ActorComponent extends Component
	native
	noexport
	abstract;

var native	transient	const	pointer			Scene{FSceneInterface};
var 		transient	const	Actor			Owner;
var	native	transient	const	bool			bAttached;
var						const 	bool			bTickInEditor;

/** Is this component in need of an update? */
var	transient	const	bool			bNeedsReattach;

/** Is this component's transform in need of an update? */
var	transient	const	bool			bNeedsUpdateTransform;

/** The ticking group this component belongs to */
var const ETickingGroup TickGroup;

/** Changes the ticking group for this component */
native final function SetTickGroup(ETickingGroup NewTickGroup);

/**
 *	Sets whether or not the physics for this object should be 'fixed' (ie kinematic) or allowed to move with dynamics.
 *	If bFixed is true, all bodies within this component will be fixed.
 *	If bFixed is false, bodies will be set back to the default defined by their BodySetup.
 */
native final function SetComponentRBFixed(bool bFixed);

/** force this component to be updated right now
 * component must be directly attached to its Owner (not attached to another component)
 * @param bTransformOnly - if true, only update transform, otherwise do a full reattachment
 */
native final function ForceUpdate(bool bTransformOnly);

/** detaches the component from whatever it's attached to */
native final function DetachFromAny();

defaultproperties
{
	// All things now default to being ticked during async work
	TickGroup=TG_DuringAsyncWork
}
