/*=============================================================================
	HTTPDownload.h: HTTP downloading support functions
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef UHTTPDownload_H
#define UHTTPDownload_H

#if WITH_UE3_NETWORKING

enum EHTTPState
{
	HTTP_Error,
	HTTP_Initialized,
	HTTP_Resolving,
	HTTP_Resolved,
	HTTP_Connecting,
	HTTP_ReceivingHeader,
	HTTP_ParsingHeader,
	HTTP_ReceivingData,
	HTTP_PostPayload,
	HTTP_Closed
};

#define HTTP_GET_COMMAND TEXT("GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: UE3-%s\r\nContent-Type: application/x-www-form-urlencoded\r\nConnection: close\r\n\r\n")
#define HTTP_BUFFER_SIZE 1024

#define HTTP_POST_COMMAND TEXT("POST %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: UE3-%s\r\nContent-Type: text/plain\r\nContent-Length: %d\r\nConnection: close\r\n\r\n")

/** The type of request to send */
enum EHttpRequestType
{
	HRT_Get,
	HRT_Post
};

/**
 * Class for GET/POST-ing a file via HTTP and writing the results into an archive
 */
class FHttpDownload
{
	/** The temporary buffer used while reading the headers from the HTTP server */
	FString HeaderBuffer;
	/** Buffer that we'll read data into */
	TArray<BYTE> ReadBuffer;
	/** How long to wait before bailing out on a connection */
	FLOAT ConnectionTimeout;
	/** The maximum number of redirects to follow before generating an error */
	INT MaxRedirects;
	/** The number of redirects to left before generating an error */
	INT RemainingRedirects;
	/** The current state of the HTTP connection */
	EHTTPState HttpState;
	/** The socket connected to the server we are downloading from */
	FSocket* ServerSocket;
	/** Address that we are connecting to */
	FInternetIpAddr ServerAddr;
	/** An optional override port to use (ignores URL and/or default ports) */
	INT OverrideHttpPort;
	/** DNS resolution of host name if needed */
	FResolveInfo* ResolveInfo;
	/** The set of HTTP headers returned by the server */
	TArray<FString>	Headers;
	/** The time the connection was started */
	FLOAT ConnectionStartTime;
	/** The URL to download from */
	FURL RequestUrl;
	/** The archive to write the downloaded data to */
	FArchive* Archive;
	/** The amount of data left to read for the file */
	INT SizeRemaining;
	/** the wrapper handling package downloading (NULL if not downloading a package) */
	UDownload* PackageDownload;
	/** Optional URL parameters */
	FString Parameters;
	/** The type of request to send to the web server */
	EHttpRequestType RequestType;
	/** The buffer holding the data that is being delivered during a HTTP POST */
	TArray<BYTE> Payload;
	/** The amount of data posted so far */
	INT AmountPosted;
	/** The amount of data to post per tick */
	static const INT PostChunkSize = 4096;
	/** Whether to read the data and dump to the log or just treat as an error */
	UBOOL bWas500Error;
#if !FINAL_RELEASE
	/** The file name to write the contents of the HTTP result to when debugging the gets/posts */
	FString DebugFileName;
#endif

	/**
	 * Handle resolving an address
	 */
	void StateResolving(void);

	/**
	 * Handle when the address was resolved
	 */
	void StateResolved(void);

	/**
	 * Handle connecting to a server
	 */
	void StateConnecting(void);

	/**
	 * Handle receiving the header after connecting
	 */
	void StateReceivingHeader(void);

	/**
	 * Handle parsing the header after having read the them
	 */
	void StateParsingHeader(void);

	/**
	 * Handle receiving data
	 */
	void StateReceivingData(void);

	/**
	 * Handle the session being closed
	 */
	void StateClosed(void);

	/**
	 * Handle the session having an error
	 */
	void StateError(void);

	/**
	 * POSTs the payload data to the server and transitions to receiving the header when done
	 */
	void StatePostPayload(void);

	/**
	 * Formats a http request and sends it to the web server for processing
	 */
	void SendHttpRequest(void);

	/**
	 * Resolves the IP address for the specified host and changes the internal state
	 */
	void ResolveHostIp(void);

	/**
	 * Sets the port based off of the URL or overrides
	 */
	void ResolveHostPort(void);

	/**
	 * Cleans up any outstanding resources
	 */
	void Cleanup(void);

#if !FINAL_RELEASE
protected:
	/**
	 * Save off the files for verification
	 *
	 * @param FileName the name of the file to dump to disk
	 * @param FileData the contents of the file to write out
	 */
	void DebugWriteHttpData(const FString& FileName,const TArray<BYTE>& FileData)
	{
		// Make sure the directory exists
		FString PathName(appGameDir() + TEXT("Logs") PATH_SEPARATOR + TEXT("HTTP"));
		GFileManager->MakeDirectory(*PathName);
		// Now append the file name to the path and write it out
		FString FullPathName = PathName + PATH_SEPARATOR + FileName;
		appSaveArrayToFile(FileData,*FullPathName);
	}

