/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __UNNETLIVE_H__
#define __UNNETLIVE_H__

#if WITH_UE3_NETWORKING && (XBOX || WITH_PANORAMA)

// Windows needs this, whereas the 360 doesn't
#if !CONSOLE
	#include "PreWindowsApi.h"
	#include "MinWindows.h"
		//NOTE: If you get an error here, make sure the G4WLive directories are in your additional includes/libs
		//NOTE: Tools->Options->Projects and Solutions->VC++ Directories
		//NOTE: $(GFWLSDK_DIR)include
		//NOTE: $(GFWLSDK_DIR)lib\x86
		#include <WinLive.h>
		#pragma warning(push) // Mirror what Windows.h does before including these
			#pragma warning(disable:4001)
			#pragma warning(disable:4201)
			#pragma warning(disable:4214)
			#include <MMSystem.h>
			#include <dsound.h>
			#include <shlobj.h>
		#pragma warning(pop)
	#include "PostWindowsApi.h"

	//NOTE: If you get an error here, you have the wrong version of the SDK installed
	const INT REQUIRED_G4WLIVE_SDK_VERSION = 0x35005800;
	checkAtCompileTime(CURRENT_XLIVE_VERSION == REQUIRED_G4WLIVE_SDK_VERSION, G4WLIVE_SDK_VERSION_DoesNotMatchRequiredVersion);
#endif

#if !CONSOLE
	#define MAX_LOCAL_PLAYERS 1
#else
	#define MAX_LOCAL_PLAYERS 4
#endif

#define MAX_PROFILE_DATA_SIZE 3000
#define MAX_PERSISTENT_DATA_SIZE (64 * 1024)

#pragma pack(push,8)
	#if CONSOLE
		// Console uses XHV2
		#include <xhv2.h>
		#include <xparty.h>
//		#include <xsocialpost.h>
	#else
		// Windows uses XHV
		#include <xhv.h>
	#endif
#pragma pack(pop)


/** Determines how long a secure address can be idle before getting cleaned up */
#define SECURE_ADDRESS_MAX_IDLE_TIME 5.f

/**
 * Threadsafe cache of secure addresses
 */
class FSecureAddressCache
{
	/**
	 * Handles converting a non-secure ip address to a secure ip address by 
	 * registering/unregistering it.  Maintains a ref count to determine when
	 * the secure ip address is no longer in use.
	 */
	class FCachedSecureIpAddress
	{
		/** How many sockets have connections in flight using the secure address */
		INT RefCount;
		/** The original non-secure addr that needs to be converted */
		FInternetIpAddr NonSecureAddr;
		/** The secure address to release when RefCount hits zero */
		FInternetIpAddr SecureAddr;
		/** The LSP service id is cached for secure address registration */
		INT ServiceId;
		/** This is the amount of time to wait before releasing the address, in case the next connection requests it */
		FLOAT TimeToUnregister;

		/**
		 * Converts a non-secure ip address to a secure ip address by
		 * registering a security association with the Live service.
		 * 
		 * @param InAddr [in] non-secure ip address to convert	
		 * @return TRUE if succeeded
		 */
		UBOOL RegisterSecureAddr(const FInternetIpAddr& InAddr)
		{
			IN_ADDR AddrIP;
			InAddr.GetIp(AddrIP);
			
			IN_ADDR SecureAddrIP;
			appMemzero(&SecureAddrIP,sizeof(IN_ADDR));		

			// Request the secure ip for the given non-secure server ip
			// Note that this registration will create an implicit connection to the server.
			DWORD Result = XNetServerToInAddr(AddrIP,ServiceId,&SecureAddrIP);
			debugf(NAME_DevOnline,
				TEXT("XNetServerToInAddr(%s,%d) returned 0x%08X"),
				*InAddr.ToString(FALSE),
				ServiceId,
				Result);
			if (Result == ERROR_SUCCESS)
			{
				// Now, start the key exchange and block until it completes properly
				Result = XNetConnect(SecureAddrIP);
				debugf(NAME_DevOnline,
					TEXT("XNetConnect() returned 0x%08X"),
					Result);
				SecureAddr.SetIp(SecureAddrIP);
			}			
			return TRUE;
		}

