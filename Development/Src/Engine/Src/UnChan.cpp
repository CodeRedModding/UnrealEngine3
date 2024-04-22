/*=============================================================================
	UnChan.cpp: Unreal datachannel implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnIpDrv.h"
#include "UnNet.h"

#if WITH_UE3_NETWORKING

#include "NetworkProfiler.h"

// passing NULL for UActorChannel will skip recent property update
static inline void SerializeCompressedInitial( FArchive& Bunch, FVector& Location, FRotator& Rotation, UBOOL bSerializeRotation, UActorChannel* Ch )
{
    // read/write compressed location
    Location.SerializeCompressed( Bunch );
    if( Ch && Ch->Recent.Num() )
        ((AActor*)&Ch->Recent(0))->Location = Location;

    // optionally read/write compressed rotation
    if( bSerializeRotation )
    {
		Rotation.SerializeCompressed( Bunch );
	    if( Ch && Ch->Recent.Num() )
            ((AActor*)&Ch->Recent(0))->Rotation = Rotation;
    }
}

#endif	//#if WITH_UE3_NETWORKING

/*-----------------------------------------------------------------------------
	UChannel implementation.
-----------------------------------------------------------------------------*/

//
// Initialize the base channel.
//
UChannel::UChannel()
{}
void UChannel::Init( UNetConnection* InConnection, INT InChIndex, UBOOL InOpenedLocally )
{
#if WITH_UE3_NETWORKING
	// if child connection then use its parent
	if (InConnection->GetUChildConnection() != NULL)
	{
		Connection = ((UChildConnection*)InConnection)->Parent;
	}
	else
	{
		Connection = InConnection;
	}
	ChIndex			= InChIndex;
	OpenedLocally	= InOpenedLocally;
	OpenPacketId	= INDEX_NONE;
	NegotiatedVer	= InConnection->NegotiatedVer;
#endif	//#if WITH_UE3_NETWORKING
}

//
// Set the closing flag.
//
void UChannel::SetClosingFlag()
{
	Closing = 1;
}

//
// Close the base channel.
//
void UChannel::Close()
{
#if WITH_UE3_NETWORKING
	check(Connection->Channels[ChIndex]==this);
	if
	(	!Closing
	&&	(Connection->State==USOCK_Open || Connection->State==USOCK_Pending) )
	{
		// Send a close notify, and wait for ack.
		FOutBunch CloseBunch( this, 1 );
		check(!CloseBunch.IsError());
		check(CloseBunch.bClose);
		CloseBunch.bReliable = 1;
		SendBunch( &CloseBunch, 0 );
	}
#endif	//#if WITH_UE3_NETWORKING
}

/** cleans up channel structures and NULLs references to the channel */
void UChannel::CleanUp()
{
#if WITH_UE3_NETWORKING
	checkSlow(Connection != NULL);
	checkSlow(Connection->Channels[ChIndex] == this);

	// if this is the control channel, make sure we properly killed the connection
	if (ChIndex == 0 && !Closing)
	{
		Connection->Close();
	}

	// remember sequence number of first non-acked outgoing reliable bunch for this slot
	if (OutRec != NULL)
	{
		Connection->PendingOutRec[ChIndex] = OutRec->ChSequence;
		//debugf(TEXT("%i save pending out bunch %i"),ChIndex,Connection->PendingOutRec[ChIndex]);
	}
	// Free any pending incoming and outgoing bunches.
	for (FOutBunch* Out = OutRec, *NextOut; Out != NULL; Out = NextOut)
	{
		NextOut = Out->Next;
		delete Out;
	}
	for (FInBunch* In = InRec, *NextIn; In != NULL; In = NextIn)
	{
		NextIn = In->Next;
		delete In;
	}

	// Remove from connection's channel table.
	verifySlow(Connection->OpenChannels.RemoveItem(this) == 1);
	Connection->Channels[ChIndex] = NULL;
#endif	//#if WITH_UE3_NETWORKING
	Connection = NULL;
}

//
// Base channel destructor.
//
void UChannel::BeginDestroy()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		ConditionalCleanUp();
	}
	
	Super::BeginDestroy();
}

#if WITH_UE3_NETWORKING
//
// Handle an acknowledgement on this channel.
//
void UChannel::ReceivedAcks()
{
	check(Connection->Channels[ChIndex]==this);

	/*
	// Verify in sequence.
	for( FOutBunch* Out=OutRec; Out && Out->Next; Out=Out->Next )
		check(Out->Next->ChSequence>Out->ChSequence);
	*/

	// Release all acknowledged outgoing queued bunches.
	UBOOL DoClose = 0;
	while( OutRec && OutRec->ReceivedAck )
	{
#if SUPPORT_SUPPRESSED_LOGGING
		if( OutRec->bReliable )
		{
			debugfSuppressed( NAME_DevNetTraffic, TEXT("   Received reliable ack, Channel %i Sequence %i: Packet %i"), OutRec->ChIndex, OutRec->ChSequence, OutRec->PacketId );
		}
#endif

		DoClose |= OutRec->bClose;
		FOutBunch* Release = OutRec;
		OutRec = OutRec->Next;
		delete Release;
		NumOutRec--;
	}

	// If a close has been acknowledged in sequence, we're done.
	if( DoClose || (OpenTemporary && OpenAcked) )
	{
		check(!OutRec);
		ConditionalCleanUp();
	}

}
#endif	//#if WITH_UE3_NETWORKING

//
// Return the maximum amount of data that can be sent in this bunch without overflow.
//
INT UChannel::MaxSendBytes()
{
#if WITH_UE3_NETWORKING
	INT ResultBits
	=	Connection->MaxPacket*8
	-	(Connection->Out.GetNumBits() ? 0 : MAX_PACKET_HEADER_BITS)
#if WITH_STEAMWORKS_SOCKETS
	-	((Connection->bUseSessionUID && Connection->Out.GetNumBits() == 0) ? SESSION_UID_BITS : 0)
#endif
	-	Connection->Out.GetNumBits()
	-	MAX_PACKET_TRAILER_BITS
	-	MAX_BUNCH_HEADER_BITS;
	return Max( 0, ResultBits/8 );
#else	//#if WITH_UE3_NETWORKING
	return 0;
#endif	//#if WITH_UE3_NETWORKING
}

//
// Handle time passing on this channel.
//
void UChannel::Tick()
{
#if WITH_UE3_NETWORKING
	checkSlow(Connection->Channels[ChIndex]==this);

	// Check if we have any reliable packets that have been blocked for too long
	// For some reason we are seeing this from time to time on iOS devices
	// Leave this failsafe in until we solve the underlying issue
	// @todo ib2merge: Should this maybe be IPHONE only, or maybe removed?
	if( NumInRec > 0 && Connection->Driver->Time - LastUnqueueTime > Connection->Driver->ConnectionTimeout )
	{
		// Timeout.
		if( Connection->State != USOCK_Closed )
		{
			debugf( NAME_DevNet, TEXT("%s Reliable packets blocked for %f seconds (%f)"), *Connection->Driver->GetName(), Connection->Driver->ConnectionTimeout, Connection->Driver->Time - LastUnqueueTime );
		}
		if (Connection->Driver->bIsPeer)
		{
			// Notify peer connection failure
			GEngine->SetProgress(PMT_PeerConnectionFailure,
				LocalizeError(TEXT("ConnectionFailed_Title"),TEXT("Engine")),
				LocalizeError(TEXT("ConnectionTimeout"),TEXT("Engine")));
				
		}
		else if (Connection->Actor != NULL)
		{
			// Let the player controller know why the connection was dropped
			Connection->Actor->eventClientSetProgressMessage(PMT_ConnectionFailure,
				LocalizeError(TEXT("ConnectionTimeout"),TEXT("Engine")),
				LocalizeError(TEXT("ConnectionFailed_Title"),TEXT("Engine")));
		}
		Connection->Close();
	}
#endif	//#if WITH_UE3_NETWORKING
}

//
// Make sure the incoming buffer is in sequence and there are no duplicates.
//
void UChannel::AssertInSequenced()
{
#if WITH_UE3_NETWORKING
#if DO_CHECK
	// Verify that buffer is in order with no duplicates.
	for( FInBunch* In=InRec; In && In->Next; In=In->Next )
		check(In->Next->ChSequence>In->ChSequence);
#endif
#endif	//#if WITH_UE3_NETWORKING
}

//
// Process a properly-sequenced bunch.
//
#if WITH_UE3_NETWORKING
UBOOL UChannel::ReceivedSequencedBunch( FInBunch& Bunch )
{
	// Note this bunch's retirement.
	if( Bunch.bReliable )
		Connection->InReliable[ChIndex] = Bunch.ChSequence;

	// Handle a regular bunch.
	if( !Closing )
		ReceivedBunch( Bunch );

	// We have fully received the bunch, so process it.
	if( Bunch.bClose )
	{
		// Handle a close-notify.
		if( InRec )
		{
			appErrorfDebug( TEXT("Close Anomaly %i / %i"), Bunch.ChSequence, InRec->ChSequence );
		}
		debugfSuppressed( NAME_DevNetTraffic, TEXT("      Channel %i got close-notify"), ChIndex );
		ConditionalCleanUp();
		return 1;
	}
	return 0;
}
#endif	//#if WITH_UE3_NETWORKING

#if WITH_UE3_NETWORKING
//
// Process a raw, possibly out-of-sequence bunch: either queue it or dispatch it.
// The bunch is sure not to be discarded.
//
void UChannel::ReceivedRawBunch( FInBunch& Bunch )
{
	check(Connection->Channels[ChIndex]==this);
	if
	(	Bunch.bReliable
	&&	Bunch.ChSequence!=Connection->InReliable[ChIndex]+1 )
	{
		// If this bunch has a dependency on a previous unreceived bunch, buffer it.
		checkSlow(!Bunch.bOpen);

		// Verify that UConnection::ReceivedPacket has passed us a valid bunch.
		check(Bunch.ChSequence>Connection->InReliable[ChIndex]);

		// Find the place for this item, sorted in sequence.
		debugfSuppressed( NAME_DevNetTraffic, TEXT("      Queuing bunch with unreceived dependency") );
		FInBunch** InPtr;
		for( InPtr=&InRec; *InPtr; InPtr=&(*InPtr)->Next )
		{
			if( Bunch.ChSequence==(*InPtr)->ChSequence )
			{
				// Already queued.
				return;
			}
			else if( Bunch.ChSequence<(*InPtr)->ChSequence )
			{
				// Stick before this one.
				break;
			}
		}
		FInBunch* New = new FInBunch(Bunch);
		New->Next     = *InPtr;
		*InPtr        = New;
		NumInRec++;
		checkSlow(NumInRec<=RELIABLE_BUFFER);
		//AssertInSequenced();
		if( NumInRec == 1 )
		{
			LastUnqueueTime = Connection->Driver->Time;
		}
	}
	else
	{
		// Receive it in sequence.
		UBOOL bDeleted = ReceivedSequencedBunch( Bunch );
		if( bDeleted )
		{
			return;
		}
		// Dispatch any waiting bunches.
		while( InRec )
		{
			if( InRec->ChSequence!=Connection->InReliable[ChIndex]+1 )
				break;
			debugfSuppressed( NAME_DevNetTraffic, TEXT("      Unleashing queued bunch, %d left"), NumInRec-1 );
			FInBunch* Release = InRec;
			InRec = InRec->Next;
			NumInRec--;
			bDeleted = ReceivedSequencedBunch( *Release );
			delete Release;
			if (bDeleted)
			{
				return;
			}
			//AssertInSequenced();
			LastUnqueueTime = Connection->Driver->Time;
		}
	}
}
#endif	//#if WITH_UE3_NETWORKING

