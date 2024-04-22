/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "ScopedTransaction.h"

FScopedTransaction::FScopedTransaction(const TCHAR* SessionName)
{
	Index = GEditor->BeginTransaction( SessionName );
	check( IsOutstanding() );
}

FScopedTransaction::~FScopedTransaction()
{
	if ( IsOutstanding() )
	{
		GEditor->EndTransaction();
	}
}

/**
 * Cancels the transaction.  Reentrant.
 */
void FScopedTransaction::Cancel()
{
	if ( IsOutstanding() )
	{
		GEditor->CancelTransaction( Index );
		Index = -1;
	}
}

/**
 * @return	TRUE if the transaction is still outstanding (that is, has not been cancelled).
 */
UBOOL FScopedTransaction::IsOutstanding() const
{
	return Index >= 0;
}
