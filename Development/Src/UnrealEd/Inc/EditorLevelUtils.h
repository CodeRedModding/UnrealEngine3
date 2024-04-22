/*=============================================================================
	EditorLevelUtils.h: Editor-specific level management routines
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if _MSC_VER
#pragma once
#endif

#ifndef __EditorLevelUtils_h__
#define __EditorLevelUtils_h__


namespace EditorLevelUtils
{
	/**
	 * Wrapper struct for a level grid volume and a single cell within that volume
	 */
	struct FGridAndCellCoordinatePair
	{
		/** The level grid volume */
		ALevelGridVolume* GridVolume;

		/** Cell within the volume */
		FLevelGridCellCoordinate CellCoordinate;


		/** Constructor */
		FGridAndCellCoordinatePair()
			: GridVolume( NULL ),
			  CellCoordinate()
		{
		}

		
		/** Equality operator */
		UBOOL operator==( const FGridAndCellCoordinatePair& RHS ) const
		{
			return ( RHS.GridVolume == GridVolume && RHS.CellCoordinate == CellCoordinate );
		}
	};

	
	/**
	 * Returns true if we're current in the middle of updating levels for actors.  This is used to
	 * prevent re-entrancy problems where 'actor moved' callbacks and fire off as we're moving
	 * actors between levels and such.
	 *
	 * @return	True if we're currently updating levels
	 */
	UBOOL IsCurrentlyUpdatingLevelsForActors();


	/**
	 * Creates a new level for the specified grid cell
	 *
	 * @param	GridCell	The grid volume and cell to create a level for
	 *
	 * @return	The newly-created level, or NULL on failure
	 */
	ULevel* CreateLevelForGridCell( FGridAndCellCoordinatePair& GridCell );


	/**
	 * Finds the level grid volume that an actor belongs to and returns it
	 *
	 * @param	InActor		The actor to find the level grid volume for
	 *
	 * @return	The level grid volume this actor belong to, or NULL if the actor doesn't belong to a level grid volume
	 */
	ALevelGridVolume* GetLevelGridVolumeForActor( AActor* InActor );


	/**
	 * For each actor in the specified list, checks to see if the actor needs to be moved to
	 * a different grid volume level, and if so queues the actor to be moved
	 *
	 * @param	InActorsToProcess	List of actors to update level membership for.  Note that this list may be modified by the function.
	 */
	void UpdateLevelsForActorsInLevelGridVolumes( TArray< AActor* >& InActorsToProcess );


	/**
	 * Scans all actors in memory and checks to see if each actor needs to be moved to a different
	 * grid volume level, and if so queues the actor to be moved
	 */
	void UpdateLevelsForAllActors();

	/**
	 * Moves the specified list of actors to the specified level
	 *
	 * @param	ActorsToMove		List of actors to move
	 * @param	DestLevelStreaming	The level streaming object associated with the destination level
	 * @param	OutNumMovedActors	The number of actors that were successfully moved to the new level
	 */
	void MovesActorsToLevel( TLookupMap< AActor* >& ActorsToMove, ULevelStreaming* DestLevelStreaming, INT& OutNumMovedActors );


	/**
	 * Adds the named level package to the world.  Does nothing if the level already exists in the world.
	 *
	 * @param	LevelPackageBaseFilename	The base filename of the level package to add.
	 * @param	OverrideLevelStreamingClass	Unless NULL, forces a specific level streaming class type instead of prompting the user to select one
	 *
	 * @return								The new level, or NULL if the level couldn't added.
	 */
	ULevel* AddLevelToWorld(const TCHAR* LevelPackageBaseFilename, UClass* OverrideLevelStreamingClass = NULL);

	/**
	 * Removes the specified level from the world.  Refreshes.
	 *
	 * @return	TRUE	If a level was removed.
	 */
	UBOOL RemoveLevelFromWorld(ULevel* InLevel);


	/**
	 * Creates a new streaming level.
	 *
	 * @param	bMoveSelectedActorsIntoNewLevel		If TRUE, move any selected actors into the new level.
	 * @param	DefaultFilename						Optional file name for level.  If empty, the user will be prompted during the save process.
	 * @param	OverrideLevelStreamingClass	Unless NULL, forces a specific level streaming class type instead of prompting the user to select one
	 * 
	 * @return	Returns the newly created level, or NULL on failure
	 */
	ULevel* CreateNewLevel(UBOOL bMoveSelectedActorsIntoNewLevel, const FString& DefaultFilename = TEXT( "" ), UClass* OverrideLevelStreamingClass = NULL );


	/**
	 * Given a location for the new actor, determines which level the actor would best be placed in.
	 * This uses the world's 'current' level and grid volume when selecting a level.
	 *
	 * @param	InActorLocation				Location of the actor that we're interested in finding a level for
	 * @param	bUseCurrentLevelGridVolume	True if we should try to place the actor into a level associated with the world's 'current' level grid volume, if one is set
	 *
	 * @return	The best level to place the new actor into
	 */
	ULevel* GetLevelForPlacingNewActor( const FVector& InActorLocation, const UBOOL bUseCurrentLevelGridVolume = TRUE );

}




#endif	// __EditorLevelUtils_h__
