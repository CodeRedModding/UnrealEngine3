/*=============================================================================
	GameCenterNetworking.h: GameCenter peer to peer network driver.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if WITH_UE3_NETWORKING

class UGameCenterNetConnection : public UNetConnection
{
	DECLARE_CLASS_INTRINSIC(UGameCenterNetConnection,UNetConnection,CLASS_Config|CLASS_Transient,OnlineSubsystemGameCenter)

	/**
	 * Initializes a connection with the passed in settings
	 *
	 * @param InDriver the net driver associated with this connection
	 * @param InSocket the socket associated with this connection
	 * @param InRemoteAddr the remote address for this connection
	 * @param InState the connection state to start with for this connection
	 * @param InOpenedLocally whether the connection was a client/server
	 * @param InURL the URL to init with
	 * @param InMaxPacket the max packet size that will be used for sending
	 * @param InPacketOverhead the packet overhead for this connection type
	 */
	virtual void InitConnection(UNetDriver* InDriver,FSocket* InSocket,const FInternetIpAddr& InRemoteAddr,EConnectionState InState,UBOOL InOpenedLocally,const FURL& InURL,INT InMaxPacket = 0,INT InPacketOverhead = 0);

	/**
	 * Sends a byte stream to the remote endpoint using the underlying socket
	 *
	 * @param Data the byte stream to send
	 * @param Count the length of the stream to send
	 */
	virtual void LowLevelSend(void* Data,INT Count);

	FString LowLevelGetRemoteAddress(UBOOL bAppendPort=TRUE);
	FString LowLevelDescribe();

	/** @return Returns the address of the connection as an integer */
	virtual INT GetAddrAsInt(void)
	{
		// this is pretty meaningless with GC
		return 0;
	}
	/** @return Returns the port of the connection as an integer */
	virtual INT GetAddrPort(void)
	{
		// this is pretty meaningless with GC
		return 0;
	}
};



class UGameCenterNetDriver : public UNetDriver
{
	DECLARE_CLASS_INTRINSIC(UGameCenterNetDriver,UNetDriver,CLASS_Transient|CLASS_Config,OnlineSubsystemGameCenter)

	// Constructor.
	UGameCenterNetDriver()
	{}

	// UNetDriver interface.
	UBOOL InitConnect( FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error );
	UBOOL InitListen( FNetworkNotify* InNotify, FURL& LocalURL, FString& Error );
	void TickDispatch( FLOAT DeltaTime );
	FString LowLevelGetNetworkNumber();
	void LowLevelDestroy();
	/** @return TRUE if the net resource is valid or FALSE if it should not be used */
	virtual UBOOL IsNetResourceValid(void);
};

#endif
