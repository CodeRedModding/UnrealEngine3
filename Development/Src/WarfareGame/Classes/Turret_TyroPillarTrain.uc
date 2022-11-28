class Turret_TyroPillarTrain extends Turret
	config(Game);

/** Default inventory added via AddDefaultInventory() */
var config array<class<Inventory> >	DefaultInventory;

/**
 * Overridden to iterate through the DefaultInventory array and
 * give each item to this Pawn.
 * 
 * @see			GameInfo.AddDefaultInventory
 */
function AddDefaultInventory()
{
	local int		i;
	local Inventory	Inv;
	local WarPC wfPC;
	wfPC = WarPC(Controller);
	if (wfPC == None)
	{
		for (i=0; i<DefaultInventory.Length; i++)
		{
			// Ensure we don't give duplicate items
			if (FindInventoryType( DefaultInventory[i] ) == None)
			{
				Inv = CreateInventory( DefaultInventory[i] );
				if (Weapon(Inv) != None)
				{
					// don't allow default weapon to be thrown out
					Weapon(Inv).bCanThrow = false;
				}
			}
		}
	}
	else
	{
		CreateInventory(WarPC(Controller).SecondaryWeaponClass);
		CreateInventory(WarPC(Controller).PrimaryWeaponClass);
	}
}

defaultproperties
{
	bRelativeExitPos=false

	bReplicateWeapon=true
	InventoryManagerClass=class'WarInventoryManager'
	DefaultInventory(0)=class'Weap_TyroTurret'

	Begin Object Name=CollisionCylinder
        CollisionHeight=200.000000
        CollisionRadius=160.000000
		BlockZeroExtent=false
	End Object

	Begin Object Class=AnimNodeSequence Name=anIdleAnim
    	AnimSeqName=still
    End object

	Begin Object Class=SkeletalMeshComponent Name=SkelMeshComponent0
		SkeletalMesh=SkeletalMesh'TyroPillar.prototype-train-turret'
		AnimSets(0)=AnimSet'TyroPillar.TyroTrainTurretAnims'
		Animations=anIdleAnim
		BlockZeroExtent=true
		CollideActors=true
		BlockRigidBody=true
	End Object

	Begin Object Class=TransformComponent Name=TransformComponent0
		TransformedComponent=SkelMeshComponent0
		Scale=1.0
        Translation=(Z=-597.000000)
		Rotation=(Yaw=32768)
	End Object

	Mesh=SkelMeshComponent0
	MeshTransform=TransformComponent0
	Components.Add(TransformComponent0)

	//ExitPositions(0)=(Y=-400)
	//ExitPositions(1)=(Z=400)
	PitchBone=train-turret-gun
	BaseBone=train-turret-body
	ViewPitchMin=-4096
	ViewPitchMax=8192
	POV=(DirOffset=(X=-5,Y=0,Z=4),Distance=200,fZAdjust=-350)
	CannonFireOffset=(X=160,Y=0,Z=24)

	Components.Remove(Sprite)
}