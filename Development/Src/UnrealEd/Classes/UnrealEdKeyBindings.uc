/**
 * This class handles hotkey binding management for the editor.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UnrealEdKeyBindings extends Object
	Config(EditorKeyBindings)
	native;

/** An editor hotkey binding to a parameterless exec. */
struct native EditorKeyBinding
{
	var bool bCtrlDown;
	var bool bAltDown;
	var bool bShiftDown;
	var name Key;
	var name CommandName;
};

/** An editor hotkey binding used to spawn an actor when clicking with the left mouse. */
struct native QuickActorKeyBinding
{
	var bool bCtrlDown;
	var bool bAltDown;
	var bool bShiftDown;
	var name Key;
	var name ActorClassName;
};

/** Array of keybindings */
var config array<EditorKeyBinding> KeyBindings;

/** Array of quick actor bindings. These actors will be spawned when the associated key is held down during a left mouse click provided the key was not already handled by another action. */
var config array<QuickActorKeyBinding> QuickActorKeyBindings;