//=============================================================================
// UTPlayerController.
//=============================================================================
class UTPlayerController extends PlayerController
	dependson(UTPawn)
	config(Game);

var					bool	bLateComer;
var					bool 	bDontUpdate;

var globalconfig	bool	bNoVoiceMessages;
var globalconfig	bool	bNoTextToSpeechVoiceMessages;
var globalconfig	bool	bNoVoiceTaunts;
var globalconfig	bool	bNoAutoTaunts;
var globalconfig	bool	bAutoTaunt;
var globalconfig	bool	bNoMatureLanguage;

var globalconfig	bool	bEnableDodging;
var globalconfig    bool    bLookUpStairs;  // look up/down stairs (player)
var globalconfig    bool    bSnapToLevel;   // Snap to level eyeheight when not mouselooking
var globalconfig    bool    bAlwaysMouseLook;
var globalconfig    bool    bKeyboardLook;  // no snapping when true
var					bool    bCenterView;
var globalconfig	bool	bAlwaysLevel;

var globalconfig UTPawn.EWeaponHand WeaponHandPreference;
var UTPawn.EWeaponHand				WeaponHand;

var vector		DesiredLocation;

var(TeamBeacon) float      TeamBeaconMaxDist;
var(TeamBeacon) float      TeamBeaconPlayerInfoMaxDist;
var(TeamBeacon) Texture    TeamBeaconTexture;
var(TeamBeacon) Texture    LinkBeaconTexture;
var(TeamBeacon) Texture    SpeakingBeaconTexture;
var(TeamBeacon) Color      TeamBeaconTeamColors[2];
var(TeamBeacon) Color      TeamBeaconCustomColor;

var rotator BlendedTargetViewRotation;				// used for smoothing the viewrotation of spectated players (first person mode)

var UTAnnouncer Announcer;

replication
{
	// Things the server should send to the client.
	unreliable if( Role==ROLE_Authority )
		ClientPlayTakeHit, PlayStartupMessage, ClientPlayAnnouncement;

	// Functions client can call.
	unreliable if( Role<ROLE_Authority )
        ShortServerMove;

	reliable if ( Role<ROLE_Authority )
        ServerSetHand, ServerPlayerPreferences, ServerSetAutotaunt;
}

simulated event Destroyed()
{
	Super.Destroyed();

	if ( Announcer != None )
		Announcer.Destroy();
}

/* CheckJumpOrDuck()
Called by ProcessMove()
handle jump and duck buttons which are pressed
*/
function CheckJumpOrDuck()
{
	if ( bDoubleJump && (bUpdating || UTPawn(Pawn).CanDoubleJump()) )
	{
		UTPawn(Pawn).DoDoubleJump( bUpdating );
	}
    else if ( bPressedJump )
	{
		Pawn.DoJump( bUpdating );
	}
	if ( Pawn.Physics != PHYS_Falling && Pawn.bCanCrouch )
	{
		// crouch if pressing duck
		Pawn.ShouldCrouch(bDuck != 0);
	}
}

event PreRender(Canvas Canvas)
{
	if ( UTPawn(Pawn) != None )
		UTPawn(Pawn).PreRender(Canvas);
}

exec function TestGravity( float F )
{
	PhysicsVolume.Gravity.Z = -475 * F;
	Pawn.JumpZ = Pawn.Default.JumpZ * sqrt(F);
	UTPawn(Pawn).DodgeSpeedZ = UTPawn(Pawn).Default.DodgeSpeedZ * sqrt(F);
	UTPawn(Pawn).DodgeSpeed = UTPawn(Pawn).DodgeSpeed * sqrt(F);
}

function Possess(Pawn aPawn)
{
	Super.Possess(aPawn);
	ServerPlayerPreferences(WeaponHandPreference, bAutoTaunt);
}

function AcknowledgePossession(Pawn P)
{
	Super.AcknowledgePossession(P);

	if ( LocalPlayer(Player) != None )
	{
		ServerPlayerPreferences(WeaponHandPreference, bAutoTaunt);
	}
}

function SetPawnFemale()
{
// FIXMESTEVE - only if no valid character	if ( PawnSetupRecord.Species == None )
		PlayerReplicationInfo.bIsFemale = true;
}

function NotifyChangedWeapon( Weapon PrevWeapon, Weapon NewWeapon )
{
	// FIXME - should pawn do this directly?
	if ( UTPawn(Pawn) != None )
		UTPawn(Pawn).SetHand(WeaponHand);

	/* FIXMESTEVE
    if ( Pawn != None && Pawn.Weapon != None )
    {
        LastPawnWeapon = Pawn.Weapon.Class;
    }
	*/
}

function ServerPlayerPreferences( UTPawn.EWeaponHand NewWeaponHand, bool bNewAutoTaunt)
{
	ServerSetHand(NewWeaponHand);
	ServerSetAutoTaunt(bNewAutoTaunt);
}

function ServerSetHand(UTPawn.EWeaponHand NewWeaponHand)
{
	WeaponHand = NewWeaponHand;
    if ( UTPawn(Pawn) != None )
		UTPawn(Pawn).SetHand(WeaponHand);
}

function SetHand(UTPawn.EWeaponHand NewWeaponHand)
{
	WeaponHandPreference = NewWeaponHand;
    SaveConfig();
    if ( UTPawn(Pawn) != None )
		UTPawn(Pawn).SetHand(WeaponHand);

    ServerSetHand(NewWeaponHand);
}

event ResetCameraMode()
{
	if ( PlayerCamera != None )
	{
		Super.ResetCameraMode();
	}
	else if ( (UTPawn(Pawn) != None) && UTPawn(Pawn).bBehindView )
	{
		UTPawn(Pawn).BehindView();
	}
}
// ------------------------------------------------------------------------

function ServerSetAutoTaunt(bool Value)
{
	bAutoTaunt = Value;
}

exec function SetAutoTaunt(bool Value)
{
	Default.bAutoTaunt = Value;
	StaticSaveConfig();
	bAutoTaunt = Value;

	ServerSetAutoTaunt(Value);
}

exec function ToggleScreenShotMode()
{
	if ( UTHUD(myHUD).bCrosshairShow )
	{
		UTHUD(myHUD).bCrosshairShow = false;
		//FIXMESTEVE SetWeaponHand("Hidden");
		//FIXMESTEVE myHUD.bHideHUD = true;
		TeamBeaconMaxDist = 0;
	}
	else
	{
		// return to normal
		UTHUD(myHUD).bCrosshairShow = true;
		//FIXMESTEVE SetWeaponHand("Right");
		//FIXMESTEVE myHUD.bHideHUD = false;
		TeamBeaconMaxDist = default.TeamBeaconMaxDist;
	}
}

function PlayStartupMessage(byte StartupStage)
{
	ReceiveLocalizedMessage( class'UTStartupMessage', StartupStage, PlayerReplicationInfo );
}

function NotifyTakeHit(pawn InstigatedBy, vector HitLocation, int Damage, class<DamageType> damageType, vector Momentum)
{
	local int iDam;

	Super.NotifyTakeHit(InstigatedBy,HitLocation,Damage,DamageType,Momentum);

	iDam = Clamp(Damage,0,250);
	if ( (Level.NetMode == NM_DedicatedServer) || (Level.NetMode == NM_ListenServer) )
		ClientPlayTakeHit(hitLocation - Pawn.Location, iDam, damageType);
}

