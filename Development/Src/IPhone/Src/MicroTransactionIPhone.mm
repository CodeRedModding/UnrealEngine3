/*=============================================================================
	MicroTransactionIPhone.cpp: IPhone specific micro transaction support
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include "EnginePlatformInterfaceClasses.h"
#import "IPhoneAppDelegate.h"
#import "IPhoneAsyncTask.h"
#import <StoreKit/StoreKit.h>

/**
 * A helper to respond to callbacks related to micro transactions
 */
@interface FStoreKitHelper : NSObject<SKProductsRequestDelegate, SKPaymentTransactionObserver>

/**
 * Kicks off the store request, in the main thread 
 *
 * @param ProductSet The list of products to request information for
 */
- (void)StartRequest:(NSSet*)ProductSet;

@end

char* base64_encode(const void* buf, size_t size);
char* base64_decode(const char* s, size_t * data_len);


/**
 * StoreKit subclass of generic MicroTrans API
 */
class UMicroTransactionIPhone : public UMicroTransactionBase
{
	DECLARE_CLASS_INTRINSIC(UMicroTransactionIPhone, UMicroTransactionBase, 0, IPhoneDrv)

	/**
	 * Perform any initialization
	 */
	virtual void Init()
	{
		// get the list of products from the .ini
		TArray<FString> ProductIDs;
		GConfig->GetArray(TEXT("Engine.MicroTransactionInfo"), TEXT("ProductIDs"), ProductIDs, GEngineIni);

		if (ProductIDs.Num() == 0)
		{
			//Never got a good answer on whether changing an inifile location would mess up other peoples games, so supporting this 
			//deprecated location in the meantime.
			GConfig->GetArray(TEXT("IPhoneDrv.MicroTransactionIPhone"), TEXT("ProductIDs"), ProductIDs, GEngineIni);			
		}


		if (ProductIDs.Num() == 0)
		{
			debugf(TEXT("There are no product IDs configured for Microtransactions, all MT operations will fail"));
			return;
		}

		// instantiate the helper object for Obj-C operations
		Helper = [[FStoreKitHelper alloc] init];

		// the helper will listen for transaction results
		[[SKPaymentQueue defaultQueue] addTransactionObserver:Helper];

		// autoreleased NSSet to hold IDs
		NSMutableSet* ProductSet = [NSMutableSet setWithCapacity:ProductIDs.Num()];
		for (INT ProductIndex = 0; ProductIndex < ProductIDs.Num(); ProductIndex++)
		{
			NSString* ID = [NSString stringWithCString:TCHAR_TO_UTF8(*ProductIDs(ProductIndex)) encoding:NSUTF8StringEncoding];
debugf(TEXT("Requesting product id %s"), *ProductIDs(ProductIndex));
NSLog(@"Requesting product id %@", ID);
			// convert to NSString for the set objects
			[ProductSet addObject:ID];
		}

		// note the product request is in flight
		bIsProductRequestInFlight = TRUE;
		
		// go on main thread
		[Helper performSelectorOnMainThread:@selector(StartRequest:) withObject:ProductSet waitUntilDone:NO];
	}


	/**
	 * Query system for what purchases are available. Will fire a MTD_PurchaseQueryComplete
	 * if this function returns true.
	 *
	 * @return True if the query started successfully (delegate will receive final result)
	 */
	UBOOL QueryForAvailablePurchases()
	{
		debugf(TEXT("is product request in flight? %d"), bIsProductRequestInFlight);
		debugf(TEXT("avail products.num() %d"), AvailableProducts.Num());
		// if the product request isn't in flight, then we already have results, so fire the delegate now
		if (!bIsProductRequestInFlight)
		{
			FPlatformInterfaceDelegateResult Result(EC_EventParm);
			Result.bSuccessful = TRUE;
			CallDelegates(MTD_PurchaseQueryComplete, Result);
		}

		// this always succeeds
		return TRUE;
	}

