/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _NETWORKMANAGER_H_
#define _NETWORKMANAGER_H_

#include "AndroidTarget.h"

//forward declarations
enum EDebugServerMessageType;

typedef FReferenceCountPtr<CAndroidTarget> AndroidTargetPtr;

/**
 * This class contains all of the network code for interacting with android targets.
 */
class FAndroidNetworkManager : public FConsoleNetworkManager
{
public:
	FAndroidNetworkManager();
	virtual ~FAndroidNetworkManager();

	/**
	 * Creates a new socket
	 */
	inline FAndroidSocket* CreateSocket( void ) const
	{
		return new FAndroidSocket(FConsoleSocket::SF_IPv4, FConsoleSocket::ST_Datagram, FConsoleSocket::SP_UDP);
	}

	/**
	 * Creates a new target
	 */
	inline CAndroidTarget* CreateTarget( const sockaddr_in* InAddress ) const
	{
		return new CAndroidTarget(InAddress, new FAndroidSocket(), new FAndroidSocket() );
	}

	/**
	 * Makes sure the platform is correct
	 */ 
	inline bool ValidatePlatform( const char* InPlatform ) const
	{ 
		return !( strcmp( InPlatform, "Android" ) );
	}

	/**
	 * Get the configuration
	 */ 
	inline wstring GetConfiguration( void ) const
	{
		return L"Android";
	}

	/**
	 * Get the platform
	 */ 
	inline FConsoleSupport::EPlatformType GetPlatform( void ) const
	{
		return FConsoleSupport::EPlatformType_Android;
	}

	/**
	 * Gets an CAndroidTarget from a TARGETHANDLE
	 */
	CAndroidTarget* ConvertTarget( const TARGETHANDLE Handle );

	/**
	 * Gets an CAndroidTarget from a sockaddr_in
	 */
	CAndroidTarget* ConvertTarget( const sockaddr_in &Address );

	/**
	 * Gets an CAndroidTarget from a TargetPtr
	 */
	CAndroidTarget* ConvertTarget( TargetPtr InTarget );

	/**
	 * Initalizes sock and the FConsoleNetworkManager instance.
	 */
	void Initialize();

	/**
	 * Cleans up sock and all of the resources allocated by the FConsoleNetworkManager instance.
	 */
	void Cleanup();

	/**
	 * Exists for compatability with UnrealConsole. Index is disregarded and CurrentTarget is disconnected if it contains a valid pointer.
	 *
	 * @param	Handle		Handle to the target to disconnect.
	 */
	void DisconnectTarget(const TARGETHANDLE Handle);

	/**
	 *	Sends message using given UDP client.
	 *	@param Handle			client to send message to
	 *	@param MessageType		sent message type
	 *	@param Message			actual text of the message
	 *	@return true on success, false otherwise
	 */
	bool SendToConsole(const TARGETHANDLE Handle, const EDebugServerMessageType MessageType, const string& Message = "");

	/**
	 *	Attempts to determine available targets.
	 */
	void DetermineTargets();

	// accessors
	inline void SetTTYCallback(TTYEventCallbackPtr Callback) { TTYCallback = Callback; }

	/**
	 * Connects to the target with the specified name.
	 *
	 * @param	TargetName		The name of the target to connect to.
	 * @return	Handle to the target that was connected or else INVALID_TARGETHANDLE.
	 */
	TARGETHANDLE ConnectToTarget(const wchar_t* TargetName);

	/**
	 * Connects to the target with the specified handle.
	 *
	 * @param	Handle		The handle of the target to connect to.
	 */
	bool ConnectToTarget(TARGETHANDLE Handle);

private:
	/** Handle to the IO completion port. */
	HANDLE IOCompletionPort;
	/** Handle for triggering the cleanup event for IOCP worker threads. */
	HANDLE ThreadCleanupEvent;
	/** The callback for TTY notifications. */
	TTYEventCallbackPtr TTYCallback;
	/** Array of handles to IOCP worker threads. */
	vector<HANDLE> WorkerThreads;
	/** Used to broadcast 'server announce' message. */
	FAndroidSocket Broadcaster;
	/** Used to receive 'server response' from server. */
	FAndroidSocket ServerResponseListener;

