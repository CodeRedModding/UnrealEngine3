/*=============================================================================
	HTTPDownload.cpp: HTTP downloading support functions
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnIpDrv.h"
#include "HTTPDownload.h"

#if WITH_UE3_NETWORKING

static FString FormatFilename( const TCHAR* InFilename )
{
	FString Result;
	for( INT i=0;InFilename[i];i++ )
	{
		if( (InFilename[i]>='a'&&InFilename[i]<='z') || 
			(InFilename[i]>='A'&&InFilename[i]<='Z') || 
			(InFilename[i]>='0'&&InFilename[i]<='9') || 
			InFilename[i]=='/' || InFilename[i]=='?' || 
			InFilename[i]=='&' || InFilename[i]=='.' || 
			InFilename[i]=='=' || InFilename[i]=='-'
		  )
			Result = Result + FString::Printf(TEXT("%c"),InFilename[i]);
		else
			Result = Result + FString::Printf(TEXT("%%%02x"),InFilename[i]);
	}
	return Result;
}

#define DEBUG_HTTP_DOWNLOAD 0

#if DEBUG_HTTP_DOWNLOAD
/**
 * Logs the response headers from the Web server
 */
void DumpHeaders(TArray<FString>& Headers)
{
	debugf(NAME_DevHTTP,TEXT("Dumping headers:"));
	for (INT HeaderIndex = 0; HeaderIndex < Headers.Num(); HeaderIndex++)
	{
		debugf(NAME_DevHTTP,*Headers(HeaderIndex));
	}
}
#endif

/**
 * Sets the vars that control connections for this object
 *
 * @param InConnectionTimeout how long to wait for a connection
 * @param InMaxRedirects the maximum number of redirects to follow
 * @param InPackageDownload wrapper for downloading a package file (may be NULL)
 * @param InRequestType the type of HTTP request to issue
 */
FHttpDownload::FHttpDownload(FLOAT InConnectionTimeout, INT InMaxRedirects, UDownload* InPackageDownload,EHttpRequestType InRequestType) :
	ConnectionTimeout(InConnectionTimeout),
	MaxRedirects(InMaxRedirects),
	HttpState(HTTP_Initialized),
	ServerSocket(NULL),
	ResolveInfo(NULL),
	Archive(NULL),
	SizeRemaining(0),
	PackageDownload(InPackageDownload),
	RequestType(InRequestType),
	AmountPosted(0),
	bWas500Error(FALSE)
{
}

/**
 * Sets the vars that control connections for this object
 *
 * @param InConnectionTimeout how long to wait for a connection
 * @param InParameters additional URL Parameters to pass to the web server
 * @param InResolveInfo the resolve info object to use for resolving the host (useful for platform specific resolves)
 * @param InRequestType the type of HTTP request to issue
 */
FHttpDownload::FHttpDownload(FLOAT InConnectionTimeout,const FString& InParameters,FResolveInfo* InResolveInfo,EHttpRequestType InRequestType) :
	ConnectionTimeout(InConnectionTimeout),
	MaxRedirects(0),
	HttpState(HTTP_Initialized),
	ServerSocket(NULL),
	ResolveInfo(InResolveInfo),
	Archive(NULL),
	SizeRemaining(0),
	PackageDownload(NULL),
	Parameters(InParameters),
	RequestType(InRequestType),
	AmountPosted(0),
	bWas500Error(FALSE)
{
#if !FINAL_RELEASE
	TArray<FString> ParamsSplit;
	// Split the parameters into a list of strings
	Parameters.ParseIntoArray(&ParamsSplit,TEXT("&"),TRUE);
	// Scan the parameters for a filename
	for (INT Index = 0; Index < ParamsSplit.Num(); Index++)
	{
		INT FileIndex = ParamsSplit(Index).InStr(TEXT("Filename"));
		if (FileIndex != INDEX_NONE)
		{
			FileIndex = ParamsSplit(Index).InStr(TEXT("="));
			if (FileIndex != INDEX_NONE)
			{
				// Grab everything to the right of the =
				DebugFileName = ParamsSplit(Index).Mid(FileIndex + 1);
				DebugFileName = DebugFileName.Replace(TEXT("."),TEXT("_"));
				DebugFileName += TEXT("_");
				break;
			}
		}
	}
#endif
}

