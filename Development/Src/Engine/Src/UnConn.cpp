/*=============================================================================
	UnConn.h: Unreal connection base class.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"

/*-----------------------------------------------------------------------------
	UNetConnection implementation.
-----------------------------------------------------------------------------*/

UNetConnection::UNetConnection()
:	Out					( 0 )
{}
UNetConnection::UNetConnection( UNetDriver* InDriver, const FURL& InURL )
:	Driver				( InDriver )
,	Out					( 0 )
,	State				( USOCK_Invalid )
,	ProtocolVersion		( MIN_PROTOCOL_VERSION )
,	MaxPacket			( 0 )
,	LastReceiveTime		( Driver->Time )
,	LastSendTime		( Driver->Time )
,	LastTickTime		( Driver->Time )
,	StatUpdateTime		( Driver->Time )
,	LastRepTime			( 0.f )
,	QueuedBytes			( 0 )
,	URL					( InURL )
,	StatPeriod          ( 1.f  )
,	OutAckPacketId		( -1 )
,	InPacketId			( -1 )
,	LagAcc				( 9999 )
,	BestLagAcc			( 9999 )
,	BestLag				( 9999 )
,	AvgLag				( 9999 )
,	NegotiatedVer		( GEngineNegotiationVersion )
{
	// Command-line parameters.
#if DO_ENABLE_NET_TEST
	Parse(appCmdLine(),TEXT("PktLoss="), PktLoss);
	Parse(appCmdLine(),TEXT("PktOrder="),PktOrder);
	Parse(appCmdLine(),TEXT("PktDup="),  PktDup);
	Parse(appCmdLine(),TEXT("PktLag="),  PktLag);
	Parse(appCmdLine(),TEXT("PktLagVariance="),  PktLagVariance);
#endif

	// Other parameters.
	CurrentNetSpeed = URL.HasOption(TEXT("LAN")) ? GetDefault<UPlayer>()->ConfiguredLanSpeed : GetDefault<UPlayer>()->ConfiguredInternetSpeed;

	if ( CurrentNetSpeed == 0 )
		CurrentNetSpeed = 2600;
	else
		CurrentNetSpeed = ::Max<INT>(CurrentNetSpeed, 1800);

	// Create package map.
	PackageMap = new(this)UPackageMapLevel(this);

}
void UNetConnection::Serialize( FArchive& Ar )
{
	UObject::Serialize( Ar );
	Ar << PackageMap;
	for( INT i=0; i<MAX_CHANNELS; i++ )
		Ar << Channels[i];
	Ar << Download;

}
void UNetConnection::Destroy()
{
	// Log.
	debugf( NAME_NetComeGo, TEXT("Close %s %s %s"), GetName(), *LowLevelGetRemoteAddress(), appTimestamp() );

	// Close the control channel.
	if( Channels[0] )
	{
		Channels[0]->Close();
	}
	FlushNet();

	// Remove from driver.
	if( Driver->ServerConnection )
	{
		check(Driver->ServerConnection==this);
		Driver->ServerConnection=NULL;
	}
	else
	{
		check(Driver->ServerConnection==NULL);
		verify(Driver->ClientConnections.RemoveItem( this )==1);
	}

	// Set to closed so the channels don't try to send data.
	State = USOCK_Closed;

	// Kill all channels.
	for( INT i=OpenChannels.Num()-1; i>=0; i-- )
		delete OpenChannels(i);

	// Kill package map.
	delete PackageMap;

	// Kill download object.
	if( Download )
		delete Download;

	// Destroy the connection's actor.
//FIXMESTEVE - is this dangerous to do here?
	if( GIsRunning && Actor )
	{
		ULevel* Level = Actor->GetLevel();
		Actor->Player = NULL;
		Level->DestroyActor( Actor, 1 );
		Actor = NULL;
	}

	Super::Destroy();
}
UBOOL UNetConnection::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( ParseCommand(&Cmd,TEXT("GETPING")) )
	{
		Ar.Logf( TEXT(" %i"), ::Max(5,(INT)(BestLag*1000.f)) );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("GETLOSS")) )
	{
		Ar.Logf( TEXT(" %i"), (INT) InLoss );
		return 1;
	}
	return 0;
}
void UNetConnection::AssertValid()
{
	// Make sure this connection is in a reasonable state.
	check(ProtocolVersion>=MIN_PROTOCOL_VERSION);
	check(ProtocolVersion<=MAX_PROTOCOL_VERSION);
	check(State==USOCK_Closed || State==USOCK_Pending || State==USOCK_Open);

}
void UNetConnection::SendPackageMap()
{
	// Send package map to the remote.
	for( TArray<FPackageInfo>::TIterator It(PackageMap->List); It; ++It )
	{
		FString Filename = *It->URL;
		INT i;
		while( (i = Filename.InStr( PATH_SEPARATOR )) != -1 )
			Filename = Filename.Mid( i+1 );
		while( (i = Filename.InStr( TEXT(":") )) != -1 )
			Filename = Filename.Mid( i+1 );
		while( (i = Filename.InStr( TEXT("/") )) != -1 )		//!! ini doesn't use same format as PATH_SEPARATOR!
			Filename = Filename.Mid( i+1 );
		while( (i = Filename.InStr( TEXT("\\") )) != -1 )
			Filename = Filename.Mid( i+1 );
		Logf
		(
			TEXT("USES GUID=%s PKG=%s FLAGS=%i SIZE=%i GEN=%i FNAME=%s"),
			*It->Guid.String(),
			It->Parent->GetName(),
			It->PackageFlags,
			It->FileSize,
			It->LocalGeneration,
			*Filename
		);
	}
	for( INT i=0; i<Driver->DownloadManagers.Num(); i++ )
	{
		UClass* DownloadClass = StaticLoadClass( UDownload::StaticClass(), NULL, *Driver->DownloadManagers(i), NULL, LOAD_NoWarn, NULL );
		if( DownloadClass )
		{
			FString Params = *CastChecked<UDownload>(DownloadClass->GetDefaultObject())->DownloadParams;
			UBOOL Compression = CastChecked<UDownload>(DownloadClass->GetDefaultObject())->UseCompression;
			if( *(*Params) )
			{
				Logf
				( 
					TEXT("DLMGR CLASS=%s PARAMS=%s COMPRESSION=%d"), 
					*DownloadClass->GetPathName(),
					*Params,
					Compression
				);
			}
		}
	}
}
void UNetConnection::InitOut()
{
	// Initialize the one outgoing buffer.
	Out = FBitWriter(MaxPacket*8);

}
void UNetConnection::ReceivedRawPacket( void* InData, INT Count )
{
	BYTE* Data = (BYTE*)InData;

	// Handle an incoming raw packet from the driver.
	debugfSlow( NAME_DevNetTraffic, TEXT("%03i: Received %i"), (INT)(appSeconds()*1000)%1000, Count );
	InByteAcc += Count + PacketOverhead;
	InPktAcc++;
	if( Count>0 )
	{
		BYTE LastByte = Data[Count-1];
		if( LastByte )
		{
			INT BitSize = Count*8-1;
			while( !(LastByte & 0x80) )
			{
				LastByte *= 2;
				BitSize--;
			}
			FBitReader Reader( Data, BitSize );
			ReceivedPacket( Reader );
		}
		else appErrorfSlow( TEXT("Packet missing trailing 1") );
	}
	else appErrorfSlow( TEXT("Received zero-size packet") );

}
void UNetConnection::FlushNet()
{
	// Update info.
	check(!Out.IsError());
	LastEnd = FBitWriterMark();
	TimeSensitive = 0;

	// If there is any pending data to send, send it.
	if( Out.GetNumBits() || Driver->Time-LastSendTime>Driver->KeepAliveTime )
	{
		// If sending keepalive packet, still generate header.
		if( Out.GetNumBits()==0 )
			PreSend( 0 );

		// Make sure packet size is byte-aligned.
		Out.WriteBit( 1 );
		check(!Out.IsError());
		while( Out.GetNumBits() & 7 )
			Out.WriteBit( 0 );
		check(!Out.IsError());

		// Send now.
#if DO_ENABLE_NET_TEST
		if( PktOrder )
		{
			DelayedPacket& B = *(new(Delayed)DelayedPacket);
			B.Data.Add( Out.GetNumBytes() );
			appMemcpy( &B.Data(0), Out.GetData(), Out.GetNumBytes() );

			for( INT i=Delayed.Num()-1; i>=0; i-- )
			{
				if( appFrand()>0.50 )
				{
					if( !PktLoss || appFrand()*100.f>PktLoss )
						LowLevelSend( (char*)&Delayed(i).Data(0), Delayed(i).Data.Num() );
					Delayed.Remove( i );
				}
			}
		}
		else if( PktLag )
		{
			if( !PktLoss || appFrand()*100.f>PktLoss )
			{
				DelayedPacket& B = *(new(Delayed)DelayedPacket);
				B.Data.Add( Out.GetNumBytes() );
				appMemcpy( &B.Data(0), Out.GetData(), Out.GetNumBytes() );
				B.SendTime = appSeconds() + (DOUBLE(PktLag)  + 2.0f * (appFrand() - 0.5f) * DOUBLE(PktLagVariance))/ 1000.f;
			}
		}
		else if( !PktLoss || appFrand()*100.f>=PktLoss )
		{
#endif
			LowLevelSend( Out.GetData(), Out.GetNumBytes() );
#if DO_ENABLE_NET_TEST
			if( PktDup && appFrand()*100.f<PktDup )
				LowLevelSend( (char*)Out.GetData(), Out.GetNumBytes() );
		}
#endif

		// Update stuff.
		INT Index = OutPacketId & (ARRAY_COUNT(OutLagPacketId)-1);
		OutLagPacketId [Index] = OutPacketId;
		OutLagTime     [Index] = Driver->Time;
		OutPacketId++;
		OutPktAcc++;
		LastSendTime = Driver->Time;
		QueuedBytes += Out.GetNumBytes() + PacketOverhead;
		OutByteAcc  += Out.GetNumBytes() + PacketOverhead;
		InitOut();
	}

	// Move acks around.
	for( INT i=0; i<QueuedAcks.Num(); i++ )
		ResendAcks.AddItem(QueuedAcks(i));
	QueuedAcks.Empty(32);

}
void UNetConnection::Serialize( const TCHAR* Data, EName MsgType )
{
	// Send data to the control channel.
	if( Channels[0] && !Channels[0]->Closing )
		((UControlChannel*)Channels[0])->Serialize( Data, MsgType );

}
INT UNetConnection::IsNetReady( UBOOL Saturate )
{
	// Return whether we can send more data without saturation the connection.
	if( Saturate )
		QueuedBytes = -Out.GetNumBytes();
	return QueuedBytes+Out.GetNumBytes() <= 0;

}
void UNetConnection::ReadInput( FLOAT DeltaSeconds )
{}
IMPLEMENT_CLASS(UNetConnection);

