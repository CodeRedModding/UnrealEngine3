class SeqAct_ActorFactory extends SeqAct_Latent
	native(Sequence);

cpptext
{
	void Activated();
	UBOOL UpdateOp(FLOAT deltaTime);
	void DeActivated();
};

enum EPointSelection
{
	PS_Normal,
	PS_Random,
};

// type of factory to use when creating the actor
var() export editinline ActorFactory Factory;

// method of point selection for choosing a spawn
// point
var() EPointSelection				PointSelection;

// number of actors to create
var() int							SpawnCount;

// delay applied after creating an actor, before 
// creating the next one only used if SpawnCount > 1
var() float							SpawnDelay;

// number of actors already spawned
var transient int					SpawnedCount;

// amount of time before we can spawn another
var transient float					RemainingDelay;

defaultproperties
{
	ObjName="Actor Factory"

	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="Spawn Point")
	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Spawned",MinVars=0,bWriteable=true)

	SpawnCount=1
	SpawnDelay=0.5f
}
