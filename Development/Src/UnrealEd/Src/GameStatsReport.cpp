/*=============================================================================
	GameStatsReport.cpp: 
	Commandlet for generating an aggregated	report from a gamestats file
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "GameStatsUtilities.h"
#include "GameFrameworkGameStatsClasses.h"
#include "UnrealEdGameStatsClasses.h"
#include "UnIpDrv.h"
#include "OnlineSubsystemUtilities.h"
#include "GameplayEventsUtilities.h"
#include "GameStatsDatabaseTypes.h"
#include "GameStatsReport.h"

//Warn helper
#ifndef warnexprf
#define warnexprf(expr, msg) { if(!(expr)) warnf(msg); }
#endif

/*
 *   Write out a key value pair as an xml tag
 * @param Ar - archive to write to
 * @param KeyValuePair - key value pair to output
 * @param InIndent - indentation to prepend to the tag
 */
void Event_ToXml(FArchive& Ar, const FMetaKeyValuePair& KeyValuePair, const FString& InIndent)
{
	FString Indent(InIndent);
	Indent += TEXT('\t');

	Ar.Logf(TEXT("%s<%s key=\"%s\" value=\"%s\"/>"),
				*Indent,
				*KeyValuePair.Tag,
				*KeyValuePair.Key,
				*KeyValuePair.Value);
} 

/*
 *   Write out a xml category (representation of key value pairs and sub categories)
 * @param Ar - archive to write to
 * @param Category - category to output
 * @param IndentCount - number of tabs to prepend to the output
 */
void Category_ToXml(FArchive& Ar, const FCategory Category, DWORD IndentCount)
{
	FString Indent = BuildIndentString(IndentCount);

	if (Category.Id != INDEX_NONE)
	{
		if (Category.Header.Len() > 0)
		{
			Ar.Logf(TEXT("%s<%s Idx=\"%d\" Name=\"%s\">"), *Indent, *Category.Tag, Category.Id, *Category.Header);
		}
		else
		{
			Ar.Logf(TEXT("%s<%s Idx=\"%d\">"), *Indent, *Category.Tag, Category.Id);
		}
	}
	else
	{				 
		if (Category.Header.Len() > 0)
		{
			Ar.Logf(TEXT("%s<%s Name=\"%s\">"), *Indent, *Category.Tag, *Category.Header);
		}
		else
		{
			Ar.Logf(TEXT("%s<%s>"), *Indent, *Category.Tag);
		}
	}

	for (INT PairIdx=0; PairIdx<Category.KeyValuePairs.Num(); PairIdx++)
	{
		const FMetaKeyValuePair& KeyValuePair = Category.KeyValuePairs(PairIdx);
		Event_ToXml(Ar, KeyValuePair, Indent);
	}

	for (INT SubCatIdx=0; SubCatIdx<Category.SubCategories.Num(); SubCatIdx++)
	{
		const FCategory& SubCat = Category.SubCategories(SubCatIdx);
		Category_ToXml(Ar, SubCat, IndentCount + 1);
	}

	Ar.Logf(TEXT("%s</%s>"), *Indent, *Category.Tag);
}

/*
 *  Find the given event ids in the game events container for a given time period
 * @param EventColumns - array of event IDs to find in the container
 * @param GameEvents - container of events recorded
 * @param TimePeriod - time period of interest (0 game total, 1+ round totals)
 */
void GetEventKeyValuePairs(const TArray<INT>& EventColumns, const FGameEvents& GameEvents, INT TimePeriod, TArray<FMetaKeyValuePair>& out_KeyValuePairs)
{
	FMetaKeyValuePair KeyValuePair(EVENT_METATAG);
	for (INT EventColumnIdx=0; EventColumnIdx<EventColumns.Num(); EventColumnIdx++)
	{
		const INT EventID = EventColumns(EventColumnIdx);
		const FGameEvent* GameEvent = GameEvents.Events.Find(EventID);

		FLOAT EventValue = 0.0f;
		if (GameEvent != NULL)
		{
			EventValue = GameEvent->GetEventData(TimePeriod);
		}

		KeyValuePair.Key = appItoa(EventID);
		KeyValuePair.Value = FString::Printf(TEXT("%d"), (INT)(EventValue + 0.5f) );
		
		out_KeyValuePairs.AddItem(KeyValuePair);
	}
}

/*
 *   Get all the weapon events for a given time period (uses WeaponStatsColumns)
 * @param ParentCategory - XML container to fill with the data
 * @param TimePeriod - TimePeriod (0 game, 1+ round)
 * @param WeaponEvents - the aggregate events structure to get the data from
 * @param StatsReader - the file reader containing the weapon metadata
 */
void UGameStatsReport::GetWeaponValuesForTimePeriod(FCategory& ParentCategory, INT TimePeriod, const FWeaponEvents& WeaponEvents, const UGameplayEventsReader* StatsReader)
{
	// Totals across all weapon types
	FCategory TotalsSubCategory(AGGREGATE_METATAG, EMPTY_METATAG);
	GetEventKeyValuePairs(WeaponStatsColumns, WeaponEvents.TotalEvents, TimePeriod, TotalsSubCategory.KeyValuePairs);
	ParentCategory.SubCategories.AddItem(TotalsSubCategory);

	// Totals per weapon type
	for (INT MetaIter = 0; MetaIter < StatsReader->WeaponClassArray.Num(); MetaIter++)
	{	 
		const FWeaponClassEventData& WeaponMetadata = StatsReader->GetWeaponMetaData(MetaIter);
		const FGameEvents& Events = WeaponEvents.EventsByClass(MetaIter);
		FCategory WeaponSubCategory(WEAPON_METATAG, WeaponMetadata.WeaponClassName.ToString());
		WeaponSubCategory.Id = MetaIter;
		GetEventKeyValuePairs(WeaponStatsColumns, Events, TimePeriod, WeaponSubCategory.KeyValuePairs);

		ParentCategory.SubCategories.AddItem(WeaponSubCategory);
	}
}

