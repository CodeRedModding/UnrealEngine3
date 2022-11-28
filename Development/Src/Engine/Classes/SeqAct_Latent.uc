class SeqAct_Latent extends SequenceAction
	abstract
	native(Sequence);

cpptext
{
	virtual void PreActorHandle(AActor *inActor);
	virtual UBOOL UpdateOp(FLOAT deltaTime);
	virtual void DeActivated();
};

/** List of all actors currently performing this op */
var transient array<Actor> LatentActors;

/** Indicates whether or not this latent action has been aborted */
var transient bool bAborted;

/**
 * Allows an actor to abort this current latent action, forcing
 * the Aborted output link to be activated instead of the default
 * one on normal completion.
 * 
 * @param	latentActor - actor aborting the latent action
 */
native final function AbortFor(Actor latentActor);

defaultproperties
{
	ObjName="Undefined Latent"
	ObjColor=(R=128,G=128,B=0,A=255)
	OutputLinks(0)=(LinkDesc="Finished")
	OutputLinks(1)=(LinkDesc="Aborted")
}
