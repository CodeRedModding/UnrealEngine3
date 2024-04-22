/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Provides a mechanism for downloading arbitrary files from the MCP server
 */
class OnlineTitleFileDownloadMcpLive extends OnlineTitleFileDownloadMcp
	native
	dependson(OnlineSubsystem);

cpptext
{
	/**
	 * Builds the URL to use when fetching the specified file
	 *
	 * @param FileName the file that is being requested
	 *
	 * @return the URL to use with all of the per platform extras
	 */
	virtual FString BuildURLParameters(const FString& FileName);
}