/*
 *   Get all the damage events for a given time period (uses DamageStatsColumns)
 * @param ParentCategory - XML container to fill with the data
 * @param TimePeriod - TimePeriod (0 game, 1+ round)
 * @param DamageEvents - the aggregate events structure to get the data from
 * @param StatsReader - the file reader containing the damage metadata
 */
void UGameStatsReport::GetDamageValuesForTimePeriod(FCategory& ParentCategory, INT TimePeriod, const FDamageEvents& DamageEvents, const UGameplayEventsReader* StatsReader)
{
	// Totals across all damage types
	FCategory TotalsSubCategory(AGGREGATE_METATAG, EMPTY_METATAG);
	GetEventKeyValuePairs(DamageStatsColumns, DamageEvents.TotalEvents, TimePeriod, TotalsSubCategory.KeyValuePairs);
	ParentCategory.SubCategories.AddItem(TotalsSubCategory);

	// Totals per damage type
	for (INT MetaIter = 0; MetaIter < StatsReader->DamageClassArray.Num(); MetaIter++)
	{	 
		const FDamageClassEventData& DamageMetadata = StatsReader->GetDamageMetaData(MetaIter);
		const FGameEvents& Events = DamageEvents.EventsByClass(MetaIter);
		FCategory DamageSubCategory(DAMAGE_METATAG, DamageMetadata.DamageClassName.ToString());
		DamageSubCategory.Id = MetaIter;
		GetEventKeyValuePairs(DamageStatsColumns, Events, TimePeriod, DamageSubCategory.KeyValuePairs);

		ParentCategory.SubCategories.AddItem(DamageSubCategory);
	}
}

/*
 *   Get all the projectile events for a given time period (uses ProjectileStatsColumns)
 * @param ParentCategory - XML container to fill with the data
 * @param TimePeriod - TimePeriod (0 game, 1+ round)
 * @param ProjectileEvents - the aggregate events structure to get the data from
 * @param StatsReader - the file reader containing the projectile metadata
 */
void UGameStatsReport::GetProjectileValuesForTimePeriod(FCategory& ParentCategory, INT TimePeriod, const FProjectileEvents& ProjectileEvents, const UGameplayEventsReader* StatsReader)
{
	// Totals across all projectile types
	FCategory TotalsSubCategory(AGGREGATE_METATAG, EMPTY_METATAG);
	GetEventKeyValuePairs(ProjectileStatsColumns, ProjectileEvents.TotalEvents, TimePeriod, TotalsSubCategory.KeyValuePairs);
	ParentCategory.SubCategories.AddItem(TotalsSubCategory);

	// Totals per projectile type
	for (INT MetaIter = 0; MetaIter < StatsReader->ProjectileClassArray.Num(); MetaIter++)
	{	 
		const FProjectileClassEventData& ProjectileMetadata = StatsReader->GetProjectileMetaData(MetaIter);
		const FGameEvents& Events = ProjectileEvents.EventsByClass(MetaIter);
		FCategory ProjectileSubCategory(PROJECTILE_METATAG, ProjectileMetadata.ProjectileClassName.ToString());
		ProjectileSubCategory.Id = MetaIter;
		GetEventKeyValuePairs(ProjectileStatsColumns, Events, TimePeriod, ProjectileSubCategory.KeyValuePairs);

		ParentCategory.SubCategories.AddItem(ProjectileSubCategory);
	}
}

/*
 *   Get all the pawn events for a given time period (uses PawnStatsColumns)
 * @param ParentCategory - XML container to fill with the data
 * @param TimePeriod - TimePeriod (0 game, 1+ round)
 * @param PawnEvents - the aggregate events structure to get the data from
 * @param StatsReader - the file reader containing the pawn metadata
 */
void UGameStatsReport::GetPawnValuesForTimePeriod(FCategory& ParentCategory, INT TimePeriod, const FPawnEvents& PawnEvents, const UGameplayEventsReader* StatsReader)
{
	// Totals across all pawn types
	FCategory TotalsSubCategory(AGGREGATE_METATAG, EMPTY_METATAG);
	GetEventKeyValuePairs(PawnStatsColumns, PawnEvents.TotalEvents, TimePeriod, TotalsSubCategory.KeyValuePairs);
	ParentCategory.SubCategories.AddItem(TotalsSubCategory);

	// Totals per pawn type
	for (INT MetaIter = 0; MetaIter < StatsReader->PawnClassArray.Num(); MetaIter++)
	{	 
		const FPawnClassEventData& PawnMetadata = StatsReader->GetPawnMetaData(MetaIter);
		const FGameEvents& Events = PawnEvents.EventsByClass(MetaIter);
		FCategory PawnSubCategory(PAWN_METATAG, PawnMetadata.PawnClassName.ToString());
		PawnSubCategory.Id = MetaIter;
		GetEventKeyValuePairs(PawnStatsColumns, Events, TimePeriod, PawnSubCategory.KeyValuePairs);

		ParentCategory.SubCategories.AddItem(PawnSubCategory);
	}
}

/************************************************************************/
/* UGameStatsReport                                                     */
/************************************************************************/
IMPLEMENT_CLASS(UGameStatsReport);

/** 
 *  Generate a filename for the given match report
 * @param MapName - name of the map played
 * @param Timestamp - timestamp string when match was played (YYYY.MM.DD-HH.MM.SS) 
 * @return Filename of the form <mapname>-YYYY.MM.DD-HH.MM.ext
 */
FString UGameStatsReport::GetReportFilename(const FString& FileExt)
{
	FString OutputDir;
	GetStatsDirectory(OutputDir);

	const FString ReportFilename = FString::Printf(TEXT("%s-%s%s"), 
		*SessionInfo.MapName, 
		*SessionInfo.GameplaySessionTimestamp.LeftChop(3), *FileExt); 
	return OutputDir + ReportFilename;
}

