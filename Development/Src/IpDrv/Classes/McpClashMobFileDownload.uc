/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Provides the interface for ClashMob Mcp services
 */
class McpClashMobFileDownload extends OnlineTitleFileDownloadWeb
	config(Engine);

/**
 * Build the clashmob specific Url for downloading a given file
 *
 * @param FileName the file to search the table for
 *
 * @param the URL to use to request the file or BaseURL if no special mapping is present
 */
function string GetUrlForFile(string FileName)
{
	local string Url;

	Url = GetBaseURL() $ RequestFileURL $ 
		"?appKey=" $ McpConfig.AppKey $ "&appSecret=" $ McpConfig.AppSecret $
		"&dlName=";

	return Url;
}