/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Interface for processing events as they are read out of the game stats stream
 */
class GameplayEventsHandler extends Object
	abstract
	config(GameStats)
	native;

cpptext
{
	/** The function that does the actual handling of data (override with particular implementation) */
	virtual void HandleEvent(struct FGameEventHeader& GameEvent, class IGameEvent* GameEventData);

	/** Handlers for parsing the game stats stream */

	// Game Event Handling
	virtual void HandleGameStringEvent(struct FGameEventHeader& GameEvent, struct FGameStringEvent* GameEventData) {}
	virtual void HandleGameIntEvent(struct FGameEventHeader& GameEvent, struct FGameIntEvent* GameEventData) {}
	virtual void HandleGameFloatEvent(struct FGameEventHeader& GameEvent, struct FGameFloatEvent* GameEventData) {}
	virtual void HandleGamePositionEvent(struct FGameEventHeader& GameEvent, struct FGamePositionEvent* GameEventData) {}

	// Team Event Handling
	virtual void HandleTeamStringEvent(struct FGameEventHeader& GameEvent, struct FTeamStringEvent* GameEventData) {}
	virtual void HandleTeamIntEvent(struct FGameEventHeader& GameEvent, struct FTeamIntEvent* GameEventData) {}
	virtual void HandleTeamFloatEvent(struct FGameEventHeader& GameEvent, struct FTeamFloatEvent* GameEventData) {}

	// Player Event Handling
	virtual void HandlePlayerIntEvent(struct FGameEventHeader& GameEvent, struct FPlayerIntEvent* GameEventData) {}
	virtual void HandlePlayerFloatEvent(struct FGameEventHeader& GameEvent, struct FPlayerFloatEvent* GameEventData) {}
	virtual void HandlePlayerStringEvent(struct FGameEventHeader& GameEvent, struct FPlayerStringEvent* GameEventData) {}
	virtual void HandlePlayerSpawnEvent(struct FGameEventHeader& GameEvent, struct FPlayerSpawnEvent* GameEventData) {}
	virtual void HandlePlayerLoginEvent(struct FGameEventHeader& GameEvent, struct FPlayerLoginEvent* GameEventData) {}
	virtual void HandlePlayerKillDeathEvent(struct FGameEventHeader& GameEvent, struct FPlayerKillDeathEvent* GameEventData) {}
	virtual void HandlePlayerPlayerEvent(struct FGameEventHeader& GameEvent, struct FPlayerPlayerEvent* GameEventData) {}
	virtual void HandlePlayerLocationsEvent(struct FGameEventHeader& GameEvent, struct FPlayerLocationsEvent* GameEventData) {}
	virtual void HandleWeaponIntEvent(struct FGameEventHeader& GameEvent, struct FWeaponIntEvent* GameEventData) {}
	virtual void HandleDamageIntEvent(struct FGameEventHeader& GameEvent, struct FDamageIntEvent* GameEventData) {}
	virtual void HandleProjectileIntEvent(struct FGameEventHeader& GameEvent, struct FProjectileIntEvent* GameEventData) {}

	/** Access the current session info */
	const FGameSessionInformation& GetSessionInfo() const
	{
		check(Reader);
		return Reader->CurrentSessionInfo;
	}

	/** Returns the metadata associated with the given index */
	virtual const FGameplayEventMetaData& GetEventMetaData(INT EventID) const
	{
		check(Reader);
		return Reader->GetEventMetaData(EventID);
	}

	/** Returns the metadata associated with the given index */
	const FTeamInformation& GetTeamMetaData(INT TeamIndex) const
	{
		check(Reader);
		return Reader->GetTeamMetaData(TeamIndex);
	}

	/** Returns the metadata associated with the given index */
	const FPlayerInformation& GetPlayerMetaData(INT PlayerIndex) const
	{
		check(Reader);
		return Reader->GetPlayerMetaData(PlayerIndex);
	}

	/** Returns the metadata associated with the given index */
	const FPawnClassEventData& GetPawnMetaData(INT PawnClassIndex) const
	{
		check(Reader);
		return Reader->GetPawnMetaData(PawnClassIndex);
	}

	/** Returns the metadata associated with the given index */
	const FWeaponClassEventData& GetWeaponMetaData(INT WeaponClassIndex) const
	{
		check(Reader);
		return Reader->GetWeaponMetaData(WeaponClassIndex);
	}

	/** Returns the metadata associated with the given index */
	const FDamageClassEventData& GetDamageMetaData(INT DamageClassIndex) const
	{
		check(Reader);
		return Reader->GetDamageMetaData(DamageClassIndex);
	}

	/** Returns the metadata associated with the given index */
	const FProjectileClassEventData& GetProjectileMetaData(INT ProjectileClassIndex) const
	{
		check(Reader);
		return Reader->GetProjectileMetaData(ProjectileClassIndex);
	}

	/**
	 * Returns the metadata associated with the given index
	 * @param ActorIndex the index of the actor being looked up
	 * @return the name of the actor at that index
	 */
	const FString& GetActorMetaData(INT ActorIndex) const
	{
		check(Reader);
		return Reader->GetActorMetaData(ActorIndex);
	}

	/**
	 * Returns the metadata associated with the given index
	 * @param SoundIndex the index of the soundcue being looked up
	 * @return the name of the actor at that index
	 */
	const FString& GetSoundMetaData(INT SoundIndex) const
	{
		check(Reader);
		return Reader->GetSoundMetaData(SoundIndex);
	}

	/** Returns whether or not this processor handles this event */
	inline UBOOL IsEventFiltered(int EventID) const
	{
		return (EventIDFilter.FindItemIndex(EventID) != INDEX_NONE);
	}
}

/** Array of event types that will be ignored */
var config array<int> EventIDFilter;

/** Array of groups to filter, expands out into EventIDFilter above */
var config array<GameStatGroup> GroupFilter;

/** Reference to the reader for access to metadata, etc */
var transient private{protected} GameplayEventsReader Reader;

/** Set the reader on this handler */
function SetReader(GameplayEventsReader NewReader)
{
	Reader = NewReader;
}

/** A chance to do something before the stream starts */
native event PreProcessStream();

/** A chance to do something after the stream ends */
event PostProcessStream();

/** Iterate over all events, checking to see if they should be filtered out by their group */
event ResolveGroupFilters()
{
	local int EventIdx, FilterIdx;

	for (EventIdx=0; EventIdx<Reader.SupportedEvents.length; EventIdx++)
	{
		// Are we filtering this stats group at all?
		FilterIdx = GroupFilter.Find('Group', Reader.SupportedEvents[EventIdx].StatGroup.Group);
		if (FilterIdx != INDEX_NONE)
		{
			// Stats filter above the indicated level
			if (Reader.SupportedEvents[EventIdx].StatGroup.Level > GroupFilter[FilterIdx].Level)
			{
				AddFilter(Reader.SupportedEvents[EventIdx].EventID);
			}
		}
	}
}

/** Add an event id to ignore while processing */
function AddFilter(int EventID)
{
	if (EventIDFilter.Find(EventID) == INDEX_NONE)
	{
		EventIDFilter.AddItem(EventID);
	}
}

/** Remove an event id to ignore while processing */
function RemoveFilter(int EventID)
{
	EventIDFilter.RemoveItem(EventID);
}