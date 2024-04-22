/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Base class for stats read and write objects. Contains common structures
 * and methods used by both read and write objects.
 */
class OnlineStats extends Object
	native
	abstract;

/** Provides metadata view ids so that we can present their human readable form */
var const array<StringIdToStringMapping> ViewIdMappings;

/**
 * Searches the view id mappings to find the view id that matches the name
 *
 * @param ViewName the name of the view being searched for
 * @param ViewId the id of the view that matches the name
 *
 * @return true if it was found, false otherwise
 */
native function bool GetViewId(name ViewName,out int ViewId);

/**
 * Finds the human readable name for the view
 *
 * @param ViewId the id to look up in the mappings table
 *
 * @return the name of the view that matches the id or NAME_None if not found
 */
native function name GetViewName(int ViewId);
