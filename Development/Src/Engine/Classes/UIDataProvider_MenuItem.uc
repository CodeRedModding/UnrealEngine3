/**
 * Provides all required information for dynmically generating a widget to be placed into a scene.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UIDataProvider_MenuItem extends UIResourceDataProvider
	native(inherit)
	config(UI)
	PerObjectConfig;

enum EMenuOptionType
{
	MENUOT_ComboReadOnly,
	MENUOT_ComboNumeric,
	MENUOT_CheckBox,
	MENUOT_Slider,
	MENUOT_Spinner,
	MENUOT_EditBox,
	MENUOT_CollectionCheckBox,
	MENUOT_CollapsingList,
};

var config EMenuOptionType OptionType;

/** Name of the option set that this option belongs to. */
var config array<name> OptionSet;

/** Markup for the option */
var config string DataStoreMarkup;

/** Markup for the description for the option */
var config string DescriptionMarkup;

/** Game mode required for this option to appear. */
var config name RequiredGameMode;

/** Friendly displayable name to the player. */
var config localized string FriendlyName;

/** Script settable friendly name. */
var string CustomFriendlyName;

/** Localized description of the option */
var config localized string Description;

/** Whether or not the options presented to the user are the only options they can choose from, used on PC only for setting whether combobox edit boxes are read only or not. */
var config bool bEditableCombo;

/** Whether or not the combobox is numeric. */
var config bool	bNumericCombo;

/** Maximum length of the editbox property. */
var config int EditBoxMaxLength;

/** Range data for the option, only used if its a slider type. */
var config UIRangeData	RangeData;

/** the names of the fields to use for populating schemas for list widgets */
var	config	array<name>	SchemaCellFields;

/** Whether the option is a keyboard or mouse option. */
var config bool	bKeyboardOrMouseOption;

/** Whether the option is a online only option or not. */
var config bool bOnlineOnly;

/** Whether the option is a offline only option or not. */
var config bool bOfflineOnly;

/* === From UTUIResourceDataProvider === */

/** whether to search all .inis for valid resource provider instances instead of just the our specified config file
 * this is used for lists that need to support additions via extra files, i.e. mods
 */
var() bool bSearchAllInis;
/** the .ini file that this instance was created from, if not the class default .ini (for bSearchAllInis classes) */
var const string IniName;

/** Options to remove certain menu items on a per platform basis. */
var config bool bRemoveOn360;
var config bool bRemoveOnPC;
var config bool bRemoveOnPS3;