/** @return list of heatmap queries to run on the database for this report */
void UGameStatsReport::GetHeatmapQueries(TArray<FHeatmapQuery>& HeatmapQueries)
{
	FHeatmapQuery HeatmapQuery(EC_EventParm);

	// Player position heatmap
	HeatmapQuery.HeatmapName = TEXT("LocationHeatmap");
	HeatmapQuery.EventIDs.AddItem(UCONST_GAMEEVENT_PLAYER_LOCATION_POLL);
	HeatmapQuery.ImageFilename = GetReportFilename(TEXT("L") GAME_STATS_REPORT_IMAGE_FILE_EXT);
	HeatmapQueries.AddItem(HeatmapQuery);
}
   
/*
 *   Get all the event columns to be displayed in the whole report 
 * @param EventColumns - structure to add columns to
 */
void UGameStatsReport::GetAllEventColumns(TArray<INT>& EventColumns)
{
	for (INT Idx = 0; Idx<HighlightEvents.Num(); Idx++)
	{
		EventColumns.AddUniqueItem(HighlightEvents(Idx));
	}

	for (INT Idx = 0; Idx<GameStatsColumns.Num(); Idx++)
	{
		EventColumns.AddUniqueItem(GameStatsColumns(Idx));
	}

	for (INT Idx = 0; Idx<TeamStatsColumns.Num(); Idx++)
	{
		EventColumns.AddUniqueItem(TeamStatsColumns(Idx));
	}

	for (INT Idx = 0; Idx<PlayerStatsColumns.Num(); Idx++)
	{
		EventColumns.AddUniqueItem(PlayerStatsColumns(Idx));
	}

	for (INT Idx = 0; Idx<WeaponStatsColumns.Num(); Idx++)
	{
		EventColumns.AddUniqueItem(WeaponStatsColumns(Idx));
	}

	for (INT Idx = 0; Idx<DamageStatsColumns.Num(); Idx++)
	{
		EventColumns.AddUniqueItem(DamageStatsColumns(Idx));
	}

	for (INT Idx = 0; Idx<ProjectileStatsColumns.Num(); Idx++)
	{
		EventColumns.AddUniqueItem(ProjectileStatsColumns(Idx));
	}

	for (INT Idx = 0; Idx<PawnStatsColumns.Num(); Idx++)
	{
		EventColumns.AddUniqueItem(PawnStatsColumns(Idx));
	}
}

/*
 *   Top level function to write out an XML report from an stats aggregation
 * @param Ar - archive to write to
 */
void UGameStatsReport::WriteReport(FArchive& Ar)
{
	Ar.Logf(TEXT("<?xml version=\"1.0\" encoding=\"utf-8\" ?>"));
	Ar.Logf(TEXT("<?xml-stylesheet type=\"text/xsl\" href=\"/StatsReportv3.xsl\"?>"));
	//Ar.Logf(TEXT("<?xml-stylesheet type=\"text/xsl\" href=\"./StatsReport.xsl\"?>"));
	//Ar.Logf(TEXT("<!-- saved from url=(0014)about:internet -->"));

	Ar.Logf(TEXT("<MatchReport>"));

	WriteSessionHeader(Ar, 1);
	WriteImageMetadata(Ar, 1);
	WriteMetadata(Ar, 1);
	WriteWeaponValues(Ar, 1);
	WriteProjectileValues(Ar, 1);
	WriteDamageValues(Ar, 1);
	WritePawnValues(Ar, 1);
	WritePlayerValues(Ar, 1);
	WriteTeamValues(Ar, 1);
	WriteGameSpecificValues(Ar, 1);

	Ar.Logf(TEXT("</MatchReport>"));
}

/*
 *   Writes out the session info of a game stats file in XML format with key value pairs
 * @param Ar - archive to write to
 */
void UGameStatsReport::WriteSessionHeader(FArchive& Ar, INT IndentCount)
{
	check(Aggregator);

	const FGameSessionInformation& CurrentSessionInfo = StatsFileReader->GetSessionInfo();
	const FString SessionID = CurrentSessionInfo.GetSessionID();
	FCategory Session(TEXT("SessionInfo"), *SessionID);
	FMetaKeyValuePair KeyValuePair(TEXT("Meta"));

	KeyValuePair.Key = TEXT("EngineVersion");
	KeyValuePair.Value = appItoa(StatsFileReader->Header.EngineVersion);
	Session.KeyValuePairs.AddItem(KeyValuePair);

	KeyValuePair.Key = TEXT("AppTitleID");
	KeyValuePair.Value = FString::Printf(TEXT("0x%08x"), CurrentSessionInfo.AppTitleID);
	Session.KeyValuePairs.AddItem(KeyValuePair);

	KeyValuePair.Key = TEXT("PlatformType");
	KeyValuePair.Value = appItoa(CurrentSessionInfo.PlatformType);
	Session.KeyValuePairs.AddItem(KeyValuePair);

	KeyValuePair.Key = TEXT("Language");
	KeyValuePair.Value = CurrentSessionInfo.Language;
	Session.KeyValuePairs.AddItem(KeyValuePair);

	// Break timestamp into left date and right time
	INT DateTimeDivider = CurrentSessionInfo.GameplaySessionTimestamp.InStr(TEXT("-"));
	INT StrLen = CurrentSessionInfo.GameplaySessionTimestamp.Len();
	FString Date = CurrentSessionInfo.GameplaySessionTimestamp.Left(DateTimeDivider);
	Date.ReplaceInline(TEXT("."),TEXT("/"));
	FString Time = CurrentSessionInfo.GameplaySessionTimestamp.Right(StrLen - DateTimeDivider - 1);
	Time.ReplaceInline(TEXT("."),TEXT(":"));

	KeyValuePair.Key = TEXT("GameplaySessionTimestamp");
	KeyValuePair.Value = FString::Printf(TEXT("%s %s"), *Date, *Time);
	Session.KeyValuePairs.AddItem(KeyValuePair);

	KeyValuePair.Key = TEXT("GameplaySessionTime");
	KeyValuePair.Value = FString::Printf(TEXT("%s"), *appPrettyTime(CurrentSessionInfo.GameplaySessionEndTime - CurrentSessionInfo.GameplaySessionStartTime));
	Session.KeyValuePairs.AddItem(KeyValuePair);

	KeyValuePair.Key = TEXT("GameplaySessionID");
	KeyValuePair.Value = SessionID; 
	Session.KeyValuePairs.AddItem(KeyValuePair);

	KeyValuePair.Key = TEXT("GameClassName");
	KeyValuePair.Value = CurrentSessionInfo.GameClassName; 
	Session.KeyValuePairs.AddItem(KeyValuePair);

	KeyValuePair.Key = TEXT("MapName");
	KeyValuePair.Value = CurrentSessionInfo.MapName; 
	Session.KeyValuePairs.AddItem(KeyValuePair);

	KeyValuePair.Key = TEXT("MapURL");
	KeyValuePair.Value = CurrentSessionInfo.MapURL; 
	Session.KeyValuePairs.AddItem(KeyValuePair);

	Category_ToXml(Ar, Session, IndentCount);
}

