/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This file contains the definitions of the networked stat notify provider
 */

#ifndef _STATS_NOTIFY_PROVIDER_UDP_H
#define _STATS_NOTIFY_PROVIDER_UDP_H

#if WITH_UE3_NETWORKING

#if STATS

#define MAX_STAT_PACKET_SIZE 128
/** Class that specifies a specific sized array of data to nbo write to */
class FNboSerializeBuffer :
	public FNboSerializeToBuffer
{
public:
	FNboSerializeBuffer(void) :
		FNboSerializeToBuffer(MAX_STAT_PACKET_SIZE)
	{
	}
};

/**
 * The type of game that is running
 */
enum EStatGameType
{
	STATGAMETYPE_Unknown,
	STATGAMETYPE_Editor,
	STATGAMETYPE_Server,
	STATGAMETYPE_ListenServer,
	STATGAMETYPE_Client
};

/**
 * Determines the type of game we currently are running
 */
inline EStatGameType appGetStatGameType(void)
{
	EStatGameType GameType = STATGAMETYPE_Editor;
	if (GIsEditor == FALSE)
	{
		// Check the world's info object
		if (GWorld != NULL && GWorld->GetWorldInfo() != NULL)
		{
			// Map the netmode to our enum
			switch (GWorld->GetWorldInfo()->NetMode)
			{
				case NM_Standalone:
				case NM_Client:
				{
					GameType = STATGAMETYPE_Client;
					break;
				}
				case NM_ListenServer:
				{
					GameType = STATGAMETYPE_ListenServer;
					break;
				}
				case NM_DedicatedServer:
				{
					GameType = STATGAMETYPE_Server;
					break;
				}
			};
		}
		else
		{
			// Can't figure it out
			GameType = STATGAMETYPE_Unknown;
		}
	}
	return GameType;
}

#define MAX_CYCLE_COUNTERS_COPIED	2000
#define MAX_DWORD_COUNTERS_COPIED	500
#define MAX_FLOAT_COUNTERS_COPIED	500

/**
 * Struct that copies the data gathered on this frame so it can be packetized
 * and sent by a different thread.
 */
struct FPerFrameStatData
{
	/** Holds a copy of a single cycle counter's data */
	struct FCycleStatCopy
	{
		DWORD StatId;
		DWORD ParentId;
		DWORD InstanceId;
		DWORD ParentInstanceId;
		DWORD ThreadId;
		DWORD Value;
		DWORD CallsPerFrame;
	};
	/** Holds a copy of a single DWORD counter's data */
	struct FDwordStatCopy
	{
		DWORD StatId;
		DWORD Value;
	};
	/** Holds a copy of a single FLOAT counter's data */
	struct FFloatStatCopy
	{
		DWORD StatId;
		FLOAT Value;
	};

	/** Internal queues of stats data */
	FCycleStatCopy CycleCounters[MAX_CYCLE_COUNTERS_COPIED];
	/** Holds the number of counters that are in the queue */
	DWORD NumCycleCounters;
	/** Queue of dword stats data */
	FDwordStatCopy DwordCounters[MAX_DWORD_COUNTERS_COPIED];
	/** Holds the number of counters that are in the queue */
	DWORD NumDwordCounters;
	/** Queue of float stats data */
	FFloatStatCopy FloatCounters[MAX_FLOAT_COUNTERS_COPIED];
	/** Holds the number of counters that are in the queue */
	DWORD NumFloatCounters;
	/** The frame this data is for */
	DWORD FrameNumber;

	/** Just zeros the array offsets */
	FPerFrameStatData(void) :
		NumCycleCounters(0),
		NumDwordCounters(0),
		NumFloatCounters(0),
		FrameNumber(0)
	{
#if _DEBUG
		appMemset(this,sizeof(FPerFrameStatData),0xBE);
		Reset();
#endif
	}

	/**
	 * Zeros the offsets into the arrays
	 */
	FORCEINLINE void Reset(void)
	{
		NumFloatCounters = NumDwordCounters = NumCycleCounters = 0;
	}