/*-----------------------------------------------------------------------------
	Packet reception.
-----------------------------------------------------------------------------*/

//
// Packet was negatively acknowledged.
//
void UNetConnection::ReceivedNak( INT NakPacketId )
{
	// Make note of the nak.
	for( INT i=OpenChannels.Num()-1; i>=0; i-- )
	{
		UChannel* Channel = OpenChannels(i);
		Channel->ReceivedNak( NakPacketId );
		if( Channel->OpenPacketId==NakPacketId )
			Channel->ReceivedAcks(); //warning: May destroy Channel.
	}
}

//
// Handle a packet we just received.
//
void UNetConnection::ReceivedPacket( FBitReader& Reader )
{
	AssertValid();

	// Handle PacketId.
	if( Reader.IsError() )
	{
		appErrorfSlow( TEXT("Packet too small") );
		return;
	}

	// Update receive time to avoid timeout.
	LastReceiveTime = Driver->Time;

	// Check packet ordering.
	INT PacketId = MakeRelative(Reader.ReadInt(MAX_PACKETID),InPacketId,MAX_PACKETID);
	if( PacketId > InPacketId )
	{
		InLossAcc += PacketId - InPacketId - 1;
		InPacketId = PacketId;
	}
	else InOrdAcc++;
	//debugf(TEXT("RcvdPacket: %i %i"),(INT)(appSeconds()*1000)%1000,PacketId);


	// Disassemble and dispatch all bunches in the packet.
	while( !Reader.AtEnd() && State!=USOCK_Closed )
	{
		// Parse the bunch.
		INT StartPos = Reader.GetPosBits();
		UBOOL IsAck = Reader.ReadBit();
		if( Reader.IsError() )
		{
			appErrorfSlow( TEXT("Bunch missing ack flag") );
			// Acknowledge the packet.
			SendAck( PacketId );
			return;
		}

		// Process the bunch.
		if( IsAck )
		{
			// This is an acknowledgement.
			INT AckPacketId = MakeRelative(Reader.ReadInt(MAX_PACKETID),OutAckPacketId,MAX_PACKETID);
			if( Reader.IsError() )
			{
				appErrorfSlow( TEXT("Bunch missing ack") );
				// Acknowledge the packet.
				SendAck( PacketId );
				return;
			}

			// Resend any old reliable packets that the receiver hasn't acknowledged.
			if( AckPacketId>OutAckPacketId )
			{
				for( INT NakPacketId=OutAckPacketId+1; NakPacketId<AckPacketId; NakPacketId++,OutLossAcc++ )
				{
					debugfSlow( NAME_DevNetTraffic, TEXT("   Received virtual nak %i (%.1f)"), NakPacketId, (Reader.GetPosBits()-StartPos)/8.f );
					ReceivedNak( NakPacketId );
				}
				OutAckPacketId = AckPacketId;
			}
			else if( AckPacketId<OutAckPacketId )
			{
				//warning: Double-ack logic makes this unmeasurable.
				//OutOrdAcc++;
			}
			// Update lag.
			INT Index = AckPacketId & (ARRAY_COUNT(OutLagPacketId)-1);
			if( OutLagPacketId[Index]==AckPacketId )
			{
				FLOAT NewLag = Driver->Time - OutLagTime[Index] - (FrameTime/2.f);
				LagAcc += NewLag;
					LagCount++;
			}

			// Forward the ack to the channel.
			debugfSlow( NAME_DevNetTraffic, TEXT("   Received ack %i (%.1f)"), AckPacketId, (Reader.GetPosBits()-StartPos)/8.f );
			for( INT i=OpenChannels.Num()-1; i>=0; i-- )
			{
				UChannel* Channel = OpenChannels(i);
				for( FOutBunch* Out=Channel->OutRec; Out; Out=Out->Next )
				{
					if( Out->PacketId==AckPacketId )
					{
						Out->ReceivedAck = 1;
						if( Out->bOpen )
							Channel->OpenAcked = 1;
					}
				}
				if( Channel->OpenPacketId==AckPacketId ) // Necessary for unreliable "bNetTemporary" channels.
					Channel->OpenAcked = 1;
				Channel->ReceivedAcks(); //warning: May destroy Channel.
			}
		}
		else
		{
			// Parse the incoming data.
			FInBunch Bunch( this );
			INT StartPos       = Reader.GetPosBits();
			BYTE bControl      = Reader.ReadBit();
			Bunch.PacketId     = PacketId;
			Bunch.bOpen        = bControl ? Reader.ReadBit() : 0;
			Bunch.bClose       = bControl ? Reader.ReadBit() : 0;
			Bunch.bReliable    = Reader.ReadBit();
			Bunch.ChIndex      = Reader.ReadInt( MAX_CHANNELS );
			Bunch.ChSequence   = Bunch.bReliable ? MakeRelative(Reader.ReadInt(MAX_CHSEQUENCE),InReliable[Bunch.ChIndex],MAX_CHSEQUENCE) : 0;
			Bunch.ChType       = (Bunch.bReliable||Bunch.bOpen) ? Reader.ReadInt(CHTYPE_MAX) : CHTYPE_None;
			INT BunchDataBits  = Reader.ReadInt( UNetConnection::MaxPacket*8 );
			INT HeaderPos      = Reader.GetPosBits();
			if( Reader.IsError() )
			{
				appErrorfSlow( TEXT("Bunch header overflowed") );
				// Acknowledge the packet.
				SendAck( PacketId );
				return;
			}
			Bunch.SetData( Reader, BunchDataBits );
			if( Reader.IsError() )
			{
				// Bunch claims it's larger than the enclosing packet.
				appErrorfSlow( TEXT("Bunch data overflowed (%i %i+%i/%i)"), StartPos, HeaderPos, BunchDataBits, Reader.GetNumBits() );
				// Acknowledge the packet.
				SendAck( PacketId );
				return;
			}
			if( Bunch.bReliable )
				debugfSlow( NAME_DevNetTraffic, TEXT("   Reliable Bunch, Channel %i Sequence %i: Size %.1f+%.1f"), Bunch.ChIndex, Bunch.ChSequence, (HeaderPos-StartPos)/8.f, (Reader.GetPosBits()-HeaderPos)/8.f );
			else
				debugfSlow( NAME_DevNetTraffic, TEXT("   Unreliable Bunch, Channel %i: Size %.1f+%.1f"), Bunch.ChIndex, (HeaderPos-StartPos)/8.f, (Reader.GetPosBits()-HeaderPos)/8.f );
			// Can't handle other channels until control channel exists.
			if( !Channels[Bunch.ChIndex] && !Channels[0] && (Bunch.ChIndex!=0 || Bunch.ChType!=CHTYPE_Control) )
			{
				appErrorfSlow( NAME_DevNetTraffic, TEXT("Received bunch before connected") );
				// Acknowledge the packet.
				SendAck( PacketId );
				return;
			}

			// Receiving data.
			UChannel* Channel = Channels[Bunch.ChIndex];

			// Ignore if reliable packet has already been processed.
			if( Bunch.bReliable && Bunch.ChSequence<=InReliable[Bunch.ChIndex] )
			{
				debugfSlow( NAME_DevNetTraffic, TEXT("      Received outdated bunch (Current Sequence %i)"), InReliable[Bunch.ChIndex] );
				continue;
			}

			// If unreliable but not one-shot open+close "bNetTemporary" packet, discard it.
			if( !Bunch.bReliable && (!Bunch.bOpen || !Bunch.bClose) && (!Channel || Channel->OpenPacketId==INDEX_NONE) )
			{
				debugfSlow( NAME_DevNetTraffic, TEXT("      Received unreliable bunch before open (Current Sequence %i)"), InReliable[Bunch.ChIndex] );
				continue;
			}

			// Create channel if necessary.
			if( !Channel )
			{
				// Validate channel type.
				if( !UChannel::IsKnownChannelType(Bunch.ChType) )
				{
					// Unknown type.
					appErrorfSlow( TEXT("Connection unknown channel type (%i)"), Bunch.ChType );
					// Acknowledge the packet.
					SendAck( PacketId );
					return;
				}

				// Reliable (either open or later), so create new channel.
				debugfSlow( NAME_DevNetTraffic, TEXT("      Bunch Create %i: ChType %i"), Bunch.ChIndex, Bunch.ChType );
				Channel = CreateChannel( (EChannelType)Bunch.ChType, 0, Bunch.ChIndex );

				// Notify the server of the new channel.
				if( !Driver->Notify->NotifyAcceptingChannel( Channel ) )
				{
					// Channel refused, so close it, flush it, and delete it.
					FOutBunch CloseBunch( Channel, 1 );
					check(!CloseBunch.IsError());
					check(CloseBunch.bClose);
					CloseBunch.bReliable = 1;
					Channel->SendBunch( &CloseBunch, 0 );
					FlushNet();
					delete Channel;
					if( Bunch.ChIndex==0 )
					{
						debugfSlow( NAME_DevNetTraffic, TEXT("Channel 0 create failed") );
						State = USOCK_Closed;
					}
					continue;
				}
			}
			if( Bunch.bOpen )
			{
				Channel->OpenAcked = 1;
				Channel->OpenPacketId = PacketId;
			}

			// Dispatch the raw, unsequenced bunch to the channel.
			Channel->ReceivedRawBunch( Bunch ); //warning: May destroy channel.
			InBunAcc++;

			// Disconnect if we received a corrupted packet from the client (eg server crash attempt).
			if( !Driver->ServerConnection && Bunch.IsCriticalError() )
			{
				debugf( NAME_DevNetTraffic, TEXT("Received corrupted packet data from client %s.  Disconnecting."), *LowLevelGetRemoteAddress() );
				State = USOCK_Closed;
			}
		}
	}

	// Acknowledge the packet.
	SendAck( PacketId );
}

