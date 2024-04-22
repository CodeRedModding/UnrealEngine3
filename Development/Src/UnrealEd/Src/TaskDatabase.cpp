/*=============================================================================
	TaskDatabase.cpp: Implements support for communicating with a task database
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "TaskDatabase.h"
#include "TaskDatabaseThread.h"


#if WITH_TESTTRACK
	#include "TestTrackTaskDatabaseProvider.h"
#endif	// WITH_TESTTRACK



namespace TaskDatabaseSystem
{

	/** Global threaded task database singleton object */
	FTaskDatabaseThreadRunnable* GTaskDatabaseThreadRunnable = NULL;

	/** The actual task database thread object */
	FRunnableThread* GTaskDatabaseThread = NULL;

	/** True if a request has been sent and we're waiting for a response */
	UBOOL GIsWaitingForResponse = FALSE;

	/** True if we're currently connected to the database (as far as we know) */
	UBOOL GIsConnected = FALSE;

	/** Array of main-thread 'listeners' that registered to hear about completed requests */
	TArray< FTaskDatabaseListener* > GListeners;





	/**
	 * Initialize the task database system.  This must be called before anything else.
	 *
	 * @return	True if successful
	 */
	UBOOL Init()
	{
		TDLOG( TEXT( "Initializing Task Database" ) );

		if( GTaskDatabaseThreadRunnable != NULL )
		{
			// Already initialized?
			return FALSE;
		}

		
		// We're not connected yet!
		GIsConnected = FALSE;

		

		// Create and initialize the task database provider
		FTaskDatabaseProviderInterface* TaskDatabaseProvider = NULL;

#if WITH_TESTTRACK
		TaskDatabaseProvider = FTestTrackProvider::CreateTestTrackProvider();
#endif	// WITH_TESTTRACK

		if( TaskDatabaseProvider == NULL )
		{
			// Failed to initialized task database provider
			return FALSE;
		}

		// Allocate threaded task database
		GTaskDatabaseThreadRunnable =
			new FTaskDatabaseThreadRunnable( TaskDatabaseProvider );
		check( GTaskDatabaseThreadRunnable != NULL );

		
		// No requests have been issued yet
		GIsWaitingForResponse = FALSE;



		// Select thread priority 
		// @todo: Consider using TPri_Low here since task database ops shouldn't require much CPU
		EThreadPriority TaskDatabaseThreadPriority = TPri_Normal;

		
		TDLOG( TEXT( "Starting Task Database Thread" ) );

		// Launch the task database thread!
		GTaskDatabaseThread = GThreadFactory->CreateThread(
			GTaskDatabaseThreadRunnable,		// Runnable module
			TEXT( "TaskDatabaseThread" ),		// Thread name
			FALSE,								// Auto-delete self?
			FALSE,								// Auto-delete runnable?
			0,									// Stack size
			TaskDatabaseThreadPriority );		// Thread priority

		if( GTaskDatabaseThread == NULL )
		{
			// Failed to launch the thread
			delete GTaskDatabaseThreadRunnable;
			GTaskDatabaseThreadRunnable = NULL;

			return FALSE;
		}


		// Suspend the task worker thread until there is actually work to do
		GTaskDatabaseThreadRunnable->bThreadSafeShouldSuspend = TRUE;
		// GTaskDatabaseThread->Suspend( TRUE );

		TDLOG( TEXT( "Task Database initialized" ) );
		
		return TRUE;
	}



	/** Process any responses that are outstanding and fire off callback events */
	void ProcessCompletedRequests()
	{
		if( GIsWaitingForResponse )
		{
			FTaskDatabaseResponse* ResponseToBroadcast = NULL;

			{
				FScopeLock ScopeLock( &GTaskDatabaseThreadRunnable->CriticalSection );

				// Any requests pending?
				if( GTaskDatabaseThreadRunnable->ThreadSafeResponse != NULL )
				{
					// There should never be another request already enqueued.  We delete and clear the
					// request data as soon as the response is generated.
					check( GTaskDatabaseThreadRunnable->ThreadSafeCurrentRequest == NULL );

					// OK, we've received a response to our last request!
					ResponseToBroadcast = GTaskDatabaseThreadRunnable->ThreadSafeResponse;

					// Tell the task database thread it can start working on other tasks now
					GTaskDatabaseThreadRunnable->ThreadSafeResponse = NULL;

					// No longer waiting for anything
					GIsWaitingForResponse = FALSE;
				}
			}

			if( ResponseToBroadcast != NULL )
			{
				// We completed our task, so go back to sleep
				GTaskDatabaseThreadRunnable->bThreadSafeShouldSuspend = TRUE;
				// GTaskDatabaseThread->Suspend( TRUE );

				TDLOG( TEXT( "Broadcasting response for completed request (Type: %i)" ), ( INT )ResponseToBroadcast->RequestType );

				
				// Peek to see if this is a connection event, and update our status if so
				if( ResponseToBroadcast->RequestType == ETaskDatabaseRequestType::ConnectToDatabase )
				{
					// We assume we're disconnected if a 'connect to database' request fails
					GIsConnected = ResponseToBroadcast->bSucceeded;
				}
				else if( ResponseToBroadcast->RequestType == ETaskDatabaseRequestType::DisconnectFromDatabase )
				{
					// We assume we're no longer connected after completing a disconnect request
					GIsConnected = FALSE;
				}

				if( ResponseToBroadcast->bFailedBecauseOfDisconnection )
				{
					// We were disconnected unexpectedly!
					GIsConnected = FALSE;
				}

				// Broadcast callbacks for this response
				for( TArray< FTaskDatabaseListener* >::TIterator ListenerIter( GListeners ); ListenerIter != NULL; ++ListenerIter )
				{
					FTaskDatabaseListener* CurListener = *ListenerIter;
					check( CurListener != NULL );

					CurListener->OnTaskDatabaseRequestCompleted( ResponseToBroadcast );
				}

				// We're done with the response data now
				delete ResponseToBroadcast;
				ResponseToBroadcast = NULL;
			}
		}

	}



	/** Clean up the task database system.  Call this after before exiting the app. */
	void Destroy()
	{
		TDLOG( TEXT( "Destroying Task Database" ) );

		// Clear listener array
		GListeners.Empty();



		if( IsInitialized() )
		{
			TDLOG( TEXT( "Destroying: Waiting for current task to complete" ) );

			// We're probably trying to shutdown the editor, so never wait an innappropriate amount of time
			const FLOAT MaxTimeToWaitForThread = 5.0f;
			const DOUBLE TimeStartedWaiting = appSeconds();


			// Wait for the current task to complete
			GTaskDatabaseThreadRunnable->bThreadSafeShouldSuspend = FALSE;
			while( GIsWaitingForResponse && ( appSeconds() - TimeStartedWaiting ) < MaxTimeToWaitForThread )
			{
				ProcessCompletedRequests();
				appSleep( 0.01f );
			}

			// Are we connected?  If so, we'll log out before shutting down
			if( IsConnected() )
			{
				TDLOG( TEXT( "Destroying: Logging out of database" ) );

				// Initiate a log out before exiting
				DisconnectFromDatabase_Async();

				TDLOG( TEXT( "Destroying: Waiting for log out to complete" ) );

				// Wait for the current task to complete
				while( GIsWaitingForResponse && ( appSeconds() - TimeStartedWaiting ) < MaxTimeToWaitForThread )
				{
					ProcessCompletedRequests();
					appSleep( 0.01f );
				}

				TDLOG( TEXT( "Destroying: Ready to destroy" ) );
			}
		}


		// Stop the task database
		if( GTaskDatabaseThreadRunnable != NULL )
		{
			GTaskDatabaseThreadRunnable->bThreadSafeShouldSuspend = FALSE;
			GTaskDatabaseThreadRunnable->Stop();
		}

		// Kill the task database thread
		if( GTaskDatabaseThread != NULL )
		{
			// GTaskDatabaseThread->Suspend( FALSE );

			TDLOG( TEXT( "Waiting for Task Database thread to complete" ) );

			// NOTE: We shouldn't really have to wait long here since we already waited earlier
			GTaskDatabaseThread->WaitForCompletion();

			GThreadFactory->Destroy( GTaskDatabaseThread );

			GTaskDatabaseThread = NULL;
		}

		// Destroy the task database object
		if( GTaskDatabaseThreadRunnable != NULL )
		{
			delete GTaskDatabaseThreadRunnable;
			GTaskDatabaseThreadRunnable = NULL;
		}


		GIsConnected = FALSE;


		TDLOG( TEXT( "Task Database destroyed" ) );
	}



	/**
	 * Returns true if the task database system was initialized successfully
	 *
	 * @return	True if initialized
	 */
	UBOOL IsInitialized()
	{
		return ( GTaskDatabaseThreadRunnable != NULL );
	}



	/**
	 * Returns true if we're currently waiting for a response from the server
	 *
	 * @return	True if a request is in progress
	 */
	UBOOL IsRequestInProgress()
	{
		return GIsWaitingForResponse;
	}



	/**
	 * Returns true if we're currently connected to a database
	 *
	 * @return	True if currently connected
	 */
	UBOOL IsConnected()
	{
		return GIsConnected;
	}



	/**
	 * Kicks off an asynchronous request using the task database thread
	 *
	 * @param	Request		The request to start.  Assumes ownership of allocated memory.
	 *
	 * @return	True if successful
	 */
	UBOOL StartAsyncRequest( FTaskDatabaseRequest* Request )
	{
		check( Request != NULL );

		TDLOG( TEXT( "Starting async request (Type: %i)" ), ( INT )Request->RequestType );

		// First, process any outstanding request so that we're not holding up the queue
		// @todo: Should we do this here?  It's a bit risky because the frontend UI may not be expecting to
		//    receive callback events while in a 'start request' stack frame.  Maybe just leave this to Update()
		ProcessCompletedRequests();

		if( GIsWaitingForResponse )
		{
			// Another request is already in progress.  Can't start this request yet.
			delete Request;
			Request = NULL;

			return FALSE;
		}


		{
			FScopeLock ScopeLock( &GTaskDatabaseThreadRunnable->CriticalSection );

			// Current request should always be NULL when GIsWaitingForResponse is FALSE
			check( GTaskDatabaseThreadRunnable->ThreadSafeCurrentRequest == NULL );

			// Start the request
			GTaskDatabaseThreadRunnable->ThreadSafeCurrentRequest = Request;
		}

		// We're waiting for a response now
		GIsWaitingForResponse = TRUE;

		// Wake the thread up to start working
		GTaskDatabaseThreadRunnable->bThreadSafeShouldSuspend = FALSE;
		// GTaskDatabaseThread->Suspend( FALSE );

		return TRUE;
	}



	/** Updates the task database and checks for any completed requests, firing callbacks if needed.  This
	    should usually be called with every engine Tick. */
	void Update()
	{
		// Process and requests that may have completed and fire callbacks
		ProcessCompletedRequests();
	}



	/**
	 * Adds the specified object as a 'listener' for completed task database requests.  Remember to remove
	 * the object using UnregisterListener() before it's deleted!
	 *
	 * @param	Listener	The object to register.  Should not be NULL.
	 */
	void RegisterListener( FTaskDatabaseListener* Listener )
	{
		check( Listener != NULL );

		TDLOG( TEXT( "Listener registered (Pointer: %i)" ), ( INT )Listener );

		GListeners.AddUniqueItem( Listener );
	}



	/**
	 * Removes the specified object from the list of 'listeners'
	 *
	 * @param	Listener	The object to unregister.  Should have been previously registered.
	 */
	void UnregisterListener( FTaskDatabaseListener* Listener )
	{
		check( Listener != NULL )
			
		TDLOG( TEXT( "Listener unregistered (Pointer: %i)" ), ( INT )Listener );

		GListeners.RemoveItemSwap( Listener );
	}



	/**
	 * Initiates asynchronous query for available task databases
	 *
	 * @param	InServerName		Server host name
	 * @param	InServerPort		Server port
	 * @param	InLoginUserName		User name
	 * @param	InLoginPassword		Password
	 *
	 * @return	True if async requested was kicked off successfully
	 */
	UBOOL QueryAvailableDatabases_Async( const FString& InServerName, const UINT InServerPort, const FString& InLoginUserName, const FString& InLoginPassword )
	{
		if( IsConnected() )
		{
			TDLOG( TEXT( "Async request failed because we're already connected to a server" ) );
			return FALSE;
		}

		// Build a server URL string (e.g. "http://myservername:80")
		FString ServerURL =
			FString( TEXT( "http://" ) ) +
			InServerName +
			FString( TEXT( ":" ) ) +
			FString( appItoa( InServerPort ) );

		FTaskDatabaseRequest_QueryAvailableDatabases* Request = new FTaskDatabaseRequest_QueryAvailableDatabases();
		Request->RequestType = ETaskDatabaseRequestType::QueryAvailableDatabases;
		Request->ServerURL = ServerURL;
		Request->LoginUserName = InLoginUserName;
		Request->LoginPassword = InLoginPassword;

		return StartAsyncRequest( Request );
	}



	/**
	 * Asynchronously connects to the task database and attempts to login to a specific database
	 *
	 * @param	InServerName		Server host name
	 * @param	InServerPort		Server port
	 * @param	InLoginUserName		User name
	 * @param	InLoginPassword		Password
	 * @param	InDatabaeName		The database to login to
	 *
	 * @return	True if async requested was kicked off successfully
	 */
	UBOOL ConnectToDatabase_Async( const FString& InServerName, const UINT InServerPort, const FString& InLoginUserName, const FString& InLoginPassword, const FString& InDatabaseName )
	{
		if( IsConnected() )
		{
			TDLOG( TEXT( "Async request failed because we're already connected to a server" ) );
			return FALSE;
		}

		// Build a server URL string (e.g. "http://myservername:80")
		FString ServerURL =
			FString( TEXT( "http://" ) ) +
			InServerName +
			FString( TEXT( ":" ) ) +
			FString( appItoa( InServerPort ) );

		FTaskDatabaseRequest_ConnectToDatabase* Request = new FTaskDatabaseRequest_ConnectToDatabase();
		Request->RequestType = ETaskDatabaseRequestType::ConnectToDatabase;
		Request->ServerURL = ServerURL;
		Request->LoginUserName = InLoginUserName;
		Request->LoginPassword = InLoginPassword;
		Request->DatabaseName = InDatabaseName;

		return StartAsyncRequest( Request );
	}



	/**
	 * Disconnects from the database asynchronously
	 *
	 * @return	True if async requested was kicked off successfully
	 */
	UBOOL DisconnectFromDatabase_Async()
	{
		if( !IsConnected() )
		{
			TDLOG( TEXT( "Async request failed because we'not connected to a server" ) );
			return FALSE;
		}

		FTaskDatabaseRequest* Request = new FTaskDatabaseRequest();
		Request->RequestType = ETaskDatabaseRequestType::DisconnectFromDatabase;

		return StartAsyncRequest( Request );
	}



	/**
	 * Asynchronously queries a list of available database filters
	 *
	 * @return	True if async requested was kicked off successfully
	 */
	UBOOL QueryFilters_Async()
	{
		if( !IsConnected() )
		{
			TDLOG( TEXT( "Async request failed because we'not connected to a server" ) );
			return FALSE;
		}

		FTaskDatabaseRequest* Request = new FTaskDatabaseRequest();
		Request->RequestType = ETaskDatabaseRequestType::QueryFilters;

		return StartAsyncRequest( Request );
	}



	/**
	 * Asynchronously queries a list of task entries for the specified filter name
	 *
	 * @param	InFilterName		Name of the filter (can be empty string)
	 *
	 * @return	True if async requested was kicked off successfully
	 */
	UBOOL QueryTasks_Async( const FString& InFilterName )
	{
		if( !IsConnected() )
		{
			TDLOG( TEXT( "Async request failed because we'not connected to a server" ) );
			return FALSE;
		}

		FTaskDatabaseRequest_QueryTasks* Request = new FTaskDatabaseRequest_QueryTasks();
		Request->RequestType = ETaskDatabaseRequestType::QueryTasks;
		Request->FilterName = InFilterName;

		return StartAsyncRequest( Request );
	}



	/**
	 * Asynchronously queries details about a single specific task entry
	 *
	 * @param	InTaskNumber		The database task number for the task entry
	 *
	 * @return	True if async requested was kicked off successfully
	 */
	UBOOL QueryTaskDetails_Async( const UINT InTaskNumber )
	{
		if( !IsConnected() )
		{
			TDLOG( TEXT( "Async request failed because we'not connected to a server" ) );
			return FALSE;
		}

		FTaskDatabaseRequest_QueryTaskDetails* Request = new FTaskDatabaseRequest_QueryTaskDetails();
		Request->RequestType = ETaskDatabaseRequestType::QueryTaskDetails;
		Request->TaskNumber = InTaskNumber;

		return StartAsyncRequest( Request );
	}



	/**
	 * Attempts to asynchronously mark the specified task as completed
	 *
	 * @param	InTaskNumber		The database task number to mark complete
	 * @param	InResolutionData	Information about how the task should be resolved
	 *
	 * @return	True if async requested was kicked off successfully
	 */
	UBOOL MarkTaskComplete_Async( const UINT InTaskNumber, const FTaskResolutionData& InResolutionData )
	{
		if( !IsConnected() )
		{
			TDLOG( TEXT( "Async request failed because we'not connected to a server" ) );
			return FALSE;
		}

		FTaskDatabaseRequest_MarkTaskComplete* Request = new FTaskDatabaseRequest_MarkTaskComplete();
		Request->RequestType = ETaskDatabaseRequestType::MarkTaskComplete;
		Request->TaskNumber = InTaskNumber;
		Request->ResolutionData = InResolutionData;

		return StartAsyncRequest( Request );
	}

}


