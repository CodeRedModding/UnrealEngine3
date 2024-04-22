/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SCOPEDCALLBACK_H__
#define __SCOPEDCALLBACK_H__

/**
 * Helper object for batching callback requests and firing on destruction of the FScopedCallback object.
 * CallbackType is a class implementing a static method called FireCallback, which does the work.
 */
template< class CallbackType >
class TScopedCallback
{
public:
	TScopedCallback()
		:	Counter( 0 )
	{}

	/**
	 * Fires a callback if outstanding requests exist.
	 */
	~TScopedCallback()
	{
		if ( HasRequests() )
		{
			CallbackType::FireCallback();
		}
	}

	/**
	 * Request a callback.
	 */
	void Request()
	{
		++Counter;
	}

	/**
	 * Unrequest a callback.
	 */
	void Unrequest()
	{
		--Counter;
	}

	/**
	 * @return	TRUE if there are outstanding requests, FALSE otherwise.
	 */
	UBOOL HasRequests() const
	{
		return Counter > 0;
	}

private:
	/** Counts callback requests. */
	INT	Counter;
};

#endif // __SCOPEDCALLBACK_H__