	/**
	 * Triggers a product purchase. Will fire a MTF_PurchaseComplete if this function
	 * returns true.
	 *
	 * @param Index which product to purchase
	 * 
	 * @param True if the purchase was kicked off (delegate will receive final result)
	 */
	virtual UBOOL BeginPurchase(int Index)
	{
		// can only do one purchase at a time
		if (bIsProductRequestInFlight || bIsPurchaseInFlight)
		{
			return FALSE;
		}

		// make sure this allowed
		if (!IsAllowedToMakePurchases())
		{
			return FALSE;
		}

		if (Index < 0 || Index >= AvailableProducts.Num())
		{
			return FALSE;
		}

		// we are now buying something
		bIsPurchaseInFlight = TRUE;

		// remember which product we want to buy 
		NSMutableSet* ProductSet = [NSMutableSet setWithObjects:
				[NSString stringWithCString:TCHAR_TO_UTF8(*AvailableProducts(Index).Identifier) encoding:NSUTF8StringEncoding],
				nil];

		// kick off a request to refresh the product info (it appears that a request needs to be performed recently
		// when making a purchase...) - this will not affect the AvailableProducts, but it will kick off the 
		// purchase when it's complete
		SKProductsRequest *Request = [[SKProductsRequest alloc] initWithProductIdentifiers:ProductSet];

		// start the request, with callbacks happening on the helper object
		Request.delegate = Helper;
		[Request start];

		return TRUE;
	}

	/**
	 * @return True if the user is allowed to make purchases - should give a nice message if not
	 */
	virtual UBOOL IsAllowedToMakePurchases()
	{
		// make sure the user is allowed to make purchases on this device
		return [SKPaymentQueue canMakePayments] ? TRUE : FALSE;
	}


	/**
	 * Process the responses from an SKProductsRequest
	 */
	void ProcessProductsResponse(SKProductsResponse* Response)
	{
		if (bIsPurchaseInFlight)
		{
			// verify some assumptions
			if ([Response.products count] != 1)
			{
				debugf(TEXT("Wrong number of products [%d] in the response when trying to make a single purchase"), [Response.products count]);
				FPlatformInterfaceDelegateResult Result(EC_EventParm);
				Result.bSuccessful = FALSE;
				CallDelegates(MTD_PurchaseComplete, Result);

				// no longer purchasing anything
				bIsPurchaseInFlight = FALSE;
			}
			else
			{
				// get the product
				SKProduct* Product = [Response.products objectAtIndex:0];

				// now that we have recently refreshed the info, we can purchase it
				SKPayment* Payment = [SKPayment paymentWithProduct:Product];
				[[SKPaymentQueue defaultQueue] addPayment:Payment];
			}

			// this portion of purchasing is complete, we can now let another purchase start (the purchase callbacks will come in later)
			bIsPurchaseInFlight = FALSE;
		}
		else if (bIsProductRequestInFlight)
		{
			// create a product for everything that was returned
			for (SKProduct* Product in Response.products)
			{
				FPurchaseInfo* Info = new(AvailableProducts)FPurchaseInfo(EC_EventParm);
				Info->Identifier = FString(Product.productIdentifier);
				Info->DisplayName = FString(Product.localizedTitle);
				Info->DisplayDescription = FString(Product.localizedDescription);

				// format the string as a price for the user (taken from sample in SKProduct class ref)
				NSNumberFormatter* Formatter = [[NSNumberFormatter alloc] init];
				[Formatter setFormatterBehavior:NSNumberFormatterBehavior10_4];
				[Formatter setNumberStyle:NSNumberFormatterCurrencyStyle];
				[Formatter setLocale:Product.priceLocale];
				Info->DisplayPrice = FString([Formatter stringFromNumber:Product.price]);
				Info->CurrencyType = FString([Formatter currencyCode]);
				[Formatter release];
			}

			// call any delegates that exist
			if (HasDelegates(MTD_PurchaseQueryComplete))
			{
				FPlatformInterfaceDelegateResult Result(EC_EventParm);
				Result.bSuccessful = TRUE;
				CallDelegates(MTD_PurchaseQueryComplete, Result);
			}

			// we are done the product request, so no need to trigger it again
			bIsProductRequestInFlight = FALSE;
		}
		else
		{
			debugf(TEXT("ProcessProductsResponse called when nothing was in flight..."));
		}
	}


private:

	/** TRUE if an SKProductsRequest is in flight - if so, we need to call the script delegate when it completes */
	UBOOL bIsProductRequestInFlight;

	/** TRUE if a purchase (which triggers a nother product request for just that product) is in progress */
	UBOOL bIsPurchaseInFlight;

	/** Retained helper ObjC object */
	FStoreKitHelper* Helper;
};







@implementation FStoreKitHelper

/**
 * Kicks off the store request, in the main thread 
 *
 * @param ProductSet The list of products to request information for
 */
- (void)StartRequest:(NSSet*)ProductSet
{
	// kick off a request to the store to see what products are actually available to purchase
	SKProductsRequest* Request = [[SKProductsRequest alloc] initWithProductIdentifiers:ProductSet];

	// start the request, with callbacks happening on the helper object
	Request.delegate = self;
	[Request start];
}

