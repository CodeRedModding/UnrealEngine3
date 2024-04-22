/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
//=============================================================================
// TerrainEditOptions
//
// A configuration class that holds information for the setup of TerrainEditing.
// Supplied so that the editor 'remembers' the last setup the user had.
//=============================================================================
class TerrainEditOptions extends Object	
	hidecategories(Object)
	config(EditorUserSettings)
	native;	

// TerrainEdit
var(Options)		config int			Solid1_Strength;
var(Options)		config int			Solid1_Radius;
var(Options)		config int			Solid1_Falloff;
var(Options)		config int			Solid2_Strength;
var(Options)		config int			Solid2_Radius;
var(Options)		config int			Solid2_Falloff;
var(Options)		config int			Solid3_Strength;
var(Options)		config int			Solid3_Radius;
var(Options)		config int			Solid3_Falloff;
var(Options)		config int			Solid4_Strength;
var(Options)		config int			Solid4_Radius;
var(Options)		config int			Solid4_Falloff;
var(Options)		config int			Solid5_Strength;
var(Options)		config int			Solid5_Radius;
var(Options)		config int			Solid5_Falloff;
var(Options)		config int			Noisy1_Strength;
var(Options)		config int			Noisy1_Radius;
var(Options)		config int			Noisy1_Falloff;
var(Options)		config int			Noisy2_Strength;
var(Options)		config int			Noisy2_Radius;
var(Options)		config int			Noisy2_Falloff;
var(Options)		config int			Noisy3_Strength;
var(Options)		config int			Noisy3_Radius;
var(Options)		config int			Noisy3_Falloff;
var(Options)		config int			Noisy4_Strength;
var(Options)		config int			Noisy4_Radius;
var(Options)		config int			Noisy4_Falloff;
var(Options)		config int			Noisy5_Strength;
var(Options)		config int			Noisy5_Radius;
var(Options)		config int			Noisy5_Falloff;

var(Options)		config int			Current_Tool;

var(Options)		config int			Current_Brush;
var(Options)		config int			Current_Strength;
var(Options)		config int			Current_Radius;
var(Options)		config int			Current_Falloff;
var(Options)		config bool			bSoftSelectEnabled;
var(Options)		config bool			bConstrainedEditing;

var(Options)		config int			Current_MirrorFlag;

var(Options)		config int			SliderRange_Low_Strength;
var(Options)		config int			SliderRange_High_Strength;
var(Options)		config int			SliderRange_Low_Radius;
var(Options)		config int			SliderRange_High_Radius;
var(Options)		config int			SliderRange_Low_Falloff;
var(Options)		config int			SliderRange_High_Falloff;

// TerrainLayerBrowser
var(Options)		config bool			bShowDecoarationMeshes;
var(Options)		config color		TerrainLayerBrowser_BackgroundColor;
var(Options)		config color		TerrainLayerBrowser_BackgroundColor2;
var(Options)		config color		TerrainLayerBrowser_BackgroundColor3;
var(Options)		config color		TerrainLayerBrowser_SelectedColor;
var(Options)		config color		TerrainLayerBrowser_SelectedColor2;
var(Options)		config color		TerrainLayerBrowser_SelectedColor3;
var(Options)		config color		TerrainLayerBrowser_BorderColor;

defaultproperties
{
}
