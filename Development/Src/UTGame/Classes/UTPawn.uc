class UTPawn extends Pawn
	config(Game)
	native
	abstract
	notplaceable;

var		bool	bNoTeamBeacon;			// never display team beacon for this pawn

var bool bBehindView;
var bool bFixedView;

var vector 	FixedViewLoc;
var rotator FixedViewRot;

var	bool	bCanDoubleJump;


/** Default inventory added via AddDefaultInventory() */
var config array<class<Inventory> >	DefaultInventory;

var int spree;

var float DodgeSpeed;
var float DodgeSpeedZ;
var eDoubleClickDir CurrentDir;
var int  MultiJumpRemaining;
var int  MaxMultiJump;
var int  MultiJumpBoost; // depends on the tolerance (100)
var int  SuperHealthMax;

var enum EWeaponHand
{
	HAND_Hidden,
	HAND_Centered,
	HAND_Left,
	HAND_Right
} WeaponHand;

var(Shield) float ShieldStrength;          // current shielding (having been activated)

var class<UTPawnSoundGroup> SoundGroupClass;

/*********************************************************************************************
 Weapon / Firing
********************************************************************************************* */

/** Holds the class type of the current weapon attachment.  It's replicated to all clients
 *  and is repnotify so that it can be tracked.
 */

var	repnotify	class<UTWeaponAttachment>	CurrentWeaponAttachmentClass;

/** This holds the local copy of the current attachment.  This "attachment" actor will exist
  * on all versions of the client.
  */

var				UTWeaponAttachment			CurrentWeaponAttachment;

/** This next group of replicated properties are used to cause 3rd person effects on
 *  remote clients.  FlashLocation and FlashCount are altered by the weapon to denote that
 *  a shot has occured and FiringMode is used to determine what type of shot.
 */

var repnotify	vector	FlashLocation;
var repnotify	byte	FlashCount;
var				byte	FiringMode;

/** WeaponBone contains the name of the bone used for attaching weapons to this pawn. */

var 			name 	WeaponBone;

cpptext
{
	virtual UBOOL TryJumpUp(FVector Dir, DWORD TraceFlags);
}

replication
{
    // xPawn replicated properties - moved here to take advantage of native replication
    reliable if (Role==ROLE_Authority)
        ShieldStrength, CurrentWeaponAttachmentClass, FlashCount, FlashLocation, FiringMode;
}


function PreRender(Canvas Canvas)
{
	if ( Weapon != None )
	{
		Weapon.SetLocation(WeaponPosition());
		if ( Controller == None )
			Weapon.SetRotation(Rotation);
		else
			Weapon.SetRotation(Controller.Rotation);
	}
}

//
// Compute offset for drawing an inventory item.
//
simulated function vector WeaponPosition()
{
	local vector DrawOffset;

	if ( UTWeapon(Weapon) == None )
		return vect(0,0,0);

	if ( Controller == None )
		return (UTWeapon(Weapon).PlayerViewOffset >> Rotation) + BaseEyeHeight * vect(0,0,1);

	if ( !IsLocallyControlled() )
		DrawOffset.Z = BaseEyeHeight;
	else
		DrawOffset.Z = EyeHeight;

    if( bWeaponBob )
		DrawOffset += WeaponBob(UTWeapon(Weapon).BobDamping);
    // FIXMESTEVE DrawOffset += UTWeapon(Weapon).BobDamping * CameraShake();

	DrawOffset = DrawOffset + (UTWeapon(Weapon).PlayerViewOffset >> Controller.Rotation);
	return Location + DrawOffset;
}

/* EyePosition()
Called by PlayerController to determine camera position in first person view.  Returns
the offset from the Pawn's location at which to place the camera
*/
simulated function vector EyePosition()
{
	return EyeHeight * vect(0,0,1) + WalkBob;
}

exec function BehindView()
{
	bBehindView = !bBehindView;
	SetMeshVisibility(bBehindView);
}

