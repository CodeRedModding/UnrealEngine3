/*=============================================================================
	UnChan.cpp: Unreal datachannel implementation.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"

static inline void SerializeCompRotator( FArchive& Ar, FRotator& R )
{
	BYTE Pitch(R.Pitch>>8), Yaw(R.Yaw>>8), Roll(R.Roll>>8), B;
	B = (Pitch!=0);
	Ar.SerializeBits( &B, 1 );
	if( B )
		Ar << Pitch;
	else
		Pitch = 0;
	B = (Yaw!=0);
	Ar.SerializeBits( &B, 1 );
	if( B )
		Ar << Yaw;
	else
		Yaw = 0;
	B = (Roll!=0);
	Ar.SerializeBits( &B, 1 );
	if( B )
		Ar << Roll;
	else
		Roll = 0;
	if( Ar.IsLoading() )
		R = FRotator(Pitch<<8,Yaw<<8,Roll<<8);
}

static inline void SerializeCompVector( FArchive& Ar, FVector& V )
{
	INT X(appRound(V.X)), Y(appRound(V.Y)), Z(appRound(V.Z));
	DWORD Bits = Clamp<DWORD>( appCeilLogTwo(1+Max(Max(Abs(X),Abs(Y)),Abs(Z))), 1, 20 )-1;
	Ar.SerializeInt( Bits, 20 );
	INT   Bias = 1<<(Bits+1);
	DWORD Max  = 1<<(Bits+2);
	DWORD DX(X+Bias), DY(Y+Bias), DZ(Z+Bias);
	Ar.SerializeInt( DX, Max );
	Ar.SerializeInt( DY, Max );
	Ar.SerializeInt( DZ, Max );
	if( Ar.IsLoading() )
		V = FVector((INT)DX-Bias,(INT)DY-Bias,(INT)DZ-Bias);
}

// passing NULL for UActorChannel will skip recent property update
static inline void SerializeCompressedInitial( FArchive& Bunch, FVector& Location, FRotator& Rotation, UBOOL bSerializeRotation, UActorChannel* Ch )
{
    // read/write compressed location
    SerializeCompVector( Bunch, Location );
    if( Ch && Ch->Recent.Num() )
        ((AActor*)&Ch->Recent(0))->Location = Location;

    // optionally read/write compressed rotation
    if( bSerializeRotation )
    {
        SerializeCompRotator( Bunch, Rotation );
	    if( Ch && Ch->Recent.Num() )
            ((AActor*)&Ch->Recent(0))->Rotation = Rotation;
    }
}

/*-----------------------------------------------------------------------------
	UChannel implementation.
-----------------------------------------------------------------------------*/

//
// Initialize the base channel.
//
UChannel::UChannel()
{}
void UChannel::Init( UNetConnection* InConnection, INT InChIndex, INT InOpenedLocally )
{
	Connection		= InConnection;
	ChIndex			= InChIndex;
	OpenedLocally	= InOpenedLocally;
	OpenPacketId	= INDEX_NONE;
	NegotiatedVer	= InConnection->NegotiatedVer;
}

//
// Route the UObject::Destroy.
//
INT UChannel::RouteDestroy()
{
	if( Connection && (Connection->GetFlags() & RF_Unreachable) )
	{
		ClearFlags( RF_Destroyed );
		if( Connection->ConditionalDestroy() )
			return 1;
		SetFlags( RF_Destroyed );
	}
	return 0;
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
}

//
// Base channel destructor.
//
void UChannel::Destroy()
{
	check(Connection);
	check(Connection->Channels[ChIndex]==this);

	// remember sequence number of first non-acked outgoing reliable bunch for this slot
	if ( OutRec )
	{
		Connection->PendingOutRec[ChIndex] = OutRec->ChSequence;
		//debugf(TEXT("%i save pending out bunch %i"),ChIndex,Connection->PendingOutRec[ChIndex]);
	}

	// Free any pending incoming and outgoing bunches.
	for( FOutBunch* Out=OutRec, *NextOut; Out; Out=NextOut )
		{NextOut = Out->Next; delete Out;}
	for( FInBunch* In=InRec, *NextIn; In; In=NextIn )
		{NextIn = In->Next; delete In;}

	// Remove from connection's channel table.
	verify(Connection->OpenChannels.RemoveItem(this)==1);
	Connection->Channels[ChIndex] = NULL;
	Connection                    = NULL;

	Super::Destroy();
}

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
		delete this;
	}

}

