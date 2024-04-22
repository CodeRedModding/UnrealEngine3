/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Base class for generating reports from the game stats data
 */
class GameStatsReport extends Object
	abstract
	config(Editor)
	native(GameStats);

enum EReportType
{
   RT_SingleSession, // Single session recorded
   RT_Game		     // All sessions related (all data under a single GUID)
};

/** Basic key value pair structure for XML output */
struct native MetaKeyValuePair
{
	var init string Tag;
	var init string Key;
	var init string Value;

	structcpptext
	{
		FMetaKeyValuePair()
		{}
		FMetaKeyValuePair(EEventParm)
		{
			appMemzero(this, sizeof(FMetaKeyValuePair));
		}
		FMetaKeyValuePair(const FString& InTag) : Tag(InTag) {}
	}
};

/** Basic XML container, contains key value pairs and other sub categories */
struct native Category
{
	var init string Tag;
	var init string Header;
	var int id;
	var init array<MetaKeyValuePair> KeyValuePairs;
	var init array<Category> SubCategories;

	structcpptext
	{
		FCategory()
		{}
		FCategory(EEventParm)
		{
			appMemzero(this, sizeof(FCategory));
		}
		FCategory(const FString& InTag, const FString& InHeader) : Tag(InTag), Header(InHeader), Id(INDEX_NONE) {}
	}
};

/** Heatmap queries to generate for this report */
struct native HeatmapQuery
{
	/** Name to call this heatmap */
	var string HeatmapName;
	/** Events to include in the query */
	var array<int> EventIDs;
	/** Filename for the query output */
	var string ImageFilename;
};

/** Copy of the session info */
var GameSessionInformation SessionInfo;
/** Instance of the game state */
var transient GameStateObject GameState;
/** Instance of the file reader */
var transient GameplayEventsReader StatsFileReader;
/** Game stats aggregator */
var transient GameStatsAggregator Aggregator;

/** Events to post in the special "highlights" sections of the report */
var array<int> HighlightEvents;

/** EventIDs to display as columns for game stats */
var array<int> GameStatsColumns;
/** EventIDs to display as columns for team stats */
var array<int> TeamStatsColumns;
/** EventIDs to display as columns for player stats */
var array<int> PlayerStatsColumns;
/** EventIDs to display as columns for weapon stats */
var array<int> WeaponStatsColumns;
/** EventIDs to display as columns for damage stats */
var array<int> DamageStatsColumns;
/** EventIDs to display as columns for projectile stats */
var array<int> ProjectileStatsColumns;
/** EventIDs to display as columns for pawn stats */
var array<int> PawnStatsColumns;

/** Base URL for report location */
var config string ReportBaseURL;

