/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __UTILS_H__
#define __UTILS_H__

namespace NSwarm
{
	/**
	 * A thread-safe reader-writer-locked Dictionary wrapper
	 */
	generic<typename TKey, typename TValue> ref class ReaderWriterDictionary
	{
	private:
		/**
		 * The key to protecting the contained dictionary properly
		 */
		ReaderWriterLock^ AccessLock;

		/**
		 * The protected dictionary
		 */
		Dictionary<TKey, TValue>^ ProtectedDictionary;

	public:
		ReaderWriterDictionary( void )
		{
			AccessLock = gcnew ReaderWriterLock();
			ProtectedDictionary = gcnew Dictionary<TKey, TValue>();
		}

		/**
		 * Wrappers around each method we need to protect in the dictionary
		 */
		void Add( TKey Key, TValue Value )
		{
			// Modifies the collection, use a writer lock
			AccessLock->AcquireWriterLock( Timeout::Infinite );
			try
			{
				ProtectedDictionary->Add( Key, Value );
			}
			finally
			{
				AccessLock->ReleaseWriterLock();
			}
		}

		bool Remove( TKey Key )
		{
			// Modifies the collection, use a writer lock
			AccessLock->AcquireWriterLock( Timeout::Infinite );
			bool ReturnValue = false;
			try
			{
				ReturnValue = ProtectedDictionary->Remove( Key );
			}
			finally
			{
				AccessLock->ReleaseWriterLock();
			}
			return ReturnValue;
		}

		void Clear()
		{
			// Modifies the collection, use a writer lock
			AccessLock->AcquireWriterLock( Timeout::Infinite );
			try
			{
				ProtectedDictionary->Clear();
			}
			finally
			{
				AccessLock->ReleaseWriterLock();
			}
		}

		bool TryGetValue( TKey Key, TValue% Value )
		{
			// Does not modify the collection, use a reader lock
			AccessLock->AcquireReaderLock( Timeout::Infinite );
			bool ReturnValue = false;
			try
			{
				ReturnValue = ProtectedDictionary->TryGetValue( Key, Value );
			}
			finally
			{
				AccessLock->ReleaseReaderLock();
			}
			return ReturnValue;
		}

		property Dictionary<TKey, TValue>::ValueCollection^ Values
		{
			Dictionary<TKey, TValue>::ValueCollection^ get()
			{
				AccessLock->AcquireReaderLock( Timeout::Infinite );
				Dictionary<TKey, TValue>::ValueCollection^ CopyOfValues = nullptr;
				try
				{
					CopyOfValues = gcnew Dictionary<TKey, TValue>::ValueCollection( ProtectedDictionary );
				}
				finally
				{
					AccessLock->ReleaseReaderLock();
				}

				return CopyOfValues;
			}
		}

		property int Count
		{
			int get()
			{
				// Does not modify the collection, use a reader lock
				AccessLock->AcquireReaderLock( Timeout::Infinite );
				int ReturnValue = 0;
				try
				{
					ReturnValue = ProtectedDictionary->Count;
				}
				finally
				{
					AccessLock->ReleaseReaderLock();
				}
				return ReturnValue;
			}
		}
	};
}

#endif // __UTILS_H__
