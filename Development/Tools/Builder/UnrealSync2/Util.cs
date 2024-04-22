// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Threading;

namespace Builder.UnrealSync
{
	/**
	 * A thread-safe reader-writer-locked Queue wrapper
	 */
	public class ReaderWriterQueue<TValue>
	{
		/**
		 * The key to protecting the contained queue properly
		 */
		private ReaderWriterLock AccessLock = new ReaderWriterLock();

		/**
		 * The protected queue
		 */
		private Queue<TValue> ProtectedQueue = new Queue<TValue>();

		/**
		 * Wrappers around each method we need to protect in the queue
		 */
		public void Clear()
		{
			// Modifies the collection, use a writer lock
			AccessLock.AcquireWriterLock( Timeout.Infinite );
			try
			{
				ProtectedQueue.Clear();
			}
			finally
			{
				AccessLock.ReleaseWriterLock();
			}
		}

		public void Enqueue( TValue V )
		{
			// Modifies the collection, use a writer lock
			AccessLock.AcquireWriterLock( Timeout.Infinite );
			try
			{
				ProtectedQueue.Enqueue( V );
			}
			finally
			{
				AccessLock.ReleaseWriterLock();
			}
		}

		public TValue Dequeue()
		{
			// Modifies the collection, use a writer lock
			AccessLock.AcquireWriterLock( Timeout.Infinite );
			TValue V = default( TValue );
			try
			{
				V = ProtectedQueue.Dequeue();
			}
			finally
			{
				AccessLock.ReleaseWriterLock();
			}

			return ( V );
		}

		public TValue[] ToArray()
		{
			// Does not modify the collection, use a reader lock
			AccessLock.AcquireReaderLock( Timeout.Infinite );
			TValue[] ReturnValues;
			try
			{
				ReturnValues = ProtectedQueue.ToArray();
			}
			finally
			{
				AccessLock.ReleaseReaderLock();
			}
			return ReturnValues;
		}

		public int Count
		{
			get
			{
				// Does not modify the collection, use a reader lock
				AccessLock.AcquireReaderLock( Timeout.Infinite );
				int ReturnValue = 0;
				try
				{
					ReturnValue = ProtectedQueue.Count;
				}
				finally
				{
					AccessLock.ReleaseReaderLock();
				}
				return ReturnValue;
			}
		}
	}
}