/**
 * Cleans up any outstanding resources
 */
void FHttpDownload::Cleanup(void)
{
	if (ServerSocket)
	{
		GSocketSubsystem->DestroySocket(ServerSocket);
		ServerSocket = NULL;
	}
	if (ResolveInfo)
	{
		// Don't delete it while waiting
		while (ResolveInfo->IsComplete() == FALSE)
		{
			// Make sure the worker lets go before deleting
			appSleep(0.f);
		}
		delete ResolveInfo;
		ResolveInfo = NULL;
	}
	Archive = NULL;
}

/**
 * Starts a download of a file specified by URL. The file is written to the
 * specified archive as data is received
 *
 * @param URL the path to the file to download
 * @param Ar the archive to write the received data to
 * @param HttpPort used to specify a port that overrides what is in the URL
 */
void FHttpDownload::DownloadUrl(FURL& Url,FArchive* Ar,INT HttpPort)
{
	RemainingRedirects = MaxRedirects;
	OverrideHttpPort = HttpPort;
	RequestUrl = Url;
#if !FINAL_RELEASE
	// If no debug file exists, use the URL name
	if (DebugFileName.Len() == 0)
	{
		DebugFileName = RequestUrl.Map;
	}
#endif
	Archive = Ar;
	if (Archive)
	{
		ConnectionStartTime = appSeconds();
		ResolveHostIp();
	}
	else
	{
		debugf(NAME_DevHTTP,TEXT("FHttpDownload::DownloadUrl: Can't write to a NULL archive"));
		HttpState = HTTP_Error;
	}
}

/**
 * Allows the object to service its state machine and/or network connection
 *
 * @param DeltaTime the amount of time that has passed since the last tick
 */
void FHttpDownload::Tick(FLOAT DeltaTime)
{
	// Tick the state machine
	switch (HttpState)
	{
		case HTTP_Resolving:
			StateResolving();
			break;

		case HTTP_Resolved:
			StateResolved();
			break;

		case HTTP_Connecting:
			StateConnecting();
			break;

		case HTTP_ReceivingHeader:
			StateReceivingHeader();
			break;

		case HTTP_ParsingHeader:
			StateParsingHeader();
			break;

		case HTTP_ReceivingData:
			StateReceivingData();
			break;

		case HTTP_Closed:
			// Ignore here, it will be caught below
			break;

		case HTTP_Error:
			StateError();
			break;

		case HTTP_PostPayload:
			StatePostPayload();
			break;

		default:
			check(0 && "Unknown state specified");
			break;
	}
	// Since the tasks are often not ticked once they become closed, make sure to let it finish
	if (HttpState == HTTP_Closed)
	{
		// The state changed to closed so tick that on the way out
		StateClosed();
	}
	// Check the session for a timeout
	if (ConnectionTimeout > 0.f &&
		appSeconds() - ConnectionStartTime > ConnectionTimeout &&
		HttpState != HTTP_Closed)
	{
		debugf(NAME_DevHTTP,TEXT("FHttpDownload::Tick: Timeout processing request"));
		HttpState = HTTP_Error;
	}
}

/**
 * Handle resolving an address
 */
void FHttpDownload::StateResolving(void)
{
	check(ResolveInfo);
	// If destination address isn't resolved yet, send nowhere.
	if (!ResolveInfo->IsComplete())
	{
		// Host name still resolving.
		return;
	}
	else if (ResolveInfo->GetErrorCode() != SE_NO_ERROR)
	{
		// Host name resolution just now failed.
		debugf(NAME_DevHTTP,TEXT("Failed to resolve hostname for HTTP download"));
		HttpState = HTTP_Error;
		delete ResolveInfo;
		ResolveInfo = NULL;
	}
	else
	{
		// Host name resolution just now succeeded so set the destination address
		ServerAddr = ResolveInfo->GetResolvedAddress();
		delete ResolveInfo;
		ResolveInfo = NULL;
		ResolveHostPort();
		debugf(NAME_DevHTTP,TEXT("FHttpDownload resolve complete to: %s"),*ServerAddr.ToString(TRUE));
		HttpState = HTTP_Resolved;
	}
}

