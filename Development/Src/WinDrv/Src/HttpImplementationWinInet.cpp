/*=============================================================================
	Http implementation using WinInet API.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "WinDrv.h"
#include "HttpImplementationWinInet.h"

#if WITH_UE3_NETWORKING
/** Translates an error returned from GetLastError returned from a WinInet API call. */
FString InternetTranslateError(DWORD GetLastErrorResult)
{
	FString ErrorStr = FString::Printf(TEXT("ErrorCode: %08X. "), GetLastErrorResult);

	HANDLE ProcessHeap = GetProcessHeap();
	if (ProcessHeap == NULL)
	{
		ErrorStr += TEXT("Call to GetProcessHeap() failed, cannot translate error... "); 
		return ErrorStr;
	}

	TCHAR FormatBuffer[1024];

	DWORD BaseLength = FormatMessageW(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE,
		GetModuleHandle(TEXT("wininet.dll")),
		GetLastErrorResult,
		0,
		FormatBuffer,
		ARRAYSIZE(FormatBuffer),
		NULL);

	if (!BaseLength)
	{
		ErrorStr += FString::Printf(TEXT("Call to FormatMessage() failed: %08X. "), GetLastError()); 
		return ErrorStr;
	}

	ErrorStr += FString::Printf(TEXT("Desc: %s. "), FormatBuffer);

	if (GetLastErrorResult == ERROR_INTERNET_EXTENDED_ERROR)
	{
		DWORD InetError;
		DWORD ExtLength = 0;

		InternetGetLastResponseInfo(&InetError, NULL, &ExtLength);
		ExtLength = ExtLength+1;
		TArray<TCHAR> ExtErrMsg(ExtLength);
		if (!InternetGetLastResponseInfo(&InetError, &ExtErrMsg(0), &ExtLength))
		{
			ErrorStr += FString::Printf(TEXT("Call to InternetGetLastResponseInfo() failed: %08X. "), GetLastError());
			return ErrorStr;
		}
	}

	return ErrorStr;
}

/** Global callback for WinInet async API calls. */
void CALLBACK InternetStatusCallbackWinInet(HINTERNET hInternet, DWORD_PTR dwContext,
	DWORD dwInternetStatus, LPVOID lpvStatusInformation,
	DWORD dwStatusInformationLength);

/** Root handle for WinInet calls. */
static HINTERNET GRootInternetHandle;

/**
 * Initializes WinInet. This is done lazily on first creation as the WinInet
 * API doesn't allow calls to be made from static global initializers.
 * 
 * @return	bool - true if startup succeeded. If false, it's not expected that any further calls to the APIs will succeed.
 */
bool InitWinInet()
{
	// Check and log the connected state so we can report early errors.
	DWORD ConnectedFlags;
	BOOL Connected = InternetGetConnectedState(&ConnectedFlags, 0);
	FString ConnectionType;
	ConnectionType += (ConnectedFlags & INTERNET_CONNECTION_CONFIGURED) ? TEXT("Configured ") : TEXT("");
	ConnectionType += (ConnectedFlags & INTERNET_CONNECTION_LAN) ? TEXT("LAN ") : TEXT("");
	ConnectionType += (ConnectedFlags & INTERNET_CONNECTION_MODEM) ? TEXT("Modem ") : TEXT("");
	ConnectionType += (ConnectedFlags & INTERNET_CONNECTION_MODEM_BUSY) ? TEXT("Modem Busy ") : TEXT("");
	ConnectionType += (ConnectedFlags & INTERNET_CONNECTION_OFFLINE) ? TEXT("Offline ") : TEXT("");
	ConnectionType += (ConnectedFlags & INTERNET_CONNECTION_PROXY) ? TEXT("Proxy Server ") : TEXT("");
	ConnectionType += (ConnectedFlags & INTERNET_RAS_INSTALLED) ? TEXT("RAS Installed ") : TEXT("");
	debugf(NAME_DevHttpRequest, TEXT("Connected State: %s. Flags: (%s)"), Connected ? TEXT("Good") : TEXT("Bad"), *ConnectionType);

	DWORD ConnectResult;
	ConnectResult = InternetAttemptConnect(0);
	if (ConnectResult != ERROR_SUCCESS)
	{
		debugf(NAME_DevHttpRequest, TEXT("InternetAttemptConnect failed: %s\n"), *InternetTranslateError(GetLastError()));
		return false;
	}

	if (!InternetCheckConnection(TEXT("http://www.google.com"), FLAG_ICC_FORCE_CONNECTION, 0))
	{
		debugf(NAME_DevHttpRequest, TEXT("InternetCheckConnection failed for www.google.com: %s"), *InternetTranslateError(GetLastError()));
		return false;
	}

	// setup net connection
	GRootInternetHandle = InternetOpen(*FString::Printf(TEXT("UE3-%s,UE3Ver(%d)"), appGetGameName(), GEngineVersion), 
		INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, INTERNET_FLAG_ASYNC);

	if (GRootInternetHandle == NULL)
	{
		debugf(NAME_DevHttpRequest, TEXT("Failed WinHttpOpen: %s"), *InternetTranslateError(GetLastError()));
		return false;
	}

	debugf(NAME_DevHttpRequest, TEXT("Opening WinINet Session"));

	// set the callback
	InternetSetStatusCallback(GRootInternetHandle, InternetStatusCallbackWinInet);
	return true;
}

