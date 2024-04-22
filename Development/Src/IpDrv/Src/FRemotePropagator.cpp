/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "UnIpDrv.h"
#include "EnginePhysicsClasses.h"

#if WITH_UE3_NETWORKING

#define PROPAGATION_PORT 9989

/**
 * This class handles sending and receiving UDP data between propagators
 */
struct FListenHelper : FUdpLink
{
	FListenPropagator* Propagator;

	FListenHelper() : Propagator(NULL)
	{}
	void OnReceivedData(FIpAddr SrcAddr, BYTE* Data, INT Count)
	{
		if (Propagator)
			Propagator->OnReceivedData(SrcAddr, Data, Count);
	}
};
FListenHelper* ListenHelper = NULL;
FListenHelper* SendHelper = NULL;

UBOOL FRemotePropagator::Connect()
{
	delete SendHelper;
	SendHelper = new FListenHelper;
	Paused = 0; 
	return TRUE; 
}

void FRemotePropagator::AddTarget(TARGETHANDLE Target, DWORD RemoteIPAddress, UBOOL bIntelByteOrder)
{
	FRemoteTargetInfo Info;
	Info.IPAddress = RemoteIPAddress;
	Info.bIntelByteOrder = bIntelByteOrder;

	TargetMap.Set(Target, Info);
}

void FRemotePropagator::RemoveTarget(TARGETHANDLE Target)
{
	TargetMap.Remove(Target);
}

UINT FRemotePropagator::GetTargetCount() const
{
	return TargetMap.Num();
}

void FRemotePropagator::ClearTargets()
{
	TargetMap.Empty();
}

UBOOL FRemotePropagator::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	// if we find a REMOTE command, send the command (minus the REMOTE) to the listener
	if (ParseCommand(&Cmd, TEXT("REMOTE")))
	{
		FNetworkRemoteConsoleCommand ConsoleCommand(Cmd);
		SendChange(&ConsoleCommand);

		return TRUE;
	}
	// handle a special case for PS3 remote map opening
	else if (ParseCommand(&Cmd, TEXT("PS3REMOTE")))
	{
		Exec(TEXT("REMOTE OPEN ENTRY"), Ar);
		appSleep(5);
		Exec(TEXT("REMOTE FLUSHFILECACHE"), Ar);
//		Exec(TEXT("REMOTE RELOADSHADERS"), Ar);
		Exec(*FString::Printf(TEXT("REMOTE OPEN %s"), Cmd), Ar);

		return TRUE;
	}

	return FALSE;
}

void FRemotePropagator::SendChange(class FNetworkPropagatorBase* Change)
{
	// create the socket address structure from the current IP address and the hardcoded port
	FInternetIpAddr SockAddr;
	SockAddr.SetPort(PROPAGATION_PORT);

	for(TMap<TARGETHANDLE, FRemoteTargetInfo>::TIterator Iter(TargetMap); Iter; ++Iter)
	{
		FRemoteTargetInfo Info = Iter.Value( );

		SockAddr.SetIp(Info.IPAddress);
		// Create the Unreal FIpAddr structure from the network standard type
		FIpAddr IpAddr(SockAddr);
		// Send the change to IpAddr over UdpLink
		Change->SendTo(SendHelper, IpAddr, Info.bIntelByteOrder);
	}
}

// On a property change, fill out a FNetworkPropertyChange structure, and send it across the network
void FRemotePropagator::OnPropertyChange(UObject* Object, UProperty* Property, INT PropertyOffset)
{
	check(Object);

	// do nothing if paused
	if (Paused || TargetMap.Num() == 0)
	{
		return;
	}

	// if we are trying to move or rotate an actor in the property window, use the ActorMove function instead
	if ((appStricmp(*Property->GetName(), TEXT("Location")) == 0 || appStricmp(*Property->GetName(), TEXT("Rotation")) == 0) && Object->IsA(AActor::StaticClass()))
	{
		OnActorMove(Cast<AActor>(Object));
		return;
	}

#ifdef PROPAGATE_SEND_OBJECT
	FStringOutputDevice Ar;
	const FExportObjectInnerContext Context;
	UExporter::ExportToOutputDevice( &Context, GWorld, NULL, Ar, TEXT("copy"), 0, PPF_ExportsNotFullyQualified );
#else
	check(Property);

#ifdef PROPAGATE_EXACT_PROPERTY_ONLY
	// get the address of the property, and then subtract off the property offset, because ExportText will add it back in
	BYTE* PropertyAddress = (BYTE*)Object + PropertyOffset - Property->Offset;
	// Get the value of the property to a string
	FString PropString;
	Property->ExportText(0, PropString, PropertyAddress, PropertyAddress, NULL, PPF_Localized);

	// fill out a network change structure
	FNetworkPropertyChange Change(Object->GetPathName(GWorld), Property->GetPathName(), PropString, PropertyOffset);
#else
	// Get the value of the property to a string
	FString PropString;
	Property->ExportText(0, PropString, (BYTE*)Object, (BYTE*)Object, NULL, PPF_Localized);

	// fill out a network change structure
	FNetworkPropertyChange Change(Object->GetPathName(GWorld), Property->GetName(), PropString, PropertyOffset);

#endif
#endif
	// send the object name and property value across the network
	SendChange(&Change);
}