/*-----------------------------------------------------------------------------
	All raw sending functions.
-----------------------------------------------------------------------------*/

//
// Called before sending anything.
//
void UNetConnection::PreSend( INT SizeBits )
{
	// Flush if not enough space.
	if( Out.GetNumBits() + SizeBits + MAX_PACKET_TRAILER_BITS > MaxPacket*8 )
		FlushNet();

	// If start of packet, send packet id.
	if( Out.GetNumBits()==0 )
	{
		Out.WriteInt( OutPacketId, MAX_PACKETID );
		check(Out.GetNumBits()<=MAX_PACKET_HEADER_BITS);
	}

	// Make sure there's enough space now.
	if( Out.GetNumBits() + SizeBits + MAX_PACKET_TRAILER_BITS > MaxPacket*8 )
		appErrorf( TEXT("PreSend overflowed: %i+%i>%i"), Out.GetNumBits(), SizeBits, MaxPacket*8 );

}

//
// Called after sending anything.
//
void UNetConnection::PostSend()
{
	// If absolutely filled now, flush so that MaxSend() doesn't return zero unnecessarily.
	check(Out.GetNumBits()<=MaxPacket*8);
	if( Out.GetNumBits()==MaxPacket*8 )
		FlushNet();

}

//
// Resend any pending acks.
//
void UNetConnection::PurgeAcks()
{
	for( INT i=0; i<ResendAcks.Num(); i++ )
		SendAck( ResendAcks(i), 0 );
	ResendAcks.Empty(32);
}