/**
 * iOS callback for when the product request is complete
 */
- (void)productsRequest:(SKProductsRequest *)request didReceiveResponse:(SKProductsResponse *)response
{
	// make sure this happens in game thread
	IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
	AsyncTask.GameThreadCallback = ^ UBOOL (void)
		{
			((UMicroTransactionIPhone*)UPlatformInterfaceBase::GetMicroTransactionInterfaceSingleton())->ProcessProductsResponse(response);
			return TRUE;
		};
	[AsyncTask FinishedTask];

	// we are done with the release now, so let it get cleaned up when the caller of this function is done with it
	[request autorelease];
}


/**
 * iOS callback for when the product request fails
 */
- (void)request:(SKRequest *)request didFailWithError:(NSError *)error
{
	debugf(TEXT("FStoreKitHelper: Request failed."));

	debugf(NAME_Dev, TEXT("didFailWithError. Store Kit request failed - %s %s: %p"), 
		*FString([error localizedDescription]),
		*FString([[error userInfo] objectForKey:NSURLErrorFailingURLStringErrorKey]),
		self);
}

/**
 * iOS callback for when the product request is done done
 */
//	- (void)requestDidFinish:(SKRequest *)request


/**
 * Callback for payment queue notifications (purchase completed, etc
 */
- (void)paymentQueue :(SKPaymentQueue *)queue updatedTransactions:(NSArray *)transactions
{
	// look over the transactions
	for (SKPaymentTransaction* Transaction in transactions)
	{
		// ignore the purchasing state, script doesn't care
		if (Transaction.transactionState == SKPaymentTransactionStatePurchasing)
		{
			continue;
		}

		// process the transactions on the game thread
		IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
		AsyncTask.GameThreadCallback = ^ UBOOL ()
			{
debugf(TEXT("Processing transaction on game thread..."));
				UMicroTransactionBase* MicroTrans = UPlatformInterfaceBase::GetMicroTransactionInterfaceSingleton();
				// we only tell script code about this transaction if there are any delegates listening,
				// because this may come in after the game has restarted and the listeners may not
				// be listening quite yet
				if (MicroTrans->HasDelegates(MTD_PurchaseComplete))
				{
					FPlatformInterfaceDelegateResult Result(EC_EventParm);
					// successful if the purchase was purchased or restored
					Result.bSuccessful = (Transaction.transactionState == SKPaymentTransactionStatePurchased) ||
						(Transaction.transactionState == SKPaymentTransactionStateRestored);
					
					// fill out the custom result
					Result.Data.Type = PIDT_Custom;
					Result.Data.StringValue = FString(Transaction.payment.productIdentifier);
					if (Result.bSuccessful)
					{
						// encode the receipt as Base64, and pass that with the purchase info
						ANSICHAR* EncodedString = base64_encode(Transaction.transactionReceipt.bytes, Transaction.transactionReceipt.length);
						Result.Data.StringValue2 = FString(EncodedString);
					}

					// set the result enum into IntValue
					if (Transaction.transactionState == SKPaymentTransactionStatePurchased)
					{
						Result.Data.IntValue = MTR_Succeeded;
					}
					else if	(Transaction.transactionState == SKPaymentTransactionStateRestored)
					{
						Result.Data.IntValue = MTR_RestoredFromServer;
					}
					else
					{
						// a canceled transaction returns as SKPaymentTransactionStateFailed, but handle
						// it specially
						if (Transaction.error.code == SKErrorPaymentCancelled)
						{
							Result.Data.IntValue = MTR_Canceled;
						}
						else
						{
							Result.Data.IntValue = MTR_Failed;

							// fill out the error strings
							MicroTrans->LastError = FString([Transaction.error localizedFailureReason]);
							MicroTrans->LastErrorSolution = FString([Transaction.error localizedRecoverySuggestion]);


/*

	// Put afterwards incase code does not handle error in app.	
	if (-1009 == iErrorCode)
	{
		NSString *messageToBeShown = [NSString stringWithFormat :@"Please check your internet connection"];
		UIAlertView *alert = [[UIAlertView alloc] initWithTitle :@"Unable to connect to the App Store" message:messageToBeShown delegate:self cancelButtonTitle:@"OK" otherButtonTitles:nil];
		[alert show];
		[alert release];
	}
	else if (0 == iErrorCode)
	{
		NSString *messageToBeShown = [NSString stringWithFormat :@"Error, please try again later"];
		UIAlertView *alert = [[UIAlertView alloc] initWithTitle :@"Unable to connect to the App Store" message:messageToBeShown delegate:self cancelButtonTitle:@"OK" otherButtonTitles:nil];
		[alert show];
		[alert release];
	}
	else
	{
		NSString *messageToBeShown = [NSString stringWithFormat :@"Reason: %@, You can try: %@", [transaction.error localizedFailureReason], [transaction.error localizedRecoverySuggestion]];
		UIAlertView *alert = [[UIAlertView alloc] initWithTitle :@"Unable to complete your purchase" message:messageToBeShown delegate:self cancelButtonTitle:@"OK" otherButtonTitles:nil];
		[alert show];
		[alert release];
	}
*/

						}
					}

					// now pass the result to script
					UPlatformInterfaceBase::GetMicroTransactionInterfaceSingleton()->CallDelegates(MTD_PurchaseComplete, Result);

					// tell the payment system that we have processed this transaction
					[[SKPaymentQueue defaultQueue] finishTransaction:Transaction];	

					// someone is listening, so we can remove this transaction from being processed (ie, remove the asynctask from the task list)
					return TRUE;
				}

				// if there weren't any listeners, we need to keep ticking until there are
				return FALSE;

			};

		[AsyncTask FinishedTask];
	}
}