cpptext
{
	/** Output the entire report in XML */
	virtual void WriteReport(FArchive& Ar);
	/** 
	 * Write the session header information to XML 
	 * @param Ar - archive to write out
	 * @param IndentCount - number of tabs to indent this information
	 */	
	virtual void WriteSessionHeader(FArchive& Ar, INT IndentCount);
	/** 
	 * Write the any image reference information to XML 
	 * @param Ar - archive to write out
	 * @param IndentCount - number of tabs to indent this information
	 */	
	virtual void WriteImageMetadata(FArchive& Ar, INT IndentCount);
	/** 
	 * Write the session metadata to XML 
	 * @param Ar - archive to write out
	 * @param IndentCount - number of tabs to indent this information
	 */	
	virtual void WriteMetadata(FArchive& Ar, INT IndentCount);
	/** 
	 * Write the game stats data to XML 
	 * @param Ar - archive to write out
	 * @param IndentCount - number of tabs to indent this information
	 */	
	virtual void WriteGameValues(FArchive& Ar, INT IndentCount);
	/** 
	 * Write the team stats data to XML 
	 * @param Ar - archive to write out
	 * @param IndentCount - number of tabs to indent this information
	 */	
	virtual void WriteTeamValues(FArchive& Ar, INT IndentCount);
	/** 
	 * Fill out the information for a single team
	 * @param Team - XML object to fill in with data
	 * @param TeamIndex - team currently being written out
	 */	
	virtual void WriteTeamValue(FCategory& Team, INT TeamIndex);
	/** 
	 * Write the player stats data to XML 
	 * @param Ar - archive to write out
	 * @param IndentCount - number of tabs to indent this information
	 */	
	virtual void WritePlayerValues(FArchive& Ar, INT IndentCount);
	/** 
	 * Fill out the information for a single player
	 * @param Player - XML object to fill in with data
	 * @param PlayerIndex - player currently being written out
	 */	
	virtual void WritePlayerValue(FCategory& Player, INT PlayerIndex);
	/** 
	 * Write the weapon stats data to XML 
	 * @param Ar - archive to write out
	 * @param IndentCount - number of tabs to indent this information
	 */	
	virtual void WriteWeaponValues(FArchive& Ar, INT IndentCount);
	/** 
	 * Write the damage stats data to XML 
	 * @param Ar - archive to write out
	 * @param IndentCount - number of tabs to indent this information
	 */	
	virtual void WriteDamageValues(FArchive& Ar, INT IndentCount);
	/** 
	 * Write the projectile stats data to XML 
	 * @param Ar - archive to write out
	 * @param IndentCount - number of tabs to indent this information
	 */	
	virtual void WriteProjectileValues(FArchive& Ar, INT IndentCount);
	/** 
	 * Write the pawn stats data to XML 
	 * @param Ar - archive to write out
	 * @param IndentCount - number of tabs to indent this information
	 */	
	virtual void WritePawnValues(FArchive& Ar, INT IndentCount);
	/** 
	 * Write anything game specific to XML 
	 * @param Ar - archive to write out
	 * @param IndentCount - number of tabs to indent this information
	 */	
	virtual void WriteGameSpecificValues(FArchive& Ar, INT IndentCount) {}

	/**
     * Get an URL related this report
     * @param ReportType - report type to generate 
     * @return URL passed to a browser to view the report 
	 */
	virtual FString GetReportURL(EReportType ReportType) { return TEXT(""); } 
	/** @return the location of the file generated */
	virtual FString GetReportFilename(const FString& FileExt);

	/** @return list of heatmap queries to run on the database for this report */
	virtual void GetHeatmapQueries(TArray<FHeatmapQuery>& HeatmapQueries);

	/*
	 *   Get all the event columns to be displayed in the whole report 
	 * @param EventColumns - structure to add columns to
	 */
	virtual void GetAllEventColumns(TArray<INT>& EventColumns);
	/*
	 *   Get all the weapon events for a given time period (uses WeaponStatsColumns)
	 * @param ParentCategory - XML container to fill with the data
	 * @param TimePeriod - TimePeriod (0 game, 1+ round)
	 * @param WeaponEvents - the aggregate events structure to get the data from
	 * @param StatsReader - the file reader containing the weapon metadata
	 */
	void GetWeaponValuesForTimePeriod(FCategory& ParentCategory, INT TimePeriod, const struct FWeaponEvents& WeaponEvents, const class UGameplayEventsReader* StatsReader);
	/*
	 *   Get all the damage events for a given time period (uses DamageStatsColumns)
	 * @param ParentCategory - XML container to fill with the data
	 * @param TimePeriod - TimePeriod (0 game, 1+ round)
	 * @param DamageEvents - the aggregate events structure to get the data from
	 * @param StatsReader - the file reader containing the damage metadata
	 */
	void GetDamageValuesForTimePeriod(FCategory& ParentCategory, INT TimePeriod, const struct FDamageEvents& DamageEvents, const class UGameplayEventsReader* StatsReader);
	/*
	 *   Get all the projectile events for a given time period (uses ProjectileStatsColumns)
	 * @param ParentCategory - XML container to fill with the data
	 * @param TimePeriod - TimePeriod (0 game, 1+ round)
	 * @param ProjectileEvents - the aggregate events structure to get the data from
	 * @param StatsReader - the file reader containing the projectile metadata
	 */
	void GetProjectileValuesForTimePeriod(FCategory& ParentCategory, INT TimePeriod, const struct FProjectileEvents& ProjectileEvents, const class UGameplayEventsReader* StatsReader);
	/*
	 *   Get all the pawn events for a given time period (uses PawnStatsColumns)
	 * @param ParentCategory - XML container to fill with the data
	 * @param TimePeriod - TimePeriod (0 game, 1+ round)
	 * @param PawnEvents - the aggregate events structure to get the data from
	 * @param StatsReader - the file reader containing the pawn metadata
	 */
	void GetPawnValuesForTimePeriod(FCategory& ParentCategory, INT TimePeriod, const struct FPawnEvents& PawnEvents, const class UGameplayEventsReader* StatsReader);

};

