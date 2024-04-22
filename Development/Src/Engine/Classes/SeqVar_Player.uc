/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqVar_Player extends SeqVar_Object
	native(Sequence);

cpptext
{
	UObject** GetObjectRef( INT Idx );

	virtual FString GetValueStr()
	{
		if (!bAllPlayers)
		{
			return FString::Printf(TEXT("Player %d"),PlayerIdx);
		}
		else
		{
			return FString(TEXT("All Players"));
		}
	}

	virtual UBOOL SupportsProperty(UProperty *Property)
	{
		return FALSE;
	}
};

/** Local list of players in the game */
var transient array<Object> Players;

/** Return all player references? */
var() bool bAllPlayers;

/** Individual player selection for multiplayer scripting */
var() int PlayerIdx;

/** updates the Players array with the list of Controllers in the game that count as players (humans or bot-players) */
native final function UpdatePlayersList();

function Object GetObjectValue()
{
	local Controller C;

	UpdatePlayersList();

	if (Players.length > 0)
	{
		if (bAllPlayers || PlayerIdx < 0 || PlayerIdx >= Players.length)
		{
			C = Controller(Players[0]);
		}
		else
		{
			C = Controller(Players[PlayerIdx]);
		}
	}

	// we usually want the pawn, so return that if possible
	return (C != None && C.Pawn != None) ? C.Pawn : C;
}

defaultproperties
{
	ObjName="Player"
	ObjCategory="Player"
	bAllPlayers=TRUE
	SupportedClasses=(class'Controller',class'Pawn')
}