function ClientPlayTakeHit(vector HitLoc, byte Damage, class<DamageType> damageType)
{
	HitLoc += Pawn.Location;
	Pawn.PlayTakeHit(HitLoc, Damage, damageType);
}

// Player movement.
// Player Standing, walking, running, falling.
state PlayerWalking
{
ignores SeePlayer, HearNoise, Bump;

	function bool NotifyLanded(vector HitNormal)
	{
		if (DoubleClickDir == DCLICK_Active)
		{
			DoubleClickDir = DCLICK_Done;
			ClearDoubleClick();
			Pawn.Velocity *= Vect(0.1,0.1,1.0);
		}
		else
			DoubleClickDir = DCLICK_None;

		if ( Global.NotifyLanded(HitNormal) )
			return true;

		return false;
	}

	function ProcessMove(float DeltaTime, vector NewAccel, eDoubleClickDir DoubleClickMove, rotator DeltaRot)
	{

		if ( !bEnableDodging )
		{
			DoubleClickMove = DCLICK_None;
		}
		if ( (DoubleClickMove == DCLICK_Active) && (Pawn.Physics == PHYS_Falling) )
			DoubleClickDir = DCLICK_Active;
		else if ( (DoubleClickMove != DCLICK_None) && (DoubleClickMove < DCLICK_Active) )
		{
			if ( UTPawn(Pawn).Dodge(DoubleClickMove) )
				DoubleClickDir = DCLICK_Active;
		}

		Super.ProcessMove(DeltaTime,NewAccel,DoubleClickMove,DeltaRot);
	}

    function PlayerMove( float DeltaTime )
    {
//		local rotator ViewRotation;

        GroundPitch = 0;
/* FIXMESTEVE
        ViewRotation = Rotation;
        if ( Pawn.Physics == PHYS_Walking )
        {
            //if walking, look up/down stairs - unless player is rotating view
             if ( (bLook == 0)
                && (((Pawn.Acceleration != Vect(0,0,0)) && bSnapToLevel) || !bKeyboardLook) )
            {
                if ( bLookUpStairs || bSnapToLevel )
                {
                    GroundPitch = FindStairRotation(deltaTime);
                    ViewRotation.Pitch = GroundPitch;
                }
                else if ( bCenterView )
                {
                    ViewRotation.Pitch = ViewRotation.Pitch & 65535;
                    if (ViewRotation.Pitch > 32768)
                        ViewRotation.Pitch -= 65536;
                    ViewRotation.Pitch = ViewRotation.Pitch * (1 - 12 * FMin(0.0833, deltaTime));
                    if ( (Abs(ViewRotation.Pitch) < 250) && (ViewRotation.Pitch < 100) )
                        ViewRotation.Pitch = -249;
                }
            }
        }
        else
        {
            if ( !bKeyboardLook && (bLook == 0) && bCenterView )
            {
                ViewRotation.Pitch = ViewRotation.Pitch & 65535;
                if (ViewRotation.Pitch > 32768)
                    ViewRotation.Pitch -= 65536;
                ViewRotation.Pitch = ViewRotation.Pitch * (1 - 12 * FMin(0.0833, deltaTime));
                if ( (Abs(ViewRotation.Pitch) < 250) && (ViewRotation.Pitch < 100) )
                    ViewRotation.Pitch = -249;
            }
        }
        SetRotation(ViewRotation);
*/
		Super.PlayerMove(DeltaTime);
	}
}

function ServerSpectate()
{
	GotoState('Spectating');
}

state Dead
{
	ignores SeePlayer, HearNoise, KilledBy, NextWeapon, PrevWeapon;

	exec function SwitchWeapon(byte T){}

	exec function StartFire( optional byte FireModeNum )
	{
		if ( bFrozen )
		{
			if ( !IsTimerActive() || GetTimerCount() > 1.f )
				bFrozen = false;
			return;
		}
		if ( PlayerReplicationInfo.bOutOfLives )
			ServerSpectate();
		else
			super.StartFire( FireModeNum );
	}

Begin:
    Sleep(3.0);
	if ( (ViewTarget == None) || (ViewTarget == self) || (VSize(ViewTarget.Velocity) < 1.0) )
	{
		Sleep(1.0);
		myHUD.bShowScores = true;
	}
	else
		Goto('Begin');
}

state GameEnded
{
ignores SeePlayer, HearNoise, KilledBy, NotifyBump, HitWall, NotifyHeadVolumeChange, NotifyPhysicsVolumeChange, Falling, TakeDamage, Suicide;

	exec function SwitchWeapon(byte T) {}
}

/** 
 * list important UTPlayerController variables on canvas. HUD will call DisplayDebug() on the current ViewTarget when
 * the ShowDebug exec is used
 *
 * @param	HUD		- HUD with canvas to draw on
 * @input	out_YL		- Height of the current font
 * @input	out_YPos	- Y position on Canvas. out_YPos += out_YL, gives position to draw text for next debug line.
 */
function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	local Canvas Canvas;

	Canvas = HUD.Canvas;
	Canvas.SetDrawColor(255,255,255,255);

	Canvas.DrawText("CONTROLLER "$GetItemName(string(self))$" Pawn "$GetItemName(string(Pawn)));
	out_YPos += out_YL;
	Canvas.SetPos(4, out_YPos);

	if ( Pawn == None )
	{
		if ( PlayerReplicationInfo == None )
			Canvas.DrawText("NO PLAYERREPLICATIONINFO", false);
		else
			PlayerReplicationInfo.DisplayDebug(HUD, out_YL, out_YPos);
		out_YPos += out_YL;
		Canvas.SetPos(4, out_YPos);

		super.DisplayDebug(HUD, out_YL, out_YPos);
		return;
	}

	if ( HUD.bShowAIDebug )
	{
		if ( Enemy != None )
			Canvas.DrawText(" STATE: "$GetStateName()$" Timer: "$GetTimerCount()$" Enemy "$Enemy.GetHumanReadableName(), false);
		else
			Canvas.DrawText(" STATE: "$GetStateName()$" Timer: "$GetTimerCount()$" NO Enemy ", false);
		out_YPos += out_YL;
		Canvas.SetPos(4, out_YPos);
	}

	if ( PlayerCamera != None )
		PlayerCamera.DisplayDebug( HUD, out_YL, out_YPos );
}

function Reset()
{
	super.Reset();
	if ( PlayerCamera != None )
	{
		PlayerCamera.Destroy();
	}
}

event ClientReset()
{
	super.ClientReset();
	if ( PlayerCamera != None )
	{
		PlayerCamera.Destroy();
	}
}

exec function SetRadius(float newradius)
{
	Pawn.SetCollisionSize(NewRadius,Pawn.CollisionHeight());
}

/* GetPlayerViewPoint: Returns Player's Point of View
	For the AI this means the Pawn's Eyes ViewPoint
	For a Human player, this means the Camera's ViewPoint */
event GetPlayerViewPoint( out vector POVLocation, out Rotator POVRotation )
{
	if ( Pawn != None )
		Pawn.PawnCalcCamera( 0, POVLocation, POVRotation, FOVAngle );
	else
		super.GetPlayerViewPoint( POVLocation, POVRotation );
}