//
// Send an acknowledgement.
//
void UNetConnection::SendAck( INT AckPacketId, UBOOL FirstTime )
{
	if( !InternalAck )
	{
		if( FirstTime )
		{
			PurgeAcks();
			QueuedAcks.AddItem(AckPacketId);
		}
		PreSend( appCeilLogTwo(MAX_PACKETID)+1 );
		Out.WriteBit( 1 );
		Out.WriteInt( AckPacketId, MAX_PACKETID );
		AllowMerge = 0;
		PostSend();
	}
}

//
// Send a raw bunch.
//
INT UNetConnection::SendRawBunch( FOutBunch& Bunch, UBOOL InAllowMerge )
{
	check(!Bunch.ReceivedAck);
	check(!Bunch.IsError());
	OutBunAcc++;
	TimeSensitive = 1;

	// Build header.
	FBitWriter Header( MAX_BUNCH_HEADER_BITS );
	Header.WriteBit( 0 );
	Header.WriteBit( Bunch.bOpen || Bunch.bClose );
	if( Bunch.bOpen || Bunch.bClose )
	{
		Header.WriteBit( Bunch.bOpen );
		Header.WriteBit( Bunch.bClose );
	}
	Header.WriteBit( Bunch.bReliable );
	Header.WriteInt( Bunch.ChIndex, MAX_CHANNELS );
	if( Bunch.bReliable )
		Header.WriteInt( Bunch.ChSequence, MAX_CHSEQUENCE );
	if( Bunch.bReliable || Bunch.bOpen )
		Header.WriteInt( Bunch.ChType, CHTYPE_MAX );
	Header.WriteInt( Bunch.GetNumBits(), UNetConnection::MaxPacket*8 );
	check(!Header.IsError());

	// If this data doesn't fit in the current packet, flush it.
	PreSend( Header.GetNumBits() + Bunch.GetNumBits() );

	// Remember start position.
	AllowMerge      = InAllowMerge;
	Bunch.PacketId  = OutPacketId;
	Bunch.Time      = Driver->Time;

	// Remember start position, and write data.
	LastStart = FBitWriterMark( Out );
	Out.SerializeBits( Header.GetData(), Header.GetNumBits() );
	Out.SerializeBits( Bunch .GetData(), Bunch .GetNumBits() );

	// Finished.
	PostSend();

	return Bunch.PacketId;
}

