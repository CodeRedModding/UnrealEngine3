/**
 * This is a basic action that supports latent execution on actors.  It will
 * remain active by default until all targeted actors have either finished the
 * latent behavior, or have been destroyed.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_Latent extends SequenceAction
	abstract
	native(Sequence);

cpptext
{
	virtual void PreActorHandle(AActor *inActor);
	virtual UBOOL UpdateOp(FLOAT deltaTime);
	virtual void Activated();
	virtual void DeActivated();
};

/** List of all actors currently performing this op */
var array<Actor> LatentActors;

/** Indicates whether or not this latent action has been aborted */
var bool bAborted;

/** Keep track of Activation Time in Latent Actions so that we don't activate them twice in the same frame */
var float LatentActivationTime;

/**
 * Allows an actor to abort this current latent action, forcing
 * the Aborted output link to be activated instead of the default
 * one on normal completion.
 *
 * @param	latentActor - actor aborting the latent action
 */
native function AbortFor(Actor latentActor);

/** script tick interface
 * the action deactivates when this function returns false and LatentActors is empty
 * @return whether the action needs to keep ticking
 */
event bool Update(float DeltaTime);

defaultproperties
{
	ObjName="Undefined Latent"
	ObjColor=(R=128,G=128,B=0,A=255)
	OutputLinks(0)=(LinkDesc="Finished")
	OutputLinks(1)=(LinkDesc="Aborted")

	bLatentExecution=TRUE
	LatentActivationTime=0.0
}
