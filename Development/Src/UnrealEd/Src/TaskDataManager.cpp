/*=============================================================================
	TaskDataManager.cpp: Data management layer between task GUI system and database connection
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "TaskDataManager.h"
#include "TaskDatabase.h"



/** Implements FString sorting */
IMPLEMENT_COMPARE_CONSTREF( FString, TaskDataManager, { return appStricmp( *A, *B ); } );


/**
 * FTaskDataManager constructor
 *
 * @param	InGUICallbackObject		Object that we'll call back to for GUI interop
 */
FTaskDataManager::FTaskDataManager( FTaskDataGUIInterface* InGUICallbackObject )
	: GUICallbackObject( InGUICallbackObject ),
	  ConnectionSettings(),
	  UserRealName(),
	  ResolutionValues(),
	  OpenTaskStatusPrefix(),
	  ConnectionStatus( ETaskDataManagerStatus::Unknown ),
	  QueryStatus( ETaskDataManagerStatus::Unknown ),
	  LastGUIStatus( ETaskDataManagerStatus::Unknown ),
	  bShouldRefreshFilterNames( FALSE ),
	  ActiveFilterName(),
	  FocusedTaskNumber( INDEX_NONE ),
	  TaskNumbersToMarkComplete(),
	  ResolutionData(),
	  CurrentlyProcessingTaskNumber( INDEX_NONE ),
	  CachedFilterNames(),
	  CachedTaskArray(),
	  CachedLastTaskArrayFilterName(),
	  CachedTaskDetailsMap()
{
	// Initialize the task database system if we need to
	if( !TaskDatabaseSystem::IsInitialized() )
	{
		// Initialize task database
		if( !TaskDatabaseSystem::Init() )
		{
			// Failed to init task database.  Oh well, these features simply won't be available here on out.
			ConnectionStatus = ETaskDataManagerStatus::FailedToInit;
		}
	}

	if( TaskDatabaseSystem::IsInitialized() )
	{
		// Register as a listener
		TaskDatabaseSystem::RegisterListener( this );

		// Task database was initialized successfully.  We're ready to connect now!
		ConnectionStatus = ETaskDataManagerStatus::ReadyToConnect;
	}
}



/** FTaskDataManager destructor */
FTaskDataManager::~FTaskDataManager()
{
	if( TaskDatabaseSystem::IsInitialized() )
	{
		// Unregister as a listener
		TaskDatabaseSystem::UnregisterListener( this );

		// Shutdown the task database.  This may take a few moments as we wait for the user to be logged
		// out from the server and the threads to join.
		TaskDatabaseSystem::Destroy();
	}
}



/**
 * Called from within UnLevTic.cpp after ticking all actors or from
 * the rendering thread (depending on bIsRenderingThreadObject)
 *
 * @param DeltaTime	Game time passed since the last call.
 */
void FTaskDataManager::Tick( FLOAT DeltaTime )
{
	// Update self
	UpdateTaskDataManager();

	if( TaskDatabaseSystem::IsInitialized() )
	{
		// Update the task database.  This may fire off events after server responses are received.
		TaskDatabaseSystem::Update();
	}
}



/**
 * Starts process of marking the specified task numbers as completed
 *
 * @param	InTaskNumbers		Numbers of tasks to mark as completed
 * @param	InResolutionData	User-entered resolution data for these tasks
 */
void FTaskDataManager::StartMarkingTasksComplete( const TArray< UINT >& InTaskNumbers,
												  const FTaskResolutionData& InResolutionData )
{
	// Add the specified tasks to our list of tasks to complete.  We'll process these when we have time to.
	for( INT CurTaskIndex = 0; CurTaskIndex < InTaskNumbers.Num(); ++CurTaskIndex )
	{
		TaskNumbersToMarkComplete.AddUniqueItem( InTaskNumbers( CurTaskIndex ) );
	}

	// @todo: It's possible for resolution data to be changed here while tasks to complete are already queued

	// Store the resolution data
	ResolutionData = InResolutionData;
}




/** Initiates or re-initiates a connection to the task database server */
void FTaskDataManager::AttemptConnection()
{
	if( ConnectionStatus == ETaskDataManagerStatus::ReadyToConnect ||
		ConnectionStatus == ETaskDataManagerStatus::ConnectionFailed )
	{
		ConnectionStatus = ETaskDataManagerStatus::Connecting;
	}
}



/** Initiates a disconnection from the task database server */
void FTaskDataManager::AttemptDisconnection()
{
	if( ConnectionStatus != ETaskDataManagerStatus::FailedToInit &&
		ConnectionStatus != ETaskDataManagerStatus::ReadyToConnect &&
		ConnectionStatus != ETaskDataManagerStatus::ConnectionFailed )
	{
		ConnectionStatus = ETaskDataManagerStatus::Disconnecting;
	}
}