/*-----------------------------------------------------------------------------
	Channel creation.
-----------------------------------------------------------------------------*/

//
// Create a channel.
//
UChannel* UNetConnection::CreateChannel( EChannelType ChType, UBOOL bOpenedLocally, INT ChIndex )
{
	check(UChannel::IsKnownChannelType(ChType));
	AssertValid();

	// If no channel index was specified, find the first available.
	if( ChIndex==INDEX_NONE )
	{
		INT FirstChannel = 1;
		if ( ChType == CHTYPE_Control )
			FirstChannel = 0;
		for( ChIndex=FirstChannel; ChIndex<MAX_CHANNELS; ChIndex++ )
			if( !Channels[ChIndex] )
				break;
		if( ChIndex==MAX_CHANNELS )
			return NULL;
	}

	// Make sure channel is valid.
	check(ChIndex<MAX_CHANNELS);
	check(Channels[ChIndex]==NULL);

	// Create channel.
	UChannel* Channel = ConstructObject<UChannel>( UChannel::ChannelClasses[ChType] );
	Channel->Init( this, ChIndex, bOpenedLocally );
	Channels[ChIndex] = Channel;
	OpenChannels.AddItem(Channel);
	//debugf( "Created channel %i of type %i", ChIndex, ChType);

	return Channel;
}

