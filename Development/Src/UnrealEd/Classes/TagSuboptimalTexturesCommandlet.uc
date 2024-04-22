/**
 * Finds textures that were imported too large for their LODSize setting
 * 
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class TagSuboptimalTexturesCommandlet extends Commandlet
	native
	config(Editor);

/** The texture groups to examine */
var config array<TextureGroup> TextureGroupsToExamine;

cpptext
{
	/**
	 * Find textures that don't match their MaxLODSize and add them to a shared collection
	 *
	 * @param Params - the command line arguments used to run the commandlet. A map list is the only expected argument at this time
	 *
	 * @return 0 - unused
	 */
	INT Main(const FString& Params);
}