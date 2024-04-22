/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
//=============================================================================
// CurveEdOptions
//
// A configuration class that holds information for the setup of the CurveEd.
// Supplied so that the editor 'remembers' the last setup the user had.
//=============================================================================
class CurveEdOptions extends Object	
	hidecategories(Object)
	config(EditorUserSettings)
	native;	

var(Options)		config float		MinViewRange;
var(Options)		config float		MaxViewRange;
var(Options)		config linearcolor	BackgroundColor;
var(Options)		config linearcolor	LabelColor;
var(Options)		config linearcolor	SelectedLabelColor;
var(Options)		config linearcolor	GridColor;
var(Options)		config linearcolor	GridTextColor;
var(Options)		config linearcolor	LabelBlockBkgColor;
var(Options)		config linearcolor	SelectedKeyColor;

defaultproperties
{
}