//
// Return the maximum amount of data that can be sent in this bunch without overflow.
//
INT UChannel::MaxSendBytes()
{
	INT ResultBits
	=	Connection->MaxPacket*8
	-	(Connection->Out.GetNumBits() ? 0 : MAX_PACKET_HEADER_BITS)
	-	Connection->Out.GetNumBits()
	-	MAX_PACKET_TRAILER_BITS
	-	MAX_BUNCH_HEADER_BITS;
	return Max( 0, ResultBits/8 );
}

//
// Handle time passing on this channel.
//
void UChannel::Tick()
{
	checkSlow(Connection->Channels[ChIndex]==this);
	if( ChIndex==0 && !OpenAcked )
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
					debugfSlow( NAME_DevNetTraffic, TEXT("Channel %i ack timeout; resending %i..."), ChIndex, Out->ChSequence );
					check(Out->bReliable);
					Connection->SendRawBunch( *Out, 0 );
				}
			}
		}
	}
}

//
// Make sure the incoming buffer is in sequence and there are no duplicates.
//
void UChannel::AssertInSequenced()
{
	// Verify that buffer is in order with no duplicates.
	for( FInBunch* In=InRec; In && In->Next; In=In->Next )
		check(In->Next->ChSequence>In->ChSequence);

}

//
// Process a properly-sequenced bunch.
//
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
			appErrorfSlow( TEXT("Close Anomaly %i / %i"), Bunch.ChSequence, InRec->ChSequence );
		debugf( NAME_DevNetTraffic, TEXT("      Channel %i got close-notify"), ChIndex );
		delete this;
		return 1;
	}
	return 0;
}

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
		debugfSlow( NAME_DevNetTraffic, TEXT("      Queuing bunch with unreceived dependency") );
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
	}
	else
	{
		// Receive it in sequence.
		UBOOL Deleted = ReceivedSequencedBunch( Bunch );
		if( Deleted )
			return;
		// Dispatch any waiting bunches.
		while( InRec )
		{
			if( InRec->ChSequence!=Connection->InReliable[ChIndex]+1 )
				break;
			debugfSlow( NAME_DevNetTraffic, TEXT("      Unleashing queued bunch") );
			FInBunch* Release = InRec;
			InRec = InRec->Next;
			NumInRec--;
			UBOOL Deleted = ReceivedSequencedBunch( *Release );
			delete Release;
			if( Deleted )
				return;
			//AssertInSequenced();
		}
	}
}

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
		Connection->LastOut.SerializeBits( Bunch->GetData(), Bunch->GetNumBits() );
		Connection->LastOut.bReliable |= Bunch->bReliable;
		Connection->LastOut.bOpen     |= Bunch->bOpen;
		Connection->LastOut.bClose    |= Bunch->bClose;
		OutBunch                       = Connection->LastOutBunch;
		Bunch                          = &Connection->LastOut;
		check(!Bunch->IsError());
		Connection->LastStart.Pop( Connection->Out );
		Connection->OutBunAcc--;
	}
	else Merge=0;

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

	// Send the raw bunch.
	OutBunch->ReceivedAck = 0;
	INT PacketId = Connection->SendRawBunch( *OutBunch, 1 );
	if( OpenPacketId==INDEX_NONE && OpenedLocally )
		OpenPacketId = PacketId;
	if( OutBunch->bClose )
		SetClosingFlag();

	// Update channel sequence count.
	Connection->LastOut = *OutBunch;
	Connection->LastEnd	= FBitWriterMark(Connection->Out);

	return PacketId;
}

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
	// If saturation allowed, ignore queued byte count.
	if( NumOutRec>=RELIABLE_BUFFER-1 )
		return 0;
	return Connection->IsNetReady( Saturate );

}