/**
 * Shuts down the WinInet API. Currently this is not called and we rely
 * on Windows to clean everything up when the process closes.
 *
 * @return	bool - true if the shutdown succeeded.
 */
bool ShutdownWinInet()
{
	// shut down WinINet
	if (!InternetCloseHandle(GRootInternetHandle))
	{
		debugf(NAME_DevHttpRequest, TEXT("InternetCloseHandle failed on the FHttpRequestWinInet: %s"), *InternetTranslateError(GetLastError()));
		return false;
	}
	GRootInternetHandle = NULL;
	debugf(NAME_DevHttpRequest, TEXT("Closing WinINet Session"));
	return true;
} 

/**
 * Global callback for WinInet API. Will use the dwInternetStatus and dwContext
 * fields to route the results back to the appropriate class instance if necessary.
 *
 * @param	hInternet - See WinInet API
 * @param	dwContext - See WinInet API
 * @param	dwInternetStatus - See WinInet API
 * @param	lpvStatusInformation - See WinInet API
 * @param	dwStatusInformationLength - See WinInet API
 */
void CALLBACK InternetStatusCallbackWinInet(
	HINTERNET hInternet, DWORD_PTR dwContext,
	DWORD dwInternetStatus, LPVOID lpvStatusInformation,
	DWORD dwStatusInformationLength)
{
	FHttpResponseWinInet *ResponseObj = (FHttpResponseWinInet*)dwContext;
	const INTERNET_ASYNC_RESULT* AsyncResult = (const INTERNET_ASYNC_RESULT*)lpvStatusInformation;

	switch (dwInternetStatus)
	{
	case INTERNET_STATUS_CLOSING_CONNECTION:
		debugf(NAME_DevHttpRequest, TEXT("CLOSING_CONNECTION: %p"), dwContext);
		break;
	case INTERNET_STATUS_CONNECTED_TO_SERVER:
		debugf(NAME_DevHttpRequest, TEXT("CONNECTED_TO_SERVER: %p"), dwContext);
		break;
	case INTERNET_STATUS_CONNECTING_TO_SERVER:
		debugf(NAME_DevHttpRequest, TEXT("CONNECTING_TO_SERVER: %p"), dwContext);
		break;
	case INTERNET_STATUS_CONNECTION_CLOSED:
		debugf(NAME_DevHttpRequest, TEXT("CONNECTION_CLOSED: %p"), dwContext);
		break;
	case INTERNET_STATUS_HANDLE_CLOSING:
		debugf(NAME_DevHttpRequest, TEXT("HANDLE_CLOSING: %p"), dwContext);
		break;
	case INTERNET_STATUS_HANDLE_CREATED:
		debugf(NAME_DevHttpRequest, TEXT("HANDLE_CREATED: %p"), dwContext);
		break;
	case INTERNET_STATUS_INTERMEDIATE_RESPONSE:
		debugf(NAME_DevHttpRequest, TEXT("INTERMEDIATE_RESPONSE: %p"), dwContext);
		break;
	case INTERNET_STATUS_NAME_RESOLVED:
		debugf(NAME_DevHttpRequest, TEXT("NAME_RESOLVED: %p"), dwContext);
		break;
	case INTERNET_STATUS_RECEIVING_RESPONSE:
		debugf(NAME_DevHttpRequest, TEXT("RECEIVING_RESPONSE: %p"), dwContext);
		break;
	case INTERNET_STATUS_RESPONSE_RECEIVED:
		debugf(NAME_DevHttpRequest, TEXT("RESPONSE_RECEIVED: %p"), dwContext);
		break;
	case INTERNET_STATUS_REDIRECT:
		debugf(NAME_DevHttpRequest, TEXT("STATUS_REDIRECT: %p"), dwContext);
		break;
	case INTERNET_STATUS_REQUEST_COMPLETE:
		debugf(NAME_DevHttpRequest, TEXT("REQUEST_COMPLETE: %p"), dwContext);
		ResponseObj->ProcessResponse(AsyncResult);
		break;
	case INTERNET_STATUS_REQUEST_SENT:
		debugf(NAME_DevHttpRequest, TEXT("REQUEST_SENT: %p"), dwContext);
		break;
	case INTERNET_STATUS_RESOLVING_NAME:
		debugf(NAME_DevHttpRequest, TEXT("RESOLVING_NAME: %p"), dwContext);
		break;
	case INTERNET_STATUS_SENDING_REQUEST:
		debugf(NAME_DevHttpRequest, TEXT("SENDING_REQUEST: %p"), dwContext);
		break;
	case INTERNET_STATUS_STATE_CHANGE:
		debugf(NAME_DevHttpRequest, TEXT("STATE_CHANGE: %p"), dwContext);
		break;
	case INTERNET_STATUS_COOKIE_SENT:
		debugf(NAME_DevHttpRequest, TEXT("COOKIE_SENT: %p"), dwContext);
		break;
	case INTERNET_STATUS_COOKIE_RECEIVED:
		debugf(NAME_DevHttpRequest, TEXT("COOKIE_RECEIVED: %p"), dwContext);
		break;
	case INTERNET_STATUS_PRIVACY_IMPACTED:
		debugf(NAME_DevHttpRequest, TEXT("PRIVACY_IMPACTED: %p"), dwContext);
		break;
	case INTERNET_STATUS_P3P_HEADER:
		debugf(NAME_DevHttpRequest, TEXT("P3P_HEADER: %p"), dwContext);
		break;
	case INTERNET_STATUS_P3P_POLICYREF:
		debugf(NAME_DevHttpRequest, TEXT("P3P_POLICYREF: %p"), dwContext);
		break;
	case INTERNET_STATUS_COOKIE_HISTORY:
		{
			const InternetCookieHistory* CookieHistory = (const InternetCookieHistory*)lpvStatusInformation;
			debugf(NAME_DevHttpRequest, TEXT("COOKIE_HISTORY: %p. Accepted: %u. Leashed: %u. Downgraded: %u. Rejected: %u.")
				, dwContext
				, CookieHistory->fAccepted
				, CookieHistory->fLeashed
				, CookieHistory->fDowngraded
				, CookieHistory->fRejected);
		}
		break;
	default:
		debugf(NAME_DevHttpRequest, TEXT("Unknown Status: %u. %p"), dwInternetStatus, dwContext);
		break;
	}
}

