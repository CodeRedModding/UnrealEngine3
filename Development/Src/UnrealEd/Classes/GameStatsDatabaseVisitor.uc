/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*
* Interface for visiting game stat database entries, must implement all database entry types
*/
interface GameStatsDatabaseVisitor
	native(GameStats);

cpptext
{
	/** Called before the visiting begins */
	virtual void BeginVisiting() = 0;

	/** Called after the visiting is over */
	virtual UBOOL EndVisiting() = 0;

	/**
	 * Abstract functions that defines a visitor's behavior when given a GameStatEntry 
	 * There must be an entry for each possible concrete datatype stored in the database
	 * or else no visit action will occur.
	 */
	virtual void Visit(class GameStringEntry* Entry) = 0; 
	virtual void Visit(class GameIntEntry* Entry) = 0;
	virtual void Visit(class GameFloatEntry* Entry) = 0;
	virtual void Visit(class GamePositionEntry* Entry) = 0;
	virtual void Visit(class TeamIntEntry* Entry) = 0;
	virtual void Visit(class TeamStringEntry* Entry) = 0; 
	virtual void Visit(class TeamFloatEntry* Entry) = 0; 
	virtual void Visit(class PlayerStringEntry* Entry) = 0;
	virtual void Visit(class PlayerIntEntry* Entry) = 0; 
	virtual void Visit(class PlayerFloatEntry* Entry) = 0; 
	virtual void Visit(class PlayerLoginEntry* Entry) = 0;
	virtual void Visit(class PlayerSpawnEntry* Entry) = 0;
	virtual void Visit(class PlayerKillDeathEntry* Entry) = 0;
	virtual void Visit(class PlayerPlayerEntry * Entry) = 0;
	virtual void Visit(class WeaponEntry* Entry) = 0;
	virtual void Visit(class DamageEntry* Entry) = 0; 
	virtual void Visit(class ProjectileIntEntry* Entry) = 0;
	virtual void Visit(class GenericParamListEntry* Entry) = 0; 


	/** Forward declarations so that games can create game specific events that can be visited */
	virtual void Visit(class EntryEx1* Entry) = 0;
	virtual void Visit(class EntryEx2* Entry) = 0;
	virtual void Visit(class EntryEx3* Entry) = 0;
	virtual void Visit(class EntryEx4* Entry) = 0;
	virtual void Visit(class EntryEx5* Entry) = 0;
	virtual void Visit(class EntryEx6* Entry) = 0;
	virtual void Visit(class EntryEx7* Entry) = 0;
	virtual void Visit(class EntryEx8* Entry) = 0;
	virtual void Visit(class EntryEx9* Entry) = 0;
	virtual void Visit(class EntryEx10* Entry) = 0;
	virtual void Visit(class EntryEx11* Entry) = 0;
	virtual void Visit(class EntryEx12* Entry) = 0;
	virtual void Visit(class EntryEx13* Entry) = 0;
	virtual void Visit(class EntryEx14* Entry) = 0;
	virtual void Visit(class EntryEx15* Entry) = 0;
}