//
// Returns whether the specified channel type exists.
//
UBOOL UChannel::IsKnownChannelType( INT Type )
{
	return Type>=0 && Type<CHTYPE_MAX && ChannelClasses[Type];
}

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
			debugfSlow( NAME_DevNetTraffic, TEXT("      Channel %i nak; resending %i..."), Out->ChIndex, Out->ChSequence );
			Connection->SendRawBunch( *Out, 0 );
		}
	}
}

// UChannel statics.
UClass* UChannel::ChannelClasses[CHTYPE_MAX]={0,0,0,0,0,0,0,0};
IMPLEMENT_CLASS(UChannel)

/*-----------------------------------------------------------------------------
	UControlChannel implementation.
-----------------------------------------------------------------------------*/

//
// Initialize the text channel.
//
UControlChannel::UControlChannel()
{}
void UControlChannel::Init( UNetConnection* InConnection, INT InChannelIndex, INT InOpenedLocally )
{
	Super::Init( InConnection, InChannelIndex, InOpenedLocally );
}

//
// UControlChannel destructor. 
//
void UControlChannel::Destroy()
{
	check(Connection);
	if( RouteDestroy() )
		return;
	check(Connection->Channels[ChIndex]==this);

	Super::Destroy();
}

//
// Handle an incoming bunch.
//
void UControlChannel::ReceivedBunch( FInBunch& Bunch )
{
	check(!Closing);
	for( ; ; )
	{
		FString Text;
		Bunch << Text;
		if( Bunch.IsError() )
			break;
		Connection->Driver->Notify->NotifyReceivedText( Connection, *Text );
	}
}

//
// Text channel FArchive interface.
//
void UControlChannel::Serialize( const TCHAR* Data, EName MsgType )
{
	// Delivery is not guaranteed because NewBunch may fail.
	FOutBunch Bunch( this, 0 );
	Bunch.bReliable = 1;
	FString Text=Data;
	Bunch << Text;
	if( !Bunch.IsError() )
	{
		SendBunch( &Bunch, 1 );
	}
	else
	{
		debugf( NAME_DevNetTraffic, TEXT("Control channel bunch overflowed") );
		//!!should signal error somehow
	}
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
void UActorChannel::Init( UNetConnection* InConnection, INT InChannelIndex, INT InOpenedLocally )
{
	Super::Init( InConnection, InChannelIndex, InOpenedLocally );
	Level			= Connection->Driver->Notify->NotifyGetLevel();
	RelevantTime	= Connection->Driver->Time;
	LastUpdateTime	= Connection->Driver->Time - Connection->Driver->SpawnPrioritySeconds;
	LastFullUpdateTime = -1.f;
	ActorDirty = false;
	bActorMustStayDirty = false;
	bActorStillInitial = false;
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
	Actor = NULL;
}

//
// Time passes...
//
void UActorChannel::Tick()
{
	UChannel::Tick();
}

//
// Actor channel destructor.
//
void UActorChannel::Destroy()
{
	check(Connection);
	if( RouteDestroy() )
		return;
	check(Connection->Channels[ChIndex]==this);

	// Remove from hash and stuff.
	SetClosingFlag();

	// Destroy Recent properties.
	if( Recent.Num() )
	{
		check(ActorClass);
		UObject::ExitProperties( &Recent(0), ActorClass );
	}

	// If we're the client, destroy this actor.
	if( Connection->Driver->ServerConnection )
	{
		check(!Actor || Actor->IsValid());
		checkSlow(Level);
		checkSlow(Level->IsValid());
		checkSlow(Connection);
		checkSlow(Connection->IsValid());
		checkSlow(Connection->Driver);
		checkSlow(Connection->Driver->IsValid());
		if( Actor )
		{
			if( Actor->bTearOff )
			{
				Actor->Role = ROLE_Authority;
				Actor->RemoteRole = ROLE_None;
			}
			else if( !Actor->bNetTemporary )
				Actor->GetLevel()->DestroyActor( Actor, 1 );
		}
	}
	else if( Actor && !OpenAcked )
	{
		// Resend temporary actors if nak'd.
		Connection->SentTemporaries.RemoveItem( Actor );
	}
	Super::Destroy();
}

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
		// debugf(TEXT("%i Actor %s WILL BE sending %i vs first %i"), ChIndex, Actor->GetName(), Connection->PendingOutRec[ChIndex],Connection->OutReliable[ChIndex]);
		INT RealOutReliable = Connection->OutReliable[ChIndex];
		Connection->OutReliable[ChIndex] = Connection->PendingOutRec[ChIndex] - 1;
		while ( Connection->PendingOutRec[ChIndex] <= RealOutReliable )
		{
			// debugf(TEXT("%i SYNCHRONIZING by sending %i"), ChIndex, Connection->PendingOutRec[ChIndex]);

			FOutBunch Bunch( this, 0 );
			if( !Bunch.IsError() )
			{
				Bunch.bReliable = true;
				SendBunch( &Bunch, 0 );
				Connection->PendingOutRec[ChIndex]++;
			}
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
		INT Size = ActorClass->Defaults.Num();
		Recent.Add( Size );
		UObject::InitProperties( &Recent(0), Size, ActorClass, NULL, 0 );

		// Init config properties, to force replicate them.
		for( UProperty* It=ActorClass->ConfigLink; It; It=It->ConfigLinkNext )
		{
			if( It->PropertyFlags & CPF_NeedCtorLink )
				It->DestroyValue( &Recent(It->Offset) );
            UBoolProperty* BoolProperty = Cast<UBoolProperty>(It,CLASS_IsAUBoolProperty);
			if( !BoolProperty )
				appMemzero( &Recent(It->Offset), It->GetSize() );
			else
				*(DWORD*)&Recent(It->Offset) &= ~BoolProperty->BitMask;
		}
	}

	// Allocate retirement list.
	Retirement.Empty( ActorClass->ClassReps.Num() );
	while( Retirement.Num()<ActorClass->ClassReps.Num() )
		new(Retirement)FPropertyRetirement;

}