void FURLWinInet::CrackUrlParameters() const
{
	// don't crack anything if the request is empty
	if (RequestURL.IsEmpty()) return;

	// used to make sure we can't early exit from this function without cleaning up.
	struct FClearCachedDataGuard
	{
	public: FClearCachedDataGuard(const FURLWinInet& InURL):CachedURL(InURL),bClearCachedData(TRUE) {}
	public: ~FClearCachedDataGuard() { if (bClearCachedData) CachedURL.ClearCachedData(); }
	public: bool bClearCachedData;
	private: const FURLWinInet& CachedURL;
	};
	FClearCachedDataGuard CachedDataGuard(*this);

	URLPtr = *RequestURL;
	// crack open the URL into its component parts
	URLParts.dwStructSize = sizeof(URLParts);
	URLParts.dwHostNameLength = 1;
	URLParts.dwUrlPathLength = 1;
	URLParts.dwExtraInfoLength = 1;

	if (!InternetCrackUrl(URLPtr, 0, 0, &URLParts))
	{
		debugf(NAME_DevHttpRequest, TEXT("Failed to crack URL parameters for URL:%s"), *RequestURL);
		return;
	}

	// make sure we didn't fail.
	FString Result;
	if (URLParts.dwExtraInfoLength > 1)
	{
		if (URLParts.lpszExtraInfo[0] == TEXT('?'))
		{
			const TCHAR* ParamPtr = URLParts.lpszExtraInfo+1;
			const TCHAR* ParamEnd = URLParts.lpszExtraInfo+URLParts.dwExtraInfoLength;

			while (ParamPtr < ParamEnd)
			{
				const TCHAR* DelimiterPtr = appStrchr(ParamPtr, TEXT('&'));
				if (DelimiterPtr == NULL)
				{
					DelimiterPtr = ParamEnd;
				}
				const TCHAR* EqualPtr = appStrchr(ParamPtr, TEXT('='));
				if (EqualPtr != NULL && EqualPtr < DelimiterPtr)
				{
					// This will handle the case of Key=&Key2=Value... by allocating a zero-length string with the math below
					URLParameters.Set(FString(EqualPtr-ParamPtr, ParamPtr), FString(DelimiterPtr-EqualPtr-1, EqualPtr+1));
				}
				else
				{
					URLParameters.Set(FString(DelimiterPtr-ParamPtr, ParamPtr), FString());
				}
				ParamPtr = DelimiterPtr + 1;
			}
		}
		else
		{
			debugf(NAME_DevHttpRequest, TEXT("URL '%s' extra info did not start with a '?', so can't parse headers."), *RequestURL);
		}
	}
	CachedDataGuard.bClearCachedData = FALSE;
}

