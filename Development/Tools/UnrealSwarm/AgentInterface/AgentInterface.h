/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#ifndef __AGENTINTERFACE_H__
#define __AGENTINTERFACE_H__

using namespace System;
using namespace System::Collections;
using namespace System::Collections::Generic;
using namespace System::Drawing;
using namespace System::Threading;

using namespace NSwarm;

namespace AgentInterface 
{
	/**
	 * Any globally special values used in Swarm
	 */
	[Serializable]
	public value class Constants
	{
	public:
		static const Int32 SUCCESS = SWARM_SUCCESS;
		static const Int32 INVALID = SWARM_INVALID;				
		static const Int32 ERROR_FILE_FOUND_NOT = SWARM_ERROR_FILE_FOUND_NOT;
		static const Int32 ERROR_NULL_POINTER = SWARM_ERROR_NULL_POINTER;
		static const Int32 ERROR_EXCEPTION = SWARM_ERROR_EXCEPTION;
		static const Int32 ERROR_INVALID_ARG = SWARM_ERROR_INVALID_ARG;
		static const Int32 ERROR_INVALID_ARG1 = SWARM_ERROR_INVALID_ARG1;
		static const Int32 ERROR_INVALID_ARG2 = SWARM_ERROR_INVALID_ARG2;
		static const Int32 ERROR_INVALID_ARG3 = SWARM_ERROR_INVALID_ARG3;
		static const Int32 ERROR_INVALID_ARG4 = SWARM_ERROR_INVALID_ARG4;
		static const Int32 ERROR_CHANNEL_NOT_FOUND = SWARM_ERROR_CHANNEL_NOT_FOUND;
		static const Int32 ERROR_CHANNEL_NOT_READY = SWARM_ERROR_CHANNEL_NOT_READY;
		static const Int32 ERROR_CHANNEL_IO_FAILED = SWARM_ERROR_CHANNEL_IO_FAILED;
		static const Int32 ERROR_CONNECTION_NOT_FOUND = SWARM_ERROR_CONNECTION_NOT_FOUND;
		static const Int32 ERROR_JOB_NOT_FOUND = SWARM_ERROR_JOB_NOT_FOUND;
		static const Int32 ERROR_JOB = SWARM_ERROR_JOB;
		static const Int32 ERROR_CONNECTION_DISCONNECTED = SWARM_ERROR_CONNECTION_DISCONNECTED;
	};

	/**
	 * Consistent version enum used by messages, Jobs, Tasks, etc.
	 */
	[Serializable]
	public enum class ESwarmVersionValue
	{
		INVALID						= VERSION_INVALID,
		VER_1_0						= VERSION_1_0,
	};

	/**
	 * Flags to determine the level of logging
	 */
	[Serializable]
	public enum class ELogFlags
	{
		LOG_NONE						= SWARM_LOG_NONE,		
		LOG_TIMINGS						= SWARM_LOG_TIMINGS,		
		LOG_CONNECTIONS					= SWARM_LOG_CONNECTIONS,
		LOG_CHANNELS					= SWARM_LOG_CHANNELS,	
		LOG_MESSAGES					= SWARM_LOG_MESSAGES,	
		LOG_JOBS						= SWARM_LOG_JOBS,		
		LOG_TASKS						= SWARM_LOG_TASKS,		
		LOG_ALL							= SWARM_LOG_ALL,		
	};

	/**
	 * The level of debug info spewed to the log files
	 */
	[Serializable]
	public enum class EVerbosityLevel
	{
		Silent							= VERBOSITY_Silent,
		Critical						= VERBOSITY_Critical,
		Simple							= VERBOSITY_Simple,
		Informative						= VERBOSITY_Informative,
		Complex							= VERBOSITY_Complex,
		Verbose							= VERBOSITY_Verbose,
		ExtraVerbose					= VERBOSITY_ExtraVerbose,
		SuperVerbose					= VERBOSITY_SuperVerbose
	};									 

