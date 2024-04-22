/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _ANDROIDTARGET_H_
#define _ANDROIDTARGET_H_

#include "AndroidSocket.h"

enum EOverlappedEventType
{
	OVT_Recv,
	OVT_Send,
};

#define OVERLAPPED_BUF_SIZE	2048 //2kb

struct OverlappedEventArgs : OVERLAPPED 
{
	EOverlappedEventType EventType;
	FReferenceCountPtr<CTarget> Owner;
	WSABUF WSABuffer;
	char Buffer[OVERLAPPED_BUF_SIZE];

	OverlappedEventArgs()
	{
		ZeroMemory(this, sizeof(OVERLAPPED));
		WSABuffer.buf = Buffer;
		WSABuffer.len = OVERLAPPED_BUF_SIZE;
	}

	OverlappedEventArgs(EOverlappedEventType EType) : EventType(EType)
	{
		ZeroMemory(this, sizeof(OVERLAPPED));
		WSABuffer.buf = Buffer;
		WSABuffer.len = OVERLAPPED_BUF_SIZE;
	}
};

/// Representation of a single UE3 instance running on PC
class CAndroidTarget : public CTarget
{
public:
	CAndroidTarget(const sockaddr_in* InRemoteAddress, FAndroidSocket* InTCPClient, FAndroidSocket* InUDPClient);
	virtual ~CAndroidTarget();

	/** Make the name user friendly. */
	void UpdateName();

	/** Periodic update */
	void Heartbeat(class FAndroidNetworkManager& Manager);

	/** Port the server listens on. */
	int ListenPortNo;

	/** System time of target registration ('server response' received). */
	__time64_t TimeRegistered;

	/** Time of last heartbeat, so we don't spam the network */
	__time64_t LastHeartbeatTime;

	/** Time that the last ping reply was received from the target */
	__time64_t LastPingReplyTime;

	/** Time to reconnect to the target if it was connected and died */
	__time64_t TimeToReconnect;

	/** TRUE if the target needs to reconnect, and shouldn't do anything else */
	bool bNeedsReconnect;

	char *PartialPacketBuffer;
	int PartialPacketBufferSize;

	/** True if this is the special local connection to the simulator */
	bool bIsLocal;
};

#endif
