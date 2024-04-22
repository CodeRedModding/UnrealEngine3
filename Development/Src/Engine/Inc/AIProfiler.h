/*=============================================================================
	AIProfiler.h: AI profiling support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef AI_PROFILER_H
#define AI_PROFILER_H

#if !USE_AI_PROFILER

	#if COMPILER_SUPPORTS_NOOP
		#define AI_PROFILER_AI_LOG					__noop
		#define AI_PROFILER_CONTROLLER_DESTROYED	__noop
		#define AI_PROFILER( x )
	#else
		#define AI_PROFILER_AI_LOG(...)
		#define AI_PROFILER_CONTROLLER_DESTROYED(...)
		#define AI_PROFILER( x )
	#endif

#else

#define AI_PROFILER_AI_LOG( AIController, AICommand, LogMessage, EventCategory )	{ FAIProfiler::GetInstance().AILog( AIController, AICommand, LogMessage, EventCategory ); }
#define AI_PROFILER_CONTROLLER_DESTROYED( AIController, AICommand, EventCategory )	{ FAIProfiler::GetInstance().AIControllerDestroyed( AIController, AICommand, EventCategory ); }
#define AI_PROFILER( x ) x

/** 
 * AI profiler. Emits AI events as tokens to a profiling file for later viewing in
 * a standalone tool.
 */
class FAIProfiler
{
public:
	/** Initialize the profiler and create the profiling output file */
	void Init();

	/** Shutdown the profiler, finalizing output to the profiling file */
	void Shutdown();

	/**
	 * Returns whether the profiler is initialized or not
	 *
	 * @return	TRUE if the profiler is initialized, FALSE if it is not
	 */
	UBOOL IsInitialized() const;

	/** Toggles the state of the profiler during the next update tick (from initialized to shutdown, or vice versa) */
	void ToggleCaptureStateNextTick();

	/** Tick/update the profiler, called every game thread tick */
	void Tick();

	/**
	 * Emit a profiling token signifying an AI log event
	 *
	 * @param	InAIController	AI controller emitting the log event (*must* be non-NULL)
	 * @param	InCommand		Current active AI command for the provided controller, if any
	 * @param	InLogText		Log text that makes up the log event
	 * @param	InEventCategory	Category of the log event
	 */
	void AILog( AAIController* InAIController, class UAICommandBase* InCommand, const FString& InLogText, const FName& InEventCategory );
	
	/**
	 * Emit a profiling token signifying the destruction of an AIController
	 *
	 * @param	InAIController	AIController about to be destroyed (*must* be non-NULL)
	 * @param	InCommand		Current active AI command for the provided controller, if any
	 * @param	InEventCategory	Category of the event
	 */
	void AIControllerDestroyed( AAIController* InAIController, class UAICommandBase* InCommand, const FName& InEventCategory );

	/**
	 * Exec handler. Parses command and returns TRUE if handled.
	 *
	 * @param	Cmd	Command to parse
	 * @param	Ar	Output device to use for logging
	 *
	 * @return	TRUE if handled, FALSE otherwise
	 */
	static UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar );

	/**
	 * Singleton interface, returns the lone instance of the profiler class.
	 *
	 * @return Instance of the profiler
	 */
	static FAIProfiler& GetInstance();