#if WITH_UE3_NETWORKING
//
// Send a bunch if it's not overflowed, and queue it if it's reliable.
//
INT UChannel::SendBunch( FOutBunch* Bunch, UBOOL Merge )
{
	check(!Closing);
	check(Connection->Channels[ChIndex]==this);
	check(!Bunch->IsError());

	// Set bunch flags.
	if( OpenPacketId==INDEX_NONE && OpenedLocally )
	{
		Bunch->bOpen = 1;
		OpenTemporary = !Bunch->bReliable;
	}

	// If channel was opened temporarily, we are never allowed to send reliable packets on it.
	if( OpenTemporary )
		check(!Bunch->bReliable);

	// Contemplate merging.
	INT PreExistingBits = 0;
	FOutBunch* OutBunch = NULL;
	if
	(	Merge
	&&	Connection->LastOut.ChIndex==Bunch->ChIndex
	&&	Connection->AllowMerge
	&&	Connection->LastEnd.GetNumBits()
	&&	Connection->LastEnd.GetNumBits()==Connection->Out.GetNumBits()
	&&	Connection->Out.GetNumBytes()+Bunch->GetNumBytes()+(MAX_BUNCH_HEADER_BITS+MAX_PACKET_TRAILER_BITS+7)/8<=Connection->MaxPacket )
	{
		// Merge.
		check(!Connection->LastOut.IsError());
		PreExistingBits = Connection->LastOut.GetNumBits();
		Connection->LastOut.SerializeBits( Bunch->GetData(), Bunch->GetNumBits() );
		Connection->LastOut.bReliable |= Bunch->bReliable;
		Connection->LastOut.bOpen     |= Bunch->bOpen;
		Connection->LastOut.bClose    |= Bunch->bClose;
		OutBunch                       = Connection->LastOutBunch;
		Bunch                          = &Connection->LastOut;
		check(!Bunch->IsError());
		Connection->LastStart.Pop( Connection->Out );
		Connection->Driver->OutBunches--;
	}

	// Find outgoing bunch index.
	if( Bunch->bReliable )
	{
		// Find spot, which was guaranteed available by FOutBunch constructor.
		if( OutBunch==NULL )
		{
			check(NumOutRec<RELIABLE_BUFFER-1+Bunch->bClose);
			Bunch->Next	= NULL;
			Bunch->ChSequence = ++Connection->OutReliable[ChIndex];
			NumOutRec++;
			OutBunch = new FOutBunch(*Bunch);
			FOutBunch** OutLink;
			for( OutLink=&OutRec; *OutLink; OutLink=&(*OutLink)->Next );
			*OutLink = OutBunch;
		}
		else
		{
			Bunch->Next = OutBunch->Next;
			*OutBunch = *Bunch;
		}
		Connection->LastOutBunch = OutBunch;
	}
	else
	{
		OutBunch = Bunch;
		Connection->LastOutBunch = NULL;//warning: Complex code, don't mess with this!
	}

	NETWORK_PROFILER(GNetworkProfiler.TrackSendBunch(OutBunch,OutBunch->GetNumBits()-PreExistingBits));

	// Send the raw bunch.
	OutBunch->ReceivedAck = 0;
	INT PacketId = Connection->SendRawBunch(*OutBunch, Merge);
	if( OpenPacketId==INDEX_NONE && OpenedLocally )
		OpenPacketId = PacketId;
	if( OutBunch->bClose )
		SetClosingFlag();

	// Update channel sequence count.
	Connection->LastOut = *OutBunch;
	Connection->LastEnd	= FBitWriterMark(Connection->Out);

	return PacketId;
}
#endif	//#if WITH_UE3_NETWORKING

//
// Describe the channel.
//
FString UChannel::Describe()
{
	return FString(TEXT("State=")) + (Closing ? TEXT("closing") : TEXT("open") );
}

//
// Return whether this channel is ready for sending.
//
INT UChannel::IsNetReady( UBOOL Saturate )
{
#if WITH_UE3_NETWORKING
	// If saturation allowed, ignore queued byte count.
	if( NumOutRec>=RELIABLE_BUFFER-1 )
		return 0;
	return Connection->IsNetReady( Saturate );
#else	//#if WITH_UE3_NETWORKING
	return FALSE;
#endif	//#if WITH_UE3_NETWORKING
}

//
// Returns whether the specified channel type exists.
//
UBOOL UChannel::IsKnownChannelType( INT Type )
{
	return Type>=0 && Type<CHTYPE_MAX && ChannelClasses[Type];
}

#if WITH_UE3_NETWORKING
//
// Negative acknowledgement processing.
//
void UChannel::ReceivedNak( INT NakPacketId )
{
	for( FOutBunch* Out=OutRec; Out; Out=Out->Next )
	{
		// Retransmit reliable bunches in the lost packet.
		if( Out->PacketId==NakPacketId && !Out->ReceivedAck )
		{
			check(Out->bReliable);
			debugfSuppressed( NAME_DevNetTraffic, TEXT("      Channel %i nak; resending %i..."), Out->ChIndex, Out->ChSequence );
			Connection->SendRawBunch( *Out, 0 );
		}
	}
}
#endif	//#if WITH_UE3_NETWORKING

// UChannel statics.
UClass* UChannel::ChannelClasses[CHTYPE_MAX]={0,0,0,0,0,0,0,0};
IMPLEMENT_CLASS(UChannel)

/*-----------------------------------------------------------------------------
	UControlChannel implementation.
-----------------------------------------------------------------------------*/

const TCHAR* FNetControlMessageInfo::Names[255];

// control channel message implementation
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Hello);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Welcome);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Upgrade);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Challenge);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Netspeed);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Login);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Failure);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Uses);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Have);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Join);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(JoinSplit);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(DLMgr);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Skip);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Abort);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Unload);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(PCSwap);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(ActorChannelFailure);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(DebugText);
// peer control message implementations (only used for peer connections)
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(PeerListen);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(PeerConnect);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(PeerJoin);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(PeerJoinResponse);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(PeerDisconnectHost);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(PeerNewHostFound);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(PeerNewHostTravel);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(PeerNewHostTravelSession);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(HandshakeStart);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(HandshakeChallenge);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(HandshakeResponse);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(HandshakeComplete);
#if WITH_STEAMWORKS_SOCKETS
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Redirect);
#endif
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(ClientAuthRequest);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(ServerAuthRequest);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(AuthRequestPeer);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(AuthBlob);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(AuthBlobPeer);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(ClientAuthEndSessionRequest);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(AuthKillPeer);
IMPLEMENT_CONTROL_CHANNEL_MESSAGE(AuthRetry);

//
// Initialize the channel.
//
UControlChannel::UControlChannel()
{}
void UControlChannel::Init( UNetConnection* InConnection, INT InChannelIndex, UBOOL InOpenedLocally )
{
	Super::Init( InConnection, InChannelIndex, InOpenedLocally );

	// Assume all clients use same byte order. So, skip endian swapping handshake.
	UBOOL bIsPeerConnect = InConnection != NULL && InConnection->Driver != NULL && InConnection->Driver->bIsPeer;

	// If we are opened as a server connection, do the endian checking
	// The client assumes that the data will always have the correct byte order
	if (!InOpenedLocally && !bIsPeerConnect)
	{
		// Mark this channel as needing endianess determination
		bNeedsEndianInspection = TRUE;
	}
}

#if WITH_UE3_NETWORKING
/**
 * Inspects the packet for endianess information. Validates this information
 * against what the client sent. If anything seems wrong, the connection is
 * closed
 *
 * @param Bunch the packet to inspect
 *
 * @return TRUE if the packet is good, FALSE otherwise (closes socket)
 */
UBOOL UControlChannel::CheckEndianess(FInBunch& Bunch)
{
	// Assume the packet is bogus and the connection needs closing
	UBOOL bConnectionOk = FALSE;
	// Get pointers to the raw packet data
	const BYTE* HelloMessage = Bunch.GetData();
	// Check for a packet that is big enough to look at (message ID (1 byte) + platform identifier (1 byte))
	if (Bunch.GetNumBytes() >= 2)
	{
		// check for an old version client still using text messages; if we have one, send it an old format upgrade message
		if (Bunch.GetNumBytes() >= 13 &&
			HelloMessage[4] == 'H' &&
			HelloMessage[5] == 'E' &&
			HelloMessage[6] == 'L' &&
			HelloMessage[7] == 'L' &&
			HelloMessage[8] == 'O' &&
			HelloMessage[9] == ' ' &&
			HelloMessage[10] == 'P' &&
			HelloMessage[11] == '=')
		{
			FControlChannelOutBunch Bunch(this, FALSE);
			FString Response(FString::Printf(TEXT("UPGRADE MINVER=%i VER=%i"), GEngineMinNetVersion, GEngineVersion));
			Bunch << Response;
			SendBunch(&Bunch, TRUE);
			Connection->FlushNet();
		}
		else if (HelloMessage[0] == NMT_HandshakeStart)
		{
			// Get platform id
			UE3::EPlatformType OtherPlatform = (UE3::EPlatformType)(HelloMessage[1]);
			debugf(NAME_DevNet, TEXT("Remote platform is %d"), INT(OtherPlatform));

			// Check whether the other platform needs byte swapping by
			// using the value sent in the packet. Note: we still validate it
			if (appNetworkNeedsByteSwapping(OtherPlatform))
			{
#if NO_BYTE_ORDER_SERIALIZE && !WANTS_XBOX_BYTE_SWAPPING
				// The xbox doesn't swap ever
				debugf(NAME_Error, TEXT("Refusing PC client"));
				return FALSE;
#else
				// Client has opposite endianess so swap this bunch
				// and mark the connection as needing byte swapping
				Bunch.SetByteSwapping(TRUE);
				Connection->bNeedsByteSwapping = TRUE;
#endif
			}
			else
			{
				// Disable all swapping
				Bunch.SetByteSwapping(FALSE);
				Connection->bNeedsByteSwapping = FALSE;
			}

			// We parsed everything so keep the connection open
			bConnectionOk = TRUE;
			bNeedsEndianInspection = FALSE;
		}
	}

	return bConnectionOk;
}
#endif	//#if WITH_UE3_NETWORKING