	/** Logs the data to a file */
	void DumpHttpResults(void)
	{
		FString Ignored;
		// See if we want to dump the http data to file or not
		if (Parse(appCmdLine(),TEXT("DUMPHTTP"),Ignored))
		{
			FString SysTimeStr(appSystemTimeString());
			INT MaxFileNameLen = Max<INT>(0,MAX_FILEPATH_LENGTH - SysTimeStr.Len() - 3);
			DebugWriteHttpData(DebugFileName.Left(MaxFileNameLen) + appSystemTimeString() + TEXT(".rx"),GetHttpResults());
			DebugWriteHttpData(DebugFileName.Left(MaxFileNameLen) + appSystemTimeString() + TEXT(".tx"),GetHttpPayload());
		}
	}

	/** Returns the buffer holding the results */
	virtual TArray<BYTE>& GetHttpResults(void)
	{
		return ReadBuffer;
	}

	/** Returns the buffer holding the payload */
	virtual TArray<BYTE>& GetHttpPayload(void)
	{
		return Payload;
	}
#endif

public:
	/**
	 * Sets the vars that control connections for this object
	 *
	 * @param InConnectionTimeout how long to wait for a connection
	 * @param InMaxRedirects the maximum number of redirects to follow
	 * @param InPackageDownload wrapper for downloading a package file (may be NULL)
	 * @param InRequestType the type of HTTP request to issue
	 */
	FHttpDownload(FLOAT InConnectionTimeout, INT InMaxRedirects, UDownload* InPackageDownload,EHttpRequestType InRequestType = HRT_Get);

	/**
	 * Sets the vars that control connections for this object
	 *
	 * @param InConnectionTimeout how long to wait for a connection
	 * @param InParameters additional URL Parameters to pass to the web server
	 * @param InResolveInfo the resolve info object to use for resolving the host (useful for platform specific resolves)
	 * @param InRequestType the type of HTTP request to issue
	 */
	FHttpDownload(FLOAT InConnectionTimeout,const FString& InParameters,FResolveInfo* InResolveInfo,EHttpRequestType InRequestType = HRT_Get);

	/**
	 * Cleans up any outstanding resources
	 */
	~FHttpDownload(void)
	{
		Cleanup();
	}

	/**
	 * Starts a download of a file specified by URL. The file is written to the
	 * specified archive as data is received
	 *
	 * @param URL the path to the file to download
	 * @param Ar the archive to write the received data to
	 * @param HttpPort used to specify a port that overrides what is in the URL
	 */
	void DownloadUrl(FURL& Url,FArchive* Ar,INT HttpPort = 0);

	/**
	 * Allows the object to service its state machine and/or network connection
	 *
	 * @param DeltaTime the amount of time that has passed since the last tick
	 */
	void Tick(FLOAT DeltaTime);

	/** @return Gets the current HTTP state of this object */
	inline EHTTPState GetHttpState(void)
	{
		return HttpState;
	}

	/**
	 * Makes a copy of the data that was passed in
	 *
	 * @param Data the buffer to read from
	 * @param DataSize the amount to copy
	 */
	void CopyPayload(const BYTE* Data,DWORD DataSize)
	{
		if (Data && DataSize)
		{
			Payload.Add(DataSize);
			appMemcpy(Payload.GetTypedData(),Data,DataSize);
		}
	}
};

/** Downloads the data to a FBufferArchive. Should only be used by child classes */
class FHttpDownloadToBuffer :
	public FHttpDownload
{
	/** The buffer data is streamed into before being converted to a string */
	FBufferArchive TempBuffer;

	/** Hidden on purpose */
	FHttpDownloadToBuffer(void);

protected:
	/**
	 * Sets the vars that control connections for this object
	 *
	 * @param InConnectionTimeout how long to wait for a connection
	 * @param InParameters additional URL Parameters to pass to the web server
	 * @param InResolveInfo the resolve info object to use for resolving the host (useful for platform specific resolves)
	 * @param InRequestType the type of HTTP request to issue
	 */
	FHttpDownloadToBuffer(FLOAT InConnectionTimeout,const FString& InParameters,FResolveInfo* InResolveInfo,EHttpRequestType InRequestType = HRT_Get) :
		FHttpDownload(InConnectionTimeout,InParameters,InResolveInfo,InRequestType)
	{
	}

	/** @return the buffer the data is in */
	inline TArray<BYTE>& GetBuffer(void)
	{
		return TempBuffer;
	}

#if !FINAL_RELEASE
protected:
	/** Returns the buffer holding the results */
	virtual TArray<BYTE>& GetHttpResults(void)
	{
		return GetBuffer();
	}
#endif

public:
	/**
	 * Starts a download of a file specified by URL. The data is stored in the buffer archive
	 *
	 * @param URL the path to the file to download
	 */
	void DownloadUrl(FURL& Url)
	{
		FHttpDownload::DownloadUrl(Url,&TempBuffer);
	}
};

