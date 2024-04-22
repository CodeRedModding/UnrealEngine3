/**
 *	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *	Advanced version of SkeletalMeshCinematicActor which uses an AnimTree instead of having a single AnimNodeSequence defined in its defaultproperties
 */

class SkeletalMeshActorMAT extends SkeletalMeshCinematicActor
	native(Anim)
	placeable;

cpptext
{

#if WITH_EDITOR
	virtual void CheckForErrors();
#endif

	virtual void GetAnimControlSlotDesc(TArray<struct FAnimSlotDesc>& OutSlotDescs);
	virtual void PreviewBeginAnimControl(class UInterpGroup* InInterpGroup);
	virtual void PreviewSetAnimPosition(FName SlotName, INT ChannelIndex, FName InAnimSeqName, FLOAT InPosition, UBOOL bLooping, UBOOL bFireNotifies, UBOOL bEnableRootMotion, FLOAT DeltaTime);
	virtual void PreviewSetAnimWeights(TArray<FAnimSlotInfo>& SlotInfos);
	virtual void PreviewFinishAnimControl(class UInterpGroup* InInterpGroup);
	virtual void PreviewSetMorphWeight(FName MorphNodeName, FLOAT MorphWeight);
	virtual void PreviewSetSkelControlScale(FName SkelControlName, FLOAT Scale);
	virtual void SetSkelControlStrength(FName SkelControlName, FLOAT ControlStrength);

	/** Called each from while the Matinee action is running, to set the animation weights for the actor. */
	virtual void SetAnimWeights( const TArray<struct FAnimSlotInfo>& SlotInfos );
}

/** Array of Slots */
var transient Array<AnimNodeSlot>	SlotNodes;

/** Update AnimTree from track weights */
native function MAT_SetAnimWeights(Array<AnimSlotInfo> SlotInfos);

native function MAT_SetMorphWeight(name MorphNodeName, float MorphWeight);

native function MAT_SetSkelControlScale(name SkelControlName, float Scale);

native function MAT_SetSkelControlStrength(name SkelControlName, float ControlStrength);

simulated event Destroyed()
{
	// Clear AnimNode references.
	ClearAnimNodes();

	Super.Destroyed();
}

simulated event PostInitAnimTree(SkeletalMeshComponent SkelComp)
{
	Super.PostInitAnimTree(SkelComp);

	ClearAnimNodes();
	CacheAnimNodes();
}

simulated function CacheAnimNodes()
{
	local AnimNodeSlot SlotNode;

	// Cache all AnimNodeSlots.
	foreach SkeletalMeshComponent.AllAnimNodes(class'AnimNodeSlot', SlotNode)
	{
		SlotNodes[SlotNodes.Length] = SlotNode;
	}
}

simulated function ClearAnimNodes()
{
	SlotNodes.Length = 0;
}

/** Called each from while the Matinee action is running, with the desired sequence name and position we want to be at. */
simulated event SetAnimPosition(name SlotName, int ChannelIndex, name InAnimSeqName, float InPosition, bool bFireNotifies, bool bLooping, bool bEnableRootMotion)
{
	MAT_SetAnimPosition(SlotName, ChannelIndex, InAnimSeqName, InPosition, bFireNotifies, bLooping, bEnableRootMotion);
}
/** Update AnimTree from track info */
native function MAT_SetAnimPosition(name SlotName, int ChannelIndex, name InAnimSeqName, float InPosition, bool bFireNotifies, bool bLooping, bool bEnableRootMotion);

/** Called when we are done with the AnimControl track. */
simulated event FinishAnimControl(InterpGroup InInterpGroup)
{
	MAT_FinishAnimControl(InInterpGroup);
}

/** Called each frame by Matinee to update the weight of a particular MorphNodeWeight. */
simulated event SetMorphWeight(name MorphNodeName, float MorphWeight)
{
	MAT_SetMorphWeight(MorphNodeName, MorphWeight);
}

/** Called each frame by Matinee to update the scaling on a SkelControl. */
simulated event SetSkelControlScale(name SkelControlName, float Scale)
{
	MAT_SetSkelControlScale(SkelControlName, Scale);
}

defaultproperties
{
	Begin Object Name=SkeletalMeshComponent0
		Animations=None
	End Object
}