		/**
		 * Un-registers the security association with the Live service for
		 * the previous secure ip address request.
		 *
		 * @return TRUE if succeeded
		 */
		UBOOL UnregisterSecureAddr(void)
		{
			IN_ADDR Addr;
			SecureAddr.GetIp(Addr);
			
			// Assuming all references to this secure ip have been released
			// we can now release the association to the secure ip. 	
			// Note that this also closes the implicit connection to the server 
			// that is required when registering for a secure ip.
			DWORD Result = XNetUnregisterInAddr(Addr);
			debugf(NAME_DevOnline,
				TEXT("XNetUnregisterInAddr(%s) returned 0x%08X"),
				*NonSecureAddr.ToString(FALSE),
				Result);

			return Result == ERROR_SUCCESS;
		}

	public:	
		/**
		 * Note that SecureAddr is simply a copy of the non-secure ip addr 
		 * at this point since it hasn't been registered.
		 *
		 * @param InAddr [in] non-secure ip address to convert
		 * @param InServiceId [in] LSP service id
		 */
		FCachedSecureIpAddress(const FInternetIpAddr& InAddr, INT InServiceId) :
			RefCount(0),
			NonSecureAddr(InAddr),
			SecureAddr(InAddr),
			ServiceId(InServiceId),
			TimeToUnregister(0.f)
		{		
		}
		
		/**
		 * Adds a reference for the current entry.
		 * The initial reference registers the secure address conversion.
		 *
		 * @return current reference count 
		 */
		INT AddRef(void)
		{
			// If the timer is still going, then it doesn't need to be registered
			if (++RefCount == 1 && TimeToUnregister == 0.f)
			{
				RegisterSecureAddr(SecureAddr);
			}
			return RefCount;
		}	
		
		/**
		 * Releases a reference for the current entry.
		 * The final reference unregisters the secure address conversion.
		 *
		 * @return current reference count 
		 */
		INT Release(void)
		{
			if (--RefCount == 0)
			{
				// Set the amount of time to wait before unregistering
				TimeToUnregister = SECURE_ADDRESS_MAX_IDLE_TIME;
			}
			// clamp to 0
			RefCount = Max<INT>(RefCount,0);
			return RefCount;	
		}
		
		/**
		 * Accessor for the secure address
		 *
		 * @return secure ip address
		 */
		FORCEINLINE const FInternetIpAddr& GetSecureAddr() const
		{
			check(RefCount > 0);
			return SecureAddr;
		}

		/**
		 * Ticks the object and only unregisters the secure addr if it has timed out
		 */
		void Tick(FLOAT DeltaTime)
		{
			if (RefCount == 0 && TimeToUnregister > 0.f)
			{
				// Decrement our elapsed time
				TimeToUnregister -= DeltaTime;
				if (TimeToUnregister <= 0.f)
				{
					TimeToUnregister = 0.f;
					UnregisterSecureAddr();
					// default back to non-secure address
					SecureAddr = NonSecureAddr;
				}
			}
		}

		/** Allows the clean up of a secure address */
		void Cleanup(void)
		{
			if (RefCount > 0 ||
				TimeToUnregister > 0.f)
			{
				RefCount = 0;
				TimeToUnregister = 0.f;
				// Let go of the address so we can re-register
				UnregisterSecureAddr();
				// Default back to non-secure address
				SecureAddr = NonSecureAddr;
			}
		}
	};

	typedef TMap<DWORD,FCachedSecureIpAddress*> FIpToSecureIpMap;
	/** Caches a mapping of the original unsecured ip addrs to their ref counted secure versions */
	FIpToSecureIpMap CachedSecureIpAddrs;
	/** Used to provide threadsafe access to the secure ip addrs cache */
	FCriticalSection CachedSecureIpAddrsSynch;