	/**
	 * The current state of the lighting build process
	 */
	[Serializable]
	public enum class EProgressionState
	{
		TaskTotal						= PROGSTATE_TaskTotal,					
		TasksInProgress					= PROGSTATE_TasksInProgress,					
		TasksCompleted					= PROGSTATE_TasksCompleted,					
		Idle							= PROGSTATE_Idle,					
		InstigatorConnected				= PROGSTATE_InstigatorConnected,
		RemoteConnected					= PROGSTATE_RemoteConnected,
		Exporting						= PROGSTATE_Exporting,
		BeginJob						= PROGSTATE_BeginJob,
		Blocked							= PROGSTATE_Blocked,
		Preparing0						= PROGSTATE_Preparing0,
		Preparing1						= PROGSTATE_Preparing1,
		Preparing2						= PROGSTATE_Preparing2,
		Preparing3						= PROGSTATE_Preparing3,
		Processing0						= PROGSTATE_Processing0,
		FinishedProcessing0				= PROGSTATE_FinishedProcessing0,
		Processing1						= PROGSTATE_Processing1,
		FinishedProcessing1				= PROGSTATE_FinishedProcessing1,
		Processing2						= PROGSTATE_Processing2,
		FinishedProcessing2				= PROGSTATE_FinishedProcessing2,
		Processing3						= PROGSTATE_Processing3,
		FinishedProcessing3				= PROGSTATE_FinishedProcessing3,
		ExportingResults				= PROGSTATE_ExportingResults,
		ImportingResults				= PROGSTATE_ImportingResults,
		Finished						= PROGSTATE_Finished,
		RemoteDisconnected				= PROGSTATE_RemoteDisconnected,
		InstigatorDisconnected			= PROGSTATE_InstigatorDisconnected
	};

	/**
	 * Flags that define the intended behavior of the channel. The most
	 * important of which are whether the channel is read or write, and
	 * whether it's a general, persistent cache channel, or whether it's
	 * a job-specific channel. Additional misc flags are available as
	 * well.
	 */
	[Serializable]
	public enum class EChannelFlags
	{
		TYPE_PERSISTENT				= SWARM_CHANNEL_TYPE_PERSISTENT,
		TYPE_JOB_ONLY				= SWARM_CHANNEL_TYPE_JOB_ONLY,
		TYPE_MASK					= SWARM_CHANNEL_TYPE_MASK,

		ACCESS_READ					= SWARM_CHANNEL_ACCESS_READ,
		ACCESS_WRITE				= SWARM_CHANNEL_ACCESS_WRITE,
		ACCESS_MASK					= SWARM_CHANNEL_ACCESS_MASK,

		// Any additional flags for debugging or extended features
		MISC_ENABLE_PAPER_TRAIL		= SWARM_CHANNEL_MISC_ENABLE_PAPER_TRAIL,
		MISC_MASK					= SWARM_CHANNEL_MISC_MASK,
	};

	/**
	 * All the different types of messages the Swarm messaging interface supports
	 */
	[Serializable]
	public enum class EMessageType
	{
		NONE						= MESSAGE_NONE,
		INFO						= MESSAGE_INFO,
		ALERT						= MESSAGE_ALERT,
		TIMING						= MESSAGE_TIMING,
		PING						= MESSAGE_PING,
		SIGNAL						= MESSAGE_SIGNAL,

		/** Job messages */
		JOB_SPECIFICATION			= MESSAGE_JOB_SPECIFICATION,
		JOB_STATE					= MESSAGE_JOB_STATE,

		/** Task messages */
		TASK_REQUEST				= MESSAGE_TASK_REQUEST,
		TASK_REQUEST_RESPONSE		= MESSAGE_TASK_REQUEST_RESPONSE,
		TASK_STATE					= MESSAGE_TASK_STATE,