/*
 *   Writes out image reference metadata for the stats report
 * @param Ar - archive to write to
 * @param IndentCount - number of tabs to prepend to the xml output
 */
void UGameStatsReport::WriteImageMetadata(FArchive& Ar, INT IndentCount)
{
	FFilename HeatmapFile;
	FCategory Images(TEXT("Images"), TEXT(""));

	TArray<FHeatmapQuery> HeatmapQueries;
	GetHeatmapQueries(HeatmapQueries);

	FMetaKeyValuePair KeyValuePair(IMAGE_METATAG);
	for (INT i=0; i<HeatmapQueries.Num(); i++)
	{
		KeyValuePair.Key = HeatmapQueries(i).HeatmapName;
		HeatmapFile = HeatmapQueries(i).ImageFilename;
		KeyValuePair.Value = FString(TEXT(".\\")) * HeatmapFile.GetCleanFilename();
		Images.KeyValuePairs.AddItem(KeyValuePair);
	}

	Category_ToXml(Ar, Images, IndentCount);
}

/*
 *   Writes out relevant metadata for the stats report
 * @param Ar - archive to write to
 * @param IndentCount - number of tabs to prepend to the xml output
 */
void UGameStatsReport::WriteMetadata(FArchive& Ar, INT IndentCount)
{
	{
		FCategory Category(TEXT("Metadata"), TEXT("PlayerInformation"));
		for (INT Idx = 0; Idx<StatsFileReader->PlayerList.Num(); Idx++)
		{
			FPlayerInformation& PlayerInfo = StatsFileReader->PlayerList(Idx);

			FCategory Metadata(PLAYER_METATAG, PlayerInfo.PlayerName);
			Metadata.Id = Idx;

			FMetaKeyValuePair KeyValuePair(TEXT("Meta"));
			KeyValuePair.Key = TEXT("ControllerName");
			KeyValuePair.Value = PlayerInfo.ControllerName.ToString();
			Metadata.KeyValuePairs.AddItem(KeyValuePair);
			KeyValuePair.Key = TEXT("UniqueId");
			KeyValuePair.Value = UOnlineSubsystem::UniqueNetIdToString(PlayerInfo.UniqueId);
			Metadata.KeyValuePairs.AddItem(KeyValuePair);
			KeyValuePair.Key = TEXT("bIsBot");
			KeyValuePair.Value = PlayerInfo.bIsBot ? TEXT("TRUE") : TEXT("FALSE");
			Metadata.KeyValuePairs.AddItem(KeyValuePair);

			Category.SubCategories.AddItem(Metadata);
		}

		Category_ToXml(Ar, Category, IndentCount);
	}

	{
		FCategory Category(TEXT("Metadata"), TEXT("TeamInformation"));
		for (INT Idx = 0; Idx<StatsFileReader->TeamList.Num(); Idx++)
		{			  
			FTeamInformation& TeamInfo = StatsFileReader->TeamList(Idx);

			FCategory Metadata(TEAM_METATAG, TeamInfo.TeamName);
			Metadata.Id = Idx;

			FMetaKeyValuePair KeyValuePair(TEXT("Meta"));
			KeyValuePair.Key = TEXT("TeamIndex");
			KeyValuePair.Value = appItoa(TeamInfo.TeamIndex);
			Metadata.KeyValuePairs.AddItem(KeyValuePair);
			KeyValuePair.Key = TEXT("TeamColor");
			KeyValuePair.Value = TeamInfo.TeamColor.ToString();
			Metadata.KeyValuePairs.AddItem(KeyValuePair);
			KeyValuePair.Key = TEXT("MaxSize");
			KeyValuePair.Value = appItoa(TeamInfo.MaxSize);
			Metadata.KeyValuePairs.AddItem(KeyValuePair);

			Category.SubCategories.AddItem(Metadata);
		}

		Category_ToXml(Ar, Category, IndentCount);
	}

	{
		// Only write out supported events that were requested in the stream
		FCategory Category(TEXT("Metadata"), TEXT("SupportedEvents"));

		TArray<INT> UniqueEvents;
		GetAllEventColumns(UniqueEvents);
		for (INT Idx = 0; Idx<UniqueEvents.Num(); Idx++)
		{
			INT EventID = UniqueEvents(Idx);

			const FGameplayEventMetaData& EventData = Aggregator->GetEventMetaData(EventID);
			if (EventData.EventID != INDEX_NONE)
			{
				FCategory Metadata(EVENT_METATAG, EventData.EventName.ToString());
				Metadata.Id = EventData.EventID;

				Category.SubCategories.AddItem(Metadata);
			}
		}

		Category_ToXml(Ar, Category, IndentCount);
	}

	{
		FCategory Category(TEXT("Metadata"), TEXT("WeaponClasses"));
		for (INT Idx = 0; Idx<StatsFileReader->WeaponClassArray.Num(); Idx++)
		{
			FWeaponClassEventData& WeaponData = StatsFileReader->WeaponClassArray(Idx);

			FCategory Metadata(WEAPON_METATAG, WeaponData.WeaponClassName.ToString());
			Metadata.Id = Idx;

			Category.SubCategories.AddItem(Metadata);
		}

		Category_ToXml(Ar, Category, IndentCount);
	}

	{
		FCategory Category(TEXT("Metadata"), TEXT("DamageClasses"));
		for (INT Idx = 0; Idx<StatsFileReader->DamageClassArray.Num(); Idx++)
		{
			FDamageClassEventData& DamageData = StatsFileReader->DamageClassArray(Idx);

			FCategory Metadata(DAMAGE_METATAG, DamageData.DamageClassName.ToString());
			Metadata.Id = Idx;

			Category.SubCategories.AddItem(Metadata);
		}

		Category_ToXml(Ar, Category, IndentCount);
	}

	{
		FCategory Category(TEXT("Metadata"), TEXT("ProjectileClasses"));
		for (INT Idx = 0; Idx<StatsFileReader->ProjectileClassArray.Num(); Idx++)
		{
			FProjectileClassEventData& ProjData = StatsFileReader->ProjectileClassArray(Idx);

			FCategory Metadata(PROJECTILE_METATAG, ProjData.ProjectileClassName.ToString());
			Metadata.Id = Idx;

			Category.SubCategories.AddItem(Metadata);
		}

		Category_ToXml(Ar, Category, IndentCount);
	}

	{
		FCategory Category(TEXT("Metadata"), TEXT("PawnClasses"));
		for (INT Idx = 0; Idx<StatsFileReader->PawnClassArray.Num(); Idx++)
		{
			FPawnClassEventData& PawnData = StatsFileReader->PawnClassArray(Idx);

			FCategory Metadata(PAWN_METATAG, PawnData.PawnClassName.ToString());
			Metadata.Id = Idx;

			Category.SubCategories.AddItem(Metadata);
		}

		Category_ToXml(Ar, Category, IndentCount);
	}

	{
		FCategory Category(TEXT("Metadata"), TEXT("Actors"));
		for (INT Idx = 0; Idx<StatsFileReader->ActorArray.Num(); Idx++)
		{
			FString& ActorName = StatsFileReader->ActorArray(Idx);

			FCategory Metadata(ACTORS_METATAG, ActorName);
			Metadata.Id = Idx;

			Category.SubCategories.AddItem(Metadata);
		}

		Category_ToXml(Ar, Category, IndentCount);
	}

	{
		FCategory Category(TEXT("Metadata"), TEXT("SoundCues"));
		for (INT Idx = 0; Idx<StatsFileReader->SoundCueArray.Num(); Idx++)
		{
			FString& SoundCueName = StatsFileReader->SoundCueArray(Idx);

			FCategory Metadata(SOUNDCUE_METATAG, SoundCueName);
			Metadata.Id = Idx;

			Category.SubCategories.AddItem(Metadata);
		}

		Category_ToXml(Ar, Category, IndentCount);
	}

	{
		FCategory Category(TEXT("Metadata"), TEXT("HighlightedEvents"));
		for (INT Idx = 0; Idx<HighlightEvents.Num(); Idx++)
		{
			const INT EventID = HighlightEvents(Idx);
			const FGameplayEventMetaData& EventData = Aggregator->GetEventMetaData(EventID);
			if (EventData.EventID != INDEX_NONE)
			{
				FCategory Metadata(EVENT_METATAG, EventData.EventName.ToString());
				Metadata.Id = EventData.EventID;
				Category.SubCategories.AddItem(Metadata);
			}
		}

		Category_ToXml(Ar, Category, IndentCount);
	}
}

