/*=============================================================================
	UnChan.h: Unreal datachannel class.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Constant for all buffers that are reading from the network
#define MAX_STRING_SERIALIZE_SIZE	2048

/*-----------------------------------------------------------------------------
	UChannel base class.
-----------------------------------------------------------------------------*/
//
// Base class of communication channels.
//
class UChannel : public UObject
{
	DECLARE_ABSTRACT_CLASS_INTRINSIC(UChannel,UObject,CLASS_Transient|0,Engine);

	// Variables.
	UNetConnection*	Connection;		// Owner connection.
	BITFIELD		OpenAcked:1;		// Whether open has been acknowledged.
	BITFIELD		Closing:1;		// State of the channel.
	BITFIELD		OpenTemporary:1;	// Opened temporarily.
	BITFIELD		Broken:1;			// Has encountered errors and is ignoring subsequent packets.
	BITFIELD		bTornOff:1;		// Actor associated with this channel was torn off
	INT             ChIndex;		// Index of this channel.
	INT				OpenedLocally;	// Whether channel was opened locally or by remote.
	INT				OpenPacketId;	// Packet the spawn message was sent in.
	EChannelType	ChType;			// Type of this channel.
	INT				NumInRec;		// Number of packets in InRec.
	INT				NumOutRec;		// Number of packets in OutRec.
	INT				NegotiatedVer;	// Negotiated version of engine = Min(client version, server version).
	FInBunch*		InRec;			// Incoming data with queued dependencies.
	FOutBunch*		OutRec;			// Outgoing reliable unacked data.
	DOUBLE			LastUnqueueTime;	// Last time the reliable queue was cleared.

	// Statics.
	static UClass* ChannelClasses[CHTYPE_MAX];
	static UBOOL IsKnownChannelType( INT Type );

	// Constructor.
	UChannel();
	virtual void BeginDestroy();

	// UChannel interface.
	virtual void Init( UNetConnection* InConnection, INT InChIndex, UBOOL InOpenedLocally );
	virtual void SetClosingFlag();
	virtual void Close();
	virtual FString Describe();
#if WITH_UE3_NETWORKING
	virtual void ReceivedBunch( FInBunch& Bunch ) PURE_VIRTUAL(UChannel::ReceivedBunch,);
	virtual void ReceivedNak( INT NakPacketId );
#endif	//#if WITH_UE3_NETWORKING
	virtual void Tick();

	// General channel functions.
#if WITH_UE3_NETWORKING
	void ReceivedAcks();
	UBOOL ReceivedSequencedBunch( FInBunch& Bunch );
	void ReceivedRawBunch( FInBunch& Bunch );
	virtual INT SendBunch(FOutBunch* Bunch, UBOOL Merge);
#endif	//#if WITH_UE3_NETWORKING
	INT IsNetReady( UBOOL Saturate );
	void AssertInSequenced();
	INT MaxSendBytes();

	/** cleans up channel if it hasn't already been */
	void ConditionalCleanUp()
	{
		if (!HasAnyFlags(RF_PendingKill))
		{
			MarkPendingKill();
			CleanUp();
		}
	}
protected:
	/** cleans up channel structures and NULLs references to the channel */
	virtual void CleanUp();
};

/*-----------------------------------------------------------------------------
	UControlChannel base class.
-----------------------------------------------------------------------------*/

/** network control channel message types
 * 
 * to add a new message type, you need to:
 * - add a DEFINE_CONTROL_CHANNEL_MESSAGE_* for the message type with the appropriate parameters to this file
 * - add IMPLEMENT_CONTROL_CHANNEL_MESSAGE for the message type to UnChan.cpp
 * - implement the fallback behavior (eat an unparsed message) to UControlChannel::ReceivedBunch()
 *
 * @warning: modifying control channel messages breaks network compatibility (update GEngineMinNetVersion)
 */
template<BYTE MessageType> class FNetControlMessage
{
};
/** contains info about a message type retrievable without static binding (e.g. whether it's a valid type, friendly name string, etc) */
class FNetControlMessageInfo
{
public:
	static inline const TCHAR* GetName(BYTE MessageIndex)
	{
		CheckInitialized();
		return Names[MessageIndex];
	}
	static inline UBOOL IsRegistered(BYTE MessageIndex)
	{
		CheckInitialized();
		return Names[MessageIndex][0] != 0;
	}
	template<BYTE MessageType> friend class FNetControlMessage;
private:
	static void CheckInitialized()
	{
		static UBOOL bInitialized = FALSE;
		if (!bInitialized)
		{
			for (INT i = 0; i < ARRAY_COUNT(Names); i++)
			{
				Names[i] = TEXT("");
			}
			bInitialized = TRUE;
		}
	}
	static void SetName(BYTE MessageType, const TCHAR* InName)
	{
		CheckInitialized();
		Names[MessageType] = InName;
	}

	static const TCHAR* Names[255];
};

#if WITH_UE3_NETWORKING