//
// Handle receiving a bunch of data on this actor channel.
//
void UActorChannel::ReceivedBunch( FInBunch& Bunch )
{
	check(!Closing);
	if ( Broken || bTornOff )
		return;

	// Initialize client if first time through.
	INT bJustSpawned = 0;
	FClassNetCache* ClassCache = NULL;
	if( Actor==NULL )
	{
		if( !Bunch.bOpen )
		{
			debugf(NAME_DevNetTraffic, TEXT("New actor channel received non-open packet: %i/%i/%i"),Bunch.bOpen,Bunch.bClose,Bunch.bReliable);
			return;
		}

		// Read class.
		UObject* Object;
		Bunch << Object;
		AActor* InActor = Cast<AActor>( Object );
		if( InActor==NULL )
		{
			// Transient actor.
			UClass* ActorClass = Cast<UClass>( Object );
			if (!ActorClass || !ActorClass->IsChildOf(AActor::StaticClass()))
			{
				// We are unsynchronized. Instead of crashing, let's try to recover.
				warnf(TEXT("Received invalid actor class"));
				Broken = 1;
				return;
			}
			check(ActorClass);
			check(ActorClass->IsChildOf(AActor::StaticClass()));

			FVector Location;
            FRotator Rotation(0,0,0);
            SerializeCompressedInitial( Bunch, Location, Rotation, ActorClass->GetDefaultActor()->bNetInitialRotation, NULL );

			InActor = Level->SpawnActor( ActorClass, NAME_None, Location, Rotation, NULL, 1, 1, NULL, NULL, 1 ); 
			bJustSpawned = 1;
			if ( !InActor )
				debugf(TEXT("Couldn't spawn %s high detail %d"),ActorClass->GetName() );
else 			if ( InActor->bDeleteMe )
				debugf(TEXT("Client received and deleted instantly %s"),InActor->GetName());
			check(InActor);
		}
		debugf( NAME_DevNetTraffic, TEXT("      Spawn %s:"), *InActor->GetFullName() );
		SetChannelActor( InActor );

		// If this is the player's channel and the connection was pending, mark it open.
		if
		(	Connection->Driver
		&&	Connection == Connection->Driver->ServerConnection
		&&	Connection->State==USOCK_Pending
		&&	Actor->GetAPlayerController() )
		{
			Connection->HandleClientPlayer( (APlayerController*)(Actor) ); 
		}
	}
	else debugf( NAME_DevNetTraffic, TEXT("      Actor %s:"), *Actor->GetFullName() );
	ClassCache = Connection->PackageMap->GetClassNetCache(ActorClass);

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
				Actor->bNetOwner = 1;
		}
		else
		{
			// We are the server.
			if( Player==Connection )
				Actor->bNetOwner = 1;
		}
	}
	// Handle the data stream.
	INT             RepIndex   = Bunch.ReadInt( ClassCache->GetMaxIndex() );
	FFieldNetCache* FieldCache = Bunch.IsError() ? NULL : ClassCache->GetFromIndex( RepIndex );
	while( FieldCache || bJustSpawned )
	{
		// Save current properties.
		//debugf(TEXT("Rep %s: %i"),FieldCache->Field->GetFullName(),RepIndex);
		UBOOL bHasFieldCache = (FieldCache != NULL);
		if ( bHasFieldCache && (Actor->Role != ROLE_Authority) )
		Actor->PreNetReceive();

		// Receive properties from the net.
		UProperty* It;
		while( FieldCache && (It=Cast<UProperty>(FieldCache->Field,CLASS_IsAUProperty))!=NULL )
		{
			// Receive array index.
			BYTE Element=0;
			if( It->ArrayDim != 1 )
				Bunch << Element;

			// Pointer to destiation.
			BYTE* DestActor  = (BYTE*)Actor;
			BYTE* DestRecent = Recent.Num() ? &Recent(0) : NULL;

			// Server, shouldn't receive properties.
			if( !Connection->Driver->ServerConnection )
			{
				debugfSlow( TEXT("Server received unwanted property value %s in %s"), It->GetName(), Actor->GetFullName() );
					DestActor  = NULL;
					DestRecent = NULL;
			}

			// Check property ordering.
			FPropertyRetirement& Retire = Retirement( It->RepIndex + Element );
			if( Bunch.PacketId>=Retire.InPacketId ) //!! problem with reliable pkts containing dynamic references, being retransmitted, and overriding newer versions. Want "OriginalPacketId" for retransmissions?
			{
				// Receive this new property.
				Retire.InPacketId = Bunch.PacketId;
			}
			else
			{
				// Skip this property, because it's out-of-date.
				debugfSlow( NAME_DevNetTraffic, TEXT("Received out-of-date %s"), It->GetName() );
				DestActor  = NULL;
				DestRecent = NULL;
			}

			// Receive property.
			FMemMark Mark(GMem);
			INT   Offset = It->Offset + Element*It->ElementSize;
			BYTE* Data   = DestActor ? (DestActor + Offset) : NewZeroed<BYTE>(GMem,It->ElementSize);
			It->NetSerializeItem( Bunch, Connection->PackageMap, Data );
			if( DestRecent )
				It->CopySingleValue( DestRecent + Offset, Data );
			Mark.Pop();
			// Successfully received it.
			debugf( NAME_DevNetTraffic, TEXT("         %s"), It->GetName() );

			// Notify the actor if this var is RepNotify

			if ( It->PropertyFlags & CPF_RepNotify )
			{
//				debugf( TEXT("Calling Actor->eventReplicatedEvent with %s"),It->GetName() );
				Actor->eventReplicatedEvent( It->GetName() );
			}

			// Get next.
			RepIndex   = Bunch.ReadInt( ClassCache->GetMaxIndex() );
			FieldCache = Bunch.IsError() ? NULL : ClassCache->GetFromIndex( RepIndex );
		}
		// Process important changed properties.
		if ( Actor->Role != ROLE_Authority )
		{
			if ( bHasFieldCache )
		Actor->PostNetReceive();
		if ( bJustSpawned )
		{
			bJustSpawned = 0;
			Actor->eventPostNetBeginPlay();
		}
		}

		// Handle function calls.
		if( FieldCache && Cast<UFunction>(FieldCache->Field,CLASS_IsAUFunction) )
		{
			FName Message = FieldCache->Field->GetFName();
			UFunction* Function = Actor->FindFunction( Message );
			check(Function);

			// See if UnrealScript replication condition is met.
			UBOOL Ignore=0;
			if ( !(Function->FunctionFlags & FUNC_NetServer) )
			{
			if( !Connection->Driver->ServerConnection )
			{
					UFunction* Test;
					for( Test=Function; Test->GetSuperFunction(); Test=Test->GetSuperFunction() );
				Exchange(Actor->Role,Actor->RemoteRole);
				DWORD Val=0;
				FFrame( Actor, Test->GetOwnerClass(), Test->RepOffset, NULL ).Step( Actor, &Val );
				Exchange(Actor->Role,Actor->RemoteRole);
				if( !Val || !Actor->bNetOwner )
				{
						debugf( NAME_DevNetTraffic, TEXT("Received unwanted function %s in %s"), *Message, Actor->GetFullName() );
					Ignore = 1;
				}
					else
						Function->FunctionFlags = Function->FunctionFlags | FUNC_NetServer; 
				}
				else
					Function->FunctionFlags = Function->FunctionFlags | FUNC_NetServer; 
			}
			debugf( NAME_DevNetTraffic, TEXT("      Received RPC: %s"), *Message );

			// Get the parameters.
			FMemMark Mark(GMem);
			BYTE* Parms = new(GMem,MEM_Zeroed,Function->ParmsSize)BYTE;
			for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
				if( Connection->PackageMap->ObjectToIndex(*It)!=INDEX_NONE )
					if( Cast<UBoolProperty>(*It,CLASS_IsAUBoolProperty) || Bunch.ReadBit() ) 
						It->NetSerializeItem(Bunch,Connection->PackageMap,Parms+It->Offset);

			// Call the function.
			if( !Ignore )
			{
				Actor->ProcessEvent( Function, Parms );
//				debugf( NAME_DevNetTraffic, TEXT("      Call RPC: %s"), Function->GetName() );
			}

			// Destroy the parameters.
			//warning: highly dependent on UObject::ProcessEvent freeing of parms!
			{for( UProperty* Destruct=Function->ConstructorLink; Destruct; Destruct=Destruct->ConstructorLinkNext )
				if( Destruct->Offset < Function->ParmsSize )
					Destruct->DestroyValue( Parms + Destruct->Offset );}
			Mark.Pop();

			// Next.
			RepIndex   = Bunch.ReadInt( ClassCache->GetMaxIndex() );
			FieldCache = Bunch.IsError() ? NULL : ClassCache->GetFromIndex( RepIndex );
		}
		else if( FieldCache )
		{
			appErrorfSlow( TEXT("Invalid replicated field %i in %s"), RepIndex, *Actor->GetFullName() );
			return;
		}
	}
	// Tear off an actor on the client-side
	if ( Actor->bTearOff && (Actor->Level->NetMode == NM_Client) )
	{
		Actor->Role = ROLE_Authority;
		Actor->RemoteRole = ROLE_None;
		bTornOff = true;
		Connection->ActorChannels.Remove( Actor );
		Actor->eventTornOff();
		Actor = NULL;
		return;
	}

}