function SetMeshVisibility(bool bVisible)
{
	// Handle the main player mesh

	if ( Mesh != None )
		Mesh.bOwnerNoSee = !bVisible;

	// Handle any weapons they might have

	if ( UTWeapon(Weapon) != None )
	{
		if ( UTWeapon(Weapon).Mesh != None )
		{
			UTWeapon(Weapon).Mesh.SetHidden(bVisible);
		}

		if (CurrentWeaponAttachment!=None && CurrentWeaponAttachment.Mesh != None)
//		if ( (UTWeapon(Weapon).WeaponAttachment != None) && (UTWeapon(Weapon).WeaponAttachment.Mesh != None) )
		{
//			UTWeapon(Weapon).WeaponAttachment.Mesh.bOwnerNoSee = !bVisible;
			CurrentWeaponAttachment.Mesh.bOwnerNoSee = !bVisible;
		}
	}

}

exec function FixedView(string VisibleMeshes)
{
	local bool bVisibleMeshes;
	local float fov;

	if (VisibleMeshes != "")
	{
		bVisibleMeshes = ( VisibleMeshes ~= "yes" || VisibleMeshes~="true" || VisibleMeshes~="1" );

		if (VisibleMeshes ~= "default")
			bVisibleMeshes = bBehindView;

		SetMeshVisibility(bVisibleMeshes);
	}

	if (!bFixedView)
		PawnCalcCamera( 0.0f, FixedViewLoc, FixedViewRot, fov );

	bFixedView = !bFixedView;

	log("FixedView:"@bFixedView);

}


function DeactivateSpawnProtection()
{
	SpawnTime = -100000;
}

simulated event StartDriving(Vehicle V)
{
	Super.StartDriving(V);
	DeactivateSpawnProtection();
}

function DoComboName( string ComboClassName );
function bool InCurrentCombo()
{
	return false;
}
//=============================================================================
// UDamage stub.
function EnableUDamage(float Amount);
function DisableUDamage();

//=============================================================================
// Shield stubs.
function float GetShieldStrengthMax();
function float GetShieldStrength();
function bool AddShieldStrength(int Amount);
function int CanUseShield(int Amount);

function SetHand(EWeaponHand NewWeaponHand)
{
	WeaponHand = NewWeaponHand;
	/* FIXMESTEVE
	if ( UTWeapon(Weapon) != None) )
        UTWeapon(Weapon).SetHand(WeaponHand);
	*/
}

function HoldFlag(Actor FlagActor);
function DropFlag();

function bool GiveHealth(int HealAmount, int HealMax)
{
	if (Health < HealMax)
	{
		Health = Min(HealMax, Health + HealAmount);
        return true;
	}
    return false;
}

/**
 * Overridden to return the actual player name from this Pawn's
 * PlayerReplicationInfo (PRI) if available.
 */
function String GetDebugName()
{
	// return the actual player name from the PRI if available
	if (PlayerReplicationInfo != None)
	{
		return "";
	}
	// otherwise return the formatted object name
	return GetItemName(string(self));
}

event PlayFootStepSound(int FootDown)
{
	SoundGroupClass.static.PlayFootStepSound(self,FootDown);
}

/**
 * Overridden to check for local footsteps.
 */
function CheckBob(float DeltaTime, vector Y)
{
	local float OldBobTime;
	local int m,n;

	OldBobTime = BobTime;
	Super.CheckBob(DeltaTime,Y);

	if ( (Physics != PHYS_Walking) || (VSize(Velocity) < 10)
		|| ((PlayerController(Controller) != None) && (PlayerController(Controller).PlayerCamera != None)) )
		return;

	m = int(0.5 * Pi + 9.0 * OldBobTime/Pi);
	n = int(0.5 * Pi + 9.0 * BobTime/Pi);

	if ( (m != n) && !bIsWalking && !bIsCrouched )
	{
		PlayFootStepSound(0);
	}
}

/**
 * Responsible for playing any death effects, animations, etc.
 *
 * @param 	DamageType - type of damage responsible for this pawn's death
 *
 * @param	HitLoc - location of the final shot
 */
simulated function PlayDying(class<DamageType> DamageType, vector HitLoc)
{
    bCanTeleport = false;
    bReplicateMovement = false;
    bTearOff = true;
    bPlayedDeath = true;

	HitDamageType = DamageType; // these are replicated to other clients
    TakeHitLocation = HitLoc;

	if ( InitRagdoll() )
	    GotoState('Dying');
	else
	{
		warn("Ragdoll init failed");
		destroy();
	}
}