function ServerChangeTeam( int N )
{
    local TeamInfo OldTeam;

    OldTeam = PlayerReplicationInfo.Team;
    Super.ServerChangeTeam(N);
    Level.Game.ChangeTeam(self, N, true);
    if ( Level.Game.bTeamGame && (PlayerReplicationInfo.Team != OldTeam) )
    {
		if ( (OldTeam != None) && (PlayerReplicationInfo.Team != None)
			&& (PlayerReplicationInfo.Team.Size > OldTeam.Size) )
			UTPlayerReplicationInfo(PlayerReplicationInfo).Adrenaline = 0;
    }
}

function ClientSetHUD(class<HUD> newHUDType, class<Scoreboard> newScoringType)
{
	Super.ClientSetHUD(newHUDType, newScoringType);

	Announcer = spawn(class'UTAnnouncer', self);
	Announcer.PrecacheAnnouncements();
}

function ClientPlayAnnouncement(class<UTLocalMessage> MessageClass, byte MessageIndex)
{
	PlayAnnouncement(MessageClass, MessageIndex);
}

function PlayAnnouncement(class<UTLocalMessage> MessageClass, byte MessageIndex)
{
	// Wait for player to be up to date with replication when joining a server, before stacking up messages
	if ( Level.GRI == None )
		return;

	Announcer.PlayAnnouncement(MessageClass, MessageIndex);
}

function bool AllowVoiceMessage(name MessageType)
{
	if ( Level.NetMode == NM_Standalone )
		return true;
	/* FIXMESTEVE
	if ( Level.TimeSeconds - OldMessageTime < 3 )
	{
		if ( (MessageType == 'TAUNT') || (MessageType == 'AUTOTAUNT') )
			return false;
		if ( Level.TimeSeconds - OldMessageTime < 1 )
			return false;
	}
	if ( Level.TimeSeconds - OldMessageTime < 6 )
		OldMessageTime = Level.TimeSeconds + 3;
	else
		OldMessageTime = Level.TimeSeconds;
	*/
	return true;
}

//=====================================================================
// UT specific implementation of networked player movement functions
//

function CallServerMove
(
	SavedMove NewMove,
    vector ClientLoc,
    bool NewbPendingJumpStatus,
    bool NewbJumpStatus,
    byte ClientRoll,
    int View,
    SavedMove OldMove
)
{
	local bool bCombine;
	local vector InAccel, BuildAccel;
	local byte OldAccelX, OldAccelY, OldAccelZ;

	// compress old move if it exists
	if ( OldMove != None )
	{
		// old move important to replicate redundantly
		BuildAccel = 0.05 * OldMove.Acceleration + vect(0.5, 0.5, 0.5);
		OldAccelX = CompressAccel(BuildAccel.X);
		OldAccelY = CompressAccel(BuildAccel.Y);
		OldAccelZ = CompressAccel(BuildAccel.Z);

		OldServerMove(OldMove.TimeStamp,OldAccelX, OldAccelY, OldAccelZ, OldMove.CompressedFlags());
	}

	InAccel = NewMove.Acceleration * 10;

	if ( PendingMove != None )
	{
		// send two moves simultaneously
		if ( (InAccel == vect(0,0,0))
			&& (PendingMove.StartVelocity == vect(0,0,0))
			&& (NewMove.DoubleClickMove == DCLICK_None)
			&& (PendingMove.Acceleration == vect(0,0,0)) && (PendingMove.DoubleClickMove == DCLICK_None) && !PendingMove.bDoubleJump )
		{
			if ( Pawn == None )
				bCombine = (Velocity == vect(0,0,0));
			else
				bCombine = (Pawn.Velocity == vect(0,0,0));

			if ( bCombine )
			{
				ServerMove
				(
					NewMove.TimeStamp,
					InAccel,
					ClientLoc,
					NewMove.CompressedFlags(),
					ClientRoll,
					View
				);
				return;
			}
		}

		DualServerMove
		(
			PendingMove.TimeStamp,
			PendingMove.Acceleration * 10,
			PendingMove.CompressedFlags(),
			(32767 & (PendingMove.Rotation.Pitch/2)) * 32768 + (32767 & (PendingMove.Rotation.Yaw/2)),
			NewMove.TimeStamp,
			InAccel,
			ClientLoc,
			NewMove.CompressedFlags(),
			ClientRoll,
			View
		);
	}
    else if ( (InAccel == vect(0,0,0)) && (NewMove.DoubleClickMove == DCLICK_None) && !NewMove.bDoubleJump )
    {
		ShortServerMove
		(
			NewMove.TimeStamp,
			ClientLoc,
			NewMove.CompressedFlags(),
			ClientRoll,
			View
		);
    }
    else
		ServerMove
        (
            NewMove.TimeStamp,
            InAccel,
            ClientLoc,
			NewMove.CompressedFlags(),
            ClientRoll,
            View
        );
}

/* ShortServerMove()
compressed version of server move for bandwidth saving
*/
function ShortServerMove
(
	float TimeStamp,
	vector ClientLoc,
	byte NewFlags,
	byte ClientRoll,
	int View
)
{
    ServerMove(TimeStamp,vect(0,0,0),ClientLoc,NewFlags,ClientRoll,View);
}

state RoundEnded
{
ignores SeePlayer, HearNoise, KilledBy, NotifyBump, HitWall, NotifyHeadVolumeChange, NotifyPhysicsVolumeChange, Falling, TakeDamage, Suicide;

	function bool GamePlayEndedState()
	{
		return true;
	}

}

function ViewNextBot()
{
	if ( CheatManager != None )
		CheatManager.ViewBot();
}

state BaseSpectating
{
	exec function SwitchWeapon(byte F){}

	event GetPlayerViewPoint( out vector out_Location, out Rotator out_Rotation )
	{
		if ( PlayerCamera == None )
		{
			out_Location = Location;
			out_Rotation = BlendedTargetViewRotation;
		}
		else
			super.GetPlayerViewPoint(out_Location, out_Rotation);
	}

    function PlayerMove(float DeltaTime)
    {
		if ( (Pawn(ViewTarget) != None) && (Level.NetMode == NM_Client) )
		{
			if ( Pawn(ViewTarget).bSimulateGravity )
				TargetViewRotation.Roll = 0;
			BlendedTargetViewRotation.Pitch = BlendRot(DeltaTime, BlendedTargetViewRotation.Pitch, TargetViewRotation.Pitch & 65535);
			BlendedTargetViewRotation.Yaw = BlendRot(DeltaTime, BlendedTargetViewRotation.Yaw, TargetViewRotation.Yaw & 65535);
			BlendedTargetViewRotation.Roll = BlendRot(DeltaTime, BlendedTargetViewRotation.Roll, TargetViewRotation.Roll & 65535);
		}

		Super.PlayerMove(DeltaTime);
	}
}

function int BlendRot(float DeltaTime, int BlendC, int NewC)
{
	if ( Abs(BlendC - NewC) > 32767 )
	{
		if ( BlendC > NewC )
			NewC += 65536;
		else
			BlendC += 65536;
	}
	if ( Abs(BlendC - NewC) > 4096 )
		BlendC = NewC;
	else
		BlendC = BlendC + (NewC - BlendC) * FMin(1,24 * DeltaTime);

	return (BlendC & 65535);
}

