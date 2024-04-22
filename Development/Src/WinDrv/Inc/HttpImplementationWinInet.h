/*=============================================================================
	Http implementation using WinInet API.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef _INC_HTTPIMPLEMENTATIONWININET
#define _INC_HTTPIMPLEMENTATIONWININET
#pragma once

#include "WinDrv.h"
#if WITH_UE3_NETWORKING
#include <wininet.h>

/**
 * Class that encapculates the logic for using WinInet to parse a URL string.
 * Shared by FHttpRequestWinInet and FHttpResponseWinInet classes to handle retrieving info about the URL.
 * Main feature is lazy evaluation of the WinInet structs used to parse the URL.
 * When the first field is accessed, it parses the string and caches the info it needs.
 * WARNING: There is a heavyweight map of URL parameters that is built, so if the URL contains
 * parameters, this class can be somewhat heavyweight.
 */
class FURLWinInet
{
public: 
	/** Default Ctor */
	inline FURLWinInet();
	/** Ctor from a string that contains the full URL */
	inline explicit FURLWinInet(const FString& InURL);
	/** Copy Ctor */
	inline FURLWinInet(const FURLWinInet& InURL);
	/** Assignment operator */
	inline FURLWinInet& operator=(const FURLWinInet& InURL);
	/** gets the full URL as a string. */
	inline const FString& GetURL() const;
	/** Extracts the host from the URL (empty if it can't be parsed). */
	inline FString GetHost() const;
	/** Extracts the port from the URL (0 if it can't be parsed). */
	inline WORD GetPort() const;
	/** Extracts the Path from the URL (empty if it can't be parsed). */
	inline FString GetPath() const;
	/** Extracts the extra info (parameters) from the URL (empty if it can't be parsed). */
	inline FString GetExtraInfo() const;
	/** Gets the full components object from windows. */
	inline const URL_COMPONENTS& GetURLComponents() const;
	/**
	 * Extract a URL parameter from the URL. If the URL doesn't use '?key=value&key2=value2'
	 * format, this will return NULL, or NULL if the parameter is not present.
	 * If the parameter has no value (?ParamWithNoValue), will return an empty string.
	 */
	inline const FString* GetParameter(const FString& ParameterName) const;
private:
	/** Shared function to clear the cached URL info. */
	inline void ClearCachedData() const;
	/** Does the real work of cracking the URL parameters on the first call. */
	void CrackUrlParameters() const;
	/** The full URL String.
	 ** WARNING: The data below uses pointers into this string, so it cannot be
	 ** modified without clearing the cache. */
	FString RequestURL;
	/** Cached data. Points into the string. */
	mutable const TCHAR* URLPtr;
	/** Cached data. Points into the string. */
	mutable URL_COMPONENTS URLParts;
	/** Cached parameter data. */
	mutable TMap<FString, FString> URLParameters;
};

// forward declare this class.
class FHttpResponseWinInet;

/**
 * WinInet implementation of an HttpRequest.
 * Basic usage is to create the object, set the URL, verb, headers, and payload,
 * then call MakeRequest, which returns a FHttpResponseWinInet instance that
 * you can wait on until it is ready, or delete to cancel the request.
 * NOTE: You can create on Request and call MakeRequest as many times on it as you wish.
 */