State Dying
{
ignores AnimEnd, Bump, HitWall, HeadVolumeChange, PhysicsVolumeChange, Falling, BreathTimer;

	function Landed(vector HitNormal)
	{
		LandBob = FMin(50, 0.055 * Velocity.Z);
	}

	function BeginState()
	{
		if ( (LastStartSpot != None) && (Level.TimeSeconds - LastStartTime < 7) )
			LastStartSpot.LastSpawnCampTime = Level.TimeSeconds;

		SetCollision(true,false);

        if ( bTearOff && (Level.NetMode == NM_DedicatedServer) )
			LifeSpan = 1.0;
		else
			SetTimer(2.0, false);

		if(Physics != PHYS_Articulated)
		{
			SetPhysics(PHYS_Falling);
		}

		bInvulnerableBody = true;

		if ( Controller != None )
		{
			if ( Controller.bIsPlayer )
				Controller.PawnDied(self);
			else
				Controller.Destroy();
		}
	}
}

/**
 * Overridden to check for any attached death events that should be
 * activated.
 *
 * @param		Killer - Controller that killed this Pawn
 *
 * @param		damageType - class of damage used
 *
 * @param 		HitLocation - location of the final hit
 */
function Died(Controller Killer, class<DamageType> damageType, vector HitLocation)
{
//FIXMESTEVE	ActivateEventClass(class'SeqEvent_Death',self);
	Super.Died(Killer,damageType,HitLocation);
}

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
	Controller.ClientSwitchToBestWeapon();
}

/**
 *	Calculate camera view point, when viewing this pawn.
 *
 * @param	fDeltaTime	delta time seconds since last update
 * @param	out_CamLoc	Camera Location
 * @param	out_CamRot	Camera Rotation
 * @param	out_FOV		Field of View
 *
 * @return	true if Pawn should provide the camera point of view.
 */
simulated function bool PawnCalcCamera( float fDeltaTime, out vector out_CamLoc, out rotator out_CamRot, out float out_FOV )
{

	// Handle the fixed camera

	if (bFixedView)
	{
		out_CamLoc = FixedViewLoc;
		out_CamRot = FixedViewRot;
	}
	else
	{
		// By default, we view through the Pawn's eyes..
		GetActorEyesViewPoint( out_CamLoc, out_CamRot );

		if (bBehindView)	// Handle BehindView
		{
			out_CamLoc = out_CamLoc - Vector(out_CamRot) * CylinderComponent.CollisionRadius * 9.0;  // fixme - should be variable
		}
	}

	return true;
}

function int GetSpree()
{
	return spree;
}

function IncrementSpree()
{
	spree++;
}

//=============================================
// Jumping functionality

function bool Dodge(eDoubleClickDir DoubleClickMove)
{
    local vector X,Y,Z, TraceStart, TraceEnd, Dir, Cross, HitLocation, HitNormal;
    local Actor HitActor;
	local rotator TurnRot;

    if ( bIsCrouched || bWantsToCrouch || (Physics != PHYS_Walking && Physics != PHYS_Falling) )
        return false;

	TurnRot.Yaw = Rotation.Yaw;
    GetAxes(TurnRot,X,Y,Z);

    if ( Physics == PHYS_Falling )
    {
        if (DoubleClickMove == DCLICK_Forward)
            TraceEnd = -X;
        else if (DoubleClickMove == DCLICK_Back)
            TraceEnd = X;
        else if (DoubleClickMove == DCLICK_Left)
            TraceEnd = Y;
        else if (DoubleClickMove == DCLICK_Right)
            TraceEnd = -Y;
        TraceStart = Location - CylinderComponent.CollisionHeight*Vect(0,0,1) + TraceEnd*CylinderComponent.CollisionRadius;
        TraceEnd = TraceStart + TraceEnd*32.0;
        HitActor = Trace(HitLocation, HitNormal, TraceEnd, TraceStart, false, vect(1,1,1));
        if ( (HitActor == None) || !HitActor.bWorldGeometry ) // FIXME - what about dodgin off movers, etc
             return false;
	}
    if (DoubleClickMove == DCLICK_Forward)
    {
		Dir = X;
		Cross = Y;
	}
    else if (DoubleClickMove == DCLICK_Back)
    {
		Dir = -1 * X;
		Cross = Y;
	}
    else if (DoubleClickMove == DCLICK_Left)
    {
		Dir = -1 * Y;
		Cross = X;
	}
    else if (DoubleClickMove == DCLICK_Right)
    {
		Dir = Y;
		Cross = X;
	}
	if ( AIController(Controller) != None )
		Cross = vect(0,0,0);
	return PerformDodge(DoubleClickMove, Dir,Cross);
}

