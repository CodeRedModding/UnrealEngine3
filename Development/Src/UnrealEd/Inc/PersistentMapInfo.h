/**
 *	This is a helper class - PersistentMapInfo
 *	It creates a maintains lists of:
 *		PMaps-->Levels
 *		Levels-->PMap that 'owns' them
 *	This is used during cooking to generate persistent facefx sets,
 *	as well as 
 * 
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _PERSISTENTMAPINFO_H_
#define _PERSISTENTMAPINFO_H_

class FPersistentMapInfo
{
public:
	enum EVerboseLevel
	{
		VL_Silent,
		VL_Simple,
		VL_Verbose
	};
	/** Garbage Collection Function */
	typedef void (*_GarbageCollectFn)();

	/** Default Garbage Collection Function */
	static void DefaultGarbageCollectFn()
	{
		UObject::CollectGarbage(RF_Native);
	}

	/** Constructor */
	FPersistentMapInfo()
	{
		GenerationVerboseLevel = VL_Silent;
		GarbageCollectFn = DefaultGarbageCollectFn;
		CallerCommandlet = NULL;
	}

	/************************************************************************/
	/*  Main functions                                                      */
	/************************************************************************/
	/**
	 *	Generate the mappings of sub-levels to PMaps... (LevelToPersistentLevelsMap)
	 *
	 *	@param	MapList			The array of persistent map names to analyze
	 *	@param	bClearExisting	If TRUE, clear all existing entries
	 *	@param	bPMapsOnly		If TRUE, the list contains ONLY PMaps
	 *							If FALSE, the list can contain any maps (ie sub-levels of PMaps)
	 */
	void GeneratePersistentMapList(const TArray<FString>& MapList, UBOOL bClearExisting, UBOOL bPMapsOnly);

	/************************************************************************/
	/*  Utility functions                                                   */
	/************************************************************************/
	/** Set Caller class information */
	void SetCallerInfo(UCommandlet* InCommandlet, _GarbageCollectFn InGarbageCollectFn = NULL);

	/** See/Get if we should Log or not */
	void SetPersistentMapInfoGenerationVerboseLevel(EVerboseLevel InGenerationVerboseLevel)
	{
		GenerationVerboseLevel = InGenerationVerboseLevel;
	}
	const EVerboseLevel GetLogLevelPersistentMapInfoGeneration()
	{
		return GenerationVerboseLevel;
	}
	const UBOOL GetLogPersistentMapInfoGeneration()
	{
		return (GenerationVerboseLevel != VL_Silent);
	}

	/**
	 *	Return the PMap name for the given level.
	 *
	 *	@param	InLevelName		The level to retrieve the name of the PMap that contains it
	 *	@param	OutPMapName		The resulting PMap name that contains the given level
	 *
	 *	@return	UBOOL			TRUE if found, FALSE if not
	 */
	UBOOL GetPersistentMapForLevel(const FString& InLevelName, FString& OutPMapName) const;

	/**
	 *	Return the array of all PMap names the given level is contained in.
	 *
	 *	@param	InLevelName		The level to retrieve the name of the PMap that contains it
	 *	@param	OutPMaps		The array to fill in for the PMaps that contain the given level
	 *
	 *	@return	UBOOL			TRUE if found, FALSE if not
	 */
	UBOOL GetPersistentMapsForLevel(const FString& InLevelName, TArray<FString>& OutPMaps) const;

	/**
	 *	Get the list of contained levels in a given persistent level
	 *
	 *	@param	InPMapName			The name of the persistent level
	 *
	 *	@return	TArray<FString>*	The contained levels if found, NULL if not
	 */
	const TArray<FString>* GetPersistentMapContainedLevelsList(const FString& InPMapName) const;

	/**
	 *	Get a list of PMaps
	 */
	UBOOL GetPersistentMapList(TArray<FString>& OutPMapList);

	/**
	 *	Get the alias PMap for a level
	 */
	UBOOL GetPersistentMapAlias(const FString& InLevelName, FString& OutPMapAlias) const;

private: 
	/** Mapping of Sublevels to their parent Persistent map */
	TMap<FString, TArray<FString> > LevelToPersistentLevelsMap;
	/** Mapping persistent maps to all sublevels they contain */
	TMap<FString, TArray<FString> > PersistentLevelToContainedLevelsMap;
	/** Mapping PMaps to an alias PMap */
	TMap<FString, FString> PMapAliasMap;

	/** This is the CallerCommandlet and GarbageCollect function */
	UCommandlet * CallerCommandlet;
	_GarbageCollectFn GarbageCollectFn;

	/** 
	 *	The verbose level for the PMap Info generation
	 *	Pass "-LOGPMAPGEN" on the commandline for simple
	 *		 "-LOGPMAPGENX" for verbose
	 */
	EVerboseLevel GenerationVerboseLevel;
};

#endif//_PERSISTENTMAPINFO_H_
