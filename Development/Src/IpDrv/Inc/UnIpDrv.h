/*=============================================================================
	IpDrvPrivate.h: Unreal TCP/IP driver.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef UNIPDRV_H
#define UNIPDRV_H

// include for all definitions
#include "Engine.h"

#if WITH_UE3_NETWORKING

#ifndef ERROR_SUCCESS
#define ERROR_SUCCESS 0
#endif

#if _MSC_VER
	#pragma warning( disable : 4201 )
#endif

// Set the socket api name depending on platform
#if _MSC_VER
#if XBOX || WITH_PANORAMA
	#define SOCKET_API TEXT("LiveSock")
#else
	#define SOCKET_API TEXT("WinSock")
#endif
#elif PS3
	#define SOCKET_API TEXT("PS3Sockets")
#elif NGP
	#define SOCKET_API TEXT("NGPSockets")
#elif WIIU
	#define SOCKET_API TEXT("WiiUSockets")
#else
	#define SOCKET_API TEXT("Sockets")
#endif

// BSD socket includes.
#if NGP 
	#include <net.h>
#elif WIIU
	#include <cafe.h>
	#include <cafe/network.h>
#elif !_MSC_VER
	#include <stdio.h>
	#include <unistd.h>
	#include <sys/types.h>
#if PS3
	#include <sdk_version.h>
	#include <sys/ansi.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <netex/errno.h>
	#include <netex/net.h>
	#include <sys/time.h>
	#include <sys/select.h>
	#include <cell/sysmodule.h>
	#include <np.h>
#else
	#include <errno.h>
	#include <fcntl.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <sys/uio.h>
	#include <sys/ioctl.h>
	#include <sys/time.h>
	#include <pthread.h>
#endif
	// Handle glibc < 2.1.3
	#ifndef MSG_NOSIGNAL
	#define MSG_NOSIGNAL 0x4000
	#endif
#endif	// !_MSC_VER


/*-----------------------------------------------------------------------------
	Includes..
-----------------------------------------------------------------------------*/

#include "UnNet.h"
// Base socket interfaces
#include "UnSocket.h"
// Platform specific implementations
#if PS3
	#include "UnSocketPS3.h"
#elif IPHONE || ANDROID || PLATFORM_MACOSX || FLASH
	#include "UnSocketBSD.h"
#elif NGP
	#include "NGPSockets.h"
#elif WIIU
	#include "UnSocketWiiU.h"
#else
	#include "UnSocketWin.h"
#endif

// make sure each platform has defined the required SE_* codes that the engine uses
// if you get any error here, then your platform did not define them in it's Socket header file
checkAtCompileTime(SE_NO_ERROR != SE_NO_ERROR + 1, CompileTest1);
checkAtCompileTime(SE_ENOTSOCK != SE_ENOTSOCK + 1, CompileTest2);
checkAtCompileTime(SE_ECONNRESET != SE_ECONNRESET + 1, CompileTest3);
checkAtCompileTime(SE_ENOBUFS != SE_ENOBUFS + 1, CompileTest4);
checkAtCompileTime(SE_ETIMEDOUT != SE_ETIMEDOUT + 1, CompileTest5);
checkAtCompileTime(SE_NO_DATA != SE_NO_DATA + 1, CompileTest6);
checkAtCompileTime(SE_UDP_ERR_PORT_UNREACH != SE_UDP_ERR_PORT_UNREACH + 1, CompileTest7);

/*-----------------------------------------------------------------------------
	Definitions.
-----------------------------------------------------------------------------*/

// Globals.
extern UBOOL GIpDrvInitialized;

#if !SHIPPING_PC_GAME
/** Network communication between UE3 executable and the "Unreal Console" tool. */
extern class FDebugServer*				GDebugChannel;
#endif

/*-----------------------------------------------------------------------------
	More Includes.
-----------------------------------------------------------------------------*/

#include "InternetLink.h"
#include "FRemotePropagator.h"
#include "FDebugServer.h"

/*-----------------------------------------------------------------------------
	Bind to next available port.
-----------------------------------------------------------------------------*/

//
// Bind to next available port.
//
inline INT bindnextport( FSocket* Socket, FInternetIpAddr& Addr, INT portcount, INT portinc )
{
	for( INT i=0; i<portcount; i++ )
	{
		if( Socket->Bind(Addr) == TRUE )
		{
			if (Addr.GetPort() != 0)
			{
				return Addr.GetPort();
			}
			else
			{
				return Socket->GetPortNo();
			}
		}
		if( Addr.GetPort() == 0 )
			break;
		Addr.SetPort(Addr.GetPort() + portinc);
	}
	return 0;
}

//
// Get local IP to bind to
//
inline FInternetIpAddr getlocalbindaddr( FOutputDevice& Out )
{
	FInternetIpAddr BindAddr;
	// If we can bind to all addresses, return 0.0.0.0
	if (GSocketSubsystem->GetLocalHostAddr(Out,BindAddr) == TRUE)
	{
		BindAddr.SetAnyAddress();
	}
	return BindAddr;

}


/*-----------------------------------------------------------------------------
	Forward Declarations
-----------------------------------------------------------------------------*/
template<typename TTask> class FAsyncTask;
class FCompressAsyncWorker;
class FUncompressAsyncWorker;

