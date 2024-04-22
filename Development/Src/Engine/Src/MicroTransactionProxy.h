/*=============================================================================
MicroTransactionProxy.cpp: The MicroTransactionProxy class provides a PC side stand 
in for the iPhone proxy class, it accesses micro transaction data from the inifile 
so we can mimic the data we would receive from the app store normally.

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EngineNames.h"


class UMicroTransactionProxy : public UMicroTransactionBase
{
public:
  
	DECLARE_CLASS_INTRINSIC(UMicroTransactionProxy, UMicroTransactionBase, 0, Engine)

    virtual void Init();
    virtual UBOOL QueryForAvailablePurchases();
    virtual UBOOL IsAllowedToMakePurchases();
    virtual UBOOL BeginPurchase(INT Index);

};

