/*=============================================================================
	HttpRequestIPhone.mm: IPhone specific Http implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include "UnIpDrv.h"
#include "IPhoneObjCWrapper.h"
#include "NSStringExtensions.h"

class UHttpRequestIPhone;
@class FHttpResponseIPhone;

/** Class that manages checking if the responses are ready and calling the delegate on them. */
class FHttpTickerIPhone : FTickableObject
{
public:
	struct RequestResponsePair
	{
		RequestResponsePair(UHttpRequestIPhone* InRequest, FHttpResponseIPhone* InResponse) : Request(InRequest), Response(InResponse) {}
		UHttpRequestIPhone* Request;
		FHttpResponseIPhone* Response;
	};

	/** Checks each response to see if it's ready, then handles delegate calling, etc. */
	inline virtual void Tick(FLOAT DeltaSeconds);

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

// Global instance of the HttpTicker for iPhone.
static FHttpTickerIPhone GHttpTicker;

/**
 * iPhone class that handles Http responses. Acts as an NSUrlRequest delegate, handling all the callbacks.
 * this class is created in the game thread, but the NSUrlRequest is told to fire delegate callbacks on the main
 * thread's run loop. When complete, this class sets IsReady to YES, and HadError to indicate whether the response 
 * was completely received without error. The global FHttpTickerIPhone polls each request, checking for IsReady
 * to be YES before firing off the script delegate associated with the request.
 * 
 * NOTE: All of the instance methods that access the response are not valid to be called until IsReady = YES.
 * This class does not check this because no one outside this file can call those members, and the underlying code
 * enforces this requirement.
 */
@interface FHttpResponseIPhone : NSObject
{
};
/** When the response is complete, indicates whether the response was received without error. */
@property BOOL HadError;
/** When the response is complete, this variable is set to YES to indicate to the main thread that it is ready. */
@property BOOL IsReady;
/** Holds the payload as we receive it. */
@property(retain) NSMutableData* Payload;
/** Holds the Response object that we can use to get headers, URL, etc. */
@property(retain) NSHTTPURLResponse* Response;
@end

/** Implementation */
@implementation FHttpResponseIPhone
@synthesize HadError;
@synthesize IsReady;
@synthesize Payload;
@synthesize Response;

/** Gets a header associated with the response. Only valid after we start receiving a response. */
-(FString) GetHeaderValue:(const FString&)HeaderName
{
	return FString((NSString*)[[Response allHeaderFields] objectForKey:[NSString stringWithFString:HeaderName]]);
}

/** Gets all the headers associated with the response. Only valid after we start receiving a response. */
-(TArray<FString>) GetAllHeaders
{
	NSDictionary* Headers = [Response allHeaderFields];
	TArray<FString> Result;
	Result.Reserve([Headers count]);
	for (NSString* Key in [Headers allKeys])
	{
		NSString* Value = [Headers objectForKey:Key];
		Result.AddItem(FString::Printf(TEXT("%s: %s"), *FString(Key), *FString(Value)));
	}
	return Result;
}

/** 
 * Gets a parameter (case sensitive) associated with the response. Only valid after we start receiving a response.
 * NOTE: This code does not cache the mapping of URLs. If this method is called a lot
 *       for some reason, the mapping should be cached.
 * Only valid after we start receiving a response.
 */
-(FString) GetURLParameter:(const FString&)ParameterName
{
	NSString* ParameterNameStr = [NSString stringWithFString:ParameterName];
	NSArray* Parameters = [[[Response URL] query] componentsSeparatedByString:@"&"];
	for (NSString* Parameter in Parameters) 
	{
		NSArray* KevValue = [Parameter componentsSeparatedByString:@"="];
		NSString* Key = [KevValue objectAtIndex:0];
		if ([Key compare:ParameterNameStr] == NSOrderedSame) 
		{
			return FString([[KevValue objectAtIndex:1] stringByReplacingPercentEscapesUsingEncoding:NSUTF8StringEncoding]);
		}
	}
	return FString();
}

/** Gets the actual length of the returned payload (the Content-Length is not guaranteed to be valid). Only valid after we start receiving a response. */
-(INT) GetContentLength
{
	return [Payload length];
}

/** Gets the full URL of the request that originated the response. Only valid after we start receiving a response. */
-(FString) GetURL
{
	return FString([[Response URL] absoluteString]);
}

/** Gets the response payload itself. Only valid after we start receiving a response. */
-(void) GetContent:(TArray<BYTE>&)Content
{
	Content.SetNum([Payload length]);
	appMemcpy(Content.GetData(), [Payload bytes], Content.Num());
}

/** 
 * Gets the response payload as a string.
 * NOTE: Makes a few copies of the payload to do this, so not a very fast function.
 *       If this becomes the common case, the payload can be pre-appended with a NULL terminator.
 * Only valid after we start receiving a response. 
 */
-(FString) GetContentAsString
{
	TArray<BYTE> ZeroTerminatedPayload([Payload length] + 1);
	appMemcpy(ZeroTerminatedPayload.GetData(), [Payload bytes], [Payload length]);
	ZeroTerminatedPayload([Payload length]) = 0;
	return FString(UTF8_TO_TCHAR(ZeroTerminatedPayload.GetData()));
}

/** Gets the response code. Only valid after we start receiving a response. */
-(INT) GetResponseCode
{
	return [Response statusCode];
}

/** Dtor. For debugging. */
-(void) dealloc
{
	debugf(NAME_DevHttpRequest, TEXT("FHttpResponseIPhone::dealloc: %p"), self);
	[super dealloc];
}

/** Delegate called with we receive a response. See iOS docs for when/how this should be used. */
-(void) connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response
{
	self.Response = (NSHTTPURLResponse*)response;
	// presize the payload container if possible
	self.Payload = [NSMutableData dataWithCapacity:([response expectedContentLength] != NSURLResponseUnknownLength ? [response expectedContentLength] : 0)];
	debugf(NAME_DevHttpRequest, TEXT("didReceiveResponse: expectedContentLength = %d. Length = %d: %p"), [response expectedContentLength], [self.Payload length], self);
}

/** Delegate called with we receive data. See iOS docs for when/how this should be used. */
-(void) connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
	[self.Payload appendData:data];
	debugf(NAME_DevHttpRequest, TEXT("didReceiveData with %d bytes. After Append, Payload Length = %d: %p"), [data length], [self.Payload length], self);
}