class FHttpRequestWinInet
{
public:
	/** Default Ctor. */
	FHttpRequestWinInet();
	/** Dtor. Doesn't really do anything because it doesn't really own any resources. */
	~FHttpRequestWinInet();
	/**
	 * Sends the request, returning the FHttpResponseWinInet object that can be waited on for completion.
	 * takes ownership of the Payload, so the array passed in will be empty on return of the function call.
	 */
	FHttpResponseWinInet* MakeRequest(const FURLWinInet& URL, const FString& Verb, TArray<BYTE>& Payload);
	/** Version of MakeRequest that copies the payload instead of transferring ownership. */
	FHttpResponseWinInet* MakeRequest(const FURLWinInet& URL, const FString& Verb, const TArray<BYTE>& Payload);
	/** Version of MakeRequest that takes a string (uses UTF8 encoding). */
	FHttpResponseWinInet* MakeRequest(const FURLWinInet& URL, const FString& Verb, const FString& Payload);
	/** Version of MakeRequest that takes no payload (useful for GET requests). */
	FHttpResponseWinInet* MakeRequest(const FURLWinInet& URL, const FString& Verb);
	/** 
	 * Gets the value of header set on the request (only for custom headers, implicit headers set by WinInet are not made available here),
	 * or NULL if the header is not present.
	 * NOTE: Stored in the class because UnrealScript doesn't natively support TMap easily.
	 */
	const FString* GetHeader(const FString& HeaderName) const;
	/** Gets all the headers associated with the request. Not very efficient. */
	TArray<FString> GetHeaders() const;
	/**
	 * Sets a custom header associated with the request.
	 * WARNING: Don't set Content-Length this way, this is handled automatically by this class when the Payload is assigned.
	 */
	FHttpRequestWinInet& SetHeader(const FString& HeaderName, const FString& HeaderValue);
private:
	/** Private function to return the buffer containing all of the headers to pass to WinInet when a request is made. */
	FString GenerateHeaderBuffer(size_t ContentLength);
	/** The response is a friend so this class can construct a new one and not allow anyone else to do it. */
	friend class FHttpResponseWinInet;
	/** The custom headers that will be used by the request. */
	TMap<FString, FString> RequestHeader;
};

/**
 * WinInet implementation of an HttpResponse.
 * Basic usage is to call FHttpRequestWinInet::MakeRequest, which returns a response.
 * When IsReady() is true, the response is ready and the payload can be accessed.
 * The URL can be accessed at any time.
 * NOTE: To cancel the request, delete the response, indicating that you don't need it.
 */
class FHttpResponseWinInet
{
public:
	/** Dtor. Can be used to cancel the request as well if IsReady() is not TRUE yet. */
	~FHttpResponseWinInet();
	/** Indicates whether the response is finished. When this becomes TRUE, check Succeeded() to see if the response completed successfully. */
	bool IsReady() const;
	/** Indicates whether the Response succeeded. Only valid after IsReady() returns TRUE. */
	bool Succeeded() const;
	/** Returns the response code from the server. Only valid after IsReady() returns TRUE. */
	INT GetResponseCode() const;
	/**
	 * Used to get the response payload when IsReady() returns TRUE.
	 * If there is no response, returns an empty array. If not ready, returns NULL.
	 */
	const TArray<BYTE>* GetPayload() const;
	/**
	 * Used to get the response payload as a string when IsReady() returns TRUE.
	 * Hardcoded to assume UTF8 encoding.
	 * If there is no response or it is not ready, returns empty string.
	 */
	const FString GetPayloadAsString() const;
	/** Gets a custom header value from the response object, or an empty string if not present. */
	FString GetHeader(const FString& HeaderName) const;
	/** Gets the Content-Type header. */
	FString GetContentType() const;
	/**
	 * Gets the Content-Length header, if present (not always present for say chunked responses). 
	 * To get the actual length of the response payload, use GetPayload().
	 */ 
	DWORD GetContentLength() const;
	/** Gets the original URL used to make the request. */
	const FURLWinInet& GetURL() const;
	TArray<FString> GetAllHeaders() const;
private:
	/** 
	 * Ctor. Private as no one but the FHttpRequestWinInet object should call it. 
	 * When this returns, the constructed object now owns the Payload and the WinInet handles.
	 */
	FHttpResponseWinInet(HINTERNET InSessionHandle, HINTERNET InRequestHandle, const FURLWinInet& InURL, TArray<BYTE>& InRequestPayload);
	/** Get the request payload (used by the Request, since it hands over ownership on construction. */
	const TArray<BYTE>& GetRequestPayload() const;
	/**
	 * This function is tricky because in async mode, responses may not come in all at once.
	 * Particularly, chunked responses could cause InternetReadFile to return IO_PENDING, in which
	 * case the largely undocumented behavior is to return and wait for the callback function
	 * to be called again later when you receive another REQUEST_COMPLETE signal.
	 * You are supposed to keep doing this until InternetReadFile returns 0 with a successful return value.
	 */
	void ProcessResponse(const INTERNET_ASYNC_RESULT* AsyncResult);
	FString PrivateGetHeaderString(DWORD HttpQueryInfoLevel, const FString& HeaderName) const;
	/**
	 * Fixed length buffer size to read responses from when the Content-Length is not available (ie, chunked responses).
	 * The response will be read in chunks of this size in this case.
	 */
	static const INT BUFFER_SIZE=16*1024;
	friend FHttpRequestWinInet;
	friend void CALLBACK InternetStatusCallbackWinInet(HINTERNET, DWORD_PTR,DWORD,LPVOID,DWORD);
	HINTERNET SessionHandle;
	HINTERNET RequestHandle;
	bool bIsReady;
	bool bResponseSucceeded;
	FURLWinInet RequestURL;
	/** The FHttpResponseWinInet has to take ownership of the payload else it could get cleaned up if the FHttpRequestWinInet is deleted early. */
	TArray<BYTE> RequestPayload;
	TArray<BYTE> ResponsePayload;
	/** 
	 * Holds the bytes read parameter that WinInet fills when a call to InternetReadFile returns asynchronously.
	 * The payload and the bytes read argument must stick around until the async call completes.
	 */
	INT AsyncBytesRead;
	/**
	 * Caches how many bytes of the response we've read so far. Since the read process could complete asynchronously, 
	 * we need to keep running state as the payload arrives.
	 */
	INT TotalBytesRead;
};


