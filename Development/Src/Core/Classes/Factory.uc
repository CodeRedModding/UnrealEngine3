/**
 * Base class for all factories
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class Factory extends Object
	abstract
	noexport
	native;

var	class			SupportedClass;
var	class			ContextClass;
var	string			Description;
var	array<string>	Formats;
var	bool			bCreateNew;
var	bool			bEditAfterNew;
var	bool			bEditorImport;
var	bool			bText;
var	int				AutoPriority;

	/** List of game names that this factory can be used for (if empty, all games valid) */
var	array<string>	ValidGameNames;

defaultproperties
{
	Description = "";
}

