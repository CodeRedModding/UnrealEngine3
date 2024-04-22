/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/** Used to determine how many players are currently logged in */
class SeqCond_IsLoggedIn extends SequenceCondition
	native(Sequence);

cpptext
{
	virtual void Activated()
	{
		USequenceOp::Activated();

		// Trigger the output based upon meeting the num logged in criteria
		if (eventCheckLogins() == TRUE)
		{
			OutputLinks(0).bHasImpulse = TRUE;
		}
		else
		{
			OutputLinks(1).bHasImpulse = TRUE;
		}
	}
}

/** The number of users that need to be logged in for it to activate as true */
var() int NumNeededLoggedIn;

/**
 * Checks with the OnlineSubsystem to determine if there are enough people logged in
 */
event bool CheckLogins()
{
	local int LoggedInCount;
	local int Count;
	local OnlineSubsystem OnlineSub;
	local OnlinePlayerInterface PlayerInt;

	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None)
	{
		PlayerInt = OnlineSub.PlayerInterface;
		if (PlayerInt != None)
		{
			// Check for the number they need
			for (Count = 0; Count < NumNeededLoggedIn; Count++)
			{
				// Local or online login is fine
				if (PlayerInt.GetLoginStatus(Count) >= LS_UsingLocalProfile)
				{
					LoggedInCount++;
				}
			}
		}
	}
	return LoggedInCount >= NumNeededLoggedIn;
}

defaultproperties
{
	ObjName="Is Logged In"

	OutputLinks(0)=(LinkDesc="True")
	OutputLinks(1)=(LinkDesc="False")
	VariableLinks(0)=(ExpectedType=class'SeqVar_Int',LinkDesc="NeededLoggedIn",PropertyName="NumNeededLoggedIn")
}