	/** Only the factory method can instantiate this */
	FSecureAddressCache()
	{
	}

public:
	/**
	 * Convert a non-secure ip address to a secure address and caches the result.  
	 * Subsequent requests to convert a non-secure ip address will return the cached entry for the secure address.
	 * Each cached entry is ref counted and if it is no longer in use the secure ip address is unregistered.
	 *
	 * @param InAddr [in] non-secure ip address to be released
	 * @param ServiceId the id to pass in for secure key
	 */
	void CacheSecureIpAddress(const FInternetIpAddr& InAddr,INT ServiceId)
	{
		if (GSocketSubsystem->RequiresEncryptedPackets())
		{
			FScopeLock sl(&CachedSecureIpAddrsSynch);

			DWORD UnsecureAddr;
			InAddr.GetIp(UnsecureAddr);

			// try to find an existing cached entry for the ip address
			FCachedSecureIpAddress* CachedSecureAddr = CachedSecureIpAddrs.FindRef(UnsecureAddr);
			if (CachedSecureAddr == NULL)
			{
				// create a new entry and cache it
				FCachedSecureIpAddress* CachedSecureAddr = new FCachedSecureIpAddress(InAddr,ServiceId);
				CachedSecureIpAddrs.Set(UnsecureAddr,CachedSecureAddr); 
				debugf(NAME_DevOnline,
					TEXT("Caching secure IP request for address: %s"),
					*InAddr.ToString(TRUE));
			}
		}
	}

	/**
	 * Releases a single reference for the cached secure address entry corresponding to the given
	 * non-secure ip address.
	 *
	 * @param InAddr [in] non-secure ip address to be released
	 */
	void ReleaseSecureIpAddress(const FInternetIpAddr& InAddr)
	{
		FScopeLock sl(&CachedSecureIpAddrsSynch);

		DWORD UnsecureAddr;
		InAddr.GetIp(UnsecureAddr);

		FCachedSecureIpAddress* CachedSecureAddr = CachedSecureIpAddrs.FindRef(UnsecureAddr);
		if (CachedSecureAddr != NULL)
		{
			// release the reference to the secure addr
			// once the ref count goes to zero the cached entry is unregistered
			DWORD RefCount = CachedSecureAddr->Release();
		}		
	}

	/**
	 * Removes the cached object completely
	 *
	 * @param InAddr [in] non-secure ip address to be released
	 */
	void ClearSecureIpAddress(const FInternetIpAddr& InAddr)
	{
		FScopeLock sl(&CachedSecureIpAddrsSynch);

		DWORD UnsecureAddr;
		InAddr.GetIp(UnsecureAddr);

		FCachedSecureIpAddress* CachedSecureAddr = CachedSecureIpAddrs.FindRef(UnsecureAddr);
		if (CachedSecureAddr != NULL)
		{
			// Unregister, delete, and clear from the cache
			CachedSecureAddr->Cleanup();
			CachedSecureIpAddrs.Remove(UnsecureAddr);
			delete CachedSecureAddr;
		}		
	}

	/**
	 * Retrieves the cached secure ip address corresponding to the non-secure address specified 
	 * and updates its reference count.
	 *
	 * @param OutSecureAddr [out] resulting secure ip address after conversion
	 * @param InAddr [in] non-secure ip address to be converted
	 */
	void GetSecureIpAddress(FInternetIpAddr& OutSecureAddr, const FInternetIpAddr& InAddr)
	{
		FScopeLock sl(&CachedSecureIpAddrsSynch);

		DWORD UnsecureAddr;
		InAddr.GetIp(UnsecureAddr);

		// try to find an existing cached entry for the ip address
		FCachedSecureIpAddress* CachedSecureAddr = CachedSecureIpAddrs.FindRef(UnsecureAddr);

		if (CachedSecureAddr != NULL)
		{
			// keep track of references to the cached secure ip entry
			// the first reference it will trigger the secure addr registration
			CachedSecureAddr->AddRef();
			OutSecureAddr = CachedSecureAddr->GetSecureAddr();
			// ports may not match so pass-through the unsecure port #
			OutSecureAddr.SetPort(InAddr.GetPort());
		}
		else
		{
			// if no entry was found then no conversion is needed for the ip addr
			OutSecureAddr = InAddr;
		}
	}

	/**
	 * @return gets the cache object from the factory method
	 */
	static FSecureAddressCache& GetCache(void)
	{
		static FSecureAddressCache Cache;
		return Cache;
	}

