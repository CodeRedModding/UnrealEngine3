/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __LEVELUTILS_H__
#define __LEVELUTILS_H__

// Forward declarations.
class ULevel;
class ULevelStreaming;

/**
 * A set of static methods for common editor operations that operate on ULevel objects.
 */
class FLevelUtils
{
public:
	/**
	 * Assembles the set of all referenced worlds.
	 *
	 * @param	Worlds				[out] The set of referenced worlds.
	 * @param	bIncludeGWorld		If TRUE, include GWorld in the output list.
	 * @param	bOnlyEditorVisible	If TRUE, only sub-levels that should be visible in-editor are included
	 */
	static void GetWorlds(TArray<UWorld*>& OutWorlds, UBOOL bIncludeGWorld, UBOOL bOnlyEditorVisible = FALSE);

	///////////////////////////////////////////////////////////////////////////
	// Given a ULevel, find the corresponding ULevelStreaming.

	/**
	 * Returns the streaming level corresponding to the specified ULevel, or NULL if none exists.
	 *
	 * @param		Level		The level to query.
	 * @return					The level's streaming level, or NULL if none exists.
	 */
	static ULevelStreaming* FindStreamingLevel(ULevel* Level);

	/**
	 * Returns the streaming level by package name, or NULL if none exists.
	 *
	 * @param		PackageName		Name of the package containing the ULevel to query
	 * @return						The level's streaming level, or NULL if none exists.
	 */
	static ULevelStreaming* FindStreamingLevel(const TCHAR* PackageName);

	///////////////////////////////////////////////////////////////////////////
	// Whether a level's bounding box is visible in the editor.

	/**
	 * Queries for the bounding box visibility a level's UStreamLevel 
	 *
	 * @param	Level		The level to query.
	 * @return				TRUE if the level's bounding box is visible, FALSE otherwise.
	 */
	static UBOOL IsLevelBoundingBoxVisible(ULevel* Level);

	/**
	 * Toggles whether or not a level's bounding box is rendered in the editor in place
	 * of the level itself.
	 *
	 * @param	Level		The level to query.
	 */
	static void ToggleLevelBoundingBox(ULevel* Level);

	///////////////////////////////////////////////////////////////////////////

	/**
	 * Removes a level from the world.  Returns TRUE if the level was removed successfully.
	 *
	 * @param	Level		The level to remove from the world.
	 * @return				TRUE if the level was removed successfully, FALSE otherwise.
	 */
	static UBOOL RemoveLevelFromWorld(ULevel* Level);

	///////////////////////////////////////////////////////////////////////////
	// Locking/unlocking levels for edit.

	/**
	 * Returns TRUE if the specified level is locked for edit, FALSE otherwise.
	 *
	 * @param	Level		The level to query.
	 * @return				TRUE if the level is locked, FALSE otherwise.
	 */
	static UBOOL IsLevelLocked(ULevel* Level);
	static UBOOL IsLevelLocked(AActor* Actor);

	/**
	 * Sets a level's edit lock.
	 *
	 * @param	Level		The level to modify.
	 */
	static void ToggleLevelLock(ULevel* Level);

	///////////////////////////////////////////////////////////////////////////
	// Controls whether the level is loaded in editor.

	/**
	 * Returns TRUE if the level is currently loaded in the editor, FALSE otherwise.
	 *
	 * @param	Level		The level to query.
	 * @return				TRUE if the level is loaded, FALSE otherwise.
	 */
	static UBOOL IsLevelLoaded(ULevel* Level);

	/**
	 * Flags an unloaded level for loading.
	 *
	 * @param	Level		The level to modify.
	 */
	static void MarkLevelForLoading(ULevel* Level);

	/**
	 * Flags a loaded level for unloading.
	 *
	 * @param	Level		The level to modify.
	 */
	static void MarkLevelForUnloading(ULevel* Level);

	///////////////////////////////////////////////////////////////////////////
	// Level visibility.

	/**
	 * Returns TRUE if the specified level is visible in the editor, FALSE otherwise.
	 *
	 * @param	StreamingLevel		The level to query.
	 */
	static UBOOL IsLevelVisible(ULevelStreaming* StreamingLevel);

	/**
	 * Returns TRUE if the specified level is visible in the editor, FALSE otherwise.
	 *
	 * @param	Level		The level to query.
	 */
	static UBOOL IsLevelVisible(ULevel* Level);

	/**
	 * Sets a level's visibility in the editor.
	 *
	 * @param	StreamingLevel			The streaming level to modify (unless it's the pmap).
	 * @param	Level					The level to modify.
	 * @param	bShouldBeVisible		The level's new visibility state.
	 * @param	bForceLayersVisible		If TRUE and the level is visible, force the level's layers to be visible.
	 */
	static void SetLevelVisibility(ULevelStreaming* StreamingLevel, ULevel* Level, UBOOL bShouldBeVisible, UBOOL bForceLayersVisible);
};

#endif