		QUIT						= MESSAGE_QUIT,
	};

	[Serializable]
	public enum class ETaskRequestResponseType
	{
		RELEASE						= RESPONSE_TYPE_RELEASE,
		RESERVATION					= RESPONSE_TYPE_RESERVATION,
		SPECIFICATION				= RESPONSE_TYPE_SPECIFICATION,
	};

	/**
	 * Flags used when creating a Job or Task
	 */
	[Serializable]
	public enum class EJobTaskFlags
	{
		FLAG_USE_DEFAULTS			= JOB_FLAG_USE_DEFAULTS,
		FLAG_ALLOW_REMOTE			= JOB_FLAG_ALLOW_REMOTE,
		FLAG_MANUAL_START			= JOB_FLAG_MANUAL_START,
		FLAG_64BIT					= JOB_FLAG_64BIT,

		TASK_FLAG_USE_DEFAULTS		= JOB_TASK_FLAG_USE_DEFAULTS,
		TASK_FLAG_ALLOW_REMOTE		= JOB_TASK_FLAG_ALLOW_REMOTE,
	};

	/**
	 * All possible states a Job or Task can be in
	 */
	[Serializable]
	public enum class EJobTaskState
	{
		STATE_INVALID				= JOB_STATE_INVALID,
		STATE_IDLE					= JOB_STATE_IDLE,
		STATE_READY					= JOB_STATE_READY,
		STATE_RUNNING				= JOB_STATE_RUNNING,
		STATE_COMPLETE_SUCCESS		= JOB_STATE_COMPLETE_SUCCESS,
		STATE_COMPLETE_FAILURE		= JOB_STATE_COMPLETE_FAILURE,
		STATE_KILLED				= JOB_STATE_KILLED,

		TASK_STATE_INVALID			= JOB_TASK_STATE_INVALID,
		TASK_STATE_IDLE				= JOB_TASK_STATE_IDLE,
		TASK_STATE_ACCEPTED			= JOB_TASK_STATE_ACCEPTED,
		TASK_STATE_REJECTED			= JOB_TASK_STATE_REJECTED,
		TASK_STATE_RUNNING			= JOB_TASK_STATE_RUNNING,
		TASK_STATE_COMPLETE_SUCCESS	= JOB_TASK_STATE_COMPLETE_SUCCESS,
		TASK_STATE_COMPLETE_FAILURE	= JOB_TASK_STATE_COMPLETE_FAILURE,
		TASK_STATE_KILLED			= JOB_TASK_STATE_KILLED,
	};

	/**
	 * The Alert levels
	 */
	[Serializable]
	public enum class EAlertLevel
	{
		ALERT_INFO					= ALERT_LEVEL_INFO,
		ALERT_WARNING				= ALERT_LEVEL_WARNING,
		ALERT_ERROR					= ALERT_LEVEL_ERROR,
		ALERT_CRITICAL_ERROR		= ALERT_LEVEL_CRITICAL_ERROR,
	};

	///////////////////////////////////////////////////////////////////////////

	/**
	 * A class encapsulating the GUID of an Agent
	 */
	[Serializable]
	public ref class AgentGuid	: public IEquatable<AgentGuid^>
	{
	public:
		AgentGuid( void )
		{
			A = 0;
			B = 0;
			C = 0;
			D = 0;
		}

		AgentGuid( UInt32 InA, UInt32 InB, UInt32 InC, UInt32 InD )
		{
			A = InA;
			B = InB;
			C = InC;
			D = InD;
		}

		virtual bool Equals( AgentGuid^ Other )
		{
			// Needed for use in Dictionary collections
			return( ( A == Other->A ) &&
					( B == Other->B ) &&
					( C == Other->C ) &&
					( D == Other->D ) );
		}

		virtual int GetHashCode( void ) override
		{
			return( ( int )( A ^ B ^ C ^ D ) );
		}

		virtual String^ ToString( void ) override
		{
			return( String::Format( "{0:X8}-{1:X8}-{2:X8}-{3:X8}", A, B, C, D ) );
		}

		UInt32 A;
		UInt32 B;
		UInt32 C;
		UInt32 D;
	};

