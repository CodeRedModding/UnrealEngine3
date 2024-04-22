/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SkeletalMeshActor extends Actor
	native(Anim)
	ClassGroup(SkeletalMeshes)
	placeable;

var()		bool		bDamageAppliesImpulse;

/** Whether or not this actor should respond to anim notifies - CURRENTLY ONLY AFFECTS PlayParticleEffect NOTIFIES**/
var() bool bShouldDoAnimNotifies;

var()	SkeletalMeshComponent			SkeletalMeshComponent;
var() const editconst LightEnvironmentComponent LightEnvironment;

var		AudioComponent					FacialAudioComp;

/** Used to replicate mesh to clients */
var repnotify transient SkeletalMesh ReplicatedMesh;

/** used to replicate the material in index 0 */
var repnotify MaterialInterface ReplicatedMaterial0, ReplicatedMaterial1;

struct CheckpointRecord
{
	var bool bReplicated;
	var bool bHidden;
	var bool bSavedPosition;
	var vector Location;
	var rotator Rotation;
};

/** @hack: force saving positional data in checkpoint - some uses in Matinee require this */
var() bool bForceSaveInCheckpoint;

/** Struct that stores info to update one skel control with a location target */
struct native SkelMeshActorControlTarget
{
	/** Name of SkelControl to update */
	var()	name	ControlName;
	/** Actor to use for location of skel control target. */
	var()	actor	TargetActor;
};

/** Set of skel controls to update targets of, based on Actor location */
var() array<SkelMeshActorControlTarget>		ControlTargets;

/** mirror of bCollideActors used for backwards compatibility with change of default bCollideActors to FALSE
 * we save the value in previous package versions into this property instead and copy back
 * thus preserving the value of the property in old content regardless of whether they modified it
 */
var deprecated bool bCollideActors_OldValue;

/** List of Matinee InterpGroup controlling this actor. */
var transient Array<InterpGroup>	InterpGroupList;

/** This is only editor only, when exiting Matinee, it should preserve previous position **/
var transient private name       SavedAnimSeqName;
var transient private float      SavedCurrentTime;

/** If TRUE, attachments are set to use this actors mesh as a shadow parent and share this actors light environment */
var() bool bShouldShadowParentAllAttachedActors;

cpptext
{
	// UObject interface
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif
	//@compatibility
	virtual void PostLoad();

	// AActor interface
	virtual void TickSpecial(FLOAT DeltaSeconds);
	virtual void ForceUpdateComponents(UBOOL bCollisionUpdate,UBOOL bTransformOnly);
	virtual void PreviewBeginAnimControl(class UInterpGroup* InInterpGroup);
	virtual void PreviewSetAnimPosition(FName SlotName, INT ChannelIndex, FName InAnimSeqName, FLOAT InPosition, UBOOL bLooping, UBOOL bFireNotifies, UBOOL bEnableRootMotion, FLOAT DeltaTime);
	virtual void PreviewSetAnimWeights(TArray<FAnimSlotInfo>& SlotInfos);
	virtual void PreviewFinishAnimControl(class UInterpGroup* InInterpGroup);
	virtual void PreviewUpdateFaceFX(UBOOL bForceAnim, const FString& GroupName, const FString& SeqName, FLOAT InPosition);
	virtual void PreviewActorPlayFaceFX(const FString& GroupName, const FString& SeqName, USoundCue* InSoundCue);
	virtual void PreviewActorStopFaceFX();
	virtual UAudioComponent* PreviewGetFaceFXAudioComponent();
	virtual class UFaceFXAsset* PreviewGetActorFaceFXAsset();

	/** Called each from while the Matinee action is running, to set the animation weights for the actor. */
	virtual void SetAnimWeights( const TArray<struct FAnimSlotInfo>& SlotInfos );

	/** Build AnimSet list, called by UpdateAnimSetList() */
	void BuildAnimSetList();
	/** Add a given list of anim sets on the top of the list (so they override the other ones */
	void AddAnimSets(const TArray<class UAnimSet*>& CustomAnimSets);
	/** Restore Mesh's AnimSets to defaults, as defined in the default properties */
	void RestoreAnimSetsToDefault();
	/** Save Mesh's defaults to AnimSets, back up*/
	void SaveDefaultsToAnimSets();

protected:
#if USE_GAMEPLAY_PROFILER
    /** 
     * This function actually does the work for the GetProfilerAssetObject and is virtual.  
     * It should only be called from GetProfilerAssetObject as GetProfilerAssetObject is safe to call on NULL object pointers
     */
	virtual UObject* GetProfilerAssetObjectInternal() const;
#endif
	/**
     * This function actually does the work for the GetDetailInfo and is virtual.
     * It should only be called from GetDetailedInfo as GetDetailedInfo is safe to call on NULL object pointers
     */
	virtual FString GetDetailedInfoInternal() const;
}