// Inline implementatation

inline FURLWinInet::FURLWinInet()
{
	ClearCachedData();
}

inline FURLWinInet::FURLWinInet( const FString& InURL ) :RequestURL(InURL)
{
	ClearCachedData();
}

inline FURLWinInet::FURLWinInet( const FURLWinInet& InURL ) :RequestURL(InURL.RequestURL)
{
	ClearCachedData();
}

inline FURLWinInet& FURLWinInet::operator=( const FURLWinInet& InURL )
{
	if (this != &InURL)
	{
		RequestURL = InURL.RequestURL;
		ClearCachedData();
	}
	return *this;
}


inline const FString& FURLWinInet::GetURL() const
{
	return RequestURL;
}

inline FString FURLWinInet::GetHost() const
{
	if (URLPtr == NULL) CrackUrlParameters();

	if (URLPtr != NULL)
	{
		return FString(URLParts.dwHostNameLength, URLParts.lpszHostName);
	}
	return FString();
}

inline WORD FURLWinInet::GetPort() const
{
	if (URLPtr == NULL) CrackUrlParameters();

	if (URLPtr != NULL)
	{
		return URLParts.nPort;
	}
	return 0;
}

inline FString FURLWinInet::GetPath() const
{
	if (URLPtr == NULL) CrackUrlParameters();

	if (URLPtr != NULL)
	{
		return FString(URLParts.dwUrlPathLength, URLParts.lpszUrlPath);
	}
	return FString();
}

inline FString FURLWinInet::GetExtraInfo() const
{
	if (URLPtr == NULL) CrackUrlParameters();

	if (URLPtr != NULL)
	{
		return FString(URLParts.dwExtraInfoLength, URLParts.lpszExtraInfo);
	}
	return FString();
}

const URL_COMPONENTS& FURLWinInet::GetURLComponents() const
{
	if (URLPtr == NULL) CrackUrlParameters();

	return URLParts;
}

inline const FString* FURLWinInet::GetParameter( const FString& ParameterName ) const
{
	if (URLPtr == NULL) CrackUrlParameters();

	return URLParameters.Find(ParameterName);
}

inline void FURLWinInet::ClearCachedData() const
{
	URLPtr = NULL;
	URLParameters.Reset();
	appMemZero(URLParts);
}


#endif
#endif