	/**
	 * The callback for IOCP worker threads. Handles IO events for the targets.
	 *
	 * @param	Data	A pointer to the owning FAndroidNetworkManager instance.
	 */
	static unsigned int __stdcall WorkerThreadProc(void *Data);

	/**
	 * Adds a target to the list of targets.
	 *
	 * @param	Address		The address of the target being added.
	 * @return	A reference pointer to the new target.
	 */
	TargetPtr AddTarget(const sockaddr_in& Address);

	/**
	 * Removes the targets that have not been active for a period of time.
	 */
	void RemoveTimeExpiredTargets();

	/**
	 * Removes the target with the specified address.
	 *
	 * @param	Handle		The handle of the target to be removed.
	 */
	TargetPtr RemoveTarget(TARGETHANDLE Handle);

	/**
	 * Triggers the TTY callback for outputting a message.
	 *
	 * @param	Data	The message to be output.
	 * @param	Length	The size in bytes of the buffer pointed to by Data.
	 * @param	Target	The target to send the TTY text to.
	 */
	void NotifyTTYCallback(TargetPtr &InTarget, const char *Txt);

	/**
	 * Triggers the TTY callback for outputting a message.
	 *
	 * @param	Channel	The channel the message is occuring on.
	 * @param	Text	The message to be displayed.
	 * @param	Target	The target to send the TTY text to.
	 */
	void NotifyTTYCallback(TargetPtr &InTarget, const string &Channel, const string &Text);

	/**
	 * Handles a packet.
	 *
	 * @param	Data				The packet data.
	 * @param	BytesRecv			The size of the packet data.
	 * @param	Target				The target that received the packet.
	 */
	void HandlePacket(char* Data, const int BytesRecv, TargetPtr& InTarget);

	/**
	 * Parses packet data for a message.
	 *
	 * @param	TCPData				The packet data.
	 * @param	TCPDataSize			The size of the packet data.
	 * @param	bIsServerReply		If TRUE, this message was just a server ping reply, not a TTY message
	 * @param	ChannelEnum			Receives the channel the packet is operating on.
	 * @param	Text				Receives the text of the message.
	 * @param	NumBytesConsumed	The number of bytes read.
	 */
	bool CheckForCompleteMessage(char* TCPData, const int TCPDataSize, bool& bIsServerReply, int& ChannelEnum, string& Text, int& NumBytesConsumed) const;

	/**
	*	Attempts to receive (and dispatch) messages from given client.
	*	@param Client client to attempt to receive message from
	*	@param AttemptsCount no. attempts (between each fixed miliseconds waiting happens)
	*/
	void ReceiveMessages(FAndroidSocket& UDPClient, int AttemptsCount);

	/**
	*	Attempts to receive message from given UDP client.
	*	@param Client client to receive message from
	*	@param MessageType received message type
	*	@param Data received message data
	*	@param DataSize received message size
	*	@return true on success, false otherwise
	*/
	bool ReceiveFromConsole(FAndroidSocket& Client, EDebugServerMessageType& MessageType, char*& Data, int& DataSize, sockaddr_in& SenderAddress);

	/**
	*	Sends message using given UDP client.
	*	@param Client client to send message to
	*	@param MessageType sent message type
	*	@param Message actual text of the message
	*	@return true on success, false otherwise
	*/
	bool SendToConsole(FAndroidSocket& Client, const EDebugServerMessageType MessageType, const string& Message = "");

	/**
	 * Connects to the target.
	 *
	 * @param	Handle		The handle of the target to connect to.
	 */
	bool ConnectToTarget(TargetPtr &InTarget);
};

#endif
