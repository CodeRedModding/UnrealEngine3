// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.


#ifndef _UNCONSOLEMANAGER_H_
#define _UNCONSOLEMANAGER_H_

// we don't need any of this on console
#if !CONSOLE && !PLATFORM_MACOSX && WITH_UE3_NETWORKING

#pragma pack(push,8)

#include "UnConsoleTarget.h"

#define DEFAULT_DEBUG_SERVER_PORT 13650

typedef FReferenceCountPtr<CTarget> TargetPtr;
typedef map<TARGETHANDLE, TargetPtr> TargetMap;

/**
 * This class contains all of the network code for interacting with targets.
 */
class FConsoleNetworkManager
{
protected:
	/** True if the FConsoleNetworkManager instance has been initialized. */
	bool bInitialized;
	/** Information about the system. */
	SYSTEM_INFO SysInfo;

	/** A map of targets. */
	TargetMap Targets;
	/** Synch object for accessing the target map/list. */
	FCriticalSectionLite TargetsLock;
	
	/** TCP client used to communicate with the game. */
	FConsoleSocket* BroadcastSocket;
	time_t LastPingTime;

	/** The broadcast address to ping for iThings */
	sockaddr_in BroadcastAddr;

	/** The subnet to iterate to ping for iThings */
	bool bDoSubnetSearch;
	sockaddr_in SubnetSearchAddr;

	/**
	 * Removes the target with the specified address.
	 *
	 * @param	Handle		The handle of the target to be removed.
	 */
	virtual TargetPtr RemoveTarget(TARGETHANDLE Handle)
	{
		TargetPtr Target = NULL;
		TargetsLock.Lock();
		{
			TargetMap::iterator Iter = Targets.find(Handle);

			if(Iter != Targets.end())
			{
				Target = (*Iter).second;
				Targets.erase(Iter);
			}
		}
		TargetsLock.Unlock();
		return Target;
	}

public:
	FConsoleNetworkManager()
		: bInitialized(false)
		, BroadcastSocket(NULL)
		, LastPingTime(0)
		, bDoSubnetSearch(false)
	{
		GetSystemInfo(&SysInfo);
	}
	virtual ~FConsoleNetworkManager()
	{
		Cleanup();
	}

	/**
	 * Creates a new socket
	 */
	virtual FConsoleSocket* CreateSocket( void ) const = 0;
	/*
		{ return new FConsoleSocket( FConsoleSocket::SF_IPv4, FConsoleSocket::ST_Datagram, FConsoleSocket::SP_UDP ); }
	*/

	/**
	 * Creates a new target
	 */
	virtual CTarget* CreateTarget( const sockaddr_in* InAddress ) const = 0;
	/*
		{ return new CTarget( InAddress, new FConsoleSocket() ); }
	*/

	/**
	 * Makes sure the platform is correct
	 */ 
	virtual bool ValidatePlatform( const char* InPlatform ) const = 0;
	/*
		{ return !( strcmp( InPlatform, "PC" ) && strcmp( InPlatform, "PCServer" ) && strcmp( InPlatform, "PCConsole" ) ); }
	*/

	/**
	 * Get the configuration
	 */ 
	virtual wstring GetConfiguration( void ) const = 0;
	/*
		{ return L"PC"; }
	*/

	/**
	 * Get the platform
	 */ 
	virtual FConsoleSupport::EPlatformType GetPlatform( void ) const = 0;
	/*
		{ return FConsoleSupport::EPlatformType_Windows; }
	*/

	/**
	 * Gets an CTarget from a TARGETHANDLE
	 */
	virtual CTarget* ConvertTarget( const TARGETHANDLE Handle )
	{
		TargetPtr Target = GetTarget(Handle);

		if( Target)
		{
			return (CTarget*)(Target.GetHandle());
		}

		return NULL;
	}

	/**
	 * Gets an CTarget from a TARGETHANDLE
	 */
	virtual CTarget* ConvertTarget( const sockaddr_in &Address )
	{
		TargetPtr Target = GetTarget(Address);

		if( Target)
		{
			return (CTarget*)(Target.GetHandle());
		}

		return NULL;
	}

	/**
	 * Gets an CTarget from a TargetPtr
	 */
	virtual CTarget* ConvertTarget( TargetPtr InTarget )
	{
		return (CTarget*)(InTarget.GetHandle());
	}

