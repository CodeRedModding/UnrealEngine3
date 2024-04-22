/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This file contains the definitions used by the the debug server.
 */

#ifndef __DEBUG_SERVER_DEFS_H__
#define __DEBUG_SERVER_DEFS_H__

/// The port the debug channel receives messages
#define DefaultDebugChannelReceivePort 13500
/// The port the debug channel sends regular messages
#define DefaultDebugChannelSendPort 13501
/// The port the debug channel sends 'server response' message
#define DefaultDebugChannelTrafficPort 13502
/// The port the debug channel listens for client connections on.
#define DefaultDebugChannelListenPort 13503

// Add this offset to any port number when running on or connecting to IPhones
#define IPhoneOffset 100

// Add this offset to any port number when running on or connecting to Android
#define AndroidOffset 300

/// Available types of debug server messages
enum EDebugServerMessageType
{
	// Sent to server from client
	EDebugServerMessageType_ServerAnnounce = 0,
	EDebugServerMessageType_ClientConnect,
	EDebugServerMessageType_ClientDisconnect,
	EDebugServerMessageType_ClientText,
	EDebugServerMessageType_ServerPing,

	// Sent to client from server
	EDebugServerMessageType_ServerResponse,
	EDebugServerMessageType_ServerDisconnect,
	EDebugServerMessageType_ServerTransmission,
	EDebugServerMessageType_ServerPingReply,

	EDebugServerMessageType_Unknown,

	EDebugServerMessageType_Max,
};

/// Length of the debug server message type name
#define DebugServerMessageTypeNameLength 2

/// Converts debug server message type to string name
inline const char* ToString(EDebugServerMessageType type)
{
	switch (type)
	{
		case EDebugServerMessageType_ServerAnnounce: return "SA";
		case EDebugServerMessageType_ClientConnect: return "CC";
		case EDebugServerMessageType_ClientDisconnect: return "CD";
		case EDebugServerMessageType_ClientText: return "CT";
		case EDebugServerMessageType_ServerPing: return "SP";

		case EDebugServerMessageType_ServerResponse: return "SR";
		case EDebugServerMessageType_ServerDisconnect: return "SD";
		case EDebugServerMessageType_ServerTransmission: return "ST";
		case EDebugServerMessageType_ServerPingReply: return "PR";
	}
	return "??";
}

/// Converts string to debug channel message type
inline EDebugServerMessageType ToDebugServerMessageType(const char* messageType)
{
	for (unsigned int i = 0; i < EDebugServerMessageType_Max; i++)
	{
		if (strcmp(ToString((EDebugServerMessageType) i), messageType) == 0)
		{
			return (EDebugServerMessageType) i;
		}
	}
	return EDebugServerMessageType_Unknown;
}

#endif //__DEBUG_SERVER_DEFS_H__