#if WITH_UE3_NETWORKING
//
// Handle an incoming bunch.
//
void UControlChannel::ReceivedBunch( FInBunch& Bunch )
{
	check(!Closing);
	// If this is a new client connection inspect the raw packet for endianess
	if (bNeedsEndianInspection && !CheckEndianess(Bunch))
	{
		// Send close bunch and shutdown this connection
		Connection->Close();
		return;
	}

	// Process the packet
	while (!Bunch.AtEnd() && Connection != NULL && Connection->State != USOCK_Closed) // if the connection got closed, we don't care about the rest
	{
		BYTE MessageType;
		Bunch << MessageType;
		if (Bunch.IsError())
		{
			break;
		}
		INT Pos = Bunch.GetPosBits();
		if (Connection->Driver->bIsPeer)
		{
			// Process control message on peer connection 
			Connection->Driver->Notify->NotifyPeerControlMessage(Connection, MessageType, Bunch);
		}
		else
		{
			// If this is the server, handle the handshake process, and block control commands if the handshake is not complete
			// @todo JohnB: Eventually, add a 'HS_Complete' check to CreateChannel as well, to block channel creation before the handshake
			//		(not implemented yet, as it may block the voice channel)
			if (Connection->Driver->ServerConnection == NULL && Connection->HandshakeStatus != HS_Complete)
			{
				switch (MessageType)
				{
					case NMT_HandshakeStart:
					{
						// Ignore the message if we're not expecting it
						if (Connection->HandshakeStatus != HS_NotStarted)
						{
							break;
						}

						// Generate a random number as the challenge, maximizing its range of values
						Connection->HandshakeChallenge = (DWORD)RandHelper(255) |  ((DWORD)RandHelper(255) << 8) |
							((DWORD)RandHelper(255) << 16) | ((DWORD)RandHelper(255) << 24);

						Connection->HandshakeStatus = HS_SentChallenge;

						// Send the challenge
						FNetControlMessage<NMT_HandshakeChallenge>::Send(Connection, Connection->HandshakeChallenge);
						Connection->FlushNet();

#if WITH_STEAMWORKS_SOCKETS
						// Only set the session UID >AFTER< sending the challenge, otherwise the client blocks
						//	the packet, due to expecting a null-UID
						if (Connection->bUseSessionUID)
						{
							Connection->SessionUID[0] = (Connection->HandshakeChallenge & 0xFF000000) >> 24;
							Connection->SessionUID[1] = (Connection->HandshakeChallenge & 0x00FF0000) >> 16;
							Connection->SessionUID[2] = (Connection->HandshakeChallenge & 0x0000FF00) >> 8;

							debugfSuppressed(NAME_DevNet, TEXT("Server set session UID to: %i %i %i"),
								Connection->SessionUID[0], Connection->SessionUID[1], Connection->SessionUID[2]);
						}
#endif

						break;
					}

					case NMT_HandshakeResponse:
					{
						// Ignore the message if we're not expecting it
						if (Connection->HandshakeStatus != HS_SentChallenge)
						{
							break;
						}

						// Very weak hashing, but isn't really important; handshake is primarily to verify IP
						// @todo JohnB: Harden the hash creation later at some point, to make fake player exploits more difficult
						FString HashStr = FString::Printf(TEXT("895fcf626f55798667e4e94cb7a636af %d"),
											Connection->HandshakeChallenge);
						DWORD ExpectedResponse = appStrCrc(*HashStr);
						DWORD ChallengeResponse;

						// Check the challenge response
						FNetControlMessage<NMT_HandshakeResponse>::Receive(Bunch, ChallengeResponse);

						if (ChallengeResponse != ExpectedResponse)
						{
							Connection->Close();
							break;	
						}

#if WITH_STEAMWORKS_SOCKETS
						// If the current net driver is for redirecting clients, pass on to the Online Subsystem
						if (Connection->Driver->bRedirectDriver)
						{
							extern void appSteamHandleRedirect(UNetConnection* Connection);
							appSteamHandleRedirect(Connection);

							break;
						}
						// If it's a normal net driver, mark the handshake as complete and continue
						else
#endif
						{
							Connection->HandshakeStatus = HS_Complete;

							// Notify the client that the challenge succeeded
							FNetControlMessage<NMT_HandshakeComplete>::Send(Connection);
							Connection->FlushNet();
						}

						break;
					}

					default:
					{
						// Discard all other control messages
						break;
					}
				}
			}
			// we handle Actor channel failure notifications ourselves
			else if (MessageType == NMT_ActorChannelFailure)
			{
				if (Connection->Driver->ServerConnection == NULL)
				{
					debugf(NAME_DevNet, TEXT("Server connection received: %s"), FNetControlMessageInfo::GetName(MessageType));
					INT ChannelIndex;
					FNetControlMessage<NMT_ActorChannelFailure>::Receive(Bunch, ChannelIndex);
					if (ChannelIndex < ARRAY_COUNT(Connection->Channels))
					{
						UActorChannel* ActorChan = Cast<UActorChannel>(Connection->Channels[ChannelIndex]);
						if (ActorChan != NULL && ActorChan->Actor != NULL)
						{
							// if the client failed to initialize the PlayerController channel, the connection is broken
							if (ActorChan->Actor == Connection->Actor)
							{
								Connection->Close();
							}
							else if (Connection->Actor != NULL)
							{
								Connection->Actor->NotifyActorChannelFailure(ActorChan);
							}
						}
					}
				}
			}
			else
			{
				// Process control message on client/server connection
				Connection->Driver->Notify->NotifyControlMessage(Connection, MessageType, Bunch);
			}
		}
		// if the message was not handled, eat it ourselves
		if (Pos == Bunch.GetPosBits() && !Bunch.IsError())
		{
			switch (MessageType)
			{
				case NMT_Hello:
					FNetControlMessage<NMT_Hello>::Discard(Bunch);
					break;
				case NMT_Welcome:
					FNetControlMessage<NMT_Welcome>::Discard(Bunch);
					break;
				case NMT_Upgrade:
					FNetControlMessage<NMT_Upgrade>::Discard(Bunch);
					break;
				case NMT_Challenge:
					FNetControlMessage<NMT_Challenge>::Discard(Bunch);
					break;
				case NMT_Netspeed:
					FNetControlMessage<NMT_Netspeed>::Discard(Bunch);
					break;
				case NMT_Login:
					FNetControlMessage<NMT_Login>::Discard(Bunch);
					break;
				case NMT_Failure:
					FNetControlMessage<NMT_Failure>::Discard(Bunch);
					break;
				case NMT_Uses:
					FNetControlMessage<NMT_Uses>::Discard(Bunch);
					break;
				case NMT_Have:
					FNetControlMessage<NMT_Have>::Discard(Bunch);
					break;
				case NMT_Join:
					break;
				case NMT_JoinSplit:
					FNetControlMessage<NMT_JoinSplit>::Discard(Bunch);
					break;
				case NMT_DLMgr:
					FNetControlMessage<NMT_DLMgr>::Discard(Bunch);
					break;
				case NMT_Skip:
					FNetControlMessage<NMT_Skip>::Discard(Bunch);
					break;
				case NMT_Abort:
					FNetControlMessage<NMT_Abort>::Discard(Bunch);
					break;
				case NMT_Unload:
					FNetControlMessage<NMT_Unload>::Discard(Bunch);
					break;
				case NMT_PCSwap:
					FNetControlMessage<NMT_PCSwap>::Discard(Bunch);
					break;
				case NMT_ActorChannelFailure:
					FNetControlMessage<NMT_ActorChannelFailure>::Discard(Bunch);
					break;
				case NMT_DebugText:
					FNetControlMessage<NMT_DebugText>::Discard(Bunch);
					break;
				case NMT_PeerListen:
					FNetControlMessage<NMT_PeerListen>::Discard(Bunch);
					break;
				case NMT_PeerConnect:
					FNetControlMessage<NMT_PeerConnect>::Discard(Bunch);
					break;
				case NMT_PeerJoin:
					FNetControlMessage<NMT_PeerJoin>::Discard(Bunch);
					break;
				case NMT_PeerJoinResponse:
					FNetControlMessage<NMT_PeerJoinResponse>::Discard(Bunch);
					break;
				case NMT_PeerDisconnectHost:
					FNetControlMessage<NMT_PeerDisconnectHost>::Discard(Bunch);
					break;
				case NMT_PeerNewHostFound:
					FNetControlMessage<NMT_PeerNewHostFound>::Discard(Bunch);
					break;
				case NMT_PeerNewHostTravel:
					FNetControlMessage<NMT_PeerNewHostTravel>::Discard(Bunch);
					break;
				case NMT_PeerNewHostTravelSession:
					FNetControlMessage<NMT_PeerNewHostTravelSession>::Discard(Bunch);
					break;
				case NMT_HandshakeStart:
					FNetControlMessage<NMT_HandshakeStart>::Discard(Bunch);
					break;
				case NMT_HandshakeChallenge:
					FNetControlMessage<NMT_HandshakeChallenge>::Discard(Bunch);
					break;
				case NMT_HandshakeResponse:
					FNetControlMessage<NMT_HandshakeResponse>::Discard(Bunch);
					break;
				case NMT_HandshakeComplete:
					break;
#if WITH_STEAMWORKS_SOCKETS
				case NMT_Redirect:
					FNetControlMessage<NMT_Redirect>::Discard(Bunch);
					break;
#endif
				case NMT_ClientAuthRequest:
					FNetControlMessage<NMT_ClientAuthRequest>::Discard(Bunch);
					break;
				case NMT_ServerAuthRequest:
					break;
				case NMT_AuthRequestPeer:
					FNetControlMessage<NMT_AuthRequestPeer>::Discard(Bunch);
					break;
				case NMT_AuthBlob:
					FNetControlMessage<NMT_AuthBlob>::Discard(Bunch);
					break;
				case NMT_AuthBlobPeer:
					FNetControlMessage<NMT_AuthBlobPeer>::Discard(Bunch);
					break;
				case NMT_ClientAuthEndSessionRequest:
					break;
				case NMT_AuthKillPeer:
					FNetControlMessage<NMT_AuthKillPeer>::Discard(Bunch);
					break;
				case NMT_AuthRetry:
					break;
				default:
					check(!FNetControlMessageInfo::IsRegistered(MessageType)); // if this fails, a case is missing above for an implemented message type

					debugf(NAME_Error, TEXT("Received unknown control channel message"));
					appErrorfDebug(TEXT("Failed to read control channel message %i"), INT(MessageType));
					Connection->Close();
					return;
			}
		}
		if (Bunch.IsError())
		{
			debugf(NAME_Error, TEXT("Failed to read control channel message '%s'"), FNetControlMessageInfo::GetName(MessageType));
			appErrorfDebug(TEXT("Failed to read control channel message"));
			Connection->Close();
			break;
		}
	}
}
#endif	//#if WITH_UE3_NETWORKING

/** adds the given string to the QueuedMessages list. Closes the connection if MAX_QUEUED_CONTROL_MESSAGES is exceeded */
void UControlChannel::QueueMessage(const FOutBunch* Bunch)
{
#if WITH_UE3_NETWORKING
	if (QueuedMessages.Num() >= MAX_QUEUED_CONTROL_MESSAGES)
	{
		// we're out of room in our extra buffer as well, so kill the connection
		debugf(NAME_DevNet, TEXT("Overflowed control channel message queue, disconnecting client"));
		// intentionally directly setting State as the messaging in Close() is not going to work in this case
		Connection->State = USOCK_Closed;
	}
	else
	{
		INT Index = QueuedMessages.AddZeroed();
		QueuedMessages(Index).Add(Bunch->GetNumBytes());
		appMemcpy(QueuedMessages(Index).GetTypedData(), Bunch->GetData(), Bunch->GetNumBytes());
	}
#endif	//#if WITH_UE3_NETWORKING
}