	/**
	 * Copies the data into the next available slot
	 *
	 * @param StatId the id of the stat that is being written out
	 * @param ParentId the id of parent stat
	 * @param InstanceId the instance id of the stat being written
	 * @param ParentInstanceId the instance id of parent stat
	 * @param ThreadId the thread this stat is for
	 * @param Value the value of the stat to write out
	 * @param CallsPerFrame the number of calls for this frame
	 */
	FORCEINLINE void CopyStat(DWORD StatId,DWORD ParentId,DWORD InstanceId,
		DWORD ParentInstanceId,DWORD ThreadId,DWORD Value,
		DWORD CallsPerFrame)
	{
		check(NumCycleCounters < MAX_CYCLE_COUNTERS_COPIED && "Increase the define if you hit this");
		CycleCounters[NumCycleCounters].StatId = StatId;
		CycleCounters[NumCycleCounters].ParentId = ParentId;
		CycleCounters[NumCycleCounters].InstanceId = InstanceId;
		CycleCounters[NumCycleCounters].ParentInstanceId = ParentInstanceId;
		CycleCounters[NumCycleCounters].ThreadId = ThreadId;
		CycleCounters[NumCycleCounters].Value = Value;
		CycleCounters[NumCycleCounters].CallsPerFrame = CallsPerFrame;
		NumCycleCounters++;
	}

	/**
	 * Copies the data into the next available slot
	 *
	 * @param StatId the id of the stat that is being written out
	 * @param Value the value of the stat to write out
	 */
	FORCEINLINE void CopyStat(DWORD StatId,FLOAT Value)
	{
		check(NumFloatCounters < MAX_FLOAT_COUNTERS_COPIED && "Increase the define if you hit this");
		FloatCounters[NumFloatCounters].StatId = StatId;
		FloatCounters[NumFloatCounters].Value = Value;
		NumFloatCounters++;
	}

	/**
	 * Copies the data into the next available slot
	 *
	 * @param StatId the id of the stat that is being written out
	 * @param Value the value of the stat to write out
	 */
	FORCEINLINE void CopyStat(DWORD StatId,DWORD Value)
	{
		check(NumDwordCounters < MAX_DWORD_COUNTERS_COPIED && "Increase the define if you hit this");
		DwordCounters[NumDwordCounters].StatId = StatId;
		DwordCounters[NumDwordCounters].Value = Value;
		NumDwordCounters++;
	}
};


/**
 * Protocol flow:
 *
 * Client				Server
 * SA		->
 *					<-	SR<ComputerNameLen><ComputerName><GameNameLen><GameName><GameType><PlatformType><UpdatePort>
 * CC		->
 *					<-	Starts sending update packets
 *
 *						Stat Descriptions (sent on initial connection)
 *					<-	PC<SecondsPerCycle>
 *					<-	SD<StatId><StatNameLen><StatName><StatType><GroupId>
 *					<-	GD<GroupId><GroupNameLen><GroupName>
 *
 *						Stat updates (sent each frame)
 *					<-	NF<FrameNum>
 *					<-	UC<StatId><ParentId><ThreadId><Value><CallsPerFrame>
 *					<-	UD<StatId><Value>
 *					<-	UF<StatId><Value>
 *
 * CD		->
 *					<-	No response, stops update packets
 *
 */

/**
 * This stats notify provider sends stats updates via the network. It consists
 * of two threads: one to pump data to clients and another to handle beacon
 * operations.
 */