	/**
	 * Initalizes sock and the FConsoleNetworkManager instance.
	 */
	virtual void Initialize()
	{
		if(bInitialized)
		{
			return;
		}

		DebugOutput(L"Initializing Network Manager...\n");

		// Init Sockets
		WSADATA WSAData;
		WORD WSAVersionRequested = MAKEWORD(2, 2);
		if (WSAStartup(WSAVersionRequested, &WSAData) != 0)
		{
			return;
		}

		DebugOutput(L"WSA Sockets Initialized.\n");

		// make a broadcast UDP socket for use with auto-discovery
		BroadcastSocket = CreateSocket();
		if (!BroadcastSocket->CreateSocket())
		{
			DebugOutput(L"Unable to create UDP socket.\n");
			return;
		}

		// bind to all interfaces, using a port selected by the system
		BroadcastSocket->SetAddress("0.0.0.0");
		BroadcastSocket->SetPort(0);

		// bind the socket to the port/address
		if (!BroadcastSocket->Bind())
		{
			DebugOutput(L"Unable to bind UDP socket.\n");
			return;
		}
		BroadcastSocket->SetNonBlocking(true);
		BroadcastSocket->SetBroadcasting(true);

		// set up and save the broadcast address
		ZeroMemory(&BroadcastAddr, sizeof(BroadcastAddr));
		BroadcastAddr.sin_family = AF_INET;
		BroadcastAddr.sin_addr.s_addr = inet_addr("255.255.255.255");
		BroadcastAddr.sin_port = htons(DEFAULT_DEBUG_SERVER_PORT);

		// HACK: initializing to EPIC-SECURE subnet
		// SetSubnetSearch("10.1.33.0");

		bInitialized = true;
	}

	/**
	 * Cleans up sock and all of the resources allocated by the FConsoleNetworkManager instance.
	 */
	virtual void Cleanup()
	{
		if (bInitialized)
		{
			DebugOutput(L"Cleaning up UDP ping socket...\n");
			if (BroadcastSocket != NULL)
			{
				BroadcastSocket->Close();
				delete BroadcastSocket;
				BroadcastSocket = NULL;
			}

			DebugOutput(L"Cleaning up Network Manager...\n");

			TargetsLock.Lock();
			Targets.clear();
			TargetsLock.Unlock();

			WSACleanup();

			DebugOutput(L"Finished cleaning up Network Manager.\n");
			bInitialized = false;
		}
	}

	/**
	 * Tells network manager to explicitly search a class-D block represented by ip when doing broadcasts
	 * 
	 * @param	ip		The IP of the subnet (i.e. "10.1.33.0"). 
	 * Note that the last digit doesn't matter but it should be a number so it parses.
	 */
	virtual void SetSubnetSearch(const char* ip)
	{	
		if (ip == NULL)
		{
			bDoSubnetSearch = false;
			return;
		}

		// initialize the SubnetSearchAddr
		ZeroMemory(&SubnetSearchAddr, sizeof(SubnetSearchAddr));
		SubnetSearchAddr.sin_family = AF_INET;
		SubnetSearchAddr.sin_addr.s_addr = inet_addr(ip);
		SubnetSearchAddr.sin_port = htons(DEFAULT_DEBUG_SERVER_PORT);

		// do the search as long as the IP parsed
		bDoSubnetSearch = (SubnetSearchAddr.sin_addr.s_addr != INADDR_NONE);
	}

	/**
	 * Retrieves a target with the specified IP Address.
	 *
	 * @param	Address		The address of the target to retrieve.
	 * @return	NULL if the target could not be found, otherwise a valid reference pointer.
	 */
	virtual TargetPtr GetTarget(const sockaddr_in &Address)
	{
		TargetPtr Ret;

		TargetsLock.Lock();
		for(TargetMap::iterator Iter = Targets.begin(); Iter != Targets.end(); ++Iter)
		{
			if(memcmp(&(*Iter).second->GetRemoteAddress(), &Address, sizeof(sockaddr_in)) == 0)
			{
				Ret = (*Iter).second;
				break;
			}
		}
		TargetsLock.Unlock();

		return Ret;
	}

	/**
	 * Retrieves a target with the specified IP Address.
	 *
	 * @param	Handle		The handle of the target to retrieve.
	 * @return	NULL if the target could not be found, otherwise a valid reference pointer.
	 */
	virtual TargetPtr GetTarget(const TARGETHANDLE Handle)
	{
		TargetPtr ptr;

		TargetsLock.Lock();
		TargetMap::iterator Iter = Targets.find(Handle);
		if(Iter != Targets.end())
		{
			ptr = (*Iter).second;
		}
		TargetsLock.Unlock();

		return ptr;
	}

