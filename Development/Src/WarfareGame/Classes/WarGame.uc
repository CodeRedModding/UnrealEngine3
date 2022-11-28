/**
 * WarGame
 * Warfare Game Info
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class WarGame extends GameInfo
	config(Game)
	abstract;

/** @see GameInfo.Login */
event PlayerController Login
(
		string	Portal,
		string	Options,
	out string	Error
)
{
	local String	PCOverride;

	log("WarGame::Login");
	PCOverride = ParseOption ( Options, "PCClass" );
	if( PCOverride != "" )
	{
		log("WarGame::Login PCOverride" @ PCOverride );
		PlayerControllerClass = class<PlayerController>(DynamicLoadObject(PCOverride, class'Class'));
	}

	return super.Login( Portal, Options, Error );
}

/** Toggles between 2 player control schemes for testing */
exec function ToggleControlScheme()
{
	local PlayerController	LocalPlayer;
	local string			CurrentClassName, NewPCClassName;
	local string			CommandLine;

	ForEach LocalPlayerControllers(LocalPlayer)
		break;

	if( LocalPlayer != None )
	{
		CurrentClassName = string(LocalPlayer.Class);
		if( CurrentClassName ~= "WarfareGame.PC_MSScheme" )
		{
			NewPCClassName = "WarfareGame.PC_DefaultScheme";
		}
		else if( CurrentClassName ~=  "WarfareGame.PC_DefaultScheme" )
		{
			NewPCClassName = "WarfareGame.PC_AltScheme";
		}
		else if( CurrentClassName ~= "WarfareGame.PC_AltScheme" )
		{
			NewPCClassName = "WarfareGame.PC_MSScheme";
		}

		log("new playercontroller spawn class is:" @ NewPCClassName );
		CommandLine = "open" @ Level.GetLocalURL() $"?PCClass=" $ NewPCClassName;
		log("CommandLine:" @ CommandLine );
		LocalPlayer.ConsoleCommand( CommandLine );
	}
}

defaultproperties
{
	HUDType="WarfareGame.WarHUD"
	ScoreBoardType="WarfareGame.WarScoreboard"
	PlayerReplicationInfoClass=class'WarfareGame.WarPRI'
	PlayerControllerClassName="WarfareGame.PC_MSScheme"
	DefaultPawnClassName="WarfareGame.Pawn_COGGear"
	bRestartLevel=false
	bDelayedStart=true
}