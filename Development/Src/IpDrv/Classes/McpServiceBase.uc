/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Common functionality for all MCP web service interfaces
 */
class McpServiceBase extends Object
	native
	config(Engine);

/** Class to load and instantiate for the handling configuration options for MCP services */
var config string McpConfigClassName;
/** Contains all the configuration options common to MCP services. Protocol, URL, access key, etc */
var McpServiceConfig McpConfig;

/* Initialize always called after constructing a new MCP service subclass instance via its factory method */
event Init()
{
	local class<McpServiceConfig> McpConfigClass;

	`log("Loading McpServerBase McpConfigClass:" $ default.McpConfigClassName);
	// load the configuration class and instantiate it
	McpConfigClass = class<McpServiceConfig>(DynamicLoadObject(default.McpConfigClassName,class'Class'));
	if (McpConfigClass != None)
	{
		McpConfig = new McpConfigClass;
	}
}

/**
 * @return Base protocol and domain for communicating with MCP server
 */
function string GetBaseURL()
{
	return McpConfig.Protocol $ "://" $ McpConfig.Domain;
}

/**
 * @return Access rights for app including title id
 */
function string GetAppAccessURL()
{
	return "?appKey=" $ McpConfig.AppKey $ "&appSecret=" $ McpConfig.AppSecret;
}

/**
 * Build the URL for auth ticket access
 * 
 * @param McpId user id to build auth access for
 * 
 * @return URL parameters needed for user auth ticket usage
 */
function string GetUserAuthURL(string McpId)
{
	local string AuthTicket;

	AuthTicket = McpConfig.GetUserAuthTicket(McpId);
	if (Len(AuthTicket) > 0)
	{
		return "&authTicket=" $ AuthTicket;
	}
	return "";
}