/*
 *   Writes out game stats to the stats report
 * @param Ar - archive to write to
 * @param IndentCount - number of tabs to prepend to the xml output
 */
void UGameStatsReport::WriteGameValues(FArchive& Ar, INT IndentCount)
{

}

/*
 *   Fills in a xml structure with all relevant stats data for a single player
 * @param Player - structure to append all player stats info to
 * @param PlayerIndex - reference to the player index that we are currently writing stats for
 */
void UGameStatsReport::WriteTeamValue(FCategory& Team, INT TeamIndex)
{
	INT NumTimePeriods = Aggregator->GameState->MaxRoundNumber + 1;

	FTeamEvents& TeamEvents = Aggregator->GetTeamEvents(TeamIndex);

	for (INT TimePeriodIter = 0; TimePeriodIter < NumTimePeriods; TimePeriodIter++ )
	{
		FCategory TimePeriodCategory(TOTALS_METATAG, TimePeriodIter == 0 ? GAMEPERIOD_METATAG : FString::Printf(ROUNDPERIOD_METATAG, TimePeriodIter));
		
		// Total team / player events
		::GetEventKeyValuePairs(TeamStatsColumns, TeamEvents.TotalEvents, TimePeriodIter, TimePeriodCategory.KeyValuePairs);
		// Total player weapon events
		FCategory WeaponsCategory(TOTALS_METATAG, TEXT("Weapons"));
		GetWeaponValuesForTimePeriod(WeaponsCategory, TimePeriodIter, TeamEvents.WeaponEvents, StatsFileReader);
		TimePeriodCategory.SubCategories.AddItem(WeaponsCategory);
		// Total player damage as player events
		FCategory DamageAsPlayerCategory(TOTALS_METATAG, TEXT("DamageAsPlayer"));
		GetDamageValuesForTimePeriod(DamageAsPlayerCategory, TimePeriodIter, TeamEvents.DamageAsPlayerEvents, StatsFileReader);
		TimePeriodCategory.SubCategories.AddItem(DamageAsPlayerCategory);
		// Total player damage as target events
		FCategory DamageAsTargetCategory(TOTALS_METATAG, TEXT("DamageAsTarget"));
		GetDamageValuesForTimePeriod(DamageAsTargetCategory, TimePeriodIter, TeamEvents.DamageAsTargetEvents, StatsFileReader);
		TimePeriodCategory.SubCategories.AddItem(DamageAsTargetCategory);
		// Total player projectile events
		FCategory ProjectilesCategory(TOTALS_METATAG, TEXT("Projectiles"));
		GetProjectileValuesForTimePeriod(ProjectilesCategory, TimePeriodIter, TeamEvents.ProjectileEvents, StatsFileReader);
		TimePeriodCategory.SubCategories.AddItem(ProjectilesCategory);
		// Total player pawn events
		FCategory PawnsCategory(TOTALS_METATAG, TEXT("Pawns"));
		GetPawnValuesForTimePeriod(PawnsCategory, TimePeriodIter, TeamEvents.PawnEvents, StatsFileReader);
		TimePeriodCategory.SubCategories.AddItem(PawnsCategory);
		
		Team.SubCategories.AddItem(TimePeriodCategory);
	}
}