replication
{
	if (Role == ROLE_Authority)
		ReplicatedMesh, ReplicatedMaterial0, ReplicatedMaterial1;
}

simulated event PostBeginPlay()
{
	// grab the current mesh for replication
	if (Role == ROLE_Authority && SkeletalMeshComponent != None)
	{
		ReplicatedMesh = SkeletalMeshComponent.SkeletalMesh;
	}

	if (bShouldShadowParentAllAttachedActors)
	{
		SetShadowParentOnAllAttachedComponents(SkeletalMeshComponent, LightEnvironment);
	}

	// Unfix bodies flagged as 'full anim weight'
	if( SkeletalMeshComponent != None &&
		//SkeletalMeshComponent.bEnableFullAnimWeightBodies &&
		SkeletalMeshComponent.PhysicsAssetInstance != None )
	{
		SkeletalMeshComponent.PhysicsAssetInstance.SetFullAnimWeightBonesFixed(FALSE, SkeletalMeshComponent);
	}

	if( bHidden )
	{
		SkeletalMeshComponent.SetClothFrozen(TRUE);
	}
}

simulated event Destroyed()
{
	Super.Destroyed();

	// Empty list of Matinee that control us
	InterpGroupList.Length = 0;
	// Free up animsets.
	UpdateAnimSetList();
}

/** Update list of AnimSets for this Pawn */
native simulated function UpdateAnimSetList();

simulated event ReplicatedEvent( name VarName )
{
	if (VarName == 'ReplicatedMesh')
	{
		SkeletalMeshComponent.SetSkeletalMesh(ReplicatedMesh);
	}
	else if (VarName == 'ReplicatedMaterial0')
	{
		SkeletalMeshComponent.SetMaterial(0, ReplicatedMaterial0);
	}
	else if (VarName == 'ReplicatedMaterial1')
	{
		SkeletalMeshComponent.SetMaterial(1, ReplicatedMaterial1);
	}
	else
	{
		Super.ReplicatedEvent(VarName);
	}
}

/** Handling Toggle event from Kismet. */
simulated function OnToggle(SeqAct_Toggle action)
{
	local AnimNodeSequence SeqNode;

	SeqNode = AnimNodeSequence(SkeletalMeshComponent.Animations);

	if( SeqNode != None )
	{
		// Turn ON
		if (action.InputLinks[0].bHasImpulse)
		{
			// If animation is not playing - start playing it now.
			if(!SeqNode.bPlaying)
			{
				// This starts the animation playing from the beginning. Do we always want that?
				SeqNode.PlayAnim(SeqNode.bLooping, SeqNode.Rate, 0.0);
			}
		}
		// Turn OFF
		else if (action.InputLinks[1].bHasImpulse)
		{
			// If animation is playing, stop it now.
			if(SeqNode.bPlaying)
			{
				SeqNode.StopAnim();
			}
		}
		// Toggle
		else if (action.InputLinks[2].bHasImpulse)
		{
			// Toggle current animation state.
			if(SeqNode.bPlaying)
			{
				SeqNode.StopAnim();
			}
			else
			{
				SeqNode.PlayAnim(SeqNode.bLooping, SeqNode.Rate, 0.0);
			}
		}
	}
}

function OnSetMaterial(SeqAct_SetMaterial Action)
{
	SkeletalMeshComponent.SetMaterial( Action.MaterialIndex, Action.NewMaterial );
	if (Action.MaterialIndex == 0)
	{
		ReplicatedMaterial0 = Action.NewMaterial;
		ForceNetRelevant();
	}
	if (Action.MaterialIndex == 1)
	{
		ReplicatedMaterial1 = Action.NewMaterial;
		ForceNetRelevant();
	}
}

simulated event BeginAnimControl(InterpGroup InInterpGroup)
{
	MAT_BeginAnimControl(InInterpGroup);
}
/** Start AnimControl. Add required AnimSets. */
native function MAT_BeginAnimControl(InterpGroup InInterpGroup);