	///////////////////////////////////////////////////////////////////////////

	[Serializable]
	public ref class AgentMessage
	{
	public:
		AgentMessage( void )
		{
			To = Constants::INVALID;
			From = Constants::INVALID;
			Version = ESwarmVersionValue::VER_1_0;
			Type = EMessageType::NONE;
		}

		AgentMessage( EMessageType NewType )
		{
			To = Constants::INVALID;
			From = Constants::INVALID;
			Version = ESwarmVersionValue::VER_1_0;
			Type = NewType;
		}

		AgentMessage( ESwarmVersionValue NewVersion, EMessageType NewType )
		{
			To = Constants::INVALID;
			From = Constants::INVALID;
			Version = NewVersion;
			Type = NewType;
		}
		
		// Handles for the sender and the recipient of this message
		Int32						To;
		Int32						From;
		ESwarmVersionValue			Version;
		EMessageType				Type;
	};

	///////////////////////////////////////////////////////////////////////////

	[Serializable]
	public ref class AgentInfoMessage : public AgentMessage
	{
	public:
		AgentInfoMessage( void )
			: AgentMessage( EMessageType::INFO )
		{
		}

		AgentInfoMessage( String^ NewTextMessage )
			: AgentMessage( EMessageType::INFO )
		{
			TextMessage = NewTextMessage;
		}

		// Generic text message for informational purposes
		String^						TextMessage;
	};

	///////////////////////////////////////////////////////////////////////////

	[Serializable]
	public ref class AgentTimingMessage : public AgentMessage
	{
	public:
		AgentTimingMessage( EProgressionState NewState, int InThreadNum )
			: AgentMessage( EMessageType::TIMING )
			, State( NewState )
			, ThreadNum( InThreadNum )
		{
		}

		// The state the process is transitioning to
		EProgressionState			State;
		// The thread this state is referring to
		int							ThreadNum;
	};

	///////////////////////////////////////////////////////////////////////////

	[Serializable]
	public ref class AgentJobMessageBase : public AgentMessage
	{
	public:
		AgentJobMessageBase( AgentGuid^ NewJobGuid, EMessageType NewType )
			: AgentMessage( NewType )
		{
			JobGuid = NewJobGuid;
		}

		// Basic identifier for the Job
		AgentGuid^ JobGuid;
	};

	///////////////////////////////////////////////////////////////////////////

	/**
	 *	A message indicating a) a warning, b) an error, or c) a critical error.
	 *	It includes the Job GUID, the GUID of the item causing the issue, a 
	 *	32-bit field intended to identify the type of the item, and a string
	 *	giving the issue message (which will be localized on the UE3-side of
	 *	things.
	 */
	[Serializable]
	public ref class AgentAlertMessage : public AgentJobMessageBase
	{
	public:
		AgentAlertMessage( AgentGuid^ NewJobGuid )
			: AgentJobMessageBase( NewJobGuid, EMessageType::ALERT )
		{
		}

		AgentAlertMessage( AgentGuid^ NewJobGuid, EAlertLevel NewAlertLevel, AgentGuid^ NewObjectGuid, Int32 NewTypeId, String^ NewTextMessage )
			: AgentJobMessageBase( NewJobGuid, EMessageType::ALERT )
		{
			AlertLevel = NewAlertLevel;
			ObjectGuid = NewObjectGuid;
			TypeId = NewTypeId;
			TextMessage = NewTextMessage;
		}

		// The type of alert
		EAlertLevel		AlertLevel;
		// The identifier for the object that is associated with the issue
		AgentGuid^		ObjectGuid;
		// App-specific identifier for the type of the object
		Int32			TypeId;
		// Generic text message for informational purposes
		String^			TextMessage;
	};

