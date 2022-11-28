class SoundNode extends Object
	native
	abstract
	collapsecategories
	hidecategories(Object)
	editinlinenew;

var array<SoundNode>	ChildNodes;

cpptext
{
	// USoundNode interface.
	
	virtual UBOOL Finished( class UAudioComponent* AudioComponent ) { return TRUE; }
	/**
	 * Returns whether this sound node will potentially loop leaf nodes.
	 *
	 * @return FALSE
	 */
	virtual UBOOL IsPotentiallyLooping() { return FALSE; }

	virtual void ParseNodes( class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );
	virtual void GetNodes( class UAudioComponent* AudioComponent, TArray<USoundNode*>& SoundNodes );
	virtual void GetAllNodes( TArray<USoundNode*>& SoundNodes ); // Like above but returns ALL (not just active) nodes.

	virtual INT GetMaxChildNodes() { return 1; }

	// Tool drawing
	virtual void DrawSoundNode(FRenderInterface* RI, const struct FSoundNodeEditorData& EdData, UBOOL bSelected);
	virtual FIntPoint GetConnectionLocation(FRenderInterface* RI, INT ConnType, INT ConnIndex, const struct FSoundNodeEditorData& EdData);

	// Editor interface.
	virtual void InsertChildNode( INT Index );
	virtual void RemoveChildNode( INT Index );
}