function bool PerformDodge(eDoubleClickDir DoubleClickMove, vector Dir, vector Cross)
{
    local float VelocityZ;

    if ( Physics == PHYS_Falling )
    {
		// FIXME - play wall dodge animation
		TakeFallingDamage();
        if (Velocity.Z < -DodgeSpeedZ*0.5)
			Velocity.Z += DodgeSpeedZ*0.5;
    }

    VelocityZ = Velocity.Z;
    Velocity = DodgeSpeed*Dir + (Velocity Dot Cross)*Cross;

	if ( Velocity.Z < -100 )
		Velocity.Z = VelocityZ + DodgeSpeedZ;
	else
		Velocity.Z = DodgeSpeedZ;

    CurrentDir = DoubleClickMove;
    SetPhysics(PHYS_Falling);
    SoundGroupClass.Static.PlayDodgeSound(self);
    return true;
}

function DoDoubleJump( bool bUpdating )
{
    // FIXME PlayDoubleJump();

    if ( !bIsCrouched && !bWantsToCrouch )
    {
		if ( !IsLocallyControlled() || (AIController(Controller) != None) )
			MultiJumpRemaining -= 1;
        Velocity.Z = JumpZ + MultiJumpBoost;
        SetPhysics(PHYS_Falling);
        if ( !bUpdating )
			SoundGroupClass.Static.PlayDoubleJumpSound(self);
    }
}

function bool DoJump( bool bUpdating )
{
    // This extra jump allows a jumping or dodging pawn to jump again mid-air
    // (via thrusters). The pawn must be within +/- 100 velocity units of the
    // apex of the jump to do this special move.
    if ( !bUpdating && CanDoubleJump()&& (Abs(Velocity.Z) < 100) && IsLocallyControlled() )
    {
		if ( PlayerController(Controller) != None )
			PlayerController(Controller).bDoubleJump = true;
        DoDoubleJump(bUpdating);
        MultiJumpRemaining -= 1;
        return true;
    }

    if ( Super.DoJump(bUpdating) )
    {
		if ( !bUpdating )
		    SoundGroupClass.Static.PlayJumpSound(self);
        return true;
    }
    return false;
}

event Landed(vector HitNormal)
{
    super.Landed( HitNormal );
    MultiJumpRemaining = MaxMultiJump;

    if ( (Health > 0) && !bHidden && (Level.TimeSeconds - SplashTime > 0.25) )
		SoundGroupClass.Static.PlayLandSound(self);
}
function bool CanDoubleJump()
{
	return ( (MultiJumpRemaining > 0) && (Physics == PHYS_Falling) );
}

function PlayDyingSound()
{
	SoundGroupClass.Static.PlayDyingSound(self);
}

simulated function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	local int i,j;
	local TransformComponent T;
	local PrimitiveComponent P;
	local string s;
	local float xl,yl;
	super.DisplayDebug(HUD, out_YL, out_YPos);

	for (i=0;i<Mesh.Attachments.Length;i++)
	{
	    HUD.Canvas.SetPos(4,out_YPos);

	    s = ""$Mesh.Attachments[i].Component;
		Hud.Canvas.Strlen(s,xl,yl);
		j = len(s);
		while ( xl > (Hud.Canvas.ClipX*0.5) && j>10)
		{
			j--;
			s = Right(S,j);
			Hud.Canvas.StrLen(s,xl,yl);
		}

		HUD.Canvas.DrawText("Attachment"@i@" = "@Mesh.Attachments[i].BoneName@s);
	    out_YPos += out_YL;

	    T = TransformComponent(Mesh.Attachments[i].Component);
	    if (T!=None)
	    {
	        P = PrimitiveComponent(T.TransformedComponent);
	        if (P!=None)
	        {
			    HUD.Canvas.SetPos(24,out_YPos);
			    HUD.Canvas.DrawText("Component = "@P.Owner@P.HiddenGame@P.bOnlyOwnerSee@P.bOwnerNoSee);
			    out_YPos += out_YL;

			    s = ""$P;
				Hud.Canvas.Strlen(s,xl,yl);
				j = len(s);
				while ( xl > (Hud.Canvas.ClipX*0.5) && j>10)
				{
					j--;
					s = Right(S,j);
					Hud.Canvas.StrLen(s,xl,yl);
				}

			    HUD.Canvas.SetPos(24,out_YPos);
			    HUD.Canvas.DrawText("Component = "@s);
			    out_YPos += out_YL;
			}
			else
			{
			    HUD.Canvas.SetPos(24,out_YPos);
			    HUD.Canvas.DrawText("Unknown = "@T.TransformedComponent);
			    out_YPos += out_YL;
			}
		}
	}
}