	///////////////////////////////////////////////////////////////////////////

	[Serializable]
	public ref class AgentJobSpecification
	{
	public:
		AgentJobSpecification( AgentGuid^ NewJobGuid, EJobTaskFlags NewJobFlags, String^ JobExecutableName, String^ JobParameters, List<String^>^ JobRequiredDependencies, List<String^>^ JobOptionalDependencies )
		{
			JobGuid = NewJobGuid;
			JobFlags = NewJobFlags;
			ExecutableName = JobExecutableName;
			Parameters = JobParameters;
			RequiredDependencies = JobRequiredDependencies;
			OptionalDependencies = JobOptionalDependencies;
			DependenciesOriginalNames = nullptr;
		}

		// Basic identifiers for the Job
		AgentGuid^						JobGuid;
		EJobTaskFlags					JobFlags;

		// Variables to set up and define a Job process
		String^							ExecutableName;
		String^							Parameters;

		// List of required channels for this Job and a mapping from the
		// cached name to the original name for each
		List<String^>^					RequiredDependencies;
		List<String^>^					OptionalDependencies;
		Dictionary<String^, String^>^	DependenciesOriginalNames;
	};

	///////////////////////////////////////////////////////////////////////////

	[Serializable]
	public ref class AgentSignalMessage : public AgentMessage
	{
	public:
		AgentSignalMessage( void )
			: AgentMessage( EMessageType::SIGNAL )
		{
			ResetEvent = gcnew ManualResetEvent( false );
		}

		ManualResetEvent^			ResetEvent;
	};

	///////////////////////////////////////////////////////////////////////////

	[Serializable]
	public ref class AgentTaskRequestResponse : public AgentJobMessageBase
	{
	public:
		AgentTaskRequestResponse( AgentGuid^ TaskJobGuid, ETaskRequestResponseType TaskResponseType )
			: AgentJobMessageBase( TaskJobGuid, EMessageType::TASK_REQUEST_RESPONSE )
		{
			ResponseType = TaskResponseType;
		}

		// The type of response this message is. Subclasses add any additional data.
		ETaskRequestResponseType	ResponseType;
	};

	///////////////////////////////////////////////////////////////////////////

	[Serializable]
	public ref class AgentTaskSpecification : public AgentTaskRequestResponse
	{
	public:
		AgentTaskSpecification( AgentGuid^ TaskJobGuid, AgentGuid^ TaskTaskGuid, Int32 TaskTaskFlags, String^ TaskParameters, Int32 TaskCost, List<String^>^ TaskDependencies )
			: AgentTaskRequestResponse( TaskJobGuid, ETaskRequestResponseType::SPECIFICATION )
		{
			TaskGuid = TaskTaskGuid;
			TaskFlags = TaskTaskFlags;
			Parameters = TaskParameters;
			Cost = TaskCost;
			Dependencies = TaskDependencies;
			DependenciesOriginalNames = nullptr;
		}

		// The GUID used for identifying the Task being referred to
		AgentGuid^					TaskGuid;
		Int32						TaskFlags;

		// The Task's parameter string specified with AddTask
		String^						Parameters;

		// The Task's cost, relative to all other Tasks in the same Job, used
		// for even distribution and scheduling
		Int32						Cost;

		// List of required channels for this Job and a mapping from the
		// cached name to the original name for each
		List<String^>^					Dependencies;
		Dictionary<String^, String^>^	DependenciesOriginalNames;
	};

	///////////////////////////////////////////////////////////////////////////

	[Serializable]
	public ref class AgentJobState : public AgentJobMessageBase
	{
	public:
		AgentJobState( AgentGuid^ NewJobGuid, EJobTaskState NewJobState )
			: AgentJobMessageBase( NewJobGuid, EMessageType::JOB_STATE )
		{
			JobState = NewJobState;
		}

		// Basic properties for the Job
		EJobTaskState				JobState;
		String^						JobMessage;

		// Additional stats for the Job
		double						JobRunningTime;
		Int32						JobExitCode;
	};