/*
 *   Writes out team stats to the stats report
 * @param Ar - archive to write to
 * @param IndentCount - number of tabs to prepend to the xml output
 */
void UGameStatsReport::WriteTeamValues(FArchive& Ar, INT IndentCount)
{
	FCategory Category(TOTALS_METATAG, TEXT("AllTeams"));

	for (INT TeamIter=0; TeamIter < StatsFileReader->TeamList.Num(); TeamIter++)
	{
		const FTeamInformation& TeamInfo = StatsFileReader->GetTeamMetaData(TeamIter);

		FCategory Team(TEAM_METATAG, *TeamInfo.TeamName);
		Team.Id = TeamIter;

		WriteTeamValue(Team, TeamIter);

		Category.SubCategories.AddItem(Team);
	}

	Category_ToXml(Ar, Category, IndentCount);
}

/*
 *   Fills in a xml structure with all relevant stats data for a single player
 * @param Player - structure to append all player stats info to
 * @param PlayerIndex - reference to the player index that we are currently writing stats for
 */
void UGameStatsReport::WritePlayerValue(FCategory& Player, INT PlayerIndex)
{
	INT NumTimePeriods = Aggregator->GameState->MaxRoundNumber + 1;

	FPlayerEvents& PlayerEvents = Aggregator->GetPlayerEvents(PlayerIndex);

	for (INT TimePeriodIter = 0; TimePeriodIter < NumTimePeriods; TimePeriodIter++ )
	{
		FCategory TimePeriodCategory(TOTALS_METATAG, TimePeriodIter == 0 ? GAMEPERIOD_METATAG : FString::Printf(ROUNDPERIOD_METATAG, TimePeriodIter));
		
		// Total player events
		::GetEventKeyValuePairs(PlayerStatsColumns, PlayerEvents.TotalEvents, TimePeriodIter, TimePeriodCategory.KeyValuePairs);
		// Total player weapon events
		FCategory WeaponsCategory(TOTALS_METATAG, TEXT("Weapons"));
		GetWeaponValuesForTimePeriod(WeaponsCategory, TimePeriodIter, PlayerEvents.WeaponEvents, StatsFileReader);
		TimePeriodCategory.SubCategories.AddItem(WeaponsCategory);
		// Total player damage as player events
		FCategory DamageAsPlayerCategory(TOTALS_METATAG, TEXT("DamageAsPlayer"));
		GetDamageValuesForTimePeriod(DamageAsPlayerCategory, TimePeriodIter, PlayerEvents.DamageAsPlayerEvents, StatsFileReader);
		TimePeriodCategory.SubCategories.AddItem(DamageAsPlayerCategory);
		// Total player damage as target events
		FCategory DamageAsTargetCategory(TOTALS_METATAG, TEXT("DamageAsTarget"));
		GetDamageValuesForTimePeriod(DamageAsTargetCategory, TimePeriodIter, PlayerEvents.DamageAsTargetEvents, StatsFileReader);
		TimePeriodCategory.SubCategories.AddItem(DamageAsTargetCategory);
		// Total player projectile events
		FCategory ProjectilesCategory(TOTALS_METATAG, TEXT("Projectiles"));
		GetProjectileValuesForTimePeriod(ProjectilesCategory, TimePeriodIter, PlayerEvents.ProjectileEvents, StatsFileReader);
		TimePeriodCategory.SubCategories.AddItem(ProjectilesCategory);
		// Total player pawn events
		FCategory PawnsCategory(TOTALS_METATAG, TEXT("Pawns"));
		GetPawnValuesForTimePeriod(PawnsCategory, TimePeriodIter, PlayerEvents.PawnEvents, StatsFileReader);
		TimePeriodCategory.SubCategories.AddItem(PawnsCategory);
		
		Player.SubCategories.AddItem(TimePeriodCategory);
	}
}

/*
 *   Writes out all player stats for the stats report
 * @param Ar - archive to write to
 * @param IndentCount - number of tabs to prepend to the xml output
 */
void UGameStatsReport::WritePlayerValues(FArchive& Ar, INT IndentCount)
{
	FCategory Category(TOTALS_METATAG, TEXT("AllPlayers"));

	for (INT TeamIter=0; TeamIter<StatsFileReader->TeamList.Num(); TeamIter++)
	{
	   const FTeamState* TeamState = Aggregator->GameState->GetTeamState(StatsFileReader->TeamList(TeamIter).TeamIndex);
	   for (INT PlayerIter=0; PlayerIter<TeamState->PlayerIndices.Num(); PlayerIter++)
	   {
		   const INT PlayerIndex = TeamState->PlayerIndices(PlayerIter);
		   const FPlayerInformation& PlayerInfo = StatsFileReader->GetPlayerMetaData(PlayerIndex);

		   FCategory Player(PLAYER_METATAG, *PlayerInfo.PlayerName);
		   Player.Id = PlayerIndex;

		   WritePlayerValue(Player, PlayerIndex);

		   Category.SubCategories.AddItem(Player);
	   }
	}
	
	Category_ToXml(Ar, Category, IndentCount);
}

/*
 *   Writes out all weapon stats for the stats report
 * @param Ar - archive to write to
 * @param IndentCount - number of tabs to prepend to the xml output
 */