	/**
	 * Makes sure that the addresses are all cleaned up
	 */
	void Cleanup(void)
	{
		FScopeLock sl(&CachedSecureIpAddrsSynch);
		// Make sure all references to the registered secure ip addrs are released
		for (FIpToSecureIpMap::TIterator It(CachedSecureIpAddrs); It; ++It)
		{
			FCachedSecureIpAddress* CachedSecureIpAddr = It.Value();
			CachedSecureIpAddr->Cleanup();
			delete CachedSecureIpAddr;
		}
		CachedSecureIpAddrs.Empty();
	}

	/**
	 * Checks each secure address to determine if it needs to be unregistered
	 *
	 * @param DeltaTime the amount of time that has passed since the last tick
	 */
	void Tick(FLOAT DeltaTime)
	{
		FScopeLock sl(&CachedSecureIpAddrsSynch);
		// Look at all cached addresses so they can be ticked
		for (FIpToSecureIpMap::TIterator It(CachedSecureIpAddrs); It; ++It)
		{
			FCachedSecureIpAddress* CachedSecureIpAddr = It.Value();
			if (CachedSecureIpAddr != NULL)
			{
				// Tick each connection allowing it to delay clean up
				CachedSecureIpAddr->Tick(DeltaTime);
			}
		}
	}
};

/**
 * LSP specific resolve info object. Resolves a host by service ID
 */
