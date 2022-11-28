class UTCharacter extends UTPawn
	Placeable;

var SkeletalMeshComponent 	HeadMesh;
var vector					HeadMeshOffset;
var rotator					HeadMeshRotation;
//var UTChar_BaseAnims		UTAnimations;

var UTAnimBlendByWeapon		WeaponBlend;

simulated event PostNetBeginPlay()
{
	super.PostNetBeginPlay();

	Mesh.AttachComponent(HeadMesh,'Head',HeadMeshOffset,HeadMeshRotation);
	HeadMesh.ShadowParent = Mesh;

	WeaponBlend = UTAnimBlendByWeapon( mesh.Animations.FindAnimNode('FireNode') );
	if (WeaponBlend==None)
		Log("WARNING: Could not find Firing Node (firenode) for this mesh ("$mesh$")");
	else
		WeaponBlend.SetChild2StartBone('Spine',1.0);

}

simulated event Destroyed()
{
	// Destroy our head.

	if (HeadMesh!=None)
	{

		Mesh.DetachComponent(HeadMesh);
		HeadMesh.ShadowParent = none;
		HeadMesh = none;
	}

	// Blank our Weapon Blend

	WeaponBlend = none;

	super.Destroyed();

}

exec function AlignHead(string Cmd)
{
	local string c,v;

	c = left(Cmd,InStr(Cmd,"="));
	v = mid(Cmd,InStr(Cmd,"=")+1);

	if (c~="x")  HeadMeshOffset.X += float(v);
	if (c~="ax") HeadMeshOffset.X =  float(v);
	if (c~="y")  HeadMeshOffset.Y += float(v);
	if (c~="ay") HeadMeshOffset.Y =  float(v);
	if (c~="z")  HeadMeshOffset.Z += float(v);
	if (c~="az") HeadMeshOffset.Z =  float(v);

	if (c~="r")   HeadMeshRotation.Roll  += int(v);
	if (c~="ar")  HeadMeshRotation.Roll  =  int(v);
	if (c~="p")   HeadMeshRotation.Pitch += int(v);
	if (c~="ap")  HeadMeshRotation.Pitch =  int(v);
	if (c~="w")   HeadMeshRotation.Yaw   += int(v);
	if (c~="aw")  HeadMeshRotation.Yaw   =  int(v);

	log("#### AlignHead:"@HeadMeshOffset@HeadMeshRotation);

	mesh.DetachComponent(HeadMesh);
	mesh.AttachComponent(HeadMesh,'Head',HeadMeshOffset, HeadMeshRotation);
}

function SetMeshVisibility(bool bVisible)
{
	super.SetMeshVisibility(bVisible);

	if (HeadMesh!=None)
		HeadMesh.bOwnerNoSee = !bVisible;
}


event Tick(float DeltaTime)
{
	local Rotator	AimRotation, EyesRotation;
	local float		AimAngle;

	super.Tick( Deltatime );
	return;
	// Test, controlling player spine rotation to match aim
	if ( Mesh != None )
	{
		EyesRotation = GetBaseAimRotation();
		AimAngle = FNormalizedRotAxis( EyesRotation.Pitch );	// angle the controller is aiming at

		if ( AimAngle != 0 )
		{
			AimRotation.Pitch	= int(-0.055f*AimAngle) & 65535; // = Yaw
			AimRotation.Yaw		= int(-0.15f*AimAngle) & 65535;	 // = Roll
			AimRotation.Roll	= int(-0.15f*AimAngle) & 65535;	 // = Pitch

			Mesh.SetBoneRotation('Spine', AimRotation);
			Mesh.SetBoneRotation('Spine1', AimRotation);
			Mesh.SetBoneRotation('Spine2', AimRotation);

			AimRotation.Pitch	= int(-0.07f*AimAngle) & 65535;	// = Yaw
			AimRotation.Yaw		= int(-0.21f*AimAngle) & 65535;	// = Roll
			AimRotation.Roll	= int(-0.21f*AimAngle) & 65535;	// = Pitch

//			Mesh.SetBoneRotation('LftShoulder', AimRotation);
//			Mesh.SetBoneRotation('RtShoulder', AimRotation);
//			Mesh.SetBoneRotation('Head', AimRotation);
		}
	}

	bWantsToCrouch = false;
}

/** Plays a Firing Animation */

simulated function PlayFiring(float Rate, name FiringMode)
{
	WeaponBlend.AnimFire('fire_straight_rif',false,Rate, 0.15);
}

simulated function StopPlayFiring()
{
	WeaponBlend.AnimStopFire(0.15);
}

defaultproperties
{
	DefaultInventory(0)=class'UTWeap_Minigun'

	Begin Object class=SkeletalMeshComponent name=smHeadMesh
		SkeletalMesh=SkeletalMesh'Sobek.Models.S_Characters_Sobek_Model_Head'
		bOwnerNoSee=true
		AlwaysLoadOnClient=true
		AlwaysLoadOnServer=true
		CastShadow=false
	End Object
	HeadMesh=smHeadMesh

	Begin Object Class=SkeletalMeshComponent Name=WPawnSkeletalMeshComponent
		SkeletalMesh=SkeletalMesh'Sobek.Models.S_Characters_Sobek_Model_Body'
		PhysicsAsset=none
		AnimSets(0)=AnimSet'Sobek.Anims.K_Characters_Sobek_Anims_BaseAnims'
		AnimTreeTemplate=AnimTree'UTCharacterAnim.Trees.BaseChar_AnimTree'
		AlwaysLoadOnClient=true
		AlwaysLoadOnServer=true
		bOwnerNoSee=true
		CastShadow=false
	End Object
	Mesh=WPawnSkeletalMeshComponent

	Begin Object Class=TransformComponent Name=WPawnTransformComponent
    	TransformedComponent=WPawnSkeletalMeshComponent
    End Object

    MeshTransform=WPawnTransformComponent
	Components.Add(WPawnTransformComponent)
	Components.Remove(Sprite)

	WeaponBone=RightHand

}