/*-----------------------------------------------------------------------------
	Public includes.
-----------------------------------------------------------------------------*/

#include "VoiceInterface.h"
// Common code shared across all platforms for the online subsystem
#include "OnlineSubsystemUtilities.h"
#include "IpDrvClasses.h"
#include "EngineUserInterfaceClasses.h"
#include "EngineUIPrivateClasses.h"
#include "IpDrvUIPrivateClasses.h"

#include "UnTcpNetDriver.h"


// Per platform driver/connection implementations
#if PS3
#endif

#include "UnStatsNotifyProviders_UDP.h"

/** Class for compressing a buffer on another thread */
class FCompressAsyncWorker
{
	/** Flags for use in compression */
	ECompressionFlags CompressionFlags;
	/** Buffer to to compress */
	const BYTE* SourceBuffer;
	/** The uncompressed size of the data */
	INT UncompressedBufferSize;
	/** Buffer to write the compressed data to */
	BYTE* CompressedBuffer;
	/** The size of the data after compression */
	INT* CompressedBufferSize;

public:
	/**
	 * Inits the members
	 *
	 * @param InBuffer the buffer being compressed
	 * @param InUncompressedSize the size of the uncompressed data
	 * @param InCompressedBuffer the buffer to compress the data to
	 * @param InCompressedBufferSize the amount of space that can be written to by compression
	 */
	FCompressAsyncWorker(ECompressionFlags InCompressionFlags, const BYTE* InBuffer,INT InUncompressedSize,BYTE* InCompressedBuffer,INT* InCompressedBufferSize) :
		CompressionFlags(InCompressionFlags),
		SourceBuffer(InBuffer),
		UncompressedBufferSize(InUncompressedSize),
		CompressedBuffer(InCompressedBuffer),
		CompressedBufferSize(InCompressedBufferSize)
	{
	}
	/** Compress the buffer on another thread */
	void DoWork()
	{
		verify(appCompressMemory(
			CompressionFlags,
			CompressedBuffer,
			*CompressedBufferSize,
			(void*)SourceBuffer,
			UncompressedBufferSize));
	}

	/**
	 * @return the name to display in external event viewers
	 */
	static const TCHAR *Name()
	{
		return TEXT("FCompressAsyncWorker");
	}

	/** Indicates to the thread pool that this task is not abandonable */
	UBOOL CanAbandon()
	{
		return FALSE;
	}

	/** Ignored */
	void Abandon()
	{
	}
};


/** Holds the data needed to compress and then post asynchronously */
struct FMCPEventPoster
{
	/** The source array that is being compressed */
	TArray<BYTE> SourceBuffer;
	/** The dest array being compressed into */
	TArray<BYTE> CompressedBuffer;
	/** Receives the compressed size when done */
	INT OutCompressedSize;
	/** The URL to post with */
	FURL Url;
	/** The object that will post to the web service */
	FHttpDownloadString* HttpPoster;
	/** The compression worker that does the compression on another thread */
	FAsyncTask<FCompressAsyncWorker>* CompressionWorker;

	/** Ctor that zeros things */
	FMCPEventPoster() :
		HttpPoster(NULL),
		CompressionWorker(NULL)
	{
	}
};

/** Class for Uncompressing a buffer on another thread */
class FUncompressAsyncWorker
{
	/** Flags for use in uncompression */
	ECompressionFlags CompressionFlags;
	/** Buffer to to Uncompress */
	const BYTE* SourceBuffer;
	/** The compressed size of the data */
	INT CompressedBufferSize;
	/** Buffer to write the Uncompressed data to */
	BYTE* UncompressedBuffer;
	/** The size of the data after Uncompression */
	INT UncompressedBufferSize;

public:
	/**
	 * Inits the members
	 *
	 * @param InBuffer the buffer being compressed
	 * @param InUncompressedSize the size of the uncompressed data
	 * @param InCompressedBuffer the buffer to compress the data to
	 * @param InCompressedBufferSize the amount of space that can be written to by compression
	 */
	FUncompressAsyncWorker(ECompressionFlags InCompressionFlags, const BYTE* InCompressedBuffer,INT InCompressedBufferSize, BYTE* InUncompressedBuffer, INT InUncompressedSize) :
		CompressionFlags(InCompressionFlags),
		SourceBuffer(InCompressedBuffer),
		CompressedBufferSize(InCompressedBufferSize),
		UncompressedBuffer(InUncompressedBuffer),
		UncompressedBufferSize(InUncompressedSize)
	{
	}
	/** Uncompress the buffer on another thread */
	void DoWork()
	{
		verify(appUncompressMemory(
			CompressionFlags,
			UncompressedBuffer,
			UncompressedBufferSize,
			(void*)SourceBuffer,
			CompressedBufferSize));
	}

	/**
	 * @return the name to display in external event viewers
	 */
	static const TCHAR *Name()
	{
		return TEXT("FUncompressAsyncWorker");
	}

	/** Indicates to the thread pool that this task is not abandonable */
	UBOOL CanAbandon()
	{
		return FALSE;
	}

	/** Ignored */
	void Abandon()
	{
	}
};


#endif	//#if WITH_UE3_NETWORKING

#endif // UNIPDRV_H

