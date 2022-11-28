class SoundNodeRandom extends SoundNode
	native(Sound)
	collapsecategories
	hidecategories(Object)
	editinlinenew;


var()				array<float>		Weights;

cpptext
{
	// USoundNode interface.
	
	virtual void GetNodes( class UAudioComponent* AudioComponent, TArray<USoundNode*>& SoundNodes );
	virtual void ParseNodes( class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );

	virtual INT GetMaxChildNodes() { return -1; }
	
	// Editor interface.
	
	virtual void InsertChildNode( INT Index );
	virtual void RemoveChildNode( INT Index );
}

defaultproperties
{
}