void FRemotePropagator::OnActorMove(AActor* Actor)
{
	check(Actor);

	// do nothing if paused
	if (Paused || TargetMap.Num() == 0)
	{
		return;
	}

	// fill out a FNetworkActorMove with the actors location and rotation
	FNetworkActorMove Change(*Actor->GetPathName(GWorld), Actor->Location, Actor->Rotation);

	// send the object name and property value across the network
	SendChange(&Change);
}

void FRemotePropagator::OnActorCreate(AActor* Actor)
{
	check(Actor);

	// do nothing if paused
	if (Paused || TargetMap.Num() == 0)
	{
		return;
	}

	// build an array that contains the components along with their template names so we can 
	// match up components in the other actor (we match up by template name as that is a good 
	// unique identifier, and not all components in the new actor as are in the old)
	TArray<FString> ComponentTemplatesAndNames;
	for (INT CompIndex = 0; CompIndex < Actor->Components.Num(); CompIndex++)
	{
		new(ComponentTemplatesAndNames) FString(Actor->Components(CompIndex)->GetArchetype()->GetName());
		new(ComponentTemplatesAndNames) FString(Actor->Components(CompIndex)->GetName());
	}

	// search for subobjects, and add the property name pointing to the object and the object name
	for (TFieldIterator<UObjectProperty> It(Actor->GetClass()); It; ++It)
	{
		// get what th obj property is pointing to
		UObject* ObjValue;
		It->CopySingleValue(&ObjValue, (BYTE*)Actor + It->Offset);
		if (ObjValue && !ObjValue->IsA(UComponent::StaticClass()))
		{
			new(ComponentTemplatesAndNames) FString(It->GetName());
			new(ComponentTemplatesAndNames) FString(ObjValue->GetName());
		}
	}

	// fill out a FNetworkActorCreate with the actors location and rotation
	FNetworkActorCreate Change(Actor->GetClass()->GetPathName(), *Actor->GetName(), Actor->Location, Actor->Rotation, ComponentTemplatesAndNames);

	// send the request across the network
	SendChange(&Change);
}

void FRemotePropagator::OnActorDelete(AActor* Actor)
{
	check(Actor);

	// do nothing if paused
	if (Paused || TargetMap.Num() == 0)
	{
		return;
	}

	// fill out a FNetworkActorDelete with the actors name
	FNetworkActorDelete Change(*Actor->GetPathName(GWorld));

	// send the request across the network
	SendChange(&Change);
}

void FRemotePropagator::OnObjectRename(UObject* Object, const TCHAR* NewName)
{
	check(Object);

	// do nothing if paused
	if (Paused || TargetMap.Num() == 0)
	{
		return;
	}

	// fill out a FNetworkActorDelete with the actors name
	FNetworkObjectRename Change(*Object->GetPathName(GWorld), FString(NewName));

	// send the request across the network
	SendChange(&Change);
}


UBOOL FListenPropagator::Connect()
{
	// create the listen helper if needed
	if (!ListenHelper)
	{
		ListenHelper = new FListenHelper;
	}

	// bind to the listening port
	if (!ListenHelper->BindPort(PROPAGATION_PORT))
	{
		debugf(TEXT("Failed to bind to port %d for listening for propagation messages."), PROPAGATION_PORT);
		return false;
	}
	ListenHelper->Propagator = this;
	ListenHelper->Poll();

	return true;
}

void FListenPropagator::Tick(FLOAT DeltaTime)
{
	if (ListenHelper)
		ListenHelper->Poll();
}

void FListenPropagator::OnReceivedData(FIpAddr SrcAddr, BYTE* Data, INT Count)
{
	// make a TArray that we can serialize out of
	TArray<BYTE> Payload;
	Payload.Add(Count);
	appMemcpy(Payload.GetData(), Data, Count);

	if( Count < 2*sizeof(DWORD) )
	{
		debugf(TEXT("Received bad network data in the FListenPropagator"));
		return;
	}

	// make an archive to read from
	FMemoryReader PayloadReader(Payload);
	DWORD TotalSize;
	DWORD Type;
	PayloadReader << TotalSize << Type;

	if (Count != TotalSize)
	{
		debugf(TEXT("Received bad network data in the FListenPropagator"));
		return;
	}

	switch (Type)
	{
		HANDLE_RECEIVED_DATA(PropertyChange);
		HANDLE_RECEIVED_DATA(ActorMove);
		HANDLE_RECEIVED_DATA(ActorCreate);
		HANDLE_RECEIVED_DATA(ActorDelete);
		HANDLE_RECEIVED_DATA(ObjectRename);
		HANDLE_RECEIVED_DATA(RemoteConsoleCommand);
		// @todo: add more change types here
	}
}