defaultproperties
{
	bEnableDodging=true
    bAlwaysMouseLook=True
	FOVAngle=90.000
    DesiredFOV=90.000000
    DefaultFOV=90.000000
	CameraClass=None
	CheatClass=class'UTCheatManager'
	
    TeamBeaconMaxDist=6000.f
    TeamBeaconPlayerInfoMaxDist=1800.f
    TeamBeaconTeamColors[0]=(R=180,G=0,B=0,A=255)
    TeamBeaconTeamColors[1]=(R=80,G=80,B=200,A=255)
    TeamBeaconCustomColor=(R=255,G=255,B=0,A=255)
    
    bAutoTaunt=false
    bLandingShake=true
}

/* FIXMESTEVE
var bool bWeaponViewShake;
var bool									bAcuteHearing;			// makes playercontroller hear much better (used to magnify hit sounds caused by player)
var bool bMenuBeforeRespawn; //forces the midgame menu to pop up before player can click to respawn
var bool  bSkippedLastUpdate, bLastPressedJump;

var float FOVBias;

var input byte
    bStrafe, bSnapLevel, bLook, bFreeLook, bTurn180, bTurnToNearest, bXAxis, bYAxis;

var input float
    aBaseX, aBaseY, aBaseZ, aMouseX, aMouseY,
    aForward, aTurn, aStrafe, aUp, aLookUp;

// Vehicle Move Replication
var float aLastForward, aLastStrafe, aLastUp, NumServerDrives, NumSkips;

// Vehicle Check Radius
var float   VehicleCheckRadius;         // Radius that is checked for nearby vehicles when pressing use
var bool	bSuccessfulUse;				// gives PC a hint that UsedBy was successful

var float       DesiredZoomLevel;

var Weapon OldClientWeapon;
var int WeaponUpdate;

var Actor	CalcViewActor;		// optimize PlayerCalcView
var vector	CalcViewActorLocation;
var vector	CalcViewLocation;
var rotator	CalcViewRotation;
var float	LastPlayerCalcView;

replication
{
    reliable if( Role==ROLE_Authority )
        ClientReliablePlaySound, ClientSetBehindView,
		ClientSetWeaponViewShake;

	unreliable if( Role<ROLE_Authority )
		SendVoiceMessage;

     unreliable if( Role==ROLE_Authority )
       ClientDamageShake, ClientUpdateFlagHolder,

	   unreliable if( Role==ROLE_Authority )
        ClientHearSound;

	    unreliable if( Role<ROLE_Authority )
        ServerToggleBehindView;

    reliable if( Role<ROLE_Authority )
		ChangeVoiceType, BehindView,
		ServerSpectate, BecomeSpectator, BecomeActivePlayer

	reliable if (ROLE==ROLE_Authority)
    	ResetFOV, ClientBecameSpectator, ClientBecameActivePlayer;
}


simulated function bool BeyondViewDistance(vector OtherLocation, float CullDistance)
{
	local float Dist;

	if ( ViewTarget == None )
		return true;

	Dist = VSize(OtherLocation - ViewTarget.Location);

	if ( (CullDistance > 0) && (CullDistance < Dist * FOVBias) )
		return true;

	return ( Region.Zone.bDistanceFog && (Dist > Region.Zone.DistanceFogEnd) );
}

function ClientSetWeaponViewShake(Bool B)
{
	bWeaponViewShake = B;
}

event ClientSetViewTarget( Actor a )
{
	local bool bNewViewTarget;

	if ( A == None )
	{
		if ( ViewTarget != self )
			SetLocation(CalcViewLocation);
		ServerVerifyViewTarget();
	}
	else
	{
		bNewViewTarget = (ViewTarget != a);
		SetViewTarget( a );
		if ( bNewViewTarget && (Vehicle(Viewtarget) != None) )
			Vehicle(Viewtarget).POVChanged(self, false);
	}
}

event TeamMessage( PlayerReplicationInfo PRI, coerce string S, name Type  )
{
	local string c;

	// Wait for player to be up to date with replication when joining a server, before stacking up messages
	if ( Level.NetMode == NM_DedicatedServer || GameReplicationInfo == None )
		return;

	if( AllowTextToSpeech(PRI, Type) )
		TextToSpeech( S, TextToSpeechVoiceVolume );
	if ( Type == 'TeamSayQuiet' )
		Type = 'TeamSay';

	if ( myHUD != None )
		myHUD.Message( PRI, c$S, Type );

	if ( (Player != None) && (Player.Console != None) )
	{
		if ( PRI!=None )
		{
			if ( PRI.Team!=None && GameReplicationInfo.bTeamGame)
			{
    			if (PRI.Team.TeamIndex==0)
					c = chr(27)$chr(200)$chr(1)$chr(1);
    			else if (PRI.Team.TeamIndex==1)
        			c = chr(27)$chr(125)$chr(200)$chr(253);
			}
			S = PRI.PlayerName$": "$S;
		}
		Player.Console.Chat( c$s, 6.0, PRI );
	}
}

simulated function ClientReliablePlaySound(sound ASound, optional bool bVolumeControl )
{
    ClientPlaySound(ASound, bVolumeControl);
}


function ServerVerifyViewTarget()
{
	if ( ViewTarget == self )
		return;
	if ( ViewTarget == None )
		return;
	ClientSetViewTarget(ViewTarget);
}

exec function SetSensitivity(float F)
{
    PlayerInput.UpdateSensitivity(F);
}

exec function SetMouseSmoothing( int Mode )
{
    PlayerInput.UpdateSmoothing( Mode );
}

exec function SetMouseAccel(float F)
{
	PlayerInput.UpdateAccel(F);
}

function ClientSetBehindView(bool B)
{
    local bool bWasBehindView;

    bWasBehindView = bBehindView;
    bBehindView = B;
    CameraDist = Default.CameraDist;
    if ( (bBehindView != bWasBehindView) && (Vehicle(Viewtarget) != None) )
	    Vehicle(Viewtarget).POVChanged(self, true);

    if (Vehicle(Pawn) != None)
    {
    	Vehicle(Pawn).bDesiredBehindView = B;
    	Pawn.SaveConfig();
    }
}

function ClientVoiceMessage(PlayerReplicationInfo Sender, PlayerReplicationInfo Recipient, name messagetype, byte messageID)
{
    local VoicePack V;

    if ( (Sender == None) || (Sender.voicetype == None) || (Player.Console == None) )
        return;

    V = Spawn(Sender.voicetype, self);
    if ( V != None )
        V.ClientInitialize(Sender, Recipient, messagetype, messageID);
}
function DamageShake(int damage) //send type of damage too!
{
    ClientDamageShake(damage);
}

// function ShakeView( float shaketime, float RollMag, vector OffsetMag, float RollRate, vector OffsetRate, float OffsetTime)

private function ClientDamageShake(int damage)
{
    // todo: add properties!
    ShakeView( Damage * vect(30,0,0),
               120000 * vect(1,0,0),
               0.15 + 0.005 * damage,
               damage * vect(0,0,0.03),
               vect(1,1,1),
               0.2);
}

function WeaponShakeView(vector shRotMag,    vector shRotRate,    float shRotTime,
                   vector shOffsetMag, vector shOffsetRate, float shOffsetTime)
{
	if ( bWeaponViewShake )
		ShakeView(shRotMag * (1.0 - ZoomLevel), shRotRate, shRotTime, shOffsetMag * (1.0 - ZoomLevel), shOffsetRate, shOffsetTime);
}

// ShakeView()
//Call this function to shake the player's view
//shaketime = how long to roll view
//RollMag = how far to roll view as it shakes
//OffsetMag = max view offset
//RollRate = how fast to roll view
//OffsetRate = how fast to offset view
//OffsetTime = how long to offset view (number of shakes)
//
function ShakeView(vector shRotMag,    vector shRotRate,    float shRotTime,
                   vector shOffsetMag, vector shOffsetRate, float shOffsetTime)
{
    if ( VSize(shRotMag) > VSize(ShakeRotMax) )
    {
        ShakeRotMax  = shRotMag;
        ShakeRotRate = shRotRate;
        ShakeRotTime = shRotTime * vect(1,1,1);
    }

    if ( VSize(shOffsetMag) > VSize(ShakeOffsetMax) )
    {
        ShakeOffsetMax  = shOffsetMag;
        ShakeOffsetRate = shOffsetRate;
        ShakeOffsetTime = shOffsetTime * vect(1,1,1);
    }
}

function StopViewShaking()
{
        ShakeRotMax  = vect(0,0,0);
        ShakeRotRate = vect(0,0,0);
        ShakeRotTime = vect(0,0,0);
	ShakeOffsetMax  = vect(0,0,0);
        ShakeOffsetRate = vect(0,0,0);
        ShakeOffsetTime = vect(0,0,0);
}

event ShakeViewEvent(vector shRotMag,    vector shRotRate,    float shRotTime, vector shOffsetMag, vector shOffsetRate, float shOffsetTime)
{
	ShakeView(shRotMag, shRotRate, shRotTime, shOffsetMag, shOffsetRate, shOffsetTime);
}

function NotifyTakeHit(pawn InstigatedBy, vector HitLocation, int Damage, class<DamageType> damageType, vector Momentum)
{
	if ( (instigatedBy != None) && (instigatedBy != pawn) )
		damageAttitudeTo(instigatedBy, Damage);
}

function damageAttitudeTo(pawn Other, float Damage)
{
    if ( (Other != None) && (Other != Pawn) && (Damage > 0) )
        Enemy = Other;
}

function ServerSpeech( name Type, int Index, string Callsign )
{
	// log("Type:"$Type@"Index:"$Index@"Callsign:"$Callsign);
	if(PlayerReplicationInfo.VoiceType != None)
		PlayerReplicationInfo.VoiceType.static.PlayerSpeech( Type, Index, Callsign, Self );
}

exec function ShowMenu()
{
	local bool bCloseHUDScreen;

	if ( MyHUD != None )
	{
		bCloseHUDScreen = MyHUD.bShowScoreboard || MyHUD.bShowLocalStats;
		if ( MyHUD.bShowScoreboard )
			MyHUD.bShowScoreboard = false;
		if ( MyHUD.bShowLocalStats )
			MyHUD.bShowLocalStats = false;
		if ( bCloseHUDScreen )
			return;
	}

	ShowMidGameMenu(true);
}

function ShowMidGameMenu(bool bPause)
{
	// Pause if not already
	if(bPause && Level.Pauser == None)
		SetPause(true);
}

exec function PipedSwitchWeapon(byte F)
{
	if ( (Pawn == None) || (Pawn.PendingWeapon != None) )
		return;

	SwitchWeapon(F);
}

// The player wants to switch to weapon group number F.
exec function SwitchWeapon(byte F)
{
	if ( Pawn != None )
		Pawn.SwitchWeapon(F);
}

exec function GetWeapon(class<Weapon> NewWeaponClass )
{
    local Inventory Inv;
    local int Count;

    if ( (Pawn == None) || (Pawn.Inventory == None) || (NewWeaponClass == None) )
        return;

    if ( (Pawn.Weapon != None) && (Pawn.Weapon.Class == NewWeaponClass) && (Pawn.PendingWeapon == None) )
    {
        Pawn.Weapon.Reselect();
        return;
    }

    if ( Pawn.PendingWeapon != None && Pawn.PendingWeapon.bForceSwitch )
        return;

    for ( Inv=Pawn.Inventory; Inv!=None; Inv=Inv.Inventory )
    {
        if ( Inv.Class == NewWeaponClass )
        {
            Pawn.PendingWeapon = Weapon(Inv);
            if ( !Pawn.PendingWeapon.HasAmmo() )
            {
                ClientMessage( Pawn.PendingWeapon.ItemName$Pawn.PendingWeapon.MessageNoAmmo );
                Pawn.PendingWeapon = None;
                return;
            }
            Pawn.Weapon.PutDown();
            return;
        }
		Count++;
		if ( Count > 1000 )
			return;
    }
}

// The player wants to fire.
exec function Fire( optional float F )
{
	if ( Level.NetMode == NM_StandAlone && bViewingMatineeCinematic )
	{
		Level.Game.SceneAbort();
	}

    if ( Level.Pauser == PlayerReplicationInfo )
    {
        SetPause(false);
        return;
    }
	if( Pawn == None )
		return;

	if ( (Pawn.Weapon != None) && Pawn.Weapon.bDebugging )
		log("PlayerController fire");
    Pawn.Fire(F);
}

// The player wants to alternate-fire.
exec function AltFire( optional float F )
{
    if ( Level.Pauser == PlayerReplicationInfo )
    {
        SetPause(false);
        return;
    }
	if( Pawn == None )
		return;
    Pawn.AltFire(F);
}

function ServerUse()
{
    local Actor A;
	local Vehicle DrivenVehicle, EntryVehicle, V;

	if ( Role < ROLE_Authority )
		return;

    if ( Level.Pauser == PlayerReplicationInfo )
    {
        SetPause(false);
        return;
    }

    if (Pawn == None || !Pawn.bCanUse)
        return;

	DrivenVehicle = Vehicle(Pawn);
	if( DrivenVehicle != None )
	{
		DrivenVehicle.KDriverLeave(false);
		return;
	}

    // Check for nearby vehicles
    ForEach Pawn.VisibleCollidingActors(class'Vehicle', V, VehicleCheckRadius)
    {
        // Found a vehicle within radius
        EntryVehicle = V.FindEntryVehicle(Pawn);
        if (EntryVehicle != None && EntryVehicle.TryToDrive(Pawn))
            return;
    }

    // Send the 'DoUse' event to each actor player is touching.
    ForEach Pawn.TouchingActors(class'Actor', A)
        A.UsedBy(Pawn);

	if ( Pawn.Base != None )
		Pawn.Base.UsedBy( Pawn );
}

exec function SetVoice( coerce string S )
{
	if ( Level.NetMode == NM_StandAlone )
	{
		if ( PlayerReplicationInfo != None )
			PlayerReplicationInfo.SetCharacterVoice(S);
	}

	else ChangeVoiceType(S);
	UpdateURL("Voice", S, True);
}

function ChangeVoiceType(string NewVoiceType)
{
	if ( VoiceChangeLimit > Level.TimeSeconds )
		return;

	VoiceChangeLimit = Level.TimeSeconds + 10.0;	// TODO - probably better to hook this up to the same limit system that playernames use
	if ( NewVoiceType != "" && PlayerReplicationInfo != None )
		PlayerReplicationInfo.SetCharacterVoice(NewVoiceType);
}

exec function BehindView( Bool B )
{
	if ( Level.NetMode == NM_Standalone || Vehicle(Pawn) != None || PlayerReplicationInfo.bOnlySpectator || PlayerReplicationInfo.bAdmin || IsA('Admin') )
	{
		if ( (Vehicle(Pawn)==None) || (Vehicle(Pawn).bAllowViewChange) )	// Allow vehicles to limit view changes
		{
			ClientSetBehindView(B);
			bBehindView = B;
		}
	}
}

exec function ToggleBehindView()
{
	ServerToggleBehindview();
}

function ServerToggleBehindView()
{
	local bool B;

	if ( Level.NetMode == NM_Standalone || Vehicle(Pawn) != None || PlayerReplicationInfo.bOnlySpectator || PlayerReplicationInfo.bAdmin || IsA('Admin') )
	{
		if ( (Vehicle(Pawn)==None) || (Vehicle(Pawn).bAllowViewChange) )	// Allow vehicles to limit view changes
		{
			B = !bBehindView;
			ClientSetBehindView(B);
			bBehindView = B;
		}
	}
}

function CalcBehindView(out vector CameraLocation, out rotator CameraRotation, float Dist)
{
    local vector View,HitLocation,HitNormal;
    local float ViewDist,RealDist;
    local vector globalX,globalY,globalZ;
    local vector localX,localY,localZ;

    CameraRotation = Rotation;
    CameraRotation.Roll = 0;
	CameraLocation.Z += 12;

    View = vect(1,0,0) >> CameraRotation;

    // add view radius offset to camera location and move viewpoint up from origin (amb)
    RealDist = Dist;

    if( Trace( HitLocation, HitNormal, CameraLocation - Dist * vector(CameraRotation), CameraLocation,false,vect(10,10,10) ) != None )
        ViewDist = FMin( (CameraLocation - HitLocation) Dot View, Dist );
    else
        ViewDist = Dist;

    if ( !bBlockCloseCamera || !bValidBehindCamera || (ViewDist > 10 + FMax(ViewTarget.CollisionRadius, ViewTarget.CollisionHeight)) )
	{
		//Log("Update Cam ");
		bValidBehindCamera = true;
		OldCameraLoc = CameraLocation - ViewDist * View;
		OldCameraRot = CameraRotation;
	}
	else
	{
		//Log("Dont Update Cam "$bBlockCloseCamera@bValidBehindCamera@ViewDist);
		SetRotation(OldCameraRot);
	}

    CameraLocation = OldCameraLoc;
    CameraRotation = OldCameraRot;
}

function CalcFirstPersonView( out vector CameraLocation, out rotator CameraRotation )
{
    local vector x, y, z;
	local float FalloffScaling;

    GetAxes(Rotation, x, y, z);

    // First-person view.
    CameraRotation = Normalize(Rotation + ShakeRot); 
    CameraLocation = CameraLocation + Pawn.EyePosition() + Pawn.WalkBob +
                     ShakeOffset.X * x +
                     ShakeOffset.Y * y +
                     ShakeOffset.Z * z;
}

event AddCameraEffect(CameraEffect NewEffect,optional bool RemoveExisting)
{
    if(RemoveExisting)
        RemoveCameraEffect(NewEffect);

    CameraEffects.Length = CameraEffects.Length + 1;
    CameraEffects[CameraEffects.Length - 1] = NewEffect;
}

event RemoveCameraEffect(CameraEffect ExEffect)
{
    local int   EffectIndex;

    for(EffectIndex = 0;EffectIndex < CameraEffects.Length;EffectIndex++)
        if(CameraEffects[EffectIndex] == ExEffect)
        {
            CameraEffects.Remove(EffectIndex,1);
            return;
        }
}

exec function CreateCameraEffect(class<CameraEffect> EffectClass)
{
    AddCameraEffect(new EffectClass);
}

simulated function rotator GetViewRotation()
{
    if ( bBehindView && (Pawn != None) )
        return Pawn.Rotation;
    return Rotation;
}

function CacheCalcView(actor ViewActor, vector CameraLocation, rotator CameraRotation)
{
	CalcViewActor		= ViewActor;
	if (ViewActor != None)
		CalcViewActorLocation = ViewActor.Location;
	CalcViewLocation	= CameraLocation;
	CalcViewRotation	= CameraRotation;
	LastPlayerCalcView	= Level.TimeSeconds;
}

event PlayerCalcView(out actor ViewActor, out vector CameraLocation, out rotator CameraRotation )
{
    local Pawn PTarget;

	if ( LastPlayerCalcView == Level.TimeSeconds && CalcViewActor != None && CalcViewActor.Location == CalcViewActorLocation )
	{
		ViewActor	= CalcViewActor;
		CameraLocation	= CalcViewLocation;
		CameraRotation	= CalcViewRotation;
		return;
	}

	// If desired, call the pawn's own special callview
	if( Pawn != None && Pawn.bSpecialCalcView && (ViewTarget == Pawn) )
	{
		// try the 'special' calcview. This may return false if its not applicable, and we do the usual.
		if ( Pawn.SpecialCalcView(ViewActor, CameraLocation, CameraRotation) )
		{
			CacheCalcView(ViewActor,CameraLocation,CameraRotation);
			return;
		}
	}

    if ( (ViewTarget == None) || ViewTarget.bDeleteMe )
    {
        if ( (Pawn != None) && !Pawn.bDeleteMe )
            SetViewTarget(Pawn);
        else if ( RealViewTarget != None )
            SetViewTarget(RealViewTarget);
        else
            SetViewTarget(self);
    }

    ViewActor = ViewTarget;
    CameraLocation = ViewTarget.Location;

    if ( ViewTarget == Pawn )
    {
        if( bBehindView ) //up and behind
            CalcBehindView(CameraLocation, CameraRotation, CameraDist * Pawn.Default.CollisionRadius);
        else
            CalcFirstPersonView( CameraLocation, CameraRotation );

		CacheCalcView(ViewActor,CameraLocation,CameraRotation);
        return;
    }
    if ( ViewTarget == self )
    {
        if ( bCameraPositionLocked )
            CameraRotation = CheatManager.LockedRotation;
        else
            CameraRotation = Rotation;

		CacheCalcView(ViewActor,CameraLocation,CameraRotation);
        return;
    }

    if ( ViewTarget.IsA('Projectile') && !bBehindView )
    {
        CameraLocation += (ViewTarget.CollisionHeight) * vect(0,0,1);
        CameraRotation = Rotation;

		CacheCalcView(ViewActor,CameraLocation,CameraRotation);
        return;
    }

    CameraRotation = ViewTarget.Rotation;
    PTarget = Pawn(ViewTarget);
    if ( PTarget != None )
    {
        if ( (Level.NetMode == NM_Client) || (Level.NetMode != NM_Standalone) )
        {
            PTarget.SetViewRotation(TargetViewRotation);
            CameraRotation = BlendedTargetViewRotation;

            PTarget.EyeHeight = TargetEyeHeight;
        }
        else if ( PTarget.IsPlayerPawn() )
            CameraRotation = PTarget.GetViewRotation();

		if (PTarget.bSpecialCalcView && PTarget.SpectatorSpecialCalcView(self, ViewActor, CameraLocation, CameraRotation))
		{
			CacheCalcView(ViewActor, CameraLocation, CameraRotation);
			return;
		}

        if ( !bBehindView )
            CameraLocation += PTarget.EyePosition();
    }
    if ( bBehindView )
    {
        CameraLocation = CameraLocation + (ViewTarget.Default.CollisionHeight - ViewTarget.CollisionHeight) * vect(0,0,1);
        CalcBehindView(CameraLocation, CameraRotation, CameraDist * ViewTarget.Default.CollisionRadius);
    }

	CacheCalcView(ViewActor,CameraLocation,CameraRotation);
}

function LoadPlayers()
{
	local int i;

	if ( GameReplicationInfo == None )
		return;

	for ( i=0; i<GameReplicationInfo.PRIArray.Length; i++ )
		GameReplicationInfo.PRIArray[i].UpdatePrecacheMaterials();
}


//active player wants to become a spectator
function BecomeSpectator()
{
	if (Role < ROLE_Authority)
		return;

	if ( !Level.Game.BecomeSpectator(self) )
		return;

	if ( Pawn != None )
		Pawn.Died(self, class'DamageType', Pawn.Location);

	if ( PlayerReplicationInfo.Team != None )
		PlayerReplicationInfo.Team.RemoveFromTeam(self);
	PlayerReplicationInfo.Team = None;
	PlayerReplicationInfo.Score = 0;
	PlayerReplicationInfo.Deaths = 0;
	PlayerReplicationInfo.GoalsScored = 0;
	PlayerReplicationInfo.Kills = 0;
	ServerSpectate();
	BroadcastLocalizedMessage(Level.Game.GameMessageClass, 14, PlayerReplicationInfo);

	ClientBecameSpectator();
}

function ClientBecameSpectator()
{
	UpdateURL("SpectatorOnly", "1", true);
}

//spectating player wants to become active and join the game
function BecomeActivePlayer()
{
	if (Role < ROLE_Authority)
		return;

	if ( !Level.Game.AllowBecomeActivePlayer(self) )
		return;

	ResetCameraMode();
	FixFOV();
	ServerViewSelf();
	PlayerReplicationInfo.bOnlySpectator = false;
	Level.Game.NumSpectators--;
	Level.Game.NumPlayers++;
	PlayerReplicationInfo.Reset();
	BroadcastLocalizedMessage(Level.Game.GameMessageClass, 1, PlayerReplicationInfo);
	if (Level.Game.bTeamGame)
		Level.Game.ChangeTeam(self, Level.Game.PickTeam(int(GetURLOption("Team")), None), false);
	if (!Level.Game.bDelayedStart)
    {
		// start match, or let player enter, immediately
		Level.Game.bRestartLevel = false;  // let player spawn once in levels that must be restarted after every death
		if (Level.Game.bWaitingToStartMatch)
			Level.Game.StartMatch();
		else
			Level.Game.RestartPlayer(PlayerController(Owner));
		Level.Game.bRestartLevel = Level.Game.Default.bRestartLevel;
    }
    else
        GotoState('PlayerWaiting');

    ClientBecameActivePlayer();
}

function ClientBecameActivePlayer()
{
	UpdateURL("SpectatorOnly","",true);
}


state RoundEnded
{
ignores SeePlayer, HearNoise, KilledBy, NotifyBump, HitWall, NotifyHeadVolumeChange, NotifyPhysicsVolumeChange, Falling, TakeDamage, Suicide;

	function ServerReStartPlayer()
	{
	}

	function bool IsSpectating() {	return true; }

    exec function Use() {}
    exec function SwitchWeapon(byte T) {}
    exec function ThrowWeapon() {}
    exec function Fire(optional float F) {}
    exec function AltFire(optional float F) {}

    function Possess(Pawn aPawn)
    {
    	Global.Possess(aPawn);

    	if (Pawn != None)
    		Pawn.TurnOff();
    }

    function PlayerMove(float DeltaTime)
    {
        local vector X,Y,Z;
        local Rotator ViewRotation;

        GetAxes(Rotation,X,Y,Z);
        // Update view rotation.

        if ( !bFixedCamera )
        {
            ViewRotation = Rotation;
            ViewRotation.Yaw += 32.0 * DeltaTime * aTurn;
            ViewRotation.Pitch += 32.0 * DeltaTime * aLookUp;
            if (Pawn != None)
	            ViewRotation.Pitch = Pawn.LimitPitch(ViewRotation.Pitch);
            SetRotation(ViewRotation);
        }
        else if ( ViewTarget != None )
            SetRotation( ViewTarget.Rotation );

        ViewShake( DeltaTime );
        ViewFlash( DeltaTime );

        if ( Role < ROLE_Authority ) // then save this move and replicate it
            ReplicateMove(DeltaTime, vect(0,0,0), DCLICK_None, rot(0,0,0));
        else
            ProcessMove(DeltaTime, vect(0,0,0), DCLICK_None, rot(0,0,0));
        bPressedJump = false;
    }

    function ServerMove
    (
        float TimeStamp,
        vector InAccel,
        vector ClientLoc,
        byte NewFlags,
        byte ClientRoll,
        int View
    )
    {
        Global.ServerMove(TimeStamp, InAccel, ClientLoc, NewFlags, ClientRoll, (32767 & (Rotation.Pitch/2)) * 32768 + (32767 & (Rotation.Yaw/2)) );

    }

    function FindGoodView()
    {
        local vector	cameraLoc;
        local rotator	cameraRot, ViewRotation;
        local int		tries, besttry;
        local float		bestdist, newdist;
        local int		startYaw;
        local actor		ViewActor;

        ViewRotation = Rotation;
        ViewRotation.Pitch = 56000;
        tries = 0;
        besttry = 0;
        bestdist = 0.0;
        startYaw = ViewRotation.Yaw;

        for (tries=0; tries<16; tries++)
        {
            cameraLoc = ViewTarget.Location;
			SetRotation(ViewRotation);
            PlayerCalcView(ViewActor, cameraLoc, cameraRot);
            newdist = VSize(cameraLoc - ViewTarget.Location);
            if ( newdist > bestdist )
            {
                bestdist = newdist;
                besttry = tries;
            }
            ViewRotation.Yaw += 4096;
        }

        ViewRotation.Yaw = startYaw + besttry * 4096;
        SetRotation(ViewRotation);
    }

    function Timer()
    {
        bFrozen = false;
    }

    function LongClientAdjustPosition
    (
        float TimeStamp,
        name newState,
        EPhysics newPhysics,
        float NewLocX,
        float NewLocY,
        float NewLocZ,
        float NewVelX,
        float NewVelY,
        float NewVelZ,
        Actor NewBase,
        float NewFloorX,
        float NewFloorY,
        float NewFloorZ
    )
    {
		if ( newState == 'PlayerWaiting' )
			GotoState( newState );
    }

    function BeginState()
    {
        local Pawn P;

        EndZoom();
		CameraDist = Default.CameraDist;
        FOVAngle = DesiredFOV;
        bFire = 0;
        bAltFire = 0;

        if ( Pawn != None )
        {
       	    if ( Vehicle(Pawn) != None )
	    		Pawn.StopWeaponFiring();

			Pawn.TurnOff();
			Pawn.bSpecialHUD = false;
            if ( Pawn.Weapon != None )
			{
				Pawn.Weapon.StopFire(0);
				Pawn.Weapon.StopFire(1);
				Pawn.Weapon.bEndOfRound = true;
			}
        }

        bFrozen = true;
		bBehindView = true;
        if ( !bFixedCamera )
            FindGoodView();

        SetTimer(5, false);
        ForEach DynamicActors(class'Pawn', P)
        {
			if ( P.Role == ROLE_Authority )
				P.RemoteRole = ROLE_SimulatedProxy;
			P.TurnOff();
        }
		StopForceFeedback();
    }

	function CalcBehindView(out vector CameraLocation, out rotator CameraRotation, float Dist)
	{
		local vector	View;
		local float		ViewDist,RealDist;
		local vector	globalX,globalY,globalZ;
		local vector	localX,localY,localZ;
		local vector	HitLocation,HitNormal;
		local Actor		HitActor;

		CameraRotation = Rotation;
		CameraRotation.Roll = 0;
		CameraLocation.Z += 12;

		View = vect(1,0,0) >> CameraRotation;

		// add view radius offset to camera location and move viewpoint up from origin (amb)
		RealDist = Dist;

		HitActor = Trace( HitLocation, HitNormal, CameraLocation - Dist * vector(CameraRotation), CameraLocation,false,vect(10,10,10));
		if ( HitActor != None && !HitActor.IsA('BlockingVolume') )
			ViewDist = FMin( (CameraLocation - HitLocation) Dot View, Dist );
		else
			ViewDist = Dist;

		if ( !bBlockCloseCamera || !bValidBehindCamera || (ViewDist > 10 + FMax(ViewTarget.CollisionRadius, ViewTarget.CollisionHeight)) )
		{
			//Log("Update Cam ");
			bValidBehindCamera = true;
			OldCameraLoc = CameraLocation - ViewDist * View;
			OldCameraRot = CameraRotation;
		}
		else
		{
			//Log("Dont Update Cam "$bBlockCloseCamera@bValidBehindCamera@ViewDist);
			SetRotation(OldCameraRot);
		}

		CameraLocation = OldCameraLoc;
		CameraRotation = OldCameraRot;
	}

Begin:
}

//------------------------------------------------------------------------------
// Control options
function ChangeStairLook( bool B )
{
    bLookUpStairs = B;
    if ( bLookUpStairs )
        bAlwaysMouseLook = false;
}

function ChangeAlwaysMouseLook(Bool B)
{
    bAlwaysMouseLook = B;
    if ( bAlwaysMouseLook )
        bLookUpStairs = false;
}

singular event UnPressButtons()
{
	bFire = 0;
	bAltFire = 0;
	bDuck = 0;
	bRun = 0;
	bVoiceTalk = 0;
	ResetInput();
}

simulated function bool IsMouseInverted()
{
	return PlayerInput.bInvertMouse;
}

exec function InvertMouse(optional string Invert)
{
	PlayerInput.InvertMouse(Invert);
}

exec function InvertLook()
{
    local bool result;

    result = PlayerInput.InvertLook();

    if (IsOnConsole())
    {
        class'XBoxPlayerInput'.default.bInvertVLook = result;
        class'XBoxPlayerInput'.static.StaticSaveConfig();
    }
}

function ViewShake(float DeltaTime)
{
    if ( ShakeOffsetRate != vect(0,0,0) )
    {
        // modify shake offset
        ShakeOffset.X += DeltaTime * ShakeOffsetRate.X;
		CheckShake(MaxShakeOffset.X, ShakeOffset.X, ShakeOffsetRate.X, ShakeOffsetTime.X, DeltaTime);

        ShakeOffset.Y += DeltaTime * ShakeOffsetRate.Y;
		CheckShake(MaxShakeOffset.Y, ShakeOffset.Y, ShakeOffsetRate.Y, ShakeOffsetTime.Y, DeltaTime);

        ShakeOffset.Z += DeltaTime * ShakeOffsetRate.Z;
		CheckShake(MaxShakeOffset.Z, ShakeOffset.Z, ShakeOffsetRate.Z, ShakeOffsetTime.Z, DeltaTime);
    }

    if ( ShakeRotRate != vect(0,0,0) )
    {
        UpdateShakeRotComponent(ShakeRotMax.X, ShakeRot.Pitch, ShakeRotRate.X, ShakeRotTime.X, DeltaTime);
        UpdateShakeRotComponent(ShakeRotMax.Y, ShakeRot.Yaw,   ShakeRotRate.Y, ShakeRotTime.Y, DeltaTime);
        UpdateShakeRotComponent(ShakeRotMax.Z, ShakeRot.Roll,  ShakeRotRate.Z, ShakeRotTime.Z, DeltaTime);
    }
}

function UpdateShakeRotComponent(out float max, out int current, out float rate, out float time, float dt)
{
    local float fCurrent;

    current = ((current & 65535) + rate * dt) & 65535;
    if ( current > 32768 )
    current -= 65536;

    fCurrent = current;
    CheckShake(max, fCurrent, rate, time, dt);
    current = fCurrent;
}

function ServerViewNextPlayer()
{
    local Controller C, Pick;
    local bool bFound, bRealSpec, bWasSpec;

    bRealSpec = PlayerReplicationInfo.bOnlySpectator;
    bWasSpec = !bBehindView && (ViewTarget != Pawn) && (ViewTarget != self);
    PlayerReplicationInfo.bOnlySpectator = true;

    // view next player
    for ( C=Level.ControllerList; C!=None; C=C.NextController )
    {
        if ( Level.Game.CanSpectate(self,bRealSpec,C) )
        {
            if ( Pick == None )
                Pick = C;
            if ( bFound )
            {
                Pick = C;
                break;
            }
            else
                bFound = ( (RealViewTarget == C) || (ViewTarget == C) );
        }
    }
    SetViewTarget(Pick);
    ClientSetViewTarget(Pick);
    if ( (ViewTarget == self) || bWasSpec )
        bBehindView = false;
    else
        bBehindView = true; //bChaseCam;
    ClientSetBehindView(bBehindView);
    PlayerReplicationInfo.bOnlySpectator = bRealSpec;
}

// FIXMESTEVE - move to UTPRI so UTBot and UTPC can share
function SendVoiceMessage(PlayerReplicationInfo Sender, PlayerReplicationInfo Recipient, name messagetype, byte messageID, name broadcasttype)
{
	local Controller P;

	if ( ((Recipient == None) || (AIController(self) == None))
		&& !AllowVoiceMessage(MessageType) )
		return;

	for ( P=Level.ControllerList; P!=None; P=P.NextController )
	{
		if ( PlayerController(P) != None )
		{
			if ((P.PlayerReplicationInfo == Sender) ||
				(P.PlayerReplicationInfo == Recipient &&
				 (Level.Game.BroadcastHandler == None ||
				  Level.Game.BroadcastHandler.AcceptBroadcastSpeech(PlayerController(P), Sender)))
				)
				P.ClientVoiceMessage(Sender, Recipient, messagetype, messageID);
			else if ( (Recipient == None) || (Level.NetMode == NM_Standalone) )
			{
				if ( (broadcasttype == 'GLOBAL') || !Level.Game.bTeamGame || (Sender.Team == P.PlayerReplicationInfo.Team) )
					if ( Level.Game.BroadcastHandler == None || Level.Game.BroadcastHandler.AcceptBroadcastSpeech(PlayerController(P), Sender) )
						P.ClientVoiceMessage(Sender, Recipient, messagetype, messageID);
			}
		}
		else if ( (messagetype == 'ORDER') && ((Recipient == None) || (Recipient == P.PlayerReplicationInfo)) )
			P.BotVoiceMessage(messagetype, messageID, self);
	}
}

//defaultproperties
{
//	FOVBias=+1.0
//    VehicleCheckRadius=700.0
//     bWeaponViewShake=true
}
*/






