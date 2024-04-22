/**
 * This class stores options global to the entire editor.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UnrealEdOptions extends Object
	Config(Editor)
	native;


cpptext
{
public:

	/**
	 * Generates a mapping from commnads to their parent sets for quick lookup.
	 */
	void GenerateCommandMap();

	/**
	 * @param Key			Key name
	 * @param bAltDown		Whether or not ALT is pressed.
	 * @param bCtrlDown		Whether or not CONTROL is pressed.
	 * @param bShiftDown	Whether or not SHIFT is pressed.
	 * @return Returns whether or not the specified key event is already bound to a command or not.
	 */
	UBOOL IsKeyBound(FName Key, UBOOL bAltDown, UBOOL bCtrlDown, UBOOL bShiftDown, FName EditorSet);

	/**
	 * Binds a hotkey.
	 *
	 * @param Key			Key name
	 * @param bAltDown		Whether or not ALT is pressed.
	 * @param bCtrlDown		Whether or not CONTROL is pressed.
	 * @param bShiftDown	Whether or not SHIFT is pressed.
	 * @param Command	Command to bind to.
	 */
	void BindKey(FName Key, UBOOL bAltDown, UBOOL bCtrlDown, UBOOL bShiftDown, FName Command);

	/**
	 * Attempts to execute a command bound to a hotkey.
	 *
	 * @param Key			Key name
	 * @param bAltDown		Whether or not ALT is pressed.
	 * @param bCtrlDown		Whether or not CONTROL is pressed.
	 * @param bShiftDown	Whether or not SHIFT is pressed.
	 * @param EditorSet		Set of bindings to search in.
	 */
	void ExecuteBinding(FName Key, UBOOL bAltDown, UBOOL bCtrlDown, UBOOL bShiftDown, FName EditorSet);

	/**
	 * Attempts to locate a exec command bound to a hotkey.
	 *
	 * @param Key			Key name
	 * @param bAltDown		Whether or not ALT is pressed.
	 * @param bCtrlDown		Whether or not CONTROL is pressed.
	 * @param bShiftDown	Whether or not SHIFT is pressed.
	 * @param EditorSet		Set of bindings to search in.
	 */
	FString GetExecCommand(FName Key, UBOOL bAltDown, UBOOL bCtrlDown, UBOOL bShiftDown, FName EditorSet);

	/**
	 * Attempts to locate a command name bound to a hotkey.
	 *
	 * @param Key			Key name
	 * @param bAltDown		Whether or not ALT is pressed.
	 * @param bCtrlDown		Whether or not CONTROL is pressed.
	 * @param bShiftDown	Whether or not SHIFT is pressed.
	 * @param EditorSet		Set of bindings to search in.
	 *
	 * @return Name of the command if found, NAME_None otherwise.
	 */
	FName GetCommand(FName Key, UBOOL bAltDown, UBOOL bCtrlDown, UBOOL bShiftDown, FName EditorSet);

	/**
	 * Retreives a editor key binding for a specified command.
	 *
	 * @param Command		Command to retrieve a key binding for.
	 *
	 * @return A pointer to a keybinding if one exists, NULL otherwise.
	 */
	FEditorKeyBinding* GetKeyBinding(FName Command);
}


/** A category to store a list of commands. */
struct native EditorCommandCategory
{
	var name Parent;
	var name Name;
};

/** A parameterless exec command that can be bound to hotkeys and menu items in the editor. */
struct native EditorCommand
{
	var name Parent;
	var name CommandName;
	var string ExecCommand;
	var string Description;
};

/** Categories of commands. */
var config array<EditorCommandCategory> EditorCategories;

/** Commands that can be bound to in the editor. */
var config array<EditorCommand> EditorCommands;

/** Pointer to the key bindings object that actually stores key bindings for the editor. */
var UnrealEdKeyBindings	EditorKeyBindings;

/** Mapping of command name's to array index. */
var native map{FName, INT}	CommandMap;

defaultproperties
{
	Begin Object Class=UnrealEdKeyBindings Name=EditorKeyBindingsInst
	End Object
	EditorKeyBindings=EditorKeyBindingsInst
}