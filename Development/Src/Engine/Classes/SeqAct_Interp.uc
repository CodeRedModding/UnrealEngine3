class SeqAct_Interp extends SeqAct_Latent
	native(Sequence);

cpptext
{
	// UObject interface
	virtual void Serialize( FArchive& Ar );

	// USequenceAction interface

	virtual void Activated();
	virtual UBOOL UpdateOp(FLOAT deltaTime);
	virtual void DeActivated();
	virtual void OnCreated();

	// USeqAct_Interp interface

	/** 
	 * Begin playback of this sequence. Only called in game. 
	 * Will then advance Position by (PlayRate * Deltatime) each time the SeqAct_Interp is ticked.
	 */
	void Play();

	/** Stop playback at current position. */
	void Stop();

	/** Similar to play, but the playback will go backwards until the beginning of the sequence is reached. */
	void Reverse();

	/** Increment track forwards by given timestep and iterate over each track updating any properties. */
	void StepInterp(FLOAT DeltaTime, UBOOL bPreview=false);

	/** Move interpolation to new position and iterate over each track updating any properties. */
	void UpdateInterp(FLOAT NewPosition, UBOOL bPreview=false, UBOOL bJump=false);

	/** For each InterGroup/Actor combination, create a InterpGroupInst, assign Actor and initialise each track. */
	void InitInterp();

	/** Destroy all InterpGroupInst. */
	void TermInterp();

	/** See if there is an instance referring to the supplied Actor. Returns NULL if not. */
	class UInterpGroupInst* FindGroupInst(AActor* Actor);

	/** Find the first group instance based on the given InterpGroup. */
	class UInterpGroupInst* FindFirstGroupInst(class UInterpGroup* InGroup);

	/** Find the first group instance based on the InterpGroup with the given name. */
	class UInterpGroupInst* FindFirstGroupInstByName(FName InGroupName);

	/** Find the InterpData connected to the first Variable connector. Returns NULL if none attached. */
	class UInterpData* FindInterpDataFromVariable();

	/** Synchronise the variable connectors with the currently attached InterpData. */
	void UpdateConnectorsFromData();

	/** Use any existing DirectorGroup to see which Actor we currently want to view through. */
	class AActor* FindViewedActor();
}

/** Time multiplier for playback. */
var()	float					PlayRate;

/** Time position in sequence - starts at 0.0 */
var		float					Position;		

/** If sequence is currently playing. */
var		bool					bIsPlaying;

/** 
 *	If sequence should pop back to beginning when finished. 
 *	Note, if true, will never get Complete/Aborted events - sequence must be explicitly Stopped. 
 */
var()	bool					bLooping;

/** If true, sequence will rewind itself back to the start each time the Play input is activated. */
var()	bool					bRewindOnPlay;

/** 
 *	Only used if bRewindOnPlay if true. Defines what should happen if the Play input is activated while currently playing. 
 *	If true, hitting Play while currently playing will pop the position back to the start and begin playback over again.
 *	If false, hitting Play while currently playing will do nothing.
 */
var()	bool					bRewindIfAlreadyPlaying;

/** If sequence playback should be reversed. */
var		bool					bReversePlayback;

/** Whether this action should be initialised and moved to the 'path building time' when building paths. */
var()	bool					bInterpForPathBuilding;

/** Actual track data. Can be shared between SeqAct_Interps. */
var		export InterpData		InterpData;

/** Instance data for interp groups. One for each variable/group combination. */
var		array<InterpGroupInst>	GroupInst;


defaultproperties
{
	ObjName="Matinee"

	PlayRate=1.0

	InputLinks(0)=(LinkDesc="Play")
	InputLinks(1)=(LinkDesc="Reverse")
	InputLinks(2)=(LinkDesc="Stop")

	OutputLinks(0)=(LinkDesc="Completed")
	OutputLinks(1)=(LinkDesc="Aborted")

	VariableLinks(0)=(ExpectedType=class'InterpData',LinkDesc="Data",MinVars=1,MaxVars=1)
}