#define DEFINE_CONTROL_CHANNEL_MESSAGE_ZEROPARAM(Name, Index) \
	enum { NMT_##Name = Index }; \
	template<> class FNetControlMessage<Index> \
	{ \
	public: \
		static BYTE Initialize(); \
		/** sends a message of this type on the specified connection's control channel */ \
		static void Send(UNetConnection* Conn) \
		{ \
			check(&Initialize != NULL); /** Stop the compiler optimizing-out Initialize */ \
			checkAtCompileTime(Index < 255, CONTROL_CHANNEL_MESSAGE_MUST_BE_BYTE); \
			checkSlow(!Conn->IsA(UChildConnection::StaticClass())); /** control channel messages can only be sent on the parent connection */ \
			if (Conn->Channels[0] != NULL && !Conn->Channels[0]->Closing) \
			{ \
				FControlChannelOutBunch Bunch(Conn->Channels[0], FALSE); \
				BYTE MessageType = Index; \
				Bunch << MessageType; \
				Conn->Channels[0]->SendBunch(&Bunch, TRUE); \
			} \
		} \
	};
#define DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(Name, Index, TypeA) \
	enum { NMT_##Name = Index }; \
	template<> class FNetControlMessage<Index> \
	{ \
	public: \
		static BYTE Initialize(); \
		/** sends a message of this type on the specified connection's control channel \
		 * @note: const not used only because of the FArchive interface; the parameters are not modified \
		 */ \
		static void Send(UNetConnection* Conn, TypeA& ParamA) \
		{ \
			check(&Initialize != NULL); /** Stop the compiler optimizing-out Initialize */ \
			checkAtCompileTime(Index < 255, CONTROL_CHANNEL_MESSAGE_MUST_BE_BYTE); \
			checkSlow(!Conn->IsA(UChildConnection::StaticClass())); /** control channel messages can only be sent on the parent connection */ \
			if (Conn->Channels[0] != NULL && !Conn->Channels[0]->Closing) \
			{ \
				FControlChannelOutBunch Bunch(Conn->Channels[0], FALSE); \
				BYTE MessageType = Index; \
				Bunch << MessageType; \
				Bunch << ParamA; \
				Conn->Channels[0]->SendBunch(&Bunch, TRUE); \
			} \
		} \
		/** receives a message of this type from the passed in bunch */ \
		static void Receive(FInBunch& Bunch, TypeA& ParamA) \
		{ \
			Bunch << ParamA; \
		} \
		/** throws away a message of this type from the passed in bunch */ \
		static void Discard(FInBunch& Bunch) \
		{ \
			TypeA ParamA; \
			Receive(Bunch, ParamA); \
		} \
	};
#define DEFINE_CONTROL_CHANNEL_MESSAGE_TWOPARAM(Name, Index, TypeA, TypeB) \
	enum { NMT_##Name = Index }; \
	template<> class FNetControlMessage<Index> \
	{ \
	public: \
		static BYTE Initialize(); \
		/** sends a message of this type on the specified connection's control channel \
		 * @note: const not used only because of the FArchive interface; the parameters are not modified \
		 */ \
		static void Send(UNetConnection* Conn, TypeA& ParamA, TypeB& ParamB) \
		{ \
			check(&Initialize != NULL); /** Stop the compiler optimizing-out Initialize */ \
			checkAtCompileTime(Index < 255, CONTROL_CHANNEL_MESSAGE_MUST_BE_BYTE); \
			checkSlow(!Conn->IsA(UChildConnection::StaticClass())); /** control channel messages can only be sent on the parent connection */ \
			if (Conn->Channels[0] != NULL && !Conn->Channels[0]->Closing) \
			{ \
				FControlChannelOutBunch Bunch(Conn->Channels[0], FALSE); \
				BYTE MessageType = Index; \
				Bunch << MessageType; \
				Bunch << ParamA; \
				Bunch << ParamB; \
				Conn->Channels[0]->SendBunch(&Bunch, TRUE); \
			} \
		} \
		static void Receive(FInBunch& Bunch, TypeA& ParamA, TypeB& ParamB) \
		{ \
			Bunch << ParamA; \
			Bunch << ParamB; \
		} \
		/** throws away a message of this type from the passed in bunch */ \
		static void Discard(FInBunch& Bunch) \
		{ \
			TypeA ParamA; \
			TypeB ParamB; \
			Receive(Bunch, ParamA, ParamB); \
		} \
	};
#define DEFINE_CONTROL_CHANNEL_MESSAGE_THREEPARAM(Name, Index, TypeA, TypeB, TypeC) \
	enum { NMT_##Name = Index }; \
	template<> class FNetControlMessage<Index> \
	{ \
	public: \
		static BYTE Initialize(); \
		/** sends a message of this type on the specified connection's control channel \
		 * @note: const not used only because of the FArchive interface; the parameters are not modified \
		 */ \
		static void Send(UNetConnection* Conn, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC) \
		{ \
			check(&Initialize != NULL); /** Stop the compiler optimizing-out Initialize */ \
			checkAtCompileTime(Index < 255, CONTROL_CHANNEL_MESSAGE_MUST_BE_BYTE); \
			checkSlow(!Conn->IsA(UChildConnection::StaticClass())); /** control channel messages can only be sent on the parent connection */ \
			if (Conn->Channels[0] != NULL && !Conn->Channels[0]->Closing) \
			{ \
				FControlChannelOutBunch Bunch(Conn->Channels[0], FALSE); \
				BYTE MessageType = Index; \
				Bunch << MessageType; \
				Bunch << ParamA; \
				Bunch << ParamB; \
				Bunch << ParamC; \
				Conn->Channels[0]->SendBunch(&Bunch, TRUE); \
			} \
		} \
		static void Receive(FInBunch& Bunch, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC) \
		{ \
			Bunch << ParamA; \
			Bunch << ParamB; \
			Bunch << ParamC; \
		} \
		/** throws away a message of this type from the passed in bunch */ \
		static void Discard(FInBunch& Bunch) \
		{ \
			TypeA ParamA; \
			TypeB ParamB; \
			TypeC ParamC; \
			Receive(Bunch, ParamA, ParamB, ParamC); \
		} \
	};
#define DEFINE_CONTROL_CHANNEL_MESSAGE_FOURPARAM(Name, Index, TypeA, TypeB, TypeC, TypeD) \
	enum { NMT_##Name = Index }; \
	template<> class FNetControlMessage<Index> \
	{ \
	public: \
		static BYTE Initialize(); \
		/** sends a message of this type on the specified connection's control channel \
		 * @note: const not used only because of the FArchive interface; the parameters are not modified \
		 */ \
		static void Send(UNetConnection* Conn, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC, TypeD& ParamD) \
		{ \
			check(&Initialize != NULL); /** Stop the compiler optimizing-out Initialize */ \
			checkAtCompileTime(Index < 255, CONTROL_CHANNEL_MESSAGE_MUST_BE_BYTE); \
			checkSlow(!Conn->IsA(UChildConnection::StaticClass())); /** control channel messages can only be sent on the parent connection */ \
			if (Conn->Channels[0] != NULL && !Conn->Channels[0]->Closing) \
			{ \
				FControlChannelOutBunch Bunch(Conn->Channels[0], FALSE); \
				BYTE MessageType = Index; \
				Bunch << MessageType; \
				Bunch << ParamA; \
				Bunch << ParamB; \
				Bunch << ParamC; \
				Bunch << ParamD; \
				Conn->Channels[0]->SendBunch(&Bunch, TRUE); \
			} \
		} \
		static void Receive(FInBunch& Bunch, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC, TypeD& ParamD) \
		{ \
			Bunch << ParamA; \
			Bunch << ParamB; \
			Bunch << ParamC; \
			Bunch << ParamD; \
		} \
		/** throws away a message of this type from the passed in bunch */ \
		static void Discard(FInBunch& Bunch) \
		{ \
			TypeA ParamA; \
			TypeB ParamB; \
			TypeC ParamC; \
			TypeD ParamD; \
			Receive(Bunch, ParamA, ParamB, ParamC, ParamD); \
		} \
	};
#define DEFINE_CONTROL_CHANNEL_MESSAGE_SEVENPARAM(Name, Index, TypeA, TypeB, TypeC, TypeD, TypeE, TypeF, TypeG) \
	enum { NMT_##Name = Index }; \
	template<> class FNetControlMessage<Index> \
	{ \
	public: \
		static BYTE Initialize(); \
		/** sends a message of this type on the specified connection's control channel \
		 * @note: const not used only because of the FArchive interface; the parameters are not modified \
		 */ \
		static void Send(UNetConnection* Conn, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC, TypeD& ParamD, TypeE& ParamE, TypeF& ParamF, TypeG& ParamG) \
		{ \
			check(&Initialize != NULL); /** Stop the compiler optimizing-out Initialize */ \
			checkAtCompileTime(Index < 255, CONTROL_CHANNEL_MESSAGE_MUST_BE_BYTE); \
			checkSlow(!Conn->IsA(UChildConnection::StaticClass())); /** control channel messages can only be sent on the parent connection */ \
			if (Conn->Channels[0] != NULL && !Conn->Channels[0]->Closing) \
			{ \
				FControlChannelOutBunch Bunch(Conn->Channels[0], FALSE); \
				BYTE MessageType = Index; \
				Bunch << MessageType; \
				Bunch << ParamA; \
				Bunch << ParamB; \
				Bunch << ParamC; \
				Bunch << ParamD; \
				Bunch << ParamE; \
				Bunch << ParamF; \
				Bunch << ParamG; \
				Conn->Channels[0]->SendBunch(&Bunch, TRUE); \
			} \
		} \
		static void Receive(FInBunch& Bunch, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC, TypeD& ParamD, TypeE& ParamE, TypeF& ParamF, TypeG& ParamG) \
		{ \
			Bunch << ParamA; \
			Bunch << ParamB; \
			Bunch << ParamC; \
			Bunch << ParamD; \
			Bunch << ParamE; \
			Bunch << ParamF; \
			Bunch << ParamG; \
		} \
		/** throws away a message of this type from the passed in bunch */ \
		static void Discard(FInBunch& Bunch) \
		{ \
			TypeA ParamA; \
			TypeB ParamB; \
			TypeC ParamC; \
			TypeD ParamD; \
			TypeE ParamE; \
			TypeF ParamF; \
			TypeG ParamG; \
			Receive(Bunch, ParamA, ParamB, ParamC, ParamD, ParamE, ParamF, ParamG); \
		} \
	};
#define DEFINE_CONTROL_CHANNEL_MESSAGE_EIGHTPARAM(Name, Index, TypeA, TypeB, TypeC, TypeD, TypeE, TypeF, TypeG, TypeH) \
	enum { NMT_##Name = Index }; \
	template<> class FNetControlMessage<Index> \
	{ \
	public: \
		static BYTE Initialize(); \
		/** sends a message of this type on the specified connection's control channel \
		 * @note: const not used only because of the FArchive interface; the parameters are not modified \
		 */ \
		static void Send(UNetConnection* Conn, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC, TypeD& ParamD, TypeE& ParamE, TypeF& ParamF, TypeG& ParamG, TypeH& ParamH) \
		{ \
			check(&Initialize != NULL); /** Stop the compiler optimizing-out Initialize */ \
			checkAtCompileTime(Index < 255, CONTROL_CHANNEL_MESSAGE_MUST_BE_BYTE); \
			checkSlow(!Conn->IsA(UChildConnection::StaticClass())); /** control channel messages can only be sent on the parent connection */ \
			if (Conn->Channels[0] != NULL && !Conn->Channels[0]->Closing) \
			{ \
				FControlChannelOutBunch Bunch(Conn->Channels[0], FALSE); \
				BYTE MessageType = Index; \
				Bunch << MessageType; \
				Bunch << ParamA; \
				Bunch << ParamB; \
				Bunch << ParamC; \
				Bunch << ParamD; \
				Bunch << ParamE; \
				Bunch << ParamF; \
				Bunch << ParamG; \
				Bunch << ParamH; \
				Conn->Channels[0]->SendBunch(&Bunch, TRUE); \
			} \
		} \
		static void Receive(FInBunch& Bunch, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC, TypeD& ParamD, TypeE& ParamE, TypeF& ParamF, TypeG& ParamG, TypeH& ParamH) \
		{ \
			Bunch << ParamA; \
			Bunch << ParamB; \
			Bunch << ParamC; \
			Bunch << ParamD; \
			Bunch << ParamE; \
			Bunch << ParamF; \
			Bunch << ParamG; \
			Bunch << ParamH; \
		} \
		/** throws away a message of this type from the passed in bunch */ \
		static void Discard(FInBunch& Bunch) \
		{ \
			TypeA ParamA; \
			TypeB ParamB; \
			TypeC ParamC; \
			TypeD ParamD; \
			TypeE ParamE; \
			TypeF ParamF; \
			TypeG ParamG; \
			TypeH ParamH; \
			Receive(Bunch, ParamA, ParamB, ParamC, ParamD, ParamE, ParamF, ParamG, ParamH); \
		} \
	};
#else

#define DEFINE_CONTROL_CHANNEL_MESSAGE_ZEROPARAM(Name, Index) \
	enum { NMT_##Name = Index }; \
	template<> class FNetControlMessage<Index> \
	{ \
	public: \
	static BYTE Initialize(); \
	static void Send(UNetConnection* Conn) {} \
	};
#define DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(Name, Index, TypeA) \
	enum { NMT_##Name = Index }; \
	template<> class FNetControlMessage<Index> \
	{ \
	public: \
	static BYTE Initialize(); \
	static void Send(UNetConnection* Conn, TypeA& ParamA) {} \
	static void Receive(FInBunch& Bunch, TypeA& ParamA) {} \
	static void Discard(FInBunch& Bunch) {} \
	};
#define DEFINE_CONTROL_CHANNEL_MESSAGE_TWOPARAM(Name, Index, TypeA, TypeB) \
	enum { NMT_##Name = Index }; \
	template<> class FNetControlMessage<Index> \
	{ \
	public: \
	static BYTE Initialize(); \
	static void Send(UNetConnection* Conn, TypeA& ParamA, TypeB& ParamB) {} \
	static void Receive(FInBunch& Bunch, TypeA& ParamA, TypeB& ParamB) {} \
	static void Discard(FInBunch& Bunch) {} \
};
#define DEFINE_CONTROL_CHANNEL_MESSAGE_THREEPARAM(Name, Index, TypeA, TypeB, TypeC) \
	enum { NMT_##Name = Index }; \
	template<> class FNetControlMessage<Index> \
	{ \
	public: \
	static BYTE Initialize(); \
	static void Send(UNetConnection* Conn, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC) {} \
	static void Receive(FInBunch& Bunch, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC) {} \
	static void Discard(FInBunch& Bunch) {} \
};
#define DEFINE_CONTROL_CHANNEL_MESSAGE_FOURPARAM(Name, Index, TypeA, TypeB, TypeC, TypeD) \
	enum { NMT_##Name = Index }; \
	template<> class FNetControlMessage<Index> \
	{ \
	public: \
	static BYTE Initialize(); \
	static void Send(UNetConnection* Conn, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC, TypeD& ParamD) {} \
	static void Receive(FInBunch& Bunch, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC, TypeD& ParamD) {} \
	static void Discard(FInBunch& Bunch) {} \
};
#define DEFINE_CONTROL_CHANNEL_MESSAGE_SEVENPARAM(Name, Index, TypeA, TypeB, TypeC, TypeD, TypeE, TypeF, TypeG) \
	enum { NMT_##Name = Index }; \
	template<> class FNetControlMessage<Index> \
	{ \
	public: \
	static BYTE Initialize(); \
	static void Send(UNetConnection* Conn, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC, TypeD& ParamD, TypeE& ParamE, TypeF& ParamF, TypeG& ParamG) {} \
	static void Receive(FInBunch& Bunch, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC, TypeD& ParamD, TypeE& ParamE, TypeF& ParamF, TypeG& ParamG) {} \
	static void Discard(FInBunch& Bunch) {} \
};
#define DEFINE_CONTROL_CHANNEL_MESSAGE_EIGHTPARAM(Name, Index, TypeA, TypeB, TypeC, TypeD, TypeE, TypeF, TypeG, TypeH) \
	enum { NMT_##Name = Index }; \
	template<> class FNetControlMessage<Index> \
	{ \
	public: \
	static BYTE Initialize(); \
	static void Send(UNetConnection* Conn, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC, TypeD& ParamD, TypeE& ParamE, TypeF& ParamF, TypeG& ParamG, TypeH& ParamH) {} \
	static void Receive(FInBunch& Bunch, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC, TypeD& ParamD, TypeE& ParamE, TypeF& ParamF, TypeG& ParamG, TypeH& ParamH) {} \
	static void Discard(FInBunch& Bunch) {} \
};

#endif


// Moved implementation of Initialize here, so that a linker error gets spat out when this second macro is forgotten

#if WITH_UE3_NETWORKING

#define IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Name) \
	BYTE FNetControlMessage<NMT_##Name>::Initialize() \
	{ \
		FNetControlMessageInfo::SetName(NMT_##Name, TEXT(#Name)); \
		return 0; \
	} \
	static BYTE Dummy##_FNetControlMessage_##Name = FNetControlMessage<NMT_##Name>::Initialize();

#else

#define IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Name) \
	BYTE FNetControlMessage<NMT_##Name>::Initialize() \
	{ \
		return 0; \
	} \
	static BYTE Dummy##_FNetControlMessage_##Name = FNetControlMessage<NMT_##Name>::Initialize();

#endif


// message type definitions
#if UDK
DEFINE_CONTROL_CHANNEL_MESSAGE_FOURPARAM(Hello, 0, INT, INT, UBOOL, FGuid); // initial client connection message (UPDATE: HandshakeStart is now the inital message)
#else
DEFINE_CONTROL_CHANNEL_MESSAGE_THREEPARAM(Hello, 0, INT, INT, UBOOL); // initial client connection message (UPDATE: HandshakeStart is now the inital message)
#endif // UDK
DEFINE_CONTROL_CHANNEL_MESSAGE_TWOPARAM(Welcome, 1, FString, FString); // server tells client they're ok'ed to load the server's level
DEFINE_CONTROL_CHANNEL_MESSAGE_TWOPARAM(Upgrade, 2, INT, INT); // server tells client their version is incompatible
DEFINE_CONTROL_CHANNEL_MESSAGE_TWOPARAM(Challenge, 3, INT, FString); // server sends client challenge string to verify integrity
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(Netspeed, 4, INT); // client sends requested transfer rate
DEFINE_CONTROL_CHANNEL_MESSAGE_THREEPARAM(Login, 5, FString, FString, FUniqueNetId); // client requests to be admitted to the game
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(Failure, 6, FString); // indicates connection failure
DEFINE_CONTROL_CHANNEL_MESSAGE_EIGHTPARAM(Uses, 7, FGuid, FString, FString, FString, DWORD, INT, FString, BYTE); // server tells client about a package they should have/acquire
DEFINE_CONTROL_CHANNEL_MESSAGE_TWOPARAM(Have, 8, FGuid, INT); // client tells server what version of a package it has
DEFINE_CONTROL_CHANNEL_MESSAGE_ZEROPARAM(Join, 9); // final join request (spawns PlayerController)
DEFINE_CONTROL_CHANNEL_MESSAGE_TWOPARAM(JoinSplit, 10, FUniqueNetId, FString); // child player (splitscreen) join request
DEFINE_CONTROL_CHANNEL_MESSAGE_THREEPARAM(DLMgr, 11, FString, FString, UBOOL); // server informs client of an available package download method
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(Skip, 12, FGuid); // client request to skip an optional package
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(Abort, 13, FGuid); // client informs server that it aborted a not-yet-verified package due to an UNLOAD request
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(Unload, 14, FGuid); // server tells client that a package is no longer needed
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(PCSwap, 15, INT); // client tells server it has completed a swap of its Connection->Actor
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(ActorChannelFailure, 16, INT); // client tells server that it failed to open an Actor channel sent by the server (e.g. couldn't serialize Actor archetype)
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(DebugText, 17, FString); // debug text sent to all clients or to server

/** Result of a peer requesting a join to another peer */
enum EPeerJoinResponse
{
	PeerJoin_Accepted=0,
	PeerJoin_Denied,
	PeerJoin_MAX
};

/** Info needed to connect with a client peer */
class FClientPeerInfo
{
public:
	/** Buffer containing platform-specific connection addr for the peer */
	TArray<BYTE> PlatformConnectAddr;
	/** Net id for the remote peer. So we can uniquely id its connection */
	FUniqueNetId PlayerId;
	/** ip address/port of peer */
	DWORD PeerIpAddrAsInt,PeerPort;
	
	/** 
	 * Constructor 
	 */
	FClientPeerInfo() 
		:	PlayerId(EC_EventParm)
		,	PeerIpAddrAsInt(0)
		,	PeerPort(0)
	{}
	
	/**
	 * Convert the remote ip:port for the client to string
	 *
	 * @param bAppendPort TRUE to add ":port" to string
	 * @return "ip:port" string for the peer connection 
	 */
	FString GetPeerConnectStr(UBOOL bAppendPort) const;
	
	/**
	 * @return TRUE if there is a valid IP addr for the client peer
	 */
	UBOOL IsValid() const
	{
		return PeerIpAddrAsInt != 0;
	}
	
	/** 
	 * Serialize FClientPeerInfo to archive to send in bunch packet 
	 *
	 * @param Ar the archive to serialize against
	 * @param Info struct to read/write with the Archive
	 */
	friend FArchive& operator<<(FArchive& Ar, FClientPeerInfo& Info);
};
/** New connected client tells server that it is listening for peer connections. Sends listening port */
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(PeerListen, 18, DWORD);
/** Server tells all clients about a new listening client peer that is ready for connection requests */
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(PeerConnect, 19, FClientPeerInfo);
/** Peer requesting to join another client peer */
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(PeerJoin, 20, FUniqueNetId);
/** Peer response to another peer requesting join */
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(PeerJoinResponse, 21, BYTE);
/** Peer notification to other peers about server disconnect */
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(PeerDisconnectHost, 22, FUniqueNetId);
/** Peer notification to other peers about a new host being selected during host migration */
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(PeerNewHostFound, 23, FUniqueNetId);

/** Travel to new host without a session */
class FClientPeerTravelInfo
{
public:
	/** Buffer containing platform-specific connection addr for the new host */
	TArray<BYTE> PlatformConnectAddr;
	/** ip address of new host */
	DWORD HostIpAddrAsInt;

	FClientPeerTravelInfo()
	{}

	/** 
	 * Serialize FClientPeerTravelInfo to archive to send in bunch packet 
	 *
	 * @param Ar the archive to serialize against
	 * @param Info struct to read/write with the Archive
	 */
	friend FArchive& operator<<(FArchive& Ar, FClientPeerTravelInfo& Info)
	{
		return Ar << Info.PlatformConnectAddr << Info.HostIpAddrAsInt;
	}
};
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(PeerNewHostTravel, 24, FClientPeerTravelInfo);

/** Travel to new host with a session */
class FClientPeerTravelSessionInfo
{
public:
	/** Buffer containing platform-specific session info for the new host */
	TArray<BYTE> PlatformSpecificInfo;
	/** Name of session to migrate/join */
	FString SessionName;
	/** Path of class to use when joining */
	FString SearchClassPath;

	FClientPeerTravelSessionInfo()
	{}

	/** 
	 * Serialize FClientPeerTravelInfo to archive to send in bunch packet 
	 *
	 * @param Ar the archive to serialize against
	 * @param Info struct to read/write with the Archive
	 */
	friend FArchive& operator<<(FArchive& Ar, FClientPeerTravelSessionInfo& Info)
	{
		return Ar << Info.PlatformSpecificInfo << Info.SessionName << Info.SearchClassPath;
	}
};
/** Peer notification to other peers about a new host being selected during host migration */
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(PeerNewHostTravelSession, 25, FClientPeerTravelSessionInfo);


/** Control-channel handshake, for validating the clients connection */
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(HandshakeStart, 26, BYTE); // Clients tells server to begin handshake process
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(HandshakeChallenge, 27, DWORD); // Servers sends client the handshake challenge
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(HandshakeResponse, 28, DWORD); // Client returns the handshake challenge
DEFINE_CONTROL_CHANNEL_MESSAGE_ZEROPARAM(HandshakeComplete, 29); // Server notifies the client that the handshake has completed

#if WITH_STEAMWORKS_SOCKETS
/** Steam-specific command (which can optionally be generalized) for redirecting clients from IP connections to Steam connections */
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(Redirect, 30, FString);
#endif // WITH_STEAMWORKS_SOCKETS

/** Auth interface control-channel messages */
DEFINE_CONTROL_CHANNEL_MESSAGE_FOURPARAM(ClientAuthRequest, 31, QWORD, DWORD, INT, UBOOL);
DEFINE_CONTROL_CHANNEL_MESSAGE_ZEROPARAM(ServerAuthRequest, 32);
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(AuthRequestPeer, 33, QWORD);
DEFINE_CONTROL_CHANNEL_MESSAGE_THREEPARAM(AuthBlob, 34, FString, BYTE, BYTE);
DEFINE_CONTROL_CHANNEL_MESSAGE_FOURPARAM(AuthBlobPeer, 35, QWORD, FString, BYTE, BYTE);
DEFINE_CONTROL_CHANNEL_MESSAGE_ZEROPARAM(ClientAuthEndSessionRequest, 36);
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(AuthKillPeer, 37, QWORD);
DEFINE_CONTROL_CHANNEL_MESSAGE_ZEROPARAM(AuthRetry, 38);

//
// A channel for exchanging connection control messages
//
class UControlChannel : public UChannel
{
	DECLARE_CLASS_INTRINSIC(UControlChannel,UChannel,CLASS_Transient,Engine);
	/**
	 * Used to interrogate the first packet received to determine endianess
	 * of the sending client
	 */
	UBOOL bNeedsEndianInspection;

	/** provies an extra buffer beyond RELIABLE_BUFFER for control channel messages
	 * as we must be able to guarantee delivery for them
	 * because they include package map updates and other info critical to client/server synchronization
	 */
	TArray< TArray<BYTE> > QueuedMessages;

	/** maximum size of additional buffer
	 * if this is exceeded as well, we kill the connection
	 */
	enum { MAX_QUEUED_CONTROL_MESSAGES = 256 };

	/**
	 * Inspects the packet for endianess information. Validates this information
	 * against what the client sent. If anything seems wrong, the connection is
	 * closed
	 *
	 * @param Bunch the packet to inspect
	 *
	 * @return TRUE if the packet is good, FALSE otherwise. FALSE means shutdown
	 */
	UBOOL CheckEndianess(FInBunch& Bunch);

	/** adds the given bunch to the QueuedMessages list. Closes the connection if MAX_QUEUED_CONTROL_MESSAGES is exceeded */
	void QueueMessage(const FOutBunch* Bunch);

	// Constructor.
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues()
	{
		ChannelClasses[CHTYPE_Control]      = GetClass();
		ChType								= CHTYPE_Control;
	}
	UControlChannel();
	void Init( UNetConnection* InConnection, INT InChIndex, UBOOL InOpenedLocally );

	// UChannel interface.
#if WITH_UE3_NETWORKING
	virtual INT SendBunch(FOutBunch* Bunch, UBOOL Merge);
	void ReceivedBunch( FInBunch& Bunch );
#endif	//#if WITH_UE3_NETWORKING
	virtual void Tick();

	// UControlChannel interface.
	FString Describe();
};

/*-----------------------------------------------------------------------------
	UActorChannel.
-----------------------------------------------------------------------------*/

/** struct containing property and offset for replicated actor properties */
struct FReplicatedActorProperty
{
	/** offset into the Actor where this reference is located - includes offsets from any outer structs */
	INT Offset;
	/** Reference to property object - primarily used for debug logging */
	const UObjectProperty* Property;

	FReplicatedActorProperty(INT InOffset, const UObjectProperty* InProperty)
		: Offset(InOffset), Property(InProperty)
	{}
};	

//
// A channel for exchanging actor properties.
//
class UActorChannel : public UChannel
{
	DECLARE_CLASS_INTRINSIC(UActorChannel,UChannel,CLASS_Transient,Engine);
	
	// Variables.
	UWorld*	World;			// World this actor channel is associated with.
	AActor* Actor;			// Actor this corresponds to.
	UClass* ActorClass;		// Class of the actor.
	DOUBLE	RelevantTime;	// Last time this actor was relevant to client.
	DOUBLE	LastUpdateTime;	// Last time this actor was replicated.
	BITFIELD SpawnAcked:1;	    // Whether spawn has been acknowledged.
	BITFIELD ActorDirty:1;		// Whether actor is dirty
	BITFIELD bActorMustStayDirty:1; // ActorDirty may not be cleared at end of this tick
	BITFIELD bActorStillInitial:1;	// Not all properties sent while bNetInitial, so still bNetInitial next tick
	BITFIELD bIsReplicatingActor:1; // true when in this channel's ReplicateActor() to avoid recursion as that can cause invalid data to be sent
	/** whether we should NULL references to this channel's Actor in other channels' Recent data when this channel is closed
	 * set to false in cases where the Actor can't become relevant again (e.g. destruction) as it's unnecessary in that case
	 */
	BITFIELD bClearRecentActorRefs:1;
	TArray<BYTE> Recent;	// Most recently sent values.
	TArray<BYTE> RepEval;	// Evaluated replication conditions.
	TArray<INT>  Dirty;     // Properties that are dirty and need resending.
	TArray<FPropertyRetirement> Retirement; // Property retransmission.
	/** list of replicated actor properties for ActorClass
	 * this is used to NULL Recent's references to Actors that lose relevancy
	 */
	TArray<FReplicatedActorProperty> ReplicatedActorProperties;

	// Constructor.
	void StaticConstructor();

	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	*/
	void InitializeIntrinsicPropertyValues()
	{
		ChannelClasses[CHTYPE_Actor]        = GetClass();
		ChType = CHTYPE_Actor;
		bClearRecentActorRefs = TRUE;
	}
	UActorChannel();
	void Init( UNetConnection* InConnection, INT InChIndex, UBOOL InOpenedLocally );

	// UChannel interface.
	virtual void SetClosingFlag();
#if WITH_UE3_NETWORKING
	virtual void ReceivedBunch( FInBunch& Bunch );
	virtual void ReceivedNak( INT NakPacketId );
#endif	//#if WITH_UE3_NETWORKING
	virtual void Close();

	// UActorChannel interface and accessors.
	AActor* GetActor() {return Actor;}
	FString Describe();
	void ReplicateActor();
	void SetChannelActor( AActor* InActor );

	/**
	* Tracks how much memory is being used by this object (no persistence)
	*
	* @param Ar the archive to serialize against
	*/
	virtual void Serialize(FArchive& Ar);

protected:
	/** cleans up channel structures and NULLs references to the channel */
	virtual void CleanUp();
};

/*-----------------------------------------------------------------------------
	File transfer channel.
-----------------------------------------------------------------------------*/

//
// A channel for exchanging binary files.
//
class UFileChannel : public UChannel
{
	DECLARE_CLASS_INTRINSIC(UFileChannel,UChannel,CLASS_Transient,Engine);

	// Receive Variables.
	UChannelDownload*	Download;		 // UDownload when receiving.

	// Send Variables.
	FArchive*			SendFileAr;		 // File being sent.
	TCHAR				SrcFilename[256];// Filename being sent.
	FGuid				PackageGUID;	 // GUID of the package, for looking up in PackageMap.
	INT					SentData;		 // Number of bytes sent.

	// Constructor.
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues()
	{
		ChannelClasses[CHTYPE_File]        = GetClass();
		ChType = CHTYPE_File;
	}
	UFileChannel();
	void Init( UNetConnection* InConnection, INT InChIndex, UBOOL InOpenedLocally );

	// UChannel interface.
#if WITH_UE3_NETWORKING
	virtual void ReceivedBunch( FInBunch& Bunch );
#endif	//#if WITH_UE3_NETWORKING

	// UFileChannel interface.
	FString Describe();
	void Tick();

protected:
	/** cleans up channel structures and NULLs references to the channel */
	virtual void CleanUp();
};

/**
 * This channel is responsible for passing voice data as part of our network data
 */
class UVoiceChannel : public UChannel
{
	DECLARE_CLASS_INTRINSIC(UVoiceChannel,UChannel,CLASS_Transient | 0,Engine);

	/** The set of voice packets for this channel */
	FVoicePacketList VoicePackets;

	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues()
	{
		ChannelClasses[CHTYPE_Voice] = GetClass();
		ChType = CHTYPE_Voice;
	}

	/**
	 * Adds the voice packet to the list to send for this channel
	 *
	 * @param VoicePacket the voice packet to send
	 */
	void AddVoicePacket(FVoicePacket* VoicePacket);

// UObject interface

	/**
	 * Tracks how much memory is being used by this object (no persistence)
	 *
	 * @param Ar the archive to serialize against
	 */
	virtual void Serialize(FArchive& Ar);

// UChannel interface
protected:
	/** Cleans up any voice data remaining in the queue */
	virtual void CleanUp(void);

#if WITH_UE3_NETWORKING
	/**
	 * Processes the in bound bunch to extract the voice data
	 *
	 * @param Bunch the voice data to process
	 */
	virtual void ReceivedBunch(FInBunch& Bunch);
#endif	//#if WITH_UE3_NETWORKING

#if !XBOX && !WITH_PANORAMA
	/**
	 * Performs any per tick update of the VoIP state
	 */
	virtual void Tick(void);
#endif

	/** Human readable information about the channel */
	virtual FString Describe(void)
	{
		return FString(TEXT("VoIP: ")) + UChannel::Describe();
	}
};