/**
 * Sets the port based off of the URL or overrides
 */
void FHttpDownload::ResolveHostPort(void)
{
	// Set port based off URL and then the override port
	if (RequestUrl.Port == FURL::DefaultPort)
	{
		// Set the default http port
		ServerAddr.SetPort(80);
	}
	else
	{
		ServerAddr.SetPort(RequestUrl.Port);
	}
	// Now check the override port
	if (OverrideHttpPort != 0)
	{
		ServerAddr.SetPort(OverrideHttpPort);
	}
}

/**
 * Handle when the address was resolved
 */
void FHttpDownload::StateResolved(void)
{
	// Connect a stream socket to the server
	ServerSocket = GSocketSubsystem->CreateStreamSocket(TEXT("HTTP download"));
	if (ServerSocket != NULL)
	{
		// Set non blocking before attempting to connect
		ServerSocket->SetReuseAddr(TRUE);
		ServerSocket->SetNonBlocking(TRUE);
		// Do an async connect
		if (ServerSocket->Connect(ServerAddr))
		{
			ConnectionStartTime = appSeconds();
			HttpState = HTTP_Connecting;
		}
		else
		{
			debugf(NAME_Error,
				TEXT("FHttpDownload::StateResolved(): Connect() failed [%s]"),
				GSocketSubsystem->GetSocketError());
			HttpState = HTTP_Error;
		}
	}
	else
	{
		HttpState = HTTP_Error;
	}
}

/**
 * Resolves the IP address for the specified host and changes the internal state
 */
void FHttpDownload::ResolveHostIp(void)
{
	UBOOL bIsValidIp = FALSE;
	// See if the IP needs a DNS look up or not
	ServerAddr.SetIp(*RequestUrl.Host,bIsValidIp);
	// If the IP address was valid we don't need to resolve
	if (bIsValidIp)
	{
		ResolveHostPort();
		HttpState = HTTP_Resolved;
		// Resolve info is not needed here
		delete ResolveInfo;
		ResolveInfo = NULL;
	}
	else
	{
		// Otherwise kick off a DNS lookup (async)
		if (ResolveInfo == NULL)
		{
			ResolveInfo = GSocketSubsystem->GetHostByName(TCHAR_TO_ANSI(*RequestUrl.Host));
		}
		debugf(NAME_DevHTTP,TEXT("Performing DNS lookup for %s"),*RequestUrl.Host);
		HttpState = HTTP_Resolving;
	}
}

/**
 * Handle connecting to a server
 */
void FHttpDownload::StateConnecting(void)
{
	// Determine the state of the connecting process
	switch (ServerSocket->GetConnectionState())
	{
		case SCS_Connected:
		{
			// Request the URL from the server
			SendHttpRequest();
			break;
		}
		case SCS_NotConnected:
		{
			// See if the time has expired
			if (appSeconds() - ConnectionStartTime > 30)
			{
				debugf(NAME_Error,TEXT("FHttpDownload::StateConnecting() timed out") );
				HttpState = HTTP_Error;
			}
			break;
		}
		case SCS_ConnectionError:
		default:
		{
			debugf(NAME_Error,
				TEXT("FHttpDownload::StateConnecting() connection failed or refused with error (%s) (%s)"),GSocketSubsystem->GetSocketError(), *RequestUrl.String() );
			HttpState = HTTP_Error;
			break;
		}
	}
}

/**
 * Formats a http request and sends it to the web server for processing
 */