/** Delegate called with we complete with an error. See iOS docs for when/how this should be used. */
-(void) connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
	self.IsReady = YES;
	self.HadError = YES;
	debugf(NAME_DevHttpRequest, TEXT("didFailWithError. Http request failed - %s %s: %p"), 
		*FString([error localizedDescription]),
		*FString([[error userInfo] objectForKey:NSURLErrorFailingURLStringErrorKey]),
		self);
	[connection release];
}

/** Delegate called with we complete successfully. See iOS docs for when/how this should be used. */
-(void) connectionDidFinishLoading:(NSURLConnection *)connection
{
	debugf(NAME_DevHttpRequest, TEXT("connectionDidFinishLoading: %p"), self);
	self.IsReady = YES;
	[connection release];
}

@end

/** iPhone implementation of a HTTP request. Basically the script class forwards all implementation details to this class. */
@interface FHttpRequestIPhone : NSObject;
/** Implementation of script function. */
-(FString) GetURL;
/** Implementation of script function. */
-(FString) GetVerb;
/** Implementation of script function. */
-(void) SetURL:(const FString&)InURL;
/** Implementation of script function. */
-(void) SetVerb:(const FString&)InVerb;
/** Implementation of script function. */
-(void) SetPayload:(const TArray<BYTE>&)InPayload;
/** Implementation of script function. */
-(void) SetPayloadWithString:(const FString&)InPayload;
/** Implementation of script function. */
-(void) GetContent:(TArray<BYTE>&)Content;
/** Implementation of script function. */
-(INT) GetContentLength;
/** Implementation of script function. */
-(void) SetHeaderName:(const FString&)HeaderName ToValue:(const FString&)HeaderValue;
/** Implementation of script function. */
-(FString) GetHeaderValue:(const FString&)HeaderName;
/** Implementation of script function. */
-(TArray<FString>) GetAllHeaders;
/** Implementation of script function. */
-(FString) GetURLParameter:(const FString&)ParameterName;
/** Starts processing the request. Uses the Request property, kicks off the request on the main thread. After that, the delegate handles all the work. */
-(UBOOL) ProcessRequest:(UHttpRequestIPhone*)RequestObj;
/** Holds onto the request object that will be used to make the request. This class is mutated by the above member functions, then used to start the connection. */
@property(retain) NSMutableURLRequest* Request;
@end


/**
 * Struct to hold the pimpl idiom so it will get cleaned up when the object is garbage collected.
 */