/*-----------------------------------------------------------------------------
	Connection polling.
-----------------------------------------------------------------------------*/

//
// Poll the connection.
// If it is timed out, close it.
//
void UNetConnection::Tick()
{
	AssertValid();

	// Lag simulation.
#if DO_ENABLE_NET_TEST
	if( PktLag )
	{
		for( INT i=0; i < Delayed.Num(); i++ )
		{
			if( appSeconds() > Delayed(i).SendTime )
			{
				LowLevelSend( (char*)&Delayed(i).Data(0), Delayed(i).Data.Num() );
				Delayed.Remove( i );
				i--;
			}
		}
	}
#endif

	// Get frame time.
	DOUBLE CurrentTime = appSeconds();
	FrameTime = CurrentTime - LastTime;
	LastTime = CurrentTime;
	CumulativeTime += FrameTime;
	CountedFrames++;
	if(CumulativeTime > 1.f)
	{
		AverageFrameTime = CumulativeTime / CountedFrames;
		CumulativeTime = 0;
		CountedFrames = 0;
	}

	// Pretend everything was acked, for 100% reliable connections or demo recording.
	if( InternalAck )
	{
		LastReceiveTime = Driver->Time;
		for( INT i=OpenChannels.Num()-1; i>=0; i-- )
		{
			UChannel* It = OpenChannels(i);
			for( FOutBunch* Out=It->OutRec; Out; Out=Out->Next )
				Out->ReceivedAck = 1;
			It->OpenAcked = 1;
			It->ReceivedAcks();
		}
	}

	// Update stats.
	if( Driver->Time-StatUpdateTime>StatPeriod )
	{
		// Update stats.
		FLOAT RealTime	= Driver->Time - StatUpdateTime;
		InRate			= InByteAcc  / RealTime;
		OutRate			= OutByteAcc / RealTime;
		InPackets		= InPktAcc   / RealTime;
		OutPackets		= OutPktAcc  / RealTime;
		InBunches		= InBunAcc   / RealTime;
		OutBunches		= OutBunAcc  / RealTime;
		InOrder         = InOrdAcc   / RealTime;
		OutOrder        = OutOrdAcc  / RealTime;
		OutLoss         = 100.f * OutLossAcc / Max(OutPackets,1.f);
		InLoss          = 100.f * InLossAcc  / Max(InPackets+InLossAcc,1.f);
		if( LagCount )
			AvgLag = LagAcc/LagCount;
		BestLag = AvgLag;

		// See if we're experiencing high packet loss.
		if( OutLoss>20 || InLoss>20 )
			HighLossCount++;
		else
			HighLossCount=0;

		if( Actor )
		{
			FLOAT PktLoss = ::Max(InLoss, OutLoss) * 0.01f;
			FLOAT ModifiedLag = BestLag + 1.2 * PktLoss;
			if ( Actor->myHUD )
				Actor->myHUD->bShowBadConnectionAlert = !InternalAck && ((ModifiedLag>0.8 || CurrentNetSpeed * (1 - PktLoss)<2000) && ActorChannels.FindRef(Actor)) || InPackets < 2; 
			if ( Actor->PlayerReplicationInfo )
				Actor->PlayerReplicationInfo->PacketLoss = ::Max(InLoss, OutLoss);
		}

		// Init counters.
		LagAcc			= 0;
		BestLagAcc		= 9999;
		InByteAcc		= 0;
		OutByteAcc		= 0;
		InPktAcc		= 0;
		OutPktAcc		= 0;
		InBunAcc		= 0;
		OutBunAcc		= 0;
		InLossAcc       = 0;
		OutLossAcc      = 0;
		InOrdAcc        = 0;
		OutOrdAcc       = 0;
		LagCount        = 0;
		StatUpdateTime	= Driver->Time;
	}

	// Compute time passed since last update.
	FLOAT DeltaTime     = Driver->Time - LastTickTime;
	LastTickTime        = Driver->Time;

	// Update queued byte count.
	FLOAT DeltaBytes = CurrentNetSpeed * DeltaTime;
	QueuedBytes     -= (INT) DeltaBytes;
	FLOAT AllowedLag = 2.f * DeltaBytes;
	if( QueuedBytes < -AllowedLag )
		QueuedBytes = (INT) -AllowedLag;

	// Handle timeouts.
	FLOAT Timeout = Driver->InitialConnectTimeout;
	if ( (State!=USOCK_Pending) && Actor && (Actor->bPendingDestroy || Actor->bShortConnectTimeOut) )
		Timeout = Actor->bPendingDestroy ? 2.f : Driver->ConnectionTimeout;
	if( Driver->Time - LastReceiveTime > Timeout )
	{
		// Timeout.
		if( State != USOCK_Closed )
			debugf( NAME_DevNet, TEXT("Connection timed out after %f seconds (%f)"), Timeout, Driver->Time - LastReceiveTime );
		State = USOCK_Closed;
	}
	else
	{
		// Tick the channels.
		for( INT i=OpenChannels.Num()-1; i>=0; i-- )
			OpenChannels(i)->Tick();

		// If channel 0 has closed, mark the conection as closed.
		if( Channels[0]==NULL && (OutReliable[0]!=0 || InReliable[0]!=0) )
			State = USOCK_Closed;
	}

	// Flush.
	PurgeAcks();
	if( TimeSensitive || Driver->Time-LastSendTime>Driver->KeepAliveTime )
		FlushNet();

	if( Download )
		Download->Tick();
}