void FHttpDownload::SendHttpRequest(void)
{
	FString FileString = FormatFilename(*(FString(TEXT("/")) + RequestUrl.Map));
	FString Request;
	// Append the URL Parameters if passed in
	if (Parameters.Len())
	{
		FileString += TEXT("?");
		FileString += Parameters;
	}
	debugf(NAME_DevHTTP,TEXT("FHttpDownload request (%s) started"),*FileString);
	if (RequestType == HRT_Get)
	{
		// Build a standard GET request (note the agent is UE3-GameName)
		Request = FString::Printf(HTTP_GET_COMMAND,*FileString,*RequestUrl.Host,appGetGameName());
	}
	else
	{
		// Build a standard POST request with the params in the body
		Request = FString::Printf(HTTP_POST_COMMAND,*FileString,*RequestUrl.Host,appGetGameName(),Payload.Num());
	}
#if DEBUG_HTTP_DOWNLOAD
	debugf(NAME_DevHTTP,
		TEXT("Sending HTTP %s command:\r\n%s"),
		RequestType == HRT_Get ? TEXT("GET") : TEXT("POST"),
		*Request);
#endif
	INT Ignored;
	if (ServerSocket->Send((BYTE*)TCHAR_TO_ANSI(*Request),Request.Len(),Ignored))
	{
		// A get will respond with headers, while post requires the payload first
		if (RequestType == HRT_Get)
		{
			// Now we're waiting to get all the headers
			HttpState = HTTP_ReceivingHeader;
		}
		else
		{
			// Send the payload and then transition to reading headers
			HttpState = HTTP_PostPayload;
		}
	}
	else
	{
		debugf(NAME_DevHTTP,TEXT("FHttpDownload::SendHttpRequest() failed"));
		HttpState = HTTP_Error;
	}
}

/**
 * Handle receiving the header after connecting
 */
void FHttpDownload::StateReceivingHeader(void)
{
	ESocketConnectionState ConnectionState = ServerSocket->GetConnectionState();
	if (ConnectionState == SCS_Connected)
	{
		BYTE ReadData;
		INT ReadCount;
		INT CrLfCount = 0;
		// Presize the array the first time
		if (HeaderBuffer.Len() == 0)
		{
			HeaderBuffer.Empty(HTTP_BUFFER_SIZE);
		}
		// While we haven't found the end of the headers
		while (HttpState == HTTP_ReceivingHeader)
		{
			// Read looking for the close of the header (CRLFCRLF)
			if (ServerSocket->Recv(&ReadData,1,ReadCount))
			{
				HeaderBuffer.AppendChar(ReadData);
				// See if the header ending is present
				if (HeaderBuffer.Len() >= 4 &&
					HeaderBuffer[HeaderBuffer.Len() - 4] == TEXT('\r') &&
					HeaderBuffer[HeaderBuffer.Len() - 3] == TEXT('\n') &&
					HeaderBuffer[HeaderBuffer.Len() - 2] == TEXT('\r') &&
					HeaderBuffer[HeaderBuffer.Len() - 1] == TEXT('\n'))
				{
					// Found the end of the header
					HeaderBuffer.ParseIntoArray(&Headers,TEXT("\r\n"),TRUE);
					HttpState = HTTP_ParsingHeader;
				}
			}
			else
			{
				// Handle an error
				if (GSocketSubsystem->GetLastErrorCode() != SE_EWOULDBLOCK)
				{
					debugf(NAME_DevHTTP,
						TEXT("FHttpDownload::StateReceivingHeader: Socket error (%s)"),
						GSocketSubsystem->GetSocketError());
					HttpState = HTTP_Error;
				}
				else
				{
					// Just leave and we'll try again next tick
					return;
				}
			}
		}
	}
	else if (ConnectionState == SCS_ConnectionError)
	{
		debugf(NAME_Error,TEXT("FHttpDownload::StateReceivingHeader: Connection to HTTP server failed"));
		HttpState = HTTP_Error;
	}
}

/**
 * Handle parsing the header after having read the them
 */
