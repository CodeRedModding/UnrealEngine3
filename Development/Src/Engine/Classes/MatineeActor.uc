// Actor used by matinee (SeqAct_Interp) objects to replicate activation, playback, and other relevant flags to net clients
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
class MatineeActor extends Actor
	native
	nativereplication;

cpptext
{
	virtual FLOAT GetNetPriority(const FVector& ViewPos, const FVector& ViewDir, APlayerController* Viewer, UActorChannel* InChannel, FLOAT Time, UBOOL bLowBandwidth);
	virtual INT* GetOptimizedRepList(BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel);
	virtual void TickSpecial(FLOAT DeltaTime);
	virtual void PreNetReceive();
	virtual void PostNetReceive();
	// returns group index
	INT GetGroupActor(FName GroupName);
	UBOOL ClientInitializeAIGroupActors();
}

/** the SeqAct_Interp associated with this actor (this is set in C++ by the action that spawns this actor)
 *	on the client, the MatineeActor will tick this SeqAct_Interp and notify the actors it should be affecting
 */
var const SeqAct_Interp InterpAction;
/** properties that may change on InterpAction that we need to notify clients about, since the object's properties will not be replicated */
var bool bIsPlaying, bReversePlayback, bPaused;
var float PlayRate;
var float Position;

/** right now Max AIGroup per matinee is 10 **/
const MAX_AIGROUP_NUMBER = 10;

/** to replicate AIGroup actors - for now up to 10 it can replicate**/
var name AIGroupNames[MAX_AIGROUP_NUMBER];
var Pawn AIGroupPawns[MAX_AIGROUP_NUMBER];

/** This is used by client to indicate if AI group needs initialization or not. Client needs to wait until pawns are spawned **/
/** This flag is used by multiple purpose. At the end of initializtion, it will be set.
 *  0: not initialized
 *  1: called InitGroup
 *  2: set init interpolation or physics stuff == DONE **/
var transient int AIGroupInitStage[MAX_AIGROUP_NUMBER];
/** This is just optimization flag to skip checking it again. If all is initialized, it will set this to be TRUE **/
var transient bool AllAIGroupsInitialized;

/** How much error is tolerated in the client-side position before the position that the server replicated is applied */
var float ClientSidePositionErrorTolerance;

/** Add AI group actors to this actor **/
native function  AddAIGroupActor(InterpGroupInstAI AIGroupInst);

replication
{
	if (bNetInitial && Role == ROLE_Authority)
		InterpAction;

	if (bNetDirty && Role == ROLE_Authority)
		bIsPlaying, bReversePlayback, bPaused, PlayRate, Position, AIGroupNames, AIGroupPawns;
}

/** called by InterpAction when significant changes occur. Updates replicated data. */
event Update()
{
	local InterpGroupInstAI AIGroupInst;
	local int   GroupID;

	bIsPlaying = InterpAction.bIsPlaying;
	bReversePlayback = InterpAction.bReversePlayback;
	bPaused = InterpAction.bPaused;
	PlayRate = InterpAction.PlayRate;
	Position = InterpAction.Position;
	bForceNetUpdate = TRUE;

	if (bIsPlaying)
	{
		SetTimer(1.0, true, nameof(CheckPriorityRefresh));
	}
	else
	{
		ClearTimer(nameof(CheckPriorityRefresh));
	}

	// Replicate all AIGroup actors
	if (InterpAction != none)
	{
		for (GroupID = 0; GroupID < InterpAction.GroupInst.Length; ++GroupID)
		{
			AIGroupInst = InterpGroupInstAI(InterpAction.GroupInst[GroupID]);
			if ( AIGroupInst!=none )
			{
				AddAIGroupActor(AIGroupInst);
			}
		}
	}
}

/** check if we should perform a network positional update of this matinee
 * to make sure it's in sync even if it hasn't had significant changes
 * because it's really important (e.g. a player is standing on it or being controlled by it)
 */
function CheckPriorityRefresh()
{
	local Controller C;
	local int i;

	if( InterpAction != None )
	{
		// check if it has a director group - if so, it's controlling the camera, so it's important
		for (i = 0; i < InterpAction.GroupInst.length; i++)
		{
			if (InterpGroupInstDirector(InterpAction.GroupInst[i]) != None)
			{
				bNetDirty = true;
				bForceNetUpdate = true;
				return;
			}
		}

		// check if it is controlling a player Pawn, or a platform a player Pawn is standing on
		foreach WorldInfo.AllControllers(class'Controller', C)
		{
			if ( C.bIsPlayer && C.Pawn != None &&
				( InterpAction.LatentActors.Find(C.Pawn) != INDEX_NONE ||
					(C.Pawn.Base != None && InterpAction.LatentActors.Find(C.Pawn.Base) != INDEX_NONE) ) )
			{
				bNetDirty = true;
				bForceNetUpdate = true;
				return;
			}
		}
	}
}

defaultproperties
{
	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.S_Actor'
		HiddenGame=True
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Matinee"
	End Object
	Components.Add(Sprite)

	bSkipActorPropertyReplication=true
	bAlwaysRelevant=true
	bReplicateMovement=false
	bUpdateSimulatedPosition=false
	bOnlyDirtyReplication=true
	RemoteRole=ROLE_SimulatedProxy
	NetPriority=2.7
	NetUpdateFrequency=1.0
	Position=-1.0
	PlayRate=1.0
	ClientSidePositionErrorTolerance=0.1
}