simulated event SetAnimPosition(name SlotName, int ChannelIndex, name InAnimSeqName, float InPosition, bool bFireNotifies, bool bLooping, bool bEnableRootMotion)
{
	local AnimNodeSequence	SeqNode;

	// Ensure anims are updated correctly in cinematics even when the mesh isn't rendered.
	SkeletalMeshComponent.LastRenderTime = WorldInfo.TimeSeconds;

	SeqNode = AnimNodeSequence(SkeletalMeshComponent.Animations);
	if( SeqNode != None )
	{
		if( SeqNode.AnimSeqName != InAnimSeqName || SeqNode.AnimSeq == None )
		{
			SeqNode.SetAnim(InAnimSeqName);
		}

		SeqNode.bLooping = bLooping;
		SeqNode.SetPosition(InPosition, bFireNotifies);
	}
}

/** Called when we are done with the AnimControl track. */
simulated event FinishAnimControl(InterpGroup InInterpGroup)
{
	MAT_FinishAnimControl(InInterpGroup);
}
/** End AnimControl. Release required AnimSets */
native function MAT_FinishAnimControl(InterpGroup InInterpGroup);

/** Handler for Matinee wanting to play FaceFX animations in the game. */
simulated event bool PlayActorFaceFXAnim(FaceFXAnimSet AnimSet, String GroupName, String SeqName, SoundCue SoundCueToPlay )
{
	return SkeletalMeshComponent.PlayFaceFXAnim(AnimSet, SeqName, GroupName, SoundCueToPlay);
}

/** Handler for Matinee wanting to stop FaceFX animations in the game. */
simulated event StopActorFaceFXAnim()
{
	SkeletalMeshComponent.StopFaceFXAnim();
}

/** Used to let FaceFX know what component to play dialogue audio on. */
simulated event AudioComponent GetFaceFXAudioComponent()
{
	return FacialAudioComp;
}

/** Function for handling the SeqAct_PlayFaceFXAnim Kismet action working on this Actor. */
simulated function OnPlayFaceFXAnim(SeqAct_PlayFaceFXAnim inAction)
{
	local PlayerController PC;

	SkeletalMeshComponent.PlayFaceFXAnim(inAction.FaceFXAnimSetRef, inAction.FaceFXAnimName, inAction.FaceFXGroupName, inAction.SoundCueToPlay);

	// tell non-local players to play as well
	foreach WorldInfo.AllControllers(class'PlayerController', PC)
	{
		if (NetConnection(PC.Player) != None)
		{
			PC.ClientPlayActorFaceFXAnim(self, inAction.FaceFXAnimSetRef, inAction.FaceFXGroupName, inAction.FaceFXAnimName, inAction.SoundCueToPlay);
		}
	}
}

/** Used by Matinee in-game to mount FaceFXAnimSets before playing animations. */
simulated event FaceFXAsset GetActorFaceFXAsset()
{
	if(SkeletalMeshComponent.SkeletalMesh != None && !SkeletalMeshComponent.bDisableFaceFX)
	{
		return SkeletalMeshComponent.SkeletalMesh.FaceFXAsset;
	}
	else
	{
		return None;
	}
}

/**
 * Returns TRUE if this actor is playing a FaceFX anim.
 */
simulated function bool IsActorPlayingFaceFXAnim()
{
	return (SkeletalMeshComponent != None && SkeletalMeshComponent.IsPlayingFaceFXAnim());
}

event OnSetMesh(SeqAct_SetMesh Action)
{
	if (Action.MeshType == MeshType_SkeletalMesh)
	{
		if (Action.NewSkeletalMesh != None && Action.NewSkeletalMesh != SkeletalMeshComponent.SkeletalMesh)
		{
			SkeletalMeshComponent.SetSkeletalMesh(Action.NewSkeletalMesh);
			ReplicatedMesh = Action.NewSkeletalMesh;
		}
	}
}

/** Handle action that forces bodies to sync to their animated location */
simulated event OnUpdatePhysBonesFromAnim(SeqAct_UpdatePhysBonesFromAnim Action)
{
	if (action.InputLinks[0].bHasImpulse)
	{
		SkeletalMeshComponent.ForceSkelUpdate();
		SkeletalMeshComponent.UpdateRBBonesFromSpaceBases(TRUE, TRUE);

		SkeletalMeshComponent.ForceApexClothingTeleportAndReset();
	}
	else if(action.InputLinks[1].bHasImpulse)
	{
		// Fix bodies flagged as 'full anim weight'
		if( SkeletalMeshComponent.PhysicsAssetInstance != None )
		{
			SkeletalMeshComponent.PhysicsAssetInstance.SetAllBodiesFixed(TRUE);
		}
	}
	else if(action.InputLinks[2].bHasImpulse)
	{
		// Unfix bodies flagged as 'full anim weight'
		if( SkeletalMeshComponent.PhysicsAssetInstance != None )
		{
			SkeletalMeshComponent.PhysicsAssetInstance.SetFullAnimWeightBonesFixed(FALSE, SkeletalMeshComponent);
		}
	}
}