struct FHttpRequestIPhoneImplContainer
{
public:
	FHttpRequestIPhoneImplContainer()
	{
		debugf(NAME_DevHttpRequest, TEXT("Creating FHttpRequestIPhoneImplContainer: %p"), this);
		Request = [[FHttpRequestIPhone alloc] init];
	}
	~FHttpRequestIPhoneImplContainer()
	{
		debugf(NAME_DevHttpRequest, TEXT("Destroying FHttpRequestIPhoneImplContainer with Request NSObject = %p: %p"), Request, this);
		[Request release];
	}
private:
	// iPhone implementation has to be able to assign the Request pointer.
	friend class UHttpRequestIPhone;
	// retained object
	FHttpRequestIPhone* Request;
};

/**
 * IPhone implementation of the UHttpRequestInterface.
 * Just forwards implementations to the underlying iPhone class.
 */
class UHttpRequestIPhone : public UHttpRequestInterface
{
	DECLARE_CLASS_INTRINSIC(UHttpRequestIPhone, UHttpRequestInterface, 0, IPhoneDrv)

public:
	/** See parent class for docs. */
	virtual FString GetHeader(const FString& HeaderName)
	{
		return [ImplContainer.Request GetHeaderValue:HeaderName];
	}
	/** See parent class for docs. */
	virtual TArray<FString> GetHeaders()
	{
		return [ImplContainer.Request GetAllHeaders];
	}
	/** See parent class for docs. */
	virtual FString GetURLParameter(const FString& ParameterName)
	{
		return [ImplContainer.Request GetURLParameter:ParameterName];
	}
	/** See parent class for docs. */
	virtual FString GetContentType()
	{
		return GetHeader(TEXT("Content-Type"));
	}
	/** See parent class for docs. */
	virtual INT GetContentLength()
	{
		return [ImplContainer.Request GetContentLength];
	}
	/** See parent class for docs. */
	virtual FString GetURL()
	{
		return [ImplContainer.Request GetURL];
	}
	/** See parent class for docs. */
	virtual void GetContent(TArray<BYTE>& Content)
	{
		[ImplContainer.Request GetContent:Content];
	}
	/** See parent class for docs. */
	virtual FString GetVerb()
	{
		return [ImplContainer.Request GetVerb];
	}
	/** See parent class for docs. */
	virtual UHttpRequestInterface* SetVerb(const FString& Verb)
	{
		[ImplContainer.Request SetVerb:Verb];
		return this;
	}
	/** See parent class for docs. */
	virtual UHttpRequestInterface* SetURL(const FString& URL)
	{
		[ImplContainer.Request SetURL:URL];
		return this;
	}
	/** See parent class for docs. */
	virtual UHttpRequestInterface* SetContent(const TArray<BYTE>& ContentPayload)
	{
		[ImplContainer.Request SetPayload:ContentPayload];
		return this;
	}
	/** See parent class for docs. */
	virtual UHttpRequestInterface* SetContentAsString(const FString& ContentString)
	{
		[ImplContainer.Request SetPayloadWithString:ContentString];
		return this;
	}
	/** See parent class for docs. */
	virtual UHttpRequestInterface* SetHeader(const FString& HeaderName,const FString& HeaderValue)
	{
		[ImplContainer.Request SetHeaderName:HeaderName ToValue:HeaderValue];
		return this;
	}
	/** See parent class for docs. */
	virtual UBOOL ProcessRequest()
	{
		return [ImplContainer.Request ProcessRequest:this];
	}
private:
	/** pImpl container to make sure we destroy the underlying iOS object appropriately. */
	FHttpRequestIPhoneImplContainer ImplContainer;
};

/** Implementation of Request object for iPhone */
@implementation FHttpRequestIPhone
@synthesize Request;

/** Ctor. allocates the NSURLRequest object. */
-(FHttpRequestIPhone*) init
{
	self = [super init];

	self.Request = [[[NSMutableURLRequest alloc] init] autorelease];

	return self;
}

/** Dtor for debugging. */
-(void) dealloc
{
	debugf(NAME_DevHttpRequest, TEXT("FHttpRequestIPhone::dealloc"));
	[super dealloc];
}

/** Gets the absolute URL of the request. */
-(FString) GetURL
{
	return FString([[Request URL] absoluteString]);
}

/** Gets the VERB of the request. */
-(FString) GetVerb
{
	return FString([Request HTTPMethod]);
}