/*********************************************************************************************
 * Weapon Firing
 *********************************************************************************************/

/**
 * Check on various replicated data and act accordingly.
 */

simulated event ReplicatedEvent(string VarName)
{
	super.ReplicatedEvent(VarName);

	// If CurrentWeaponAttachmentClass has changed, the player has switched weapons and
	// will need to update itself accordingly.

	if ( VarName ~= "CurrentWeaponAttachmentClass" )
		WeaponAttachmentChanged();

	// FlashCount and FlashLocation are changed when a weapon is fired.    Note that in both cases, we ignore
	// their reset values.

	else if ( (VarName ~= "FlashCount" && FlashCount >0) || (VarName ~= "FlashLocation" && FlashLocation != vect(0,0,0)) )
	{
		// WeaponFired() is called by IncrementFlashCount on local clients

		WeaponFired(true);
	}
}

/**
 * This function's responsibility is to signal clients that non-instant hit shot
 * has been fired.
 *
 * This is a server only function
 */
function IncrementFlashCount(byte FireModeNum)
{
	// Force replication

	if (Role==ROLE_Authority)
	{
		NetUpdateTime = Level.TimeSeconds - 1;
	}

	FiringMode = FireModeNum;
	FlashCount++;

	// This weapon has fired.

	WeaponFired(false);
}

/**
 * This function sets up the Location of a hit to be replicated to all remote clients.
 * It is also responsible for fudging a shot at 0,0,0 and setting up a timer to clear
 * the FlashLocation.
 *
 * This is a server only function
 */

function SetFlashLocation(vector Location)
{

	// If we are aiming at the origin, aim slightly up since we use 0,0,0 to denote
	// not firing.

	if ( Location == vect(0,0,0) )
		Location = vect(0,0,1);

	// Force replication

	if (Role==ROLE_Authority)
	{
		NetUpdateTime = Level.TimeSeconds - 1;
	}

	FlashLocation = Location;
	SetTimer(0.10, false, 'ClearFlashLocation');

	// This weapon has fired.

	WeaponFired(false);
}

/**
 * Clear out the FlashLocation variable so it can be used later.
 */

function ClearFlashLocation()
{
	FlashLocation = vect(0,0,0);
}


/**
 * Called when a pawn's weapon has fired and is responsibile for
 * delegating the creation off all of the different effects.
 *
 * bViaReplication denotes if this call in as the result of the
 * flashcount/flashlocation being replicated.  It's used filter out
 * when to make the effects.
 */

simulated function WeaponFired(bool bViaReplication)
{
	if (CurrentWeaponAttachment!=None)
	{
		if ( bBehindView || !IsLocallyControlled() )
			CurrentWeaponAttachment.ThirdPersonFireEffects();

		if (Role==ROLE_Authority || bViaReplication)
			CurrentWeaponAttachment.ImpactEffects();
	}
}

/**
 * Called when a weapon is changed and is responsible for making sure
 * the new weapon respects the current pawn's states/etc.
 */

simulated function WeaponChanged(UTWeapon NewWeapon)
{
	// Make sure the new weapon respects behindview

	if (NewWeapon.Mesh!=None)
		NewWeapon.Mesh.SetHidden(bBehindView);
}

/**
 * Called when there is a need to change the weapon attachment (either via
 * replication or locally if controlled.
 */

simulated function WeaponAttachmentChanged()
{
	if ( CurrentWeaponAttachment==None || CurrentWeaponAttachment.Class != CurrentWeaponAttachmentClass)
	{
    	// Detach/Destory the current attachment if we have one

		if (CurrentWeaponAttachment!=None)
		{
			CurrentWeaponAttachment.DetachFrom(Mesh);
			CurrentWeaponAttachment.Destroy();
		}

		// Create the new Attachment.

		if (CurrentWeaponAttachmentClass!=None)
		{
			CurrentWeaponAttachment = Spawn(CurrentWeaponAttachmentClass,self);
			CurrentWeaponAttachment.Instigator = self;
		}
		else
			CurrentWeaponAttachment = none;

		// If all is good, attach it to the Pawn's Mesh.

		if (CurrentWeaponAttachment!=None)
		{
			CurrentWeaponAttachment.AttachTo(Mesh,WeaponBone);
		}
	}
}