/** Starts a forced a refresh of task list/description data from the server */
void FTaskDataManager::ClearTaskDataAndInitiateRefresh()
{
	// @todo: Support pinging the server every so often to check for changes automatically?

	// Wipe out cached task details
	CachedTaskDetailsMap.Empty();

	// We'll clear the last filter name to force a full refresh of the task list.
	CachedLastTaskArrayFilterName = TEXT( "" );
}



/** Updates our state machine.  Should be called frequently (every Tick is OK) */
void FTaskDataManager::UpdateTaskDataManager()
{
	// If we're waiting for a request to finish, then don't bother sending new tasks
	if( TaskDatabaseSystem::IsInitialized() &&
		!TaskDatabaseSystem::IsRequestInProgress() )
	{
		if( !TaskDatabaseSystem::IsConnected() && ConnectionStatus == ETaskDataManagerStatus::Connecting )
		{
			// Go ahead and connect
			if( !TaskDatabaseSystem::ConnectToDatabase_Async(
					ConnectionSettings.ServerName,
					ConnectionSettings.ServerPort,
					ConnectionSettings.UserName,
					ConnectionSettings.Password,
					ConnectionSettings.ProjectName ) )
			{
				// Failed to initiate connection!
				ConnectionStatus = ETaskDataManagerStatus::ConnectionFailed;

				appMsgf( AMT_OK, *LocalizeUnrealEd( TEXT( "TaskBrowser_Error_StartingConnect" ) ) );
			}
		}

		if( TaskDatabaseSystem::IsConnected() )
		{
			// If we were asked to disconnect, then do that now
			if( ConnectionStatus == ETaskDataManagerStatus::Disconnecting )
			{
				if( !TaskDatabaseSystem::DisconnectFromDatabase_Async() )
				{
					// Failed to initiate disconnection!  Should never happen, but not a big deal.
					appMsgf( AMT_OK, *LocalizeUnrealEd( TEXT( "TaskBrowser_Error_StartingDisconnect" ) ) );
				}
			}


			// OK, we're connected and ready for work!
			if( bShouldRefreshFilterNames )
			{
				// Query filters
				if( TaskDatabaseSystem::QueryFilters_Async() )
				{
					QueryStatus = ETaskDataManagerStatus::QueryingFilters;
				}
				else
				{
					// Failed to initiate query for filters!  Should never happen, but not a big deal.
					appMsgf( AMT_OK, *LocalizeUnrealEd( TEXT( "TaskBrowser_Error_StartingFilterQuery" ) ) );
				}

				bShouldRefreshFilterNames = FALSE;
			}
			// Have we been asked to mark any tasks as completed?
			else if( TaskNumbersToMarkComplete.Num() > 0 )
			{
				// Pop the first task off the list and mark it as completed
				const UINT TaskNumber = TaskNumbersToMarkComplete( 0 );
				TaskNumbersToMarkComplete.Remove( 0 );

				// Keep track of the task number we're working on
				CurrentlyProcessingTaskNumber = TaskNumber;

				if( TaskDatabaseSystem::MarkTaskComplete_Async( TaskNumber, ResolutionData ) )
				{
					QueryStatus = ETaskDataManagerStatus::MarkingTaskComplete;
				}
				else
				{
					// Failed to initiate marking task as complete.  Should never happen.
					appMsgf( AMT_OK, *LocalizeUnrealEd( TEXT( "TaskBrowser_Error_StartingMarkAsComplete" ) ) );
				}
			}
			else if( ActiveFilterName != CachedLastTaskArrayFilterName )	// Has filter name changed?
			{
				if( ActiveFilterName.Len() > 0 )
				{
					// Query some tasks!
					if( TaskDatabaseSystem::QueryTasks_Async( ActiveFilterName ) )
					{
						QueryStatus = ETaskDataManagerStatus::QueryingTasks;

						// Store filter name these tasks were queried with so we'll know later that the
						// tasks don't need to be retrieved again
						CachedLastTaskArrayFilterName = ActiveFilterName;
					}
					else
					{
						// Failed to initiate query for tasks!
						appMsgf( AMT_OK, *LocalizeUnrealEd( TEXT( "TaskBrowser_Error_StartingQueryTasks" ) ) );
					}
				}
			}
			else
			{
				// Do we have a task number to retrieve details for?
				if( FocusedTaskNumber != INDEX_NONE )
				{
					if( FindCachedTaskDetails( FocusedTaskNumber ) == NULL )
					{
						if( TaskDatabaseSystem::QueryTaskDetails_Async( FocusedTaskNumber ) )
						{
							QueryStatus = ETaskDataManagerStatus::QueryingTaskDetails;

							// Let the GUI know that we've started downloading a task description
							if( GUICallbackObject != NULL )
							{
								GUICallbackObject->Callback_RefreshGUI(
									ETaskBrowserGUIRefreshOptions::UpdateTaskDescription );
							}
						}
						else
						{
							// Failed to initiate query for task details
							appMsgf( AMT_OK, *LocalizeUnrealEd( TEXT( "TaskBrowser_Error_StartingQueryTaskDetails" ) ) );

							// Reset the focused task number so that we don't spam requests on error
							SetFocusedTaskNumber( INDEX_NONE );
						}
					}
				}
			}
		}
	}


	// Make sure the GUI status is up to date!
	{
		if( GetGUIStatus() != LastGUIStatus )
		{
			if( GUICallbackObject != NULL )
			{
				GUICallbackObject->Callback_RefreshGUI( ETaskBrowserGUIRefreshOptions::UpdateStatusMessage );
			}
			
			LastGUIStatus = GetGUIStatus();
		}
	}
}