@end








IMPLEMENT_CLASS(UMicroTransactionIPhone);

void AutoInitializeRegistrantsIPhoneMicroTransaction( INT& Lookup )
{
	UMicroTransactionIPhone::StaticClass();
}


#pragma mark
#pragma mark Base 64 encoding


const char* Base64Map = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char* base64_encode(const void* buf, size_t size)
{
	// this will autorelease itself later
	NSMutableData* EncodedData = [NSMutableData dataWithLength:(size + 1) * 4 + 1];
	char* Encoded = (char*)[EncodedData bytes];

	char* s = (char*)buf;
	
	int i=0, j=0;
	int x,y,z;

	while (i < size)
	{
		x = (int) s[i];
		y = (i < size-1 ) ? (int) s[i+1] : 0;
		z = (i < size-2 ) ? (int) s[i+2] : 0;
		Encoded[j++] = Base64Map[x >> 2];
		Encoded[j++] = Base64Map[((x & 3) << 4) | (y >> 4)];
		Encoded[j++] = Base64Map[((y & 15) << 2) | (z >> 6)];
		Encoded[j++] = Base64Map[(z & 63)];
		i+=3;
	}

	switch (size % 3)
	{
	case 1:
		Encoded[j-2] = '=';
	case 2:
		Encoded[j-1] = '=';
	}

	Encoded[j] = '\0';

	// we can return this pointer since the holder NSMutableData will be autoreleased later on
 	return Encoded;
}

char* base64_decode(const char* s, size_t * data_len)
{
	// this will autorelease itself later
	int length = strlen(s);
	if (data_len)
	{
		*data_len = length / 4 * 3;
	}
	NSMutableData* DecodedData = [NSMutableData dataWithLength:(length / 4 * 3 + 1)];
	
	// get the data out for direct access (overwrite)
	char* Decoded = (char*)[DecodedData bytes];
	
	int ch, i=0, j=0;
	char* Current = (char*)s;

    while((ch = (int)(*Current++)) != '\0')
	{
		if (ch == '=')
			break;

		char* MappedChar = strchr(Base64Map, ch);
		if( MappedChar == NULL )
		{
			return NULL;
		}
		// convert to index
		ch = MappedChar - Base64Map;

		switch(i % 4) {
		case 0:
			Decoded[j] = ch << 2;
			break;
		case 1:
			Decoded[j++] |= ch >> 4;
			Decoded[j] = (ch & 0x0f) << 4;
			break;
		case 2:
			Decoded[j++] |= ch >>2;
			Decoded[j] = (ch & 0x03) << 6;
			break;
		case 3:
			Decoded[j++] |= ch;
			break;
		}
		i++;
	}

    /* clean up if we ended on a boundary */
    if (ch == '=') 
	{
		switch(i % 4)
		{
		case 0:
		case 1:
			return NULL;
		case 2:
			j++;
		case 3:
			Decoded[j++] = 0;
		}
	}
	Decoded[j] = '\0';	
	
	// we can return this pointer since the holder NSMutableData will be autoreleased later on
	return Decoded;	
}