/** Sets the URL of the request. */
-(void) SetURL:(const FString&)InURL
{
	[Request setURL:[NSURL URLWithString:[NSString stringWithFString:InURL]]];
}

/** Sets the VERB of the request. */
-(void) SetVerb:(const FString&)InVerb
{
	[Request setHTTPMethod:[NSString stringWithFString:InVerb]];
}

/** Sets the payload of the request. */
-(void) SetPayload:(const TArray<BYTE>&)InPayload
{
	[Request setHTTPBody:[NSData dataWithBytes:InPayload.GetData() length:InPayload.Num()]];
}

/** Sets the payload of the request using a string. */
-(void) SetPayloadWithString:(const FString&)InPayload
{
	FTCHARToUTF8 Converter(*InPayload);
	// The extra length computation here is unfortunate, but it's technically not safe to assume the length is the same.
	[Request setHTTPBody:[NSData dataWithBytes:(ANSICHAR*)Converter length:Converter.Length()]];
}

/** gets the payload of the request. */
-(void) GetContent:(TArray<BYTE>&)Content
{
	Content.SetNum([[Request HTTPBody] length]);
	appMemcpy(Content.GetData(), [[Request HTTPBody] bytes], Content.Num());
}

/** Gets the length of the payload of the request (NOT the same as the Content-Length header, which isn't set until the request is kicked off. */
-(INT) GetContentLength
{
	return [[Request HTTPBody] length];
}

/** Sets a header to a given value. */
-(void) SetHeaderName:(const FString&)HeaderName ToValue:(const FString&)HeaderValue
{
	[Request setValue:[NSString stringWithFString:HeaderValue] forHTTPHeaderField:[NSString stringWithFString:HeaderName]];
}

/** Gets a header value for a given name. */
-(FString) GetHeaderValue:(const FString&)HeaderName
{
	return FString([Request valueForHTTPHeaderField:[NSString stringWithFString:HeaderName]]);
}

/** Gets all headers for the request (Content-Length and User-Agent is not set until the request is kicked off). */
-(TArray<FString>) GetAllHeaders
{
	NSDictionary* Headers = [Request allHTTPHeaderFields];
	TArray<FString> Result;
	Result.Reserve([Headers count]);
	for (NSString* Key in [Headers allKeys])
	{
		NSString* Value = [Headers objectForKey:Key];
		Result.AddItem(FString::Printf(TEXT("%s: %s"), *FString(Key), *FString(Value)));
	}
	return Result;
}

/** 
 * Gets a parameter (case sensitive) associated with the response. Only valid after we start receiving a response.
 * NOTE: This code does not cache the mapping of URLs. If this method is called a lot
 *       for some reason, the mapping should be cached.
 */
-(FString) GetURLParameter:(const FString&)ParameterName
{
	NSString* ParameterNameStr = [NSString stringWithFString:ParameterName];
	NSArray* Parameters = [[[Request URL] query] componentsSeparatedByString:@"&"];
	for (NSString* Parameter in Parameters) 
	{
		NSArray* KeyValue = [Parameter componentsSeparatedByString:@"="];
		NSString* Key = [KeyValue objectAtIndex:0];
		if ([Key compare:ParameterNameStr] == NSOrderedSame) 
		{
			return FString([[KeyValue objectAtIndex:1] stringByReplacingPercentEscapesUsingEncoding:NSUTF8StringEncoding]);
		}
	}
	return FString();
}