void UGameStatsReport::WriteWeaponValues(FArchive& Ar, INT IndentCount)
{
	INT NumTimePeriods = Aggregator->GameState->MaxRoundNumber + 1;

	FCategory Category(TOTALS_METATAG, TEXT("AllWeapons"));
	for (INT TimePeriodIter = 0; TimePeriodIter < NumTimePeriods; TimePeriodIter++ )
	{
		FCategory TimePeriodCategory(TOTALS_METATAG, TimePeriodIter == 0 ? GAMEPERIOD_METATAG : FString::Printf(ROUNDPERIOD_METATAG, TimePeriodIter));
		GetWeaponValuesForTimePeriod(TimePeriodCategory, TimePeriodIter, Aggregator->AllWeaponEvents, StatsFileReader);
		Category.SubCategories.AddItem(TimePeriodCategory);
	}

	Category_ToXml(Ar, Category, IndentCount);
}

/*
 *   Writes out all damage stats for the stats report
 * @param Ar - archive to write to
 * @param IndentCount - number of tabs to prepend to the xml output
 */
void UGameStatsReport::WriteDamageValues(FArchive& Ar, INT IndentCount)
{
	INT NumTimePeriods = Aggregator->GameState->MaxRoundNumber + 1;

	FCategory Category(TOTALS_METATAG, TEXT("AllDamage"));
	for (INT TimePeriodIter = 0; TimePeriodIter < NumTimePeriods; TimePeriodIter++ )
	{
		FCategory TimePeriodCategory(TOTALS_METATAG, TimePeriodIter == 0 ? GAMEPERIOD_METATAG : FString::Printf(ROUNDPERIOD_METATAG, TimePeriodIter));
		GetDamageValuesForTimePeriod(TimePeriodCategory, TimePeriodIter, Aggregator->AllDamageEvents, StatsFileReader);
		Category.SubCategories.AddItem(TimePeriodCategory);
	}

	Category_ToXml(Ar, Category, IndentCount);
}

/*
 *   Writes out all projectile stats for the stats report
 * @param Ar - archive to write to
 * @param IndentCount - number of tabs to prepend to the xml output
 */
void UGameStatsReport::WriteProjectileValues(FArchive& Ar, INT IndentCount)
{
	INT NumTimePeriods = Aggregator->GameState->MaxRoundNumber + 1;

	FCategory Category(TOTALS_METATAG, TEXT("AllProjectiles"));
	for (INT TimePeriodIter = 0; TimePeriodIter < NumTimePeriods; TimePeriodIter++ )
	{
		FCategory TimePeriodCategory(TOTALS_METATAG, TimePeriodIter == 0 ? GAMEPERIOD_METATAG : FString::Printf(ROUNDPERIOD_METATAG, TimePeriodIter));
		GetProjectileValuesForTimePeriod(TimePeriodCategory, TimePeriodIter, Aggregator->AllProjectileEvents, StatsFileReader);
		Category.SubCategories.AddItem(TimePeriodCategory);
	}

	Category_ToXml(Ar, Category, IndentCount);
}

/*
 *   Writes out all pawn stats for the stats report
 * @param Ar - archive to write to
 * @param IndentCount - number of tabs to prepend to the xml output
 */
void UGameStatsReport::WritePawnValues(FArchive& Ar, INT IndentCount)
{
	INT NumTimePeriods = Aggregator->GameState->MaxRoundNumber + 1;

	FCategory Category(TOTALS_METATAG, TEXT("AllPawns"));
	for (INT TimePeriodIter = 0; TimePeriodIter < NumTimePeriods; TimePeriodIter++ )
	{
		FCategory TimePeriodCategory(TOTALS_METATAG, TimePeriodIter == 0 ? GAMEPERIOD_METATAG : FString::Printf(ROUNDPERIOD_METATAG, TimePeriodIter));
		GetPawnValuesForTimePeriod(TimePeriodCategory, TimePeriodIter, Aggregator->AllPawnEvents, StatsFileReader);
		Category.SubCategories.AddItem(TimePeriodCategory);
	}

	Category_ToXml(Ar, Category, IndentCount);
}

/************************************************************************/
/* FGameStatsReportWorkerFile                                           */
/************************************************************************/
FGameStatsReportWorkerFile::FGameStatsReportWorkerFile() : ReportWriter(NULL)
{

}

FGameStatsReportWorkerFile::~FGameStatsReportWorkerFile()
{

}

/*
 *  Initialize the stats report writer
 * @param InStatsFilename - game stats file to create a report for
 */
UBOOL FGameStatsReportWorkerFile::Init(const FString& InStatsFilename)
{
	UBOOL bSuccess = TRUE;

	if (InStatsFilename.IsEmpty())
	{
		debugf(TEXT("No filename specified for the report commandlet"));
		bSuccess = FALSE;
	}
	else
	{
		StatsFilename = InStatsFilename;
	}

	if(!GConfig->GetString( TEXT("UnrealEd.GameStatsReport"), TEXT("GameStatsGameStateClassName"), GameStatsGameStateClassName, GEditorIni))
	{
		debugf(TEXT("Failed to find the game state class config value for the report commandlet"));
		bSuccess = FALSE;
	}

	if(!GConfig->GetString( TEXT("UnrealEd.GameStatsReport"), TEXT("GameStatsAggregatorClassName"), GameStatsAggregatorClassName, GEditorIni))
	{
		debugf(TEXT("Failed to find the aggregator class config value for the report commandlet"));
		bSuccess = FALSE;
	}	  

	if(!GConfig->GetString( TEXT("UnrealEd.GameStatsReport"), TEXT("GameStatsReportWriterClassName"), GameStatsReportWriterClassName, GEditorIni))
	{
		debugf(TEXT("Failed to find the report writer class config value for the report commandlet"));
		bSuccess = FALSE;
	}

	return bSuccess;
}

/*
 *   Main body of the stats report writing class
 * Opens and parses a gamestats file then outputs a	stats report
 */
