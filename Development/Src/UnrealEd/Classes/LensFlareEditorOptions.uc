/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
//=============================================================================
// LensFlareEditorOptions
//
// A configuration class that holds information for the setup of the LensFlare editor.
// Supplied so that the editor 'remembers' the last setup the user had.
//=============================================================================
class LensFlareEditorOptions extends Object	
	hidecategories(Object)
	config(EditorUserSettings)
	native;	

var(Options)		config linearcolor	LFED_BackgroundColor;
var(Options)		config linearcolor	LFED_Empty_Background;
var(Options)		config linearcolor	LFED_Source_ElementEd_Background;
var(Options)		config linearcolor	LFED_Source_Unselected;
var(Options)		config linearcolor	LFED_Source_Selected;
var(Options)		config linearcolor	LFED_ElementEd_Background;
var(Options)		config linearcolor	LFED_Element_Unselected;
var(Options)		config linearcolor	LFED_Element_Selected;

var(Options)		config bool			bShowGrid;
var(Options)		config color		GridColor_Hi;
var(Options)		config color		GridColor_Low;
var(Options)		config float		GridPerspectiveSize;

var(Options)		config string		PostProcessChainName;

var(Options)		config int			ShowPPFlags;

defaultproperties
{
}