FHttpResponseWinInet* FHttpRequestWinInet::MakeRequest(const FURLWinInet& URL, const FString& Verb, TArray<BYTE>& Payload)
{
	if (GRootInternetHandle == NULL)
	{
		if (!InitWinInet())
		{
			return NULL;
		}
	}

	// make sure the URL is parsed correctly with a valid HTTP scheme
	if (URL.GetURLComponents().nScheme != INTERNET_SCHEME_HTTP &&
		URL.GetURLComponents().nScheme != INTERNET_SCHEME_HTTPS)
	{
		debugf(NAME_DevHttpRequest, TEXT("URL '%s' is not a valid HTTP request. %p"), *URL.GetURL(), this);
		return NULL;
	}

	FString Headers = GenerateHeaderBuffer(Payload.Num());
	FString Host(URL.GetHost());
	FString PathAndExtra(URL.GetPath()+URL.GetExtraInfo());
	
	// open a connection to the URL
	HINTERNET SessionHandle = InternetConnect(GRootInternetHandle, *Host, URL.GetPort(), NULL, NULL, INTERNET_SERVICE_HTTP, 0, (DWORD_PTR)this);
	if (!SessionHandle)
	{
		debugf(NAME_DevHttpRequest, TEXT("InternetConnect failed: %s"), *InternetTranslateError(GetLastError()));
		return NULL;
	}

	// Not currently using any request flags
	DWORD RequestFlags = URL.GetURLComponents().nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_FLAG_SECURE : 0;

	// create the right type of request, and send it
	HINTERNET RequestHandle = 0;
	
	// create the FHttpResponseWinInet object (request handle isn't available yet)
	// Have to create the response before creating the handle because the InternetSetOption function seems to fail in 64-bit mode, only keeping the first 32-bits of the payload.
	TScopedPointer<FHttpResponseWinInet> Result(new FHttpResponseWinInet(SessionHandle, RequestHandle, URL, Payload));
	RequestHandle = HttpOpenRequest(SessionHandle, Verb.IsEmpty() ? NULL : *Verb, *PathAndExtra, NULL, NULL, NULL, RequestFlags, (DWORD_PTR)Result.GetOwnedPointer());
	if (!RequestHandle)
	{
		debugf(NAME_DevHttpRequest, TEXT("HttpOpenRequest failed: %s. %p"), *InternetTranslateError(GetLastError()), this);
		return NULL;
	}
	// Now assign the request handle to the response
	Result->RequestHandle = RequestHandle;

	// send the request with the payload
	if (!HttpSendRequest(RequestHandle, *Headers, Headers.Len(), Result->GetRequestPayload().Num() > 0 ? const_cast<BYTE*>(Result->GetRequestPayload().GetTypedData()) : NULL, Result->GetRequestPayload().Num()))
	{
		DWORD ErrorCode = GetLastError();
		if (ErrorCode != ERROR_IO_PENDING)
		{
			debugf(NAME_DevHttpRequest, TEXT("HttpSendRequest failed: %s. %p"), *InternetTranslateError(ErrorCode), this);
			return NULL;
		}
		else
		{
			debugf(NAME_DevHttpRequest, TEXT("HttpSendRequest is pending async completion. %p"), this);
		}
	}
	// We're done with our handles now, the FHttpResponseWinInet owns them.
	// These cannot be owned by the FHttpRequestWinInet because the FHttpRequestWinInet may be deleted
	// before the FHttpResponseWinInet is even ready, so the FHttpResponseWinInet must take ownership of the handles. 
	return Result.Release();
}

