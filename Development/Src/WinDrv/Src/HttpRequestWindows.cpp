/*=============================================================================
	HttpRequestWindows.cpp: Windows specific Http Request implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "WinDrv.h"

#if WITH_UE3_NETWORKING
#include "UnIpDrv.h"
#include "EnginePlatformInterfaceClasses.h"
#include "WinDrvClasses.h"
#include "HttpImplementationWinInet.h"


// Implement the script class.
IMPLEMENT_CLASS(UHttpRequestWindows);


/** Class that manages checking if the responses are ready and calling the delegate on them. */
class FHttpTickerWindows : FTickableObject
{
public:
	struct RequestResponsePair
	{
		RequestResponsePair(UHttpRequestWindows* InRequest, FHttpResponseWinInet* InResponse) : Request(InRequest), Response(InResponse) {}
		UHttpRequestWindows* Request;
		FHttpResponseWinInet* Response;
	};

	/** Perform any per-frame actions */
	virtual void Tick(FLOAT DeltaSeconds)
	{
		for (TArray<RequestResponsePair>::TConstIterator It(PendingResponses);It;It)
		{
			if (It->Response->IsReady())
			{
				debugf(NAME_DevHttpRequest, TEXT("Response is ready: %p"), It->Response);
				// remove the response from the pending queue
				RequestResponsePair Pair = PendingResponses(It.GetIndex());
				PendingResponses.Remove(It.GetIndex());
				// create the response UObject and bind the response to it.
				UHttpResponseWindows* ResponseObj = ConstructObject<UHttpResponseWindows>(UHttpResponseWindows::StaticClass());
				ResponseObj->AssignResponsePointer(Pair.Response);
				// call the delegate on the originating request using the new response.
				Pair.Request->delegateOnProcessRequestComplete(Pair.Request, ResponseObj, Pair.Response->Succeeded());
				// we're done with the object, remove our reference to it.
				Pair.Request->RemoveFromRoot();
			}
			else
			{
				++It;
			}
		}
	}

	/** Always tick when there are requests. */
	virtual UBOOL IsTickable() const
	{
		return PendingResponses.Num() > 0;
	}

	/** make sure this is always ticking, even while game is paused */
	virtual UBOOL IsTickableWhenPaused() const
	{
		return TRUE;
	}
public: TArray<RequestResponsePair> PendingResponses;
};

static FHttpTickerWindows GHttpTicker;

/** Make sure we clean up our native classes. */
void UHttpRequestWindows::BeginDestroy()
{
	debugf(NAME_DevHttpRequest, TEXT("UHttpRequestWindows::BeginDestroy called: %p"), Request);
	Super::BeginDestroy();
	delete Request; Request = NULL;
	delete RequestURL; RequestURL = NULL;
}

/** Passthrough to implementation class. */
FString UHttpRequestWindows::GetHeader(const FString& HeaderName)
{
	if (!Request) return FString();

	const FString* Result = Request->GetHeader(HeaderName);
	return Result != NULL ? *Result : FString();
}

/** Passthrough to implementation class. */
TArray<FString> UHttpRequestWindows::GetHeaders()
{
	if (!Request) return TArray<FString>();
	return Request->GetHeaders();
}

/** Passthrough to implementation class. */
FString UHttpRequestWindows::GetURLParameter(const FString& ParameterName)
{
	if (!RequestURL) return FString();

	const FString* Result = RequestURL->GetParameter(ParameterName);
	return Result != NULL ? *Result : FString();
}

/** Passthrough to implementation class. */
FString UHttpRequestWindows::GetContentType()
{
	if (!Request) return FString();

	const FString* Result = Request->GetHeader(TEXT("Content-Type"));
	return Result != NULL ? *Result : FString();
}

/** Passthrough to implementation class. */
INT UHttpRequestWindows::GetContentLength()
{
	return Payload.Num();
}

/** Passthrough to implementation class. */
FString UHttpRequestWindows::GetURL()
{
	if (!RequestURL) return FString();

	return RequestURL->GetURL();
}

/** Passthrough to implementation class. */
void UHttpRequestWindows::GetContent(TArray<BYTE>& Content)
{
	Content = Payload;
}

/** Passthrough to implementation class. */
FString UHttpRequestWindows::GetVerb()
{
	return RequestVerb;
}

/** Passthrough to implementation class. */
UHttpRequestInterface* UHttpRequestWindows::SetVerb(const FString& Verb)
{
	RequestVerb = Verb;
	return this;
}

/** Passthrough to implementation class. */
UHttpRequestInterface* UHttpRequestWindows::SetURL(const FString& URL)
{
	if (!RequestURL)
	{
		RequestURL = new FURLWinInet(URL);
	}
	else
	{
		*RequestURL = FURLWinInet(URL);
	}
	return this;
}

/** Passthrough to implementation class. */
UHttpRequestInterface* UHttpRequestWindows::SetContent(const TArray<BYTE>& ContentPayload)
{
	Payload = ContentPayload;
	return this;
}

/** Passthrough to implementation class. */
UHttpRequestInterface* UHttpRequestWindows::SetContentAsString(const FString& ContentPayload)
{
	FTCHARToUTF8 Converter(*ContentPayload);
	TArray<BYTE> UTF8Payload(Converter.Length());
	// make sure not to copy the null terminator.
	appMemcpy(UTF8Payload.GetTypedData(), (BYTE*)(ANSICHAR*)Converter, UTF8Payload.Num());

	Payload = UTF8Payload;
	return this;
}

/** Passthrough to implementation class. */
UHttpRequestInterface* UHttpRequestWindows::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	if (!Request)
	{
		Request = new FHttpRequestWinInet();
	}
	Request->SetHeader(HeaderName, HeaderValue);
	return this;
}

/** Passthrough to implementation class. */
UBOOL UHttpRequestWindows::ProcessRequest()
{
	if (!RequestURL)
	{
		debugf(NAME_DevHttpRequest, TEXT("Must set a URL before calling ProcessRequest."));
		return FALSE;
	}
	if (!Request)
	{
		Request = new FHttpRequestWinInet();
	}
	// Make sure the object is not cleaned up before we are done with the callback.
	FHttpResponseWinInet* Response = Request->MakeRequest(*RequestURL, RequestVerb, Payload);
	if (Response != NULL)
	{
		AddToRoot();
		GHttpTicker.PendingResponses.AddItem(FHttpTickerWindows::RequestResponsePair(this, Response));
		return TRUE;
	}
	// if the request didn't fire correctly, call the delegate immediately.
	delegateOnProcessRequestComplete(this, NULL, FALSE);
	return FALSE;
}

#endif
