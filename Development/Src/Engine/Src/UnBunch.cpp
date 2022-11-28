/*=============================================================================
	UnBunch.h: Unreal bunch (sub-packet) functions.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"

/*-----------------------------------------------------------------------------
	FInBunch implementation.
-----------------------------------------------------------------------------*/

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
:	FBitWriter	( InChannel->Connection->MaxPacket*8-MAX_BUNCH_HEADER_BITS-MAX_PACKET_TRAILER_BITS-MAX_PACKET_HEADER_BITS )
,	Channel		( InChannel )
,	ChIndex     ( InChannel->ChIndex )
,	ChType      ( InChannel->ChType )
,	bOpen		( 0 )
,	bClose		( bInClose )
,	bReliable	( 0 )
{
	checkSlow(!Channel->Closing);
	checkSlow(Channel->Connection->Channels[Channel->ChIndex]==Channel);

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

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

