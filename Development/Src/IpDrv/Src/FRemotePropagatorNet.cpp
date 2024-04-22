/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

////////////////////////////////////////////////////////////
//
// FNetworkPropagatorBase
//
////////////////////////////////////////////////////////////

/**
* Sends a FNetworkPropagatorBase across the network. The subclass will fill
* in the actual data, in FillPayload()
*
* @param	Link			The UDP link to send the data over
* @param	IpAddr			The IP address to send the data to
*/

class FByteOrderedWriter : public FMemoryWriter
{
public:
	FByteOrderedWriter(TArray<BYTE>& InBytes, UBOOL bIsIntelByteOrder)
		: FMemoryWriter(InBytes)
	{
		// depending on our source byte order and destination byte order, set the appropriate flags 
		// so that ByteOrderSerialize do the right thing
#if __INTEL_BYTE_ORDER__
		if (!bIsIntelByteOrder)
		{
			ArForceByteSwapping = TRUE;
		}
#else
		if (bIsIntelByteOrder)
		{
			ArIsPersistent = TRUE;		
		}
#endif
	}
};

void FNetworkPropagatorBase::SendTo(FUdpLink* Link, const FIpAddr& IpAddr, UBOOL bIsIntelByteOrder)
{
	TArray<BYTE> Payload;
	// this writer will write out the bytes in the appropriate manner for the destination platform
	FByteOrderedWriter PayloadWriter(Payload, bIsIntelByteOrder);

	// write out the basic data
	DWORD TotalSize = 0; 
	PayloadWriter << TotalSize; // save space for total size later

	// EPropagationType e.g. PROP_RemoteConsoleCommand
	PayloadWriter << Type; // write out the type

	// write out our data to this archive, by serialize override
	Serialize(PayloadWriter);

	// calculate how big it actually is
	TotalSize = Payload.Num();
	
	// put the size into the stream (need to make a new writer
	// to reset the position in the array to 0)
	FByteOrderedWriter PayloadWriter2(Payload, bIsIntelByteOrder);
	PayloadWriter2 << TotalSize;

	// send it over the wire
	Link->SendTo(IpAddr, (BYTE*)Payload.GetData(), TotalSize);
}

////////////////////////////////////////////////////////////
//
// FNetworkPropertyChange
//
////////////////////////////////////////////////////////////

FNetworkPropertyChange::FNetworkPropertyChange(const FString& InObjectName, const FString& InPropertyName, const FString& InPropertyValue, DWORD InPropertyOffset)
	: FNetworkPropagatorBase(PROP_PropertyChange)
{
	ObjectName = InObjectName;
	PropertyName = InPropertyName;
	PropertyValue = InPropertyValue;
	PropertyOffset = InPropertyOffset;
}

void FNetworkPropertyChange::Serialize(FArchive& Ar)
{
	Ar << PropertyOffset << ObjectName << PropertyName << PropertyValue;
}



////////////////////////////////////////////////////////////
//
// FNetworkActorMove
//
////////////////////////////////////////////////////////////

FNetworkActorMove::FNetworkActorMove(const FString& InActorName, const FVector& InLocation, const FRotator& InRotation)
	: FNetworkPropagatorBase(PROP_ActorMove)
{
	ActorName = InActorName;
	Location = InLocation;
	Rotation = InRotation;
}

void FNetworkActorMove::Serialize(FArchive& Ar)
{
	Ar << Location << Rotation << ActorName;
}

////////////////////////////////////////////////////////////
//
// FNetworkActorCreate
//
////////////////////////////////////////////////////////////

FNetworkActorCreate::FNetworkActorCreate(const FString& InActorClass, const FString& InActorName, const FVector& InLocation, const FRotator& InRotation, const TArray<FString>& InComponentTemplatesAndNames)
	: FNetworkPropagatorBase(PROP_ActorCreate)
{
	ActorClass = InActorClass;
	ActorName = InActorName;
	Location = InLocation;
	Rotation = InRotation;
	ComponentTemplatesAndNames = InComponentTemplatesAndNames;
}

void FNetworkActorCreate::Serialize(FArchive& Ar)
{
	Ar << Location << Rotation << ActorClass << ActorName << ComponentTemplatesAndNames;
}

////////////////////////////////////////////////////////////
//
// FNetworkActorDelete
//
////////////////////////////////////////////////////////////

FNetworkActorDelete::FNetworkActorDelete(const FString& InActorName)
	: FNetworkPropagatorBase(PROP_ActorDelete)
{
	ActorName = InActorName;
}

void FNetworkActorDelete::Serialize(FArchive& Ar)
{
	Ar << ActorName;
}

////////////////////////////////////////////////////////////
//
// FNetworkObjectRename
//
////////////////////////////////////////////////////////////

FNetworkObjectRename::FNetworkObjectRename(const FString& InObjectName, const FString& InNewName)
	: FNetworkPropagatorBase(PROP_ObjectRename)
{
	ObjectName = InObjectName;
	NewName = InNewName;
}

void FNetworkObjectRename::Serialize(FArchive& Ar)
{
	Ar << ObjectName << NewName;
}

////////////////////////////////////////////////////////////
//
// FNetworkRemoteConsoleCommand
//
////////////////////////////////////////////////////////////

FNetworkRemoteConsoleCommand::FNetworkRemoteConsoleCommand(const FString& InConsoleCommand)
	: FNetworkPropagatorBase(PROP_RemoteConsoleCommand)
{
	ConsoleCommand = InConsoleCommand;
}

void FNetworkRemoteConsoleCommand::Serialize(FArchive& Ar)
{
	Ar << ConsoleCommand;
}

#endif	//#if WITH_UE3_NETWORKING