#if WITH_UE3_NETWORKING
INT UControlChannel::SendBunch(FOutBunch* Bunch, UBOOL Merge)
{
	// if we already have queued messages, we need to queue subsequent ones to guarantee proper ordering
	if (QueuedMessages.Num() > 0 || NumOutRec >= RELIABLE_BUFFER - 1 + Bunch->bClose)
	{
		QueueMessage(Bunch);
		return INDEX_NONE;
	}
	else
	{
		if (!Bunch->IsError())
		{
			return Super::SendBunch(Bunch, Merge);
		}
		else
		{
			// an error here most likely indicates an unfixable error, such as the text using more than the maximum packet size
			// so there is no point in queueing it as it will just fail again
			appErrorfDebug(TEXT("Control channel bunch overflowed"));
			debugf(NAME_Error, TEXT("Control channel bunch overflowed"));
			Connection->Close();
			return INDEX_NONE;
		}
	}
}
#endif	//#if WITH_UE3_NETWORKING

void UControlChannel::Tick()
{
	Super::Tick();

#if WITH_UE3_NETWORKING
	if( !OpenAcked )
	{
		INT Count = 0;
		for( FOutBunch* Out=OutRec; Out; Out=Out->Next )
			if( !Out->ReceivedAck )
				Count++;
		if ( Count > 8 )
			return;
		// Resend any pending packets if we didn't get the appropriate acks.
		for( FOutBunch* Out=OutRec; Out; Out=Out->Next )
		{
			if( !Out->ReceivedAck )
			{
				FLOAT Wait = Connection->Driver->Time-Out->Time;
				checkSlow(Wait>=0.f);
				if( Wait>1.f )
				{
					debugfSuppressed( NAME_DevNetTraffic, TEXT("Channel %i ack timeout; resending %i..."), ChIndex, Out->ChSequence );
					check(Out->bReliable);
					Connection->SendRawBunch( *Out, 0 );
				}
			}
		}
	}
	else
	{
		// attempt to send queued messages
		while (QueuedMessages.Num() > 0 && !Closing)
		{
			FControlChannelOutBunch Bunch(this, 0);
			if (Bunch.IsError())
			{
				break;
			}
			else
			{
				Bunch.bReliable = 1;
				Bunch.Serialize(QueuedMessages(0).GetData(), QueuedMessages(0).Num());
				if (!Bunch.IsError())
				{
					Super::SendBunch(&Bunch, 1);
					QueuedMessages.Remove(0, 1);
				}
				else
				{
					// an error here most likely indicates an unfixable error, such as the text using more than the maximum packet size
					// so there is no point in queueing it as it will just fail again
					appErrorfDebug(TEXT("Control channel bunch overflowed"));
					debugf(NAME_Error, TEXT("Control channel bunch overflowed"));
					Connection->Close();
					break;
				}
			}
		}
	}
#endif	//#if WITH_UE3_NETWORKING
}

//
// Describe the text channel.
//
FString UControlChannel::Describe()
{
	return FString(TEXT("Text ")) + UChannel::Describe();
}

IMPLEMENT_CLASS(UControlChannel);

/*-----------------------------------------------------------------------------
	UActorChannel.
-----------------------------------------------------------------------------*/

//
// Initialize this actor channel.
//
UActorChannel::UActorChannel()
{}
void UActorChannel::Init( UNetConnection* InConnection, INT InChannelIndex, UBOOL InOpenedLocally )
{
	Super::Init( InConnection, InChannelIndex, InOpenedLocally );
#if WITH_UE3_NETWORKING
	World			= Connection->Driver->Notify->NotifyGetWorld();
	RelevantTime	= Connection->Driver->Time;
	LastUpdateTime	= Connection->Driver->Time - Connection->Driver->SpawnPrioritySeconds;
#endif	//#if WITH_UE3_NETWORKING
	ActorDirty = TRUE;
	bActorMustStayDirty = FALSE;
	bActorStillInitial = FALSE;
}

//
// Set the closing flag.
//
void UActorChannel::SetClosingFlag()
{
	if( Actor )
		Connection->ActorChannels.Remove( Actor );
	UChannel::SetClosingFlag();
}

//
// Close it.
//
void UActorChannel::Close()
{
	UChannel::Close();
	if (Actor != NULL)
	{
		// SetClosingFlag() might have already done this, but we need to make sure as that won't get called if the connection itself has already been closed
		Connection->ActorChannels.Remove(Actor);

		if (!Actor->IsStatic() && !Actor->bNoDelete && bClearRecentActorRefs)
		{
			// if transient actor lost relevance, clear references to it from other channels' Recent data to mirror what happens on the other side of the connection
			// so that if it becomes relevant again, we know we need to resend those references
			for (TMap<AActor*, UActorChannel*>::TIterator It(Connection->ActorChannels); It; ++It)
			{
				UActorChannel* Chan = It.Value();
				if (Chan != NULL && Chan->Actor != NULL && !Chan->Closing && Chan->Recent.Num() > 0)
				{
					for (INT j = 0; j < Chan->ReplicatedActorProperties.Num(); j++)
					{
						AActor** ActorRef = (AActor**)((BYTE*)Chan->Recent.GetData() + Chan->ReplicatedActorProperties(j).Offset);
						if (*ActorRef == Actor)
						{
							*ActorRef = NULL;
							Chan->ActorDirty = TRUE;
							debugfSuppressed( NAME_DevNetTraffic, TEXT("Clearing Recent ref to irrelevant Actor %s from channel %i (property %s, Actor %s)"), *Actor->GetName(), Chan->ChIndex,
										*Chan->ReplicatedActorProperties(j).Property->GetPathName(), *Chan->Actor->GetName() );
						}
					}
				}
			}
		}

		Actor = NULL;
	}
}

void UActorChannel::StaticConstructor()
{
#if WITH_UE3_NETWORKING
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference(STRUCT_OFFSET(UActorChannel,Actor));
	TheClass->EmitObjectReference(STRUCT_OFFSET(UActorChannel,ActorClass));

	const DWORD SkipIndexIndex = TheClass->EmitStructArrayBegin(STRUCT_OFFSET(UActorChannel, ReplicatedActorProperties), sizeof(FReplicatedActorProperty));
	TheClass->EmitObjectReference(STRUCT_OFFSET(FReplicatedActorProperty, Property));
	TheClass->EmitStructArrayEnd(SkipIndexIndex);
#endif	//#if WITH_UE3_NETWORKING
}

/** cleans up channel structures and NULLs references to the channel */
void UActorChannel::CleanUp()
{
	// Remove from hash and stuff.
	SetClosingFlag();

	// Destroy Recent properties.
	if (Recent.Num() > 0)
	{
		checkSlow(ActorClass != NULL);
		UObject::ExitProperties(&Recent(0), ActorClass);
	}

	// If we're the client, destroy this actor.
	if (Connection->Driver->ServerConnection != NULL)
	{
		check(Actor == NULL || Actor->IsValid());
		checkSlow(Connection != NULL);
		checkSlow(Connection->IsValid());
		checkSlow(Connection->Driver != NULL);
		checkSlow(Connection->Driver->IsValid());
		if (Actor != NULL)
		{
			if (Actor->bTearOff)
			{
				Actor->Role = ROLE_Authority;
				Actor->RemoteRole = ROLE_None;
			}
			else if (!Actor->bNetTemporary && GWorld != NULL && !GIsRequestingExit)
			{
				if (Actor->bNoDelete)
				{
					// can't destroy bNoDelete actors, but send a separate notification to them so they can perform any cleanup
					// since their replicated properties will be left in an arbitrary state
					Actor->eventReplicationEnded();
				}
				else
				{
					// if actor should be destroyed and isn't already, then destroy it
					GWorld->DestroyActor(Actor, 1);
				}
			}
		}
	}
#if WITH_UE3_NETWORKING
	else if (Actor && !OpenAcked)
	{
		// Resend temporary actors if nak'd.
		Connection->SentTemporaries.RemoveItem(Actor);
	}
#endif	//#if WITH_UE3_NETWORKING

	Super::CleanUp();
}

#if WITH_UE3_NETWORKING
//
// Negative acknowledgements.
//
void UActorChannel::ReceivedNak( INT NakPacketId )
{
	UChannel::ReceivedNak(NakPacketId);
	if( ActorClass )
		for( INT i=Retirement.Num()-1; i>=0; i-- )
			if( Retirement(i).OutPacketId==NakPacketId && !Retirement(i).Reliable )
				Dirty.AddUniqueItem(i);
    ActorDirty = true; 
}
#endif	//#if WITH_UE3_NETWORKING

/** internal helper function for adding Actor properties inside structs to the ReplicatedActorProperties array - @see SetChannelActor() */
static void AddActorPropertiesFromStruct(const UStructProperty* StructProp, INT BaseOffset, TArray<FReplicatedActorProperty>& ReplicatedActorProperties)
{
	checkSlow(StructProp != NULL);
	checkSlow(StructProp->Struct != NULL);

	BaseOffset += StructProp->Offset;
	for (INT i = 0; i < StructProp->ArrayDim; i++)
	{
		for (UProperty* Prop = StructProp->Struct->PropertyLink; Prop != NULL; Prop = Prop->PropertyLinkNext)
		{
			UObjectProperty* ObjectProp = Cast<UObjectProperty>(Prop, CLASS_IsAUObjectProperty);
			if (ObjectProp != NULL)
			{
				if (ObjectProp->PropertyClass != NULL && ObjectProp->PropertyClass->IsChildOf(AActor::StaticClass()))
				{
					new(ReplicatedActorProperties) FReplicatedActorProperty(BaseOffset + i * StructProp->ElementSize + ObjectProp->Offset, ObjectProp);
				}
			}
			else
			{
				UStructProperty* InnerStructProp = Cast<UStructProperty>(Prop, CLASS_IsAUStructProperty);
				if (InnerStructProp != NULL)
				{
					AddActorPropertiesFromStruct(InnerStructProp, BaseOffset + i * StructProp->ElementSize, ReplicatedActorProperties);
				}
			}
		}
	}
}

