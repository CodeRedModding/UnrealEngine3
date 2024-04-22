/*============================================================================
	UnSocket.cpp: Common interface for WinSock and BSD sockets.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
============================================================================*/

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

#include "NetworkProfiler.h"

/** The global socket subsystem pointer */
FSocketSubsystem* GSocketSubsystem = NULL;
FSocketSubsystem* GSocketSubsystemDebug = NULL;

//
// Class for creating a background thread to resolve a host.
//
class FResolveInfoAsync :
	public FResolveInfo
{

	//
	// A simple wrapper task that calls back to FResolveInfoAsync to do the actual work
	//
	class FResolveInfoAsyncWorker
	{
	public:
		/** Pointer to FResolveInfoAsync to call for async work*/
		FResolveInfoAsync* Parent;


		/** Constructor
		* @param InParent the FResolveInfoAsync to route the async call to
		*/
		FResolveInfoAsyncWorker(FResolveInfoAsync* InParent)
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
			return TEXT("FResolveInfoAsyncWorker");
		}

		/** Indicates to the thread pool that this task is abandonable */
		UBOOL CanAbandon()
		{
			return TRUE;
		}

		/** Effects the ending of the async resolve */
		void Abandon()
		{
			appInterlockedExchange(&Parent->bShouldAbandon,TRUE);
		}
	};
	// Variables.
	FInternetIpAddr		Addr;
	ANSICHAR	HostName[256];
	/** Error code returned by GetHostByName. */
	INT			ErrorCode;
	/** Tells the worker thread whether it should abandon it's work or not */
	volatile INT bShouldAbandon;
	/** Async task for async resolve */
	FAsyncTask<FResolveInfoAsyncWorker> AsyncTask;

