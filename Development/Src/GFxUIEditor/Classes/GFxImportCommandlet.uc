/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class GFxImportCommandlet extends Commandlet
	native;

cpptext
{
	virtual INT Main(const FString& Params);
}

/**
 * A utility that imports and/or re-imports SWF assets
 *
 * @param Params the string containing the parameters for the commandlet
 */
event int Main(string Params);

