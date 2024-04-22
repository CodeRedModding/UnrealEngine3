/*=============================================================================
	UnBunch.h: Unreal bunch (sub-packet) functions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"

#if WITH_UE3_NETWORKING

/*-----------------------------------------------------------------------------
	FInBunch implementation.
-----------------------------------------------------------------------------*/

FInBunch::FInBunch( UNetConnection* InConnection )
:	FBitReader	()
,	Next		( NULL )
,	Connection  ( InConnection )
{
	check(Connection);
	// Match the byte swapping settings of the connection
	SetByteSwapping(Connection->bNeedsByteSwapping);
	// Crash protection: the max string size serializable on this archive 
	ArMaxSerializeSize = MAX_STRING_SERIALIZE_SIZE;
}

//
// Read an object.
//
FArchive& FInBunch::operator<<( UObject*& Object )
{
	Connection->PackageMap->SerializeObject( *this, UObject::StaticClass(), Object );
	return *this;
}

//
// Read a name.
//
FArchive& FInBunch::operator<<( class FName& N )
{
	Connection->PackageMap->SerializeName( *this, N );
	return *this;
}

/*-----------------------------------------------------------------------------
	FOutBunch implementation.
-----------------------------------------------------------------------------*/

//
// Construct an outgoing bunch for a channel.
// It is ok to either send or discard an FOutbunch after construction.
//
FOutBunch::FOutBunch()
: FBitWriter( 0 )
{}
FOutBunch::FOutBunch( UChannel* InChannel, UBOOL bInClose )
:	FBitWriter	( InChannel->Connection->MaxPacket*8-MAX_BUNCH_HEADER_BITS-MAX_PACKET_TRAILER_BITS-MAX_PACKET_HEADER_BITS
#if WITH_STEAMWORKS_SOCKETS
				- (InChannel->Connection->bUseSessionUID ? SESSION_UID_BITS : 0)
#endif
			)
,	Channel		( InChannel )
,	ChIndex     ( InChannel->ChIndex )
,	ChType      ( InChannel->ChType )
,	bOpen		( 0 )
,	bClose		( bInClose )
,	bReliable	( 0 )
{
	checkSlow(!Channel->Closing);
	checkSlow(Channel->Connection->Channels[Channel->ChIndex]==Channel);

	// Match the byte swapping settings of the connection
	SetByteSwapping(Channel->Connection->bNeedsByteSwapping);

	// Reserve channel and set bunch info.
	if( Channel->NumOutRec >= RELIABLE_BUFFER-1+bClose )
	{
		SetOverflowed();
		return;
	}

}

//
// Write a name.
//
FArchive& FOutBunch::operator<<( class FName& N )
{
	Channel->Connection->PackageMap->SerializeName( *this, N );
	return *this;
}

//
// Write an object.
//
FArchive& FOutBunch::operator<<( UObject*& Object )
{
	Channel->Connection->PackageMap->SerializeObject( *this, UObject::StaticClass(), Object );
	return *this;
}


FControlChannelOutBunch::FControlChannelOutBunch(UChannel* InChannel, UBOOL bClose)
	: FOutBunch(InChannel, bClose)
{
	checkSlow(Cast<UControlChannel>(InChannel) != NULL);
	// control channel bunches contain critical handshaking/synchronization and should always be reliable
	bReliable = TRUE;
}

#endif	//#if WITH_UE3_NETWORKING