/** Handle action to set skel control target from kismet. */
simulated event OnSetSkelControlTarget(SeqAct_SetSkelControlTarget Action)
{
	local int i;

	// Check we have the info we need
	if(Action.SkelControlName == '' || Action.TargetActors.length == 0)
	{
		return;
	}

	// First see if we have an entry for this control
	for(i=0; i<ControlTargets.length; i++)
	{
		// See if name matches
		if(ControlTargets[i].ControlName == Action.SkelControlName)
		{
			// It does - just update target actor
			ControlTargets[i].TargetActor = Actor(Action.TargetActors[Rand(Action.TargetActors.length)]);
			return;
		}
	}

	// Did not find an existing entry - make a new one
	ControlTargets.length = ControlTargets.length + 1;
	ControlTargets[ControlTargets.length-1].ControlName = Action.SkelControlName;
	ControlTargets[ControlTargets.length-1].TargetActor = Actor(Action.TargetActors[Rand(Action.TargetActors.length)]);
}

/** Performs actual attachment. Can be subclassed for class specific behaviors. */
function DoKismetAttachment(Actor Attachment, SeqAct_AttachToActor Action)
{
	local bool	bOldCollideActors, bOldBlockActors, bValidBone, bValidSocket;

	// If a bone/socket has been specified, see if it is valid
	if( SkeletalMeshComponent != None && Action.BoneName != '' )
	{
		// See if the bone name refers to an existing socket on the skeletal mesh.
		bValidSocket	= (SkeletalMeshComponent.GetSocketByName(Action.BoneName) != None);
		bValidBone		= (SkeletalMeshComponent.MatchRefBone(Action.BoneName) != INDEX_NONE);

		// Issue a warning if we were expecting to attach to a bone/socket, but it could not be found.
		if( !bValidBone && !bValidSocket )
		{
			`log(WorldInfo.TimeSeconds @ class @ GetFuncName() @ "bone or socket" @ Action.BoneName @ "not found on actor" @ Self @ "with mesh" @ SkeletalMeshComponent);
		}
	}

	// Special case for handling relative location/rotation w/ bone or socket
	if( bValidBone || bValidSocket )
	{
		// disable collision, so we can successfully move the attachment
		bOldCollideActors	= Attachment.bCollideActors;
		bOldBlockActors		= Attachment.bBlockActors;
		Attachment.SetCollision(FALSE, FALSE);
		Attachment.SetHardAttach(Action.bHardAttach);

		// Sockets by default move the actor to the socket location.
		// This is not the case for bones!
		// So if we use relative offsets, then first move attachment to bone's location.
		if( bValidBone && !bValidSocket )
		{
			if( Action.bUseRelativeOffset )
			{
				Attachment.SetLocation(SkeletalMeshComponent.GetBoneLocation(Action.BoneName));
			}

			if( Action.bUseRelativeRotation )
			{
				Attachment.SetRotation(QuatToRotator(SkeletalMeshComponent.GetBoneQuaternion(Action.BoneName)));
			}
		}

		// Attach attachment to base.
		Attachment.SetBase(Self,, SkeletalMeshComponent, Action.BoneName);

		if( Action.bUseRelativeRotation )
		{
			Attachment.SetRelativeRotation(Attachment.RelativeRotation + Action.RelativeRotation);
		}

		// if we're using the offset, place attachment relatively to the target
		if( Action.bUseRelativeOffset )
		{
			Attachment.SetRelativeLocation(Attachment.RelativeLocation + Action.RelativeOffset);
		}

		// restore previous collision
		Attachment.SetCollision(bOldCollideActors, bOldBlockActors);
	}
	else
	{
		// otherwise base on location
		Super.DoKismetAttachment(Attachment, Action);
	}
}

/**
* Default behaviour when shot is to apply an impulse and kick the KActor.
*/
event TakeDamage(int Damage, Controller EventInstigator, vector HitLocation, vector Momentum, class<DamageType> DamageType, optional TraceHitInfo HitInfo, optional Actor DamageCauser)
{
	local vector ApplyImpulse;

	// call Actor's version to handle any SeqEvent_TakeDamage for scripting
	Super.TakeDamage(Damage, EventInstigator, HitLocation, Momentum, DamageType, HitInfo, DamageCauser);

	if ( bDamageAppliesImpulse && damageType.default.KDamageImpulse > 0 )
	{
		if ( VSize(momentum) < 0.001 )
		{
			`Log("Zero momentum to SkeletalMeshActor.TakeDamage");
			return;
		}

		ApplyImpulse = Normal(momentum) * damageType.default.KDamageImpulse;
		if ( HitInfo.HitComponent != None )
		{
			HitInfo.HitComponent.AddImpulse(ApplyImpulse, HitLocation, HitInfo.BoneName);
		}
	}
}