void FHttpDownload::StateParsingHeader(void)
{
#if DEBUG_HEADERS
	DumpHeaders(Headers);
	//warnf( TEXT("RequestUrl: %s"), RequestUrl-)
#endif
	// Get the number code from the header
	FString Code = Headers(0).Mid(Headers(0).InStr(TEXT(" ")),5);
#if DEBUG_HTTP_DOWNLOAD
	// Was it successful? or was there a server error that we want to look at?
	if (Code == TEXT(" 200 ") ||
		Code == TEXT(" 500 "))
#else
	// Was it successful?
	if (Code == TEXT(" 200 "))
#endif
	{
#if DEBUG_HTTP_DOWNLOAD
		// Whether we should dump to the log after reading
		if (Code == TEXT(" 500 "))
		{
			bWas500Error = TRUE;
		}
#endif
		UBOOL bWasFound = FALSE;
		// Search for the file size
		for (INT Index = 0; Index < Headers.Num(); Index++)
		{
			if (Headers(Index).Left(16) == TEXT("CONTENT-LENGTH: "))
			{
				bWasFound = TRUE;
				// Parse the download size
				SizeRemaining = appAtoi(*Headers(Index).Mid(16));
				if (PackageDownload != NULL)
				{
					PackageDownload->FileSize = SizeRemaining;
				}
				// Set the next step to do
				HttpState = SizeRemaining > 0 ? HTTP_ReceivingData : HTTP_Closed;
				break;
			}
		}
		// See if we found the header we needed
		if (bWasFound == FALSE)
		{
			debugf(NAME_DevHTTP,TEXT("FHttpDownload::StateParsingHeader: Failed to find file size"));
			HttpState = HTTP_Error;
		}
	}
	// Received a redirect header, want "Location:" header and restart
	else if ((Code == TEXT(" 301 ") || Code == TEXT(" 302 "))
		&& RemainingRedirects)
	{
		// Decrement how many more hops we'll follow
		RemainingRedirects--;
		UBOOL bWasFound = FALSE;
		// Search for the location tag
		for (INT Index = 0; Index < Headers.Num(); Index++)
		{
			if (Headers(Index).Left(10) == TEXT("LOCATION: "))
			{
				FURL Base(&RequestUrl,TEXT(""),TRAVEL_Relative);
				//@todo joeg -- Should we hard code this? Seems wrong, but since we've always done it...
				Base.Port = 80;
				// Generate a new URL for the redirection
				RequestUrl = FURL(&Base,*(Headers(Index).Mid(10)),TRAVEL_Relative);
				debugf(TEXT("Download redirection to: %s"),*RequestUrl.String());
				// Clean up the socket connection
				delete ServerSocket;
				ServerSocket = NULL;
				// These will be repopulated, so dump them
				Headers.Empty();
				// Now do a resolve of the ip address, which starts the connection over
				ResolveHostIp();
				break;
			}
		}
		// See if we found the header we needed
		if (bWasFound == FALSE)
		{
			debugf(NAME_DevHTTP,TEXT("FHttpDownload::StateParsingHeader: Failed to find file redirect path"));
			HttpState = HTTP_Error;
		}
	}
	// Handle not found
	else if (Code == TEXT(" 404 "))
	{
		debugf(NAME_Error,TEXT("404: Failed to GET the requested file: %s"), *RequestUrl.String() );
		HttpState = HTTP_Error;
	}
	// Handle the HTTP POST continue response as a success
	else if (Code == TEXT(" 100 "))
	{
		HttpState = HTTP_ReceivingHeader;
		HeaderBuffer.Empty();
	}
	// All other status codes indicate failure
	else
	{
		debugf(NAME_DevHTTP,TEXT("FHttpDownload::StateParsingHeader: Got unknown status code %s URL: %s"),*Code, *RequestUrl.String());
		HttpState = HTTP_Error;
	}
}

/**
 * Handle receiving data
 */
void FHttpDownload::StateReceivingData(void)
{
	ESocketConnectionState ConnectionState = ServerSocket->GetConnectionState();
	if (ConnectionState == SCS_Connected)
	{
		BYTE ReadData[HTTP_BUFFER_SIZE];
		INT ReadCount;
		// Now check for data on the connection
		while (ServerSocket->Recv(ReadData,HTTP_BUFFER_SIZE,ReadCount))
		{
			if (ReadCount > 0)
			{
				// Don't read more data than the server said the file was
				INT Count = Clamp(ReadCount,0,SizeRemaining);
				if (Count > 0)
				{
					checkSlow(Archive);
					Archive->Serialize(ReadData,Count);
					SizeRemaining -= Count;
#if DEBUG_HTTP_DOWNLOAD
					// Whether we should dump to the log after reading
					if (bWas500Error)
					{
						ANSICHAR String[HTTP_BUFFER_SIZE + 1];
						appMemcpy(String,ReadData,ReadCount);
						String[ReadCount + 1] = '\0';
						debugf(NAME_DevHTTP,ANSI_TO_TCHAR(String));
					}
#endif
				}
				// Are we done?
				if (SizeRemaining <= 0)
				{
					HttpState = HTTP_Closed;
					break;
				}
			}
			else
			{
				// Handle an error
				if (GSocketSubsystem->GetLastErrorCode() != SE_EWOULDBLOCK)
				{
					debugf(NAME_DevHTTP,
						TEXT("FHttpDownload::StateReceivingData: Socket error (%s)"),
						GSocketSubsystem->GetSocketError());
					HttpState = HTTP_Error;
				}
				break;
			}
		}
	}
	else if (ConnectionState == SCS_ConnectionError)
	{
		debugf(NAME_DevHTTP,TEXT("FHttpDownload::StateReceivingData: Connection to HTTP server failed"));
		HttpState = HTTP_Error;
	}
}