defaultproperties
{
	// Player events to highlight at top of report
	HighlightEvents.Add(GAMEEVENT_AGGREGATED_PLAYER_KILLS);
	HighlightEvents.Add(GAMEEVENT_AGGREGATED_PLAYER_DEATHS);
	HighlightEvents.Add(GAMEEVENT_AGGREGATED_PAWN_SPAWN);
	HighlightEvents.Add(GAMEEVENT_AGGREGATED_PLAYER_MATCH_WON);

	// Team stats to display
	TeamStatsColumns.Add(GAMEEVENT_AGGREGATED_TEAM_GAME_SCORE)
	TeamStatsColumns.Add(GAMEEVENT_AGGREGATED_TEAM_MATCH_WON)
	TeamStatsColumns.Add(GAMEEVENT_AGGREGATED_TEAM_ROUND_WON)
	TeamStatsColumns.Add(GAMEEVENT_AGGREGATED_TEAM_KILLS)
	TeamStatsColumns.Add(GAMEEVENT_AGGREGATED_TEAM_DEATHS)

	// Player stats to display
	PlayerStatsColumns.Add(GAMEEVENT_AGGREGATED_PLAYER_MATCH_WON)
	PlayerStatsColumns.Add(GAMEEVENT_AGGREGATED_PLAYER_ROUND_WON)
	PlayerStatsColumns.Add(GAMEEVENT_AGGREGATED_PLAYER_KILLS)
	PlayerStatsColumns.Add(GAMEEVENT_AGGREGATED_PLAYER_DEATHS)
	PlayerStatsColumns.Add(GAMEEVENT_AGGREGATED_PLAYER_TIMEALIVE)

	// Weapon stats to display
	WeaponStatsColumns.Add(GAMEEVENT_AGGREGATED_WEAPON_FIRED)
	
	// Damage stats to display
	DamageStatsColumns.Add(GAMEEVENT_AGGREGATED_DAMAGE_KILLS)
	DamageStatsColumns.Add(GAMEEVENT_AGGREGATED_DAMAGE_DEATHS)
	DamageStatsColumns.Add(GAMEEVENT_AGGREGATED_DAMAGE_DEALT_WEAPON_DAMAGE)
	DamageStatsColumns.Add(GAMEEVENT_AGGREGATED_DAMAGE_RECEIVED_WEAPON_DAMAGE)
	DamageStatsColumns.Add(GAMEEVENT_AGGREGATED_DAMAGE_DEALT_MELEE_DAMAGE)
	DamageStatsColumns.Add(GAMEEVENT_AGGREGATED_DAMAGE_RECEIVED_MELEE_DAMAGE)
	
	// Projectile stats to display

	// Pawn stats to display
	PawnStatsColumns.Add(GAMEEVENT_AGGREGATED_PAWN_SPAWN)
}