/** Kicks off a request. */
-(UBOOL) ProcessRequest:(UHttpRequestIPhone*)RequestObj
{
	debugf(NAME_DevHttpRequest, TEXT("ProcessRequest: %p"), self);
	// set the content-length and user-agent
	if ([self GetContentLength] > 0)
	{
		[Request setValue:[NSString stringWithFormat:@"%llu", [self GetContentLength]] forHTTPHeaderField:@"Content-Length"];
	}	
	[Request addValue:[NSString stringWithFString:FString::Printf(TEXT("UE3-%s,UE3Ver(%d)"), appGetGameName(), GEngineVersion)] forHTTPHeaderField:@"User-Agent"];
	// allocates the response delegate and adds it to the global HTTP ticker that will poll for completion. 
	FHttpResponseIPhone* Response = [[FHttpResponseIPhone alloc] init];
	GHttpTicker.PendingResponses.AddItem(FHttpTickerIPhone::RequestResponsePair(RequestObj, Response));
	RequestObj->AddToRoot();
	// Create the connection, tell it to run in the main run loop, and kick it off.
	NSURLConnection* Connection = [[NSURLConnection alloc] initWithRequest:Request delegate:Response startImmediately:NO];
	[Connection scheduleInRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
	[Connection start];
	return TRUE;
}

@end

/**
 * Struct to hold the pimpl idiom so it will get cleaned up when the script object is garbage collected.
 */
struct FHttpResponseIPhoneImplContainer
{
public:
	FHttpResponseIPhoneImplContainer()
	{
		debugf(TEXT("Creating FHttpResponseIPhoneImplContainer"));
	}
	~FHttpResponseIPhoneImplContainer()
	{
		debugf(TEXT("Destroying FHttpResponseIPhoneImplContainer"));
		[Response release];
	}
private:
	/** Assigns a new response implementation to the class. */
	void AssignResponsePointer(FHttpResponseIPhone* InResponse)
	{
		[InResponse retain];
		[Response release];
		Response = InResponse;
	}

	// The response needs access to the Response
	friend class UHttpResponseIPhone;
	// The global HTTP ticker needs to call AssignResponsePointer
	friend class FHttpTickerIPhone;
	FHttpResponseIPhone* Response;
};

/** Script implementation of the UHttpResponseInterface for iPhone. */
class UHttpResponseIPhone : public UHttpResponseInterface
{
	DECLARE_CLASS_INTRINSIC(UHttpResponseIPhone, UHttpResponseInterface, 0, IPhoneDrv)

public:
	/** See parent class for docs. */
	virtual FString GetHeader(const FString& HeaderName)
	{
		return [ImplContainer.Response GetHeaderValue:HeaderName];
	}
	/** See parent class for docs. */
	virtual TArray<FString> GetHeaders()
	{
		return [ImplContainer.Response GetAllHeaders];
	}
	/** See parent class for docs. */
	virtual FString GetURLParameter(const FString& ParameterName)
	{
		return [ImplContainer.Response GetURLParameter:ParameterName];
	}
	/** See parent class for docs. */
	virtual FString GetContentType()
	{
		return GetHeader(TEXT("Content-Type"));
	}
	/** See parent class for docs. */
	virtual INT GetContentLength()
	{
		return [ImplContainer.Response GetContentLength];
	}
	/** See parent class for docs. */
	virtual FString GetURL()
	{
		return [ImplContainer.Response GetURL];
	}
	/** See parent class for docs. */
	virtual void GetContent(TArray<BYTE>& Content)
	{
		[ImplContainer.Response GetContent:Content];
	}
	/** See parent class for docs. */
	virtual FString GetContentAsString()
	{
		return [ImplContainer.Response GetContentAsString];
	}
	/** See parent class for docs. */
	virtual INT GetResponseCode()
	{
		return [ImplContainer.Response GetResponseCode];
	}
private:
	// Ticker needs access to this member to assign the response pointer.
	friend FHttpTickerIPhone;
	FHttpResponseIPhoneImplContainer ImplContainer;
};

/** Waits for requests to be ready, then fires off script delegates and cleans up the underlying implementation. */
inline void FHttpTickerIPhone::Tick(FLOAT DeltaSeconds)
{
	for (TArray<RequestResponsePair>::TConstIterator It(PendingResponses);It;It)
	{
		if ([It->Response IsReady])
		{
			debugf(NAME_DevHttpRequest, TEXT("Response is ready: %p"), It->Response);
			// remove the response from the pending queue
			RequestResponsePair Pair = PendingResponses(It.GetIndex());
			PendingResponses.Remove(It.GetIndex());
			// create the response UObject and bind the response to it.
			UHttpResponseIPhone* ResponseObj = ConstructObject<UHttpResponseIPhone>(UHttpResponseIPhone::StaticClass());
			ResponseObj->ImplContainer.AssignResponsePointer(Pair.Response);
			// call the delegate on the originating request using the new response.
			Pair.Request->delegateOnProcessRequestComplete(Pair.Request, ResponseObj, ![Pair.Response HadError]);
			// we're done with the object, remove our reference to it.
			Pair.Request->RemoveFromRoot();
			[Pair.Response release];
		}
		else
		{
			++It;
		}
	}
}

/** UObject scaffolding */
IMPLEMENT_CLASS(UHttpRequestIPhone);
IMPLEMENT_CLASS(UHttpResponseIPhone);
void AutoInitializeRegistrantsHttpIPhone( INT& Lookup )
{
	UHttpRequestIPhone::StaticClass();
	UHttpResponseIPhone::StaticClass();
}
