/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/** This commandlet finds and displays help information on other commandlets */
class HelpCommandlet extends Commandlet
	native;

cpptext
{
	virtual INT Main(const FString& Params);
}

/**
 * Looks at the parameters and displays help based upon those parameters
 *
 * @param Params the string containing the parameters for the commandlet
 */
event int Main(string Params);