	/**
	 * Connects to the target with the specified name.
	 *
	 * @param	TargetName		The name of the target to connect to.
	 * @return	Handle to the target that was connected or else INVALID_TARGETHANDLE.
	 */
	virtual TARGETHANDLE ConnectToTarget(const wchar_t* TargetName)
	{
		// Find the target
		TargetPtr CurrentTarget;

		for(TargetMap::iterator Iter = Targets.begin(); Iter != Targets.end(); ++Iter)
		{
			if((*Iter).second->Name == TargetName)
			{
				CurrentTarget = (*Iter).second;
				break;
			}
		}

		if (CurrentTarget->Connect())
		{
			return CurrentTarget.GetHandle();
		}

		return INVALID_TARGETHANDLE;
	}

	/**
	 * Connects to the target with the specified handle.
	 *
	 * @param	Handle		The handle of the target to connect to.
	 */
	virtual bool ConnectToTarget(TARGETHANDLE Handle)
	{
		TargetPtr Target = GetTarget(Handle);

		return Target->Connect();
	}

	/**
	 * Returns the number of targets available.
	 */
	virtual int GetNumberOfTargets()
	{
		int Num = 0;

		TargetsLock.Lock();
		{
			Num = (int)Targets.size();
		}
		TargetsLock.Unlock();

		return Num;
	}

	/**
	 * Exists for compatability with UnrealConsole. Index is disregarded and CurrentTarget is disconnected if it contains a valid pointer.
	 *
	 * @param	Handle		Handle to the target to disconnect.
	 */
	virtual void DisconnectTarget(const TARGETHANDLE Handle)
	{
		TargetPtr Target = GetTarget(Handle);

		if(Target)
		{
			Target->Disconnect();
		}
	}
	
	/**
	 *	Sends message using given UDP client.
	 *	@param Handle			client to send message to
	 *	@param MessageType		sent message type
	 *	@param Message			actual text of the message
	 *	@return true on success, false otherwise
	 */
	virtual bool SendToConsole(const TARGETHANDLE Handle, const wchar_t* Message )
	{
		TargetPtr Target = GetTarget(Handle);

		if(Target)
		{
			return Target->SendConsoleCommand(ToString(Message).c_str());
		}

		return false;
	}

	/**
	 * Broadcast a ping and call UpdateTargets()
	 */
	virtual void DetermineTargets()
	{
		time_t Now = time(NULL);

		// do this no more than once per second
		if (Now != LastPingTime)
		{
			DebugOutput(L"Sending a UE3PING\n");

			// send a ping out on our UDP socket
			const static char PING[] = "UE3PING";
			BroadcastSocket->SetAddress(BroadcastAddr); // also sets port
			BroadcastSocket->SendTo(PING, sizeof(PING)-1);
			LastPingTime = Now;

			// spam the search subnet if any
			if (bDoSubnetSearch)
			{
				unsigned long addr = ntohl(SubnetSearchAddr.sin_addr.s_addr) & 0xFFFFFF00;
				for (int i = 1; i <= 254; i++)
				{
					SubnetSearchAddr.sin_addr.s_addr = htonl(addr | i);
					BroadcastSocket->SetAddress(SubnetSearchAddr); // also sets port
					BroadcastSocket->SendTo(PING, sizeof(PING)-1);
				}
			}
		}

		// Read the socket regularly for several seconds to see if anything responds
		for( int i = 0; i < 10; i ++ )
		{
			Sleep( 50 );
			UpdateTargets();
		}
	}

	/**
	 * Read from the socket any replies from the device pings
	 * (ideally this should be called more often than DetermineTargets and would signal the host with a callback when the targets list changes)
	 */
	virtual void UpdateTargets()
	{
		char Buffer[1600];
		bool ReadSomething = true;

		while (ReadSomething)
		{
			ReadSomething = false;

			// check the UDP socket for pongs
			sockaddr_in FromAddress;
			int BytesRead = BroadcastSocket->RecvFrom(Buffer, sizeof(Buffer)-1, FromAddress, FConsoleSocket::SF_None);
			if (BytesRead >= 0)
			{
				ReadSomething = true;
				Buffer[BytesRead] = '\0'; // make sure we're terminated

				// make sure this is a PONG packet
				if (strncmp(Buffer, "UE3PONG", 7) == 0)
				{
					int DebugPort = 0;
					char CompName[512] = { 0 };
					char GameName[512] = { 0 };
					char GameType[512] = { 0 };
					char Platform[512] = { 0 };

					// parse the pong packet
					if (sscanf_s(Buffer, "UE3PONG\nDEBUGPORT=%d\nCNAME=%511s\nGAME=%511s\nGAMETYPE=%511s\nPLATFORM=%511s\n", 
						&DebugPort, &CompName, _countof( CompName ), &GameName, _countof( GameName ), &GameType, _countof( GameType ), &Platform, _countof( Platform ) ))
					{
						if( !ValidatePlatform( Platform ) )
						{
							continue;
						}

						DebugOutput(L"Got a UE3PONG\n");

						// use the port specified in the PONG
						FromAddress.sin_port = htons((u_short)DebugPort);

						// find the target by IP
						TargetsLock.Lock();
						TargetPtr Ret;
						for(TargetMap::iterator Iter = Targets.begin(); Iter != Targets.end(); ++Iter)
						{
							if(memcmp(&(*Iter).second->GetRemoteAddress(), &FromAddress, sizeof(sockaddr_in)) == 0)
							{
								Ret = (*Iter).second;
								break;
							}
						}

						// add if necessary
						if (!Ret)
						{
							Ret = CreateTarget( &FromAddress );
							Targets[Ret.GetHandle()] = Ret;
						}

						// update with info
						SetupTarget( Ret, CompName, GameName, GameType );

						// unlock the mutex
						TargetsLock.Unlock();
					}
				}
			}
			else if (WSAGetLastError() == WSAECONNRESET)
			{
				// had to clear the error, just read again and it'll work
				ReadSomething = true;
			}
		}
	}

