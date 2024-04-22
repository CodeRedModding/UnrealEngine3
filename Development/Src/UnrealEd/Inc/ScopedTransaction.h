/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SCOPEDTRANSACTION_H__
#define __SCOPEDTRANSACTION_H__

/**
 * Delineates a transactable block; Begin()s a transaction when entering scope,
 * and End()s a transaction when leaving scope.
 */
class FScopedTransaction
{
public:
	FScopedTransaction(const TCHAR* SessionName);
	~FScopedTransaction();

	/**
	 * Cancels the transaction.  Reentrant.
	 */
	void Cancel();

	/**
	 * @return	TRUE if the transaction is still outstanding (that is, has not been cancelled).
	 */
	UBOOL IsOutstanding() const;

private:
	/** Stores the transaction index, so that the transaction can be cancelled. */
	INT Index;
};

#endif // __SCOPEDTRANSACTION_H__