FHttpResponseWinInet* FHttpRequestWinInet::MakeRequest( const FURLWinInet& URL, const FString& Verb, const TArray<BYTE>& Payload )
{
	TArray<BYTE> CopiedPayload(Payload);
	return MakeRequest(URL, Verb, CopiedPayload);
}

FHttpResponseWinInet* FHttpRequestWinInet::MakeRequest(const FURLWinInet& URL, const FString& Verb, const FString& Payload)
{
	FTCHARToUTF8 Converter(*Payload);
	TArray<BYTE> UTF8Payload(Converter.Length());
	// make sure not to copy the null terminator.
	appMemcpy(UTF8Payload.GetTypedData(), (BYTE*)(ANSICHAR*)Converter, UTF8Payload.Num());
	return MakeRequest(URL, Verb, UTF8Payload);
}

FHttpResponseWinInet* FHttpRequestWinInet::MakeRequest( const FURLWinInet& URL, const FString& Verb )
{
	return MakeRequest(URL, Verb, TArray<BYTE>());
}
FHttpRequestWinInet::FHttpRequestWinInet()
{
	debugf(NAME_DevHttpRequest, TEXT("FHttpRequestWinInet object created: %p"), this);
}

FHttpRequestWinInet::~FHttpRequestWinInet()
{
	debugf(NAME_DevHttpRequest, TEXT("FHttpRequestWinInet object destroyed: %p"), this);
}

const FString* FHttpRequestWinInet::GetHeader( const FString& HeaderName ) const
{
	return RequestHeader.Find(HeaderName);
}

TArray<FString> FHttpRequestWinInet::GetHeaders() const
{
	TArray<FString> Result;
	for (TMap<FString, FString>::TConstIterator It(RequestHeader);It;++It)
	{
		Result.AddItem(It.Key() + TEXT(": ") + It.Value());
	}
	return Result;
}

FHttpRequestWinInet& FHttpRequestWinInet::SetHeader( const FString& HeaderName, const FString& HeaderValue )
{
	RequestHeader.Set(HeaderName, HeaderValue);
	return *this;
}

FString FHttpRequestWinInet::GenerateHeaderBuffer( size_t ContentLength )
{
	FString Result;
	for (TMap<FString, FString>::TConstIterator It(RequestHeader);It;++It)
	{
		Result += It.Key() + TEXT(": ") + It.Value() + TEXT("\r\n");
	}
	if (ContentLength > 0)
	{
		Result += FString(TEXT("Content-Length: ")) + appItoa(ContentLength) + TEXT("\r\n");
	}
	return Result;
}