/**
 * This class expects that the data returned by web server is string data
 */
class FHttpDownloadString :
	public FHttpDownloadToBuffer
{
	/** Whether the returned string is unicode or not */
	UBOOL bIsUnicode;
	/** Whether the string has been null terminated yet */
	UBOOL bIsStringTerminated;

public:
	/**
	 * Sets the vars that control connections for this object
	 *
	 * @param InbIsUnicode whether the string is Unicode or not
	 * @param InConnectionTimeout how long to wait for a connection
	 * @param InParameters additional URL Parameters to pass to the web server
	 * @param InResolveInfo the resolve info object to use for resolving the host (useful for platform specific resolves)
	 * @param InRequestType the type of HTTP request to issue
	 */
	FHttpDownloadString(UBOOL InbIsUnicode,FLOAT InConnectionTimeout,const FString& InParameters,FResolveInfo* InResolveInfo,EHttpRequestType InRequestType = HRT_Get) :
		FHttpDownloadToBuffer(InConnectionTimeout,InParameters,InResolveInfo,InRequestType),
		bIsUnicode(InbIsUnicode),
		bIsStringTerminated(FALSE)
	{
	}

	/**
	 * Copies the downloaded data to the string that was specified
	 *
	 * @param OutString the string to fill with the data
	 */
	void GetString(FString& OutString)
	{
		if (bIsStringTerminated == FALSE)
		{
			bIsStringTerminated = TRUE;
			// Append one or two zeros depending on the Unicode flag
			GetBuffer().AddItem(0);
			if (bIsUnicode)
			{
				GetBuffer().AddItem(0);
			}
		}
		// Copy the data based upon the type
		if (bIsUnicode)
		{
			OutString = (const TCHAR*)GetBuffer().GetData();
		}
		else
		{
			OutString = (const ANSICHAR*)GetBuffer().GetData();
		}
	}
};

/**
 * This class treats the data returned by web server is a blob
 */
class FHttpDownloadBinary :
	public FHttpDownloadToBuffer
{
public:
	/**
	 * Sets the vars that control connections for this object
	 *
	 * @param InConnectionTimeout how long to wait for a connection
	 * @param InParameters additional URL Parameters to pass to the web server
	 * @param InResolveInfo the resolve info object to use for resolving the host (useful for platform specific resolves)
	 * @param InRequestType the type of HTTP request to issue
	 */
	FHttpDownloadBinary(FLOAT InConnectionTimeout,const FString& InParameters,FResolveInfo* InResolveInfo,EHttpRequestType InRequestType = HRT_Get) :
		FHttpDownloadToBuffer(InConnectionTimeout,InParameters,InResolveInfo,InRequestType)
	{
	}

	/**
	 * Copies the data into the specified array
	 *
	 * @param Data the buffer to copy into
	 */
	void GetBinaryData(TArray<BYTE>& Data)
	{
		Data = GetBuffer();
	}
};

/**
 * Plug-in for the download manage to use when downloading via HTTP
 */
class UHTTPDownload : public UDownload
{
	DECLARE_CLASS_INTRINSIC(UHTTPDownload,UDownload,CLASS_Transient|CLASS_Config|0,IpDrv);

#if !XBOX
	// Config.
	FStringNoInit	ProxyServerHost;
	INT				ProxyServerPort;
	INT				MaxRedirection;
	FLOAT			ConnectionTimeout;

	// Variables.
	DOUBLE			LastTickTime;
	FURL			DownloadURL;
	/** Object responsible for downloading files via HTTP */
	FHttpDownload*	Downloader;
	/** Holds the temporary data that is read each tick */
	FBufferArchive	MemoryBuffer;

	// Constructors.
	void StaticConstructor();
	UHTTPDownload();

	// UObject interface.
	void FinishDestroy();
	void Serialize( FArchive& Ar );

	// UDownload Interface.
	virtual void ReceiveFile( UNetConnection* InConnection, INT PackageIndex, const TCHAR *Params, UBOOL InCompression );
	UBOOL TrySkipFile();

	/**
	 * Tick the connection
	 */
	void Tick(void);

#endif
};

#endif	//#if WITH_UE3_NETWORKING

#endif