/**
 * Handle the session being closed
 */
void FHttpDownload::StateClosed(void)
{
	debugf(NAME_DevHTTP,
		TEXT("FHttpDownload request (%s?%s) completed"),
		*FormatFilename(*(FString(TEXT("/")) + RequestUrl.Map)),
		*Parameters);
#if !FINAL_RELEASE
	// Write the file if requested
	DumpHttpResults();
#endif
	Cleanup();
}

/**
 * Handle the session having an error
 */
void FHttpDownload::StateError(void)
{
	Cleanup();
}

/**
 * Continues to send the POST data until complete and then transitions to receiving the response
 */
void FHttpDownload::StatePostPayload(void)
{
	// Clamp to our block send size
	INT SendSize = Min(Payload.Num() - AmountPosted,PostChunkSize);
	if (SendSize > 0)
	{
#if DEBUG_HTTP_DOWNLOAD
		debugf(NAME_DevHTTP,TEXT("Continuing POST with %d bytes"),SendSize);
#endif
		INT BytesSent = 0;
		// Send the next block of data for this tick
		if (ServerSocket->Send(&Payload(AmountPosted),SendSize,BytesSent))
		{
			AmountPosted += BytesSent;
		}
		else
		{
			INT SocketError = GSocketSubsystem->GetLastErrorCode();
			// Skip these two errors since it just means that the buffer is full
			if (SocketError != SE_EWOULDBLOCK && SocketError != SE_ENOBUFS)
			{
				debugf(NAME_DevHTTP,TEXT("FHttpDownload::StatePostPayload() failed"));
				HttpState = HTTP_Error;
			}
		}
	}
	else
	{
		debugf(NAME_DevHTTP,TEXT("HTTP POST payload sent, waiting for response"));
		HttpState = HTTP_ReceivingHeader;
	}
}

#if !XBOX

/*-----------------------------------------------------------------------------
	UHTTPDownload implementation.
-----------------------------------------------------------------------------*/
UHTTPDownload::UHTTPDownload()
{
	// Init.
	bDownloadSendsFileSizeInData = FALSE;
	Downloader = NULL;
	if (ConnectionTimeout == 0.f)
	{
		ConnectionTimeout = 30.f;
	}
}

void UHTTPDownload::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << DownloadURL;
}

void UHTTPDownload::StaticConstructor()
{
	// Config.
	new(GetClass(),TEXT("ProxyServerHost"),		RF_Public)UStrProperty(CPP_PROPERTY(ProxyServerHost		), TEXT("Settings"), CPF_Config );
	new(GetClass(),TEXT("ProxyServerPort"),		RF_Public)UIntProperty(CPP_PROPERTY(ProxyServerPort		), TEXT("Settings"), CPF_Config );
	new(GetClass(),TEXT("RedirectToURL"),		RF_Public)UStrProperty(CPP_PROPERTY(DownloadParams		), TEXT("Settings"), CPF_Config );
	new(GetClass(),TEXT("UseCompression"),		RF_Public)UBoolProperty(CPP_PROPERTY(UseCompression		), TEXT("Settings"), CPF_Config );
	new(GetClass(),TEXT("MaxRedirection"),		RF_Public)UBoolProperty(CPP_PROPERTY(MaxRedirection		), TEXT("Settings"), CPF_Config );
	new(GetClass(),TEXT("ConnectionTimeout"),	RF_Public)UFloatProperty(CPP_PROPERTY(ConnectionTimeout	), TEXT("Settings"), CPF_Config );
}