bool FHttpResponseWinInet::IsReady() const
{
	return bIsReady;
}

bool FHttpResponseWinInet::Succeeded() const
{
	return bResponseSucceeded;
}

const TArray<BYTE>* FHttpResponseWinInet::GetPayload() const
{
	return IsReady() ? &ResponsePayload : NULL;
}

const FString FHttpResponseWinInet::GetPayloadAsString() const
{
	if (!IsReady() || GetPayload()->Num() == 0) return FString();

	TArray<BYTE> ZeroTerminatedPayload(ResponsePayload);
	ZeroTerminatedPayload.AddItem(0);
	return FString(UTF8_TO_TCHAR(ZeroTerminatedPayload.GetData()));
}

FString FHttpResponseWinInet::GetHeader( const FString& HeaderName ) const
{
	return PrivateGetHeaderString(HTTP_QUERY_CUSTOM, HeaderName);
}

FString FHttpResponseWinInet::GetContentType() const
{
	return PrivateGetHeaderString(HTTP_QUERY_CONTENT_TYPE, FString());
}

DWORD FHttpResponseWinInet::GetContentLength() const
{
	return _tcstoul(*PrivateGetHeaderString(HTTP_QUERY_CONTENT_LENGTH, FString()), NULL, 10);
}

const FURLWinInet& FHttpResponseWinInet::GetURL() const
{
	return RequestURL;
}

FHttpResponseWinInet::FHttpResponseWinInet( HINTERNET InSessionHandle, HINTERNET InRequestHandle, const FURLWinInet& InURL, TArray<BYTE>& InRequestPayload ) :SessionHandle(InSessionHandle)
	,RequestHandle(InRequestHandle)
	,bIsReady(FALSE)
	,bResponseSucceeded(FALSE)
	,RequestURL(InURL)
	,AsyncBytesRead(0)
	,TotalBytesRead(0)
{
	Exchange(RequestPayload, InRequestPayload);
}

const TArray<BYTE>& FHttpResponseWinInet::GetRequestPayload() const
{
	return RequestPayload;
}

FHttpResponseWinInet::~FHttpResponseWinInet()
{
	LPVOID NullPtr = 0;
	if (!InternetSetOption(SessionHandle, INTERNET_OPTION_CONTEXT_VALUE, &NullPtr, sizeof(NullPtr)))
	{
		debugf(NAME_DevHttpRequest, TEXT("Attempt to remove context value from FHttpRequestWinInet with InternetSetOption failed: %s. %p"), *InternetTranslateError(GetLastError()), this);
	}
	// Don't die if the handle can't be closed for any reason, try to limp along.
	if (!InternetCloseHandle(RequestHandle))
	{
		debugf(NAME_DevHttpRequest, TEXT("InternetCloseHandle failed on the FHttpRequestWinInet: %s. %p"), *InternetTranslateError(GetLastError()), this);
	}
	RequestHandle = NULL;

	// Don't die if the handle can't be closed for any reason, try to limp along.
	if (!InternetCloseHandle(SessionHandle))
	{
		debugf(NAME_DevHttpRequest, TEXT("InternetCloseHandle failed on the session: %s. %p"), *InternetTranslateError(GetLastError()), this);
	}
	SessionHandle = NULL;
	debugf(NAME_DevHttpRequest, TEXT("FHttpResponseWinInet object destroyed. %p"), this);
}

