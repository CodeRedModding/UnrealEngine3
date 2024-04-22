/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "AndroidNetworkManager.h"
#include "..\..\IpDrv\Inc\DebugServerDefs.h"

/** How long in seconds before a target is considered disconnected when no ping replies are received */
const int HEARTBEAT_TIMEOUT = 4;

/** How long in seconds to wait before reconnecting after a connected target goes away */
const int RECONNECT_DELAY = 5;

///////////////////////CAndroidTarget/////////////////////////////////

CAndroidTarget::CAndroidTarget(const sockaddr_in* InRemoteAddress, FAndroidSocket* InTCPClient, FAndroidSocket* InUDPClient)
 : CTarget( InRemoteAddress, InTCPClient, InUDPClient )
 , LastHeartbeatTime(0)
 , LastPingReplyTime(0)
 , TimeToReconnect(0)
 , bNeedsReconnect(false)
 , PartialPacketBuffer(NULL)
 , PartialPacketBufferSize(0)
{
}

CAndroidTarget::~CAndroidTarget()
{
	if(PartialPacketBuffer)
	{
		delete [] PartialPacketBuffer;
		PartialPacketBuffer = NULL;
	}
}

/** Make the name user friendly. */
void CAndroidTarget::UpdateName()
{
	if ( UDPClient )
	{
		const unsigned int Address = UDPClient->GetIP();
		Name = ComputerName + L" (" + ToWString( inet_ntoa(*(in_addr*) &Address) ) + L")";
	}
}

/** Periodic update */
void CAndroidTarget::Heartbeat(class FAndroidNetworkManager& NetworkManager)
{
	__time64_t CurrentTime;
	_time64(&CurrentTime);

	// is if time to do a heartbeat (do it once a second)
	if (CurrentTime - LastHeartbeatTime >= 1)
	{
		LastHeartbeatTime = CurrentTime;

		// do we currently think we are connected to the target?
		if (!bNeedsReconnect)
		{
			// if we've ever gotten a ping reply, then make sure it's recent enough
			if (LastPingReplyTime > 0 && CurrentTime - LastPingReplyTime >= HEARTBEAT_TIMEOUT)
			{
				// if we timed out waiting for a heartbeat, then try to reconnect
				if (TimeToReconnect == 0)
				{
					SendTTY(L"Timed out on heartbeat, going to reconnect soon...\n");
					TimeToReconnect = CurrentTime + RECONNECT_DELAY;
					bNeedsReconnect = true;
				}
			}
			else
			{
				NetworkManager.SendToConsole(this, EDebugServerMessageType_ServerPing);
			}
		}
		else if (TimeToReconnect != 0 && CurrentTime >= TimeToReconnect)
		{
			// protect against reentrancy
			TimeToReconnect = 0;

			// attempt to reconnect (this will make TCPClient invalid)
			SendTTY(L"Attempting to reconnect...\n");
			if (NetworkManager.ConnectToTarget(this))
			{
				// we have reconnected, can stop trying
				bNeedsReconnect = false;
			}
			else
			{
				// if we didn't reconnect, then try again soon
				TimeToReconnect = CurrentTime + RECONNECT_DELAY;
			}
		}
	}
}
