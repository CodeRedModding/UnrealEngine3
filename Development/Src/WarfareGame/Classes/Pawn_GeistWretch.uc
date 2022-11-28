/**
 * Geist Wretch
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

class Pawn_GeistWretch extends WarPawn
	config(Game);

defaultproperties
{
	KillReward=50
	FastKillBonus=10

	SoundGroupClass=class'SGroup_GeistWretch'
	DefaultInventory(0)=class'Weap_WretchMelee'

	Begin Object Class=AnimTree_GeistWretch Name=aWretchAnimTree
	End Object

	Begin Object Name=WarPawnMesh
		SkeletalMesh=SkeletalMesh'Geist_Wretch.Wretch'
		PhysicsAsset=PhysicsAsset'Geist_Wretch.Wretch_Physics'
		Animations=aWretchAnimTree
		AnimSets(0)=AnimSet'Geist_Wretch.DefaultAnimSet'
	End Object

	Begin Object Name=WarPawnTransform
    	Translation=(Z=-82)
    End Object

	// Locational Damage definition
	LocDmgArray(0)=(fDamageMultiplier=2.0,BodyPartName="Head",BoneNameArray[0]=Neck)
	LocDmgArray(1)=(fDamageMultiplier=1.0,BodyPartName="Torso",BoneNameArray[0]=Hips,BoneNameArray[1]=Spine,BoneNameArray[2]=Spine2)
	LocDmgArray(2)=(fDamageMultiplier=1.0,BodyPartName="Left Arm",BoneNameArray[0]=LftArm,BoneNameArray[1]=LftForeArm,BoneNameArray[2]=LftHand)
	LocDmgArray(3)=(fDamageMultiplier=1.0,BodyPartName="Right Arm",BoneNameArray[0]=RtArm,BoneNameArray[1]=RtForeArm,BoneNameArray[2]=rthand)
	LocDmgArray(4)=(fDamageMultiplier=1.0,BodyPartName="Left Leg",BoneNameArray[0]=LftUpLeg,BoneNameArray[1]=LftLeg,BoneNameArray[2]=LftFoot)
	LocDmgArray(5)=(fDamageMultiplier=1.0,BodyPartName="Right Leg",BoneNameArray[0]=RtUpLeg,BoneNameArray[1]=RtLeg,BoneNameArray[2]=RtFoot)
	LocDmgArray(6)=(fDamageMultiplier=1.0,BodyPartName="Torso",BoneNameArray[0]=LftShoulder,BoneNameArray[1]=Spine,BoneNameArray[2]=RtShoulder)
}