void FHttpResponseWinInet::ProcessResponse( const INTERNET_ASYNC_RESULT* AsyncResult )
{
	if (AsyncResult->dwResult)
	{
		// Clear the request payload to save memory, as we don't need it anymore.
		RequestPayload.Empty();

		TotalBytesRead += AsyncBytesRead;
		// We might be calling back into this from another asynchronous read, so continue where we left off.
		// if there is no content length, we're prolly receiving chunked data.
		INT ContentLength = (INT)GetContentLength();
		// Allocate one extra byte when the content length is known so we can check if we receive too much data.
		INT BufferSize = ContentLength > 0 && TotalBytesRead == 0 ? ContentLength+1 : TotalBytesRead + BUFFER_SIZE;
		// For non-chunked responses, allocate one extra byte to check if we are sent extra content
		// For chunked responses, add data using a fixed size buffer at a time.
		ResponsePayload.SetNum(BufferSize);
		do
		{
			// It's possible (especially in chunked responses) for this call to block asynchronously.
			// When this happens, we need to wait for our Internet callback to be called again and continue the operation.
			if (!InternetReadFile(RequestHandle, &ResponsePayload(TotalBytesRead), ResponsePayload.Num()-TotalBytesRead, (DWORD*)&AsyncBytesRead))
			{
				DWORD ErrorCode = GetLastError();
				if (ErrorCode == ERROR_IO_PENDING)
				{
					debugf(NAME_DevHttpRequest, TEXT("InternetReadFile is completing asynchronously, so waiting for our callback to be called again. %p"), this);
				}
				else
				{
					debugf(NAME_DevHttpRequest, TEXT("InternetReadFile failed (%u bytes read). Returning what we've read so far: %s. %p"), AsyncBytesRead, *InternetTranslateError(ErrorCode), this);
					bIsReady = TRUE;
				}
				return;
			}
			TotalBytesRead += AsyncBytesRead;
			// resize the buffer if we don't know our content length, otherwise don't let the buffer grow larger than content length.
			if (TotalBytesRead >= ResponsePayload.Num())
			{
				if (ContentLength > 0)
				{
					debugf(NAME_DevHttpRequest, TEXT("Warning: Response payload (%d bytes read so far) is larger than the content-length (%d). Resizing buffer to accommodate. %p"), TotalBytesRead, ContentLength, this);
				}
				ResponsePayload.Add(BUFFER_SIZE);
			}
		} while (AsyncBytesRead > 0);

		if (ContentLength != 0 && TotalBytesRead != ContentLength)
		{
			debugf(NAME_DevHttpRequest, TEXT("Warning: Response payload was %d bytes, content-length indicated (%d) bytes. %p"), TotalBytesRead, ContentLength, this);
		}

		debugf(NAME_DevHttpRequest, TEXT("TotalBytesRead = %d. %p"), TotalBytesRead, this);
		// Make sure the array only contains valid data.
		ResponsePayload.SetNum(TotalBytesRead);
		bResponseSucceeded = TRUE;
	}
	else
	{
		debugf(NAME_DevHttpRequest, TEXT("REQUEST_COMPLETE returned error: %s. %p"), *InternetTranslateError(AsyncResult->dwError), this);
	}
	bIsReady = TRUE;
}

