/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __GAMESTATSREPORT_H__
#define __GAMESTATSREPORT_H__

#define EVENT_METATAG		TEXT("Event")
#define TEAM_METATAG		TEXT("Team")
#define PLAYER_METATAG		TEXT("Player")
#define KILLS_METATAG		TEXT("Kills")
#define DEATHS_METATAG		TEXT("Deaths")
#define DAMAGE_METATAG		TEXT("DamageClass")
#define WEAPON_METATAG		TEXT("WeaponClass")
#define PROJECTILE_METATAG	TEXT("ProjectileClass")
#define PAWN_METATAG		TEXT("PawnClass")
#define ACTORS_METATAG		TEXT("ActorClass")
#define SOUNDCUE_METATAG	TEXT("SoundCueNames")

#define IMAGE_METATAG		TEXT("Image")
#define TOTALS_METATAG		TEXT("Totals")
#define GAMEPERIOD_METATAG	TEXT("Game")
#define ROUNDPERIOD_METATAG TEXT("Round%d")
#define AGGREGATE_METATAG	TEXT("Aggregate")
#define EMPTY_METATAG		TEXT("")


/*
 *   Write out a key value pair as an xml tag
 * @param Ar - archive to write to
 * @param KeyValuePair - key value pair to output
 * @param InIndent - indentation to prepend to the tag
 */
void Event_ToXml(FArchive& Ar, const FMetaKeyValuePair& KeyValuePair, const FString& InIndent);

/*
 *   Write out a xml category (representation of key value pairs and sub categories)
 * @param Ar - archive to write to
 * @param Category - category to output
 * @param IndentCount - number of tabs to prepend to the output
 */
void Category_ToXml(FArchive& Ar, const FCategory Category, DWORD IndentCount = 0);

/*
 *  Find the given event ids in the game events container for a given time period
 * @param EventColumns - array of event IDs to find in the container
 * @param GameEvents - container of events recorded
 * @param TimePeriod - time period of interest (0 game total, 1+ round totals)
 */
void GetEventKeyValuePairs(const TArray<INT>& EventColumns, const FGameEvents& GameEvents, INT TimePeriod, TArray<FMetaKeyValuePair>& out_KeyValuePairs);

/** 
 * Class that handles the report writing logic 
 * Wrapped this way so both the editor and commandlet can make use of common functionality
 */
class FGameStatsReportWorker
{
public:

	FGameStatsReportWorker() {}
	virtual ~FGameStatsReportWorker() {}

	/*
	 * Main body of the stats report writing class
	 * Opens and parses a gamestats file then outputs a	stats report
	 */
	virtual INT Main() = 0;

	/**
     * Get an URL related this report
     * @param ReportType - report type to generate 
     * @return URL passed to a browser to view the report 
	 */
	virtual FString GetReportURL(EReportType ReportType) = 0;

	/** 
     * Get a description of the heatmap queries required for this report
     * @param HeatmapQueries - queries to run
     */
	virtual void GetHeatmapQueries(TArray<FHeatmapQuery>& HeatmapQueries) = 0;
};

/** 
 * Report from file implementation of the report worker
 */
class FGameStatsReportWorkerFile : public FGameStatsReportWorker
{
public:

	FGameStatsReportWorkerFile();
	~FGameStatsReportWorkerFile();

	/*
     *  Initialize the stats report writer
	 * @param InStatsFilename - game stats file to create a report for
	 */
	UBOOL Init(const FString& InStatsFilename);
	/*
	 * Main body of the stats report writing class
	 * Opens and parses a gamestats file then outputs a	stats report
	 */
	INT Main();

	/**
     * Get an URL related this report
     * @param ReportType - report type to generate 
     * @return URL passed to a browser to view the report 
	 */
	FString GetReportURL(EReportType ReportType)
	{
		if (ReportWriter)
		{
			return ReportWriter->GetReportURL(ReportType);
		}

		return TEXT("");
	}

	/** 
     * Get a description of the heatmap queries required for this report
     * @param HeatmapQueries - queries to run
     */
	void GetHeatmapQueries(TArray<FHeatmapQuery>& HeatmapQueries)
	{
		HeatmapQueries.Empty();
		if (ReportWriter)
		{
			ReportWriter->GetHeatmapQueries(HeatmapQueries);
		}
	}

protected:
	/** Report source data */
	FString	StatsFilename;
	/** Actual report writer object */
	UGameStatsReport* ReportWriter;
	/** GameState class when parsing the stats file */
	FString GameStatsGameStateClassName;
	/** Aggregation class when parsing the stats file */
	FString GameStatsAggregatorClassName;
	/** Report writing class when outputting the report to disk */
	FString GameStatsReportWriterClassName;
};

/** 
 * Report from DB implementation of the report worker
 */
class FGameStatsReportWorkerDB : public FGameStatsReportWorker
{
public:

	FGameStatsReportWorkerDB() : ReportWriter(NULL) 
	{
		appMemzero(&SessionInfo, sizeof(FGameSessionInformation));
	}
	~FGameStatsReportWorkerDB() {}

	/*
     *  Initialize the stats report writer
	 */
	UBOOL Init(const FGameSessionInformation& InSessionInfo);
	/*
	 * Main body of the stats report writing class
	 * Opens and parses a gamestats file then outputs a	stats report
	 */
	INT Main();

	/**
     * Get an URL related this report
     * @param ReportType - report type to generate 
     * @return URL passed to a browser to view the report 
	 */
	FString GetReportURL(EReportType ReportType);

	/** 
     * Get a description of the heatmap queries required for this report
     * @param HeatmapQueries - queries to run
     */
	void GetHeatmapQueries(TArray<FHeatmapQuery>& HeatmapQueries)
	{
		HeatmapQueries.Empty();
		if (ReportWriter)
		{
			ReportWriter->GetHeatmapQueries(HeatmapQueries);
		}
	}

protected:
	/** Session Info */
	FGameSessionInformation SessionInfo;
	/** Actual report writer object */
	UGameStatsReport* ReportWriter;
	/** Report writing class to get heatmap query information from */
	FString GameStatsReportWriterClassName;
};

#endif //__GAMESTATSREPORT_H__