//
// Allocate replication tables for the actor channel.
//
void UActorChannel::SetChannelActor( AActor* InActor )
{
	check(!Closing);
	check(Actor==NULL);

	// Set stuff.
	Actor                      = InActor;
	ActorClass                 = Actor->GetClass();
	FClassNetCache* ClassCache = Connection->PackageMap->GetClassNetCache( ActorClass );

	if ( Connection->PendingOutRec[ChIndex] > 0 )
	{
		// send empty reliable bunches to synchronize both sides
		// debugf(TEXT("%i Actor %s WILL BE sending %i vs first %i"), ChIndex, *Actor->GetName(), Connection->PendingOutRec[ChIndex],Connection->OutReliable[ChIndex]);
		INT RealOutReliable = Connection->OutReliable[ChIndex];
		Connection->OutReliable[ChIndex] = Connection->PendingOutRec[ChIndex] - 1;
		while ( Connection->PendingOutRec[ChIndex] <= RealOutReliable )
		{
			// debugf(TEXT("%i SYNCHRONIZING by sending %i"), ChIndex, Connection->PendingOutRec[ChIndex]);

#if WITH_UE3_NETWORKING
			FOutBunch Bunch( this, 0 );
			if( !Bunch.IsError() )
			{
				Bunch.bReliable = true;
				SendBunch( &Bunch, 0 );
				Connection->PendingOutRec[ChIndex]++;
			}
#endif	//#if WITH_UE3_NETWORKING
		}

		Connection->OutReliable[ChIndex] = RealOutReliable;
		Connection->PendingOutRec[ChIndex] = 0;
	}


	// Add to map.
	Connection->ActorChannels.Set( Actor, this );

	// Allocate replication condition evaluation cache.
	RepEval.AddZeroed( ClassCache->GetRepConditionCount() );

	// Init recent properties.
	if( !InActor->bNetTemporary )
	{
		// Allocate recent property list.
		INT Size = ActorClass->GetDefaultsCount();
		// use Reserve() first so we allocate exactly as much as we need
		Recent.Reserve(Size);
		Recent.Add( Size );
		UObject::InitProperties( &Recent(0), Size, ActorClass, NULL, 0 );
		BYTE* DefaultData = NULL;
		INT DefaultCount = 0;
		if (Actor->GetArchetype())
		{
			DefaultData = (BYTE*)Actor->GetArchetype();
			DefaultCount = Size;
		}
		UObject::InitProperties( &Recent(0), Size, ActorClass, DefaultData, DefaultCount );
	}

	// Allocate retirement list.
	Retirement.Empty( ActorClass->ClassReps.Num() );
	while( Retirement.Num()<ActorClass->ClassReps.Num() )
		new(Retirement)FPropertyRetirement;

	// figure out list of replicated actor properties
	for (UProperty* Prop = ActorClass->PropertyLink; Prop != NULL; Prop = Prop->PropertyLinkNext)
	{
		if (Prop->PropertyFlags & CPF_Net)
		{
			UObjectProperty* ObjectProp = Cast<UObjectProperty>(Prop, CLASS_IsAUObjectProperty);
			if (ObjectProp != NULL)
			{
				if (ObjectProp->PropertyClass != NULL && ObjectProp->PropertyClass->IsChildOf(AActor::StaticClass()))
				{
					for (INT i = 0; i < ObjectProp->ArrayDim; i++)
					{
						new(ReplicatedActorProperties) FReplicatedActorProperty(ObjectProp->Offset + i * ObjectProp->ElementSize, ObjectProp);
					}
				}
			}
			else
			{
				UStructProperty* StructProp = Cast<UStructProperty>(Prop, CLASS_IsAUStructProperty);
				if (StructProp != NULL)
				{
					AddActorPropertiesFromStruct(StructProp, 0, ReplicatedActorProperties);
				}
			}
		}
	}
}