class FStatNotifyProvider_UDP :
	public FStatNotifyProvider
{
	/**
	 * Holds information about a given client connection
	 */
	struct FClientConnection
	{
		/**
		 * The IP address of the client
		 */
		FIpAddr Address;
		/**
		 * Whether this is a new connection or not (determines if the sender
		 * should first send the descriptions packets before stats data)
		 */
		UBOOL bNewConnection;

		/**
		 * Constructor. Copies the IP address and sets this to a new connection
		 */
		FClientConnection(FIpAddr InAddr) :
			Address(InAddr),
			bNewConnection(TRUE)
		{
		}

		/** Default ctor */
		FClientConnection(void) :
			bNewConnection(FALSE)
		{
		}
	};

	/**
	 * Holds the list of clients that we are multicasting data to. This class
	 * is thread safe
	 */
	class FStatClientList
	{
		/**
		 * Holds the list of IP addresses to send the data to
		 */
		TArray<FClientConnection> Clients;
		/**
		 * The port to send the data on
		 */
		INT PortNo;
		/**
		 * The synchronization object for thread safe access
		 */
		FCriticalSection* AccessSync;
		/**
		 * Used during iteration of clients
		 */
		INT CurrentIndex;

	public:
		/**
		 * Initalizes the port and creates our synchronization object
		 */
		FStatClientList(INT InPortNo) :
			PortNo(InPortNo)
		{
			AccessSync = GSynchronizeFactory->CreateCriticalSection();
			check(AccessSync);
		}

		/**
		 * Adds a client to our list
		 *
		 * @param Client the new client to add to the list
		 */
		inline void AddClient(FIpAddr Client)
		{
			FScopeLock sl(AccessSync);
			// Assign the common port number
			Client.Port = PortNo;
			// Add it to our list
			new(Clients)FClientConnection(Client);
		}

		/**
		 * Removes a client from our list
		 *
		 * @param Client the client to remove from the list
		 */
		inline void RemoveClient(FIpAddr Client)
		{
			FScopeLock sl(AccessSync);
			// Assign the common port number
			Client.Port = PortNo;
			UBOOL bFound = FALSE;
			// Search for a client that matches and remove it
			for (INT Index = 0; Index < Clients.Num() && bFound == FALSE; Index++)
			{
				if (Client == Clients(Index).Address)
				{
					Clients.Remove(Index);
					bFound = TRUE;
				}
			}
		}

		/**
		 * Provides for thread safe forward iteration through the client list.
		 * This method starts the iteration by locking the data. Must call the
		 * EndIterator() function to unlock the list.
		 */
		inline FClientConnection* BeginIterator(void)
		{
			// Manually lock. Locks across scope
			AccessSync->Lock();
			CurrentIndex = 0;
			FClientConnection* Current = NULL;
			if (Clients.Num() > 0)
			{
				Current = &Clients(CurrentIndex);
			}
			return Current;
		}

		/**
		 * Provides for thread safe forward iteration through the client list.
		 * This method ends the iteration by unlocking the data
		 */
		inline void EndIterator(void)
		{
			// Manually unlock. Any blocked threads start now
			AccessSync->Unlock();
		}

		/**
		 * Provides for thread safe forward iteration through the client list.
		 * Moves to the next item in the list. Returns null, when at the end
		 * of the list
		 */
		inline FClientConnection* GetNext(void)
		{
			CurrentIndex++;
			FClientConnection* Current = NULL;
			// Return null if we are at the end to indicate being done
			if (CurrentIndex < Clients.Num())
			{
				Current = &Clients(CurrentIndex);
			}
			return Current;
		}

		/**
		 * Determines the number of connections that are present
		 */
		inline INT GetConnectionCount(void)
		{
			FScopeLock sl(AccessSync);
			return Clients.Num();
		}
	};

	/**
	 * This is the runnable that will perform the async beacon queries
	 */
	class FStatListenerRunnable : public FRunnable
	{
		/**
		 * Holds the port that beacon listens for client broadcasts on
		 */
		INT ListenPort;
		/**
		 * This is the port that will be used to send stats traffic to a client
		 */
		INT StatsTrafficPort;
		/**
		 * This is the "time to die" event. Triggered when UDP notify provider
		 * is shutting down
		 */
		FEvent* TimeToDie;
		/**
		 * The thread safe list of clients that we are sending stats data to
		 */
		FStatClientList* ClientList;
		/** The socket to listen for requests on */
		FSocket* ListenSocket;
		/** The address in bound requests come in on */
		FInternetIpAddr ListenAddr;

		/**
		 * Hidden on purpose
		 */
		FStatListenerRunnable();
	
		/**
		 * Receives data from a given call to Poll(). For the beacon, we
		 * just send an announcement directly to the source address
		 *
		 * @param SrcAddr the source address of the request
		 * @param Data the packet data
		 * @param Count the number of bytes in the packet
		 */
		void ProcessPacket(FIpAddr SrcAddr, BYTE* Data, INT Count);

	public:
		/**
		 * Copies the specified values to use as the beacon
		 *
		 * @param InPort the port to listen for client requests on
		 * @param InStatsTrafficPort the port that stats updates will be sent on
		 * @param InClients the list that clients will be added to/removed from
		 */
		FStatListenerRunnable(INT InPort,INT InStatsTrafficPort,
			FStatClientList* InClients);

	// FRunnable interface

		/**
		 * Binds to the specified port
		 *
		 * @return True if initialization was successful, false otherwise
		 */
		virtual UBOOL Init(void);

		/**
		 * Sends an announcement message on start up and periodically thereafter
		 *
		 * @return The exit code of the runnable object
		 */
		virtual DWORD Run(void);

		/**
		 * Triggers the time to die event causing the thread to exit
		 */
		virtual void Stop(void)
		{
			if (TimeToDie)
			{
				TimeToDie->Trigger();
			}
		}

		/**
		 * Releases the time to die event
		 */
		virtual void Exit(void)
		{
			GSynchronizeFactory->Destroy(TimeToDie);
			TimeToDie = NULL;
			if (ListenSocket != NULL)
			{
				GSocketSubsystem->DestroySocket(ListenSocket);
				ListenSocket = NULL;
			}
		}
	};

	/**
	 * This is the runnable that will perform the async sends of stats data
	 * to remote clients
	 */
	class FStatSenderRunnable :
		public FRunnable
	{
		/**
		 * Flag indicating whether it's time to shut down or not
		 */
		UBOOL bIsTimeToDie;
		/**
		 * This is the event that tells the sender thread to wake up and
		 * send its packet data
		 */
		FEvent* WorkEvent;
		/**
		 * Used to tell the main thread that the sender is done and it is ok to
		 * swap queues now
		 */
		FEvent* QueueEmptyEvent;
		/**
		 * The queue of stats data that needs to be delivered to the list of clients
		 */
		FPerFrameStatData SendQueues[2];
		/**
		 * The index of the queue that the sender thread is reading from
		 */
		DWORD SendFromIndex;
		/**
		 * The index of the queue that the main thread is writing packets to
		 */
		DWORD QueueIntoIndex;
		/**
		 * The synchronization object for thread safe access to the queue
		 */
		FCriticalSection* AccessSync;
		/**
		 * The thread safe list of clients that we are sending stats data to
		 */
		FStatClientList* ClientList;
		/**
		 * The queue of packets that needs to be delivered whenever a new client
		 * connects to our game. Sends the meta data needed to process the
		 * stats data by the client
		 */
		TArray<FNboSerializeBuffer*> DescriptionsQueue;
		/** The socket to send responses on */
		FSocket* SenderSocket;

		/**
		 * Hidden on purpose
		 */
		FStatSenderRunnable();
	
		/**
		 * Handles sending all of the descriptions to a new client
		 *
		 * @param Clients the client list that needs to be checked
		 * @param ClientCount the number of clients to check
		 */
		void SendDescriptions(FClientConnection* Clients,DWORD ClientCount);

		/**
		 * Handles sending all of the cycle counters to each client
		 *
		 * @param Clients the clients that will receive the packets
		 * @param ClientCount the number of clients to send to
		 */
		void SendCycleCounters(FClientConnection* Clients,DWORD ClientCount);

		/**
		 * Handles sending all of the dword counters to each client
		 *
		 * @param Clients the clients that will receive the packets
		 * @param ClientCount the number of clients to send to
		 */
		void SendDwordCounters(FClientConnection* Clients,DWORD ClientCount);

		/**
		 * Handles sending all of the float counters to each client
		 *
		 * @param Clients the clients that will receive the packets
		 * @param ClientCount the number of clients to send to
		 */
		void SendFloatCounters(FClientConnection* Clients,DWORD ClientCount);

		/**
		 * Sends the new frame packet to all clients
		 *
		 * @param Clients the clients that will receive the packets
		 * @param ClientCount the number of clients to send to
		 */
		void SendNewFramePacket(FClientConnection* Clients,DWORD ClientCount);

	public:
		/**
		 * Copies the specified values to use for the send thread
		 *
		 * @param InSync the critical section to use
		 * @param InClients the list of clients that will receive updates
		 */
		FStatSenderRunnable(FCriticalSection* InSync,FStatClientList* InClients);

		/**
		 * Adds a packet to the descriptions queue. The entire description
		 * queue is sent to a newly connected client
		 *
		 * @param Packet the data that is to be sent to all clients
		 */
		void AddDescriptionPacket(FNboSerializeBuffer* Packet);

		/**
		 * Swaps the queues that are being written to/read from
		 */
		void SwapQueues(void);

		/**
		 * Returns the queue that is holding the data
		 */
		FORCEINLINE FPerFrameStatData& GetPerFrameQueue(void)
		{
			return SendQueues[QueueIntoIndex];
		}

	// FRunnable interface

		/**
		 * Creates the event that will tell the thread there is work to do
		 *
		 * @return True if initialization was successful, false otherwise
		 */
		virtual UBOOL Init(void);

		/**
		 * Waits for the work event to be triggered. Once triggered it sends
		 * each pending packet out to each client in the clients list
		 *
		 * @return The exit code of the runnable object
		 */
		virtual DWORD Run(void);

		/**
		 * Triggers the work event after setting the time to die flag
		 */
		virtual void Stop(void)
		{
			if (bIsTimeToDie == FALSE && WorkEvent)
			{
				appInterlockedIncrement((INT*)&bIsTimeToDie);
				WorkEvent->Trigger();
			}
		}

		/**
		 * Cleans up any allocated resources
		 */
		virtual void Exit(void);
	};

	/**
	 * The thread that handles beacon sends and queries
	 */
	FRunnableThread* ListenerThread;
	/**
	 * The runnable object used to handle client beacon requests
	 */
	FRunnable* ListenerRunnable;
	/**
	 * The thread safe list of clients that we are sending stats data to
	 */
	FStatClientList* ClientList;
	/**
	 * The thread that handles beacon sends and queries
	 */
	FRunnableThread* SenderThread;
	/**
	 * The runnable object used to send data to the connected clients
	 */
	FStatSenderRunnable* SenderRunnable;

public:
	/**
	 * Constructor called by the factory
	 */
	FStatNotifyProvider_UDP(void) :
		FStatNotifyProvider(TEXT("StatsNotifyProvider_UDP"),TEXT("UDPStats")),
		ListenerThread(NULL),
		ListenerRunnable(NULL),
		ClientList(NULL),
		SenderThread(NULL),
		SenderRunnable(NULL)
	{
	}

// FStatNotifyProvider interface

	/**
	 * Initializes the threads that handle the network layer
	 */
	virtual UBOOL Init(void);

	/**
	 * Shuts down the network threads
	 */
	virtual void Destroy(void);

	/**
	 * Queues a packet for the new frame information
	 *
	 * @param FrameNumber the new frame number being processed
	 */
	void SetFrameNumber(DWORD FrameNumber);

	/**
	 * Tells the provider that we are starting to supply it with descriptions
	 * for all of the stats/groups.
	 */
	virtual void StartDescriptions(void);

	/**
	 * Tells the provider that we are finished sending descriptions for all of
	 * the stats/groups.
	 */
	virtual void EndDescriptions(void) { }

	/**
	 * Tells the provider that we are starting to supply it with group descriptions
	 */
	virtual void StartGroupDescriptions(void) { }

	/**
	 * Tells the provider that we are finished sending stat descriptions
	 */
	virtual void EndGroupDescriptions(void) { }

	/**
	 * Tells the provider that we are starting to supply it with stat descriptions
	 */
	virtual void StartStatDescriptions(void) { }

	/**
	 * Tells the provider that we are finished sending group descriptions
	 */
	virtual void EndStatDescriptions(void) { }

	/**
	 * Adds a stat to the list of descriptions. Used to allow custom stats to
	 * report who they are, parentage, etc. Prevents applications that consume
	 * the stats data from having to change when stats information changes
	 *
	 * @param StatId the id of the stat
	 * @param StatName the name of the stat
	 * @param StatType the type of stat this is
	 * @param GroupId the id of the group this stat belongs to
	 */
	virtual void AddStatDescription(DWORD StatId,const TCHAR* StatName,
		DWORD StatType,DWORD GroupId);

	/**
	 * Adds a group to the list of descriptions
	 *
	 * @param GroupId the id of the group being added
	 * @param GroupName the name of the group
	 */
	virtual void AddGroupDescription(DWORD GroupId,const TCHAR* GroupName);

	/**
	 * Builds a packet from the passed in data. Queues that packet to be sent
	 * on the sender thread. The packet will be sent to every client in the
	 * clients list
	 *
	 * @param StatId the id of the stat that is being written out
	 * @param GroupId the id of the group the stat belongs to
	 * @param ParentId the id of parent stat
	 * @param InstanceId the instance id of the stat being written
	 * @param ParentInstanceId the instance id of parent stat
	 * @param ThreadId the thread this stat is for
	 * @param Value the value of the stat to write out
	 * @param CallsPerFrame the number of calls for this frame
	 */
	virtual void WriteStat(DWORD StatId,DWORD GroupId,DWORD ParentId,DWORD InstanceId,
		DWORD ParentInstanceId,DWORD ThreadId,DWORD Value,
		DWORD CallsPerFrame);

	/**
	 * Builds a packet from the passed in data. Queues that packet to be sent
	 * on the sender thread. The packet will be sent to every client in the
	 * clients list
	 *
	 * @param StatId the id of the stat that is being written out
	 * @param GroupId the id of the group the stat belongs to
	 * @param Value the value of the stat to write out
	 */
	virtual void WriteStat(DWORD StatId,DWORD GroupId,FLOAT Value);

	/**
	 * Builds a packet from the passed in data. Queues that packet to be sent
	 * on the sender thread. The packet will be sent to every client in the
	 * clients list
	 *
	 * @param StatId the id of the stat that is being written out
	 * @param GroupId the id of the group the stat belongs to
	 * @param Value the value of the stat to write out
	 */
	virtual void WriteStat(DWORD StatId,DWORD GroupId,DWORD Value);
};

#endif

#endif	//#if WITH_UE3_NETWORKING

#endif