	/**
	 * Sets up the target with the information it needs
	 */
	virtual void SetupTarget( TargetPtr InTarget, const char* CompName, const char* GameName, const char* GameType )
	{
		wchar_t WideBuf[1024] = { 0 };
		MultiByteToWideChar(CP_UTF8, 0, CompName, -1, WideBuf, sizeof(WideBuf)/sizeof(WideBuf[0]) - 1);
		InTarget->Name = WideBuf;
		InTarget->ComputerName = WideBuf;
		MultiByteToWideChar(CP_UTF8, 0, GameName, -1, WideBuf, sizeof(WideBuf)/sizeof(WideBuf[0]) - 1);
		InTarget->GameName = WideBuf;
		InTarget->Configuration = GetConfiguration();
		MultiByteToWideChar(CP_UTF8, 0, GameType, -1, WideBuf, sizeof(WideBuf)/sizeof(WideBuf[0]) - 1);
		InTarget->GameTypeName = WideBuf;
		if (strcmp(GameType, "Editor") == 0)
		{
			InTarget->GameType = EGameType_Editor;
		}
		else if (strcmp(GameType, "Server") == 0)
		{
			InTarget->GameType = EGameType_Server;
		}
		else if (strcmp(GameType, "Listen Server") == 0)
		{
			InTarget->GameType = EGameType_ListenServer;
		}
		else if (strcmp(GameType, "Client") == 0)
		{
			InTarget->GameType = EGameType_Client;
		}
		else
		{
			InTarget->GameType = EGameType_Unknown;
		}
		InTarget->PlatformType = GetPlatform();		
	}

	/**
	 * Gets the default target.
	 */
	virtual TargetPtr GetDefaultTarget()
	{
		TargetPtr Ret;

		TargetsLock.Lock();
		TargetMap::iterator Iter = Targets.begin();

		if(Iter != Targets.end())
		{
			Ret = (*Iter).second;
		}

		TargetsLock.Unlock();

		return Ret;
	}

	/**
	 * Retrieves a handle to each available target.
	 *
	 * @param	OutTargetList			An array to copy all of the target handles into.
	 */
	virtual int GetTargets(TARGETHANDLE *OutTargetList)
	{
		if( OutTargetList != NULL )
		{
			TargetsLock.Lock();

			int TargetsCopied = 0;
			for(TargetMap::iterator Iter = Targets.begin(); Iter != Targets.end(); ++Iter, ++TargetsCopied)
			{
				OutTargetList[TargetsCopied] = (*Iter).first;
			}

			TargetsLock.Unlock();
		}

		return (int)Targets.size();
	}

#pragma warning(push)
#pragma warning(disable : 4100) // Unreferenced formal parameter

	/**
	 * Forces a stub target to be created to await connection
	 *
	 * @returns Handle of new stub target
	 */
	virtual TARGETHANDLE ForceAddTarget( const wchar_t* TargetIP ){ return NULL; }

#pragma warning(pop)

#if DEBUG || _DEBUG
	void DebugOutput(const wchar_t* Str) const
	{
		OutputDebugString(Str);
	}
#else
	void DebugOutput(const wchar_t*) const
	{
	}
#endif
};

#pragma pack( pop )

#endif // !CONSOLE && !PLATFORM_MACOSX && WITH_UE3_NETWORKING

#endif // _UNCONSOLEMANAGER_H_