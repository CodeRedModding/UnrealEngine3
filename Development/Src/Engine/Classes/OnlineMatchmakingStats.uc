/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Holds the matchmaking stats when searching for sessions. 
 * These can be converted to XML and uploaded to MCP.
 */
class OnlineMatchmakingStats extends Object
	native;

struct native MMStats_Timer
{
	var bool bInProgress;
	var double MSecs;
};

native function StartTimer(out MMStats_Timer Timer);
native function StopTimer(out MMStats_Timer Timer);

cpptext
{
	virtual void ToXML(FString& OutXmlStr,const FUniqueNetId& UniqueId,const FString& XmlPlatformStr,UBOOL bShouldIndent)
	{
		check(0 && "not implemented");
	}
}