// checkpointing
function bool ShouldSaveForCheckpoint()
{
	return (RemoteRole != ROLE_None || bForceSaveInCheckpoint || IsInPersistentLevel(true));
}
function CreateCheckpointRecord(out CheckpointRecord Record)
{
	Record.bReplicated = (RemoteRole != ROLE_None);
	Record.bHidden = bHidden;
	if (bForceSaveInCheckpoint || IsInPersistentLevel(true))
	{
		Record.bSavedPosition = true;
		Record.Location = Location;
		Record.Rotation = Rotation;
	}
}
function ApplyCheckpointRecord(const out CheckpointRecord Record)
{
	SetHidden(Record.bHidden);
	if (Record.bSavedPosition)
	{
		SetLocation(Record.Location);
		SetRotation(Record.Rotation);
	}
	if (Record.bReplicated)
	{
		ForceNetRelevant();
		if (RemoteRole != ROLE_None)
		{
			SetForcedInitialReplicatedProperty(Property'Engine.Actor.bHidden', (bHidden == default.bHidden));
		}
	}
}

/**
 * Called by AnimNotify_PlayParticleEffect
 * Looks for a socket name first then bone name
 *
 * @param AnimNotifyData The AnimNotify_PlayParticleEffect which will have all of the various params on it
 *
 *	@return	bool		true if the particle effect was played, false if not;
 */
event bool PlayParticleEffect( const AnimNotify_PlayParticleEffect AnimNotifyData )
{
	local vector Loc;
	local rotator Rot;
	local ParticleSystemComponent PSC;
	local bool bPlayNonExtreme;

	if (WorldInfo.NetMode == NM_DedicatedServer)
	{
		`Log("(SkeletalMeshActor): PlayParticleEffect on dedicated server!");
		return true;
	}

	// should I play non extreme content?
	bPlayNonExtreme = ( AnimNotifyData.bIsExtremeContent == TRUE ) && ( WorldInfo.GRI.ShouldShowGore() == FALSE ) ;

	// if we should not respond to anim notifies OR if this is extreme content and we can't show extreme content then return
	if( ( bShouldDoAnimNotifies == FALSE )
		// if playing non extreme but no data is set, just return
		|| ( bPlayNonExtreme && AnimNotifyData.PSNonExtremeContentTemplate==None )
		)
	{
		// Return TRUE to prevent the SkelMeshComponent from playing it as well!
		return true;
	}

	// now go ahead and spawn the particle system based on whether we need to attach it or not
	if( AnimNotifyData.bAttach == TRUE )
	{
		PSC = new(self) class'ParticleSystemComponent';  // move this to the object pool once it can support attached to bone/socket and relative translation/rotation
		if ( bPlayNonExtreme )
		{
			PSC.SetTemplate( AnimNotifyData.PSNonExtremeContentTemplate );
		}
		else
		{
			PSC.SetTemplate( AnimNotifyData.PSTemplate );
		}

		if( AnimNotifyData.SocketName != '' )
		{
			//`log( "attaching AnimNotifyData.SocketName" );
			SkeletalMeshComponent.AttachComponentToSocket( PSC, AnimNotifyData.SocketName );
		}
		else if( AnimNotifyData.BoneName != '' )
		{
			//`log( "attaching AnimNotifyData.BoneName" );
			SkeletalMeshComponent.AttachComponent( PSC, AnimNotifyData.BoneName );
		}

		PSC.ActivateSystem();
		PSC.OnSystemFinished = SkelMeshActorOnParticleSystemFinished;
	}
	else
	{
		// find the location
		if( AnimNotifyData.SocketName != '' )
		{
			SkeletalMeshComponent.GetSocketWorldLocationAndRotation( AnimNotifyData.SocketName, Loc, Rot );
		}
		else if( AnimNotifyData.BoneName != '' )
		{
			Loc = SkeletalMeshComponent.GetBoneLocation( AnimNotifyData.BoneName );
			Rot = QuatToRotator(SkeletalMeshComponent.GetBoneQuaternion(AnimNotifyData.BoneName));
		}
		else
		{
			Loc = Location;
			Rot = rot(0,0,1);
		}

		PSC = WorldInfo.MyEmitterPool.SpawnEmitter( AnimNotifyData.PSTemplate, Loc,  Rot);
	}

	if( PSC != None && AnimNotifyData.BoneSocketModuleActorName != '' )
	{
		PSC.SetActorParameter(AnimNotifyData.BoneSocketModuleActorName, self);
	}

	return true;
}

