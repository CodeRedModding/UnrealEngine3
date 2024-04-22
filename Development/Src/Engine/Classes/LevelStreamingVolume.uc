/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Used to affect level streaming in the game and level visibility in the editor.
 */
class LevelStreamingVolume extends Volume
	native
	hidecategories(Advanced,Attachment,Collision,Volume)
	placeable;

struct CheckpointRecord
{
	var bool bDisabled;
};

/** Levels affected by this level streaming volume. */
var() noimport const editconst array<LevelStreaming> StreamingLevels;

/** If TRUE, this streaming volume should only be used for editor streaming level previs. */
var() bool						bEditorPreVisOnly;

/**
 * If TRUE, this streaming volume is ignored by the streaming volume code.  Used to either
 * disable a level streaming volume without disassociating it from the level, or to toggle
 * the control of a level's streaming between Kismet and volume streaming.
 */
var() bool						bDisabled;

/** Enum for different usage cases of level streaming volumes. */
enum EStreamingVolumeUsage
{
	SVB_Loading,
	SVB_LoadingAndVisibility,
	SVB_VisibilityBlockingOnLoad,
	SVB_BlockingOnLoad,
	SVB_LoadingNotVisible
};

/** Determines what this volume is used for, e.g. whether to control loading, loading and visibility or just visibilty (blocking on load) */
var() EStreamingVolumeUsage	StreamingUsage;

/** If TRUE, level will stream when closer than TestVolumeDistance to the volume. */
var()	bool	bTestDistanceToVolume;

/** If bTestDistanceToVolume is TRUE, level will stream in if closer than this to volume.  */
var()	float	TestVolumeDistance;



var deprecated EStreamingVolumeUsage	Usage;


/**
 * Kismet support for toggling bDisabled.
 */
simulated function OnToggle(SeqAct_Toggle action)
{
	if (action.InputLinks[0].bHasImpulse)
	{
		// "Turn On" -- mapped to enabling of volume streaming for this volume.
		bDisabled = FALSE;
	}
	else if (action.InputLinks[1].bHasImpulse)
	{
		// "Turn Off" -- mapped to disabling of volume streaming for this volume.
		bDisabled = TRUE;
	}
	else if (action.InputLinks[2].bHasImpulse)
	{
		// "Toggle"
		bDisabled = !bDisabled;
	}
}

function CreateCheckpointRecord(out CheckpointRecord Record)
{
	Record.bDisabled = bDisabled;
}

function ApplyCheckpointRecord(const out CheckpointRecord Record)
{
	bDisabled = Record.bDisabled;
}

cpptext
{
	// UObject interace.
	/**
	 * Serialize function.
	 *
	 * @param	Ar	Archive to serialize with.
	 */
	void Serialize( FArchive& Ar );

	/**
	 * Performs operations after the object is loaded. 
	 * Used for fixing up deprecated fields. 
	 */
	virtual void PostLoad();

	// AActor interface.
	/**
	 * Function that gets called from within Map_Check to allow this actor to check itself
	 * for any potential errors and register them with map check dialog.
	 */
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif
}

defaultproperties
{
	Begin Object Name=BrushComponent0
		CollideActors=False
		BlockActors=False
		BlockZeroExtent=False
		BlockNonZeroExtent=False
		BlockRigidBody=False
	End Object

	bColored=true
	// Orange Brush
	BrushColor=(R=255,G=165,B=0,A=255)

	bCollideActors=False
	bBlockActors=False
	bProjTarget=False
	SupportedEvents.Empty
	SupportedEvents(0)=class'SeqEvent_Touch'
	// streaming volumes are server side - resultant levels to load or not is what is sent to the client
	bForceAllowKismetModification=true
	StreamingUsage=SVB_LoadingAndVisibility
}