//
// Replicate this channel's actor differences.
//
void UActorChannel::ReplicateActor()
{
	checkSlow(Actor);
	checkSlow(!Closing);

	// Create an outgoing bunch, and skip this actor if the channel is saturated.
	FOutBunch Bunch( this, 0 );
	if( Bunch.IsError() )
		return;

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
	Actor->bNetOwner = Connection->Driver->ServerConnection ? Cast<ULocalPlayer>(Player)!=NULL : Player==Connection;

	// If initial, send init data.
	if( Actor->bNetInitial && OpenedLocally )
	{
		if( Actor->bStatic || Actor->bNoDelete )
		{
			// Persistent actor.
			Bunch << Actor;
		}
		else
		{
			// Transient actor.
            Bunch << ActorClass;
            SerializeCompressedInitial( Bunch, Actor->Location, Actor->Rotation, Actor->bNetInitialRotation, this );
		}
	}

	// Save out the actor's RemoteRole, and downgrade it if necessary.
	BYTE ActualRemoteRole=Actor->RemoteRole;
	if( Actor->RemoteRole==ROLE_AutonomousProxy && Actor->Instigator && !Actor->Instigator->bNetOwner && !Actor->bNetOwner )
		Actor->RemoteRole=ROLE_SimulatedProxy;

	Actor->bNetDirty = ActorDirty || Actor->bNetInitial;
	Actor->bNetInitial = Actor->bNetInitial || bActorStillInitial; // for replication purposes, bNetInitial stays true until all properties sent
	bActorMustStayDirty = false;
	bActorStillInitial = false;
	debugf(NAME_DevNetTraffic,TEXT("Replicate %s: NetDirty %d Still Initial %d"),ActorClass->GetName(), Actor->bNetDirty,Actor->bNetInitial);

	// Get memory for retirement list.
	FMemMark Mark(GMem);
	appMemzero( &RepEval(0), RepEval.Num() );
	INT* Reps = New<INT>( GMem, Retirement.Num() ), *LastRep;
	UBOOL		FilledUp = 0;

	// Figure out which properties to replicate.
	BYTE*   CompareBin = Recent.Num() ? &Recent(0) : &ActorClass->Defaults(0);
	INT     iCount     = ClassCache->RepProperties.Num();
	LastRep            = Actor->GetOptimizedRepList( CompareBin, &Retirement(0), Reps, Connection->PackageMap,this );
	if ( Actor->bNetDirty )
	{
		if ( Actor->DelayScriptReplication(LastFullUpdateTime) )
		{
			bActorMustStayDirty = true;
		}
		else
		{
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
							debugf(NAME_DevNetTraffic,TEXT("MUST STAY DIRTY Because of %s"),(*(UObject**)Src)->GetName());
							if (!(*(UObject**)Src)->IsPendingKill())
							{
								bActorMustStayDirty = true;
							}
							Src = NULL;
						}
						if( !It->Identical(CompareBin+Offset,Src) )
						{
							if( !(Eval & 2) )
							{
								DWORD Val=0;
								FFrame( Actor, It->RepOwner->GetOwnerClass(), It->RepOwner->RepOffset, NULL ).Step( Actor, &Val );
								Eval = Val | 2;
							}
							if( Eval & 1 )
								*LastRep++ = It->RepIndex+Index;
						}
					}
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

		// Figure out field to replicate.
		FFieldNetCache* FieldCache
		=	It->GetFName()==NAME_Role
		?	ClassCache->GetFromField(Connection->Driver->RemoteRoleProperty)
		:	It->GetFName()==NAME_RemoteRole
		?	ClassCache->GetFromField(Connection->Driver->RoleProperty)
		:	ClassCache->GetFromField(It);
		checkSlow(FieldCache);

		// Send property name and optional array index.
		FBitWriterMark Mark( Bunch );
		Bunch.WriteInt( FieldCache->FieldNetIndex, ClassCache->GetMaxIndex() );
		if( It->ArrayDim != 1 )
		{
			BYTE Element = Index;
			Bunch << Element;
		}

		// Send property.
		UBOOL Mapped = It->NetSerializeItem( Bunch, Connection->PackageMap, (BYTE*)Actor + Offset );
		debugf(NAME_DevNetTraffic,TEXT("   Send %s %i"),It->GetName(),Mapped);
		if( !Bunch.IsError() )
		{
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
			Mark.Pop( Bunch );
			LastRep  = iPtr;
			//debugf(NAME_DevNetTraffic,TEXT("FILLED UP"));
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
		debugfSlow(NAME_DevNetTraffic,TEXT("Filled packet up before finishing %s still initial %d"),Actor->GetName(),bActorStillInitial);
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

	Mark.Pop();
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

IMPLEMENT_CLASS(UActorChannel);

/*-----------------------------------------------------------------------------
	UFileChannel implementation.
-----------------------------------------------------------------------------*/

UFileChannel::UFileChannel()
{
	Download = NULL;
}
void UFileChannel::Init( UNetConnection* InConnection, INT InChannelIndex, INT InOpenedLocally )
{
	Super::Init( InConnection, InChannelIndex, InOpenedLocally );
}
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
		if( !Connection->Driver->AllowDownloads )
		{
			// Refuse the download by sending a 0 bunch.
			debugf( NAME_DevNet, *LocalizeError(TEXT("NetInvalid"),TEXT("Engine")) );
			FOutBunch Bunch( this, 1 );
			SendBunch( &Bunch, 0 );
			return;
		}
		if( SendFileAr )
		{
			FString Cmd;
			Bunch << Cmd;
			if( !Bunch.IsError() && Cmd==TEXT("SKIP") )
			{
				// User cancelled optional file download.
				// Remove it from the package map
				debugf( TEXT("User skipped download of '%s'"), SrcFilename );
				Connection->PackageMap->List.Remove( PackageIndex );
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
				for( INT i=0; i<Connection->PackageMap->List.Num(); i++ )
				{
					FPackageInfo& Info = Connection->PackageMap->List(i);
					if( Info.Guid==Guid && Info.URL!=TEXT("") )
					{
						if( Connection->Driver->MaxDownloadSize>0 &&
							GFileManager->FileSize(*Info.URL)>Connection->Driver->MaxDownloadSize )
							break;							
						appStrncpy( SrcFilename, *Info.URL, ARRAY_COUNT(SrcFilename) );
						if( Connection->Driver->Notify->NotifySendingFile( Connection, Guid ) )
						{
							check(Info.Linker);
							SendFileAr = GFileManager->CreateFileReader( SrcFilename );
							if( SendFileAr )
							{
								// Accepted! Now initiate file sending.
								debugf( NAME_DevNet, *LocalizeProgress(TEXT("NetSend"),TEXT("Engine")), SrcFilename );
								PackageIndex = i;
								return;
							}
						}
					}
				}
			}
		}

		// Illegal request; refuse it by closing the channel.
		debugf( NAME_DevNet, *LocalizeError(TEXT("NetInvalid"),TEXT("Engine")) );
		FOutBunch Bunch( this, 1 );
		SendBunch( &Bunch, 0 );
	}
}
void UFileChannel::Tick()
{
	UChannel::Tick();
	Connection->TimeSensitive = 1;
	INT Size;
	//TIM: IsNetReady(1) causes the client's bandwidth to be saturated. Good for clients, very bad
	// for bandwidth-limited servers. IsNetReady(0) caps the clients bandwidth.
	static UBOOL LanPlay = ParseParam(appCmdLine(),TEXT("lanplay"));
	while( !OpenedLocally && SendFileAr && IsNetReady(LanPlay) && (Size=MaxSendBytes())!=0 )
	{
		// Sending.
		INT Remaining = Connection->PackageMap->List(PackageIndex).FileSize-SentData;
		FOutBunch Bunch( this, Size>=Remaining );
		Size = Min( Size, Remaining );
		BYTE* Buffer = (BYTE*)appAlloca( Size );
		SendFileAr->Serialize( Buffer, Size );
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
}
void UFileChannel::Destroy()
{
	check(Connection);
	if( RouteDestroy() )
		return;
	check(Connection->Channels[ChIndex]==this);

	// Close the file.
	if( SendFileAr )
	{
		delete SendFileAr;
		SendFileAr = NULL;
	}

	// Notify that the receive succeeded or failed.
	if( OpenedLocally && Download )
	{
		Download->DownloadDone();
		delete Download;
	}
	Super::Destroy();
}
FString UFileChannel::Describe()
{
	FPackageInfo& Info = Connection->PackageMap->List( PackageIndex );
	return FString::Printf
	(
		TEXT("File='%s', %s=%i/%i "),
		OpenedLocally ? (Download?Download->TempFilename:TEXT("")): SrcFilename,
		OpenedLocally ? TEXT("Received") : TEXT("Sent"),
		OpenedLocally ? (Download?Download->Transfered:0): SentData,
		Info.FileSize
	) + UChannel::Describe();
}
IMPLEMENT_CLASS(UFileChannel)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

