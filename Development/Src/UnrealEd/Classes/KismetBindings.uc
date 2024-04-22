/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class KismetBindings extends Object
	native
	config(Editor);

struct native KismetKeyBind
{
	var config name		Key;
	var config bool		bControl;
	var config bool		bShift;
	var config name		SeqObjClassName;
};

var config array<KismetKeyBind>	Bindings;

/** Represent one style preset for a Kismet comment box. */
struct native KismetCommentPreset
{
	var config name		PresetName;
	var config int		BorderWidth;
	var config color	BorderColor;
	var config bool		bFilled;
	var config color	FillColor;
};

/** Array of comment style presets, specified in ini file. */
var editoronly config array<KismetCommentPreset> CommentPresets;