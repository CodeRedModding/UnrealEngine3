/*=============================================================================
	InternetLink.h: TCP/UDP abstraction class
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#if WITH_UE3_NETWORKING

/*-----------------------------------------------------------------------------
	FInternetLink
-----------------------------------------------------------------------------*/

class FInternetLink
{
protected:
	FSocketData SocketData;
	static UBOOL ThrottleSend;
	static UBOOL ThrottleReceive;
	static INT BandwidthSendBudget;
	static INT BandwidthReceiveBudget;

	virtual UBOOL HasBudgetToRecv() { return !ThrottleReceive || BandwidthReceiveBudget > 0; }
public:
	// tors.
	FInternetLink()
	{}
	FInternetLink(const FSocketData &InSocketData)
	:	SocketData(InSocketData)
	{}

	// Bandwidth throttling
	static void ThrottleBandwidth( INT SendBudget, INT ReceiveBudget );
};

/*-----------------------------------------------------------------------------
	FUdpLink
-----------------------------------------------------------------------------*/

class FUdpLink : protected FInternetLink
{
	UBOOL ExternalSocket;

protected:
	INT StatBytesSent;
	INT StatBytesReceived;

public:
	// tors.
	FUdpLink();
	FUdpLink(const FSocketData &InSocketData);
	virtual ~FUdpLink();

	// FUdpLink interface
	virtual UBOOL BindPort( INT Port=0 );
	virtual void Poll();
	virtual INT SendTo( FIpAddr DstAddr, BYTE* Data, INT Count );
	virtual void OnReceivedData( FIpAddr SrcAddr, BYTE* Data, INT Count ) {}
};

#endif	//#if WITH_UE3_NETWORKING