private:

	/** 
	 * Helper struct containing information to uniquely identify an AI controller; used as an optimization so each token
	 * only has to emit a controller index instead of several data members to identify their owner controller
	 */
	struct FAIPControllerInfo
	{
		/** Index in the profiler's name table of the name of the controller */
		INT		ControllerNameIndex;

		/** Instance number of the controller name */
		INT		ControllerNameInstance;

		/** Index in the profiler's name table of the name of the class of the controller */
		INT		ControllerClassNameIndex;

		/** Time when the controller was created (relative to TimeSeconds) */
		FLOAT	ControllerCreationTime;

		/**
		 * Equality operator
		 *
		 * @param	Other	Other controller info to compare against
		 *
		 * @return	TRUE if the provided controller and the current controller are equal, FALSE if they are not
		 */
		UBOOL operator==( const FAIPControllerInfo& Other ) const
		{
			return ControllerNameIndex == Other.ControllerNameIndex 
				&& ControllerNameInstance == Other.ControllerNameInstance 
				&& ControllerClassNameIndex == Other.ControllerClassNameIndex
				&& ControllerCreationTime == Other.ControllerCreationTime;
		}

		/**
		 * Inequality operator
		 *
		 * @param	Other	Other controller info to compare against
		 *
		 * @return	TRUE if the provided controller and the current controller are not equal, FALSE if they are
		 */
		UBOOL operator!=( const FAIPControllerInfo& Other ) const
		{
			return ControllerNameIndex != Other.ControllerNameIndex 
				|| ControllerNameInstance != Other.ControllerNameInstance 
				|| ControllerClassNameIndex != Other.ControllerClassNameIndex
				|| ControllerCreationTime != Other.ControllerCreationTime;
		}

		/**
		 * Serialization operator.
		 *
		 * @param	Ar		Archive to serialize to
		 * @param	Header	Header to serialize
		 * @return	Passed in archive
		 */
		friend FArchive& operator << ( FArchive& Ar, FAIPControllerInfo ControllerInfo )
		{
			Ar	<< ControllerInfo.ControllerNameIndex
				<< ControllerInfo.ControllerNameInstance
				<< ControllerInfo.ControllerClassNameIndex
				<< ControllerInfo.ControllerCreationTime;
			check( Ar.IsSaving() );
			return Ar;
		}

		/**
		 * Helper method for map support; generates a hash value for the provided
		 * controller info.
		 *
		 * @param	ControllerInfo	Controller info to generate a hash value for
		 *
		 * @return	Hash value of the provided controller info
		 */
		friend DWORD GetTypeHash( const FAIPControllerInfo& ControllerInfo )
		{
			return appMemCrc( &ControllerInfo, sizeof( FAIPControllerInfo ) );
		}
	};

	/** Constructor */
	FAIProfiler();

	/** Destructor */
	~FAIProfiler();

	// Copy constructor and assignment operator intentionally left unimplemented
	FAIProfiler( const FAIProfiler& );
	FAIProfiler& operator=( const FAIProfiler& );

	/**
	 * Returns the index of the provided string in the profiler's name table. If the name doesn't
	 * already exist in the table, it is added.
	 *
	 * @param	InName		Name to find the index of in the name table
	 *
	 * @return	Index of the provided name in the profiler's name table
	 */
	INT GetNameIndex( const FString& InName );

	/**
	 * Returns the index of the provided controller info in the profiler's controller table. If the controller
	 * doesn't already exist in the table, it is added.
	 *
	 * @param	InControllerInfo	Controller info to find the index of in the controller table
	 *
	 * @return	Index of the provided controller info in the profiler's controller table
	 */
	INT GetControllerIndex( const FAIPControllerInfo& InControllerInfo );

	/**
	 * Helper method to populate a token with basic information about the controller causing
	 * its emission
	 *
	 * @param	InAIController	AI controller causing the token emission (*must* be non-NULL)
	 * @param	InCommand		Current command of the provided AI controller, if any
	 * @param	InEventCategory	Event category of the token emission
	 * @param	OutEmittedToken	Token to populate
	 */
	void PopulateEmittedToken( AAIController* InAIController, class UAICommandBase* InCommand, const FName& InEventCategory, struct FAIPEmittedToken& OutEmittedToken );
	
	/**
	 * Helper method to populate a controller info struct with information from the provided AI controller
	 *
	 * @param	InAIController		Controller to use to populate the controller info struct with
	 * @param	OutControllerInfo	Controller info to populate from the provided controller
	 */
	void ExtractControllerInfo( const AAIController* InAIController, struct FAIPControllerInfo& OutControllerInfo );

	/** Helper method to flush the memory writer to HDD */
	void FlushMemoryWriter();

	/** Whether the profiler is currently initialized or not */
	UBOOL bInitialized;

	/** Whether the profiler should toggle states (initialized<->shutdown) on the next update tick or not */
	UBOOL bShouldToggleCaptureNextTick;

	/** File writer used for serialization to HDD. */
	FArchive* FileWriter;

	/** Transient memory writer used to avoid blocking on I/O during capture. */
	FBufferArchive MemoryWriter;

	/** Name of file written to. */
	FString FileName;

	/** Lookup map representing the name table for the profiler */
	TLookupMap<FString> NameIndexLookupMap;

	/** Lookup map representing the controller table for the profiler */
	TLookupMap<FAIPControllerInfo> ControllerIndexLookupMap;
};

#endif // #if !USE_AI_PROFILER
#endif // #ifndef AI_PROFILER_H