FString FHttpResponseWinInet::PrivateGetHeaderString( DWORD HttpQueryInfoLevel, const FString& HeaderName ) const
{
	// try to use stack allocation where possible.
	DWORD HeaderSize = 0;
	TCHAR HeaderValue[128];
	TArray<TCHAR> HeaderValueLong;
	TCHAR* HeaderValueReal = HeaderValue;
	if (!HttpQueryInfo(RequestHandle, HttpQueryInfoLevel, const_cast<TCHAR*>(*HeaderName), &HeaderSize, NULL))
	{
		DWORD ErrorCode = GetLastError();
		if (ErrorCode == ERROR_HTTP_HEADER_NOT_FOUND)
		{
			return FString();
		}
		else if (ErrorCode == ERROR_INSUFFICIENT_BUFFER)
		{
			// make sure we have enough room to supply the HeaderName and Value. If not, dynamically allocate.
			DWORD HeaderSizeChars = HeaderSize / sizeof(TCHAR) + 1;
			DWORD HeaderNameChars = HeaderName.Len() == 0 ? 0 : HeaderName.Len() + 1;
			if (HeaderSizeChars > ARRAYSIZE(HeaderValue) || HeaderNameChars > ARRAYSIZE(HeaderValue))
			{
				// we have to create a dynamic allocation to hold the result.
				debugf(NAME_DevHttpRequest, TEXT("Having to resize default buffer for retrieving header %s. Name length: %u. Value length: %u. %p"), *HeaderName, HeaderNameChars, HeaderSizeChars, this);
				DWORD NewBufferSizeChars = Max(HeaderSizeChars, HeaderNameChars);
				// Have to copy the HeaderName into the buffer as well for the API to work.
				if (HeaderName.Len() > 0)
				{
					HeaderValueLong = HeaderName.GetCharArray();
				}
				// Set the size of the array to hold the entire value.
				HeaderValueLong.SetNum(NewBufferSizeChars);
				HeaderSize = NewBufferSizeChars * sizeof(TCHAR);
				HeaderValueReal = &HeaderValueLong(0);
			}
			else
			{
				// Use the stack allocated space if we have the room.
				appMemcpy(HeaderValue, *HeaderName, HeaderName.Len()*sizeof(TCHAR));
				HeaderValue[HeaderName.Len()] = 0;
			}
			if (!HttpQueryInfo(RequestHandle, HttpQueryInfoLevel, HeaderValueReal, &HeaderSize, NULL))
			{
				debugf(NAME_DevHttpRequest, TEXT("HttpQueryInfo failed trying to get Header Value for Name %s: %s. %p"), *HeaderName, *InternetTranslateError(GetLastError()), this);
				return FString();
			}
		}
	}
	return FString(HeaderValueReal);
}

TArray<FString> FHttpResponseWinInet::GetAllHeaders() const
{
	DWORD HeaderSize = 0;
	TArray<FString> Result;
	if (!HttpQueryInfo(RequestHandle, HTTP_QUERY_RAW_HEADERS_CRLF, NULL, &HeaderSize, NULL))
	{
		DWORD ErrorCode = GetLastError();
		if (ErrorCode != ERROR_INSUFFICIENT_BUFFER)
		{
			debugf(NAME_DevHttpRequest, TEXT("HttpQueryInfo to get header length for all headers failed: %s. %p"), *InternetTranslateError(GetLastError()), this);
			return Result;
		}
		if (HeaderSize == 0)
		{
			debugf(NAME_DevHttpRequest, TEXT("HttpQueryInfo for all headers returned zero header size. %p"), this);
			return Result;
		}
		TArray<TCHAR> HeaderBuffer(HeaderSize/sizeof(TCHAR));
		if (!HttpQueryInfo(RequestHandle, HTTP_QUERY_RAW_HEADERS_CRLF, HeaderBuffer.GetTypedData(), &HeaderSize, NULL))
		{
			debugf(NAME_DevHttpRequest, TEXT("HttpQueryInfo for all headers failed: %s. %p"), *InternetTranslateError(GetLastError()), this);
			return Result;
		}
		// parse all the key/value pairs
		const TCHAR* HeaderPtr = HeaderBuffer.GetTypedData();
		// don't count the terminating NULL character as one to search.
		const TCHAR* EndPtr = HeaderPtr + HeaderBuffer.Num()-1;
		while (HeaderPtr < EndPtr)
		{
			const TCHAR* DelimiterPtr = appStrstr(HeaderPtr, TEXT("\r\n"));
			if (DelimiterPtr == NULL)
			{
				DelimiterPtr = EndPtr;
			}
			Result.AddItem(FString(DelimiterPtr-HeaderPtr, HeaderPtr));
			HeaderPtr = DelimiterPtr + 2;
		}
	}
	else
	{
		debugf(NAME_DevHttpRequest, TEXT("HttpQueryInfo for all headers returned success when trying to determine the size for the header buffer. %p"), this);
	}
	return Result;
}

INT FHttpResponseWinInet::GetResponseCode() const
{
	// get the response code
	INT Code;
	DWORD CodeSize = sizeof(Code);
	if (!HttpQueryInfo(RequestHandle, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &Code, &CodeSize, NULL))
	{
		debugf(NAME_DevHttpRequest, TEXT("HttpQueryInfo for response code failed: %s. %p"), *InternetTranslateError(GetLastError()), this);
	}
	return Code;
}
#endif