void FListenPropagator::OnNetworkPropertyChange(FIpAddr SrcAddr, struct FNetworkPropertyChange* PropertyChange)
{
//	debugf(TEXT("Setting Property '%s::%s' to '%s'"), *PropertyChange->ObjectName, *PropertyChange->PropertyName, *PropertyChange->PropertyValue);
#ifdef PROPAGATE_SEND_OBJECT
#else
	UObject* Object = UObject::StaticFindObject(UObject::StaticClass(), GWorld, *PropertyChange->ObjectName);

	// if it wasn't found, then we just silently ignore the request
	if (!Object)
	{
		return;
	}

	//	debugf(TEXT("Found object %s"), *Object->GetFullName());

#ifdef PROPAGATE_EXACT_PROPERTY_ONLY
	UProperty* Property = (UProperty*)UObject::StaticFindObject(UProperty::StaticClass(), ANY_PACKAGE, *PropertyChange->PropertyName);

	// if it wasn't found, then we just silently ignore the request
	if (!Property)
	{
		return;
	}

	// get the address of the property, and then subtract off the property offset, because ExportText will add it back in
	BYTE* PropertyAddress = (BYTE*)Object + PropertyChange->PropertyOffset;

	// set the value
	Property->ImportText(*PropertyChange->PropertyValue, PropertyAddress, PPF_Localized, Object);
#else

	UProperty* Property = FindField<UProperty>(Object->GetClass(), *PropertyChange->PropertyName);

	// if it wasn't found, then we just silently ignore the request
	if (!Property)
	{
		return;
	}

	// set the value
	Property->ImportText(*PropertyChange->PropertyValue, (BYTE*)Object + Property->Offset, PPF_Localized, Object);
#endif
#endif

	//	debugf(TEXT("Set Property '%s' to '%s'"), *Property->GetFullName(), *PropertyChange->PropertyValue);

	// handle any component updating that is necessary
	PostPropertyChange(Object, Property);
}

void FListenPropagator::OnNetworkActorMove(FIpAddr SrcAddr, struct FNetworkActorMove* ActorMove)
{
	// find the referenced actor
	AActor* Actor = (AActor*)UObject::StaticFindObject(AActor::StaticClass(), GWorld, *ActorMove->ActorName);

	if (!Actor)
	{
		// if we failed to find it, then just return silently
		return;
	}

	//	debugf(TEXT("Moving actor '%s' [%s]"), *Actor->GetFullName(), *ActorMove->ActorName);

	// actually move the actor
	ProcessActorMove(Actor, ActorMove->Location, ActorMove->Rotation);
}

void FListenPropagator::OnNetworkActorCreate(FIpAddr SrcAddr, struct FNetworkActorCreate* ActorCreate)
{
	// find the referenced actor
	UClass* Class = (UClass*)UObject::StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *ActorCreate->ActorClass);
	if (!Class)
	{
		// if we failed to find it, then just return silently
		return;
	}

	// actually create the actor
	AActor* NewActor = ProcessActorCreate(Class, FName(*ActorCreate->ActorName), ActorCreate->Location, ActorCreate->Rotation, ActorCreate->ComponentTemplatesAndNames);

	//	debugf(TEXT("Created actor '%s'"), *NewActor->GetFullName());
}

void FListenPropagator::OnNetworkActorDelete(FIpAddr SrcAddr, struct FNetworkActorDelete* ActorDelete)
{
	// find the referenced actor
	AActor* Actor = (AActor*)UObject::StaticFindObject(AActor::StaticClass(), GWorld, *ActorDelete->ActorName);

	if (!Actor)
	{
		// if we failed to find it, then just return silently
		return;
	}

	// actually delete the actor
	ProcessActorDelete(Actor);
}

void FListenPropagator::OnNetworkObjectRename(FIpAddr SrcAddr, struct FNetworkObjectRename* ObjectRename)
{
	UObject* Object = UObject::StaticFindObject(UObject::StaticClass(), GWorld, *ObjectRename->ObjectName);

	// if it wasn't found, then we just silently ignore the request
	if (!Object)
	{
		return;
	}

	//	debugf(TEXT("Renaming object '%s' to '%s'"), *Object->GetFullName(), *ObjectRename->NewName);

	// actually rename the object
	ProcessObjectRename(Object, *ObjectRename->NewName);
}

void FListenPropagator::OnNetworkRemoteConsoleCommand(FIpAddr SrcAddr, struct FNetworkRemoteConsoleCommand* RemoteConsoleCommand)
{
	UBOOL bCalledExec = FALSE;

	// first try to run the command 
	for ( INT ViewportIndex = 0; ViewportIndex < GEngine->GamePlayers.Num(); ViewportIndex++ )
	{
		ULocalPlayer* Player = GEngine->GamePlayers(ViewportIndex);
		if ( Player->Exec(*RemoteConsoleCommand->ConsoleCommand, *GLog) )
			return;

		bCalledExec = TRUE;
	}

	if ( !bCalledExec )
	{
		GEngine->Exec(*RemoteConsoleCommand->ConsoleCommand);
	}
}

#endif	//#if WITH_UE3_NETWORKING