/** We so we detach the Component once we are done playing it **/
simulated function SkelMeshActorOnParticleSystemFinished( ParticleSystemComponent PSC )
{
	SkeletalMeshComponent.DetachComponent( PSC );
}

/**
 * Called by AnimNotify_ForceField
 * Looks for a socket name first then bone name
 *
 * @param AnimNotifyData The AnimNotify_ForceField which will have all of the various params on it
 *
 *	@return	bool		true if the forcefield was created, false if not;
 */
event bool CreateForceField( const AnimNotify_Forcefield AnimNotifyData)
{
	local NxForceFieldComponent NewForceFieldComponent;

	NewForceFieldComponent = 
		new(SkeletalMeshComponent) AnimNotifyData.ForceFieldComponent.Class(AnimNotifyData.ForceFieldComponent);
	NewForceFieldComponent.DoInitRBPhys();
	if( AnimNotifyData.SocketName != '' )
	{
		SkeletalMeshComponent.AttachComponentToSocket(  NewForceFieldComponent, AnimNotifyData.SocketName );
	}
	else if( AnimNotifyData.BoneName != '' )
	{	
		SkeletalMeshComponent.AttachComponent( NewForceFieldComponent, AnimNotifyData.BoneName );
	}

	return true;
}

defaultproperties
{
	Begin Object Class=AnimNodeSequence Name=AnimNodeSeq0
	End Object

	Begin Object Class=DynamicLightEnvironmentComponent Name=MyLightEnvironment
		bEnabled=TRUE
		TickGroup=TG_DuringAsyncWork
		// Using a skylight for secondary lighting by default to be cheap
		// Characters and other important skeletal meshes should set bSynthesizeSHLight=true
	End Object
	Components.Add(MyLightEnvironment)
	LightEnvironment=MyLightEnvironment

	Begin Object Class=SkeletalMeshComponent Name=SkeletalMeshComponent0
		Animations=AnimNodeSeq0
		bUpdateSkelWhenNotRendered=FALSE
		CollideActors=TRUE //@warning: leave at TRUE until backwards compatibility code is removed (bCollideActors_OldValue, etc)
		BlockActors=FALSE
		BlockZeroExtent=TRUE
		BlockNonZeroExtent=FALSE
		BlockRigidBody=FALSE
		LightEnvironment=MyLightEnvironment
		RBChannel=RBCC_GameplayPhysics
		RBCollideWithChannels=(Default=TRUE,BlockingVolume=TRUE,GameplayPhysics=TRUE,EffectPhysics=TRUE)
	End Object
	CollisionComponent=SkeletalMeshComponent0
	SkeletalMeshComponent=SkeletalMeshComponent0
	Components.Add(SkeletalMeshComponent0)

	Begin Object Class=AudioComponent Name=FaceAudioComponent
	End Object
	FacialAudioComp=FaceAudioComponent
	Components.Add(FaceAudioComponent)

	Physics=PHYS_None
	bEdShouldSnap=TRUE
	bStatic=FALSE
	bCollideActors=false
	bBlockActors=FALSE
	bWorldGeometry=FALSE
	bCollideWorld=FALSE
	bNoEncroachCheck=TRUE
	bProjTarget=TRUE
	bUpdateSimulatedPosition=FALSE

	RemoteRole=ROLE_None
	bNoDelete=TRUE

	bShouldDoAnimNotifies=true

	bCollideActors_OldValue=true
}
