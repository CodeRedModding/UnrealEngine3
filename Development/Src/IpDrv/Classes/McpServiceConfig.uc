/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Base configuration for all MCP web services
 */
class McpServiceConfig extends Object
	config(Engine);

/** The protocol information to prefix when building the url (e.g. https) */
var config string Protocol;

/** The domain to prefix when building the url (e.g. localhost:8080) */
var config string Domain;

/** AppKey for access privileges to the online services for the current title. Not config so it can be hidden */
var string AppKey;

/** AppSecret for access privileges to the online services for the current title. Not config so it can be hidden */
var string AppSecret;

/**
 * Get the auth ticket for a user
 * 
 * @param McpId user id to get auth access for
 * 
 * @return auth ticket string
 */
function string GetUserAuthTicket(string McpId);