/** We've been disconnected (either voluntarily or otherwise), so clean up state and refresh GUI */
void FTaskDataManager::CleanUpAfterDisconnect()
{
	// Clear filter names
	CachedFilterNames.Empty();

	// Clear task entry array
	CachedTaskArray.Empty();

	// Wipe out cached task details
	CachedTaskDetailsMap.Empty();

	// No longer focused on any task
	FocusedTaskNumber = INDEX_NONE;

	// Cancel any requests to mark tasks as completed
	TaskNumbersToMarkComplete.Empty();

	// No filters are active now
	ActiveFilterName = TEXT( "" );
	CachedLastTaskArrayFilterName = TEXT( "" );


	// Refresh the user interface
	if( GUICallbackObject != NULL )
	{
		GUICallbackObject->Callback_RefreshGUI( ( ETaskBrowserGUIRefreshOptions::Type )(
			ETaskBrowserGUIRefreshOptions::RebuildFilterList |
			ETaskBrowserGUIRefreshOptions::RebuildTaskList |
			ETaskBrowserGUIRefreshOptions::UpdateTaskDescription |
			ETaskBrowserGUIRefreshOptions::UpdateStatusMessage ) );
	}
}



/**
 * Called when a response is received from the task database
 *
 * @param	InGenericResponse	Response data for the completed request.  This is a polymorphic object
 *								that should be casted to the appropriate response type for the request.
 */
