/**
* MobileMenuGame
* A replacement game type that pops up a menu
*
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

class MobileMenuGame extends GameInfo;

var class<MobileMenuScene> InitialSceneToDisplayClass;

/**
 * We override PostLogin and display the scene directly after the login process is finished.                                                                     
 */
event PostLogin( PlayerController NewPlayer )
{
	local MobilePlayerInput MI;
	
	Super.PostLogin(NewPlayer);

	`log("" $ Class $"::PostLogin" @ InitialSceneToDisplayClass);

	if (InitialSceneToDisplayClass != none)
	{
		MI = MobilePlayerInput(NewPlayer.PlayerInput);
		if (MI != none)
		{
			MI.OpenMenuScene(InitialSceneToDisplayClass);
		}
		else
		{
			`Log("MobileMenuGame.Login - Could not find a MobilePlayerInput to open the scene!");
		}
	}
	else
	{
		`Log("MobileMenuGame.Login - No scene to open");
	}

}

/**
 * Never start a match in the menus
 */
function StartMatch()
{
}

/**
 * Never restart a player in the menus                                                                     
 */
function RestartPlayer(Controller NewPlayer)
{
}


defaultproperties
{
	PlayerControllerClass=class'MobileMenuPlayerController'
	HUDType=class'MobileHud'
}

