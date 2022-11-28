/**
 * Geist Locust
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

class Pawn_GeistLocust extends WarPawn
	config(Game);

defaultproperties
{
	Health=150
	KillReward=100
	FastKillBonus=25
	EliteKillBonus=50

	SoundGroupClass=class'SGroup_COGGear'

	// Locational Damage definition
	LocDmgArray(0)=(fDamageMultiplier=2.0,BodyPartName="Head",BoneNameArray[0]=Neck,BoneNameArray[1]=Head)
	LocDmgArray(1)=(fDamageMultiplier=1.0,BodyPartName="Torso",BoneNameArray[0]=Hips,BoneNameArray[1]=Spine,BoneNameArray[2]=Spine2,BoneNameArray[3]=Spine1)
	LocDmgArray(2)=(fDamageMultiplier=1.0,BodyPartName="Left Arm",BoneNameArray[0]=LftArm,BoneNameArray[1]=LftForeArm,BoneNameArray[2]=LftHand)
	LocDmgArray(3)=(fDamageMultiplier=1.0,BodyPartName="Right Arm",BoneNameArray[0]=RtArm,BoneNameArray[1]=RtForeArm,BoneNameArray[2]=rthand)
	LocDmgArray(4)=(fDamageMultiplier=1.0,BodyPartName="Left Leg",BoneNameArray[0]=LftUpLeg,BoneNameArray[1]=LftLeg,BoneNameArray[2]=LftFoot)
	LocDmgArray(5)=(fDamageMultiplier=1.0,BodyPartName="Right Leg",BoneNameArray[0]=RtUpLeg,BoneNameArray[1]=RtLeg,BoneNameArray[2]=RtFoot)
	LocDmgArray(6)=(fDamageMultiplier=1.0,BodyPartName="Torso",BoneNameArray[0]=PadLft_2,BoneNameArray[1]=PadLft_2a,BoneNameArray[2]=PadLft_1)

	Begin Object Class=AnimTree_COGGear Name=anAnimTree
	End Object

	Begin Object Name=WarPawnMesh
		SkeletalMesh=Geist_Grunt.Geist
		AnimSets(0)=AnimSet'COG_Grunt.COG_Grunt_BasicAnims'
		AnimSets(1)=AnimSet'COG_Grunt.COG_Grunt_CoverAnims'
		AnimSets(2)=AnimSet'COG_Grunt.COG_Grunt_EvadeAnims'
		AnimSets(3)=AnimSet'COG_Grunt.COG_Grunt_Attacks'
		Animations=anAnimTree
		PhysicsAsset=Geist_Grunt.Geist_Physics
	End Object

	CoverTranslationOffset=(X=28,Y=0,Z=0)

	Begin Object Name=WarPawnTransform
		TransformedComponent=WarPawnMesh
		Translation=(X=0,Y=0,Z=-74)
	End Object
}