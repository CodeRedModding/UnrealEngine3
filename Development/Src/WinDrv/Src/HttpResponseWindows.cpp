/*=============================================================================
	HttpResponseWindows.cpp: Windows specific Http Response implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "WinDrv.h"
#if WITH_UE3_NETWORKING
#include "UnIpDrv.h"
#include "EnginePlatformInterfaceClasses.h"
#include "WinDrvClasses.h"
#include "HttpImplementationWinInet.h"

// Implement the script class.
IMPLEMENT_CLASS(UHttpResponseWindows);

/** Make sure we clean up our native classes. */
void UHttpResponseWindows::BeginDestroy()
{
	debugf(NAME_DevHttpRequest, TEXT("UHttpResponseWindows::BeginDestroy called: %p"), Response);
	Super::BeginDestroy();
	delete Response; Response = NULL;
}


void UHttpResponseWindows::AssignResponsePointer(FHttpResponseWinInet* ResponseImpl)
{
	check(!Response && "AssignResponsePointer called twice on the same UHttpResponseWindows instance!");
	Response = ResponseImpl;
}

/** Passthrough to implementation class. */
FString UHttpResponseWindows::GetHeader(const FString& HeaderName)
{
	return Response->GetHeader(HeaderName);
}

/** Passthrough to implementation class. */
TArray<FString> UHttpResponseWindows::GetHeaders()
{
	return Response->GetAllHeaders();
}

/** Passthrough to implementation class. */
FString UHttpResponseWindows::GetURLParameter(const FString& ParameterName)
{
	const FString* Result = Response->GetURL().GetParameter(ParameterName);
	return Result != NULL ? *Result : FString();
}

/** Passthrough to implementation class. */
FString UHttpResponseWindows::GetContentType()
{
	return Response->GetContentType();
}

/** Passthrough to implementation class. */
INT UHttpResponseWindows::GetContentLength()
{
	return Response->GetContentLength();
}

/** Passthrough to implementation class. */
FString UHttpResponseWindows::GetURL()
{
	return Response->GetURL().GetURL();
}

/** Passthrough to implementation class. */
void UHttpResponseWindows::GetContent(TArray<BYTE>& Content)
{
	static TArray<BYTE> EmptyContent;
	Content = Response->IsReady() ? *Response->GetPayload() : EmptyContent;
}

/** Passthrough to implementation class. */
FString UHttpResponseWindows::GetContentAsString()
{
	if (Response->IsReady())
	{
		return Response->GetPayloadAsString();
	}
	return FString();
}

/** Passthrough to implementation class. */
INT UHttpResponseWindows::GetResponseCode()
{
	INT NoCode = 0;
	if (Response->IsReady())
	{
		return Response->GetResponseCode();
	}
	return NoCode;
}
#endif