INT FGameStatsReportWorkerFile::Main()
{
	INT Result = -1;

	// Load the writer class
	UClass* GameStatsReportClass = LoadClass<UGameStatsReport>(NULL, *GameStatsReportWriterClassName, NULL, LOAD_None, NULL);
	if (GameStatsReportClass != NULL)
	{
		ReportWriter = ConstructObject<UGameStatsReport>(GameStatsReportClass);
		if (ReportWriter != NULL && ReportWriter->IsA(UGameStatsReport::StaticClass()))
		{
			// Load the aggregator class
			UClass* GameStatsAggregatorClass = LoadClass<UGameStatsAggregator>(NULL, *GameStatsAggregatorClassName, NULL, LOAD_None, NULL);
			UClass* GameStateObjectClass = LoadClass<UGameStateObject>(NULL, *GameStatsGameStateClassName, NULL, LOAD_None, NULL);
			if (GameStatsAggregatorClass != NULL && GameStateObjectClass != NULL)
			{
				ReportWriter->StatsFileReader = ConstructObject<UGameplayEventsReader>(UGameplayEventsReader::StaticClass());
				ReportWriter->GameState = ConstructObject<UGameStateObject>(GameStateObjectClass);
				ReportWriter->Aggregator = ConstructObject<UGameStatsAggregator>(GameStatsAggregatorClass);
				if (ReportWriter->Aggregator != NULL && ReportWriter->Aggregator->IsA(UGameStatsAggregator::StaticClass()))
				{
					ReportWriter->Aggregator->SetGameState(ReportWriter->GameState);

					// Put game state first so it gives other handlers a chance to use that state data
					ReportWriter->StatsFileReader->eventRegisterHandler(ReportWriter->GameState);
					ReportWriter->StatsFileReader->eventRegisterHandler(ReportWriter->Aggregator);
					if (ReportWriter->StatsFileReader->OpenStatsFile(StatsFilename) == TRUE)
					{
						// Copy the session info
						ReportWriter->SessionInfo = ReportWriter->StatsFileReader->GetSessionInfo();
						// Process the data
						ReportWriter->StatsFileReader->ProcessStream();

						// Write the report
						FArchive* Ar = GFileManager->CreateFileWriter( *ReportWriter->GetReportFilename(GAME_STATS_REPORT_FILE_EXT), 0 );
						if (Ar)
						{
							ReportWriter->WriteReport(*Ar);
							Ar->Close();
							delete Ar;

							Result = 0;
						}

						ReportWriter->StatsFileReader->CloseStatsFile();
					}
					else
					{
						debugf(TEXT("Unable to open the report file %s for writing."), *ReportWriter->GetReportFilename(GAME_STATS_REPORT_FILE_EXT));
					}

					ReportWriter->StatsFileReader->eventUnregisterHandler(ReportWriter->GameState);
					ReportWriter->StatsFileReader->eventUnregisterHandler(ReportWriter->Aggregator);
				}
			}
			else
			{
				debugf(TEXT("Failed to find the game state class %s or aggregator class %s for the report commandlet"), *GameStatsGameStateClassName, *GameStatsAggregatorClassName);
			}
		}
	}
	else
	{
		debugf(TEXT("Failed to find the report writer class %s for the report commandlet"), *GameStatsReportWriterClassName);
	}

	return Result;
}

/************************************************************************/
/* FGameStatsReportWorkerDB                                             */
/************************************************************************/

/*
 *  Initialize the stats report writer
 * @param InSessionInfo - game stats session to create a report for
 */
UBOOL FGameStatsReportWorkerDB::Init(const FGameSessionInformation& InSessionInfo)
{
	UBOOL bSuccess = TRUE;
	if(!GConfig->GetString( TEXT("UnrealEd.GameStatsReport"), TEXT("GameStatsReportWriterClassName"), GameStatsReportWriterClassName, GEditorIni))
	{
		debugf(TEXT("Failed to find the report writer class config value for the report commandlet"));
		bSuccess = FALSE;
	}

	SessionInfo = InSessionInfo;
	return bSuccess;
}

/*
 *  Main body of the stats report writing class
 * DB implementation doesn't do much, all the work is on the backend
 */
INT FGameStatsReportWorkerDB::Main()
{		
	INT Result = -1;

	// Load the writer class
	UClass* GameStatsReportClass = LoadClass<UGameStatsReport>(NULL, *GameStatsReportWriterClassName, NULL, LOAD_None, NULL);
	if (GameStatsReportClass != NULL)
	{
		ReportWriter = ConstructObject<UGameStatsReport>(GameStatsReportClass);
		if (ReportWriter != NULL && ReportWriter->IsA(UGameStatsReport::StaticClass()))
		{
			ReportWriter->SessionInfo = SessionInfo;
			Result = 0; 
		}
	}
	else
	{
		debugf(TEXT("Failed to find the report writer class %s for the report commandlet"), *GameStatsReportWriterClassName);
	}

	return Result;
}

/**
 * Get an URL related this report
 * @param ReportType - report type to generate 
 * @return URL passed to a browser to view the report 
 */
FString FGameStatsReportWorkerDB::GetReportURL(EReportType ReportType)
{
	if (ReportWriter && !ReportWriter->ReportBaseURL.IsEmpty())
	{
		FString SessionGUID;
		INT InstanceID;
		if (ParseSessionID(SessionInfo.GetSessionID(), SessionGUID, InstanceID))
		{
			FString URLPrefix = ReportWriter->ReportBaseURL + PATH_SEPARATOR + GGameName + TEXT("Game");
			switch (ReportType)
			{
			case RT_Game:
				return URLPrefix + PATH_SEPARATOR + TEXT("Game") + PATH_SEPARATOR + SessionGUID;
			case RT_SingleSession:
				return URLPrefix + PATH_SEPARATOR + TEXT("Match") + PATH_SEPARATOR + appItoa(InstanceID);
			}
		}
	}

	return TEXT("");
}

/************************************************************************/
/* UWriteGameStatsReportCommandlet                                      */
/************************************************************************/
IMPLEMENT_CLASS(UWriteGameStatsReportCommandlet);
INT UWriteGameStatsReportCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	if (Tokens.Num() == 1)
	{
		FString ReportFilename = Tokens(0);

		Worker = new FGameStatsReportWorkerFile;
		if (Worker->Init(ReportFilename))
		{
			INT Result = Worker->Main();
		}

		delete Worker;
		Worker = NULL;
	}

	return 0;
}

#undef warnexprf