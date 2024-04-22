/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * UnrealScript Commandlet (command-line applet) class.
 *
 * Commandlets are executed from the ucc.exe command line utility, using the
 * following syntax:
 *
 *     yourgame.exe package_name.commandlet_class_name [parm=value]...
 *
 * for example:
 *
 *     yourgame.exe Core.HelloWorldCommandlet
 *     yourgame.exe UnrealEd.MakeCommandlet
 *
 * As a convenience, if a user tries to run a commandlet and the exact
 * name he types isn't found, then ucc.exe appends the text "commandlet"
 * onto the name and tries again.  Therefore, the following shortcuts
 * perform identically to the above:
 *
 *     yourgame.exe Core.HelloWorld
 *     yourgame.exe UnrealEd.Make
 *
 * Commandlets are executed in a "raw" UnrealScript environment, in which
 * the game isn't loaded, the client code isn't loaded, no levels are
 * loaded, and no actors exist.
 */
class Commandlet
	extends Object
	abstract
	transient
	native;

/** Description of the commandlet's purpose */
var localized string HelpDescription;

/** Usage template to show for "ucc help" */
var localized string HelpUsage;

/** Hyperlink for more info */
var localized string HelpWebLink;

/** The name of the parameter the commandlet takes */
var localized array<string> HelpParamNames;

/** The description of the parameter */
var localized array<string> HelpParamDescriptions;

/**
 * Whether to load objects required in server, client, and editor context.  If IsEditor is set to false, then a
 * UGameEngine (or whatever the value of Engine.Engine.GameEngine is) will be created for the commandlet instead
 * of a UEditorEngine (or Engine.Engine.EditorEngine), unless the commandlet overrides the CreateCustomEngine method.
 */
var bool IsServer, IsClient, IsEditor;

/** Whether to redirect standard log to the console */
var bool LogToConsole;

/** Whether to show standard error and warning count on exit */
var bool ShowErrorCount;

cpptext
{
	virtual INT Main(const FString& Params);

	/**
	 * Parses a string into tokens, separating switches (beginning with - or /) from
	 * other parameters
	 *
	 * @param	CmdLine		the string to parse
	 * @param	Tokens		[out] filled with all parameters found in the string
	 * @param	Switches	[out] filled with all switches found in the string
	 *
	 * @return	@todo
	 */
	static void ParseCommandLine( const TCHAR* CmdLine, TArray<FString>& Tokens, TArray<FString>& Switches )
	{
		FString NextToken;
		while ( ParseToken(CmdLine, NextToken, FALSE) )
		{
			if ( **NextToken == TCHAR('-') || **NextToken == TCHAR('/') )
			{
				new(Switches) FString(NextToken.Mid(1));
			}
			else
			{
				new(Tokens) FString(NextToken);
			}
		}
	}

	/**
	 * This is where you put any custom code that needs to be executed from InitializeIntrinsicPropertyValues() in
	 * your commandlet
	 */
	void StaticInitialize() {}

	/**
	 * Allows commandlets to override the default behavior and create a custom engine class for the commandlet. If
	 * the commandlet implements this function, it should fully initialize the UEngine object as well.  Commandlets
	 * should indicate that they have implemented this function by assigning the custom UEngine to GEngine.
	 */
	virtual void CreateCustomEngine() {}
}

/**
 * Entry point for your commandlet
 *
 * @param Params the string containing the parameters for the commandlet
 */
event int Main(string Params);

defaultproperties
{
	IsServer=true
	IsClient=true
	IsEditor=true
	LogToConsole=false
	ShowErrorCount=true
}
