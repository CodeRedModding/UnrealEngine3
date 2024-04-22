/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Generic implementation of the GameStatsDatabaseVisitor interface
 */
class GameStatsVisitorImpl extends Object
	implements(GameStatsDatabaseVisitor)
	abstract
	native(GameStats)
	config(Editor);

cpptext
{
	/** Called before the visiting begins */
	virtual void BeginVisiting() { /* Do nothing */ }

	/** Called after the visiting is over */
	virtual UBOOL EndVisiting() { return TRUE; }

	/**
	 * Abstract functions that defines a visitor's behavior when given a GameStatEntry 
	 * There must be an entry for each possible concrete datatype stored in the database
	 * or else no visit action will occur.
	 */
	virtual void Visit(class GameStringEntry* Entry) { /* Do nothing */ } 
	virtual void Visit(class GameIntEntry* Entry) { /* Do nothing */ }
	virtual void Visit(class GameFloatEntry* Entry) { /* Do nothing */ }
	virtual void Visit(class GamePositionEntry* Entry) { /* Do nothing */ }
	virtual void Visit(class TeamIntEntry* Entry) { /* Do nothing */ } 
	virtual void Visit(class TeamFloatEntry* Entry) { /* Do nothing */ } 
	virtual void Visit(class TeamStringEntry* Entry) { /* Do nothing */ } 
	virtual void Visit(class PlayerStringEntry* Entry) { /* Do nothing */ }
	virtual void Visit(class PlayerIntEntry* Entry) { /* Do nothing */ } 
	virtual void Visit(class PlayerFloatEntry* Entry) { /* Do nothing */ } 
	virtual void Visit(class PlayerLoginEntry* Entry) { /* Do nothing */ }
	virtual void Visit(class PlayerSpawnEntry* Entry) { /* Do nothing */ }
	virtual void Visit(class PlayerKillDeathEntry* Entry) { /* Do nothing */ }
	virtual void Visit(class PlayerPlayerEntry * Entry) { /* Do nothing */ }
	virtual void Visit(class WeaponEntry* Entry) { /* Do nothing */ }
	virtual void Visit(class DamageEntry* Entry) { /* Do nothing */ } 
	virtual void Visit(class ProjectileIntEntry* Entry) { /* Do nothing */ }
	virtual void Visit(class GenericParamListEntry* Entry){ /* Do SOMETHING! */ }


	/** Forward declarations so that games can create game specific events that can be visited */
	virtual void Visit(class EntryEx1* Entry) { /* Do nothing */ }
	virtual void Visit(class EntryEx2* Entry) { /* Do nothing */ }
	virtual void Visit(class EntryEx3* Entry) { /* Do nothing */ }
	virtual void Visit(class EntryEx4* Entry) { /* Do nothing */ }
	virtual void Visit(class EntryEx5* Entry) { /* Do nothing */ }
	virtual void Visit(class EntryEx6* Entry) { /* Do nothing */ }
	virtual void Visit(class EntryEx7* Entry) { /* Do nothing */ }
	virtual void Visit(class EntryEx8* Entry) { /* Do nothing */ }
	virtual void Visit(class EntryEx9* Entry) { /* Do nothing */ }
	virtual void Visit(class EntryEx10* Entry) { /* Do nothing */ }
	virtual void Visit(class EntryEx11* Entry) { /* Do nothing */ }
	virtual void Visit(class EntryEx12* Entry) { /* Do nothing */ }
	virtual void Visit(class EntryEx13* Entry) { /* Do nothing */ }
	virtual void Visit(class EntryEx14* Entry) { /* Do nothing */ }
	virtual void Visit(class EntryEx15* Entry) { /* Do nothing */ }
}
