/*=============================================================================
MicroTransactionProxy.cpp: The MicroTransactionProxy class provides a PC side stand 
in for the iPhone proxy class, it accesses micro transaction data from the inifile 
so we can mimic the data we would receive from the app store normally.

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineGameEngineClasses.h"
#include "EnginePlatformInterfaceClasses.h"
#include "AES.h"
#include "MicroTransactionProxy.h"

/*******************************************
 * Microtransactions                       *
 *******************************************/

/**
 * Perform any initialization
 */
void UMicroTransactionProxy::Init()
{
	TArray<FString> ProductIDs;
	TArray<FString> DisplayNames;
	TArray<FString> DisplayDescriptions;
	TArray<FString> DisplayPrices;

	GConfig->GetArray(TEXT("Engine.MicroTransactionInfo"),	TEXT("ProductIDs"),		ProductIDs,				GEngineIni);
	GConfig->GetArray(TEXT("Engine.MicroTransactionInfo"),	TEXT("DisplayNames"),	DisplayNames,			GEngineIni);
	GConfig->GetArray(TEXT("Engine.MicroTransactionInfo"),	TEXT("Descriptions"),	DisplayDescriptions,	GEngineIni);
	GConfig->GetArray(TEXT("Engine.MicroTransactionInfo"),  TEXT("DisplayPrices"),	DisplayPrices,			GEngineIni);

	for (INT ProductIndex = 0; ProductIndex < ProductIDs.Num(); ProductIndex++)
	{
		FPurchaseInfo Info(EC_EventParm); 

		Info.Identifier = ProductIDs(ProductIndex);
		Info.DisplayName = DisplayNames(ProductIndex);
		Info.DisplayDescription = DisplayDescriptions(ProductIndex);
		Info.DisplayPrice = DisplayPrices(ProductIndex);

		AvailableProducts.AddItem(Info);
	}	
}


/**
 * Query system for what purchases are available. Will fire a MTD_PurchaseQueryComplete
 * if this function returns true.
 *
 * @return True if the query started successfully (delegate will receive final result)
 */
UBOOL UMicroTransactionProxy::QueryForAvailablePurchases()
{
	return TRUE;
}

/**
 * @return True if the user is allowed to make purchases - should give a nice message if not
 */
UBOOL UMicroTransactionProxy::IsAllowedToMakePurchases()
{
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
UBOOL UMicroTransactionProxy::BeginPurchase(INT Index)
{
	//This is what success looks like
	FPlatformInterfaceDelegateResult Result(EC_EventParm);
	
	Result.bSuccessful = TRUE;	
	Result.Data.Type = PIDT_Custom;	
	Result.Data.StringValue = AvailableProducts(Index).Identifier;
	Result.Data.StringValue2 = "PCBuild"; 
	Result.Data.IntValue = MTR_Succeeded;

	CallDelegates(MTD_PurchaseComplete, Result);

	return TRUE;
}


IMPLEMENT_CLASS(UMicroTransactionProxy);

void AutoInitializeRegistrantsMicroTransactionProxy( INT& Lookup )
{
	UMicroTransactionProxy::StaticClass();
}