class FResolveInfoLsp :
	public FResolveInfo
{
	//
	// A simple wrapper task that calls back to FResolveInfoLsp to do the actual work
	//
	class FResolveInfoLspWorker : public FNonAbandonableTask
	{
	public:
		/** Pointer to FResolveInfoLsp to call for async work*/
		FResolveInfoLsp* Parent;

		/** Constructor
		* @param InParent the FResolveInfoLsp to route the async call to
		*/
		FResolveInfoLspWorker(FResolveInfoLsp* InParent)
			: Parent(InParent)
		{
		}

		/** Call DoWork on the parent */
		void DoWork()
		{
			Parent->DoWork();
		}
		/** Give the name for external event viewers
		* @return	the name to display in external event viewers
		*/
		static const TCHAR *Name()
		{
			return TEXT("FResolveInfoLspWorker");
		}
	};
	/** The host name to resolve */
	ANSICHAR HostName[256];
	/** The service id to use */
	INT ServiceId;
	/** The handle of the enumeration task */
	HANDLE LspEnumerateHandle;
	/** The buffer that is used to read into */
	BYTE* EnumBuffer;
	/** The result of the host resolve */
	INT ErrorCode;
	/** The cached ip address */
	FInternetIpAddr Addr;
	/** Read 16 ips at a time */
	static const DWORD NumLspsPerEnum = 16;
	/** Async task for async resolve */
	FAsyncTask<FResolveInfoLspWorker> AsyncTask;

public:
	/**
	 * Forwards the host name to the base class and zeros the handle and buffer
	 *
	 * @param InHostName the name of the host being resolved
	 */
	FResolveInfoLsp(const ANSICHAR* InHostName,INT InServiceId) :
		ServiceId(InServiceId),
		LspEnumerateHandle(NULL),
		EnumBuffer(NULL),
		ErrorCode(SE_NO_ERROR),
		AsyncTask(this)
	{
		appStrncpyANSI(HostName,InHostName,256);
	}

	/** Cleans up any resources */
	virtual ~FResolveInfoLsp(void)
	{
		if (LspEnumerateHandle)
		{
			XCloseHandle(LspEnumerateHandle);
		}
		delete [] EnumBuffer;
	}

	/**
	 * Resolves the specified host name using the XTitleServerCreateEnumerator
	 */
	virtual void DoWork(void)
	{
		Addr.SetIp(0);
		DWORD BufferSize = 0;
		// Ask for how large the buffer will be
	    DWORD Result = XTitleServerCreateEnumerator(HostName,
			NumLspsPerEnum,
			&BufferSize,
			&LspEnumerateHandle);
		debugf(NAME_DevOnline,
			TEXT("XTitleServerCreateEnumerator(%s,%d) returned 0x%08X"),
			ANSI_TO_TCHAR(HostName),
			BufferSize,
			Result);
		if (Result == ERROR_SUCCESS)
		{
			DWORD ItemsReturned = NumLspsPerEnum;
			EnumBuffer = new BYTE[BufferSize];
			// Holds a list of enumerated addresses
			TArray<IN_ADDR,TInlineAllocator<100> > LspAddrsFound;
			do 
			{
				// Have it read the next enumeration chunk in the list
				Result = XEnumerate(LspEnumerateHandle,
					EnumBuffer,
					BufferSize,
					&ItemsReturned,
					NULL);
				debugf(NAME_DevOnline,
					TEXT("XEnumerate(%s,%d,%d) returned 0x%08X"),
					ANSI_TO_TCHAR(HostName),
					BufferSize,
					ItemsReturned,
					Result);
				if (Result == ERROR_SUCCESS)
				{
					XTITLE_SERVER_INFO* ServerInfo = (XTITLE_SERVER_INFO*)EnumBuffer;
					// Add each in_addr to the list of enumerated LSPs
					for (DWORD Index = 0; Index < ItemsReturned; Index++)
					{
						LspAddrsFound.AddItem(ServerInfo[Index].inaServer);
					}
				}
			}
			while(Result == ERROR_SUCCESS);
			// Error no more files means the enumeration has completed
			if (Result == ERROR_NO_MORE_FILES)
			{
				if (LspAddrsFound.Num() > 0)
				{
					DWORD Rand;
					XNetRandom((BYTE*)&Rand,sizeof(DWORD));
					// Choose a random one to use as our server ip from the list of all LSP addresses
					DWORD ServerIndex = Rand % (DWORD)LspAddrsFound.Num();
					debugf(NAME_DevOnline,TEXT("Choosing random SG %d out of %d"),ServerIndex + 1,LspAddrsFound.Num());
					// Copy the address
					Addr.SetIp(LspAddrsFound(ServerIndex));
					debugf(NAME_DevOnline,TEXT("Resolved (%s) to %s"),ANSI_TO_TCHAR(HostName),*Addr.ToString(FALSE));
					// Cache for reuse so we don't do additional resolves
					GSocketSubsystem->AddHostNameToCache(HostName,Addr);
					// keep track of ips that will need to be converted to secure addrs before establishing a connection
					FSecureAddressCache::GetCache().CacheSecureIpAddress(Addr,ServiceId);
					// Don't treat enumeration being complete as an error
					Result = ERROR_SUCCESS;
				}
				else
				{
					Result = E_FAIL;
					debugf(NAME_DevOnline,TEXT("XEnumerate() returned %d items"),LspAddrsFound.Num());
				}
			}
			// Clean up the handles/memory
			XCloseHandle(LspEnumerateHandle);
			LspEnumerateHandle = NULL;
			delete [] EnumBuffer;
			EnumBuffer = NULL;
		}
		else
		{
			// Failed to resolve
			Result = E_FAIL;
		}
		WSASetLastError(SE_HOST_NOT_FOUND);
		ErrorCode = Result == ERROR_SUCCESS ? SE_NO_ERROR : SE_HOST_NOT_FOUND;
	}

	/**
	 * Start the async work and perform it synchronously if no thread pool is available
	 */
	void StartAsyncTask(void)
	{
		check(AsyncTask.GetTask().Parent == this); // need to make sure these aren't memcpy'd around after contruction
		AsyncTask.StartBackgroundTask();
	}

// FResolveInfo interface

	/**
	 * Whether the async process has completed or not
	 *
	 * @return true if it completed successfully, false otherwise
	 */
	virtual UBOOL IsComplete(void) const
	{
		// this semantically const, but IsDone syncs the async task, and that causes writes
		return const_cast<FAsyncTask<FResolveInfoLspWorker> &>(AsyncTask).IsDone();
	}

	/**
	 * The error that occured when trying to resolve
	 *
	 * @return error code from the operation
	 */
	virtual INT GetErrorCode(void) const
	{
		return ErrorCode;
	}

	/**
	 * Returns a copy of the resolved address
	 *
	 * @return the resolved IP address
	 */
	virtual FInternetIpAddr GetResolvedAddress(void) const
	{
		return Addr;
	}
};