/*---------------------------------------------------------------------------------------
	Client Player Connection.
---------------------------------------------------------------------------------------*/

void UNetConnection::HandleClientPlayer( APlayerController *PC )
{
	// Hook up the Viewport to the new player actor.
	ULocalPlayer*	LocalPlayer = NULL;
	for(FPlayerIterator It(Cast<UGameEngine>(GEngine));It;++It)
	{
		LocalPlayer = *It;
		break;
	}
	LocalPlayer->CurrentNetSpeed = CurrentNetSpeed;

	// Init the new playerpawn.
	PC->Role = ROLE_AutonomousProxy;
	PC->SetPlayer(LocalPlayer);
	debugf(TEXT("%s setplayer %s"),PC->GetName(),LocalPlayer->GetName());
	PC->Level->LevelAction = LEVACT_None;
	// Mark this connection as open.
	check(State==USOCK_Pending);
	State = USOCK_Open;
	Actor = PC;
}

/*---------------------------------------------------------------------------------------
	File transfer.
---------------------------------------------------------------------------------------*/
//
// Initiate downloading a file to the cache directory.
// The transfer will eventually succeed or fail, and the
// NotifyReceivedFile will be called with the results.
//
void UNetConnection::ReceiveFile( INT PackageIndex )
{
	check(PackageMap->List.IsValidIndex(PackageIndex));
	if( DownloadInfo.Num() == 0 )
	{
		DownloadInfo.AddZeroed();
		DownloadInfo(0).Class = UChannelDownload::StaticClass();
		DownloadInfo(0).ClassName = TEXT("Engine.UChannelDownload");
		DownloadInfo(0).Params = TEXT("");
		DownloadInfo(0).Compression = 0;
	}
	if( Download )
		delete Download;
	Download = ConstructObject<UDownload>( DownloadInfo(0).Class );	
	Download->ReceiveFile( this, PackageIndex, *DownloadInfo(0).Params, DownloadInfo(0).Compression );
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