void UHTTPDownload::ReceiveFile( UNetConnection* InConnection, INT InPackageIndex, const TCHAR *Params, UBOOL InCompression )
{
	UDownload::ReceiveFile( InConnection, InPackageIndex, Params, InCompression);

	// we need a params string
	if( !*Params )
	{
		return;
	}

	// remember if it is compressed
	IsCompressed = InCompression;

	// HTTP 301/302 support
	if (MaxRedirection < 1)
	{
		MaxRedirection = 5; // default to 5
	}

	FPackageInfo& Info = Connection->PackageMap->List( PackageIndex );
	FURL Base(NULL, TEXT(""), TRAVEL_Absolute);
	Base.Port = 80;

	FString File = Info.PackageName.ToString() + TEXT(".") + Info.Extension;

	// this adds an extension unlike normal with no extension
	if( IsCompressed )
	{
		File = File + COMPRESSED_EXTENSION;
	}

	// patch up any URL variables with their values
	FString StringURL = Params;
	StringURL = StringURL.Replace(TEXT("%guid%"), *Info.Guid.String());

	StringURL = StringURL.Replace(TEXT("%file%"), *File);
	StringURL = StringURL.Replace(TEXT("%lcfile%"), *File.ToLower());
	StringURL = StringURL.Replace(TEXT("%ucfile%"), *File.ToUpper());

	StringURL = StringURL.Replace(TEXT("%ext%"), *Info.Extension);
	StringURL = StringURL.Replace(TEXT("%lcext%"), *Info.Extension.ToLower());
	StringURL = StringURL.Replace(TEXT("%ucext%"), *Info.Extension.ToUpper());


	// if we didn't substitute anything, just take the filename on the end.
	if (StringURL == Params)
	{
		StringURL = StringURL + File;
	}

	DownloadURL = FURL(&Base, *StringURL, TRAVEL_Relative);
	if (ProxyServerHost.Len())
	{
		DownloadURL.Host = ProxyServerHost;
	}

	if (Downloader == NULL)
	{
		Downloader = new FHttpDownload(ConnectionTimeout, MaxRedirection, this);
	}
	LastTickTime = appSeconds();
	// Kick off a download
	Downloader->DownloadUrl(DownloadURL,&MemoryBuffer,ProxyServerPort == 0 ? 80 : ProxyServerPort);
}

/**
 * Tick the connection
 */
void UHTTPDownload::Tick(void)
{
	if (Downloader)
	{
		if (Downloader->GetHttpState() != HTTP_Error)
		{
			Downloader->Tick(appSeconds() - LastTickTime);
			// Keep track of time so we can pass in deltas
			LastTickTime = appSeconds();
			// Process any data that was read
			if (MemoryBuffer.Num() > 0)
			{
				// Let the base class do something with the data
				ReceiveData(MemoryBuffer.GetTypedData(),MemoryBuffer.Num());
				// Throw away the data without triggering a realloc
				MemoryBuffer.TArray<BYTE>::Reset();
				MemoryBuffer.Seek(0);
			}
			if (Downloader->GetHttpState() == HTTP_Closed)
			{
				delete Downloader;
				Downloader = NULL;
				DownloadDone();
			}
		}
		else
		{
			delete Downloader;
			Downloader = NULL;
			// Indicate that downloading failed
			DownloadError(*LocalizeError(TEXT("ConnectionFailed"),TEXT("Engine")));
			DownloadDone();
		}
	}
}

UBOOL UHTTPDownload::TrySkipFile()
{
	if( Super::TrySkipFile() )
	{
		FNetControlMessage<NMT_Skip>::Send(Connection, Info->Guid);
		return 1;
	}
	return 0;
}

void UHTTPDownload::FinishDestroy()
{
	delete Downloader;
	Downloader = NULL;

	Super::FinishDestroy();
}

#endif

IMPLEMENT_CLASS(UHTTPDownload)

#endif	//#if WITH_UE3_NETWORKING