void FTaskDataManager::OnTaskDatabaseRequestCompleted( const FTaskDatabaseResponse* InGenericResponse )
{
	check( InGenericResponse != NULL );
	switch( InGenericResponse->RequestType )
	{
		case ETaskDatabaseRequestType::ConnectToDatabase:
			{
				const FTaskDatabaseResponse_ConnectToDatabase* Response =
					static_cast< const FTaskDatabaseResponse_ConnectToDatabase* >( InGenericResponse );

				if( Response->bSucceeded )
				{
					// We connected successfully!
					ConnectionStatus = ETaskDataManagerStatus::Connected;

					// Store the real name of the user if we were able to retrieve that
					UserRealName = Response->UserRealName;

					// Store resolution values (custom field values)
					ResolutionValues = Response->ResolutionValues;

					// Store name of 'Open' tasks
					OpenTaskStatusPrefix = Response->OpenTaskStatusPrefix;

					// We want to refresh filter names as soon as possible
					bShouldRefreshFilterNames = TRUE;
				}
				else
				{
					// We failed to connect to the server.  We don't want to bother trying again right now.
					ConnectionStatus = ETaskDataManagerStatus::ConnectionFailed;

					appMsgf( AMT_OK, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT( "TaskBrowser_Error_ConnectFailed_F" ) ), *Response->ErrorMessage ) ) );
				}
			}
			break;


		case ETaskDatabaseRequestType::DisconnectFromDatabase:
			{
				if( InGenericResponse->bSucceeded )
				{
					// We disconnected successfully!
					ConnectionStatus = ETaskDataManagerStatus::ReadyToConnect;

					// We're disconnected, so clear out state and update the GUI
					CleanUpAfterDisconnect();
				}
				else
				{
					// We failed to disconnect from the server.  Not sure where that leaves us.
					appMsgf( AMT_OK, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT( "TaskBrowser_Error_DisconnectFailed_F" ) ), *InGenericResponse->ErrorMessage ) ) );

					if( !TaskDatabaseSystem::IsConnected() )
					{
						// We were disconnected during the request, so try to handle that gracefully
						ConnectionStatus = ETaskDataManagerStatus::ConnectionFailed;
						CleanUpAfterDisconnect();
					}
				}
			}
			break;


		case ETaskDatabaseRequestType::QueryFilters:
			{
				const FTaskDatabaseResponse_QueryFilters* Response =
					static_cast< const FTaskDatabaseResponse_QueryFilters* >( InGenericResponse );

				if( Response->bSucceeded )
				{
					// Update our list of filters
					CachedFilterNames = Response->FilterNames;


					// Sort the list of filters alphabetically
					Sort< USE_COMPARE_CONSTREF( FString, TaskDataManager ) >( CachedFilterNames.GetTypedData(), CachedFilterNames.Num() );


					// Tell the GUI to update!
					if( GUICallbackObject != NULL )
					{
						GUICallbackObject->Callback_RefreshGUI( ETaskBrowserGUIRefreshOptions::RebuildFilterList );
					}
				}
				else
				{
					// Query failed
					appMsgf( AMT_OK, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT( "TaskBrowser_Error_QueryFiltersFailed_F" ) ), *Response->ErrorMessage ) ) );

					if( !TaskDatabaseSystem::IsConnected() )
					{
						// We were disconnected during the request, so try to handle that gracefully
						ConnectionStatus = ETaskDataManagerStatus::ConnectionFailed;
						CleanUpAfterDisconnect();
					}
				}

				QueryStatus = ETaskDataManagerStatus::Unknown;
			}
			break;


		case ETaskDatabaseRequestType::QueryTasks:
			{
				const FTaskDatabaseResponse_QueryTasks* Response =
					static_cast< const FTaskDatabaseResponse_QueryTasks* >( InGenericResponse );

				if( Response->bSucceeded )
				{
					// Update our list of tasks
					CachedTaskArray = Response->TaskEntries;


					// Tell the GUI to update!
					if( GUICallbackObject != NULL )
					{
						GUICallbackObject->Callback_RefreshGUI( ETaskBrowserGUIRefreshOptions::RebuildTaskList );
					}
				}
				else
				{
					// Query failed
					appMsgf( AMT_OK, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT( "TaskBrowser_Error_QueryTasksFailed_F" ) ), *Response->ErrorMessage ) ) );

					if( !TaskDatabaseSystem::IsConnected() )
					{
						// We were disconnected during the request, so try to handle that gracefully
						ConnectionStatus = ETaskDataManagerStatus::ConnectionFailed;
						CleanUpAfterDisconnect();
					}
				}

				QueryStatus = ETaskDataManagerStatus::Unknown;
			}
			break;


		case ETaskDatabaseRequestType::QueryTaskDetails:
			{
				const FTaskDatabaseResponse_QueryTaskDetails* Response =
					static_cast< const FTaskDatabaseResponse_QueryTaskDetails* >( InGenericResponse );

				if( Response->bSucceeded )
				{
					// Add/update our task cache
					CachedTaskDetailsMap.Set( Response->TaskEntryDetails.Number, Response->TaskEntryDetails );


					// Tell the GUI to update!
					if( GUICallbackObject != NULL )
					{
						GUICallbackObject->Callback_RefreshGUI(
							ETaskBrowserGUIRefreshOptions::UpdateTaskDescription );
					}
				}
				else
				{
					// Query failed
					appMsgf( AMT_OK, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT( "TaskBrowser_Error_QueryTaskDetailsFailed_F" ) ), *Response->ErrorMessage ) ) );

					// Invalidate our focused task number so that we won't spam requests
					SetFocusedTaskNumber( INDEX_NONE );

					if( !TaskDatabaseSystem::IsConnected() )
					{
						// We were disconnected during the request, so try to handle that gracefully
						ConnectionStatus = ETaskDataManagerStatus::ConnectionFailed;
						CleanUpAfterDisconnect();
					}
				}

				QueryStatus = ETaskDataManagerStatus::Unknown;
			}
			break;


		case ETaskDatabaseRequestType::MarkTaskComplete:
			{
				const FTaskDatabaseResponse* Response =
					static_cast< const FTaskDatabaseResponse* >( InGenericResponse );

				if( Response->bSucceeded )
				{
					// OK, the task was edited, so we'll remove our cached entry so that the details
					// will be refreshed.
					CachedTaskDetailsMap.Remove( CurrentlyProcessingTaskNumber );

					// Make sure the main task list gets refreshed, since the bug Status field was probably changed
					// We'll clear the last filter name to force a full refresh of the task list.
					// @todo: Improve perf/reduce flicker by only refreshing the task entry that we changed!
					CachedLastTaskArrayFilterName = TEXT( "" );
				}
				else
				{
					// Failed to mark task complete
					appMsgf( AMT_OK, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT( "TaskBrowser_Error_MarkCompleteFailed_F" ) ), *Response->ErrorMessage ) ) );

					if( !TaskDatabaseSystem::IsConnected() )
					{
						// We were disconnected during the request, so try to handle that gracefully
						ConnectionStatus = ETaskDataManagerStatus::ConnectionFailed;
						CleanUpAfterDisconnect();
					}
				}

				QueryStatus = ETaskDataManagerStatus::Unknown;
			}
			break;


		default:
			// Unrecognized response!
			break;			
	}
}