/**
 * Temp Code - Adjust the Weapon Attachment
 */

exec function AttachAdjustMesh(string cmd)
{
	local string c,v;
	local vector t,s,l;
	local rotator r;
	local float sc;

	c = left(Cmd,InStr(Cmd,"="));
	v = mid(Cmd,InStr(Cmd,"=")+1);

	t  = CurrentWeaponAttachment.MeshTransform.Translation;
	r  = CurrentWeaponAttachment.MeshTransform.Rotation;
	s  = CurrentWeaponAttachment.MeshTransform.Scale3D;
	sc = CurrentWeaponAttachment.MeshTransform.	Scale;
	l  = CurrentWeaponAttachment.MuzzleFlashLightTransform.Translation;

	if (c~="x")  t.X += float(v);
	if (c~="ax") t.X =  float(v);
	if (c~="y")  t.Y += float(v);
	if (c~="ay") t.Y =  float(v);
	if (c~="z")  t.Z += float(v);
	if (c~="az") t.Z =  float(v);

	if (c~="r")   R.Roll  += int(v);
	if (c~="ar")  R.Roll  =  int(v);
	if (c~="p")   R.Pitch += int(v);
	if (c~="ap")  R.Pitch =  int(v);
	if (c~="w")   R.Yaw   += int(v);
	if (c~="aw")  R.Yaw   =  int(v);

	if (c~="scalex") s.X = float(v);
	if (c~="scaley") s.Y = float(v);
	if (c~="scalez") s.Z = float(v);

	if (c~="scale") sc = float(v);

	if (c~="lx") l.X = float(v);
	if (c~="ly") l.Y = float(v);
	if (c~="lz") l.Z = float(v);

	CurrentWeaponAttachment.MeshTransform.SetTranslation(t);
	CurrentWeaponAttachment.MeshTransform.SetRotation(r);
	CurrentWeaponAttachment.MeshTransform.SetScale(sc);
	CurrentWeaponAttachment.MeshTransform.SetScale3D(s);
	CurrentWeaponAttachment.MuzzleFlashLightTransform.SetTranslation(l);

	log("#### AdjustMesh ####");
	log("####    Translation :"@CurrentWeaponAttachment.MeshTransform.Translation);
	log("####    Rotation    :"@CurrentWeaponAttachment.MeshTransform.Rotation);
	log("####    Scale3D     :"@CurrentWeaponAttachment.MeshTransform.Scale3D);
	log("####    scale       :"@CurrentWeaponAttachment.MeshTransform.Scale);
	log("####	 light       :"@CurrentWeaponAttachment.MuzzleFlashLightTransform.Translation);
}


defaultproperties
{
	Begin Object Name=CollisionCylinder
		CollisionRadius=+0025.000000
		CollisionHeight=+0044.000000
	End Object
	CylinderComponent=CollisionCylinder

    WalkingPct=+0.4
    CrouchedPct=+0.4

    BaseEyeHeight=38.0
    EyeHeight=38.0
    GroundSpeed=440.0
    AirSpeed=440.0
    WaterSpeed=220.0
    DodgeSpeed=660.0
    DodgeSpeedZ=210.0
    AccelRate=2048.0
    JumpZ=340.0
    CrouchHeight=29.0
    CrouchRadius=25.0

	InventoryManagerClass=class'UTInventoryManager'

	MeleeRange=+20.0
	bMuffledHearing=true

    Buoyancy=+000.99000000
    UnderWaterTime=+00020.000000
    bCanStrafe=True
	bCanSwim=true
    RotationRate=(Pitch=0,Yaw=20000,Roll=2048)
	AirControl=+0.35
	bStasis=false
	bCanCrouch=true
	bCanClimbLadders=True
	bCanPickupInventory=True
	bCanDoubleJump=true
	SightRadius=+12000.0

    MaxMultiJump=1
    MultiJumpRemaining=1
    MultiJumpBoost=25

    SoundGroupClass=class'UTGame.UTPawnSoundGroup'
}