#if WITH_UE3_NETWORKING
//
// Handle receiving a bunch of data on this actor channel.
//
void UActorChannel::ReceivedBunch( FInBunch& Bunch )
{
	check(!Closing);

	if ( Broken || bTornOff )
	{
		return;
	}

	// Initialize client if first time through.
	INT bJustSpawned = 0;
	FClassNetCache* ClassCache = NULL;
	if( Actor==NULL )
	{
		if( !Bunch.bOpen )
		{
			debugfSuppressed(NAME_DevNetTraffic, TEXT("New actor channel received non-open packet: %i/%i/%i"),Bunch.bOpen,Bunch.bClose,Bunch.bReliable);
			return;
		}

		// Read class.
		UObject* Object;
		Bunch << Object;
		AActor* InActor = Cast<AActor>( Object );
		if (InActor == NULL)
		{
			// We are unsynchronized. Instead of crashing, let's try to recover.
			debugf(TEXT("Received invalid actor class on channel %i"), ChIndex);
			Broken = 1;
			FNetControlMessage<NMT_ActorChannelFailure>::Send(Connection, ChIndex);
			return;
		}
		if (InActor->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			// Transient actor.
			FVector Location;
			FRotator Rotation(0,0,0);
			SerializeCompressedInitial(Bunch, Location, Rotation, InActor->bNetInitialRotation, NULL);

			InActor = GWorld->SpawnActor(InActor->GetClass(), NAME_None, Location, Rotation, InActor, 1, 1, NULL, NULL, 1); 
			bJustSpawned = 1;
			if ( !InActor )
			{
				debugf(TEXT("Couldn't spawn %s replicated from server"), *ActorClass->GetName());
			}
			else if ( InActor->bDeleteMe )
			{
				debugf(TEXT("Client received and deleted instantly %s"),*InActor->GetName());
			}
			check(InActor);
		}
		debugfSuppressed( NAME_DevNetTraffic, TEXT("      Spawn %s:"), *InActor->GetFullName() );
		SetChannelActor( InActor );

		// if it's a PlayerController, attempt to match it to a local viewport
		APlayerController* PC = Actor->GetAPlayerController();
		if (PC != NULL)
		{
			Bunch << PC->NetPlayerIndex;
			if (Connection->Driver != NULL && Connection == Connection->Driver->ServerConnection)
			{
				if (PC->NetPlayerIndex == 0)
				{
					// main connection PlayerController
					Connection->HandleClientPlayer(PC); 
				}
				else
				{
					INT ChildIndex = INT(PC->NetPlayerIndex) - 1;
					if (Connection->Children.IsValidIndex(ChildIndex))
					{
						// received a new PlayerController for an already existing child
						Connection->Children(ChildIndex)->HandleClientPlayer(PC);
					}
					else
					{
						// create a split connection on the client
						UChildConnection* Child = Connection->Driver->CreateChild(Connection); 
						Child->HandleClientPlayer(PC);
						debugf(NAME_DevNet, TEXT("Client received PlayerController=%s. Num child connections=%i."), *Actor->GetName(), Connection->Children.Num());
					}
				}
			}
		}
	}
	else
	{
		debugfSuppressed( NAME_DevNetTraffic, TEXT("      Actor %s:"), *Actor->GetFullName() );
	}
	ClassCache = Connection->PackageMap->GetClassNetCache(ActorClass);
	checkSlow(ClassCache != NULL);

	// Owned by connection's player?
	Actor->bNetOwner = 0;
	APlayerController* Top = Actor->GetTopPlayerController();
	if(Top)
	{
		UPlayer* Player = Top ? Top->Player : NULL;
		// Set quickie replication variables.
		if( Connection->Driver->ServerConnection )
		{
			// We are the client.
			if( Player && Player->IsA( ULocalPlayer::StaticClass() ) )
			{
				Actor->bNetOwner = TRUE;
			}
		}
		else
		{
			// We are the server.
			if (Player == Connection || (Player != NULL && Player->IsA(UChildConnection::StaticClass()) && ((UChildConnection*)Player)->Parent == Connection))
			{
				Actor->bNetOwner = TRUE;
			}
		}
	}

	// Handle the data stream.
	INT             RepIndex   = Bunch.ReadInt( ClassCache->GetMaxIndex() );
	FFieldNetCache* FieldCache = Bunch.IsError() ? NULL : ClassCache->GetFromIndex( RepIndex );
	const UBOOL bIsServer = (Connection->Driver->ServerConnection == NULL);
	UFunction* RepNotifyFunc = NULL;

	// Made a inline allocator to relieve malloc pressure
	TArray<UProperty*,TInlineAllocator<32> > RepNotifies;

	while (FieldCache || bJustSpawned)
	{
		// Save current properties.k
		//debugf(TEXT("Rep %s: %i"),FieldCache->Field->GetFullName(),RepIndex);
		UBOOL bHasReplicatedProperties = ((FieldCache != NULL) && Cast<UProperty>(FieldCache->Field, CLASS_IsAUProperty) != NULL);
		if (bHasReplicatedProperties && !bIsServer)
		{
			Actor->PreNetReceive();
		}

		// Receive properties from the net.
		// NOTE: Even though servers do not support receiving replicated properties, they must be processed anyway
		//			(this is because RPC's may be merged into the same bunch as variables, and servers need to execute them)
		RepNotifies.Reset();
		UProperty* ReplicatedProp;
		while( FieldCache && (ReplicatedProp=Cast<UProperty>(FieldCache->Field,CLASS_IsAUProperty))!=NULL )
		{
			// Server shouldn't receive properties.
			if (bIsServer)
			{
				debugfSlow( TEXT("Server received unwanted property value %s in %s"), *ReplicatedProp->GetName(), *Actor->GetFullName() );
			}


			// Whether or not to discard the current replicated property (servers do by default anyway)
			UBOOL bDiscard = bIsServer;

			// Receive array index.
			BYTE Element=0;
			if( ReplicatedProp->ArrayDim != 1 )
			{
				Bunch << Element;

				if (Element >= ReplicatedProp->ArrayDim)
				{
					Element = ReplicatedProp->ArrayDim-1;
					bDiscard = TRUE;
				}
			}

			// Check property ordering.
			if (!bDiscard)
			{
				FPropertyRetirement& Retire = Retirement( ReplicatedProp->RepIndex + Element );

				if( Bunch.PacketId>=Retire.InPacketId ) //!! problem with reliable pkts containing dynamic references, being retransmitted, and overriding newer versions. Want "OriginalPacketId" for retransmissions?
				{
					// Receive this new property.
					Retire.InPacketId = Bunch.PacketId;
				}
				else
				{
					// Skip this property, because it's out-of-date.
					debugfSuppressed( NAME_DevNetTraffic, TEXT("Received out-of-date %s"), *ReplicatedProp->GetName() );
					bDiscard = TRUE;
				}
			}

			// Pointer to destiation.
			BYTE* DestActor  = NULL;
			BYTE* DestRecent = NULL;

			if (!bDiscard)
			{
				DestActor = (BYTE*)Actor;
				DestRecent = Recent.Num() ? &Recent(0) : NULL;
			}

			// Receive property.
			FMemMark Mark(GMainThreadMemStack);
			INT   Offset = ReplicatedProp->Offset + Element*ReplicatedProp->ElementSize;
			BYTE* Data   = DestActor ? (DestActor + Offset) : NewZeroed<BYTE>(GMainThreadMemStack,ReplicatedProp->ElementSize);
			ReplicatedProp->NetSerializeItem( Bunch, Connection->PackageMap, Data );
			if( DestRecent )
			{
				ReplicatedProp->CopySingleValue( DestRecent + Offset, Data );
			}
			Mark.Pop();
			// Successfully received it.
			debugfSuppressed( NAME_DevNetTraffic, TEXT("         %s"), *ReplicatedProp->GetName() );

			// Notify the actor if this var is RepNotify
			if ( ReplicatedProp->HasAnyPropertyFlags(CPF_RepNotify) )
			{
				//@note: AddUniqueItem() here for static arrays since RepNotify() currently doesn't indicate index,
				//			so reporting the same property multiple times is not useful and wastes CPU
				//			were that changed, this should go back to AddItem() for efficiency
				RepNotifies.AddUniqueItem(ReplicatedProp);
			}

			// Get next.
			RepIndex   = Bunch.ReadInt( ClassCache->GetMaxIndex() );
			FieldCache = Bunch.IsError() ? NULL : ClassCache->GetFromIndex( RepIndex );
		}
		
		// Process important changed properties.
		if (!bIsServer)
		{
			if ( bHasReplicatedProperties )
			{
				Actor->PostNetReceive();
				if (Actor == NULL || Actor->bDeleteMe)
				{
					// PostNetReceive() destroyed Actor
					return;
				}

				if ( RepNotifies.Num() > 0 )
				{
					if ( RepNotifyFunc == NULL )
					{
						RepNotifyFunc = Actor->FindFunctionChecked(ENGINE_ReplicatedEvent);
					}

					Actor_eventReplicatedEvent_Parms RepNotifyParms(EC_EventParm);
					for (INT RepNotifyIdx = 0; RepNotifyIdx < RepNotifies.Num(); RepNotifyIdx++)
					{
						//debugf( TEXT("Calling Actor->eventReplicatedEvent with %s"), RepNotifies(RepNotifyIdx)->GetName());
						RepNotifyParms.VarName = RepNotifies(RepNotifyIdx)->GetFName();
						Actor->ProcessEvent(RepNotifyFunc, &RepNotifyParms);
						if (Actor == NULL || Actor->bDeleteMe)
						{
							// script event destroyed Actor
							return;
						}
					}
				}
			}
			bJustSpawned = 0;
		}

		// Handle function calls.
		if( FieldCache && Cast<UFunction>(FieldCache->Field,CLASS_IsAUFunction) )
		{
			FName Message = FieldCache->Field->GetFName();
			UFunction* Function = Actor->FindFunction( Message );
			check(Function);

			debugfSuppressed( NAME_DevNetTraffic, TEXT("      Received RPC: %s"), *Message.ToString() );

			// Get the parameters.
			FMemMark Mark(GMainThreadMemStack);
			BYTE* Parms = new(GMainThreadMemStack,MEM_Zeroed,Function->ParmsSize)BYTE;
			for( TFieldIterator<UProperty> Itr(Function); Itr && (Itr->PropertyFlags & (CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++Itr )
			{
				if( Connection->PackageMap->SupportsObject(*Itr) )
				{
					if( Cast<UBoolProperty>(*Itr,CLASS_IsAUBoolProperty) || Bunch.ReadBit() ) 
					{
						for (INT i = 0; i < Itr->ArrayDim; i++)
						{
							Itr->NetSerializeItem(Bunch, Connection->PackageMap, Parms + Itr->Offset + (i * Itr->ElementSize));
						}
					}
				}
			}

			// validate that the function is callable here
			UBOOL bCanExecute = ( (Function->FunctionFlags & FUNC_Net) && (Function->FunctionFlags & (Connection->Driver->ServerConnection ? FUNC_NetClient : FUNC_NetServer)) &&
									(Connection->Driver->ServerConnection != NULL || Actor->bNetOwner) );
			if (bCanExecute)
			{
				// Call the function.
				Actor->ProcessEvent( Function, Parms );
				//debugfSuppressed( NAME_DevNetTraffic, TEXT("      Call RPC: %s"), *Function->GetName() );
			}
			else
			{
				debugf(NAME_DevNet, TEXT("Rejected unwanted function %s in %s"), *Message.ToString(), *Actor->GetFullName());
			}

			// Destroy the parameters.
			//warning: highly dependent on UObject::ProcessEvent freeing of parms!
			{for( UProperty* Destruct=Function->ConstructorLink; Destruct; Destruct=Destruct->ConstructorLinkNext )
				if( Destruct->Offset < Function->ParmsSize )
					Destruct->DestroyValue( Parms + Destruct->Offset );}
			Mark.Pop();

			if (Actor == NULL || Actor->bDeleteMe)
			{
				// replicated function destroyed Actor
				return;
			}

			// Next.
			RepIndex   = Bunch.ReadInt( ClassCache->GetMaxIndex() );
			FieldCache = Bunch.IsError() ? NULL : ClassCache->GetFromIndex( RepIndex );
		}
		else if( FieldCache )
		{
			appErrorfDebug( TEXT("Invalid replicated field %i in %s"), RepIndex, *Actor->GetFullName() );
			return;
		}
	}
	// Tear off an actor on the client-side
	if ( Actor && Actor->bTearOff && GWorld->GetNetMode() == NM_Client )
	{
		Actor->Role = ROLE_Authority;
		Actor->RemoteRole = ROLE_None;
		bTornOff = true;
		Connection->ActorChannels.Remove( Actor );
		Actor->eventTornOff();
		Actor = NULL;
	}
}
#endif	//#if WITH_UE3_NETWORKING

//
// Replicate this channel's actor differences.
//
void UActorChannel::ReplicateActor()
{
#if WITH_UE3_NETWORKING
	checkSlow(Actor);
	checkSlow(!Closing);

	// triggering replication of an Actor while already in the middle of replication can result in invalid data being sent and is therefore illegal
	if (bIsReplicatingActor)
	{
		FString Error(FString::Printf(TEXT("Attempt to replicate '%s' while already replicating that Actor!"), *Actor->GetName()));
		debugf(*Error);
		appErrorfDebug(*Error);
		return;
	}

	// Create an outgoing bunch, and skip this actor if the channel is saturated.
	FOutBunch Bunch( this, 0 );
	if( Bunch.IsError() )
		return;

	bIsReplicatingActor = TRUE;

	// Send initial stuff.
	if( OpenPacketId!=INDEX_NONE )
	{
		Actor->bNetInitial = 0;
		if( !SpawnAcked && OpenAcked )
		{
			// After receiving ack to the spawn, force refresh of all subsequent unreliable packets, which could
			// have been lost due to ordering problems. Note: We could avoid this by doing it in FActorChannel::ReceivedAck,
			// and avoid dirtying properties whose acks were received *after* the spawn-ack (tricky ordering issues though).
			SpawnAcked = 1;
			for( INT i=Retirement.Num()-1; i>=0; i-- )
				if( Retirement(i).OutPacketId!=INDEX_NONE && !Retirement(i).Reliable )
					Dirty.AddUniqueItem(i);
		}
	}
	else
	{
		Actor->bNetInitial = 1;
		Bunch.bClose    =  Actor->bNetTemporary;
		Bunch.bReliable = !Actor->bNetTemporary;
	}
	// Get class network info cache.
	FClassNetCache* ClassCache = Connection->PackageMap->GetClassNetCache(Actor->GetClass());
	check(ClassCache);

	// Owned by connection's player?
	Actor->bNetOwner = 0;
	APlayerController* Top = Actor->GetTopPlayerController();
	UPlayer* Player = Top ? Top->Player : NULL;

	// Set quickie replication variables.
	UBOOL bDemoOwner = 0;
	if (Actor->bDemoRecording)
	{
		Actor->bNetOwner = 1;
		bDemoOwner = (Actor->WorldInfo->NetMode == NM_Client) ? (Cast<ULocalPlayer>(Player) != NULL) : Actor->bDemoOwner;
	}
	else
	{
		Actor->bNetOwner = Connection->Driver->ServerConnection ? Cast<ULocalPlayer>(Player)!=NULL : Player==Connection;
		// use child connection's parent
		if (Connection->Driver->ServerConnection == NULL &&	Player != NULL && Player->IsA(UChildConnection::StaticClass()) &&
			((UChildConnection*)Player)->Parent == Connection )
		{
			Actor->bNetOwner = 1;
		}
	}
#if CLIENT_DEMO
	Actor->bRepClientDemo = Actor->bNetOwner && Top && Top->bClientDemo;
#endif

	// If initial, send init data.
	if( Actor->bNetInitial && OpenedLocally )
	{
		if (Actor->IsStatic() || Actor->bNoDelete)
		{
			// Persistent actor.
			Bunch << Actor;
		}
		else
		{
			// Transient actor.
			
			// check for conditions that would result in the client being unable to properly receive this Actor
			if (Actor->GetArchetype()->GetClass() != Actor->GetClass())
			{
				debugf( NAME_Warning, TEXT("Attempt to replicate %s with archetype class (%s) different from Actor class (%s)"),
						*Actor->GetName(), *Actor->GetArchetype()->GetClass()->GetName(), *Actor->GetClass()->GetName() );
			}

			// serialize it
			UObject* Archetype = Actor->GetArchetype();
			Bunch << Archetype;
			SerializeCompressedInitial( Bunch, Actor->Location, Actor->Rotation, Actor->bNetInitialRotation, this );

			// serialize PlayerIndex as part of the initial bunch for PlayerControllers so they can be matched to the correct clientside viewport
			APlayerController* PC = Actor->GetAPlayerController();
			if (PC != NULL)
			{
				Bunch << PC->NetPlayerIndex;
			}
		}
	}

	// Save out the actor's RemoteRole, and downgrade it if necessary.
	BYTE ActualRemoteRole=Actor->RemoteRole;
	if (Actor->RemoteRole==ROLE_AutonomousProxy && (((Actor->Instigator == NULL || !Actor->Instigator->bNetOwner) && !Actor->bNetOwner) || (Actor->bDemoRecording && !bDemoOwner)))
	{
		Actor->RemoteRole=ROLE_SimulatedProxy;
	}

	Actor->bNetDirty = ActorDirty || Actor->bNetInitial;
	Actor->bNetInitial = Actor->bNetInitial || bActorStillInitial; // for replication purposes, bNetInitial stays true until all properties sent
	bActorMustStayDirty = false;
	bActorStillInitial = false;

	debugfSuppressed(NAME_DevNetTrafficDetail, TEXT("Replicate %s, bNetDirty: %d, bNetInitial: %d, bNetOwner: %d"), *Actor->GetName(), Actor->bNetDirty, Actor->bNetInitial, Actor->bNetOwner );
	NETWORK_PROFILER(GNetworkProfiler.TrackReplicateActor(Actor));

	// Get memory for retirement list.
	FMemMark MemMark(GMainThreadMemStack);
	appMemzero( &RepEval(0), RepEval.Num() );
	INT* Reps = New<INT>( GMainThreadMemStack, Retirement.Num() ), *LastRep;
	UBOOL		FilledUp = 0;

	// Figure out which properties to replicate.
	BYTE*   CompareBin = NULL;
	if (Recent.Num())
	{
		CompareBin = &Recent(0);
	}
	else
	{
		if (Actor->GetArchetype())
		{
			CompareBin = (BYTE*)Actor->GetArchetype();
		}
		else
		{
			CompareBin = ActorClass->GetDefaults();
		}
	}

	INT     iCount     = ClassCache->RepProperties.Num();
	LastRep            = Actor->GetOptimizedRepList( CompareBin, &Retirement(0), Reps, Connection->PackageMap,this );
	if ( Actor->bNetDirty )
	{
		//if ( iCount > 0 ) debugf(TEXT("%s iCount %d"),Actor->GetName(), iCount);
		for( INT iField=0; iField<iCount; iField++  )
		{
			FFieldNetCache* FieldCache = ClassCache->RepProperties(iField);
			UProperty* It = (UProperty*)(FieldCache->Field);  
			BYTE& Eval = RepEval(FieldCache->ConditionIndex);
			if( Eval!=2 )
			{
				UObjectProperty* Op = Cast<UObjectProperty>(It,CLASS_IsAUObjectProperty);
				for( INT Index=0; Index<It->ArrayDim; Index++ )
				{
					// Evaluate need to send the property.
					INT Offset = It->Offset + Index*It->ElementSize;
					BYTE* Src = (BYTE*)Actor + Offset;
					if( Op && !Connection->PackageMap->CanSerializeObject(*(UObject**)Src) )
					{
						if (!(*(UObject**)Src)->IsPendingKill())
						{
							if( !(Eval & 2) )
							{
								checkSlow(It->GetRepOwner());
								DWORD Val=0;
								FFrame( Actor, It->GetOwnerClass(), It->RepOffset, NULL ).Step( Actor, &Val );
								Eval = Val | 2;
							}
							if( Eval & 1 )
							{
								debugfSuppressed(NAME_DevNetTraffic,TEXT("MUST STAY DIRTY Because of %s"),*(*(UObject**)Src)->GetName());
								bActorMustStayDirty = true;
							}
						}
						Src = NULL;
					}
					if ((OpenPacketId == INDEX_NONE && (It->PropertyFlags & CPF_Config)) || !It->Identical(CompareBin + Offset, Src))
					{
						if( !(Eval & 2) )
						{
							checkSlow(It->GetRepOwner());
							DWORD Val=0;
							FFrame( Actor, It->GetOwnerClass(), It->RepOffset, NULL ).Step( Actor, &Val );
							Eval = Val | 2;
						}
						if( Eval & 1 )
						{
							*LastRep++ = It->RepIndex+Index;
						}
					}
				}
			}
		}
	}
	if (Actor->bNetInitial)
	{
		// add forced initial replicated properties to dirty list
		const TArray<UProperty*>* PropArray = Connection->Driver->ForcedInitialReplicationMap.Find(Actor);
		if (PropArray != NULL)
		{
			for (INT i = 0; i < PropArray->Num(); i++)
			{
				for (INT j = 0; j < (*PropArray)(i)->ArrayDim; j++)
				{
					Dirty.AddItem((*PropArray)(i)->RepIndex + j);
				}
			}
		}
	}
	check(!Bunch.IsError());

	// Add dirty properties to list.
	for( INT i=Dirty.Num()-1; i>=0; i-- )
	{
		INT D=Dirty(i);
		INT* R;
		for( R=Reps; R<LastRep; R++ )
			if( *R==D )
				break;
		if( R==LastRep )
			*LastRep++=D;
	}
	TArray<INT>  StillDirty;

	// Replicate those properties.
	for( INT* iPtr=Reps; iPtr<LastRep; iPtr++ )
	{
		// Get info.
		FRepRecord* Rep    = &ActorClass->ClassReps(*iPtr);
		UProperty*	It     = Rep->Property;
		INT         Index  = Rep->Index;
		INT         Offset = It->Offset + Index*It->ElementSize;

		if (It->ArrayDim != 1 && Index > 255)
		{
			debugf(NAME_DevNet, TEXT("Failed to replicate element '%i/%i' of property '%s' in class '%s'; can only replicate elements 0-255"), Index, It->ArrayDim-1, *It->GetName(), *ActorClass->GetName());
			continue;
		}

		// Figure out field to replicate.
		FFieldNetCache* FieldCache
		=	It->GetFName()==NAME_Role
		?	ClassCache->GetFromField(Connection->Driver->RemoteRoleProperty)
		:	It->GetFName()==NAME_RemoteRole
		?	ClassCache->GetFromField(Connection->Driver->RoleProperty)
		:	ClassCache->GetFromField(It);
		checkSlow(FieldCache);

		// Send property name and optional array index.
		INT BitsWrittenBeforeThis = Bunch.GetNumBits();
		FBitWriterMark WriterMark( Bunch );
		Bunch.WriteIntWrapped(FieldCache->FieldNetIndex, ClassCache->GetMaxIndex());
		if( It->ArrayDim != 1 )
		{
			BYTE Element = Index;
			Bunch << Element;
		}

		// Send property.
		UBOOL Mapped = It->NetSerializeItem( Bunch, Connection->PackageMap, (BYTE*)Actor + Offset );
		if( !Bunch.IsError() )
		{
			INT BitsWritten = Bunch.GetNumBits() - BitsWrittenBeforeThis;
			NETWORK_PROFILER(GNetworkProfiler.TrackReplicateProperty(It,BitsWritten));
			debugfSuppressed( NAME_DevNetTraffic, TEXT("   Send %s [%.1f bytes] %s"), *It->GetName(), BitsWritten / 8.f, Mapped ? TEXT("Mapped") : TEXT("") );

			// Update recent value.
			if( Recent.Num() )
			{
				if( Mapped )
					It->CopySingleValue( &Recent(Offset), (BYTE*)Actor + Offset );
				else
					StillDirty.AddUniqueItem(*iPtr);
			}
		}
		else
		{
			// Stop the changes because we overflowed.
			WriterMark.Pop( Bunch );
			LastRep  = iPtr;
			//debugfSuppressed(NAME_DevNetTraffic,TEXT("FILLED UP"));
			FilledUp = 1;
			break;
		}
	}
	// If not empty, send and mark as updated.
	if( Bunch.GetNumBits() )
	{
		INT PacketId = SendBunch( &Bunch, 1 );
		for( INT* Rep=Reps; Rep<LastRep; Rep++ )
		{
			Dirty.RemoveItem(*Rep);
			FPropertyRetirement& Retire = Retirement(*Rep);
			Retire.OutPacketId = PacketId;
			Retire.Reliable    = Bunch.bReliable;
		}
		if( Actor->bNetTemporary )
		{
			Connection->SentTemporaries.AddItem( Actor );
		}
	}
	for ( INT i=0; i<StillDirty.Num(); i++ )
		Dirty.AddUniqueItem(StillDirty(i));

	// If we evaluated everything, mark LastUpdateTime, even if nothing changed.
	if ( FilledUp )
	{
		debugfSuppressed(NAME_DevNetTraffic,TEXT("Filled packet up before finishing %s still initial %d"),*Actor->GetName(),bActorStillInitial);
	}
	else
	{
		LastUpdateTime = Connection->Driver->Time;
	}

	bActorStillInitial = Actor->bNetInitial && (FilledUp || (!Actor->bNetTemporary && bActorMustStayDirty));
	ActorDirty = bActorMustStayDirty || FilledUp;

	// Reset temporary net info.
	Actor->bNetOwner  = 0;
	Actor->RemoteRole = ActualRemoteRole;
	Actor->bNetDirty = 0;

	MemMark.Pop();

	bIsReplicatingActor = FALSE;
#endif	//#if WITH_UE3_NETWORKING
}

//
// Describe the actor channel.
//
FString UActorChannel::Describe()
{
	if( Closing || !Actor )
		return FString(TEXT("Actor=None ")) + UChannel::Describe();
	else
		return FString::Printf(TEXT("Actor=%s (Role=%i RemoteRole=%i) "), *Actor->GetFullName(), Actor->Role, Actor->RemoteRole) + UChannel::Describe();
}

/**
* Tracks how much memory is being used by this object (no persistence)
*
* @param Ar the archive to serialize against
*/
void UActorChannel::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	
	if (Ar.IsCountingMemory())
	{
		Recent.CountBytes(Ar);
		RepEval.CountBytes(Ar);
		Dirty.CountBytes(Ar);
		Retirement.CountBytes(Ar);
		ReplicatedActorProperties.CountBytes(Ar);
	}
}

IMPLEMENT_CLASS(UActorChannel);

/*-----------------------------------------------------------------------------
	UFileChannel implementation.
-----------------------------------------------------------------------------*/

UFileChannel::UFileChannel()
	: Download(NULL)
	, SendFileAr(NULL)
	, PackageGUID(0,0,0,0)
	, SentData(0)
{
}
void UFileChannel::Init( UNetConnection* InConnection, INT InChannelIndex, UBOOL InOpenedLocally )
{
	Super::Init( InConnection, InChannelIndex, InOpenedLocally );
}

#if WITH_UE3_NETWORKING
void UFileChannel::ReceivedBunch( FInBunch& Bunch )
{
	check(!Closing);
	if( OpenedLocally )
	{
		// Receiving a file sent from the other side.  If Bunch.GetNumBytes()==0, it means the server refused to send the file.
		Download->ReceiveData( Bunch.GetData(), Bunch.GetNumBytes() );
	}
	else
	{
#if IPHONE
		// Currently, we do not allow downloading on iDevices
		// Just execute the 'AllowDownloads == false' handling code...
#else
		if( !Connection->Driver->AllowDownloads )
#endif
		{
			// Refuse the download by sending a 0 bunch.
			debugf( NAME_DevNet, *LocalizeError(TEXT("NetInvalid"),TEXT("Engine")) );
			FOutBunch Bunch( this, 1 );
			SendBunch( &Bunch, 0 );
			return;
		}
#if IPHONE
		// Currently, we do not allow downloading on iDevices
		// So don't bother compiling the remaining code
#else
		if( SendFileAr )
		{
			FString Cmd;
			Bunch << Cmd;

			if (!Bunch.IsError() && Cmd==TEXT("SKIP"))
			{
				if (PackageGUID.IsValid())
				{
					INT PackageIndex = INDEX_NONE;

					for (INT i=0; i<Connection->PackageMap->List.Num(); i++)
					{
						if (Connection->PackageMap->List(i).Guid == PackageGUID)
						{
							PackageIndex = i;
							break;
						}
					}

					// User cancelled optional file download.
					// Remove it from the package map
					if (PackageIndex != INDEX_NONE)
					{
						debugf(NAME_DevNet, TEXT("User skipped download of '%s'"), SrcFilename);
						Connection->PackageMap->List.Remove( PackageIndex );
					}

					// Reset PackageGUID now that the package is removed
					PackageGUID.Invalidate();
				}

				return;
			}
		}
		else
		{
			// Request to send a file.
			FGuid Guid;
			Bunch << Guid;

			if( !Bunch.IsError() )
			{
				FPackageInfo* Info = NULL;

				for( INT i=0; i<Connection->PackageMap->List.Num(); i++ )
				{
					FPackageInfo& CurInfo = Connection->PackageMap->List(i);

					if (CurInfo.Guid == Guid && CurInfo.PackageName != NAME_None)
					{
						Info = &CurInfo;
						break;
					}
				}

				if (Info != NULL)
				{
					FString ServerPackage;

					if (GPackageFileCache->FindPackageFile(*Info->PackageName.ToString(), NULL, ServerPackage))
					{
						// Check that the file does not exceed MaxDownloadSize, and that it does not have its 'AllowDownload' package flag set to false
						if ((Connection->Driver->MaxDownloadSize <= 0 || (GFileManager->FileSize(*ServerPackage) <= Connection->Driver->MaxDownloadSize)) &&
								(Info->PackageFlags & PKG_AllowDownload) != 0)
						{
							// find the path to the source package file
							appStrncpy(SrcFilename, *ServerPackage, ARRAY_COUNT(SrcFilename));
							if( Connection->Driver->Notify->NotifySendingFile( Connection, Guid ) )
							{
								SendFileAr = GFileManager->CreateFileReader( SrcFilename );
								if( SendFileAr )
								{
									// Accepted! Now initiate file sending.
									debugf( NAME_DevNet, LocalizeSecure(LocalizeProgress(TEXT("NetSend"),TEXT("Engine")), SrcFilename) );
									PackageGUID = Guid;

									return;
								}
							}
						}
					}
					else
					{
						debugf(NAME_DevNet, TEXT("ERROR: Server failed to find a package file that it reported was in its PackageMap [%s]"), *Info->PackageName.ToString());
					}
				}
			}
		}

		// Illegal request; refuse it by closing the channel.
		debugf( NAME_DevNet, *LocalizeError(TEXT("NetInvalid"),TEXT("Engine")) );
		FOutBunch Bunch( this, 1 );
		SendBunch( &Bunch, 0 );
#endif
	}
}
#endif	//#if WITH_UE3_NETWORKING

void UFileChannel::Tick()
{
	UChannel::Tick();
#if WITH_UE3_NETWORKING
	Connection->TimeSensitive = 1;
	INT Size;
	//TIM: IsNetReady(1) causes the client's bandwidth to be saturated. Good for clients, very bad
	// for bandwidth-limited servers. IsNetReady(0) caps the clients bandwidth.
	static UBOOL LanPlay = ParseParam(appCmdLine(),TEXT("lanplay"));

	// JohnB: Added !Closing check, as otherwise SendBunch can crash if the client closes first
	while( !Closing && !OpenedLocally && SendFileAr && IsNetReady(LanPlay) && (Size=MaxSendBytes())!=0 )
	{
		// Sending.
		// get the filesize (we can't use the PackageInfo's size, because this may be a streaming texture pacakge
		// and so the size will be zero
		INT FileSize = SendFileAr->TotalSize();

		INT Remaining = (sizeof(INT) + FileSize) - SentData;
		FOutBunch Bunch( this, Size>=Remaining );
		Size = Min( Size, Remaining );
		BYTE* Buffer = (BYTE*)appAlloca( Size );

		// first chunk gets total size prepended to it
		if (SentData == 0)
		{
			// put it in the buffer
			appMemcpy(Buffer, &FileSize, sizeof(INT));

			// read a little less then Size of the source data
			SendFileAr->Serialize(Buffer + sizeof(INT), Size - sizeof(INT));
		}
		else
		{
			// normal case, just read Size amount
			SendFileAr->Serialize(Buffer, Size);
		}

		if( SendFileAr->IsError() )
		{
			//!!
		}
		SentData += Size;
		Bunch.Serialize( Buffer, Size );
		Bunch.bReliable = 1;
		check(!Bunch.IsError());
		SendBunch( &Bunch, 0 );
		Connection->FlushNet();
		if( Bunch.bClose )
		{
			// Finished.
			delete SendFileAr;
			SendFileAr = NULL;
		}
	}
#endif	//#if WITH_UE3_NETWORKING
}

/** cleans up channel structures and NULLs references to the channel */
void UFileChannel::CleanUp()
{
	// Close the file.
	if (SendFileAr != NULL)
	{
		delete SendFileAr;
		SendFileAr = NULL;
	}

	// Notify that the receive succeeded or failed.
	if (OpenedLocally && Download != NULL)
	{
		Download->DownloadDone();
		Download->CleanUp();
	}

	Super::CleanUp();
}

FString UFileChannel::Describe()
{
	return FString::Printf
	(
		TEXT("File='%s', %s=%i "),
		OpenedLocally ? (Download?Download->TempFilename:TEXT("")): SrcFilename,
		OpenedLocally ? TEXT("Received") : TEXT("Sent"),
		OpenedLocally ? (Download?Download->Transfered:0): SentData
	) + UChannel::Describe();
}
IMPLEMENT_CLASS(UFileChannel)


IMPLEMENT_CLASS(UVoiceChannel)

/**
 * Serializes the voice packet data into/from an archive
 *
 * @param Ar the archive to serialize with
 * @param VoicePacket the voice data to serialize
 */
FArchive& operator<<(FArchive& Ar,FVoicePacket& VoicePacket)
{
	Ar << (QWORD&)VoicePacket.Sender;
	Ar << VoicePacket.Length;
	// Make sure not to overflow the buffer by reading an invalid amount
	if (Ar.IsLoading())
	{
		// Verify the packet is a valid size
		if (VoicePacket.Length <= MAX_VOICE_DATA_SIZE)
		{
			Ar.Serialize(VoicePacket.Buffer,VoicePacket.Length);
		}
		else
		{
			VoicePacket.Length = 0;
		}
	}
	else
	{
		// Always safe to save the data as the voice code prevents overwrites
		Ar.Serialize(VoicePacket.Buffer,VoicePacket.Length);
	}
	return Ar;
}

#if WITH_UE3_NETWORKING
/**
 * Processes the in bound bunch to extract the voice data
 *
 * @param Bunch the voice data to process
 */
void UVoiceChannel::ReceivedBunch(FInBunch& Bunch)
{
	while (Bunch.IsError() == FALSE)
	{
		// Construct a new voice packet with ref counting
		FVoicePacket* VoicePacket = new FVoicePacket(1);
		Bunch << *VoicePacket;
		if (Bunch.IsError() == FALSE && VoicePacket->Length > 0)
		{
			// Now add the packet to the list to process
			GVoiceData.RemotePackets.AddItem(VoicePacket);
			// Send this packet to other clients as needed if we are a server
			if (Connection->Driver->ServerConnection == NULL && 
				// Client peers send voice data on peer connection directly
				!Connection->Driver->bIsPeer)
			{
				Connection->Driver->ReplicateVoicePacket(VoicePacket,Connection);
			}
#if STATS
			// Increment the number of voice packets we've received
			Connection->Driver->VoicePacketsRecv++;
			Connection->Driver->VoiceBytesRecv += VoicePacket->Length;
#endif
		}
		else
		{
			// Overflow reading the packet so delete it
			VoicePacket->DecRef();
		}
	}
}
#endif	//#if WITH_UE3_NETWORKING

#if !XBOX && !WITH_PANORAMA // Send voice not merged with game data

/**
 * Takes any pending packets and sends them via the channel
 */
void UVoiceChannel::Tick(void)
{
#if WITH_UE3_NETWORKING
	// If the handshaking hasn't completed throw away all voice data
	if (Connection->Actor &&
		Connection->Actor->bHasVoiceHandshakeCompleted)
	{
		// Try to append each packet in turn
		for (INT Index = 0; Index < VoicePackets.Num(); Index++)
		{
			FOutBunch Bunch(this,0);
			// Don't want reliable delivery. The bunch will be lost if the connection is saturated
			// First send needs to be reliable
			Bunch.bReliable = OpenAcked == FALSE;
			FVoicePacket* Packet = VoicePackets(Index);
			// Append the packet data (copies into the bunch)
			Bunch << *Packet;
#if STATS
			// Increment the number of voice packets we've sent
			Connection->Driver->VoicePacketsSent++;
			Connection->Driver->VoiceBytesSent += Packet->Length;
#endif
			// Let the packet free itself if no longer in use
			Packet->DecRef();
			// Don't submit the bunch if something went wrong
			if (Bunch.IsError() == FALSE)
			{
				// Submit the bunching with merging on
				SendBunch(&Bunch,1);
			}
			// If the network is saturated, throw away any remaining packets
			if (Connection->IsNetReady(0) == FALSE)
			{
				// Iterate from the current location to end discarding
				for (INT DiscardIndex = Index + 1; DiscardIndex < VoicePackets.Num(); DiscardIndex++)
				{
					VoicePackets(DiscardIndex)->DecRef();
				}
				VoicePackets.Empty();
			}
		}
	}
	VoicePackets.Empty();
#endif
}
#endif

/** Cleans up any voice data remaining in the queue */
void UVoiceChannel::CleanUp(void)
{
	// Clear out refs to any voice packets so we don't leak
	for (INT Index = 0; Index < VoicePackets.Num(); Index++)
	{
		VoicePackets(Index)->DecRef();
	}
	VoicePackets.Empty();
	// Route to the parent class for their cleanup
	Super::CleanUp();
}

/**
 * Adds the voice packet to the list to send for this channel
 *
 * @param VoicePacket the voice packet to send
 */
void UVoiceChannel::AddVoicePacket(FVoicePacket* VoicePacket)
{
#if WITH_UE3_NETWORKING
	if (VoicePacket != NULL)
	{
		VoicePackets.AddItem(VoicePacket);
		VoicePacket->AddRef();

#if 0
		debugf(NAME_DevNetTraffic,TEXT("AddVoicePacket: %s [%s] to=0x%016I64X from=0x%016I64X"),
			Connection->PlayerId.Uid,
			*Connection->Driver->GetDescription(),
			*Connection->LowLevelDescribe(),
			VoicePacket->Sender.Uid);
#endif
	}
#endif
}

/**
 * Tracks how much memory is being used by this object (no persistence)
 *
 * @param Ar the archive to serialize against
 */
void UVoiceChannel::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	// Don't persist the data since we are transient
	if (!Ar.IsLoading() && !Ar.IsSaving())
	{
		// Serialize any pending packets
		for (INT Index = 0; Index < VoicePackets.Num(); Index++)
		{
			Ar << *VoicePackets(Index);
		}
	}
}