/**
 * Live specific IP net driver implementation
 */
class UIpNetDriverLive :
	public UTcpNetDriver
{
	DECLARE_CLASS_INTRINSIC(UIpNetDriverLive,UTcpNetDriver,CLASS_Config|CLASS_Transient,OnlineSubsystemLive)

#if STATS
	/** Tracks the overhead percentage of in bound bytes */
	DWORD InPercentOverhead;
	/** Tracks the overhead percentage of out bound bytes */
	DWORD OutPercentOverhead;
#endif

private:
	/**
	 * Queues any local voice packets for replication
	 */
	virtual void TickFlush(void);

	/** Used to disable auto downloading */
	virtual UBOOL InitConnect( FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error )
	{
		AllowDownloads = FALSE;
		return Super::InitConnect(InNotify,ConnectURL,Error);
	}

	/** Used to disable auto downloading */
	virtual UBOOL InitListen( FNetworkNotify* InNotify, FURL& ListenURL, FString& Error )
	{
		AllowDownloads = FALSE;
		return Super::InitListen(InNotify,ListenURL,Error);
	}
};

// The size of the VDP buffer
#define VDP_BUFFER_SIZE 512

// The max size we can have for sending
#define XE_MAX_PACKET_SIZE (VDP_BUFFER_SIZE - 2)

/**
 * Live specific IP connection implementation
 */
class UIpConnectionLive :
	public UTcpipConnection
{
	DECLARE_CLASS_INTRINSIC(UIpConnectionLive,UTcpipConnection,CLASS_Config|CLASS_Transient,OnlineSubsystemLive)

private:
	/** If TRUE, -nolive was specified, so we should just fall back to UTcpipConnection */
	UBOOL bUseFallbackConnection;
	/** So we don't have to cast to get to the ReplicateVoicePacket() method */
	UIpNetDriverLive* XeNetDriver;
	/** Whether this connection is sending voice data as part of game data packets */
	UBOOL bUseVDP;
	/** Buffer used to merge game and voice data before sending */
	BYTE Buffer[VDP_BUFFER_SIZE];
	/** Whether we are running as a server or not */
	UBOOL bIsServer;
	/** TRUE if the socket has detected a critical error */
	UBOOL bHasSocketError;

	/**
	 * Emits object references for GC
	 */
	void StaticConstructor(void);

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
	virtual void InitConnection(UNetDriver* InDriver,FSocket* InSocket,
		const FInternetIpAddr& InRemoteAddr,EConnectionState InState,
		UBOOL InOpenedLocally,const FURL& InURL,INT InMaxPacket = 0,
		INT InPacketOverhead = 0);

	/**
	 * Sends a byte stream to the remote endpoint using the underlying socket.
	 * To minimize badwidth being consumed by encryption overhead, the voice
	 * data is appended to the gamedata before sending
	 *
	 * @param Data the byte stream to send
	 * @param Count the length of the stream to send
	 */
	virtual void LowLevelSend(void* Data,INT Count);

	/**
	 * Attempts to pack all of the voice packets in the voice channel into the buffer.
	 *
	 * @param WriteAt the point in the buffer to write voice data
	 * @param SpaceAvail the amount of space left in the merge buffer
	 * @param OutBytesMerged the amount of data added to the buffer
	 *
	 * @return TRUE if the merging included all replicated packets, FALSE otherwise (not enough space)
	 */
	UBOOL MergeVoicePackets(BYTE* WriteAt,DWORD SpaceAvail,WORD& OutBytesMerged);

	/**
	 * Processes the byte stream for VDP merged packet support. Parses out all
	 * of the voice packets and then forwards the game data to the base class
	 * for handling
	 *
	 * @param Data the data to process
	 * @param Count the size of the data buffer to process
	 */
	virtual void ReceivedRawPacket(void* Data,INT Count);
};

#endif	// #if WITH_UE3_NETWORKING && (XBOX || WITH_PANORAMA)

#endif
