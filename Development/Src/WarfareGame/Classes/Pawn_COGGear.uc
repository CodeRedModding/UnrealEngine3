/**
 * Pawn_COGGear
 * Base Warfare COG Gear Character klass
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

class Pawn_COGGear extends WarPawn
	config(Game);

simulated event Tick( float DeltaTime )
{
	local Rotator	AimRotation, EyesRotation;
	local float		AimAngle;

	super.Tick( Deltatime );

	// Test, controlling player spine rotation to match aim
	if( Mesh != None )
	{
		if( (CoverType == CT_None) && !InFreeCam() )
		{
			EyesRotation = GetBaseAimRotation();
			AimAngle = FNormalizedRotAxis( EyesRotation.Pitch );		// angle the controller is aiming at

			if( AimAngle != 0 )
			{
				AimRotation.Pitch	= int(-0.055f*AimAngle) & 65535;	// = Yaw
				AimRotation.Yaw		= int(-0.15f*AimAngle) & 65535;		// = Roll
				AimRotation.Roll	= int(-0.15f*AimAngle) & 65535;		// = Pitch

				Mesh.SetBoneRotation('Spine', AimRotation);
				Mesh.SetBoneRotation('Spine1', AimRotation);
				Mesh.SetBoneRotation('Spine2', AimRotation);

				AimRotation.Pitch	= int(-0.07f*AimAngle) & 65535;		// = Yaw
				AimRotation.Yaw		= int(-0.21f*AimAngle) & 65535;		// = Roll
				AimRotation.Roll	= int(-0.21f*AimAngle) & 65535;		// = Pitch

				Mesh.SetBoneRotation('LftShoulder', AimRotation);
				Mesh.SetBoneRotation('RtShoulder', AimRotation);
				Mesh.SetBoneRotation('Head', AimRotation);
			}
		}
		else
		{
			//@todo - handle aiming in various cover states
			Mesh.ClearBoneRotations();
		}
	}
}

defaultproperties
{
	ShieldAmount=100
	ShieldRechargeRate=0.20
	ShieldRechargeDelay=5

	KillReward=100
	FastKillBonus=25
	EliteKillBonus=50

	SoundGroupClass=class'SGroup_COGGear'

	DefaultInventory(0)=class'Weap_AssaultRifle'
	DefaultInventory(1)=class'Weap_MX8SnubPistol'
	DefaultInventory(2)=class'Weap_FragGrenade'
	DefaultInventory(3)=class'Weap_ScorchRifle'
	DefaultInventory(4)=class'Weap_MagnetGun'
	//DefaultInventory(5)=class'Weap_PhysicsGun'

	// Locational Damage definition
	LocDmgArray(0)=(fDamageMultiplier=2.0,BodyPartName="Head",BoneNameArray[0]=Neck)
	LocDmgArray(1)=(fDamageMultiplier=1.0,BodyPartName="Torso",BoneNameArray[0]=Hips,BoneNameArray[1]=Spine,BoneNameArray[2]=Spine2)
	LocDmgArray(2)=(fDamageMultiplier=1.0,BodyPartName="Left Arm",BoneNameArray[0]=LftArm,BoneNameArray[1]=LftForeArm,BoneNameArray[2]=LftHand)
	LocDmgArray(3)=(fDamageMultiplier=1.0,BodyPartName="Right Arm",BoneNameArray[0]=RtArm,BoneNameArray[1]=RtForeArm,BoneNameArray[2]=rthand)
	LocDmgArray(4)=(fDamageMultiplier=1.0,BodyPartName="Left Leg",BoneNameArray[0]=LftUpLeg,BoneNameArray[1]=LftLeg,BoneNameArray[2]=LftFoot)
	LocDmgArray(5)=(fDamageMultiplier=1.0,BodyPartName="Right Leg",BoneNameArray[0]=RtUpLeg,BoneNameArray[1]=RtLeg,BoneNameArray[2]=RtFoot)

	Begin Object Class=AnimTree_COGGear Name=anAnimTree
	End Object

	Begin Object Name=WarPawnMesh
		SkeletalMesh=SkeletalMesh'COG_Grunt.COG_Grunt_AMesh'
		PhysicsAsset=PhysicsAsset'COG_Grunt.COG_Grunt_AMesh_Physics'
		AnimSets(0)=AnimSet'COG_Grunt.COG_Grunt_BasicAnims'
		AnimSets(1)=AnimSet'COG_Grunt.COG_Grunt_CoverAnims'
		AnimSets(2)=AnimSet'COG_Grunt.COG_Grunt_EvadeAnims'
		AnimSets(3)=AnimSet'COG_Grunt.COG_Grunt_Attacks'
		Animations=anAnimTree
	End Object

	CrouchHeight=+60.0
	CrouchRadius=+44.0
	Begin Object Name=CollisionCylinder
		CollisionRadius=+0044.000000
		CollisionHeight=+0070.000000
	End Object

	Begin Object Name=WarPawnTransform
    	Translation=(Z=-74)
    End Object
}