/** 
 * Serialize FClientPeerInfo to archive to send in bunch packet 
 *
 * @param Ar the archive to serialize against
 * @param Info struct to read/write with the Archive
 */
FArchive& operator<<(FArchive& Ar, FClientPeerInfo& Info)
{
	Ar << Info.PlayerId << Info.PeerPort;
#if WITH_UE3_NETWORKING
	if (Ar.IsLoading())
	{
		// Platform ip data read in as byte array
		Ar << Info.PlatformConnectAddr;
		FPlatformIpAddr PlatformIpAddr;
		// Convert platform data to ip addr
		if (PlatformIpAddr.SerializeFromBuffer(Info.PlatformConnectAddr))
		{
			Info.PeerIpAddrAsInt = PlatformIpAddr.Addr;
		}
		else
		{
			Info.PeerIpAddrAsInt = 0;
		}
	}
	else
	{
		// Copy platform ip data to buffer from the ipaddr
		FPlatformIpAddr PlatformIpAddr(Info.PeerIpAddrAsInt,Info.PeerPort);
		PlatformIpAddr.SerializeToBuffer(Info.PlatformConnectAddr);
		Ar << Info.PlatformConnectAddr;
	}
#else
	Ar << Info.PlatformConnectAddr;
#endif
	return Ar;
}

/**
 * Convert the remote ip:port for the client to string
 *
 * @param bAppendPort TRUE to add ":port" to string
 * @return "ip:port" string for the peer connection 
 */
FString FClientPeerInfo::GetPeerConnectStr(UBOOL bAppendPort) const
{
#if WITH_UE3_NETWORKING
	FIpAddr IpAddr(PeerIpAddrAsInt,PeerPort);
	return IpAddr.ToString(bAppendPort);
#else
	return FString();
#endif 
}