public:
	/**
	 * Copies the host name for async resolution
	 *
	 * @param InHostName the host name to resolve
	 */
	FResolveInfoAsync(const ANSICHAR* InHostName) :
		ErrorCode(SE_NO_ERROR),
		bShouldAbandon(FALSE),
		AsyncTask(this)
	{
		appStrncpyANSI(HostName,InHostName,256);
	}

	/**
	 * Start the async work and perform it synchronously if no thread pool is available
	 */
	void StartAsyncTask(void)
	{
		check(AsyncTask.GetTask().Parent == this); // need to make sure these aren't memcpy'd around after contruction
		AsyncTask.StartBackgroundTask();
	}

	/**
	 * Resolves the specified host name
	 */
	void DoWork()
	{
		Addr.SetIp(0);
		INT AttemptCount = 0;
		// Make up to 3 attempts to resolve it
		do 
		{
			ErrorCode = GSocketSubsystem->GetHostByName(HostName,Addr);
			if (ErrorCode != SE_NO_ERROR)
			{
				if (ErrorCode == SE_HOST_NOT_FOUND || ErrorCode == SE_NO_DATA || ErrorCode == SE_ETIMEDOUT)
				{
					// Force a failure
					AttemptCount = 3;
				}
			}
			AttemptCount++;
		}
		while (ErrorCode != SE_NO_ERROR && AttemptCount < 3 && bShouldAbandon == FALSE);
		if (ErrorCode == SE_NO_ERROR)
		{
			// Cache for reuse
			GSocketSubsystem->AddHostNameToCache(HostName,Addr);
		}
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
		return const_cast<FAsyncTask<FResolveInfoAsyncWorker> &>(AsyncTask).IsDone();
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
 * Determines if the result code is considered an error or not
 *
 * @param ResultCode the return code to check
 *
 * @return TRUE if the code is an error code, FALSE otherwise
 */
UBOOL FSocketSubsystem::IsSocketError(INT ResultCode)
{
	if (ResultCode == -1)
	{
		ResultCode = GetLastErrorCode();
	}
	return ResultCode != SE_NO_ERROR;
}

/**
 * Checks the host name cache for an existing entry (faster than resolving again)
 *
 * @param HostName the host name to search for
 * @param Addr the out param that the IP will be copied to
 *
 * @return TRUE if the host was found, FALSE otherwise
 */
UBOOL FSocketSubsystem::GetHostByNameFromCache(ANSICHAR* HostName,FInternetIpAddr& Addr)
{
	// Lock for thread safety
	FScopeLock sl(&HostNameCacheSync);
	// Now search for the entry
	FInternetIpAddr* FoundAddr = HostNameCache.Find(FString(HostName));
	if (FoundAddr)
	{
		Addr = *FoundAddr;
	}
	return FoundAddr != NULL;
}

/**
 * Stores the ip address with the matching host name
 *
 * @param HostName the host name to search for
 * @param Addr the out param that the IP will be copied to
 */
void FSocketSubsystem::AddHostNameToCache(ANSICHAR* HostName,const FInternetIpAddr& Addr)
{
	// Lock for thread safety
	FScopeLock sl(&HostNameCacheSync);
	HostNameCache.Set(FString(HostName),Addr);
}

/**
 * Removes the host name to ip mapping from the cache
 *
 * @param HostName the host name to search for
 */
void FSocketSubsystem::RemoveHostNameFromCache(ANSICHAR* HostName)
{
	// Lock for thread safety
	FScopeLock sl(&HostNameCacheSync);
	HostNameCache.Remove(FString(HostName));
}

/**
 * Creates a platform specific async hostname resolution object
 *
 * @param HostName the name of the host to look up
 *
 * @return the resolve info to query for the address
 */
FResolveInfo* FSocketSubsystem::GetHostByName(ANSICHAR* HostName)
{
	FResolveInfo* Result = NULL;
	FInternetIpAddr Addr;
	// See if we have it cached or not
	if (GetHostByNameFromCache(HostName,Addr))
	{
		Result = new FResolveInfoCached(Addr);
	}
	else
	{
		// Create an async resolve info
		FResolveInfoAsync* AsyncResolve = new FResolveInfoAsync(HostName);
		AsyncResolve->StartAsyncTask();
		Result = AsyncResolve;
	}
	return Result;
}

//
// FSocketData functions
//

FString FSocketData::GetString( UBOOL bAppendPort )
{
	return Addr.ToString(bAppendPort);
}

void FSocketData::UpdateFromSocket(void)
{
	if (Socket != NULL)
	{
		Addr = Socket->GetAddress();
		Addr.GetPort(Port);
	}
}

//
// FIpAddr functions
//

/**
 * Constructs an ip address from an internet ip address
 *
 * @param InternetIpAddr the ip address to get host order info from
 */
FIpAddr::FIpAddr(const FInternetIpAddr& SockAddr)
{
	const FIpAddr New = SockAddr.GetAddress();
	Addr = New.Addr;
	Port = New.Port;
}

/**
 * Converts this address into string form. Optionally including the port info
 *
 * @param bShowPort whether to append the port number or not
 *
 * @return A new string object with the ip address data in it
 */
FString FIpAddr::ToString( UBOOL bAppendPort ) const
{
	// Get the individual bytes
	const INT A = (Addr >> 24) & 0xFF;
	const INT B = (Addr >> 16) & 0xFF;
	const INT C = (Addr >> 8) & 0xFF;
	const INT D = Addr & 0xFF;
	if (bAppendPort)
	{
		return FString::Printf(TEXT("%i.%i.%i.%i:%i"),A,B,C,D,Port);
	}
	else
	{
		return FString::Printf(TEXT("%i.%i.%i.%i"),A,B,C,D);
	}
}

/**
 * Builds an internet address from this host ip address object
 */
FInternetIpAddr FIpAddr::GetSocketAddress(void) const
{
	FInternetIpAddr Result;
	Result.SetIp(Addr);
	Result.SetPort(Port);
	return Result;
}


//
// FSocket stats implementation
//

/**
 * Sends a buffer to a network byte ordered address
 *
 * @param Data the buffer to send
 * @param Count the size of the data to send
 * @param BytesSent out param indicating how much was sent
 * @param Destination the network byte ordered address to send to
 */
UBOOL FSocket::SendTo(const BYTE* Data,INT Count,INT& BytesSent,const FInternetIpAddr& Destination)
{
	NETWORK_PROFILER(GNetworkProfiler.TrackSocketSendTo(this,Data,BytesSent,Destination));
	debugfSuppressed( NAME_DevNetTrafficDetail, TEXT("Socket '%s' SendTo %i Bytes"), *SocketDescription, BytesSent );
	return TRUE;
}

/**
 * Sends a buffer on a connected socket
 *
 * @param Data the buffer to send
 * @param Count the size of the data to send
 * @param BytesSent out param indicating how much was sent
 */
UBOOL FSocket::Send(const BYTE* Data,INT Count,INT& BytesSent)
{
	NETWORK_PROFILER(GNetworkProfiler.TrackSocketSend(this,Data,BytesSent));
	debugfSuppressed( NAME_DevNetTrafficDetail, TEXT("Socket '%s' Send %i Bytes"), *SocketDescription, BytesSent );
	return TRUE;
}

/**
 * Reads a chunk of data from the socket. Gathers the source address too
 *
 * @param Data the buffer to read into
 * @param BufferSize the max size of the buffer
 * @param BytesRead out param indicating how many bytes were read from the socket
 * @param Source out param receiving the address of the sender of the data
 */
UBOOL FSocket::RecvFrom(BYTE* Data,INT BufferSize,INT& BytesRead,FInternetIpAddr& Source)
{
	if( BytesRead > 0 )
	{
		debugfSuppressed( NAME_DevNetTrafficDetail, TEXT("Socket '%s' RecvFrom %i Bytes"), *SocketDescription, BytesRead );
	}
	return TRUE;
}

/**
 * Reads a chunk of data from a connected socket
 *
 * @param Data the buffer to read into
 * @param BufferSize the max size of the buffer
 * @param BytesRead out param indicating how many bytes were read from the socket
 */
UBOOL FSocket::Recv(BYTE* Data,INT BufferSize,INT& BytesRead)
{
	if( BytesRead > 0 )
	{
		debugfSuppressed( NAME_DevNetTrafficDetail, TEXT("Socket '%s' Recv %i Bytes"), *SocketDescription, BytesRead );
	}
	return TRUE;
}

/**
 * Convert ip address to platform data and serialize to buffer
 *
 * @param Buffer array to serialize to
 * @return TRUE if encoding was successful
 */
UBOOL FPlatformIpAddr::SerializeToBuffer(TArray<BYTE>& ToBuffer)
{	
	UBOOL bValid = FALSE;
 	if (GSocketSubsystem->RequiresEncryptedPackets())
 	{
 		bValid = GSocketSubsystem->EncodeIpAddr(ToBuffer,*this);
 	}
 	else
 	{
 		ToBuffer.Empty(sizeof(DWORD));
 		ToBuffer.AddZeroed(sizeof(DWORD));
 		BYTE* Buffer = ToBuffer.GetData();
 		appMemcpy(Buffer,&Addr,sizeof(DWORD));
 		bValid = TRUE;
 	}
	if (!bValid)
	{
		debugf(NAME_DevNet,TEXT("FPlatformIpAddr failed to encode ip into buffer."));
	}
	return bValid;
}

/**
 * Serialize from buffer and convert platform data to ip address
 *
 * @param Buffer array to serialize from
 * @return TRUE if decoding was successful
 */
UBOOL FPlatformIpAddr::SerializeFromBuffer(const TArray<BYTE>& FromBuffer)
{
	UBOOL bValid = FALSE;
	if (GSocketSubsystem->RequiresEncryptedPackets())
	{
		bValid = GSocketSubsystem->DecodeIpAddr(*this,FromBuffer);
	}
	else
	{
		if (FromBuffer.Num() == sizeof(DWORD))
		{
			const BYTE* Buffer = FromBuffer.GetData();
			appMemcpy(&Addr,Buffer,sizeof(DWORD));
			bValid = TRUE;
		}
	}
	if (!bValid)
	{
		debugf(NAME_DevNet,TEXT("FPlatformIpAddr failed to decode ip from buffer."));
	}
	return bValid;
}

#endif	//#if WITH_UE3_NETWORKING
