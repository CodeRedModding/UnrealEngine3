/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * A configuration class used by the Material Editor to save editor
 * settings across sessions.
 */

class MaterialEditorOptions extends Object	
	hidecategories(Object)
	config(EditorUserSettings)
	native;	

/** If TRUE, render grid the preview scene. */
var(Options)	config bool			bShowGrid;

/** If TRUE, render background object in the preview scene. */
var(Options)	config bool			bShowBackground;

/** If TRUE, don't render connectors that are not connected to anything. */
var(Options)	config bool			bHideUnusedConnectors;

/** If TRUE, draw connections with splines.  If FALSE, use straight lines. */
var(Options) deprecated	config bool			bDrawCurves;

/** If TRUE, the 3D material preview viewport updates in realtime. */
var(Options)	config bool			bRealtimeMaterialViewport;

/** If TRUE, the linked object viewport updates in realtime. */
var(Options)	config bool			bRealtimeExpressionViewport;

/** If TRUE, always refresh all expression previews. */
var(Options)	config bool			bAlwaysRefreshAllPreviews;

/** If TRUE, use expression categorized menus. */
var(Options) deprecated	config bool			bUseSortedMenus;

/** If FALSE, use expression categorized menus. */
var(Options)	config bool			bUseUnsortedMenus;

/** The users favorite material expressions. */
var(Options)	config array<string>	FavoriteExpressions;