	///////////////////////////////////////////////////////////////////////////

	[Serializable]
	public ref class AgentTaskState : public AgentJobMessageBase
	{
	public:
		AgentTaskState( AgentGuid^ NewJobGuid, AgentGuid^ NewTaskGuid, EJobTaskState NewTaskState )
			: AgentJobMessageBase( NewJobGuid, EMessageType::TASK_STATE )
		{
			TaskGuid = NewTaskGuid;
			TaskState = NewTaskState;
		}

		// Basic properties for the Task
		AgentGuid^					TaskGuid;
		EJobTaskState				TaskState;
		String^						TaskMessage;

		// Additional stats for the Task
		double						TaskRunningTime;
		Int32						TaskExitCode;
	};

	///////////////////////////////////////////////////////////////////////////

	/**
	 * The primary interface to the Swarm system
	 */
	public interface class IAgentInterface 
	{
	public:
		/**
		 * Opens a new local connection to the Swarm
		 */
		Int32 OpenConnection( Hashtable^ InParameters, Hashtable^% OutParameters );

		/**
		 * Closes an existing connection to the Swarm, if one is open
		 */
		Int32 CloseConnection( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );

		/**
		 * Sends a message to an Agent (return messages are sent via the FConnectionCallback)
		 */
		Int32 SendMessage( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );

		/*
		 * Gets the next message in the queue, optionally blocking until a message is available
		 * 
		 * TimeOut - Milliseconds to wait (-1 is infinite)
		 */
		Int32 GetMessage( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );

		/**
	 	 * Adds an existing file to the cache. Note, any existing channel with the same
		 * name will be overwritten.
		 */
		Int32 AddChannel( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );

		/**
		 * Determines if the named channel is in the cache
		 */
		Int32 TestChannel( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );

		/**
		 * Determines if a requested channel is safe to open (returns >= 0 if so)
		 */
		Int32 OpenChannel( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );

		/**
		 * Closes an open channel
		 */
		Int32 CloseChannel( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );

		/**
		 * Opens a Job session, which allows a Job to be specified, Tasks added, Job
		 * channels opened and used, etc. When the Job is complete and no more Job
		 * related data is needed from the Swarm, call CloseJob.
		 */
		Int32 OpenJob( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );

		/**
		 * Begins a Job specification, which allows a series of Tasks to be specified
		 * via AddTask. When Tasks are done being specified, call EndJobSpecification.
		 *
		 * The default behavior will be to execute the Job executable with the
		 * specified parameters. If Tasks are added for the Job, they are expected
		 * to be requested by the executable run for the Job. If no Tasks are added
		 * for the Job, it is expected that the Job executable will perform its
		 * operations without additional Task input from Swarm.
		 */
		Int32 BeginJobSpecification( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );

		/**
		 * Adds a Task to the current Job
		 */
		Int32 AddTask( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );

		/**
		 * Ends the Job specification, after which no additional Tasks may be defined. Also,
		 * this is generally the point when the Agent will validate and launch the Job executable,
		 * potentially distributing the Job to other Agents.
		 */
		Int32 EndJobSpecification( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );

		/**
		 * Ends the Job, after which all Job-related API usage (except OpenJob) will be rejected
		 */
		Int32 CloseJob( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );

		/**
		 * A fully general interface to the Agent which is used for extending the API in novel
		 * ways, debugging interfaces, extended statistics gathering, etc.
		 */
		Int32 Method( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );

		/*
		 * Logs a line of text to the agent log window
		 */
		Int32 Log( EVerbosityLevel Verbosity, Color TextColour, String^ Line );
	};
}

#endif // __AGENTINTERFACE_H__
// end
