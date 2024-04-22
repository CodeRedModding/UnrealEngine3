/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_ControlMovieTexture extends SequenceAction;

enum EMovieControlType
{
	MCT_Play,
	MCT_Stop,
	MCT_Pause,
};

var() TextureMovie MovieTexture;

event Activated()
{
	local PlayerController PC;
	local EMovieControlType Mode;

	if (MovieTexture != None)
	{
		// determine the appropriate action
		if (InputLinks[0].bHasImpulse)
		{
			Mode = MCT_Play;
		}
		else if (InputLinks[1].bHasImpulse)
		{
			Mode = MCT_Stop;
		}
		else if (InputLinks[2].bHasImpulse)
		{
			Mode = MCT_Pause;
		}

		// notify all players, making sure to send only one message per unique machine
		foreach GetWorldInfo().AllControllers(class'PlayerController', PC)
		{
			if ( (LocalPlayer(PC.Player) != None && PC.IsPrimaryPlayer()) ||
				(NetConnection(PC.Player) != None && ChildConnection(PC.Player) == None) )
			{
				PC.ClientControlMovieTexture(MovieTexture, Mode);
			}
		}
	}
}

defaultproperties
{
	ObjCategory="Cinematic"
	ObjName="Control Movie Texture"
	bCallHandler=false
	InputLinks(0)=(LinkDesc="Play")
	InputLinks(1)=(LinkDesc="Stop")
	InputLinks(2)=(LinkDesc="Pause")
	VariableLinks.Empty()
}
